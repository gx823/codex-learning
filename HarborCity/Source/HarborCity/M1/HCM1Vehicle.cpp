#include "HCM1Vehicle.h"
#include "HCM1Wheel.h"
#include "HCM1PlayerController.h"
#include "HCM1CameraBoom.h"
#include "HCM1CameraDiagnostics.h"
#include "M4/HCM4R1SteeringMovement.h"
#include "M4R1/HCM4R1VehicleImpactComponent.h"
#include "M4R2/HCM4R2CockpitComponent.h"
#include "M3/HCM3NPC.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/RotationMatrix.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    const FName HCM1WheelBones[] = {
        TEXT("Phys_Wheel_FL"), TEXT("Phys_Wheel_FR"),
        TEXT("Phys_Wheel_BL"), TEXT("Phys_Wheel_BR")
    };

    bool IsUsableGround(const FHitResult& Hit, float MinimumNormalZ)
    {
        return Hit.bBlockingHit && !Hit.bStartPenetrating &&
            Hit.ImpactNormal.Z >= MinimumNormalZ &&
            !Cast<APawn>(Hit.GetActor());
    }

    FString JsonNumber(double Value)
    {
        return FMath::IsFinite(Value) ? FString::SanitizeFloat(Value) : TEXT("null");
    }

    FString JsonVector(const FVector& Value)
    {
        return FString::Printf(TEXT("[%s,%s,%s]"),
            *JsonNumber(Value.X), *JsonNumber(Value.Y), *JsonNumber(Value.Z));
    }
}

