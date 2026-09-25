#include "HCM4R1ReactionComponent.h"
#include "HCM4CombatComponent.h"
#include "HCM4Facing.h"
#include "M3/HCM3NPC.h"
#include "M3/HCM3AIController.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UHCM4R1ReactionComponent::UHCM4R1ReactionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    CounterAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01")));
}

void UHCM4R1ReactionComponent::BeginPlay()
{
    Super::BeginPlay();
    NPC = Cast<AHCM3NPC>(GetOwner());
    if (NPC) AddTickPrerequisiteComponent(NPC->GetMesh());
    SetComponentTickEnabled(false);
}

bool UHCM4R1ReactionComponent::CanCounterMelee() const
{
    if (ReactionPreference != EHCM4R1ReactionPreference::SceneDefault)
        return ReactionPreference == EHCM4R1ReactionPreference::DefendAgainstMelee;
    // Two authored ordinary pedestrians, independent of clothing, gender or named role.
    // This stable assignment also applies to existing level instances without a CDO-only change.
    return NPC && (NPC->StableId == TEXT("M3_Walker02") || NPC->StableId == TEXT("M3_Walker05"));
}

void UHCM4R1ReactionComponent::Record(FName Kind, float Value, const FVector& Start, const FVector& End, float PhaseStart, float PhaseEnd)
{
    if (Events.Num() >= 192) Events.RemoveAt(0);
    auto& E = Events.AddDefaulted_GetRef();
    E.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0; E.Kind = Kind; E.Value = Value;
    E.Start = Start; E.End = End; E.PhaseStart = PhaseStart; E.PhaseEnd = PhaseEnd;
}

AHCM1Character* UHCM4R1ReactionComponent::GetValidTarget() const
{
    AHCM1Character* Target = CounterTarget.Get();
    const AHCM1PlayerController* PC = Target ? Cast<AHCM1PlayerController>(Target->GetController()) : nullptr;
    const UHCM4CombatComponent* Combat = Target ? Target->GetCombatComponent() : nullptr;
    return Target && !Target->IsHidden() && PC && PC->GetPawn() == Target
        && PC->GetPlayerMode() == EHCPlayerMode::OnFoot && Combat && Combat->GetPlayerHealth() > 0
        && !PC->IsPauseMenuOpen() ? Target : nullptr;
}

bool UHCM4R1ReactionComponent::HasSightTo(const AActor* Target) const
{
    if (!NPC || !Target || !GetWorld()) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(M4R1ThreatSight), false, NPC);
    Q.AddIgnoredActor(Target);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, NPC->GetActorLocation() + FVector(0,0,45),
        Target->GetActorLocation() + FVector(0,0,35), ECC_Visibility, Q);
}

bool UHCM4R1ReactionComponent::ReceiveDamageThreat(AActor* Source, bool bMelee, const FVector& SourcePoint)
{
    if (!NPC || !NPC->IsCombatEnabled() || NPC->GetIsPassenger() || NPC->IsPhysicalReactionActive()) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    StopPunch(true);
    Threat = Source; ThreatOrigin = SourcePoint; ThreatUntil = Now + 12.; NextThink = Now;
    ConsecutiveCounterPathFailures = 0;
    CounterTarget = Cast<AHCM1Character>(Source);
    const AHCM1Character* Target = GetValidTarget();
    const bool bGunThreat = Target && Target->GetCombatComponent()->GetWeaponMode() == EHCM4WeaponMode::Pistol;
    bCountering = bMelee && !bGunThreat && CanCounterMelee() && NPC->GetHealth() > LowHealthFleeThreshold && Target;
    if (bCountering)
    {
        NPC->PanicUntil = 0; NPC->StopNavigation(); NPC->Behaviour = EHCM3NPCBehaviour::CounterApproach;
        NextCounter = FMath::Max(NextCounter, Now + .38); // Allows the original .35 s hurt response to finish.
        NPC->GetCharacterMovement()->MaxWalkSpeed = 240.f;
        Record(TEXT("CounterSelected"), NPC->GetHealth(), SourcePoint);
    }
    else Record(TEXT("FleeSelected"), NPC->GetHealth(), SourcePoint);
    SetComponentTickEnabled(true);
    return bCountering;
}

