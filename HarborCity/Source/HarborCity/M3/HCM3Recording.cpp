#include "HCM3Recording.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Character.h"
#include "Async/Async.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FrameGrabber.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/SceneViewport.h"
#include "UObject/Package.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace
{
bool ParseVS2RecordingSettings(float& Seconds, float& Delay, FString& Slot)
{
#if !UE_BUILD_SHIPPING
    Delay = 8.f;
    if (!FParse::Value(FCommandLine::Get(), TEXT("M5VS2RecordSeconds="), Seconds)
        || !FMath::IsFinite(Seconds) || Seconds < 20.f || Seconds > 120.f) return false;
    FParse::Value(FCommandLine::Get(), TEXT("M5VS2RecordDelay="), Delay);
    if (!FMath::IsFinite(Delay) || Delay < 3.f || Delay > 60.f
        || !FParse::Value(FCommandLine::Get(), TEXT("HCM1SaveSlot="), Slot)
        || !Slot.StartsWith(TEXT("HarborCity_VS2_"), ESearchCase::CaseSensitive)
        || Slot.Len() <= FString(TEXT("HarborCity_VS2_")).Len() || Slot.Len() > 100) return false;
    for (const TCHAR Letter : Slot) if (!FChar::IsAlnum(Letter) && Letter != TEXT('_')) return false;
    return true;
#else
    return false;
#endif
}

// Match the local UE 5.8 FFrameGrabber constructor's window-relative physical
// rectangle. Its surface reader normalizes DrawRectangle by this extent, so a
// smaller requested buffer leaves an unwritten border instead of resizing.
bool GetM3CaptureGeometry(const TSharedRef<SViewport>& Widget, FIntRect& Rect, FIntPoint& WindowSize)
{
    if (!FSlateApplication::IsInitialized()) return false;
    const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Widget);
    if (!Window.IsValid()) return false;
    const FGeometry InnerGeometry = Window->GetWindowGeometryInWindow();
    FArrangedChildren JustWindow(EVisibility::Visible);
    JustWindow.AddWidget(FArrangedWidget(Window.ToSharedRef(), InnerGeometry));
    FWidgetPath Path(Window.ToSharedRef(), JustWindow);
    if (!Path.ExtendPathTo(FWidgetMatcher(Widget), EVisibility::Visible)) return false;
    const FArrangedWidget Arranged = Path.FindArrangedWidget(Widget).Get(FArrangedWidget::GetNullWidget());
    const FVector2D Position = Arranged.Geometry.GetAbsolutePosition();
    const FVector2D Size = Arranged.Geometry.GetAbsoluteSize();
    const FVector2D AbsoluteWindowSize = InnerGeometry.GetAbsoluteSize();
    Rect = FIntRect(static_cast<int32>(Position.X), static_cast<int32>(Position.Y),
        static_cast<int32>(Position.X + Size.X), static_cast<int32>(Position.Y + Size.Y));
    WindowSize = FIntPoint(static_cast<int32>(AbsoluteWindowSize.X), static_cast<int32>(AbsoluteWindowSize.Y));
    return Rect.Min.X >= 0 && Rect.Min.Y >= 0 && Rect.Width() > 0 && Rect.Height() > 0
        && Rect.Max.X <= WindowSize.X && Rect.Max.Y <= WindowSize.Y;
}

TSharedRef<FJsonObject> M3SizeJSON(FIntPoint Size)
{
    TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
    Value->SetNumberField(TEXT("width"), Size.X);
    Value->SetNumberField(TEXT("height"), Size.Y);
    return Value;
}
}

