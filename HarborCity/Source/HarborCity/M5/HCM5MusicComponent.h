#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HCM5MusicComponent.generated.h"

class UAudioComponent;
class USoundWave;

UENUM(BlueprintType)
enum class EHCM5MusicMode : uint8 { Silence, Exploration, Combat, Interior };

/** Exactly two owned non-spatial players. No persistent audio survives map teardown. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5MusicComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5MusicComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;
    UFUNCTION(BlueprintCallable) void RequestMode(EHCM5MusicMode Mode);
    UFUNCTION(BlueprintCallable) void SetDialogueDucking(bool bDialogue);
    UFUNCTION(BlueprintCallable) void NotifyForegroundEffect();
    UFUNCTION(BlueprintCallable) void StopMusic();
    UFUNCTION(BlueprintPure) FString GetDiagnostics() const;
    UFUNCTION(BlueprintPure) EHCM5MusicMode GetRequestedMode() const { return RequestedMode; }
    UFUNCTION(BlueprintPure) bool IsMusicPaused() const { return bMusicPaused; }
    UFUNCTION(BlueprintPure) int32 GetPlayingChannelCount() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5|Music") TSoftObjectPtr<USoundWave> ExplorationTrack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5|Music") TSoftObjectPtr<USoundWave> CombatTrack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5|Music") TSoftObjectPtr<USoundWave> InteriorTrack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|PrivateMusic") bool bUsePrivatePlaylist=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|PrivateMusic") TArray<TSoftObjectPtr<USoundWave>> PrivatePlaylist;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5|Music", meta=(ClampMin="0", ClampMax="1")) float MasterVolume = .20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5|Music", meta=(ClampMin="0.1", ClampMax="8")) float CrossfadeSeconds = 1.4f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Channels;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundWave>> Tracks;
    EHCM5MusicMode RequestedMode = EHCM5MusicMode::Silence;
    EHCM5MusicMode CurrentMode = EHCM5MusicMode::Silence;
    EHCM5MusicMode TransitionMode = EHCM5MusicMode::Silence;
    EHCM5MusicMode ChannelModes[2] = {EHCM5MusicMode::Silence, EHCM5MusicMode::Silence};
    bool bChannelStarted[2] = {false, false};
    bool bTransitioning = false, bMusicPaused = false, bDialogueDucking = false;
    int32 ActiveChannel = 0, IncomingChannel = 1, TransitionCount = 0;
    float TransitionElapsed = 0, DuckGain = 1, EffectDuckRemaining = 0;
    double RetryAfter = 0;
    float PlaylistElapsed[2]={0,0};
    int32 PlaylistIndex=0;
    bool bPublicCaptureMute=false;
    void TickPlaylist(float DeltaTime);
    USoundWave* ResolveTrack(EHCM5MusicMode Mode);
    bool BeginTransition(EHCM5MusicMode Mode);
    void ApplyVolumes(float Alpha);
    static float ModeGain(EHCM5MusicMode Mode);
};
