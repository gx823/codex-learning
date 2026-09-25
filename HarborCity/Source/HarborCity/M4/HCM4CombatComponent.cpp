#include "HCM4CombatComponent.h"
#include "M5VS3/HCM5VS3Abilities.h"
#include "EngineUtils.h"

#include "HCM4Weapon.h"
#include "HCM4DamageTypes.h"
#include "HCM4Facing.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1SaveGame.h"
#include "M3/HCM3Experience.h"
#include "M3/HCM3NPC.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UHCM4CombatComponent::UHCM4CombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    PlayerAnimationBlueprint = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/HarborCity/M4R2/Animation/ABP_M4R2_Player.ABP_M4R2_Player_C")));
    // Third strike deliberately reuses the verified right-hand punch at a heavier cadence.
    // Template attack03 is not accepted as a third punch. Window times derive from source .4-.533333s.
    for (const int32 Index : {1, 2, 1})
        PunchAnimations.Add(TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(
            TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_%02d.MM_Attack_%02d"), Index, Index))));
    // The former held frame was sampled mid-equip: its tucked forearms hid the
    // pistol. Reuse the installed, already validated two-hand idle for ready.
    // Hip spread, ADS state/projection, and first-person raise offset stay separate.
    PistolIdleAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS")));
    PistolAimAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS")));
    PistolFireAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire.MM_Pistol_Fire")));
    PistolReloadAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload")));
    PlayerHitFrontMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/HarborCity/M4/Animation/AM_M4_HitFront.AM_M4_HitFront")));
}

void UHCM4CombatComponent::BeginPlay()
{
    Super::BeginPlay();
    // Authoritative R2 projection contract; stale serialized Blueprint component defaults
    // cannot silently retain the old .6/.25 input-only ADS values.
    ADSMagnification = 1.6f;
    AimTransitionSeconds = .2f;
    AimLookScale = 1.f / ADSMagnification;
    if(PistolIdleAnimation.ToSoftObjectPath()==FSoftObjectPath(TEXT("/Game/HarborCity/M4/Animation/MM_M4_PistolHipIdle.MM_M4_PistolHipIdle")))
        PistolIdleAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS")));
    Character = Cast<AHCM1Character>(GetOwner());
    MagazineAmmo = FMath::Max(1, MagazineCapacity);
    ReserveAmmo = FMath::Max(0, InitialReserve);
    SpreadRandom.Initialize(int32(FPlatformTime::Cycles()));
    if (Character) AddTickPrerequisiteComponent(Character->GetMesh());
}

void UHCM4CombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(WeaponActor)) WeaponActor->Destroy();
    Super::EndPlay(Reason);
}

AHCM1PlayerController* UHCM4CombatComponent::GetPC() const
{ return Character ? Cast<AHCM1PlayerController>(Character->GetController()) : nullptr; }
double UHCM4CombatComponent::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0; }
bool UHCM4CombatComponent::IsCombatEnabled() const
{ const AHCM1PlayerController* PC = GetPC(); return PC && PC->GetM3Experience(); }
bool UHCM4CombatComponent::CanUseCombat() const
{
    const AHCM1PlayerController* PC = GetPC();
    return IsCombatEnabled() && PC->GetPlayerMode() == EHCPlayerMode::OnFoot && !bSeated
        && !PC->IsPauseMenuOpen() && !PC->IsDialogueOpen() && !PC->IsFlying() && PC->IsGameplayFocused() && PlayerHealth > 0;
}

void UHCM4CombatComponent::EnsureWeapon()
{
    if (!IsCombatEnabled() || !Character) return;
    if (!bAnimationInitialized)
    {
        bAnimationInitialized = true;
        LoadedPlayerAnimationClass = PlayerAnimationBlueprint.LoadSynchronous();
        if (!LoadedPlayerAnimationClass)
        {
            Record(TEXT("R2PlayerAnimationBlueprintUnavailableFallback"));
            LoadedPlayerAnimationClass = LoadClass<UAnimInstance>(nullptr,TEXT("/Game/HarborCity/M4/Animation/ABP_M4_Player.ABP_M4_Player_C"));
        }
    }
    UAnimInstance* CurrentAnim = Character->GetMesh()->GetAnimInstance();
    // Experience applies its original appearance once after BeginPlay. Detect the real instance,
    // not just a prior request to set it; the asset is loaded only once and the live class is compared.
    if (LoadedPlayerAnimationClass && (!CurrentAnim || CurrentAnim->GetClass() != LoadedPlayerAnimationClass))
    {
        CancelTransient(TEXT("AnimationInstanceChanged"));
        Character->GetMesh()->SetAnimInstanceClass(LoadedPlayerAnimationClass);
        // Attack clips do not take over movement; the existing CharacterMovement still handles inputs.
        if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance()) Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        Record(TEXT("PlayerAnimationInstanceApplied"));
    }
    if (IsValid(WeaponActor)) return;
    FActorSpawnParameters Params;
    Params.Owner = Character; Params.Instigator = Character;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    WeaponActor = GetWorld()->SpawnActor<AHCM4Weapon>(AHCM4Weapon::StaticClass(), Character->GetActorTransform(), Params);
    if (WeaponActor)
    {
        const bool bOfficialGrip=WeaponActor->UsesArticulatedMesh() && Character->GetMesh()->DoesSocketExist(TEXT("HandGrip_R"));
        WeaponActor->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale,bOfficialGrip?TEXT("HandGrip_R"):TEXT("hand_r"));
        // Measured ADS hand_r pose + native MakeRelativeTransform, assets_author.json.
        WeaponActor->SetActorRelativeTransform(bOfficialGrip?FTransform::Identity:FTransform(FRotator(-8.21222212,176.32626190,1.13691458),
            FVector(-4.63102409,1.82995991,-2.16906953),FVector::OneVector));
        // A uniformly scaled character still holds the same physical pistol.
        // Keep the socket's location/rotation but do not inherit body scale.
        WeaponActor->SetActorScale3D(FVector::OneVector);
        WeaponActor->SetEquipped(WeaponMode == EHCM4WeaponMode::Pistol && !bSeated);
    }
}

