#include "HCM5VS2HeroModestyReviewDirector.h"
#include "HCM5VS2HeroModestyComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
namespace
{
const TCHAR* ViewName(int32 I){const TCHAR* N[]={TEXT("Front"),TEXT("Rear"),TEXT("Left"),TEXT("Right")};return I>=0&&I<4?N[I]:TEXT("Warmup");}
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V){return{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
TArray<TSharedPtr<FJsonValue>> Rows(const TArray<TSharedPtr<FJsonObject>>& O){TArray<TSharedPtr<FJsonValue>> R;for(const auto& X:O)R.Add(MakeShared<FJsonValueObject>(X));return R;}
TSharedPtr<FJsonObject> Parsed(const FString& S){TSharedPtr<FJsonObject> R;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),R);return R?R:MakeShared<FJsonObject>();}
bool PNG(const FString& File)
{
    TArray<uint8> B;if(!FFileHelper::LoadFileToArray(B,*File)||B.Num()<24)return false;
    const uint8 Magic[]={137,80,78,71,13,10,26,10};for(int32 I=0;I<8;++I)if(B[I]!=Magic[I])return false;
    auto BE=[&](int32 I){return(uint32(B[I])<<24)|(uint32(B[I+1])<<16)|(uint32(B[I+2])<<8)|uint32(B[I+3]);};return BE(16)==1920&&BE(20)==1080;
}
}
AHCM5VS2HeroModestyReviewDirector::AHCM5VS2HeroModestyReviewDirector()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.bTickEvenWhenPaused=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AHCM5VS2HeroModestyReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if(!FParse::Param(FCommandLine::Get(),TEXT("M5VS2HeroModestyReview")))return;
    const FString Map=GetWorld()->GetOutermost()->GetName(),Prefix=TEXT("/Game/HarborCity/M5VS2/HeroModestyReview/Run_");
    if(!Map.StartsWith(Prefix)||Map!=Prefix+Map.Mid(Prefix.Len(),12)+TEXT("/L_HeroModestyReview"))return;
    for(TCHAR C:Map.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return;
    FString Root;if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root)||FPaths::IsRelative(Root))return;
    Root=FPaths::ConvertRelativePathToFull(Root);FPaths::NormalizeDirectoryName(Root);
    if(!FPaths::CollapseRelativeDirectories(Root)||!FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2")))return;
    Directory=Root/(TEXT("HeroModestyReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if(!IFileManager::Get().MakeDirectory(*Directory,true))return;
    bEnabled=true;StartedWall=FPlatformTime::Seconds();bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));SetActorTickEnabled(true);
    if(auto* V=GetWorld()->GetGameViewport()){Viewport=V;InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2HeroModestyReviewDirector::ObserveInput);}
    if(FSlateApplication::IsInitialized())ActivationHandle=FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&AHCM5VS2HeroModestyReviewDirector::ObserveActivation);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2HeroModestyReviewDirector::ScreenshotProcessed);Write();
