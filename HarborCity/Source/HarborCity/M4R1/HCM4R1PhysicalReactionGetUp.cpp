#include "HCM4R1PhysicalReactionComponent.h"
#include "M3/HCM3NPC.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"

namespace
{
bool GetUpUnitAxes(const FVector& Forward, const FVector& Right)
{
    return !Forward.ContainsNaN() && !Right.ContainsNaN() &&
        FMath::IsNearlyEqual(Forward.SizeSquared(),1.,.02) && FMath::IsNearlyEqual(Right.SizeSquared(),1.,.02) &&
        FMath::Abs(FVector::DotProduct(Forward,Right))<.02;
}
}

bool UHCM4R1PhysicalReactionComponent::ValidateAuthoredGetUp(FString& Error) const
{
    const AHCM3NPC* OwnerNPC=NPC.Get();
    const USkeletalMeshComponent* Mesh=OwnerNPC ? OwnerNPC->GetMesh() : nullptr;
    if (!bUseAuthoredGetUp || !OwnerNPC || !OwnerNPC->SupportsAuthoredGetUp() || !Mesh ||
        !Mesh->GetSkeletalMeshAsset() || !Mesh->GetAnimInstance() || !bGetUpPoseDataVerified || GetUpClips.Num()!=4)
    { Error=TEXT("FOUR_TARGET_CLIPS_AND_NATIVE_POSE_DATA_REQUIRED"); return false; }
    if (GetUpChestBone.IsNone() || Mesh->GetBoneIndex(GetUpChestBone)==INDEX_NONE || !Mesh->GetBodyInstance(GetUpChestBone) ||
        !GetUpUnitAxes(GetUpChestForwardBoneLocal,GetUpChestRightBoneLocal) ||
        GetUpAuthoredComponentScale.ContainsNaN() || GetUpAuthoredComponentScale.GetMin()<=0 ||
        !Mesh->GetComponentScale().Equals(GetUpAuthoredComponentScale,.001) ||
        !OwnerNPC->GetActorScale3D().Equals(FVector::OneVector,.001))
    { Error=TEXT("TARGET_CHEST_AXES_OR_RUNTIME_SCALE_MISMATCH"); return false; }
    USkeleton* Skeleton=Mesh->GetSkeletalMeshAsset()->GetSkeleton();
    if (!Skeleton || !Skeleton->ContainsSlotName(TEXT("FullBody")))
    { Error=TEXT("FULLBODY_SLOT_REQUIRED"); return false; }
    uint8 Directions=0;
    for (const FHCM4R1GetUpClip& Clip:GetUpClips)
    {
        const uint8 Direction=uint8(Clip.Direction);
        UAnimSequence* Animation=Clip.Animation.LoadSynchronous();
        if (Direction>3 || (Directions&(1<<Direction)) || !Animation || Animation->GetSkeleton()!=Skeleton ||
            !Animation->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/")) || !Animation->HasRootMotion() ||
            !FMath::IsFinite(Animation->GetPlayLength()) || Animation->GetPlayLength()<=0 || Animation->GetPlayLength()>10 ||
            Clip.FirstHipsComponent.ContainsNaN() || Clip.EndHipsComponent.ContainsNaN() ||
            Clip.EndHipsComponent.Z<=Clip.FirstHipsComponent.Z+20 ||
            !GetUpUnitAxes(Clip.FirstChestForwardComponent,Clip.FirstChestRightComponent))
        { Error=TEXT("TARGET_ANIMATION_OR_EFFECTIVE_FIRST_END_POSE_INVALID"); return false; }
        Directions|=1<<Direction;
    }
    return Directions==15;
}