AHCM1Vehicle::AHCM1Vehicle(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UHCM4R1SteeringMovement>(VehicleMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    Cockpit = CreateDefaultSubobject<UHCM4R2CockpitComponent>(TEXT("R2Cockpit"));
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AutoPossessAI = EAutoPossessAI::Disabled;
    bUseControllerRotationYaw = false;

    // These are Unreal asset references retained by cooking, not filesystem paths.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SkeletonMesh(
        TEXT("/Game/SportsCar/SKM_SportsCar.SKM_SportsCar"));
    static ConstructorHelpers::FObjectFinder<UPhysicsAsset> PhysicsAsset(
        TEXT("/Game/SportsCar/PA_SportsCar.PA_SportsCar"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Animation(
        TEXT("/Game/SportsCar/ABP_SportsCar"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(
        TEXT("/Game/SportsCar/SM_SportsCar.SM_SportsCar"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassAsset(
        TEXT("/Game/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelAsset(
        TEXT("/Game/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
    static ConstructorHelpers::FObjectFinder<UCurveFloat> TorqueAsset(
        TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/FC_Torque_SportsCar.FC_Torque_SportsCar"));

    GetMesh()->SetSkeletalMesh(SkeletonMesh.Object);
    GetMesh()->SetPhysicsAsset(PhysicsAsset.Object);
    GetMesh()->SetAnimInstanceClass(Animation.Class);
    GetMesh()->SetCollisionProfileName(TEXT("Vehicle"));
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->SetGenerateOverlapEvents(true);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(GetMesh());
    BodyVisual->SetStaticMesh(BodyAsset.Object);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetCanEverAffectNavigation(false);

    GlassVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlassVisual"));
    GlassVisual->SetupAttachment(GetMesh(), TEXT("Root"));
    GlassVisual->SetStaticMesh(GlassAsset.Object);
    GlassVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GlassVisual->SetCanEverAffectNavigation(false);

    const TCHAR* WheelComponentNames[] = {
        TEXT("WheelFLVisual"), TEXT("WheelFRVisual"), TEXT("WheelBLVisual"), TEXT("WheelBRVisual")
    };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(WheelComponentNames[Index]);
        Wheel->SetupAttachment(GetMesh(), HCM1WheelBones[Index]);
        Wheel->SetStaticMesh(WheelAsset.Object);
        const bool bLeftWheel = Index == 0 || Index == 2;
        Wheel->SetRelativeRotation(FRotator(0.0f, bLeftWheel ? -90.0f : 90.0f, 0.0f));
        Wheel->SetRelativeScale3D(bLeftWheel ? FVector(-1.0f, 1.0f, 1.0f) : FVector::OneVector);
        Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Wheel->SetCanEverAffectNavigation(false);
        WheelVisuals.Add(Wheel);
    }

    CameraBoom = CreateDefaultSubobject<UHCM1CameraBoom>(TEXT("DrivingCameraBoom"));
    CameraBoom->SetupAttachment(GetMesh());
    CameraBoom->TargetArmLength = 650.0f;
    CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 145.0f);
    CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bInheritYaw = true;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->ProbeSize = 16.0f;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 8.0f;
    CameraBoom->CameraLagMaxDistance = 60.0f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 5.0f;
    DrivingCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DrivingCamera"));
    DrivingCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    DrivingCamera->FieldOfView = 85.0f;
    DrivingCamera->bUsePawnControlRotation = false;
    CreateDefaultSubobject<UHCM1CameraDiagnostics>(TEXT("CameraDiagnostics"));
    NPCImpact = CreateDefaultSubobject<UHCM4R1VehicleImpactComponent>(TEXT("NPCImpact"));

    UChaosWheeledVehicleMovementComponent* Movement = GetChaosMovement();
    Movement->SetRequiresControllerForInputs(false); // Parking brakes must work after unpossessing.
    Movement->bReverseAsBrake = false;
    Movement->bThrottleAsBrake = false;
    Movement->Mass = 1500.0f;
    Movement->bEnableCenterOfMassOverride = false; // Preserve the official physics asset's mass distribution.
    Movement->ChassisHeight = 144.0f;
    Movement->DragCoefficient = 0.31f;
    Movement->bLegacyWheelFrictionPosition = true;
    Movement->WheelSetups.SetNum(4);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Movement->WheelSetups[Index].WheelClass = Index < 2
            ? UHCM1WheelFront::StaticClass() : UHCM1WheelRear::StaticClass();
        Movement->WheelSetups[Index].BoneName = HCM1WheelBones[Index];
        Movement->WheelSetups[Index].AdditionalOffset = FVector::ZeroVector;
    }
    Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;
    Movement->DifferentialSetup.FrontRearSplit = 0.0f;
    Movement->EngineSetup.TorqueCurve.ExternalCurve = TorqueAsset.Object;
    Movement->EngineSetup.MaxTorque = 750.0f;
    Movement->EngineSetup.MaxRPM = 7000.0f;
    Movement->EngineSetup.EngineIdleRPM = 900.0f;
    Movement->EngineSetup.EngineBrakeEffect = 0.2f;
    Movement->EngineSetup.EngineRevUpMOI = 5.0f;
    Movement->EngineSetup.EngineRevDownRate = 600.0f;
    Movement->TransmissionSetup.bUseAutomaticGears = true;
    Movement->TransmissionSetup.bUseAutoReverse = false;
    Movement->TransmissionSetup.FinalRatio = 2.81f;
    Movement->TransmissionSetup.ChangeUpRPM = 6000.0f;
    Movement->TransmissionSetup.ChangeDownRPM = 2000.0f;
    Movement->TransmissionSetup.GearChangeTime = 0.2f;
    Movement->TransmissionSetup.TransmissionEfficiency = 0.9f;
    Movement->TransmissionSetup.ForwardGearRatios = {4.25f, 2.52f, 1.66f, 1.22f, 1.0f};
    Movement->TransmissionSetup.ReverseGearRatios = {4.04f};
    Movement->SteeringSetup.SteeringType = ESteeringType::Ackermann;
    Movement->SteeringSetup.AngleRatio = 0.7f;
    // Chaos' steering-curve x axis is MPH. This is the local engine's default reduction curve.
    FRichCurve* SteeringCurve = Movement->SteeringSetup.SteeringCurve.GetRichCurve();
    SteeringCurve->Reset();
    SteeringCurve->AddKey(0.0f, 1.0f);
    SteeringCurve->AddKey(20.0f, 0.8f);
    SteeringCurve->AddKey(60.0f, 0.4f);
    SteeringCurve->AddKey(120.0f, 0.3f);
}

void AHCM1Vehicle::BeginPlay()
{
    Super::BeginPlay();
    InitialSafeTransform = GetActorTransform();
    if (const UHCM4R1SteeringMovement* Steering = Cast<UHCM4R1SteeringMovement>(GetChaosMovement()))
    {
        UE_LOG(LogTemp, Display, TEXT("HCM4R1 runtime steering component %s gain %.1f diagnostics %s"),
            *Steering->GetClass()->GetPathName(), Steering->GetEffectiveSteeringGain(),
            Steering->IsSteeringCalibrationEnabled() ? TEXT("enabled") : TEXT("disabled"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("HCM4R1 loaded vehicle %s has an unexpected movement override %s"),
            *GetPathName(), *GetChaosMovement()->GetClass()->GetPathName());
    }
    ConfigureDrivingCamera();
    // Keep UE's Controller -> MovementComponent -> Pawn order. Adding the pawn as
    // a movement prerequisite would cycle against MovementComponent::bTickBeforeOwner.
    ClearDriveInput(true);
    if (!GetMesh()->GetSkeletalMeshAsset() || !GetMesh()->GetPhysicsAsset() ||
        !GetMesh()->GetAnimClass() || !BodyVisual->GetStaticMesh() ||
        !GlassVisual->GetStaticMesh() || !GetChaosMovement()->EngineSetup.TorqueCurve.ExternalCurve)
    {
        UE_LOG(LogTemp, Error, TEXT("HCM1Vehicle is missing an official vehicle dependency: %s"), *GetPathName());
    }
    for (const FName& Bone : HCM1WheelBones)
    {
        if (GetMesh()->GetBoneIndex(Bone) == INDEX_NONE)
        {
            UE_LOG(LogTemp, Error, TEXT("HCM1Vehicle wheel bone is missing: %s"), *Bone.ToString());
        }
    }
}

void AHCM1Vehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateDrivingCamera(DeltaSeconds);
    if (!Driver.IsValid())
    {
        RequestedThrottleAxis = 0.0f;
        RequestedSteeringAxis = 0.0f;
        bParkingBrake = true;
    }
    // This actor is TG_PrePhysics. Chaos samples these raw inputs in its physics
    // scene PreTickGT, after the pre-physics tick group has completed.
    ApplyDriveInput();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("M5VS2ReverseTrace")) && RequestedThrottleAxis < -.5f && ReverseTraceSamples < 120)
    {
        ReverseTraceWait -= DeltaSeconds;
        if (ReverseTraceWait <= 0 && GetChaosMovement())
        {
            ReverseTraceWait = .05f; ++ReverseTraceSamples;
            auto* M = GetChaosMovement();
            UE_LOG(LogTemp, Display, TEXT("VS2Reverse n=%d t=%.3f speed=%.3f throttle=%.3f brake=%.3f gear=%d target=%d rpm=%.1f x=%.1f y=%.1f"),
                ReverseTraceSamples, GetWorld()->GetTimeSeconds(), GetSignedSpeed(), M->GetThrottleInput(), M->GetBrakeInput(),
                M->GetCurrentGear(), M->GetTargetGear(), M->GetEngineRotationSpeed(), GetActorLocation().X, GetActorLocation().Y);
        }
    }
#endif
}

void AHCM1Vehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Driver.Reset();
    ClearDriveInput(true);
    Super::EndPlay(EndPlayReason);
}

UChaosWheeledVehicleMovementComponent* AHCM1Vehicle::GetChaosMovement() const
{
    return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
}

void AHCM1Vehicle::SetDriveInput(float ThrottleAxis, float SteeringAxis, bool bHandbrake)
{
    if (!Driver.IsValid())
    {
        ClearDriveInput(true);
        return;
    }
    RequestedThrottleAxis = FMath::IsFinite(ThrottleAxis) ? FMath::Clamp(ThrottleAxis, -1.0f, 1.0f) : 0.0f;
    RequestedSteeringAxis = FMath::IsFinite(SteeringAxis) ? FMath::Clamp(SteeringAxis, -1.0f, 1.0f) : 0.0f;
    bRequestedHandbrake = bHandbrake;
    bParkingBrake = false;
    ApplyDriveInput();
}

void AHCM1Vehicle::ClearDriveInput(bool bPark)
{
    RequestedThrottleAxis = 0.0f;
    RequestedSteeringAxis = 0.0f;
    bRequestedHandbrake = false;
    bParkingBrake = bPark;
    if (UChaosWheeledVehicleMovementComponent* Movement = GetChaosMovement())
    {
        Movement->SetThrottleInput(0.0f);
        Movement->SetSteeringInput(0.0f);
        Movement->SetBrakeInput(bPark ? 1.0f : 0.0f);
        Movement->SetHandbrakeInput(bPark);
        Movement->SetParked(bPark);
    }
}

void AHCM1Vehicle::ApplyDriveInput()
{
    UChaosWheeledVehicleMovementComponent* Movement = GetChaosMovement();
    if (!Movement)
    {
        return;
    }
    const bool bPark = bParkingBrake || !Driver.IsValid();
    float Throttle = 0.0f;
    float Brake = bPark ? 1.0f : 0.0f;
    if (!bPark && !bRequestedHandbrake && FMath::Abs(RequestedThrottleAxis) > 0.01f)
    {
        const float SignedSpeed = GetSignedSpeed();
        const int32 Direction = RequestedThrottleAxis > 0.0f ? 1 : -1;
        const bool bOppositeMotion = SignedSpeed * Direction < -DirectionChangeSpeedCmS;
        const bool bOppositeGear = Movement->GetTargetGear() * Direction < 0 &&
            FMath::Abs(SignedSpeed) > DirectionChangeSpeedCmS;
        if (bOppositeMotion || bOppositeGear)
        {
            Brake = FMath::Abs(RequestedThrottleAxis);
        }
        else
        {
            if ((Direction < 0 && Movement->GetTargetGear() >= 0) ||
                (Direction > 0 && Movement->GetTargetGear() <= 0))
            {
                Movement->SetTargetGear(Direction, true);
            }
            Throttle = FMath::Abs(RequestedThrottleAxis);
        }
    }
    Movement->SetParked(bPark);
    Movement->SetHandbrakeInput(bPark || bRequestedHandbrake);
    Movement->SetBrakeInput(Brake);
    Movement->SetThrottleInput(Throttle);
    Movement->SetSteeringInput(bPark ? 0.0f : RequestedSteeringAxis);
}

float AHCM1Vehicle::GetSpeedKmh() const
{
    return GetMesh()->GetPhysicsLinearVelocity().Size() * 0.036f;
}

float AHCM1Vehicle::GetSignedSpeed() const
{
    return FVector::DotProduct(GetMesh()->GetPhysicsLinearVelocity(), GetActorForwardVector());
}

void AHCM1Vehicle::SetDriver(AController* NewDriver)
{
    if (NewDriver && Driver.IsValid() && Driver.Get() != NewDriver)
    {
        return;
    }
    Driver = NewDriver;
    ClearDriveInput(NewDriver == nullptr);
    if (NewDriver && HasActorBegunPlay())
    {
        GetChaosMovement()->SetSleeping(false);
        ResetDrivingCamera(true);
    }
}

bool AHCM1Vehicle::HasDriver() const
{
    return Driver.IsValid();
}

FVector AHCM1Vehicle::GetInteractionLocation() const
{
    return GetActorTransform().TransformPosition(FVector(15.0f, -105.0f, 75.0f));
}

FString AHCM1Vehicle::GetInteractionText_Implementation(APlayerController* Player) const
{
    if (HasDriver())
    {
        return TEXT("车辆驾驶位已被占用");
    }
    if (GetSpeedKmh() > MaximumExitSpeedKmh)
    {
        return TEXT("请等车辆停稳后再上车");
    }
    return TEXT("E 上车驾驶");
}

void AHCM1Vehicle::Interact_Implementation(APlayerController* Player)
{
    if (AHCM1PlayerController* HarborController = Cast<AHCM1PlayerController>(Player))
    {
        HarborController->RequestEnterVehicle(this);
    }
}

bool AHCM1Vehicle::FindSafeExitTransform(ACharacter* Character, FTransform& Out) const
{
    if (!IsValid(Character) || !GetWorld() || GetSpeedKmh() > MaximumExitSpeedKmh ||
        GetActorUpVector().Z < 0.65f)
    {
        return false;
    }
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius() + 3.0f;
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() + 3.0f;
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HCM1SafeExit), false, this);
    Query.AddIgnoredActor(Character);
    Query.bFindInitialOverlaps = true;
    const FRotator Yaw(0.0f, GetActorRotation().Yaw, 0.0f);
    const FVector Forward = Yaw.Vector();
    const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
    const float DoorY = 65.0f;
    const float ExitY = FMath::Max(190.0f, 110.0f + Radius + 30.0f);
    const float FloorNormalZ = Character->GetCharacterMovement()->GetWalkableFloorZ();
    // A waiting passenger approaches the nearer door. Driver exits and an
    // already seated passenger retain their existing left-first preference.
    const AHCM3NPC* WaitingNPC = Cast<AHCM3NPC>(Character);
    const float PreferredSide = WaitingNPC && !WaitingNPC->GetIsPassenger()
        && FVector::DotProduct(Character->GetActorLocation()-GetActorLocation(),Right)>0.f ? 1.f : -1.f;
    const float SideOrder[] = {PreferredSide, -PreferredSide};
    const float ForeAftOrder[] = {15.0f, -100.0f, 105.0f};
    for (const float Side : SideOrder)
    {
        for (const float ForeAft : ForeAftOrder)
        {
            const FVector CandidateXY = GetActorLocation() + Forward * ForeAft + Right * (Side * ExitY);
            FHitResult Ground;
            if (!GetWorld()->LineTraceSingleByChannel(Ground, CandidateXY + FVector(0, 0, 200),
                CandidateXY - FVector(0, 0, 300), ECC_Visibility, Query) ||
                !IsUsableGround(Ground, FloorNormalZ) ||
                FMath::Abs(Ground.ImpactPoint.Z - GetActorLocation().Z) > 150.0f)
            {
                continue;
            }
            const FVector ExitCenter = Ground.ImpactPoint + FVector(0, 0, HalfHeight + 4.0f);
            if (GetWorld()->OverlapBlockingTestByChannel(ExitCenter, FQuat::Identity, ECC_Pawn, Shape, Query))
            {
                continue;
            }
            FVector DoorCenter = GetActorLocation() + Forward * ForeAft + Right * (Side * DoorY);
            DoorCenter.Z = ExitCenter.Z;
            FHitResult PathHit;
            if (GetWorld()->SweepSingleByChannel(PathHit, DoorCenter, ExitCenter, FQuat::Identity,
                ECC_Pawn, Shape, Query))
            {
                continue;
            }
            const FTransform Candidate(Yaw, ExitCenter, Character->GetActorScale3D());
            if (WaitingNPC && !WaitingNPC->GetIsPassenger())
            {
                FTransform WalkableStand;
                if (!WaitingNPC->ResolvePassengerDoorStand(Candidate, WalkableStand)) continue;
            }
            Out = Candidate;
            return true;
        }
    }
    return false;
}

bool AHCM1Vehicle::ResolveSafeVehicleTransform(const FTransform& Requested, AActor* AvoidActor, FTransform& Out) const
{
    PlacementDiagnostic = TEXT("pending");
    FString FilterDiagnostic;
    const auto Reject = [this](const FString& Reason) { PlacementDiagnostic = Reason; return false; };
    if (!GetWorld() || Requested.ContainsNaN() || Requested.GetLocation().GetAbsMax() > 1000000.0)
    {
        return Reject(TEXT("invalid requested transform/world"));
    }
    FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(HCM1ResetGround), false, this);
    GroundQuery.AddIgnoredActor(AvoidActor);
    const FVector RequestLocation = Requested.GetLocation();
    const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaximumResetSlopeDegrees));
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, RequestLocation + FVector(0, 0, 350),
        RequestLocation - FVector(0, 0, 1000), ECC_Visibility, GroundQuery) || !IsUsableGround(Ground, MinNormalZ))
    {
        return Reject(TEXT("ground missing or unsafe: ")+GetNameSafe(Ground.GetComponent()));
    }

    // Include the real official body and wheel visuals, which have no separate collision bodies.
    // Explicit components exclude the chase camera's editor visualization from vehicle clearance.
    FBox LocalBox(ForceInit);
    const auto AddVisualBounds = [this, &LocalBox](const UStaticMeshComponent* Visual)
    {
        if (Visual && Visual->GetStaticMesh())
        {
            const FTransform LocalTransform = Visual->GetComponentTransform().GetRelativeTransform(GetActorTransform());
            LocalBox += Visual->CalcBounds(LocalTransform).GetBox();
        }
    };
    AddVisualBounds(BodyVisual);
    AddVisualBounds(GlassVisual);
    for (const UStaticMeshComponent* Wheel : WheelVisuals)
    {
        AddVisualBounds(Wheel);
    }
    if (!LocalBox.IsValid)
    {
        return Reject(TEXT("missing vehicle visual bounds"));
    }
    const FVector BoxCenter = LocalBox.GetCenter();
    const FVector Extent = LocalBox.GetExtent().ComponentMax(FVector(225.0f, 100.0f, 55.0f));
    LocalBox = FBox(BoxCenter - Extent, BoxCenter + Extent);
    const FVector YawForward = FRotator(0, Requested.Rotator().Yaw, 0).Vector();
    const FVector GroundForward = FVector::VectorPlaneProject(YawForward, Ground.ImpactNormal).GetSafeNormal();
    if (GroundForward.IsNearlyZero())
    {
        return Reject(TEXT("invalid ground forward"));
    }
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(GroundForward, Ground.ImpactNormal).ToQuat();
    const FVector Origin = Ground.ImpactPoint + Ground.ImpactNormal * (FMath::Max(0.0, -LocalBox.Min.Z) + 12.0);
    FTransform Candidate(Rotation, Origin, FVector::OneVector);

    // Check support under all four corners; a single trace must not accept a pole, ledge or player.
    for (const float X : {-0.65f, 0.65f})
    {
        for (const float Y : {-0.80f, 0.80f})
        {
            const FVector Footprint = Candidate.TransformPosition(FVector(BoxCenter.X + Extent.X * X,
                BoxCenter.Y + Extent.Y * Y, LocalBox.Min.Z));
            FHitResult Support;
            if (!GetWorld()->LineTraceSingleByChannel(Support, Footprint + FVector(0, 0, 100),
                Footprint - FVector(0, 0, 160), ECC_Visibility, GroundQuery) ||
                !IsUsableGround(Support, MinNormalZ) ||
                FMath::Abs(FVector::DotProduct(Support.ImpactPoint - Ground.ImpactPoint, Ground.ImpactNormal)) > 30.0f)
            {
                return Reject(TEXT("corner support missing/uneven: ")+GetNameSafe(Support.GetComponent()));
            }
        }
    }

    FCollisionQueryParams ClearanceQuery(SCENE_QUERY_STAT(HCM1ResetClearance), false, this);
    if (GetWorld()->OverlapBlockingTestByChannel(Candidate.TransformPosition(BoxCenter), Rotation,
        ECC_Vehicle, FCollisionShape::MakeBox(Extent + FVector(3.0)), ClearanceQuery))
    {
        return Reject(TEXT("vehicle clearance overlaps world"));
    }
    if (IsValid(AvoidActor) && AvoidActor != this)
    {
        // Respect the player's saved capsule even while its normal collision is disabled in a transition.
        FBox AvoidWorldBox(ForceInit), PreviousAllPrimitivesBox(ForceInit);
        const ACharacter* AvoidCharacter = Cast<ACharacter>(AvoidActor);
        FString TestMode;
        const bool bR2SaveDiagnostic = FParse::Value(FCommandLine::Get(),TEXT("M4Test="),TestMode) && TestMode.StartsWith(TEXT("r2_"));
        AvoidActor->ForEachComponent<UPrimitiveComponent>(false, [&AvoidWorldBox,&PreviousAllPrimitivesBox,&FilterDiagnostic,AvoidActor,AvoidCharacter,bR2SaveDiagnostic](const UPrimitiveComponent* Component)
        {
            // Editor camera proxies/frustums are not the player. Keep the real
            // capsule and mesh even when a possession transition disables collision.
            if (Component->IsRegistered() && !Component->IsEditorOnly())
            {
                PreviousAllPrimitivesBox += Component->Bounds.GetBox();
                // Native first-person primitives are a camera-space display copy,
                // not the player's physical capsule/world mesh. Their inactive or
                // transformed display bounds must not invalidate a real saved car.
                const bool bDisplayOnly = Component->FirstPersonPrimitiveType==EFirstPersonPrimitiveType::FirstPerson;
                // A character's capsule and world body remain physical even while
                // disabled during possession. Wings, weapons and anchored takeoff
                // effects are presentation, not additional player clearance.
                const bool bPhysicalBody = AvoidCharacter
                    ? Component==AvoidCharacter->GetCapsuleComponent() || Component==AvoidCharacter->GetMesh()
                    : Component->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
                if(!bDisplayOnly && bPhysicalBody) AvoidWorldBox += Component->Bounds.GetBox();
                else if(FVector::Dist(Component->Bounds.Origin,AvoidActor->GetActorLocation())>300.f)
                    FilterDiagnostic += Component->GetName()+TEXT(" ");
                if(bR2SaveDiagnostic) UE_LOG(LogTemp,Display,TEXT("M4R2_SAVE_AVOID component=%s firstPersonDisplay=%d bounds=%s"),*Component->GetName(),bDisplayOnly,*Component->Bounds.GetBox().ToString());
            }
        });
        if (AvoidWorldBox.IsValid)
        {
            const FBox AvoidLocalBox = AvoidWorldBox.TransformBy(Candidate.ToInverseMatrixWithScale());
            if(PreviousAllPrimitivesBox.IsValid && LocalBox.ExpandBy(15.f).Intersect(PreviousAllPrimitivesBox.TransformBy(Candidate.ToInverseMatrixWithScale()))
                && !LocalBox.ExpandBy(15.f).Intersect(AvoidLocalBox))
                FilterDiagnostic=TEXT("old decoration aggregate intersected; physical body clear; excluded distant effects: ")+FilterDiagnostic;
            else FilterDiagnostic.Empty();
            if(bR2SaveDiagnostic) UE_LOG(LogTemp,Display,TEXT("M4R2_SAVE_AVOID_COMPARE oldIntersects=%d physicalIntersects=%d old=%s physical=%s vehicle=%s"),
                PreviousAllPrimitivesBox.IsValid && LocalBox.ExpandBy(15.f).Intersect(PreviousAllPrimitivesBox.TransformBy(Candidate.ToInverseMatrixWithScale())),
                LocalBox.ExpandBy(15.f).Intersect(AvoidLocalBox),*PreviousAllPrimitivesBox.ToString(),*AvoidWorldBox.ToString(),*LocalBox.ToString());
            if (LocalBox.ExpandBy(15.0f).Intersect(AvoidLocalBox))
            {
                return Reject(TEXT("player physical bounds intersects vehicle; ")+AvoidWorldBox.ToString());
            }
        }
    }
    Out = Candidate;
    PlacementDiagnostic = TEXT("safe; ")+FilterDiagnostic.Left(1000);
    return true;
}

