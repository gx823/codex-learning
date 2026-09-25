#include "HCM1PlayerController.h"

#include "HCM1Character.h"
#include "M5VS2/HCM5VS2FlightComponent.h"
#include "M5VS2/HCM5VS2CornerTimeDirector.h"
#include "HCM1LightSwitch.h"
#include "HCM1SaveGame.h"
#include "HCM1Vehicle.h"
#include "HCM1CameraBoom.h"
#include "M2/HCM2SceneSettings.h"
#include "M3/HCM3Experience.h"
#include "M5/HCM5StoryDirector.h"
#include "M3/HCM3NPC.h"
#include "M4/HCM4CombatComponent.h"
#include "M4/HCM4Facing.h"
#include "M4R2/HCM4R2PlayerCameraManager.h"
#include "M4R2/HCM4R2Navigation.h"
#include "HCInteractable.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
double GetSaveHorizontalLimit(const UWorld* World, const AHCM2SceneSettings* Scene)
{
    // Only the live M5 map can opt into its 360 m playable footprint. Save data
    // and test-slot names must never select a more permissive coordinate limit.
    const bool bM5World = World && IsValid(Scene) && Scene->GetWorld() == World
        && Scene->HasValidSaveIdentity()
        && Scene->SceneId == FName(TEXT("M5_CyberHarbor"))
        && Scene->MapName == FName(TEXT("L_M5_CyberHarbor"))
        && UGameplayStatics::GetCurrentLevelName(World, true) == TEXT("L_M5_CyberHarbor");
    return bM5World ? 18000.0 : 10000.0;
}

bool IsBoundedTransform(const FTransform& Transform, double HorizontalLimit)
{
    const FVector P = Transform.GetLocation();
    return Transform.IsValid() && !Transform.ContainsNaN()
        && Transform.GetScale3D().Equals(FVector::OneVector, 0.01)
        && FMath::Abs(P.X) <= HorizontalLimit && FMath::Abs(P.Y) <= HorizontalLimit
        && P.Z >= -1000 && P.Z <= 5000;
}
}

AHCM1PlayerController::AHCM1PlayerController()
{
    bShouldPerformFullTickWhenPaused = true;
    PlayerCameraManagerClass = AHCM4R2PlayerCameraManager::StaticClass();
    Navigation = CreateDefaultSubobject<UHCM4R2NavigationComponent>(TEXT("R2Navigation"));
}

void AHCM1PlayerController::CreateInputObjects()
{
    if (CommonContext) return;
    CommonContext = NewObject<UInputMappingContext>(this, TEXT("M1_Common"));
    OnFootContext = NewObject<UInputMappingContext>(this, TEXT("M1_OnFoot"));
    DrivingContext = NewObject<UInputMappingContext>(this, TEXT("M1_Driving"));
    FlightContext = NewObject<UInputMappingContext>(this, TEXT("VS2_Flight"));
    auto AddAction = [this](FName Name, EInputActionValueType Type)
    {
        UInputAction* Action = NewObject<UInputAction>(this, FName(*FString::Printf(TEXT("M1_%s"), *Name.ToString())));
        Action->ValueType = Type;
        if (Type == EInputActionValueType::Axis2D)
            Action->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
        Actions.Add(Name, Action);
        return Action;
    };
    for (const TCHAR* Name : {TEXT("Move"), TEXT("Look"), TEXT("Drive")}) AddAction(Name, EInputActionValueType::Axis2D);
    for (const TCHAR* Name : {TEXT("Sprint"), TEXT("Jump"), TEXT("Interact"), TEXT("Handbrake"),
        TEXT("ResetVehicle"), TEXT("Pause"), TEXT("Save"), TEXT("Load"), TEXT("DialogueCancel"),
        TEXT("Attack"), TEXT("Aim"), TEXT("ToggleWeapon"), TEXT("Reload"), TEXT("Perspective"), TEXT("FlightToggle"), TEXT("FlightDescend"), TEXT("Help")}) AddAction(Name, EInputActionValueType::Boolean);
    CommonContext->MapKey(GetM1Action(TEXT("Help")), EKeys::H);
    CommonContext->MapKey(AddAction(TEXT("TimeOfDay"), EInputActionValueType::Boolean), EKeys::T);
    FlightContext->MapKey(GetM1Action(TEXT("FlightToggle")), EKeys::F);
    FlightContext->MapKey(GetM1Action(TEXT("FlightDescend")), EKeys::LeftControl);
    CommonContext->MapKey(GetM1Action(TEXT("Perspective")), EKeys::V);
    GetM1Action(TEXT("Pause"))->bTriggerWhenPaused = true;
    CommonContext->MapKey(GetM1Action(TEXT("Interact")), EKeys::E);
    CommonContext->MapKey(GetM1Action(TEXT("Pause")), EKeys::Escape);
    CommonContext->MapKey(GetM1Action(TEXT("Pause")), EKeys::P);
    CommonContext->MapKey(GetM1Action(TEXT("Save")), EKeys::F5);
    CommonContext->MapKey(GetM1Action(TEXT("Load")), EKeys::F9);
    CommonContext->MapKey(GetM1Action(TEXT("DialogueCancel")), EKeys::BackSpace);
    OnFootContext->MapKey(GetM1Action(TEXT("Sprint")), EKeys::LeftShift);
    OnFootContext->MapKey(GetM1Action(TEXT("Jump")), EKeys::SpaceBar);
    OnFootContext->MapKey(GetM1Action(TEXT("Look")), EKeys::Mouse2D);
    OnFootContext->MapKey(GetM1Action(TEXT("Attack")), EKeys::LeftMouseButton);
    OnFootContext->MapKey(GetM1Action(TEXT("Aim")), EKeys::RightMouseButton);
    OnFootContext->MapKey(GetM1Action(TEXT("ToggleWeapon")), EKeys::Q);
    OnFootContext->MapKey(GetM1Action(TEXT("Reload")), EKeys::R);
    bool bLegacyCameraInput = false;
#if !UE_BUILD_SHIPPING
    bLegacyCameraInput = FParse::Param(FCommandLine::Get(), TEXT("HCM1CameraLegacy"));
#endif
    if (!bLegacyCameraInput) DrivingContext->MapKey(GetM1Action(TEXT("Look")), EKeys::Mouse2D);
    DrivingContext->MapKey(GetM1Action(TEXT("Handbrake")), EKeys::SpaceBar);
    DrivingContext->MapKey(GetM1Action(TEXT("ResetVehicle")), EKeys::R);
    auto MapAxis = [this](UInputMappingContext* Context, UInputAction* Action, FKey Key, bool bY, bool bNegative)
    {
        FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
        if (bNegative) Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
        if (bY)
        {
            UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
            Swizzle->Order = EInputAxisSwizzle::YXZ;
            Mapping.Modifiers.Add(Swizzle);
        }
    };
    for (UInputMappingContext* Context : {OnFootContext.Get(), DrivingContext.Get()})
    {
        UInputAction* Action = GetM1Action(Context == OnFootContext ? TEXT("Move") : TEXT("Drive"));
        MapAxis(Context, Action, EKeys::W, true, false);
        MapAxis(Context, Action, EKeys::S, true, true);
        MapAxis(Context, Action, EKeys::A, false, true);
        MapAxis(Context, Action, EKeys::D, false, false);
    }
}

