#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2NPCEditor.generated.h"

class UHCM5VS2NPCProfile;
class UAnimBlueprint;
class UAnimSequence;
class USkeleton;
class USkeletalMesh;
class UIKRetargeter;
class UPhysicsAsset;

UCLASS()
class HARBORCITY_API UHCM5VS2NPCEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Source slot membership is copied only to a new M5VS2 NPC skeleton. Does not save. */
    UFUNCTION(BlueprintCallable) static bool CopyNPCSlots(USkeleton* Source, USkeleton* Target);
    /** Three Q/R locomotion clips only. Full-frame native replay before removing source-root XY; no saves. */
    UFUNCTION(BlueprintCallable) static FString RepairNPCHorizontalRootTravel(UAnimSequence* Source,
        UAnimSequence* Target, USkeletalMesh* TargetMesh, UObject* RetargeterObject, bool bApply);
    /** Detailed read-only physics probe or bounded anatomy candidate. Apply never saves; script binds a prior probe. */
    UFUNCTION(BlueprintCallable) static FString InspectOrRepairNPCPhysics(UHCM5VS2NPCProfile* Profile,
        UPhysicsAsset* Physics, bool bApply);
    /** Native locomotion, reaction slot, mapped look graph and humanoid physics. Fresh new namespace only; no saves. */
    UFUNCTION(BlueprintCallable) static FString BuildNPCPresentation(UHCM5VS2NPCProfile* Profile,
        UAnimBlueprint* Blueprint, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run,
        const FString& Destination);
};
