#include "HCM5VS2LookAnimInstance.h"

#include "HCM5VS2ExpressionComponent.h"
#include "HCM5VS2FlightComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M4/HCM4CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"

void UHCM5VS2LookAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    GlanceRandom.Initialize(int32(GetUniqueID() ^ FPlatformTime::Cycles()));
    GlanceCountdown = GlanceRandom.FRandRange(2.5f, 5.5f);
    GlanceRemaining = 0; bDirectionInitialized = false;
    VS2HeadLookAlpha = VS2EyeLookAlpha = 0;
    bMotionInitialized = false; bWasFalling = bHadMoveInput = bPendingStop = false;
    VS2MotionPoseIndex = 0; VS2MotionElapsed = 0; VS2StrideScale = 1;
    FlightLandingVisualTime=0;bVS2Flying=false;bVS2FlightPoseEligible=false;
    VS2FlightHairForce=VS2FlightRibbonForce=VS2FlightSkirtForce=FVector::ZeroVector;
}

void UHCM5VS2LookAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    AActor* Owner = Mesh ? Mesh->GetOwner() : nullptr;
    const APawn* Pawn = Cast<APawn>(Owner);
    const AHCM1PlayerController* PC = Pawn ? Cast<AHCM1PlayerController>(Pawn->GetController()) : nullptr;
    if (PC && PC->IsPauseMenuOpen()) return;
    UpdateVS2Motion(DeltaSeconds);
    const auto* Face = Owner ? Owner->FindComponentByClass<UHCM5VS2ExpressionComponent>() : nullptr;
    const auto* Combat = Owner ? Owner->FindComponentByClass<UHCM4CombatComponent>() : nullptr;
    const bool bCombatPose = Combat && (Combat->IsAttacking() || Combat->IsReloading() || Combat->IsAimHeld()
        || Combat->GetWeaponMode() == EHCM4WeaponMode::Pistol || Combat->GetPlayerHealth() <= 0);
    // FP, driving and combat retain their existing head/aim poses without a fade competing with them.
    if (!Owner || !Mesh || !Face || !Face->IsFaceReady() || Owner->IsHidden() || bCombatPose
        || Face->GetEffectiveEmotion() == TEXT("Pain")
        || (PC && (PC->IsFirstPersonPerspective() || PC->GetPlayerMode() != EHCPlayerMode::OnFoot)))
    {
        VS2HeadLookAlpha = VS2EyeLookAlpha = 0;
        bVS2ExplicitLookTarget = false; bDirectionInitialized = false;
        return;
    }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
    const FVector Origin = Mesh->GetSocketLocation(TEXT("Head"));
    FVector Target;
    bVS2ExplicitLookTarget = Face->GetLookTargetLocation(Target);
    const FQuat ActorYaw(FRotator(0, Owner->GetActorRotation().Yaw, 0));
    bool bHasTarget = bVS2ExplicitLookTarget;
    if (!bHasTarget && bVS2IdleGlances && Owner->GetVelocity().SizeSquared2D() < 25.f * 25.f)
    {
        if (GlanceRemaining > 0) GlanceRemaining = FMath::Max(0.f, GlanceRemaining - DeltaSeconds);
        else
        {
            GlanceCountdown -= DeltaSeconds;
            if (GlanceCountdown <= 0)
            {
                GlanceRotation = FRotator(GlanceRandom.FRandRange(-3.f, 3.f), GlanceRandom.FRandRange(-9.f, 9.f), 0);
                GlanceRemaining = GlanceRandom.FRandRange(.65f, 1.15f);
                GlanceCountdown = GlanceRandom.FRandRange(3.5f, 7.f);
            }
        }
        Target = Origin + ActorYaw.RotateVector((GlanceRemaining > 0 ? GlanceRotation : FRotator::ZeroRotator).Vector()) * 500.f;
        bHasTarget = GlanceRemaining > 0;
    }
    else if (!bHasTarget) GlanceRemaining = 0;
    const float DesiredAlpha = bHasTarget ? 1.f : 0.f;
    VS2HeadLookAlpha = FMath::FInterpTo(VS2HeadLookAlpha, DesiredAlpha, DeltaSeconds, 5.f);
    VS2EyeLookAlpha = FMath::FInterpTo(VS2EyeLookAlpha, DesiredAlpha, DeltaSeconds, 9.f);
    if (!bHasTarget) Target = Origin + ActorYaw.GetForwardVector() * 500.f;
    FVector Delta = Target - Origin;
    if (Delta.ContainsNaN() || Delta.SizeSquared() < 25.f * 25.f)
    {
        VS2HeadLookAlpha = VS2EyeLookAlpha = 0;
        return;
    }
    // Do not snap around to a target behind the body. Native node clamps also constrain the final bones.
    FRotator Local = ActorYaw.UnrotateVector(Delta).Rotation();
    Local.Yaw = FMath::Clamp(FRotator::NormalizeAxis(Local.Yaw), -40.f, 40.f);
    Local.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Local.Pitch), -18.f, 18.f);
    Local.Roll = 0;
    const FVector DesiredDirection = ActorYaw.RotateVector(Local.Vector()).GetSafeNormal();
    if (!bDirectionInitialized) { SmoothedDirection = ActorYaw.GetForwardVector(); bDirectionInitialized = true; }
    SmoothedDirection = FMath::Lerp(SmoothedDirection, DesiredDirection,
        1.f - FMath::Exp(-DeltaSeconds * 7.f)).GetSafeNormal();
    VS2LookTargetWorld = Origin + SmoothedDirection * FMath::Clamp(Delta.Size(), 60.f, 3000.f);
}

