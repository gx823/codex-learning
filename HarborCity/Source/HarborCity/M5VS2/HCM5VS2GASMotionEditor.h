#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASMotionEditor.generated.h"

class USkeleton;
class USkeletalMesh;
class UAnimSequence;
class UAnimBlueprint;
class UBlendSpace;

/** Native authoring only, guarded fresh candidates; callers save and audit hashes. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASMotionEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|GAS")
    static FString PrepareWarpSkeleton(USkeleton* Source, USkeleton* Candidate, USkeletalMesh* CandidateMesh);
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|GAS")
    static FString AddRootSpeedCurve(UAnimSequence* SourceRootMotion, UAnimSequence* CandidateLoop,
        float CalibratedWorldSpeed, float CalibrationMeshScale);
    /** Transitions order: RunStart,SprintStart,RunStop,SprintStop,JumpStand,JumpRun,JumpSprint,Fall,LandStand,LandRun. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|GAS")
    static FString ConfigureMotionGraph(UAnimBlueprint* Candidate, USkeletalMesh* CandidateMesh,
        UBlendSpace* OriginalSpace, UBlendSpace* CandidateSpace, const TArray<UAnimSequence*>& ForwardLoops,
        const TArray<UAnimSequence*>& Transitions);
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|GAS")
    static FString InspectMotionCandidate(UAnimBlueprint* Candidate, USkeletalMesh* CandidateMesh,
        UBlendSpace* OriginalSpace, UBlendSpace* CandidateSpace);
};