void UHCM4CombatComponent::Record(FName Kind, float Value, const FHitResult* Hit,
    const FVector& Start, const FVector& End, bool bBlocked)
{
    if (Events.Num() >= 256) Events.RemoveAt(0);
    FHCM4CombatEvent& Event = Events.AddDefaulted_GetRef();
    Event.Time = Now(); Event.Kind = Kind; Event.Combo = ComboIndex;
    Event.Value = Value; Event.Start = Start; Event.End = End; Event.bBlocked = bBlocked;
    if (Hit) { Event.Point = Hit->ImpactPoint; Event.Actor = Hit->GetActor() ? Hit->GetActor()->GetFName() : NAME_None; Event.Bone = Hit->BoneName; }
}

float UHCM4CombatComponent::GetAttackElapsed() const { return bAttacking ? float(Now() - AttackStartTime) : 0; }
float UHCM4CombatComponent::GetAttackAnimationElapsed() const
{
    const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!bAttacking || !Anim || !ActiveMontage || !Anim->Montage_IsPlaying(ActiveMontage)
        || !AttackDurations.IsValidIndex(ComboIndex)) return -1.f;
    const float Length = ActiveMontage->GetPlayLength(), Duration = AttackDurations[ComboIndex];
    if (!FMath::IsFinite(Length) || Length <= SMALL_NUMBER || !FMath::IsFinite(Duration) || Duration <= 0) return -1.f;
    // Montage position is already advanced by its real play rate. Do not multiply DeltaTime
    // or rate again. This maps the original sequence timeline to the unchanged .60/.62/.78 scale.
    const float Progress = Anim->Montage_GetPosition(ActiveMontage) * Duration / Length;
    return FMath::IsFinite(Progress) ? Progress : -1.f;
}
bool UHCM4CombatComponent::IsHitWindowOpen() const
{
    return bAttacking && HitWindowStarts.IsValidIndex(ComboIndex) && HitWindowEnds.IsValidIndex(ComboIndex)
        && GetAttackAnimationElapsed() >= HitWindowStarts[ComboIndex] && GetAttackAnimationElapsed() <= HitWindowEnds[ComboIndex];
}
float UHCM4CombatComponent::GetAimLookMultiplier() const
{ return 1.f / GetCurrentADSMagnification(); }
float UHCM4CombatComponent::GetCurrentADSMagnification() const
{ return IsCombatEnabled() && !bSeated ? FMath::Lerp(1.f, ADSMagnification, FMath::Clamp(AimBlend, 0.f, 1.f)) : 1.f; }
float UHCM4CombatComponent::GetReloadProgress() const
{ return bReloading ? FMath::Clamp(1.f-float(ReloadEnd-Now())/FMath::Max(.01f,ReloadSeconds),0.f,1.f) : 0.f; }
float UHCM4CombatComponent::GetShotPresentationPulse() const
{
    if (LastAcceptedShot.ShotId <= 0 || WeaponMode != EHCM4WeaponMode::Pistol || bSeated) return 0.f;
    const float T = float(Now()-LastAcceptedShot.Time);
    // 20 ms impulse, 180 ms recovery. No accumulated aim/control-rotation drift.
    return T < 0 || T >= .20f ? 0.f : T < .02f ? T/.02f : FMath::Square(1.f-(T-.02f)/.18f);
}
float UHCM4CombatComponent::GetCurrentSpreadDegrees() const
{ return FMath::Lerp(HipSpreadDegrees, AimSpreadDegrees, AimBlend); }
float UHCM4CombatComponent::GetAnimationSlotWeight() const
{
    const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    const FName Slot = GetAnimationSlot();
    return Anim && !Slot.IsNone() ? Anim->GetSlotMontageGlobalWeight(Slot) : 0.f;
}
FName UHCM4CombatComponent::GetAnimationSlot() const
{
    const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (Anim && ActiveMontage && Anim->Montage_IsPlaying(ActiveMontage)) return LastAnimationSlot;
    return Anim && PistolBaseMontage && Anim->Montage_IsPlaying(PistolBaseMontage) ? FName(TEXT("UpperBody")) : NAME_None;
}
float UHCM4CombatComponent::GetAnimationPosition() const
{
    const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Anim) return 0.f;
    if (ActiveMontage && Anim->Montage_IsPlaying(ActiveMontage)) return Anim->Montage_GetPosition(ActiveMontage);
    return PistolBaseMontage && Anim->Montage_IsPlaying(PistolBaseMontage) ? Anim->Montage_GetPosition(PistolBaseMontage) : 0.f;
}
bool UHCM4CombatComponent::HasAnimationPlayback() const
{
    const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    return Anim && GetAnimationSlotWeight() > .01f;
}

