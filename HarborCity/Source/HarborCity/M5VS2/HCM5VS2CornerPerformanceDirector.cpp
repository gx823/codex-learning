#include "HCM5VS2CornerPerformanceDirector.h"
#include "HCM5VS2CornerTimeDirector.h"
#include "M1/HCM1PlayerController.h"
#include "M3/HCM3Recording.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProcess.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dxgi1_4.h>
#include <d3d12.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// QueryVideoMemoryInfo describes THIS process's usage on the actual D3D12
// adapter, not texture-pool allocation and not all applications' device usage.
// https://learn.microsoft.com/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo
struct FHCM5VS2ProcessVideoMemory
{
    FString Status=TEXT("NOT_RUN"),AdapterName,AdapterLuid,Reason; uint64 DedicatedBytes=0;uint32 Nodes=0;
#if PLATFORM_WINDOWS
    void* Library=nullptr;IDXGIAdapter3* Adapter=nullptr;
#endif
    ~FHCM5VS2ProcessVideoMemory()
    {
#if PLATFORM_WINDOWS
        if(Adapter)Adapter->Release();if(Library)FPlatformProcess::FreeDllHandle(Library);
#endif
    }
    bool Initialize()
    {
#if PLATFORM_WINDOWS
        if(!GDynamicRHI||FString(GDynamicRHI->GetName())!=TEXT("D3D12")){Reason=TEXT("Actual RHI is not D3D12");return false;}
        IUnknown* Native=static_cast<IUnknown*>(GDynamicRHI->RHIGetNativeDevice());ID3D12Device* Device=nullptr;
        if(!Native||FAILED(Native->QueryInterface(__uuidof(ID3D12Device),reinterpret_cast<void**>(&Device)))){Reason=TEXT("No actual native D3D12 device");return false;}
        const LUID Id=Device->GetAdapterLuid();Nodes=Device->GetNodeCount();Device->Release();
        if(Nodes!=1){Reason=TEXT("Multiple GPU nodes are outside this single-node sampler");return false;}
        AdapterLuid=FString::Printf(TEXT("%08x:%08x"),uint32(Id.HighPart),Id.LowPart);
        Library=FPlatformProcess::GetDllHandle(TEXT("dxgi.dll"));
        using FCreate=HRESULT(WINAPI*)(REFIID,void**);
        FCreate Create=Library?reinterpret_cast<FCreate>(FPlatformProcess::GetDllExport(Library,TEXT("CreateDXGIFactory1"))):nullptr;
        IDXGIFactory4* Factory=nullptr;
        if(!Create||FAILED(Create(__uuidof(IDXGIFactory4),reinterpret_cast<void**>(&Factory)))){Reason=TEXT("DXGI factory unavailable");return false;}
        const HRESULT H=Factory->EnumAdapterByLuid(Id,__uuidof(IDXGIAdapter3),reinterpret_cast<void**>(&Adapter));Factory->Release();
        if(FAILED(H)||!Adapter){Reason=TEXT("Could not resolve the actual RHI adapter LUID");return false;}
        DXGI_ADAPTER_DESC1 Desc{};
        if(FAILED(Adapter->GetDesc1(&Desc))){Reason=TEXT("Adapter description unavailable");return false;}
        AdapterName=Desc.Description;DedicatedBytes=uint64(Desc.DedicatedVideoMemory);
        Status=TEXT("READY");return true;
#else
        Reason=TEXT("Windows DXGI unavailable on this platform");return false;
#endif
    }
    bool Sample(uint64& Local,uint64& Shared,uint64& Budget)
    {
#if PLATFORM_WINDOWS
        if(!Adapter||Status!=TEXT("READY"))return false;
        DXGI_QUERY_VIDEO_MEMORY_INFO L{},S{};
        const HRESULT A=Adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&L);
        const HRESULT B=Adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,&S);
        if(FAILED(A)||FAILED(B)){Reason=TEXT("DXGI per-process usage query failed");return false;}
        Local=L.CurrentUsage;Shared=S.CurrentUsage;Budget=L.Budget;return true;
#else
        return false;
#endif
    }
};

