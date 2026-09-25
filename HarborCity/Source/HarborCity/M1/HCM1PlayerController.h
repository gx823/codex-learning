#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "M4R2/HCM4R2ViewPreferences.h"
#include "M5/HCM5StoryState.h"
#include "HCM1PlayerController.generated.h"

class AHCM1Character;
class AHCM1Vehicle;
class AHCM2SceneSettings;
class AHCM3Experience;
class AHCM3NPC;
class UInputAction;
class UInputMappingContext;
class UHCM4CombatComponent;
class AHCM4R2PlayerCameraManager;
class UHCM4R2NavigationComponent;
class AHCM5StoryDirector;
class UHCM5VS2FlightComponent;

UENUM(BlueprintType)
enum class EHCPlayerMode : uint8
{
    OnFoot,
    Entering,
    Driving,
    Exiting
};

/** The only owner of possession, interaction transitions and input mode. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM1PlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AHCM1PlayerController();
    bool IsControlsPanelOpen() const { return bControlsPanelOpen; }
    void ToggleControlsPanel() { bControlsPanelOpen = !bControlsPanelOpen; }
    void CycleVS2TimeOfDay();
    UFUNCTION(BlueprintPure, Category="HarborCity|Flight") UHCM5VS2FlightComponent* GetFlightComponent() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|Flight") bool IsFlying() const;
    UFUNCTION(BlueprintCallable, Category="HarborCity|Flight") void RequestToggleFlight();
    UFUNCTION(BlueprintPure) bool IsFirstPersonPerspective() const;
    UFUNCTION(BlueprintPure) EHCM4R2Perspective GetOnFootPerspective() const { return OnFootPerspective; }
    UFUNCTION(BlueprintPure) EHCM4R2Perspective GetDrivingPerspective() const { return DrivingPerspective; }
    UFUNCTION(BlueprintCallable) void TogglePerspective();
    UFUNCTION(BlueprintCallable) void ToggleOnFootPerspectivePreference();
    UFUNCTION(BlueprintCallable) void ToggleDrivingPerspectivePreference();
    AHCM4R2PlayerCameraManager* GetPerspectiveManager() const;
    UHCM4R2NavigationComponent* GetNavigationComponent() const { return Navigation; }
    FString GetPerspectivePreferencesSlot() const;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void FlushPressedKeys() override;
    uint64 GetLookCallbackCount() const { return LookCallbackCount; }
    FVector2D GetLastLookAxis() const { return LastLookAxis; }
    FString GetLookInputDiagnostics() const;
    bool IsGameplayFocused() const { return bGameplayFocused; }
    /** Effective signed degrees per Look Action unit, including the shared look scale. */
    FVector2D GetOnFootLookDegreesPerActionUnit() const;
    FVector2D GetDrivingLookDegreesPerActionUnit() const;
    static constexpr float LookSensitivityScale = 0.60f;
    static constexpr float DrivingLookSensitivityRatio = 0.75f;
    UFUNCTION(BlueprintPure, Category="HarborCity|M4") UHCM4CombatComponent* GetCombatComponent() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|M4") float GetCurrentAimLookMultiplier() const;

    UFUNCTION(BlueprintCallable, Category="HarborCity|Interaction")
    void RequestEnterVehicle(AHCM1Vehicle* Vehicle);
    UFUNCTION(BlueprintCallable, Category="HarborCity|Interaction")
    void RequestExitVehicle();
    UFUNCTION(BlueprintCallable, Category="HarborCity|Interaction")
    void RequestInteract();
    UFUNCTION(BlueprintCallable, Category="HarborCity|Save")
    bool SaveGameNow();
    UFUNCTION(BlueprintCallable, Category="HarborCity|Save")
    bool LoadGameNow();
    UFUNCTION(BlueprintCallable, Category="HarborCity|UI")
    void TogglePauseMenu();
    UFUNCTION(BlueprintCallable, Category="HarborCity|UI")
    void RestartPrototype();
    UFUNCTION(BlueprintCallable, Category="HarborCity|UI")
    void QuitPrototype();
    UFUNCTION(BlueprintCallable, Category="HarborCity|Vehicle")
    void RequestVehicleReset();

    UFUNCTION(BlueprintPure, Category="HarborCity|Input")
    UInputAction* GetM1Action(FName Name) const;
    UFUNCTION(BlueprintPure, Category="HarborCity|State")
    EHCPlayerMode GetPlayerMode() const { return PlayerMode; }
    UFUNCTION(BlueprintPure, Category="HarborCity|State")
    AHCM1Character* GetControlledCharacter() const { return ControlledCharacter; }
    UFUNCTION(BlueprintPure, Category="HarborCity|State")
    AHCM1Vehicle* GetActiveVehicle() const { return ActiveVehicle; }
    UFUNCTION(BlueprintPure, Category="HarborCity|State")
    bool IsPauseMenuOpen() const { return bPauseMenuOpen; }
    UFUNCTION(BlueprintPure, Category="HarborCity|UI")
    FString GetInteractionPrompt() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|UI")
    FString GetStatusMessage() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|UI")
    float GetSpeedKmh() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|Save")
    FString GetSaveSlotName() const { return SaveSlotName; }
    UFUNCTION(BlueprintPure, Category="HarborCity|Scene")
    AHCM2SceneSettings* GetSceneSettings() const { return SceneSettings; }
    UFUNCTION(BlueprintPure, Category="HarborCity|UI") FString GetExperienceTitle() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|UI") FString GetSceneControlHint() const;
    UFUNCTION(BlueprintCallable, Category="HarborCity|Dialogue") void BeginNPCDialogue(AHCM3NPC* NPC);
    UFUNCTION(BlueprintCallable, Category="HarborCity|Dialogue") void EndNPCDialogue();
    UFUNCTION(BlueprintPure, Category="HarborCity|Dialogue") bool IsDialogueOpen() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|Dialogue") FString GetDialogueName() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|Dialogue") FString GetDialogueLine() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|Dialogue") FString GetDialogueAdvanceHint() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|M3") FString GetQuestHUDText() const;
    UFUNCTION(BlueprintPure, Category="HarborCity|M3") AHCM3Experience* GetM3Experience() const { return M3Experience; }
    AHCM5StoryDirector* GetM5Story() const { return M5Story; }
    UFUNCTION(BlueprintPure, Category="HarborCity|Interaction")
    bool CanReachInteraction(AActor* Target) const;
    UFUNCTION(BlueprintCallable, Category="HarborCity|UI")
    void ShowStatusMessage(const FString& Message, float Lifetime = 3.0f);
    /** Reuses the existing ground/capsule-safe on-foot recovery after NPC defeat. */
    bool RecoverAfterNPCDefeat();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="HarborCity|State")
    EHCPlayerMode PlayerMode = EHCPlayerMode::OnFoot;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHCM4R2NavigationComponent> Navigation;
    EHCM4R2Perspective OnFootPerspective = EHCM4R2Perspective::ThirdPerson;
    EHCM4R2Perspective DrivingPerspective = EHCM4R2Perspective::ThirdPerson;
    void LoadPerspectivePreferences();
    void SavePerspectivePreferences();
    UPROPERTY(Transient) TObjectPtr<AHCM2SceneSettings> SceneSettings;
    UPROPERTY(Transient) TObjectPtr<AHCM3Experience> M3Experience;
    UPROPERTY(Transient) TObjectPtr<AHCM5StoryDirector> M5Story;
    TArray<FHCM5DialogueLine> StoryDialogueLines;
    double DialogueLineStartedAt = 0;
    bool bDialogueLineRevealed = false;
    TWeakObjectPtr<AHCM3NPC> DialogueNPC;
    TArray<FString> DialogueLines;
    int32 DialogueLineIndex = 0;
    void AdvanceNPCDialogue();
    bool bSceneSaveIdentityValid = true;
    FName SavePlayerId = TEXT("M1_Player");
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> ControlledCharacter;
    UPROPERTY(Transient) TObjectPtr<AHCM1Vehicle> ActiveVehicle;
    UPROPERTY(Transient) TObjectPtr<AHCM1Vehicle> PendingVehicle;
    bool bControlsPanelOpen = false;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputAction>> Actions;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> CommonContext;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> OnFootContext;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DrivingContext;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> FlightContext;
    TWeakObjectPtr<AActor> InteractionTarget;
    FTimerHandle TransitionTimer;
    FDelegateHandle ActivationHandle;
    FTransform InitialPlayerTransform;
    FVector2D DriveAxis = FVector2D::ZeroVector;
    bool bHandbrake = false;
    bool bPauseMenuOpen = false;
    bool bRecoverySpawnAttempted = false;
    bool bGameplayFocused = true;
    uint64 LookCallbackCount = 0;
    FVector2D LastLookAxis = FVector2D::ZeroVector;
    FString StatusMessage;
    double StatusMessageUntil = 0;
    FString SaveSlotName = TEXT("HarborCity_M1_V1");

    void CreateInputObjects();
    void ApplyInputContexts();
    void ClearGameplayInput();
    void HandleApplicationActivation(bool bActive);
    void RefreshInteractionTarget();
    bool HasInteractionLine(AActor* Target) const;
    void FinishEntering();
    void FinishExiting();
    void RecoverOnFoot(const FString& Reason);
    bool ResolveSafePlayerTransform(const FTransform& Requested, FTransform& Out, AActor* IgnoreVehicle = nullptr) const;
    bool CanUseSave() const;
    AHCM1Vehicle* FindSaveVehicle() const;
    void MoveInput(const FInputActionValue& Value);
    void MoveReleased(const FInputActionValue& Value);
    void LookInput(const FInputActionValue& Value);
    void SprintPressed();
    void SprintReleased();
    void JumpPressed();
    void JumpReleased();
    void FlightDescendPressed();
    void FlightDescendReleased();
    void DriveInput(const FInputActionValue& Value);
    void DriveReleased(const FInputActionValue& Value);
    void HandbrakePressed();
    void HandbrakeReleased();
    void SavePressed();
    void LoadPressed();
    void AttackPressed();
    void AimPressed();
    void AimReleased();
    void ToggleWeaponPressed();
    void ReloadPressed();
};
