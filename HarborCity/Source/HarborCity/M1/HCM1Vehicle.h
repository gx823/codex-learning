#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "HCInteractable.h"
#include "HCM1Vehicle.generated.h"

class UHCM4R2CockpitComponent;

class ACharacter;
class AController;
class UCameraComponent;
class UChaosWheeledVehicleMovementComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UHCM4R1VehicleImpactComponent;

/** M1 sports car. The player controller owns interaction state and input contexts. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM1Vehicle : public AWheeledVehiclePawn, public IHCInteractable
{
    GENERATED_BODY()

public:
    uint64 GetCameraResetSerial() const { return CameraResetSerial; }
    UHCM4R2CockpitComponent* GetCockpitComponent() const { return Cockpit; }
    FVector GetDriverEyeLocal() const { return FVector(-50,-38,98); }
    AHCM1Vehicle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    void AddCameraLookInput(const FVector2D& RawMouse);
    void ResetDrivingCamera(bool bResetOrbit = true);
    FRotator GetCameraOrbitRotation() const { return CameraOrbitRotation; }
    float GetCameraIdleSeconds() const { return CameraIdleSeconds; }
    bool IsCameraRecentering() const { return bCameraRecentering; }
    bool IsLegacyCamera() const { return bLegacyCamera; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    void SetDriveInput(float ThrottleAxis, float SteeringAxis, bool bHandbrake);

    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    void ClearDriveInput(bool bPark = true);

    UFUNCTION(BlueprintPure, Category="HarborCity|Vehicle")
    float GetSpeedKmh() const;

    /** Signed forward velocity in centimetres per second. */
    UFUNCTION(BlueprintPure, Category="HarborCity|Vehicle")
    float GetSignedSpeed() const;

    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    bool FindSafeExitTransform(ACharacter* Character, FTransform& Out) const;

    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    bool TrySafeReset(AActor* AvoidActor = nullptr);

    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    bool RestoreSavedTransform(const FTransform& Transform, AActor* AvoidActor = nullptr);

    /** Last placement query only; bounded readback for save/reset diagnostics. */
    const FString& GetPlacementDiagnostic() const { return PlacementDiagnostic; }

    /** Read-only ground, footprint and clearance check; does not move the vehicle. */
    bool QuerySafeVehicleTransform(const FTransform& Requested, FTransform& Out) const
    { return ResolveSafeVehicleTransform(Requested, nullptr, Out); }

    /** Sets only the weak seat reservation; nullptr releases it and applies parking brakes. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    void SetDriver(AController* NewDriver);

    UFUNCTION(BlueprintPure, Category="HarborCity|Vehicle")
    bool HasDriver() const;

    UFUNCTION(BlueprintPure, Category="HarborCity|Vehicle")
    FVector GetInteractionLocation() const;

    UFUNCTION(BlueprintPure, Category="HarborCity|Vehicle")
    FString GetDriveTelemetry() const;

    UFUNCTION(BlueprintPure, Category="HarborCity|Vehicle")
    UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;

    virtual FString GetInteractionText_Implementation(APlayerController* Player) const override;
    virtual void Interact_Implementation(APlayerController* Player) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Vehicle")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Vehicle")
    TObjectPtr<UCameraComponent> DrivingCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Vehicle")
    TObjectPtr<UHCM4R1VehicleImpactComponent> NPCImpact;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HarborCity|Vehicle|Safety", meta=(ClampMin="0.0"))
    float MaximumExitSpeedKmh = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HarborCity|Vehicle|Safety", meta=(ClampMin="1.0"))
    float DirectionChangeSpeedCmS = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HarborCity|Vehicle|Safety", meta=(ClampMin="0.0", ClampMax="40.0"))
    float MaximumResetSlopeDegrees = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Vehicle")
    FName StableId = TEXT("M1_Car_01");

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Vehicle")
    TObjectPtr<UStaticMeshComponent> BodyVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Vehicle")
    TObjectPtr<UStaticMeshComponent> GlassVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Vehicle")
    TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;

private:
    mutable FString PlacementDiagnostic;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHCM4R2CockpitComponent> Cockpit;
    uint64 CameraResetSerial = 0;
    UPROPERTY(Transient)
    TWeakObjectPtr<AController> Driver;

    FTransform InitialSafeTransform;
    float RequestedThrottleAxis = 0.0f;
    float RequestedSteeringAxis = 0.0f;
    float ReverseTraceWait = 0.f;
    int32 ReverseTraceSamples = 0;
    bool bRequestedHandbrake = false;
    bool bParkingBrake = true;
    FRotator CameraOrbitRotation = FRotator(-12.0f, 0.0f, 0.0f);
    float CameraIdleSeconds = 0.0f;
    bool bCameraRecentering = false;
    bool bLegacyCamera = false;
    void ConfigureDrivingCamera();
    void UpdateDrivingCamera(float DeltaSeconds);

    void ApplyDriveInput();
    bool ResolveSafeVehicleTransform(const FTransform& Requested, AActor* AvoidActor, FTransform& Out) const;
    bool TeleportToValidatedTransform(const FTransform& Transform);
};