void FHCM5VS2ProcessVideoMemoryDeleter::operator()(FHCM5VS2ProcessVideoMemory* Value) const
{delete Value;}

namespace
{
const FString Docs=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2");
const TCHAR* Groups[]={TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality")};
TArray<TSharedPtr<FJsonValue>> Vector(const FVector& V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
bool Under(FString& P,const FString& Root){if(P.IsEmpty()||FPaths::IsRelative(P))return false;P=FPaths::ConvertRelativePathToFull(P);FPaths::NormalizeFilename(P);return FPaths::CollapseRelativeDirectories(P)&&FPaths::IsUnderDirectory(P,Root)&&!P.Mid(2).Contains(TEXT(":"));}
double Variable(const TCHAR* Name){const auto* V=IConsoleManager::Get().FindConsoleVariable(Name);return V?V->GetFloat():-99999.;}
FVector Point(const TSharedPtr<FJsonValue>& Value)
{const auto& A=Value->AsArray();return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());}
}

AHCM5VS2CornerPerformanceDirector::AHCM5VS2CornerPerformanceDirector()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.bTickEvenWhenPaused=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
AHCM5VS2CornerPerformanceDirector::~AHCM5VS2CornerPerformanceDirector()=default;

void AHCM5VS2CornerPerformanceDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if(!FParse::Param(FCommandLine::Get(),TEXT("M5VS2CornerPerformance")))return;
    Started=LastTick=FPlatformTime::Seconds();bStarting=true;bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));SetActorTickEnabled(true);Observe();
    FString Root;
    if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root)||!Under(Root,Docs)){Finish(TEXT("FAIL"),TEXT("Rejected evidence root"));return;}
    Directory=Root/(TEXT("CornerPerformance_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if(!IFileManager::Get().MakeDirectory(*Directory,true)){Directory.Empty();Finish(TEXT("FAIL"),TEXT("Evidence directory failed"));return;}
    FString Error;if(!LoadPlan(Error)){Finish(TEXT("FAIL"),Error);return;}
#endif
}

bool AHCM5VS2CornerPerformanceDirector::LoadPlan(FString& Error)
{
    Error=TEXT("Invalid source-bound performance plan");
    if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2CornerPerformancePlan="),PlanFile)||!Under(PlanFile,Docs)||FPaths::GetCleanFilename(PlanFile)!=TEXT("corner_v2_performance_plan.json"))return false;
    if(IFileManager::Get().FileSize(*PlanFile)<=0||IFileManager::Get().FileSize(*PlanFile)>2*1024*1024)return false;
    FString Text;TArray<uint8> Bytes;
    if(!FFileHelper::LoadFileToString(Text,*PlanFile)||!FFileHelper::LoadFileToArray(Bytes,*PlanFile)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Plan)||!Plan)return false;
    PlanSHA1=FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num()).ToString();
    FString Type,Map,State,PlanOwner;
    if(!Plan->TryGetStringField(TEXT("plan_type"),Type)||Type!=TEXT("HARBOR_CORNER_REV2_PERFORMANCE")||!Plan->TryGetStringField(TEXT("status"),State)||State!=TEXT("READY_FOR_RUNTIME_NOT_MEASURED")
       ||!Plan->TryGetStringField(TEXT("map"),Map)||Map!=GetWorld()->GetOutermost()->GetName()||!Map.StartsWith(TEXT("/Game/HarborCity/M5VS2/WorldRev2/Performance_"))
       ||!Map.EndsWith(TEXT("/L_CornerRevTwoPerformance"))||Map.Contains(TEXT(".."))||!Plan->TryGetStringField(TEXT("owner"),PlanOwner)||PlanOwner!=TEXT("HarborCity_M5VS2_HarborCorner_Rev2"))return false;
    double Schema=0,Warm=0,Measure=0;
    if(!Plan->TryGetNumberField(TEXT("schema_version"),Schema)||Schema!=1||!Plan->TryGetNumberField(TEXT("warmup_seconds"),Warm)||Warm!=30
       ||!Plan->TryGetNumberField(TEXT("measurement_seconds"),Measure)||Measure!=65)return false;
    WarmTarget=Warm;MeasureTarget=Measure;
    FString PeriodName;
    if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2PerfQuality="),Quality)||(Quality!=TEXT("High")&&Quality!=TEXT("Epic"))
       ||!FParse::Value(FCommandLine::Get(),TEXT("M5VS2PerfPeriod="),PeriodName)||(PeriodName!=TEXT("Afternoon")&&PeriodName!=TEXT("Dusk")&&PeriodName!=TEXT("Night")))return false;
    QualityLevel=Quality==TEXT("High")?2:3;Period=FName(PeriodName);
    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
    if(!Plan->TryGetArrayField(TEXT("road_camera_points_cm"),Points)||Points->Num()<5||Points->Num()>200)return false;
    for(const auto& V:*Points)
    {
        const TArray<TSharedPtr<FJsonValue>>* Coordinates=nullptr;if(!V||!V->TryGetArray(Coordinates)||Coordinates->Num()!=3)return false;
        for(const auto& C:*Coordinates){double N;if(!C->TryGetNumber(N)||!FMath::IsFinite(N)||FMath::Abs(N)>2500)return false;}
        const FVector P=Point(V);if(P.Z!=170)return false;
        if(!Route.IsEmpty()){const double D=FVector::Dist(P,Route.Last());if(D<.01)return false;RouteLength+=D;}
        Route.Add(P);RouteDistances.Add(RouteLength);
    }
    return RouteLength>=3000&&RouteLength<=4500;
}

