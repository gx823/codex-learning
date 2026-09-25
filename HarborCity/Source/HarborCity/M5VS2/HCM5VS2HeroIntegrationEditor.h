#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2HeroIntegrationEditor.generated.h"
class USkeletalMesh;

/** Bounded editor bridge: UE's USkeletalMesh::SetSkeleton is not Python reflected. */
UCLASS()
class HARBORCITY_API UHCM5VS2HeroIntegrationEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="HarborCity|VS2|Hero")
    static FString BindPrivateArmsSkeleton(USkeletalMesh* SourceArms,USkeletalMesh* PrivateArms,
        USkeletalMesh* BodyMesh,bool Apply);
};
