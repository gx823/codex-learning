#include "HCM5VS2CornerRevTwoReviewDirector.h"
#include "HCM5VS2CornerTimeDirector.h"
#include "HCM5VS2HeroOutlineComponent.h"
#include "HCM5VS2HaloComponent.h"
#include "HCM5VS2ExpressionComponent.h"
#include "HCM5VS2NPC.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "MaterialDomain.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Animation/MorphTarget.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "DataDrivenShaderPlatformInfo.h"
#include <atomic>
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ContentStreaming.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimInstance.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "UObject/Package.h"
#include "Serialization/JsonSerializer.h"
#include "RHI.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif



struct FHCM5VS2RevTwoRenderMaterialCheck
{
 struct FSlot { const FMaterialRenderProxy* Proxy=nullptr;bool Ready=false;FString Actual; };
 TArray<FSlot> Slots;std::atomic<bool> Complete{false};
};

namespace
{
const FString EvidenceRoot = TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2");
const FName OwnerTag(TEXT("HarborCity_M5VS2_HarborCorner_Rev2"));
bool OwnedCornerMap(const FString& Package)
{
 const FString Prefix=TEXT("/Game/HarborCity/M5VS2/WorldRev2/Review_");
 if(!Package.StartsWith(Prefix)) return false;
 const FString Tail=Package.Mid(Prefix.Len());
 if(Tail.Len()!=12+FString(TEXT("/L_CornerRevTwoReview")).Len() || Tail.Mid(12)!=TEXT("/L_CornerRevTwoReview")) return false;
 for(int32 I=0;I<12;++I) if(!((Tail[I]>='0'&&Tail[I]<='9')||(Tail[I]>='a'&&Tail[I]<='f'))) return false;
 return true;
}
TArray<TSharedPtr<FJsonValue>> V3(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> Rotation(const FRotator& R) { return V3(FVector(R.Pitch,R.Yaw,R.Roll)); }
TArray<TSharedPtr<FJsonValue>> Color(const FLinearColor& C)
{ return {MakeShared<FJsonValueNumber>(C.R),MakeShared<FJsonValueNumber>(C.G),MakeShared<FJsonValueNumber>(C.B),MakeShared<FJsonValueNumber>(C.A)}; }
bool Under(FString& Path, const FString& Root)
{
    if (Path.IsEmpty() || FPaths::IsRelative(Path)) return false;
    Path=FPaths::ConvertRelativePathToFull(Path); FPaths::NormalizeFilename(Path);
    return FPaths::CollapseRelativeDirectories(Path) && FPaths::IsUnderDirectory(Path,Root)
        && !Path.Mid(2).Contains(TEXT(":"));
}
bool TextIs(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Expected)
{ FString Value; return O && O->TryGetStringField(Key,Value) && Value==Expected; }
bool Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Min, double Max)
{ double V=0; return O && O->TryGetNumberField(Key,V) && FMath::IsFinite(V) && V>=Min && V<=Max; }
bool Array(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32 Count, double Min, double Max)
{
    const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if (!O || !O->TryGetArrayField(Key,Values) || Values->Num()!=Count) return false;
    for (const auto& Value:*Values)
    { double V=0; if (!Value || !Value->TryGetNumber(V) || !FMath::IsFinite(V) || V<Min || V>Max) return false; }
    return true;
}
FVector VectorField(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
{ const auto& A=O->GetArrayField(Key); return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()); }
FRotator RotField(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
{ const FVector V=VectorField(O,Key); return FRotator(V.X,V.Y,V.Z); }
FLinearColor ColorField(const TSharedPtr<FJsonObject>& O)
{ const auto& A=O->GetArrayField(TEXT("sun_color_linear_rgba")); return FLinearColor(float(A[0]->AsNumber()),float(A[1]->AsNumber()),float(A[2]->AsNumber()),float(A[3]->AsNumber())); }
bool ValidPNG(const FString& Path, int32& W, int32& H)
{
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Path));
    if (!File || File->TotalSize()<33) return false;
    uint8 Bytes[24]; File->Serialize(Bytes,24); const uint8 Magic[]={137,80,78,71,13,10,26,10};
    if (File->IsError() || FMemory::Memcmp(Bytes,Magic,8) || FMemory::Memcmp(Bytes+12,"IHDR",4)) return false;
    auto Big=[](const uint8* P) { return int32(uint32(P[0])<<24|uint32(P[1])<<16|uint32(P[2])<<8|uint32(P[3])); };
    W=Big(Bytes+16); H=Big(Bytes+20); return W==1920 && H==1080;
}
template<class T> T* UniqueTagged(UWorld* World, const TSharedRef<FJsonObject>& Readback, const TCHAR* Key)
{
    T* Found=nullptr; int32 Count=0; bool bExactClass=true;
    TArray<TSharedPtr<FJsonValue>> Actors;
    if (World) for (TActorIterator<T> It(World);It;++It) if (It->ActorHasTag(FName(Key)))
    {
        ++Count; Found=*It; bExactClass&=It->GetClass()==T::StaticClass();
        auto Actor=MakeShared<FJsonObject>(); Actor->SetStringField(TEXT("path"),It->GetPathName());
        Actor->SetStringField(TEXT("class"),It->GetClass()->GetPathName()); Actors.Add(MakeShared<FJsonValueObject>(Actor));
    }
    auto Binding=MakeShared<FJsonObject>(); Binding->SetNumberField(TEXT("tagged_count"),Count);
    Binding->SetBoolField(TEXT("all_exact_native_class"),bExactClass); Binding->SetArrayField(TEXT("actors"),Actors);
    Readback->SetObjectField(Key,Binding);
    return Count==1 && bExactClass?Found:nullptr;
}
}


AHCM5VS2CornerRevTwoReviewDirector::AHCM5VS2CornerRevTwoReviewDirector()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
 PrimaryActorTick.bTickEvenWhenPaused=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}