void UHCM4R1ReactionComponent::StopPunch(bool bInterrupted)
{
    if (CounterMontage && NPC)
    {
        if (UAnimInstance* Anim = NPC->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.12f, CounterMontage);
        if (bInterrupted) { ++CounterInterruptions; Record(TEXT("CounterInterrupted")); }
        else if (!bAppliedThisPunch) { ++CounterMisses; Record(TEXT("CounterMiss")); }
        NPC->GetMesh()->bEnableUpdateRateOptimizations = bSavedUpdateRate;
        NPC->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption(SavedVisibilityTickOption);
        NPC->GetCharacterMovement()->bOrientRotationToMovement = true;
        NextCounter = FMath::Max(NextCounter, double(GetWorld()->GetTimeSeconds()) + FMath::Clamp(CounterCooldownSeconds, .6f, 4.f));
    }
    CounterMontage = nullptr; bHasHandSample = false; PreviousPhase = -1;
}

void UHCM4R1ReactionComponent::CancelCounter(const TCHAR* Reason)
{
    StopPunch(true);
    if (bCountering) { Record(FName(Reason)); if (NPC) NPC->StopNavigation(); }
    bCountering = false; CounterTarget.Reset();
}

void UHCM4R1ReactionComponent::ResetReaction()
{
    CancelCounter(TEXT("LifeOrRestoreReset")); Threat.Reset(); ThreatUntil = 0;
    NextThink = 0; NextCounter = 0; SetComponentTickEnabled(false);
}

bool UHCM4R1ReactionComponent::StartCounter(AHCM1Character* Target)
{
    UAnimInstance* Anim = NPC->GetMesh()->GetAnimInstance();
    UAnimSequence* Sequence = CounterAnimation.LoadSynchronous();
    if (!Anim || !Sequence || !Target || !HasSightTo(Target)) return false;
    const FVector Separation=NPC->GetActorLocation()-Target->GetActorLocation();
    if(Separation.Size2D()<92.f){FTransform Safe;const FVector Desired=Target->GetActorLocation()+Separation.GetSafeNormal2D()*98.f;
        if(!NPC->ResolveStandTransform(FTransform(NPC->GetActorRotation(),Desired),Safe))return false;
        FHitResult Stop;NPC->GetCharacterMovement()->SafeMoveUpdatedComponent(Safe.GetLocation()-NPC->GetActorLocation(),NPC->GetActorQuat(),true,Stop);
        if(FVector::Dist2D(NPC->GetActorLocation(),Target->GetActorLocation())<90)return false;
    }
    NPC->StopNavigation(); NPC->GetCharacterMovement()->bOrientRotationToMovement = false;
    bSavedUpdateRate = NPC->GetMesh()->bEnableUpdateRateOptimizations;
    SavedVisibilityTickOption = uint8(NPC->GetMesh()->VisibilityBasedAnimTickOption);
    NPC->GetMesh()->bEnableUpdateRateOptimizations = false;
    NPC->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    CounterMontage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("FullBody"), .09f, .12f,
        Sequence->GetPlayLength() / .65f, 1);
    if (!CounterMontage)
    {
        NPC->GetMesh()->bEnableUpdateRateOptimizations = bSavedUpdateRate;
        NPC->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption(SavedVisibilityTickOption);
        NPC->GetCharacterMovement()->bOrientRotationToMovement = true;
        Record(TEXT("CounterAnimationUnavailable")); return false;
    }
    PunchStarted = GetWorld()->GetTimeSeconds(); bAppliedThisPunch = false; bHasHandSample = false; PreviousPhase = -1;
    NPC->Behaviour = EHCM3NPCBehaviour::CounterAttack; ++CounterStarts;
    Record(TEXT("CounterStarted"), .65f, NPC->GetActorLocation(), Target->GetActorLocation());
    return true;
}

