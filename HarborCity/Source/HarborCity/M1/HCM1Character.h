#pragma once

#include "CoreMinimal.h"
#include "HarborCityCharacter.h"
#include "HCM1Character.generated.h"

class UHCM4CombatComponent;
class UHCM5VS3Abilities;
class UHCM5VS2FlightComponent;
class UHCM4R2PresentationComponent;

/** M1 derives from the unmodified official third-person character. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM1Character : public AHarborCityCharacter
{
    GENERATED_BODY()
public:
    AHCM1Character();
    UHCM5VS3Abilities* GetAbilities() const { return Abilities; }
    UFUNCTION(BlueprintPure, Category="HarborCity|Flight") UHCM5VS2FlightComponent* GetFlightComponent() const { return Flight; }
    UHCM4R2PresentationComponent* GetR2PresentationComponent() const { return R2Presentation; }
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure, Category="HarborCity|M4") UHCM4CombatComponent* GetCombatComponent() const { return Combat; }
    void SetCombatMovementScale(float Scale);
    /** Body alignment only: never writes the controller or camera rotation. */
    void FaceBodyYawOnce(float Yaw, float MaxSeconds = .7f);
    void StopBodyFacing();

    UFUNCTION(BlueprintCallable, Category="HarborCity|M1")
    void SetSprinting(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category="HarborCity|M1")
    void SetSeated(bool bSeated);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HarborCity|M1")
    float WalkSpeed = 400.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HarborCity|M1")
    float SprintSpeed = 650.0f;

protected:
    // The single M1 controller binds both movement modes. Do not bind the
    // template's Blueprint-only InputActions, which are null on this native CDO.
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHCM5VS3Abilities> Abilities;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|Flight", meta=(AllowPrivateAccess="true")) TObjectPtr<UHCM5VS2FlightComponent> Flight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHCM4R2PresentationComponent> R2Presentation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HarborCity|M4", meta=(AllowPrivateAccess="true")) TObjectPtr<UHCM4CombatComponent> Combat;
    bool bSprinting = false;
    float CombatMovementScale = 1;
    bool bFacingBody = false;
    bool bPreviousOrientToMovement = true;
    float BodyTargetYaw = 0;
    double BodyFacingUntil = 0;
};