void AHCM5VS2CornerRevTwoReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2CornerRevTwoReview"))) return;
    Started=FPlatformTime::Seconds(); bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    bStarting=true; SetActorTickEnabled(true);
    // The client may exist before its native viewport/controller. Observe Escape now,
    // including during validation and bounded readiness waiting; never consume it.
    ObserveViewport();
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root) || !Under(Root,EvidenceRoot))
    { Finish(TEXT("FAIL"),TEXT("Rejected evidence path; no report written outside the authorized root")); return; }
    Directory=Root/(TEXT("CornerRevTwoReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true))
    { Directory.Empty(); Finish(TEXT("FAIL"),TEXT("Could not create evidence directory")); return; }
    FString Error;
    if (!LoadPlan(Error)) { Finish(TEXT("FAIL"),Error); return; }
    TryStart();
#endif
}

bool AHCM5VS2CornerRevTwoReviewDirector::LoadPlan(FString& Error)
{
 Error=TEXT("Invalid source-bound revision-two environment plan");
 if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2CornerRevTwoPlan="),PlanFile)||!Under(PlanFile,EvidenceRoot)
   ||FPaths::GetCleanFilename(PlanFile)!=TEXT("corner_v2_capture_plan.json"))return false;
 const int64 Bytes=IFileManager::Get().FileSize(*PlanFile);if(Bytes<1||Bytes>2*1024*1024)return false;
 TArray<uint8> Raw;FString JSON;
 if(!FFileHelper::LoadFileToArray(Raw,*PlanFile)||!FFileHelper::LoadFileToString(JSON,*PlanFile))return false;
 PlanSHA1=FSHA1::HashBuffer(Raw.GetData(),Raw.Num()).ToString();
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JSON),Plan)||!Plan)return false;
 FString Map;
 if(!Plan->TryGetStringField(TEXT("profile"),Profile)||(Profile!=TEXT("Environment")&&Profile!=TEXT("HeroPortrait")&&Profile!=TEXT("HeroFullBody")))return false;
 bHeroProfile=Profile!=TEXT("Environment");
 if(!Plan->TryGetStringField(TEXT("map"),Map)||!OwnedCornerMap(Map)
  ||!Number(Plan,TEXT("schema_version"),2,2)||!Number(Plan,TEXT("expected_screenshots"),bHeroProfile?4:7,bHeroProfile?4:7)
  ||!TextIs(Plan,TEXT("plan_type"),TEXT("HARBOR_CORNER_REV2_NATIVE_VIEWPORT"))
  ||!TextIs(Plan,TEXT("status"),TEXT("READY_FOR_RUNTIME_NOT_CAPTURED"))
  ||!TextIs(Plan,TEXT("owner"),OwnerTag.ToString())||!Array(Plan,TEXT("expected_resolution"),2,1080,1920))return false;
 const auto& Res=Plan->GetArrayField(TEXT("expected_resolution"));if(Res[0]->AsNumber()!=1920||Res[1]->AsNumber()!=1080)return false;
 const TArray<TSharedPtr<FJsonValue>>* Sources=nullptr;
 if(!Plan->TryGetArrayField(TEXT("source_bindings"),Sources)||Sources->Num()<5||Sources->Num()>800)return false;
 bool MapFound=false;
 for(const auto& V:*Sources)
 {
  auto S=V?V->AsObject():nullptr;FString Path,Hash,Kind;
  if(!S||!S->TryGetStringField(TEXT("path"),Path)||!S->TryGetStringField(TEXT("kind"),Kind)
    ||!S->TryGetStringField(TEXT("sha256"),Hash)||Hash.Len()!=64||!Number(S,TEXT("bytes"),1,1024.0*1024*512))return false;
  FString Root=Kind==TEXT("package")?FPaths::ProjectContentDir():Kind==TEXT("module")?FPaths::ProjectDir()/TEXT("Binaries/Win64"):
    Kind==TEXT("script")?TEXT("D:/科研学习/codex学习/tools"):Kind==TEXT("cpp")?FPaths::ProjectDir()/TEXT("Source"):EvidenceRoot;
  if(!Under(Path,Root)||IFileManager::Get().FileSize(*Path)!=S->GetNumberField(TEXT("bytes")))return false;
  if(Kind==TEXT("package")&&TextIs(S,TEXT("package"),Map))
  {
   FString Expected=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/Map.Mid(6)+TEXT(".umap"));FPaths::NormalizeFilename(Expected);
   if(!Path.Equals(Expected,ESearchCase::IgnoreCase))return false;MapFound=true;
  }
 }
 if(!MapFound)return false;
 const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Plan->TryGetArrayField(TEXT("shots"),Rows)||Rows->Num()!=(bHeroProfile?4:7))return false;
 if(bHeroProfile)
 {
  const TSharedPtr<FJsonObject>* Subject=nullptr;FString Class;
  if(!Plan->TryGetObjectField(TEXT("hero"),Subject)||!(*Subject)->TryGetStringField(TEXT("blueprint_class"),Class)
    ||!Class.StartsWith(TEXT("/Game/HarborCity/M5VS2/"))||Class.Contains(TEXT(".."))||!Class.EndsWith(TEXT("_C"))
    ||!TextIs(*Subject,TEXT("left_eye_bone"),TEXT("LeftEye"))||!TextIs(*Subject,TEXT("right_eye_bone"),TEXT("RightEye")))return false;
  for(int32 I=0;I<4;++I)
  {
   auto S=(*Rows)[I]?(*Rows)[I]->AsObject():nullptr;
   const FString Period=I==0?TEXT("Afternoon"):I==1?TEXT("Dusk"):I==2?TEXT("Night"):TEXT("Afternoon");
   const FString Scene=I==3?TEXT("Interior"):Period;
   const FString Label=FString::Printf(TEXT("%02d_%s_%s"),I,*Scene,*Profile);
   if(!Number(S,TEXT("index"),I,I)||!TextIs(S,TEXT("label"),Label)||!TextIs(S,TEXT("expected_png"),Label+TEXT(".png"))
    ||!TextIs(S,TEXT("light_preset"),Period)||!Number(S,TEXT("camera_fov_degrees"),Profile==TEXT("HeroPortrait")?40:65,Profile==TEXT("HeroPortrait")?40:65)
    ||!Number(S,TEXT("minimum_settle_seconds"),10,10)||!Array(S,TEXT("hero_feet_cm"),3,-2500,2500)
    ||!Number(S,TEXT("hero_yaw"),-180,180))return false;
   Shots.Add(S);
  }
  return true;
 }
 for(int32 I=0;I<7;++I)
 {
  auto S=(*Rows)[I]?(*Rows)[I]->AsObject():nullptr;
  FString Period=I<2?TEXT("Afternoon"):I<4?TEXT("Dusk"):I<6?TEXT("Night"):TEXT("Afternoon");
  FString Camera=I==6?TEXT("Air50m"):(I%2==0?TEXT("SouthStreet"):TEXT("Promenade"));
  FString Label=FString::Printf(TEXT("%02d_%s_%s"),I,*Period,*Camera);
  if(!Number(S,TEXT("index"),I,I)||!TextIs(S,TEXT("label"),Label)||!TextIs(S,TEXT("expected_png"),Label+TEXT(".png"))
   ||!TextIs(S,TEXT("light_preset"),Period)||!TextIs(S,TEXT("camera_name"),Camera)
   ||!Array(S,TEXT("camera_location_cm"),3,-10000,10000)||!Array(S,TEXT("camera_rotation_pitch_yaw_roll"),3,-180,180)
   ||!Number(S,TEXT("camera_fov_degrees"),20,100)||!Number(S,TEXT("minimum_settle_seconds"),I%2==0?10:2,I%2==0?10:2)
   ||!Number(S,TEXT("minimum_settle_rendered_frames"),30,30))return false;
  if(I>=2&&I<6&&(!VectorField(S,TEXT("camera_location_cm")).Equals(VectorField(Shots[I%2],TEXT("camera_location_cm")),.001)
    ||!RotField(S,TEXT("camera_rotation_pitch_yaw_roll")).Equals(RotField(Shots[I%2],TEXT("camera_rotation_pitch_yaw_roll")),.001)
    ||S->GetNumberField(TEXT("camera_fov_degrees"))!=Shots[I%2]->GetNumberField(TEXT("camera_fov_degrees"))))return false;
  Shots.Add(S);
 }
 return true;
}


void AHCM5VS2CornerRevTwoReviewDirector::ObserveViewport()
{
    if (bStopped) return;
    UGameViewportClient* Live=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if (Live==Viewport.Get() && InputHandle.IsValid()) return;
    if (Viewport.IsValid())
    { if (InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle); if (DrawHandle.IsValid()) Viewport->OnEndDraw().Remove(DrawHandle); }
    InputHandle.Reset(); DrawHandle.Reset(); Viewport=Live;
    if (Live)
    {
        InputHandle=Live->OnInputKey().AddUObject(this,&AHCM5VS2CornerRevTwoReviewDirector::Input);
        if (bActive) DrawHandle=Live->OnEndDraw().AddUObject(this,&AHCM5VS2CornerRevTwoReviewDirector::Drawn);
    }
}

