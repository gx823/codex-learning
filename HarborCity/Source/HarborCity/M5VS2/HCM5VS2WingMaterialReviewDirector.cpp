#include "HCM5VS2WingMaterialReviewDirector.h"
#include "HCM5VS2FlightComponent.h"
#include "HCM5VS2FlightVisualComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ContentStreaming.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include <atomic>
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

struct FHCM5VS2WingMaterialReadiness
{
    const FMaterialRenderProxy* Proxy=nullptr;
    bool Ready=false; FString Actual;
    std::atomic<bool> Complete{false};
};
namespace
{
TArray<TSharedPtr<FJsonValue>> Vec(const FVector& V)
{return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
bool OwnedMap(const FString& Name)
{
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/FlightVisual/Review_");
    if(!Name.StartsWith(Prefix))return false;
    const FString Rest=Name.Mid(Prefix.Len());
    if(Rest.Len()!=12+FString(TEXT("/L_WingMaterialReview")).Len()||Rest.Mid(12)!=TEXT("/L_WingMaterialReview"))return false;
    for(int32 I=0;I<12;++I)if(!((Rest[I]>='0'&&Rest[I]<='9')||(Rest[I]>='a'&&Rest[I]<='f')))return false;
    return true;
}
bool PNG(const FString& File)
{
    TUniquePtr<FArchive> A(IFileManager::Get().CreateFileReader(*File));if(!A||A->TotalSize()<33)return false;
    uint8 B[24];A->Serialize(B,24);const uint8 Sig[]={137,80,78,71,13,10,26,10};
    auto Big=[](const uint8* P){return (uint32(P[0])<<24)|(uint32(P[1])<<16)|(uint32(P[2])<<8)|P[3];};
    return !A->IsError()&&!FMemory::Memcmp(B,Sig,8)&&!FMemory::Memcmp(B+12,"IHDR",4)&&Big(B+16)==1920&&Big(B+20)==1080;
}
}
AHCM5VS2WingMaterialReviewDirector::AHCM5VS2WingMaterialReviewDirector()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.bTickEvenWhenPaused=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}

