#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HCM4CombatComponent.generated.h"

class AHCM1Character;
class AHCM1PlayerController;
class AHCM4Weapon;
class UAnimSequence;
class UAnimMontage;
class UAnimInstance;
class UHCM1SaveGame;

/** One accepted gameplay shot. Cosmetic subscribers never consume ammo or apply damage. */
struct FHCM4AcceptedShot
{
    int32 ShotId = 0;
    double Time = 0;
    FVector WorldMuzzle = FVector::ZeroVector;
    FVector AimPoint = FVector::ZeroVector;
    bool bMuzzleBlocked = false;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FHCM4AcceptedShotEvent, const FHCM4AcceptedShot&);

UENUM(BlueprintType)
enum class EHCM4WeaponMode : uint8 { Unarmed=0, Pistol=1, Sword=2 };

/** Bounded runtime observations. Inputs, geometric traces and damage are distinguished. */
struct FHCM4CombatEvent
{
    double Time = 0;
    FName Kind;
    int32 Combo = 0;
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FVector Point = FVector::ZeroVector;
    FName Actor;
    FName Bone;
    float Value = 0;
    bool bBlocked = false;
    // Punch samples use evaluated montage progress scaled to the configured attack duration.
    // Negative values mean this event is not a timed punch segment.
    float SampleTimeStart = -1, SampleTimeEnd = -1;
    float WindowTimeStart = -1, WindowTimeEnd = -1, AttackWorldElapsed = -1;
    FVector SampleStart = FVector::ZeroVector, SampleEnd = FVector::ZeroVector;
};

