#include "HCM5MusicComponent.h"
#include "M1/HCM1Character.h"
#include "M5VS3/HCM5VS3Abilities.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "M1/HCM1PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UHCM5MusicComponent::UHCM5MusicComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEvenWhenPaused = true;
    ExplorationTrack = FSoftObjectPath(TEXT("/Game/HarborCity/M5/Audio/SW_M5_NeonDelivery.SW_M5_NeonDelivery"));
    CombatTrack = FSoftObjectPath(TEXT("/Game/HarborCity/M5/Audio/SW_M5_HandshakeOverride.SW_M5_HandshakeOverride"));
    InteriorTrack = FSoftObjectPath(TEXT("/Game/HarborCity/M5/Audio/SW_M5_AfterRainTerminal.SW_M5_AfterRainTerminal"));
}

void UHCM5MusicComponent::BeginPlay()
{
    Super::BeginPlay();
    bPublicCaptureMute=FParse::Param(FCommandLine::Get(),TEXT("M5PublicCapture"));
    Tracks.SetNum(4);
    for (int32 I = 0; I < 2; ++I)
    {
        UAudioComponent* Audio = NewObject<UAudioComponent>(GetOwner(), *FString::Printf(TEXT("M5MusicChannel%d"), I));
        Audio->bAutoActivate = false;
        Audio->bAutoDestroy = false;
        Audio->bAllowSpatialization = false;
        // Pause is explicitly shared with application focus and the pause menu below.
        Audio->bIsUISound = true;
        Audio->SetVolumeMultiplier(0);
        Audio->RegisterComponent();
        Channels.Add(Audio);
    }
}

void UHCM5MusicComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopMusic();
    for (UAudioComponent* Channel : Channels) if (Channel) Channel->DestroyComponent();
    Channels.Reset();
    Tracks.Reset();
    Super::EndPlay(Reason);
}

void UHCM5MusicComponent::RequestMode(EHCM5MusicMode Mode)
{
    if (uint8(Mode) <= uint8(EHCM5MusicMode::Interior)) RequestedMode = Mode;
}

void UHCM5MusicComponent::SetDialogueDucking(bool bDialogue) { bDialogueDucking = bDialogue; }
void UHCM5MusicComponent::NotifyForegroundEffect() { EffectDuckRemaining = .45f; }

USoundWave* UHCM5MusicComponent::ResolveTrack(EHCM5MusicMode Mode)
{
    const int32 Index = int32(Mode);
    if (Index <= 0 || !Tracks.IsValidIndex(Index)) return nullptr;
    if (!Tracks[Index])
    {
        const TSoftObjectPtr<USoundWave>& Asset = Mode == EHCM5MusicMode::Exploration ? ExplorationTrack
            : Mode == EHCM5MusicMode::Combat ? CombatTrack : InteriorTrack;
        Tracks[Index] = Asset.LoadSynchronous();
        if (Tracks[Index])
        {
            Tracks[Index]->bLooping = true;
            Tracks[Index]->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
        }
        else UE_LOG(LogTemp, Warning, TEXT("M5_MUSIC_MISSING mode=%d asset=%s"), Index, *Asset.ToString());
    }
    return Tracks[Index];
}

float UHCM5MusicComponent::ModeGain(EHCM5MusicMode Mode)
{
    return Mode == EHCM5MusicMode::Combat ? 1.f : Mode == EHCM5MusicMode::Interior ? .65f : .8f;
}