void AHCM5VS2CornerRevTwoReviewDirector::TryStart()
{
    if (bStopped || !bStarting) return;
    ObserveViewport();
    FString Error; bool bRetryable=false;
    if (!Start(Error,bRetryable))
    {
        if (bStopped) return;
        if (bRetryable && FPlatformTime::Seconds()-Started<10)
        {
            if (StartupAttempts==1)
            {
                UE_LOG(LogTemp,Display,TEXT("M5VS2_CORNER_REV2_REVIEW_WAITING %s"),*Error);
                if (!Write(TEXT("RUNNING"),TEXT("Waiting at most 10 seconds for controller/camera/viewport readiness: ")+Error))
                    Finish(TEXT("FAIL"),TEXT("Could not save startup evidence report"));
            }
            return;
        }
        Finish(TEXT("FAIL"),Error); return;
    }
    if (bStopped) return;
    bStarting=false; bActive=true;
    if (!BeginShot(0,Error)) { Finish(TEXT("FAIL"),Error); return; }
    if (!Write(TEXT("RUNNING"),TEXT("Staged lighting review; seven native viewport requests planned, no visual acceptance")))
        Finish(TEXT("FAIL"),TEXT("Could not save initial evidence report"));
}

bool AHCM5VS2CornerRevTwoReviewDirector::Start(FString& Error, bool& bRetryable)
{
    bRetryable=false;
    if (bStopped || !bStarting) { Error=TEXT("Startup stopped"); return false; }
    UWorld* World=GetWorld();
    const auto Readback=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Checks; TArray<FString> Failures; bool bPermanentFailure=false;
    const auto Check=[&](const TCHAR* Name, bool Pass, const FString& Actual, const FString& Expected, bool MayWait=false)
    {
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("condition"),Name); Row->SetBoolField(TEXT("pass"),Pass);
        Row->SetStringField(TEXT("actual"),Actual); Row->SetStringField(TEXT("expected"),Expected);
        Row->SetBoolField(TEXT("may_wait_for_initialization"),MayWait); Checks.Add(MakeShared<FJsonValueObject>(Row));
        if (!Pass) { Failures.Add(FString(Name)+TEXT("=")+Actual+TEXT(" (expected ")+Expected+TEXT(")")); bPermanentFailure|=!MayWait; }
    };
    const auto YesNo=[](bool Value) { return Value?TEXT("true"):TEXT("false"); };
    Check(TEXT("world_present"),World!=nullptr,GetPathNameSafe(World),TEXT("live world"));
    Check(TEXT("not_commandlet"),!IsRunningCommandlet(),YesNo(IsRunningCommandlet()),TEXT("false"));
    Check(TEXT("rendering_enabled"),!GUsingNullRHI,YesNo(GUsingNullRHI),TEXT("false (NullRHI disabled)"));
    const FString ActualMap=World?World->GetOutermost()->GetName():TEXT("None");
    const FString MapPackage=Plan.IsValid()?Plan->GetStringField(TEXT("map")):FString();
    Check(TEXT("exact_corner_map"),ActualMap==MapPackage,ActualMap,MapPackage);
    int32 Directors=0; if (World) for (TActorIterator<AHCM5VS2CornerRevTwoReviewDirector> It(World);It;++It) ++Directors;
    Check(TEXT("review_director_count"),Directors==1,FString::FromInt(Directors),TEXT("1"));
    Controller=UGameplayStatics::GetPlayerController(this,0);
    UGameViewportClient* Live=World?World->GetGameViewport():nullptr;
    const bool bHasViewport=Live && Live->Viewport;
    const FIntPoint Size=bHasViewport?Live->Viewport->GetSizeXY():FIntPoint::ZeroValue;
    Check(TEXT("player_controller"),IsValid(Controller),GetPathNameSafe(Controller.Get()),TEXT("valid player 0 controller"),true);
    Check(TEXT("player_camera_manager"),IsValid(Controller) && IsValid(Controller->PlayerCameraManager),
        IsValid(Controller)?GetPathNameSafe(Controller->PlayerCameraManager.Get()):TEXT("None"),TEXT("valid camera manager"),true);
    Check(TEXT("game_viewport_client"),IsValid(Live),GetPathNameSafe(Live),TEXT("valid world game viewport client"),true);
    Check(TEXT("native_viewport"),bHasViewport,YesNo(bHasViewport),TEXT("true"),true);
    Check(TEXT("viewport_resolution"),Size==FIntPoint(1920,1080),FString::Printf(TEXT("%dx%d"),Size.X,Size.Y),TEXT("1920x1080"),true);
    Check(TEXT("escape_observer_bound"),Live && Viewport.Get()==Live && InputHandle.IsValid(),YesNo(InputHandle.IsValid()),TEXT("true on world game viewport client"),true);
    MainLight=UniqueTagged<ADirectionalLight>(World,Readback,TEXT("HC_VS2_Rev2_Sun"));
    SkyLight=UniqueTagged<ASkyLight>(World,Readback,TEXT("HC_VS2_Rev2_SkyLight"));
    PostProcess=UniqueTagged<APostProcessVolume>(World,Readback,TEXT("HC_VS2_Rev2_PostProcess"));
    Check(TEXT("unique_native_tagged_main_light"),MainLight!=nullptr,GetPathNameSafe(MainLight.Get()),TEXT("one exact native DirectionalLight"));
    Check(TEXT("unique_native_tagged_sky_light"),SkyLight!=nullptr,GetPathNameSafe(SkyLight.Get()),TEXT("one exact native SkyLight"));
    Check(TEXT("unique_native_tagged_post_process"),PostProcess!=nullptr,GetPathNameSafe(PostProcess.Get()),TEXT("one exact native PostProcessVolume"));
    const auto* Sun=MainLight?MainLight->GetLightComponent():nullptr;
    const auto* Sky=SkyLight?SkyLight->GetLightComponent():nullptr;
    Check(TEXT("main_light_component"),Sun!=nullptr,GetPathNameSafe(Sun),TEXT("light component"));
    Check(TEXT("sky_light_component"),Sky!=nullptr,GetPathNameSafe(Sky),TEXT("sky light component"));
    Check(TEXT("main_light_movable"),Sun && Sun->Mobility==EComponentMobility::Movable,Sun?FString::FromInt(int32(Sun->Mobility)):TEXT("None"),TEXT("2 (Movable)"));
    Check(TEXT("sky_light_movable"),Sky && Sky->Mobility==EComponentMobility::Movable,Sky?FString::FromInt(int32(Sky->Mobility)):TEXT("None"),TEXT("2 (Movable)"));
    Check(TEXT("post_process_unbound"),PostProcess && PostProcess->bUnbound,PostProcess?YesNo(PostProcess->bUnbound):TEXT("None"),TEXT("true"));
    Check(TEXT("post_process_enabled"),PostProcess && PostProcess->bEnabled,PostProcess?YesNo(PostProcess->bEnabled):TEXT("None"),TEXT("true"));
    Check(TEXT("post_process_blend_weight"),PostProcess && PostProcess->BlendWeight==1.f,PostProcess?FString::SanitizeFloat(PostProcess->BlendWeight):TEXT("None"),TEXT("1"));
    const double Elapsed=FPlatformTime::Seconds()-Started;
    Check(TEXT("startup_within_10_seconds"),Elapsed<=10,FString::SanitizeFloat(Elapsed),TEXT("at most 10 seconds"));
    ++StartupAttempts; Readback->SetNumberField(TEXT("attempt"),StartupAttempts); Readback->SetNumberField(TEXT("frame"),double(GFrameCounter));
    Readback->SetNumberField(TEXT("elapsed_wall_seconds"),Elapsed); Readback->SetArrayField(TEXT("conditions"),Checks);
    Readback->SetBoolField(TEXT("all_conditions_pass"),Failures.IsEmpty()); Readback->SetBoolField(TEXT("has_permanent_failure"),bPermanentFailure);
    LastStartupReadback=Readback; if (!FirstStartupReadback) FirstStartupReadback=Readback;
    if (!Failures.IsEmpty()) { Error=FString::Join(Failures,TEXT("; ")); bRetryable=!bPermanentFailure; return false; }

    // Capture original actual state before camera, HUD or lighting overrides.
    OriginalView=Controller->GetViewTarget(); OriginalState=Snapshot();
    TimeDirector=UniqueTagged<AHCM5VS2CornerTimeDirector>(World,Readback,TEXT("HC_VS2_Rev2_TimeDirector"));
    if(!TimeDirector || TimeDirector->CurrentPeriod.IsNone()) { Error=TEXT("One initialized permanent time director required");bRetryable=true;return false; }
    SavedPeriod=TimeDirector->CurrentPeriod; OriginalState=Snapshot(); bStateSaved=true;
    if(bHeroProfile && !BindHero(Error))return false;
    if(!bHeroProfile)Hero=Cast<ACharacter>(Controller->GetPawn());
    if (AHUD* HUD=Controller->GetHUD()) { bHUDSaved=true; bSavedHUD=HUD->bShowHUD; HUD->bShowHUD=false; }
    DrawHandle=Live->OnEndDraw().AddUObject(this,&AHCM5VS2CornerRevTwoReviewDirector::Drawn);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2CornerRevTwoReviewDirector::Processed);
    FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
    ReviewCamera=GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!ReviewCamera) { Error=TEXT("Failed to spawn transient native review camera"); return false; }
    ReviewCamera->GetCameraComponent()->bConstrainAspectRatio=false;
    ReviewCamera->GetCameraComponent()->PostProcessBlendWeight=0;
    return true;
}

