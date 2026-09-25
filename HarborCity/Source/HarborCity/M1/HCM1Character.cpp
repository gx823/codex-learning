#include "HCM1Character.h"
#include "M5VS3/HCM5VS3Abilities.h"
#include "M5VS2/HCM5VS2FlightComponent.h"
#include "M4R2/HCM4R2PresentationComponent.h"
#include "HCM1PlayerController.h"
#include "M4/HCM4CombatComponent.h"
#include "M4/HCM4Facing.h"
#include "M4/HCM4R1ReactionComponent.h"
#include "Engine/DamageEvents.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AHCM1Character::AHCM1Character()
{
    Abilities=CreateDefaultSubobject<UHCM5VS3Abilities>(TEXT("VS3Abilities"));
    Flight = CreateDefaultSubobject<UHCM5VS2FlightComponent>(TEXT("VS2Flight"));
    R2Presentation = CreateDefaultSubobject<UHCM4R2PresentationComponent>(TEXT("R2Presentation"));
    Combat = CreateDefaultSubobject<UHCM4CombatComponent>(TEXT("M4Combat"));
    // Values inspected from the original BP_ThirdPersonCharacter CDO in UE 5.8.2.
    GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -89));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Unarmed(
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    if (Quinn.Succeeded()) GetMesh()->SetSkeletalMesh(Quinn.Object);
    if (Unarmed.Succeeded()) GetMesh()->SetAnimInstanceClass(Unarmed.Class);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCameraBoom()->bDoCollisionTest = true;
    GetCameraBoom()->ProbeSize = 15.0f;
}

void AHCM1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // Input belongs to AHCM1PlayerController; inherited DoMove/DoLook are reused.
}

float AHCM1Character::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!Combat || !DamageEvent.DamageTypeClass || !DamageEvent.DamageTypeClass->IsChildOf(UHCM4R1CounterDamageType::StaticClass())) return 0;
    const float Accepted = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    return Combat->ReceiveNPCPunch(Accepted, DamageCauser);
}

void AHCM1Character::SetSprinting(bool bEnabled)
{
    bSprinting = bEnabled;
    GetCharacterMovement()->MaxWalkSpeed = (bEnabled ? SprintSpeed : WalkSpeed) * CombatMovementScale;
}

void AHCM1Character::SetCombatMovementScale(float Scale)
{
    CombatMovementScale = FMath::Clamp(Scale, .1f, 1.f);
    GetCharacterMovement()->MaxWalkSpeed = (bSprinting ? SprintSpeed : WalkSpeed) * CombatMovementScale;
}

void AHCM1Character::FaceBodyYawOnce(float Yaw, float MaxSeconds)
{
    if (!GetWorld() || !FMath::IsFinite(Yaw)) return;
    if (!bFacingBody) bPreviousOrientToMovement = GetCharacterMovement()->bOrientRotationToMovement;
    bFacingBody = true; BodyTargetYaw = Yaw;
    BodyFacingUntil = GetWorld()->GetTimeSeconds() + FMath::Max(.05f, MaxSeconds);
    GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AHCM1Character::StopBodyFacing()
{
    if (bFacingBody) GetCharacterMovement()->bOrientRotationToMovement = bPreviousOrientToMovement;
    bFacingBody = false;
}

void AHCM1Character::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const AHCM1PlayerController* LocalPC = Cast<AHCM1PlayerController>(GetController());
    if ((!Flight || !Flight->IsFlying()) && LocalPC && LocalPC->GetPlayerMode() == EHCPlayerMode::OnFoot && LocalPC->IsFirstPersonPerspective()
        && !LocalPC->IsPauseMenuOpen() && !LocalPC->IsDialogueOpen() && Combat && Combat->GetPlayerHealth() > 0)
        FaceBodyYawOnce(LocalPC->GetControlRotation().Yaw, .2f);
    if (bFacingBody)
    {
        const float Next = HCM4Facing::StepYaw(GetActorRotation().Yaw, BodyTargetYaw, DeltaSeconds, 360.f);
        SetActorRotation(FRotator(0,Next,0));
        if (FMath::Abs(FMath::FindDeltaAngleDegrees(Next, BodyTargetYaw)) < 1.f || GetWorld()->GetTimeSeconds() >= BodyFacingUntil)
            StopBodyFacing();
    }
}

void AHCM1Character::SetSeated(bool bSeated)
{
    if (Flight) Flight->ResetForGroundTransition();
    StopBodyFacing();
    StopJumping();
    SetSprinting(false);
    GetCharacterMovement()->StopMovementImmediately();
    if (bSeated) GetCharacterMovement()->DisableMovement();
    SetActorHiddenInGame(bSeated);
    SetActorEnableCollision(!bSeated);
    if (!bSeated) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (Combat) Combat->SetSeated(bSeated);
}
