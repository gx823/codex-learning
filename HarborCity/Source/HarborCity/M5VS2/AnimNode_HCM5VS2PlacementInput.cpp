#include "AnimNode_HCM5VS2PlacementInput.h"
#include "Animation/AnimInstanceProxy.h"

void FAnimNode_HCM5VS2PlacementInput::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
    bPreviousValid = bLastSampleValid = false;
    SampleDelta = 0;
    UpdateCounter.Reset();
    EvaluatedCounter.Reset();
}

void FAnimNode_HCM5VS2PlacementInput::InitializeBoneReferences(const FBoneContainer& Bones)
{
    for (FBoneReference* Bone : {&FloorReference,&FootL,&FootR,&BallL,&BallR,&GoalL,&GoalR})
        Bone->Initialize(Bones);
    bPreviousValid = bLastSampleValid = false;
}

bool FAnimNode_HCM5VS2PlacementInput::IsValidToEvaluate(const USkeleton*, const FBoneContainer& Bones)
{
    if (FloorReference.BoneName != TEXT("VB VS2_PlacementFloor")
        || !FMath::IsFinite(ReferenceSurfaceZ) || ReferenceSurfaceZ < -2.f || ReferenceSurfaceZ > -.2f)
        return false;
    for (const FBoneReference* Bone : {&FloorReference,&FootL,&FootR,&BallL,&BallR,&GoalL,&GoalR})
        if (!Bone->IsValidToEvaluate(Bones)) return false;
    return true;
}

void FAnimNode_HCM5VS2PlacementInput::UpdateInternal(const FAnimationUpdateContext& Context)
{
    if (UpdateCounter.HasEverBeenUpdated()
        && !UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter()))
        bPreviousValid = false;
    UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());
    SampleDelta = Context.GetDeltaTime();
    if (!FMath::IsFinite(ActivityAlpha) || ActivityAlpha <= SMALL_NUMBER
        || !FMath::IsFinite(SampleDelta) || SampleDelta <= SMALL_NUMBER || SampleDelta > .075f)
        bPreviousValid = false;
}

