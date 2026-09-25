#include "HCM5VS2CornerReviewDirector.h"
#include "HCM5VS2NPC.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

namespace
{
const FString EvidenceRoot = TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2");
const FString BaseMapPackage = TEXT("/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0");
bool OwnedCornerMap(const FString& Package)
{
    if (Package==BaseMapPackage) return true;
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/World/StyleReview_");
    if (!Package.StartsWith(Prefix)) return false;
    const FString Tail=Package.Mid(Prefix.Len());
    if (Tail.Len()<14 || Tail[12]!=TCHAR('/')) return false;
    for (int32 I=0; I<12; ++I)
        if (!(Tail[I]>=TCHAR('0') && Tail[I]<=TCHAR('9')) && !(Tail[I]>=TCHAR('a') && Tail[I]<=TCHAR('f'))) return false;
    const FString Name=Tail.Mid(13);
    return Name==TEXT("L_OriginalHandPainted") || Name==TEXT("L_SoftAnime") || Name==TEXT("L_WaterWorldScale");
}
const FName OwnerTag(TEXT("HarborCity_M5_VS2_HarborCorner_P0"));
const TCHAR* CameraNames[] = {TEXT("SouthStreet"), TEXT("Promenade"), TEXT("GuildSquare")};
const TCHAR* PortraitNames[] = {TEXT("HeroPortrait"), TEXT("QPortrait"), TEXT("RPortrait")};
const TCHAR* PortraitIDs[] = {TEXT("Hero"), TEXT("Q"), TEXT("R")};
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
    if (World) for (TActorIterator<T> It(World);It;++It) if (It->ActorHasTag(OwnerTag))
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

AHCM5VS2CornerReviewDirector::AHCM5VS2CornerReviewDirector()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}