void AHCM1PlayerController::CycleVS2TimeOfDay()
{
    if (bPauseMenuOpen || !bGameplayFocused) return;
    for (TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld()); It; ++It)
    {
        const FName Next = It->CurrentPeriod == TEXT("Afternoon") ? FName(TEXT("Dusk")) :
            It->CurrentPeriod == TEXT("Dusk") ? FName(TEXT("Night")) : FName(TEXT("Afternoon"));
        It->SetTimeOfDay(Next); break;
    }
}

UInputAction* AHCM1PlayerController::GetM1Action(FName Name) const
{
    const TObjectPtr<UInputAction>* Found = Actions.Find(Name);
    return Found ? Found->Get() : nullptr;
}

void AHCM1PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    CreateInputObjects();
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
    if (!ensureMsgf(Input, TEXT("M1 requires the project EnhancedInputComponent default"))) return;
    Input->BindAction(GetM1Action(TEXT("Help")), ETriggerEvent::Started, this, &ThisClass::ToggleControlsPanel);
    Input->BindAction(GetM1Action(TEXT("TimeOfDay")), ETriggerEvent::Started, this, &ThisClass::CycleVS2TimeOfDay);
    Input->BindAction(GetM1Action(TEXT("FlightToggle")), ETriggerEvent::Started, this, &ThisClass::RequestToggleFlight);
    Input->BindAction(GetM1Action(TEXT("FlightDescend")), ETriggerEvent::Started, this, &ThisClass::FlightDescendPressed);
    Input->BindAction(GetM1Action(TEXT("FlightDescend")), ETriggerEvent::Completed, this, &ThisClass::FlightDescendReleased);
    Input->BindAction(GetM1Action(TEXT("FlightDescend")), ETriggerEvent::Canceled, this, &ThisClass::FlightDescendReleased);
    Input->BindAction(GetM1Action(TEXT("Move")), ETriggerEvent::Triggered, this, &ThisClass::MoveInput);
    Input->BindAction(GetM1Action(TEXT("Move")), ETriggerEvent::Completed, this, &ThisClass::MoveReleased);
    Input->BindAction(GetM1Action(TEXT("Move")), ETriggerEvent::Canceled, this, &ThisClass::MoveReleased);
    Input->BindAction(GetM1Action(TEXT("Look")), ETriggerEvent::Triggered, this, &ThisClass::LookInput);
    Input->BindAction(GetM1Action(TEXT("Sprint")), ETriggerEvent::Started, this, &ThisClass::SprintPressed);
    Input->BindAction(GetM1Action(TEXT("Sprint")), ETriggerEvent::Completed, this, &ThisClass::SprintReleased);
    Input->BindAction(GetM1Action(TEXT("Sprint")), ETriggerEvent::Canceled, this, &ThisClass::SprintReleased);
    Input->BindAction(GetM1Action(TEXT("Jump")), ETriggerEvent::Started, this, &ThisClass::JumpPressed);
    Input->BindAction(GetM1Action(TEXT("Jump")), ETriggerEvent::Completed, this, &ThisClass::JumpReleased);
    Input->BindAction(GetM1Action(TEXT("Jump")), ETriggerEvent::Canceled, this, &ThisClass::JumpReleased);
    Input->BindAction(GetM1Action(TEXT("Drive")), ETriggerEvent::Triggered, this, &ThisClass::DriveInput);
    Input->BindAction(GetM1Action(TEXT("Drive")), ETriggerEvent::Completed, this, &ThisClass::DriveReleased);
    Input->BindAction(GetM1Action(TEXT("Drive")), ETriggerEvent::Canceled, this, &ThisClass::DriveReleased);
    Input->BindAction(GetM1Action(TEXT("Handbrake")), ETriggerEvent::Started, this, &ThisClass::HandbrakePressed);
    Input->BindAction(GetM1Action(TEXT("Handbrake")), ETriggerEvent::Completed, this, &ThisClass::HandbrakeReleased);
    Input->BindAction(GetM1Action(TEXT("Handbrake")), ETriggerEvent::Canceled, this, &ThisClass::HandbrakeReleased);
    Input->BindAction(GetM1Action(TEXT("Interact")), ETriggerEvent::Started, this, &ThisClass::RequestInteract);
    Input->BindAction(GetM1Action(TEXT("ResetVehicle")), ETriggerEvent::Started, this, &ThisClass::RequestVehicleReset);
    Input->BindAction(GetM1Action(TEXT("Pause")), ETriggerEvent::Started, this, &ThisClass::TogglePauseMenu);
    Input->BindAction(GetM1Action(TEXT("Save")), ETriggerEvent::Started, this, &ThisClass::SavePressed);
    Input->BindAction(GetM1Action(TEXT("Load")), ETriggerEvent::Started, this, &ThisClass::LoadPressed);
    Input->BindAction(GetM1Action(TEXT("DialogueCancel")), ETriggerEvent::Started, this, &ThisClass::EndNPCDialogue);
    Input->BindAction(GetM1Action(TEXT("Attack")), ETriggerEvent::Started, this, &ThisClass::AttackPressed);
    // Level-triggered hold restores ADS after reload; SetAimHeld ignores unchanged states and
    // still gates reload, pause, focus, dialogue and possession. Completed/Canceled releases it.
    Input->BindAction(GetM1Action(TEXT("Aim")), ETriggerEvent::Triggered, this, &ThisClass::AimPressed);
    Input->BindAction(GetM1Action(TEXT("Aim")), ETriggerEvent::Completed, this, &ThisClass::AimReleased);
    Input->BindAction(GetM1Action(TEXT("Aim")), ETriggerEvent::Canceled, this, &ThisClass::AimReleased);
    Input->BindAction(GetM1Action(TEXT("ToggleWeapon")), ETriggerEvent::Started, this, &ThisClass::ToggleWeaponPressed);
    Input->BindAction(GetM1Action(TEXT("Reload")), ETriggerEvent::Started, this, &ThisClass::ReloadPressed);
    Input->BindAction(GetM1Action(TEXT("Perspective")), ETriggerEvent::Started, this, &ThisClass::TogglePerspective);
}

