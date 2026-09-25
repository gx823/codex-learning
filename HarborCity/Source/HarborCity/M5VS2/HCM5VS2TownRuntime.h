#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2TownRuntime.generated.h"
class UAudioComponent;
class UPointLightComponent;
class USoundWave;
class AHCM1Character;
class UHCM4R2MapData;
class UAnimSequence;
class UAnimMontage;

/** Saved opt-in actor for the harbor: ordinary engine light and audible world feedback. */
UCLASS()
class HARBORCITY_API AHCM5VS2TownRuntime : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2TownRuntime();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundWave> HarborAmbience;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundWave> Footstep;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundWave> PistolShot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundWave> MeleeWhoosh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundWave> MotorLoop;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UHCM4R2MapData> NavigationMap;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UAnimSequence>> IdleGestures;
    UFUNCTION(BlueprintPure) FString GetTownDiagnostics() const;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> HeroFill;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> AmbientAudio;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> MotorAudio;
    UPROPERTY(Transient) TWeakObjectPtr<AHCM1Character> Hero;
    int32 LastShot = 0, FootstepCount = 0, AudibleShotCount = 0;
    bool bWasAttacking = false, bAnnounced = false;
    float FootstepTravel = 0;
    float IdleSeconds = 0;
    int32 IdleIndex = 0;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> IdleMontage;
};
