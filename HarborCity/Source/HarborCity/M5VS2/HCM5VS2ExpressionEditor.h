#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2ExpressionEditor.generated.h"

class UBlueprint;
class UAnimBlueprint;
class USkeletalMesh;

/** Native VS2-only authoring. Caller backs up and saves; no implicit package or map writes. */
UCLASS()
class HARBORCITY_API UHCM5VS2ExpressionEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|ExpressionAuthoring")
    static FString InstallHeroExpressionComponent(UBlueprint* Blueprint);

    /** Run after Physics authoring. Adds Head/LeftEye/RightEye before the first Kawaii node. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|ExpressionAuthoring")
    static FString ConfigureLookGraph(UAnimBlueprint* Blueprint, USkeletalMesh* Mesh);
};
