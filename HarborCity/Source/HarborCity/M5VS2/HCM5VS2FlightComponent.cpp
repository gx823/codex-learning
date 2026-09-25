#include "HCM5VS2FlightComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M3/HCM3NPC.h"
#include "M4/HCM4CombatComponent.h"
#include "M5/HCM5StoryDirector.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AHCM5VS2FlightBounds::AHCM5VS2FlightBounds()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("FlightBoundsRoot"));
    PrimaryActorTick.bCanEverTick = false;
}

UHCM5VS2FlightComponent::UHCM5VS2FlightComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UHCM5VS2FlightComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<AHCM1Character>(GetOwner());
    if (Character.IsValid())
        Character->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
}

void UHCM5VS2FlightComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetForGroundTransition();
    Super::EndPlay(Reason);
}

AHCM1PlayerController* UHCM5VS2FlightComponent::GetPC() const
{ return Character.IsValid() ? Cast<AHCM1PlayerController>(Character->GetController()) : nullptr; }

AHCM5VS2FlightBounds* UHCM5VS2FlightComponent::GetFlightBounds() const
{
    if (!bEnableFlight || !GetWorld()) return nullptr;
    AHCM5VS2FlightBounds* Result = nullptr;
    for (TActorIterator<AHCM5VS2FlightBounds> It(GetWorld()); It; ++It)
    {
        // Ambiguous authoring fails closed even if only one of two actors is enabled.
        if (Result) return nullptr;
        Result = *It;
    }
    if (!IsValid(Result) || !Result->bEnableFlight || !FMath::IsFinite(Result->SeaLevelZ)
        || Result->FlightAreaCenter.ContainsNaN() || Result->FlightAreaHalfExtent.ContainsNaN()
        || Result->FlightAreaHalfExtent.X <= 0 || Result->FlightAreaHalfExtent.Y <= 0
        || !FMath::IsFinite(Result->CeilingHeight) || Result->CeilingHeight < 200
        || !FMath::IsFinite(Result->WaterClearance) || Result->WaterClearance < 0
        || Result->WaterClearance >= Result->CeilingHeight
        || !FMath::IsFinite(Result->SoftReturnDistance) || Result->SoftReturnDistance < 100) return nullptr;
    return Result;
}

bool UHCM5VS2FlightComponent::IsFlightAvailable() const { return GetFlightBounds() != nullptr; }
bool UHCM5VS2FlightComponent::CanAcceptInput() const
{
    const AHCM1PlayerController* PC = GetPC();
    return PC && PC->GetPlayerMode() == EHCPlayerMode::OnFoot && PC->GetPawn() == Character.Get()
        && !PC->IsPauseMenuOpen() && PC->IsGameplayFocused() && !PC->IsDialogueOpen()
        && Character->GetCombatComponent() && Character->GetCombatComponent()->GetPlayerHealth() > 0;
}

bool UHCM5VS2FlightComponent::Reject(const FString& Reason)
{
    LastReason = Reason; ++RejectedCount;
    if (AHCM1PlayerController* PC = GetPC()) PC->ShowStatusMessage(Reason);
    return false;
}

float UHCM5VS2FlightComponent::GetHeightAboveSea() const
{
    const AHCM5VS2FlightBounds* Bounds = GetFlightBounds();
    return Bounds && Character.IsValid() ? Character->GetActorLocation().Z
        - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Bounds->SeaLevelZ : 0.f;
}

bool UHCM5VS2FlightComponent::IsWaterSurface(const FHitResult& Hit) const
{
    const AHCM5VS2FlightBounds* Bounds = GetFlightBounds();
    return !Bounds || Hit.ImpactPoint.Z <= Bounds->SeaLevelZ + 1.f
        || (Hit.GetActor() && (Hit.GetActor()->ActorHasTag(TEXT("Water")) || Hit.GetActor()->ActorHasTag(TEXT("M5VS2.Water"))))
        || (Hit.GetComponent() && (Hit.GetComponent()->ComponentHasTag(TEXT("Water")) || Hit.GetComponent()->ComponentHasTag(TEXT("M5VS2.Water"))));
}

