#include "HCM5VS2FlightDemoDirector.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M3/HCM3Recording.h"
#include "M5VS2/HCM5VS2FlightComponent.h"
#include "M5VS2/HCM5VS2FlightVisualComponent.h"
#include "M5VS2/HCM5VS2HeroModestyComponent.h"
#include "M5VS2/HCM5VS2LookAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputKeyEventArgs.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/Application/SlateApplication.h"
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
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
namespace
{
enum EDemoStage { AwaitCapture,GroundHold,Takeoff,Ascend,Hover,FirstPerson,RestoreThird,OrbitAlign,Orbit,DiveAlign,Dive,PullUp,AirBrake,ReturnAlign,ReturnGroundXY,Land,GroundedHold };
const TCHAR* StageText(int32 Value)
{
    static const TCHAR* Names[]={TEXT("AwaitNativeFirstFrame"),TEXT("GroundHold"),TEXT("NormalFKeyTakeoff"),TEXT("SpaceAscend30m"),TEXT("Hover"),TEXT("VFirstPerson"),TEXT("VThirdPerson"),TEXT("MouseTangentAlign"),TEXT("WMouseWindmillOrbit"),TEXT("DiveAlign"),TEXT("ShiftDive"),TEXT("ShiftPullUp"),TEXT("ReleaseAirBrake"),TEXT("ReturnAlign"),TEXT("ReturnGroundXY"),TEXT("NormalFKeyLand"),TEXT("GroundedHold")};
    return Value>=0 && Value<UE_ARRAY_COUNT(Names)?Names[Value]:TEXT("Unknown");
}
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> XY(const FVector2D& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y)}; }
TArray<TSharedPtr<FJsonValue>> Rows(const TArray<TSharedPtr<FJsonObject>>& Objects)
{ TArray<TSharedPtr<FJsonValue>> Values;for(const auto& O:Objects)Values.Add(MakeShared<FJsonValueObject>(O));return Values; }
TSharedPtr<FJsonObject> ParseJSON(const FString& Text)
{ TSharedPtr<FJsonObject> Result;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result);if(!Result)Result=MakeShared<FJsonObject>();return Result; }
bool LoadJSON(const FString& File,TSharedPtr<FJsonObject>& Out)
{ FString Text;return FFileHelper::LoadFileToString(Text,*File)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out)&&Out.IsValid(); }
}
AHCM5VS2FlightDemoDirector::AHCM5VS2FlightDemoDirector()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}
void AHCM5VS2FlightDemoDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if(!FParse::Param(FCommandLine::Get(),TEXT("M5VS2FlightDemo")))return;
    const FString Map=GetWorld()->GetOutermost()->GetName();
    const bool R5=Map.StartsWith(TEXT("/Game/HarborCity/M5VS2/FlightDemoR5/Run_"));
    const FString Prefix=R5?TEXT("/Game/HarborCity/M5VS2/FlightDemoR5/Run_"):TEXT("/Game/HarborCity/M5VS2/FlightDemoR4/Run_");
    const FString Suffix=R5?TEXT("/L_FlightDemoR5"):TEXT("/L_FlightDemoR4");
    if(!Map.StartsWith(Prefix)||Map.Len()!=Prefix.Len()+12+Suffix.Len()||Map!=Prefix+Map.Mid(Prefix.Len(),12)+Suffix)return;
    for(TCHAR C:Map.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return;
    FString Root;if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root)||FPaths::IsRelative(Root))return;
    Root=FPaths::ConvertRelativePathToFull(Root);FPaths::NormalizeDirectoryName(Root);
    if(!FPaths::CollapseRelativeDirectories(Root)||!FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2")))return;
    Directory=Root/(TEXT("FlightDemo_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if(!IFileManager::Get().MakeDirectory(*Directory,true))return;
    StartedWall=StageWall=FPlatformTime::Seconds();bEnabled=true;bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));SetActorTickEnabled(true);BindStopObservers();Write();
#endif
}
void AHCM5VS2FlightDemoDirector::BindStopObservers()
{
    if(UGameViewportClient* V=GetWorld()->GetGameViewport())if(!InputHandle.IsValid())
    {Viewport=V;InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2FlightDemoDirector::ObserveInput);}
    if(FSlateApplication::IsInitialized()&&!ActivationHandle.IsValid())ActivationHandle=FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&AHCM5VS2FlightDemoDirector::ObserveActivation);
}
bool AHCM5VS2FlightDemoDirector::Check(bool Passed,const FString& Name,const FString& Detail)
{
    // Repeated invariants are still evaluated every tick. Keep one success row
    // per name/stage with a sample count; retain a failure immediately.
    if(Passed) for(const auto& Existing:Checks)
        if(Existing->GetStringField(TEXT("name"))==Name && Existing->GetStringField(TEXT("stage"))==StageText(Stage)
            && Existing->GetStringField(TEXT("status"))==TEXT("PASS"))
        {
            double Count=1;Existing->TryGetNumberField(TEXT("samples"),Count);
            Existing->SetNumberField(TEXT("samples"),Count+1);return true;
        }
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name);Row->SetStringField(TEXT("status"),Passed?TEXT("PASS"):TEXT("FAIL"));Row->SetStringField(TEXT("detail"),Detail);Row->SetStringField(TEXT("stage"),StageText(Stage));Checks.Add(Row);
    Row->SetNumberField(TEXT("samples"),1);
    if(!Passed)Stop(TEXT("FAIL"),Name+TEXT(": ")+Detail);return Passed;
}
bool AHCM5VS2FlightDemoDirector::ReadInputChain()
{
    auto* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);const UInputAction* Look=PC->GetM1Action(TEXT("Look"));
    if(!Input||!Look||Look->ValueType!=EInputActionValueType::Axis2D)return false;
    MappingScale=FVector2D(1.,1.);bool Valid=true;int32 Count=0;TArray<TSharedPtr<FJsonObject>> Modifiers;
    auto Accumulate=[&](const TArray<TObjectPtr<UInputModifier>>& Items,const TCHAR* Layer)
    {
        for(const UInputModifier* Modifier:Items)
        {
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("layer"),Layer);Row->SetStringField(TEXT("class"),GetPathNameSafe(Modifier));
            if(const UInputModifierScalar* Scalar=Cast<UInputModifierScalar>(Modifier))
            {MappingScale*=FVector2D(Scalar->Scalar.X,Scalar->Scalar.Y);Row->SetArrayField(TEXT("scalar"),XYZ(Scalar->Scalar));}
            else Valid=false;Modifiers.Add(Row);
        }
    };
    for(const FEnhancedActionKeyMapping& M:Input->GetEnhancedActionMappingsView())if(M.Action==Look)
    {++Count;Valid&=M.Key==EKeys::Mouse2D;Accumulate(M.Modifiers,TEXT("mapping"));}
    const FInputActionInstance* Instance=Input->FindActionInstanceData(Look);Accumulate(Instance?Instance->GetModifiers():Look->Modifiers,TEXT("action"));
    const FVector2D Gain=PC->GetOnFootLookDegreesPerActionUnit();DegreesPerRaw=MappingScale*Gain;
    InputChain=MakeShared<FJsonObject>();InputChain->SetArrayField(TEXT("modifiers"),Rows(Modifiers));InputChain->SetNumberField(TEXT("look_mapping_count"),Count);
    InputChain->SetArrayField(TEXT("raw_to_action"),XY(MappingScale));InputChain->SetArrayField(TEXT("action_to_degrees"),XY(Gain));InputChain->SetArrayField(TEXT("degrees_per_raw_key_unit"),XY(DegreesPerRaw));InputChain->SetObjectField(TEXT("controller_diagnostics"),ParseJSON(PC->GetLookInputDiagnostics()));
    return Valid&&Count==1&&Gain.Equals(FVector2D(1.5,1.5),.0001)&&!DegreesPerRaw.ContainsNaN()&&FMath::Abs(DegreesPerRaw.X)>.00001&&FMath::Abs(DegreesPerRaw.Y)>.00001;
}
bool AHCM5VS2FlightDemoDirector::GroundPreflight()
{
    auto* Move=Character->GetCharacterMovement();auto* Capsule=Character->GetCapsuleComponent();
    const FVector Position=Character->GetActorLocation();const float HH=Capsule->GetScaledCapsuleHalfHeight();const float Radius=Capsule->GetScaledCapsuleRadius();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightDemoStart),false,Character);FHitResult Floor,Roof;const FVector Feet=Position-FVector(0,0,HH);
    if(!Check(FVector::Dist2D(Feet,ExpectedStartFeet)<25&&Move->IsMovingOnGround(),TEXT("authored_start_without_teleport"),Feet.ToString()))return false;
    if(!Check(GetWorld()->LineTraceSingleByChannel(Floor,Position,Feet-FVector(0,0,100),ECC_Visibility,Params)&&Floor.bBlockingHit&&!Floor.bStartPenetrating&&Move->IsWalkable(Floor)&&Floor.ImpactPoint.Z>Flight->GetFlightBounds()->SeaLevelZ+1&&FMath::Abs(Floor.ImpactPoint.Z-ExpectedStartFeet.Z)<30,TEXT("actual_dry_start_floor"),GetPathNameSafe(Floor.GetActor())))return false;
    if(!Check(!GetWorld()->OverlapBlockingTestByChannel(Position+FVector(0,0,3),FQuat::Identity,Capsule->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Radius,HH),Params)
        &&!GetWorld()->LineTraceSingleByChannel(Roof,Position,Position+FVector(0,0,HH+500),ECC_Visibility,Params)
        &&!GetWorld()->SweepSingleByChannel(Roof,Position+FVector(0,0,3),Position+FVector(0,0,HeightAboveStart+200),FQuat::Identity,Capsule->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Radius,HH),Params),TEXT("actual_capsule_and_full_ascent_column_clear"),GetPathNameSafe(Roof.GetActor())))return false;
    StartFeet=Floor.ImpactPoint;for(const FBox& Box:Flight->GetFlightBounds()->NoTakeoffVolumes)if(!Check(!Box.IsInsideOrOn(Position),TEXT("outside_saved_indoor_no_takeoff"),Position.ToString()))return false;return true;
}
bool AHCM5VS2FlightDemoDirector::Initialize()
{
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));Character=PC?Cast<AHCM1Character>(PC->GetPawn()):nullptr;
    if(!PC||!Character||!PC->IsGameplayFocused()||PC->IsPauseMenuOpen()||!Viewport.IsValid()||!Viewport->Viewport)return false;
    bHadFocus=true;
    // Give the normally spawned capsule one bounded settling window; no pose write.
    if(!Character->GetCharacterMovement()->IsMovingOnGround() && FPlatformTime::Seconds()-StartedWall<2.0)return false;
    Flight=Character->GetFlightComponent();int32 RecordingCount=0;for(TActorIterator<AHCM3Recording> It(GetWorld());It;++It){Recorder=*It;++RecordingCount;}
    if(!Check(Flight&&Flight->IsFlightAvailable()&&RecordingCount==1&&AHCM3Recording::IsVS2GameplayCaptureRequested(),TEXT("real_flight_and_single_native_recorder"),TEXT("No flight, camera, recorder or pawn substitute")))return false;
    SaveSlot=PC->GetSaveSlotName();if(!Check(SaveSlot.StartsWith(TEXT("HarborCity_VS2_FlightDemo_"))&&!UGameplayStatics::DoesSaveGameExist(SaveSlot,0),TEXT("fresh_private_slot"),SaveSlot))return false;
    if(!Check(ReadInputChain(),TEXT("actual_mouse_chain_and_frozen_1_5_gain"),PC->GetLookInputDiagnostics()))return false;
    if(!Check(OrbitCenter.Equals(FVector(-1850,1550,0),.01)&&FMath::IsNearlyEqual(OrbitRadius,1200.f)&&FMath::IsNearlyEqual(HeightAboveStart,3000.f),TEXT("bounded_authored_demo_geometry"),OrbitCenter.ToString()))return false;
    if(!Check(FMath::IsNearlyEqual(Flight->CruiseSpeed,1000.f)&&FMath::IsNearlyEqual(Flight->BoostSpeed,2500.f)&&FMath::IsNearlyEqual(Flight->VerticalSpeed,600.f),TEXT("actual_unchanged_flight_units"),Flight->GetFlightDiagnostics()))return false;
    if(!GroundPreflight())return false;Character->GetCapsuleComponent()->OnComponentHit.AddDynamic(this,&AHCM5VS2FlightDemoDirector::CapsuleHit);
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());InputCallbacksBefore=PreviousInputCallbacks=PC->GetLookCallbackCount();
    PC->ShowStatusMessage(TEXT("ENGINE_INPUT_NOT_OS · 引擎输入飞行录像，非真实鼠标实测；Esc / P 停止"),60.f);return true;
}
void AHCM5VS2FlightDemoDirector::Key(const FKey& KeyName,bool Pressed)
{
    if(!PC||(Pressed&&(bStopLatched||bFinalizing)))return;if(HeldKeys.Contains(KeyName)==Pressed)return;
    const FInputDeviceId Device=IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();PC->InputKey(FInputKeyEventArgs(nullptr,Device,KeyName,Pressed?IE_Pressed:IE_Released,0u));
    if(Pressed)HeldKeys.Add(KeyName);else HeldKeys.Remove(KeyName);
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("key"),KeyName.ToString());Row->SetStringField(TEXT("event"),Pressed?TEXT("Pressed"):TEXT("Released"));Row->SetNumberField(TEXT("frame"),double(GFrameCounter));Row->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());Row->SetStringField(TEXT("stage"),StageText(Stage));KeyEvents.Add(Row);
}
void AHCM5VS2FlightDemoDirector::Pulse(const FKey& KeyName){Key(KeyName,true);PulsedKeys.AddUnique(KeyName);}
void AHCM5VS2FlightDemoDirector::ReleaseOwnedInput()
{
    const TArray<FKey> Owned=HeldKeys.Array();for(const FKey& K:Owned)Key(K,false);PulsedKeys.Reset();
    // Emergency input cleanup only; never a position/rotation/velocity write.
    if(Flight)Flight->ClearFlightInput();
}
void AHCM5VS2FlightDemoDirector::LookToward(float Yaw,float Pitch,float DeltaSeconds,float MaxYawRate)
{
    if(!PC||bStopLatched||bFinalizing)return;const FRotator Before=PC->GetControlRotation();
    const FVector2D Degrees(FMath::Clamp(float(FMath::FindDeltaAngleDegrees(Before.Yaw,double(Yaw)))*4.f,-MaxYawRate,MaxYawRate)*DeltaSeconds,FMath::Clamp(float(FMath::FindDeltaAngleDegrees(Before.Pitch,double(Pitch)))*5.f,-50.f,50.f)*DeltaSeconds);
    if(Degrees.IsNearlyZero(.00001))return;const FVector2D Raw(Degrees.X/DegreesPerRaw.X,Degrees.Y/DegreesPerRaw.Y);const FInputDeviceId Device=IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
    PC->InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseX,float(Raw.X),DeltaSeconds,1,0u));PC->InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseY,float(Raw.Y),DeltaSeconds,1,0u));QueuedRawSum+=Raw;bAwaitLookObservation=true;
    auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("frame"),double(GFrameCounter));Row->SetArrayField(TEXT("queued_raw_xy"),XY(Raw));Row->SetArrayField(TEXT("expected_degree_delta"),XY(Degrees));Row->SetStringField(TEXT("control_before"),Before.ToString());Row->SetStringField(TEXT("stage"),StageText(Stage));if(LookEvents.Num()<5000)LookEvents.Add(Row);
}
void AHCM5VS2FlightDemoDirector::SetStage(int32 Next)
{
    Stage=Next;StageWall=FPlatformTime::Seconds();Sample();Write();if(Stage==Orbit){const FVector Offset=Character->GetActorLocation()-OrbitCenter;PreviousBearing=FMath::RadiansToDegrees(FMath::Atan2(Offset.Y,Offset.X));}
}
bool AHCM5VS2FlightDemoDirector::CorridorClear()
{
    if(!Flight||!Flight->IsFlying()||Stage==Land||Stage==Takeoff||Stage==Ascend)return true;const FVector Velocity=Character->GetVelocity();if(Velocity.Size()<100)return true;
    const FVector Start=Character->GetActorLocation();const float Distance=FMath::Clamp(float(Velocity.SizeSquared())/(2.f*Flight->FlightBraking)+80.f,120.f,1300.f);auto* Cap=Character->GetCapsuleComponent();FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightDemoAhead),false,Character);
    const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Start,Start+Velocity.GetSafeNormal()*Distance,FQuat::Identity,Cap->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(),Cap->GetScaledCapsuleHalfHeight()),Params);
    return Check(!Blocked,TEXT("measured_velocity_stopping_corridor_clear"),GetPathNameSafe(Hit.GetActor()));
}
void AHCM5VS2FlightDemoDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!bEnabled||bEnded)return;BindStopObservers();const double Now=FPlatformTime::Seconds();
    if(bFinalizing){FinalizeTick();return;}if(bStopLatched)return;
    if(Now-StartedWall>100){Stop(TEXT("FAIL"),TEXT("Bounded total wall deadline"));return;}
    if(!bInitialized)
    {
        bInitialized=Initialize();if(bFinalizing)return;
        if(!bInitialized){if(Now-StartedWall>20)Stop(TEXT("NOT_RUN"),TEXT("No ready focused native pawn within startup bound"));return;}
    }
    if(!PC->IsGameplayFocused()||PC->IsPauseMenuOpen()||UGameplayStatics::IsGamePaused(this))
    {Stop(TEXT("USER_ABORTED"),TEXT("Gameplay pause/focus loss; never resumes"),true);return;}
    if(!Check(PC->GetPawn()==Character&&PC->GetOnFootLookDegreesPerActionUnit().Equals(FVector2D(1.5,1.5),.0001),TEXT("pawn_and_sensitivity_remain_unchanged"),TEXT("One captured player, original 1.5 action gain")))return;
    const TArray<FKey> Pulses=PulsedKeys;PulsedKeys.Reset();for(const FKey& K:Pulses)Key(K,false);
    if(bAwaitLookObservation)
    {
        const FVector Raw=PC->PlayerInput->GetRawVectorKeyValue(EKeys::Mouse2D);ObservedRawSum+=FVector2D(Raw.X,Raw.Y);
        const uint64 Count=PC->GetLookCallbackCount()-PreviousInputCallbacks;ObservedActionSum+=PC->GetLastLookAxis()*double(Count);
        if(!Check(Count<=1,TEXT("single_actual_look_callback_per_engine_frame"),FString::Printf(TEXT("callbacks %llu"),Count)))return;
        if(LookEvents.Num()>0){auto& Row=LookEvents.Last();Row->SetArrayField(TEXT("observed_raw_next_frame"),XY(FVector2D(Raw.X,Raw.Y)));Row->SetArrayField(TEXT("observed_action_next_frame"),XY(Count?PC->GetLastLookAxis():FVector2D::ZeroVector));Row->SetStringField(TEXT("control_after"),PC->GetControlRotation().ToString());}
        PreviousInputCallbacks=PC->GetLookCallbackCount();bAwaitLookObservation=false;
    }
    if(Stage==AwaitCapture)
    {
        if(!Recorder->HasCapturedFirstFrame()){if(Now-StartedWall>25)Stop(TEXT("NOT_RUN"),TEXT("Native recorder produced no first frame"));return;}
#if WITH_EDITOR
        if(!Check(!GShaderCompilingManager||!GShaderCompilingManager->IsCompiling(),TEXT("no_outstanding_shader_compile_when_input_begins"),TEXT("Global compiler queue only; not a material-fallback/art PASS")))return;
#endif
        FirstFrameWall=Now;SetStage(GroundHold);return;
    }
    const double Age=Now-StageWall;const double RunTime=Now-FirstFrameWall;
    if(RunTime>55){Stop(TEXT("FAIL"),TEXT("55 second real-time sequence deadline; no replay"));return;}
    ++Frames;SimSeconds+=DeltaSeconds;FrameDtSum+=DeltaSeconds;MaxFrameDt=FMath::Max(MaxFrameDt,double(DeltaSeconds));
    if(Now>=NextSampleWall){Sample();NextSampleWall=Now+.1;}
    if(DeltaSeconds<=0||DeltaSeconds>.25){Stop(TEXT("FAIL"),TEXT("Frame hitch >250ms; controls released rather than oversized synthetic mouse delta"));return;}
    const FVector Position=Character->GetActorLocation();const FVector Velocity=Character->GetVelocity();const float FeetZ=float(Position.Z)-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector2D Radial(Position.X-OrbitCenter.X,Position.Y-OrbitCenter.Y);const double Radius=Radial.Size();
    if(!Check(!Position.ContainsNaN()&&Radius<6500&&FeetZ<StartFeet.Z+5500,TEXT("finite_bounded_real_flight_envelope"),Position.ToString()))return;
    if(!CorridorClear())return;
    switch(Stage)
    {
    case GroundHold:
        if(PC->IsFirstPersonPerspective()&&Age<.5&&PulsedKeys.IsEmpty())Pulse(EKeys::V);
        if(Age<1.2)break;
        if(!Check(!PC->IsFirstPersonPerspective(),TEXT("normal_V_third_person_start"),TEXT("No perspective function shortcut")))return;
        Pulse(EKeys::F);SetStage(Takeoff);break;
    case Takeoff:
        if(Flight->IsFlying()){Key(EKeys::SpaceBar,true);SetStage(Ascend);}
        else if(Age>1)Stop(TEXT("FAIL"),TEXT("Normal F key did not enter flight"));break;
    case Ascend:
        if(FeetZ>=StartFeet.Z+HeightAboveStart-65){Key(EKeys::SpaceBar,false);SetStage(Hover);}
        else if(Age>8)Stop(TEXT("FAIL"),TEXT("Ascent did not reach 30m in bound"));break;
    case Hover:
        if(Age<2)break;
        if(!Check(Flight->IsFlying()&&Velocity.Size()<35&&FMath::Abs(FeetZ-StartFeet.Z-HeightAboveStart)<150,TEXT("real_hover_near_30m"),Position.ToString()))return;
        Pulse(EKeys::V);SetStage(FirstPerson);break;
    case FirstPerson:
        bFirstPersonObserved|=PC->IsFirstPersonPerspective();LookToward(float(PC->GetControlRotation().Yaw),-8.f,DeltaSeconds);
        if(Age<1.6)break;
        if(!Check(bFirstPersonObserved,TEXT("normal_V_first_person_observed"),TEXT("Actual PC perspective flag")))return;
        Pulse(EKeys::V);SetStage(RestoreThird);break;
    case RestoreThird:
        if(Age<.7)break;bThirdPersonRestored=!PC->IsFirstPersonPerspective();
        if(!Check(bThirdPersonRestored,TEXT("normal_V_third_person_restored"),TEXT("Actual PC perspective flag")))return;
        SetStage(OrbitAlign);break;
    case OrbitAlign:
    {
        const float Heading=float(FMath::RadiansToDegrees(FMath::Atan2(Radial.Y,Radial.X)))+90.f;LookToward(Heading,-5.f,DeltaSeconds);
        if(FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Yaw,double(Heading)))<5&&Age>.35){Key(EKeys::W,true);SetStage(Orbit);}
        else if(Age>4)Stop(TEXT("FAIL"),TEXT("Mouse tangent alignment timeout"));break;
    }
    case Orbit:
    {
        if(!Check(Radius>750&&Radius<1800&&FMath::Abs(FeetZ-StartFeet.Z-HeightAboveStart)<180,TEXT("measured_orbit_corridor"),FString::Printf(TEXT("radius %.3f"),Radius)))return;
        OrbitMinRadius=FMath::Min(OrbitMinRadius,Radius);OrbitMaxRadius=FMath::Max(OrbitMaxRadius,Radius);
        const double Bearing=FMath::RadiansToDegrees(FMath::Atan2(Radial.Y,Radial.X));ArcDegrees+=FMath::FindDeltaAngleDegrees(PreviousBearing,Bearing);PreviousBearing=Bearing;
        const FVector2D Unit=Radial.GetSafeNormal();const FVector2D Direction=FVector2D(-Unit.Y,Unit.X)-Unit*FMath::Clamp((Radius-OrbitRadius)/OrbitRadius*.9,-.4,.4);
        LookToward(float(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y,Direction.X))),-5.f,DeltaSeconds,100.f);
        if(ArcDegrees>=360){Key(EKeys::W,false);SetStage(DiveAlign);}
        else if(Age>12)Stop(TEXT("FAIL"),TEXT("No measured complete 360 degree orbit within bound"));break;
    }
    case DiveAlign:
    {
        const float Heading=float(FMath::RadiansToDegrees(FMath::Atan2(Radial.Y,Radial.X)));LookToward(Heading,-14.f,DeltaSeconds);
        if(Age>1&&Velocity.Size()<40&&FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Yaw,double(Heading)))<5&&FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Pitch,-14.0))<3)
        {Key(EKeys::W,true);Key(EKeys::LeftShift,true);SetStage(Dive);}
        else if(Age>4)Stop(TEXT("FAIL"),TEXT("Dive input alignment timeout"));break;
    }
    case Dive:
        BoostMaxSpeed=FMath::Max(BoostMaxSpeed,Velocity.Size());DiveMinVerticalSpeed=FMath::Min(DiveMinVerticalSpeed,double(Velocity.Z));bDiveObserved|=Velocity.Z<-150;
        LookToward(float(PC->GetControlRotation().Yaw),-14.f,DeltaSeconds);if(Age>=1.0)SetStage(PullUp);break;
    case PullUp:
        BoostMaxSpeed=FMath::Max(BoostMaxSpeed,Velocity.Size());ClimbMaxVerticalSpeed=FMath::Max(ClimbMaxVerticalSpeed,double(Velocity.Z));bClimbObserved|=Velocity.Z>150;
        LookToward(float(PC->GetControlRotation().Yaw),22.f,DeltaSeconds);
        if(Age>=1.25){Key(EKeys::LeftShift,false);Key(EKeys::W,false);SetStage(AirBrake);}break;
    case AirBrake:
        if(Age<1.2||Velocity.Size()>35)break;
        if(!Check(bDiveObserved&&bClimbObserved&&BoostMaxSpeed>1600,TEXT("actual_boost_dive_then_pullup"),FString::Printf(TEXT("max %.3f down %.3f up %.3f"),BoostMaxSpeed,DiveMinVerticalSpeed,ClimbMaxVerticalSpeed)))return;
        SetStage(ReturnAlign);break;
    case ReturnAlign:
    {
        const FVector ToStart=StartFeet-Position;const float Heading=float(ToStart.Rotation().Yaw);LookToward(Heading,-5.f,DeltaSeconds);
        if(FMath::Abs(FMath::FindDeltaAngleDegrees(PC->GetControlRotation().Yaw,double(Heading)))<5&&Age>.35){Key(EKeys::W,true);SetStage(ReturnGroundXY);}
        else if(Age>4)Stop(TEXT("FAIL"),TEXT("Return alignment timeout"));break;
    }
    case ReturnGroundXY:
    {
        const FVector ToStart=StartFeet-Position;const double Distance=ToStart.Size2D();LookToward(float(ToStart.Rotation().Yaw),-5.f,DeltaSeconds);Key(EKeys::W,Distance>180);
        if(Distance<=180&&Velocity.Size2D()<35)
        {
            FHitResult Floor;FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2FlightDemoLanding),false,Character);
            if(!Check(GetWorld()->LineTraceSingleByChannel(Floor,Position,FVector(Position.X,Position.Y,StartFeet.Z-100),ECC_Visibility,Params)&&Character->GetCharacterMovement()->IsWalkable(Floor)&&FMath::Abs(Floor.ImpactPoint.Z-StartFeet.Z)<30,TEXT("actual_return_dry_floor"),GetPathNameSafe(Floor.GetActor())))return;
            Key(EKeys::W,false);Pulse(EKeys::F);SetStage(Land);
        }
        else if(Age>9)Stop(TEXT("FAIL"),TEXT("Position feedback did not return to dry pad; no teleport or retry"));break;
    }
    case Land:
        if(!Flight->IsFlying()&&Character->GetCharacterMovement()->IsMovingOnGround()&&Velocity.Size()<35)SetStage(GroundedHold);
        else if(Age>9)Stop(TEXT("FAIL"),TEXT("Normal F landing did not complete"));break;
    case GroundedHold:
        if(Age>=1.2&&RunTime>=32)
        {
            if(!Check(UnexpectedHits==0&&ArcDegrees>=360&&bFirstPersonObserved&&bThirdPersonRestored&&PC->GetLookCallbackCount()>InputCallbacksBefore,TEXT("actual_bounded_demo_sequence_completed"),TEXT("Input/render observation only, no OS or art acceptance")))return;
            Stop(TEXT("PASS_ENGINE_INPUT_ONLY"),TEXT("Normal input completed one measured orbit, dive, pullup and dry landing"));
        }break;
    }
}
void AHCM5VS2FlightDemoDirector::CapsuleHit(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent* OtherComponent,FVector,const FHitResult& Result)
{
    if(!bEnabled||bFinalizing||!Character)return;const bool GroundContact=Result.ImpactNormal.Z>.7&&(Stage<=Ascend||Stage>=Land);
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("actor"),GetPathNameSafe(Other));Row->SetStringField(TEXT("component"),GetPathNameSafe(OtherComponent));Row->SetArrayField(TEXT("impact"),XYZ(Result.ImpactPoint));Row->SetArrayField(TEXT("normal"),XYZ(Result.ImpactNormal));Row->SetBoolField(TEXT("expected_ground_contact"),GroundContact);Row->SetStringField(TEXT("stage"),StageText(Stage));HitEvents.Add(Row);
    if(!GroundContact){++UnexpectedHits;Stop(TEXT("FAIL"),TEXT("Unexpected real capsule collision; sequence not retried"));}
}
void AHCM5VS2FlightDemoDirector::ObserveInput(const FInputKeyEventArgs& Event)
{
    if(!bEnabled||Event.Event!=IE_Pressed)return;
    if(Event.Key==EKeys::Escape)Stop(TEXT("USER_ABORTED"),TEXT("Esc permanently cancels input, recording and autoquit"),true);
    else if(Event.Key==EKeys::P)Stop(TEXT("USER_ABORTED"),TEXT("P permanently ends this input/recording sequence; no resume"),true);
}
void AHCM5VS2FlightDemoDirector::ObserveActivation(bool Active)
{if(bEnabled&&bHadFocus&&!Active)Stop(TEXT("USER_ABORTED"),TEXT("Application lost focus; no reacquire or resume"),true);}
void AHCM5VS2FlightDemoDirector::Stop(const FString& Status,const FString& Reason,bool UserStop)
{
    if(UserStop)UE_LOG(LogTemp,Warning,TEXT("M5VS2_FLIGHT_DEMO_USER_STOP reason=%s evidence_failed=%d; inputs released; no autoquit or resume"),*Reason,bEvidenceWriteFailed);
    if(!bEnabled||bEnded)return;if(UserStop){bStopLatched=true;bAutoQuit=false;}if(bFinalizing&&!UserStop)return;
    FinalStatus=Status;FinalReason=Reason;bFinalizing=true;FinalizeWall=FPlatformTime::Seconds();ReleaseOwnedInput();Sample();Write();
    // Next tick finalizes after recorder Esc/P/activation callbacks retain their
    // true stop reason; never steals focus or unpauses the game.
}
void AHCM5VS2FlightDemoDirector::FinalizeTick()
{
    if(Recorder)Recorder->FinishVS2GameplayCapture();const bool Pending=Recorder&&Recorder->IsVS2AudioExportPending();
    if(Pending&&FPlatformTime::Seconds()-FinalizeWall<20)return;
    if(Pending){FinalStatus=TEXT("FAIL");FinalReason+=TEXT("; native audio export not complete in bound");}
    if(FinalStatus==TEXT("PASS_ENGINE_INPUT_ONLY"))
    {
        TSharedPtr<FJsonObject> Capture;const bool Loaded=Recorder&&LoadJSON(Recorder->GetCaptureDirectory()/TEXT("capture.json"),Capture);
        const bool Complete=Loaded&&Capture->GetStringField(TEXT("status"))==TEXT("CAPTURED_PENDING_REVIEW")
            &&Capture->GetStringField(TEXT("stop_reason"))==TEXT("vs2_gameplay_sequence_finished")
            &&Capture->GetNumberField(TEXT("wall_seconds"))>=30&&Capture->GetNumberField(TEXT("wall_seconds"))<=60
            &&Capture->GetNumberField(TEXT("write_failures"))==0&&Capture->GetBoolField(TEXT("capture_geometry_valid"))
            &&Capture->GetBoolField(TEXT("audio_export_requested"))&&Capture->GetArrayField(TEXT("frames")).Num()>1&&!Pending;
        if(!Complete){FinalStatus=TEXT("FAIL");FinalReason+=TEXT("; actual native capture/audio/duration did not satisfy delivery guard");}
    }
    const bool Published=Write();bEnded=true;SetActorTickEnabled(false);
    if(!Published)UE_LOG(LogTemp,Error,TEXT("M5VS2_FLIGHT_DEMO_FINAL_EVIDENCE_FAILED; process result must not be PASS"));
    if(!bStopLatched&&bAutoQuit)FPlatformMisc::RequestExitWithStatus(false,Published&&!bEvidenceWriteFailed&&FinalStatus==TEXT("PASS_ENGINE_INPUT_ONLY")?0:1,TEXT("M5VS2 bounded engine-input flight demo finished"));
}
void AHCM5VS2FlightDemoDirector::Sample()
{
    if(!Character||!Flight||!PC||Samples.Num()>=1000)return;auto Row=MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("wall_since_first_frame"),FirstFrameWall>0?FPlatformTime::Seconds()-FirstFrameWall:0);Row->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());Row->SetStringField(TEXT("stage"),StageText(Stage));
    Row->SetArrayField(TEXT("position_cm"),XYZ(Character->GetActorLocation()));Row->SetArrayField(TEXT("velocity_cm_s"),XYZ(Character->GetVelocity()));Row->SetStringField(TEXT("control_rotation"),PC->GetControlRotation().ToString());
    FVector View;FRotator Rotation;PC->GetPlayerViewPoint(View,Rotation);Row->SetArrayField(TEXT("final_view_position_cm"),XYZ(View));Row->SetStringField(TEXT("final_view_rotation"),Rotation.ToString());Row->SetStringField(TEXT("view_target"),GetPathNameSafe(PC->GetViewTarget()));
    Row->SetBoolField(TEXT("first_person"),PC->IsFirstPersonPerspective());Row->SetObjectField(TEXT("flight"),ParseJSON(Flight->GetFlightDiagnostics()));
    if(const auto* Visual=Character->FindComponentByClass<UHCM5VS2FlightVisualComponent>())Row->SetObjectField(TEXT("flight_visual"),ParseJSON(Visual->GetFlightVisualDiagnostics()));
    if(const auto* Modesty=Character->FindComponentByClass<UHCM5VS2HeroModestyComponent>())Row->SetObjectField(TEXT("modesty"),ParseJSON(Modesty->GetModestyDiagnostics()));
    if(const auto* Body=Character->GetMesh())
    {
        auto Pose=MakeShared<FJsonObject>();Pose->SetStringField(TEXT("actual_instance"),GetPathNameSafe(Body->GetAnimInstance()));
        if(const auto* Anim=Cast<UHCM5VS2LookAnimInstance>(Body->GetAnimInstance()))
        {Pose->SetBoolField(TEXT("flight_poses_enabled"),Anim->bVS2FlightPosesEnabled);Pose->SetBoolField(TEXT("flight_pose_eligible"),Anim->bVS2FlightPoseEligible);Pose->SetNumberField(TEXT("pose_index"),Anim->VS2FlightPoseIndex);}
        for(const FString Side:{FString(TEXT("L")),FString(TEXT("R"))})
        {
            auto Bend=[&](const TCHAR* A,const TCHAR* B,const TCHAR* C)
            {
                const FName N0(*(FString(A)+Side)),N1(*(FString(B)+Side)),N2(*(FString(C)+Side));
                if(Body->GetBoneIndex(N0)==INDEX_NONE||Body->GetBoneIndex(N1)==INDEX_NONE||Body->GetBoneIndex(N2)==INDEX_NONE)return -1.0;
                const FVector U=(Body->GetSocketLocation(N1)-Body->GetSocketLocation(N0)).GetSafeNormal();
                const FVector V=(Body->GetSocketLocation(N2)-Body->GetSocketLocation(N1)).GetSafeNormal();
                return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(U,V),-1.0,1.0)));
            };
            Pose->SetNumberField(TEXT("knee_bend_deg_")+Side,Bend(TEXT("UpperLeg_"),TEXT("LowerLeg_"),TEXT("Foot_")));
            Pose->SetNumberField(TEXT("elbow_bend_deg_")+Side,Bend(TEXT("UpperArm_"),TEXT("LowerArm_"),TEXT("Hand_")));
        }
        Pose->SetStringField(TEXT("scope"),TEXT("Actual evaluated skeletal socket positions at existing bounded samples; 0 degrees means a straight segment pair. Not clothing contact acceptance."));Row->SetObjectField(TEXT("evaluated_flight_pose"),Pose);
    }
    Samples.Add(Row);
}
bool AHCM5VS2FlightDemoDirector::Write()
{
    if(bEvidenceWriteFailed)return false;
    auto Failed=[&](const TCHAR* Step,uint32 Error,const FString& File)
    {
        UE_LOG(LogTemp,Error,TEXT("M5VS2_FLIGHT_DEMO_EVIDENCE_WRITE_FAILED stage=%s error=%u file=%s; no retry"),Step,Error,*File);
        bEvidenceWriteFailed=true;FinalStatus=TEXT("FAIL");FinalReason=TEXT("Evidence publication failed: ")+FString(Step);
        if(!bFinalizing){bFinalizing=true;FinalizeWall=FPlatformTime::Seconds();}
        ReleaseOwnedInput();return false;
    };
    if(Directory.IsEmpty())return Failed(TEXT("empty_directory"),0,Directory);auto Root=MakeShared<FJsonObject>();
    const bool R5=GetWorld()->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/FlightDemoR5/Run_"));
    Root->SetStringField(TEXT("schema"),R5?TEXT("HarborCity.M5VS2.FlightDemoR5.v1"):TEXT("HarborCity.M5VS2.FlightDemoR4.v1"));Root->SetNumberField(TEXT("art_revision"),R5?5:4);Root->SetStringField(TEXT("input_scope"),TEXT("ENGINE_INPUT_NOT_OS"));
    Root->SetStringField(TEXT("input_detail"),TEXT("PC::InputKey W/Space/F/V/Shift and paired MouseX/Y through real Mouse2D Action modifiers. Position feedback computes input only. No transform/camera/controller rotation setters."));
    Root->SetStringField(TEXT("status"),FinalStatus);Root->SetStringField(TEXT("reason"),FinalReason);Root->SetStringField(TEXT("stage"),StageText(Stage));Root->SetStringField(TEXT("os_input"),TEXT("NOT_RUN"));Root->SetStringField(TEXT("packaged_runtime"),TEXT("NOT_RUN_EDITOR_GAME"));Root->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));Root->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName());Root->SetStringField(TEXT("private_save_slot"),SaveSlot);Root->SetBoolField(TEXT("stop_latched"),bStopLatched);
    Root->SetNumberField(TEXT("scripted_teleports"),0);Root->SetNumberField(TEXT("camera_pose_setters"),0);Root->SetNumberField(TEXT("retries"),0);Root->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedWall);Root->SetNumberField(TEXT("sequence_wall_seconds"),FirstFrameWall>0?FPlatformTime::Seconds()-FirstFrameWall:0);
    Root->SetNumberField(TEXT("actual_orbit_angle_degrees"),ArcDegrees);Root->SetNumberField(TEXT("actual_orbit_min_radius_cm"),OrbitMinRadius<1.e8?OrbitMinRadius:0);Root->SetNumberField(TEXT("actual_orbit_max_radius_cm"),OrbitMaxRadius);Root->SetNumberField(TEXT("boost_max_speed_cm_s"),BoostMaxSpeed);Root->SetNumberField(TEXT("dive_min_z_speed_cm_s"),DiveMinVerticalSpeed);Root->SetNumberField(TEXT("pullup_max_z_speed_cm_s"),ClimbMaxVerticalSpeed);
    Root->SetNumberField(TEXT("sampled_engine_frames"),Frames);Root->SetNumberField(TEXT("sampled_world_seconds"),SimSeconds);Root->SetNumberField(TEXT("world_frame_rate_mean"),FrameDtSum>0?Frames/FrameDtSum:0);Root->SetNumberField(TEXT("max_frame_dt_seconds"),MaxFrameDt);
    Root->SetArrayField(TEXT("queued_mouse_raw_sum"),XY(QueuedRawSum));Root->SetArrayField(TEXT("observed_mouse_raw_sum"),XY(ObservedRawSum));Root->SetArrayField(TEXT("observed_look_action_sum"),XY(ObservedActionSum));Root->SetNumberField(TEXT("actual_look_callbacks"),PC?double(PC->GetLookCallbackCount()-InputCallbacksBefore):0);Root->SetBoolField(TEXT("last_input_unobserved_on_stop"),bAwaitLookObservation);
    if(InputChain)Root->SetObjectField(TEXT("actual_input_chain"),InputChain);Root->SetArrayField(TEXT("checks"),Rows(Checks));Root->SetArrayField(TEXT("samples"),Rows(Samples));Root->SetArrayField(TEXT("engine_key_events"),Rows(KeyEvents));Root->SetArrayField(TEXT("engine_mouse_events"),Rows(LookEvents));Root->SetArrayField(TEXT("capsule_hits"),Rows(HitEvents));
    if(Recorder)
    {
        const FString Capture=Recorder->GetCaptureDirectory();Root->SetStringField(TEXT("native_capture_directory"),Capture);Root->SetBoolField(TEXT("audio_export_pending"),Recorder->IsVS2AudioExportPending());TSharedPtr<FJsonObject> CaptureJSON;if(LoadJSON(Capture/TEXT("capture.json"),CaptureJSON))Root->SetObjectField(TEXT("actual_native_capture"),CaptureJSON);
    }
    FString Text;
    if(!FJsonSerializer::Serialize(Root,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text)))return Failed(TEXT("serialize"),0,Directory);
    const FString Destination=Directory/TEXT("flight_demo.json");
    const FString Temporary=Directory/(TEXT("flight_demo_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp"));
    if(!FFileHelper::SaveStringToFile(Text,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return Failed(TEXT("temp_write"),FPlatformMisc::GetLastError(),Temporary);
#if PLATFORM_WINDOWS
    const FString From=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Temporary).Replace(TEXT("/"),TEXT("\\"));
    const FString To=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Destination).Replace(TEXT("/"),TEXT("\\"));
    if(!::MoveFileExW(*From,*To,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return Failed(TEXT("atomic_replace"),FPlatformMisc::GetLastError(),Temporary);
    return true;
#else
    return Failed(TEXT("unsupported_platform"),0,Temporary);
#endif
}
void AHCM5VS2FlightDemoDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bEnabled&&!bEnded){bAutoQuit=false;Stop(TEXT("USER_ABORTED"),TEXT("World ended before normal demo finalization"),true);if(Recorder)Recorder->FinishVS2GameplayCapture();Write();}
    if(Viewport.IsValid()&&InputHandle.IsValid())Viewport->OnInputKey().Remove(InputHandle);if(FSlateApplication::IsInitialized()&&ActivationHandle.IsValid())FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
    if(Character)Character->GetCapsuleComponent()->OnComponentHit.RemoveDynamic(this,&AHCM5VS2FlightDemoDirector::CapsuleHit);Super::EndPlay(Reason);
}