void AHCM5VS2CornerReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2CornerReview"))) return;
    Started=FPlatformTime::Seconds(); bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    bStarting=true; SetActorTickEnabled(true);
    // The client may exist before its native viewport/controller. Observe Escape now,
    // including during validation and bounded readiness waiting; never consume it.
    ObserveViewport();
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root) || !Under(Root,EvidenceRoot))
    { Finish(TEXT("FAIL"),TEXT("Rejected evidence path; no report written outside the authorized root")); return; }
    Directory=Root/(TEXT("CornerReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true))
    { Directory.Empty(); Finish(TEXT("FAIL"),TEXT("Could not create evidence directory")); return; }
    FString Error;
    if (!LoadPlan(Error)) { Finish(TEXT("FAIL"),Error); return; }
    TryStart();
#endif
}

bool AHCM5VS2CornerReviewDirector::LoadPlan(FString& Error)
{
    Error=TEXT("Invalid bounded corner capture plan/schema or source binding");
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2CornerPlan="),PlanFile) || !Under(PlanFile,EvidenceRoot)
        || FPaths::GetCleanFilename(PlanFile)!=TEXT("corner_capture_plan.json")) return false;
    const int64 Bytes=IFileManager::Get().FileSize(*PlanFile);
    if (Bytes<1 || Bytes>1024*1024) return false;
    TArray<uint8> Raw; FString JSON;
    if (!FFileHelper::LoadFileToArray(Raw,*PlanFile) || !FFileHelper::LoadFileToString(JSON,*PlanFile)) return false;
    PlanSHA1=FSHA1::HashBuffer(Raw.GetData(),Raw.Num()).ToString();
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JSON),Plan) || !Plan) return false;
    FString Profile=TEXT("Base");
    if (Plan->HasField(TEXT("profile")) && !Plan->TryGetStringField(TEXT("profile"),Profile)) return false;
    if (Profile!=TEXT("Base") && Profile!=TEXT("Portraits")) return false;
    bPortraits=Profile==TEXT("Portraits");
    if (bPortraits)
    {
        const TArray<TSharedPtr<FJsonValue>>* Subjects=nullptr;
        if (!Plan->TryGetArrayField(TEXT("portrait_subjects"),Subjects) || Subjects->Num()!=3) return false;
        for (int32 I=0;I<3;++I)
        {
            const auto S=(*Subjects)[I]?(*Subjects)[I]->AsObject():nullptr;
            FString Class, NPCProfile;
            const FString NPCPrefix=FString(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_"))+PortraitIDs[I]+TEXT("/Runtime_");
            if (!TextIs(S,TEXT("id"),PortraitIDs[I]) || !S->TryGetStringField(TEXT("blueprint_class"),Class)
                || !TextIs(S,TEXT("left_eye_bone"),I==0?TEXT("LeftEye"):TEXT("J_Adj_L_FaceEye"))
                || !TextIs(S,TEXT("right_eye_bone"),I==0?TEXT("RightEye"):TEXT("J_Adj_R_FaceEye"))
                || !Number(S,TEXT("camera_distance_cm"),I==0?130:150,I==0?130:150)) return false;
            if (I==0)
            {
                if (Class!=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia.BP_M5VS2_Selestia_C")) return false;
            }
            else if (!Class.StartsWith(NPCPrefix) || Class.Contains(TEXT(".."))
                || !Class.EndsWith(FString(TEXT("/BP_AvatarSample_"))+PortraitIDs[I]+TEXT(".BP_AvatarSample_")+PortraitIDs[I]+TEXT("_C"))
                || !S->TryGetStringField(TEXT("npc_profile"),NPCProfile) || NPCProfile.Contains(TEXT(".."))
                || !NPCProfile.StartsWith(NPCPrefix) || !NPCProfile.EndsWith(TEXT("/DA_NPCProfile.DA_NPCProfile"))) return false;
        }
    }
    FString MapPackage;
    if (!Plan->TryGetStringField(TEXT("map"),MapPackage) || !OwnedCornerMap(MapPackage)) return false;
    if (!Number(Plan,TEXT("schema_version"),1,1) || !Number(Plan,TEXT("expected_screenshots"),6,6)
        || !TextIs(Plan,TEXT("plan_type"),TEXT("HARBOR_CORNER_NATIVE_VIEWPORT"))
        || !TextIs(Plan,TEXT("status"),TEXT("READY_FOR_RUNTIME_NOT_CAPTURED"))
        || !TextIs(Plan,TEXT("map"),MapPackage) || !TextIs(Plan,TEXT("owner"),OwnerTag.ToString())
        || !Array(Plan,TEXT("expected_resolution"),2,1080,1920)) return false;
    const auto& Resolution=Plan->GetArrayField(TEXT("expected_resolution"));
    if (Resolution[0]->AsNumber()!=1920 || Resolution[1]->AsNumber()!=1080) return false;
    const TSharedPtr<FJsonObject>* Bindings=nullptr;
    if (!Plan->TryGetObjectField(TEXT("actor_bindings"),Bindings)) return false;
    const TCHAR* Keys[]={TEXT("main_light"),TEXT("sky_light"),TEXT("post_process")};
    const TCHAR* Classes[]={TEXT("/Script/Engine.DirectionalLight"),TEXT("/Script/Engine.SkyLight"),TEXT("/Script/Engine.PostProcessVolume")};
    for (int32 I=0;I<3;++I)
    {
        const TSharedPtr<FJsonObject>* Binding=nullptr;
        if (!(*Bindings)->TryGetObjectField(Keys[I],Binding) || !TextIs(*Binding,TEXT("owner_tag"),OwnerTag.ToString())
            || !TextIs(*Binding,TEXT("class_path"),Classes[I])) return false;
    }
    const TSharedPtr<FJsonObject>* Sources=nullptr;
    if (!Plan->TryGetObjectField(TEXT("sources"),Sources)) return false;
    // SHA256 is verified by the plan generator/launcher. This runtime does not call
    // GenericPlatformMisc::GetSHA256Signature: Windows has no implementation in this UE build.
    for (const TCHAR* Key:{TEXT("layout"),TEXT("corner_author"),TEXT("village_copy"),TEXT("plan_script")})
    {
        const TSharedPtr<FJsonObject>* Source=nullptr; FString Path, Hash;
        if (!(*Sources)->TryGetObjectField(Key,Source) || !(*Source)->TryGetStringField(TEXT("path"),Path)
            || !Under(Path,FString(Key)==TEXT("plan_script")?TEXT("D:/科研学习/codex学习/tools"):EvidenceRoot)
            || !(*Source)->TryGetStringField(TEXT("sha256"),Hash) || Hash.Len()!=64
            || !Number(*Source,TEXT("bytes"),1,1024*1024*128)
            || IFileManager::Get().FileSize(*Path)!=(*Source)->GetNumberField(TEXT("bytes"))) return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Packages=nullptr; bool bMapFound=false;
    if (!(*Sources)->TryGetArrayField(TEXT("map_and_dedicated_packages"),Packages) || Packages->Num()<1 || Packages->Num()>32) return false;
    for (const auto& Value:*Packages)
    {
        const auto P=Value?Value->AsObject():nullptr; FString Path, Package, Hash;
        if (!P || !P->TryGetStringField(TEXT("path"),Path) || !Under(Path,FPaths::ProjectContentDir())
            || !P->TryGetStringField(TEXT("package"),Package) || !Package.StartsWith(TEXT("/Game/HarborCity/M5VS2/World/"))
            || Package.Contains(TEXT("..")) || !P->TryGetStringField(TEXT("sha256"),Hash) || Hash.Len()!=64
            || !Number(P,TEXT("bytes"),1,1024*1024*128) || IFileManager::Get().FileSize(*Path)!=P->GetNumberField(TEXT("bytes"))) return false;
        FString Expected=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/Package.Mid(6)+(Package==MapPackage?TEXT(".umap"):TEXT(".uasset")));
        FPaths::NormalizeFilename(Expected); if (!Path.Equals(Expected,ESearchCase::IgnoreCase)) return false;
        bMapFound|=Package==MapPackage;
    }
    if (!bMapFound) return false;
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if (!Plan->TryGetArrayField(TEXT("shots"),Rows) || Rows->Num()!=6) return false;
    for (int32 I=0;I<6;++I)
    {
        const auto S=(*Rows)[I]?(*Rows)[I]->AsObject():nullptr;
        const FString Preset=I<3?TEXT("Afternoon"):TEXT("Dusk");
        const TCHAR* Name=bPortraits?PortraitNames[I%3]:CameraNames[I%3];
        const FString Label=FString::Printf(TEXT("%02d_%s_%s"),I,*Preset,Name);
        if (!Number(S,TEXT("index"),I,I) || !TextIs(S,TEXT("label"),Label) || !TextIs(S,TEXT("expected_png"),Label+TEXT(".png"))
            || !TextIs(S,TEXT("capture_status"),TEXT("NOT_RUN")) || !TextIs(S,TEXT("camera_name"),Name)
            || !TextIs(S,TEXT("light_preset"),Preset) || !Number(S,TEXT("camera_fov_degrees"),bPortraits?40:20,bPortraits?40:100)
            || !Array(S,TEXT("sun_rotation_pitch_yaw_roll"),3,-180,180) || !Array(S,TEXT("sun_color_linear_rgba"),4,0,1)
            || !Number(S,TEXT("sun_intensity"),0,20) || !Number(S,TEXT("sky_intensity"),0,5)
            || !Number(S,TEXT("exposure_bias"),-4,4) || !Number(S,TEXT("bloom_intensity"),0,2)
            || !Number(S,TEXT("minimum_settle_seconds"),I%3==0?10:2,I%3==0?10:2)
            || !Number(S,TEXT("minimum_settle_rendered_frames"),30,30)
            || !Number(S,TEXT("same_camera_comparison_index"),I<3?I+3:I-3,I<3?I+3:I-3)) return false;
        if (bPortraits)
        {
            if (!TextIs(S,TEXT("portrait_subject"),PortraitIDs[I%3])
                || !TextIs(S,TEXT("camera_mode"),TEXT("LOCK_ACTUAL_ANIMATED_EYES_ON_FIRST_USE"))
                || S->HasField(TEXT("camera_location_cm")) || S->HasField(TEXT("camera_rotation_pitch_yaw_roll"))) return false;
        }
        else if (!Array(S,TEXT("camera_location_cm"),3,-10000,10000)
            || !Array(S,TEXT("camera_rotation_pitch_yaw_roll"),3,-180,180)) return false;
        if (!bPortraits && I>=3 && (!VectorField(S,TEXT("camera_location_cm")).Equals(VectorField(Shots[I-3],TEXT("camera_location_cm")),.001)
            || !RotField(S,TEXT("camera_rotation_pitch_yaw_roll")).Equals(RotField(Shots[I-3],TEXT("camera_rotation_pitch_yaw_roll")),.001)
            || S->GetNumberField(TEXT("camera_fov_degrees"))!=Shots[I-3]->GetNumberField(TEXT("camera_fov_degrees")))) return false;
        if (I%3!=0)
        {
            const auto& First=Shots[I-I%3];
            if (!RotField(S,TEXT("sun_rotation_pitch_yaw_roll")).Equals(RotField(First,TEXT("sun_rotation_pitch_yaw_roll")),.001)
                || !ColorField(S).Equals(ColorField(First),.001f)) return false;
            for (const TCHAR* Key:{TEXT("sun_intensity"),TEXT("sky_intensity"),TEXT("exposure_bias"),TEXT("bloom_intensity")})
                if (S->GetNumberField(Key)!=First->GetNumberField(Key)) return false;
        }
        Shots.Add(S);
    }
    return true;
}

void AHCM5VS2CornerReviewDirector::ObserveViewport()
{
    if (bStopped) return;
    UGameViewportClient* Live=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if (Live==Viewport.Get() && InputHandle.IsValid()) return;
    if (Viewport.IsValid())
    { if (InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle); if (DrawHandle.IsValid()) Viewport->OnEndDraw().Remove(DrawHandle); }
    InputHandle.Reset(); DrawHandle.Reset(); Viewport=Live;
    if (Live)
    {
        InputHandle=Live->OnInputKey().AddUObject(this,&AHCM5VS2CornerReviewDirector::Input);
        if (bActive) DrawHandle=Live->OnEndDraw().AddUObject(this,&AHCM5VS2CornerReviewDirector::Drawn);
    }
}

void AHCM5VS2CornerReviewDirector::TryStart()
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
                UE_LOG(LogTemp,Display,TEXT("M5VS2_CORNER_REVIEW_WAITING %s"),*Error);
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
    if (!Write(TEXT("RUNNING"),TEXT("Staged lighting review; six native viewport requests planned, no visual acceptance")))
        Finish(TEXT("FAIL"),TEXT("Could not save initial evidence report"));
}