bool UHCM5VS2FlightComponent::FindLandingSurface(FHitResult& Hit) const
{
    if (!Character.IsValid() || !GetFlightBounds()) return false;
    const FVector Position = Character->GetActorLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightLanding), false, Character.Get());
    for (TActorIterator<AHCM3NPC> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
    const FVector End(Position.X, Position.Y, GetFlightBounds()->SeaLevelZ - 200.f);
    return GetWorld()->LineTraceSingleByChannel(Hit, Position, End, ECC_Visibility, Params)
        && Hit.bBlockingHit && !IsWaterSurface(Hit) && Character->GetCharacterMovement()->IsWalkable(Hit)
        && (!Hit.GetComponent() || !Hit.GetComponent()->IsSimulatingPhysics());
}

bool UHCM5VS2FlightComponent::TryToggleFlight()
{
    if (!CanAcceptInput() || !IsFlightAvailable()) return false;
    if (bFlying)
    {
        if(bExhausted) return Reject(TEXT("体力耗尽，正滑翔降落；可水平移动寻找陆地。"));
        if (bLanding) { bLanding = false; LastReason = TEXT("降落已取消，继续悬停。"); GetPC()->ShowStatusMessage(LastReason); return true; }
        FHitResult Ground;
        if (!FindLandingSurface(Ground)) return Reject(TEXT("这里不能降落，请飞到可站立的地面上方。"));
        bLanding = true; ClearFlightInput(); LastReason = TEXT("正在平稳降落；再次按 F 可取消。");
        GetPC()->ShowStatusMessage(LastReason); return true;
    }
    UCharacterMovementComponent* Move = Character->GetCharacterMovement();
    if(Stamina<30.f) return Reject(TEXT("体力需恢复至 30% 才能起飞，请在地面休息。"));
    if (!Move->IsMovingOnGround()) return Reject(TEXT("请先在地面站稳后起飞。"));
    UHCM4CombatComponent* Combat = Character->GetCombatComponent();
    if (Combat->IsAttacking() || Combat->IsReloading() || Combat->GetPlayerHealth() <= 0)
        return Reject(TEXT("当前动作未结束，暂时不能起飞。"));
    if (GetPC()->GetM5Story())
    {
        FString Reason;
        if (GetPC()->GetM5Story()->IsFlightTakeoffRestricted(Character->GetActorLocation(), Reason)) return Reject(Reason);
    }
    const AHCM5VS2FlightBounds* Bounds = GetFlightBounds();
    for (const FBox& Volume : Bounds->NoTakeoffVolumes)
        if (Volume.IsValid && Volume.IsInsideOrOn(Character->GetActorLocation())) return Reject(TEXT("室内无法起飞，请先到室外。"));
    // Visibility roof check plus a real capsule sweep prevent lifting into an eave.
    FHitResult Roof;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightRoof), false, Character.Get());
    const FVector Position = Character->GetActorLocation();
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float HH = Capsule->GetScaledCapsuleHalfHeight();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    if (GetWorld()->LineTraceSingleByChannel(Roof, Position, Position + FVector(0,0,HH+500), ECC_Visibility, Params)
        || GetWorld()->SweepSingleByChannel(Roof, Position + FVector(0,0,3), Position + FVector(0,0,150),
            FQuat::Identity, Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Radius,HH), Params))
        return Reject(TEXT("上方空间不足，移到没有屋顶遮挡的地方再起飞。"));
    if (!FMath::IsFinite(CruiseSpeed) || !FMath::IsFinite(BoostSpeed) || !FMath::IsFinite(VerticalSpeed)
        || !FMath::IsFinite(FlightAcceleration) || !FMath::IsFinite(FlightBraking)
        || CruiseSpeed <= 0 || BoostSpeed < CruiseSpeed || VerticalSpeed <= 0 || FlightAcceleration <= 0 || FlightBraking <= 0)
        return Reject(TEXT("飞行参数无效，已保留步行状态。"));
    Character->StopBodyFacing(); Character->StopJumping(); Character->SetSprinting(false);
    Character->ConsumeMovementInputVector(); ClearFlightInput();
    Combat->PrepareForFlight();
    SavedMaxFlySpeed = Move->MaxFlySpeed; SavedMaxAcceleration = Move->MaxAcceleration;
    SavedFlyingBraking = Move->BrakingDecelerationFlying;
    bSavedPhysicsInteraction = Move->bEnablePhysicsInteraction;
    bSavedRequestedAcceleration = Move->bRequestedMoveUseAcceleration;
    bSavedOrientToMovement = Move->bOrientRotationToMovement;
    bSavedControllerDesiredRotation = Move->bUseControllerDesiredRotation;
    Move->MaxFlySpeed = FMath::Max(BoostSpeed, VerticalSpeed);
    Move->MaxAcceleration = FlightAcceleration; Move->BrakingDecelerationFlying = FlightBraking;
    Move->bEnablePhysicsInteraction = false;
    // The request is smoothed explicitly before CMC. CMC's own path deceleration is instantaneous.
    Move->bRequestedMoveUseAcceleration = false;
    // CMC Flying otherwise permits pitch/roll. Keep the physical capsule upright;
    // the visual bank is exposed separately for the flight pose, never the camera.
    Move->bOrientRotationToMovement = false;
    Move->bUseControllerDesiredRotation = false;
    bFlying = true; bLanding = false; ++TakeoffCount;
    TakeoffFeetZ = Position.Z - HH;
    TakeoffUntil = GetWorld()->GetTimeSeconds() + .75;
    Move->SetMovementMode(MOVE_Flying);
    RefreshIgnoredNPCs();
    LastReason = TEXT("F 降落 · Space 上升 · 左 Ctrl 下降 · Shift 沿镜头加速");
    GetPC()->ShowStatusMessage(LastReason, 5); return true;
}