void UHCM4CombatComponent::PlayAnimation(UAnimSequence* Animation, float Duration, bool bLoop)
{
    bAnimationPlayback = false;
    if (!Animation || !Character || !Character->GetMesh()->GetAnimInstance())
    { Record(TEXT("AnimationUnavailable")); return; }
    const float Rate = Duration > 0 && !bLoop ? Animation->GetPlayLength() / Duration : 1.f;
    UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
    // These slots are provided by the authored M4 player AnimBP; old locomotion assets remain untouched.
    const bool bFireAdditive = Animation == PistolFireAnimation.Get();
    const FName Slot = WeaponMode == EHCM4WeaponMode::Unarmed ? TEXT("FullBody")
        : bFireAdditive ? TEXT("AdditiveHitReact") : TEXT("UpperBody");
    LastAnimationSlot = Slot;
    ActiveMontage = Anim->PlaySlotAnimationAsDynamicMontage(Animation, Slot,
        bLoop ? AimTransitionSeconds : bFireAdditive ? .012f : .09f,
        bFireAdditive ? .065f : .12f, FMath::Max(.01f, Rate), bLoop ? 10000 : 1);
    if (WeaponMode == EHCM4WeaponMode::Pistol && !bFireAdditive) PistolBaseMontage = ActiveMontage;
    bAnimationPlayback = ActiveMontage != nullptr;
    Record(TEXT("AnimationStarted"), bAnimationPlayback ? 1.f : 0.f);
}

bool UHCM4CombatComponent::RequestAttack()
{
    if (!CanUseCombat() || bReloading) return false;
    if(Character->GetAbilities()->IsEnabled()) {
        if(WeaponMode==EHCM4WeaponMode::Sword)return Character->GetAbilities()->PressSword();
        if(Character->GetAbilities()->IsBusy())return false;
    }
    EnsureWeapon();
    if (WeaponMode == EHCM4WeaponMode::Pistol) return FirePistol();
    if (bAttacking)
    {
        const float Elapsed = GetAttackElapsed();
        if (ComboIndex < 2 && ComboWindowStarts.IsValidIndex(ComboIndex) && AttackDurations.IsValidIndex(ComboIndex)
            && Elapsed >= ComboWindowStarts[ComboIndex] && Elapsed <= AttackDurations[ComboIndex])
        { bComboQueued = true; Record(TEXT("ComboQueued"), Elapsed); return true; }
        Record(TEXT("ComboInputOutsideWindow"), Elapsed); return false;
    }
    const int32 Next = Now() - LastAttackEnd <= ComboResetSeconds && ComboIndex >= 0 && ComboIndex < 2 ? ComboIndex + 1 : 0;
    return StartPunch(Next);
}

bool UHCM4CombatComponent::StartPunch(int32 Index)
{
    if (!AttackDurations.IsValidIndex(Index) || !HitWindowStarts.IsValidIndex(Index) || !HitWindowEnds.IsValidIndex(Index)
        || !MeleeDamage.IsValidIndex(Index) || !ComboWindowStarts.IsValidIndex(Index)) return false;
    ResetPunchPoseSample();
    if(Character->GetAbilities()->IsEnabled()) {
        AHCM3NPC* Nearest=nullptr;float Closest=92;
        for(TActorIterator<AHCM3NPC> It(GetWorld());It;++It){const FVector D=It->GetActorLocation()-Character->GetActorLocation();const float Dist=D.Size2D();if(!It->IsDead()&&Dist<Closest&&(D.GetSafeNormal2D()|GetPC()->GetControlRotation().Vector())>.45f){Nearest=*It;Closest=Dist;}}
        if(Nearest){const FVector Back=(Character->GetActorLocation()-Nearest->GetActorLocation()).GetSafeNormal2D();FHitResult Stop;Character->GetCharacterMovement()->SafeMoveUpdatedComponent(Back*(94-Closest),Character->GetActorQuat(),true,Stop);}
        Character->GetCharacterMovement()->StopMovementImmediately();
    }
    ComboIndex = Index; bAttacking = true; bComboQueued = false;
    AttackStartTime = Now(); SwingHits.Reset();
    Character->SetCombatMovementScale(AttackMovementScale);
    Character->FaceBodyYawOnce(GetPC()->GetControlRotation().Yaw, .16f);
    PlayAnimation(PunchAnimations.IsValidIndex(Index) ? PunchAnimations[Index].LoadSynchronous() : nullptr, AttackDurations[Index]);
    Record(TEXT("PunchStarted"), AttackDurations[Index]);
    return true;
}

