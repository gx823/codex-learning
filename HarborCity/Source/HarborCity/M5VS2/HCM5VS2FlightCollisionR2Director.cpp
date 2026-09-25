#include "HCM5VS2FlightCollisionR2Director.h"
#include "HCM5VS2FlightComponent.h"
#include "HCM5VS2FlightVisualComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M4/HCM4CombatComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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
enum EPhase { Warmup, Takeoff, RoofSetup, RoofBoost, RoofClear,
    DiveSetup, DiveBoost, DiveClear, PassageSetup, PassageBoost,
    BendSetup, BendBoost, BendClear, WaterSetup, WaterDive, WaterReject, Done };
const TCHAR* PhaseName(int32 P)
{
    static const TCHAR* Names[]={TEXT("Warmup"),TEXT("ActualFTakeoff"),TEXT("RoofSetup"),TEXT("RoofBoost"),TEXT("RoofClear"),
        TEXT("DiveSetup"),TEXT("HighDryPlatformDive"),TEXT("DiveClear"),TEXT("PassageSetup"),TEXT("StraightNarrowPassage"),
        TEXT("BendSetup"),TEXT("HighSpeedObliqueWall"),TEXT("BendClear"),TEXT("WaterSetup"),TEXT("WaterBoostDive"),
        TEXT("WaterFLandingRejected"),TEXT("Done")};
    return P>=0 && P<UE_ARRAY_COUNT(Names)?Names[P]:TEXT("INVALID");
}
bool IsMeasuredPhase(int32 P)
{ return P==RoofBoost || P==DiveBoost || P==PassageBoost || P==BendBoost || P==WaterDive; }
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> Objects(const TArray<TSharedPtr<FJsonObject>>& Rows)
{ TArray<TSharedPtr<FJsonValue>> Result; for (const auto& Row:Rows) Result.Add(MakeShared<FJsonValueObject>(Row)); return Result; }
TSharedPtr<FJsonObject> Parse(const FString& Text)
{
    TSharedPtr<FJsonObject> Result;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result) || !Result) Result=MakeShared<FJsonObject>();
    return Result;
}
}

