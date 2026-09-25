#include "HCM4R1PhysicalReactionComponent.h"
#include "HCM4R1VehicleDamageType.h"
#include "M3/HCM3NPC.h"
#include "M1/HCM1Vehicle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UHCM4R1PhysicalReactionComponent::UHCM4R1PhysicalReactionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    // Recovery establishes the animation reference before mesh evaluation and end-physics blending.
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.TickInterval = .05f;
}

bool UHCM4R1PhysicalReactionComponent::TryAnimatedKnockdown(const FVector& Source)
{
    NPC=Cast<AHCM3NPC>(GetOwner());auto* N=NPC.Get();
    if(!bAnimationPrimaryKnockdown||!N||N->IsDead()||N->GetIsPassenger()||bActive||bPendingImpact||AnimatedFallStage>0||!bGetUpPoseDataVerified)return false;
    const FVector From=N->GetActorTransform().InverseTransformVectorNoScale(Source-N->GetActorLocation()).GetSafeNormal2D();
    const auto Direction=FMath::Abs(From.X)>FMath::Abs(From.Y)?(From.X>0?EHCM4R1GetUpDirection::Supine:EHCM4R1GetUpDirection::Prone):(From.Y>0?EHCM4R1GetUpDirection::Left:EHCM4R1GetUpDirection::Right);
    GetUpClipIndex=GetUpClips.IndexOfByPredicate([&](const FHCM4R1GetUpClip& C){return C.Direction==Direction;});
    if(!AnimatedFallClips.IsValidIndex(GetUpClipIndex))return false;
    auto* Fall=AnimatedFallClips[GetUpClipIndex].LoadSynchronous();auto* GetUp=GetUpClips[GetUpClipIndex].Animation.LoadSynchronous();auto* Mesh=N->GetMesh();auto* Anim=Mesh->GetAnimInstance();
    if(!Fall||!GetUp||!Anim)return false;
    if(!N->ResolvePhysicalGetUpStand(N->GetActorTransform(),RecoveryStand))return false;
    InitialMeshRelative=Mesh->GetRelativeTransform();DangerPoint=Source;
    bOriginalUpdateRate=Mesh->bEnableUpdateRateOptimizations;OriginalVisibilityTickOption=uint8(Mesh->VisibilityBasedAnimTickOption);bSavedAnimationTick=true;
    N->BeginPhysicalReactionState();N->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->bEnableUpdateRateOptimizations=false;Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    AnimatedFallMontage=Anim->PlaySlotAnimationAsDynamicMontage(Fall,TEXT("FullBody"),.10f,.02f,Fall->GetPlayLength()/1.05f);
    if(!AnimatedFallMontage){RestoreAnimationTick();N->EndPhysicalReactionState(RecoveryStand,Source);return false;}
    bActive=true;bRecovering=false;AnimatedFallStage=1;AnimatedStageStart=GetWorld()->GetTimeSeconds();SetComponentTickInterval(0);SetComponentTickEnabled(true);
    UE_LOG(LogTemp,Display,TEXT("VS3_ANIMATED_KNOCKDOWN id=%s direction=%d"),*N->StableId.ToString(),int32(Direction));return true;
}

bool UHCM4R1PhysicalReactionComponent::TickAnimatedKnockdown()
{
    auto* N=NPC.Get();auto* Mesh=N->GetMesh();auto* Anim=Mesh->GetAnimInstance();const double Age=GetWorld()->GetTimeSeconds()-AnimatedStageStart;
    if(!Anim){LastRecoveryReason=TEXT("ANIMATED_FALL_ANIM_INSTANCE_LOST");return false;}
    if(AnimatedFallStage==1 && Age>=.95){
        // Contact is the only physics window. Animation retains 75% of the pose.
        Anim->Montage_Pause(AnimatedFallMontage);
        Mesh->SetCollisionObjectType(ECC_PhysicsBody);Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetSimulatePhysics(true);Mesh->SetAllBodiesPhysicsBlendWeight(.25f);Mesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);Mesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector,false);
        AnimatedFallStage=2;AnimatedStageStart=GetWorld()->GetTimeSeconds();
    } else if(AnimatedFallStage==2 && Age>=.16){
        Mesh->SetSimulatePhysics(false);Mesh->SetAllBodiesPhysicsBlendWeight(0);Mesh->AttachToComponent(N->GetCapsuleComponent(),FAttachmentTransformRules::KeepWorldTransform);Mesh->SetRelativeTransform(InitialMeshRelative);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Mesh->SetCollisionObjectType(ECC_Pawn);Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        auto* Clip=GetUpClips[GetUpClipIndex].Animation.Get();Anim->Montage_Stop(0,AnimatedFallMontage);
        AnimatedFallMontage=Anim->PlaySlotAnimationAsDynamicMontage(Clip,TEXT("FullBody"),.12f,.12f,1);
        AnimatedGetUpLength=Clip->GetPlayLength();AnimatedFallStage=3;bRecovering=true;AnimatedStageStart=GetWorld()->GetTimeSeconds();
    } else if(AnimatedFallStage==3 && Age>=AnimatedGetUpLength){
        // Recheck the actual capsule before enabling walking; never force it through a wall/car.
        FTransform Safe;if(!N->ResolvePhysicalGetUpStand(RecoveryStand,Safe)){LastRecoveryReason=TEXT("ANIMATED_GETUP_STAND_BLOCKED");return false;}
        RecoveryStand=Safe;LastLanding=N->GetActorLocation();AnimatedFallStage=0;CompleteRecovery();
        UE_LOG(LogTemp,Display,TEXT("VS3_ANIMATED_GETUP_END id=%s direction=%d"),*N->StableId.ToString(),int32(GetUpClips[GetUpClipIndex].Direction));
    }
    return true;
}

