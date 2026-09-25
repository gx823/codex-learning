#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "M1/HCInteractable.h"
#include "HCM3State.h"
#include "HCM3NPC.generated.h"

class AHCM1Vehicle;
class AHCM3NavRegion;
class AHCM3AIController;
class AHCM3Experience;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UHCM4R1ReactionComponent;
class UHCM4R1PhysicalReactionComponent;

UENUM(BlueprintType)
enum class EHCM3NPCBehaviour : uint8 { Idle, Walking, Yielding, Conversation, Passenger, Dormant, Disabled, HitReaction, Panic, Dead, RespawnWait, CounterApproach, CounterAttack, KnockedDown, Recovering };

/** Pedestrian with passive combat reactions; authored routes still use navigation and CharacterMovement. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM3NPC : public ACharacter, public IHCInteractable
{
    GENERATED_BODY()
public:
    AHCM3NPC();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
        bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") FName StableId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") FName RoleId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") TArray<FString> DialogueLines;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") bool bTalkable = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") bool bStationary = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") TObjectPtr<AHCM3NavRegion> NavigationRegion;
    /** World-space feet positions; never capsule-center positions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC") TArray<FVector> PatrolPoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC", meta=(ClampMin="40", ClampMax="220")) float WalkSpeed = 140;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|NPC", meta=(ClampMin="0.5")) float PatrolWaitSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4", meta=(ClampMin="1", ClampMax="1000")) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4") TObjectPtr<UAnimMontage> HitFrontMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4") TObjectPtr<UAnimMontage> HitBackMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4") TObjectPtr<UAnimMontage> HitLeftMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4") TObjectPtr<UAnimMontage> HitRightMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4", meta=(ClampMin="0.1")) float HitReactionCooldownSeconds = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4", meta=(ClampMin="0.1", ClampMax="0.5")) float HitStunSeconds = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4", meta=(ClampMin="140", ClampMax="450")) float PanicSpeed = 280.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4", meta=(ClampMin="1", ClampMax="600")) float CorpseLifetimeSeconds = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M4", meta=(ClampMin="0.1", ClampMax="5")) float CorpseFadeSeconds = 1.f;
    UFUNCTION(BlueprintPure) bool IsDead() const { return bDead; }
    /** Default mannequin pelvis; opt-in imported characters supply their verified humanoid hips. */
    virtual FName GetPhysicalRootBone() const { return TEXT("pelvis"); }
    virtual FName GetCounterHandBone() const { return TEXT("hand_r"); }
    /** Opt-in for reviewed imported ragdolls; legacy NPC collision filters stay unchanged. */
    virtual bool ShouldBlockPhysicsBodiesDuringRagdoll() const { return false; }
    /** Only opt-in characters replace inherited animation velocities on a new death ragdoll. */
    virtual bool ShouldInitializeDeathRagdollVelocity() const { return false; }
    virtual bool SupportsAuthoredGetUp() const { return false; }
    /** Reviewed imported NPCs may show a short hit-animation phase before handing off to physics. */
    virtual float GetPreRagdollReactionSeconds() const { return 0.f; }
    float PreparePreRagdollReaction();
    bool HasAdvancedPreRagdollReaction() const;
    bool QueueDeferredCorpseImpulse(const FVector& DeltaV);
    UFUNCTION(BlueprintPure) bool IsCombatEnabled() const { return bNPCEnabled && !bDead; }
    UFUNCTION(BlueprintPure) float GetHealth() const { return Health; }
    UFUNCTION(BlueprintPure) UHCM4R1ReactionComponent* GetReactionComponent() const { return Reaction; }
    UFUNCTION(BlueprintPure) UHCM4R1PhysicalReactionComponent* GetPhysicalReaction() const { return PhysicalReaction; }
    UFUNCTION(BlueprintPure) bool IsPhysicalReactionActive() const;
    void BeginPhysicalReactionState();
    void EndPhysicalReactionState(const FTransform& SafeStand, const FVector& Danger);
    bool ResolvePhysicalRecoveryStand(const FVector& ActualPelvisLocation, FTransform& Out) const;
    bool ResolvePhysicalGetUpStand(const FTransform& Requested, FTransform& Out) const;
    bool BeginPhysicalGetUpMovement();
    // Emergency recovery only: live validation without moving actors or changing health/tasks.
    bool ResolvePhysicalAuthoredRecoveryStand(FTransform& Out) const;
    UFUNCTION(BlueprintPure) bool IsPanicking() const { return !bDead && PanicUntil > 0; }
    UFUNCTION(BlueprintPure) bool IsCorpsePresent() const { return bDead && !bCorpseRemoved; }
    UFUNCTION(BlueprintPure) bool IsRagdollActive() const;
    UFUNCTION(BlueprintPure) float GetSecondsUntilCorpseRemoval() const;
    UFUNCTION(BlueprintPure) FString GetCombatDiagnostics() const;
    UFUNCTION(BlueprintCallable) void BeginPanic(const FVector& DangerLocation, float DurationSeconds = 12.f);
    /** Ends simulation immediately, then fades the oldest visible corpse without blocking gameplay. */
    UFUNCTION(BlueprintCallable) void BeginCorpseRemoval();
    bool RespawnAtAuthoredPost();
    void RestoreNamedDeathCooldown();
    bool ResetCombatForTest();
    UFUNCTION(BlueprintCallable) bool BeginConversation(AActor* Partner);
    UFUNCTION(BlueprintCallable) void EndConversation();
    /** Completes only at a valid, nearby door-side stand point; no visible seated animation. */
    UFUNCTION(BlueprintCallable) bool TryBoardPassenger(AHCM1Vehicle* Vehicle);
    /** Read-only validation using this passenger's own permitted navigation area. */
    bool ResolvePassengerDoorStand(const FTransform& Requested, FTransform& Out) const;
    UFUNCTION(BlueprintCallable) bool TryRestorePassenger(const FTransform& RequestedTransform, AHCM3NavRegion* DestinationRegion = nullptr);
    UFUNCTION(BlueprintPure) bool GetIsPassenger() const { return bPassenger; }
    UFUNCTION(BlueprintPure) bool IsConversationActive() const { return ConversationPartner.IsValid(); }
    UFUNCTION(BlueprintPure) bool IsNPCEnabled() const { return bNPCEnabled; }
    UFUNCTION(BlueprintCallable) void SetNPCEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure) EHCM3NPCBehaviour GetBehaviourState() const { return Behaviour; }
    UFUNCTION(BlueprintPure) int32 GetPatrolIndex() const { return PatrolIndex; }
    UFUNCTION(BlueprintPure) float GetTravelDistance() const { return TravelDistance; }
    UFUNCTION(BlueprintPure) int32 GetPathFailureCount() const { return PathFailures; }
    UFUNCTION(BlueprintPure) int32 GetSafetyYieldCount() const { return SafetyYields; }
    UFUNCTION(BlueprintPure) int32 GetSafetyContactCount() const { return SafetyContacts; }
    UFUNCTION(BlueprintPure) int32 GetDecisionTickCount() const { return DecisionTicks; }
    UFUNCTION(BlueprintPure) bool IsAtPermittedLocation() const;
    FHCM3NPCState CaptureState() const;
    bool CanRestoreState(const FHCM3NPCState& State) const;
    bool RestoreState(const FHCM3NPCState& State, AHCM1Vehicle* PassengerVehicle = nullptr);
    void OnNavigationMoveFinished(bool bSuccess);
    virtual FString GetInteractionText_Implementation(APlayerController* Player) const override;
    virtual void Interact_Implementation(APlayerController* Player) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Default retains the existing M4 appearance path. New independent NPC subclasses may opt out. */
    virtual bool ApplyInitialVisuals();
