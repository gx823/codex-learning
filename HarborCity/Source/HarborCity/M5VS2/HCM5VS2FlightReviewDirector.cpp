#include "HCM5VS2FlightReviewDirector.h"
#include "HCM5VS2FlightComponent.h"
#include "HCM5VS2FlightVisualComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Vehicle.h"
#include "M1/HCM1LightSwitch.h"
#include "M3/HCM3Experience.h"
#include "M4/HCM4CombatComponent.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputKeyEventArgs.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
enum EFlightTest { Warmup, Ground, Takeoff, Hover, Look, Cruise, Boost, Brake,
    PauseStart, PauseResume, Wall, RoofGround, RoofDenied, Retakeoff, RoofAir,
    Narrow, Water, WaterReject, Ceiling, Boundary, SlopeGround, SlopeTakeoff,
    SlopeLand, FinalGround, LoadCheck, Done };
TSharedPtr<FJsonObject> Parse(const FString& Text)
{
    TSharedPtr<FJsonObject> R;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),R) || !R) R=MakeShared<FJsonObject>();
    return R;
}
TArray<TSharedPtr<FJsonValue>> Objects(const TArray<TSharedPtr<FJsonObject>>& Rows)
{ TArray<TSharedPtr<FJsonValue>> Out; for (const auto& Row:Rows) Out.Add(MakeShared<FJsonValueObject>(Row)); return Out; }
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> XY(const FVector2D& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y)}; }
bool ReadLookScalarChain(AHCM1PlayerController* PC, FVector2D& Scale, TSharedPtr<FJsonObject>& Evidence)
{
    Evidence=MakeShared<FJsonObject>();
    Scale=FVector2D(1.,1.);
    const UEnhancedPlayerInput* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);
    const UInputAction* Look=PC->GetM1Action(TEXT("Look"));
    if (!Input || !Look || Look->ValueType!=EInputActionValueType::Axis2D) return false;
    bool Valid=true;
    TArray<TSharedPtr<FJsonObject>> ModifierRows;
    auto Accumulate=[&](const TArray<TObjectPtr<UInputModifier>>& Modifiers, const TCHAR* Layer)
    {
        for (const UInputModifier* Modifier:Modifiers)
        {
            auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("layer"),Layer);
            Row->SetStringField(TEXT("class"),Modifier ? Modifier->GetClass()->GetPathName() : TEXT("null"));
            const UInputModifierScalar* Scalar=Cast<UInputModifierScalar>(Modifier);
            if (Scalar && !Scalar->Scalar.ContainsNaN())
            {
                Row->SetArrayField(TEXT("scalar"),XYZ(Scalar->Scalar));
                Scale*=FVector2D(Scalar->Scalar.X,Scalar->Scalar.Y);
            }
            else Valid=false; // Unknown/nonlinear modifiers require a different explicit prediction.
            ModifierRows.Add(Row);
        }
    };
    int32 MappingCount=0;
    for (const FEnhancedActionKeyMapping& Mapping:Input->GetEnhancedActionMappingsView())
    {
        if (Mapping.Action!=Look) continue;
        ++MappingCount;
        if (Mapping.Key!=EKeys::Mouse2D) Valid=false;
        Accumulate(Mapping.Modifiers,TEXT("active_mapping"));
    }
    const FInputActionInstance* Instance=Input->FindActionInstanceData(Look);
    Accumulate(Instance ? Instance->GetModifiers() : Look->Modifiers,TEXT("action"));
    Evidence->SetNumberField(TEXT("active_look_mapping_count"),MappingCount);
    Evidence->SetBoolField(TEXT("action_instance_present"),Instance!=nullptr);
    Evidence->SetArrayField(TEXT("modifiers"),Objects(ModifierRows));
    Evidence->SetArrayField(TEXT("raw_to_action_scalar"),XY(Scale));
    Evidence->SetObjectField(TEXT("controller_input"),Parse(PC->GetLookInputDiagnostics()));
    return Valid && MappingCount==1 && !Scale.ContainsNaN() && Scale.X!=0 && Scale.Y!=0;
}
FString PhaseName(int32 P)
{
    static const TCHAR* Names[]={TEXT("Warmup"),TEXT("Ground"),TEXT("Takeoff"),TEXT("HoverRules"),TEXT("LookKeyInjection"),
        TEXT("Cruise"),TEXT("Boost"),TEXT("Brake"),TEXT("PauseStart"),TEXT("PauseResume"),TEXT("WallSweep"),
        TEXT("RoofGround"),TEXT("RoofDenied"),TEXT("Retakeoff"),TEXT("RoofAirSweep"),TEXT("NarrowSweep"),
        TEXT("WaterClearance"),TEXT("WaterLandingRejected"),TEXT("Ceiling"),TEXT("SoftBoundary"),TEXT("SlopeGround"),
        TEXT("SlopeTakeoff"),TEXT("SlopeLand"),TEXT("FinalGround"),TEXT("LoadCheck"),TEXT("Done")};
    return P>=0 && P<UE_ARRAY_COUNT(Names)?Names[P]:TEXT("Unknown");
}
}

AHCM5VS2FlightReviewDirector::AHCM5VS2FlightReviewDirector()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}