void UHCM5VS2LookAnimInstance::UpdateVS2Motion(float DeltaSeconds)
{
    const AHCM1Character* Character = Cast<AHCM1Character>(TryGetPawnOwner());
    const AHCM1PlayerController* PC = Character ? Cast<AHCM1PlayerController>(Character->GetController()) : nullptr;
    const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
    const UHCM4CombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
    const UHCM5VS2FlightComponent* Flight = Character ? Character->GetFlightComponent() : nullptr;
    const bool bPreviouslyFlying=bVS2Flying;
    bVS2Flying = Flight && Flight->IsFlying();
    bVS2FlightBoost = Flight && Flight->IsBoosting();
    VS2FlightState = Flight ? Flight->GetFlightPresentationState() : FName(TEXT("Grounded"));
    VS2FlightBank = Flight ? Flight->VisualBankDegrees : 0;
    VS2FlightVelocityLocal = Character ? Character->GetActorQuat().UnrotateVector(Character->GetVelocity()) : FVector::ZeroVector;
    UpdateVS2FlightPose(DeltaSeconds,bPreviouslyFlying);
    bVS2MotionEligible = bVS2GASMotionEnabled && PC && Move && Combat && !Character->IsHidden()
        && PC->GetPlayerMode() == EHCPlayerMode::OnFoot && !PC->IsFirstPersonPerspective()
        && PC->IsGameplayFocused() && !PC->IsDialogueOpen() && !bVS2Flying
        && Combat->GetPlayerHealth() > 0 && Combat->GetWeaponMode() == EHCM4WeaponMode::Unarmed
        && !Combat->IsAttacking() && !Combat->IsReloading() && !Combat->IsAimHeld();
    if (!bVS2MotionEligible || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
    {
        VS2MotionPoseIndex = 0; VS2MotionElapsed = 0; VS2WarpAlpha = 0; VS2StrideScale = 1;
        bMotionInitialized = false; bWasFalling = bHadMoveInput = bPendingStop = false;
        return;
    }
    const FVector Velocity = Character->GetVelocity();
    VS2GroundSpeed = Velocity.Size2D();
    const bool bGrounded = Move->IsMovingOnGround(), bFalling = Move->IsFalling();
    const bool bInput = Move->GetCurrentAcceleration().SizeSquared2D() > 1.f;
    const bool bSprint = Move->MaxWalkSpeed > Character->WalkSpeed + 1.f;
    const float Direction = VS2GroundSpeed > 1.f ? FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw,Velocity.Rotation().Yaw) : 0.f;
    const bool bForward = FMath::Abs(Direction) <= 25.f;
    // Preserve the exact side/back samples. A continuous forward band delegates
    // the remaining heading difference to the native orientation-warp node.
    VS2SampleDirection = FMath::Abs(Direction) < 45.f
        ? FMath::Sign(Direction)*FMath::GetMappedRangeValueClamped(FVector2D(15,45),FVector2D(0,45),FMath::Abs(Direction)) : Direction;
    VS2OrientationAngle = FMath::FindDeltaAngleDegrees(VS2SampleDirection,Direction);
    VS2StrideDirection = GetSkelMeshComponent()->GetComponentQuat().UnrotateVector(Velocity.GetSafeNormal2D());
    const auto SetPose = [this](int32 Index)
    { if (VS2MotionPoseIndex != Index) { VS2MotionPoseIndex=Index; VS2MotionElapsed=0; } };
    VS2MotionElapsed += DeltaSeconds;
    if (!bMotionInitialized)
    {
        bMotionInitialized=true; bWasFalling=bFalling; bHadMoveInput=bInput;
        PreviousGroundSpeed=VS2GroundSpeed; bLastSprint=bSprint;
    }
    if (bFalling)
    {
        bPendingStop=false;
        if (!bWasFalling)
            SetPose(Velocity.Z > 100.f ? (PreviousGroundSpeed < 100.f ? 5 : bLastSprint ? 7 : 6) : 8);
        else if (Velocity.Z <= 0.f || VS2MotionPoseIndex < 5 || VS2MotionPoseIndex > 8) SetPose(8);
    }
    else if (bGrounded && bWasFalling) SetPose(VS2GroundSpeed > 80.f ? 10 : 9);
    else if (bGrounded)
    {
        if (VS2MotionPoseIndex==9 || VS2MotionPoseIndex==10)
        {
            // Visual absorption only; never delay movement or a new Jump.
            if (VS2MotionElapsed >= (VS2MotionPoseIndex==9 && !bInput ? .70f : .12f)) SetPose(0);
        }
        else
        {
            if (bHadMoveInput && !bInput && bForward && PreviousGroundSpeed>100.f) bPendingStop=true;
            if (bInput && !bHadMoveInput && PreviousGroundSpeed<30.f && bForward)
            { bPendingStop=false; SetPose(bSprint ? 2 : 1); }
            if ((VS2MotionPoseIndex==1 || VS2MotionPoseIndex==2)
                && (!bInput || !bForward || VS2MotionElapsed >= (VS2MotionPoseIndex==2 ? .3158f : .2016f))) SetPose(0);
            if (bPendingStop && !bInput && VS2GroundSpeed<20.f)
            { SetPose(bLastSprint ? 4 : 3); bPendingStop=false; }
            if ((VS2MotionPoseIndex==3 || VS2MotionPoseIndex==4)
                && (bInput || !bForward || VS2MotionElapsed >= (VS2MotionPoseIndex==4 ? .6667f : .7667f))) SetPose(0);
            if (bInput) bPendingStop=false;
            if (VS2MotionPoseIndex>=5) SetPose(0);
        }
    }
    else SetPose(0);
    const float ForwardWeight=FMath::GetMappedRangeValueClamped(FVector2D(15,45),FVector2D(1,0),FMath::Abs(Direction));
    VS2WarpAlpha=bGrounded && VS2MotionPoseIndex==0 ? FMath::Clamp(VS2GroundSpeed/100.f,0.f,1.f)*ForwardWeight : 0;
    // Candidate loop curves store source root-speed shape in target mesh units,
    // including the existing single BlendSpace sample rate. No DeltaTime gain.
    const float RawNominalSpeed=GetCurveValue(TEXT("VS2NominalRootSpeed"));
    float NominalWeight=0;
    const bool bHasNominalWeight=GetSkelMeshComponent()->GetCurveValue(TEXT("VS2NominalRootWeight"),0,NominalWeight);
    // Only new private loops carry the companion curve. Both values come from
    // the same previously evaluated graph, so transition/side-clip weight must
    // divide out before interpreting the authored speed as a stride length.
    // Existing candidates without this curve retain their exact old path.
    const float NominalSpeed=bHasNominalWeight
        ? (FMath::IsFinite(NominalWeight) && NominalWeight>SMALL_NUMBER ? RawNominalSpeed/NominalWeight : 0.f)
        : RawNominalSpeed;
    VS2NominalRootSpeed=NominalSpeed*GetSkelMeshComponent()->GetComponentScale().GetAbsMax();
    const float Wanted=VS2WarpAlpha>0 && VS2NominalRootSpeed>30.f ? FMath::Clamp(VS2GroundSpeed/VS2NominalRootSpeed,.75f,1.3f) : 1.f;
    VS2StrideScale=FMath::FInterpTo(VS2StrideScale,Wanted,DeltaSeconds,12.f);
    bWasFalling=bFalling; bHadMoveInput=bInput; PreviousGroundSpeed=VS2GroundSpeed;
    if (bInput) bLastSprint=bSprint;
}