FVector AHCM5VS2CornerRevTwoReviewDirector::ExpectedCameraLocation() const
{return bHeroProfile?HeroCameraLocations[ShotIndex==3?1:0]:VectorField(Shots[ShotIndex],TEXT("camera_location_cm"));}
FRotator AHCM5VS2CornerRevTwoReviewDirector::ExpectedCameraRotation() const
{return bHeroProfile?HeroCameraRotations[ShotIndex==3?1:0]:RotField(Shots[ShotIndex],TEXT("camera_rotation_pitch_yaw_roll"));}
bool AHCM5VS2CornerRevTwoReviewDirector::LightingMatchesRequest() const
{
 if(!TimeDirector||!Shots.IsValidIndex(ShotIndex)||TimeDirector->CurrentPeriod.ToString()!=Shots[ShotIndex]->GetStringField(TEXT("light_preset")))return false;
 TSharedPtr<FJsonObject> Read;const FString JSON=TimeDirector->GetLightingDiagnostics();
 return JSON==RequestedLightingReadback && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JSON),Read)&&Read&&Read->GetBoolField(TEXT("bound"))
  &&Read->HasField(TEXT("sky_parameters_complete"))&&Read->GetBoolField(TEXT("sky_parameters_complete"))
  &&TextIs(Read,TEXT("period"),Shots[ShotIndex]->GetStringField(TEXT("light_preset")));
}
bool AHCM5VS2CornerRevTwoReviewDirector::BeginShot(int32 Index,FString& Error)
{
 if(bStopped||!Shots.IsValidIndex(Index)||!TimeDirector||!ReviewCamera){Error=TEXT("Capture binding or shot invalid");return false;}
 ShotIndex=Index;auto S=Shots[Index];
 if(!TimeDirector->SetTimeOfDay(FName(*S->GetStringField(TEXT("light_preset"))))){Error=TEXT("Playable time-of-day API rejected request");return false;}
 RequestedLightingReadback=TimeDirector->GetLightingDiagnostics();
 if(bHeroProfile&&!StageHero(Index,Error))return false;
 if(!bHeroProfile){LastMaterialReadiness.Reset();PendingMaterialCheck.Reset();bMaterialsReady=false;NextMaterialPoll=0;}
 if(!bHeroProfile||HeroCameraLocks[Index==3?1:0])ReviewCamera->SetActorLocationAndRotation(ExpectedCameraLocation(),ExpectedCameraRotation());
 ReviewCamera->GetCameraComponent()->SetFieldOfView(float(S->GetNumberField(TEXT("camera_fov_degrees"))));Controller->SetViewTarget(ReviewCamera);
 ShotStarted=FPlatformTime::Seconds();ShotDrawStart=DrawCount;ReadyDraws=0;bReady=false;return true;
}

bool AHCM5VS2CornerRevTwoReviewDirector::BindHero(FString& Error)
{
 auto Subject=Plan->GetObjectField(TEXT("hero"));int32 Count=0;
 for(TActorIterator<ACharacter> It(GetWorld());It;++It)
  if(It->GetClass()->GetPathName()==Subject->GetStringField(TEXT("blueprint_class"))){Hero=*It;++Count;}
 if(Count!=1||!Hero||Controller->GetPawn()!=Hero||!Hero->GetMesh()||Hero->IsHidden())
 {Error=TEXT("Exact selected Hero must be the unique visible possessed pawn");return false;}
 OriginalHeroTransform=Hero->GetActorTransform();bHeroStateSaved=true;return true;
}

