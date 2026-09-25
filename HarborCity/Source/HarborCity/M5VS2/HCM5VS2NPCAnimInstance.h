#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HCM5VS2NPCAnimInstance.generated.h"

UCLASS(Transient, Blueprintable)
class HARBORCITY_API UHCM5VS2NPCAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly) float NPCGroundSpeed = 0;
    UPROPERTY(BlueprintReadOnly) FVector NPCLookTarget = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float NPCHeadLookAlpha = 0;
    UPROPERTY(BlueprintReadOnly) float NPCEyeLookAlpha = 0;
private:
    FVector SmoothedDirection = FVector::ForwardVector;
    bool bDirectionInitialized = false;
};
