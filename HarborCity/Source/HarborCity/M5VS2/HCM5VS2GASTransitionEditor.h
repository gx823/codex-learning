#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASTransitionEditor.generated.h"

class UAnimBlueprint;

/** Exact VS2 transition diagnostics. No authoring, compile, asset save or gameplay side effects. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASTransitionEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Read actual transition/alias fields and complete rule/state pin wiring from the installed VS2 graph. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASTransition")
    static FString ReadTransitionGraph(UAnimBlueprint* Blueprint);
};
