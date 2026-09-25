#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "HCM4R2PlayerCameraManager.generated.h"

class AHCM1Vehicle;

/** Final POV owner. Existing unarmed and driving third-person cameras are preserved. */
UCLASS()
class HARBORCITY_API AHCM4R2PlayerCameraManager : public APlayerCameraManager
{
    GENERATED_BODY()
public:
    virtual void UpdateCamera(float DeltaTime) override;
    virtual void ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot) override;
    static constexpr float FootFirstPersonPitchLimit = 65.f;
    void AddCockpitLook(const FVector2D& RawMouse);
    void ResetPerspectiveState();
    FRotator GetCockpitLook() const { return CockpitLook; }
    float GetLastBaseWorldFOV() const { return LastBaseWorldFOV; }
    float GetLastMagnification() const { return LastMagnification; }
    FString GetPerspectiveDiagnostics() const;
protected:
    virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
private:
    TArray<TWeakObjectPtr<UPrimitiveComponent>> ProximityHidden;
    FRotator CockpitLook = FRotator::ZeroRotator;
    FRotator FilteredChassis = FRotator::ZeroRotator;
    TWeakObjectPtr<AHCM1Vehicle> LastVehicle;
    uint64 LastVehicleReset = 0;
    float CockpitIdleSeconds = 0;
    float LastBaseWorldFOV = 90;
    float LastMagnification = 1;
    float GunShoulderBlend = 0;
    bool bResetFilter = true;
};
