#include "HCM5VS2DrivingHandsReviewDirector.h"
#include "HCM5VS2DrivingHandsComponent.h"
#include "HCM5VS2HeroModestyComponent.h"
#include "HCM5VS2CornerTimeDirector.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Vehicle.h"
#include "M3/HCM3Experience.h"
#include "M4/HCM4CombatComponent.h"
#include "M4R2/HCM4R2CockpitComponent.h"
#include "M4R2/HCM4R2PresentationComponent.h"
#include "M4R2/HCM4R2FirstPersonMesh.h"
#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
namespace
{
enum EStage { Warm,Enter,DriverFP,Center,ParkLeft,ParkRight,ParkCenter,MoveLeft,BrakeLeft,MoveRight,BrakeRight,DriverTP,Exit,FootFP,UnarmedLow,PistolLow,Restore };
const TCHAR* StageName(int32 I){const TCHAR* N[]={TEXT("Warmup"),TEXT("RealEEnter"),TEXT("RealVDriverFP"),TEXT("Center"),TEXT("ParkLeft"),TEXT("ParkRight"),TEXT("ParkCenter"),TEXT("LowSpeedWA"),TEXT("BrakeLeft"),TEXT("LowSpeedWD"),TEXT("BrakeRight"),TEXT("RealVDriverTP"),TEXT("RealEExit"),TEXT("RealVFootFP"),TEXT("UnarmedLow"),TEXT("RealQPistolLow"),TEXT("RealQRestore")};return I>=0&&I<UE_ARRAY_COUNT(N)?N[I]:TEXT("Unknown");}
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V){return{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
TArray<TSharedPtr<FJsonValue>> Rows(const TArray<TSharedPtr<FJsonObject>>& O){TArray<TSharedPtr<FJsonValue>> R;for(const auto& X:O)R.Add(MakeShared<FJsonValueObject>(X));return R;}
TSharedPtr<FJsonObject> Parsed(const FString& S){TSharedPtr<FJsonObject> J;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),J);return J?J:MakeShared<FJsonObject>();}
bool PNG(const FString& File){TArray<uint8> B;if(!FFileHelper::LoadFileToArray(B,*File)||B.Num()<24)return false;const uint8 M[]={137,80,78,71,13,10,26,10};for(int32 I=0;I<8;++I)if(B[I]!=M[I])return false;auto BE=[&](int32 I){return(uint32(B[I])<<24)|(uint32(B[I+1])<<16)|(uint32(B[I+2])<<8)|uint32(B[I+3]);};return BE(16)==1920&&BE(20)==1080;}
}
AHCM5VS2DrivingHandsReviewDirector::AHCM5VS2DrivingHandsReviewDirector()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.bTickEvenWhenPaused=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AHCM5VS2DrivingHandsReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if(!FParse::Param(FCommandLine::Get(),TEXT("M5VS2DrivingHandsReview")))return;
    const FString Map=GetWorld()->GetOutermost()->GetName(),Prefix=TEXT("/Game/HarborCity/M5VS2/DrivingHandsReview/Run_");
    if(!Map.StartsWith(Prefix)||!(ExpectedPeriod==TEXT("Afternoon")||ExpectedPeriod==TEXT("Night"))||Map!=Prefix+Map.Mid(Prefix.Len(),12)+TEXT("/L_DrivingHands_")+ExpectedPeriod.ToString())return;
    for(TCHAR C:Map.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return;
    FString Root;if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root)||FPaths::IsRelative(Root))return;
    Root=FPaths::ConvertRelativePathToFull(Root);FPaths::NormalizeDirectoryName(Root);if(!FPaths::CollapseRelativeDirectories(Root)||!FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2")))return;
    Directory=Root/(TEXT("DrivingHandsReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));if(!IFileManager::Get().MakeDirectory(*Directory,true))return;
    bEnabled=true;StartedWall=StageWall=FPlatformTime::Seconds();bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));SetActorTickEnabled(true);
    if(auto* V=GetWorld()->GetGameViewport()){Viewport=V;InputHandle=V->OnInputKey().AddUObject(this,&AHCM5VS2DrivingHandsReviewDirector::ObserveInput);}
    if(FSlateApplication::IsInitialized())ActivationHandle=FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this,&AHCM5VS2DrivingHandsReviewDirector::ObserveActivation);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2DrivingHandsReviewDirector::ScreenshotProcessed);Write();
