#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASClockEditor.generated.h"

class UAnimBlueprint;
class UBlendSpace;

/** Private, single-variable BlendSpace clock experiment. Never saves assets. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASClockEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|GAS")
    static FString ConfigureUnifiedClock(UAnimBlueprint* SourceBlueprint, UBlendSpace* SourceSpace,
        UAnimBlueprint* CandidateBlueprint, UBlendSpace* CandidateSpace, bool bApply);
};
