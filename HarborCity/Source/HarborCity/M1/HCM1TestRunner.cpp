#include "HCM1TestRunner.h"
#include "HCM1PlayerController.h"
#include "HCM1Character.h"
#include "HCM1Vehicle.h"
#include "HCM1LightSwitch.h"
#include "HCM1SaveGame.h"
#include "M3/HCM3Recording.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "EnhancedPlayerInput.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputKeyEventArgs.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

AHCM1TestRunner::AHCM1TestRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void AHCM1TestRunner::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    bM5Run = FParse::Value(FCommandLine::Get(), TEXT("M5Test="), Mode);
    bM4Run = bM5Run || FParse::Value(FCommandLine::Get(), TEXT("M4Test="), Mode);
    bM3Run = !bM4Run && FParse::Value(FCommandLine::Get(), TEXT("M3Test="), Mode);
    bM2Run = !bM4Run && !bM3Run && FParse::Value(FCommandLine::Get(), TEXT("M2Test="), Mode);
    bRunning = bM4Run || bM3Run || bM2Run || FParse::Value(FCommandLine::Get(), TEXT("M1Test="), Mode);
#endif
    SetActorTickEnabled(bRunning);
    if (!bRunning) return;
    StartedAt = LastFrameAt = FPlatformTime::Seconds();
    RunDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / (bM5Run ? TEXT("M5Tests") : bM4Run ? TEXT("M4Tests") : bM3Run ? TEXT("M3Tests") : bM2Run ? TEXT("M2Tests") : TEXT("M1Tests")) /
        (FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_") + Mode));
    IFileManager::Get().MakeDirectory(*RunDirectory, true);
    // Only this revision's explicitly isolated test launches, across M1/M2/M3
    // namespaces. Plain gameplay never subscribes or changes its Esc/P bindings.
    FString StopSlot;
    if (FParse::Value(FCommandLine::Get(),TEXT("HCM1SaveSlot="),StopSlot))
    {
        bM3EscapeScope=(StopSlot.StartsWith(TEXT("HarborCity_M2_V1_Test_M3_"))
            || StopSlot.StartsWith(TEXT("HarborCity_M1_R2_Test_M3_"))
            || StopSlot.StartsWith(TEXT("HarborCity_M2_V1_Test_M4_"))
            || StopSlot.StartsWith(TEXT("HarborCity_M1_R2_Test_M4_"))
            || StopSlot.StartsWith(TEXT("HarborCity_M5_VS1_Test_"))) && StopSlot.Len()<=100;
        for (TCHAR C : StopSlot) bM3EscapeScope &= FChar::IsAlnum(C) || C==TEXT('_');
    }
    if (bM3EscapeScope)
    {
        BindM3EscapeStop();
        // Opt-in B-Key developer reproduction, never a desktop key press.
        FParse::Value(FCommandLine::Get(),TEXT("M3TestEscapeStopAfter="),M3EscapeExerciseAfter);
        if (!FMath::IsFinite(M3EscapeExerciseAfter) || M3EscapeExerciseAfter<0 || M3EscapeExerciseAfter>360) M3EscapeExerciseAfter=0;
    }
    AddStep(TEXT("world_settle"), 3.0, [] {}, [this]
    {
        PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
        Character = PC ? PC->GetControlledCharacter() : nullptr;
        for (TActorIterator<AHCM1Vehicle> It(GetWorld()); It; ++It) { Car = *It; break; }
        for (TActorIterator<AHCM1LightSwitch> It(GetWorld()); It; ++It) { Lamp = *It; break; }
        Check(TEXT("Runtime actors"), PC && Character && Car && Lamp, TEXT("M1 controller, character, car, switch"),
            FString::Printf(TEXT("PC=%d character=%d car=%d lamp=%d"), !!PC, !!Character, !!Car, !!Lamp), TEXT("A"));
        if (!PC || !Character || !Car || !Lamp) { Finish(); return; }
        const bool bIsolatedSlot = bM5Run ? PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_M5_VS1_Test_")) : (bM2Run || bM3Run || bM4Run) ? PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_M2_V1_Test_")) :
            PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_M1_V1_Test_")) ||
            PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_M1_R1_Test_")) ||
            PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_M1_R2_Test_"));
        Check(TEXT("Isolated save slot"), bIsolatedSlot,
            (bM2Run || bM3Run || bM4Run) ? TEXT("HarborCity_M2_V1_Test_*") : TEXT("HarborCity_M1_V1_Test_* or HarborCity_M1_R1_Test_* or HarborCity_M1_R2_Test_*"), PC->GetSaveSlotName(), TEXT("A"));
        if (!bIsolatedSlot) { Finish(); return; }
        CarHome = Car->GetActorTransform(); CharacterHome = Character->GetActorTransform();
        bInitialLight = Lamp->IsLightEnabled();
        if (bM5Run || PC->GetSaveSlotName().StartsWith(TEXT("HarborCity_M2_V1_Test_M4_R2_")))
            AddStep(TEXT("M4 R2 await initial game focus"),0,[]{},[this]
            {
                Check(TEXT("R2 initial real application focus"),PC->IsGameplayFocused(),
                    TEXT("actual application activation before any test input; no focus override"),
                    FString::Printf(TEXT("focused=%d"),PC->IsGameplayFocused()),TEXT("A-runtime prerequisite"));
            });
        if(bM5Run) AddM5Tests(); else BuildSequence();
    });
}