void UHCM4CombatComponent::ResetPunchPoseSample()
{
    bHasPunchPoseSample = false; PreviousHand = FVector::ZeroVector; PreviousPunchPoseTime = -1;
    SampledPunchMontage.Reset(); SampledPunchPlayRate = 0; SampledPunchCombo = -1;
}

void UHCM4CombatComponent::SweepPunch()
{
    const float CurrentTime = GetAttackAnimationElapsed();
    UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!FMath::IsFinite(CurrentTime) || CurrentTime < 0 || !Anim || !ActiveMontage) { ResetPunchPoseSample(); return; }
    const float PlayRate = Anim->Montage_GetPlayRate(ActiveMontage);
    if (!FMath::IsFinite(PlayRate) || PlayRate <= 0) { ResetPunchPoseSample(); return; }
    const FName Hand = ComboIndex == 1 ? TEXT("hand_l") : TEXT("hand_r");
    const FVector HandNow = Character->GetMesh()->GetSocketLocation(Hand);
    const FVector Forward = Character->GetActorForwardVector();
    const FVector KnuckleNow = HandNow + Forward * 12.f;
    if (KnuckleNow.ContainsNaN()) { ResetPunchPoseSample(); return; }
    // Never connect poses across a new montage/combo, a seek backwards, a rate change,
    // cancellation or an unavailable animation. PostPhysics already depends on mesh evaluation.
    const bool bContinuous = bHasPunchPoseSample && SampledPunchMontage.Get() == ActiveMontage
        && SampledPunchCombo == ComboIndex && FMath::IsNearlyEqual(SampledPunchPlayRate, PlayRate)
        && CurrentTime > PreviousPunchPoseTime;
    const FVector RawStart = bContinuous ? PreviousHand : KnuckleNow;
    const float RawTimeStart = bContinuous ? PreviousPunchPoseTime : CurrentTime;
    PreviousHand = KnuckleNow; PreviousPunchPoseTime = CurrentTime; bHasPunchPoseSample = true;
    SampledPunchMontage = ActiveMontage; SampledPunchPlayRate = PlayRate; SampledPunchCombo = ComboIndex;
    const auto Stamp = [this, RawStart, KnuckleNow, RawTimeStart, CurrentTime](float ClippedStart = -1.f, float ClippedEnd = -1.f)
    {
        FHCM4CombatEvent& E = Events.Last();
        E.SampleStart = RawStart; E.SampleEnd = KnuckleNow;
        E.SampleTimeStart = RawTimeStart; E.SampleTimeEnd = CurrentTime;
        E.WindowTimeStart = ClippedStart; E.WindowTimeEnd = ClippedEnd; E.AttackWorldElapsed = GetAttackElapsed();
    };
    Record(TEXT("PunchHandSample"), CurrentTime, nullptr, RawStart, KnuckleNow); Stamp();
    if (!bContinuous) return;
    const float Open = HitWindowStarts[ComboIndex], Close = HitWindowEnds[ComboIndex];
    // A frame that misses the entire active pose interval has no trustworthy interior path.
    // Retain the previous conservative policy instead of synthesizing damage after a stall.
    if (RawTimeStart < Open && CurrentTime > Close)
    { Record(TEXT("HitWindowSkippedLongFrame"), CurrentTime - RawTimeStart); Stamp(); return; }
    const float ClipStart = FMath::Max(RawTimeStart, Open), ClipEnd = FMath::Min(CurrentTime, Close);
    if (ClipEnd <= ClipStart) return;
    const float Span = CurrentTime - RawTimeStart;
    // Only the portion inside the unchanged active window is swept. Wind-up/recovery portions
    // of the measured frame segment are discarded, including both crossing boundaries.
    const FVector Start = FMath::Lerp(RawStart, KnuckleNow, (ClipStart - RawTimeStart) / Span);
    const FVector End = FMath::Lerp(RawStart, KnuckleNow, (ClipEnd - RawTimeStart) / Span);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M4PunchSweep), false, Character);
    if (WeaponActor) Query.AddIgnoredActor(WeaponActor);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(MeleeSphereRadius), Query);
    Record(TEXT("PunchSweep"), ClipEnd, Hits.IsEmpty() ? nullptr : &Hits[0], Start, End, !Hits.IsEmpty()); Stamp(ClipStart, ClipEnd);
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!Target || SwingHits.Contains(Target)) continue;
        if (AHCM3NPC* NPC = Cast<AHCM3NPC>(Target))
        {
            if (!NPC->IsDead() && NPC->IsCombatEnabled())
            {
                // An animated hand can already be beyond a thin wall on the first active
                // sample. Keep the hand sweep as the hit authority, then reject a target
                // occluded from the real chest before applying damage or impact cosmetics.
                const USkeletalMeshComponent* Mesh = Character->GetMesh();
                const FName ChestBone = Mesh->GetBoneIndex(TEXT("spine_03")) == INDEX_NONE
                    && Mesh->GetBoneIndex(TEXT("Chest")) != INDEX_NONE ? FName(TEXT("Chest")) : FName(TEXT("spine_03"));
                const FVector Chest = Mesh->GetSocketLocation(ChestBone);
                FHitResult Occluder;
                const bool bOccluded = GetWorld()->LineTraceSingleByChannel(Occluder, Chest,
                    Hit.ImpactPoint, ECC_Visibility, Query) && Occluder.GetActor() != Target;
                if (bOccluded)
                { Record(TEXT("PunchOccluded"), ClipEnd, &Occluder, Chest, Hit.ImpactPoint, true); Stamp(ClipStart, ClipEnd); }
                else
                { SwingHits.Add(Target); ApplyHit(Hit, Forward, MeleeDamage[ComboIndex], false);
                  if(Character->GetAbilities()->IsEnabled())Anim->Montage_Stop(.08f,ActiveMontage); }
            }
        }
        // Multi channel sweep ends at the closest blocking geometry, including walls.
        if (Hit.bBlockingHit) break;
    }
}

