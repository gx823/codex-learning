#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputActionValue.h"
#include "HCM1TestRunner.generated.h"

class AHCM1PlayerController;
class AHCM1Character;
class AHCM1Vehicle;
class AHCM1LightSwitch;
class FJsonObject;
class UGameViewportClient;
struct FInputKeyEventArgs;

/** Dormant in normal play. Opt-in Development input/physics regression recorder. */
UCLASS()
class HARBORCITY_API AHCM1TestRunner : public AActor
{
    GENERATED_BODY()
public:
    AHCM1TestRunner();
    virtual void Tick(float DeltaSeconds) override;
    /** Read-only diagnostics through public C++ accessors unavailable to Python reflection. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|Diagnostics")
    static FString InspectVehicleMaterialReferences();
    /** Development B-Key exercise through the real viewport event; never OS input. */
    UFUNCTION(BlueprintCallable, Category="HarborCity|Diagnostics")
    bool ExerciseM3EscapeStopThroughViewport();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FStep
    {
        FString Name;
        double Duration;
        TFunction<void()> Begin;
        TFunction<void()> End;
    };
    UPROPERTY() TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY() TObjectPtr<AHCM1Character> Character;
    UPROPERTY() TObjectPtr<AHCM1Vehicle> Car;
    UPROPERTY() TObjectPtr<AHCM1LightSwitch> Lamp;
    UPROPERTY() TArray<TObjectPtr<AActor>> Fixtures;
    TArray<FStep> Steps;
    TMap<FName, FInputActionValue> Held;
    TArray<TSharedPtr<FJsonObject>> Results;
    TArray<double> FrameTimes;
    FString Mode, RunDirectory;
    int32 StepIndex = -1;
    int32 CompletedCycles = 0;
    double StartedAt = 0, StepStartedAt = 0, LastFrameAt = 0;
    double StepElapsed = 0, SimulationElapsed = 0;
    bool bRunning = false, bFinished = false, bInitialLight = false;
    bool bM2Run = false, bM3Run = false, bM4Run = false;
    bool bM5Run = false;
    TFunction<void()> M5ReviewCameraCleanup;
    void AddM5Tests();
    void AddM5VS2Tests();
    void AddM5VS3Tests();
    void AddM5VS3SideQuests();
    bool bM3FinishPending = false, bM3SuppressAutoQuit = false, bM3EndingPlay = false;
    uint64 M3LastCaptureFrame = MAX_uint64;
    double M3FinishWaitStarted = 0;
    TArray<FString> M3CaptureFiles;
    bool bM3EscapeScope = false, bM3UserAbort = false, bM3UserAbortReported = false;
    bool bM3EscapeExercise = false, bM3AbortWasBKey = false, bM3EscapeExerciseReleasePending = false;
    uint64 M3AbortFrame = 0, M3InjectedActions = 0, M3InjectedActionsAtAbort = 0;
    double M3AbortWall = 0, M3EscapeExerciseAfter = 0;
    int32 M3HeldActionsAtAbort = 0;
    FDelegateHandle M3EscapeHandle;
    TWeakObjectPtr<UGameViewportClient> M3EscapeViewport;
    void BindM3EscapeStop();
    void OnM3ViewportInputKey(const FInputKeyEventArgs& Event);
    void CompleteM3UserAbort();
    bool bCycleEntered = false, bUnsafeReverse = false, bObservedNearStop = false;
    bool bTrackBraking = false;
    float ReverseConfirmedSeconds = 0.f;
    bool bPlaythroughNavigating = false, bPlaythroughArrived = false;
    bool bPlaythroughBraking = false, bPlaythroughBrakeLatched = false;
    bool bPlaythroughBrakeApplied = false, bPlaythroughUnsafeReverse = false;
    bool bPlaythroughReversing = false;
    float PlaythroughMinimumSignedSpeed = 0;
    FVector PlaythroughTarget = FVector::ZeroVector;
    FVector PlaythroughNavigationStart = FVector::ZeroVector;
    float PeakSpeed = 0, PeakHeight = 0, WalkDistance = 0;
    FVector SamplePosition;
    FRotator SampleRotation;
    FTransform CarHome, CharacterHome;
    struct FCameraTestState;
    TSharedPtr<FCameraTestState> CameraTest;
    struct FLookCalibrationState;
    TSharedPtr<FLookCalibrationState> LookCalibration;
    struct FM2RouteState;
    TSharedPtr<FM2RouteState> M2Route;
    struct FM2CameraState;
    TSharedPtr<FM2CameraState> M2Camera;
    struct FM3TestState;
    TSharedPtr<FM3TestState> M3Test;
    struct FM3PlaythroughState;
    TSharedPtr<FM3PlaythroughState> M3PlaythroughState;
    struct FM4TestState;
    TSharedPtr<FM4TestState> M4Test;
    struct FM4PlaythroughState;
    TSharedPtr<FM4PlaythroughState> M4Playthrough;
    struct FM4R1SteeringState;
    struct FM4R1ImpactState;
    struct FM4R1ReactionState;
    struct FM4R1ProfileState;
    struct FM4R1JointState;
    struct FM4R1QuestsState;
    TSharedPtr<FM4R1SteeringState> M4R1Steering;
    TSharedPtr<FM4R1ImpactState> M4R1Impact;
    TSharedPtr<FM4R1ReactionState> M4R1Reaction;
    TSharedPtr<FM4R1ProfileState> M4R1Profile;
    TSharedPtr<FM4R1JointState> M4R1Joint;
    TSharedPtr<FM4R1QuestsState> M4R1Quests;
    void BuildM4R1QuestsSequence();
    void UpdateM4R1Quests(float DeltaSeconds);
    void WriteM4R1QuestsReport(const TSharedRef<FJsonObject>& Root) const;
    void UpdateM4R1Profile(double RealDelta);
    void WriteM4R1Profile(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4R1PerfSequence();
    void BuildM4R1JointSequence();
    void UpdateM4R1Joint(float DeltaSeconds);
    void WriteM4R1JointReport(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4R1SteeringSequence();
    void UpdateM4R1Steering(float DeltaSeconds);
    void WriteM4R1SteeringReport(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4R1ImpactSequence();
    void UpdateM4R1Impact(float DeltaSeconds);
    void WriteM4R1ImpactReport(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4R1ReactionSequence();
    void UpdateM4R1Reaction(float DeltaSeconds);
    void WriteM4R1ReactionReport(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4PlaythroughSequence();
    void UpdateM4Playthrough(float DeltaSeconds);
    void StopM4Playthrough();
    void WriteM4PlaythroughReport(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4Sequence();
    void BuildM4R2PerspectiveSequence();
    void BuildM4R2NavigationSequence();
    void BuildM4R2PerformanceSequence();
    struct FM4R2CombatState;
    TSharedPtr<FM4R2CombatState> M4R2Combat;
    void BuildM4R2CombatSequence();
    void BuildM4R2PlaythroughSequence();
    void UpdateM4R2Combat(float DeltaSeconds);
    void StopM4R2CombatInput();
    void WriteM4R2CombatReport(const TSharedRef<FJsonObject>& Root) const;
    void BuildM4BallisticsSequence();
    void BuildM4NPCSequence();
    void UpdateM4Test(float DeltaSeconds);
    void WriteM4Report(const TSharedRef<FJsonObject>& Root) const;
    void StopM4TestInput();
    void BuildM3PlaythroughSequence();
    void UpdateM3Playthrough(float DeltaSeconds);
    void WriteM3PlaythroughReport(TSharedRef<FJsonObject> Root) const;
    void StopM3PlaythroughForUser();
    void BuildM3Sequence();
    void BuildM3LegacyActualSequence();
    TSharedPtr<FJsonObject> M3LegacyActualReport;
    void UpdateM3Test(float DeltaSeconds);
    void WriteM3Report(const TSharedRef<FJsonObject>& Root) const;
    void StopM3TestInput();
    bool PlaceM3NearNPC(FName StableId);
    void AddM3Dialogue(FName StableId, bool bComplete, const FString& Label);
    void AddM3RideCompletion();
    struct FM3SafetyState;
    TSharedPtr<FM3SafetyState> M3Safety;
    void BuildM3SafetySequence();
    void UpdateM3Safety(float DeltaSeconds);
    void WriteM3SafetyReport(const TSharedRef<FJsonObject>& Root) const;
    void StopM3SafetyInput();
    void BuildSequence();
    void BuildM2Sequence();
    void BuildM2RouteSequence(bool bWalking);
    void BuildM2ReviewSequence();
    void UpdateM2Route(float DeltaSeconds, double RealDelta);
    void WriteM2RouteReport(const TSharedRef<FJsonObject>& Root) const;
    void StopM2RouteInput();
    void BuildM2CameraSequence();
    void UpdateM2Camera(float DeltaSeconds);
    void WriteM2CameraReport(const TSharedRef<FJsonObject>& Root) const;
    void StopM2CameraInput();
    void BuildLookCalibrationSequence();
    void UpdateLookCalibration(float DeltaSeconds);
    void WriteLookCalibrationReport(const TSharedRef<FJsonObject>& Root) const;
    void StopLookCalibrationInput();
    void BuildCameraSequence();
    void UpdateCameraTest(float DeltaSeconds);
    void WriteCameraReport(const TSharedRef<FJsonObject>& Root) const;
    void StartCameraLook(const FVector2D& UnitsPerSecond);
    void StopCameraLook();
    void CheckCameraPose(const FString& Name);
    bool RequireCamera(const FString& Name, bool bPassed, const FString& Expected, const FString& Actual, const FString& Evidence = TEXT("A"));
    void BuildSafetySequence();
    void BuildPlaythroughSequence();
    void UpdatePlaythroughInput();
    void StartPlaythroughNavigation(const FVector& Target);
    void CheckPlaythroughNavigation(const FString& Name);
    void StopPlaythroughOnFailure(const FString& Name, bool bPassed, const FString& Expected, const FString& Actual);
    void AddStep(const FString& Name, double Duration, TFunction<void()> Begin, TFunction<void()> End = nullptr);
    void Hold(FName Action, const FInputActionValue& Value);
    void Release(FName Action);
    void Tap(FName Action, double Wait = 0.65);
    void Check(const FString& Name, bool bPassed, const FString& Expected, const FString& Actual, const FString& Evidence = TEXT("B"));
    void WriteReport();
    void Finish();
    void CompleteFinish();
    void Capture(const FString& Name);
    bool IsM4R1ImpactProfileCaptureSuppressed() const;
    void AddM3CaptureCompletion(const FString& Name);
    void PlaceCharacter(const FVector& Location, float Yaw = 0);
    void ApproachCar();
    void PlaceCar(const FTransform& Transform);
    AActor* BoxFixture(const FVector& Location, const FVector& Size, float Yaw = 0);
    void ClearFixtures();
    int32 CountCharacters() const;
    bool IsDriving() const;
    bool IsOnFoot() const;
    FString State() const;
};
