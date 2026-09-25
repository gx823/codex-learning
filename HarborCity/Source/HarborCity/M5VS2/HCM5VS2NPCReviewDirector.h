#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2NPCReviewDirector.generated.h"

class AHCM5VS2NPC;
class ACameraActor;
class APlayerController;
class ADirectionalLight;
class UGameViewportClient;
class FJsonObject;
struct FInputKeyEventArgs;

/** Opt-in native review and component exercises, explicitly not an OS-input/gameplay acceptance test. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2NPCReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2NPCReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<AHCM5VS2NPC>> Specimens;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<ADirectionalLight> MainLight;
    /** New opt-in 8-source identity review; the original 2-person/20-shot mode is unchanged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCastPortraits = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> SpecimenIds;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Start(FString& Error);
    bool BeginPhase(int32 Index, FString& Error);
    bool BeginPhysics(int32 Index, FString& Error);
    void RestoreExercise();
    void Capture();
    void Processed();
    void Input(const FInputKeyEventArgs& Event);
    void Finish(const FString& Status, const FString& Detail);
    void Write(const FString& Status, const FString& Detail);
    TSharedPtr<FJsonObject> Snapshot(AHCM5VS2NPC* NPC) const;
    bool ReadCastExpression(FString& Error);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> OriginalView;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle, ScreenshotHandle;
    TArray<FVector> EyeSamples, MoveStarts;
    TArray<bool> OriginalActorTicks, OriginalBlinks;
    TArray<TSharedPtr<FJsonObject>> Rows;
    TArray<TSharedPtr<FJsonObject>> CastExpressionRows;
    TArray<float> CastPeakBlink;
    int32 CastProbeIndex = 0;
    bool bCastProbesDone = false;
    TSharedPtr<FJsonObject> Pending;
    TSharedPtr<FJsonObject> Exercises;
    FString Directory, PendingPNG, PhaseLabel;
    FTransform PhysicsMeshRelative;
    FCollisionResponseContainer PhysicsResponses;
    ECollisionEnabled::Type PhysicsMeshCollision = ECollisionEnabled::NoCollision, PhysicsCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
    ECollisionChannel PhysicsObjectType = ECC_Pawn;
    int32 PhysicsIndex = INDEX_NONE, Phase = INDEX_NONE, Frames = 0;
    int32 PhysicsCaptureIndex = 0;
    int32 PhysicsViewCaptureIndex = INDEX_NONE;
    uint64 PhysicsViewChangedFrame = 0;
    double PhysicsSimulationStart = 0;
    bool bPhysicsOnly = false;
    bool bPhysicsSettledOnly = false;
    TArray<double> PhysicsFrameSeconds;
    bool bPhysicsDisablePostProcess = false, bPhysicsOriginalPostProcessDisabled = false;
    float PeakSpeed = 0;
    uint64 SampleFrame = 0, CaptureFrame = 0, StopFrame = 0;
    double Started = 0, PhaseSeconds = 0, Requested = 0, Finished = 0;
    bool bActive = false, bWarm = false, bPending = false, bProcessed = false, bStopped = false;
    bool bAutoQuit = false, bExitPending = false, bSavedHUD = true, bMoving = false;
};
