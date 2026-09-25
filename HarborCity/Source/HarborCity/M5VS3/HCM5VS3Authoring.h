#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS3Authoring.generated.h"
class UAnimSequence;
UCLASS()
class HARBORCITY_API UHCM5VS3Authoring : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) static FString RepairStandingStarts(UObject* WorldContextObject);
    UFUNCTION(BlueprintCallable) static FString AuthorCombatPoses(UAnimSequence* Source,const TArray<UAnimSequence*>& Targets);
    UFUNCTION(BlueprintCallable) static bool AuthorFallPose(UAnimSequence* GetUp,UAnimSequence* Target);
};
