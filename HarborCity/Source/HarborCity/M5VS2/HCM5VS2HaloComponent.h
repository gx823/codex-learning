#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2HaloComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UBlueprint;

/** The seven purchased source pieces retain their original geometry/rest placement. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2HaloComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2HaloComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTick) override;
    UPROPERTY(EditAnywhere, Category="Halo") TArray<TObjectPtr<UStaticMesh>> Pieces;
    UPROPERTY(EditAnywhere, Category="Halo") TArray<float> SourceDegreesPerSecond;
    UPROPERTY(EditAnywhere, Category="Halo") TObjectPtr<UMaterialInterface> GlowMaterial;
    UPROPERTY(EditAnywhere, Category="Halo") FTransform SourceHeadTransform;
    UFUNCTION(BlueprintPure, Category="Halo") FString GetHaloDiagnostics() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> RenderPieces;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glow;
    double AnimationSeconds=0;
    FVector SmoothedAnchor=FVector::ZeroVector;
    bool bAnchorInitialized=false;
    float Brightness=1.f;
};

UCLASS()
class HARBORCITY_API UHCM5VS2HaloEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|Authoring")
    static FString ConfigureHalo(UBlueprint* Blueprint,const TArray<UStaticMesh*>& Pieces,
        const TArray<float>& SourceDegreesPerSecond,UMaterialInterface* Material,const FTransform& SourceHeadTransform);
};
