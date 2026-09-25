#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2SoleDiagnostics.h"
#include "HCM5VS2HeroExerciseDirector.generated.h"

class AHCM1Character;
class AHCM1PlayerController;
class AHCM3Recording;
class ACameraActor;
class ADirectionalLight;
class ATargetPoint;
class UAnimInstance;
class UAnimSequence;
class UBlendSpace;
class USkeletalMesh;
class UMaterialInterface;
class UGameViewportClient;
class UHCM5VS2ExpressionComponent;
class FJsonObject;
struct FInputKeyEventArgs;

USTRUCT(BlueprintType)
struct FHCM5VS2ExercisePhase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    /** Warmup/Rest/Walk/TurnWalk/RunHome/Jump/Turn/Emotion/Speech/LookLeft/LookRight. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Action;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 2.f;
    /** Negative disables capture; Rest_Blink waits for a real closed automatic blink. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CaptureAt = -1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFaceCamera = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Emotion = TEXT("Neutral");
};

USTRUCT(BlueprintType)
struct FHCM5VS2ExerciseBone
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Bone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Anchor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Family;
};

/** Opt-in isolated native calls. No OS input, forced animation pose, or gameplay tuning. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2HeroExerciseDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2HeroExerciseDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AHCM1Character> ExpectedCharacterClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UAnimInstance> ExpectedAnimationClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> ExpectedMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<ADirectionalLight> DirectionalLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UMaterialInterface>> ExerciseMaterials;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHCM5VS2ExercisePhase> Phases;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHCM5VS2ExerciseBone> ObservedBones;
    /** Only the new private R2 gait fixture supplies these; old modes ignore them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UBlendSpace> ExpectedGaitBlendSpace;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString GaitSourceReport;
    /** Ignored without the private M5VS2HeroGaitSoleCapture flag. Native probe selection only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHCM5VS2SolePoint> SolePoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SoleSelectionEvidence;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Initialize(FString& Failure);
    bool ValidateBinding(FString& Failure) const;
    bool CheckShaders(FString& Failure);
    bool BeginPhase(int32 Index, FString& Failure);
    void UpdatePhase(float DeltaSeconds);
    TSharedPtr<FJsonObject> Observe() const;
    TSharedPtr<FJsonObject> ObserveProportions(const FString& MeasurementLabel) const;
    bool ObserveGait(float DeltaSeconds, FString& Failure);
    bool ObserveSole(FString& Failure);
    bool PrepareSoleRecording(FString& Failure);
    void SoleActivation(bool bActive);
    void StopSoleCapture(const FString& Reason);
    void RequestCapture();
    void ScreenshotProcessed();
    void ViewportInput(const FInputKeyEventArgs& Event);
    void Finish(const FString& Status, const FString& Detail);
    void WriteReport(const FString& Status, const FString& Detail);
    void Restore();
    void RemoveDelegates();
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> Controller;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2ExpressionComponent> Expression;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    UPROPERTY(Transient) TObjectPtr<ATargetPoint> LookTarget;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> Originals;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> RuntimeMaterials;
    UPROPERTY(Transient) TObjectPtr<UBlendSpace> ActiveGaitSpace;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> GaitRunClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> GaitSprintClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> GaitWalkClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> GaitIdleClip;
    UPROPERTY(Transient) TObjectPtr<AHCM3Recording> SoleRecorder;
    TWeakObjectPtr<AActor> OriginalViewTarget;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle, ScreenshotHandle;
    FDelegateHandle SoleActivationHandle;
    FString RunDirectory, PendingPNG;
    TArray<TSharedPtr<FJsonObject>> Samples, Captures;
    TArray<TSharedPtr<FJsonObject>> GaitSamples;
    TArray<TSharedPtr<FJsonObject>> SoleSamples;
    TSharedPtr<FJsonObject> SoleRecordingReport;
    FString SoleFinalStatus, SoleFinalDetail;
    double SoleNextSampleWorld = 0, SoleRecorderSpawnWall = -1, SoleFirstFrameWorld = -1;
    bool bSoleHadFocus = false, bSoleAudioFinished = false;
    // R2 only: one settled Warmup and one first-capture readback; no geometry writes.
    TArray<TSharedPtr<FJsonObject>> ProportionSamples;
    TSharedPtr<FJsonObject> PendingCapture, ShaderState;
    TArray<FTransform> FirstBoneInAnchor;
    TArray<double> MaxBoneAngleDegrees, MaxBoneDisplacement;
    FVector Origin = FVector::ZeroVector;
    double StartedAt = 0, FinishedAt = 0, NextSampleAt = 0, NextShaderCheck = 0, ScreenshotRequestedAt = 0;
    double PhaseElapsed = 0, SimulationElapsed = 0;
    uint64 ShaderReadyFrame = 0, RequestFrame = 0, StopFrame = 0;
    int32 PhaseIndex = INDEX_NONE, BindingChecks = 0, BlinkClosedSamples = 0, AirborneSamples = 0;
    float MaximumSpeed = 0, MaximumTalkWeight = 0, MaximumLookAlpha = 0;
    bool bEnabled = false, bInitialized = false, bStopped = false, bDone = false, bAutoQuit = false;
    bool bCaptureRequested = false, bScreenshotProcessed = false, bOriginalHUD = true, bRestored = false;
    bool bJumpReleased = false;
    // Opt-in review evidence only; never used to drive animation or movement.
    FVector PreviousGaitToes[2] = {FVector::ZeroVector, FVector::ZeroVector};
    double PreviousGaitTime = -1, PreviousGaitYaw = 0;
    float PreviousGaitContact[2] = {0, 0};
    bool bPreviousGaitGroundHit[2] = {false, false};
    bool bPreviousGaitGrounded = false;
    int32 PreviousGaitPhase = INDEX_NONE, PreviousGaitSteady = INDEX_NONE;
    uint64 PreviousGaitFrame = 0;
    int32 GaitSteadyFrames[2] = {0, 0};
    int32 GaitContactIntervals[2][2] = {{0, 0}, {0, 0}};
    bool bGaitIdleObserved = false, bGaitWalkBlendObserved = false, bGaitTurnObserved = false;
};