bool UHCM4CombatComponent::ApplyHit(const FHitResult& Hit, const FVector& Direction, float Damage, bool bGun)
{
    AHCM3NPC* NPC = Cast<AHCM3NPC>(Hit.GetActor());
    const bool bLiveNPC = NPC && !NPC->IsDead() && NPC->IsCombatEnabled();
    if (NPC && !bLiveNPC) return false;
    LastHit = Hit;
    if (WeaponActor) WeaponActor->PlayImpact(Hit, bLiveNPC, bGun);
    if (!bLiveNPC) return false;
    const TSubclassOf<UDamageType> Type = bGun ? UHCM4PistolDamageType::StaticClass() : UHCM4MeleeDamageType::StaticClass();
    const float Applied = UGameplayStatics::ApplyPointDamage(NPC, Damage, Direction, Hit, GetPC(),
        bGun && WeaponActor ? static_cast<AActor*>(WeaponActor.Get()) : Character.Get(), Type);
    if (Applied > 0) ++DamageHitCount;
    Record(bGun ? TEXT("BulletDamage") : TEXT("PunchDamage"), Applied, &Hit);
    return Applied > 0;
}

bool UHCM4CombatComponent::FirePistol()
{
    if (Now() < NextShotTime || !WeaponActor) return false;
    NextShotTime = Now() + ShotInterval;
    if (MagazineAmmo <= 0)
    { ++EmptyTriggerCount; WeaponActor->PlayEmpty(); GetPC()->ShowStatusMessage(ReserveAmmo > 0 ? TEXT("弹匣已空，按 R 装弹。") : TEXT("弹药已耗尽。"), 1.5f); Record(TEXT("EmptyTrigger")); return false; }
    FVector Camera; FRotator View;
    GetPC()->GetPlayerViewPoint(Camera, View);
    const FVector Direction = SpreadRandom.VRandCone(View.Vector(), FMath::DegreesToRadians(GetCurrentSpreadDegrees()));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M4Shot), true, Character);
    Query.AddIgnoredActor(WeaponActor);
    FHitResult CameraHit;
    const FVector RayEnd = Camera + Direction * ShotRange;
    const bool bCameraHit = GetWorld()->LineTraceSingleByChannel(CameraHit, Camera, RayEnd, ECC_Visibility, Query);
    const FVector Target = bCameraHit ? CameraHit.ImpactPoint : RayEnd;
    const FVector Muzzle = WeaponActor->GetMuzzleLocation();
    FHitResult MuzzleHit;
    // Starting slightly behind the muzzle also prevents a tip embedded just through a thin wall.
    const FVector MuzzleStart = WeaponActor->GetActorLocation();
    FHitResult BarrelHit;
    const bool bBarrelHit = GetWorld()->LineTraceSingleByChannel(BarrelHit, MuzzleStart, Muzzle, ECC_Visibility, Query);
    const bool bMuzzleHit = bBarrelHit || GetWorld()->LineTraceSingleByChannel(MuzzleHit, Muzzle, Target + Direction * 2.f, ECC_Visibility, Query);
    if (bBarrelHit) MuzzleHit = BarrelHit;
    bLastMuzzleBlocked = bMuzzleHit && (!bCameraHit || MuzzleHit.GetActor() != CameraHit.GetActor()
        || FVector::DistSquared(MuzzleHit.ImpactPoint, CameraHit.ImpactPoint) > FMath::Square(15.f));
    --MagazineAmmo; ++ShotCount;
    const float ActualAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, View.Vector()), -1.0, 1.0)));
    Record(TEXT("CameraRay"), ActualAngle, bCameraHit ? &CameraHit : nullptr, Camera, RayEnd, bCameraHit);
    Record(TEXT("MuzzleRay"), GetCurrentSpreadDegrees(), bMuzzleHit ? &MuzzleHit : nullptr, Muzzle, Target, bLastMuzzleBlocked);
    LastAcceptedShot = {ShotCount, Now(), Muzzle, Target, bLastMuzzleBlocked};
    WeaponActor->PlayShot(LastAcceptedShot.ShotId);
    PlayAnimation(PistolFireAnimation.LoadSynchronous(), ShotInterval);
    Character->FaceBodyYawOnce(View.Yaw, .14f);
    if (GetPC()->GetM3Experience()) GetPC()->GetM3Experience()->NotifyGunshot(Muzzle, Character);
    if (bMuzzleHit || bCameraHit)
    {
        const FHitResult& Hit = bMuzzleHit ? MuzzleHit : CameraHit;
        const FString Bone = Hit.BoneName.ToString().ToLower();
        const bool bHead = Bone == TEXT("head");
        ApplyHit(Hit, (Hit.ImpactPoint - Muzzle).GetSafeNormal(), bHead ? HeadDamage : BodyDamage, true);
    }
    else LastHit = FHitResult();
    OnAcceptedShot.Broadcast(LastAcceptedShot);
    Record(TEXT("AcceptedShotPresentation"), float(LastAcceptedShot.ShotId), nullptr, Muzzle, Target, bLastMuzzleBlocked);
    return true;
}