bool UHCM5MusicComponent::BeginTransition(EHCM5MusicMode Mode)
{
    if (Channels.Num() != 2) return false;
    USoundWave* Wave = Mode == EHCM5MusicMode::Silence ? nullptr : ResolveTrack(Mode);
    if (Mode != EHCM5MusicMode::Silence && !Wave) return false;
    IncomingChannel = 1 - ActiveChannel;
    UAudioComponent* Incoming = Channels[IncomingChannel];
    Incoming->Stop();
    bChannelStarted[IncomingChannel] = false;
    ChannelModes[IncomingChannel] = Mode;
    if (Wave)
    {
        Incoming->SetSound(Wave);
        Incoming->SetVolumeMultiplier(0);
        Incoming->Play();
        Incoming->SetPaused(bMusicPaused);
        bChannelStarted[IncomingChannel] = true;
    }
    TransitionMode = Mode;
    TransitionElapsed = 0;
    bTransitioning = true;
    ++TransitionCount;
    UE_LOG(LogTemp, Display, TEXT("M5_MUSIC_TRANSITION from=%d to=%d channels=%d"), int32(CurrentMode), int32(Mode), GetPlayingChannelCount());
    return true;
}

void UHCM5MusicComponent::ApplyVolumes(float Alpha)
{
    if (Channels.Num() != 2) return;
    const float Base = FMath::Clamp(MasterVolume, 0.f, 1.f) * DuckGain;
    const float OutGain = bTransitioning ? FMath::Cos(Alpha * HALF_PI) : 1.f;
    const float InGain = bTransitioning ? FMath::Sin(Alpha * HALF_PI) : 0.f;
    Channels[ActiveChannel]->SetVolumeMultiplier(Base * ModeGain(ChannelModes[ActiveChannel]) * OutGain);
    Channels[IncomingChannel]->SetVolumeMultiplier(Base * ModeGain(ChannelModes[IncomingChannel]) * InGain);
}

void UHCM5MusicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    if (!GetWorld() || Channels.Num() != 2) return;
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    const bool bPause = UGameplayStatics::IsGamePaused(this) || !PC || !PC->IsGameplayFocused() || PC->IsPauseMenuOpen();
    if (bPause != bMusicPaused)
    {
        bMusicPaused = bPause;
        for (UAudioComponent* Channel : Channels) Channel->SetPaused(bPause);
    }
    if (bPause) return;
    EffectDuckRemaining = FMath::Max(0.f, EffectDuckRemaining - DeltaTime);
    if(PC&&PC->GetControlledCharacter()&&PC->GetControlledCharacter()->GetAbilities()->IsBusy())EffectDuckRemaining=FMath::Max(EffectDuckRemaining,.4f);
    const float TargetDuck = bDialogueDucking ? .50f : EffectDuckRemaining > 0 ? .65f : 1.f;
    DuckGain = FMath::FInterpTo(DuckGain, TargetDuck, DeltaTime, 7.f);
    if(bUsePrivatePlaylist){TickPlaylist(DeltaTime);return;}
    if (!bTransitioning && RequestedMode != CurrentMode && GetWorld()->GetTimeSeconds() >= RetryAfter)
        if (!BeginTransition(RequestedMode)) RetryAfter = GetWorld()->GetTimeSeconds() + 3.;
    if (bTransitioning)
    {
        TransitionElapsed += DeltaTime;
        const float Alpha = FMath::Clamp(TransitionElapsed / FMath::Max(.1f, CrossfadeSeconds), 0.f, 1.f);
        ApplyVolumes(Alpha);
        if (Alpha >= 1.f)
        {
            Channels[ActiveChannel]->Stop();
            bChannelStarted[ActiveChannel] = false;
            ActiveChannel = IncomingChannel;
            IncomingChannel = 1 - ActiveChannel;
            CurrentMode = TransitionMode;
            bTransitioning = false;
        }
    }
    else ApplyVolumes(1.f);
}

void UHCM5MusicComponent::StopMusic()
{
    for (UAudioComponent* Channel : Channels) if (Channel) Channel->Stop();
    bChannelStarted[0] = bChannelStarted[1] = false;
    bTransitioning = false;
    RequestedMode = CurrentMode = EHCM5MusicMode::Silence;
    TransitionElapsed = 0;
    EffectDuckRemaining = 0;
}

int32 UHCM5MusicComponent::GetPlayingChannelCount() const
{
    int32 Count = 0;
    for (UAudioComponent* Channel : Channels)
        if (Channel && Channel->GetPlayState() != EAudioComponentPlayState::Stopped) ++Count;
    return Count;
}