void AHCM5VS2CornerPerformanceDirector::Observe()
{
    if(bStopped)return;auto* Live=GetWorld()?GetWorld()->GetGameViewport():nullptr;
    if(Live==Viewport.Get()&&InputHandle.IsValid())return;
    if(Viewport.IsValid()){Viewport->OnInputKey().Remove(InputHandle);Viewport->OnEndDraw().Remove(DrawHandle);}
    InputHandle.Reset();DrawHandle.Reset();Viewport=Live;
    if(Live){InputHandle=Live->OnInputKey().AddUObject(this,&AHCM5VS2CornerPerformanceDirector::Input);DrawHandle=Live->OnEndDraw().AddUObject(this,&AHCM5VS2CornerPerformanceDirector::Drawn);}
}

bool AHCM5VS2CornerPerformanceDirector::Bind(FString& Error)
{
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if(!GEngine||!PC||!PC->PlayerCameraManager||!Viewport.IsValid()||!Viewport->Viewport){Error=TEXT("Waiting for actual viewport/controller");return false;}
    int32 Count=0;for(TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld());It;++It){TimeDirector=*It;++Count;}
    if(Count!=1){Error=TEXT("Exactly one permanent time director required");return false;}
    InitialSettings=Settings();OriginalView=PC->GetViewTarget();OriginalPeriod=TimeDirector->CurrentPeriod;
    bOriginalSmooth=GEngine->bSmoothFrameRate;bOriginalFixed=GEngine->bUseFixedFrameRate;bOriginalFixedStep=FApp::UseFixedTimeStep();bSavedState=true;
    GEngine->bSmoothFrameRate=false;GEngine->bUseFixedFrameRate=false;FApp::SetUseFixedTimeStep(false);
    if(!TimeDirector->SetTimeOfDay(Period)){Error=TEXT("Actual playable lighting API failed");return false;}
    LightingReadback=TimeDirector->GetLightingDiagnostics();
    Camera=GetWorld()->SpawnActor<ACameraActor>();if(!Camera){Error=TEXT("Native route camera unavailable");return false;}
    Camera->GetCameraComponent()->SetFieldOfView(65);Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
    PC->SetViewTarget(Camera);MoveCamera(0);InitialScene=Scene();
    VideoMemory.Reset(new FHCM5VS2ProcessVideoMemory());VideoMemory->Initialize();
    if(!Conditions(Error))return false;
    Frames.Reserve(120000);ResourceSamples.Reserve(70);bStarting=false;bActive=true;LastTick=FPlatformTime::Seconds();
    Write(TEXT("RUNNING"),TEXT("Separate warmup started; no performance samples yet"));return true;
}