struct FM3FrameTimestamp : IFramePayload { double Time = 0; explicit FM3FrameTimestamp(double InTime) : Time(InTime) {} };
struct AHCM3Recording::FRecordingState
{
    TUniquePtr<FFrameGrabber> Grabber;
    TFuture<bool> WriteTask;
    TArray<TSharedPtr<FJsonValue>> Frames;
    FString Directory;
    double Started = 0, NextCapture = 0;
    int32 Requested = 0, Dropped = 0, WriteFailures = 0;
    int32 FrameLatency = -1;
    FIntRect CaptureRect = FIntRect(0, 0, 0, 0);
    FIntPoint WindowSize = FIntPoint::ZeroValue, SceneViewportSize = FIntPoint::ZeroValue;
    FIntPoint RequestedSize = FIntPoint::ZeroValue, OutputSize = FIntPoint::ZeroValue;
    TWeakPtr<SViewport> ViewportWidget;
    bool bCaptureGeometryValid = false;
    float Seconds = 0, Delay = 8;
    bool bStopped = false, bM4 = false;
    bool bR2 = false, bAudioStarted = false, bAudioExportRequested = false;
    bool bVS2Part3 = false;
    bool bVS2 = false, bVS2HadFocus = false, bVS2EscapeStopped = false;
    uint64 VS2StopFrame = 0;
    FString RequestedSaveSlot, ActualSaveSlot;
    TWeakObjectPtr<UGameViewportClient> VS2Viewport;
    FDelegateHandle VS2InputHandle, VS2ActivationHandle;
    double AudioStartedWall = 0;
    int32 VS2PreviousNeverDisableSubmixes = -1;
    bool bVS2SubmixOverrideApplied = false, bVS2SubmixOverrideRestored = false;
    double FirstCaptureWorldSeconds = -1;
    FVector InitialPlayerLocation = FVector::ZeroVector;
    FVector InitialPlayerVelocity = FVector::ZeroVector;
};

AHCM3Recording::AHCM3Recording()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    Recording = MakeShared<FRecordingState>();
#if !UE_BUILD_SHIPPING
    FParse::Value(FCommandLine::Get(), TEXT("HCM3RecordSeconds="), Recording->Seconds);
    FParse::Value(FCommandLine::Get(), TEXT("HCM3RecordDelay="), Recording->Delay);
    Recording->bM4 = FParse::Value(FCommandLine::Get(), TEXT("HCM4RecordSeconds="), Recording->Seconds);
    if (Recording->bM4) Recording->Delay = 0;
    FString Mode;
    Recording->bR2 = FParse::Value(FCommandLine::Get(), TEXT("M4Test="), Mode) && Mode.StartsWith(TEXT("r2_"));
    Recording->bR2 |= FParse::Value(FCommandLine::Get(), TEXT("M5Test="), Mode) && Mode.StartsWith(TEXT("m5_"));
    Recording->bVS2Part3 = Mode.StartsWith(TEXT("m5_vs2_"));
#endif
    Recording->Seconds = FMath::Clamp(Recording->Seconds, 0.f, Recording->bM4 ? 360.f : 150.f);
    Recording->Delay = Recording->bM4 ? 0.f : FMath::Clamp(Recording->Delay, 3.f, 60.f);
    if (Recording->bVS2Part3) Recording->Delay = 8.f;
#if !UE_BUILD_SHIPPING
    float VS2Seconds = 0.f, VS2Delay = 8.f;
    if (FParse::Value(FCommandLine::Get(), TEXT("M5VS2RecordSeconds="), VS2Seconds))
    {
        // Explicit VS2 arguments take precedence; invalid values cannot silently
        // fall back to a legacy recording or clamp into a different duration.
        Recording->bVS2 = ParseVS2RecordingSettings(VS2Seconds, VS2Delay, Recording->RequestedSaveSlot);
        Recording->Seconds = Recording->bVS2 ? VS2Seconds : 0.f;
        Recording->Delay = Recording->bVS2 ? VS2Delay : 0.f;
        Recording->bM4 = Recording->bR2 = false;
    }
