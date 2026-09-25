#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2SoleDiagnostics.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class FJsonObject;

/** Fixed native render vertex, selected only after inspecting the actual LOD0 probe. */
USTRUCT(BlueprintType)
struct FHCM5VS2SolePoint
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Side;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Region;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VertexIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SectionIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ReferencePosition = FVector::ZeroVector;
};

/** Read-only diagnostics. Never changes mesh, animation, physics, or rendering state. */
UCLASS()
class HARBORCITY_API UHCM5VS2SoleDiagnostics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Candidate geometry only: PASS does not identify or approve a shoe support surface. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Diagnostics")
    static FString InspectShoeGeometry(USkeletalMesh* Mesh);

    static bool ValidateSelection(USkeletalMeshComponent* Mesh,
        const TArray<FHCM5VS2SolePoint>& Points, FString& Failure);

    /** CPU linear blend skinning before morph/material WPO, not the final GPU surface. */
    static TSharedPtr<FJsonObject> CaptureSurface(USkeletalMeshComponent* Mesh,
        const TArray<FHCM5VS2SolePoint>& Points);
};