bool UHCM4CombatComponent::RequestReload()
{
    if (!CanUseCombat() || WeaponMode != EHCM4WeaponMode::Pistol || bReloading || bAttacking
        || MagazineAmmo >= MagazineCapacity || ReserveAmmo <= 0) return false;
    SetAimHeld(false); bReloading = true; ReloadEnd = Now() + ReloadSeconds;
    EnsureWeapon(); if (WeaponActor) WeaponActor->PlayReload();
    PlayAnimation(PistolReloadAnimation.LoadSynchronous(), ReloadSeconds);
    Record(TEXT("ReloadStarted"), ReloadSeconds); return true;
}
bool UHCM4CombatComponent::RequestToggleWeapon()
{
    if (!CanUseCombat() || bReloading || bAttacking) return false;
    const bool VS3=Character->GetAbilities()->IsEnabled();
    if(VS3 && Character->GetAbilities()->IsBusy())return false;
    SetAimHeld(false); AimBlend = 0;
    WeaponMode = VS3 ? (WeaponMode==EHCM4WeaponMode::Unarmed?EHCM4WeaponMode::Sword:WeaponMode==EHCM4WeaponMode::Sword?EHCM4WeaponMode::Pistol:EHCM4WeaponMode::Unarmed)
        : WeaponMode == EHCM4WeaponMode::Unarmed ? EHCM4WeaponMode::Pistol : EHCM4WeaponMode::Unarmed;
    EnsureWeapon(); UpdatePresentation();
    if (WeaponMode == EHCM4WeaponMode::Pistol) PlayAnimation(PistolIdleAnimation.LoadSynchronous(), 0, true);
    else if (Character->GetMesh()->GetAnimInstance())
    {
        if (ActiveMontage) Character->GetMesh()->GetAnimInstance()->Montage_Stop(.12f, ActiveMontage);
        if (PistolBaseMontage) Character->GetMesh()->GetAnimInstance()->Montage_Stop(.12f, PistolBaseMontage);
    }
    if(VS3)Character->GetAbilities()->WeaponChanged();
    Record(TEXT("WeaponChanged"), int32(WeaponMode)); return true;
}
void UHCM4CombatComponent::SetAimHeld(bool bHeld)
{
    const bool bNext = bHeld && CanUseCombat() && WeaponMode == EHCM4WeaponMode::Pistol && !bReloading && !bAttacking;
    if (bAimHeld == bNext) return;
    bAimHeld = bNext;
    if (bAimHeld) PlayAnimation(PistolAimAnimation.LoadSynchronous(), 0, true);
    else if (!bReloading && WeaponMode == EHCM4WeaponMode::Pistol && CanUseCombat())
        PlayAnimation(PistolIdleAnimation.LoadSynchronous(), 0, true);
    Record(TEXT("AimChanged"), bAimHeld ? 1.f : 0.f);
}
void UHCM4CombatComponent::CancelTransient(const TCHAR* Reason)
{
    if (bReloading){Record(TEXT("ReloadInterruptedNoTransfer"));if(WeaponActor)WeaponActor->StopReloadPresentation();}
    if (bAttacking || bAimHeld || bReloading) Record(FName(Reason));
    bAttacking = false; bComboQueued = false; bReloading = false; bAimHeld = false; AimBlend = 0;
    ComboIndex = -1; SwingHits.Reset(); ResetPunchPoseSample();
    if (Character)
    {
        Character->SetCombatMovementScale(1.f);
        if (Character->GetMesh()->GetAnimInstance() && ActiveMontage) Character->GetMesh()->GetAnimInstance()->Montage_Stop(.1f, ActiveMontage);
        if (Character->GetMesh()->GetAnimInstance() && PistolBaseMontage) Character->GetMesh()->GetAnimInstance()->Montage_Stop(.1f, PistolBaseMontage);
    }
    ActiveMontage = nullptr;
    PistolBaseMontage = nullptr;
}
void UHCM4CombatComponent::PrepareForFlight()
{
    CancelTransient(TEXT("VS2FlightTakeoff"));
    WeaponMode = EHCM4WeaponMode::Unarmed;
    AimBlend = 0;
    UpdatePresentation();
}
void UHCM4CombatComponent::SetSeated(bool bNewSeated)
{ CancelTransient(TEXT("SeatTransition")); bSeated = bNewSeated; UpdatePresentation(); }
void UHCM4CombatComponent::UpdatePresentation()
{ if (WeaponActor) WeaponActor->SetEquipped(!bSeated && WeaponMode == EHCM4WeaponMode::Pistol && Character && !Character->IsHidden()); }