bool AHCM5VS2CornerPerformanceDirector::Conditions(FString& Error) const
{
    Error=TEXT("Actual 1080p quality/uncapped/no-capture condition failed");
    if(!Viewport.IsValid()||!Viewport->Viewport||Viewport->Viewport->GetSizeXY()!=FIntPoint(1920,1080)||!GEngine||GEngine->bSmoothFrameRate||GEngine->bUseFixedFrameRate||FApp::UseFixedTimeStep())return false;
    if(Variable(TEXT("t.MaxFPS"))!=0||Variable(TEXT("r.VSync"))!=0||Variable(TEXT("r.ScreenPercentage"))!=100||Variable(TEXT("r.DynamicRes.OperationMode"))!=0)return false;
    for(const TCHAR* Name:Groups)if(Variable(Name)!=QualityLevel)return false;
    if(FScreenshotRequest::IsScreenshotRequested())return false;
    for(const TCHAR* Name:{TEXT("r.Streamline.DLSSG.Enable"),TEXT("r.FidelityFX.FI.Enabled"),TEXT("r.NGX.DLSS.Enable")})
        if(const auto* V=IConsoleManager::Get().FindConsoleVariable(Name))if(V->GetInt()!=0)return false;
    if(PC&&Camera&&PC->GetViewTarget()!=Camera){Error=TEXT("Native route ViewTarget changed");return false;}
    if(TimeDirector&&TimeDirector->CurrentPeriod!=Period){Error=TEXT("Playable lighting period changed during matched benchmark");return false;}
    return true;
}

TSharedPtr<FJsonObject> AHCM5VS2CornerPerformanceDirector::Settings() const
{
    auto J=MakeShared<FJsonObject>();auto C=MakeShared<FJsonObject>();
    for(const TCHAR* Name:Groups)C->SetNumberField(Name,Variable(Name));
    for(const TCHAR* Name:{TEXT("sg.ResolutionQuality"),TEXT("r.ScreenPercentage"),TEXT("r.SecondaryScreenPercentage.GameViewport"),TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("r.DynamicRes.OperationMode"),TEXT("r.Streamline.DLSSG.Enable"),TEXT("r.FidelityFX.FI.Enabled"),TEXT("r.NGX.DLSS.Enable")})
    {if(auto* V=IConsoleManager::Get().FindConsoleVariable(Name))C->SetStringField(Name,V->GetString());else C->SetStringField(Name,TEXT("not_registered"));}
    J->SetObjectField(TEXT("console"),C);J->SetBoolField(TEXT("engine_smooth_frame_rate"),GEngine&&GEngine->bSmoothFrameRate);J->SetBoolField(TEXT("engine_fixed_frame_rate"),GEngine&&GEngine->bUseFixedFrameRate);
    J->SetBoolField(TEXT("fixed_timestep"),FApp::UseFixedTimeStep());J->SetBoolField(TEXT("benchmarking_flag"),FApp::IsBenchmarking());
    if(Viewport.IsValid()&&Viewport->Viewport){const auto S=Viewport->Viewport->GetSizeXY();J->SetNumberField(TEXT("width"),S.X);J->SetNumberField(TEXT("height"),S.Y);}
    return J;
}

TSharedPtr<FJsonObject> AHCM5VS2CornerPerformanceDirector::Scene() const
{
    auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> People;int32 Actors=0;
    for(TActorIterator<AActor> It(GetWorld());It;++It)++Actors;
    for(TActorIterator<ACharacter> It(GetWorld());It;++It)
    {auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("class"),It->GetClass()->GetPathName());P->SetArrayField(TEXT("location"),Vector(It->GetActorLocation()));People.Add(MakeShared<FJsonValueObject>(P));}
    J->SetNumberField(TEXT("actors"),Actors);J->SetArrayField(TEXT("characters"),People);
    J->SetStringField(TEXT("view_target"),GetPathNameSafe(PC?PC->GetViewTarget():nullptr));
    J->SetStringField(TEXT("lighting"),TimeDirector?TimeDirector->GetLightingDiagnostics():TEXT("unavailable"));return J;
}

