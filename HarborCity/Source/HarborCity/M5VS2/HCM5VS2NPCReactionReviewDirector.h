#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M3/HCM3State.h"
#include "HCM5VS2NPCReactionReviewDirector.generated.h"

class AHCM5VS2NPC;
class AHCM3Experience;
class AHCM1Vehicle;
class ACameraActor;
class APlayerController;
class UGameViewportClient;
class FJsonObject;
struct FInputKeyEventArgs;

/** Isolated opt-in gameplay entry-point test. Does not manufacture vehicle contacts or OS input. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2NPCReactionReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2NPCReactionReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ProbeSite = TEXT("Open");
    /** Empty preserves legacy Q/R. New isolated fixtures select one exact two-person group. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ReviewPair;
    /** New private review fixtures only. Does not affect NPCs or gameplay cameras. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOcclusionAwareReviewCamera = false;
    /** Only new private slope fixtures opt in; terrain queries are not contact manifolds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequireRampSupport = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<AActor>> RampSupportActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<AHCM5VS2NPC>> Specimens;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AHCM3Experience> Experience;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AHCM1Vehicle> Vehicle;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Start(FString& Error);
    void ListenForStop();
    bool BeginSpecimen();
    FVector ImpactDirection() const;
    TArray<FTransform> BonePose() const;
    bool ObserveHitPose(bool bLethal);
    bool Check(const FString& Name, bool bPass, const FString& Detail);
    void SetPhase(int32 Next);
    void AimCamera();
    void Capture(const FString& Label, int32 NextPhase);
    void Processed();
    void Input(const FInputKeyEventArgs& Event);
    void Finish(const FString& Status, const FString& Detail);
    bool Write(const FString& Status, const FString& Detail);
    TSharedPtr<FJsonObject> Snapshot(AHCM5VS2NPC* NPC, bool bDetailed) const;
    void SampleGetUpSupport(AHCM5VS2NPC* NPC, const TSharedPtr<FJsonObject>& State);
    bool CheckRampSupport(AHCM5VS2NPC* NPC, const FString& Label);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> OriginalView;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    TArray<FHCM3NPCState> InitialStates;
    TArray<FTransform> InitialMeshRelative, HitStartPose;
    FString FallDirection;
    float MaximumHitPoseDeltaDegrees = 0;
    bool bObservedAdvancingPose = false;
    TArray<TSharedPtr<FJsonObject>> Checks, Events, Samples, Captures, GetUpSupportSamples;
    TArray<TSharedPtr<FJsonObject>> RampSupportSamples;
    TSharedPtr<FJsonObject> Pending;
    TSharedPtr<FJsonObject> CameraVisibilityDecision;
    int32 VisibleCameraCandidate = 0, VisibleCameraSpecimen = INDEX_NONE;
    FDelegateHandle InputHandle, ScreenshotHandle;
    FString Directory, PendingPNG;
    double StartedWall = 0, LastTickWall = 0, UnpausedWall = 0, StartedGame = 0;
    double PhaseStarted = 0, ImpactStarted = 0, StableSince = -1, NextSample = 0, NextWrite = 0;
    double RequestUnpausedSeconds = 0, FinishedWall = 0;
    double RecoveryBefore = 0, EmergencyBefore = 0;
    float HealthAfterImpact = 0;
    int32 Phase = 0, SpecimenIndex = 0, AfterCapturePhase = 0, PauseKeyPresses = 0, FocusPauseCount = 0;
    uint64 CaptureFrame = 0, StopFrame = 0;
    bool bActive = false, bStopped = false, bAutoQuit = false, bExitPending = false, bReady = false;
    bool bFinalEvidenceWriteFailed = false;
    bool bPending = false, bProcessed = false, bDownCaptured = false;
};