bool UHCM4R1PhysicalReactionComponent::TryBeginAuthoredGetUp()
{
    if (!ValidateAuthoredGetUp(GetUpLastResult)) return false;
    AHCM3NPC* OwnerNPC=NPC.Get(); USkeletalMeshComponent* Mesh=OwnerNPC->GetMesh();
    FBodyInstance* Chest=Mesh->GetBodyInstance(GetUpChestBone);
    if (!Chest || !Chest->IsInstanceSimulatingPhysics() || !Mesh->IsSimulatingPhysics())
    { GetUpLastResult=TEXT("SETTLED_PHYSICAL_POSE_REQUIRED"); return false; }
    const FQuat ChestWorld=Chest->GetUnrealWorldTransform().GetRotation();
    const FVector ActualForward=ChestWorld.RotateVector(GetUpChestForwardBoneLocal).GetSafeNormal();
    const FVector ActualRight=ChestWorld.RotateVector(GetUpChestRightBoneLocal).GetSafeNormal();
    const FVector Hips=GetPhysicalLocation();
    if (Hips.ContainsNaN() || !GetUpUnitAxes(ActualForward,ActualRight))
    { GetUpLastResult=TEXT("ACTUAL_PHYSICAL_AXES_INVALID"); return false; }
    int32 Best=INDEX_NONE; double BestScore=-2; FTransform BestStand; float BestError=0;
    int32 MatchingPoses=0, SafeStands=0; float SmallestSafeError=MAX_flt;
    for (int32 I=0; I<GetUpClips.Num(); ++I)
    {
        const FHCM4R1GetUpClip& Clip=GetUpClips[I];
        const FVector LocalForward=InitialMeshRelative.TransformVectorNoScale(Clip.FirstChestForwardComponent).GetSafeNormal();
        const FVector LocalRight=InitialMeshRelative.TransformVectorNoScale(Clip.FirstChestRightComponent).GetSafeNormal();
        // Least-squares yaw from both horizontal axis projections; vertical axes
        // still contribute to the full 3-D pose score used to choose B/F/L/R.
        const double C=LocalForward.X*ActualForward.X+LocalForward.Y*ActualForward.Y+
            LocalRight.X*ActualRight.X+LocalRight.Y*ActualRight.Y;
        const double S=LocalForward.X*ActualForward.Y-LocalForward.Y*ActualForward.X+
            LocalRight.X*ActualRight.Y-LocalRight.Y*ActualRight.X;
        if (FMath::Square(C)+FMath::Square(S)<.01) continue;
        // The unconstrained best yaw can put the standing capsule in a nearby
        // wall. Try bounded alternatives around that same fallen pose. Every
        // alternative keeps the existing navigation, capsule and hips checks;
        // no physical body or obstacle is moved to make a candidate fit.
        for (const double OffsetDegrees : {0., 15., -15., 30., -30.})
        {
            const FQuat Yaw(FVector::UpVector,FMath::Atan2(S,C)+FMath::DegreesToRadians(OffsetDegrees));
            const double Score=.5*(FVector::DotProduct(Yaw.RotateVector(LocalForward),ActualForward)+
                FVector::DotProduct(Yaw.RotateVector(LocalRight),ActualRight));
            if (Score<.5 || Score<=BestScore) continue;
            ++MatchingPoses;
            const FVector CandidateCenter=Hips-Yaw.RotateVector(InitialMeshRelative.TransformPosition(Clip.FirstHipsComponent));
            FTransform Stand;
            if (!OwnerNPC->ResolvePhysicalGetUpStand(FTransform(Yaw,CandidateCenter),Stand)) continue;
            ++SafeStands;
            const FVector ExpectedHips=(InitialMeshRelative*Stand).TransformPosition(Clip.FirstHipsComponent);
            const float Error=FVector::Dist(ExpectedHips,Hips);
            SmallestSafeError=FMath::Min(SmallestSafeError,Error);
            if (Error>FMath::Clamp(GetUpMaximumAlignmentErrorCm,1.f,30.f)) continue;
            Best=I; BestScore=Score; BestStand=Stand; BestError=Error;
        }
    }
    if (Best==INDEX_NONE)
    {
        GetUpLastResult=FString::Printf(TEXT("NO_MATCHING_POSE_WITH_SAFE_CAPSULE_AND_HIPS_ALIGNMENT poses=%d safe=%d min_hips_cm=%.3f"),
            MatchingPoses,SafeStands,SafeStands?SmallestSafeError:-1.f);
        return false;
    }
    UAnimInstance* Anim=Mesh->GetAnimInstance();
    SavedRootMotionMode=uint8(Anim->RootMotionMode.GetValue()); bSavedRootMotionMode=true;
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    GetUpMontage=Anim->PlaySlotAnimationAsDynamicMontage(GetUpClips[Best].Animation.Get(),TEXT("FullBody"),0.f,.12f,1.f,1,-1.f,0.f);
    if (!GetUpMontage)
    { CancelAuthoredGetUp(); GetUpLastResult=TEXT("DYNAMIC_FULLBODY_MONTAGE_FAILED"); return false; }
    Anim->Montage_Pause(GetUpMontage);
    FOnMontageEnded EndDelegate; EndDelegate.BindUObject(this,&UHCM4R1PhysicalReactionComponent::GetUpMontageEnded);
    Anim->Montage_SetEndDelegate(EndDelegate,GetUpMontage);
    GetUpClipIndex=Best; GetUpDirectionScore=BestScore; GetUpAlignmentErrorCm=BestError;
    GetUpEndHipsErrorCm=0;
    GetUpLastMontagePosition=0; bGetUpMontageEnded=false; bGetUpMontageInterrupted=false; GetUpFirstPoseReadyFrame=0;
    GetUpStage=EGetUpStage::Align; RecoveryStand=BestStand; LastLanding=Hips; bEmergencyRecovery=false;
    RecoveryBodyBeforeReference=GetPhysicalLocation();
    // The mesh stays detached and simulating. Only its animation reference and
    // disabled capsule are aligned; no physical body is teleported to standing.
    OwnerNPC->SetActorTransform(BestStand,false,nullptr,ETeleportType::None);
    OriginalPhysicsTransformUpdateMode=uint8(Mesh->PhysicsTransformUpdateMode); bRecoveryOverridesTransformMode=true;
    Mesh->PhysicsTransformUpdateMode=EPhysicsTransformUpdateMode::ComponentTransformIsKinematic;
    RecoveryMeshWorld=InitialMeshRelative*BestStand;
    Mesh->SetWorldTransform(RecoveryMeshWorld,false,nullptr,ETeleportType::None);
    RecoveryBodyAfterReference=GetPhysicalLocation();
    Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
    RecoveryStartedAt=GetWorld()->GetTimeSeconds(); bRecovering=true;
    GetUpLastResult=TEXT("ALIGNING_PHYSICS_TO_VERIFIED_FIRST_POSE");
    return true;
}

