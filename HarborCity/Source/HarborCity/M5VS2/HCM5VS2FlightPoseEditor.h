#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2FlightPoseEditor.generated.h"
class UAnimSequence;
class UAnimBlueprint;
UCLASS()
class HARBORCITY_API UHCM5VS2FlightPoseEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Five fresh copies: Hover,Forward,Boost,Ascending,Descending. Never saves. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Flight")
    static FString AuthorFlightLoops(UAnimSequence* SourceIdle,const TArray<UAnimSequence*>& Loops,bool bApply,bool bReadbackOnly = false);
    /** New copy of an authored GASMotion ABP; existing physics/FP/combat retained. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Flight")
    static FString ConfigureFlightGraph(UAnimBlueprint* Candidate,const TArray<UAnimSequence*>& Loops,
        UAnimSequence* Takeoff,UAnimSequence* Landing);
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Flight")
    static FString InspectFlightGraph(UAnimBlueprint* Candidate);
};