void AHCM5VS2FlightReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2FlightReview"))) return;
    const FString Map=GetWorld()->GetOutermost()->GetName();
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/FlightReview/Run_");
    if (!Map.StartsWith(Prefix) || !Map.EndsWith(TEXT("/L_FlightReview"))) return;
    const FString Token=Map.Mid(Prefix.Len(),12);
    if (Map!=Prefix+Token+TEXT("/L_FlightReview")) return;
    for (TCHAR C:Token) if (!FChar::IsHexDigit(C)) return;
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root) || FPaths::IsRelative(Root)) return;
    Root=FPaths::ConvertRelativePathToFull(Root); FPaths::NormalizeDirectoryName(Root);
    if (!FPaths::CollapseRelativeDirectories(Root) || !FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2"))) return;
    Directory=Root/(TEXT("FlightReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true)) return;
    StartedWall=LastWall=FPlatformTime::Seconds(); bActive=true;
    bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit")); SetActorTickEnabled(true);
    Write(TEXT("RUNNING"),TEXT("Waiting for actual pawn/controller/viewport; no automation until focused."));
#endif
}

bool AHCM5VS2FlightReviewDirector::Start()
{
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));
    Character=PC?Cast<AHCM1Character>(PC->GetPawn()):nullptr;
    UGameViewportClient* V=GetWorld()->GetGameViewport();
    if (!PC || !Character || !V || !V->Viewport || !PC->IsGameplayFocused()) return false;
    Flight=PC->GetFlightComponent(); Viewport=V;
    if (!InputHandle.IsValid()) InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2FlightReviewDirector::Input);
    if (!Check(TEXT("exact_fixture_references"),Flight && Flight->IsFlightAvailable() &&
        IsValid(Vehicle) && IsValid(InteractionSwitch) && PC->GetM3Experience()!=nullptr,
        TEXT("One enabled bounds actor, persistent car, switch and combat experience are required"))) return false;
    SaveSlot=PC->GetSaveSlotName();
    if (!Check(TEXT("private_test_slot"),SaveSlot.StartsWith(TEXT("HarborCity_M1_R2_Test_Flight_")) &&
        !UGameplayStatics::DoesSaveGameExist(SaveSlot,0),SaveSlot)) return false;
    const AHCM5VS2FlightBounds* B=Flight->GetFlightBounds();
    if (!Check(TEXT("production_units_and_limits"),B->SeaLevelZ==-150 && B->CeilingHeight==10000 && B->WaterClearance==50 &&
        B->SoftReturnDistance==10000 && B->FlightAreaHalfExtent==FVector2D(2500,2500) &&
        Flight->CruiseSpeed==1000 && Flight->BoostSpeed==2500 && Flight->VerticalSpeed==600,
        Flight->GetFlightDiagnostics())) return false;
    for (const FName Action:{FName(TEXT("Move")),FName(TEXT("Jump")),FName(TEXT("Sprint")),FName(TEXT("FlightToggle")),FName(TEXT("FlightDescend"))})
        if (!Check(TEXT("action_exists_")+Action.ToString(),PC->GetM1Action(Action)!=nullptr,Action.ToString())) return false;
    if (!Check(TEXT("enhanced_player_input"),Cast<UEnhancedPlayerInput>(PC->PlayerInput)!=nullptr,TEXT("Real controller Action bindings required"))) return false;
    bOriginalFirstPerson=PC->IsFirstPersonPerspective();
    bInitialSwitch=InteractionSwitch->IsLightEnabled();
    Character->GetCapsuleComponent()->OnComponentHit.AddDynamic(this,&AHCM5VS2FlightReviewDirector::Hit);
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    Samples.Add(Snapshot()); Phase(Warmup); return true;
}

bool AHCM5VS2FlightReviewDirector::Check(const FString& Name,bool Passed,const FString& Detail)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("check"),Name); R->SetStringField(TEXT("status"),Passed?TEXT("PASS"):TEXT("FAIL"));
    R->SetStringField(TEXT("detail"),Detail); R->SetStringField(TEXT("phase"),PhaseName(CurrentPhase));
    R->SetNumberField(TEXT("active_seconds"),ActiveSeconds); Checks.Add(R);
    if (!Passed) Finish(TEXT("FAIL"),Name+TEXT(": ")+Detail); return Passed;
}
void AHCM5VS2FlightReviewDirector::Phase(int32 Value)
{ CurrentPhase=Value; PhaseStart=ActiveSeconds; HitStart=TotalHits; bPhasePulse=false; Samples.Add(Snapshot()); Write(TEXT("RUNNING"),PhaseName(Value)); }

void AHCM5VS2FlightReviewDirector::Inject(const FVector2D& Move,bool Up,bool Down,bool Boosting)
{
    if (!PC) return;
    if (auto* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput))
    {
        Input->InjectInputForAction(PC->GetM1Action(TEXT("Move")),FInputActionValue(Move));
        Input->InjectInputForAction(PC->GetM1Action(TEXT("Jump")),FInputActionValue(Up));
        Input->InjectInputForAction(PC->GetM1Action(TEXT("FlightDescend")),FInputActionValue(Down));
        Input->InjectInputForAction(PC->GetM1Action(TEXT("Sprint")),FInputActionValue(Boosting));
    }
}
void AHCM5VS2FlightReviewDirector::ToggleAction()
{
    CastChecked<UEnhancedPlayerInput>(PC->PlayerInput)->InjectInputForAction(PC->GetM1Action(TEXT("FlightToggle")),FInputActionValue(true));
    bReleaseToggle=true;
}