void AHCM1PlayerController::BeginPlay()
{
    Super::BeginPlay();
    CreateInputObjects();
    int32 SceneSettingsCount = 0;
    for (TActorIterator<AHCM2SceneSettings> It(GetWorld()); It; ++It)
    { SceneSettings = *It; ++SceneSettingsCount; }
    for (TActorIterator<AHCM3Experience> It(GetWorld()); It; ++It) { M3Experience = *It; break; }
    M5Story = AHCM5StoryDirector::Find(GetWorld());
    if (SceneSettings)
    {
        bSceneSaveIdentityValid = SceneSettingsCount == 1 && SceneSettings->HasValidSaveIdentity();
        // A malformed M2 scene must never fall back to the M1 user's slot.
        SaveSlotName = bSceneSaveIdentityValid ? SceneSettings->SaveSlot : TEXT("HarborCity_M2_InvalidScene");
        SavePlayerId = SceneSettings->PlayerId;
    }
    else if (UGameplayStatics::GetCurrentLevelName(this, true).StartsWith(TEXT("L_M2_")))
    { bSceneSaveIdentityValid = false; SaveSlotName = TEXT("HarborCity_M2_InvalidScene"); }
    if (AHCM1Character* M1Pawn = Cast<AHCM1Character>(GetPawn()))
    {
        ControlledCharacter = M1Pawn;
        InitialPlayerTransform = M1Pawn->GetActorTransform();
    }
    FString TestSlot;
    bool bVS2PrivateTestMap=false;
#if !UE_BUILD_SHIPPING
    bVS2PrivateTestMap=GetWorld() && GetWorld()->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/"));
#endif
    if (FParse::Value(FCommandLine::Get(), TEXT("HCM1SaveSlot="), TestSlot)
        && ((bVS2PrivateTestMap && TestSlot.StartsWith(TEXT("HarborCity_VS2_")) && TestSlot.Len()>15)
            || (SceneSettings ? (TestSlot.StartsWith(TEXT("HarborCity_M2_V1_Test_")) || TestSlot.StartsWith(TEXT("HarborCity_M5_VS1_Test_")))
            : (TestSlot.StartsWith(TEXT("HarborCity_M1_V1_Test_")) || TestSlot.StartsWith(TEXT("HarborCity_M1_R1_Test_"))
                || TestSlot.StartsWith(TEXT("HarborCity_M1_R2_Test_"))))) && TestSlot.Len() <= 100)
    {
        bool bSafeName = true;
        for (TCHAR C : TestSlot) bSafeName &= FChar::IsAlnum(C) || C == TEXT('_');
        if (bSafeName) SaveSlotName = TestSlot;
    }
    LoadPerspectivePreferences();
    if (IsLocalController() && FSlateApplication::IsInitialized())
    {
        bGameplayFocused = FSlateApplication::Get().IsActive();
        ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::HandleApplicationActivation);
    }
    ApplyInputContexts();
    if (bGameplayFocused) SetInputMode(FInputModeGameOnly());
    ShowStatusMessage(M5Story ? TEXT("澪光的第一夜 · 先到夜航情报屋找到弦音。E 互动，V 切换视角。") : SceneSettings ? TEXT("海湾漫游 · 海滨街道。E 上车或互动，T 切换下午／黄昏。")
        : TEXT("海湾漫游 M1-R2：驾驶鼠标灵敏度为步行的 75%。靠近车辆按 E 上车。"), 6);
}

void AHCM1PlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (AHCM1Character* M1Pawn = Cast<AHCM1Character>(InPawn))
    {
        if (!IsValid(ControlledCharacter)) InitialPlayerTransform = M1Pawn->GetActorTransform();
        ControlledCharacter = M1Pawn;
        bRecoverySpawnAttempted = false;
    }
}

void AHCM1PlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    EndNPCDialogue();
    GetWorldTimerManager().ClearTimer(TransitionTimer);
    ClearGameplayInput();
    if (IsValid(ActiveVehicle)) ActiveVehicle->SetDriver(nullptr);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
    if (ULocalPlayer* Local = GetLocalPlayer())
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            for (UInputMappingContext* Context : {CommonContext.Get(), OnFootContext.Get(), DrivingContext.Get(), FlightContext.Get()})
                if (Context) Subsystem->RemoveMappingContext(Context);
    Super::EndPlay(Reason);
}

void AHCM1PlayerController::ApplyInputContexts()
{
    CreateInputObjects();
    FlushPressedKeys();
    if (ULocalPlayer* Local = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            FModifyContextOptions Options;
            Options.bForceImmediately = true;
            Options.bIgnoreAllPressedKeysUntilRelease = true;
            Subsystem->RemoveMappingContext(OnFootContext, Options);
            Subsystem->RemoveMappingContext(DrivingContext, Options);
            Subsystem->RemoveMappingContext(FlightContext, Options);
            Subsystem->AddMappingContext(CommonContext, 100, Options);
            if (PlayerMode == EHCPlayerMode::OnFoot) Subsystem->AddMappingContext(OnFootContext, 0, Options);
            if (PlayerMode == EHCPlayerMode::Driving) Subsystem->AddMappingContext(DrivingContext, 0, Options);
            if (PlayerMode == EHCPlayerMode::OnFoot && GetFlightComponent() && GetFlightComponent()->IsFlightAvailable())
                Subsystem->AddMappingContext(FlightContext, 1, Options);
        }
    }
}

void AHCM1PlayerController::ClearGameplayInput()
{
    if (GetFlightComponent()) GetFlightComponent()->ClearFlightInput();
    if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->CancelTransient(TEXT("ClearGameplayInput"));
    DriveAxis = FVector2D::ZeroVector;
    LastLookAxis = FVector2D::ZeroVector;
    bHandbrake = false;
    if (IsValid(ActiveVehicle)) ActiveVehicle->ClearDriveInput(true);
    if (IsValid(ControlledCharacter))
    {
        ControlledCharacter->StopJumping();
        ControlledCharacter->SetSprinting(false);
        ControlledCharacter->ConsumeMovementInputVector();
        ControlledCharacter->GetCharacterMovement()->StopMovementImmediately();
    }
}

void AHCM1PlayerController::FlushPressedKeys()
{
    Super::FlushPressedKeys();
    ClearGameplayInput();
}

void AHCM1PlayerController::HandleApplicationActivation(bool bActive)
{
    if (!bActive) EndNPCDialogue();
    bGameplayFocused = bActive;
    FlushPressedKeys();
    if (IsValid(ActiveVehicle)) ActiveVehicle->ResetDrivingCamera(false);
    if (bActive && !bPauseMenuOpen)
    {
        // Do not steal focus from editor panels when the application is PIE.
        // Standalone game activation owns its viewport; PIE keeps UE's capture policy.
        if (GetWorld() && GetWorld()->WorldType == EWorldType::Game) SetInputMode(FInputModeGameOnly());
        ApplyInputContexts();
    }
}

void AHCM1PlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (PlayerMode == EHCPlayerMode::Driving && (!IsValid(ActiveVehicle) || !IsValid(ControlledCharacter)))
        RecoverOnFoot(TEXT("角色或车辆不可用，已恢复步行。"));
    if (IsDialogueOpen() && (bPauseMenuOpen || !bGameplayFocused || PlayerMode != EHCPlayerMode::OnFoot
        || !ControlledCharacter || !DialogueNPC->IsConversationActive() || !CanReachInteraction(DialogueNPC.Get()))) EndNPCDialogue();
    if (!bPauseMenuOpen && !IsDialogueOpen() && PlayerMode == EHCPlayerMode::OnFoot) RefreshInteractionTarget();
    else InteractionTarget.Reset();
}

bool AHCM1PlayerController::HasInteractionLine(AActor* Target) const
{
    if (!IsValid(Target) || !IsValid(ControlledCharacter) || !Target->Implements<UHCInteractable>()) return false;
    const AHCM1Vehicle* Vehicle = Cast<AHCM1Vehicle>(Target);
    const FVector Destination = Vehicle ? Vehicle->GetInteractionLocation() : Target->GetActorLocation();
    FVector Start = ControlledCharacter->GetActorLocation() + FVector(0, 0, 25);
    if (IsFirstPersonPerspective() && PlayerMode == EHCPlayerMode::OnFoot)
    { FRotator View; GetPlayerViewPoint(Start, View); }
    if (FVector::DistSquared(ControlledCharacter->GetActorLocation(), Destination) > FMath::Square(Vehicle ? 325.0f : 240.0f)) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(M1Interact), false, ControlledCharacter);
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, Destination, ECC_Visibility, Params);
    return !bHit || Hit.GetActor() == Target;
}

bool AHCM1PlayerController::CanReachInteraction(AActor* Target) const
{
    return !bPauseMenuOpen && !IsFlying() && PlayerMode == EHCPlayerMode::OnFoot && HasInteractionLine(Target);
}

