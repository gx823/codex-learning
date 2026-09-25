#include "HCM5VS2NPC.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "M4/HCM4R1ReactionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Package.h"

AHCM5VS2NPC::AHCM5VS2NPC()
{
    NPCFace = CreateDefaultSubobject<UHCM5VS2NPCFaceComponent>(TEXT("M5VS2NPCFace"));
}

FName AHCM5VS2NPC::GetPhysicalRootBone() const
{
    const FName* Bone = NPCProfile ? NPCProfile->HumanoidBones.Find(TEXT("hips")) : nullptr;
    return Bone ? *Bone : NAME_None;
}

bool AHCM5VS2NPC::ShouldBlockPhysicsBodiesDuringRagdoll() const
{
    // The reviewed PhysicsAsset keeps adjacent pairs disabled. Its enabled
    // non-adjacent pairs also need the component's PhysicsBody channel to block.
    return true;
}

bool AHCM5VS2NPC::ApplyInitialVisuals()
{
    bVisualProfileReady = false;
    USkeletalMeshComponent* BodyMesh = GetMesh();
    if (!NPCProfile || !NPCProfile->Mesh || !BodyMesh || BodyMesh->GetSkeletalMeshAsset() != NPCProfile->Mesh
        || !BodyMesh->GetSkeletalMeshAsset()->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/")))
    { UE_LOG(LogTemp, Error, TEXT("M5VS2_NPC_PROFILE_INVALID id=%s"), *StableId.ToString()); return false; }
    const FName Hips = GetPhysicalRootBone();
    if (Hips.IsNone() || BodyMesh->GetBoneIndex(Hips) == INDEX_NONE || !BodyMesh->GetAnimInstance()
        || !BodyMesh->GetPhysicsAsset() || BodyMesh->GetPhysicsAsset()->FindBodyIndex(Hips) == INDEX_NONE)
    { UE_LOG(LogTemp, Error, TEXT("M5VS2_NPC_RIG_INCOMPLETE id=%s hips=%s"), *StableId.ToString(), *Hips.ToString()); return false; }
    for (const UAnimMontage* Montage : {HitFrontMontage.Get(), HitBackMontage.Get(), HitLeftMontage.Get(), HitRightMontage.Get()})
        if (!Montage || Montage->GetSkeleton() != NPCProfile->Mesh->GetSkeleton())
        { UE_LOG(LogTemp, Error, TEXT("M5VS2_NPC_REACTION_INCOMPATIBLE id=%s"), *StableId.ToString()); return false; }
    const UAnimSequence* Counter = GetReactionComponent() ? GetReactionComponent()->CounterAnimation.LoadSynchronous() : nullptr;
    if (GetCounterHandBone().IsNone() || BodyMesh->GetBoneIndex(GetCounterHandBone()) == INDEX_NONE
        || !Counter || Counter->GetSkeleton() != NPCProfile->Mesh->GetSkeleton())
    { UE_LOG(LogTemp, Error, TEXT("M5VS2_NPC_COUNTER_INCOMPATIBLE id=%s"), *StableId.ToString()); return false; }
    BodyMesh->GetAnimInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    BodyMesh->bEnableUpdateRateOptimizations = false; // Two close review specimens; retain visible face updates.
    NPCFace->Profile = NPCProfile; NPCFace->TargetMesh = BodyMesh;
    bVisualProfileReady = NPCFace->ReinitializeFace();
    UE_LOG(LogTemp, Display, TEXT("M5VS2_NPC_PROFILE id=%s ready=%d mesh=%s hips=%s face=%d"),
        *StableId.ToString(), bVisualProfileReady, *GetPathNameSafe(BodyMesh->GetSkeletalMeshAsset()), *Hips.ToString(), NPCFace->IsFaceReady());
    return bVisualProfileReady;
}

FName AHCM5VS2NPC::GetCounterHandBone() const
{
    const FName* Bone = NPCProfile ? NPCProfile->HumanoidBones.Find(TEXT("rightHand")) : nullptr;
    return Bone ? *Bone : NAME_None;
}

void AHCM5VS2NPC::BeginPlay()
{
    Super::BeginPlay();
    // A standalone review map may intentionally have no M3Experience.
    if (!bVisualProfileReady) ApplyInitialVisuals();
}