void FAnimNode_HCM5VS2PlacementInput::EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
    const FTransform Component = Output.AnimInstanceProxy->GetComponentTransform();
    const FVector Scale = Component.GetScale3D().GetAbs();
    const float UnitScale = Scale.X;
    const bool Uniform = UnitScale > SMALL_NUMBER && FMath::IsNearlyEqual(Scale.X,Scale.Y,1.e-4f)
        && FMath::IsNearlyEqual(Scale.X,Scale.Z,1.e-4f);
    // A second evaluation in the same update must not differentiate against
    // itself. Retain that update's measurement instead of reporting false zero.
    if (!EvaluatedCounter.IsSynchronized_All(UpdateCounter))
    {
        const FBoneReference* Feet[] = {&FootL,&FootR};
        const FBoneReference* Balls[] = {&BallL,&BallR};
        const FBoneReference* Goals[] = {&GoalL,&GoalR};
        FVector WorldBalls[2];
        bool Finite = !Component.ContainsNaN();
        for (int32 Side=0;Side<2;++Side)
        {
            const FTransform FK = Output.Pose.GetComponentSpaceTransform(Feet[Side]->GetCompactPoseIndex(Bones));
            const FTransform Ball = Output.Pose.GetComponentSpaceTransform(Balls[Side]->GetCompactPoseIndex(Bones));
            const FTransform IK = Output.Pose.GetComponentSpaceTransform(Goals[Side]->GetCompactPoseIndex(Bones));
            // Same current FK-ball-to-ankle conversion as native FootPlacement.
            // Goals here have only Orientation/Stride, never this frame's lock.
            const FTransform CurrentBall = Ball.GetRelativeTransform(FK) * IK;
            WorldBalls[Side] = Component.TransformPosition(CurrentBall.GetLocation());
            Finite &= !WorldBalls[Side].ContainsNaN();
        }
        const bool Enabled = FMath::IsFinite(ActivityAlpha) && ActivityAlpha > SMALL_NUMBER;
        const bool DeltaValid = FMath::IsFinite(SampleDelta) && SampleDelta > SMALL_NUMBER && SampleDelta <= .075f;
        // A capsule translation beyond any existing ground speed in one valid
        // sample, or a discontinuous heading, reinitializes rather than locks.
        const bool Continuous = FVector::DistSquared(Component.GetTranslation(),PreviousComponent.GetTranslation()) < FMath::Square(100.f)
            && Component.GetRotation().AngularDistance(PreviousComponent.GetRotation()) < FMath::DegreesToRadians(45.f)
            && Component.GetScale3D().Equals(PreviousComponent.GetScale3D(),1.e-4f);
        bLastSampleValid = bPreviousValid && Enabled && DeltaValid && Finite && Uniform && Continuous;
        for (int32 Side=0;Side<2;++Side)
        {
            LastWorldSpeeds[Side] = bLastSampleValid ? FVector::Distance(WorldBalls[Side],PreviousWorldBalls[Side])/SampleDelta : 1000000.f;
            // Native Graph mode measures mesh-space cm/s. Normalize the actual
            // world displacement by the verified uniform scale for Manual mode.
            LastSpeeds[Side] = bLastSampleValid ? LastWorldSpeeds[Side]/UnitScale : 1000000.f;
            PreviousWorldBalls[Side] = WorldBalls[Side];
        }
        PreviousComponent = Component;
        bPreviousValid = Enabled && DeltaValid && Finite && Uniform;
        EvaluatedCounter.SynchronizeWith(UpdateCounter);
    }
    Output.Curve.Set(TEXT("VS2PlacementSpeed_L"),LastSpeeds[0]);
    Output.Curve.Set(TEXT("VS2PlacementSpeed_R"),LastSpeeds[1]);
    Output.Curve.Set(TEXT("VS2PlacementWorldSpeed_L"),LastWorldSpeeds[0]);
    Output.Curve.Set(TEXT("VS2PlacementWorldSpeed_R"),LastWorldSpeeds[1]);
    Output.Curve.Set(TEXT("VS2PlacementInputValid"),bLastSampleValid?1.f:0.f);
    Output.Curve.Set(TEXT("VS2PlacementDisableLock"),bLastSampleValid?0.f:1.f);
    Output.Curve.Set(TEXT("VS2PlacementPlaneZ"),ReferenceSurfaceZ);
    Output.Curve.Set(TEXT("VS2PlacementUnitScale"),UnitScale);
    // Diagnostic curves preserve the current, unplanted input. They are never
    // read by the lock calculation. Final evaluated curves can be blend-weighted.
    auto RecordPosition=[&](const TCHAR* Prefix,const FVector& Value)
    {
        Output.Curve.Set(FName(*(FString(Prefix)+TEXT("X"))),float(Value.X));
        Output.Curve.Set(FName(*(FString(Prefix)+TEXT("Y"))),float(Value.Y));
        Output.Curve.Set(FName(*(FString(Prefix)+TEXT("Z"))),float(Value.Z));
    };
    RecordPosition(TEXT("VS2PlacementInputHips"),
        Output.Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(0)).GetLocation());
    RecordPosition(TEXT("VS2PlacementInputGoalL"),
        Output.Pose.GetComponentSpaceTransform(GoalL.GetCompactPoseIndex(Bones)).GetLocation());
    RecordPosition(TEXT("VS2PlacementInputGoalR"),
        Output.Pose.GetComponentSpaceTransform(GoalR.GetCompactPoseIndex(Bones)).GetLocation());
    const FTransform Reference(FQuat::Identity,FVector(0,0,ReferenceSurfaceZ),FVector::OneVector);
    OutBoneTransforms.Emplace(FloorReference.GetCompactPoseIndex(Bones),Reference);
}
