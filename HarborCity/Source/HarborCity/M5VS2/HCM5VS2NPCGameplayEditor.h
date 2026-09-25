#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2NPCGameplayEditor.generated.h"

/** Exact isolated NPC gameplay map only; never changes the older navigation author whitelist. */
UCLASS()
class HARBORCITY_API UHCM5VS2NPCGameplayEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Configure its serialized M3 pedestrian navigation class, build, and inspect. Never saves. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|NPCGameplayAuthoring")
    static FString BuildNavigation(UObject* WorldContextObject);

    /** Initialize the saved class if necessary, then inspect saved nav data without rebuilding. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|NPCGameplayAuthoring")
    static FString InspectNavigation(UObject* WorldContextObject);
};