bool AHCM5VS2CornerRevTwoReviewDirector::StageHero(int32 Index,FString& Error)
{
 if(!Hero||Controller->GetPawn()!=Hero||!Hero->GetCapsuleComponent())
 {Error=TEXT("Selected Hero binding lost during staging");return false;}
 if(Index==0||Index==3)
 {
  auto S=Shots[Index];const FVector Feet=VectorField(S,TEXT("hero_feet_cm"));
  Hero->GetCharacterMovement()->StopMovementImmediately();
  const FVector Center=Feet+FVector(0,0,Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
  if(!Hero->SetActorLocationAndRotation(Center,FRotator(0,S->GetNumberField(TEXT("hero_yaw")),0),false,nullptr,ETeleportType::TeleportPhysics))
  {Error=TEXT("Bounded Hero inside/outside scene staging failed");return false;}
 }
 LastMaterialReadiness.Reset();PendingMaterialCheck.Reset();bMaterialsReady=false;NextMaterialPoll=0;
 return true;
}

bool AHCM5VS2CornerRevTwoReviewDirector::LockHeroCamera(FString& Error)
{
 const int32 I=ShotIndex==3?1:0;if(!Hero||!Hero->GetMesh()||HeroCameraLocks[I])return false;
 auto* Body=Hero->GetMesh();const FName LeftName(TEXT("LeftEye")),RightName(TEXT("RightEye"));
 if(Body->GetBoneIndex(LeftName)==INDEX_NONE||Body->GetBoneIndex(RightName)==INDEX_NONE)
 {Error=TEXT("Selected Hero actual LeftEye/RightEye bones missing; no guessed height fallback");return false;}
 const FVector L=Body->GetBoneLocation(LeftName,EBoneSpaces::WorldSpace),R=Body->GetBoneLocation(RightName,EBoneSpaces::WorldSpace),Eye=(L+R)*.5;
 const FVector Feet=Hero->GetActorLocation()-FVector(0,0,Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 if(Eye.ContainsNaN()||FVector::Dist(L,R)<1||FVector::Dist(L,R)>25||Eye.Z-Feet.Z<70||Eye.Z-Feet.Z>220)
 {Error=TEXT("Actual animated eye geometry outside bounded human scale");return false;}
 const FVector Forward=FRotator(0,Hero->GetActorRotation().Yaw,0).Vector();FVector Target=Eye;
 if(Profile==TEXT("HeroPortrait"))HeroCameraLocations[I]=Eye+Forward*130;
 else
 {
  // Eye height alone cropped the crown and halo in the previous full-body proof.
  // Frame the visible body's imported extent and the actual moving halo; this
  // affects only this review camera, never a gameplay camera or character scale.
  FBox Fit=Body->Bounds.GetBox();
  if(Body->GetSkeletalMeshAsset())Fit+=Body->GetSkeletalMeshAsset()->GetImportedBounds().TransformBy(Body->GetComponentTransform()).GetBox();
  TArray<UStaticMeshComponent*> Parts;Hero->GetComponents<UStaticMeshComponent>(Parts);
  for(const auto* Part:Parts)if(Part&&Part->IsVisible()&&Part->GetName().StartsWith(TEXT("VS2HaloPart")))Fit+=Part->Bounds.GetBox();
  if(!Fit.IsValid||Fit.GetSize().ContainsNaN()||Fit.GetSize().Z<70||Fit.GetSize().Z>300)
  {Error=TEXT("Full-body visible extent invalid; refuse cropped fallback");return false;}
  const FVector Right=FRotationMatrix(FRotator(0,Hero->GetActorRotation().Yaw,0)).GetUnitAxis(EAxis::Y);
  const double TanH=FMath::Tan(FMath::DegreesToRadians(ReviewCamera->GetCameraComponent()->FieldOfView*.5));
  const double TanV=TanH/(1920./1080.);
  Target=Fit.GetCenter();double Distance=0;
  for(int32 Corner=0;Corner<8;++Corner)
  {
   const FVector P((Corner&1)?Fit.Max.X:Fit.Min.X,(Corner&2)?Fit.Max.Y:Fit.Min.Y,(Corner&4)?Fit.Max.Z:Fit.Min.Z);
   const FVector D=P-Target;
   Distance=FMath::Max(Distance,FVector::DotProduct(D,Forward)+1.12*FMath::Max(FMath::Abs(D.Z)/TanV,FMath::Abs(FVector::DotProduct(D,Right))/TanH));
  }
  HeroCameraLocations[I]=Target+Forward*Distance;
 }
 HeroCameraRotations[I]=(Target-HeroCameraLocations[I]).Rotation();HeroCameraLocks[I]=MakeShared<FJsonObject>();
 auto O=HeroCameraLocks[I];O->SetStringField(TEXT("status"),TEXT("LOCKED_FROM_ACTUAL_ANIMATED_EYES"));
 O->SetArrayField(TEXT("left_eye_world_cm"),V3(L));O->SetArrayField(TEXT("right_eye_world_cm"),V3(R));
 O->SetArrayField(TEXT("actual_capsule_feet_cm"),V3(Feet));O->SetArrayField(TEXT("camera_location_cm"),V3(HeroCameraLocations[I]));
 O->SetArrayField(TEXT("camera_rotation_pitch_yaw_roll"),Rotation(HeroCameraRotations[I]));O->SetNumberField(TEXT("frame"),double(GFrameCounter));
 O->SetBoolField(TEXT("cdo_fallback_used"),false);
 ReviewCamera->SetActorLocationAndRotation(HeroCameraLocations[I],HeroCameraRotations[I]);
 CameraLockedAt=FPlatformTime::Seconds();CameraLockDraw=DrawCount;ReadyDraws=0;return true;
}

bool AHCM5VS2CornerRevTwoReviewDirector::PollHeroMaterials(FString& Error)
{
 if(!Hero||!Hero->GetMesh()){Error=TEXT("Actual controlled Hero unavailable for material readiness");return false;}
 const double Now=FPlatformTime::Seconds();
 if(PendingMaterialCheck)
 {
  if(!PendingMaterialCheck->Complete.load(std::memory_order_acquire))return false;
  bool All=LastMaterialReadiness->GetBoolField(TEXT("game_thread_ready"));
  const auto& Rows=LastMaterialReadiness->GetArrayField(TEXT("slots"));
  for(int32 I=0;I<PendingMaterialCheck->Slots.Num();++I)
  {auto O=Rows[I]->AsObject();O->SetBoolField(TEXT("render_ready_without_fallback"),PendingMaterialCheck->Slots[I].Ready);O->SetStringField(TEXT("render_actual_resource"),PendingMaterialCheck->Slots[I].Actual);All&=PendingMaterialCheck->Slots[I].Ready;}
  PendingMaterialCheck.Reset();bMaterialsReady=All;
  LastMaterialReadiness->SetStringField(TEXT("status"),All?TEXT("READY"):TEXT("NOT_READY"));LastMaterialReadiness->SetNumberField(TEXT("observed_frame"),double(GFrameCounter));
  if(LastMaterialReadiness->GetBoolField(TEXT("definite_failure"))){Error=TEXT("Hero/outline material missing usage or has actual shader compile errors");return false;}
 }
 if(Now-ShotStarted>120&&!bMaterialsReady){Error=TEXT("Bounded Hero material readiness timeout");return false;}
 if(Now<NextMaterialPoll)return bMaterialsReady;
 NextMaterialPoll=Now+.5;CheckedMaterials.Reset();LastMaterialReadiness=MakeShared<FJsonObject>();
 auto State=MakeShared<FHCM5VS2RevTwoRenderMaterialCheck,ESPMode::ThreadSafe>();
 TArray<TSharedPtr<FJsonValue>> Rows;bool All=true,Failed=false;TArray<UMeshComponent*> Meshes;Hero->GetComponents(Meshes);
 const auto FeatureLevel=GetWorld()->GetFeatureLevel();const EShaderPlatform Platform=GetFeatureLevelShaderPlatform_Checked(FeatureLevel);
 for(auto* Body:Meshes)
 {
  if(!Body||!Body->IsVisible()||Body->bHiddenInGame)continue;
  auto* Skeletal=Cast<USkeletalMeshComponent>(Body);
  if(Skeletal&&Skeletal->GetSkeletalMeshAsset()!=Hero->GetMesh()->GetSkeletalMeshAsset())continue;
  TSet<UMaterialInterface*> MorphMaterials;
  if(Skeletal)if(const auto* RenderData=Skeletal->GetSkeletalMeshRenderData())for(const auto& Active:Skeletal->ActiveMorphTargets)
  {
   if(!Active.Key)continue;const auto& LODs=Active.Key->GetMorphLODModels();
   for(int32 L=0;L<FMath::Min(RenderData->LODRenderData.Num(),LODs.Num());++L)for(int32 S:LODs[L].SectionIndices)
    if(RenderData->LODRenderData[L].RenderSections.IsValidIndex(S))MorphMaterials.Add(Body->GetMaterial(RenderData->LODRenderData[L].RenderSections[S].MaterialIndex));
  }
  for(int32 I=0;I<Body->GetNumMaterials();++I)
  {
   auto* M=Body->GetMaterial(I);auto* Base=M?M->GetMaterial():nullptr;auto* Resource=M?M->GetMaterialResource(Platform):nullptr;
   bool Compiling=M&&M->IsCompiling();TArray<TSharedPtr<FJsonValue>> Errors;
#if WITH_EDITOR
   if(Resource){Compiling|=!Resource->IsCompilationFinished();for(const auto& E:Resource->GetCompileErrors())if(Errors.Num()<8)Errors.Add(MakeShared<FJsonValueString>(E.Left(2048)));}
#endif
   auto* Map=Resource?Resource->GetGameThreadShaderMap():nullptr;
   const bool Usage=Base&&(!Skeletal||Base->GetUsageByFlag(MATUSAGE_SkeletalMesh))&&(!MorphMaterials.Contains(M)||Base->GetUsageByFlag(MATUSAGE_MorphTargets));
   const bool Complete=Resource&&Resource->IsGameThreadShaderMapComplete();
   const bool CompileFailed=!Errors.IsEmpty()||(Map&&Map->IsCompilationFinalized()&&!Map->CompiledSuccessfully());
#if WITH_EDITOR
   if(Resource&&!Complete&&!CompileFailed)Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
#endif
   const bool IsReady=M&&Resource&&Usage&&!Compiling&&!CompileFailed&&Complete&&Map&&Map->IsValidForRendering();
   All&=IsReady;Failed|=!M||!Usage||(CompileFailed&&!Compiling);
   auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("component"),Body->GetPathName());O->SetNumberField(TEXT("slot"),I);O->SetStringField(TEXT("actual_material"),GetPathNameSafe(M));
   O->SetBoolField(TEXT("required_usage"),Usage);O->SetBoolField(TEXT("game_thread_ready"),IsReady);O->SetBoolField(TEXT("compiling"),Compiling);O->SetArrayField(TEXT("compile_errors"),Errors);
   Rows.Add(MakeShared<FJsonValueObject>(O));CheckedMaterials.Add(M);auto& Slot=State->Slots.AddDefaulted_GetRef();Slot.Proxy=M?M->GetRenderProxy():nullptr;
  }
 }
 LastMaterialReadiness->SetStringField(TEXT("status"),TEXT("RENDER_QUERY_PENDING"));LastMaterialReadiness->SetArrayField(TEXT("slots"),Rows);
 LastMaterialReadiness->SetBoolField(TEXT("game_thread_ready"),All&&Rows.Num()>=5);LastMaterialReadiness->SetBoolField(TEXT("definite_failure"),Failed||Rows.Num()<5);
 PendingMaterialCheck=State;
 ENQUEUE_RENDER_COMMAND(HCM5VS2RevTwoMaterialReadback)([State,FeatureLevel](FRHICommandListImmediate& RHICmdList)
 {
  for(auto& S:State->Slots)if(S.Proxy){const FMaterial* Direct=S.Proxy->GetMaterialNoFallback(FeatureLevel);const auto* Map=Direct?Direct->GetRenderingThreadShaderMap():nullptr;
   const FMaterialRenderProxy* Fallback=nullptr;const FMaterial& Actual=S.Proxy->GetMaterialWithFallback(FeatureLevel,Fallback);S.Actual=Actual.GetFriendlyName();
   S.Ready=Direct&&Direct->IsRenderingThreadShaderMapComplete()&&Map&&Map->IsValidForRendering()&&!Fallback;}
  State->Complete.store(true,std::memory_order_release);
 });
 return false;
}


bool AHCM5VS2CornerRevTwoReviewDirector::CameraMatchesRequest() const
{
    const auto* Camera=Controller?Controller->PlayerCameraManager.Get():nullptr;
    return Camera && Controller->GetViewTarget()==ReviewCamera && Viewport.IsValid() && Viewport->Viewport
        && Viewport->Viewport->GetSizeXY()==FIntPoint(1920,1080)
        && Camera->GetCameraLocation().Equals(ExpectedCameraLocation(),.1)
        && Camera->GetCameraRotation().Equals(ExpectedCameraRotation(),.05)
        && FMath::IsNearlyEqual(Camera->GetFOVAngle(),float(Shots[ShotIndex]->GetNumberField(TEXT("camera_fov_degrees"))),.01f);
}

bool AHCM5VS2CornerRevTwoReviewDirector::Ready()
{
    ShaderJobs=0; bool bCompiling=false;
#if WITH_EDITOR
    if (GShaderCompilingManager)
    { ShaderJobs=GShaderCompilingManager->GetNumRemainingJobs(); bCompiling=GShaderCompilingManager->IsCompiling(); }
#endif
    WantingResources=IStreamingManager::Get().GetNumWantingResources();
    bReady=!bCompiling && ShaderJobs==0 && WantingResources==0;
    if (!bReady) ReadyDraws=0;
    return bReady;
}

TSharedPtr<FJsonObject> AHCM5VS2CornerRevTwoReviewDirector::Snapshot() const
{
    auto O=MakeShared<FJsonObject>(); O->SetNumberField(TEXT("frame"),double(GFrameCounter));
    if (Controller && Controller->PlayerCameraManager)
    {
        const auto* Camera=Controller->PlayerCameraManager.Get();
        O->SetStringField(TEXT("view_target"),GetPathNameSafe(Controller->GetViewTarget()));
        O->SetArrayField(TEXT("camera_location_cm"),V3(Camera->GetCameraLocation()));
        O->SetArrayField(TEXT("camera_rotation_pitch_yaw_roll"),Rotation(Camera->GetCameraRotation()));
        O->SetNumberField(TEXT("camera_fov_degrees"),Camera->GetFOVAngle());
        if (const AHUD* HUD=Controller->GetHUD()) O->SetBoolField(TEXT("hud_visible"),HUD->bShowHUD);
    }
    if (MainLight && MainLight->GetLightComponent())
    {
        O->SetStringField(TEXT("main_light_actor"),MainLight->GetPathName());
        O->SetArrayField(TEXT("sun_rotation_pitch_yaw_roll"),Rotation(MainLight->GetActorRotation()));
        O->SetArrayField(TEXT("sun_component_rotation_pitch_yaw_roll"),Rotation(MainLight->GetLightComponent()->GetComponentRotation()));
        O->SetArrayField(TEXT("sun_color_linear_rgba"),Color(MainLight->GetLightComponent()->GetLightColor()));
        O->SetNumberField(TEXT("sun_intensity"),MainLight->GetLightComponent()->Intensity);
    }
    if (SkyLight && SkyLight->GetLightComponent())
    {
        O->SetStringField(TEXT("sky_light_actor"),SkyLight->GetPathName());
        O->SetNumberField(TEXT("sky_intensity"),SkyLight->GetLightComponent()->Intensity);
        O->SetBoolField(TEXT("sky_realtime_capture"),SkyLight->GetLightComponent()->IsRealTimeCaptureEnabled());
    }
    if (PostProcess)
    {
        O->SetStringField(TEXT("post_process_actor"),PostProcess->GetPathName());
        const auto& PP=PostProcess->Settings;
        O->SetNumberField(TEXT("exposure_method"),int32(PP.AutoExposureMethod));
        O->SetBoolField(TEXT("override_exposure_method"),PP.bOverride_AutoExposureMethod);
        O->SetBoolField(TEXT("physical_camera_exposure"),PP.AutoExposureApplyPhysicalCameraExposure);
        O->SetNumberField(TEXT("exposure_bias"),PP.AutoExposureBias); O->SetNumberField(TEXT("bloom_intensity"),PP.BloomIntensity);
    }
    TArray<TSharedPtr<FJsonValue>> People;
    for (TActorIterator<ACharacter> It(GetWorld());It;++It)
    {
        const ACharacter* Person=*It; const USkeletalMeshComponent* Body=Person->GetMesh();
        auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("actor"),Person->GetPathName());
        P->SetStringField(TEXT("blueprint_class"),Person->GetClass()->GetPathName());
        P->SetArrayField(TEXT("location_cm"),V3(Person->GetActorLocation())); P->SetBoolField(TEXT("hidden"),Person->IsHidden());

        if (Body)
        {
            P->SetStringField(TEXT("mesh"),GetPathNameSafe(Body->GetSkeletalMeshAsset()));
            P->SetStringField(TEXT("animation_class"),Body->GetAnimInstance()?Body->GetAnimInstance()->GetClass()->GetPathName():TEXT("None"));
            P->SetStringField(TEXT("postprocess_class"),Body->GetPostProcessInstance()?Body->GetPostProcessInstance()->GetClass()->GetPathName():TEXT("None"));
            TArray<TSharedPtr<FJsonValue>> Materials;
            for (int32 I=0;I<Body->GetNumMaterials();++I) Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Body->GetMaterial(I))));
            P->SetArrayField(TEXT("actual_materials"),Materials);
        }
        if (const AHCM5VS2NPC* NPC=Cast<AHCM5VS2NPC>(Person)) P->SetStringField(TEXT("npc_profile"),GetPathNameSafe(NPC->NPCProfile.Get()));
        for(const auto& Pair:TArray<TPair<FString,FString>>{
          {TEXT("outline"),Person->FindComponentByClass<UHCM5VS2HeroOutlineComponent>()?Person->FindComponentByClass<UHCM5VS2HeroOutlineComponent>()->GetOutlineDiagnostics():TEXT("")},
          {TEXT("halo"),Person->FindComponentByClass<UHCM5VS2HaloComponent>()?Person->FindComponentByClass<UHCM5VS2HaloComponent>()->GetHaloDiagnostics():TEXT("")}})
        {TSharedPtr<FJsonObject> D;if(!Pair.Value.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Pair.Value),D)&&D)P->SetObjectField(Pair.Key,D);else P->SetStringField(Pair.Key+TEXT("_status"),TEXT("COMPONENT_NOT_PRESENT"));}
        People.Add(MakeShared<FJsonValueObject>(P));
    }
    O->SetArrayField(TEXT("characters_unmodified_by_director"),People);

    if(TimeDirector){TSharedPtr<FJsonObject> D;const FString JSON=TimeDirector->GetLightingDiagnostics();
        if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JSON),D)&&D)O->SetObjectField(TEXT("playable_time_readback"),D);}
    return O;
}