bool AHCM1Vehicle::TeleportToValidatedTransform(const FTransform& Transform)
{
    ClearDriveInput(true);
    if (!SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics))
    {
        return false;
    }
    // Explicit recovery only: ordinary motion is generated solely by Chaos vehicle inputs.
    GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    GetChaosMovement()->ResetVehicle();
    ClearDriveInput(true);
    GetChaosMovement()->SetSleeping(false);
    ResetDrivingCamera(true);
    if (NPCImpact) NPCImpact->ResetMotionHistory();
    return true;
}

void AHCM1Vehicle::ConfigureDrivingCamera()
{
#if !UE_BUILD_SHIPPING
    bLegacyCamera = FParse::Param(FCommandLine::Get(), TEXT("HCM1CameraLegacy"));
#endif
    // Apply to the actual loaded component, including serialized map instances.
    // The diagnostic legacy mode reproduces the V1 settings without altering assets.
    CameraBoom->SetUsingAbsoluteRotation(!bLegacyCamera);
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bInheritYaw = true;
    CameraBoom->TargetArmLength = 650.0f;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->ProbeChannel = ECC_Camera;
    CameraBoom->ProbeSize = 16.0f;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 8.0f;
    CameraBoom->CameraLagMaxDistance = 60.0f;
    CameraBoom->bUseCameraLagSubstepping = true;
    CameraBoom->bEnableCameraRotationLag = bLegacyCamera;
    CameraBoom->CameraRotationLagSpeed = 5.0f;
    CameraBoom->SetRelativeLocation(FVector::ZeroVector);
    // Height belongs to the sweep origin, not just the far end of the boom.
    if (!ensureMsgf(BodyVisual && BodyVisual->GetStaticMesh(), TEXT("M1-R1 camera requires the actual vehicle body bounds"))) return;
    const FBox BodyBounds = BodyVisual->GetStaticMesh()->GetBoundingBox();
    const FBox LocalBody = BodyBounds.TransformBy(BodyVisual->GetRelativeTransform());
    CameraBoom->TargetOffset = bLegacyCamera ? FVector::ZeroVector : FVector(0, 0, LocalBody.Max.Z + 36.0f);
    CameraBoom->SocketOffset = bLegacyCamera ? FVector(0, 0, 145.0f) : FVector::ZeroVector;
    DrivingCamera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    DrivingCamera->bUsePawnControlRotation = false;
    UHCM1CameraBoom* RuntimeBoom = Cast<UHCM1CameraBoom>(CameraBoom);
    if (ensureMsgf(RuntimeBoom, TEXT("M1-R1 actual loaded camera component must be UHCM1CameraBoom"))) RuntimeBoom->SetLegacyMode(bLegacyCamera);
    ResetDrivingCamera(true);
}