void AHCM5VS2CornerPerformanceDirector::MoveCamera(double Seconds)
{
    if(!Camera||Route.Num()<2)return;
    // Same 30s out/turn/back/turn path in every setting. Camera only, no Pawn motion.
    const double T=FMath::Fmod(Seconds,30.);double Distance=0,Turn=0;bool Reverse=false;
    if(T<12)Distance=RouteLength*T/12.;else if(T<15){Distance=RouteLength;Turn=(T-12)/3.;}
    else if(T<27){Distance=RouteLength*(1-(T-15)/12.);Reverse=true;}else{Distance=0;Turn=1-(T-27)/3.;}
    int32 I=1;while(I<RouteDistances.Num()-1&&RouteDistances[I]<Distance)++I;
    const double Alpha=FMath::Clamp((Distance-RouteDistances[I-1])/(RouteDistances[I]-RouteDistances[I-1]),0.,1.);
    const FVector Position=FMath::Lerp(Route[I-1],Route[I],Alpha);
    FRotator Facing=(Route[I]-Route[I-1]).Rotation();Facing.Yaw+=Reverse?180.:180.*Turn;
    Facing.Yaw+=24.*FMath::Sin(Seconds*UE_DOUBLE_PI/6.);Facing.Pitch=2;Facing.Roll=0;
    Camera->SetActorLocationAndRotation(Position,Facing,false,nullptr,ETeleportType::None);
}

void AHCM5VS2CornerPerformanceDirector::RestartWindow(const FString& Why)
{
    if(bSuspended)return;bSuspended=true;
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("reason"),Why);J->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);
    J->SetNumberField(TEXT("discarded_measurement_frames"),Frames.Num());J->SetNumberField(TEXT("discarded_measurement_seconds"),MeasureSeconds);J->SetNumberField(TEXT("warmup_active_seconds"),WarmSeconds);
    Interruptions.Add(MakeShared<FJsonValueObject>(J));Frames.Reset();ResourceSamples.Reset();
    MeasureSeconds=WarmSeconds=StableSeconds=LastDraw=LastResource=PeakLocalBytes=PeakSharedBytes=0;
    ResourceFailures=MaxMeasureShaders=MaxMeasureStreaming=0;bMeasuring=false;
}

void AHCM5VS2CornerPerformanceDirector::SampleResources(double Now)
{
    if(!bMeasuring||Now-LastResource<1)return;LastResource=Now;
    // Full diagnostics and recorder scan are bounded to 1 Hz, not every frame.
    if(!TimeDirector||TimeDirector->GetLightingDiagnostics()!=LightingReadback){Finish(TEXT("FAIL"),TEXT("Actual playable lighting readback changed during measurement"));return;}
    for(TActorIterator<AHCM3Recording> It(GetWorld());It;++It)if(It->HasCapturedFirstFrame()){Finish(TEXT("FAIL"),TEXT("Native recording detected during measurement"));return;}
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("elapsed_wall_seconds"),Now-Started);J->SetNumberField(TEXT("measurement_seconds"),MeasureSeconds);
    const auto Memory=FPlatformMemory::GetStats();J->SetNumberField(TEXT("process_working_set_bytes"),double(Memory.UsedPhysical));
    J->SetNumberField(TEXT("shader_jobs"),ShaderJobs);J->SetNumberField(TEXT("streaming_resources_wanted"),StreamingWanted);
    if(PC&&PC->PlayerCameraManager){J->SetArrayField(TEXT("actual_camera_cm"),Vector(PC->PlayerCameraManager->GetCameraLocation()));J->SetArrayField(TEXT("actual_camera_pitch_yaw_roll"),Vector(FVector(PC->PlayerCameraManager->GetCameraRotation().Pitch,PC->PlayerCameraManager->GetCameraRotation().Yaw,PC->PlayerCameraManager->GetCameraRotation().Roll)));}
    uint64 Local=0,Shared=0,Budget=0;
    if(VideoMemory&&VideoMemory->Sample(Local,Shared,Budget))
    {J->SetStringField(TEXT("vram_status"),TEXT("PASS"));J->SetNumberField(TEXT("process_local_usage_bytes"),double(Local));J->SetNumberField(TEXT("process_nonlocal_usage_bytes"),double(Shared));J->SetNumberField(TEXT("process_local_budget_bytes"),double(Budget));PeakLocalBytes=FMath::Max(PeakLocalBytes,double(Local));PeakSharedBytes=FMath::Max(PeakSharedBytes,double(Shared));}
    else{++ResourceFailures;J->SetStringField(TEXT("vram_status"),TEXT("NOT_RUN"));}
    ResourceSamples.Add(MakeShared<FJsonValueObject>(J));
}

