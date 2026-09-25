#include "HCM5VS2NPCAnimInstance.h"
#include "HCM5VS2NPC.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UHCM5VS2NPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const AHCM5VS2NPC* NPC = Cast<AHCM5VS2NPC>(TryGetPawnOwner());
    USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    NPCGroundSpeed = NPC ? NPC->GetVelocity().Size2D() : 0;
    FVector Target;
    const bool bLook = NPC && Mesh && NPC->NPCFace && NPC->NPCFace->IsFaceReady()
        && !NPC->IsDead() && !NPC->IsPhysicalReactionActive() && !NPC->IsHidden()
        && !Mesh->IsSimulatingPhysics(NPC->GetPhysicalRootBone())
        && NPC->GetBehaviourState() != EHCM3NPCBehaviour::HitReaction
        && NPC->NPCFace->GetLookTargetLocation(Target);
    if (!bLook)
    { NPCHeadLookAlpha = NPCEyeLookAlpha = 0; bDirectionInitialized = false; return; }
    const FName Head = NPC->NPCFace->GetHumanoidBone(TEXT("head"));
    if (Head.IsNone() || Mesh->GetBoneIndex(Head) == INDEX_NONE) return;
    const FVector Origin = Mesh->GetSocketLocation(Head);
    const FQuat BodyYaw(FRotator(0, NPC->GetActorRotation().Yaw, 0));
    FRotator Local = BodyYaw.UnrotateVector(Target - Origin).Rotation();
    Local.Yaw = FMath::Clamp(FRotator::NormalizeAxis(Local.Yaw), -35.f, 35.f);
    Local.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Local.Pitch), -15.f, 15.f); Local.Roll = 0;
    const FVector Direction = BodyYaw.RotateVector(Local.Vector());
    if (!bDirectionInitialized) { SmoothedDirection = BodyYaw.GetForwardVector(); bDirectionInitialized = true; }
    SmoothedDirection = FMath::Lerp(SmoothedDirection, Direction, 1.f - FMath::Exp(-FMath::Max(DeltaSeconds, 0.f) * 7.f)).GetSafeNormal();
    NPCLookTarget = Origin + SmoothedDirection * FMath::Clamp(FVector::Dist(Origin, Target), 60.f, 2000.f);
    NPCHeadLookAlpha = FMath::FInterpTo(NPCHeadLookAlpha, 1.f, DeltaSeconds, 5.f);
    NPCEyeLookAlpha = FMath::FInterpTo(NPCEyeLookAlpha, 1.f, DeltaSeconds, 9.f);
}