void AHCM1Vehicle::ResetDrivingCamera(bool bResetOrbit)
{
    if (bResetOrbit) ++CameraResetSerial;
    if (bResetOrbit) CameraOrbitRotation = FRotator(-12.0f, GetActorRotation().Yaw, 0.0f);
    CameraIdleSeconds = 0.0f;
    bCameraRecentering = false;
    if (bLegacyCamera) CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0, 0));
    else CameraBoom->SetWorldRotation(CameraOrbitRotation);
    if (UHCM1CameraBoom* Boom = Cast<UHCM1CameraBoom>(CameraBoom)) Boom->ResetLag();
}

void AHCM1Vehicle::AddCameraLookInput(const FVector2D& RawMouse)
{
    if (bLegacyCamera || !HasDriver() || !FMath::IsFinite(RawMouse.X) || !FMath::IsFinite(RawMouse.Y)
        || RawMouse.IsNearlyZero()) return;
    // Mouse displacement is already accumulated per frame. Never multiply it by DeltaSeconds.
    // Use the live on-foot Controller baseline, including its existing axis signs.
    // The shared Action already contains mouse mapping modifiers; apply the ratio only once.
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(GetController());
    if (!PC) return;
    const FVector2D DegreesPerActionUnit = PC->GetDrivingLookDegreesPerActionUnit();
    CameraOrbitRotation.Yaw = FRotator::NormalizeAxis(CameraOrbitRotation.Yaw + RawMouse.X * DegreesPerActionUnit.X);
    CameraOrbitRotation.Pitch = FMath::Clamp(CameraOrbitRotation.Pitch + RawMouse.Y * DegreesPerActionUnit.Y, -55.0f, -5.0f);
    CameraOrbitRotation.Roll = 0;
    CameraIdleSeconds = 0;
    bCameraRecentering = false;
    CameraBoom->SetWorldRotation(CameraOrbitRotation);
}