void UHCM4R1PhysicalReactionComponent::TickAuthoredGetUp()
{
    AHCM3NPC* OwnerNPC=NPC.Get(); USkeletalMeshComponent* Mesh=OwnerNPC ? OwnerNPC->GetMesh() : nullptr;
    UAnimInstance* Anim=Mesh ? Mesh->GetAnimInstance() : nullptr;
    if (!OwnerNPC || !Mesh || !Anim || !GetUpMontage || !GetUpClips.IsValidIndex(GetUpClipIndex))
    { GetUpLastResult=TEXT("GETUP_RUNTIME_DEPENDENCY_LOST"); AbortRecovery(); return; }
    const double Now=GetWorld()->GetTimeSeconds();
    if (Anim->Montage_IsActive(GetUpMontage)) GetUpLastMontagePosition=Anim->Montage_GetPosition(GetUpMontage);
    if (GetUpStage==EGetUpStage::Align)
    {
        FTransform StillSafe;
        if (!Anim->Montage_IsActive(GetUpMontage) || GetUpLastMontagePosition>.02f ||
            !OwnerNPC->ResolvePhysicalGetUpStand(RecoveryStand,StillSafe) ||
            FVector::Dist(StillSafe.GetLocation(),RecoveryStand.GetLocation())>2)
        { GetUpLastResult=TEXT("FIRST_POSE_OR_STAND_NO_LONGER_SAFE"); AbortRecovery(); return; }
        const float Alpha=FMath::Clamp(float((Now-RecoveryStartedAt)/FMath::Clamp(RecoveryBlendSeconds,.35f,1.5f)),0.f,1.f);
        Mesh->SetAllBodiesPhysicsBlendWeight(1.f-FMath::SmoothStep(0.f,1.f,Alpha));
        if (Alpha<1.f) return;
        // Let the mesh actually evaluate the full paused first pose before
        // detaching the physics state; setting weight zero alone is not evaluation.
        if (!GetUpFirstPoseReadyFrame) { GetUpFirstPoseReadyFrame=GFrameCounter; return; }
        if (GFrameCounter<=GetUpFirstPoseReadyFrame) return;
        GetUpStage=EGetUpStage::Playing; // Any failure below must restart physical bodies from this pose.
        Mesh->SetSimulatePhysics(false); Mesh->SetAllBodiesPhysicsBlendWeight(0.f);
        RestorePhysicsTransformMode();
        bLastRecoveryAttachSucceeded=Mesh->AttachToComponent(OwnerNPC->GetCapsuleComponent(),FAttachmentTransformRules::KeepWorldTransform);
        if (!bLastRecoveryAttachSucceeded)
        { GetUpLastResult=TEXT("GETUP_MESH_ATTACH_FAILED"); AbortRecovery(); return; }
        Mesh->SetRelativeTransform(InitialMeshRelative);
        Mesh->SetCollisionObjectType(ECC_Pawn); Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        if (!OwnerNPC->BeginPhysicalGetUpMovement())
        { GetUpLastResult=TEXT("GETUP_CAPSULE_OR_MOVEMENT_FAILED"); AbortRecovery(); return; }
        OwnerNPC->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
        Mesh->ConsumeRootMotion(); // The paused alignment stage must contribute no old delta.
        Anim->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
        GetUpPlayStarted=Now; GetUpStartActorLocation=OwnerNPC->GetActorLocation();
        Anim->Montage_Resume(GetUpMontage);
        GetUpLastResult=TEXT("PLAYING_AUTHORED_ROOT_MOTION_GETUP");
        return;
    }
    if (bGetUpMontageEnded)
    {
        // The delegate runs inside animation update, before CharacterMovement
        // consumes that frame's last root delta. Finish on a later frame only.
        if (GFrameCounter<=GetUpEndedFrame) return;
        if (bGetUpMontageInterrupted)
        { GetUpLastResult=TEXT("GETUP_MONTAGE_INTERRUPTED"); AbortRecovery(); return; }
        GetUpEndHipsErrorCm=FVector::Dist(Mesh->GetSocketLocation(OwnerNPC->GetPhysicalRootBone()),
            Mesh->GetComponentTransform().TransformPosition(GetUpClips[GetUpClipIndex].EndHipsComponent));
        FTransform CurrentSafe;
        const bool bSafe=OwnerNPC->ResolvePhysicalGetUpStand(OwnerNPC->GetActorTransform(),CurrentSafe);
        const float SafeDistance=bSafe ? FVector::Dist(CurrentSafe.GetLocation(),OwnerNPC->GetActorLocation()) : -1.f;
        const bool bCapsule=OwnerNPC->GetCapsuleComponent()->BodyInstance.IsValidBodyInstance();
        const bool bWalking=OwnerNPC->GetCharacterMovement()->MovementMode==MOVE_Walking;
        const bool bAttached=Mesh->GetAttachParent()==OwnerNPC->GetCapsuleComponent();
        const bool bRelative=Mesh->GetRelativeTransform().Equals(InitialMeshRelative,.1f);
        const bool bHips=FMath::IsFinite(GetUpEndHipsErrorCm) && GetUpEndHipsErrorCm<=FMath::Clamp(GetUpMaximumAlignmentErrorCm,1.f,30.f);
        UE_LOG(LogTemp,Display,TEXT("M5VS2_GETUP_END_STAND id=%s safe=%d distance_cm=%.3f capsule=%d walking=%d attached=%d relative=%d hips_ok=%d hips_error_cm=%.3f"),
            *OwnerNPC->StableId.ToString(),bSafe,SafeDistance,bCapsule,bWalking,bAttached,bRelative,bHips,GetUpEndHipsErrorCm);
        if (!bSafe || SafeDistance>10 || !bCapsule || !bWalking || !bAttached || !bRelative || !bHips)
        { GetUpLastResult=TEXT("GETUP_END_STAND_INVALID"); AbortRecovery(); return; }
        CancelAuthoredGetUp(); RestoreAnimationTick();
        bActive=false; bRecovering=false;
        OwnerNPC->EndPhysicalReactionState(CurrentSafe,DangerPoint);
        ++RecoveryEvents; ++AuthoredGetUpCompletions; LastRecoveryReason=TEXT("AUTHORED_GETUP_COMPLETED_AT_CURRENT_CAPSULE");
        GetUpLastResult=LastRecoveryReason; SetComponentTickEnabled(false);
        UE_LOG(LogTemp,Display,TEXT("M5VS2_GETUP_COMPLETE id=%s clip=%s hips_error_cm=%.3f displacement_cm=%.3f"),
            *OwnerNPC->StableId.ToString(),*GetUpClips[GetUpClipIndex].Animation.ToString(),GetUpAlignmentErrorCm,
            FVector::Dist(GetUpStartActorLocation,OwnerNPC->GetActorLocation()));
        return;
    }
    const UAnimSequence* Clip=GetUpClips[GetUpClipIndex].Animation.Get();
    // A normally blending-out montage is removed from ActiveMontagesMap before
    // its end delegate fires. Keep waiting for that existing instance to finish.
    if (!Clip || !Anim->GetInstanceForMontage(GetUpMontage) || Now-GetUpPlayStarted>Clip->GetPlayLength()+1.5)
    { GetUpLastResult=TEXT("GETUP_MONTAGE_DISAPPEARED_OR_TIMED_OUT"); AbortRecovery(); }
}