void UHCM4R1ReactionComponent::SamplePunch()
{
    AHCM1Character* Target = GetValidTarget();
    UAnimInstance* Anim = NPC->GetMesh()->GetAnimInstance();
    if (!Target || !Anim || !CounterMontage) { StopPunch(true); return; }
    if (!Anim->Montage_IsPlaying(CounterMontage))
    {
        // UE removes the active montage instance when its .12 s blend-out begins.
        // The first runtime trace showed this at .533 s of the .65 s punch, after
        // its hit window. Keep the authored recovery time without calling this an interruption.
        const double Elapsed = GetWorld()->GetTimeSeconds() - PunchStarted;
        if (Elapsed >= .65) StopPunch(false);
        else if (Elapsed < .50) StopPunch(true);
        return;
    }
    const float Length = CounterMontage->GetPlayLength();
    if (Length <= SMALL_NUMBER) { StopPunch(true); return; }
    const float Phase = Anim->Montage_GetPosition(CounterMontage) * .65f / Length;
    const FVector Hand = NPC->GetMesh()->GetSocketLocation(NPC->GetCounterHandBone()) + NPC->GetActorForwardVector() * 12.f;
    if (!FMath::IsFinite(Phase) || Hand.ContainsNaN()) { StopPunch(true); return; }
    const float FromPhase = bHasHandSample && Phase > PreviousPhase ? PreviousPhase : Phase;
    const FVector FromHand = bHasHandSample && Phase > PreviousPhase ? PreviousHand : Hand;
    PreviousHand = Hand; PreviousPhase = Phase; bHasHandSample = true;
    // Same real source punch used by the player: source .4-.5333333 mapped to .65 duration.
    constexpr float Open = .26f, Close = .3466667f;
    const float A = FMath::Max(FromPhase, Open), B = FMath::Min(Phase, Close);
    if (!bAppliedThisPunch && A <= B && Phase >= Open && FromPhase <= Close)
    {
        const float Span = Phase - FromPhase;
        const FVector Start = Span > SMALL_NUMBER ? FMath::Lerp(FromHand, Hand, (A - FromPhase) / Span) : Hand;
        const FVector End = Span > SMALL_NUMBER ? FMath::Lerp(FromHand, Hand, (B - FromPhase) / Span) : Hand;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(M4R1CounterHand), false, NPC);
        TArray<FHitResult> Hits;
        GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(18.f), Q);
        ++SweepCount; Record(TEXT("CounterSweep"), Hits.Num(), Start, End, A, B);
        for (const FHitResult& Hit : Hits)
        {
            if (Hit.GetActor() != Target || FVector::Dist2D(NPC->GetActorLocation(), Target->GetActorLocation()) > 145.f) continue;
            FCollisionQueryParams Sight(SCENE_QUERY_STAT(M4R1CounterOcclusion), false, NPC);
            Sight.AddIgnoredActor(Target);
            FHitResult Block;
            if (GetWorld()->LineTraceSingleByChannel(Block, NPC->GetActorLocation() + FVector(0,0,35), Hit.ImpactPoint, ECC_Visibility, Sight))
            { ++OccludedCount; Record(TEXT("CounterOccluded"), 0, NPC->GetActorLocation(), Hit.ImpactPoint, A, B); continue; }
            bAppliedThisPunch = true;
            const float Applied = UGameplayStatics::ApplyPointDamage(Target, FMath::Clamp(CounterDamage, 1.f, 30.f),
                (Hit.ImpactPoint - NPC->GetActorLocation()).GetSafeNormal(), Hit, NPC->GetController(), NPC, UHCM4R1CounterDamageType::StaticClass());
            if (Applied > 0) ++CounterHits;
            // Cancel forward follow-through at contact, retaining the recovery cooldown.
            Anim->Montage_Stop(.07f,CounterMontage);
            Record(TEXT("CounterDamage"), Applied, Start, Hit.ImpactPoint, A, B);
            break;
        }
    }
    if (GetWorld()->GetTimeSeconds() - PunchStarted >= .65 || Phase >= .645f) StopPunch(false);
}

void UHCM4R1ReactionComponent::UpdateThreat(double Now)
{
    AActor* Source = Threat.Get();
    if (!Source || Source->IsHidden()) return;
    const float Distance = FVector::Dist2D(NPC->GetActorLocation(), Source->GetActorLocation());
    const AHCM1Character* Character = Cast<AHCM1Character>(Source);
    const UHCM4CombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
    // A known recent attacker still at touching distance is a credible threat. Merely walking
    // near an uninvolved NPC never establishes Threat. Fire/aim/another punch renews from farther away.
    const bool bActivelyThreatening = Combat && (Combat->IsAttacking() || Combat->IsAimHeld());
    if (Distance < (bActivelyThreatening ? 650.f : 250.f) && HasSightTo(Source))
    {
        ThreatUntil = Now + 12.; NPC->DangerPoint = Source->GetActorLocation(); ++ThreatRefreshes;
        if (NPC->PanicUntil > 0) NPC->PanicUntil = FMath::Max(NPC->PanicUntil, ThreatUntil);
        if (bCountering && Combat && Combat->GetWeaponMode() == EHCM4WeaponMode::Pistol)
            NPC->BeginPanic(Source->GetActorLocation(), 12.f);
    }
}

