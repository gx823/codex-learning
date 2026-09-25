#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2HeroModestyComponent.generated.h"
class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;
class UBlueprint;
/** Opt-in private garment follows the current actual body bones; no simulation or camera changes. */
UCLASS(ClassGroup=(HarborCity),meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2HeroModestyComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2HeroModestyComponent();
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Modesty") TObjectPtr<USkeletalMesh> SafetyShorts;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Modesty") TObjectPtr<UMaterialInterface> ClothMaterial;
    UFUNCTION(BlueprintPure,Category="Modesty") FString GetModestyDiagnostics() const;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Garment;
};
UCLASS()
class HARBORCITY_API UHCM5VS2HeroModestyEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Probe does not mutate. Source must be current native GASMotion body. */
    UFUNCTION(BlueprintCallable,Category="HarborCity|Editor") static FString ProbeBody(USkeletalMesh* Source);
    /** Target is a fresh duplicate in HeroModesty/Review_<12hex>; never saves itself. */
    UFUNCTION(BlueprintCallable,Category="HarborCity|Editor") static FString BuildSafetyShorts(USkeletalMesh* Source,USkeletalMesh* Target,UMaterialInterface* Material);
    UFUNCTION(BlueprintCallable,Category="HarborCity|Editor") static FString InspectSafetyShorts(USkeletalMesh* Source,USkeletalMesh* Target);
    UFUNCTION(BlueprintCallable,Category="HarborCity|Editor") static FString ConfigureModesty(UBlueprint* Blueprint,USkeletalMesh* Shorts,UMaterialInterface* Material);
};
