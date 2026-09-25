#pragma once
#include "CoreMinimal.h"
#include "M3/HCM3NPC.h"
#include "HCM5VS2NPC.generated.h"

class UHCM5VS2NPCFaceComponent;
class UHCM5VS2NPCProfile;

/** Explicit new Q/R Blueprint parent; old NPCs remain AHCM3NPC. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2NPC : public AHCM3NPC
{
    GENERATED_BODY()
public:
    AHCM5VS2NPC();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="M5VS2|NPC") TObjectPtr<UHCM5VS2NPCProfile> NPCProfile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="M5VS2|NPC") TObjectPtr<UHCM5VS2NPCFaceComponent> NPCFace;
    UFUNCTION(BlueprintPure) bool IsVisualProfileReady() const { return bVisualProfileReady; }
    virtual FName GetPhysicalRootBone() const override;
    virtual FName GetCounterHandBone() const override;
    virtual bool ShouldBlockPhysicsBodiesDuringRagdoll() const override;
    virtual bool ShouldInitializeDeathRagdollVelocity() const override { return true; }
    virtual bool SupportsAuthoredGetUp() const override { return true; }
    virtual float GetPreRagdollReactionSeconds() const override { return .18f; }
protected:
    virtual void BeginPlay() override;
    virtual bool ApplyInitialVisuals() override;
private:
    bool bVisualProfileReady = false;
};