void UHCM5VS2FlightComponent::SetHorizontalInput(FVector2D Input)
{ HorizontalInput = bFlying && CanAcceptInput() && !Input.ContainsNaN() ? Input.GetClampedToMaxSize(1.) : FVector2D::ZeroVector; }
void UHCM5VS2FlightComponent::SetAscendHeld(bool bHeld) { bAscend = bHeld && bFlying && CanAcceptInput(); }
void UHCM5VS2FlightComponent::SetDescendHeld(bool bHeld) { bDescend = bHeld && bFlying && CanAcceptInput(); }
void UHCM5VS2FlightComponent::SetBoostHeld(bool bHeld) { bBoost = bHeld && bFlying && CanAcceptInput(); }
void UHCM5VS2FlightComponent::ClearFlightInput()
{ HorizontalInput = FVector2D::ZeroVector; bAscend = bDescend = bBoost = false; RequestedFlightVelocity = FVector::ZeroVector; }

void UHCM5VS2FlightComponent::RefreshIgnoredNPCs()
{
    if (!Character.IsValid()) return;
    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    for (TActorIterator<AHCM3NPC> It(GetWorld()); It; ++It)
        if (!Capsule->GetMoveIgnoreActors().Contains(*It))
        { Capsule->IgnoreActorWhenMoving(*It, true); IgnoredNPCs.Add(*It); }
    LastNPCRefresh = GetWorld()->GetTimeSeconds();
}

void UHCM5VS2FlightComponent::RestoreMovementSettings()
{
    if (!Character.IsValid()) return;
    UCharacterMovementComponent* Move = Character->GetCharacterMovement();
    Move->MaxFlySpeed = SavedMaxFlySpeed; Move->MaxAcceleration = SavedMaxAcceleration;
    Move->BrakingDecelerationFlying = SavedFlyingBraking;
    Move->bEnablePhysicsInteraction = bSavedPhysicsInteraction;
    Move->bRequestedMoveUseAcceleration = bSavedRequestedAcceleration;
    Move->bOrientRotationToMovement = bSavedOrientToMovement;
    Move->bUseControllerDesiredRotation = bSavedControllerDesiredRotation;
    Move->StopActiveMovement();
    for (const TWeakObjectPtr<AActor>& NPC : IgnoredNPCs)
        if (NPC.IsValid()) Character->GetCapsuleComponent()->IgnoreActorWhenMoving(NPC.Get(), false);
    IgnoredNPCs.Reset();
}