void UHCM4R1PhysicalReactionComponent::GetUpMontageEnded(UAnimMontage* Montage,bool bInterrupted)
{
    if (GetUpStage==EGetUpStage::None || Montage!=GetUpMontage) return;
    bGetUpMontageEnded=true; bGetUpMontageInterrupted=bInterrupted; GetUpEndedFrame=GFrameCounter;
}

void UHCM4R1PhysicalReactionComponent::CancelAuthoredGetUp()
{
    GetUpStage=EGetUpStage::None;
    if (AHCM3NPC* OwnerNPC=NPC.Get())
    {
        OwnerNPC->GetCharacterMovement()->RemoveTickPrerequisiteComponent(this);
        if (UAnimInstance* Anim=OwnerNPC->GetMesh()->GetAnimInstance())
        {
            if (GetUpMontage)
            {
                FOnMontageEnded NoDelegate; Anim->Montage_SetEndDelegate(NoDelegate,GetUpMontage);
                Anim->Montage_Stop(0.f,GetUpMontage);
            }
            if (bSavedRootMotionMode) Anim->SetRootMotionMode(ERootMotionMode::Type(SavedRootMotionMode));
        }
    }
    GetUpMontage=nullptr; bSavedRootMotionMode=false; bGetUpMontageEnded=false; bGetUpMontageInterrupted=false;
}