void AHCM5VS2CornerPerformanceDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(bStopped)return;Observe();const double Now=FPlatformTime::Seconds();const double Dt=Now-LastTick;LastTick=Now;
    if(bExitPending){if(Now-Finished>2){bExitPending=false;SetActorTickEnabled(false);FPlatformMisc::RequestExit(false,*ExitReason);}return;}
    if(bStarting){FString Error;if(!Bind(Error)&&(bSavedState||Now-Started>15))Finish(TEXT("FAIL"),Error);return;}
    if(!bActive)return;
    if(Now-Started>360){Finish(TEXT("NOT_RUN"),TEXT("Bounded wall deadline; no uninterrupted complete measurement"));return;}
    if(!PC||!PC->IsGameplayFocused()||UGameplayStatics::IsGamePaused(this)){RestartWindow(TEXT("P/pause or gameplay focus lost; interrupted window excluded, resume requires complete warmup"));return;}
    if(bSuspended){bSuspended=false;LastDraw=0;MoveCamera(0);return;}
    FString Error;if(!Conditions(Error)){Finish(TEXT("FAIL"),Error);return;}
    ShaderJobs=0;
#if WITH_EDITOR
    if(GShaderCompilingManager)ShaderJobs=GShaderCompilingManager->GetNumRemainingJobs();
#endif
    StreamingWanted=IStreamingManager::Get().GetNumWantingResources();
    if(!bMeasuring)
    {
        WarmSeconds+=Dt;StableSeconds=ShaderJobs==0&&StreamingWanted==0?StableSeconds+Dt:0;MoveCamera(WarmSeconds);
        if(WarmSeconds>=WarmTarget&&StableSeconds>=2&&TotalDraws>=120)
        {bMeasuring=true;LastDraw=0;MeasureSeconds=0;LastResource=0;MoveCamera(0);FinalSettings=Settings();InitialScene=Scene();}
        return;
    }
    MaxMeasureShaders=FMath::Max(MaxMeasureShaders,ShaderJobs);MaxMeasureStreaming=FMath::Max(MaxMeasureStreaming,StreamingWanted);
    MoveCamera(MeasureSeconds);SampleResources(Now);if(!bActive)return;
    if(MeasureSeconds>=MeasureTarget){FinalSettings=Settings();FinalScene=Scene();Finish(TEXT("PASS"),TEXT("Complete uninterrupted native viewport frame sample; performance acceptance is USER_REVIEW"));}
}

void AHCM5VS2CornerPerformanceDirector::Drawn()
{
    if(!bActive||bStopped||!PC||!PC->IsGameplayFocused()||UGameplayStatics::IsGamePaused(this)||bSuspended)return;
    if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("FAIL"),TEXT("Screenshot request detected; capture overhead is excluded by invalidating this benchmark"));return;}
    if(LastDrawFrame==GFrameCounter)return;LastDrawFrame=GFrameCounter;++TotalDraws;
    if(!bMeasuring)return;const double Now=FPlatformTime::Seconds();
    if(LastDraw>0)
    {
        const double Ms=(Now-LastDraw)*1000;
        if(!FMath::IsFinite(Ms)||Ms<=0||Frames.Num()>=120000){Finish(TEXT("FAIL"),TEXT("Invalid interval or bounded sample capacity reached"));return;}
        Frames.Add(Ms);MeasureSeconds+=Ms/1000.;
    }
    LastDraw=Now;
}

void AHCM5VS2CornerPerformanceDirector::Input(const FInputKeyEventArgs& Event)
{
    if((bStarting||bActive||bExitPending)&&!bStopped&&Event.Event==IE_Pressed&&Event.Key==EKeys::Escape)
    {bStopped=true;bStarting=bActive=bExitPending=bAutoQuit=false;StopFrame=GFrameCounter;SetActorTickEnabled(false);Write(TEXT("NOT_RUN"),TEXT("Esc permanently stopped route, sampling, restore and autoquit; key observed without consumption"));}
}