private:
    friend class UHCM4R1ReactionComponent;
    UPROPERTY(VisibleAnywhere, Category="HarborCity|M4R1") TObjectPtr<UHCM4R1ReactionComponent> Reaction;
    UPROPERTY(VisibleAnywhere, Category="HarborCity|M4R1") TObjectPtr<UHCM4R1PhysicalReactionComponent> PhysicalReaction;
    EHCM3NPCBehaviour Behaviour = EHCM3NPCBehaviour::Idle;
    bool bNPCEnabled = true;
    bool bPassenger = false;
    bool bStoppingMove = false;
    bool bDead = false;
    bool bCorpseRemoved = false;
    bool bCorpseFading = false;
    bool bM4Available = false;
    bool bInitialStationary = false;
    bool bReturningToPost = false;
    float Health = 100.f;
    double DeathTime = 0;
    double FadeStarted = 0;
    double LastHitReactionTime = -1000;
    double HitStunUntil = 0;
    double PanicUntil = 0;
    double NextPanicMove = 0;
    FVector DangerPoint = FVector::ZeroVector;
    FVector HitFacingTarget = FVector::ZeroVector;
    FTransform InitialMeshRelativeTransform;
    FHCM3NPCState AuthoredState;
    mutable double NextSaveCaptureDiagnosticTime = -1;
    FName LastHitDirection;
    FName LastHitBone;
    float LastDamage = 0;
    int32 DamageEvents = 0;
    int32 ReactionPlays = 0;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> PreRagdollMontage;
    bool bPendingCorpsePhysics = false;
    double CorpsePhysicsDue = 0, PreRagdollStarted = 0;
    uint64 PreRagdollStartedFrame = 0;
    FHitResult DeferredCorpseHit;
    FVector DeferredCorpseDirection = FVector::ZeroVector;
    FVector DeferredCorpseVelocity = FVector::ZeroVector;
    FVector DeferredCorpseDeltaV = FVector::ZeroVector;
    bool bDeferredCorpseDefaultImpulse = true;
    float LastPreRagdollPoseSeconds = 0;
    int32 PanicEntries = 0;
    int32 PanicRecoveries = 0;
    int32 PanicPathAttempts = 0, PanicPathFailures = 0;
    float CorpseOpacity = 1.f;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CorpseMaterials;
    TWeakObjectPtr<AHCM3Experience> Experience;
    TWeakObjectPtr<AHCM3AIController> DetachedAI;
    int32 PatrolIndex = 0;
    int32 PathFailures = 0;
    int32 SafetyYields = 0;
    int32 SafetyContacts = 0;
    int32 DecisionTicks = 0;
    float TravelDistance = 0;
    double NextDecisionTime = 0;
    double LastProgressTime = 0;
    double SafetyCooldownUntil = 0;
    FVector LastObservedPosition = FVector::ZeroVector;
    FVector LastProgressPosition = FVector::ZeroVector;
    TWeakObjectPtr<AActor> ConversationPartner;
    TWeakObjectPtr<AHCM1Vehicle> PassengerVehicle;
    TArray<TWeakObjectPtr<AHCM3NavRegion>> Regions;
    TArray<TWeakObjectPtr<AHCM1Vehicle>> Vehicles;
    AHCM3AIController* GetNPCAI() const;
    bool IsPermittedFeetPoint(const FVector& Point, bool bForStanding, const AHCM3NavRegion* RegionOverride = nullptr) const;
    bool ResolveStandTransform(const FTransform& Requested, FTransform& Out, const AHCM3NavRegion* RegionOverride = nullptr, bool bSavedStateValidation = false) const;
    AHCM3NavRegion* FindRegionById(FName Id) const;
    AHCM3NavRegion* FindUniqueRegionAt(const FVector& Point) const;
    bool RequestWalk(const FVector& FeetGoal, bool bSafetyMove = false, float AcceptanceRadius = 35.f);
    void StopNavigation();
    bool TryYieldFrom(AActor* Obstacle, bool bVehicle);
    bool CheckNearbySafety();
    void ApplyVisibilityAndTicks();
    void Die(const FHitResult& Hit, const FVector& Direction, bool bApplyDefaultImpulse = true);
    void StartCorpsePhysics(const FHitResult& Hit, const FVector& Direction,
        const FVector& InitialVelocity, bool bInitializeVelocity, bool bApplyDefaultImpulse);
    void UpdateCorpse(double Now);
    void ClearCorpse();
    void ResetCombatForRestore();
    void UpdatePanic(double Now);
    bool IsPanicPathAllowed(const FVector& Point) const;
};
