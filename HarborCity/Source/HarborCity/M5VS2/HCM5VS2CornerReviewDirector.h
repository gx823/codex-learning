#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2CornerReviewDirector.generated.h"

class ACameraActor;
class ACharacter;
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;
class APlayerController;
class UGameViewportClient;
class FJsonObject;
struct FInputKeyEventArgs;

/** Six opt-in, source-bound staged-lighting viewport captures. Never an input test. */
UCLASS()
class HARBORCITY_API AHCM5VS2CornerReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2CornerReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool LoadPlan(FString& Error);
    bool Start(FString& Error, bool& bRetryable);
    void TryStart();
    void ObserveViewport();
    bool BeginShot(int32 Index, FString& Error);
    bool BindPortraitSubjects(FString& Error);
    bool LockPortraitCamera(int32 SubjectIndex, FString& Error);
    bool PortraitSubjectsValid(FString& Error) const;
    TSharedPtr<FJsonObject> PortraitSubjectSnapshot(int32 SubjectIndex) const;
    FVector ExpectedCameraLocation() const;
    FRotator ExpectedCameraRotation() const;
    bool CameraMatchesRequest() const;
    bool Ready();
    TSharedPtr<FJsonObject> Snapshot() const;
    void Capture();
    void Processed();
    void Drawn();
    void Input(const FInputKeyEventArgs& Event);
    void Restore();
    void Finish(const FString& Status, const FString& Detail);
    bool Write(const FString& Status, const FString& Detail);
    void RemoveDelegates();

    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    UPROPERTY(Transient) TObjectPtr<ADirectionalLight> MainLight;
    UPROPERTY(Transient) TObjectPtr<ASkyLight> SkyLight;
    UPROPERTY(Transient) TObjectPtr<APostProcessVolume> PostProcess;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> OriginalView;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle, ScreenshotHandle, DrawHandle;
    TSharedPtr<FJsonObject> Plan, OriginalState, Pending, FirstStartupReadback, LastStartupReadback;
    TArray<TSharedPtr<FJsonObject>> Shots, Results;
    TWeakObjectPtr<ACharacter> PortraitSubjects[3];
    TSharedPtr<FJsonObject> PortraitEyeSamples[3];
    FVector PortraitCameraLocations[3];
    FRotator PortraitCameraRotations[3];
    double PortraitCameraLockedAt = 0;
    uint64 PortraitCameraLockDraw = 0;
    bool bPortraits = false;
    FString Directory, PlanFile, PlanSHA1, PendingPNG, ExitReason;
    FRotator SavedSunRotation;
    FLinearColor SavedSunColor;
    FPostProcessSettings SavedPostProcess;
    float SavedSunIntensity = 0, SavedSkyIntensity = 0;
    int32 ShotIndex = INDEX_NONE, ReadyDraws = 0, WantingResources = 0, ShaderJobs = 0, StartupAttempts = 0;
    uint64 DrawCount = 0, ShotDrawStart = 0, RequestFrame = 0, ProcessedFrame = 0, StopFrame = 0;
    double Started = 0, ShotStarted = 0, Requested = 0, Finished = 0;
    bool bStarting = false, bActive = false, bStopped = false, bPending = false, bProcessed = false;
    bool bReady = false, bStateSaved = false, bSavedHUD = true, bHUDSaved = false;
    bool bAutoQuit = false, bExitPending = false;
};