void AHCM5VS2CornerPerformanceDirector::Restore()
{
    if(bStopped||!bSavedState)return;bSavedState=false;
    if(GEngine){GEngine->bSmoothFrameRate=bOriginalSmooth;GEngine->bUseFixedFrameRate=bOriginalFixed;}FApp::SetUseFixedTimeStep(bOriginalFixedStep);
    if(PC&&OriginalView.IsValid())PC->SetViewTarget(OriginalView.Get());
    if(TimeDirector&&!OriginalPeriod.IsNone())TimeDirector->SetTimeOfDay(OriginalPeriod);
}

void AHCM5VS2CornerPerformanceDirector::Finish(const FString& Status,const FString& Detail)
{
    if(bStopped)return;bStarting=bActive=bMeasuring=false;Finished=FPlatformTime::Seconds();
    if(!FinalSettings)FinalSettings=Settings();if(!FinalScene)FinalScene=Scene();
    bExitPending=bAutoQuit;ExitReason=TEXT("M5VS2 corner performance ")+Status;
    if(!Write(Status,Detail))UE_LOG(LogTemp,Error,TEXT("M5VS2_PERFORMANCE_REPORT_WRITE_FAILED %s"),*Directory);
    Restore();SetActorTickEnabled(bExitPending);UE_LOG(LogTemp,Display,TEXT("M5VS2_PERFORMANCE_%s %s"),*Status,*Directory);
}