void UHCM5MusicComponent::TickPlaylist(float Dt)
{
    if(PrivatePlaylist.Num()!=2)return;
    const float Fade=2.5f;
    auto Start=[&](int32 Channel,int32 Index)
    {
        USoundWave* Wave=PrivatePlaylist[Index].LoadSynchronous();if(!Wave)return false;
        Wave->bLooping=false;Wave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;
        Channels[Channel]->Stop();Channels[Channel]->SetSound(Wave);Channels[Channel]->SetVolumeMultiplier(0);
        Channels[Channel]->Play();bChannelStarted[Channel]=true;PlaylistElapsed[Channel]=0;return true;
    };
    if(!bChannelStarted[ActiveChannel]){if(!Start(ActiveChannel,PlaylistIndex))return;}
    CurrentMode=RequestedMode;
    for(int32 I=0;I<2;++I)if(bChannelStarted[I])PlaylistElapsed[I]+=Dt;
    const auto* Current=Cast<USoundWave>(Channels[ActiveChannel]->Sound);
    if(!bTransitioning && Current && PlaylistElapsed[ActiveChannel]>=Current->GetDuration()-Fade)
    {
        IncomingChannel=1-ActiveChannel;
        if(Start(IncomingChannel,1-PlaylistIndex))
        {bTransitioning=true;TransitionElapsed=0;++TransitionCount;UE_LOG(LogTemp,Display,TEXT("VS3_PRIVATE_BGM_CROSSFADE index=%d next=%d"),PlaylistIndex,1-PlaylistIndex);}
    }
    const float Gain=bPublicCaptureMute?0.f:FMath::Clamp(MasterVolume,0.f,1.f)*DuckGain;
    if(bTransitioning)
    {
        TransitionElapsed+=Dt;const float A=FMath::Clamp(TransitionElapsed/Fade,0.f,1.f);
        Channels[ActiveChannel]->SetVolumeMultiplier(Gain*FMath::Cos(A*HALF_PI));
        Channels[IncomingChannel]->SetVolumeMultiplier(Gain*FMath::Sin(A*HALF_PI));
        if(A>=1){Channels[ActiveChannel]->Stop();bChannelStarted[ActiveChannel]=false;ActiveChannel=IncomingChannel;PlaylistIndex=1-PlaylistIndex;bTransitioning=false;}
    }
    else Channels[ActiveChannel]->SetVolumeMultiplier(Gain);
}

FString UHCM5MusicComponent::GetDiagnostics() const
{
    auto J = MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("requested_mode"), int32(RequestedMode));
    J->SetNumberField(TEXT("current_mode"), int32(CurrentMode));
    J->SetBoolField(TEXT("crossfading"), bTransitioning);
    J->SetBoolField(TEXT("paused"), bMusicPaused);
    J->SetNumberField(TEXT("owned_channels"), Channels.Num());
    J->SetNumberField(TEXT("playing_channels"), GetPlayingChannelCount());
    J->SetNumberField(TEXT("transitions"), TransitionCount);
    J->SetNumberField(TEXT("duck_gain"), DuckGain);
    J->SetNumberField(TEXT("master_volume"), MasterVolume);
    J->SetBoolField(TEXT("private_playlist"),bUsePrivatePlaylist);
    J->SetBoolField(TEXT("public_capture_bgm_muted"),bPublicCaptureMute);
    J->SetNumberField(TEXT("playlist_index"),PlaylistIndex);
    J->SetNumberField(TEXT("playlist_elapsed"),PlaylistElapsed[ActiveChannel]);
    J->SetStringField(TEXT("exploration_asset"), ExplorationTrack.ToString());
    J->SetStringField(TEXT("combat_asset"), CombatTrack.ToString());
    J->SetStringField(TEXT("interior_asset"), InteriorTrack.ToString());
    FString Out; const auto Writer = TJsonWriterFactory<>::Create(&Out); FJsonSerializer::Serialize(J, Writer); return Out;
}