void AHCM1PlayerController::RefreshInteractionTarget()
{
    InteractionTarget.Reset();
    if (!ControlledCharacter) return;
    float BestDistance = TNumericLimits<float>::Max();
    int32 BestPriority = TNumericLimits<int32>::Max();
    FString BestName;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (!CanReachInteraction(*It)) continue;
        if (IHCInteractable::Execute_GetInteractionText(*It, this).IsEmpty()) continue;
        // Existing E has one deterministic owner: vehicle, then light switch, then NPC.
        const int32 Priority = Cast<AHCM1Vehicle>(*It) ? 0 : (Cast<AHCM1LightSwitch>(*It) ? 1 : 2);
        const float Distance = FVector::DistSquared((*It)->GetActorLocation(), ControlledCharacter->GetActorLocation());
        const FString Name = It->GetPathName();
        if (Priority < BestPriority || (Priority == BestPriority && (Distance < BestDistance
            || (FMath::IsNearlyEqual(Distance, BestDistance) && (BestName.IsEmpty() || Name < BestName)))))
        { BestPriority = Priority; BestDistance = Distance; BestName = Name; InteractionTarget = *It; }
    }
}

void AHCM1PlayerController::RequestInteract()
{
    if (bPauseMenuOpen) return;
    if (IsFlying()) { ShowStatusMessage(TEXT("降落以继续")); return; }
    if (const UHCM4CombatComponent* Combat = GetCombatComponent())
        if (Combat->GetPlayerHealth() <= 0) return;
    if (IsDialogueOpen()) { AdvanceNPCDialogue(); return; }
    if (PlayerMode == EHCPlayerMode::Driving) { RequestExitVehicle(); return; }
    if (PlayerMode != EHCPlayerMode::OnFoot) { ShowStatusMessage(TEXT("正在切换，请稍候。"), 1); return; }
    RefreshInteractionTarget();
    if (AActor* Target = InteractionTarget.Get()) IHCInteractable::Execute_Interact(Target, this);
    else ShowStatusMessage(M3Experience ? TEXT("请靠近可见的车门、照明开关或可交谈的路人。") : TEXT("请靠近可见的车门或照明开关。"));
}

void AHCM1PlayerController::RequestEnterVehicle(AHCM1Vehicle* Vehicle)
{
    if (bPauseMenuOpen || IsDialogueOpen() || PlayerMode != EHCPlayerMode::OnFoot) return;
    if (const UHCM4CombatComponent* Combat = GetCombatComponent())
        if (Combat->GetPlayerHealth() <= 0) return;
    if (!CanReachInteraction(Vehicle)) { ShowStatusMessage(TEXT("请靠近无遮挡的驾驶位。")); return; }
    if (Vehicle->HasDriver()) { ShowStatusMessage(TEXT("驾驶位已被占用。")); return; }
    if (Vehicle->GetSpeedKmh() > 1) { ShowStatusMessage(TEXT("车辆尚未停稳，暂时不能上车。")); return; }
    ClearGameplayInput();
    if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->SetSeated(true);
    ControlledCharacter->GetCharacterMovement()->DisableMovement();
    PendingVehicle = Vehicle;
    PlayerMode = EHCPlayerMode::Entering;
    ApplyInputContexts();
    GetWorldTimerManager().SetTimer(TransitionTimer, this, &ThisClass::FinishEntering, 0.2f, false);
}

void AHCM1PlayerController::FinishEntering()
{
    if (PlayerMode != EHCPlayerMode::Entering) return;
    AHCM1Vehicle* Vehicle = PendingVehicle;
    PendingVehicle = nullptr;
    if (!IsValid(ControlledCharacter))
    {
        RecoverOnFoot(TEXT("角色不可用，上车已取消并恢复步行。"));
        return;
    }
    if (!IsValid(Vehicle) || Vehicle->HasDriver() || Vehicle->GetSpeedKmh() > 1 || !HasInteractionLine(Vehicle))
    {
        ControlledCharacter->SetSeated(false);
        PlayerMode = EHCPlayerMode::OnFoot;
        ApplyInputContexts();
        ShowStatusMessage(TEXT("上车条件已变化，已恢复步行。"));
        return;
    }
    ControlledCharacter->SetSeated(true);
    ActiveVehicle = Vehicle;
    Vehicle->SetDriver(this);
    Possess(Vehicle);
    PlayerMode = EHCPlayerMode::Driving;
    // Possession may change controller rotation; the vehicle's independent orbit
    // is initialized from its own heading, never from the old foot look angle.
    Vehicle->ResetDrivingCamera(true);
    SetViewTargetWithBlend(Vehicle, 0.2f);
    ApplyInputContexts();
    ShowStatusMessage(TEXT("已上车。直接移动鼠标环视，停车后按 E 下车。"));
}

void AHCM1PlayerController::RequestExitVehicle()
{
    if (bPauseMenuOpen || PlayerMode != EHCPlayerMode::Driving || !IsValid(ActiveVehicle)) return;
    if (ActiveVehicle->GetSpeedKmh() > 1) { ShowStatusMessage(TEXT("请先停车：速度低于 1 km/h 才能下车。")); return; }
    FTransform Safe;
    if (!ActiveVehicle->FindSafeExitTransform(ControlledCharacter, Safe))
    { ShowStatusMessage(TEXT("出口受阻或地面不安全，请换个位置停车。")); return; }
    ClearGameplayInput();
    PlayerMode = EHCPlayerMode::Exiting;
    ApplyInputContexts();
    GetWorldTimerManager().SetTimer(TransitionTimer, this, &ThisClass::FinishExiting, 0.2f, false);
}

void AHCM1PlayerController::FinishExiting()
{
    if (PlayerMode != EHCPlayerMode::Exiting) return;
    if (!IsValid(ControlledCharacter) || !IsValid(ActiveVehicle))
    {
        RecoverOnFoot(TEXT("角色或车辆不可用，下车切换已取消并恢复步行。"));
        return;
    }
    FTransform Safe;
    if (!IsValid(ActiveVehicle) || !ActiveVehicle->FindSafeExitTransform(ControlledCharacter, Safe))
    {
        PlayerMode = EHCPlayerMode::Driving;
        ApplyInputContexts();
        ShowStatusMessage(TEXT("出口已变化，留在车内以保证安全。"));
        return;
    }
    AHCM1Vehicle* Vehicle = ActiveVehicle;
    ControlledCharacter->SetActorTransform(Safe, false, nullptr, ETeleportType::TeleportPhysics);
    ControlledCharacter->SetSeated(false);
    Vehicle->SetDriver(nullptr);
    Possess(ControlledCharacter);
    ActiveVehicle = nullptr;
    PlayerMode = EHCPlayerMode::OnFoot;
    SetControlRotation(Safe.Rotator());
    SetViewTargetWithBlend(ControlledCharacter, 0.2f);
    ApplyInputContexts();
    ShowStatusMessage(TEXT("已安全下车。"));
}

bool AHCM1PlayerController::RecoverAfterNPCDefeat()
{
    if (!IsValid(ControlledCharacter) || PlayerMode != EHCPlayerMode::OnFoot || bPauseMenuOpen) return false;
    FTransform Safe;
    if (!ResolveSafePlayerTransform(ControlledCharacter->GetActorTransform(), Safe) && !ResolveSafePlayerTransform(InitialPlayerTransform, Safe)) return false;
    RecoverOnFoot(TEXT("已恢复行动。短暂保护期间可离开危险位置。"));
    return GetPawn() == ControlledCharacter && ControlledCharacter->GetCharacterMovement()->IsMovingOnGround();
}