bool AHCM5VS2CornerPerformanceDirector::Write(const FString& Status,const FString& Detail)
{
    if(Directory.IsEmpty())return false;auto J=MakeShared<FJsonObject>();
    J->SetStringField(TEXT("status"),Status);J->SetStringField(TEXT("detail"),Detail);J->SetStringField(TEXT("acceptance"),TEXT("USER_REVIEW; no FPS target invented"));
    J->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName());J->SetStringField(TEXT("layer"),TEXT("EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED"));
    J->SetStringField(TEXT("quality_requested"),Quality);J->SetStringField(TEXT("period"),Period.ToString());J->SetStringField(TEXT("plan"),PlanFile);J->SetStringField(TEXT("plan_sha1"),PlanSHA1);
    J->SetStringField(TEXT("input_scope"),TEXT("Native camera actor follows the actual authored 50m road centerline; Pawn/OS input/traversal are not tested. Live world simulation and HUD remain enabled."));
    J->SetStringField(TEXT("timing_scope"),TEXT("One FPlatformTime interval per distinct game-frame viewport OnEndDraw. Includes ordinary world/renderer scheduling and bounded sampler overhead; not GPU timestamp or physical display present time."));
    J->SetStringField(TEXT("statistics"),TEXT("Average FPS=N/sum(frame seconds); p99 nearest rank sorted[ceil(.99*N)-1], same definition as M4R1/M4R2 profile. No outlier filtering. Only final uninterrupted measurement window; interrupted attempts explicitly excluded."));
    J->SetNumberField(TEXT("warmup_active_seconds"),WarmSeconds);J->SetNumberField(TEXT("warmup_minimum_seconds"),WarmTarget);J->SetNumberField(TEXT("measurement_minimum_seconds"),MeasureTarget);
    J->SetNumberField(TEXT("active_measurement_seconds"),MeasureSeconds);J->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);J->SetNumberField(TEXT("sample_count"),Frames.Num());
    J->SetBoolField(TEXT("user_stop_latched"),bStopped);J->SetNumberField(TEXT("stop_frame"),double(StopFrame));J->SetBoolField(TEXT("os_input_used"),false);J->SetBoolField(TEXT("screenshots_or_recording_requested"),false);
    TArray<TSharedPtr<FJsonValue>> Raw;for(double F:Frames)Raw.Add(MakeShared<FJsonValueNumber>(F));J->SetArrayField(TEXT("frame_ms"),Raw);
    if(!Frames.IsEmpty()){auto Sorted=Frames;Sorted.Sort();double Sum=0;for(double F:Frames)Sum+=F;J->SetNumberField(TEXT("average_fps"),Frames.Num()*1000./Sum);J->SetNumberField(TEXT("p99_frame_ms"),Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.99)-1,0,Sorted.Num()-1)]);J->SetNumberField(TEXT("max_frame_ms"),Sorted.Last());}
    J->SetArrayField(TEXT("resource_samples_nominal_1hz"),ResourceSamples);J->SetArrayField(TEXT("interrupted_windows"),Interruptions);J->SetNumberField(TEXT("max_shader_jobs_during_measurement"),MaxMeasureShaders);J->SetNumberField(TEXT("max_streaming_resources_wanted_during_measurement"),MaxMeasureStreaming);
    auto GPU=MakeShared<FJsonObject>();const bool MemoryValid=VideoMemory&&VideoMemory->Status==TEXT("READY")&&!ResourceSamples.IsEmpty()&&ResourceFailures==0;
    GPU->SetStringField(TEXT("status"),MemoryValid?TEXT("PASS"):TEXT("NOT_RUN"));GPU->SetStringField(TEXT("scope"),TEXT("Sample maximum of DXGI QueryVideoMemoryInfo LOCAL CurrentUsage for this process on actual RHI adapter LUID, nominal 1Hz; not exact instantaneous peak, not total GPU usage. NON_LOCAL reported separately. Real sample times retained."));
    GPU->SetNumberField(TEXT("query_failures"),ResourceFailures);
    if(VideoMemory){GPU->SetStringField(TEXT("adapter"),VideoMemory->AdapterName);GPU->SetStringField(TEXT("luid"),VideoMemory->AdapterLuid);GPU->SetStringField(TEXT("unavailable_reason"),VideoMemory->Reason);GPU->SetNumberField(TEXT("dedicated_hardware_bytes"),double(VideoMemory->DedicatedBytes));}
    if(MemoryValid){GPU->SetNumberField(TEXT("peak_sampled_process_local_bytes"),PeakLocalBytes);GPU->SetNumberField(TEXT("peak_sampled_process_nonlocal_bytes"),PeakSharedBytes);}J->SetObjectField(TEXT("vram"),GPU);
    auto Hardware=MakeShared<FJsonObject>();Hardware->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand());Hardware->SetStringField(TEXT("gpu_platform_primary"),FPlatformMisc::GetPrimaryGPUBrand());Hardware->SetStringField(TEXT("gpu_rhi"),GRHIAdapterName);Hardware->SetStringField(TEXT("rhi"),GDynamicRHI?GDynamicRHI->GetName():TEXT("none"));
    const auto Driver=FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);Hardware->SetStringField(TEXT("gpu_driver"),Driver.UserDriverVersion);Hardware->SetStringField(TEXT("gpu_driver_internal"),Driver.InternalDriverVersion);Hardware->SetStringField(TEXT("gpu_driver_date"),Driver.DriverDate);
    Hardware->SetNumberField(TEXT("cpu_logical_cores"),FPlatformMisc::NumberOfCoresIncludingHyperthreads());Hardware->SetNumberField(TEXT("physical_ram_bytes"),double(FPlatformMemory::GetConstants().TotalPhysical));Hardware->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());J->SetObjectField(TEXT("hardware"),Hardware);
    if(InitialSettings)J->SetObjectField(TEXT("settings_before_benchmark_overrides"),InitialSettings);if(FinalSettings)J->SetObjectField(TEXT("actual_measurement_settings"),FinalSettings);if(InitialScene)J->SetObjectField(TEXT("initial_scene"),InitialScene);if(FinalScene)J->SetObjectField(TEXT("final_scene"),FinalScene);if(Plan)J->SetObjectField(TEXT("input_plan"),Plan);
    FString Text;return FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("corner_performance.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void AHCM5VS2CornerPerformanceDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if((bActive||bStarting)&&!bStopped)Finish(TEXT("NOT_RUN"),TEXT("World ended before full measurement"));
    bExitPending=false;if(Viewport.IsValid()){Viewport->OnInputKey().Remove(InputHandle);Viewport->OnEndDraw().Remove(DrawHandle);}InputHandle.Reset();DrawHandle.Reset();
    if(!bStopped)Restore();VideoMemory.Reset();Super::EndPlay(Reason);
}
