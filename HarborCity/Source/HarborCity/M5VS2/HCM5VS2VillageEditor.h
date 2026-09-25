#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2VillageEditor.generated.h"

/** Bounded editor-only access to the locally acquired Village source. No actor/world creation. */
UCLASS()
class HARBORCITY_API UHCM5VS2VillageEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Reads all fourteen actual house BP graphs and SCS templates; never runs construction scripts. */
    UFUNCTION(BlueprintCallable) static FString ProbeVillageHouses();
    /** Dry-run by default. Apply copies only this dependency closure into a fresh VS2 namespace. */
    UFUNCTION(BlueprintCallable) static FString CopyVillageSelection(const TArray<FString>& Roots,
        const FString& Destination, bool bApply = false);
};