bool AHCM5VS2CornerReviewDirector::Start(FString& Error, bool& bRetryable)
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
    int32 Directors=0; if (World) for (TActorIterator<AHCM5VS2CornerReviewDirector> It(World);It;++It) ++Directors;
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
    MainLight=UniqueTagged<ADirectionalLight>(World,Readback,TEXT("main_light_binding"));
    SkyLight=UniqueTagged<ASkyLight>(World,Readback,TEXT("sky_light_binding"));
    PostProcess=UniqueTagged<APostProcessVolume>(World,Readback,TEXT("post_process_binding"));
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
    if (bPortraits && !BindPortraitSubjects(Error)) return false;
    // Capture original actual state before camera, HUD or lighting overrides.
    OriginalView=Controller->GetViewTarget(); OriginalState=Snapshot();
    SavedSunRotation=MainLight->GetActorRotation(); SavedSunColor=MainLight->GetLightComponent()->GetLightColor();
    SavedSunIntensity=MainLight->GetLightComponent()->Intensity; SavedSkyIntensity=SkyLight->GetLightComponent()->Intensity;
    SavedPostProcess=PostProcess->Settings; bStateSaved=true;
    if (AHUD* HUD=Controller->GetHUD()) { bHUDSaved=true; bSavedHUD=HUD->bShowHUD; HUD->bShowHUD=false; }
    DrawHandle=Live->OnEndDraw().AddUObject(this,&AHCM5VS2CornerReviewDirector::Drawn);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2CornerReviewDirector::Processed);
    FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
    ReviewCamera=GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!ReviewCamera) { Error=TEXT("Failed to spawn transient native review camera"); return false; }
    ReviewCamera->GetCameraComponent()->bConstrainAspectRatio=false;
    ReviewCamera->GetCameraComponent()->PostProcessBlendWeight=0;
    return true;
}

