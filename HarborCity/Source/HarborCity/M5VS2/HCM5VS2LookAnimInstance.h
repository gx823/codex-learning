#pragma once

#include "CoreMinimal.h"
#include "M4R2/HCM4R2PlayerAnimInstance.h"
#include "HCM5VS2LookAnimInstance.generated.h"

/** New VS2 graph only: preserve all inherited locomotion/combat inputs; add native LookAt inputs. */
UCLASS(Transient, Blueprintable)
class HARBORCITY_API UHCM5VS2LookAnimInstance : public UHCM4R2PlayerAnimInstance
{
    GENERATED_BODY()
public:
    bool bTownIdleGestureActive = false;
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Look") FVector VS2LookTargetWorld = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Look") float VS2HeadLookAlpha = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Look") float VS2EyeLookAlpha = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Look") bool bVS2ExplicitLookTarget = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="M5VS2|Look") bool bVS2IdleGlances = true;

    /** Only the new authored candidate enables this; old VS2/FP/combat graphs stay intact. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="M5VS2|Motion") bool bVS2GASMotionEnabled = false;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") bool bVS2MotionEligible = false;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") int32 VS2MotionPoseIndex = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2MotionElapsed = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2GroundSpeed = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2SampleDirection = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2OrientationAngle = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2WarpAlpha = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") FVector VS2StrideDirection = FVector(0,1,0);
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2StrideScale = 1;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Motion") float VS2NominalRootSpeed = 0;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") bool bVS2Flying = false;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") bool bVS2FlightBoost = false;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") FName VS2FlightState = TEXT("Grounded");
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") FVector VS2FlightVelocityLocal = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") float VS2FlightBank = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="M5VS2|Flight") bool bVS2FlightPosesEnabled = false;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") bool bVS2FlightPoseEligible = false;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") int32 VS2FlightPoseIndex = 1;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") FRotator VS2FlightBankRotation = FRotator::ZeroRotator;
    /** Kawaii SimpleExternalForce uses centimeters/second of solver displacement, not acceleration. */
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") FVector VS2FlightHairForce = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") FVector VS2FlightRibbonForce = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="M5VS2|Flight") FVector VS2FlightSkirtForce = FVector::ZeroVector;

private:
    FRandomStream GlanceRandom;
    float GlanceCountdown = 3.f, GlanceRemaining = 0;
    FRotator GlanceRotation = FRotator::ZeroRotator;
    FVector SmoothedDirection = FVector::ForwardVector;
    bool bDirectionInitialized = false;
    bool bMotionInitialized = false, bWasFalling = false, bHadMoveInput = false;
    bool bLastSprint = false, bPendingStop = false;
    float PreviousGroundSpeed = 0;
    float FlightLandingVisualTime = 0;
    void UpdateVS2Motion(float DeltaSeconds);
    void UpdateVS2FlightPose(float DeltaSeconds, bool bPreviouslyFlying);
};