void AHCM5VS2WingMaterialReviewDirector::BeginPlay()
{
    Super::BeginPlay();
    if(!FParse::Param(FCommandLine::Get(),TEXT("M5VS2WingMaterialReview")))return;
    Started=StageStarted=FPlatformTime::Seconds();bActive=true;bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));SetActorTickEnabled(true);BindViewport();
    FString Root;
    FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root);Root=FPaths::ConvertRelativePathToFull(Root);FPaths::NormalizeFilename(Root);
    if(!FPaths::CollapseRelativeDirectories(Root)||!FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime"))||!OwnedMap(GetWorld()->GetOutermost()->GetName()))
    {Finish(TEXT("FAIL"),TEXT("Private map/evidence boundary invalid"));return;}
    Directory=Root/TEXT("WingMaterialReview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if(!IFileManager::Get().MakeDirectory(*Directory,true)){Directory.Empty();Finish(TEXT("FAIL"),TEXT("Evidence directory unavailable"));return;}
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2WingMaterialReviewDirector::Processed);
    if(!Write(TEXT("RUNNING"),TEXT("Four-picture FUNCTION_CALL_VISUAL_FIXTURE; no OS input or flight acceptance")))Finish(TEXT("FAIL"),TEXT("Initial evidence write failed"));
}
void AHCM5VS2WingMaterialReviewDirector::BindViewport()
{
    auto* Live=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if(bStopped||!Live||(Live==Viewport.Get()&&InputHandle.IsValid()))return;
    if(Viewport.IsValid()){Viewport->OnInputKey().Remove(InputHandle);Viewport->OnEndDraw().Remove(DrawHandle);}
    Viewport=Live;InputHandle=Live->OnInputKey().AddUObject(this,&AHCM5VS2WingMaterialReviewDirector::Input);
    DrawHandle=Live->OnEndDraw().AddUObject(this,&AHCM5VS2WingMaterialReviewDirector::Drawn);
}
bool AHCM5VS2WingMaterialReviewDirector::Start()
{
    PC=Cast<AHCM1PlayerController>(GetWorld()->GetFirstPlayerController());Hero=PC?Cast<AHCM1Character>(PC->GetPawn()):nullptr;
    if(!PC||!Hero||!Viewport.IsValid()||!Viewport->Viewport||!PC->IsGameplayFocused())return false;
    bHadFocus=true;
    if(Viewport->Viewport->GetSizeXY()!=FIntPoint(1920,1080)||!ReferenceHeroClass||!CandidateHeroClass||Hero->GetClass()!=CandidateHeroClass.Get()
        ||!ReferenceMaterial||!CandidateMaterial||PC->IsFirstPersonPerspective()||PC->GetPlayerMode()!=EHCPlayerMode::OnFoot)
    {Finish(TEXT("FAIL"),TEXT("Candidate class, first-person state, source materials or viewport invalid"));return false;}
    Flight=Hero->GetFlightComponent();Visual=Hero->FindComponentByClass<UHCM5VS2FlightVisualComponent>();
    if(!Flight||!Visual||!Flight->IsFlightAvailable()||Visual->LightMaterial!=ReferenceMaterial||Visual->WingMaterial!=CandidateMaterial)
    {Finish(TEXT("FAIL"),TEXT("Actual bound wing component does not match the two authored source BPs"));return false;}
    StartZ=Hero->GetActorLocation().Z;
    if(!Flight->TryToggleFlight()){Finish(TEXT("FAIL"),TEXT("Public TryToggleFlight function rejected fixture takeoff"));return false;}
    Flight->SetAscendHeld(true);Stage=1;StageStarted=FPlatformTime::Seconds();
    MaterialA=UMaterialInstanceDynamic::Create(ReferenceMaterial,this);MaterialB=UMaterialInstanceDynamic::Create(CandidateMaterial,this);
    AddTickPrerequisiteComponent(Visual);Transitions.Add(Snapshot());return true;
}
bool AHCM5VS2WingMaterialReviewDirector::Freeze()
{
    Feathers.Reset();TArray<UStaticMeshComponent*> Parts;Hero->GetComponents(Parts);
    for(auto* P:Parts)if(P->GetName().StartsWith(TEXT("VS2LightFeather")))Feathers.Add(P);
    Feathers.Sort([](const UStaticMeshComponent& A,const UStaticMeshComponent& B){return A.GetName()<B.GetName();});
    if(Feathers.Num()!=20)return false;
    FrozenFeathers.Reset();
    for(int32 I=0;I<20;++I)
    {
        const auto* P=Feathers[I].Get();
        if(P->GetName()!=FString::Printf(TEXT("VS2LightFeather%02d"),I)||P->GetStaticMesh()!=Visual->FeatherMesh||!P->IsVisible()||P->bHiddenInGame||P->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)return false;
        FrozenFeathers.Add(P->GetComponentTransform());
    }
    FrozenPose=Hero->GetMesh()->GetComponentSpaceTransforms();FrozenHero=Hero->GetActorTransform();
    FrozenComponents.Reset();FrozenTickStates.Reset();TArray<UActorComponent*> Components;Hero->GetComponents(Components);
    for(auto* C:Components){FrozenComponents.Add(C);FrozenTickStates.Add(C->IsComponentTickEnabled());C->SetComponentTickEnabled(false);}
    bSavedActorTick=Hero->IsActorTickEnabled();Hero->SetActorTickEnabled(false);bFrozen=true;
    if(!Camera)
    {
        const FVector Anchor=Hero->GetMesh()->GetSocketLocation(TEXT("Chest"));
        const FQuat Facing(FRotator(0,Hero->GetActorRotation().Yaw,0));
        CameraLocation=Anchor+Facing.RotateVector(FVector(-520,0,40));CameraRotation=(Anchor+FVector(0,0,20)-CameraLocation).Rotation();
        Camera=GetWorld()->SpawnActor<ACameraActor>(CameraLocation,CameraRotation);
        if(!Camera)return false;Camera->GetCameraComponent()->SetFieldOfView(55);PC->SetViewTarget(Camera);
    }
    return true;
}
void AHCM5VS2WingMaterialReviewDirector::Unfreeze()
{
    if(!bFrozen||bStopped)return;
    for(int32 I=0;I<FrozenComponents.Num();++I)if(FrozenComponents[I].IsValid())FrozenComponents[I]->SetComponentTickEnabled(FrozenTickStates[I]);
    if(Hero)Hero->SetActorTickEnabled(bSavedActorTick);bFrozen=false;
}
bool AHCM5VS2WingMaterialReviewDirector::Invariants() const
{
    if(!bFrozen||!Hero||!PC||!Camera||!Viewport.IsValid()||!Viewport->Viewport||Viewport->Viewport->GetSizeXY()!=FIntPoint(1920,1080)
        ||PC->GetViewTarget()!=Camera||!Hero->GetActorTransform().Equals(FrozenHero,1.e-5)||!PC->PlayerCameraManager)return false;
    if(!PC->PlayerCameraManager->GetCameraLocation().Equals(CameraLocation,.02)||!PC->PlayerCameraManager->GetCameraRotation().Equals(CameraRotation,.005))return false;
    const auto& Pose=Hero->GetMesh()->GetComponentSpaceTransforms();if(Pose.Num()!=FrozenPose.Num())return false;
    for(int32 I=0;I<Pose.Num();++I)if(!Pose[I].Equals(FrozenPose[I],1.e-5))return false;
    for(int32 I=0;I<20;++I)if(!Feathers[I]||!Feathers[I]->GetComponentTransform().Equals(FrozenFeathers[I],1.e-5)
        ||Feathers[I]->GetMaterial(0)!=(ShotIndex%2?MaterialB.Get():MaterialA.Get()))return false;
    return Flight->IsFlying()&&Flight->IsBoosting()==(ShotIndex>=2);
}
bool AHCM5VS2WingMaterialReviewDirector::ApplyShot()
{
    const float Gain=ShotIndex>=2?3.5f:2.3f;
    // Both variants use the actual runtime scalar values, only their parent differs.
    TSharedPtr<FJsonObject> VisualReadback;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Visual->GetFlightVisualDiagnostics()),VisualReadback);
    if(!VisualReadback)return false;
    const float Opacity=float(VisualReadback->GetNumberField(TEXT("wing_alpha")))*.82f;
    if(Opacity<.80f)return false;
    for(auto* M:{MaterialA.Get(),MaterialB.Get()}){M->SetScalarParameterValue(TEXT("GlowGain"),Gain);M->SetScalarParameterValue(TEXT("OpacityGain"),Opacity);}
    for(auto& P:Feathers)P->SetMaterial(0,ShotIndex%2?MaterialB.Get():MaterialA.Get());
    bShaderReady=false;PendingReadiness.Reset();Readiness.Reset();MaterialReadyDraw=0;ShotStarted=FPlatformTime::Seconds();Stage=3;return true;
}
bool AHCM5VS2WingMaterialReviewDirector::Ready()
{
    ShaderJobs=0;
#if WITH_EDITOR
    if(GShaderCompilingManager)ShaderJobs=GShaderCompilingManager->GetNumRemainingJobs();
#endif
    StreamingWanted=IStreamingManager::Get().GetNumWantingResources();
    if(ShaderJobs||StreamingWanted)return false;
    if(PendingReadiness)
    {
        if(!PendingReadiness->Complete.load(std::memory_order_acquire))return false;
        bShaderReady=Readiness->GetBoolField(TEXT("game_thread_ready"))&&PendingReadiness->Ready;
        Readiness->SetBoolField(TEXT("render_thread_ready_without_fallback"),PendingReadiness->Ready);
        Readiness->SetStringField(TEXT("render_resource"),PendingReadiness->Actual);
        Readiness->SetStringField(TEXT("status"),bShaderReady?TEXT("READY"):TEXT("NOT_READY"));PendingReadiness.Reset();
        if(bShaderReady){MaterialReadyDraw=DrawCount;return false;}
    }
    if(bShaderReady)return DrawCount>=MaterialReadyDraw+2;
    auto* M=ShotIndex%2?MaterialB.Get():MaterialA.Get();const auto FeatureLevel=GetWorld()->GetFeatureLevel();
    auto* Resource=M?M->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(FeatureLevel)):nullptr;
    bool Compiling=M&&M->IsCompiling(),Failed=false;
