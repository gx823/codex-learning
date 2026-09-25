#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2CornerTimeDirector.generated.h"
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/** Permanent VS2 corner lighting, shared by playable world and evidence views. No camera-specific exposure. */
UCLASS()
class HARBORCITY_API AHCM5VS2CornerTimeDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2CornerTimeDirector();
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|Time") bool SetTimeOfDay(FName Period);
    UFUNCTION(BlueprintPure, Category="HarborCity|VS2|Time") FString GetLightingDiagnostics() const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Time") FName InitialPeriod = TEXT("Afternoon");
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Time") FName CurrentPeriod;
protected:
    virtual void BeginPlay() override;
private:
    bool BindWorld();
    UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Sun;
    UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Moon;
    UPROPERTY(Transient) TObjectPtr<ASkyLight> Sky;
    UPROPERTY(Transient) TObjectPtr<APostProcessVolume> Post;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DomeMaterial;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> NightLights;
    TArray<float> NightIntensities;
    bool bBound = false;
};