void AHCM5VS2CornerRevTwoReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (bStopped) return;
    if (bStarting || bActive || bExitPending) ObserveViewport();
    const double Now=FPlatformTime::Seconds();
    if (bExitPending)
    {
        if (Now-Finished>=2 && !bStopped)
        { bExitPending=false; SetActorTickEnabled(false); FPlatformMisc::RequestExit(false,*ExitReason); }
        return;
    }
    if (bStarting) { TryStart(); return; }
    if (!bActive) return;
    if (Now-Started>(bHeroProfile?300:180)) { Finish(TEXT("FAIL"),TEXT("Bounded review wall-time deadline")); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (bPending)
    {
        if (bProcessed && !FScreenshotRequest::IsScreenshotRequested() && GFrameCounter>RequestFrame)
        {
            int32 W=0,H=0;
            const bool FinalViewValid=Pending->HasField(TEXT("final_view_matches_requested_camera")) && Pending->GetBoolField(TEXT("final_view_matches_requested_camera"))
                &&Pending->HasField(TEXT("final_lighting_matches_requested_period"))&&Pending->GetBoolField(TEXT("final_lighting_matches_requested_period"));
            const bool Valid=ValidPNG(PendingPNG,W,H) && FinalViewValid;
            Pending->SetStringField(TEXT("status"),Valid?TEXT("PASS"):TEXT("FAIL"));
            Pending->SetNumberField(TEXT("processed_frame"),double(ProcessedFrame)); Pending->SetNumberField(TEXT("width"),W); Pending->SetNumberField(TEXT("height"),H);
            Pending->SetNumberField(TEXT("file_bytes"),IFileManager::Get().FileSize(*PendingPNG)); Results.Add(Pending); Pending.Reset(); bPending=false;
            if (!Valid) { Finish(TEXT("FAIL"),TEXT("Native screenshot missing/not 1920x1080 or final camera/lighting changed")); return; }
            if (ShotIndex==Shots.Num()-1) { Finish(TEXT("PASS"),TEXT("All planned raw native PNGs saved; visual acceptance remains USER_REVIEW")); return; }
            if (!Write(TEXT("RUNNING"),TEXT("Raw native capture verified; staged lighting review remains incomplete")))
            { Finish(TEXT("FAIL"),TEXT("Could not update capture report")); return; }
            FString Error; if (!BeginShot(ShotIndex+1,Error)) Finish(TEXT("FAIL"),Error);
        }
        else if (Now-Requested>15) Finish(TEXT("FAIL"),TEXT("Native screenshot processing timeout"));
        return;
    }
    if (!IsValid(Controller) || !IsValid(ReviewCamera) || !IsValid(MainLight) || !IsValid(SkyLight) || !IsValid(PostProcess))
    { Finish(TEXT("FAIL"),TEXT("Runtime capture binding lost")); return; }
    if (!Ready() || ReadyDraws<30 || Now-ShotStarted<Shots[ShotIndex]->GetNumberField(TEXT("minimum_settle_seconds"))
        || FScreenshotRequest::IsScreenshotRequested()) return;
    FString Error;if(!PollHeroMaterials(Error)){if(!Error.IsEmpty())Finish(TEXT("FAIL"),Error);return;}
    if(bHeroProfile)
    {
      if(!HeroCameraLocks[ShotIndex==3?1:0]){if(!LockHeroCamera(Error))Finish(TEXT("FAIL"),Error);return;}
      if(Now-CameraLockedAt<2||DrawCount-CameraLockDraw<30)return;
      // Review the eyes between natural blinks; never disable or overwrite expression.
      if(const auto* Face=Hero->FindComponentByClass<UHCM5VS2ExpressionComponent>())
          if(Face->GetBlinkWeight()>.02f)return;
    }
    Capture();
}

