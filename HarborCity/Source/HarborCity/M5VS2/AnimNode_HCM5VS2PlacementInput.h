#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_HCM5VS2PlacementInput.generated.h"

/** Private-candidate preparation only. Does not lock or solve a real bone. */
USTRUCT(BlueprintInternalUseOnly)
struct HARBORCITY_API FAnimNode_HCM5VS2PlacementInput : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference FloorReference;
    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference FootL;
    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference FootR;
    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference BallL;
    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference BallR;
    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference GoalL;
    UPROPERTY(EditAnywhere, Category="VS2")
    FBoneReference GoalR;

    // Mean Z of the twelve frozen reference shoe-surface samples. This is an
    // animation reference plane in mesh space, never a fixed world altitude.
    UPROPERTY(EditAnywhere, Category="VS2")
    float ReferenceSurfaceZ = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VS2", meta=(PinShownByDefault))
    float ActivityAlpha = 0;

    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,
        TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    FGraphTraversalCounter UpdateCounter;
    FGraphTraversalCounter EvaluatedCounter;
    float SampleDelta = 0;
    bool bPreviousValid = false;
    FTransform PreviousComponent = FTransform::Identity;
    FVector PreviousWorldBalls[2] = {FVector::ZeroVector, FVector::ZeroVector};
    float LastSpeeds[2] = {1000000.f,1000000.f};
    float LastWorldSpeeds[2] = {1000000.f,1000000.f};
    bool bLastSampleValid = false;
};