void AHCM1Vehicle::UpdateDrivingCamera(float DeltaSeconds)
{
    if (bLegacyCamera || !HasDriver()) return;
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(GetController());
    if (!PC || PC->IsPauseMenuOpen() || !PC->IsGameplayFocused()) return;
    CameraIdleSeconds += DeltaSeconds;
    bCameraRecentering = CameraIdleSeconds >= 1.5f && GetSignedSpeed() > 100.0f;
    if (bCameraRecentering)
    {
        const float Difference = FMath::FindDeltaAngleDegrees(CameraOrbitRotation.Yaw, GetActorRotation().Yaw);
        CameraOrbitRotation.Yaw = FRotator::NormalizeAxis(CameraOrbitRotation.Yaw + Difference * (1.0f - FMath::Exp(-3.0f * DeltaSeconds)));
    }
    CameraBoom->SetWorldRotation(CameraOrbitRotation);
}

bool AHCM1Vehicle::TrySafeReset(AActor* AvoidActor)
{
    if (!HasActorBegunPlay())
    {
        return false;
    }
    TArray<FTransform> Candidates;
    Candidates.Add(GetActorTransform());
    Candidates.Add(InitialSafeTransform);
    const FVector Offsets[] = {
        FVector(600, 0, 0), FVector(-600, 0, 0), FVector(0, 600, 0), FVector(0, -600, 0),
        FVector(1000, 1000, 0), FVector(-1000, 1000, 0), FVector(1000, -1000, 0), FVector(-1000, -1000, 0)
    };
    for (const FVector& Offset : Offsets)
    {
        FTransform Candidate = InitialSafeTransform;
        Candidate.AddToTranslation(Offset);
        Candidates.Add(Candidate);
    }
    for (const FTransform& Requested : Candidates)
    {
        FTransform Validated;
        if (ResolveSafeVehicleTransform(Requested, AvoidActor, Validated))
        {
            return TeleportToValidatedTransform(Validated);
        }
    }
    return false;
}