bool AHCM5VS2FlightReviewDirector::Fixture(FVector Feet,bool Grounded,float Yaw)
{
    // Explicit test setup only. Every measured movement after this point uses real CMC sweeps.
    Inject(); PC->FlushPressedKeys(); Flight->ClearFlightInput();
    auto* Move=Character->GetCharacterMovement(); auto* Capsule=Character->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightFixture),false,Character);
    if (Grounded)
    {
        FHitResult Floor;
        if (!Check(TEXT("fixture_actual_dry_floor"),GetWorld()->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,400),Feet-FVector(0,0,500),ECC_Visibility,Params)
            && Move->IsWalkable(Floor) && Floor.ImpactPoint.Z>Flight->GetFlightBounds()->SeaLevelZ+1,Feet.ToString())) return false;
        Feet=Floor.ImpactPoint+FVector(0,0,3); Flight->ResetForGroundTransition();
    }
    else if (!Check(TEXT("air_fixture_requires_real_prior_takeoff"),Flight->IsFlying(),TEXT("Never set bFlying or movement mode to manufacture takeoff"))) return false;
    const FVector Destination=Feet+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    if (!Check(TEXT("fixture_capsule_space_clear"),!GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params),Destination.ToString())) return false;
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("kind"),TEXT("TELEPORT_FIXTURE_SETUP_NOT_FLIGHT"));
    R->SetArrayField(TEXT("before"),XYZ(Character->GetActorLocation())); R->SetArrayField(TEXT("feet"),XYZ(Feet));
    R->SetBoolField(TEXT("grounded_fixture"),Grounded); R->SetNumberField(TEXT("heading_yaw_fixture_not_mouse"),Yaw);
    Move->StopActiveMovement(); Move->StopMovementImmediately();
    Character->SetActorLocationAndRotation(Destination,FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    PC->SetControlRotation(FRotator(0,Yaw,0));
    const FVector Teleported=Character->GetActorLocation();
    R->SetArrayField(TEXT("requested_capsule_center"),XYZ(Destination));
    R->SetArrayField(TEXT("actual_after_teleport_before_mode_change"),XYZ(Teleported));
    // Walking entry performs FindFloor/AdjustFloorHeight. Verify the teleport itself
    // before that legitimate CMC adjustment, and validate the real ground support separately.
    Fixtures.Add(R);
    if (!Check(TEXT("fixture_location_exact"),FVector::Distance(Destination,Teleported)<.1,Destination.ToString())) return false;
    const EMovementMode PreviousMode=Move->MovementMode;
    if (Grounded) Move->SetMovementMode(MOVE_Walking);
    const FVector Actual=Character->GetActorLocation();
    R->SetArrayField(TEXT("actual_after"),XYZ(Actual));
    R->SetArrayField(TEXT("native_mode_change_adjustment"),XYZ(Actual-Teleported));
    R->SetNumberField(TEXT("movement_mode_before_change"),PreviousMode);
    if (Grounded)
    {
        FFindFloorResult Floor;
        Move->FindFloor(Actual,Floor,false);
        R->SetBoolField(TEXT("actual_walkable_floor"),Floor.IsWalkableFloor());
        R->SetNumberField(TEXT("actual_capsule_floor_distance"),Floor.FloorDist);
        R->SetStringField(TEXT("actual_floor_actor"),GetPathNameSafe(Floor.HitResult.GetActor()));
        R->SetStringField(TEXT("actual_floor_component"),GetPathNameSafe(Floor.HitResult.GetComponent()));
        R->SetArrayField(TEXT("actual_floor_normal"),XYZ(Floor.HitResult.ImpactNormal));
        const bool ChangedToWalking=PreviousMode!=MOVE_Walking;
        const bool CorrectGap=ChangedToWalking
            ? Floor.FloorDist>=UCharacterMovementComponent::MIN_FLOOR_DIST-.01f &&
                Floor.FloorDist<=UCharacterMovementComponent::MAX_FLOOR_DIST+.01f
            : FVector::Distance(Actual,Teleported)<.1;
        return Check(TEXT("fixture_native_walking_support"),Move->IsMovingOnGround() && Floor.IsWalkableFloor() &&
            !Floor.HitResult.bStartPenetrating && Floor.HitResult.ImpactPoint.Z>Flight->GetFlightBounds()->SeaLevelZ+1 &&
            FVector::Dist2D(Actual,Teleported)<.1 && CorrectGap &&
            !GetWorld()->OverlapBlockingTestByChannel(Actual,FQuat::Identity,Capsule->GetCollisionObjectType(),
                FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params),
            FString::Printf(TEXT("CMC ground adjustment %s; real capsule-floor gap %.6f cm"),
                *(Actual-Teleported).ToString(),Floor.FloorDist));
    }
    return true;
}