void UHCM4CombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    if (PlayerHealth <= 0 && DefeatRecoveryUntil > 0 && Now() >= DefeatRecoveryUntil)
    {
        AHCM1PlayerController* PC = GetPC();
        if (PC && !PC->IsPauseMenuOpen() && PC->RecoverAfterNPCDefeat())
        {
            PlayerHealth = 100.f; DefeatRecoveryUntil = 0; RecoveryInvulnerableUntil = Now() + 3.;
            Record(TEXT("PlayerRecoveredAfterNPCDefeat"), PlayerHealth);
        }
        else DefeatRecoveryUntil = Now() + 1.;
    }
    if (!CanUseCombat())
    { if (bAttacking || bReloading || bAimHeld || AimBlend > 0) CancelTransient(TEXT("GameplayUnavailable")); return; }
    EnsureWeapon(); UpdatePresentation();
    AimBlend = FMath::FInterpConstantTo(AimBlend, bAimHeld ? 1.f : 0.f, DeltaTime, 1.f / FMath::Max(.01f, AimTransitionSeconds));
    if (bAimHeld) Character->FaceBodyYawOnce(GetPC()->GetControlRotation().Yaw, .2f);
    if (bReloading && Now() >= ReloadEnd)
    {
        const int32 Transferred = FMath::Min(MagazineCapacity - MagazineAmmo, ReserveAmmo);
        MagazineAmmo += Transferred; ReserveAmmo -= Transferred; bReloading = false;
        Record(TEXT("ReloadCompleted"), Transferred);
    }
    if (bAttacking)
    {
        const float Elapsed = GetAttackElapsed();
        SweepPunch();
        if (Elapsed >= AttackDurations[ComboIndex])
        {
            const int32 Next = ComboIndex + 1;
            const bool bContinue = bComboQueued && Next < 3;
            bAttacking = false; LastAttackEnd = Now(); Character->SetCombatMovementScale(1.f); ResetPunchPoseSample();
            Record(TEXT("PunchFinished"), Elapsed);
            if (bContinue) StartPunch(Next);
        }
    }
    else if (ComboIndex >= 0 && Now() - LastAttackEnd > ComboResetSeconds) { ComboIndex = -1; Record(TEXT("ComboReset")); }
    if (WeaponMode == EHCM4WeaponMode::Pistol && !bReloading && Character->GetMesh()->GetAnimInstance()
        && (!PistolBaseMontage || !Character->GetMesh()->GetAnimInstance()->Montage_IsPlaying(PistolBaseMontage)))
        PlayAnimation(bAimHeld ? PistolAimAnimation.LoadSynchronous() : PistolIdleAnimation.LoadSynchronous(), 0, true);
}