void AHCM1TestRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bM3EscapeScope && !M3EscapeHandle.IsValid()) BindM3EscapeStop();
    if (bM3EscapeScope && !bM3UserAbort && !bFinished && M3EscapeExerciseAfter>0
        && FPlatformTime::Seconds()-StartedAt>=M3EscapeExerciseAfter)
    {
        M3EscapeExerciseAfter=0;
        ExerciseM3EscapeStopThroughViewport();
    }
    // This is before all clocks, phase callbacks and injection/update paths.
    if (bM3UserAbort)
    {
        // Let the unconsumed key finish its normal game input routing first.
        if (GFrameCounter>M3AbortFrame+1)
        {
            if (bM3EscapeExerciseReleasePending)
            {
                if (UGameViewportClient* Viewport=M3EscapeViewport.Get())
                    if (PC)
                    {
                        const FInputDeviceId Device=IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
                        Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport,Device,EKeys::Escape,IE_Released,0u));
                    }
                bM3EscapeExerciseReleasePending=false;
            }
            CompleteM3UserAbort();
        }
        return;
    }
    if (bM3FinishPending)
    {
        const bool bPending = FScreenshotRequest::IsScreenshotRequested();
        const bool bPastCaptureFrame = M3LastCaptureFrame == MAX_uint64 || GFrameCounter > M3LastCaptureFrame + 1;
        bool bAudioPending = false;
        if (bM5Run || Mode.StartsWith(TEXT("r2_"))) for (TActorIterator<AHCM3Recording> It(GetWorld()); It; ++It) bAudioPending |= It->IsR2AudioExportPending();
        if (!bPending && bPastCaptureFrame && !bAudioPending) CompleteFinish();
        else if (FPlatformTime::Seconds() - M3FinishWaitStarted > 8.0)
        {
            Check(TEXT("Final screenshot/audio drain timeout"), false, TEXT("screenshot and any R2 audio export processed before normal exit within 8 wall seconds"),
                FString::Printf(TEXT("pending=%d frame=%llu capture_frame=%llu; auto-quit withheld for normal external close"),
                    bPending, GFrameCounter, M3LastCaptureFrame), TEXT("A"));
            bM3SuppressAutoQuit = true;
            CompleteFinish();
        }
        return;
    }
    if (!bRunning || bFinished) return;
    // Loading may briefly recreate/deactivate the game window. Wait only before
    // starting this revision's isolated sequence; never force application focus.
    if (PC && Steps.IsValidIndex(StepIndex) && Steps[StepIndex].Name==TEXT("M4 R2 await initial game focus") && !PC->IsGameplayFocused())
    {
        LastFrameAt=FPlatformTime::Seconds();
        if (LastFrameAt-StepStartedAt>30.0)
        {
            Check(TEXT("R2 initial focus timeout"),false,TEXT("actual focused window within30s"),TEXT("no test input sent"),TEXT("A-runtime prerequisite"));
            Finish();
        }
        return;
    }
    if (Mode==TEXT("r2_playthrough") && PC &&
        (!PC->IsGameplayFocused() || PC->IsPauseMenuOpen() || UGameplayStatics::IsGamePaused(this)))
    {
        Held.Empty();
        Check(TEXT("R2 natural recording focus stop"),false,
            TEXT("continuous focused gameplay; no auto-resume"),
            TEXT("focus or pause changed; all injected actions released"),TEXT("B-Action"));
        Finish(); return;
    }
    const double Now = FPlatformTime::Seconds();
    const double RealDelta = LastFrameAt > 0 ? Now - LastFrameAt : 0;
    if (LastFrameAt > 0) FrameTimes.Add(RealDelta * 1000.0);
    LastFrameAt = Now;
    if (bM4Run && PC) UpdateM4R1Profile(RealDelta);
    SimulationElapsed += DeltaSeconds;
    // Physics and controller timers use world time. Background throttling and
    // synchronous editor tools must not consume an input phase without simulating it.
    StepElapsed += UGameplayStatics::IsGamePaused(this) ? RealDelta : DeltaSeconds;
    if (!bM3Run && !bM4Run && Mode == TEXT("playthrough")) UpdatePlaythroughInput();
    if (M4Test.IsValid()) UpdateM4Test(DeltaSeconds);
    if (M4R2Combat.IsValid()) UpdateM4R2Combat(DeltaSeconds);
    if (M4R1Steering.IsValid()) UpdateM4R1Steering(DeltaSeconds);
    if (M4R1Reaction.IsValid()) UpdateM4R1Reaction(DeltaSeconds);
    if (M4R1Impact.IsValid()) UpdateM4R1Impact(DeltaSeconds);
    if (M4R1Joint.IsValid()) UpdateM4R1Joint(DeltaSeconds);
    if (M4R1Quests.IsValid()) UpdateM4R1Quests(DeltaSeconds);
    if (M3Test.IsValid()) UpdateM3Test(DeltaSeconds);
    if (M3Safety.IsValid()) UpdateM3Safety(DeltaSeconds);
    if (M3PlaythroughState.IsValid()) UpdateM3Playthrough(DeltaSeconds);
    if (CameraTest.IsValid()) UpdateCameraTest(DeltaSeconds);
    if (LookCalibration.IsValid()) UpdateLookCalibration(DeltaSeconds);
    if (M2Route.IsValid()) UpdateM2Route(DeltaSeconds, RealDelta);
    if (M2Camera.IsValid()) UpdateM2Camera(DeltaSeconds);
    if (bFinished) return;
    if (PC)
    {
        if (UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
            for (const auto& Pair : Held)
                if (const UInputAction* Action = PC->GetM1Action(Pair.Key))
                { Input->InjectInputForAction(Action, Pair.Value); ++M3InjectedActions; }
    }
    if (Car) PeakSpeed = FMath::Max(PeakSpeed, Car->GetSpeedKmh());
    if (Car && bTrackBraking)
    {
        const float V = Car->GetSignedSpeed();
        bObservedNearStop |= FMath::Abs(V) < 60;
        bUnsafeReverse |= V > 50 && Car->GetChaosMovement()->GetTargetGear() < 0 && Car->GetChaosMovement()->GetThrottleInput() > 0.05f;
        ReverseConfirmedSeconds = V < -40 && Car->GetChaosMovement()->GetCurrentGear() < 0
            ? ReverseConfirmedSeconds + DeltaSeconds : 0.f;
        // Stop after sustained actual reverse motion. The old fixed 3-second hold
        // drove backwards into the playground perimeter, then sampled the rebound.
        // Retain that exact old scenario behind a diagnostic mode for collision evidence.
        if (Mode != TEXT("reverse_diagnostic_legacy") && ReverseConfirmedSeconds >= .25f && Steps.IsValidIndex(StepIndex)
            && Steps[StepIndex].Name == TEXT("reverse hold")) StepElapsed = Steps[StepIndex].Duration;
    }
    if (Character) PeakHeight = FMath::Max(PeakHeight, float(Character->GetActorLocation().Z));
    if (StepIndex < 0)
    {
        StepIndex = 0; StepStartedAt = Now; StepElapsed = 0;
        Steps[0].Begin(); return;
    }
    if (StepElapsed < Steps[StepIndex].Duration) return;
    // Move callbacks out before they append steps and potentially reallocate the array.
    TFunction<void()> End = MoveTemp(Steps[StepIndex].End);
    if (End) End();
    if (bFinished) return;
    ++StepIndex;
    if (!Steps.IsValidIndex(StepIndex)) { Finish(); return; }
    StepStartedAt = Now; StepElapsed = 0;
    UE_LOG(LogTemp, Display, TEXT("M1_TEST_STEP %d %s"), StepIndex, *Steps[StepIndex].Name);
    if (Steps[StepIndex].Begin) Steps[StepIndex].Begin();
}

void AHCM1TestRunner::AddStep(const FString& Name, double Duration, TFunction<void()> Begin, TFunction<void()> End)
{ Steps.Add({Name, Duration, MoveTemp(Begin), MoveTemp(End)}); }
void AHCM1TestRunner::Hold(FName Action, const FInputActionValue& Value) { if (!bM3UserAbort) Held.Add(Action, Value); }
void AHCM1TestRunner::Release(FName Action) { Held.Remove(Action); }
void AHCM1TestRunner::Tap(FName Action, double Wait)
{
    AddStep(Action.ToString() + TEXT(" press"), 0.10, [this, Action] { Hold(Action, FInputActionValue(true)); });
    AddStep(Action.ToString() + TEXT(" release"), Wait, [this, Action] { Release(Action); });
}

void AHCM1TestRunner::BindM3EscapeStop()
{
    if (!bM3EscapeScope || M3EscapeHandle.IsValid() || !GetWorld()) return;
    if (UGameViewportClient* Viewport=GetWorld()->GetGameViewport())
    {
        M3EscapeViewport=Viewport;
        M3EscapeHandle=Viewport->OnInputKey().AddUObject(this,&AHCM1TestRunner::OnM3ViewportInputKey);
    }
}

