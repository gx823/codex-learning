#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASTransitionAssetsEditor.generated.h"

/** Ten fixed GAS transition candidates; creates only independent, unbound assets. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASTransitionAssetsEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Dry run validates the exact source closure and all cut data. Apply requires its successful report.
     * Saves ten clean source copies and ten cropped, root-fixed working clips in one fresh batch.
     * Existing source, skeleton, mesh, Blueprint, gameplay and map packages are never saved. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASTransition")
    static FString PrepareTransitionAssets(const FString& ProbeJsonPath, const FString& CutPlanJsonPath,
        const FString& DestinationRoot, const FString& PriorDryRunJsonPath, bool bApply);

    /** Fresh-commandlet readback: compare all saved cut tracks/curves/markers with their owned clean source. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASTransition")
    static FString InspectTransitionAssets(const FString& DestinationRoot);
};
