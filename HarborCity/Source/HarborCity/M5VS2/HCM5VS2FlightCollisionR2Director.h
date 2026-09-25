#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2FlightCollisionR2Director.generated.h"

class AHCM1PlayerController;
class AHCM1Character;
class UHCM5VS2FlightComponent;
class UPrimitiveComponent;
class UGameViewportClient;
struct FInputKeyEventArgs;
class FJsonObject;

/** Private, bounded real-CMC collision diagnostic. It never replaces the production flight implementation. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2FlightCollisionR2Director : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2FlightCollisionR2Director();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> HighRoof;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> HighPlatform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> PassageNorth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> PassageSouth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> BendNorth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> BendSouth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> WaterSurface;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Start();
    bool Check(const FString& Name, bool Passed, const FString& Detail);
    void ChangePhase(int32 Next);
    void Inject(bool Boost=false, bool Ascend=false, bool Descend=false);
    void ToggleFlight();
    bool AirFixture(const FVector& Feet, const FRotator& Look, AActor* Target);
    bool ObserveFlightFrame(float DeltaSeconds);
    bool CheckImpact(const FString& Name, const FVector& RequiredNormal);
    void Input(const FInputKeyEventArgs& Event);
    UFUNCTION() void Hit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent,
        FVector NormalImpulse, const FHitResult& Result);
    TSharedPtr<FJsonObject> Snapshot() const;
    void Write(const FString& Status, const FString& Detail);
    void Finish(const FString& Status, const FString& Detail);
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2FlightComponent> Flight;
    UPROPERTY(Transient) TObjectPtr<AActor> ExpectedTarget;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle;
    TArray<TSharedPtr<FJsonObject>> Checks, Fixtures, Frames, Hits;
    TSharedPtr<FJsonObject> FirstImpact;
    FString Directory;
    FVector PreviousPosition=FVector::ZeroVector, PreviousVelocity=FVector::ZeroVector;
    FVector ContactPoint=FVector::ZeroVector, ContactNormal=FVector::ZeroVector;
    FVector BeforeTakeoff=FVector::ZeroVector;
    uint64 PreviousFrame=0;
    double StartedWall=0, ActiveSeconds=0, PhaseStarted=0, PeakSpeed=0, LowestSeaClearance=1.e9;
    double MinFrameSeconds=1.e9, MaxFrameSeconds=0, MeasuredFrameSeconds=0;
    int32 CurrentPhase=0, PhaseHitCount=0, UnexpectedHitCount=0, UserPausePresses=0, FocusPauseRequests=0;
    bool bActive=false, bReady=false, bStopped=false, bAutoQuit=false, bReleaseToggle=false;
    bool bSawPassageExit=false, bSawWaterPlateau=false;
};
