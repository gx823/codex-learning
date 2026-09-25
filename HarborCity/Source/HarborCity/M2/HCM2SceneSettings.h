#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM2SceneSettings.generated.h"

class APlayerStart;
class ADirectionalLight;
class ASkyLight;
class AHCM1Vehicle;
class AHCM1LightSwitch;

USTRUCT(BlueprintType)
struct FHCM2ReviewView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator ControlRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDusk = false;
};

/** References saved scene actors; never generates geometry or drives the player. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM2SceneSettings : public AActor
{
    GENERATED_BODY()
public:
    AHCM2SceneSettings();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Scene")
    FName SceneId = TEXT("M2_SeafrontStreet");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Scene")
    FName MapName = TEXT("L_M2_SeafrontStreet");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Scene")
    FString SaveSlot = TEXT("HarborCity_M2_V1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Scene")
    bool bExternalTimeOfDayController = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Scene")
    FName PlayerId = TEXT("M2_Player");

    // Actor transforms in the map are the sole spawn/reset coordinate source.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TObjectPtr<APlayerStart> PlayerStart;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TObjectPtr<AHCM1Vehicle> MainVehicle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TObjectPtr<AHCM1LightSwitch> InteriorSwitch;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TObjectPtr<ADirectionalLight> Sun;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TObjectPtr<ASkyLight> SkyLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TArray<TObjectPtr<AActor>> EveningLightActors;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|References") TArray<TObjectPtr<AActor>> EveningEmissionActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Afternoon") FRotator AfternoonSunRotation = FRotator(-32, -35, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Afternoon", meta=(ClampMin="0")) float AfternoonSunIntensity = 20000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Afternoon") FLinearColor AfternoonSunColor = FLinearColor(1.0f, 0.94f, 0.82f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Afternoon", meta=(ClampMin="0")) float AfternoonSkyIntensity = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Afternoon", meta=(ClampMin="0")) float AfternoonStreetIntensity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Dusk") FRotator DuskSunRotation = FRotator(-6, -35, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Dusk", meta=(ClampMin="0")) float DuskSunIntensity = 1800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Dusk") FLinearColor DuskSunColor = FLinearColor(1.0f, 0.61f, 0.34f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Dusk", meta=(ClampMin="0")) float DuskSkyIntensity = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Dusk", meta=(ClampMin="0")) float DuskStreetIntensity = 1500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Dusk") bool bStartAtDusk = false;

    // Test-only data. Normal play never traverses or teleports to these targets.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Review") TArray<FVector> TestDriveWaypoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Review") TArray<FVector> TestWalkToCafeWaypoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|Review") TArray<FHCM2ReviewView> ReviewViews;

    UFUNCTION(BlueprintPure, Category="HarborCity|Scene") bool IsDusk() const { return bDusk; }
    UFUNCTION(BlueprintPure, Category="HarborCity|Scene") bool HasValidSaveIdentity() const;
    UFUNCTION(BlueprintCallable, Category="HarborCity|Scene") void SetDuskPreset(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="HarborCity|Scene") void RequestTogglePreset();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool bDusk = false;
};