void AHCM5VS2CornerRevTwoReviewDirector::Capture()
{
    if (bStopped || !Viewport.IsValid() || !Viewport->Viewport) return;
    const auto& S=Shots[ShotIndex];
    if(!CameraMatchesRequest()||!LightingMatchesRequest())
    {Finish(TEXT("FAIL"),TEXT("Actual final view or playable lighting does not match request"));return;}
    PendingPNG=Directory/S->GetStringField(TEXT("expected_png"));
    if (IFileManager::Get().FileExists(*PendingPNG)) { Finish(TEXT("FAIL"),TEXT("Refuse PNG overwrite")); return; }
    Pending=MakeShared<FJsonObject>(); Pending->SetNumberField(TEXT("index"),ShotIndex);
    Pending->SetStringField(TEXT("label"),S->GetStringField(TEXT("label"))); Pending->SetStringField(TEXT("file"),PendingPNG);
    Pending->SetStringField(TEXT("status"),TEXT("REQUESTED_NOT_YET_SAVED")); Pending->SetStringField(TEXT("lighting_origin"),TEXT("PLAYABLE_TIME_DIRECTOR_PUBLIC_API"));
    Pending->SetObjectField(TEXT("requested"),S); Pending->SetObjectField(TEXT("actual_at_request"),Snapshot());
    if(Hero)if(const auto* Face=Hero->FindComponentByClass<UHCM5VS2ExpressionComponent>())
        Pending->SetNumberField(TEXT("natural_blink_weight_at_request"),Face->GetBlinkWeight());

    Pending->SetNumberField(TEXT("request_frame"),double(GFrameCounter)); Pending->SetNumberField(TEXT("viewport_draws_since_change"),double(DrawCount-ShotDrawStart));
    Pending->SetNumberField(TEXT("consecutive_ready_viewport_draws"),ReadyDraws); Pending->SetNumberField(TEXT("settle_wall_seconds"),FPlatformTime::Seconds()-ShotStarted);
    Pending->SetNumberField(TEXT("shader_jobs_remaining"),ShaderJobs); Pending->SetNumberField(TEXT("streaming_resources_wanted"),WantingResources);
    if(LastMaterialReadiness)Pending->SetObjectField(TEXT("hero_and_optional_components_material_readiness"),LastMaterialReadiness);
    if(bHeroProfile)Pending->SetObjectField(TEXT("actual_hero_camera_lock"),HeroCameraLocks[ShotIndex==3?1:0]);
    bPending=true; bProcessed=false; RequestFrame=GFrameCounter; Requested=FPlatformTime::Seconds();
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}

void AHCM5VS2CornerRevTwoReviewDirector::Drawn()
{
    if (!bActive || bStopped) return; ++DrawCount;
    if (bReady && !UGameplayStatics::IsGamePaused(this)) ++ReadyDraws;
    if (bPending && Pending && GFrameCounter==RequestFrame)
    {
        Pending->SetObjectField(TEXT("actual_at_viewport_end_draw"),Snapshot());
        Pending->SetBoolField(TEXT("final_view_matches_requested_camera"),CameraMatchesRequest());
        Pending->SetBoolField(TEXT("final_lighting_matches_requested_period"),LightingMatchesRequest());
    }
}

void AHCM5VS2CornerRevTwoReviewDirector::Processed()
{ if (!bStopped && bPending) { bProcessed=true; ProcessedFrame=GFrameCounter; } }