#endif
}
AHCM3Recording::~AHCM3Recording() = default;
bool AHCM3Recording::IsVS2GameplayCaptureRequested()
{
    float Seconds = 0.f, Delay = 8.f; FString Slot;
    return ParseVS2RecordingSettings(Seconds, Delay, Slot);
}
void AHCM3Recording::BeginPlay()
{
    Super::BeginPlay();
    FRecordingState& S = *Recording;
    if (!S.bVS2) return;
    if (!GetWorld()->IsGameWorld() || !GetWorld()->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/"), ESearchCase::CaseSensitive))
    { Stop(TEXT("vs2_private_map_required")); return; }
    // Create an isolated evidence directory before the delay so Esc can leave a
    // truthful zero-frame stop record instead of silently arming a later capture.
    S.Directory = FString(TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/recordings")) /
        (FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    if (!IFileManager::Get().MakeDirectory(*S.Directory, true))
    { Stop(TEXT("vs2_recording_directory_unavailable")); return; }
    BindVS2StopObservers();
}
void AHCM3Recording::BindVS2StopObservers()
{
    FRecordingState& S = *Recording;
    if (!S.bVS2 || S.bStopped) return;
    if (!S.VS2InputHandle.IsValid()) if (UGameViewportClient* Client = GetWorld()->GetGameViewport())
    {
        S.VS2Viewport = Client;
        S.VS2InputHandle = Client->OnInputKey().AddUObject(this, &AHCM3Recording::ObserveVS2Input);
    }
    if (!S.VS2ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
        S.VS2ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &AHCM3Recording::ObserveVS2Activation);
}
void AHCM3Recording::ObserveVS2Input(const FInputKeyEventArgs& Event)
{
    FRecordingState& S = *Recording;
    if (!S.bVS2 || S.bStopped || Event.Event != IE_Pressed) return;
    if (Event.Key == EKeys::Escape)
    {
        S.bVS2EscapeStopped = true; S.VS2StopFrame = GFrameCounter;
        Stop(TEXT("escape_pressed")); // Observe only; ordinary controller input is not consumed.
    }
    else if (Event.Key == EKeys::P) Stop(TEXT("pause_key_pressed"));
}
void AHCM3Recording::ObserveVS2Activation(bool bActive)
{
    if (Recording && Recording->bVS2 && !Recording->bStopped && !bActive)
        Stop(TEXT("application_focus_lost"));
}
void AHCM3Recording::FinishVS2GameplayCapture()
{
    if (Recording && Recording->bVS2) Stop(TEXT("vs2_gameplay_sequence_finished"));
}
bool AHCM3Recording::IsVS2AudioExportPending() const
{
    return Recording && Recording->bVS2 && IsR2AudioExportPending();
}
bool AHCM3Recording::HasCapturedFirstFrame() const { return Recording && Recording->Frames.Num() > 0; }
FString AHCM3Recording::GetCaptureDirectory() const { return Recording ? Recording->Directory : FString(); }
bool AHCM3Recording::IsR2AudioExportPending() const
{
    if(!Recording || !Recording->bAudioExportRequested)return false;
    // UE's WAV exporter writes on a worker thread. File existence/44 bytes can
    // precede the remaining PCM. Windows OpenRead(false) cannot share that writer;
    // successful open plus the actual PCM16 header/length proves export is closed.
    TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*(Recording->Directory/TEXT("game_audio.wav")),false));
    uint8 H[44];
    if(!File || File->Size()<=44 || !File->Read(H,44))return true;
    const auto U16=[&H](int32 I){return uint32(H[I])|(uint32(H[I+1])<<8);};
    const auto U32=[&H](int32 I){return uint32(H[I])|(uint32(H[I+1])<<8)|(uint32(H[I+2])<<16)|(uint32(H[I+3])<<24);};
    const uint32 Channels=U16(22),Rate=U32(24),Bytes=U32(40);
    return FMemory::Memcmp(H,"RIFF",4)!=0 || FMemory::Memcmp(H+8,"WAVEfmt ",8)!=0
        || U32(16)!=16 || U16(20)!=1 || Channels==0 || Rate==0 || U16(34)!=16
        || FMemory::Memcmp(H+36,"data",4)!=0 || U16(32)!=Channels*2 || U32(28)!=Rate*Channels*2
        || Bytes==0 || Bytes%(Channels*2)!=0 || int64(U32(4))+8!=File->Size() || int64(Bytes)+44!=File->Size();
}
void AHCM3Recording::FinishR2GameplayCapture()
{
#if !UE_BUILD_SHIPPING
    FString Slot;
    if (!Recording || !Recording->bR2 || !FParse::Value(FCommandLine::Get(), TEXT("HCM1SaveSlot="), Slot)
        || (!Slot.StartsWith(TEXT("HarborCity_M2_V1_Test_M4_R2_")) && !Slot.StartsWith(TEXT("HarborCity_M5_VS1_Test_"))) || Slot.Len() > 100) return;
    for (const TCHAR Letter : Slot) if (!FChar::IsAlnum(Letter) && Letter != TEXT('_')) return;
    Stop(Slot.StartsWith(TEXT("HarborCity_M5_VS1_Test_")) ? TEXT("m5_gameplay_sequence_finished") : TEXT("r2_gameplay_sequence_finished"));
#endif
}
void AHCM3Recording::FinishR1GameplayCapture()
{
#if !UE_BUILD_SHIPPING
    FString Mode, Slot;
    if (!FParse::Value(FCommandLine::Get(), TEXT("M4Test="), Mode) || Mode != TEXT("r1_joint") ||
        !FParse::Value(FCommandLine::Get(), TEXT("HCM1SaveSlot="), Slot) ||
        !Slot.StartsWith(TEXT("HarborCity_M2_V1_Test_M4_R1_")) || Slot.Len() > 100) return;
    for (const TCHAR Letter : Slot) if (!FChar::IsAlnum(Letter) && Letter != TEXT('_')) return;
    Stop(TEXT("r1_gameplay_sequence_finished"));
#endif
}

void AHCM3Recording::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FRecordingState& S = *Recording;
    if (S.bStopped || S.Seconds <= 0) return;
    if (S.bVS2)
    {
        BindVS2StopObservers();
        const AHCM1PlayerController* VS2Controller = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
        if (VS2Controller && VS2Controller->HasActorBegunPlay())
        {
            S.ActualSaveSlot = VS2Controller->GetSaveSlotName();
            if (S.ActualSaveSlot != S.RequestedSaveSlot) { Stop(TEXT("vs2_actual_save_slot_mismatch")); return; }
            if (VS2Controller->IsPauseMenuOpen() || UGameplayStatics::IsGamePaused(this))
            { Stop(TEXT("pause_observed")); return; }
            if (VS2Controller->IsGameplayFocused()) S.bVS2HadFocus = true;
            else if (S.bVS2HadFocus) { Stop(TEXT("gameplay_focus_lost")); return; }
        }
    }
    if (GetWorld()->GetTimeSeconds() < S.Delay) return;
    AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    const double Now = FPlatformTime::Seconds();
    if (!S.Grabber)
    {
        if (!PC || !PC->GetControlledCharacter() || !PC->IsGameplayFocused() || PC->IsPauseMenuOpen()) return;
        if (S.bVS2 && (!PC->HasActorBegunPlay() || S.ActualSaveSlot != S.RequestedSaveSlot
            || !S.VS2InputHandle.IsValid() || !S.VS2ActivationHandle.IsValid())) return;
        UGameViewportClient* Client = GetWorld()->GetGameViewport();
        TSharedPtr<SViewport> Widget = Client ? Client->GetGameViewportWidget() : nullptr;
        TSharedPtr<ISlateViewport> Interface = Widget ? Widget->GetViewportInterface().Pin() : nullptr;
        if (!Interface.IsValid()) { Stop(TEXT("viewport_unavailable")); return; }
        if (!S.bVS2) S.Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / (S.bM4 ? TEXT("M4Recording") : TEXT("M3Recording")) /
            (FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
        // M5 raw frames use its authorized report volume (D), avoiding old E packages/captures.
        FString M5Slot,M5Mode;
        if(!S.bVS2 && FParse::Value(FCommandLine::Get(),TEXT("M5Test="),M5Mode) && M5Mode.StartsWith(TEXT("m5_")) &&
           FParse::Value(FCommandLine::Get(),TEXT("HCM1SaveSlot="),M5Slot) && M5Slot.StartsWith(TEXT("HarborCity_M5_VS1_Test_")))
            S.Directory=FPaths::ConvertRelativePathToFull(FString(S.bVS2Part3?TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/part3/recordings"):TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS1/recordings"))/
                (FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
        IFileManager::Get().MakeDirectory(*S.Directory, true);
        const IConsoleVariable* Latency = IConsoleManager::Get().FindConsoleVariable(TEXT("framegrabber.framelatency"));
        S.FrameLatency = Latency ? Latency->GetInt() : -1;
        // Delayed ring surfaces need later frames to flush. This bounded recorder
        // only supports the engine's verified default immediate-readback path.
        if (S.FrameLatency != 0) { Stop(TEXT("unsupported_framegrabber_latency")); return; }
        const TSharedRef<FSceneViewport> SceneViewport = StaticCastSharedPtr<FSceneViewport>(Interface).ToSharedRef();
        S.SceneViewportSize = SceneViewport->GetSizeXY();
        S.bCaptureGeometryValid = GetM3CaptureGeometry(Widget.ToSharedRef(), S.CaptureRect, S.WindowSize);
        S.RequestedSize = S.CaptureRect.Size();
        // Capture the complete native rectangle without cropping, padding,
        // changing DPI, or changing the game's window/render resolution.
        S.bCaptureGeometryValid = S.bCaptureGeometryValid && S.RequestedSize.X <= 4096 && S.RequestedSize.Y <= 2160
            && S.RequestedSize.X * 9 == S.RequestedSize.Y * 16
            && S.RequestedSize.X % 2 == 0 && S.RequestedSize.Y % 2 == 0;
        if (!S.bCaptureGeometryValid) { Stop(TEXT("unsupported_viewport_geometry")); return; }
        S.ViewportWidget = Widget;
        S.Grabber = MakeUnique<FFrameGrabber>(SceneViewport, S.RequestedSize);
        S.Grabber->StartCapturingFrames();
        S.Started = S.NextCapture = Now;
        if (S.bR2 || S.bVS2)
        {
            // UE 5.8 can auto-disable a silent master submix before its recording
            // buffer is filled. Render the actual silent mix during this private
            // capture; never synthesize or substitute audio after recording.
            if (S.bVS2)
            {
                IConsoleVariable* NeverDisable = IConsoleManager::Get().FindConsoleVariable(TEXT("au.NeverDisableSubmixes"));
                if (!NeverDisable) { Stop(TEXT("vs2_native_submix_capture_control_missing")); return; }
                S.VS2PreviousNeverDisableSubmixes = NeverDisable->GetInt();
                NeverDisable->Set(1, ECVF_SetByCode);
                S.bVS2SubmixOverrideApplied = NeverDisable->GetInt() == 1;
                if (!S.bVS2SubmixOverrideApplied) { Stop(TEXT("vs2_native_submix_capture_control_rejected")); return; }
            }
            S.AudioStartedWall = FPlatformTime::Seconds();
            UAudioMixerBlueprintLibrary::StartRecordingOutput(this, S.Seconds + 5.f);
            S.bAudioStarted = true;
        }
        S.FirstCaptureWorldSeconds = GetWorld()->GetTimeSeconds();
        S.InitialPlayerLocation = PC->GetControlledCharacter()->GetActorLocation();
        S.InitialPlayerVelocity = PC->GetControlledCharacter()->GetVelocity();
        UE_LOG(LogTemp, Display, TEXT("M3_RECORDING_STARTED %s rect=(%d,%d)-(%d,%d) native=%dx%d scene=%dx%d window=%dx%d"),
            *S.Directory, S.CaptureRect.Min.X, S.CaptureRect.Min.Y, S.CaptureRect.Max.X, S.CaptureRect.Max.Y,
            S.RequestedSize.X, S.RequestedSize.Y, S.SceneViewportSize.X, S.SceneViewportSize.Y, S.WindowSize.X, S.WindowSize.Y);
    }
    if (!PC || !PC->IsGameplayFocused() || PC->IsPauseMenuOpen()) { Stop(TEXT("pause_or_focus_lost")); return; }
    FIntRect CurrentRect(0, 0, 0, 0);
    FIntPoint CurrentWindowSize = FIntPoint::ZeroValue;
    const TSharedPtr<SViewport> CurrentWidget = S.ViewportWidget.Pin();
    if (!CurrentWidget.IsValid() || !GetM3CaptureGeometry(CurrentWidget.ToSharedRef(), CurrentRect, CurrentWindowSize)
        || CurrentRect != S.CaptureRect || CurrentWindowSize != S.WindowSize)
    {
        S.bCaptureGeometryValid = false;
        Stop(TEXT("viewport_geometry_changed")); return;
    }
    if (S.WriteTask.IsValid() && S.WriteTask.IsReady())
    { if (!S.WriteTask.Get()) ++S.WriteFailures; S.WriteTask = TFuture<bool>(); }
    TArray<FCapturedFrameData> Captured = S.Grabber->GetCapturedFrames();
    for (FCapturedFrameData& Frame : Captured)
    {
        if (Frame.BufferSize != S.RequestedSize || Frame.ColorBuffer.Num() != S.RequestedSize.X * S.RequestedSize.Y)
        {
            S.bCaptureGeometryValid = false;
            Stop(TEXT("captured_pixel_dimensions_mismatch")); return;
        }
        if (S.WriteTask.IsValid()) { ++S.Dropped; continue; }
        S.OutputSize = Frame.BufferSize;
        const double Timestamp = Frame.GetPayload<FM3FrameTimestamp>()->Time;
        const FString Name = FString::Printf(TEXT("frame_%06d.%s"), S.Frames.Num(),S.bVS2Part3?TEXT("jpg"):TEXT("ppm"));
        const FString File = S.Directory / Name;
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("file"), Name); Row->SetNumberField(TEXT("seconds"), Timestamp);
        S.Frames.Add(MakeShared<FJsonValueObject>(Row));
        const FIntPoint Size = Frame.BufferSize;
        const bool PNG=S.bVS2Part3;
        S.WriteTask = Async(EAsyncExecution::ThreadPool, [Pixels = MoveTemp(Frame.ColorBuffer), Size, File, PNG]() mutable
        {
            if(PNG)
            {
                for(FColor& Pixel:Pixels)Pixel.A=255;
                TArray64<uint8> Bytes;
                FImageUtils::CompressImage(Bytes,TEXT("jpg"),FImageView(Pixels.GetData(),Size.X,Size.Y),95);
                return !Bytes.IsEmpty()&&FFileHelper::SaveArrayToFile(Bytes,*File);
            }
            const FString Header = FString::Printf(TEXT("P6\n%d %d\n255\n"), Size.X, Size.Y);
            FTCHARToUTF8 UTF8(*Header);
            TArray<uint8> Bytes;
            Bytes.Reserve(UTF8.Length() + Pixels.Num() * 3);
            Bytes.Append(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
            for (const FColor& Pixel : Pixels) { Bytes.Add(Pixel.R); Bytes.Add(Pixel.G); Bytes.Add(Pixel.B); }
            return FFileHelper::SaveArrayToFile(Bytes, *File);
        });
    }
    if (Now - S.Started >= S.Seconds) { Stop(TEXT("duration_reached")); return; }
    if (Now >= S.NextCapture)
    {
        S.Grabber->CaptureThisFrame(MakeShared<FM3FrameTimestamp, ESPMode::ThreadSafe>(Now - S.Started));
        ++S.Requested;
        do { S.NextCapture += 1.0 / 30.0; } while (S.NextCapture <= Now);
        // Skip overdue deadlines; never fixed-step, duplicate poses or slow gameplay for recording.
    }
}

void AHCM3Recording::Stop(const FString& Reason)
{
    FRecordingState& S = *Recording;
    if (S.bStopped) return;
    S.bStopped = true;
    if (S.bAudioStarted)
    {
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile,
            TEXT("game_audio"), S.Directory);
        S.bAudioExportRequested = true;
        S.bAudioStarted = false;
    }
    if (S.bVS2SubmixOverrideApplied)
    {
        if (IConsoleVariable* NeverDisable = IConsoleManager::Get().FindConsoleVariable(TEXT("au.NeverDisableSubmixes")))
        {
            NeverDisable->Set(S.VS2PreviousNeverDisableSubmixes, ECVF_SetByCode);
            S.bVS2SubmixOverrideRestored = NeverDisable->GetInt() == S.VS2PreviousNeverDisableSubmixes;
        }
    }
    if (S.Grabber)
    {
        S.Grabber->StopCapturingFrames();
        S.Grabber->Shutdown();
        S.Dropped += S.Grabber->GetCapturedFrames().Num();
        S.Grabber.Reset();
    }
    if (S.WriteTask.IsValid()) { if (!S.WriteTask.Get()) ++S.WriteFailures; }
    if (S.Directory.IsEmpty()) return;
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    FString R1Mode;
    const bool bR1 = FParse::Value(FCommandLine::Get(), TEXT("M4Test="), R1Mode) && R1Mode.StartsWith(TEXT("r1_"));
    FString M5Mode,M5Slot;
    const bool bM5=FParse::Value(FCommandLine::Get(),TEXT("M5Test="),M5Mode) && M5Mode.StartsWith(TEXT("m5_"))
        && FParse::Value(FCommandLine::Get(),TEXT("HCM1SaveSlot="),M5Slot) && M5Slot.StartsWith(TEXT("HarborCity_M5_VS1_Test_"));
    Report->SetStringField(TEXT("milestone"), (S.bVS2||S.bVS2Part3) ? TEXT("M5_VS2") : bM5 ? TEXT("M5_VS1") : S.bR2 ? TEXT("M4_R2") : bR1 ? TEXT("M4_R1") : S.bM4 ? TEXT("M4_V1") : TEXT("M3_V1"));
    Report->SetStringField(TEXT("audio_scope"), (S.bR2 || S.bVS2) ? TEXT("Native master game submix only; WAV export requested, file duration/level verification pending") : TEXT("NOT_RECORDED"));
    Report->SetStringField(TEXT("audio_file"), S.bAudioExportRequested ? TEXT("game_audio.wav") : TEXT(""));
    Report->SetNumberField(TEXT("audio_start_offset_seconds"), S.AudioStartedWall > 0 ? S.AudioStartedWall-S.Started : 0);
    Report->SetNumberField(TEXT("configured_delay_seconds"), S.Delay);
    Report->SetNumberField(TEXT("requested_duration_seconds"), S.Seconds);
    Report->SetNumberField(TEXT("first_capture_world_seconds"), S.FirstCaptureWorldSeconds);
    Report->SetStringField(TEXT("initial_player_location"), S.InitialPlayerLocation.ToString());
    Report->SetStringField(TEXT("initial_player_velocity"), S.InitialPlayerVelocity.ToString());
    Report->SetStringField(TEXT("scope"), TEXT("Live game viewport backbuffer; no camera control or OS input. Capture overhead excluded from performance pair."));
    Report->SetStringField(TEXT("stop_reason"), Reason);
    Report->SetStringField(TEXT("status"), S.bVS2EscapeStopped ? TEXT("USER_ABORTED") : S.Frames.Num() > 1 && S.WriteFailures == 0 && S.bCaptureGeometryValid ? TEXT("CAPTURED_PENDING_REVIEW") : TEXT("FAIL"));
    if (S.bVS2)
    {
        Report->SetStringField(TEXT("map"), GetWorld()->GetOutermost()->GetName());
        Report->SetStringField(TEXT("requested_save_slot"), S.RequestedSaveSlot);
        Report->SetStringField(TEXT("actual_save_slot"), S.ActualSaveSlot);
        Report->SetBoolField(TEXT("user_stop_latched"), S.bVS2EscapeStopped);
        Report->SetNumberField(TEXT("stop_frame"), double(S.VS2StopFrame));
        Report->SetBoolField(TEXT("audio_export_requested"), S.bAudioExportRequested);
        Report->SetNumberField(TEXT("audio_never_disable_submixes_before"), S.VS2PreviousNeverDisableSubmixes);
        Report->SetBoolField(TEXT("audio_silent_submix_rendering_during_capture"), S.bVS2SubmixOverrideApplied);
        Report->SetBoolField(TEXT("audio_submix_override_restored"), S.bVS2SubmixOverrideRestored);
        Report->SetBoolField(TEXT("automatic_restart"), false);
        Report->SetStringField(TEXT("input_scope"), TEXT("Passive live viewport capture; recorder never injects input or changes the camera. Input provenance is established by the separate gameplay test."));
    }
    Report->SetNumberField(TEXT("wall_seconds"), S.Started > 0 ? FPlatformTime::Seconds() - S.Started : 0);
    Report->SetNumberField(TEXT("requested"), S.Requested); Report->SetNumberField(TEXT("dropped"), S.Dropped);
    Report->SetNumberField(TEXT("framegrabber_latency"), S.FrameLatency);
    Report->SetStringField(TEXT("timestamp_scope"), TEXT("Game-thread CaptureThisFrame request wall times; not precise presentation timestamps. Default latency zero uses synchronous GPU readback."));
    Report->SetNumberField(TEXT("write_failures"), S.WriteFailures); Report->SetNumberField(TEXT("requested_fps"), 30);
    Report->SetNumberField(TEXT("width"), S.OutputSize.X); Report->SetNumberField(TEXT("height"), S.OutputSize.Y);
    Report->SetObjectField(TEXT("requested_size"), M3SizeJSON(S.RequestedSize));
    Report->SetObjectField(TEXT("output_size"), M3SizeJSON(S.OutputSize));
    Report->SetObjectField(TEXT("scene_viewport_size"), M3SizeJSON(S.SceneViewportSize));
    Report->SetObjectField(TEXT("window_size"), M3SizeJSON(S.WindowSize));
    TSharedRef<FJsonObject> Rect = MakeShared<FJsonObject>();
    Rect->SetNumberField(TEXT("min_x"), S.CaptureRect.Min.X); Rect->SetNumberField(TEXT("min_y"), S.CaptureRect.Min.Y);
    Rect->SetNumberField(TEXT("max_x"), S.CaptureRect.Max.X); Rect->SetNumberField(TEXT("max_y"), S.CaptureRect.Max.Y);
    Report->SetObjectField(TEXT("capture_rect"), Rect);
    Report->SetBoolField(TEXT("capture_geometry_valid"), S.bCaptureGeometryValid);
    Report->SetStringField(TEXT("pixel_scope"), TEXT("Complete native game viewport rectangle using the same Slate path as FFrameGrabber; no pixel resizing, cropping or padding. Visual review still required."));
    Report->SetArrayField(TEXT("frames"), S.Frames);
    FString Json; auto Writer = TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Report, Writer);
    FFileHelper::SaveStringToFile(Json, *(S.Directory / TEXT("capture.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("M3_RECORDING_FINISHED %s"), *S.Directory);
}
void AHCM3Recording::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Recording)
    {
        if (Recording->VS2Viewport.IsValid() && Recording->VS2InputHandle.IsValid())
            Recording->VS2Viewport->OnInputKey().Remove(Recording->VS2InputHandle);
        if (FSlateApplication::IsInitialized() && Recording->VS2ActivationHandle.IsValid())
            FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(Recording->VS2ActivationHandle);
    }
    Stop(TEXT("world_ended")); Super::EndPlay(Reason);
}