bool AHCM5VS2CornerReviewDirector::BindPortraitSubjects(FString& Error)
{
    const auto& Subjects=Plan->GetArrayField(TEXT("portrait_subjects"));
    for (int32 I=0;I<3;++I)
    {
        const auto S=Subjects[I]->AsObject(); ACharacter* Found=nullptr; int32 Count=0;
        for (TActorIterator<ACharacter> It(GetWorld());It;++It)
            if (It->GetClass()->GetPathName()==S->GetStringField(TEXT("blueprint_class"))) { Found=*It; ++Count; }
        if (Count!=1 || !IsValid(Found) || !Found->GetMesh() || Found->IsHidden())
        { Error=FString::Printf(TEXT("Portrait %s requires exactly one visible actual character; count=%d"),PortraitIDs[I],Count); return false; }
        if (I==0 && Controller->GetPawn()!=Found)
        { Error=TEXT("Portrait Hero is not the actual controlled pawn"); return false; }
        if (I>0)
        {
            const auto* NPC=Cast<AHCM5VS2NPC>(Found);
            if (!NPC || GetPathNameSafe(NPC->NPCProfile.Get())!=S->GetStringField(TEXT("npc_profile")))
            { Error=TEXT("Portrait NPC exact profile mismatch"); return false; }
        }
        PortraitSubjects[I]=Found;
    }
    return true;
}

bool AHCM5VS2CornerReviewDirector::PortraitSubjectsValid(FString& Error) const
{
    const auto& Subjects=Plan->GetArrayField(TEXT("portrait_subjects"));
    for (int32 I=0;I<3;++I)
    {
        const auto S=Subjects[I]->AsObject(); const auto* Person=PortraitSubjects[I].Get(); int32 Count=0;
        for (TActorIterator<ACharacter> It(GetWorld());It;++It)
            if (It->GetClass()->GetPathName()==S->GetStringField(TEXT("blueprint_class"))) ++Count;
        if (!Person || Count!=1 || Person->IsHidden() || !Person->GetMesh()
            || Person->GetClass()->GetPathName()!=S->GetStringField(TEXT("blueprint_class")))
        { Error=TEXT("Portrait character binding was lost, duplicated or hidden"); return false; }
        if (I==0 && Controller->GetPawn()!=Person)
        { Error=TEXT("Portrait controlled Hero changed"); return false; }
        if (I>0)
        {
            const auto* NPC=Cast<AHCM5VS2NPC>(Person);
            if (!NPC || GetPathNameSafe(NPC->NPCProfile.Get())!=S->GetStringField(TEXT("npc_profile")))
            { Error=TEXT("Portrait NPC profile changed"); return false; }
        }
    }
    return true;
}

TSharedPtr<FJsonObject> AHCM5VS2CornerReviewDirector::PortraitSubjectSnapshot(int32 I) const
{
    auto O=MakeShared<FJsonObject>(); const auto S=Plan->GetArrayField(TEXT("portrait_subjects"))[I]->AsObject();
    O->SetStringField(TEXT("subject"),PortraitIDs[I]); O->SetNumberField(TEXT("frame"),double(GFrameCounter));
    const auto* Person=PortraitSubjects[I].Get(); const auto* Mesh=Person?Person->GetMesh():nullptr;
    O->SetStringField(TEXT("actor"),GetPathNameSafe(Person)); O->SetBoolField(TEXT("valid"),Person && Mesh);
    if (!Person || !Mesh) return O;
    O->SetStringField(TEXT("blueprint_class"),Person->GetClass()->GetPathName());
    O->SetArrayField(TEXT("actor_location_cm"),V3(Person->GetActorLocation()));
    O->SetArrayField(TEXT("actor_rotation_pitch_yaw_roll"),Rotation(Person->GetActorRotation()));
    O->SetArrayField(TEXT("actor_scale"),V3(Person->GetActorScale3D()));
    O->SetStringField(TEXT("mesh"),GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
    O->SetStringField(TEXT("animation_class"),Mesh->GetAnimInstance()?Mesh->GetAnimInstance()->GetClass()->GetPathName():TEXT("None"));
    O->SetStringField(TEXT("postprocess_class"),Mesh->GetPostProcessInstance()?Mesh->GetPostProcessInstance()->GetClass()->GetPathName():TEXT("None"));
    const FName L(*S->GetStringField(TEXT("left_eye_bone"))), R(*S->GetStringField(TEXT("right_eye_bone")));
    O->SetStringField(TEXT("left_eye_bone"),L.ToString()); O->SetStringField(TEXT("right_eye_bone"),R.ToString());
    O->SetNumberField(TEXT("left_eye_index"),Mesh->GetBoneIndex(L)); O->SetNumberField(TEXT("right_eye_index"),Mesh->GetBoneIndex(R));
    if (Mesh->GetBoneIndex(L)!=INDEX_NONE && Mesh->GetBoneIndex(R)!=INDEX_NONE)
    {
        const FVector Left=Mesh->GetBoneLocation(L,EBoneSpaces::WorldSpace), Right=Mesh->GetBoneLocation(R,EBoneSpaces::WorldSpace);
        O->SetArrayField(TEXT("left_eye_world_cm"),V3(Left)); O->SetArrayField(TEXT("right_eye_world_cm"),V3(Right));
        O->SetArrayField(TEXT("eye_midpoint_world_cm"),V3((Left+Right)*.5));
    }
    TArray<TSharedPtr<FJsonValue>> Materials;
    for (int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot) Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Mesh->GetMaterial(Slot))));
    O->SetArrayField(TEXT("actual_materials"),Materials);
    return O;
}

