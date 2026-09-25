#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2NPCJClothingEditor.generated.h"

class USkeletalMesh;
class UPhysicsAsset;
class UAnimBlueprint;
class UVrmMetaObject;

/** Editor-only private J cloth candidate. Does not save, mutate source or run. */
UCLASS()
class HARBORCITYEDITOR_API UHCM5VS2NPCJClothingEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|PrivateJCloth")
    static FString ProbeSource(USkeletalMesh* SourceMesh, UPhysicsAsset* BodyPhysics);

    /** Read-only: exact seven private compatibility copies, graph/parameter equivalence and Clothing usage. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|PrivateJCloth")
    static FString ValidatePrivateMaterials(USkeletalMesh* SourceMesh, USkeletalMesh* CandidateMesh);

    /** Apply only: retain identical texture sampling metadata after private usage recompilation. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|PrivateJCloth")
    static FString CopyPrivateStreamingData(USkeletalMesh* SourceMesh, USkeletalMesh* CandidateMesh);

    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|PrivateJCloth")
    static FString BuildPrivateClothing(USkeletalMesh* SourceMesh, USkeletalMesh* CandidateMesh, UPhysicsAsset* BodyPhysics, bool bApply);

    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|PrivateJCloth")
    static FString ConfigurePrivateSpring(USkeletalMesh* CandidateMesh, UVrmMetaObject* SourceMeta, UVrmMetaObject* CandidateMeta, UAnimBlueprint* SourcePost, UAnimBlueprint* CandidatePost, bool bApply);
};
