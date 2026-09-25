#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HCM4R1PhysicalReactionComponent.generated.h"

class AHCM3NPC;
class AHCM1Vehicle;
class UAnimSequence;
class UAnimMontage;

UENUM(BlueprintType)
enum class EHCM4R1GetUpDirection : uint8 { Supine, Prone, Left, Right };

/** Native author readback of the TARGET clip, with its actual root extraction/lock applied. */
USTRUCT(BlueprintType)
struct FHCM4R1GetUpClip
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) EHCM4R1GetUpDirection Direction = EHCM4R1GetUpDirection::Supine;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequence> Animation;
    UPROPERTY(EditAnywhere) FVector FirstHipsComponent = FVector::ZeroVector;
    UPROPERTY(EditAnywhere) FVector FirstChestForwardComponent = FVector::ZeroVector;
    UPROPERTY(EditAnywhere) FVector FirstChestRightComponent = FVector::ZeroVector;
    UPROPERTY(EditAnywhere) FVector EndHipsComponent = FVector::ZeroVector;
};

/** Temporary, living ragdoll. Death and corpse ownership remain on AHCM3NPC. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM4R1PhysicalReactionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM4R1PhysicalReactionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    UFUNCTION(BlueprintPure) bool IsLivingRagdollActive() const { return bActive || bPendingImpact; }
    UFUNCTION(BlueprintPure) bool IsRecovering() const { return bRecovering; }
    UFUNCTION(BlueprintPure) FVector GetPhysicalLocation() const;
    UFUNCTION(BlueprintPure) FString GetDiagnostics() const;
    /** Called exactly once for each accepted car/NPC contact episode. Units: cm/s. */
    float ReceiveVehicleImpact(AHCM1Vehicle* Vehicle, const FHitResult& Hit, const FVector& RelativeVelocity,
        const FVector& OutwardDirection, float ClosingSpeedCmS, int32 ContactSerial, FName Source);
    /** Death takes over the same physical bodies; never clear its simulation here. */
    void OnOwnerDied();
    void ResetForRestore();
    void NotifySurvivingDamage(const FVector& Source);
    bool TryAnimatedKnockdown(const FVector& Source);
    UPROPERTY(EditAnywhere,Category="HarborCity|VS3") bool bAnimationPrimaryKnockdown=false;
    /** Same direction order as GetUpClips; only generated for verified adult NPC skeletons. */
    UPROPERTY(EditAnywhere,Category="HarborCity|VS3") TArray<TSoftObjectPtr<UAnimSequence>> AnimatedFallClips;

    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Impact") float DamageThresholdCmS = 180.f;
    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Impact") float KnockdownThresholdCmS = 360.f;
    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Impact") float MaximumDeltaVCmS = 1250.f;
    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Recovery") float MinimumDownSeconds = 1.25f;
    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Recovery") float StableSeconds = .55f;
    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Recovery") float RecoveryBlendSeconds = .65f;
    UPROPERTY(EditAnywhere, Category="HarborCity|M4R1|Recovery") float EmergencyRecoverySeconds = 30.f;
    /** Disabled until four target clips and their native pose/scale readbacks have been bound. */
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") bool bUseAuthoredGetUp = false;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") bool bGetUpPoseDataVerified = false;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") TArray<FHCM4R1GetUpClip> GetUpClips;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") FName GetUpChestBone;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") FVector GetUpChestForwardBoneLocal = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") FVector GetUpChestRightBoneLocal = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp") FVector GetUpAuthoredComponentScale = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="HarborCity|GetUp", meta=(ClampMin="1", ClampMax="30")) float GetUpMaximumAlignmentErrorCm = 15.f;
private:
    int32 AnimatedFallStage=0;
    double AnimatedStageStart=0;
    float AnimatedGetUpLength=0;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> AnimatedFallMontage;
    bool TickAnimatedKnockdown();
    enum class EGetUpStage : uint8 { None, Align, Playing };
    EGetUpStage GetUpStage = EGetUpStage::None;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> GetUpMontage;
    int32 GetUpClipIndex = INDEX_NONE;
    uint8 SavedRootMotionMode = 0;
    bool bSavedRootMotionMode = false, bGetUpMontageEnded = false, bGetUpMontageInterrupted = false;
    uint64 GetUpEndedFrame = 0;
    uint64 GetUpFirstPoseReadyFrame = 0;
    int32 AuthoredGetUpCompletions = 0;
    double GetUpPlayStarted = 0;
    float GetUpAlignmentErrorCm = 0, GetUpDirectionScore = 0, GetUpLastMontagePosition = 0;
    float GetUpEndHipsErrorCm = 0;
    FVector GetUpStartActorLocation = FVector::ZeroVector;
    FString GetUpLastResult;
    bool ValidateAuthoredGetUp(FString& Error) const;
    bool TryBeginAuthoredGetUp();
    void TickAuthoredGetUp();
    void CancelAuthoredGetUp();
    bool RestartGetUpRagdoll();
    void GetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    TWeakObjectPtr<AHCM3NPC> NPC;
    FTransform InitialMeshRelative;
    FTransform LastSafeStand;
    FTransform RecoveryStand;
    FVector DangerPoint = FVector::ZeroVector;
    FVector LastLanding = FVector::ZeroVector;
    FVector LastImpulse = FVector::ZeroVector;
    FVector LastDeltaV = FVector::ZeroVector;
    double StartedAt = 0, StableSince = 0, NextRecoveryCheck = 0, RecoveryStartedAt = 0;
    bool bActive = false, bRecovering = false, bHasSafeStand = false;
    bool bPendingImpact = false;
    double PendingImpactStarted = 0, PendingImpactDue = 0;
    FVector PendingImpactVelocity = FVector::ZeroVector, PendingImpactDeltaV = FVector::ZeroVector;
    float LastPreImpactSeconds = 0;
    bool bEmergencyRecovery = false;
    bool bSavedAnimationTick = false, bOriginalUpdateRate = false;
    bool bRecoveryOverridesTransformMode = false, bLastRecoveryAttachSucceeded = false;
    uint8 OriginalVisibilityTickOption = 0;
    uint8 OriginalPhysicsTransformUpdateMode = 0;
    FTransform RecoveryMeshWorld = FTransform::Identity;
    FVector RecoveryBodyBeforeReference = FVector::ZeroVector, RecoveryBodyAfterReference = FVector::ZeroVector;
    float LastTotalMassKg = 0, LastClosingSpeed = 0, LastDamage = 0;
    float LastMaxLinearSpeed = 0, LastMaxAngularSpeed = 0;
    int32 ImpactEvents = 0, ImpulseEvents = 0, RecoveryEvents = 0, RecoveryBlocked = 0;
    int32 EmergencyRecoveries = 0, RecoveryInterruptions = 0, LastBodyCount = 0;
    int32 LastContactSerial = 0;
    FName LastSource;
    FString LastRecoveryReason;
    FString LastEmergencySource;
    bool StartLivingRagdoll(const FVector& InitialVelocity);
    void ApplyDistributedImpulse(const FVector& DeltaV);
    void BeginRecovery(const FTransform& Stand, bool bEmergency);
    void AbortRecovery();
    void CompleteRecovery();
    void RestoreAnimationTick();
    void RestorePhysicsTransformMode();
};
