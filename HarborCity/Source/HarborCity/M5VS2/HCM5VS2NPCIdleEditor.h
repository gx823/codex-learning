#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2NPCIdleEditor.generated.h"
class UAnimBlueprint;
class UBlendSpace1D;
class UAnimSequence;

/** Bounded editor bridge for the actual protected AnimGraph node array. */
UCLASS()
class HARBORCITY_API UHCM5VS2NPCIdleEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="HarborCity|VS2|NPC")
    static FString InspectIdleGraph(UAnimBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable,Category="HarborCity|VS2|NPC")
    static FString ConfigurePrivateIdle(UAnimBlueprint* SourceBlueprint,UAnimBlueprint* PrivateBlueprint,
        UBlendSpace1D* PrivateSpace,UAnimSequence* PrivateIdle,bool Apply);
    /** Original restrained upper-body choreography over the actual retargeted idle; never saves. */
    UFUNCTION(BlueprintCallable,Category="HarborCity|VS2|NPC")
    static FString AuthorSceneIdle(UAnimSequence* SourceIdle,UAnimSequence* PrivateIdle,FName Activity);
};