#endif
}
bool AHCM5VS2DrivingHandsReviewDirector::Check(bool Good,const FString& Name,const FString& Why)
{
    if(Good)for(auto& C:Checks)if(C->GetStringField(TEXT("name"))==Name&&C->GetStringField(TEXT("stage"))==StageName(Stage))return true;
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Name);J->SetStringField(TEXT("stage"),StageName(Stage));J->SetStringField(TEXT("status"),Good?TEXT("PASS"):TEXT("FAIL"));J->SetStringField(TEXT("detail"),Why);Checks.Add(J);if(!Good)Finish(TEXT("FAIL"),Name+TEXT(": ")+Why);return Good;
}
bool AHCM5VS2DrivingHandsReviewDirector::ReadLookChain()
{
    auto* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);const UInputAction* Action=PC->GetM1Action(TEXT("Look"));if(!Input||!Action||Action->ValueType!=EInputActionValueType::Axis2D)return false;
    int32 Count=0;bool Valid=true;MappingScale=FVector2D(1,1);
    auto Add=[&](const TArray<TObjectPtr<UInputModifier>>& Mods){for(const UInputModifier* M:Mods){if(const auto* S=Cast<UInputModifierScalar>(M))MappingScale*=FVector2D(S->Scalar.X,S->Scalar.Y);else Valid=false;}};
    for(const FEnhancedActionKeyMapping& M:Input->GetEnhancedActionMappingsView())if(M.Action==Action){++Count;Valid&=M.Key==EKeys::Mouse2D;Add(M.Modifiers);}
    const auto* Instance=Input->FindActionInstanceData(Action);Add(Instance?Instance->GetModifiers():Action->Modifiers);
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),StageName(Stage));J->SetNumberField(TEXT("mapping_count"),Count);J->SetArrayField(TEXT("raw_to_action_xy"),XYZ(FVector(MappingScale.X,MappingScale.Y,0)));J->SetObjectField(TEXT("controller"),Parsed(PC->GetLookInputDiagnostics()));LookChains.Add(J);
    return Count==1&&Valid&&FMath::IsFinite(MappingScale.X)&&FMath::IsFinite(MappingScale.Y)&&FMath::Abs(MappingScale.X)>.00001&&FMath::Abs(MappingScale.Y)>.00001;
}
bool AHCM5VS2DrivingHandsReviewDirector::Initialize()
{
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));Character=PC?PC->GetControlledCharacter():nullptr;
    if(!PC||!Character||!Viewport.IsValid()||!Viewport->Viewport||!PC->IsGameplayFocused()||PC->IsPauseMenuOpen())return false;bHadFocus=true;
    if(!Character->GetCharacterMovement()->IsMovingOnGround())return false;
    int32 Count=0;for(TActorIterator<AHCM1Vehicle> It(GetWorld());It;++It){Vehicle=*It;++Count;}
    if(!Check(Count==1&&Vehicle&&Vehicle->GetClass()==ExpectedVehicleClass.Get(),TEXT("one_exact_candidate_vehicle"),GetPathNameSafe(Vehicle)))return false;
    Hands=Vehicle->FindComponentByClass<UHCM5VS2DrivingHandsComponent>();auto* Experience=PC->GetM3Experience();
    if(!Check(Hands&&Experience&&Experience->NPCs.IsEmpty()&&!Experience->PlayerAppearanceMesh&&!Experience->PlayerAnimationClass,TEXT("empty_existing_combat_fixture_no_appearance_override"),GetPathNameSafe(Experience))||!Check(BindingValid(),TEXT("actual_selected_Hero_binding"),TEXT("Body/ABP/materials and safety-layer component")))return false;
    const FString Slot=PC->GetSaveSlotName();if(!Check(Slot.StartsWith(TEXT("HarborCity_VS2_DrivingHands_Test_"))&&!UGameplayStatics::DoesSaveGameExist(Slot,0),TEXT("fresh_isolated_test_slot"),Slot))return false;
    if(!Check(PC->GetPlayerMode()==EHCPlayerMode::OnFoot&&!PC->IsFirstPersonPerspective()&&PC->CanReachInteraction(Vehicle),TEXT("natural_ground_start_at_actual_driver_door"),PC->GetInteractionPrompt())||!Check(ReadLookChain(),TEXT("actual_mouse_mapping_chain"),TEXT("No Action injection shortcut")))return false;
    VehicleOrigin=Vehicle->GetActorLocation();CallbacksBefore=PC->GetLookCallbackCount();AddTickPrerequisiteComponent(Hands);AddTickPrerequisiteComponent(Character->GetMesh());
    PC->ShowStatusMessage(TEXT("驾驶手检查 · ENGINE_INPUT_NOT_OS；原 E/V/键鼠输入，Esc/P 停止"),60.f);return true;
}
bool AHCM5VS2DrivingHandsReviewDirector::BindingValid() const
{
    if(!Character||!Vehicle||!PC||Character->GetClass()!=ExpectedCharacterClass.Get()||Vehicle->GetClass()!=ExpectedVehicleClass.Get())return false;
    auto* B=Character->GetMesh();if(!B||B->GetSkeletalMeshAsset()!=ExpectedBody||!B->GetAnimInstance()||B->GetAnimInstance()->GetClass()!=ExpectedAnimationClass.Get()||B->GetNumMaterials()!=ExpectedMaterials.Num())return false;
    for(int32 I=0;I<ExpectedMaterials.Num();++I)if(B->GetMaterial(I)!=ExpectedMaterials[I])return false;
    return Character->FindComponentByClass<UHCM5VS2HeroModestyComponent>()!=nullptr;
}
bool AHCM5VS2DrivingHandsReviewDirector::MaterialsReady(FString& Failure)
{
    ShaderState=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonObject>> Slots;bool Ready=true;TArray<USkinnedMeshComponent*> Meshes;Meshes.Add(Character->GetMesh());
    for(AActor* MeshOwner:{static_cast<AActor*>(Character.Get()),static_cast<AActor*>(Vehicle.Get())})
    {TArray<USkinnedMeshComponent*> Found;MeshOwner->GetComponents(Found);for(auto* M:Found)if(M->IsVisible()&&!M->bHiddenInGame)Meshes.AddUnique(M);}
    const EShaderPlatform Platform=GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel());
    for(auto* Mesh:Meshes)for(int32 I=0;I<Mesh->GetNumMaterials();++I)
    {
        auto* M=Mesh->GetMaterial(I);auto* R=M?M->GetMaterialResource(Platform):nullptr;bool Compiling=M&&M->IsCompiling();TArray<TSharedPtr<FJsonValue>> Errors;
#if WITH_EDITOR
        if(R){Compiling|=!R->IsCompilationFinished();for(const auto& E:R->GetCompileErrors())if(Errors.Num()<8)Errors.Add(MakeShared<FJsonValueString>(E.Left(2048)));if(!R->IsGameThreadShaderMapComplete()&&Errors.IsEmpty())R->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);}
