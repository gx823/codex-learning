#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2FlightDemoDirector.generated.h"
class AHCM1PlayerController;
class AHCM1Character;
class AHCM3Recording;
class UHCM5VS2FlightComponent;
class UGameViewportClient;
class UPrimitiveComponent;
class FJsonObject;
struct FInputKeyEventArgs;
/** Private opt-in recording: ENGINE_INPUT_NOT_OS, never a camera/teleport track. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2FlightDemoDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2FlightDemoDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo") FVector ExpectedStartFeet = FVector(-2150,350,2);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo") FVector OrbitCenter = FVector(-1850,1550,0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo") float OrbitRadius = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo") float HeightAboveStart = 3000.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Initialize();
    bool ReadInputChain();
    bool GroundPreflight();
    bool Check(bool Passed,const FString& Name,const FString& Detail);
    void BindStopObservers();
    void ObserveInput(const FInputKeyEventArgs& Event);
    void ObserveActivation(bool Active);
    void Key(const FKey& KeyName,bool Pressed);
    void Pulse(const FKey& KeyName);
    void ReleaseOwnedInput();
    void LookToward(float Yaw,float Pitch,float DeltaSeconds,float MaxYawRate=85.f);
    void SetStage(int32 Next);
    void Stop(const FString& Status,const FString& Reason,bool UserStop=false);
    void FinalizeTick();
    bool Write();
    void Sample();
    bool CorridorClear();
    UFUNCTION() void CapsuleHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Result);
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2FlightComponent> Flight;
    UPROPERTY(Transient) TObjectPtr<AHCM3Recording> Recorder;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle;
    FDelegateHandle ActivationHandle;
    TSet<FKey> HeldKeys;
    TArray<FKey> PulsedKeys;
    TArray<TSharedPtr<FJsonObject>> Checks, Samples, KeyEvents, LookEvents, HitEvents;
    TSharedPtr<FJsonObject> InputChain;
    FString Directory,FinalStatus=TEXT("RUNNING"),FinalReason,SaveSlot;
    FVector StartFeet=FVector::ZeroVector;
    FVector2D MappingScale=FVector2D::ZeroVector,DegreesPerRaw=FVector2D::ZeroVector;
    FVector2D QueuedRawSum=FVector2D::ZeroVector,ObservedRawSum=FVector2D::ZeroVector,ObservedActionSum=FVector2D::ZeroVector;
    double StartedWall=0,FirstFrameWall=0,StageWall=0,NextSampleWall=0,FinalizeWall=0;
    double ArcDegrees=0,PreviousBearing=0,OrbitMinRadius=1.e9,OrbitMaxRadius=0;
    double BoostMaxSpeed=0,DiveMinVerticalSpeed=0,ClimbMaxVerticalSpeed=0;
    double SimSeconds=0,FrameDtSum=0,MaxFrameDt=0;
    uint64 InputCallbacksBefore=0,PreviousInputCallbacks=0;
    int32 Stage=0,Frames=0,UnexpectedHits=0;
    bool bEnabled=false,bInitialized=false,bStopLatched=false,bFinalizing=false,bAutoQuit=false,bHadFocus=false,bEnded=false;
    bool bEvidenceWriteFailed=false;
    bool bAwaitLookObservation=false,bDiveObserved=false,bClimbObserved=false,bFirstPersonObserved=false,bThirdPersonRestored=false;
};