FVector UHCM4R1PhysicalReactionComponent::GetPhysicalLocation() const
{
    const AHCM3NPC* OwnerNPC = NPC.IsValid() ? NPC.Get() : Cast<AHCM3NPC>(GetOwner());
    if (!OwnerNPC || !OwnerNPC->GetMesh()) return FVector::ZeroVector;
    if (const FBodyInstance* Body = OwnerNPC->GetMesh()->GetBodyInstance(OwnerNPC->GetPhysicalRootBone()))
        if (Body->IsInstanceSimulatingPhysics()) return Body->GetUnrealWorldTransform().GetLocation();
    return OwnerNPC->GetMesh()->GetSocketLocation(OwnerNPC->GetPhysicalRootBone());
}

bool UHCM4R1PhysicalReactionComponent::StartLivingRagdoll(const FVector& InitialVelocity)
{
    AHCM3NPC* OwnerNPC = NPC.Get();
    if (!OwnerNPC || OwnerNPC->IsDead() || OwnerNPC->GetIsPassenger()) return false;
    USkeletalMeshComponent* Mesh = OwnerNPC->GetMesh();
    if (!Mesh || !Mesh->GetPhysicsAsset() || Mesh->GetBoneIndex(OwnerNPC->GetPhysicalRootBone()) == INDEX_NONE) return false;
    InitialMeshRelative = Mesh->GetRelativeTransform();
    bOriginalUpdateRate = Mesh->bEnableUpdateRateOptimizations;
    OriginalVisibilityTickOption = uint8(Mesh->VisibilityBasedAnimTickOption); bSavedAnimationTick = true;
    bHasSafeStand = OwnerNPC->ResolvePhysicalRecoveryStand(Mesh->GetSocketLocation(OwnerNPC->GetPhysicalRootBone()), LastSafeStand);
    OwnerNPC->BeginPhysicalReactionState();
    if (UAnimInstance* Anim = Mesh->GetAnimInstance()) Anim->Montage_Stop(.08f);
    OwnerNPC->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Mesh->SetCollisionObjectType(ECC_PhysicsBody);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    if (OwnerNPC->ShouldBlockPhysicsBodiesDuringRagdoll())
        Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    // Query overlaps permit another car contact while its solver cannot trap the body.
    Mesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetComponentTickEnabled(true);
    Mesh->bPauseAnims = false;
    Mesh->bEnableUpdateRateOptimizations = false;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->AddTickPrerequisiteComponent(this);
    Mesh->SetSimulatePhysics(true);
    Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
    if (!Mesh->IsSimulatingPhysics())
    {
        Mesh->SetAllBodiesPhysicsBlendWeight(0.f);
        Mesh->AttachToComponent(OwnerNPC->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        Mesh->SetRelativeTransform(InitialMeshRelative);
        Mesh->SetCollisionObjectType(ECC_Pawn);
        Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        RestoreAnimationTick();
        OwnerNPC->EndPhysicalReactionState(OwnerNPC->GetActorTransform(), DangerPoint);
        return false;
    }
    // Carry existing locomotion velocity once at the state transition; later motion is Chaos.
    Mesh->SetAllPhysicsLinearVelocity(InitialVelocity);
    if (OwnerNPC->GetPreRagdollReactionSeconds() > 0)
        Mesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
    Mesh->WakeAllRigidBodies();
    bActive = true; bRecovering = false; bEmergencyRecovery = false;
    StartedAt = GetWorld()->GetTimeSeconds(); StableSince = 0; NextRecoveryCheck = StartedAt + 1.25;
    SetComponentTickEnabled(true);
    return true;
}

void UHCM4R1PhysicalReactionComponent::ApplyDistributedImpulse(const FVector& DeltaV)
{
    AHCM3NPC* OwnerNPC = NPC.Get();
    // Logical death can precede the short visible hit pose. Do not lose the car impulse
    // before the corpse's bodies exist; the same mass-weighted impulse is applied once then.
    if (OwnerNPC && OwnerNPC->QueueDeferredCorpseImpulse(DeltaV)) return;
    if (!OwnerNPC || !OwnerNPC->GetMesh()->IsSimulatingPhysics()) return;
    USkeletalMeshComponent* Mesh = OwnerNPC->GetMesh();
    LastTotalMassKg = 0; LastBodyCount = 0; LastImpulse = FVector::ZeroVector;
    for (FBodyInstance* Body : Mesh->Bodies)
    {
        if (!Body || !Body->IsInstanceSimulatingPhysics()) continue;
        const float Mass = Body->GetBodyMass();
        if (!FMath::IsFinite(Mass) || Mass <= 0) continue;
        const FVector BodyImpulse = DeltaV * Mass;
        // Sum(J_i) = total simulated mass * deltaV. Never apply the whole-person J per bone.
        Body->AddImpulse(BodyImpulse, false);
        LastTotalMassKg += Mass; LastImpulse += BodyImpulse; ++LastBodyCount;
    }
    if (LastBodyCount > 0) { ++ImpulseEvents; LastDeltaV = DeltaV; Mesh->WakeAllRigidBodies(); }
}

float UHCM4R1PhysicalReactionComponent::ReceiveVehicleImpact(AHCM1Vehicle* Vehicle, const FHitResult& Hit,
    const FVector& RelativeVelocity, const FVector& OutwardDirection, float ClosingSpeedCmS, int32 Serial, FName Source)
{
    NPC = Cast<AHCM3NPC>(GetOwner());
    AHCM3NPC* OwnerNPC = NPC.Get();
    if (!OwnerNPC || !IsValid(Vehicle) || !OwnerNPC->IsCombatEnabled() || OwnerNPC->GetIsPassenger() ||
        RelativeVelocity.ContainsNaN() || !FMath::IsFinite(ClosingSpeedCmS)) return 0;
    ++ImpactEvents; LastClosingSpeed = FMath::Max(0.f, ClosingSpeedCmS); LastContactSerial = Serial; LastSource = Source;
    DangerPoint = Vehicle->GetActorLocation(); LastDamage = 0;
    if (ClosingSpeedCmS < FMath::Max(100.f, DamageThresholdCmS)) return 0;
    if (bRecovering && AnimatedFallStage==0) AbortRecovery();
    const FVector BeforeVelocity = bActive ? OwnerNPC->GetMesh()->GetPhysicsLinearVelocity(OwnerNPC->GetPhysicalRootBone()) : OwnerNPC->GetVelocity();
    const float Damage = FMath::Clamp(4.f + (ClosingSpeedCmS - FMath::Max(100.f, DamageThresholdCmS)) * .08f, 4.f, 110.f);
    const FVector Direction = (OutwardDirection.GetSafeNormal2D() * .7f + RelativeVelocity.GetSafeNormal2D() * .3f).GetSafeNormal2D();
    FPointDamageEvent Event(Damage, Hit, Direction, UHCM4R1VehicleDamageType::StaticClass());
    LastDamage = OwnerNPC->TakeDamage(Damage, Event, Vehicle->GetController(), Vehicle);
    if (LastDamage <= 0) return 0;
    if (ClosingSpeedCmS >= FMath::Max(DamageThresholdCmS, KnockdownThresholdCmS))
    {
        if(!OwnerNPC->IsDead() && bAnimationPrimaryKnockdown && (AnimatedFallStage>0 || TryAnimatedKnockdown(DangerPoint)))return LastDamage;
        const float Horizontal = FMath::Clamp(ClosingSpeedCmS * .65f, 180.f, FMath::Clamp(MaximumDeltaVCmS, 300.f, 1600.f));
        const float Lift = FMath::Clamp(100.f + (ClosingSpeedCmS - KnockdownThresholdCmS) * .15f, 100.f, 260.f);
        const FVector DeltaV = Direction * Horizontal + FVector(0, 0, Lift);
        if (!OwnerNPC->IsDead() && !bActive && !bPendingImpact)
        {
            const float Delay = OwnerNPC->PreparePreRagdollReaction();
            if (Delay > 0)
            {
                bPendingImpact = true; PendingImpactStarted = GetWorld()->GetTimeSeconds();
                PendingImpactDue = PendingImpactStarted + Delay;
                PendingImpactVelocity = BeforeVelocity; PendingImpactDeltaV = DeltaV;
                SetComponentTickEnabled(true);
                UE_LOG(LogTemp, Display, TEXT("M5VS2_IMPACT_REACTION_BEGIN id=%s delay=%.3f"), *OwnerNPC->StableId.ToString(), Delay);
                return LastDamage;
            }
        }
        if (bPendingImpact)
        {
            PendingImpactDeltaV = (PendingImpactDeltaV + DeltaV).GetClampedToMaxSize(1800.f);
            return LastDamage;
        }
        if (!OwnerNPC->IsDead() && !bActive && !StartLivingRagdoll(BeforeVelocity))
        {
            LastRecoveryReason = TEXT("PHYSICS_ASSET_OR_SIMULATION_UNAVAILABLE");
            UE_LOG(LogTemp, Error, TEXT("M4R1_IMPACT_NO_PHYSICS id=%s"), *OwnerNPC->StableId.ToString());
            return LastDamage;
        }
        ApplyDistributedImpulse(DeltaV);
        if (bActive) { StartedAt = GetWorld()->GetTimeSeconds(); StableSince = 0; NextRecoveryCheck = StartedAt + 1.25; }
    }
    return LastDamage;
}

void UHCM4R1PhysicalReactionComponent::BeginRecovery(const FTransform& Stand, bool bEmergency)
{
    AHCM3NPC* OwnerNPC = NPC.Get();
    if (!OwnerNPC || OwnerNPC->IsDead()) return;
    if (bUseAuthoredGetUp && OwnerNPC->SupportsAuthoredGetUp())
    {
        // Opt-in recovery must match the actual nearby fallen pose. An authored
        // post/emergency teleport is not a substitute for a real get-up clip.
        if (bEmergency || !TryBeginAuthoredGetUp())
        {
            if (bEmergency) GetUpLastResult=TEXT("AUTHORED_GETUP_BLOCKED_NO_EMERGENCY_TELEPORT");
            LastRecoveryReason=GetUpLastResult; ++RecoveryBlocked;
            NextRecoveryCheck=GetWorld()->GetTimeSeconds()+1;
        }
        return;
    }
    LastLanding = GetPhysicalLocation(); RecoveryStand = Stand; bEmergencyRecovery = bEmergency;
    USkeletalMeshComponent* Mesh = OwnerNPC->GetMesh();
    // Only the disabled capsule is aligned to a validated landing stand point.
    // The detached simulated skeleton keeps its physical world pose during this move.
    OwnerNPC->SetActorTransform(Stand, false, nullptr, ETeleportType::None);
    // UE refuses to attach a simulating body at runtime. Keep it detached and set
    // only the component's animation reference; Teleport=None leaves simulated bones in Chaos.
    OriginalPhysicsTransformUpdateMode = uint8(Mesh->PhysicsTransformUpdateMode);
    bRecoveryOverridesTransformMode = true;
    Mesh->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::ComponentTransformIsKinematic;
    RecoveryMeshWorld = InitialMeshRelative * Stand;
    RecoveryBodyBeforeReference = GetPhysicalLocation();
    Mesh->SetWorldTransform(RecoveryMeshWorld, false, nullptr, ETeleportType::None);
    RecoveryBodyAfterReference = GetPhysicalLocation();
    if (bEmergency)
    {
        // Exceptional recovery is reported explicitly, never presented as a physical landing.
        Mesh->SetSimulatePhysics(false);
        CompleteRecovery();
        return;
    }
    Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
    RecoveryStartedAt = GetWorld()->GetTimeSeconds(); bRecovering = true;
}

void UHCM4R1PhysicalReactionComponent::AbortRecovery()
{
    AHCM3NPC* OwnerNPC = NPC.Get();
    if (!OwnerNPC || !bRecovering) return;
    if (GetUpStage==EGetUpStage::Playing)
    {
        const bool bRestarted=RestartGetUpRagdoll();
        ++RecoveryInterruptions; StableSince=0;
        NextRecoveryCheck=GetWorld()->GetTimeSeconds()+.75;
        if (!bRestarted) LastRecoveryReason=TEXT("GETUP_INTERRUPT_PHYSICS_RESTART_FAILED");
        return;
    }
    if (GetUpStage==EGetUpStage::Align) CancelAuthoredGetUp();
    OwnerNPC->GetMesh()->SetAllBodiesPhysicsBlendWeight(1.f);
    OwnerNPC->GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    RestorePhysicsTransformMode();
    bRecovering = false; ++RecoveryInterruptions; StableSince = 0;
    NextRecoveryCheck = GetWorld()->GetTimeSeconds() + .75;
}

void UHCM4R1PhysicalReactionComponent::NotifySurvivingDamage(const FVector& Source)
{
    if (!bActive || !GetWorld()) return;
    if (!Source.ContainsNaN()) DangerPoint = Source;
    if(AnimatedFallStage>0)return;
    if (bRecovering) AbortRecovery();
    StartedAt = GetWorld()->GetTimeSeconds(); StableSince = 0;
    NextRecoveryCheck = StartedAt + FMath::Max(1.f, MinimumDownSeconds);
}

void UHCM4R1PhysicalReactionComponent::RestoreAnimationTick()
{
    if (AHCM3NPC* OwnerNPC = NPC.Get()) OwnerNPC->GetMesh()->RemoveTickPrerequisiteComponent(this);
    if (bSavedAnimationTick)
        if (AHCM3NPC* OwnerNPC = NPC.Get())
        {
            OwnerNPC->GetMesh()->bEnableUpdateRateOptimizations = bOriginalUpdateRate;
            OwnerNPC->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption(OriginalVisibilityTickOption);
        }
    bSavedAnimationTick = false;
}

void UHCM4R1PhysicalReactionComponent::RestorePhysicsTransformMode()
{
    if (bRecoveryOverridesTransformMode)
        if (AHCM3NPC* OwnerNPC = NPC.Get()) OwnerNPC->GetMesh()->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::Type(OriginalPhysicsTransformUpdateMode);
    bRecoveryOverridesTransformMode = false;
}

void UHCM4R1PhysicalReactionComponent::CompleteRecovery()
{
    AHCM3NPC* OwnerNPC = NPC.Get();
    if (!OwnerNPC || OwnerNPC->IsDead()) return;
    USkeletalMeshComponent* Mesh = OwnerNPC->GetMesh();
    Mesh->SetSimulatePhysics(false);
    Mesh->SetAllBodiesPhysicsBlendWeight(0.f);
    RestorePhysicsTransformMode();
    bLastRecoveryAttachSucceeded = Mesh->AttachToComponent(OwnerNPC->GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
    if (!bLastRecoveryAttachSucceeded)
    {
        // Do not apply relative coordinates to an unattached mesh. Keep the living
        // actor and its real bodies, and retry a validated recovery later.
        Mesh->SetSimulatePhysics(true); Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
        bRecovering = false; StableSince = 0; ++RecoveryBlocked;
        NextRecoveryCheck = GetWorld()->GetTimeSeconds() + .4;
        LastRecoveryReason = TEXT("ATTACH_FAILED_RETRY_LIVING_PHYSICS");
        UE_LOG(LogTemp, Warning, TEXT("M4R1_RECOVERY_ATTACH_FAILED id=%s"), *OwnerNPC->StableId.ToString());
        return;
    }
    Mesh->SetRelativeTransform(InitialMeshRelative);
    Mesh->SetCollisionObjectType(ECC_Pawn);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->bPauseAnims = false;
    RestoreAnimationTick();
    bActive = false; bRecovering = false;
    OwnerNPC->EndPhysicalReactionState(RecoveryStand, DangerPoint);
    if (!OwnerNPC->GetCapsuleComponent()->BodyInstance.IsValidBodyInstance())
    {
        // A failed capsule allocation must not count as recovery or leave walking
        // AI with collision only in configuration. Keep this living actor physical.
        StartLivingRagdoll(FVector::ZeroVector);
        LastRecoveryReason = TEXT("CAPSULE_BODY_RECREATE_FAILED_RETRY_LIVING_PHYSICS");
        ++RecoveryBlocked;
        return;
    }
    ++RecoveryEvents;
    if (bEmergencyRecovery) { ++EmergencyRecoveries; LastRecoveryReason = TEXT("OUTSIDE_REACHABLE_AREA_SAFE_FALLBACK"); }
    else LastRecoveryReason = TEXT("LANDED_GROUND_CAPSULE_NAV_VALID");
    SetComponentTickEnabled(false);
    UE_LOG(LogTemp, Display, TEXT("M4R1_PHYSICAL_RECOVERY id=%s reason=%s landing=%s stand=%s health=%.2f"),
        *OwnerNPC->StableId.ToString(), *LastRecoveryReason, *LastLanding.ToCompactString(), *RecoveryStand.GetLocation().ToCompactString(), OwnerNPC->GetHealth());
}

void UHCM4R1PhysicalReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AHCM3NPC* OwnerNPC = NPC.Get();
    if ((!bActive && !bPendingImpact) || !OwnerNPC || OwnerNPC->IsDead()) { OnOwnerDied(); return; }
    if(AnimatedFallStage>0){TickAnimatedKnockdown();return;}
    const double Now = GetWorld()->GetTimeSeconds();
    if (bPendingImpact)
    {
        if (Now < PendingImpactDue) return;
        const bool bMontageAdvanced = OwnerNPC->HasAdvancedPreRagdollReaction();
        if (!bMontageAdvanced && Now - PendingImpactStarted < .5) return;
        LastPreImpactSeconds = float(Now - PendingImpactStarted);
        UE_LOG(LogTemp, Display, TEXT("M5VS2_IMPACT_REACTION_HANDOVER id=%s montage_advanced=%d elapsed_s=%.3f"),
            *OwnerNPC->StableId.ToString(), bMontageAdvanced, LastPreImpactSeconds);
        bPendingImpact = false;
        if (StartLivingRagdoll(PendingImpactVelocity)) ApplyDistributedImpulse(PendingImpactDeltaV);
        else { LastRecoveryReason = TEXT("PRE_IMPACT_HANDOVER_PHYSICS_FAILED"); SetComponentTickEnabled(false); }
        PendingImpactDeltaV = FVector::ZeroVector;
        return;
    }
    if (GetUpStage!=EGetUpStage::None) { TickAuthoredGetUp(); return; }
    if (bRecovering)
    {
        FTransform StillSafe;
        if (!OwnerNPC->ResolvePhysicalRecoveryStand(RecoveryStand.GetLocation() - FVector(0, 0, 30), StillSafe) ||
            FVector::Dist2D(StillSafe.GetLocation(), RecoveryStand.GetLocation()) > 10)
        { AbortRecovery(); return; }
        const float Alpha = FMath::Clamp(float((Now - RecoveryStartedAt) / FMath::Clamp(RecoveryBlendSeconds, .35f, 1.5f)), 0.f, 1.f);
        OwnerNPC->GetMesh()->SetAllBodiesPhysicsBlendWeight(1.f - FMath::SmoothStep(0.f, 1.f, Alpha));
        if (Alpha >= 1.f) CompleteRecovery();
        return;
    }
    LastMaxLinearSpeed = 0; LastMaxAngularSpeed = 0;
    for (const FBodyInstance* Body : OwnerNPC->GetMesh()->Bodies)
        if (Body && Body->IsInstanceSimulatingPhysics())
        {
            LastMaxLinearSpeed = FMath::Max(LastMaxLinearSpeed, float(Body->GetUnrealWorldVelocity().Size()));
            LastMaxAngularSpeed = FMath::Max(LastMaxAngularSpeed, float(Body->GetUnrealWorldAngularVelocityInRadians().Size()));
        }
    const bool bStable = LastMaxLinearSpeed < 55.f && LastMaxAngularSpeed < 1.5f;
    if (!bStable) StableSince = 0; else if (StableSince <= 0) StableSince = Now;
    if (Now < NextRecoveryCheck) return;
    NextRecoveryCheck = Now + .4;
    const FVector Pelvis = GetPhysicalLocation();
    FTransform Stand;
    if (Now - StartedAt >= FMath::Max(1.f, MinimumDownSeconds) && StableSince > 0 &&
        Now - StableSince >= FMath::Clamp(StableSeconds, .25f, 2.f))
    {
        if (OwnerNPC->ResolvePhysicalRecoveryStand(Pelvis, Stand)) { BeginRecovery(Stand, false); return; }
        ++RecoveryBlocked;
        // Physics retains world collision; sleep settled blocked bodies and retry at 2.5 Hz.
        OwnerNPC->GetMesh()->PutAllRigidBodiesToSleep();
    }
    if (Now - StartedAt >= FMath::Max(20.f, EmergencyRecoverySeconds))
    {
        const bool NearbySafe = bHasSafeStand && OwnerNPC->ResolvePhysicalRecoveryStand(LastSafeStand.GetLocation() - FVector(0, 0, 30), Stand);
        if (NearbySafe || OwnerNPC->ResolvePhysicalAuthoredRecoveryStand(Stand))
        {
            LastEmergencySource = NearbySafe ? TEXT("LAST_VALIDATED_NEARBY") : TEXT("AUTHORED_POST_REALTIME_VALIDATED");
            UE_LOG(LogTemp, Warning, TEXT("M4R1_EMERGENCY_RECOVERY id=%s source=%s physical=%s safe=%s health=%.2f"),
                *OwnerNPC->StableId.ToString(), NearbySafe ? TEXT("LAST_VALIDATED_NEARBY") : TEXT("AUTHORED_POST_REALTIME_VALIDATED"),
                *GetPhysicalLocation().ToCompactString(), *Stand.GetLocation().ToCompactString(), OwnerNPC->GetHealth());
            BeginRecovery(Stand, true);
        }
    }
}

void UHCM4R1PhysicalReactionComponent::OnOwnerDied()
{
    AnimatedFallStage=0;
    CancelAuthoredGetUp();
    RestorePhysicsTransformMode();
    RestoreAnimationTick();
    bPendingImpact = false; PendingImpactDeltaV = FVector::ZeroVector;
    bActive = false; bRecovering = false; StableSince = 0; NextRecoveryCheck = 0;
    SetComponentTickEnabled(false);
}

void UHCM4R1PhysicalReactionComponent::ResetForRestore()
{
    AnimatedFallStage=0;
    CancelAuthoredGetUp();
    if (bActive)
        if (AHCM3NPC* OwnerNPC = NPC.Get())
        {
            OwnerNPC->GetMesh()->SetSimulatePhysics(false);
            OwnerNPC->GetMesh()->SetAllBodiesPhysicsBlendWeight(0.f);
            OwnerNPC->GetMesh()->AttachToComponent(OwnerNPC->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            OwnerNPC->GetMesh()->SetRelativeTransform(InitialMeshRelative);
            OwnerNPC->GetMesh()->SetCollisionObjectType(ECC_Pawn);
            OwnerNPC->GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
            OwnerNPC->GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            OwnerNPC->GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
    RestoreAnimationTick();
    OnOwnerDied(); bHasSafeStand = false; bEmergencyRecovery = false;
}

FString UHCM4R1PhysicalReactionComponent::GetDiagnostics() const
{
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("living_physics_active"), bActive); Data->SetBoolField(TEXT("recovering"), bRecovering);
    Data->SetBoolField(TEXT("pre_impact_reaction_pending"), bPendingImpact);
    Data->SetNumberField(TEXT("pre_impact_reaction_seconds_at_handover"), LastPreImpactSeconds);
    Data->SetBoolField(TEXT("authored_getup_enabled"),bUseAuthoredGetUp);
    Data->SetBoolField(TEXT("authored_getup_pose_data_verified"),bGetUpPoseDataVerified);
    Data->SetNumberField(TEXT("getup_stage"),int32(GetUpStage));
    Data->SetNumberField(TEXT("getup_clip_index"),GetUpClipIndex);
    Data->SetStringField(TEXT("getup_result"),GetUpLastResult);
    Data->SetNumberField(TEXT("getup_alignment_error_cm"),GetUpAlignmentErrorCm);
    Data->SetNumberField(TEXT("getup_end_hips_error_cm"),GetUpEndHipsErrorCm);
    Data->SetNumberField(TEXT("getup_direction_score"),GetUpDirectionScore);
    Data->SetNumberField(TEXT("getup_montage_position_s"),GetUpLastMontagePosition);
    Data->SetNumberField(TEXT("authored_getup_completions"),AuthoredGetUpCompletions);
    Data->SetBoolField(TEXT("getup_montage_end_observed"),bGetUpMontageEnded);
    Data->SetStringField(TEXT("getup_clip"),GetUpClips.IsValidIndex(GetUpClipIndex) ? GetUpClips[GetUpClipIndex].Animation.ToString() : FString());
    Data->SetStringField(TEXT("physical_location_cm"), GetPhysicalLocation().ToCompactString());
    Data->SetNumberField(TEXT("impact_events"), ImpactEvents); Data->SetNumberField(TEXT("whole_person_impulse_events"), ImpulseEvents);
    Data->SetNumberField(TEXT("recovery_events"), RecoveryEvents); Data->SetNumberField(TEXT("blocked_recovery_checks"), RecoveryBlocked);
    Data->SetNumberField(TEXT("emergency_recoveries"), EmergencyRecoveries); Data->SetNumberField(TEXT("recovery_interruptions"), RecoveryInterruptions);
    Data->SetNumberField(TEXT("last_contact_serial"), LastContactSerial); Data->SetStringField(TEXT("source"), LastSource.ToString());
    Data->SetNumberField(TEXT("closing_speed_cm_s"), LastClosingSpeed); Data->SetNumberField(TEXT("damage"), LastDamage);
    Data->SetNumberField(TEXT("simulated_mass_kg"), LastTotalMassKg); Data->SetNumberField(TEXT("distributed_body_count"), LastBodyCount);
    Data->SetStringField(TEXT("whole_person_impulse_kg_cm_s"), LastImpulse.ToCompactString());
    Data->SetStringField(TEXT("delta_v_cm_s"), LastDeltaV.ToCompactString());
    Data->SetNumberField(TEXT("max_body_speed_cm_s"), LastMaxLinearSpeed); Data->SetNumberField(TEXT("max_body_angular_rad_s"), LastMaxAngularSpeed);
    Data->SetStringField(TEXT("last_recovery_reason"), LastRecoveryReason);
    Data->SetStringField(TEXT("last_emergency_source"), LastEmergencySource);
    Data->SetStringField(TEXT("last_landing_pelvis_cm"), LastLanding.ToCompactString());
    Data->SetStringField(TEXT("last_stand_center_cm"), RecoveryStand.GetLocation().ToCompactString());
    Data->SetStringField(TEXT("recovery_animation"), bUseAuthoredGetUp
        ? TEXT("direction_selected_authored_root_motion_getup")
        : TEXT("0.65s_physics_to_existing_ABP_blend_placeholder"));
    Data->SetNumberField(TEXT("damage_threshold_cm_s"), DamageThresholdCmS);
    Data->SetNumberField(TEXT("knockdown_threshold_cm_s"), KnockdownThresholdCmS);
    Data->SetNumberField(TEXT("horizontal_delta_v_limit_cm_s"), MaximumDeltaVCmS);
    Data->SetNumberField(TEXT("minimum_down_seconds"), MinimumDownSeconds);
    Data->SetNumberField(TEXT("stable_seconds"), StableSeconds);
    Data->SetNumberField(TEXT("recovery_blend_seconds"), RecoveryBlendSeconds);
    Data->SetNumberField(TEXT("emergency_recovery_seconds"), EmergencyRecoverySeconds);
    if (AHCM3NPC* OwnerNPC = NPC.Get())
    {
        const auto Mesh=OwnerNPC->GetMesh();const FBodyInstance* Root=Mesh->GetBodyInstance();
        const FBodyInstance* Pelvis=Mesh->GetBodyInstance(OwnerNPC->GetPhysicalRootBone());
        Data->SetStringField(TEXT("physical_root_bone"), OwnerNPC->GetPhysicalRootBone().ToString());
        Data->SetNumberField(TEXT("physics_transform_update_mode"),int32(Mesh->PhysicsTransformUpdateMode));
        Data->SetStringField(TEXT("mesh_attach_parent"),GetPathNameSafe(Mesh->GetAttachParent()));
        Data->SetBoolField(TEXT("last_recovery_attach_succeeded"),bLastRecoveryAttachSucceeded);
        Data->SetStringField(TEXT("recovery_expected_mesh_world"),RecoveryMeshWorld.ToString());
        Data->SetNumberField(TEXT("recovery_reference_body_drift_cm"),FVector::Dist(RecoveryBodyBeforeReference,RecoveryBodyAfterReference));
        Data->SetStringField(TEXT("recovery_body_before_reference_cm"),RecoveryBodyBeforeReference.ToCompactString());
        Data->SetStringField(TEXT("recovery_body_after_reference_cm"),RecoveryBodyAfterReference.ToCompactString());
        Data->SetBoolField(TEXT("pelvis_body_simulating"),Pelvis&&Pelvis->IsInstanceSimulatingPhysics());
        Data->SetStringField(TEXT("visible_pelvis_socket_cm"),Mesh->GetSocketLocation(OwnerNPC->GetPhysicalRootBone()).ToCompactString());
        if(Pelvis)Data->SetStringField(TEXT("actual_pelvis_body_cm"),Pelvis->GetUnrealWorldTransform().GetLocation().ToCompactString());
        Data->SetStringField(TEXT("mesh_world_transform"),Mesh->GetComponentTransform().ToString());
        Data->SetStringField(TEXT("mesh_relative_transform"),Mesh->GetRelativeTransform().ToString());
        Data->SetBoolField(TEXT("root_body_awake"),Root&&Root->IsInstanceAwake());
        Data->SetBoolField(TEXT("pelvis_body_awake"),Pelvis&&Pelvis->IsInstanceAwake());
        if(Pelvis)Data->SetNumberField(TEXT("pelvis_physics_blend_weight"),Pelvis->PhysicsBlendWeight);
    }
    int32 LivingSimulators = 0, AwakeLivingBodies = 0, SceneNPCs = 0;
    if (GetWorld())
        for (TActorIterator<AHCM3NPC> It(GetWorld()); It; ++It)
        {
            ++SceneNPCs;
            if (It->IsDead() || !It->GetPhysicalReaction() || !It->GetPhysicalReaction()->IsLivingRagdollActive()) continue;
            ++LivingSimulators;
            for (const FBodyInstance* Body : It->GetMesh()->Bodies) if (Body && Body->IsInstanceAwake()) ++AwakeLivingBodies;
        }
    Data->SetNumberField(TEXT("scene_npc_population_bound"), SceneNPCs);
    Data->SetNumberField(TEXT("living_simulators"), LivingSimulators);
    Data->SetNumberField(TEXT("awake_living_bodies"), AwakeLivingBodies);
    Data->SetNumberField(TEXT("recovery_check_interval_seconds"), .4);
    Data->SetBoolField(TEXT("living_ragdoll_uses_corpse_ttl_or_cap"), false);
    FString Output; const auto Writer = TJsonWriterFactory<>::Create(&Output); FJsonSerializer::Serialize(Data, Writer); return Output;
}