/** On-foot combat in the M3 experience. It never owns camera rotation or vehicle input. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM4CombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM4CombatComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;
    UFUNCTION(BlueprintCallable) bool RequestAttack();
    UFUNCTION(BlueprintCallable) bool RequestReload();
    UFUNCTION(BlueprintCallable) bool RequestToggleWeapon();
    UFUNCTION(BlueprintCallable) void SetAimHeld(bool bHeld);
    /** System transitions abort an unfinished reload before any ammunition transfer. */
    void CancelTransient(const TCHAR* Reason);
    void SetSeated(bool bSeated);
    /** Flight takes off and lands unarmed; no ammunition or combat tuning changes. */
    void PrepareForFlight();
    bool IsCombatEnabled() const;
    bool CanUseCombat() const;
    UFUNCTION(BlueprintPure) EHCM4WeaponMode GetWeaponMode() const { return WeaponMode; }
    UFUNCTION(BlueprintPure) int32 GetMagazineAmmo() const { return MagazineAmmo; }
    UFUNCTION(BlueprintPure) int32 GetReserveAmmo() const { return ReserveAmmo; }
    UFUNCTION(BlueprintPure) float GetPlayerHealth() const { return PlayerHealth; }
    /** Only the player's TakeDamage route forwards a verified NPC counterpunch here. */
    float ReceiveNPCPunch(float Damage, AActor* Attacker);
    void HealPlayer(float Amount) { if(PlayerHealth>0 && FMath::IsFinite(Amount)) PlayerHealth=FMath::Clamp(PlayerHealth+FMath::Max(0.f,Amount),0.f,100.f); }
    UFUNCTION(BlueprintPure) float GetDefeatRecoveryRemaining() const;
    UFUNCTION(BlueprintPure) bool IsReloading() const { return bReloading; }
    UFUNCTION(BlueprintPure) bool IsAttacking() const { return bAttacking; }
    UFUNCTION(BlueprintPure) bool IsAimHeld() const { return bAimHeld; }
    UFUNCTION(BlueprintPure) bool IsHitWindowOpen() const;
    UFUNCTION(BlueprintPure) int32 GetComboIndex() const { return ComboIndex; }
    UFUNCTION(BlueprintPure) float GetAttackElapsed() const;
    float GetAttackAnimationElapsed() const;
    UFUNCTION(BlueprintPure) float GetAimBlend() const { return AimBlend; }
    UFUNCTION(BlueprintPure) float GetADSMagnification() const { return ADSMagnification; }
    UFUNCTION(BlueprintPure) float GetCurrentADSMagnification() const;
    UFUNCTION(BlueprintPure) float GetReloadProgress() const;
    UFUNCTION(BlueprintPure) float GetShotPresentationPulse() const;
    const FHCM4AcceptedShot& GetLastAcceptedShot() const { return LastAcceptedShot; }
    FHCM4AcceptedShotEvent OnAcceptedShot;
    UFUNCTION(BlueprintPure) float GetAimLookMultiplier() const;
    UFUNCTION(BlueprintPure) float GetCurrentSpreadDegrees() const;
    UFUNCTION(BlueprintPure) int32 GetShotCount() const { return ShotCount; }
    UFUNCTION(BlueprintPure) int32 GetDamageHitCount() const { return DamageHitCount; }
    UFUNCTION(BlueprintPure) int32 GetEmptyTriggerCount() const { return EmptyTriggerCount; }
    UFUNCTION(BlueprintPure) AHCM4Weapon* GetWeaponActor() const { return WeaponActor; }
    const TArray<FHCM4CombatEvent>& GetEvents() const { return Events; }
    const FHitResult& GetLastHit() const { return LastHit; }
    bool WasLastMuzzleBlocked() const { return bLastMuzzleBlocked; }
    bool HasAnimationPlayback() const;
    float GetAnimationSlotWeight() const;
    float GetAnimationPosition() const;
    FName GetAnimationSlot() const;
    FString GetHUDText() const;
    void FillSave(UHCM1SaveGame* Save) const;
    bool ValidateSave(const UHCM1SaveGame* Save) const;
    void RestoreSave(const UHCM1SaveGame* Save);

    // Kept for old reflected data; the live gain is now derived once from projection magnification.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="M4|Aim") float AimLookScale = .625f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="M4|Aim") float ADSMagnification = 1.6f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="M4|Aim") float AimTransitionSeconds = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") int32 MagazineCapacity = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") int32 InitialReserve = 48;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float ShotInterval = .28f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float ReloadSeconds = 1.6f;
    /** Cone half-angles, measured from the camera's center ray. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float HipSpreadDegrees = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float AimSpreadDegrees = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float ShotRange = 15000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float BodyDamage = 34.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Pistol") float HeadDamage = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") float MeleeSphereRadius = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") float AttackMovementScale = .45f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") float ComboResetSeconds = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") TArray<float> AttackDurations = {.60f,.62f,.78f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") TArray<float> HitWindowStarts = {.240f,.248f,.312f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") TArray<float> HitWindowEnds = {.320f,.330667f,.416f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") TArray<float> ComboWindowStarts = {.31f,.32f,.45f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Melee") TArray<float> MeleeDamage = {25.f,25.f,40.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TArray<TSoftObjectPtr<UAnimSequence>> PunchAnimations;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TSoftClassPtr<UAnimInstance> PlayerAnimationBlueprint;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TSoftObjectPtr<UAnimSequence> PistolIdleAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TSoftObjectPtr<UAnimSequence> PistolAimAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TSoftObjectPtr<UAnimSequence> PistolFireAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TSoftObjectPtr<UAnimSequence> PistolReloadAnimation;
    /** Matching player-skeleton reaction; the accepted legacy asset remains the default. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M4|Animation") TSoftObjectPtr<UAnimMontage> PlayerHitFrontMontage;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<AHCM4Weapon> WeaponActor;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> PistolBaseMontage;
    UPROPERTY(Transient) TObjectPtr<UClass> LoadedPlayerAnimationClass;
    bool bAnimationInitialized = false;
    FName LastAnimationSlot;
    EHCM4WeaponMode WeaponMode = EHCM4WeaponMode::Unarmed;
    int32 MagazineAmmo = 12, ReserveAmmo = 48;
    float PlayerHealth = 100;
    bool bSeated = false, bAttacking = false, bReloading = false, bAimHeld = false;
    bool bComboQueued = false, bHasPunchPoseSample = false, bLastMuzzleBlocked = false, bAnimationPlayback = false;
    int32 ComboIndex = -1, ShotCount = 0, DamageHitCount = 0, EmptyTriggerCount = 0;
    double AttackStartTime = 0, LastAttackEnd = -100, ReloadEnd = 0, NextShotTime = 0;
    double LastNPCDamageTime = -1000, DefeatRecoveryUntil = 0, RecoveryInvulnerableUntil = 0;
    float AimBlend = 0;
    FVector PreviousHand = FVector::ZeroVector;
    float PreviousPunchPoseTime = -1, SampledPunchPlayRate = 0;
    int32 SampledPunchCombo = -1;
    TWeakObjectPtr<UAnimMontage> SampledPunchMontage;
    TSet<TWeakObjectPtr<AActor>> SwingHits;
    TArray<FHCM4CombatEvent> Events;
    FHitResult LastHit;
    FRandomStream SpreadRandom;
    FHCM4AcceptedShot LastAcceptedShot;
    AHCM1PlayerController* GetPC() const;
    double Now() const;
    void EnsureWeapon();
    void UpdatePresentation();
    bool StartPunch(int32 Index);
    void SweepPunch();
    void ResetPunchPoseSample();
    bool FirePistol();
    bool ApplyHit(const FHitResult& Hit, const FVector& Direction, float Damage, bool bGun);
    void PlayAnimation(UAnimSequence* Animation, float Duration, bool bLoop = false);
    void Record(FName Kind, float Value = 0, const FHitResult* Hit = nullptr,
        const FVector& Start = FVector::ZeroVector, const FVector& End = FVector::ZeroVector, bool bBlocked = false);
};
