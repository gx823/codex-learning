#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2HeroFillEditor.generated.h"

class UBlueprint;

/** Isolated candidate authoring only. Does not save packages or modify the original Hero. */
UCLASS()
class HARBORCITY_API UHCM5VS2HeroFillEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|HeroFillAuthoring")
    static FString ConfigureHeroFill(UBlueprint* Blueprint, float Candela);
};