bool AHCM1Vehicle::RestoreSavedTransform(const FTransform& Transform, AActor* AvoidActor)
{
    FTransform Validated;
    return ResolveSafeVehicleTransform(Transform, AvoidActor, Validated) && TeleportToValidatedTransform(Validated);
}

FString AHCM1Vehicle::GetDriveTelemetry() const
{
    UChaosWheeledVehicleMovementComponent* Movement = GetChaosMovement();
    if (!Movement)
    {
        return TEXT("{\"available\":false}");
    }
    FString Wheels = TEXT("[");
    for (int32 Index = 0; Index < Movement->GetNumWheels(); ++Index)
    {
        const FWheelStatus& State = Movement->GetWheelState(Index);
        if (Index > 0)
        {
            Wheels += TEXT(",");
        }
        Wheels += FString::Printf(TEXT("{\"index\":%d,\"valid\":%s,\"contact\":%s,\"suspension\":%s,\"spring_force\":%s,\"drive_torque\":%s,\"brake_torque\":%s,\"contact_point\":%s}"),
            Index, State.bIsValid ? TEXT("true") : TEXT("false"), State.bInContact ? TEXT("true") : TEXT("false"),
            *JsonNumber(State.NormalizedSuspensionLength), *JsonNumber(State.SpringForce),
            *JsonNumber(State.DriveTorque), *JsonNumber(State.BrakeTorque), *JsonVector(State.ContactPoint));
    }
    Wheels += TEXT("]");
    const UHCM4R1SteeringMovement* Steering = Cast<UHCM4R1SteeringMovement>(Movement);
    const FString SteeringJson = Steering ? Steering->GetSteeringCalibrationTelemetry() : TEXT("{\"enabled\":false,\"component_mismatch\":true}");
    return FString::Printf(TEXT("{\"available\":true,\"occupied\":%s,\"speed_kmh\":%s,\"signed_speed_cm_s\":%s,\"gear\":%d,\"target_gear\":%d,\"rpm\":%s,\"throttle\":%s,\"brake\":%s,\"steering\":%s,\"handbrake\":%s,\"parking\":%s,\"position\":%s,\"velocity\":%s,\"up_z\":%s,\"wheels\":%s,\"steering_diagnostics\":%s}"),
        HasDriver() ? TEXT("true") : TEXT("false"), *JsonNumber(GetSpeedKmh()), *JsonNumber(GetSignedSpeed()),
        Movement->GetCurrentGear(), Movement->GetTargetGear(), *JsonNumber(Movement->GetEngineRotationSpeed()),
        *JsonNumber(Movement->GetThrottleInput()), *JsonNumber(Movement->GetBrakeInput()),
        *JsonNumber(Movement->GetSteeringInput()), Movement->GetHandbrakeInput() ? TEXT("true") : TEXT("false"),
        bParkingBrake ? TEXT("true") : TEXT("false"), *JsonVector(GetActorLocation()),
        *JsonVector(GetMesh()->GetPhysicsLinearVelocity()), *JsonNumber(GetActorUpVector().Z), *Wheels, *SteeringJson);
}
