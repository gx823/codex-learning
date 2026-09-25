#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2GASAnimationEditor.generated.h"

class UAnimSequence;

/** One fixed GAS P0 selection; no changes to historical Quinn/Selestia helpers. */
UCLASS()
class HARBORCITY_API UHCM5VS2GASAnimationEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Read original GAS through scoped, collision-checked submounts; optionally save seven new clean copies. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASAuthoring")
    static FString PrepareSourceP0(const FString& ProbeJsonPath, bool bApply);

    /** Make five separate working clips; only their independent floor-root X/Y travel is removed. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASAuthoring")
    static FString CreateInPlaceP0(bool bApply);

    /** Exact native data-model trajectory summary on the five permitted source/working/target clips. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASAuthoring")
    static FString InspectSequenceP0(UAnimSequence* Sequence);

    /** Exact four installed getups plus Relaxed Sprint. Reuses existing clean P0 mesh/skeleton; five fresh sequences only. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASAuthoring")
    static FString PrepareRecoverySource(const FString& ProbeJsonPath, const FString& DestinationRoot,
        const FString& PriorDryRunJsonPath, bool bApply);

    /** One separately owned Sprint working copy; getup root trajectories and old P0 are never modified. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|GASAuthoring")
    static FString CreateSprintInPlace(const FString& DestinationRoot, bool bApply);
};
