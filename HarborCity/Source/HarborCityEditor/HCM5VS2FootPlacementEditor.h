#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2FootPlacementEditor.generated.h"

class UAnimBlueprint;
class USkeleton;
class USkeletalMesh;

UCLASS()
class HARBORCITYEDITOR_API UHCM5VS2FootPlacementEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Fresh copies only. Does not save; false inspects already saved candidates. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Diagnostic")
    static FString ConfigureCandidate(UAnimBlueprint* SourceBlueprint, USkeletalMesh* SourceMesh,
        UAnimBlueprint* CandidateBlueprint, USkeleton* CandidateSkeleton,
        USkeletalMesh* CandidateMesh, bool bApply);
    /** Exact original arms copied into a private FootPlacement batch only. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Diagnostic")
    static FString BindPrivateArms(USkeletalMesh* SourceArms, USkeletalMesh* CandidateArms,
        USkeletalMesh* CandidateBody, bool bApply);
};