void AHCM5VS2CornerRevTwoReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if ((bStarting || bActive || bExitPending) && !bStopped && Event.Event==IE_Pressed && Event.Key==EKeys::Escape)
    {
        bStopped=true; bStarting=false; bActive=false; bExitPending=false; bAutoQuit=false; StopFrame=GFrameCounter;
        if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
        bPending=false; SetActorTickEnabled(false);
        Write(TEXT("NOT_RUN"),TEXT("Viewport Escape permanently stopped all camera/light/capture/restore/auto-exit actions. Event observed, not consumed; ordinary P route unchanged."));
    }
}

void AHCM5VS2CornerRevTwoReviewDirector::Restore()
{
 if(bStopped||!bStateSaved)return;bStateSaved=false;
 if(TimeDirector&&!SavedPeriod.IsNone())TimeDirector->SetTimeOfDay(SavedPeriod);
 if(bHeroStateSaved&&Hero){Hero->GetCharacterMovement()->StopMovementImmediately();Hero->SetActorTransform(OriginalHeroTransform,false,nullptr,ETeleportType::TeleportPhysics);bHeroStateSaved=false;}
 if(Controller){if(OriginalView.IsValid())Controller->SetViewTarget(OriginalView.Get());if(AHUD* HUD=Controller->GetHUD())if(bHUDSaved)HUD->bShowHUD=bSavedHUD;}
}


void AHCM5VS2CornerRevTwoReviewDirector::Finish(const FString& Status, const FString& Detail)
{
    if (bStopped) return; bStarting=false; bActive=false;
    if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    bPending=false; Restore(); Finished=FPlatformTime::Seconds();
    const bool bAllVerified=Results.Num()==Shots.Num() && !Shots.IsEmpty() && Results.ContainsByPredicate([](const TSharedPtr<FJsonObject>& Result)
        { return !TextIs(Result,TEXT("status"),TEXT("PASS")); })==false;
    bExitPending=bAutoQuit && (Status==TEXT("FAIL") || (Status==TEXT("PASS") && bAllVerified));
    ExitReason=TEXT("M5VS2 corner staged lighting review ")+Status;
    // A failed report write must not leave an explicitly auto-quitting failed run idle.
    // The log remains the evidence for an inaccessible/rejected report destination.
    if (!Write(Status,Detail))
    {
        bExitPending=bAutoQuit; ExitReason=TEXT("M5VS2 corner review FAIL: evidence report write failed");
        UE_LOG(LogTemp,Error,TEXT("M5VS2_CORNER_REV2_REVIEW_FAIL report write failed; autoquit_explicit=%d"),bAutoQuit);
    }
    SetActorTickEnabled(bExitPending);
    UE_LOG(LogTemp,Display,TEXT("M5VS2_CORNER_REV2_REVIEW_%s %s %s"),*Status,*Directory,*Detail);
}

bool AHCM5VS2CornerRevTwoReviewDirector::Write(const FString& Status, const FString& Detail)
{
    if (Directory.IsEmpty()) return false;
    auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("status"),Status); O->SetStringField(TEXT("detail"),Detail);
    O->SetStringField(TEXT("title"),TEXT("Harbor Corner Rev2 - bounded native viewport review"));

    O->SetStringField(TEXT("profile"),Profile);
    O->SetStringField(TEXT("hero_staging_scope"),bHeroProfile?TEXT("Explicit actor-position function staging outside/inside only; live animation and all saved materials retained; no OS input or traversal proof"):TEXT("No character transforms changed"));
    O->SetStringField(TEXT("pass_scope"),TEXT("Native raw PNG delivery and controlled-Hero/component shader readiness; art quality, OS input, all-world materials, gameplay and performance acceptance are not established"));
    O->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW")); O->SetStringField(TEXT("actual_map"),GetWorld()->GetOutermost()->GetName());
    O->SetStringField(TEXT("plan_file"),PlanFile); O->SetStringField(TEXT("actual_plan_sha1"),PlanSHA1);
    O->SetStringField(TEXT("sha256_verification_scope"),TEXT("Plan generator/launcher validates SHA256. Runtime records SHA1 of actual plan and checks source sizes/paths; it does not independently verify declared SHA256."));
    if (Plan) O->SetObjectField(TEXT("input_plan"),Plan);
    if (OriginalState) O->SetObjectField(TEXT("original_actual_state_before_overrides"),OriginalState);
    if (LastMaterialReadiness) O->SetObjectField(TEXT("latest_hero_component_material_readiness"),LastMaterialReadiness);
    O->SetNumberField(TEXT("global_shader_jobs_remaining"),ShaderJobs);
    O->SetNumberField(TEXT("global_streaming_resources_wanted"),WantingResources);
    O->SetBoolField(TEXT("os_input_used"),false); O->SetBoolField(TEXT("user_stop_latched"),bStopped);
    O->SetBoolField(TEXT("autoquit_explicit"),bAutoQuit); O->SetBoolField(TEXT("autoquit_pending"),bExitPending);
    O->SetStringField(TEXT("autoquit_policy"),TEXT("Explicit flag only: normal exit after every planned PNG or non-Escape FAIL; Escape permanently cancels exit"));
    O->SetBoolField(TEXT("startup_waiting"),bStarting); O->SetNumberField(TEXT("startup_attempt_count"),StartupAttempts);
    O->SetNumberField(TEXT("startup_deadline_seconds"),10);
    if (FirstStartupReadback) O->SetObjectField(TEXT("startup_first_readback"),FirstStartupReadback);
    if (LastStartupReadback) O->SetObjectField(TEXT("startup_latest_readback"),LastStartupReadback);
    O->SetNumberField(TEXT("stop_frame"),double(StopFrame)); O->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("sky_policy"),TEXT("Permanent authored time-of-day API controls actual lights, sky MID and lamp groups. Seven captures are review evidence, not automatic weather, input, flight or performance acceptance."));
    TArray<TSharedPtr<FJsonValue>> Rows; int32 SavedCount=0;
    for (int32 I=0;I<(bHeroProfile?4:7);++I)
    {
        if (Results.IsValidIndex(I))
        { Rows.Add(MakeShared<FJsonValueObject>(Results[I])); if (TextIs(Results[I],TEXT("status"),TEXT("PASS"))) ++SavedCount; }
        else
        {
            auto Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("index"),I); Row->SetStringField(TEXT("status"),TEXT("NOT_RUN"));
            if (Shots.IsValidIndex(I)) Row->SetStringField(TEXT("label"),Shots[I]->GetStringField(TEXT("label")));
            Row->SetStringField(TEXT("detail"),bStopped?TEXT("User stop; no further automation"):TEXT("No verified raw PNG"));
            if (I==ShotIndex && Pending) Row->SetObjectField(TEXT("unverified_request"),Pending);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    O->SetNumberField(TEXT("actual_verified_png_count"),SavedCount); O->SetArrayField(TEXT("captures"),Rows);
    FString JSON; const bool Saved=FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&JSON))
        && FFileHelper::SaveStringToFile(JSON,*(Directory/TEXT("corner_v2_review.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (!Saved) UE_LOG(LogTemp,Error,TEXT("M5VS2_CORNER_REV2_REVIEW_REPORT_WRITE_FAILED %s"),*Directory);
    return Saved;
}

void AHCM5VS2CornerRevTwoReviewDirector::RemoveDelegates()
{
    if (Viewport.IsValid())
    { if (InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle); if (DrawHandle.IsValid()) Viewport->OnEndDraw().Remove(DrawHandle); }
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    InputHandle.Reset(); DrawHandle.Reset(); ScreenshotHandle.Reset();
}

void AHCM5VS2CornerRevTwoReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if ((bStarting || bActive) && !bStopped) Finish(TEXT("NOT_RUN"),TEXT("World ended before seven verified native captures"));
    bExitPending=false; SetActorTickEnabled(false);
    RemoveDelegates(); if (!bStopped) Restore(); Super::EndPlay(Reason);
}
