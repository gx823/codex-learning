#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2FlightVisualComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UBlueprint;
class UHCM5VS2FlightComponent;

/** Bounded original flight VFX only. No movement, camera, collision or bone writes. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2FlightVisualComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2FlightVisualComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTick) override;
    /** Authored centimeter meshes: feather X=0..100, ribbon X=0..100, rune radius=100. */
    UPROPERTY(EditAnywhere, Category="FlightVisual") TObjectPtr<UStaticMesh> FeatherMesh;
    UPROPERTY(EditAnywhere, Category="FlightVisual") TObjectPtr<UStaticMesh> RibbonMesh;
    UPROPERTY(EditAnywhere, Category="FlightVisual") TObjectPtr<UStaticMesh> RuneMesh;
    /** Unlit two-sided material with GlowGain / OpacityGain scalar parameters. */
    UPROPERTY(EditAnywhere, Category="FlightVisual") TObjectPtr<UMaterialInterface> LightMaterial;
    /** Optional feather-only material. Null preserves the complete original effect. */
    UPROPERTY(EditAnywhere, Category="FlightVisual") TObjectPtr<UMaterialInterface> WingMaterial;
    UFUNCTION(BlueprintPure, Category="FlightVisual") FString GetFlightVisualDiagnostics() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2FlightComponent> Flight;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Feathers;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Ribbons;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Rune;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WingGlow;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WindGlow;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RuneGlow;
    float VisualSeconds=0, WingAlpha=0, WindAlpha=0, RuneAge=100;
    bool bWasFlying=false, bFirstPersonHidden=true;
    int32 VisibleFeathers=0,VisibleRibbons=0;
    FTransform RuneTransform;
    UStaticMeshComponent* MakePiece(UStaticMesh* Shape,UMaterialInterface* Material,FName Name);
    void StartRune();
};

UCLASS()
class HARBORCITY_API UHCM5VS2FlightVisualEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Fresh HeroRev2/Review_* blueprint only; configure, compile, leave caller to save. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|VS2|FlightVisual")
    static FString ConfigureFlightVisual(UBlueprint* HeroCandidate,UStaticMesh* Feather,
        UStaticMesh* Ribbon,UStaticMesh* Rune,UMaterialInterface* Material);
};