bool AHCM5VS2CornerReviewDirector::LockPortraitCamera(int32 I, FString& Error)
{
    if (bStopped || !PortraitSubjectsValid(Error) || PortraitEyeSamples[I]) return false;
    const auto S=Plan->GetArrayField(TEXT("portrait_subjects"))[I]->AsObject();
    const auto* Person=PortraitSubjects[I].Get(); const auto* Mesh=Person->GetMesh();
    const FName L(*S->GetStringField(TEXT("left_eye_bone"))), R(*S->GetStringField(TEXT("right_eye_bone")));
    if (Mesh->GetBoneIndex(L)==INDEX_NONE || Mesh->GetBoneIndex(R)==INDEX_NONE)
    { Error=TEXT("Portrait requires actual named eye bones; no CDO/head/actor-height fallback"); return false; }
    const FVector Left=Mesh->GetBoneLocation(L,EBoneSpaces::WorldSpace), Right=Mesh->GetBoneLocation(R,EBoneSpaces::WorldSpace);
    const FVector Eye=(Left+Right)*.5;
    if (Left.ContainsNaN() || Right.ContainsNaN() || FVector::Dist(Left,Right)<1 || FVector::Dist(Left,Right)>25
        || FVector::Dist(Eye,Person->GetActorLocation())>250)
    { Error=TEXT("Portrait actual eye transforms are invalid or outside the bounded character scale"); return false; }
    const FVector Forward=FRotator(0,Person->GetActorRotation().Yaw,0).Vector();
    PortraitCameraLocations[I]=Eye+Forward*S->GetNumberField(TEXT("camera_distance_cm"));
    PortraitCameraRotations[I]=(-Forward).Rotation();
    PortraitEyeSamples[I]=PortraitSubjectSnapshot(I);
    PortraitEyeSamples[I]->SetStringField(TEXT("status"),TEXT("LOCKED_FROM_ACTUAL_ANIMATED_EYES"));
    PortraitEyeSamples[I]->SetBoolField(TEXT("cdo_fallback_used"),false);
    PortraitEyeSamples[I]->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);
    PortraitEyeSamples[I]->SetArrayField(TEXT("locked_camera_location_cm"),V3(PortraitCameraLocations[I]));
    PortraitEyeSamples[I]->SetArrayField(TEXT("locked_camera_rotation_pitch_yaw_roll"),Rotation(PortraitCameraRotations[I]));
    ReviewCamera->SetActorLocationAndRotation(PortraitCameraLocations[I],PortraitCameraRotations[I]);
    Controller->SetViewTarget(ReviewCamera); PortraitCameraLockedAt=FPlatformTime::Seconds(); PortraitCameraLockDraw=DrawCount;
    ReadyDraws=0;
    return true;
}

FVector AHCM5VS2CornerReviewDirector::ExpectedCameraLocation() const
{ return bPortraits?PortraitCameraLocations[ShotIndex%3]:VectorField(Shots[ShotIndex],TEXT("camera_location_cm")); }
FRotator AHCM5VS2CornerReviewDirector::ExpectedCameraRotation() const
{ return bPortraits?PortraitCameraRotations[ShotIndex%3]:RotField(Shots[ShotIndex],TEXT("camera_rotation_pitch_yaw_roll")); }
bool AHCM5VS2CornerReviewDirector::CameraMatchesRequest() const
{
    const auto* Camera=Controller?Controller->PlayerCameraManager.Get():nullptr;
    return Camera && Controller->GetViewTarget()==ReviewCamera && Viewport.IsValid() && Viewport->Viewport
        && Viewport->Viewport->GetSizeXY()==FIntPoint(1920,1080)
        && Camera->GetCameraLocation().Equals(ExpectedCameraLocation(),.1)
        && Camera->GetCameraRotation().Equals(ExpectedCameraRotation(),.05)
        && FMath::IsNearlyEqual(Camera->GetFOVAngle(),float(Shots[ShotIndex]->GetNumberField(TEXT("camera_fov_degrees"))),.01f);
}