#endif
}
bool AHCM5VS2HeroModestyReviewDirector::Initialize()
{
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));Character=PC?Cast<AHCM1Character>(PC->GetPawn()):nullptr;
    if(!PC||!Character||!Viewport.IsValid()||!Viewport->Viewport||!PC->IsGameplayFocused()||PC->IsPauseMenuOpen())return false;
    bHadFocus=true;if(!Character->GetCharacterMovement()->IsMovingOnGround())return false;
    Modesty=Character->FindComponentByClass<UHCM5VS2HeroModestyComponent>();
    TArray<USkeletalMeshComponent*> Meshes;Character->GetComponents(Meshes);int32 Matches=0;
    for(auto* M:Meshes)if(M->GetName()==TEXT("VS2SafetyShortsMesh")){Garment=M;++Matches;}
    if(Matches!=1||!Modesty||!BindingValid()||LowCameras.Num()!=4||!PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_VS2_Modesty_"))||UGameplayStatics::DoesSaveGameExist(PC->GetSaveSlotName(),0))
    {Finish(TEXT("FAIL"),TEXT("Exact fresh private candidate, unchanged body/animation/materials, one opaque garment and four cameras required"));return false;}
    auto* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);const UInputAction* Jump=PC->GetM1Action(TEXT("Jump"));int32 Mappings=0;bool Valid=Input&&Jump&&Jump->ValueType==EInputActionValueType::Boolean;
    if(Input)for(const FEnhancedActionKeyMapping& M:Input->GetEnhancedActionMappingsView())if(M.Action==Jump){++Mappings;Valid&=M.Key==EKeys::SpaceBar;}
    if(!Valid||Mappings!=1){Finish(TEXT("FAIL"),TEXT("One actual Space -> Jump Action mapping required"));return false;}
    Origin=Character->GetActorLocation();for(const auto& Item:LowCameras){auto* C=Item.Get();if(!C){Finish(TEXT("FAIL"),TEXT("Missing fixed camera"));return false;}CameraTransforms.Add(C->GetActorTransform());}
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());AddTickPrerequisiteComponent(Character->GetMesh());AddTickPrerequisiteComponent(Modesty);
    PC->ShowStatusMessage(TEXT("固定低机位服装检查 · 引擎 Space 跳跃，非鼠标测试；Esc / P 停止"),6.f);return true;
}
bool AHCM5VS2HeroModestyReviewDirector::BindingValid() const
{
    if(!Character||!PC||PC->GetPawn()!=Character||Character->GetClass()!=ExpectedCharacterClass.Get()||PC->IsFirstPersonPerspective())return false;
    auto* Body=Character->GetMesh();if(!Body||Body->GetSkeletalMeshAsset()!=ExpectedBody||!Body->GetAnimInstance()||Body->GetAnimInstance()->GetClass()!=ExpectedAnimationClass.Get()||Body->GetNumMaterials()!=ExpectedBodyMaterials.Num())return false;
    for(int32 I=0;I<ExpectedBodyMaterials.Num();++I)if(Body->GetMaterial(I)!=ExpectedBodyMaterials[I])return false;
    return Garment&&Modesty&&Garment->GetSkeletalMeshAsset()==ExpectedShorts&&Garment->GetNumMaterials()==1&&Garment->GetMaterial(0)==ExpectedCloth
        &&ExpectedCloth&&ExpectedCloth->GetBlendMode()==BLEND_Opaque&&Garment->LeaderPoseComponent.Get()==Body
        &&ExpectedShorts&&ExpectedShorts->GetSkeleton()==ExpectedBody->GetSkeleton()&&Garment->GetRelativeTransform().Equals(FTransform::Identity,.0001)
        &&Garment->IsVisible()&&!Garment->bHiddenInGame&&Garment->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
}
bool AHCM5VS2HeroModestyReviewDirector::ShadersReady(FString& Failure)
{
    ShaderState=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonObject>> Slots;bool Ready=true;
    const EShaderPlatform Platform=GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel());
    for(auto* Mesh:{Character->GetMesh(),Garment.Get()})for(int32 I=0;I<Mesh->GetNumMaterials();++I)
    {
        auto* M=Mesh->GetMaterial(I);auto* R=M?M->GetMaterialResource(Platform):nullptr;bool Compiling=M&&M->IsCompiling();TArray<TSharedPtr<FJsonValue>> Errors;
#if WITH_EDITOR
        if(R){Compiling|=!R->IsCompilationFinished();for(const auto& E:R->GetCompileErrors())if(Errors.Num()<8)Errors.Add(MakeShared<FJsonValueString>(E.Left(2048)));if(!R->IsGameThreadShaderMapComplete()&&Errors.IsEmpty())R->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);}