void AHCM1PlayerController::RecoverOnFoot(const FString& Reason)
{
    EndNPCDialogue();
    GetWorldTimerManager().ClearTimer(TransitionTimer);
    ClearGameplayInput();
    PendingVehicle = nullptr;
    if (IsValid(ActiveVehicle)) ActiveVehicle->SetDriver(nullptr);
    ActiveVehicle = nullptr;
    // Leave the transition before recovery: a failed spawn cannot retry every tick.
    PlayerMode = EHCPlayerMode::OnFoot;
    if (!IsValid(ControlledCharacter))
    {
        ControlledCharacter = nullptr;
        UnPossess();
        if (!bRecoverySpawnAttempted && HasAuthority())
        {
            bRecoverySpawnAttempted = true;
            if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode()) GameMode->RestartPlayer(this);
            ControlledCharacter = Cast<AHCM1Character>(GetPawn());
        }
    }
    if (IsValid(ControlledCharacter))
    {
        FTransform Safe;
        if (ResolveSafePlayerTransform(ControlledCharacter->GetActorTransform(), Safe)
            || ResolveSafePlayerTransform(InitialPlayerTransform, Safe))
            ControlledCharacter->SetActorTransform(Safe, false, nullptr, ETeleportType::TeleportPhysics);
        ControlledCharacter->SetSeated(false);
        if (GetPawn() != ControlledCharacter) Possess(ControlledCharacter);
        SetViewTargetWithBlend(ControlledCharacter, 0.2f);
        ShowStatusMessage(Reason);
    }
    else ShowStatusMessage(TEXT("角色恢复失败。请按 P 打开菜单并重新开始。"), 15);
    ApplyInputContexts();
}

void AHCM1PlayerController::MoveInput(const FInputActionValue& Value)
{
    if (IsDialogueOpen()) { EndNPCDialogue(); return; }
    if (PlayerMode == EHCPlayerMode::OnFoot && !bPauseMenuOpen && ControlledCharacter)
    {
        const FVector2D Axis = Value.Get<FVector2D>();
        if (IsFlying()) GetFlightComponent()->SetHorizontalInput(Axis);
        else ControlledCharacter->DoMove(Axis.X, Axis.Y);
    }
}
void AHCM1PlayerController::MoveReleased(const FInputActionValue& Value)
{
    if (GetFlightComponent()) GetFlightComponent()->SetHorizontalInput(FVector2D::ZeroVector);
    if (ControlledCharacter) ControlledCharacter->ConsumeMovementInputVector();
}
FVector2D AHCM1PlayerController::GetOnFootLookDegreesPerActionUnit() const
{
    // The character applies the live legacy Controller scales after LookInput's
    // single shared multiplier. This getter reports that effective Action gain.
    if (GetDefault<UInputSettings>()->bEnableLegacyInputScales)
    {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const FVector2D Gain(GetDeprecatedInputYawScale(), -GetDeprecatedInputPitchScale());
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        return Gain * LookSensitivityScale;
    }
    return FVector2D(1.0f, -1.0f) * LookSensitivityScale;
}

FVector2D AHCM1PlayerController::GetDrivingLookDegreesPerActionUnit() const
{
    return GetOnFootLookDegreesPerActionUnit() * DrivingLookSensitivityRatio;
}

void AHCM1PlayerController::LookInput(const FInputActionValue& Value)
{
    ++LookCallbackCount;
    LastLookAxis = Value.Get<FVector2D>();
    if (!bGameplayFocused) return;
    if (PlayerMode == EHCPlayerMode::OnFoot && !bPauseMenuOpen && ControlledCharacter)
    {
        // Preserve the unscaled Action in LastLookAxis for input diagnostics.
        // Driving applies this same factor once through its effective gain getter.
        const FVector2D Axis = LastLookAxis * LookSensitivityScale * GetCurrentAimLookMultiplier();
        ControlledCharacter->DoLook(Axis.X, -Axis.Y);
    }
    else if (PlayerMode == EHCPlayerMode::Driving && !bPauseMenuOpen && IsValid(ActiveVehicle))
    {
        if (IsFirstPersonPerspective() && GetPerspectiveManager()) GetPerspectiveManager()->AddCockpitLook(LastLookAxis);
        else ActiveVehicle->AddCameraLookInput(LastLookAxis);
    }
}
UHCM5VS2FlightComponent* AHCM1PlayerController::GetFlightComponent() const
{ return IsValid(ControlledCharacter) ? ControlledCharacter->GetFlightComponent() : nullptr; }
bool AHCM1PlayerController::IsFlying() const
{ return GetFlightComponent() && GetFlightComponent()->IsFlying(); }
void AHCM1PlayerController::RequestToggleFlight()
{ if (GetFlightComponent() && GetFlightComponent()->TryToggleFlight()) InteractionTarget.Reset(); }
void AHCM1PlayerController::FlightDescendPressed()
{ if (GetFlightComponent()) GetFlightComponent()->SetDescendHeld(true); }
void AHCM1PlayerController::FlightDescendReleased()
{ if (GetFlightComponent()) GetFlightComponent()->SetDescendHeld(false); }
void AHCM1PlayerController::SprintPressed()
{
    if (IsFlying()) { GetFlightComponent()->SetBoostHeld(true); return; }
    if (PlayerMode == EHCPlayerMode::OnFoot && !bPauseMenuOpen && !IsDialogueOpen() && ControlledCharacter) ControlledCharacter->SetSprinting(true);
}
void AHCM1PlayerController::SprintReleased()
{
    if (GetFlightComponent()) GetFlightComponent()->SetBoostHeld(false);
    if (ControlledCharacter) ControlledCharacter->SetSprinting(false);
}
void AHCM1PlayerController::JumpPressed()
{
    if (IsFlying()) { GetFlightComponent()->SetAscendHeld(true); return; }
    if (PlayerMode == EHCPlayerMode::OnFoot && !bPauseMenuOpen && !IsDialogueOpen() && ControlledCharacter) ControlledCharacter->DoJumpStart();
}
void AHCM1PlayerController::JumpReleased()
{
    if (GetFlightComponent()) GetFlightComponent()->SetAscendHeld(false);
    if (ControlledCharacter) ControlledCharacter->DoJumpEnd();
}
void AHCM1PlayerController::DriveInput(const FInputActionValue& Value)
{
    if (PlayerMode != EHCPlayerMode::Driving || bPauseMenuOpen || !IsValid(ActiveVehicle)) return;
    DriveAxis = Value.Get<FVector2D>();
    ActiveVehicle->SetDriveInput(DriveAxis.Y, DriveAxis.X, bHandbrake);
}
void AHCM1PlayerController::DriveReleased(const FInputActionValue& Value)
{
    DriveAxis = FVector2D::ZeroVector;
    if (IsValid(ActiveVehicle))
    {
        if (PlayerMode == EHCPlayerMode::Driving && !bPauseMenuOpen) ActiveVehicle->SetDriveInput(0, 0, bHandbrake);
        else ActiveVehicle->ClearDriveInput(true);
    }
}
void AHCM1PlayerController::HandbrakePressed()
{
    if (PlayerMode != EHCPlayerMode::Driving || bPauseMenuOpen || !IsValid(ActiveVehicle)) return;
    bHandbrake = true;
    ActiveVehicle->SetDriveInput(DriveAxis.Y, DriveAxis.X, true);
}
void AHCM1PlayerController::HandbrakeReleased()
{
    bHandbrake = false;
    if (PlayerMode == EHCPlayerMode::Driving && !bPauseMenuOpen && IsValid(ActiveVehicle))
        ActiveVehicle->SetDriveInput(DriveAxis.Y, DriveAxis.X, false);
}
void AHCM1PlayerController::RequestVehicleReset()
{
    if (PlayerMode != EHCPlayerMode::Driving || bPauseMenuOpen || !IsValid(ActiveVehicle)) return;
    ClearGameplayInput();
    ShowStatusMessage(ActiveVehicle->TrySafeReset(ControlledCharacter)
        ? TEXT("车辆已安全复位，可以继续驾驶。") : TEXT("复位空间不足，请移开附近障碍后重试。"));
}