#if WITH_EDITOR
    if(Resource){Compiling|=!Resource->IsCompilationFinished();Failed=!Resource->GetCompileErrors().IsEmpty();}
#endif
    if(Failed){Finish(TEXT("FAIL"),TEXT("Wing material has actual shader compile errors"));return false;}
    const bool Complete=Resource&&Resource->IsGameThreadShaderMapComplete();
#if WITH_EDITOR
    if(Resource&&!Complete&&!Failed)Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
#endif
    // IsCompilationFinished may finalize a cache request and replace its map.
    // Read the live pointer only after those game-thread compilation operations.
    auto* Map=Resource?Resource->GetGameThreadShaderMap():nullptr;
    const bool Good=Resource&&!Compiling&&Complete&&Map&&Map->IsValidForRendering();
    Readiness=MakeShared<FJsonObject>();Readiness->SetBoolField(TEXT("game_thread_ready"),Good);
    Readiness->SetStringField(TEXT("material"),GetPathNameSafe(M));
    auto State=MakeShared<FHCM5VS2WingMaterialReadiness,ESPMode::ThreadSafe>();State->Proxy=M?M->GetRenderProxy():nullptr;PendingReadiness=State;
    ENQUEUE_RENDER_COMMAND(HCReadWingMaterial)([State,FeatureLevel](FRHICommandListImmediate& RHICmdList)
    {
        if(State->Proxy){const FMaterial* Direct=State->Proxy->GetMaterialNoFallback(FeatureLevel);const auto* ShaderMap=Direct?Direct->GetRenderingThreadShaderMap():nullptr;
            const FMaterialRenderProxy* Fallback=nullptr;const FMaterial& Actual=State->Proxy->GetMaterialWithFallback(FeatureLevel,Fallback);
            State->Actual=Actual.GetFriendlyName();State->Ready=Direct&&Direct->IsRenderingThreadShaderMapComplete()&&ShaderMap&&ShaderMap->IsValidForRendering()&&!Fallback;}
        State->Complete.store(true,std::memory_order_release);
    });return false;
}
void AHCM5VS2WingMaterialReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(bStopped)return;BindViewport();const double Now=FPlatformTime::Seconds();
    if(bExitPending){if(Now-Finished>1){bExitPending=false;FPlatformMisc::RequestExitWithStatus(false,bReportFailure||FinalStatus!=TEXT("PASS")?1:0,TEXT("Wing visual fixture complete"));}return;}
    if(!bActive)return;
    if(PC&&PC->IsGameplayFocused())bHadFocus=true;
    if(PC&&(PC->IsPauseMenuOpen()||(bHadFocus&&!PC->IsGameplayFocused()))){Finish(TEXT("NOT_RUN"),TEXT("Pause or lost gameplay focus permanently stopped visual fixture"),true);return;}
    if(Now-Started>180){Finish(TEXT("FAIL"),TEXT("Bounded fixture timeout"));return;}
    if(Stage==0){if(Now-Started>2)Start();return;}
    if(Stage==1)
    {
        if(Hero->GetActorLocation().Z-StartZ>=500){Flight->SetAscendHeld(false);Flight->ClearFlightInput();Hero->GetCharacterMovement()->StopMovementImmediately();
            Flight->SetComponentTickEnabled(false);Hero->GetCharacterMovement()->SetComponentTickEnabled(false);Stage=2;StageStarted=Now;Transitions.Add(Snapshot());}
        else if(Now-StageStarted>8)Finish(TEXT("FAIL"),TEXT("Public ascent did not reach bounded 5m fixture height"));return;
    }
    if(Stage==2){if(Now-StageStarted>=1.3){if(!Freeze()||!ApplyShot())Finish(TEXT("FAIL"),TEXT("Exact 20 feathers/full-body pose could not be frozen"));}return;}
    if(Stage==3){if(Ready()&&Invariants()&&!FScreenshotRequest::IsScreenshotRequested())Capture();return;}
    if(Stage==4)
    {
        if(bProcessed&&!FScreenshotRequest::IsScreenshotRequested()&&GFrameCounter>RequestFrame)
        {
            const bool Good=PNG(PendingPNG)&&Invariants();PendingShot->SetStringField(TEXT("status"),Good?TEXT("PASS"):TEXT("FAIL"));
            PendingShot->SetNumberField(TEXT("file_bytes"),double(IFileManager::Get().FileSize(*PendingPNG)));PendingShot->SetBoolField(TEXT("same_pose_after_save"),Invariants());
            Captures.Add(PendingShot);PendingShot.Reset();if(!Good){Finish(TEXT("FAIL"),TEXT("Original PNG or same-pose/camera/material guard failed"));return;}
            ++ShotIndex;if(ShotIndex==4){Finish(TEXT("PASS"),TEXT("Four original same-pose A/B material fixture PNGs; art USER_REVIEW, not input or full flight acceptance"));return;}
            if(ShotIndex==2){Unfreeze();Flight->SetBoostHeld(true);Stage=2;StageStarted=Now;Transitions.Add(Snapshot());}
            else if(!ApplyShot())Finish(TEXT("FAIL"),TEXT("Second material switch failed"));
        }
        else if(Now-Requested>15)Finish(TEXT("FAIL"),TEXT("Screenshot processing timeout"));
    }
}
void AHCM5VS2WingMaterialReviewDirector::Capture()
{
    PendingPNG=Directory/FString::Printf(TEXT("%02d_%s_%s.png"),ShotIndex,ShotIndex>=2?TEXT("BoostGain"):TEXT("DefaultGain"),ShotIndex%2?TEXT("B_SoftLayers"):TEXT("A_Additive"));
    if(IFileManager::Get().FileExists(*PendingPNG)){Finish(TEXT("FAIL"),TEXT("Refuse overwrite"));return;}
    PendingShot=Snapshot();PendingShot->SetStringField(TEXT("file"),PendingPNG);PendingShot->SetStringField(TEXT("status"),TEXT("REQUESTED"));
    PendingShot->SetObjectField(TEXT("shader_readiness"),Readiness);PendingShot->SetNumberField(TEXT("width"),1920);PendingShot->SetNumberField(TEXT("height"),1080);
    PendingShot->SetNumberField(TEXT("draws_after_material_ready"),double(DrawCount-MaterialReadyDraw));
    RequestFrame=GFrameCounter;Requested=FPlatformTime::Seconds();bProcessed=false;Stage=4;
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}
void AHCM5VS2WingMaterialReviewDirector::Drawn(){if(bActive&&!bStopped)++DrawCount;}
void AHCM5VS2WingMaterialReviewDirector::Processed(){if(bActive&&!bStopped&&Stage==4)bProcessed=true;}
void AHCM5VS2WingMaterialReviewDirector::Input(const FInputKeyEventArgs& E)
{if(!bStopped&&(bActive||bExitPending)&&E.Event==IE_Pressed&&(E.Key==EKeys::Escape||E.Key==EKeys::P))Finish(TEXT("NOT_RUN"),TEXT("Observed real Escape/P; no further fixture/capture/restore/auto-exit actions"),true);}
TSharedPtr<FJsonObject> AHCM5VS2WingMaterialReviewDirector::Snapshot() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("shot_index"),ShotIndex);J->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());
    J->SetStringField(TEXT("variant"),ShotIndex%2?TEXT("B_SoftLayers"):TEXT("A_Additive"));J->SetNumberField(TEXT("glow_gain"),ShotIndex>=2?3.5:2.3);
    J->SetBoolField(TEXT("frozen"),bFrozen);J->SetBoolField(TEXT("same_pose_camera_material"),Invariants());J->SetNumberField(TEXT("feather_count"),Feathers.Num());
    J->SetArrayField(TEXT("camera_location_cm"),Vec(CameraLocation));J->SetArrayField(TEXT("camera_pitch_yaw_roll"),Vec(FVector(CameraRotation.Pitch,CameraRotation.Yaw,CameraRotation.Roll)));
    if(Hero){J->SetStringField(TEXT("actual_hero_class"),Hero->GetClass()->GetPathName());J->SetArrayField(TEXT("hero_location_cm"),Vec(Hero->GetActorLocation()));
        J->SetArrayField(TEXT("chest_world_cm"),Vec(Hero->GetMesh()->GetSocketLocation(TEXT("Chest"))));J->SetArrayField(TEXT("head_world_cm"),Vec(Hero->GetMesh()->GetSocketLocation(TEXT("Head"))));}
    if(Flight)J->SetStringField(TEXT("actual_flight_diagnostics"),Flight->GetFlightDiagnostics());if(Visual)J->SetStringField(TEXT("actual_visual_diagnostics"),Visual->GetFlightVisualDiagnostics());
    TArray<TSharedPtr<FJsonValue>> Pieces;
    for(const auto& P:Feathers){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),P->GetName());R->SetArrayField(TEXT("root_world_cm"),Vec(P->GetComponentLocation()));R->SetStringField(TEXT("transform"),P->GetComponentTransform().ToString());Pieces.Add(MakeShared<FJsonValueObject>(R));}
    J->SetArrayField(TEXT("actual_feathers"),Pieces);return J;
}
bool AHCM5VS2WingMaterialReviewDirector::Write(const FString& Status,const FString& Detail)
{
    if(Directory.IsEmpty())return false;auto J=MakeShared<FJsonObject>();
    J->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.WingMaterialReview.Runtime.v1"));J->SetStringField(TEXT("status"),Status);J->SetStringField(TEXT("detail"),Detail);
    J->SetStringField(TEXT("scope"),TEXT("FUNCTION_CALL_VISUAL_FIXTURE; same actual 20 runtime feathers, two actual Blueprint-sourced materials. Body/feather pose frozen per A/B pair. Boost flag/emission only at frozen position. No OS input or flight acceptance."));
    J->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName());J->SetStringField(TEXT("source_blueprint_class"),GetPathNameSafe(ReferenceHeroClass.Get()));J->SetStringField(TEXT("candidate_blueprint_class"),GetPathNameSafe(CandidateHeroClass.Get()));
    J->SetStringField(TEXT("material_a"),GetPathNameSafe(ReferenceMaterial));J->SetStringField(TEXT("material_b"),GetPathNameSafe(CandidateMaterial));
    J->SetBoolField(TEXT("user_stop_latched"),bStopped);J->SetNumberField(TEXT("stop_frame"),double(StopFrame));J->SetBoolField(TEXT("os_input_used"),false);J->SetStringField(TEXT("art"),TEXT("USER_REVIEW"));
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& C:Captures)Rows.Add(MakeShared<FJsonValueObject>(C));J->SetArrayField(TEXT("captures"),Rows);
    Rows.Reset();for(const auto& T:Transitions)Rows.Add(MakeShared<FJsonValueObject>(T));J->SetArrayField(TEXT("transitions"),Rows);
    J->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);J->SetNumberField(TEXT("shader_jobs"),ShaderJobs);J->SetNumberField(TEXT("streaming_wanted"),StreamingWanted);
    FString Text;const FString File=Directory/TEXT("wing_material_review.json"),Temp=File+TEXT(".")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp");
    return FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        &&IFileManager::Get().Move(*File,*Temp,true,true,false,true);
}
void AHCM5VS2WingMaterialReviewDirector::Finish(const FString& Status,const FString& Detail,bool Stop)
{
    if(bStopped)return;if(Stop){bStopped=true;StopFrame=GFrameCounter;bAutoQuit=false;}
    if(Stage==4&&FScreenshotRequest::IsScreenshotRequested()&&FScreenshotRequest::GetFilename()==PendingPNG)FScreenshotRequest::Reset();
    bActive=false;FinalStatus=Status;Finished=FPlatformTime::Seconds();bExitPending=bAutoQuit&&!bStopped;
    if(!Write(Status,Detail)){bReportFailure=true;FinalStatus=TEXT("FAIL");UE_LOG(LogTemp,Error,TEXT("M5VS2_WING_MATERIAL_FAIL evidence write failed"));}
    else UE_LOG(LogTemp,Display,TEXT("M5VS2_WING_MATERIAL_%s %s"),*Status,*Detail);
}
void AHCM5VS2WingMaterialReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bActive&&!bStopped)Finish(TEXT("NOT_RUN"),TEXT("World ended before four images"));
    if(Viewport.IsValid()){Viewport->OnInputKey().Remove(InputHandle);Viewport->OnEndDraw().Remove(DrawHandle);}
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    // Keep both transient MIDs alive until the one pending readback no longer uses their proxies.
    if(PendingReadiness)FlushRenderingCommands();PendingReadiness.Reset();Super::EndPlay(Reason);
}