#endif
        const auto* Map=R?R->GetGameThreadShaderMap():nullptr;const bool NeedMorph=Mesh==Character->GetMesh();
        const bool Usage=M&&M->GetUsageByFlag(MATUSAGE_SkeletalMesh)&&(!NeedMorph||M->GetUsageByFlag(MATUSAGE_MorphTargets));
        const bool Failed=!Errors.IsEmpty()||(Map&&Map->IsCompilationFinalized()&&!Map->CompiledSuccessfully());
        const bool Good=Usage&&R&&!Compiling&&!Failed&&Map&&Map->IsValidForRendering()&&R->IsGameThreadShaderMapComplete();Ready&=Good;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("component"),Mesh->GetName());Row->SetNumberField(TEXT("slot"),I);Row->SetStringField(TEXT("material"),GetPathNameSafe(M));Row->SetBoolField(TEXT("needs_morph_usage"),NeedMorph);Row->SetBoolField(TEXT("usage_valid"),Usage);Row->SetBoolField(TEXT("game_thread_ready"),Good);Row->SetArrayField(TEXT("errors"),Errors);Slots.Add(Row);
        if(!Usage||(Failed&&!Compiling))Failure=TEXT("Actual body/shorts material usage or compilation invalid");
    }
    ShaderState->SetArrayField(TEXT("slots"),Rows(Slots));ShaderState->SetStringField(TEXT("scope"),TEXT("Public game-thread material readiness; not GPU surface occlusion/fallback proof"));return Ready;
}
void AHCM5VS2HeroModestyReviewDirector::SetView(int32 Index)
{
    ViewIndex=Index;Stage=1;StageStartedWorld=GetWorld()->GetTimeSeconds();PC->SetViewTarget(LowCameras[Index]);Write();
}
bool AHCM5VS2HeroModestyReviewDirector::FinalViewValid() const
{
    if(!PC||!LowCameras.IsValidIndex(ViewIndex)||!PC->PlayerCameraManager)return false;const auto* C=LowCameras[ViewIndex].Get();const auto* PCM=PC->PlayerCameraManager.Get();
    return PC->GetViewTarget()==C&&C->GetActorTransform().Equals(CameraTransforms[ViewIndex],.0001)
        &&PCM->GetCameraLocation().Equals(C->GetActorLocation(),.1)&&PCM->GetCameraRotation().Equals(C->GetActorRotation(),.05)
        &&FMath::IsNearlyEqual(PCM->GetFOVAngle(),C->GetCameraComponent()->FieldOfView,.05f);
}
void AHCM5VS2HeroModestyReviewDirector::JumpKey(bool Pressed)
{
    if(!PC||bJumpHeld==Pressed||(Pressed&&(bDone||bStopped)))return;
    PC->InputKey(FInputKeyEventArgs(nullptr,IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(),EKeys::SpaceBar,Pressed?IE_Pressed:IE_Released,0u));bJumpHeld=Pressed;
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("key"),TEXT("SpaceBar"));Row->SetBoolField(TEXT("pressed"),Pressed);Row->SetNumberField(TEXT("frame"),double(GFrameCounter));Row->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());Row->SetStringField(TEXT("view"),ViewName(ViewIndex));KeyEvents.Add(Row);
}
TSharedPtr<FJsonObject> AHCM5VS2HeroModestyReviewDirector::Observe() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("frame"),double(GFrameCounter));J->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());J->SetStringField(TEXT("view"),ViewName(ViewIndex));if(!Character||!PC)return J;
    auto* Body=Character->GetMesh();J->SetStringField(TEXT("character_class"),GetPathNameSafe(Character->GetClass()));J->SetArrayField(TEXT("character_position_cm"),XYZ(Character->GetActorLocation()));J->SetArrayField(TEXT("velocity_cm_s"),XYZ(Character->GetVelocity()));J->SetStringField(TEXT("actor_rotation"),Character->GetActorRotation().ToString());J->SetStringField(TEXT("body_transform"),Body->GetComponentTransform().ToString());J->SetStringField(TEXT("body_relative_transform"),Body->GetRelativeTransform().ToString());J->SetBoolField(TEXT("falling"),Character->GetCharacterMovement()->IsFalling());J->SetBoolField(TEXT("grounded"),Character->GetCharacterMovement()->IsMovingOnGround());
    J->SetStringField(TEXT("body_mesh"),GetPathNameSafe(Body->GetSkeletalMeshAsset()));J->SetStringField(TEXT("animation_class"),Body->GetAnimInstance()?GetPathNameSafe(Body->GetAnimInstance()->GetClass()):TEXT("None"));
    TArray<TSharedPtr<FJsonValue>> Materials;for(int32 I=0;I<Body->GetNumMaterials();++I)Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Body->GetMaterial(I))));J->SetArrayField(TEXT("unchanged_body_materials"),Materials);
    if(Modesty)J->SetObjectField(TEXT("garment_binding"),Parsed(Modesty->GetModestyDiagnostics()));
    FVector View;FRotator Rotation;PC->GetPlayerViewPoint(View,Rotation);J->SetArrayField(TEXT("final_view_cm"),XYZ(View));J->SetStringField(TEXT("final_view_rotation"),Rotation.ToString());J->SetStringField(TEXT("view_target"),GetPathNameSafe(PC->GetViewTarget()));J->SetBoolField(TEXT("fixed_authored_camera_and_PCM_match"),FinalViewValid());
    TArray<TSharedPtr<FJsonObject>> Bones;double MaxError=0;bool Found=true;
    for(FName Bone:{FName(TEXT("Hips")),FName(TEXT("UpperLeg_L")),FName(TEXT("UpperLeg_R")),FName(TEXT("LowerLeg_L")),FName(TEXT("LowerLeg_R")),FName(TEXT("Skirt_03_L")),FName(TEXT("Skirt_04_R"))})
    {
        auto B=MakeShared<FJsonObject>();const bool Present=Garment&&Body->GetBoneIndex(Bone)!=INDEX_NONE&&Garment->GetBoneIndex(Bone)!=INDEX_NONE;Found&=Present;B->SetStringField(TEXT("bone"),Bone.ToString());B->SetBoolField(TEXT("present_in_body_and_garment"),Present);
        if(Present){const FTransform A=Body->GetSocketTransform(Bone,RTS_World),G=Garment->GetSocketTransform(Bone,RTS_World);const double Error=FVector::Distance(A.GetLocation(),G.GetLocation());MaxError=FMath::Max(MaxError,Error);B->SetArrayField(TEXT("body_world_cm"),XYZ(A.GetLocation()));B->SetArrayField(TEXT("garment_leader_world_cm"),XYZ(G.GetLocation()));B->SetNumberField(TEXT("position_error_cm"),Error);B->SetNumberField(TEXT("rotation_error_degrees"),FMath::RadiansToDegrees(A.GetRotation().AngularDistance(G.GetRotation())));}Bones.Add(B);
    }
    J->SetArrayField(TEXT("bone_follow_samples"),Rows(Bones));J->SetBoolField(TEXT("all_observed_bones_present"),Found);J->SetNumberField(TEXT("max_observed_bone_follow_error_cm"),MaxError);
    J->SetStringField(TEXT("geometry_boundary"),TEXT("Actual leader/bone readback only, no CPU/GPU surface intersection test. PNGs require garment/body/stocking/ribbon/skirt review; four viewpoints do not prove all angles."));
    return J;
}
void AHCM5VS2HeroModestyReviewDirector::Capture(bool Jump)
{
    if(!FinalViewValid()||!BindingValid()){Finish(TEXT("FAIL"),TEXT("Actual camera or garment binding changed before capture"));return;}
    const bool Grounded=Character->GetCharacterMovement()->IsMovingOnGround(),Falling=Character->GetCharacterMovement()->IsFalling();
    if((Jump&&(!Falling||Character->GetActorLocation().Z-Origin.Z<10))||(!Jump&&(!Grounded||Character->GetVelocity().Size()>1)))
    {Finish(TEXT("FAIL"),TEXT("Requested standing/airborne pose not observed"));return;}
    Pending=Observe();if(!Pending->GetBoolField(TEXT("all_observed_bones_present"))||Pending->GetNumberField(TEXT("max_observed_bone_follow_error_cm"))>.05){Pending.Reset();Finish(TEXT("FAIL"),TEXT("Garment not following the actual current body bones"));return;}
    PendingPNG=Directory/FString::Printf(TEXT("%02d_%s_%s.png"),Captures.Num(),ViewName(ViewIndex),Jump?TEXT("RealJump"):TEXT("Standing"));
    if(IFileManager::Get().FileExists(*PendingPNG)){Pending.Reset();Finish(TEXT("FAIL"),TEXT("Refuse PNG overwrite"));return;}
    Pending->SetStringField(TEXT("file"),PendingPNG);Pending->SetStringField(TEXT("pose"),Jump?TEXT("ACTUAL_ENGINE_SPACE_JUMP"):TEXT("NATURAL_STANDING"));Pending->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));Pending->SetStringField(TEXT("frame_relation"),TEXT("Native pose read at screenshot request in PostUpdateWork; render/processed callback may occur later. No frozen pose or exact GPU-frame claim."));
    RequestFrame=GFrameCounter;ShotWall=FPlatformTime::Seconds();bShotProcessed=false;FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}