void AHCM1PlayerController::TogglePauseMenu()
{
    EndNPCDialogue();
    const bool bWantPause = !bPauseMenuOpen;
    FlushPressedKeys();
    if (!SetPause(bWantPause)) return;
    bPauseMenuOpen = bWantPause;
    bShowMouseCursor = bWantPause;
    if (bWantPause)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
    else
    {
        if (bGameplayFocused) SetInputMode(FInputModeGameOnly());
        if (IsValid(ActiveVehicle)) ActiveVehicle->ResetDrivingCamera(false);
        ApplyInputContexts();
    }
}

FString AHCM1PlayerController::GetLookInputDiagnostics() const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("callback_count"), double(LookCallbackCount));
    Root->SetNumberField(TEXT("last_look_x"), LastLookAxis.X);
    Root->SetNumberField(TEXT("last_look_y"), LastLookAxis.Y);
    float MouseX = 0, MouseY = 0;
    GetInputMouseDelta(MouseX, MouseY);
    Root->SetNumberField(TEXT("mouse_delta_x"), MouseX);
    Root->SetNumberField(TEXT("mouse_delta_y"), MouseY);
    // GetInputMouseDelta above is legacy processed data, not Enhanced Input raw units.
    const FVector RawMouse2D = PlayerInput ? PlayerInput->GetRawVectorKeyValue(EKeys::Mouse2D) : FVector::ZeroVector;
    Root->SetNumberField(TEXT("enhanced_raw_mouse2d_x"), RawMouse2D.X);
    Root->SetNumberField(TEXT("enhanced_raw_mouse2d_y"), RawMouse2D.Y);
    const FVector2D FootGain = GetOnFootLookDegreesPerActionUnit();
    Root->SetNumberField(TEXT("on_foot_yaw_degrees_per_action_unit"), FootGain.X);
    Root->SetNumberField(TEXT("on_foot_pitch_degrees_per_action_unit"), FootGain.Y);
    Root->SetNumberField(TEXT("drive_steer"), DriveAxis.X);
    Root->SetNumberField(TEXT("drive_throttle"), DriveAxis.Y);
    Root->SetBoolField(TEXT("handbrake"), bHandbrake);
    Root->SetBoolField(TEXT("application_active"), bGameplayFocused);
    Root->SetBoolField(TEXT("pause_menu"), bPauseMenuOpen);
    Root->SetBoolField(TEXT("show_mouse_cursor"), bShowMouseCursor);
    Root->SetBoolField(TEXT("ignore_look"), IsLookInputIgnored());
    Root->SetNumberField(TEXT("player_mode"), int32(PlayerMode));
    TArray<TSharedPtr<FJsonValue>> Keys;
    if (const ULocalPlayer* Local = GetLocalPlayer())
        if (const UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            Root->SetBoolField(TEXT("common_context"), Subsystem->HasMappingContext(CommonContext));
            Root->SetBoolField(TEXT("foot_context"), Subsystem->HasMappingContext(OnFootContext));
            Root->SetBoolField(TEXT("driving_context"), Subsystem->HasMappingContext(DrivingContext));
            for (const FKey& Key : Subsystem->QueryKeysMappedToAction(GetM1Action(TEXT("Look"))))
                Keys.Add(MakeShared<FJsonValueString>(Key.ToString()));
        }
    Root->SetArrayField(TEXT("look_mapped_keys"), Keys);
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

void AHCM1PlayerController::RestartPrototype()
{
    if (bPauseMenuOpen) TogglePauseMenu();
    ClearGameplayInput();
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
FString AHCM1PlayerController::GetExperienceTitle() const
{ return GetWorld() && GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS2/")) ? TEXT("海湾漫游 · M5-VS2 港町") : M5Story ? TEXT("海湾漫游 · 夜航") : M3Experience ? TEXT("海湾漫游 · M4-R2") : SceneSettings ? TEXT("海湾漫游 · 海滨街道") : TEXT("海湾漫游"); }
FString AHCM1PlayerController::GetSceneControlHint() const
{ return M5Story ? FString::Printf(TEXT("T  深夜／蓝调时刻（当前：%s）"),SceneSettings && SceneSettings->IsDusk()?TEXT("深夜"):TEXT("蓝调")) : SceneSettings ? FString::Printf(TEXT("T  下午／黄昏（当前：%s）"), SceneSettings->IsDusk() ? TEXT("黄昏") : TEXT("下午")) : FString(); }
void AHCM1PlayerController::QuitPrototype()
{
    ClearGameplayInput();
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
void AHCM1PlayerController::ShowStatusMessage(const FString& Message, float Lifetime)
{
    StatusMessage = Message;
    StatusMessageUntil = GetWorld() ? GetWorld()->GetRealTimeSeconds() + FMath::Max(0.1f, Lifetime) : 0;
    UE_LOG(LogTemp, Display, TEXT("HCM1: %s"), *Message);
}
FString AHCM1PlayerController::GetStatusMessage() const
{ return GetWorld() && GetWorld()->GetRealTimeSeconds() <= StatusMessageUntil ? StatusMessage : FString(); }
FString AHCM1PlayerController::GetInteractionPrompt() const
{
    if (bPauseMenuOpen) return FString();
    if (IsDialogueOpen()) return FString();
    if (IsFlying()) return TEXT("F 平稳降落 · 降落以继续互动与任务");
    if (PlayerMode == EHCPlayerMode::Entering) return TEXT("正在上车…");
    if (PlayerMode == EHCPlayerMode::Exiting) return TEXT("正在检查安全出口…");
    if (PlayerMode == EHCPlayerMode::Driving) return GetSpeedKmh() <= 1 ? TEXT("E  安全下车") : TEXT("停车后按 E 下车");
    AActor* Target = InteractionTarget.Get();
    return Target ? IHCInteractable::Execute_GetInteractionText(Target, const_cast<AHCM1PlayerController*>(this)) : FString();
}
float AHCM1PlayerController::GetSpeedKmh() const
{ return IsValid(ActiveVehicle) ? ActiveVehicle->GetSpeedKmh() : 0; }

bool AHCM1PlayerController::CanUseSave() const
{
    return !bPauseMenuOpen && !IsDialogueOpen() && !IsFlying() && PlayerMode == EHCPlayerMode::OnFoot && IsValid(ControlledCharacter)
        && ControlledCharacter->GetVelocity().Size() < 10
        && ControlledCharacter->GetCharacterMovement()->IsMovingOnGround();
}
AHCM1Vehicle* AHCM1PlayerController::FindSaveVehicle() const
{
    AHCM1Vehicle* Found = nullptr;
    for (TActorIterator<AHCM1Vehicle> It(GetWorld()); It; ++It)
    {
        // This prototype intentionally supports one persistent car.
        if (Found || It->StableId.IsNone()) return nullptr;
        Found = *It;
    }
    return Found;
}
bool AHCM1PlayerController::ResolveSafePlayerTransform(const FTransform& Requested, FTransform& Out, AActor* IgnoreVehicle) const
{
    const double HorizontalLimit = GetSaveHorizontalLimit(GetWorld(), SceneSettings);
    if (!ControlledCharacter || !IsBoundedTransform(Requested, HorizontalLimit)) return false;
    const UCapsuleComponent* Capsule = ControlledCharacter->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(M1SavePlayer), false, ControlledCharacter);
    if (IgnoreVehicle) Params.AddIgnoredActor(IgnoreVehicle);
    const FVector P = Requested.GetLocation();
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, P + FVector(0, 0, 100), P - FVector(0, 0, 400), ECC_Visibility, Params)
        || Ground.ImpactNormal.Z < 0.7f) return false;
    const FVector Safe = Ground.ImpactPoint + FVector(0, 0, HalfHeight + 3);
    if (GetWorld()->OverlapBlockingTestByChannel(Safe, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius + 2, HalfHeight), Params)) return false;
    Out = FTransform(FRotator(0, Requested.Rotator().Yaw, 0), Safe);
    return IsBoundedTransform(Out, HorizontalLimit);
}

