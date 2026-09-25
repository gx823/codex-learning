#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "M5VS2/AnimNode_HCM5VS2PlacementInput.h"
#include "AnimGraphNode_HCM5VS2PlacementInput.generated.h"

UCLASS()
class HARBORCITYEDITOR_API UAnimGraphNode_HCM5VS2PlacementInput : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="VS2")
    FAnimNode_HCM5VS2PlacementInput Node;
    virtual FText GetNodeTitle(ENodeTitleType::Type) const override
    { return FText::FromString(TEXT("VS2 candidate placement inputs")); }
    virtual FText GetTooltipText() const override
    { return FText::FromString(TEXT("Unconstrained input-pose world ball speed and separate measured mesh reference plane; no real-bone solve.")); }
protected:
    virtual FText GetControllerDescription() const override
    { return FText::FromString(TEXT("VS2 placement input preparation")); }
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
