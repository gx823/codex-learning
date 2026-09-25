#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2PhysicsEditor.generated.h"

class UAnimBlueprint;
class USkeletalMesh;
class UBlendSpace;

/** VS2-only authoring. Diagnostics never edit; authoring only edits the exact new ABP. */
UCLASS()
class HARBORCITY_API UHCM5VS2PhysicsEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Inspect the actual imported reference pose and Kawaii reflection schema before
     * approving a source PhysBone mapping. SourceAuditJson is the contents of the
     * local SELESTIA_SOURCE_AUDIT.json; PrefabPath selects one exact prefabs[].path.
     * Dot-to-underscore matches are candidates only, never automatic assignments.
     * The caller writes the returned JSON. No asset is changed, compiled or saved.
     */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|PhysicsAuthoring")
    static FString InspectPhysicsSetup(USkeletalMesh* Target, UAnimBlueprint* Blueprint,
        const FString& SourceAuditJson, const FString& PrefabPath);

    /** Configure the isolated VS2 copy, compile and return reflected readback. Never saves. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|PhysicsAuthoring")
    static FString ApplyPhysicsSetup(USkeletalMesh* Target, UAnimBlueprint* Blueprint,
        const FString& ConfigJson);

    /** Read back installed VS2 nodes without modifying or compiling the asset. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|PhysicsAuthoring")
    static FString ReadPhysicsSetup(UAnimBlueprint* Blueprint);
    /** Enable only the two reviewed long-hair chains on a new private hero ABP. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|PhysicsAuthoring")
    static FString AnchorPrivateHeroHair(UAnimBlueprint* Blueprint);

    /** Exact VS2 ABP graph, runtime node settings and pin links; read-only, no compile. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|LocomotionAuthoring")
    static FString ReadLocomotionGraph(UAnimBlueprint* Blueprint);

    /** Read exact original/candidate BlendSpace samples and native fixed axis arrays. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|LocomotionAuthoring")
    static FString ReadLocomotionBlendSpace(UBlendSpace* BlendSpace);

    /** Change only three sample animation references and one VS2 player. Compile, never save. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|LocomotionAuthoring")
    static FString ApplyLocomotionSamples(UAnimBlueprint* Blueprint, UBlendSpace* Source,
        UBlendSpace* Target, const FString& ChangesJson);
};
