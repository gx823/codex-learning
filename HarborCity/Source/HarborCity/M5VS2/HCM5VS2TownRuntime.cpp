#include "HCM5VS2TownRuntime.h"
#include "HCM5VS2LookAnimInstance.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Vehicle.h"
#include "M4/HCM4CombatComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"

AHCM5VS2TownRuntime::AHCM5VS2TownRuntime()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    auto* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Scene);
    AmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HarborAudio"));
    AmbientAudio->SetupAttachment(Scene); AmbientAudio->bAutoActivate = false;
    AmbientAudio->bAllowSpatialization = false; AmbientAudio->SetVolumeMultiplier(.28f);
    MotorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MotorAudio"));
    MotorAudio->SetupAttachment(Scene); MotorAudio->bAutoActivate = false;
    MotorAudio->bAllowSpatialization = false; MotorAudio->SetVolumeMultiplier(0.f);
}
void AHCM5VS2TownRuntime::BeginPlay()
{
    Super::BeginPlay();
    if (HarborAmbience) { AmbientAudio->SetSound(HarborAmbience); AmbientAudio->Play(); }
    if (MotorLoop) { MotorAudio->SetSound(MotorLoop); MotorAudio->Play(); }
}
void AHCM5VS2TownRuntime::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    auto* Character = PC ? PC->GetControlledCharacter() : nullptr;
    if (!PC || !Character || PC->IsPauseMenuOpen() || !PC->IsGameplayFocused()) return;
    if (!Hero.IsValid())
    {
        Hero = Character;
        // Standard UE component, channel 1 only. No renderer callbacks, proxy access or custom shader code.
        HeroFill = NewObject<UPointLightComponent>(Character, TEXT("VS2StandardHeroFill"));
        Character->AddInstanceComponent(HeroFill);
        HeroFill->SetupAttachment(Character->GetRootComponent());
        HeroFill->SetMobility(EComponentMobility::Movable);
        HeroFill->SetIntensityUnits(ELightUnits::Candelas); HeroFill->SetIntensity(.25f);
        HeroFill->SetUseInverseSquaredFalloff(true); HeroFill->SetAttenuationRadius(200.f);
        HeroFill->SetSourceRadius(20.f); HeroFill->SetCastShadows(false);
        HeroFill->SetSpecularScale(0.f); HeroFill->SetIndirectLightingIntensity(0.f);
        HeroFill->SetVolumetricScatteringIntensity(0.f); HeroFill->SetLightingChannels(false,true,false);
        HeroFill->RegisterComponent();
        TArray<USkeletalMeshComponent*> Meshes; Character->GetComponents(Meshes);
        for (auto* Mesh : Meshes) Mesh->SetLightingChannels(true,true,false);
    }
    const FVector Head = Character->GetMesh()->GetSocketLocation(TEXT("Head"));
    HeroFill->SetWorldLocation(Head + Character->GetActorForwardVector()*90.f + Character->GetActorRightVector()*25.f);
    HeroFill->SetVisibility(!Character->IsHidden());
    if (!bAnnounced)
    {
        PC->ShowStatusMessage(TEXT("海湾漫游 · M5-VS2 港町候选　H 操作说明 · T 时段 · F 飞行 · P 暂停"),8.f);
        bAnnounced = true;
    }
    const auto* Vehicle = Cast<AHCM1Vehicle>(PC->GetPawn());
    MotorAudio->SetVolumeMultiplier(Vehicle ? .12f + FMath::Clamp(Vehicle->GetSpeedKmh()/150.f,0.f,.25f) : 0.f);
    MotorAudio->SetPitchMultiplier(Vehicle ? .75f + FMath::Clamp(Vehicle->GetSpeedKmh()/100.f,0.f,1.2f) : 1.f);
    const auto* Combat = Character->GetCombatComponent();
    auto* Anim=Character->GetMesh()->GetAnimInstance();
    const bool CanIdle=Combat&&Anim&&!Character->IsHidden()&&!PC->IsFlying()&&!PC->IsFirstPersonPerspective()
        &&PC->GetPlayerMode()==EHCPlayerMode::OnFoot&&!PC->IsDialogueOpen()
        &&Character->GetCharacterMovement()->IsMovingOnGround()&&Character->GetVelocity().SizeSquared()<4
        &&Character->GetCharacterMovement()->GetCurrentAcceleration().SizeSquared()<1
        &&Combat->GetWeaponMode()==EHCM4WeaponMode::Unarmed&&!Combat->IsAttacking()&&Combat->GetPlayerHealth()>0;
    if(!CanIdle)
    {
        IdleSeconds=0;
        if(Anim&&IdleMontage&&Anim->Montage_IsActive(IdleMontage))Anim->Montage_Stop(.12f,IdleMontage);
        IdleMontage=nullptr;
    }
    else
    {
        IdleSeconds+=DeltaSeconds;
        if(IdleSeconds>7.f&&!IdleGestures.IsEmpty()&&!Anim->IsAnyMontagePlaying())
        {
            auto* Clip=IdleGestures[IdleIndex++%IdleGestures.Num()].Get();
            if(Clip&&Clip->GetSkeleton()==Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
                IdleMontage=Anim->PlaySlotAnimationAsDynamicMontage(Clip,TEXT("FullBody"),.3f,.3f,1.f,1);
            IdleSeconds=0;
        }
    }
    if(auto* Look=Cast<UHCM5VS2LookAnimInstance>(Anim))Look->bTownIdleGestureActive=IdleMontage&&Anim->Montage_IsActive(IdleMontage);
    if (Combat)
    {
        if (Combat->GetShotCount() > LastShot && PistolShot)
        { UGameplayStatics::PlaySound2D(this,PistolShot,.65f); ++AudibleShotCount; }
        LastShot = Combat->GetShotCount();
        if (Combat->IsAttacking() && !bWasAttacking && MeleeWhoosh)
            UGameplayStatics::PlaySound2D(this,MeleeWhoosh,.45f);
        bWasAttacking = Combat->IsAttacking();
    }
    if (PC->GetPlayerMode() == EHCPlayerMode::OnFoot && Character->GetCharacterMovement()->IsMovingOnGround())
    {
        FootstepTravel += Character->GetVelocity().Size2D()*DeltaSeconds;
        if (FootstepTravel > 145.f && Footstep)
        { FootstepTravel = FMath::Fmod(FootstepTravel,145.f); ++FootstepCount; UGameplayStatics::PlaySound2D(this,Footstep,.3f); }
    }
    else FootstepTravel = 0;
}
FString AHCM5VS2TownRuntime::GetTownDiagnostics() const
{
    return FString::Printf(TEXT("{\"ordinary_point_light\":%s,\"ambience_playing\":%s,\"footsteps\":%d,\"audible_shots\":%d}"),
        HeroFill ? TEXT("true") : TEXT("false"), AmbientAudio && AmbientAudio->IsPlaying() ? TEXT("true") : TEXT("false"), FootstepCount, AudibleShotCount);
}
