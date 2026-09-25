#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "HCM5VS3Abilities.generated.h"
class AHCM1Character;
class UHCM4CombatComponent;
class UStaticMeshComponent;
class UAnimMontage;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FHCM4AcceptedShot;

/** Local single-player VS3 abilities. Existing combat remains the damage authority. */
UCLASS(ClassGroup=(HarborCity),meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS3Abilities : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS3Abilities();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
    bool IsEnabled() const { return bEnabled; }
    bool IsBusy() const { return bHeld || SwingTime>0 || CastKind>=0; }
    bool PressSword();
    void ReleaseSword();
    bool CastSpell(int32 Kind);
    void WeaponChanged();
    void ResetTransient();
    float GetMP() const { return MP; }
    float GetDamageMultiplier() const { return ShieldRemaining>0?.5f:1.f; }
    float GetCooldown(int32 Kind) const { return Kind>=0&&Kind<4?Cooldown[Kind]:0; }
    FString GetHUDText() const;
private:
    UPROPERTY() TObjectPtr<AHCM1Character> Hero;
    UPROPERTY() TObjectPtr<UHCM4CombatComponent> Combat;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Sword;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FirstSword;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MagicGun;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FirstMagicGun;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> CastCircle;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> RuneLayers;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> RuneMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> RuneSurfaces;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> RuneInstances;
    float RuneAge=0,RuneCastDuration=.7f;
    int32 RuneKind=-1;
    UPROPERTY() TObjectPtr<UAnimMontage> SwingMontage;
    bool bEnabled=false,bHeld=false,bHeavy=false,bBladeSample=false,bContact=false;
    float MP=100,IdleMP=0,HoldTime=0,SwingTime=0,SwingDuration=.75f,LastSwingAge=100,HealRemaining=0,ShieldRemaining=0,CastTime=0;
    float Cooldown[4]={0,0,0,0};
    int32 Combo=-1,CastKind=-1;
    FVector PreviousBase,PreviousTip;
    TSet<TWeakObjectPtr<AActor>> SwingHits;
    void StartSwing(bool Heavy);
    void SwordTrace();
    void ReleaseSpell();
    void UpdateVisuals(float Dt);
    void AcceptedShot(const FHCM4AcceptedShot& Shot);
    void PlayPose(const TCHAR* Name,float Duration);
};

/** Bounded cosmetic/projectile actor; gun bolts never apply a second hit. */
UCLASS()
class HARBORCITY_API AHCM5VS3SpellFX : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS3SpellFX();
    virtual void Tick(float Dt) override;
    void Initialize(AHCM1Character* Source,const FVector& Velocity,float Damage,float Life,int32 Kind,bool bProjectile);
    static AHCM5VS3SpellFX* Spawn(AHCM1Character* Source,const FVector& Where,const FVector& Velocity,float Damage,float Life,int32 Kind,bool bProjectile=true);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    TWeakObjectPtr<AHCM1Character> Hero;
    FVector Travel;
    float HitDamage=0,Remaining=0,Age=0;
    int32 Style=0;
    bool bTravel=false;
};
