#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASNormalizedEditor.generated.h"
class UAnimBlueprint;
class UBlendSpace;
class UAnimSequence;
/** One-curve A/B author; never saves or changes source assets. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASNormalizedEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|GAS")
    static FString ConfigureNormalizedLoops(UAnimBlueprint* SourceBlueprint, UBlendSpace* SourceSpace,
        UAnimBlueprint* CandidateBlueprint, UBlendSpace* CandidateSpace,
        const TArray<UAnimSequence*>& CandidateLoops, bool bApply);
};
