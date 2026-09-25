#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASLocomotionEditor.generated.h"

class UAnimBlueprint;
class UBlendSpace;
class UAnimSequence;

/** Exact VS2 forward-gait author. Rates come from a separate native target-foot probe. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASLocomotionEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Edits only an intact new BlendSpace duplicate and two existing VS2 graph references.
     * Preserves 16 side/back moving samples. Compiles, validates and resamples; never saves. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GAS")
    static FString ApplyForwardLocomotion(UAnimBlueprint* Blueprint, UBlendSpace* Source, UBlendSpace* Target,
        UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, UAnimSequence* Sprint,
        float WalkSampleSpeed, float RunSampleRate, float SprintSampleRate);
};