void AHCM5VS2FlightReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (!bActive) return;
    const double WallNow=FPlatformTime::Seconds(),Dt=FMath::Clamp(WallNow-LastWall,0.,1.); LastWall=WallNow;
    if (!InputHandle.IsValid()) if (auto* V=GetWorld()->GetGameViewport())
    { Viewport=V; InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2FlightReviewDirector::Input); }
    if (!bReady)
    {
        if (Start()) bReady=true;
        else if (bActive && WallNow-StartedWall>15) Finish(TEXT("FAIL"),TEXT("Controller/pawn/focus did not become ready within fifteen wall seconds"));
        return;
    }
    if (PC->IsPauseMenuOpen())
    {
        if (bAutomaticPause && WallNow-AutoPauseAt>.4)
        {
            bAutomaticPause=false;
            if (!Check(TEXT("pause_clears_flight_input_and_velocity"),Flight->IsFlying() && Character->GetVelocity().Size()<.1 &&
                Flight->RequestedFlightVelocity.Size()<.1,Flight->GetFlightDiagnostics())) return;
            PC->TogglePauseMenu(); Phase(PauseResume);
        }
        return;
    }
    if (!PC->IsGameplayFocused()) return;
    ActiveSeconds+=Dt;
    if (ActiveSeconds>110) { Finish(TEXT("FAIL"),TEXT("110 active-second bound exceeded")); return; }
    if (bReleaseToggle)
    {
        CastChecked<UEnhancedPlayerInput>(PC->PlayerInput)->InjectInputForAction(PC->GetM1Action(TEXT("FlightToggle")),FInputActionValue(false));
        bReleaseToggle=false;
    }
    if (ActiveSeconds>=NextSample) { Samples.Add(Snapshot()); NextSample=ActiveSeconds+.1; }
    const double Age=ActiveSeconds-PhaseStart;
    auto* Move=Character->GetCharacterMovement(); auto* Combat=PC->GetCombatComponent();
    const float HH=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector P=Character->GetActorLocation();
    const double Speed=Character->GetVelocity().Size();
    Inject();
    switch (CurrentPhase)
    {
    case Warmup:
        if (Age<4) break;
        if (!Fixture(FVector(0,0,0),true)) return; Phase(Ground); break;
    case Ground:
        if (Age<.7) break;
        if (!Check(TEXT("ground_control_valid"),Move->IsMovingOnGround() && Combat && Combat->CanUseCombat() &&
            !Vehicle->HasDriver() && Vehicle->GetSpeedKmh()<1 && PC->CanReachInteraction(InteractionSwitch),TEXT("Actual standing, working combat and nearby visible interaction before restriction tests"))) return;
        PC->RequestInteract();
        if (!Check(TEXT("ground_interact_works"),InteractionSwitch->IsLightEnabled()!=bInitialSwitch,TEXT("Real RequestInteract toggled nearby switch"))) return;
        bInitialSwitch=InteractionSwitch->IsLightEnabled();
        if (!Check(TEXT("ground_save_baseline"),PC->SaveGameNow() && UGameplayStatics::LoadDataFromSlot(SavedBytes,SaveSlot,0),PC->GetStatusMessage())) return;
        if (!Check(TEXT("equip_before_takeoff"),Combat->RequestToggleWeapon() && Combat->GetWeaponMode()==EHCM4WeaponMode::Pistol,TEXT("Actual combat entry point, not input injection"))) return;
        PositionBefore=P; ToggleAction(); Phase(Takeoff); break;
    case Takeoff:
        if (Age<1.5) break;
        if (!Check(TEXT("F_action_takeoff"),Flight->IsFlying() && Move->MovementMode==MOVE_Flying && P.Z-PositionBefore.Z>80 &&
            Combat->GetWeaponMode()==EHCM4WeaponMode::Unarmed && !Combat->IsAimHeld(),Flight->GetFlightDiagnostics())) return;
        Capture(TEXT("01_real_takeoff")); Phase(Hover); break;
    case Hover:
        if (Age<.4) break;
        {
            const int32 Shots=Combat->GetShotCount(),Ammo=Combat->GetMagazineAmmo();
            const bool Attack=Combat->RequestAttack(),Equip=Combat->RequestToggleWeapon(); Combat->SetAimHeld(true);
            const bool Saved=PC->SaveGameNow(); const FString SaveMessage=PC->GetStatusMessage();
            TArray<uint8> Now; UGameplayStatics::LoadDataFromSlot(Now,SaveSlot,0);
            PC->RequestInteract();
            if (!Check(TEXT("air_save_rejected_original_bytes_preserved"),!Saved && Now==SavedBytes,SaveMessage)) return;
            if (!Check(TEXT("air_interaction_rejected"),!PC->CanReachInteraction(InteractionSwitch) && InteractionSwitch->IsLightEnabled()==bInitialSwitch &&
                PC->GetStatusMessage().Contains(TEXT("降落")),PC->GetStatusMessage())) return;
            if (!Check(TEXT("air_attack_equip_aim_rejected"),!Attack && !Equip && !Combat->IsAimHeld() &&
                Combat->GetShotCount()==Shots && Combat->GetMagazineAmmo()==Ammo,TEXT("Real gameplay methods; F5/E/mouse buttons were not OS injected"))) return;
            if (!Check(TEXT("look_gain_preserved"),PC->GetOnFootLookDegreesPerActionUnit().Equals(FVector2D(1.5,1.5),.0001),PC->GetLookInputDiagnostics())) return;
            LookBefore=PC->GetControlRotation();
            FVector ViewBefore;
            PC->GetPlayerViewPoint(ViewBefore,LookViewBefore);
            LookCallbacksBefore=LookPreviousCallbacks=PC->GetLookCallbackCount();
            LookRawSum=LookActionSum=FVector2D::ZeroVector;
            LookFrames.Reset(); bLookMultipleCallbacks=false; LastLookPulse=0;
            LookMeasurement=MakeShared<FJsonObject>();
            TSharedPtr<FJsonObject> Chain;
            const bool ValidChain=ReadLookScalarChain(PC,LookMappingScale,Chain);
            LookMeasurement->SetObjectField(TEXT("input_chain_before"),Chain);
            if (!Check(TEXT("engine_mouse_known_live_scalar_chain"),ValidChain,
                TEXT("Read the active Mouse2D mapping and Action modifiers; raw key units are not Action units"))) return;
            Phase(Look);
        } break;
    case Look:
        {
            // Observe this processed frame before queueing the next one, as in M1-R2 calibration.
            const FVector Raw=PC->PlayerInput->GetRawVectorKeyValue(EKeys::Mouse2D);
            const uint64 Callbacks=PC->GetLookCallbackCount();
            const uint64 NewCallbacks=Callbacks-LookPreviousCallbacks;
            const FVector2D Action=NewCallbacks ? PC->GetLastLookAxis() : FVector2D::ZeroVector;
            LookRawSum+=FVector2D(Raw.X,Raw.Y);
            LookActionSum+=Action*double(NewCallbacks);
            bLookMultipleCallbacks|=NewCallbacks>1;
            LookPreviousCallbacks=Callbacks;
            auto Frame=MakeShared<FJsonObject>();
            Frame->SetNumberField(TEXT("frame"),double(GFrameCounter));
            Frame->SetNumberField(TEXT("delta_seconds"),DeltaSeconds);
            Frame->SetArrayField(TEXT("raw_mouse2d"),XYZ(Raw));
            Frame->SetNumberField(TEXT("callback_count_delta"),double(NewCallbacks));
            Frame->SetArrayField(TEXT("look_action"),XY(Action));
            Frame->SetStringField(TEXT("controller_rotation"),PC->GetControlRotation().ToString());
            FVector FrameLocation; FRotator FrameView;
            PC->GetPlayerViewPoint(FrameLocation,FrameView);
            Frame->SetStringField(TEXT("final_view_rotation"),FrameView.ToString());
            LookFrames.Add(Frame);
        }
        if (LastLookPulse<4)
        {
            // Engine paired MouseX/Y events traverse actual Mouse2D mapping, not the OS.
            const FInputDeviceId Device=IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
            PC->InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseX,1.f,DeltaSeconds,1,0u));
            PC->InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseY,.5f,DeltaSeconds,1,0u));
            ++LastLookPulse;
        }
        if (Age<.7) break;
        {
            FVector ViewLocation;
            FRotator ViewAfter;
            PC->GetPlayerViewPoint(ViewLocation,ViewAfter);
            const FRotator ControlAfter=PC->GetControlRotation();
            const FVector2D ControllerDelta(
                FMath::FindDeltaAngleDegrees(LookBefore.Yaw,ControlAfter.Yaw),
                FMath::FindDeltaAngleDegrees(LookBefore.Pitch,ControlAfter.Pitch));
            const FVector2D ViewDelta(
                FMath::FindDeltaAngleDegrees(LookViewBefore.Yaw,ViewAfter.Yaw),
                FMath::FindDeltaAngleDegrees(LookViewBefore.Pitch,ViewAfter.Pitch));
            FVector2D FinalMappingScale;
            TSharedPtr<FJsonObject> FinalChain;
            const bool ValidChain=ReadLookScalarChain(PC,FinalMappingScale,FinalChain);
            const FVector2D PlannedRaw(4.,2.);
            const FVector2D ExpectedAction=PlannedRaw*LookMappingScale;
            const FVector2D Expected=ExpectedAction*PC->GetOnFootLookDegreesPerActionUnit();
            // Retain a strict gain-relative check, not an arbitrary >1 degree threshold.
            // Raw4/2 * live Scalar.07 * Action gain1.5 legitimately produces .42/.21 degrees.
            const FVector2D AngleTolerance(FMath::Max(1.e-5,FMath::Abs(Expected.X)*.005),
                FMath::Max(1.e-5,FMath::Abs(Expected.Y)*.005));
            const FVector2D ViewTolerance(FMath::Min(.25,FMath::Max(1.e-4,FMath::Abs(Expected.X)*.01)),
                FMath::Min(.25,FMath::Max(1.e-4,FMath::Abs(Expected.Y)*.01)));
            LookMeasurement->SetStringField(TEXT("scope"),TEXT("Engine MouseX/Y through actual mapping to Controller and final player viewpoint; not OS or precise sensitivity calibration"));
            LookMeasurement->SetNumberField(TEXT("mouse_x_pulses_sum"),4.);
            LookMeasurement->SetNumberField(TEXT("mouse_y_pulses_sum"),2.);
            LookMeasurement->SetObjectField(TEXT("input_chain_after"),FinalChain);
            LookMeasurement->SetArrayField(TEXT("raw_mouse2d_observed_sum"),XY(LookRawSum));
            LookMeasurement->SetArrayField(TEXT("look_action_observed_sum"),XY(LookActionSum));
            LookMeasurement->SetArrayField(TEXT("look_action_expected_sum"),XY(ExpectedAction));
            LookMeasurement->SetArrayField(TEXT("controller_expected_delta_degrees"),XY(Expected));
            LookMeasurement->SetArrayField(TEXT("controller_delta_tolerance_degrees"),XY(AngleTolerance));
            LookMeasurement->SetNumberField(TEXT("callback_count_delta"),double(PC->GetLookCallbackCount()-LookCallbacksBefore));
            LookMeasurement->SetBoolField(TEXT("multiple_callbacks_in_one_frame"),bLookMultipleCallbacks);
            LookMeasurement->SetArrayField(TEXT("processed_frames_before_next_key_queue"),Objects(LookFrames));
            LookMeasurement->SetStringField(TEXT("controller_before"),LookBefore.ToString());
            LookMeasurement->SetStringField(TEXT("controller_after"),ControlAfter.ToString());
            LookMeasurement->SetStringField(TEXT("final_view_before"),LookViewBefore.ToString());
            LookMeasurement->SetStringField(TEXT("final_view_after"),ViewAfter.ToString());
            LookMeasurement->SetNumberField(TEXT("controller_yaw_delta_degrees"),ControllerDelta.X);
            LookMeasurement->SetNumberField(TEXT("controller_pitch_delta_degrees"),ControllerDelta.Y);
            LookMeasurement->SetNumberField(TEXT("final_view_yaw_delta_degrees"),ViewDelta.X);
            LookMeasurement->SetNumberField(TEXT("final_view_pitch_delta_degrees"),ViewDelta.Y);
            LookMeasurement->SetNumberField(TEXT("camera_controller_delta_tolerance_degrees"),.25);
            LookMeasurement->SetArrayField(TEXT("camera_controller_effective_axis_tolerance_degrees"),XY(ViewTolerance));
            LookMeasurement->SetStringField(TEXT("view_target"),GetPathNameSafe(PC->GetViewTarget()));
            if (!Check(TEXT("engine_mouse_raw_and_action_chain"),ValidChain && FinalMappingScale.Equals(LookMappingScale,1.e-8) &&
                LastLookPulse==4 && LookRawSum.Equals(PlannedRaw,1.e-5) &&
                LookActionSum.Equals(ExpectedAction,1.e-5) && !bLookMultipleCallbacks &&
                PC->GetLookCallbackCount()-LookCallbacksBefore==4,
                TEXT("Four paired key pulses must be observed once each through the actual unchanged mapping and Action modifier chain"))) return;
            if (!Check(TEXT("engine_mouse_axes_reach_controller"),
                ControllerDelta.X*Expected.X>0 && ControllerDelta.Y*Expected.Y>0 &&
                FMath::Abs(ControllerDelta.X-Expected.X)<=AngleTolerance.X &&
                FMath::Abs(ControllerDelta.Y-Expected.Y)<=AngleTolerance.Y,
                TEXT("Controller change must equal raw key displacement times live mapping scalar times effective Action gain (no DeltaTime multiplier)"))) return;
            if (!Check(TEXT("engine_mouse_axes_reach_final_view"),PC->GetViewTarget()==Character &&
                ViewDelta.X*ControllerDelta.X>0 && ViewDelta.Y*ControllerDelta.Y>0 &&
                FMath::Abs(ViewDelta.X-ControllerDelta.X)<=ViewTolerance.X &&
                FMath::Abs(ViewDelta.Y-ControllerDelta.Y)<=ViewTolerance.Y,
                TEXT("Final player viewpoint yaw/pitch deltas agree with Controller after 0.7 seconds; separate from OS input"))) return;
        }
        if (!Fixture(FVector(-2200,-1000,300),false,90)) return; MaxSpeed=0; Phase(Cruise); break;
    case Cruise:
        Inject(FVector2D(0,1)); MaxSpeed=FMath::Max(MaxSpeed,Speed);
        if (Age<1.1) break;
        if (!Check(TEXT("cruise_actual_speed"),MaxSpeed>850 && MaxSpeed<1050 && Flight->IsFlying(),FString::Printf(TEXT("max %.3f cm/s"),MaxSpeed))) return;
        MaxSpeed=0; Phase(Boost); break;
    case Boost:
        Inject(FVector2D::ZeroVector,false,false,true); MaxSpeed=FMath::Max(MaxSpeed,Speed);
        if (Age<1.2) break;
        if (!Check(TEXT("shift_without_W_actual_boost"),MaxSpeed>2300 && MaxSpeed<2600 && Flight->IsBoosting(),FString::Printf(TEXT("max %.3f cm/s"),MaxSpeed))) return;
        BrakeStartSpeed=Speed; IntermediateBrakeSpeed=-1; Phase(Brake); break;
    case Brake:
        if (Age>.08 && IntermediateBrakeSpeed<0) IntermediateBrakeSpeed=Speed;
        if (Age<1.2) break;
        if (!Check(TEXT("release_boost_smooth_brake"),BrakeStartSpeed>2300 && IntermediateBrakeSpeed>50 &&
            IntermediateBrakeSpeed<BrakeStartSpeed && Speed<5 && !Flight->IsBoosting(),
            FString::Printf(TEXT("before %.3f; after first >=80ms %.3f; final %.3f cm/s"),BrakeStartSpeed,IntermediateBrakeSpeed,Speed))) return;
        Phase(PauseStart); break;
    case PauseStart:
        Inject(FVector2D(0,1)); if (Age<.35) break;
        bAutomaticPause=true; AutoPauseAt=WallNow; PC->TogglePauseMenu(); break;
    case PauseResume:
        if (Age<.3) break;
        if (!Check(TEXT("pause_resume_no_stuck_input"),Flight->IsFlying() && Speed<.1,Flight->GetFlightDiagnostics())) return;
        // 2045 cm to the wall contact plane exceeds the 1421 cm acceleration distance.
        // The assertion below still requires the actual velocity at the named wall hit.
        if (!Fixture(FVector(-1200,0,200),false)) return;
        WallImpact.Reset(); WallPreviousPosition=Character->GetActorLocation();
        WallPreviousVelocity=Character->GetVelocity(); WallPreviousFrame=GFrameCounter;
        Phase(Wall); break;
    case Wall:
        Inject(FVector2D::ZeroVector,false,false,true);
        if (!WallImpact.IsValid())
        {
            WallPreviousPosition=P; WallPreviousVelocity=Character->GetVelocity();
            WallPreviousFrame=GFrameCounter;
        }
        if (Age<2) break;
        if (!Check(TEXT("boost_wall_capsule_sweep_at_measured_speed"),P.X>790 && P.X<=846 && Flight->IsFlying() &&
            WallImpact.IsValid() && WallImpact->GetBoolField(TEXT("primary_flight_sweep")) &&
            WallImpact->GetNumberField(TEXT("previous_frame_actual_speed_cm_s"))>=2300 &&
            WallImpact->GetNumberField(TEXT("sweep_speed_from_trace_cm_s"))>=2300 &&
            WallImpact->GetNumberField(TEXT("sweep_speed_from_trace_cm_s"))<=2600 &&
            !WallImpact->GetBoolField(TEXT("start_penetrating")),
            WallImpact.IsValid() ? FString::Printf(TEXT("%s; incoming %.3f cm/s; stopped position %s"),
                *WallImpact->GetStringField(TEXT("hit_actor")),WallImpact->GetNumberField(TEXT("sweep_speed_from_trace_cm_s")),*P.ToString())
                : TEXT("No actual hit against the authored HCFlight_Wall actor"))) return;
        if (!Fixture(FVector(-1500,0,0),true)) return; Phase(RoofGround); break;
    case RoofGround:
        if (Age<.6) break; ToggleAction(); Phase(RoofDenied); break;
    case RoofDenied:
        if (Age<.4) break;
        if (!Check(TEXT("low_roof_takeoff_rejected"),!Flight->IsFlying() && Move->IsMovingOnGround() && PC->GetStatusMessage().Contains(TEXT("上方")),PC->GetStatusMessage())) return;
        if (!Fixture(FVector(0,0,0),true)) return; Phase(Retakeoff); break;
    case Retakeoff:
        if (Age<.6) break;
        if (!bPhasePulse) { ToggleAction(); bPhasePulse=true; break; }
        if (Age<2) break;
        if (!Check(TEXT("open_retakeoff_after_roof_rejection"),Flight->IsFlying(),Flight->GetFlightDiagnostics())) return;
        if (!Fixture(FVector(-1500,0,180),false)) return; Phase(RoofAir); break;
    case RoofAir:
        Inject(FVector2D::ZeroVector,true);
        if (Age<1.5) break;
        if (!Check(TEXT("ascending_roof_capsule_sweep"),P.Z+HH<=480.8 && P.Z+HH>460 && TotalHits>HitStart, P.ToString())) return;
        if (!Fixture(FVector(-800,1800,220),false)) return; Phase(Narrow); break;
    case Narrow:
        Inject(FVector2D(.4,1));
        if (Age<1.8) break;
        if (!Check(TEXT("narrow_diagonal_actual_sweep"),P.X>300 && P.Y<=1825.8 && P.Y>1810 && TotalHits>HitStart,P.ToString())) return;
        if (!Fixture(FVector(6000,0,200),false)) return; WaterLowest=1.e9; Phase(Water); break;
    case Water:
        Inject(FVector2D::ZeroVector,false,true); WaterLowest=FMath::Min(WaterLowest,double(Flight->GetHeightAboveSea()));
        if (Age<2) break;
        if (!Check(TEXT("water_clearance_not_ground_collision"),WaterLowest>=49.5 && Flight->GetHeightAboveSea()<55 && Flight->IsFlying(),FString::Printf(TEXT("minimum %.4f cm above sea"),WaterLowest))) return;
        ToggleAction(); Phase(WaterReject); break;
    case WaterReject:
        if (Age<.4) break;
        if (!Check(TEXT("water_F_landing_rejected"),Flight->IsFlying() && !Flight->IsLanding() && PC->GetStatusMessage().Contains(TEXT("不能降落")),PC->GetStatusMessage())) return;
        if (!Fixture(FVector(0,0,9800),false)) return; CeilingPeak=0; Phase(Ceiling); break;
    case Ceiling:
        Inject(FVector2D::ZeroVector,true); CeilingPeak=FMath::Max(CeilingPeak,double(Flight->GetHeightAboveSea()));
        if (Age<2) break;
        if (!Check(TEXT("actual_100m_sea_ceiling"),CeilingPeak<=10000.5 && CeilingPeak>9990,FString::Printf(TEXT("maximum feet height %.4f cm"),CeilingPeak))) return;
        if (!Fixture(FVector(12700,0,500),false)) return; PositionBefore=Character->GetActorLocation(); Phase(Boundary); break;
    case Boundary:
        if (Age<1.8) break;
        if (!Check(TEXT("100m_beyond_area_soft_return"),P.X<PositionBefore.X-90 && P.X>12000 && Flight->IsFlying(),P.ToString())) return;
        if (!Fixture(FVector(0,-1600,150),true)) return; Phase(SlopeGround); break;
    case SlopeGround:
        if (Age<.6) break;
        if (!Check(TEXT("actual_slope_support"),Move->IsMovingOnGround() && Move->CurrentFloor.IsWalkableFloor() &&
            Move->CurrentFloor.HitResult.ImpactNormal.Z>.97 && Move->CurrentFloor.HitResult.ImpactNormal.Z<.999,Move->CurrentFloor.HitResult.ImpactNormal.ToString())) return;
        ToggleAction(); Phase(SlopeTakeoff); break;
    case SlopeTakeoff:
        if (Age<1.7) break;
        if (!Check(TEXT("slope_takeoff"),Flight->IsFlying(),Flight->GetFlightDiagnostics())) return;
        ToggleAction(); Phase(SlopeLand); break;
    case SlopeLand:
        if (Flight->IsFlying() && Age<7) break;
        if (!Check(TEXT("F_actual_slope_landing"),!Flight->IsFlying() && Move->IsMovingOnGround() && Move->CurrentFloor.IsWalkableFloor() &&
            Move->CurrentFloor.HitResult.ImpactNormal.Z<.999 && Speed<10,Flight->GetFlightDiagnostics())) return;
        if (!Fixture(FVector(0,0,0),true)) return; Phase(FinalGround); break;
    case FinalGround:
        if (Age<.7) break;
        if (!Check(TEXT("ground_save_after_flight"),PC->SaveGameNow(),PC->GetStatusMessage())) return;
        PositionBefore=P;
        if (!Check(TEXT("ground_load_actual_entry"),PC->LoadGameNow(),PC->GetStatusMessage())) return;
        Phase(LoadCheck); break;
    case LoadCheck:
        if (Age<.6) break;
        if (!Check(TEXT("load_restores_ground_and_rules"),!Flight->IsFlying() && Move->IsMovingOnGround() &&
            FVector::Distance(PositionBefore,P)<10 && Combat->CanUseCombat() && PC->CanReachInteraction(InteractionSwitch),Flight->GetFlightDiagnostics())) return;
        if (!Check(TEXT("physics_interaction_restored"),Move->bEnablePhysicsInteraction,TEXT("Read actual CMC flag after landing/load"))) return;
        Capture(TEXT("06_ground_restored")); Phase(Done); break;
    case Done:
        if (Age<1) break;
        Finish(TEXT("PASS"),TEXT("Bounded fixture Action/engine-key/function regression only; OS mouse, packaged game, art and remaining story/NPC tests NOT_RUN.")); break;
    default: Finish(TEXT("FAIL"),TEXT("Invalid bounded test phase")); break;
    }
}

