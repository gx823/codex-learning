#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2FlightComponent.generated.h"

class AHCM1Character;
class AHCM1PlayerController;

/** Explicit opt-in, authored only in VS2 worlds. No map-name or save-data switch. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2FlightBounds : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2FlightBounds();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bEnableFlight = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") float SeaLevelZ = -150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") FVector2D FlightAreaCenter = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") FVector2D FlightAreaHalfExtent = FVector2D(2500,2500);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="100")) float SoftReturnDistance = 10000.f;
    /** Relative to sea level; measured at the capsule's feet. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="200")) float CeilingHeight = 10000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0")) float WaterClearance = 50.f;
    /** World-space authored interiors; supplements the live story interior and roof test. */
    UPROPERTY(EditAnywhere, Category="Flight") TArray<FBox> NoTakeoffVolumes;
};

/** Input target generation only. CMC MOVE_Flying performs all movement and sweeps. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2FlightComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2FlightComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;
    UFUNCTION(BlueprintCallable, Category="Flight") bool TryToggleFlight();
    UFUNCTION(BlueprintPure, Category="Flight") bool IsFlightAvailable() const;
    UFUNCTION(BlueprintPure, Category="Flight") bool IsFlying() const { return bFlying; }
    UFUNCTION(BlueprintPure, Category="Flight") bool IsLanding() const { return bLanding; }
    UFUNCTION(BlueprintPure, Category="Flight") float GetStamina() const { return Stamina; }
    UFUNCTION(BlueprintPure, Category="Flight") bool IsExhausted() const { return bExhausted; }
    void RestoreFullStamina() { Stamina=100.f; bExhausted=false; }
    UFUNCTION(BlueprintPure, Category="Flight") bool IsBoosting() const { return bFlying && bBoost && !bLanding; }
    UFUNCTION(BlueprintPure, Category="Flight") FName GetFlightPresentationState() const;
    UFUNCTION(BlueprintPure, Category="Flight") float GetHeightAboveSea() const;
    UFUNCTION(BlueprintPure, Category="Flight") FString GetFlightDiagnostics() const;
    UFUNCTION(BlueprintCallable, Category="Flight") void SetHorizontalInput(FVector2D Input);
    UFUNCTION(BlueprintCallable, Category="Flight") void SetAscendHeld(bool bHeld);
    UFUNCTION(BlueprintCallable, Category="Flight") void SetDescendHeld(bool bHeld);
    UFUNCTION(BlueprintCallable, Category="Flight") void SetBoostHeld(bool bHeld);
    void ClearFlightInput();
    /** Used only by existing safe ground recovery/load/seat lifecycle, never as flight locomotion. */
    void ResetForGroundTransition();
    AHCM5VS2FlightBounds* GetFlightBounds() const;

    // Requires both this property and exactly one enabled bounds actor in this world.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight") bool bEnableFlight = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="100")) float CruiseSpeed = 1000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="100")) float BoostSpeed = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="100")) float VerticalSpeed = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="100")) float FlightAcceleration = 2200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="100")) float FlightBraking = 3000.f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Flight") FVector RequestedFlightVelocity = FVector::ZeroVector;
    /** Presentation consumers may use this without rotating the collision capsule or camera. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Flight") float VisualBankDegrees = 0.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<AHCM1Character> Character;
    TArray<TWeakObjectPtr<AActor>> IgnoredNPCs;
    FVector2D HorizontalInput = FVector2D::ZeroVector;
    bool bFlying = false, bLanding = false, bAscend = false, bDescend = false, bBoost = false;
    bool bSavedPhysicsInteraction = true, bSavedRequestedAcceleration = true;
    bool bSavedOrientToMovement = true, bSavedControllerDesiredRotation = false;
    float SavedMaxFlySpeed = 0, SavedMaxAcceleration = 0, SavedFlyingBraking = 0;
    float TakeoffFeetZ = 0;
    float Stamina = 100.f;
    bool bExhausted = false, bLowStaminaNotified = false;
    double TakeoffUntil = 0, LastBoundaryNotice = -100, LastNPCRefresh = -100;
    FString LastReason;
    int32 TakeoffCount = 0, LandingCount = 0, RejectedCount = 0;
    AHCM1PlayerController* GetPC() const;
    bool CanAcceptInput() const;
    bool Reject(const FString& Reason);
    bool FindLandingSurface(FHitResult& Hit) const;
    bool IsWaterSurface(const FHitResult& Hit) const;
    void FinishLanding();
    void RefreshIgnoredNPCs();
    void RestoreMovementSettings();
};