#endif
        const auto* Map=R?R->GetGameThreadShaderMap():nullptr;const auto* SK=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset());const bool NeedMorph=SK&&SK->GetMorphTargets().Num()>0;
        // GPUSkinVertexFactory accepts special engine materials for both skin
        // and morph permutations without ordinary material usage flags.
        const bool Special=R&&R->IsSpecialEngineMaterial();
        const bool SkinUsage=M&&M->GetUsageByFlag(MATUSAGE_SkeletalMesh);
        const bool MorphUsage=M&&M->GetUsageByFlag(MATUSAGE_MorphTargets);
        const bool Usage=M&&(Special||(SkinUsage&&(!NeedMorph||MorphUsage)));
        const bool Failed=!Errors.IsEmpty()||(Map&&Map->IsCompilationFinalized()&&!Map->CompiledSuccessfully());const bool Good=Usage&&R&&!Compiling&&!Failed&&Map&&Map->IsValidForRendering()&&R->IsGameThreadShaderMapComplete();Ready&=Good;
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("component"),Mesh->GetName());J->SetNumberField(TEXT("slot"),I);J->SetStringField(TEXT("material"),GetPathNameSafe(M));J->SetBoolField(TEXT("GT_ready"),Good);J->SetBoolField(TEXT("usage_valid"),Usage);J->SetBoolField(TEXT("special_engine_material"),Special);J->SetBoolField(TEXT("raw_skin_usage"),SkinUsage);J->SetBoolField(TEXT("raw_morph_usage"),MorphUsage);J->SetArrayField(TEXT("compile_errors"),Errors);Slots.Add(J);if(!Usage||(Failed&&!Compiling))Failure=TEXT("Required actual skin/morph material compilation invalid");
    }
    ShaderState->SetArrayField(TEXT("slots"),Rows(Slots));ShaderState->SetStringField(TEXT("scope"),TEXT("Public GT readiness; visual intersections/fallback require PNG review"));return Ready;
}
void AHCM5VS2DrivingHandsReviewDirector::Key(const FKey& K,bool Pressed)
{
    if(!PC||(Pressed&&(bDone||bStopped))||Held.Contains(K)==Pressed)return;PC->InputKey(FInputKeyEventArgs(nullptr,IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(),K,Pressed?IE_Pressed:IE_Released,0u));if(Pressed)Held.Add(K);else Held.Remove(K);
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("key"),K.ToString());J->SetBoolField(TEXT("pressed"),Pressed);J->SetStringField(TEXT("stage"),StageName(Stage));J->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());J->SetNumberField(TEXT("frame"),double(GFrameCounter));Events.Add(J);
}
void AHCM5VS2DrivingHandsReviewDirector::Pulse(const FKey& K){Key(K,true);Pulsed.AddUnique(K);}
void AHCM5VS2DrivingHandsReviewDirector::ReleaseOwned(){const auto Keys=Held.Array();for(const auto& K:Keys)Key(K,false);Pulsed.Reset();}
void AHCM5VS2DrivingHandsReviewDirector::Look(float Pitch,float Yaw,float Dt)
{
    if(!PC||!PC->PlayerCameraManager||bStopped||bDone)return;const FRotator Before=PC->PlayerCameraManager->GetCameraRotation();
    const FVector2D D(FMath::Clamp(float(FMath::FindDeltaAngleDegrees(Before.Yaw,double(Yaw)))*4.f,-70.f,70.f)*Dt,FMath::Clamp(float(FMath::FindDeltaAngleDegrees(Before.Pitch,double(Pitch)))*4.f,-45.f,45.f)*Dt);
    const FVector2D Gain=MappingScale*(PC->GetPlayerMode()==EHCPlayerMode::Driving?PC->GetDrivingLookDegreesPerActionUnit():PC->GetOnFootLookDegreesPerActionUnit());if(D.IsNearlyZero(.0001))return;
    const FVector2D Raw(D.X/Gain.X,D.Y/Gain.Y);const auto Device=IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();PC->InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseX,float(Raw.X),Dt,1,0u));PC->InputKey(FInputKeyEventArgs(nullptr,Device,EKeys::MouseY,float(Raw.Y),Dt,1,0u));RawMouseSum+=Raw;
}
void AHCM5VS2DrivingHandsReviewDirector::Advance(int32 Next)
{
    ReleaseOwned();Stage=Next;StageWall=FPlatformTime::Seconds();bThrottled=false;
    switch(Stage){case Enter:Pulse(EKeys::E);break;case DriverFP:if(!PC->IsFirstPersonPerspective())Pulse(EKeys::V);break;case Center:if(!Check(ReadLookChain(),TEXT("driving_actual_mouse_chain"),TEXT("Current mapping after possession")))return;break;
        case ParkLeft:Key(EKeys::SpaceBar,true);Key(EKeys::A,true);break;case ParkRight:Key(EKeys::SpaceBar,true);Key(EKeys::D,true);break;case ParkCenter:case BrakeLeft:case BrakeRight:Key(EKeys::SpaceBar,true);break;
        case MoveLeft:Key(EKeys::W,true);Key(EKeys::A,true);break;case MoveRight:Key(EKeys::W,true);Key(EKeys::D,true);break;case DriverTP:Pulse(EKeys::V);break;case Exit:Pulse(EKeys::E);break;case FootFP:if(!PC->IsFirstPersonPerspective())Pulse(EKeys::V);break;
        case UnarmedLow:FootYaw=PC->PlayerCameraManager->GetCameraRotation().Yaw;if(!Check(ReadLookChain(),TEXT("restored_foot_actual_mouse_chain"),TEXT("Current mapping after exit")))return;break;case PistolLow:case Restore:Pulse(EKeys::Q);break;default:break;}
    Write();
}
TSharedPtr<FJsonObject> AHCM5VS2DrivingHandsReviewDirector::Observe() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("frame"),double(GFrameCounter));J->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedWall);J->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());J->SetStringField(TEXT("stage"),StageName(Stage));if(!PC||!Character||!Vehicle)return J;
    J->SetNumberField(TEXT("player_mode"),int32(PC->GetPlayerMode()));J->SetBoolField(TEXT("first_person"),PC->IsFirstPersonPerspective());J->SetStringField(TEXT("pawn"),GetPathNameSafe(PC->GetPawn()));J->SetStringField(TEXT("view_target"),GetPathNameSafe(PC->GetViewTarget()));J->SetBoolField(TEXT("character_actor_hidden"),Character->IsHidden());J->SetStringField(TEXT("vehicle_world"),Vehicle->GetActorTransform().ToString());J->SetNumberField(TEXT("speed_kmh"),Vehicle->GetSpeedKmh());J->SetNumberField(TEXT("actual_wheel_degrees"),Vehicle->GetCockpitComponent()->GetSteeringWheelAngleDegrees());J->SetObjectField(TEXT("hands"),Parsed(Hands->GetDrivingHandsDiagnostics()));
    if(PC->PlayerCameraManager){J->SetArrayField(TEXT("PCM_position_cm"),XYZ(PC->PlayerCameraManager->GetCameraLocation()));J->SetStringField(TEXT("PCM_rotation"),PC->PlayerCameraManager->GetCameraRotation().ToString());J->SetNumberField(TEXT("PCM_FOV"),PC->PlayerCameraManager->GetFOVAngle());}
    if(auto* M=Character->FindComponentByClass<UHCM5VS2HeroModestyComponent>())J->SetObjectField(TEXT("modesty"),Parsed(M->GetModestyDiagnostics()));
    if(auto* P=Character->GetR2PresentationComponent()){J->SetObjectField(TEXT("foot_geometry"),Parsed(P->GetGeometryDiagnostics()));J->SetObjectField(TEXT("foot_pose"),Parsed(P->GetLocomotionPresentationDiagnostics()));}
    if(auto* C=PC->GetCombatComponent()){J->SetNumberField(TEXT("weapon_mode"),int32(C->GetWeaponMode()));J->SetNumberField(TEXT("shots"),C->GetShotCount());J->SetBoolField(TEXT("combat_enabled"),C->IsCombatEnabled());}
    for(TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld());It;++It)J->SetObjectField(TEXT("lighting"),Parsed(It->GetLightingDiagnostics()));
    J->SetObjectField(TEXT("look_chain_observation"),Parsed(PC->GetLookInputDiagnostics()));return J;
}
void AHCM5VS2DrivingHandsReviewDirector::Capture(const FString& Label,int32 Next)
{
    if(Pending||bDone||bStopped)return;FString Failure;if(!MaterialsReady(Failure)){if(!Failure.IsEmpty())Finish(TEXT("FAIL"),Failure);return;}
    Pending=Observe();PendingPNG=Directory/FString::Printf(TEXT("%02d_%s.png"),Captures.Num(),*Label);if(IFileManager::Get().FileExists(*PendingPNG)){Finish(TEXT("FAIL"),TEXT("Refuse existing screenshot"));return;}
    Pending->SetStringField(TEXT("file"),PendingPNG);Pending->SetStringField(TEXT("label"),Label);Pending->SetStringField(TEXT("art"),TEXT("USER_REVIEW"));Pending->SetStringField(TEXT("frame_scope"),TEXT("Request observation precedes render/processed callback. Real movement not frozen; compare both, no exact GPU-frame claim."));PendingNext=Next;RequestFrame=GFrameCounter;ShotWall=FPlatformTime::Seconds();bShotProcessed=false;FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
    // Moving captures initiate ordinary braking immediately after the request;
    // processed callback records the actual later state instead of posing it.
    if(Stage==MoveLeft||Stage==MoveRight){Key(EKeys::W,false);Key(EKeys::A,false);Key(EKeys::D,false);Key(EKeys::SpaceBar,true);}
}
void AHCM5VS2DrivingHandsReviewDirector::ScreenshotProcessed(){if(Pending&&!bStopped){bShotProcessed=true;Pending->SetNumberField(TEXT("processed_frame"),double(GFrameCounter));Pending->SetObjectField(TEXT("processed_observation"),Observe());}}
void AHCM5VS2DrivingHandsReviewDirector::Tick(float Dt)
{
    Super::Tick(Dt);if(!bEnabled||bStopped)return;const double Now=FPlatformTime::Seconds();if(bDone){if(bAutoQuit&&Now-FinishedWall>.5&&!FScreenshotRequest::IsScreenshotRequested()){bAutoQuit=false;FPlatformMisc::RequestExitWithStatus(false,!bWriteFailed&&Status==TEXT("PASS_CAPTURE_ONLY")?0:1,TEXT("Driving hand review finished"));}return;}
    if(Now-StartedWall>180){Finish(TEXT("FAIL"),TEXT("180 second total bound"));return;}if(!bInitialized){bInitialized=Initialize();if(!bInitialized&&!bDone&&Now-StartedWall>15)Finish(TEXT("NOT_RUN"),TEXT("Valid focused natural ground fixture unavailable"));return;}
    if(!PC->IsGameplayFocused()||PC->IsPauseMenuOpen()||UGameplayStatics::IsGamePaused(this)){Finish(TEXT("USER_ABORTED"),TEXT("Pause/focus loss; no resume"),true);return;}
    const auto Pulses=Pulsed;Pulsed.Reset();for(const auto& K:Pulses)Key(K,false);
    // Spawned chassis settles vertically under unchanged vehicle physics before
    // the driving sequence. The driving speed limit must not reject that fall.
    // Warmup still has its existing deadline and must reach full speed <0.2 km/h.
    const bool SafeMotion=Stage==Warm
        ? Vehicle->GetVelocity().Size2D()*.036f<12.f&&FMath::Abs(Vehicle->GetActorLocation().Z-VehicleOrigin.Z)<250.f
        : Vehicle->GetSpeedKmh()<12.f;
    if(!Check(BindingValid(),TEXT("selected_body_animation_materials_unchanged"),TEXT("No fixture appearance override"))||!Check(SafeMotion&&FVector::Dist2D(VehicleOrigin,Vehicle->GetActorLocation())<700,TEXT("bounded_real_low_speed_car"),Vehicle->GetDriveTelemetry()))return;
    if(Now>=NextSample){NextSample=Now+.1;if(Samples.Num()>=1500){Finish(TEXT("FAIL"),TEXT("Sample bound"));return;}Samples.Add(Observe());}
    if(Pending){if(bShotProcessed&&!FScreenshotRequest::IsScreenshotRequested()&&GFrameCounter>RequestFrame){if(!PNG(PendingPNG)){Finish(TEXT("FAIL"),TEXT("Missing native 1920x1080 PNG"));return;}Pending->SetStringField(TEXT("capture_status"),TEXT("PASS"));Captures.Add(Pending);Pending.Reset();if(!Write())return;Advance(PendingNext);}else if(Now-ShotWall>15)Finish(TEXT("FAIL"),TEXT("Screenshot deadline"));return;}
    const double Age=Now-StageWall;const float Angle=Vehicle->GetCockpitComponent()->GetSteeringWheelAngleDegrees(),Speed=Vehicle->GetSpeedKmh();const bool Driving=PC->GetPlayerMode()==EHCPlayerMode::Driving;
    if(Stage!=Warm&&Age>12){Finish(TEXT("FAIL"),FString(TEXT("Stage deadline: "))+StageName(Stage));return;}
    if(Stage>=Center&&Stage<=BrakeRight)
    {if(!Check(Driving&&PC->IsFirstPersonPerspective()&&PC->GetActiveVehicle()==Vehicle,TEXT("real_driver_FP_possession"),TEXT("Original E/V transition only")))return;const auto H=Parsed(Hands->GetDrivingHandsDiagnostics());if(!H->GetStringField(TEXT("failure")).IsEmpty()){Finish(TEXT("FAIL"),H->GetStringField(TEXT("failure")));return;}if(!H->GetBoolField(TEXT("active"))){if(Age>3)Finish(TEXT("FAIL"),TEXT("Actual driver hands never became active"));return;}}
    if(Stage==Warm){if(Now>=NextShader){NextShader=Now+.5;FString F;bWarmReady=MaterialsReady(F);if(!F.IsEmpty()){Finish(TEXT("FAIL"),F);return;}}if(Age>120){Finish(TEXT("FAIL"),TEXT("Warm material deadline"));return;}if(bWarmReady&&Age>2&&Speed<.2)Advance(Enter);return;}
    if(Stage==Enter){if(Driving&&PC->GetPawn()==Vehicle&&Age>.5)Advance(DriverFP);return;}
    if(Stage==DriverFP){if(Driving&&PC->IsFirstPersonPerspective()&&Age>.5)Advance(Center);return;}
    if(Stage==Center){Look(-18,Vehicle->GetActorRotation().Yaw,Dt);if(Age>2&&FMath::Abs(FRotator::NormalizeAxis(PC->PlayerCameraManager->GetCameraRotation().Pitch)+18)<1.5)Capture(TEXT("DriverCenter"),ParkLeft);return;}
    if(Stage==ParkLeft){if(!Check(Speed<.5,TEXT("stationary_left"),TEXT("Real A + Space handbrake")))return;if(Age>2){LeftWheel=Angle;if(Check(FMath::Abs(Angle)>30,TEXT("actual_left_wheel_response"),FString::SanitizeFloat(Angle)))Capture(TEXT("ActualParkLeft"),ParkRight);}return;}
    if(Stage==ParkRight){if(!Check(Speed<.5,TEXT("stationary_right"),TEXT("Real D + Space handbrake")))return;if(Age>2.5){RightWheel=Angle;if(Check(Angle*LeftWheel<0&&FMath::Abs(Angle)>30,TEXT("actual_opposite_wheel_response"),FString::SanitizeFloat(Angle)))Capture(TEXT("ActualParkRight"),ParkCenter);}return;}
    if(Stage==ParkCenter){if(Age>1&&FMath::Abs(Angle)<5&&Speed<.2)Capture(TEXT("ActualWheelReturn"),MoveLeft);return;}
    if(Stage==MoveLeft||Stage==MoveRight){if(!bThrottled&&(Speed>4||Age>.8)){Key(EKeys::W,false);bThrottled=true;}if(Age>.4&&Speed>1&&FMath::Abs(Angle)>25)Capture(Stage==MoveLeft?TEXT("ActualSlowWA"):TEXT("ActualSlowWD"),Stage==MoveLeft?BrakeLeft:BrakeRight);return;}
    if(Stage==BrakeLeft||Stage==BrakeRight){if(Age>.7&&Speed<.2&&FMath::Abs(Angle)<5)Advance(Stage==BrakeLeft?MoveRight:DriverTP);return;}
    if(Stage==DriverTP){if(Age>.7&&!PC->IsFirstPersonPerspective()){if(Check(!Parsed(Hands->GetDrivingHandsDiagnostics())->GetBoolField(TEXT("active")),TEXT("driving_hands_hidden_in_third_person"),TEXT("V result")))Capture(TEXT("DriverThirdPersonHidden"),Exit);}return;}
    if(Stage==Exit){if(PC->GetPlayerMode()==EHCPlayerMode::OnFoot&&PC->GetPawn()==Character&&Age>.6){if(Check(!Character->IsHidden()&&!Parsed(Hands->GetDrivingHandsDiagnostics())->GetBoolField(TEXT("active")),TEXT("E_exit_hides_driver_hands_restores_character"),TEXT("Original exit placement")))Advance(FootFP);}return;}
    if(Stage==FootFP){if(PC->IsFirstPersonPerspective()&&Age>.5)Advance(UnarmedLow);return;}
    auto* Combat=PC->GetCombatComponent();if(Stage>=UnarmedLow&&!Check(Combat&&Combat->CanUseCombat()&&Combat->GetShotCount()==0,TEXT("normal_Q_presentation_only_no_shots"),TEXT("Empty existing Experience enables Q; no damage or quest test")))return;
    if(Stage==UnarmedLow||Stage==PistolLow){Look(-55,FootYaw,Dt);const auto Expected=Stage==UnarmedLow?EHCM4WeaponMode::Unarmed:EHCM4WeaponMode::Pistol;if(Age>2&&Combat->GetWeaponMode()==Expected&&FMath::Abs(FRotator::NormalizeAxis(PC->PlayerCameraManager->GetCameraRotation().Pitch)+55)<1.5)Capture(Stage==UnarmedLow?TEXT("FootUnarmedLow"):TEXT("FootPistolLow"),Stage==UnarmedLow?PistolLow:Restore);return;}
    if(Stage==Restore){Look(0,FootYaw,Dt);if(Age>2&&Combat->GetWeaponMode()==EHCM4WeaponMode::Unarmed&&FMath::Abs(FRotator::NormalizeAxis(PC->PlayerCameraManager->GetCameraRotation().Pitch))<1.5){if(!Check(PC->GetLookCallbackCount()>CallbacksBefore,TEXT("actual_Mouse2D_action_callbacks_observed"),TEXT("Final PCM and per-stage chains are in captures")))return;Finish(Captures.Num()==9?TEXT("PASS_CAPTURE_ONLY"):TEXT("FAIL"),TEXT("Real key-driven entry, actual steering, low-speed movement, exit and foot presentation captured. Natural hands, intersections and garment coverage require PNG review; OS/package NOT_RUN."));}}
}
void AHCM5VS2DrivingHandsReviewDirector::ObserveInput(const FInputKeyEventArgs& E){if(bEnabled&&E.Event==IE_Pressed&&(E.Key==EKeys::Escape||E.Key==EKeys::P))Finish(TEXT("USER_ABORTED"),TEXT("Esc/P permanently cancels owned input and autoquit"),true);}
void AHCM5VS2DrivingHandsReviewDirector::ObserveActivation(bool Active){if(bEnabled&&bHadFocus&&!Active)Finish(TEXT("USER_ABORTED"),TEXT("Focus loss; no reacquire"),true);}
void AHCM5VS2DrivingHandsReviewDirector::Finish(const FString& Result,const FString& Why,bool Stop)
{
    if(Stop){bStopped=true;bAutoQuit=false;UE_LOG(LogTemp,Warning,TEXT("M5VS2_DRIVING_HANDS_REVIEW_USER_STOP %s"),*Why);}if(bDone&&!Stop)return;Status=Result;Detail=Why;bDone=true;FinishedWall=FPlatformTime::Seconds();ReleaseOwned();if(Pending&&FScreenshotRequest::IsScreenshotRequested()&&FScreenshotRequest::GetFilename()==PendingPNG)FScreenshotRequest::Reset();Write();
}
bool AHCM5VS2DrivingHandsReviewDirector::Write()
{
    if(bWriteFailed)return false;auto Failed=[&](const TCHAR* Step,uint32 Error){bWriteFailed=true;Status=TEXT("FAIL");Detail=FString(TEXT("Evidence publication failed: "))+Step;bDone=true;FinishedWall=FPlatformTime::Seconds();ReleaseOwned();UE_LOG(LogTemp,Error,TEXT("M5VS2_DRIVING_HANDS_REVIEW_WRITE_FAIL %s error=%u; no retry"),Step,Error);return false;};
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.DrivingHandsReview.v1"));J->SetStringField(TEXT("status"),Status);J->SetStringField(TEXT("detail"),Detail);J->SetStringField(TEXT("input_scope"),TEXT("ENGINE_KEYS_MOUSE_NOT_OS"));J->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW_NOT_PROVEN_BY_CAPTURE_COUNT"));J->SetStringField(TEXT("packaged_runtime"),TEXT("NOT_RUN_EDITOR_GAME"));J->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName());J->SetStringField(TEXT("period"),ExpectedPeriod.ToString());J->SetStringField(TEXT("save_slot"),PC?PC->GetSaveSlotName():TEXT(""));J->SetBoolField(TEXT("stop_latched"),bStopped);J->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedWall);J->SetNumberField(TEXT("pose_camera_steering_setters"),0);J->SetNumberField(TEXT("actual_park_left_degrees"),LeftWheel);J->SetNumberField(TEXT("actual_park_right_degrees"),RightWheel);J->SetArrayField(TEXT("queued_raw_mouse_xy"),XYZ(FVector(RawMouseSum.X,RawMouseSum.Y,0)));J->SetArrayField(TEXT("captures"),Rows(Captures));J->SetArrayField(TEXT("samples"),Rows(Samples));J->SetArrayField(TEXT("key_events"),Rows(Events));J->SetArrayField(TEXT("checks"),Rows(Checks));J->SetArrayField(TEXT("look_chains"),Rows(LookChains));if(ShaderState)J->SetObjectField(TEXT("material_readiness"),ShaderState);
    FString Text;if(!FJsonSerializer::Serialize(J,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text)))return Failed(TEXT("serialize"),0);const FString Temp=Directory/(TEXT("hands_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp")),Dest=Directory/TEXT("driving_hands_review.json");if(!FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return Failed(TEXT("temp_write"),FPlatformMisc::GetLastError());
#if PLATFORM_WINDOWS
    const FString From=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Temp).Replace(TEXT("/"),TEXT("\\")),To=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Dest).Replace(TEXT("/"),TEXT("\\"));if(!::MoveFileExW(*From,*To,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return Failed(TEXT("atomic_replace"),FPlatformMisc::GetLastError());return true;
#else
    return Failed(TEXT("unsupported_platform"),0);
#endif
}
void AHCM5VS2DrivingHandsReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bEnabled&&!bDone){bAutoQuit=false;Finish(TEXT("USER_ABORTED"),TEXT("World ended before completed sequence"),true);}if(Viewport.IsValid()&&InputHandle.IsValid())Viewport->OnInputKey().Remove(InputHandle);if(FSlateApplication::IsInitialized()&&ActivationHandle.IsValid())FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);if(ScreenshotHandle.IsValid())FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);Super::EndPlay(Reason);
}