void UHCM5VS2LookAnimInstance::UpdateVS2FlightPose(float DeltaSeconds,bool bPreviouslyFlying)
{
    const auto* Character=Cast<AHCM1Character>(TryGetPawnOwner());
    const auto* PC=Character?Cast<AHCM1PlayerController>(Character->GetController()):nullptr;
    const auto* Move=Character?Character->GetCharacterMovement():nullptr;
    const auto* Combat=Character?Character->GetCombatComponent():nullptr;
    if(!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;
    if(bPreviouslyFlying&&!bVS2Flying&&Move&&Move->IsMovingOnGround())FlightLandingVisualTime=.55f;
    else FlightLandingVisualTime=FMath::Max(0.f,FlightLandingVisualTime-DeltaSeconds);
    // A fresh jump/takeoff immediately owns the pose; the visual landing tail
    // is only for an idle grounded character, never a movement lock.
    if(bVS2Flying||!Move||!Move->IsMovingOnGround()||Move->GetCurrentAcceleration().SizeSquared2D()>1.f)
        FlightLandingVisualTime=0;
    bVS2FlightPoseEligible=bVS2FlightPosesEnabled&&PC&&Move&&Combat&&PC->IsGameplayFocused()
        &&!PC->IsPauseMenuOpen()&&!PC->IsDialogueOpen()&&!PC->IsFirstPersonPerspective()
        &&PC->GetPlayerMode()==EHCPlayerMode::OnFoot&&!Character->IsHidden()&&Combat->GetPlayerHealth()>0
        &&Combat->GetWeaponMode()==EHCM4WeaponMode::Unarmed&&!Combat->IsAttacking()
        &&(bVS2Flying||FlightLandingVisualTime>0);
    VS2FlightPoseIndex=FlightLandingVisualTime>0?6:VS2FlightState==TEXT("Takeoff")?0:
        VS2FlightState==TEXT("Boost")?3:VS2FlightState==TEXT("Forward")?2:
        VS2FlightState==TEXT("Ascending")?4:
        (VS2FlightState==TEXT("Descending")||VS2FlightState==TEXT("Landing"))?5:1;
    VS2FlightBankRotation=FRotator::ZeroRotator;
    if(!bVS2FlightPoseEligible||!bVS2Flying)
    {VS2FlightHairForce=VS2FlightRibbonForce=VS2FlightSkirtForce=FVector::ZeroVector;return;}
    const FVector Velocity=Character->GetVelocity();
    VS2FlightHairForce=FMath::VInterpTo(VS2FlightHairForce,(-Velocity*.009f).GetClampedToMaxSize(18.f),DeltaSeconds,6.f);
    VS2FlightRibbonForce=FMath::VInterpTo(VS2FlightRibbonForce,(-Velocity*.011f).GetClampedToMaxSize(22.f),DeltaSeconds,6.f);
    // No upward skirt drive. Existing cone/leg colliders remain necessary but
    // these bounded parameters alone do not establish coverage at low angles.
    const FVector SkirtTarget=FVector(-Velocity.X,-Velocity.Y,0).GetClampedToMaxSize(2000.f)*.001f;
    VS2FlightSkirtForce=FMath::VInterpTo(VS2FlightSkirtForce,SkirtTarget,DeltaSeconds,6.f);
    const FQuat MeshQ=GetSkelMeshComponent()->GetComponentQuat();
    const FQuat WorldBank(Character->GetActorForwardVector(),FMath::DegreesToRadians(FMath::Clamp(VS2FlightBank,-12.f,12.f)));
    VS2FlightBankRotation=(MeshQ.Inverse()*WorldBank*MeshQ).Rotator();
}
