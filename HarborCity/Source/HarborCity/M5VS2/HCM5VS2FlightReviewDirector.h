#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2FlightReviewDirector.generated.h"

class AHCM1PlayerController;
class AHCM1Character;
class AHCM1Vehicle;
class AHCM1LightSwitch;
class UHCM5VS2FlightComponent;
class UPrimitiveComponent;
class UGameViewportClient;
class FJsonObject;
struct FInputKeyEventArgs;

/** Isolated opt-in flight regression. Action injection is explicitly not operating-system input. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2FlightReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2FlightReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AHCM1Vehicle> Vehicle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AHCM1LightSwitch> InteractionSwitch;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Start();
    bool Check(const FString& Name, bool Passed, const FString& Detail);
    void Phase(int32 Value);
    void Inject(const FVector2D& Move = FVector2D::ZeroVector, bool Up=false, bool Down=false, bool Boost=false);
    void ToggleAction();
    bool Fixture(FVector Feet, bool Ground, float Yaw=0.f);
    void Capture(const FString& Label);
    void Input(const FInputKeyEventArgs& Event);
    UFUNCTION() void Hit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent,
        FVector NormalImpulse, const FHitResult& Result);
    TSharedPtr<FJsonObject> Snapshot() const;
    void Write(const FString& Status, const FString& Detail);
    void Finish(const FString& Status, const FString& Detail);
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2FlightComponent> Flight;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle;
    TArray<TSharedPtr<FJsonObject>> Checks, Samples, Fixtures, Hits, Captures, LookFrames;
    TSharedPtr<FJsonObject> LookMeasurement, WallImpact;
    FString Directory, SaveSlot;
    TArray<uint8> SavedBytes;
    double StartedWall=0, LastWall=0, ActiveSeconds=0, PhaseStart=0, NextSample=0, AutoPauseAt=0;
    double MaxSpeed=0, BrakeStartSpeed=0, IntermediateBrakeSpeed=-1, CeilingPeak=0, WaterLowest=1.e9;
    FRotator LookBefore, LookViewBefore;
    FVector2D LookRawSum=FVector2D::ZeroVector, LookActionSum=FVector2D::ZeroVector;
    FVector2D LookMappingScale=FVector2D::ZeroVector;
    FVector PositionBefore;
    FVector WallPreviousPosition=FVector::ZeroVector, WallPreviousVelocity=FVector::ZeroVector;
    uint64 WallPreviousFrame=0;
    uint64 LookCallbacksBefore=0, LookPreviousCallbacks=0;
    int32 CurrentPhase=0, HitStart=0, TotalHits=0, LastLookPulse=0, UserPausePresses=0;
    bool bActive=false, bReady=false, bStopped=false, bAutoQuit=false, bReleaseToggle=false;
    bool bAutomaticPause=false, bInitialSwitch=false, bOriginalFirstPerson=false, bPhasePulse=false;
    bool bLookMultipleCallbacks=false;
};