AHCM5VS2FlightCollisionR2Director::AHCM5VS2FlightCollisionR2Director()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}
void AHCM5VS2FlightCollisionR2Director::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2FlightCollisionR2"))) return;
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/FlightCollisionR2/Run_");
    const FString Map=GetWorld()->GetOutermost()->GetName();
    if (!Map.StartsWith(Prefix)) return;
    const FString Token=Map.Mid(Prefix.Len(),12);
    if (Map!=Prefix+Token+TEXT("/L_FlightCollisionR2")) return;
    for (TCHAR C:Token) if (!FChar::IsHexDigit(C)) return;
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root) || FPaths::IsRelative(Root)) return;
    Root=FPaths::ConvertRelativePathToFull(Root); FPaths::NormalizeDirectoryName(Root);
    if (!FPaths::CollapseRelativeDirectories(Root) || !FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime"))) return;
    Directory=Root/(TEXT("FlightCollisionR2_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true)) return;
    bActive=true; StartedWall=FPlatformTime::Seconds();
    bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    SetActorTickEnabled(true);
    Write(TEXT("RUNNING"),TEXT("Waiting for actual selected Hero, production flight and focused viewport."));
#endif
}
bool AHCM5VS2FlightCollisionR2Director::Start()
{
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));
    Character=PC?Cast<AHCM1Character>(PC->GetPawn()):nullptr;
    UGameViewportClient* V=GetWorld()->GetGameViewport();
    if (!PC || !Character || !V || !V->Viewport || !PC->IsGameplayFocused()) return false;
    Viewport=V;
    if (!InputHandle.IsValid()) InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2FlightCollisionR2Director::Input);
    Flight=PC->GetFlightComponent();
    if (!Check(TEXT("production_flight_available"),Flight && Flight->IsFlightAvailable() && PC->GetM3Experience() &&
        Cast<UEnhancedPlayerInput>(PC->PlayerInput),TEXT("Real controller/CMC/flight and normal combat owner required"))) return false;
    const AHCM5VS2FlightBounds* B=Flight->GetFlightBounds();
    if (!Check(TEXT("frozen_production_parameters"),Flight->CruiseSpeed==1000 && Flight->BoostSpeed==2500 &&
        Flight->VerticalSpeed==600 && Flight->FlightAcceleration==2200 && Flight->FlightBraking==3000 &&
        B->SeaLevelZ==-150 && B->CeilingHeight==10000 && B->WaterClearance==50 && B->SoftReturnDistance==10000 &&
        B->FlightAreaCenter==FVector2D::ZeroVector && B->FlightAreaHalfExtent==FVector2D(2500,2500) &&
        PC->GetOnFootLookDegreesPerActionUnit().Equals(FVector2D(1.5,1.5),.0001),Flight->GetFlightDiagnostics())) return false;
    const TArray<AActor*> Targets={HighRoof,HighPlatform,PassageNorth,PassageSouth,BendNorth,BendSouth,WaterSurface};
    const TCHAR* FixtureTags[]={TEXT("HCFCR2_HighRoof"),TEXT("HCFCR2_HighPlatform"),TEXT("HCFCR2_PassageNorth"),TEXT("HCFCR2_PassageSouth"),
        TEXT("HCFCR2_BendNorth"),TEXT("HCFCR2_BendSouth"),TEXT("HCFCR2_WaterSurface")};
    for (int32 I=0;I<Targets.Num();++I)
    {
        AActor* Target=Targets[I];
        const UStaticMeshComponent* Mesh=IsValid(Target)?Target->FindComponentByClass<UStaticMeshComponent>():nullptr;
        if (!Check(TEXT("exact_collision_fixture_")+FString(FixtureTags[I]),Mesh && Target->GetLevel()==GetLevel() && Target->ActorHasTag(FixtureTags[I]) &&
            GetPathNameSafe(Mesh->GetStaticMesh())==TEXT("/Engine/BasicShapes/Cube.Cube") && Mesh->GetCollisionProfileName()==TEXT("BlockAll"),
            GetPathNameSafe(Target))) return false;
    }
    for (const FName Action:{FName(TEXT("Move")),FName(TEXT("Jump")),FName(TEXT("Sprint")),FName(TEXT("FlightToggle")),FName(TEXT("FlightDescend"))})
        if (!Check(TEXT("actual_action_")+Action.ToString(),PC->GetM1Action(Action)!=nullptr,Action.ToString())) return false;
    Character->GetCapsuleComponent()->OnComponentHit.AddDynamic(this,&AHCM5VS2FlightCollisionR2Director::Hit);
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    ChangePhase(Warmup); return true;
}
bool AHCM5VS2FlightCollisionR2Director::Check(const FString& Name,bool Passed,const FString& Detail)
{
    auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"),Name);
    Row->SetStringField(TEXT("status"),Passed?TEXT("PASS"):TEXT("FAIL")); Row->SetStringField(TEXT("detail"),Detail);
    Row->SetStringField(TEXT("phase"),PhaseName(CurrentPhase)); Row->SetNumberField(TEXT("active_seconds"),ActiveSeconds);
    Checks.Add(Row);
    if (!Passed) Finish(TEXT("FAIL"),Name+TEXT(": ")+Detail);
    return Passed;
}
void AHCM5VS2FlightCollisionR2Director::Inject(bool Boost,bool Ascend,bool Descend)
{
    if (!PC) return;
    UEnhancedPlayerInput* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);
    if (!Input) return;
    Input->InjectInputForAction(PC->GetM1Action(TEXT("Move")),FInputActionValue(FVector2D::ZeroVector));
    Input->InjectInputForAction(PC->GetM1Action(TEXT("Sprint")),FInputActionValue(Boost));
    Input->InjectInputForAction(PC->GetM1Action(TEXT("Jump")),FInputActionValue(Ascend));
    Input->InjectInputForAction(PC->GetM1Action(TEXT("FlightDescend")),FInputActionValue(Descend));
    if (bReleaseToggle)
    {
        Input->InjectInputForAction(PC->GetM1Action(TEXT("FlightToggle")),FInputActionValue(false));
        bReleaseToggle=false;
    }
}
void AHCM5VS2FlightCollisionR2Director::ToggleFlight()
{
    CastChecked<UEnhancedPlayerInput>(PC->PlayerInput)->InjectInputForAction(PC->GetM1Action(TEXT("FlightToggle")),FInputActionValue(true));
    bReleaseToggle=true;
}
void AHCM5VS2FlightCollisionR2Director::ChangePhase(int32 Next)
{
    CurrentPhase=Next; PhaseStarted=ActiveSeconds;
    if (IsMeasuredPhase(Next))
    {
        FirstImpact.Reset(); PhaseHitCount=UnexpectedHitCount=0; PeakSpeed=0;
        PreviousPosition=Character->GetActorLocation(); PreviousVelocity=Character->GetVelocity(); PreviousFrame=GFrameCounter;
    }
    Write(TEXT("RUNNING"),FString(TEXT("Entered "))+PhaseName(Next));
}
bool AHCM5VS2FlightCollisionR2Director::AirFixture(const FVector& Feet,const FRotator& Look,AActor* Target)
{
    if (!Check(TEXT("fixture_requires_actual_prior_takeoff"),Flight->IsFlying() &&
        Character->GetCharacterMovement()->MovementMode==MOVE_Flying,TEXT("No forced flying state/movement mode in fixture setup"))) return false;
    Inject(); PC->FlushPressedKeys(); Flight->ClearFlightInput();
    auto* Move=Character->GetCharacterMovement(); auto* Capsule=Character->GetCapsuleComponent();
    const FVector Destination=Feet+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightCollisionR2Fixture),false,Character);
    if (!Check(TEXT("fixture_capsule_clear"),!GetWorld()->OverlapBlockingTestByChannel(Destination,FQuat::Identity,
        Capsule->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params),Destination.ToString())) return false;
    auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("kind"),TEXT("TELEPORT_AND_INITIAL_LOOK_FIXTURE_NOT_MEASURED_FLIGHT_OR_MOUSE"));
    Row->SetArrayField(TEXT("before"),XYZ(Character->GetActorLocation())); Row->SetArrayField(TEXT("requested_feet"),XYZ(Feet));
    Row->SetStringField(TEXT("initial_look_fixture"),Look.ToString());
    Move->StopActiveMovement(); Move->StopMovementImmediately();
    Character->SetActorLocationAndRotation(Destination,FRotator(0,Look.Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    PC->SetControlRotation(Look);
    Row->SetArrayField(TEXT("actual_after"),XYZ(Character->GetActorLocation())); Fixtures.Add(Row);
    ExpectedTarget=Target; FirstImpact.Reset(); PhaseHitCount=UnexpectedHitCount=0;
    return Check(TEXT("fixture_exact_teleport"),FVector::Distance(Destination,Character->GetActorLocation())<.1,Destination.ToString());
}
bool AHCM5VS2FlightCollisionR2Director::ObserveFlightFrame(float DeltaSeconds)
{
    if (Frames.Num()>=24000) return Check(TEXT("bounded_frame_storage"),false,TEXT("Hard cap; never silently truncate a measured trajectory"));
    auto Row=Snapshot();
    Row->SetNumberField(TEXT("frame_delta_seconds"),DeltaSeconds);
    Frames.Add(Row);
    MinFrameSeconds=FMath::Min(MinFrameSeconds,double(DeltaSeconds)); MaxFrameSeconds=FMath::Max(MaxFrameSeconds,double(DeltaSeconds));
    MeasuredFrameSeconds+=DeltaSeconds;
    PeakSpeed=FMath::Max(PeakSpeed,Character->GetVelocity().Size());
    const UCapsuleComponent* Capsule=Character->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightCollisionR2Space),false,Character);
    const bool Overlap=GetWorld()->OverlapBlockingTestByChannel(Character->GetActorLocation(),FQuat::Identity,
        Capsule->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params);
    Row->SetBoolField(TEXT("actual_capsule_blocking_overlap"),Overlap);
    if (Overlap) return Check(TEXT("measured_capsule_no_penetration"),false,Character->GetActorLocation().ToString());
    if (!Flight->IsFlying() || Character->GetCharacterMovement()->MovementMode!=MOVE_Flying)
        return Check(TEXT("measured_flight_mode_preserved"),false,Flight->GetFlightDiagnostics());
    if (FirstImpact)
    {
        const double Support=Capsule->GetScaledCapsuleRadius()+
            (Capsule->GetScaledCapsuleHalfHeight()-Capsule->GetScaledCapsuleRadius())*FMath::Abs(ContactNormal.Z);
        const double Gap=FVector::DotProduct(Character->GetActorLocation()-ContactPoint,ContactNormal)-Support;
        Row->SetNumberField(TEXT("first_contact_plane_capsule_gap_cm"),Gap);
        if (Gap<-.1) return Check(TEXT("named_contact_plane_not_crossed"),false,FString::Printf(TEXT("support gap %.6f cm"),Gap));
    }
    PreviousPosition=Character->GetActorLocation(); PreviousVelocity=Character->GetVelocity(); PreviousFrame=GFrameCounter;
    return true;
}
bool AHCM5VS2FlightCollisionR2Director::CheckImpact(const FString& Name,const FVector& RequiredNormal)
{
    const bool Valid=FirstImpact && FirstImpact->GetBoolField(TEXT("primary_flight_sweep")) &&
        !FirstImpact->GetBoolField(TEXT("start_penetrating")) &&
        FirstImpact->GetNumberField(TEXT("previous_frame_actual_speed_cm_s"))>=2300 &&
        FirstImpact->GetNumberField(TEXT("sweep_speed_from_trace_cm_s"))>=2300 &&
        FirstImpact->GetNumberField(TEXT("sweep_speed_from_trace_cm_s"))<=2600 &&
        FVector::DotProduct(ContactNormal,RequiredNormal)>.95 && UnexpectedHitCount==0;
    return Check(Name,Valid,FirstImpact
        ? FString::Printf(TEXT("actor %s; primary %.0f; incoming %.3f cm/s; unexpected hits %d"),
            *FirstImpact->GetStringField(TEXT("hit_actor")),FirstImpact->GetBoolField(TEXT("primary_flight_sweep"))?1.:0.,
            FirstImpact->GetNumberField(TEXT("sweep_speed_from_trace_cm_s")),UnexpectedHitCount)
        : TEXT("No measured hit on the assigned physical Actor/Component; timeout is not PASS"));
}
void AHCM5VS2FlightCollisionR2Director::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActive) return;
    if (!InputHandle.IsValid()) if (UGameViewportClient* V=GetWorld()->GetGameViewport())
    { Viewport=V; InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2FlightCollisionR2Director::Input); }
    if (!bReady)
    {
        if (Start()) bReady=true;
        else if (bActive && FPlatformTime::Seconds()-StartedWall>15) Finish(TEXT("FAIL"),TEXT("Initial controller/focus timeout"));
        return;
    }
    if (!PC->IsGameplayFocused())
    {
        if (!PC->IsPauseMenuOpen() && !UGameplayStatics::IsGamePaused(this))
        { Inject(); Flight->ClearFlightInput(); PC->TogglePauseMenu(); ++FocusPauseRequests; }
        return; // Never activate another window or automatically resume a focus pause.
    }
    if (PC->IsPauseMenuOpen() || UGameplayStatics::IsGamePaused(this)) return;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0) return;
    ActiveSeconds+=DeltaSeconds;
    if (ActiveSeconds>90) { Finish(TEXT("FAIL"),TEXT("Ninety active seconds exhausted; remaining phases NOT_RUN")); return; }
    const double Age=ActiveSeconds-PhaseStarted;
    Inject();
    if (IsMeasuredPhase(CurrentPhase) && !ObserveFlightFrame(DeltaSeconds)) return;
    switch (CurrentPhase)
    {
    case Warmup:
        if (Age<3) break;
        if (!Check(TEXT("actual_grounded_start"),Character->GetCharacterMovement()->IsMovingOnGround(),Snapshot()->GetStringField(TEXT("position_text")))) return;
        BeforeTakeoff=Character->GetActorLocation(); ToggleFlight(); ChangePhase(Takeoff); break;
    case Takeoff:
        if (Age<1.5) break;
        if (!Check(TEXT("actual_F_takeoff"),Flight->IsFlying() && Character->GetActorLocation().Z-BeforeTakeoff.Z>80 &&
            PC->GetCombatComponent()->GetWeaponMode()==EHCM4WeaponMode::Unarmed,Flight->GetFlightDiagnostics())) return;
        if (!AirFixture(FVector(-6500,-5500,3000),FRotator(65,0,0),HighRoof)) return;
        ChangePhase(RoofSetup); break;
    case RoofSetup:
    case DiveSetup:
    case PassageSetup:
    case BendSetup:
    case WaterSetup:
        if (Age<.25) break;
        {
            const double RequiredPitch=CurrentPhase==RoofSetup?65.:(CurrentPhase==DiveSetup || CurrentPhase==WaterSetup)?-65.:0.;
            if (!Check(TEXT("fixture_control_pitch_readback"),FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Pitch,RequiredPitch))<.1,
                PC->GetControlRotation().ToString())) return;
            ChangePhase(CurrentPhase==RoofSetup?RoofBoost:CurrentPhase==DiveSetup?DiveBoost:
                CurrentPhase==PassageSetup?PassageBoost:CurrentPhase==BendSetup?BendBoost:WaterDive);
        } break;
    case RoofBoost:
        Inject(true);
        if (!FirstImpact && Age<3.5) break;
        if (!CheckImpact(TEXT("high_speed_roof_upward_primary_sweep"),FVector(0,0,-1))) return;
        ChangePhase(RoofClear); break;
    case RoofClear:
        Inject(false,false,true);
        if (Age<.6) break;
        if (!Check(TEXT("roof_can_depart_after_contact"),Character->GetVelocity().Z<-100 && Flight->IsFlying(),Flight->GetFlightDiagnostics())) return;
        if (!AirFixture(FVector(3500,-5500,5200),FRotator(-65,0,0),HighPlatform)) return;
        ChangePhase(DiveSetup); break;
    case DiveBoost:
        Inject(true);
        if (!FirstImpact && Age<3.5) break;
        if (!CheckImpact(TEXT("high_speed_dry_platform_dive_primary_sweep"),FVector::UpVector)) return;
        ChangePhase(DiveClear); break;
    case DiveClear:
        Inject(false,true,false);
        if (Age<.6) break;
        if (!Check(TEXT("platform_contact_does_not_force_landing"),Flight->IsFlying() && Character->GetVelocity().Z>100,Flight->GetFlightDiagnostics())) return;
        if (!AirFixture(FVector(-7000,3000,220),FRotator::ZeroRotator,nullptr)) return;
        ChangePhase(PassageSetup); break;
    case PassageBoost:
        Inject(true);
        if (Character->GetActorLocation().X>-2000) bSawPassageExit=true;
        if (!bSawPassageExit && Age<3.5) break;
        if (!Check(TEXT("narrow_straight_actual_high_speed_transit"),bSawPassageExit && PeakSpeed>=2300 &&
            Character->GetVelocity().Size()>=2300 && Character->GetVelocity().Size()<=2600 && PhaseHitCount==0 &&
            FMath::Abs(Character->GetActorLocation().Y-3000)<1,Flight->GetFlightDiagnostics())) return;
        if (!AirFixture(FVector(-7000,6500,220),FRotator::ZeroRotator,BendNorth)) return;
        ChangePhase(BendSetup); break;
    case BendBoost:
        Inject(true);
        if (!FirstImpact && Age<3.5) break;
        if (!CheckImpact(TEXT("narrow_oblique_wall_at_measured_boost_speed"),-BendNorth->GetActorRightVector())) return;
        ChangePhase(BendClear); break;
    case BendClear:
        Inject(false,true,false);
        if (Age<.6) break;
        if (!Check(TEXT("oblique_wall_control_remains_live"),Flight->IsFlying() && Character->GetVelocity().Z>100,Flight->GetFlightDiagnostics())) return;
        if (!AirFixture(FVector(3500,5500,3000),FRotator(-65,0,0),nullptr)) return;
        LowestSeaClearance=1.e9; bSawWaterPlateau=false; ChangePhase(WaterSetup); break;
    case WaterDive:
        Inject(true);
        LowestSeaClearance=FMath::Min(LowestSeaClearance,double(Flight->GetHeightAboveSea()));
        bSawWaterPlateau=Flight->GetHeightAboveSea()<55 && FMath::Abs(Character->GetVelocity().Z)<5;
        if (!bSawWaterPlateau && Age<4.5) break;
        if (!Check(TEXT("boost_dive_brakes_above_water_without_collision"),bSawWaterPlateau && PeakSpeed>=2300 && PeakSpeed<=2600 &&
            LowestSeaClearance>=49.5 && PhaseHitCount==0 && Flight->IsFlying(),
            FString::Printf(TEXT("peak %.3f cm/s; lowest %.6f cm above sea; actual hits %d"),PeakSpeed,LowestSeaClearance,PhaseHitCount))) return;
        ToggleFlight(); ChangePhase(WaterReject); break;
    case WaterReject:
        if (Age<.4) break;
        if (!Check(TEXT("actual_F_water_landing_refused"),Flight->IsFlying() && !Flight->IsLanding() &&
            PC->GetStatusMessage().Contains(TEXT("不能降落")),PC->GetStatusMessage())) return;
        ChangePhase(Done); break;
    case Done:
        if (Age<.5) break;
        Finish(TEXT("PASS"),TEXT("Five bounded production-CMC collision/clearance scenarios only; OS input, packaged game, story/NPC rules and art NOT_RUN.")); break;
    default: Finish(TEXT("FAIL"),TEXT("Unknown collision phase")); break;
    }
}
TSharedPtr<FJsonObject> AHCM5VS2FlightCollisionR2Director::Snapshot() const
{
    auto Row=MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("phase"),PhaseName(CurrentPhase)); Row->SetNumberField(TEXT("active_seconds"),ActiveSeconds);
    Row->SetNumberField(TEXT("frame"),double(GFrameCounter));
    if (Character)
    {
        Row->SetStringField(TEXT("hero_class"),Character->GetClass()->GetPathName());
        Row->SetArrayField(TEXT("position"),XYZ(Character->GetActorLocation()));
        Row->SetStringField(TEXT("position_text"),Character->GetActorLocation().ToString());
        Row->SetArrayField(TEXT("actual_velocity_cm_s"),XYZ(Character->GetVelocity()));
        Row->SetNumberField(TEXT("actual_speed_cm_s"),Character->GetVelocity().Size());
        Row->SetNumberField(TEXT("capsule_radius_cm"),Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
        Row->SetNumberField(TEXT("capsule_half_height_cm"),Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    }
    if (Flight) Row->SetArrayField(TEXT("requested_flight_velocity_cm_s"),XYZ(Flight->RequestedFlightVelocity));
    if (PC)
    {
        FVector Location; FRotator Rotation; PC->GetPlayerViewPoint(Location,Rotation);
        Row->SetStringField(TEXT("control_rotation"),PC->GetControlRotation().ToString());
        Row->SetArrayField(TEXT("final_camera_position"),XYZ(Location)); Row->SetStringField(TEXT("final_camera_rotation"),Rotation.ToString());
        Row->SetBoolField(TEXT("focused"),PC->IsGameplayFocused()); Row->SetBoolField(TEXT("pause_menu"),PC->IsPauseMenuOpen());
    }
    return Row;
}
void AHCM5VS2FlightCollisionR2Director::Hit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse,const FHitResult& Result)
{
    if (!bActive || !IsMeasuredPhase(CurrentPhase) || !Other) return;
    ++PhaseHitCount;
    const UStaticMeshComponent* ExpectedMesh=ExpectedTarget?ExpectedTarget->FindComponentByClass<UStaticMeshComponent>():nullptr;
    if (Other!=ExpectedTarget || OtherComponent!=ExpectedMesh) ++UnexpectedHitCount;
    if (Hits.Num()>=1000) { Finish(TEXT("FAIL"),TEXT("Hit evidence cap reached; no silent truncation")); return; }
    auto Row=Snapshot();
    const float DT=GetWorld()->GetDeltaSeconds();
    const FVector SweepVelocity=DT>SMALL_NUMBER?(Result.TraceEnd-Result.TraceStart)/DT:FVector::ZeroVector;
    const bool Primary=GFrameCounter==PreviousFrame+1 && DT>SMALL_NUMBER &&
        FVector::Distance(Result.TraceStart,PreviousPosition)<.5 &&
        FVector::Distance(Result.TraceEnd-Result.TraceStart,Flight->RequestedFlightVelocity*DT)<.5;
    Row->SetStringField(TEXT("hit_actor"),Other->GetPathName()); Row->SetStringField(TEXT("hit_component"),GetPathNameSafe(OtherComponent));
    Row->SetBoolField(TEXT("assigned_target_and_component"),Other==ExpectedTarget && OtherComponent==ExpectedMesh);
    Row->SetNumberField(TEXT("frame_delta_seconds"),DT); Row->SetNumberField(TEXT("previous_frame"),double(PreviousFrame));
    Row->SetArrayField(TEXT("previous_frame_actual_position"),XYZ(PreviousPosition));
    Row->SetArrayField(TEXT("previous_frame_actual_velocity_cm_s"),XYZ(PreviousVelocity));
    Row->SetNumberField(TEXT("previous_frame_actual_speed_cm_s"),PreviousVelocity.Size());
    Row->SetArrayField(TEXT("trace_start"),XYZ(Result.TraceStart)); Row->SetArrayField(TEXT("trace_end"),XYZ(Result.TraceEnd));
    Row->SetArrayField(TEXT("sweep_velocity_from_trace_cm_s"),XYZ(SweepVelocity)); Row->SetNumberField(TEXT("sweep_speed_from_trace_cm_s"),SweepVelocity.Size());
    Row->SetNumberField(TEXT("hit_time_fraction"),Result.Time); Row->SetArrayField(TEXT("impact_point"),XYZ(Result.ImpactPoint));
    Row->SetArrayField(TEXT("impact_normal"),XYZ(Result.ImpactNormal)); Row->SetBoolField(TEXT("start_penetrating"),Result.bStartPenetrating);
    Row->SetNumberField(TEXT("penetration_depth_cm"),Result.PenetrationDepth); Row->SetBoolField(TEXT("primary_flight_sweep"),Primary);
    Hits.Add(Row);
    if (!FirstImpact && Other==ExpectedTarget && OtherComponent==ExpectedMesh)
    { FirstImpact=Row; ContactPoint=Result.ImpactPoint; ContactNormal=Result.ImpactNormal; }
}
void AHCM5VS2FlightCollisionR2Director::Input(const FInputKeyEventArgs& Event)
{
    if (!bActive || Event.Event!=IE_Pressed) return;
    if (Event.Key==EKeys::Escape) { bStopped=true; Finish(TEXT("STOPPED"),TEXT("Esc latched permanently: no more automatic control, quit or restart")); }
    else if (Event.Key==EKeys::P) ++UserPausePresses;
}
void AHCM5VS2FlightCollisionR2Director::Write(const FString& Status,const FString& Detail)
{
    auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.FlightCollisionR2.v1"));
    Row->SetStringField(TEXT("status"),Status); Row->SetStringField(TEXT("detail"),Detail);
    Row->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName()); Row->SetStringField(TEXT("directory"),Directory);
    Row->SetStringField(TEXT("input_scope"),TEXT("Production Enhanced Action entry points. Fixture teleports/initial control rotation are SETUP, not flight or OS mouse input."));
    Row->SetStringField(TEXT("os_input"),TEXT("NOT_RUN")); Row->SetStringField(TEXT("packaged_runtime"),TEXT("NOT_RUN"));
    Row->SetStringField(TEXT("visual_acceptance"),TEXT("NOT_RUN_COLLISION_DIAGNOSTIC")); Row->SetStringField(TEXT("video"),TEXT("NOT_RUN"));
    Row->SetBoolField(TEXT("stop_latched"),bStopped); Row->SetNumberField(TEXT("user_pause_presses"),UserPausePresses);
    Row->SetNumberField(TEXT("focus_pause_requests"),FocusPauseRequests); Row->SetNumberField(TEXT("active_seconds"),ActiveSeconds);
    Row->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedWall); Row->SetNumberField(TEXT("active_limit_seconds"),90);
    Row->SetNumberField(TEXT("measured_frame_count"),Frames.Num());
    if (Frames.Num())
    {
        Row->SetNumberField(TEXT("measured_min_dt_seconds"),MinFrameSeconds);
        Row->SetNumberField(TEXT("measured_max_dt_seconds"),MaxFrameSeconds);
        Row->SetNumberField(TEXT("measured_mean_fps"),MeasuredFrameSeconds>0?Frames.Num()/MeasuredFrameSeconds:0.);
    }
    Row->SetArrayField(TEXT("checks"),Objects(Checks)); Row->SetArrayField(TEXT("fixtures"),Objects(Fixtures));
    Row->SetArrayField(TEXT("measured_trajectory_frames"),Objects(Frames)); Row->SetArrayField(TEXT("actual_capsule_hits"),Objects(Hits));
    Row->SetStringField(TEXT("not_run"),TEXT("Real harbor map, NPC/dialogue/vehicle/story rules, independent save relaunch, OS input, packaged EXE, visual quality and clothing acceptance."));
    FString Text; FJsonSerializer::Serialize(Row,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("flight_collision_r2.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void AHCM5VS2FlightCollisionR2Director::Finish(const FString& Status,const FString& Detail)
{
    if (!bActive) return;
    bActive=false;
    if (PC && Flight) { Inject(); Flight->ClearFlightInput(); Character->GetCharacterMovement()->StopMovementImmediately(); }
    Write(Status,Detail); SetActorTickEnabled(false);
    if (!bStopped && bAutoQuit && PC) PC->ConsoleCommand(TEXT("quit"),true);
}
void AHCM5VS2FlightCollisionR2Director::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bActive) Finish(TEXT("STOPPED"),TEXT("World closed before bounded collision diagnostic completed"));
    if (Viewport.IsValid() && InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle);
    if (Character) Character->GetCapsuleComponent()->OnComponentHit.RemoveDynamic(this,&AHCM5VS2FlightCollisionR2Director::Hit);
    Super::EndPlay(Reason);
}