void UHCM5VS2FlightComponent::ResetForGroundTransition()
{
    if (bFlying) RestoreMovementSettings();
    bFlying = bLanding = false; VisualBankDegrees = 0; ClearFlightInput();
}

void UHCM5VS2FlightComponent::FinishLanding()
{
    ResetForGroundTransition(); ++LandingCount;
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Character->GetCharacterMovement()->StopMovementImmediately();
    LastReason = TEXT("已落地，保持空手；Q 切换武器。");
    if (GetPC()) GetPC()->ShowStatusMessage(LastReason);
}

void UHCM5VS2FlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime,TickType,ThisTick);
    if (!Character.IsValid() || DeltaTime <= 0) return;
    if(!bFlying)
    {
        if(CanAcceptInput() && Character->GetCharacterMovement()->IsMovingOnGround())
        {Stamina=FMath::Min(100.f,Stamina+10.f*DeltaTime); if(Stamina>=30.f){bExhausted=false;bLowStaminaNotified=false;}}
        return;
    }
    UCharacterMovementComponent* Move = Character->GetCharacterMovement();
    AHCM5VS2FlightBounds* Bounds = GetFlightBounds();
    if (!Bounds || Move->MovementMode != MOVE_Flying)
    { ResetForGroundTransition(); if (Move->MovementMode == MOVE_Flying) Move->SetMovementMode(MOVE_Falling); return; }
    if (!CanAcceptInput())
    { ClearFlightInput(); Move->StopMovementImmediately(); Move->StopActiveMovement(); return; }
    Stamina=FMath::Max(0.f,Stamina-DeltaTime*(IsBoosting()?10.f:5.f));
    if(Stamina<25.f&&!bLowStaminaNotified)
    {bLowStaminaNotified=true;GetPC()->ShowStatusMessage(TEXT("飞行体力不足 25%，请尽快降落。"));}
    if(Stamina<=0 && !bExhausted)
    {bExhausted=true;bBoost=false;bAscend=false;bLanding=true;GetPC()->ShowStatusMessage(TEXT("体力耗尽：自动滑翔降落，不能上升。"));}
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastNPCRefresh > 1.) RefreshIgnoredNPCs();
    Character->ConsumeMovementInputVector();
    const FVector Position = Character->GetActorLocation();
    const float HH = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FRotator Aim = GetPC()->GetControlRotation();
    const FVector Forward = FRotationMatrix(FRotator(0,Aim.Yaw,0)).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(FRotator(0,Aim.Yaw,0)).GetUnitAxis(EAxis::Y);
    FVector Target = (Forward * HorizontalInput.Y + Right * HorizontalInput.X) * CruiseSpeed;
    Target.Z = (int32(bAscend) - int32(bDescend)) * VerticalSpeed;
    if (bBoost)
    {
        // Holding Shift supplies forward thrust even without W; mouse pitch permits diving/climbing.
        const float ForwardAmount = HorizontalInput.Y < 0 ? HorizontalInput.Y : 1.f;
        Target = (Aim.Vector() * ForwardAmount + Right * HorizontalInput.X).GetSafeNormal() * BoostSpeed;
        Target += FVector(0,0,(int32(bAscend) - int32(bDescend)) * VerticalSpeed);
        Target = Target.GetClampedToMaxSize(BoostSpeed);
    }
    if (Now < TakeoffUntil && !bLanding && !bDescend && Position.Z-HH < TakeoffFeetZ + 120.f)
        Target.Z = FMath::Max(Target.Z, 240.);
    if (bLanding)
    {
        FHitResult Ground;
        if (!FindLandingSurface(Ground))
        {
            if(!bExhausted){ bLanding=false; Reject(TEXT("下方不再是安全地面，降落已取消。"));Target=FVector::ZeroVector;}
            else {Target=(Forward*HorizontalInput.Y+Right*HorizontalInput.X)*CruiseSpeed*.5f;Target.Z=-180.f;}
        }
        else
        {
            const float Distance = Position.Z - HH - Ground.ImpactPoint.Z;
            // Use the capsule's real floor gap on slopes, not only its centre line.
            FFindFloorResult Floor;
            Move->FindFloor(Position,Floor,false);
            if (Floor.IsWalkableFloor() && Floor.FloorDist <= 5.f && !IsWaterSurface(Floor.HitResult)
                && Move->Velocity.Size2D() < 30.f && FMath::Abs(Move->Velocity.Z) < 100.f)
            { FinishLanding(); return; }
            Target = FVector(0,0,-FMath::Min(VerticalSpeed,FMath::Max(30.f,FMath::Sqrt(2.f*FlightBraking*FMath::Max(0.f,Distance-3.f)))));
            if(bExhausted)Target.Z=FMath::Max(Target.Z,-180.);
        }
    }
    // Soft return begins 100 m beyond the sample bounds; no teleport or abrupt reversal.
    const FVector2D Outer = Bounds->FlightAreaHalfExtent + FVector2D(Bounds->SoftReturnDistance);
    bool bReturning = false;
    for (int32 Axis=0; Axis<2; ++Axis)
    {
        const double Offset = Position[Axis] - Bounds->FlightAreaCenter[Axis];
        const double Excess = FMath::Abs(Offset) - Outer[Axis];
        if (Excess > 0)
        { Target[Axis] = -FMath::Sign(Offset) * FMath::Clamp(Excess * 2., 100., double(CruiseSpeed)); bReturning = true; }
    }
    if (bReturning && Now-LastBoundaryNotice > 4.)
    { GetPC()->ShowStatusMessage(TEXT("已到样板边界，正平稳返回。")); LastBoundaryNotice = Now; }
    const float MinCenterZ = Bounds->SeaLevelZ + Bounds->WaterClearance + HH;
    const float MaxCenterZ = Bounds->SeaLevelZ + Bounds->CeilingHeight + HH;
    // A dry landing can finish below the water-clearance band, never on/below water itself.
    FHitResult ExhaustedGround;
    const bool bDryLanding=bLanding&&FindLandingSurface(ExhaustedGround);
    const float MinimumZ = bDryLanding ? Bounds->SeaLevelZ + HH + 1.f : MinCenterZ;
    const float LowerDistance = FMath::Max(0.f,float(Position.Z-MinimumZ));
    const float UpperDistance = FMath::Max(0.f,float(MaxCenterZ-Position.Z));
    Target.Z = FMath::Clamp(Target.Z, -double(FMath::Sqrt(2.f*FlightBraking*LowerDistance)), double(FMath::Sqrt(2.f*FlightBraking*UpperDistance)));
    // Taking off from a low dry quay raises gently into the clearance band.
    if (Position.Z < MinimumZ) Target.Z = FMath::Max(Target.Z, 240.);
    if(bExhausted && Position.Z>=MinimumZ) Target.Z=FMath::Min(Target.Z,0.);
    const bool bDecelerating = Target.SizeSquared() < Move->Velocity.SizeSquared();
    FVector Next = FMath::VInterpConstantTo(Move->Velocity,Target,DeltaTime,bDecelerating ? FlightBraking : FlightAcceleration);
    // Final velocity clamp prevents a long frame from crossing the sea/ceiling plane.
    Next.Z = FMath::Clamp(Next.Z,FMath::Min(0.,double((MinimumZ-Position.Z)/DeltaTime)),FMath::Max(0.,double((MaxCenterZ-Position.Z)/DeltaTime)));
    RequestedFlightVelocity = Next;
    Move->StopActiveMovement();
    if (Next.IsNearlyZero(.1)) Move->StopMovementImmediately();
    else Move->RequestDirectMove(Next, false);
    const float Heading = GetPC()->IsFirstPersonPerspective() || Next.Size2D() < 100.f
        ? Aim.Yaw : Next.Rotation().Yaw;
    const float PreviousYaw = Character->GetActorRotation().Yaw;
    const float Yaw = FMath::FixedTurn(PreviousYaw,Heading,180.f*DeltaTime);
    Character->SetActorRotation(FRotator(0,Yaw,0));
    // Use the body's actual smoothed turn, including mouse-steered W/Shift flight.
    // The visual pose alone banks; the capsule and camera remain upright.
    const float YawRate = FMath::FindDeltaAngleDegrees(PreviousYaw,Yaw) / DeltaTime;
    const float TurnBank = -YawRate * (12.f / 180.f);
    const float DesiredBank = bLanding ? 0.f : FMath::Clamp(TurnBank - HorizontalInput.X * 12.f,-12.f,12.f);
    VisualBankDegrees = FMath::FInterpTo(VisualBankDegrees,DesiredBank,DeltaTime,4.f);
}

