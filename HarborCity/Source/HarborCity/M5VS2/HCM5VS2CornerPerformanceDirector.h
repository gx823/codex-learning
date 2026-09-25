#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2CornerPerformanceDirector.generated.h"
class ACameraActor; class AHCM1PlayerController; class AHCM5VS2CornerTimeDirector;
class UGameViewportClient; class FJsonObject; class FJsonValue; struct FInputKeyEventArgs;
struct FHCM5VS2ProcessVideoMemory;
// UHT also emits a vtable-helper constructor. Its cleanup must not instantiate
// default delete for an incomplete DXGI implementation type in generated.cpp.
struct FHCM5VS2ProcessVideoMemoryDeleter
{
    void operator()(FHCM5VS2ProcessVideoMemory* Value) const;
};

/** Opt-in native camera benchmark. No screenshots, recording or synthetic player input. */
UCLASS()
class HARBORCITY_API AHCM5VS2CornerPerformanceDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2CornerPerformanceDirector();
    virtual ~AHCM5VS2CornerPerformanceDirector() override;
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool LoadPlan(FString& Error); bool Bind(FString& Error); bool Conditions(FString& Error) const;
    void Observe(); void Input(const FInputKeyEventArgs& Event); void Drawn();
    void MoveCamera(double Seconds); void RestartWindow(const FString& Why); void SampleResources(double Now);
    void Finish(const FString& Status,const FString& Detail); void Restore(); bool Write(const FString& Status,const FString& Detail);
    TSharedPtr<FJsonObject> Settings() const; TSharedPtr<FJsonObject> Scene() const;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<AHCM5VS2CornerTimeDirector> TimeDirector;
    TWeakObjectPtr<AActor> OriginalView;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle,DrawHandle;
    TUniquePtr<FHCM5VS2ProcessVideoMemory,FHCM5VS2ProcessVideoMemoryDeleter> VideoMemory;
    TSharedPtr<FJsonObject> Plan,InitialSettings,FinalSettings,InitialScene,FinalScene;
    TArray<TSharedPtr<FJsonValue>> ResourceSamples,Interruptions;
    TArray<FVector> Route; TArray<double> RouteDistances,Frames;
    FString Directory,PlanFile,PlanSHA1,Quality,LightingReadback,ExitReason;
    FName Period=TEXT("Afternoon"),OriginalPeriod;
    double Started=0,LastTick=0,WarmSeconds=0,StableSeconds=0,MeasureSeconds=0,LastDraw=0,Finished=0,LastResource=0;
    double RouteLength=0,WarmTarget=30,MeasureTarget=65,PeakLocalBytes=0,PeakSharedBytes=0;
    int32 QualityLevel=2,ShaderJobs=0,StreamingWanted=0,MaxMeasureShaders=0,MaxMeasureStreaming=0,ResourceFailures=0;
    uint64 LastDrawFrame=0,StopFrame=0,TotalDraws=0;
    bool bStarting=false,bActive=false,bMeasuring=false,bStopped=false,bSuspended=false,bExitPending=false,bAutoQuit=false;
    bool bSavedState=false,bOriginalSmooth=false,bOriginalFixed=false,bOriginalFixedStep=false;
};