bool UHCM4R1PhysicalReactionComponent::RestartGetUpRagdoll()
{
    AHCM3NPC* OwnerNPC=NPC.Get();
    if (!OwnerNPC || OwnerNPC->IsDead() || GetUpStage!=EGetUpStage::Playing) return false;
    USkeletalMeshComponent* Mesh=OwnerNPC->GetMesh(); const FVector Velocity=OwnerNPC->GetVelocity();
    const FTransform CanonicalRelative=InitialMeshRelative;
    CancelAuthoredGetUp();
    // Synchronize to the already evaluated visible animation pose, without
    // advancing the animation again or restoring the earlier fallen position.
    Mesh->HandleExistingParallelEvaluationTask(true,true);
    Mesh->UpdateKinematicBonesToAnim(Mesh->GetComponentSpaceTransforms(),ETeleportType::TeleportPhysics,false,EAllowKinematicDeferral::DisallowDeferral);
    RestoreAnimationTick(); // Re-entering must preserve the original, not the temporary get-up tick policy.
    bActive=false; bRecovering=false;
    const bool bRestarted=StartLivingRagdoll(Velocity.ContainsNaN() ? FVector::ZeroVector : Velocity);
    InitialMeshRelative=CanonicalRelative; // Also retain this if a preceding attach attempt failed.
    if (!bRestarted) return false;
    Mesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector,false);
    return true;
}