bool AHCM5VS2CornerReviewDirector::BeginShot(int32 Index, FString& Error)
{
    if (bStopped || !Shots.IsValidIndex(Index) || !IsValid(MainLight) || !IsValid(SkyLight) || !IsValid(PostProcess) || !IsValid(ReviewCamera))
    { Error=TEXT("Capture binding disappeared or invalid shot index"); return false; }
    ShotIndex=Index; const auto& S=Shots[Index];
    MainLight->SetActorRotation(RotField(S,TEXT("sun_rotation_pitch_yaw_roll")));
    MainLight->GetLightComponent()->SetLightColor(ColorField(S));
    MainLight->GetLightComponent()->SetIntensity(float(S->GetNumberField(TEXT("sun_intensity"))));
    SkyLight->GetLightComponent()->SetIntensity(float(S->GetNumberField(TEXT("sky_intensity"))));
    auto& PP=PostProcess->Settings;
    PP.bOverride_AutoExposureMethod=true; PP.AutoExposureMethod=AEM_Manual;
    PP.bOverride_AutoExposureApplyPhysicalCameraExposure=true; PP.AutoExposureApplyPhysicalCameraExposure=false;
    PP.bOverride_AutoExposureBias=true; PP.AutoExposureBias=float(S->GetNumberField(TEXT("exposure_bias")));
    PP.bOverride_BloomIntensity=true; PP.BloomIntensity=float(S->GetNumberField(TEXT("bloom_intensity")));
    if (!bPortraits || PortraitEyeSamples[Index%3])
    {
        ReviewCamera->SetActorLocationAndRotation(ExpectedCameraLocation(),ExpectedCameraRotation());
        Controller->SetViewTarget(ReviewCamera);
    }
    ReviewCamera->GetCameraComponent()->SetFieldOfView(float(S->GetNumberField(TEXT("camera_fov_degrees"))));
    ShotStarted=FPlatformTime::Seconds(); ShotDrawStart=DrawCount; ReadyDraws=0; bReady=false;
    return true;
}

bool AHCM5VS2CornerReviewDirector::Ready()
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

TSharedPtr<FJsonObject> AHCM5VS2CornerReviewDirector::Snapshot() const
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
        if (bPortraits)
        {
            P->SetArrayField(TEXT("rotation_pitch_yaw_roll"),Rotation(Person->GetActorRotation()));
            P->SetArrayField(TEXT("scale"),V3(Person->GetActorScale3D()));
        }
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
        People.Add(MakeShared<FJsonValueObject>(P));
    }
    O->SetArrayField(TEXT("characters_unmodified_by_director"),People);
    if (bPortraits)
    {
        TArray<TSharedPtr<FJsonValue>> Subjects;
        for (int32 I=0;I<3;++I) Subjects.Add(MakeShared<FJsonValueObject>(PortraitSubjectSnapshot(I)));
        O->SetArrayField(TEXT("portrait_subjects_actual"),Subjects);
    }
    return O;
}