void AHCM1PlayerController::SavePressed() { SaveGameNow(); }
bool AHCM1PlayerController::IsDialogueOpen() const
{ return DialogueNPC.IsValid() && DialogueLines.IsValidIndex(DialogueLineIndex); }

void AHCM1PlayerController::BeginNPCDialogue(AHCM3NPC* NPC)
{
    if (!M3Experience || !M3Experience->IsNPCEnabled() || !bGameplayFocused || !CanReachInteraction(NPC)) return;
    TArray<FHCM5DialogueLine> StoryLines;
    const bool bStory = M5Story && M5Story->GetDialogue(NPC->StableId,StoryLines);
    TArray<FString> Lines;
    if (bStory) for (const FHCM5DialogueLine& Line : StoryLines) Lines.Add(Line.Text);
    else Lines = M3Experience->GetDialogueLines(NPC);
    if (Lines.IsEmpty()) return;
    EndNPCDialogue();
    if (!NPC->BeginConversation(ControlledCharacter)) return;
    ClearGameplayInput();
    ControlledCharacter->FaceBodyYawOnce(HCM4Facing::YawToTarget(ControlledCharacter->GetActorLocation(),
        NPC->GetActorLocation(), ControlledCharacter->GetActorRotation().Yaw), .8f);
    DialogueNPC = NPC;
    DialogueLines = MoveTemp(Lines);
    StoryDialogueLines = MoveTemp(StoryLines);
    DialogueLineIndex = 0;
    DialogueLineStartedAt = GetWorld()->GetTimeSeconds(); bDialogueLineRevealed = false;
}

void AHCM1PlayerController::EndNPCDialogue()
{
    const bool bWasOpen = IsDialogueOpen();
    if (AHCM3NPC* NPC = DialogueNPC.Get()) NPC->EndConversation();
    DialogueNPC.Reset(); DialogueLines.Reset(); DialogueLineIndex = 0;
    StoryDialogueLines.Reset(); bDialogueLineRevealed = false;
    if (ControlledCharacter) ControlledCharacter->StopBodyFacing();
    // No mapping removal/addition or sensitivity change on a dialogue transition.
    if (bWasOpen) ClearGameplayInput();
}

void AHCM1PlayerController::AdvanceNPCDialogue()
{
    AHCM3NPC* NPC = DialogueNPC.Get();
    if (!NPC || !CanReachInteraction(NPC)) { EndNPCDialogue(); return; }
    if (!StoryDialogueLines.IsEmpty() && !bDialogueLineRevealed && GetDialogueLine().Len()<DialogueLines[DialogueLineIndex].Len())
    { bDialogueLineRevealed = true; return; }
    if (DialogueLineIndex + 1 < DialogueLines.Num())
    { ++DialogueLineIndex; DialogueLineStartedAt=GetWorld()->GetTimeSeconds(); bDialogueLineRevealed=false; return; }
    if (M5Story && M5Story->OwnsDialogueTarget(NPC->StableId)) M5Story->CompleteDialogue(NPC->StableId,this);
    else if (M3Experience) M3Experience->CompleteDialogue(NPC, this);
    EndNPCDialogue();
}

FString AHCM1PlayerController::GetDialogueName() const
{ return StoryDialogueLines.IsValidIndex(DialogueLineIndex) ? StoryDialogueLines[DialogueLineIndex].Speaker : IsDialogueOpen() ? DialogueNPC->DisplayName : FString(); }
FString AHCM1PlayerController::GetDialogueLine() const
{
    if (!IsDialogueOpen()) return FString();
    const FString& Text=DialogueLines[DialogueLineIndex];
    if (StoryDialogueLines.IsEmpty() || bDialogueLineRevealed) return Text;
    return Text.Left(FMath::Clamp(FMath::FloorToInt((GetWorld()->GetTimeSeconds()-DialogueLineStartedAt)*32.),0,Text.Len()));
}
FString AHCM1PlayerController::GetDialogueAdvanceHint() const
{
    if (!IsDialogueOpen()) return FString();
    if (!StoryDialogueLines.IsEmpty() && GetDialogueLine().Len()<DialogueLines[DialogueLineIndex].Len())
        return TEXT("E 显示完整句    Backspace 结束    P 暂停");
    const FString Advance = DialogueLineIndex + 1 < DialogueLines.Num() ? TEXT("E 下一句")
        : (M5Story && M5Story->OwnsDialogueTarget(DialogueNPC->StableId) ? M5Story->GetDialogueFinishLabel(DialogueNPC->StableId)
           : M3Experience ? M3Experience->GetDialogueFinishLabel(DialogueNPC.Get()) : TEXT("E 结束对话"));
    return FString::Printf(TEXT("%d / %d    %s    Backspace 结束    P 暂停"), DialogueLineIndex + 1, DialogueLines.Num(), *Advance);
}
FString AHCM1PlayerController::GetQuestHUDText() const
{ return M5Story && M5Story->IsTrackingMainStory() ? M5Story->GetQuestHUD() : M3Experience ? M3Experience->GetQuestHUD(this) : FString(); }

void AHCM1PlayerController::LoadPressed() { LoadGameNow(); }
bool AHCM1PlayerController::SaveGameNow()
{
    if (!bSceneSaveIdentityValid) { ShowStatusMessage(TEXT("场景存档标识异常，未写入任何存档。")); return false; }
    if (IsFlying()) { ShowStatusMessage(TEXT("飞行时不能保存，请落地站稳后按 F5。")); return false; }
    if (!CanUseSave()) { ShowStatusMessage(TEXT("请在步行状态站稳后保存；切换和驾驶时不能保存。")); return false; }
    AHCM1Vehicle* Vehicle = FindSaveVehicle();
    if (!Vehicle || Vehicle->HasDriver() || Vehicle->GetSpeedKmh() > 1)
    { ShowStatusMessage(TEXT("车辆未停稳或车辆标识异常，暂时不能保存。")); return false; }
    FTransform Safe;
    const double HorizontalLimit = GetSaveHorizontalLimit(GetWorld(), SceneSettings);
    if (!ResolveSafePlayerTransform(ControlledCharacter->GetActorTransform(), Safe)
        || !IsBoundedTransform(Vehicle->GetActorTransform(), HorizontalLimit))
    { ShowStatusMessage(TEXT("当前位置不适合保存，请移到开阔的平地。")); return false; }
    UHCM1SaveGame* Save = Cast<UHCM1SaveGame>(UGameplayStatics::CreateSaveGameObject(UHCM1SaveGame::StaticClass()));
    if (!Save) return false;
    Save->LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
    Save->PlayerId = SavePlayerId;
    Save->SceneId = SceneSettings ? SceneSettings->SceneId : NAME_None;
    Save->bSceneDusk = SceneSettings && SceneSettings->IsDusk();
    Save->PlayerTransform = Safe;
    Save->VehicleId = Vehicle->StableId;
    Save->VehicleTransform = Vehicle->GetActorTransform();
    for (TActorIterator<AHCM1LightSwitch> It(GetWorld()); It; ++It)
    {
        if (It->StableId.IsNone() || Save->LightStates.Contains(It->StableId))
        { ShowStatusMessage(TEXT("开关标识异常，存档未写入。")); return false; }
        Save->LightStates.Add(It->StableId, It->IsLightEnabled());
    }
    FString M3Error;
    if (M3Experience && !M3Experience->FillSave(Save, M3Error)) { ShowStatusMessage(M3Error); return false; }
    if (M5Story) Save->M5Story=M5Story->ExportState();
    if (M3Experience && GetCombatComponent()) GetCombatComponent()->FillSave(Save);
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
    ShowStatusMessage(bSaved ? TEXT("保存成功。退出并重新启动后，可按 F9 读取。") : TEXT("保存失败，原有存档未主动删除。"));
    return bSaved;
}

