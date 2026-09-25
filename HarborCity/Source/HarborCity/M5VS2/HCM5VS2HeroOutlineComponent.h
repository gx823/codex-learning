#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2HeroOutlineComponent.generated.h"

class USkeletalMeshComponent;
class UMaterialInterface;
class UBlueprint;

/** VS2 opt-in second material pass. The original mesh remains the only pose/physics authority. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2HeroOutlineComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2HeroOutlineComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outline") TArray<TObjectPtr<UMaterialInterface>> OutlineMaterials;
    UFUNCTION(BlueprintPure, Category="Outline") FString GetOutlineDiagnostics() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Source;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Outline;
};

UCLASS()
class HARBORCITY_API UHCM5VS2HeroOutlineEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Caller owns backup, native save and reload. Only a fresh VS2 HeroRev2 review BP is writable. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|M5VS2|Authoring")
    static FString ConfigureOutline(UBlueprint* Blueprint, const TArray<UMaterialInterface*>& Materials);
};
