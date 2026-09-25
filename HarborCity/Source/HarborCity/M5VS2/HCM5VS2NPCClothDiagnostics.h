#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2NPCClothDiagnostics.generated.h"

class USkeletalMeshComponent;
class FJsonObject;

/** Read-only J garment probe. Does not author assets, modify node values or add physics. */
UCLASS()
class HARBORCITY_API UHCM5VS2NPCClothDiagnostics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Diagnostics")
    static FString InspectJComponent(USkeletalMeshComponent* Mesh);

    /** Actual post-process instance and sparse current CPU-LBS cloth surface, at request time. */
    static TSharedPtr<FJsonObject> CaptureJ(USkeletalMeshComponent* Mesh);
    /** Final Chaos particles after the fixture's cloth tick. Does not use CPU LBS. */
    static TSharedPtr<FJsonObject> CaptureChaosJ(USkeletalMeshComponent* Mesh);
};