bool AHCM1TestRunner::ExerciseM3EscapeStopThroughViewport()
{
#if !UE_BUILD_SHIPPING
    if (!bM3EscapeScope || !bRunning || bFinished || bM3UserAbort) return false;
    BindM3EscapeStop();
    UGameViewportClient* Viewport=M3EscapeViewport.Get();
    AHCM1PlayerController* Controller=PC?PC.Get():Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!Viewport || !Viewport->Viewport || !Controller || !M3EscapeHandle.IsValid()) return false;
    const FInputDeviceId Device=IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(Controller->GetPlatformUserId());
    bM3EscapeExercise=true;
    Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport,Device,EKeys::Escape,IE_Pressed,0u));
    bM3EscapeExercise=false;
    bM3EscapeExerciseReleasePending=bM3UserAbort;
    return bM3UserAbort;
#else
    return false;
#endif
}

void AHCM1TestRunner::OnM3ViewportInputKey(const FInputKeyEventArgs& Event)
{
    if (!bM3EscapeScope || !bRunning || bM3UserAbort || (bFinished && !bM3FinishPending)
        || Event.Event!=IE_Pressed || Event.Key!=EKeys::Escape) return;
    bM3UserAbort=true; bM3SuppressAutoQuit=true; bM3FinishPending=false;
    bM3AbortWasBKey=bM3EscapeExercise; M3AbortFrame=GFrameCounter; M3AbortWall=FPlatformTime::Seconds();
    M3HeldActionsAtAbort=Held.Num(); M3InjectedActionsAtAbort=M3InjectedActions;
    Held.Empty();
    if (!PC) PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (LookCalibration.IsValid()) StopLookCalibrationInput();
    if (CameraTest.IsValid()) StopCameraLook();
    if (M2Route.IsValid()) StopM2RouteInput();
    if (M2Camera.IsValid()) StopM2CameraInput();
    if (M4Test.IsValid()) StopM4TestInput();
    if (M3Safety.IsValid()) StopM3SafetyInput();
    if (M3PlaythroughState.IsValid()) StopM3PlaythroughForUser();
    if (PC) PC->FlushPressedKeys();
    if (Car) Car->ClearDriveInput(true);
    // Multicast observer: do not consume/replay Esc, change pause or input maps,
    // finish a phase, write a save, invoke normal Finish, or close the game.
    UE_LOG(LogTemp,Display,TEXT("M3_TEST_USER_STOP_LATCHED source=%s frame=%llu held=%d"),
        bM3AbortWasBKey?TEXT("B-Key-viewport-exercise"):TEXT("viewport-Escape"),M3AbortFrame,M3HeldActionsAtAbort);
}

void AHCM1TestRunner::CompleteM3UserAbort()
{
    if (!bM3UserAbort || bM3UserAbortReported) return;
    if (M5ReviewCameraCleanup)
    {
        TFunction<void()> Cleanup = MoveTemp(M5ReviewCameraCleanup);
        M5ReviewCameraCleanup = nullptr;
        Cleanup();
    }
    bM3UserAbortReported=true; bFinished=true; bM3FinishPending=false;
    const auto StopRow=MakeShared<FJsonObject>();
    StopRow->SetStringField(TEXT("name"),TEXT("Automation stopped by viewport Escape"));
    StopRow->SetStringField(TEXT("status"),TEXT("NOT_RUN"));
    StopRow->SetStringField(TEXT("evidence_level"),bM3AbortWasBKey?TEXT("B-Key developer exercise"):TEXT("native viewport event"));
    StopRow->SetStringField(TEXT("expected"),TEXT("user stop is not a gameplay failure or completed test"));
    StopRow->SetStringField(TEXT("actual"),TEXT("automation permanently latched off; no new steps, injected actions, auto-resume or auto-quit; original Esc/P gameplay mapping retained"));
    StopRow->SetNumberField(TEXT("elapsed_seconds"),M3AbortWall-StartedAt); Results.Add(StopRow);
    for (int32 Index=FMath::Max(0,StepIndex);Index<Steps.Num();++Index)
    {
        const auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"),TEXT("Unfinished step: ")+Steps[Index].Name);
        Row->SetStringField(TEXT("status"),TEXT("NOT_RUN"));
        Row->SetStringField(TEXT("evidence_level"),TEXT("user stop"));
        Row->SetStringField(TEXT("actual"),Index==StepIndex?TEXT("active step interrupted; no End callback executed"):TEXT("step never started after stop"));
        Row->SetNumberField(TEXT("elapsed_seconds"),M3AbortWall-StartedAt); Results.Add(Row);
    }
    WriteReport();
    UE_LOG(LogTemp,Display,TEXT("M3_TEST_USER_ABORTED %s"),*RunDirectory);
    // Deliberately no M1_TEST_FINISHED: successful collectors require COMPLETE.
    SetActorTickEnabled(false);
}
bool AHCM1TestRunner::IsDriving() const
{ return PC && Car && PC->GetPlayerMode() == EHCPlayerMode::Driving && PC->GetPawn() == Car && Car->HasDriver(); }
bool AHCM1TestRunner::IsOnFoot() const
{ return PC && Character && PC->GetPlayerMode() == EHCPlayerMode::OnFoot && PC->GetPawn() == Character && !Character->IsHidden() && Character->GetActorEnableCollision(); }
FString AHCM1TestRunner::State() const
{ return PC ? FString::Printf(TEXT("mode=%d pawn=%s speed=%.3f km/h characterCount=%d"), int32(PC->GetPlayerMode()), *GetNameSafe(PC->GetPawn()), Car ? Car->GetSpeedKmh() : -1, CountCharacters()) : TEXT("No controller"); }
int32 AHCM1TestRunner::CountCharacters() const
{ int32 Count = 0; for (TActorIterator<AHCM1Character> It(GetWorld()); It; ++It) ++Count; return Count; }

