#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2LookdevDirector.generated.h"

class ACameraActor;
class ADirectionalLight;
class AHCM1Character;
class APlayerController;
class UGameViewportClient;
class UMaterialInterface;
struct FInputKeyEventArgs;
class FJsonObject;
struct FHCM5VS2RenderMaterialCheck;

USTRUCT(BlueprintType)
struct FHCM5VS2MaterialVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    /** Original VS1 baseline uses the saved material interfaces without modifying them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseOriginalMaterials = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UMaterialInterface>> Materials;
};

USTRUCT(BlueprintType)
struct FHCM5VS2LookdevShot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VariantIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector CameraLocation = FVector::ZeroVector;
    /** Face shots use animated eye-bone height sampled once after warmup; false uses CameraLocation.Z. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUsePlayerEyeHeight = true;
    /** Yaw is used; pitch and roll are always zero for the eye-level comparison. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator CameraRotation = FRotator::ZeroRotator;
    /** Rotation of the actual directional light actor, not a fabricated shader-only sun. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator LightDirection = FRotator(-35.f, -45.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor LightColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LightIntensity = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor AmbientColor = FLinearColor(.30f, .32f, .38f);
};

/** Opt-in, bounded native viewport comparisons. Never generates OS input. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2LookdevDirector : public AActor
{
    GENERATED_BODY()

public:
    AHCM5VS2LookdevDirector();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5VS2|Lookdev")
    TArray<FHCM5VS2MaterialVariant> MaterialVariants;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5VS2|Lookdev")
    TArray<FHCM5VS2LookdevShot> Shots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5VS2|Lookdev")
    TObjectPtr<ADirectionalLight> DirectionalLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5VS2|Lookdev")
    bool bUsePlayerEyeHeight = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HarborCity|M5VS2|Lookdev")
    bool bHideHUD = true;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool InitializeRun(FString& Failure);
    bool ApplyShot(int32 Index, FString& Failure);
    bool LockAnimatedEyeHeight(FString& Failure);
    bool PollMaterialReadiness(double Now, FString& Failure);
    void RequestCurrentShot();
    void OnViewportInputKey(const FInputKeyEventArgs& Event);
    void OnScreenshotProcessed();
    void StopForUser();
    void Finish(const FString& Status, const FString& Detail);
    void RestoreOriginalState();
    void WriteReport(const FString& Status, const FString& Detail);
    void RemoveDelegates();

    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<APlayerController> Controller;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> CaptureCamera;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
    UPROPERTY(Transient) TArray<FHCM5VS2MaterialVariant> RuntimeVariants;
    TWeakObjectPtr<AActor> OriginalViewTarget;
    TWeakObjectPtr<UGameViewportClient> ObservedViewport;
    FDelegateHandle InputHandle;
    FDelegateHandle ScreenshotHandle;
    TArray<TSharedPtr<FJsonObject>> Results;
    TSharedPtr<FJsonObject> PendingResult;
    TSharedPtr<FJsonObject> LastMaterialReadiness;
    TSharedPtr<FHCM5VS2RenderMaterialCheck, ESPMode::ThreadSafe> PendingRenderMaterialCheck;
    FString RunDirectory;
    FString PendingScreenshot;
    FRotator OriginalLightRotation = FRotator::ZeroRotator;
    FLinearColor OriginalLightColor = FLinearColor::White;
    float OriginalLightIntensity = 0.f;
    FVector InitialEyeLocation = FVector::ZeroVector;
    FVector SampledLeftEye = FVector::ZeroVector;
    FVector SampledRightEye = FVector::ZeroVector;
    FVector LockedEyeMidpoint = FVector::ZeroVector;
    int32 LeftEyeBoneIndex = INDEX_NONE;
    int32 RightEyeBoneIndex = INDEX_NONE;
    bool bEyeHeightSampled = false;
    double EyeSampleWarmupSeconds = 0;
    uint64 EyeSampleFrame = 0;
    bool bOriginalShowHUD = true;
    bool bSavedState = false;
    bool bRunning = false;
    bool bUserStopped = false;
    bool bCapturePending = false;
    bool bScreenshotProcessed = false;
    bool bAwaitingAutoQuit = false;
    bool bAutoQuit = false;
    bool bRestored = false;
    int32 ShotIndex = INDEX_NONE;
    int32 HoldFrames = 0;
    double StartedAt = 0;
    double PhaseElapsed = 0;
    double RequestedAt = 0;
    double FinishedAt = 0;
    uint64 RequestFrame = 0;
    uint64 StopFrame = 0;
    double ShaderWaitStartedAt = 0;
    double NextShaderPollAt = 0;
    uint64 ShaderReadyFrame = 0;
    int32 ShaderPollCount = 0;
    bool bMaterialsReady = false;
};