FString UHCM4CombatComponent::GetHUDText() const
{
    if (!IsCombatEnabled()) return FString();
    if (PlayerHealth <= 0) return FString::Printf(TEXT("生命 0  |  暂时失去行动\n约 %.0f 秒后安全恢复"), FMath::CeilToFloat(GetDefeatRecoveryRemaining()));
    if (WeaponMode == EHCM4WeaponMode::Sword)
        return FString::Printf(TEXT("星潮剑  |  生命 %.0f\n左键松开 挥剑   按住 蓄力\nQ / 滚轮 切换武器"),PlayerHealth);
    if (WeaponMode == EHCM4WeaponMode::Unarmed)
        return FString::Printf(TEXT("空手  |  生命 %.0f\n左键 三段拳击    Q 切换武器"), PlayerHealth);
    return FString::Printf(TEXT("手枪  %d / %d  |  生命 %.0f%s\n左键 单发    按住右键 瞄准\nR 装弹    Q 收枪"), MagazineAmmo, ReserveAmmo, PlayerHealth,
        bReloading ? TEXT("  装弹中") : TEXT(""));
}
void UHCM4CombatComponent::FillSave(UHCM1SaveGame* Save) const
{
    if (!Save || !IsCombatEnabled()) return;
    Save->M4Version = 1; Save->M4Weapon = uint8(WeaponMode); Save->M4MagazineAmmo = MagazineAmmo;
    Save->M4ReserveAmmo = ReserveAmmo; Save->M4PlayerHealth = PlayerHealth;
}
bool UHCM4CombatComponent::ValidateSave(const UHCM1SaveGame* Save) const
{
    return Save && (Save->M4Version == 0 || (Save->M4Version == 1 && Save->M4Weapon <= (Character&&Character->GetAbilities()->IsEnabled()?2:1)
        && Save->M4MagazineAmmo >= 0 && Save->M4MagazineAmmo <= MagazineCapacity
        && Save->M4ReserveAmmo >= 0 && Save->M4ReserveAmmo <= InitialReserve
        && FMath::IsFinite(Save->M4PlayerHealth) && Save->M4PlayerHealth >= 0 && Save->M4PlayerHealth <= 100));
}
void UHCM4CombatComponent::RestoreSave(const UHCM1SaveGame* Save)
{
    if (!ValidateSave(Save)) return;
    CancelTransient(TEXT("LoadState"));
    WeaponMode = Save->M4Version == 0 ? EHCM4WeaponMode::Unarmed : EHCM4WeaponMode(Save->M4Weapon);
    MagazineAmmo = Save->M4Version == 0 ? MagazineCapacity : Save->M4MagazineAmmo;
    ReserveAmmo = Save->M4Version == 0 ? InitialReserve : Save->M4ReserveAmmo;
    PlayerHealth = Save->M4Version == 0 ? 100.f : Save->M4PlayerHealth;
    LastNPCDamageTime = -1000; RecoveryInvulnerableUntil = 0;
    DefeatRecoveryUntil = PlayerHealth <= 0 ? Now() + 3. : 0.;
    if (PlayerHealth <= 0 && Character)
    { Character->GetCharacterMovement()->StopMovementImmediately(); Character->GetCharacterMovement()->DisableMovement(); }
    if (WeaponActor) WeaponActor->ClearTransientEffects();
    EnsureWeapon(); UpdatePresentation();
    Record(TEXT("SaveRestored"), Save->M4Version);
}

float UHCM4CombatComponent::GetDefeatRecoveryRemaining() const
{ return DefeatRecoveryUntil > 0 ? float(FMath::Max(0., DefeatRecoveryUntil - Now())) : 0.f; }

float UHCM4CombatComponent::ReceiveNPCPunch(float Damage, AActor* Attacker)
{
    AHCM3NPC* NPC = Cast<AHCM3NPC>(Attacker);
    AHCM1PlayerController* PC = GetPC();
    if (!Character || !NPC || !NPC->IsCombatEnabled() || NPC->IsPhysicalReactionActive() || !PC ||
        PC->GetPlayerMode() != EHCPlayerMode::OnFoot || Character->IsHidden() || !IsCombatEnabled() || PC->IsPauseMenuOpen() ||
        PlayerHealth <= 0 || !FMath::IsFinite(Damage) || Damage <= 0 || Now() < RecoveryInvulnerableUntil || Now() - LastNPCDamageTime < .45) return 0;
    const float Applied = FMath::Min(PlayerHealth, FMath::Clamp(Damage, 0.f, 30.f)*Character->GetAbilities()->GetDamageMultiplier());
    PlayerHealth -= Applied; LastNPCDamageTime = Now();
    CancelTransient(TEXT("NPCPunchInterruptedPlayerAction")); PC->EndNPCDialogue();
    Character->GetAbilities()->ResetTransient();
    if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
        if (UAnimMontage* Hit = PlayerHitFrontMontage.LoadSynchronous()) Anim->Montage_Play(Hit);
    PC->ShowStatusMessage(FString::Printf(TEXT("受到拳击 -%.0f，生命 %.0f。可后退躲避。"), Applied, PlayerHealth), 2.f);
    Record(TEXT("PlayerNPCPunchDamage"), Applied, nullptr, NPC->GetActorLocation(), Character->GetActorLocation());
    if (PlayerHealth <= 0)
    {
        Character->StopBodyFacing(); Character->GetCharacterMovement()->StopMovementImmediately(); Character->GetCharacterMovement()->DisableMovement();
        DefeatRecoveryUntil = Now() + 3.;
        PC->ShowStatusMessage(TEXT("暂时失去行动，3秒后恢复。"), 3.f);
    }
    return Applied;
}