void AHCM5VS2CornerReviewDirector::Tick(float DeltaSeconds)
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
    if (Now-Started>120) { Finish(TEXT("FAIL"),TEXT("120 second bounded review deadline")); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (bPending)
    {
        if (bProcessed && !FScreenshotRequest::IsScreenshotRequested() && GFrameCounter>RequestFrame)
        {
            int32 W=0,H=0;
            const bool FinalViewValid=!bPortraits || (Pending->HasField(TEXT("final_view_matches_locked_camera"))
                && Pending->GetBoolField(TEXT("final_view_matches_locked_camera")));
            const bool Valid=ValidPNG(PendingPNG,W,H) && FinalViewValid;
            Pending->SetStringField(TEXT("status"),Valid?TEXT("PASS"):TEXT("FAIL"));
            Pending->SetNumberField(TEXT("processed_frame"),double(ProcessedFrame)); Pending->SetNumberField(TEXT("width"),W); Pending->SetNumberField(TEXT("height"),H);
            Pending->SetNumberField(TEXT("file_bytes"),IFileManager::Get().FileSize(*PendingPNG)); Results.Add(Pending); Pending.Reset(); bPending=false;
            if (!Valid) { Finish(TEXT("FAIL"),TEXT("Native screenshot missing/not 1920x1080 or portrait final view did not match locked camera")); return; }
            if (ShotIndex==5) { Finish(TEXT("PASS"),TEXT("Six actual native staged-lighting PNGs saved; visual acceptance remains USER_REVIEW")); return; }
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
    if (bPortraits)
    {
        FString Error;
        if (!PortraitSubjectsValid(Error)) { Finish(TEXT("FAIL"),Error); return; }
        if (!PortraitEyeSamples[ShotIndex%3])
        {
            if (!LockPortraitCamera(ShotIndex%3,Error)) Finish(TEXT("FAIL"),Error);
            return;
        }
        if (Now-PortraitCameraLockedAt<2 || DrawCount-PortraitCameraLockDraw<30) return;
    }
    Capture();
}

void AHCM5VS2CornerReviewDirector::Capture()
{
    if (bStopped || !Viewport.IsValid() || !Viewport->Viewport) return;
    const auto& S=Shots[ShotIndex];
    if ((bPortraits && !PortraitEyeSamples[ShotIndex%3]) || !CameraMatchesRequest())
    { Finish(TEXT("FAIL"),TEXT("Actual PlayerCameraManager view/resolution does not match requested shot")); return; }
    const auto* Light=MainLight->GetLightComponent();
    if (!MainLight->GetActorRotation().Equals(RotField(S,TEXT("sun_rotation_pitch_yaw_roll")),.05)
        || !Light->GetLightColor().Equals(ColorField(S),.015f)
        || !FMath::IsNearlyEqual(Light->Intensity,float(S->GetNumberField(TEXT("sun_intensity"))),.001f)
        || !FMath::IsNearlyEqual(SkyLight->GetLightComponent()->Intensity,float(S->GetNumberField(TEXT("sky_intensity"))),.001f)
        || !FMath::IsNearlyEqual(PostProcess->Settings.AutoExposureBias,float(S->GetNumberField(TEXT("exposure_bias"))),.001f)
        || !FMath::IsNearlyEqual(PostProcess->Settings.BloomIntensity,float(S->GetNumberField(TEXT("bloom_intensity"))),.001f))
    { Finish(TEXT("FAIL"),TEXT("Actual staged lighting readback does not match requested shot (color tolerance accounts for FColor quantization)")); return; }
    PendingPNG=Directory/S->GetStringField(TEXT("expected_png"));
    if (IFileManager::Get().FileExists(*PendingPNG)) { Finish(TEXT("FAIL"),TEXT("Refuse PNG overwrite")); return; }
    Pending=MakeShared<FJsonObject>(); Pending->SetNumberField(TEXT("index"),ShotIndex);
    Pending->SetStringField(TEXT("label"),S->GetStringField(TEXT("label"))); Pending->SetStringField(TEXT("file"),PendingPNG);
    Pending->SetStringField(TEXT("status"),TEXT("REQUESTED_NOT_YET_SAVED")); Pending->SetStringField(TEXT("lighting_origin"),TEXT("STAGED_LIGHTING_OVERRIDE_NOT_DEFAULT_CAPTURE"));
    Pending->SetObjectField(TEXT("requested"),S); Pending->SetObjectField(TEXT("actual_at_request"),Snapshot());
    if (bPortraits)
    {
        Pending->SetObjectField(TEXT("portrait_eye_lock"),PortraitEyeSamples[ShotIndex%3]);
        Pending->SetArrayField(TEXT("resolved_camera_location_cm"),V3(ExpectedCameraLocation()));
        Pending->SetArrayField(TEXT("resolved_camera_rotation_pitch_yaw_roll"),Rotation(ExpectedCameraRotation()));
        Pending->SetBoolField(TEXT("request_view_matches_locked_camera"),true);
    }
    Pending->SetNumberField(TEXT("request_frame"),double(GFrameCounter)); Pending->SetNumberField(TEXT("viewport_draws_since_change"),double(DrawCount-ShotDrawStart));
    Pending->SetNumberField(TEXT("consecutive_ready_viewport_draws"),ReadyDraws); Pending->SetNumberField(TEXT("settle_wall_seconds"),FPlatformTime::Seconds()-ShotStarted);
    Pending->SetNumberField(TEXT("shader_jobs_remaining"),ShaderJobs); Pending->SetNumberField(TEXT("streaming_resources_wanted"),WantingResources);
    Pending->SetStringField(TEXT("material_fallback_validation"),TEXT("NOT_RUN: global job and streaming counters only; per-material render proxy completeness is not established"));
    bPending=true; bProcessed=false; RequestFrame=GFrameCounter; Requested=FPlatformTime::Seconds();
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}

void AHCM5VS2CornerReviewDirector::Drawn()
{
    if (!bActive || bStopped) return; ++DrawCount;
    if (bReady && !UGameplayStatics::IsGamePaused(this)) ++ReadyDraws;
    if (bPending && Pending && GFrameCounter==RequestFrame)
    {
        Pending->SetObjectField(TEXT("actual_at_viewport_end_draw"),Snapshot());
        if (bPortraits) Pending->SetBoolField(TEXT("final_view_matches_locked_camera"),CameraMatchesRequest());
    }
}
void AHCM5VS2CornerReviewDirector::Processed()
{ if (!bStopped && bPending) { bProcessed=true; ProcessedFrame=GFrameCounter; } }

void AHCM5VS2CornerReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if ((bStarting || bActive || bExitPending) && !bStopped && Event.Event==IE_Pressed && Event.Key==EKeys::Escape)
    {
        bStopped=true; bStarting=false; bActive=false; bExitPending=false; bAutoQuit=false; StopFrame=GFrameCounter;
        if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
        bPending=false; SetActorTickEnabled(false);
        Write(TEXT("NOT_RUN"),TEXT("Viewport Escape permanently stopped all camera/light/capture/restore/auto-exit actions. Event observed, not consumed; ordinary P route unchanged."));
    }
}

void AHCM5VS2CornerReviewDirector::Restore()
{
    if (bStopped || !bStateSaved) return; bStateSaved=false;
    if (IsValid(MainLight))
    { MainLight->SetActorRotation(SavedSunRotation); MainLight->GetLightComponent()->SetLightColor(SavedSunColor); MainLight->GetLightComponent()->SetIntensity(SavedSunIntensity); }
    if (IsValid(SkyLight)) SkyLight->GetLightComponent()->SetIntensity(SavedSkyIntensity);
    if (IsValid(PostProcess)) PostProcess->Settings=SavedPostProcess;
    if (IsValid(Controller))
    { if (OriginalView.IsValid()) Controller->SetViewTarget(OriginalView.Get()); if (AHUD* HUD=Controller->GetHUD()) if (bHUDSaved) HUD->bShowHUD=bSavedHUD; }
}

void AHCM5VS2CornerReviewDirector::Finish(const FString& Status, const FString& Detail)
{
    if (bStopped) return; bStarting=false; bActive=false;
    if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    bPending=false; Restore(); Finished=FPlatformTime::Seconds();
    const bool bSixVerified=Results.Num()==6 && Results.ContainsByPredicate([](const TSharedPtr<FJsonObject>& Result)
        { return !TextIs(Result,TEXT("status"),TEXT("PASS")); })==false;
    bExitPending=bAutoQuit && (Status==TEXT("FAIL") || (Status==TEXT("PASS") && bSixVerified));
    ExitReason=TEXT("M5VS2 corner staged lighting review ")+Status;
    // A failed report write must not leave an explicitly auto-quitting failed run idle.
    // The log remains the evidence for an inaccessible/rejected report destination.
    if (!Write(Status,Detail))
    {
        bExitPending=bAutoQuit; ExitReason=TEXT("M5VS2 corner review FAIL: evidence report write failed");
        UE_LOG(LogTemp,Error,TEXT("M5VS2_CORNER_REVIEW_FAIL report write failed; autoquit_explicit=%d"),bAutoQuit);
    }
    SetActorTickEnabled(bExitPending);
    UE_LOG(LogTemp,Display,TEXT("M5VS2_CORNER_REVIEW_%s %s %s"),*Status,*Directory,*Detail);
}

bool AHCM5VS2CornerReviewDirector::Write(const FString& Status, const FString& Detail)
{
    if (Directory.IsEmpty()) return false;
    auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("status"),Status); O->SetStringField(TEXT("detail"),Detail);
    O->SetStringField(TEXT("title"),TEXT("Harbor Corner P0 - native staged lighting review"));
    if (bPortraits)
    {
        O->SetStringField(TEXT("profile"),TEXT("Portraits"));
        O->SetStringField(TEXT("portrait_scope"),TEXT("Same authored corner and two existing light presets. Three actual eye-locked cameras; no character pose, transform, material, animation or extra-light changes. Static portraits do not establish gameplay or per-material render proxy readiness."));
        TArray<TSharedPtr<FJsonValue>> Samples;
        for (int32 I=0;I<3;++I) if (PortraitEyeSamples[I]) Samples.Add(MakeShared<FJsonValueObject>(PortraitEyeSamples[I]));
        O->SetArrayField(TEXT("portrait_eye_locks"),Samples);
    }
    O->SetStringField(TEXT("pass_scope"),TEXT("Native PNG delivery only; art quality, individual material fallback, OS input and gameplay acceptance are not established"));
    O->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW")); O->SetStringField(TEXT("actual_map"),GetWorld()->GetOutermost()->GetName());
    O->SetStringField(TEXT("plan_file"),PlanFile); O->SetStringField(TEXT("actual_plan_sha1"),PlanSHA1);
    O->SetStringField(TEXT("sha256_verification_scope"),TEXT("Plan generator/launcher validates SHA256. Runtime records SHA1 of actual plan and checks source sizes/paths; it does not independently verify declared SHA256."));
    if (Plan) O->SetObjectField(TEXT("input_plan"),Plan);
    if (OriginalState) O->SetObjectField(TEXT("original_actual_state_before_overrides"),OriginalState);
    O->SetBoolField(TEXT("os_input_used"),false); O->SetBoolField(TEXT("user_stop_latched"),bStopped);
    O->SetBoolField(TEXT("autoquit_explicit"),bAutoQuit); O->SetBoolField(TEXT("autoquit_pending"),bExitPending);
    O->SetStringField(TEXT("autoquit_policy"),TEXT("Explicit flag only: normal exit after six verified PNGs or non-Escape FAIL; Escape permanently cancels exit"));
    O->SetBoolField(TEXT("startup_waiting"),bStarting); O->SetNumberField(TEXT("startup_attempt_count"),StartupAttempts);
    O->SetNumberField(TEXT("startup_deadline_seconds"),10);
    if (FirstStartupReadback) O->SetObjectField(TEXT("startup_first_readback"),FirstStartupReadback);
    if (LastStartupReadback) O->SetObjectField(TEXT("startup_latest_readback"),LastStartupReadback);
    O->SetNumberField(TEXT("stop_frame"),double(StopFrame)); O->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("sky_policy"),TEXT("Consult the source-bound input_plan sky policy and saved author readback for the actual atmosphere. Dusk is a staged lighting comparison; no completed playable time/weather system is claimed."));
    TArray<TSharedPtr<FJsonValue>> Rows; int32 SavedCount=0;
    for (int32 I=0;I<6;++I)
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
        && FFileHelper::SaveStringToFile(JSON,*(Directory/TEXT("corner_review.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (!Saved) UE_LOG(LogTemp,Error,TEXT("M5VS2_CORNER_REVIEW_REPORT_WRITE_FAILED %s"),*Directory);
    return Saved;
}

void AHCM5VS2CornerReviewDirector::RemoveDelegates()
{
    if (Viewport.IsValid())
    { if (InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle); if (DrawHandle.IsValid()) Viewport->OnEndDraw().Remove(DrawHandle); }
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    InputHandle.Reset(); DrawHandle.Reset(); ScreenshotHandle.Reset();
}
void AHCM5VS2CornerReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if ((bStarting || bActive) && !bStopped) Finish(TEXT("NOT_RUN"),TEXT("World ended before six verified native captures"));
    bExitPending=false; SetActorTickEnabled(false);
    RemoveDelegates(); if (!bStopped) Restore(); Super::EndPlay(Reason);
}