void AHCM1TestRunner::Check(const FString& Name, bool bPassed, const FString& Expected, const FString& Actual, const FString& Evidence)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), Name); Row->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Row->SetStringField(TEXT("evidence_level"), Evidence); Row->SetStringField(TEXT("expected"), Expected);
    Row->SetStringField(TEXT("actual"), Actual); Row->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - StartedAt);
    Results.Add(Row);
    UE_LOG(LogTemp, Display, TEXT("M1_TEST_%s [%s] %s : %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Evidence, *Name, *Actual);
    WriteReport();
}

void AHCM1TestRunner::WriteReport()
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), bM3UserAbort ? TEXT("USER_ABORTED") : bFinished ? TEXT("COMPLETE") : TEXT("RUNNING"));
    Root->SetStringField(TEXT("mode"), Mode); Root->SetStringField(TEXT("map"), GetWorld()->GetMapName());
    Root->SetStringField(TEXT("test_suite"), bM5Run ? TEXT("M5_VS1") : bM4Run ? (Mode.StartsWith(TEXT("r2_")) ? TEXT("M4_R2") : TEXT("M4_V1")) : bM3Run ? TEXT("M3_V1") : bM2Run ? TEXT("M2_V1") : TEXT("M1"));
    Root->SetStringField(TEXT("world_type"), GetWorld()->WorldType == EWorldType::PIE ? TEXT("PIE") : TEXT("Game"));
    Root->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("recorded_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("duration_seconds"), FPlatformTime::Seconds() - StartedAt);
    Root->SetNumberField(TEXT("simulation_seconds"), SimulationElapsed);
    Root->SetStringField(TEXT("phase_clock"), TEXT("world DeltaSeconds while running; wall time only while paused"));
    Root->SetNumberField(TEXT("completed_enter_exit_cycles"), CompletedCycles);
    Root->SetStringField(TEXT("input_method"), bM4Run
        ? TEXT("M4: explicit A fixtures and normal B EnhancedInput/Key input; per-check provenance. Not OS input or user visual acceptance.") : bM3Run
        ? TEXT("M3 B-Action: normal EnhancedInput actions. A position/slot fixtures are explicitly labeled; no OS input, natural complete route, or video is inferred.") : bM2Run
        ? TEXT("M2 B: normal Move/Drive/Interact/Pause/Save/Load Actions; mouse MouseX/Y and preset T use the Key layer. A placement is labeled. This is not OS input or a full street route/performance acceptance.") : Mode == TEXT("playthrough")
        ? TEXT("B: continuous UEnhancedPlayerInput::InjectInputForAction gameplay. No actor placement, teleport, fixture, direct possession or player-mode assignment in this route; control-view angles only.")
        : TEXT("B: UEnhancedPlayerInput::InjectInputForAction each frame; normal bound actions. Explicit placements/obstacles are test fixtures, never counted as driving."));
    if (Mode == TEXT("playthrough"))
    {
        double PlannedSeconds = 0;
        for (const FStep& Step : Steps) PlannedSeconds += Step.Duration;
        Root->SetNumberField(TEXT("planned_route_seconds"), PlannedSeconds);
        Root->SetBoolField(TEXT("auto_quit_disabled_for_recording"), !bM3Run && !bM4Run);
    }
    Root->SetStringField(TEXT("save_slot"), PC ? PC->GetSaveSlotName() : TEXT(""));
    if (bM3EscapeScope)
    {
        const auto Stop=MakeShared<FJsonObject>();
        Stop->SetBoolField(TEXT("viewport_hook_bound"),M3EscapeHandle.IsValid());
        Stop->SetBoolField(TEXT("user_stop_latched"),bM3UserAbort);
        Stop->SetStringField(TEXT("event_source"),bM3UserAbort?(bM3AbortWasBKey?TEXT("B-Key developer GameViewport::InputKey exercise; NOT OS"):TEXT("GameViewport::OnInputKey Escape pressed; native viewport event, not proof of OS provenance")):TEXT("NOT_RUN"));
        Stop->SetStringField(TEXT("scope"),TEXT("This revision's explicit isolated test only. Esc is not consumed; normal game pause binding remains. No global keyboard interception, P override, automatic resume or auto-quit after stop."));
        Stop->SetStringField(TEXT("queued_input_scope"),TEXT("UE5.8 exposes no action-scoped removal for private InputsInjectedThisTick. No mappings cleared or private access; pressed keys flushed, gameplay motion cleared, every harness update/injection/new step permanently stopped. Normal Esc pause handles already queued input; same-frame queue clearing is not claimed."));
        Stop->SetNumberField(TEXT("event_frame"),double(M3AbortFrame));
        Stop->SetNumberField(TEXT("held_actions_at_event"),M3HeldActionsAtAbort);
        Stop->SetNumberField(TEXT("held_actions_now"),Held.Num());
        Stop->SetNumberField(TEXT("runner_held_injection_calls_after_event"),double(bM3UserAbort?M3InjectedActions-M3InjectedActionsAtAbort:0));
        Stop->SetBoolField(TEXT("game_paused_at_report"),UGameplayStatics::IsGamePaused(this));
        Stop->SetBoolField(TEXT("pause_menu_at_report"),PC && PC->IsPauseMenuOpen());
        Stop->SetStringField(TEXT("car_input_telemetry_at_report"),Car?Car->GetDriveTelemetry():TEXT("no resolved car"));
        Stop->SetStringField(TEXT("character_velocity_at_report"),Character?Character->GetVelocity().ToString():TEXT("no resolved character"));
        Stop->SetBoolField(TEXT("auto_quit_suppressed"),bM3UserAbort && bM3SuppressAutoQuit);
        Root->SetObjectField(TEXT("m3_escape_stop"),Stop);
    }
    if (CameraTest.IsValid()) WriteCameraReport(Root.ToSharedRef());
    if (LookCalibration.IsValid()) WriteLookCalibrationReport(Root.ToSharedRef());
    if (M2Route.IsValid()) WriteM2RouteReport(Root.ToSharedRef());
    if (M2Camera.IsValid()) WriteM2CameraReport(Root.ToSharedRef());
    if (M4Test.IsValid()) WriteM4Report(Root.ToSharedRef());
    if (M4R2Combat.IsValid()) WriteM4R2CombatReport(Root.ToSharedRef());
    if (M4R1Steering.IsValid()) WriteM4R1SteeringReport(Root.ToSharedRef());
    if (M4R1Reaction.IsValid()) WriteM4R1ReactionReport(Root.ToSharedRef());
    if (M4R1Impact.IsValid()) WriteM4R1ImpactReport(Root.ToSharedRef());
    if (M4R1Profile.IsValid()) WriteM4R1Profile(Root.ToSharedRef());
    if (M4R1Joint.IsValid()) WriteM4R1JointReport(Root.ToSharedRef());
    if (M4R1Quests.IsValid()) WriteM4R1QuestsReport(Root.ToSharedRef());
    if (M3Test.IsValid()) WriteM3Report(Root.ToSharedRef());
    if (M3LegacyActualReport.IsValid())
    {
        if (bFinished && M3LegacyActualReport->GetStringField(TEXT("status")) == TEXT("RUNNING"))
            M3LegacyActualReport->SetStringField(TEXT("status"), bM3UserAbort?TEXT("NOT_RUN"):TEXT("FAIL"));
        Root->SetObjectField(TEXT("m3_actual_legacy"), M3LegacyActualReport);
        Root->SetStringField(TEXT("input_method"), TEXT("B-Action F9 loads root-provided historical save bytes from an isolated M3 slot. Read-only byte comparisons before/after; no actor fixture, save creation, reserialization or OS input."));
    }
    if (M3Safety.IsValid()) WriteM3SafetyReport(Root.ToSharedRef());
    if (M3PlaythroughState.IsValid()) WriteM3PlaythroughReport(Root.ToSharedRef());
    TArray<TSharedPtr<FJsonValue>> Rows; for (const auto& Row : Results) Rows.Add(MakeShared<FJsonValueObject>(Row));
    Root->SetArrayField(TEXT("checks"), Rows);
    int32 Failures = 0; for (const auto& Row : Results) if (Row->GetStringField(TEXT("status")) == TEXT("FAIL")) ++Failures;
    Root->SetNumberField(TEXT("failures"), Failures);
    if (FrameTimes.Num())
    {
        TArray<double> Sorted = FrameTimes; Sorted.Sort();
        double Sum = 0; int32 Hitches = 0; for (double T : Sorted) { Sum += T; if (T > 100) ++Hitches; }
        Root->SetNumberField(TEXT("render_loop_mean_fps"), 1000.0 / (Sum / Sorted.Num()));
        Root->SetNumberField(TEXT("p99_frame_ms"), Sorted[FMath::Min(Sorted.Num() - 1, FMath::FloorToInt(Sorted.Num() * 0.99))]);
        Root->SetNumberField(TEXT("frames_over_100ms"), Hitches);
        Root->SetStringField(TEXT("performance_scope"), TEXT("Wall-clock actor ticks during scripted functional tests, includes pause and fixture overhead; not a standardized gameplay benchmark."));
    }
    FString Json; TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(Json, *(RunDirectory / TEXT("results.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool AHCM1TestRunner::IsM4R1ImpactProfileCaptureSuppressed() const
{
    if (!bM4Run || Mode != TEXT("r1_impact") || !PC || !FParse::Param(FCommandLine::Get(), TEXT("HCM4R1ImpactProfile"))) return false;
    const FString Slot = PC->GetSaveSlotName();
    const FString Prefix = TEXT("HarborCity_M2_V1_Test_M4_R1_");
    if (!Slot.StartsWith(Prefix) || Slot.Len() <= Prefix.Len() || Slot.Len() > 100) return false;
    for (TCHAR C : Slot) if (!FChar::IsAlnum(C) && C != TEXT('_')) return false;
    return true;
}

void AHCM1TestRunner::Capture(const FString& Name)
{
    // Extra performance sample only. Normal functional runs retain their real PNG evidence.
    if (IsM4R1ImpactProfileCaptureSuppressed()) return;
    const FString Filename = RunDirectory / (Name + TEXT(".png"));
    if (bM3Run || bM4Run)
    {
        M3CaptureFiles.AddUnique(Filename);
        M3LastCaptureFrame = GFrameCounter;
    }
    FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), bM2Run || bM3Run || bM4Run);
}
void AHCM1TestRunner::AddM3CaptureCompletion(const FString& Name)
{
    AddStep(Name + TEXT(" screenshot completion"), .8, [] {}, [this, Name]
    {
        const FString Filename = RunDirectory / (Name + TEXT(".png"));
        const int64 Bytes = IFileManager::Get().FileSize(*Filename);
        const bool bPending = FScreenshotRequest::IsScreenshotRequested();
        Check(Name + TEXT(" actual screenshot after settle"), Bytes > 0 && !bPending,
            TEXT("actual nonempty PNG and screenshot request processed after .8 seconds"),
            FString::Printf(TEXT("file=%s bytes=%lld pending=%d"), *Filename, Bytes, bPending), TEXT("A"));
    });
}
void AHCM1TestRunner::PlaceCharacter(const FVector& Location, float Yaw)
{
    if (!Character || !IsOnFoot()) return;
    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->SetActorLocationAndRotation(Location, FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation(FRotator(-10, Yaw, 0));
}
void AHCM1TestRunner::ApproachCar()
{
    const FVector Position = Car->GetActorTransform().TransformPosition(FVector(15, -240, 0));
    PlaceCharacter(FVector(Position.X, Position.Y, CarHome.GetLocation().Z + 90), Car->GetActorRotation().Yaw + 90);
}
void AHCM1TestRunner::PlaceCar(const FTransform& Transform)
{
    Car->ClearDriveInput(true);
    Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    Car->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
    Car->GetMesh()->WakeAllRigidBodies();
}
AActor* AHCM1TestRunner::BoxFixture(const FVector& Location, const FVector& Size, float Yaw)
{
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator(0, Yaw, 0), Params);
    Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Actor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Actor->SetActorScale3D(Size / 100.0); Fixtures.Add(Actor); return Actor;
}
void AHCM1TestRunner::ClearFixtures() { for (AActor* A : Fixtures) if (IsValid(A)) A->Destroy(); Fixtures.Empty(); }

void AHCM1TestRunner::BuildSequence()
{
    if (bM4Run) { BuildM4Sequence(); return; }
    if (bM3Run)
    {
        if (Mode == TEXT("legacy_actual")) BuildM3LegacyActualSequence();
        else BuildM3Sequence();
        return;
    }
    if (bM2Run) { BuildM2Sequence(); return; }
    if (Mode == TEXT("look_calibration")) { BuildLookCalibrationSequence(); return; }
    if (Mode == TEXT("camera") || Mode == TEXT("camera_legacy") || Mode == TEXT("camera_load")) { BuildCameraSequence(); return; }
    if (Mode == TEXT("safety")) { BuildSafetySequence(); return; }
    if (Mode == TEXT("playthrough")) { BuildPlaythroughSequence(); return; }
    if (Mode == TEXT("load"))
    {
        AddStep(TEXT("load baseline"), 0.4, [this]
        { Check(TEXT("Save exists after fresh process launch"), UGameplayStatics::DoesSaveGameExist(PC->GetSaveSlotName(), 0), TEXT("existing isolated save"), PC->GetSaveSlotName(), TEXT("A")); });
        Tap(TEXT("Load"), 1.5);
        AddStep(TEXT("persistent data assertion"), 0.5, [this]
        {
            UHCM1SaveGame* S = Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(), 0));
            const bool Match = S && FVector::Dist(Character->GetActorLocation(), S->PlayerTransform.GetLocation()) < 30 &&
                FVector::Dist(Car->GetActorLocation(), S->VehicleTransform.GetLocation()) < 40 &&
                FMath::Abs(FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw, S->VehicleTransform.Rotator().Yaw)) < 3 &&
                S->LightStates.Contains(Lamp->StableId) && Lamp->IsLightEnabled() == S->LightStates[Lamp->StableId];
            Check(TEXT("Fresh launch F9 restores player car light"), Match && IsOnFoot(), TEXT("saved transforms within 30/40cm, yaw <3deg, exact light state, OnFoot"), State() + TEXT(" ") + PC->GetStatusMessage());
            Capture(TEXT("05_packaged_reloaded"));
        });
        return;
    }
    AddStep(TEXT("initial foot"), 0.4, [this] { Check(TEXT("Initial on-foot ownership"), IsOnFoot() && CountCharacters() == 1, TEXT("one visible character possessed"), State()); Capture(TEXT("01_on_foot")); });
    AddStep(TEXT("walk start fixture"), 0.5, [this] { PlaceCharacter(FVector(-1800, -3300, 110)); });
    AddStep(TEXT("walk"), 1.2, [this] { SamplePosition = Character->GetActorLocation(); Hold(TEXT("Move"), FInputActionValue(FVector2D(0, 1))); }, [this]
    { WalkDistance = FVector::Dist2D(SamplePosition, Character->GetActorLocation()); Check(TEXT("Walking movement"), WalkDistance > 200, TEXT(">200cm from 1.2s forward action"), FString::Printf(TEXT("distance=%.1fcm velocity=%.1f"), WalkDistance, Character->GetVelocity().Size())); Release(TEXT("Move")); });
    AddStep(TEXT("walk release"), 0.6, [] {}, [this] { Check(TEXT("Walk release stops"), Character->GetVelocity().Size2D() < 10, TEXT("horizontal speed <10cm/s"), Character->GetVelocity().ToString()); });
    AddStep(TEXT("sprint"), 1.2, [this] { SamplePosition = Character->GetActorLocation(); Hold(TEXT("Sprint"), FInputActionValue(true)); Hold(TEXT("Move"), FInputActionValue(FVector2D(0,1))); }, [this]
    { const float D = FVector::Dist2D(SamplePosition, Character->GetActorLocation()); Check(TEXT("Sprinting faster than walking"), D > WalkDistance * 1.25f, TEXT(">1.25x walk distance for same duration"), FString::Printf(TEXT("sprint %.1fcm walk %.1fcm"), D, WalkDistance)); Release(TEXT("Move")); Release(TEXT("Sprint")); });
    AddStep(TEXT("settle for jump"), 0.5, [this] { PeakHeight = Character->GetActorLocation().Z; SamplePosition = Character->GetActorLocation(); });
    Tap(TEXT("Jump"), 1.5);
    AddStep(TEXT("jump landing"), 0.3, [this] { Check(TEXT("Jump and grounded landing"), PeakHeight > SamplePosition.Z + 35 && Character->GetCharacterMovement()->IsMovingOnGround(), TEXT("rise >35cm then grounded"), FString::Printf(TEXT("rise %.1f grounded=%d"), PeakHeight-SamplePosition.Z, Character->GetCharacterMovement()->IsMovingOnGround())); });
    AddStep(TEXT("look"), 0.6, [this] { SampleRotation=PC->GetControlRotation(); Hold(TEXT("Look"), FInputActionValue(FVector2D(1.5,0.2))); }, [this] { Release(TEXT("Look")); Check(TEXT("Look changes camera rotation"), FMath::Abs(FMath::FindDeltaAngleDegrees(SampleRotation.Yaw, PC->GetControlRotation().Yaw)) > 5, TEXT("yaw change >5 degrees"), PC->GetControlRotation().ToString()); });
    AddStep(TEXT("camera wall fixture"), 0.6, [this] { PlaceCharacter(FVector(-1800,-2800,110)); BoxFixture(FVector(-2000,-2800,200), FVector(30,600,400)); }, [this]
    { const float D=FVector::Dist(Character->GetFollowCamera()->GetComponentLocation(), Character->GetCameraBoom()->GetComponentLocation()); Check(TEXT("Chase camera collision"), D < Character->GetCameraBoom()->TargetArmLength - 50, TEXT("wall contracts spring arm >50cm"), FString::Printf(TEXT("camera distance %.1f target %.1f"),D,Character->GetCameraBoom()->TargetArmLength)); ClearFixtures(); });
    if (Mode == TEXT("foot")) return;
    AddStep(TEXT("vehicle contact inspection"), 1, [this] { PlaceCar(CarHome); ApproachCar(); }, [this]
    {
        auto* M=Car->GetChaosMovement(); int32 Contacts=0;
        for(int32 I=0; I<M->GetNumWheels(); ++I) if(M->GetWheelState(I).bInContact) ++Contacts;
        Check(TEXT("Chaos wheels contact ground"), M->GetNumWheels()==4 && Contacts==4 && Car->GetMesh()->IsSimulatingPhysics(), TEXT("four contacts with simulating chassis"), FString::Printf(TEXT("wheels=%d contacts=%d sim=%d"), M->GetNumWheels(),Contacts,Car->GetMesh()->IsSimulatingPhysics()), TEXT("A"));
        Capture(TEXT("02_near_car_prompt"));
    });
    Tap(TEXT("Interact"));
    AddStep(TEXT("driving ownership"), 0.2, [this] { Check(TEXT("Normal E enters car"), IsDriving() && Character->IsHidden() && !Character->GetActorEnableCollision(), TEXT("car possessed, character hidden with collision disabled"), State()); PeakSpeed=0; SamplePosition=Car->GetActorLocation(); });
    AddStep(TEXT("accelerate"), 2.2, [this] { Hold(TEXT("Drive"), FInputActionValue(FVector2D(0,1))); }, [this]
    { Check(TEXT("Physics acceleration"), PeakSpeed>12 && FVector::Dist2D(SamplePosition,Car->GetActorLocation())>250, TEXT(">12km/h and >250cm through Chaos throttle"), FString::Printf(TEXT("peak %.2fkm/h travel %.1fcm %s"),PeakSpeed,FVector::Dist2D(SamplePosition,Car->GetActorLocation()),*Car->GetDriveTelemetry())); Capture(TEXT("03_driving")); });
    Tap(TEXT("Interact"),0.4);
    AddStep(TEXT("moving exit rejection"),0.2,[this] { Check(TEXT("High-speed exit rejected"), IsDriving() && Car->GetSpeedKmh()>1,TEXT("stays Driving while >1km/h"),State()+TEXT(" ")+PC->GetStatusMessage()); });
    AddStep(TEXT("turn"),0.8,[this] { SampleRotation=Car->GetActorRotation(); Hold(TEXT("Drive"),FInputActionValue(FVector2D(0.5,0.25))); },[this]
    { Check(TEXT("Physics steering"), FMath::Abs(FMath::FindDeltaAngleDegrees(SampleRotation.Yaw,Car->GetActorRotation().Yaw))>3,TEXT("yaw change >3deg from steering input"),Car->GetActorRotation().ToString()); Release(TEXT("Drive")); });
    AddStep(TEXT("brake before reverse"),0.8,[this] { bTrackBraking=true; bUnsafeReverse=false; bObservedNearStop=false; Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,-1))); });
    AddStep(TEXT("reverse hold"),3,[this] {},[this]
    { bTrackBraking=false; Check(TEXT("S brakes before reversing"),!bUnsafeReverse && bObservedNearStop,TEXT("per-frame: no reverse throttle while forward >50cm/s; passes near zero"),FString::Printf(TEXT("unsafe=%d near_stop=%d"),bUnsafeReverse,bObservedNearStop)); Check(TEXT("S reverses after stop"),Car->GetSignedSpeed()<-40,TEXT("negative forward speed <-40cm/s after braking"),Car->GetDriveTelemetry()); Release(TEXT("Drive")); });
    if (Mode.StartsWith(TEXT("reverse_diagnostic"))) return;
    AddStep(TEXT("handbrake stop"),2.5,[this] { Hold(TEXT("Handbrake"),FInputActionValue(true)); },[this]
    { Check(TEXT("Handbrake stops car"),Car->GetSpeedKmh()<1,TEXT("speed <1km/h"),Car->GetDriveTelemetry()); Release(TEXT("Handbrake")); });
    Tap(TEXT("Pause"),0.4);
    AddStep(TEXT("paused"),0.2,[this] { Check(TEXT("Pause action"),PC->IsPauseMenuOpen() && UGameplayStatics::IsGamePaused(this),TEXT("pause menu and paused world"),State()); });
    Tap(TEXT("Pause"),0.5);
    AddStep(TEXT("resumed"),0.2,[this] { Check(TEXT("Resume clears residual input"),!PC->IsPauseMenuOpen() && FMath::IsNearlyZero(Car->GetChaosMovement()->GetThrottleInput()) && FMath::IsNearlyZero(Car->GetChaosMovement()->GetSteeringInput()),TEXT("unpaused; zero throttle and steering"),Car->GetDriveTelemetry()); });
    AddStep(TEXT("rollover fixture"),0.6,[this] { PlaceCar(FTransform(FRotator(0,0,180),FVector(-2000,-3800,140))); });
    Tap(TEXT("ResetVehicle"),1.5);
    AddStep(TEXT("reset assertion"),0.2,[this] { Check(TEXT("R safe upright reset preserves possession"),IsDriving() && Car->GetActorUpVector().Z>0.85 && Car->GetSpeedKmh()<2,TEXT("Driving upright, speed<2km/h"),State()+TEXT(" ")+Car->GetActorRotation().ToString()); });
    AddStep(TEXT("drive after reset"),1,[this] { Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,1))); },[this] { Check(TEXT("Can drive after reset"),Car->GetSpeedKmh()>3,TEXT(">3km/h after throttle"),State()); Release(TEXT("Drive")); });
    AddStep(TEXT("stop after reset"),2,[this] { Hold(TEXT("Handbrake"),FInputActionValue(true)); },[this] { Release(TEXT("Handbrake")); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("exit after reset"),0.3,[this] { Check(TEXT("Exit restores foot character"),IsOnFoot() && !Car->HasDriver(),TEXT("visible colliding possessed character and empty seat"),State()); });
    AddStep(TEXT("slope entry fixture"),0.5,[this] { ApproachCar(); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("slope vehicle fixture"),1,[this] { PlaceCar(FTransform(FRotator(6,0,0),FVector(-700,1000,100))); });
    AddStep(TEXT("drive up six degree ramp"),0.9,[this] { SamplePosition=Car->GetActorLocation(); Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,0.6))); },[this]
    { Check(TEXT("Physics drives up actual 6 degree ramp"),Car->GetActorLocation().X>SamplePosition.X+40 && Car->GetActorLocation().Z>SamplePosition.Z+3,TEXT("advances uphill >40cm and rises >3cm"),FString::Printf(TEXT("delta=%s speed=%.2f"),*(Car->GetActorLocation()-SamplePosition).ToString(),Car->GetSpeedKmh())); Release(TEXT("Drive")); });
    AddStep(TEXT("slope brake"),2,[this] { Hold(TEXT("Handbrake"),FInputActionValue(true)); },[this] { SamplePosition=Car->GetActorLocation(); });
    AddStep(TEXT("slope stationary observation"),4,[] {},[this]
    { Check(TEXT("Slope handbrake holds for four seconds"),Car->GetSpeedKmh()<1 && FVector::Dist(SamplePosition,Car->GetActorLocation())<15 && FMath::Abs(Car->GetActorRotation().Pitch)>3,TEXT("on slope >3deg, speed<1km/h, 4s drift<15cm"),FString::Printf(TEXT("drift=%.1fcm pitch=%.2f speed=%.2fkm/h"),FVector::Dist(SamplePosition,Car->GetActorLocation()),Car->GetActorRotation().Pitch,Car->GetSpeedKmh())); Release(TEXT("Handbrake")); });
    Tap(TEXT("Interact"));
    if (Mode == TEXT("vehicle")) return;
    for(int32 Cycle=1; Cycle<=20; ++Cycle)
    {
        AddStep(FString::Printf(TEXT("cycle %d fixture"),Cycle),0.4,[this] { Held.Empty(); PlaceCar(CarHome); ApproachCar(); });
        Tap(TEXT("Interact"),0.4);
        AddStep(TEXT("cycle enter check"),0.05,[this,Cycle] { bCycleEntered=IsDriving() && CountCharacters()==1 && PC->GetControlledCharacter()==Character; Check(FString::Printf(TEXT("Cycle %02d enter"),Cycle),bCycleEntered,TEXT("normal Interact drives same car, exactly one original character"),State()); });
        Tap(TEXT("Interact"),0.4);
        AddStep(TEXT("cycle exit check"),0.05,[this,Cycle] { const bool Pass=bCycleEntered && IsOnFoot() && !Car->HasDriver() && CountCharacters()==1 && PC->GetControlledCharacter()==Character; if(Pass) ++CompletedCycles; Check(FString::Printf(TEXT("Cycle %02d exit"),Cycle),Pass,TEXT("successful paired entry then exit returns original character and clears seat"),State()); });
    }
    AddStep(TEXT("blocked exit fixture"),0.5,[this] { PlaceCar(CarHome); ApproachCar(); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("left barrier fixture"),0.4,[this]
    { const FTransform T=Car->GetActorTransform(); FVector L=T.TransformPosition(FVector(0,-190,0)); L.Z=170; BoxFixture(L,FVector(540,70,340),T.Rotator().Yaw); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("alternate exit check"),0.3,[this]
    { const double Side=FVector::DotProduct(Character->GetActorLocation()-Car->GetActorLocation(),Car->GetActorRightVector()); Check(TEXT("Blocked driver side uses passenger side"),IsOnFoot() && Side>100,TEXT("on foot on positive Y side"),State()+FString::Printf(TEXT(" side=%.1f"),Side)); ClearFixtures(); });
    AddStep(TEXT("both exit setup"),0.4,[this] { ApproachCar(); }); Tap(TEXT("Interact"));
    AddStep(TEXT("both barriers fixture"),0.4,[this]
    { Check(TEXT("All-exits-blocked precondition"),IsDriving() && Car->GetSpeedKmh()<1,TEXT("driving ownership but stopped before obstacles"),State(),TEXT("A")); for(float Side : {-1.f,1.f}) { FVector L=Car->GetActorTransform().TransformPosition(FVector(0,Side*190,0)); L.Z=170; BoxFixture(L,FVector(540,70,340),Car->GetActorRotation().Yaw); } });
    Tap(TEXT("Interact"));
    AddStep(TEXT("both blocked assertion"),0.3,[this] { Check(TEXT("All exits blocked rejects exit"),IsDriving(),TEXT("stays Driving with rejection prompt"),State()+TEXT(" ")+PC->GetStatusMessage()); ClearFixtures(); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("transition stress fixture"),0.4,[this] { ApproachCar(); });
    for (int32 I=0; I<6; ++I)
    {
        AddStep(TEXT("rapid E down"),0.025,[this] { Hold(TEXT("Interact"),FInputActionValue(true)); });
        AddStep(TEXT("rapid E up"),0.025,[this] { Release(TEXT("Interact")); });
    }
    AddStep(TEXT("stress settle"),0.7,[] {},[this]
    { Check(TEXT("Rapid E transitions settle without duplicate actors"),(IsDriving()||IsOnFoot()) && CountCharacters()==1 && PC->GetControlledCharacter()==Character,TEXT("stable mode and exactly original character after 6 taps"),State()); });
    AddStep(TEXT("stress release and exit"),0.1,[this] { if(IsDriving()) Hold(TEXT("Interact"),FInputActionValue(true)); });
    AddStep(TEXT("stress exit settled"),0.6,[this] { Release(TEXT("Interact")); });
    AddStep(TEXT("paused transition fixture"),0.4,[this] { ApproachCar(); });
    AddStep(TEXT("entering E"),0.03,[this] { Hold(TEXT("Interact"),FInputActionValue(true)); });
    AddStep(TEXT("pause during entering"),0.06,[this] { Release(TEXT("Interact")); Hold(TEXT("Pause"),FInputActionValue(true)); });
    AddStep(TEXT("transition pause release"),0.4,[this] { Release(TEXT("Pause")); },[this] { Check(TEXT("Pause available during transition"),PC->IsPauseMenuOpen(),TEXT("pause accepted while transitioning"),State()); });
    Tap(TEXT("Pause"),0.7);
    AddStep(TEXT("transition resumed"),0.2,[this] { Check(TEXT("Paused transition resumes correctly"),IsDriving() && !PC->IsPauseMenuOpen(),TEXT("Driving after resume"),State()); });
    AddStep(TEXT("save rejection while driving"),0.1,[this] { Check(TEXT("Driving save is explicitly refused"),!PC->SaveGameNow(),TEXT("false with gameplay message"),PC->GetStatusMessage(),TEXT("A")); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("interaction fixture"),0.6,[this] { PlaceCharacter(Lamp->GetActorLocation()+FVector(-180,0,60)); });
    Tap(TEXT("Interact"));
    AddStep(TEXT("real light changed"),0.5,[this]
    { const UPointLightComponent* L=Lamp->FindComponentByClass<UPointLightComponent>(); Check(TEXT("E changes physical point light"),L && Lamp->IsLightEnabled()!=bInitialLight && L->IsVisible()==Lamp->IsLightEnabled() && (L->Intensity>0)==Lamp->IsLightEnabled(),TEXT("bool, visibility and intensity all change"),FString::Printf(TEXT("enabled=%d intensity=%.1f"),Lamp->IsLightEnabled(),L ? L->Intensity : -1)); Capture(TEXT("04_after_exit_interaction")); });
    AddStep(TEXT("save settled"),0.5,[] {}); Tap(TEXT("Save"));
    AddStep(TEXT("save disk assertion"),0.4,[this]
    { UHCM1SaveGame* S=Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0)); const bool Match=S && S->Version==1 && S->VehicleId==Car->StableId && S->LightStates.Contains(Lamp->StableId) && S->LightStates[Lamp->StableId]==Lamp->IsLightEnabled() && FVector::Dist(S->PlayerTransform.GetLocation(),Character->GetActorLocation())<30 && FVector::Dist(S->VehicleTransform.GetLocation(),Car->GetActorLocation())<30; Check(TEXT("F5 saves current versioned player vehicle light"),Match,TEXT("version1 stable IDs and exact current light, both current transforms within30cm; isolated per-run slot"),PC->GetStatusMessage()); });
}

void AHCM1TestRunner::Finish()
{
    if (bM3UserAbort) return;
    if (bFinished) return;
    if (M5ReviewCameraCleanup)
    {
        TFunction<void()> Cleanup = MoveTemp(M5ReviewCameraCleanup);
        M5ReviewCameraCleanup = nullptr;
        Cleanup();
    }
    if (LookCalibration.IsValid()) StopLookCalibrationInput();
    if (M2Route.IsValid()) StopM2RouteInput();
    if (M2Camera.IsValid()) StopM2CameraInput();
    if (M3Test.IsValid()) StopM3TestInput();
    if (M4Test.IsValid()) StopM4TestInput();
    if (M3Safety.IsValid()) StopM3SafetyInput();
    bFinished=true; Held.Empty(); ClearFixtures();
    if (PC) PC->FlushPressedKeys();
    bool bAudioPending = false;
    if (bM5Run || Mode.StartsWith(TEXT("r2_"))) for (TActorIterator<AHCM3Recording> It(GetWorld()); It; ++It)
    { It->FinishR2GameplayCapture(); bAudioPending |= It->IsR2AudioExportPending(); }
    // M3 UI screenshots synchronously redraw/read back through Slate after the
    // actor tick. Do not request engine exit in that same frame or while pending.
    if ((bM3Run || bM4Run) && !bM3EndingPlay && (bAudioPending || FScreenshotRequest::IsScreenshotRequested()
        || (M3LastCaptureFrame != MAX_uint64 && GFrameCounter <= M3LastCaptureFrame + 1)))
    {
        bM3FinishPending = true;
        M3FinishWaitStarted = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("M3_TEST_FINISH_WAIT_SCREENSHOT frame=%llu pending=%d"),
            GFrameCounter, FScreenshotRequest::IsScreenshotRequested());
        return;
    }
    CompleteFinish();
}
void AHCM1TestRunner::CompleteFinish()
{
    if (bM3UserAbort) { CompleteM3UserAbort(); return; }
    bM3FinishPending = false;
    if (bM4Run && Mode == TEXT("r1_joint"))
        for (TActorIterator<AHCM3Recording> It(GetWorld()); It; ++It) It->FinishR1GameplayCapture();
    if (bM3Run || bM4Run)
    {
        for (const FString& Filename : M3CaptureFiles)
        {
            const int64 Bytes = IFileManager::Get().FileSize(*Filename);
            Check(TEXT("M3 final screenshot file ") + FPaths::GetCleanFilename(Filename),
                Bytes > 0 && !FScreenshotRequest::IsScreenshotRequested(),
                TEXT("actual nonempty requested PNG before test completion/normal exit"),
                FString::Printf(TEXT("file=%s bytes=%lld pending=%d"), *Filename, Bytes,
                    FScreenshotRequest::IsScreenshotRequested()), TEXT("A"));
        }
    }
    WriteReport();
    UE_LOG(LogTemp, Display,TEXT("M1_TEST_FINISHED %s"),*RunDirectory);
    SetActorTickEnabled(false);
    if ((Mode != TEXT("playthrough") || bM3Run || bM4Run) && PC && FParse::Param(FCommandLine::Get(),TEXT("M1AutoQuit"))
        && !bM3UserAbort && (!(bM3Run || bM4Run) || (!bM3SuppressAutoQuit && !bM3EndingPlay))) PC->QuitPrototype();
}
void AHCM1TestRunner::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UGameViewportClient* Viewport=M3EscapeViewport.Get()) Viewport->OnInputKey().Remove(M3EscapeHandle);
    M3EscapeHandle.Reset();
    if (bM3UserAbort)
    {
        CompleteM3UserAbort();
        Super::EndPlay(Reason);
        return;
    }
    if (bM3Run || bM4Run) bM3EndingPlay = true;
    if (bM3FinishPending)
    {
        Check(TEXT("M3 screenshot drain interrupted"), false, TEXT("screenshot completion before world shutdown"), TEXT("EndPlay while waiting"), TEXT("A"));
        bM3SuppressAutoQuit = true;
        CompleteFinish();
    }
    else if(bRunning && !bFinished) { Check(TEXT("Test interrupted"),false,TEXT("sequence finishes"),TEXT("EndPlay before completion"),TEXT("A")); Finish(); }
    Super::EndPlay(Reason);
}