bool AHCM1PlayerController::LoadGameNow()
{
    if (!bSceneSaveIdentityValid) { ShowStatusMessage(TEXT("场景存档标识异常，保留当前游戏。")); return false; }
    if (!CanUseSave()) { ShowStatusMessage(TEXT("请先下车并站稳，再读取存档。")); return false; }
    if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
    { ShowStatusMessage(TEXT("当前场景尚无存档，继续当前游戏。")); return false; }
    UHCM1SaveGame* Save = Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
    AHCM1Vehicle* Vehicle = FindSaveVehicle();
    const double HorizontalLimit = GetSaveHorizontalLimit(GetWorld(), SceneSettings);
    if (!Save || Save->Version != UHCM1SaveGame::CurrentVersion || Save->PlayerId != SavePlayerId
        || Save->SceneId != (SceneSettings ? SceneSettings->SceneId : NAME_None)
        || Save->LevelName != UGameplayStatics::GetCurrentLevelName(this, true)
        || !Vehicle || Vehicle->StableId != Save->VehicleId || Vehicle->HasDriver() || Vehicle->GetSpeedKmh() > 1
        || !IsBoundedTransform(Save->PlayerTransform, HorizontalLimit)
        || !IsBoundedTransform(Save->VehicleTransform, HorizontalLimit))
    { ShowStatusMessage(TEXT("存档不兼容或车辆未停稳，保留当前游戏。")); return false; }
    TMap<FName, AHCM1LightSwitch*> Lights;
    for (TActorIterator<AHCM1LightSwitch> It(GetWorld()); It; ++It)
    {
        if (It->StableId.IsNone() || Lights.Contains(It->StableId) || !Save->LightStates.Contains(It->StableId))
        { ShowStatusMessage(TEXT("存档中的开关与场景不一致，保留当前游戏。")); return false; }
        Lights.Add(It->StableId, *It);
    }
    if (Lights.Num() != Save->LightStates.Num())
    { ShowStatusMessage(TEXT("存档中的开关与场景不一致，保留当前游戏。")); return false; }
    FString M3Error;
    if (M3Experience && !M3Experience->ValidateSave(Save, M3Error)) { ShowStatusMessage(M3Error); return false; }
    if (M5Story && !M5Story->ValidateState(Save->M5Story,M3Error)) { ShowStatusMessage(M3Error); return false; }
    if (M3Experience && GetCombatComponent() && !GetCombatComponent()->ValidateSave(Save))
    { ShowStatusMessage(TEXT("存档武器或弹药数据无效，保留当前游戏。")); return false; }
    FTransform SafePlayer;
    if (!ResolveSafePlayerTransform(Save->PlayerTransform, SafePlayer, Vehicle))
    { ShowStatusMessage(TEXT("存档中的玩家位置受阻，保留当前游戏。")); return false; }
    // Reject a capsule intersecting the car's future volume before changing either actor.
    const FVector FutureLocal = Save->VehicleTransform.InverseTransformPosition(SafePlayer.GetLocation());
    const FBox LocalBounds = Vehicle->CalculateComponentsBoundingBoxInLocalSpace();
    const float Radius = ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() + 8;
    const float HalfHeight = ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (LocalBounds.ExpandBy(FVector(Radius, Radius, HalfHeight)).IsInsideOrOn(FutureLocal))
    { ShowStatusMessage(TEXT("存档中的玩家与车辆位置冲突，保留当前游戏。")); return false; }
    const FTransform PreviousVehicle = Vehicle->GetActorTransform();
    if (!Vehicle->RestoreSavedTransform(Save->VehicleTransform, ControlledCharacter))
    { ShowStatusMessage(TEXT("存档中的车辆位置不安全，保留当前游戏。")); return false; }
    FTransform Rechecked;
    if (!ResolveSafePlayerTransform(SafePlayer, Rechecked))
    {
        Vehicle->RestoreSavedTransform(PreviousVehicle, ControlledCharacter);
        ShowStatusMessage(TEXT("读取位置检查未通过，玩家仍在原处。"));
        return false;
    }
    FlushPressedKeys();
    if (M3Experience && !M3Experience->ApplySave(Save, Vehicle, M3Error))
    {
        Vehicle->RestoreSavedTransform(PreviousVehicle, ControlledCharacter);
        ShowStatusMessage(M3Error); return false;
    }
    ControlledCharacter->SetActorTransform(Rechecked, false, nullptr, ETeleportType::TeleportPhysics);
    ControlledCharacter->SetSeated(false);
    SetControlRotation(Rechecked.Rotator());
    for (const auto& Pair : Lights) Pair.Value->SetLightEnabled(Save->LightStates.FindChecked(Pair.Key));
    if (SceneSettings) SceneSettings->SetDuskPreset(Save->bSceneDusk);
    if (M3Experience && GetCombatComponent()) GetCombatComponent()->RestoreSave(Save);
    if (M5Story && !M5Story->ImportState(Save->M5Story,M3Error)) { ShowStatusMessage(M3Error); return false; }
    ShowStatusMessage(M3Experience ? TEXT("读取成功：玩家、车辆、照明、任务和路人状态已恢复。")
        : TEXT("读取成功：玩家、车辆和照明状态已恢复。"));
    return true;
}

UHCM4CombatComponent* AHCM1PlayerController::GetCombatComponent() const
{ return IsValid(ControlledCharacter) ? ControlledCharacter->GetCombatComponent() : nullptr; }
float AHCM1PlayerController::GetCurrentAimLookMultiplier() const
{ const UHCM4CombatComponent* Combat = GetCombatComponent(); return Combat ? Combat->GetAimLookMultiplier() : 1.f; }
void AHCM1PlayerController::AttackPressed() { if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->RequestAttack(); }
void AHCM1PlayerController::AimPressed() { if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->SetAimHeld(true); }
void AHCM1PlayerController::AimReleased() { if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->SetAimHeld(false); }
void AHCM1PlayerController::ToggleWeaponPressed() { if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->RequestToggleWeapon(); }
void AHCM1PlayerController::ReloadPressed() { if (UHCM4CombatComponent* Combat = GetCombatComponent()) Combat->RequestReload(); }