void UHCM4R1ReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    if (!NPC || !NPC->IsCombatEnabled() || NPC->GetIsPassenger() || NPC->IsPhysicalReactionActive()) { ResetReaction(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextThink) { NextThink = Now + .25; UpdateThreat(Now); }
    if (!bCountering)
    {
        if (NPC->PanicUntil <= 0 && Now > ThreatUntil) { Threat.Reset(); SetComponentTickEnabled(false); }
        return;
    }
    AHCM1Character* Target = GetValidTarget();
    if (!Target || Now > ThreatUntil || FVector::Dist2D(NPC->GetActorLocation(), Target->GetActorLocation()) > CounterPursuitRange
        || FVector::Dist2D(ThreatOrigin, Target->GetActorLocation()) > CounterPursuitRange)
    {
        const FVector From = Target ? Target->GetActorLocation() : NPC->DangerPoint;
        CancelCounter(TEXT("TargetLeftOrInvalid")); NPC->BeginPanic(From, 3.f); return;
    }
    if (NPC->HitStunUntil > Now) { if (CounterMontage) StopPunch(true); return; }
    if (CounterMontage) { SamplePunch(); return; }
    const FVector Here = NPC->GetActorLocation(), There = Target->GetActorLocation();
    const float Distance = FVector::Dist2D(Here, There);
    if (Distance <= 112.f && FMath::Abs(Here.Z - There.Z) < 65.f && HasSightTo(Target))
    {
        NPC->StopNavigation(); NPC->GetCharacterMovement()->bOrientRotationToMovement = false;
        HCM4Facing::TurnBodyToward(NPC, There, DeltaTime, 240.f);
        NPC->Behaviour = EHCM3NPCBehaviour::CounterApproach;
        const float Error = FMath::Abs(FMath::FindDeltaAngleDegrees(NPC->GetActorRotation().Yaw, HCM4Facing::YawToTarget(Here, There, NPC->GetActorRotation().Yaw)));
        if (Now >= NextCounter && Error < 12.f && !StartCounter(Target)) NextCounter = Now + 1.;
        return;
    }
    // Navigation may go round a barrier, but never uses a partial path or warps the NPC.
    if (Now >= NPC->NextDecisionTime)
    {
        NPC->NextDecisionTime = Now + .75;
        const FVector Stand = There + (Here - There).GetSafeNormal2D() * 95.f;
        FTransform Safe;
        ++CounterPathRequests;
        if (NPC->ResolveStandTransform(FTransform(NPC->GetActorRotation(), Stand), Safe) &&
            // Goal is 95 cm from the player: stop within 10 cm, inside the 112 cm strike threshold.
            NPC->RequestWalk(Safe.GetLocation() - FVector(0,0,NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), true, 10.f))
        { NPC->Behaviour = EHCM3NPCBehaviour::CounterApproach; ConsecutiveCounterPathFailures = 0; }
        else { ++CounterPathFailures; ++ConsecutiveCounterPathFailures; NPC->StopNavigation(); }
        if (ConsecutiveCounterPathFailures >= 4)
            NPC->BeginPanic(There, 12.f);
    }
}

FString UHCM4R1ReactionComponent::GetDiagnostics() const
{
    auto D = MakeShared<FJsonObject>();
    D->SetBoolField(TEXT("counter_preference"), CanCounterMelee()); D->SetBoolField(TEXT("counter_active"), bCountering);
    D->SetBoolField(TEXT("counter_montage_active"), CounterMontage != nullptr);
    D->SetStringField(TEXT("counter_animation"), CounterAnimation.ToSoftObjectPath().ToString());
    D->SetNumberField(TEXT("counter_starts"), CounterStarts); D->SetNumberField(TEXT("counter_hits"), CounterHits);
    D->SetNumberField(TEXT("counter_misses"), CounterMisses); D->SetNumberField(TEXT("counter_interruptions"), CounterInterruptions);
    D->SetNumberField(TEXT("counter_sweeps"), SweepCount); D->SetNumberField(TEXT("counter_occluded"), OccludedCount);
    D->SetNumberField(TEXT("counter_path_requests"), CounterPathRequests); D->SetNumberField(TEXT("counter_path_failures"), CounterPathFailures);
    D->SetNumberField(TEXT("threat_refreshes"), ThreatRefreshes); D->SetNumberField(TEXT("counter_damage"), CounterDamage);
    D->SetNumberField(TEXT("counter_cooldown_seconds"), CounterCooldownSeconds); D->SetNumberField(TEXT("low_health_flee_threshold"), LowHealthFleeThreshold);
    D->SetStringField(TEXT("threat_actor"), GetNameSafe(Threat.Get()));
    D->SetNumberField(TEXT("threat_remaining_seconds"), GetWorld() ? FMath::Max(0., ThreatUntil - GetWorld()->GetTimeSeconds()) : 0.);
    FString Out; FJsonSerializer::Serialize(D, TJsonWriterFactory<>::Create(&Out)); return Out;
}