FName UHCM5VS2FlightComponent::GetFlightPresentationState() const
{
    if (!bFlying) return TEXT("Grounded");
    if (bLanding) return TEXT("Landing");
    if (GetWorld() && GetWorld()->GetTimeSeconds() < TakeoffUntil) return TEXT("Takeoff");
    if (bBoost) return TEXT("Boost");
    if (RequestedFlightVelocity.Z > 100) return TEXT("Ascending");
    if (RequestedFlightVelocity.Z < -100) return TEXT("Descending");
    return RequestedFlightVelocity.Size2D() > 100 ? TEXT("Forward") : TEXT("Hover");
}

FString UHCM5VS2FlightComponent::GetFlightDiagnostics() const
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetBoolField(TEXT("available"),IsFlightAvailable()); Json->SetBoolField(TEXT("flying"),bFlying);
    Json->SetBoolField(TEXT("landing"),bLanding); Json->SetBoolField(TEXT("boost"),bBoost);
    Json->SetNumberField(TEXT("stamina"),Stamina); Json->SetBoolField(TEXT("exhausted"),bExhausted);
    Json->SetStringField(TEXT("presentation_state"),GetFlightPresentationState().ToString());
    Json->SetBoolField(TEXT("ascend"),bAscend); Json->SetBoolField(TEXT("descend"),bDescend);
    Json->SetNumberField(TEXT("input_x"),HorizontalInput.X); Json->SetNumberField(TEXT("input_y"),HorizontalInput.Y);
    Json->SetNumberField(TEXT("height_above_sea_cm"),GetHeightAboveSea());
    Json->SetNumberField(TEXT("cruise_cm_s"),CruiseSpeed); Json->SetNumberField(TEXT("boost_cm_s"),BoostSpeed);
    Json->SetNumberField(TEXT("vertical_cm_s"),VerticalSpeed); Json->SetNumberField(TEXT("takeoffs"),TakeoffCount);
    Json->SetNumberField(TEXT("visual_bank_degrees"),VisualBankDegrees);
    Json->SetNumberField(TEXT("landings"),LandingCount); Json->SetNumberField(TEXT("rejected"),RejectedCount);
    Json->SetStringField(TEXT("last_reason"),LastReason); Json->SetStringField(TEXT("requested_velocity"),RequestedFlightVelocity.ToString());
    if (Character.IsValid())
    {
        Json->SetStringField(TEXT("position"),Character->GetActorLocation().ToString());
        Json->SetStringField(TEXT("velocity"),Character->GetVelocity().ToString());
        for (int32 Axis=0; Axis<3; ++Axis)
        {
            const FString Suffix=Axis==0 ? TEXT("x") : Axis==1 ? TEXT("y") : TEXT("z");
            Json->SetNumberField(TEXT("position_")+Suffix,Character->GetActorLocation()[Axis]);
            Json->SetNumberField(TEXT("velocity_")+Suffix,Character->GetVelocity()[Axis]);
            Json->SetNumberField(TEXT("requested_velocity_")+Suffix,RequestedFlightVelocity[Axis]);
        }
        Json->SetNumberField(TEXT("movement_mode"),Character->GetCharacterMovement()->MovementMode);
        Json->SetBoolField(TEXT("physics_interaction"),Character->GetCharacterMovement()->bEnablePhysicsInteraction);
    }
    if (GetPC())
    { const FVector2D Gain=GetPC()->GetOnFootLookDegreesPerActionUnit(); Json->SetNumberField(TEXT("look_yaw"),Gain.X); Json->SetNumberField(TEXT("look_pitch"),Gain.Y); }
    FString Output; TSharedRef<TJsonWriter<>> Writer=TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Json,Writer); return Output;
}