void AHCM5VS2HeroModestyReviewDirector::ScreenshotProcessed(){if(Pending&&!bStopped){bShotProcessed=true;Pending->SetNumberField(TEXT("processed_frame"),double(GFrameCounter));Pending->SetObjectField(TEXT("processed_callback_observation"),Observe());}}
void AHCM5VS2HeroModestyReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!bEnabled||bStopped)return;const double Now=FPlatformTime::Seconds();
    if(bDone){if(bAutoQuit&&Now-FinishedWall>.5&&!FScreenshotRequest::IsScreenshotRequested()){bAutoQuit=false;FPlatformMisc::RequestExitWithStatus(false,!bWriteFailed&&Status==TEXT("PASS_CAPTURE_ONLY")?0:1,TEXT("Private modesty capture finished"));}return;}
    if(Now-StartedWall>150){Finish(TEXT("FAIL"),TEXT("150 second total bound"));return;}
    if(!bInitialized){bInitialized=Initialize();if(!bInitialized&&!bDone&&Now-StartedWall>15)Finish(TEXT("NOT_RUN"),TEXT("No focused valid spawned pawn in startup bound"));return;}
    if(!PC->IsGameplayFocused()||PC->IsPauseMenuOpen()||UGameplayStatics::IsGamePaused(this)){Finish(TEXT("USER_ABORTED"),TEXT("Pause or focus loss; no auto resume"),true);return;}
    if(!BindingValid()||FVector::Dist2D(Origin,Character->GetActorLocation())>2||Character->GetActorLocation().ContainsNaN()){Finish(TEXT("FAIL"),TEXT("Body binding or stationary jump fixture changed"));return;}
    const double World=GetWorld()->GetTimeSeconds(),Age=World-StageStartedWorld;
    if(Stage>0&&World>=NextSampleWorld){NextSampleWorld=World+.1;if(Samples.Num()>=500){Finish(TEXT("FAIL"),TEXT("Sample bound"));return;}Samples.Add(Observe());}
    if(Pending)
    {
        if(bShotProcessed&&!FScreenshotRequest::IsScreenshotRequested()&&GFrameCounter>RequestFrame)
        {if(!PNG(PendingPNG)){Finish(TEXT("FAIL"),TEXT("Native 1920x1080 PNG missing"));return;}Pending->SetNumberField(TEXT("bytes"),IFileManager::Get().FileSize(*PendingPNG));Pending->SetStringField(TEXT("capture_status"),TEXT("PASS"));Captures.Add(Pending);Pending.Reset();if(!Write())return;}
        else if(Now-ShotWall>15){Finish(TEXT("FAIL"),TEXT("Screenshot deadline"));return;}
    }
    if(Stage==0)
    {
        if(Now>=NextShaderWall){NextShaderWall=Now+.5;FString Failure;const bool Ready=ShadersReady(Failure);if(!Failure.IsEmpty()){Finish(TEXT("FAIL"),Failure);return;}if(!Ready)ReadyFrame=0;else if(ReadyFrame==0)ReadyFrame=GFrameCounter;}
        if(Now-StartedWall>120){Finish(TEXT("FAIL"),TEXT("Material readiness deadline"));return;}
        if(ReadyFrame>0&&GFrameCounter-ReadyFrame>=30&&Character->GetVelocity().Size()<1){SetView(0);}return;
    }
    if(Stage==1&&Age>=1&&!Pending){Capture(false);if(bDone)return;Stage=2;return;}
    if(Stage==2&&!Pending){JumpKey(true);++JumpCount;Stage=3;StageStartedWorld=World;bAirborne=false;return;}
    if(Stage==3)
    {
        if(Age>=.12)JumpKey(false);bAirborne|=Character->GetCharacterMovement()->IsFalling();
        if(bAirborne&&Character->GetCharacterMovement()->IsFalling()&&Character->GetActorLocation().Z-Origin.Z>10&&Character->GetVelocity().Z<=160&&Age>=.18)
        {Capture(true);if(bDone)return;Stage=4;StageStartedWorld=World;return;}
        if(Age>2){Finish(TEXT("FAIL"),TEXT("Real Space jump did not reach required airborne capture"));return;}
    }
    if(Stage==4&&!Pending&&Character->GetCharacterMovement()->IsMovingOnGround()&&Character->GetVelocity().Size()<1&&Age>=1)
    {if(ViewIndex<3)SetView(ViewIndex+1);else Finish(Captures.Num()==8&&JumpCount==4?TEXT("PASS_CAPTURE_ONLY"):TEXT("FAIL"),TEXT("Four fixed low views with real standing/jump captured. Clothing coverage, intersections and aesthetics require original PNG review; flight and OS camera NOT_RUN."));}
    else if(Stage==4&&Age>5){Finish(TEXT("FAIL"),TEXT("Natural landing deadline"));}
}
void AHCM5VS2HeroModestyReviewDirector::ObserveInput(const FInputKeyEventArgs& E)
{if(bEnabled&&E.Event==IE_Pressed&&(E.Key==EKeys::Escape||E.Key==EKeys::P))Finish(TEXT("USER_ABORTED"),TEXT("Esc/P permanently cancels diagnostic input, camera changes and autoquit"),true);}
void AHCM5VS2HeroModestyReviewDirector::ObserveActivation(bool Active)
{if(bEnabled&&bHadFocus&&!Active)Finish(TEXT("USER_ABORTED"),TEXT("Application focus loss; no reacquire"),true);}
void AHCM5VS2HeroModestyReviewDirector::Finish(const FString& NewStatus,const FString& Why,bool UserStop)
{
    if(UserStop){bStopped=true;bAutoQuit=false;UE_LOG(LogTemp,Warning,TEXT("M5VS2_MODESTY_REVIEW_USER_STOP %s; no subsequent camera/capture/autoquit"),*Why);}
    if(bDone&&!UserStop)return;Status=NewStatus;Detail=Why;bDone=true;FinishedWall=FPlatformTime::Seconds();JumpKey(false);
    if(Pending&&FScreenshotRequest::IsScreenshotRequested()&&FScreenshotRequest::GetFilename()==PendingPNG)FScreenshotRequest::Reset();Write();
}
bool AHCM5VS2HeroModestyReviewDirector::Write()
{
    if(bWriteFailed)return false;auto Fail=[&](const TCHAR* Step,uint32 Error){bWriteFailed=true;Status=TEXT("FAIL");Detail=FString(TEXT("Evidence publication failed: "))+Step;bDone=true;FinishedWall=FPlatformTime::Seconds();JumpKey(false);UE_LOG(LogTemp,Error,TEXT("M5VS2_MODESTY_REVIEW_WRITE_FAIL %s error=%u; no retries"),Step,Error);return false;};
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.HeroModestyReview.v1"));J->SetStringField(TEXT("status"),Status);J->SetStringField(TEXT("detail"),Detail);J->SetStringField(TEXT("input_scope"),TEXT("ENGINE_SPACE_KEY_NOT_OS"));J->SetStringField(TEXT("camera_scope"),TEXT("FOUR_FIXED_DIAGNOSTIC_CAMERAS_NOT_PLAYER_MOUSE"));J->SetStringField(TEXT("art_coverage"),TEXT("USER_REVIEW_NOT_PROVEN_BY_CAPTURE_COUNT"));J->SetStringField(TEXT("flight_dive"),TEXT("NOT_RUN"));J->SetStringField(TEXT("packaged_runtime"),TEXT("NOT_RUN_EDITOR_GAME"));J->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName());J->SetStringField(TEXT("save_slot"),PC?PC->GetSaveSlotName():TEXT(""));J->SetBoolField(TEXT("stop_latched"),bStopped);J->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedWall);J->SetNumberField(TEXT("jump_key_press_count"),JumpCount);J->SetNumberField(TEXT("scripted_actor_pose_writes"),0);J->SetNumberField(TEXT("scripted_clothing_pose_writes"),0);J->SetArrayField(TEXT("captures"),Rows(Captures));J->SetArrayField(TEXT("samples"),Rows(Samples));J->SetArrayField(TEXT("engine_key_events"),Rows(KeyEvents));if(ShaderState)J->SetObjectField(TEXT("material_readiness"),ShaderState);
    FString Text;if(!FJsonSerializer::Serialize(J,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text)))return Fail(TEXT("serialize"),0);
    const FString Temp=Directory/(TEXT("modesty_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp")),Dest=Directory/TEXT("modesty_review.json");
    if(!FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return Fail(TEXT("temp_write"),FPlatformMisc::GetLastError());
#if PLATFORM_WINDOWS
    const FString From=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Temp).Replace(TEXT("/"),TEXT("\\"));
    const FString To=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Dest).Replace(TEXT("/"),TEXT("\\"));
    if(!::MoveFileExW(*From,*To,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return Fail(TEXT("atomic_replace"),FPlatformMisc::GetLastError());return true;
#else
    return Fail(TEXT("unsupported_platform"),0);
#endif
}
void AHCM5VS2HeroModestyReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bEnabled&&!bDone){bAutoQuit=false;Finish(TEXT("USER_ABORTED"),TEXT("World ended before completed captures"),true);}
    if(Viewport.IsValid()&&InputHandle.IsValid())Viewport->OnInputKey().Remove(InputHandle);
    if(FSlateApplication::IsInitialized()&&ActivationHandle.IsValid())FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
    if(ScreenshotHandle.IsValid())FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);Super::EndPlay(Reason);
}