TSharedPtr<FJsonObject> AHCM5VS2FlightReviewDirector::Snapshot() const
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("phase"),PhaseName(CurrentPhase));
    R->SetNumberField(TEXT("active_seconds"),ActiveSeconds); R->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());
    R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    if (Flight) R->SetObjectField(TEXT("flight"),Parse(Flight->GetFlightDiagnostics()));
    if (Character)
    {
        R->SetArrayField(TEXT("position"),XYZ(Character->GetActorLocation())); R->SetArrayField(TEXT("velocity"),XYZ(Character->GetVelocity()));
        R->SetStringField(TEXT("pawn_class"),Character->GetClass()->GetPathName());
        R->SetStringField(TEXT("animation_class"),GetPathNameSafe(Character->GetMesh()->GetAnimClass()));
        const UHCM5VS2FlightVisualComponent* Visual=Character->FindComponentByClass<UHCM5VS2FlightVisualComponent>();
        R->SetBoolField(TEXT("flight_visual_component_present"),Visual!=nullptr);
        if (Visual)
        {
            auto V=Parse(Visual->GetFlightVisualDiagnostics());
            V->SetBoolField(TEXT("component_tick_enabled"),Visual->IsComponentTickEnabled());
            V->SetBoolField(TEXT("component_active"),Visual->IsActive());
            V->SetBoolField(TEXT("flight_available_now"),Flight && Flight->IsFlightAvailable());
            V->SetNumberField(TEXT("body_chest_bone_index_now"),Character->GetMesh()->GetBoneIndex(TEXT("Chest")));
            V->SetStringField(TEXT("light_material"),GetPathNameSafe(Visual->LightMaterial));
            R->SetObjectField(TEXT("flight_visual"),V);
        }
    }
    if (PC)
    {
        FVector View; FRotator Rot; PC->GetPlayerViewPoint(View,Rot);
        R->SetArrayField(TEXT("final_view_location"),XYZ(View)); R->SetStringField(TEXT("final_view_rotation"),Rot.ToString());
        R->SetStringField(TEXT("control_rotation"),PC->GetControlRotation().ToString());
        R->SetStringField(TEXT("view_target"),GetPathNameSafe(PC->GetViewTarget()));
        R->SetBoolField(TEXT("first_person"),PC->IsFirstPersonPerspective()); R->SetBoolField(TEXT("focused"),PC->IsGameplayFocused());
    }
    return R;
}
void AHCM5VS2FlightReviewDirector::Hit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Result)
{
    if (!bActive || !Other) return;
    ++TotalHits;
    if (Hits.Num()>=500) return;
    auto R=Snapshot(); R->SetStringField(TEXT("hit_actor"),Other->GetPathName()); R->SetStringField(TEXT("hit_component"),GetPathNameSafe(OtherComponent));
    // Scoped CMC movement can defer this callback until after velocity is reduced.
    // PhysFlying's primary sweep uses Velocity * deltaTime. Retain the sweep endpoints
    // and previous real frame separately; never label callback velocity as incoming.
    const float FrameDt=GetWorld()->GetDeltaSeconds();
    const FVector SweepVelocity=FrameDt>SMALL_NUMBER ? (Result.TraceEnd-Result.TraceStart)/FrameDt : FVector::ZeroVector;
    R->SetArrayField(TEXT("callback_movement_velocity_cm_s"),XYZ(Character->GetCharacterMovement()->Velocity));
    R->SetArrayField(TEXT("requested_velocity_at_hit_cm_s"),XYZ(Flight->RequestedFlightVelocity));
    R->SetArrayField(TEXT("sweep_trace_start"),XYZ(Result.TraceStart));
    R->SetArrayField(TEXT("sweep_trace_end"),XYZ(Result.TraceEnd));
    R->SetNumberField(TEXT("frame_delta_seconds"),FrameDt);
    R->SetArrayField(TEXT("sweep_velocity_from_trace_cm_s"),XYZ(SweepVelocity));
    R->SetNumberField(TEXT("sweep_speed_from_trace_cm_s"),SweepVelocity.Size());
    R->SetArrayField(TEXT("normal"),XYZ(Result.ImpactNormal)); R->SetArrayField(TEXT("impact_point"),XYZ(Result.ImpactPoint));
    R->SetBoolField(TEXT("start_penetrating"),Result.bStartPenetrating); R->SetNumberField(TEXT("penetration_depth"),Result.PenetrationDepth); Hits.Add(R);
    if (CurrentPhase==Wall && !WallImpact.IsValid() && Other->ActorHasTag(TEXT("HCFlight_Wall")) && Result.ImpactNormal.X<-.9)
    {
        R->SetNumberField(TEXT("previous_frame"),double(WallPreviousFrame));
        R->SetArrayField(TEXT("previous_frame_actual_position"),XYZ(WallPreviousPosition));
        R->SetArrayField(TEXT("previous_frame_actual_velocity_cm_s"),XYZ(WallPreviousVelocity));
        R->SetNumberField(TEXT("previous_frame_actual_speed_cm_s"),WallPreviousVelocity.Size());
        R->SetBoolField(TEXT("primary_flight_sweep"),GFrameCounter==WallPreviousFrame+1 && FrameDt>SMALL_NUMBER &&
            FVector::Distance(Result.TraceStart,WallPreviousPosition)<.5 && SweepVelocity.X>0 &&
            FMath::Abs(SweepVelocity.Y)<1 && FMath::Abs(SweepVelocity.Z)<1);
        WallImpact=R;
    }
}
void AHCM5VS2FlightReviewDirector::Capture(const FString& Label)
{
    auto R=Snapshot(); const FString File=Directory/(Label+TEXT(".png")); R->SetStringField(TEXT("path"),File);
    R->SetStringField(TEXT("scope"),TEXT("Actual player viewpoint; no camera actor or final camera rotation written"));
    if (!FScreenshotRequest::IsScreenshotRequested()) { FScreenshotRequest::RequestScreenshot(File,true,false); R->SetStringField(TEXT("status"),TEXT("REQUESTED")); }
    else R->SetStringField(TEXT("status"),TEXT("NOT_RUN_SCREENSHOT_ALREADY_PENDING")); Captures.Add(R);
}
void AHCM5VS2FlightReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if (!bActive || Event.Event!=IE_Pressed) return;
    if (Event.Key==EKeys::Escape) { bStopped=true; Finish(TEXT("STOPPED"),TEXT("Esc latched: no further control or automatic quit")); }
    else if (Event.Key==EKeys::P) { ++UserPausePresses; bAutomaticPause=false; }
}
void AHCM5VS2FlightReviewDirector::Write(const FString& Status,const FString& Detail)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.FlightReview.v1"));
    R->SetStringField(TEXT("status"),Status); R->SetStringField(TEXT("detail"),Detail);
    R->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName()); R->SetStringField(TEXT("directory"),Directory);
    R->SetStringField(TEXT("input_scope"),TEXT("Enhanced Action injection; paired engine MouseX/Y; explicit gameplay function calls. NO OS INPUT."));
    R->SetStringField(TEXT("os_input"),TEXT("NOT_RUN")); R->SetStringField(TEXT("packaged_runtime"),TEXT("NOT_RUN"));
    R->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW")); R->SetStringField(TEXT("video"),TEXT("NOT_RUN_DIRECTOR_DOES_NOT_RECORD_VIDEO"));
    R->SetArrayField(TEXT("not_run"),{MakeShared<FJsonValueString>(TEXT("Real-map story interior/stage2 courtyard/arrival gating")),
        MakeShared<FJsonValueString>(TEXT("Flying through real NPCs without damage")),MakeShared<FJsonValueString>(TEXT("Alt-Tab/OS focus and perspective switching")),
        MakeShared<FJsonValueString>(TEXT("Independent process save relaunch")),MakeShared<FJsonValueString>(TEXT("OS mouse and keyboard; packaged release"))});
    R->SetBoolField(TEXT("stop_latched"),bStopped); R->SetNumberField(TEXT("user_pause_presses"),UserPausePresses);
    R->SetNumberField(TEXT("active_seconds"),ActiveSeconds); R->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedWall);
    R->SetNumberField(TEXT("total_capsule_hits"),TotalHits); R->SetNumberField(TEXT("retained_hit_samples_max_500"),Hits.Num());
    R->SetStringField(TEXT("private_save_slot"),SaveSlot); R->SetNumberField(TEXT("fixture_teleports"),Fixtures.Num());
    R->SetArrayField(TEXT("checks"),Objects(Checks)); R->SetArrayField(TEXT("samples"),Objects(Samples));
    R->SetArrayField(TEXT("fixtures"),Objects(Fixtures)); R->SetArrayField(TEXT("actual_capsule_hits"),Objects(Hits));
    if (LookMeasurement.IsValid()) R->SetObjectField(TEXT("engine_mouse_final_view_measurement"),LookMeasurement);
    if (WallImpact.IsValid()) R->SetObjectField(TEXT("boost_wall_first_impact"),WallImpact);
    for (auto& C:Captures) if (C->GetStringField(TEXT("status"))==TEXT("REQUESTED") && IFileManager::Get().FileSize(*C->GetStringField(TEXT("path")))>32)
        C->SetStringField(TEXT("status"),TEXT("FILE_WRITTEN_NOT_ART_ACCEPTANCE"));
    R->SetArrayField(TEXT("captures"),Objects(Captures));
    FString Text; FJsonSerializer::Serialize(R,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("flight_review.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void AHCM5VS2FlightReviewDirector::Finish(const FString& Status,const FString& Detail)
{
    if (!bActive) return;
    bActive=false; bAutomaticPause=false;
    // Release only automation-owned flight inputs. Stop remains latched; never resume or relaunch.
    if (PC && Flight) { Inject(); Flight->ClearFlightInput(); Character->GetCharacterMovement()->StopMovementImmediately(); }
    Write(Status,Detail); SetActorTickEnabled(false);
    if (!bStopped && bAutoQuit && PC) PC->ConsoleCommand(TEXT("quit"),true);
}
void AHCM5VS2FlightReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bActive) Finish(TEXT("STOPPED"),TEXT("World closed before bounded tests completed"));
    if (Viewport.IsValid() && InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle);
    if (Character) Character->GetCapsuleComponent()->OnComponentHit.RemoveDynamic(this,&AHCM5VS2FlightReviewDirector::Hit);
    Super::EndPlay(Reason);
}
