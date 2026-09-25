#include "HCM5VS2HeroExerciseDirector.h"

#include "HCM5VS2ExpressionComponent.h"
#include "HCM5VS2LookAnimInstance.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M3/HCM3Recording.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "RHI.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "UnrealClient.h"

namespace
{
bool FaceReviewOnly()
{ return FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroFaceReview")); }
bool GaitRevisionTwoOnly()
{ return FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroGaitR2")); }
bool GaitReviewOnly()
{ return FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroGaitReview")) || GaitRevisionTwoOnly(); }
bool SoleCaptureOnly()
{ return FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroGaitSoleCapture")); }
bool FootPlacementOnly()
{ return FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroGaitFootPlacement")); }

bool PrivatePlacementObject(const UObject* Object,const TCHAR* Name)
{
    if (!Object) return false;
    const FString Path=Object->GetOutermost()->GetName();
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/FootPlacementAB/Batch_");
    const FString Suffix=FString(TEXT("/"))+Name;
    if (!Path.StartsWith(Prefix) || !Path.EndsWith(Suffix)
        || Path.Len()!=Prefix.Len()+12+Suffix.Len()) return false;
    for (TCHAR Character:Path.Mid(Prefix.Len(),12)) if (!FChar::IsHexDigit(Character)) return false;
    return true;
}

const TCHAR* GaitSpace = TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun.BS_M5VS2_GAS_IdleWalkRun");

bool PrivateGaitR2Map(const FString& Name)
{
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/HeroGaitR2/Run_");
    const FString Suffix=TEXT("/L_HeroGaitR2");
    if (!Name.StartsWith(Prefix) || !Name.EndsWith(Suffix) || Name.Len()!=Prefix.Len()+12+Suffix.Len()) return false;
    for (TCHAR C:Name.Mid(Prefix.Len(),12)) if (!FChar::IsHexDigit(C)) return false;
    return true;
}

TSharedRef<FJsonObject> CaptureFaceMorphReadback(const USkeletalMeshComponent* Mesh)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("scope"), TEXT("Capture only, PostUpdateWork after mesh prerequisite: component overrides and evaluated active morph weights; LOD0 source delta statistics are not final GPU surface proof"));
    Result->SetNumberField(TEXT("frame"), double(GFrameCounter));
    const FName Names[] = {TEXT("mouth_∧"), TEXT("mouth_H"), TEXT("mouth_narrow"), TEXT("mouth_ω"),
        TEXT("mouth_smile"), TEXT("mouth_straight"), TEXT("mouth_sad"), TEXT("brow_anger")};
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (FName Name : Names)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Name.ToString());
        const UMorphTarget* Morph = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->FindMorphTarget(Name) : nullptr;
        Row->SetBoolField(TEXT("target_exists"), Morph != nullptr);
        Row->SetNumberField(TEXT("component_override_weight"), Mesh ? Mesh->GetMorphTarget(Name) : 0.f);
        const int32* Index = Mesh && Morph ? Mesh->ActiveMorphTargets.Find(Morph) : nullptr;
        const bool ValidWeight = Index && Mesh->MorphTargetWeights.IsValidIndex(*Index);
        const float Weight = ValidWeight ? Mesh->MorphTargetWeights[*Index] : 0.f;
        Row->SetBoolField(TEXT("in_evaluated_active_map"), Index != nullptr);
        Row->SetNumberField(TEXT("evaluated_weight_index"), Index ? *Index : INDEX_NONE);
        Row->SetBoolField(TEXT("evaluated_weight_index_valid"), ValidWeight);
        Row->SetNumberField(TEXT("evaluated_weight"), Weight);
        const bool HasLOD = Morph && Morph->GetMorphLODModels().IsValidIndex(0);
        int32 NonzeroPositions = 0, NonzeroNormals = 0, StoredVertices = 0;
        double MaximumDelta = 0;
        if (HasLOD)
        {
            const FMorphTargetLODModel& LOD = Morph->GetMorphLODModels()[0];
            StoredVertices = LOD.Vertices.Num();
            for (const FMorphTargetDelta& Delta : LOD.Vertices)
            {
                const double Size = Delta.PositionDelta.Size();
                NonzeroPositions += Size > 1.e-8;
                NonzeroNormals += Delta.TangentZDelta.SizeSquared() > 1.e-16;
                MaximumDelta = FMath::Max(MaximumDelta, Size);
            }
            Row->SetNumberField(TEXT("lod0_runtime_num_vertices"), LOD.NumVertices);
        }
        Row->SetBoolField(TEXT("lod0_model_exists"), HasLOD);
        Row->SetStringField(TEXT("lod0_cpu_delta_status"), HasLOD && StoredVertices > 0 ? TEXT("AVAILABLE") : TEXT("NO_CPU_DELTAS_AVAILABLE"));
        Row->SetNumberField(TEXT("lod0_stored_vertices"), StoredVertices);
        Row->SetNumberField(TEXT("lod0_nonzero_position_deltas"), NonzeroPositions);
        Row->SetNumberField(TEXT("lod0_nonzero_normal_deltas"), NonzeroNormals);
        Row->SetNumberField(TEXT("lod0_max_position_delta_mesh_cm"), MaximumDelta);
        Row->SetNumberField(TEXT("single_morph_weighted_max_delta_mesh_cm"), MaximumDelta * FMath::Abs(Weight));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("morphs"), Rows); return Result;
}

TArray<TSharedPtr<FJsonValue>> VectorValues(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> QuatValues(const FQuat& Q)
{ return {MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W)}; }
bool SafeName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 64) return false;
    for (TCHAR C : Name) if (!FChar::IsAlnum(C) && C != TEXT('_')) return false;
    return true;
}
bool ValidPNG(const FString& Path)
{
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Path));
    if (!File || File->TotalSize() < 33) return false;
    uint8 B[24]; File->Serialize(B, 24);
    const uint8 Signature[8] = {137,80,78,71,13,10,26,10};
    auto BE = [](const uint8* P) { return uint32(P[0]) << 24 | uint32(P[1]) << 16 | uint32(P[2]) << 8 | uint32(P[3]); };
    return !File->IsError() && FMemory::Memcmp(B, Signature, 8) == 0
        && FMemory::Memcmp(B+12, "IHDR", 4) == 0 && BE(B+16) == 1920 && BE(B+20) == 1080;
}
}

AHCM5VS2HeroExerciseDirector::AHCM5VS2HeroExerciseDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AHCM5VS2HeroExerciseDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroExercise")) && !FaceReviewOnly() && !GaitReviewOnly()) return;
    FString Evidence;
    if (FParse::Param(FCommandLine::Get(), TEXT("M5VS2Lookdev")) || GUsingNullRHI
        || !FParse::Value(FCommandLine::Get(), TEXT("M5VS2EvidenceDir="), Evidence) || FPaths::IsRelative(Evidence)) return;
    Evidence = FPaths::ConvertRelativePathToFull(Evidence); FPaths::NormalizeDirectoryName(Evidence);
    if (!FPaths::CollapseRelativeDirectories(Evidence)
        || !FPaths::IsUnderDirectory(Evidence, TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2"))) return;
    RunDirectory = Evidence / (TEXT("HeroExercise_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))
        + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*RunDirectory, true)) return;
    StartedAt = FPlatformTime::Seconds(); bEnabled = true;
    bAutoQuit = FParse::Param(FCommandLine::Get(), TEXT("M5VS2AutoQuit"));
    SetActorTickEnabled(true); // Defer discovery until all player/component BeginPlay calls finish.
#endif
}

bool AHCM5VS2HeroExerciseDirector::Initialize(FString& Failure)
{
    Controller = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    Character = Controller ? Cast<AHCM1Character>(Controller->GetPawn()) : nullptr;
    Expression = Character ? Character->FindComponentByClass<UHCM5VS2ExpressionComponent>() : nullptr;
    UGameViewportClient* GameViewport = GetWorld()->GetGameViewport();
    if (!GameViewport || !GameViewport->Viewport || !ValidateBinding(Failure))
    { if (Failure.IsEmpty()) Failure = TEXT("Live viewport required"); return false; }
    if (FootPlacementOnly() && (!SoleCaptureOnly() || !GaitRevisionTwoOnly()
        || !ActorHasTag(TEXT("HarborCity_M5VS2_FootPlacementAB"))))
    { Failure=TEXT("Placement diagnostic requires the authored private sole/R2 fixture and explicit opt-in tag");return false; }
    if (SoleCaptureOnly())
    {
        float RecordSeconds = 0.f, RecordDelay = 0.f;
        Failure = TEXT("Sole capture requires private unified-clock R2, explicit recorder, authored native selection and no other capture mode");
        if (!GaitRevisionTwoOnly() || !FParse::Param(FCommandLine::Get(), TEXT("M5VS2HeroGaitUnifiedClock"))
            || FaceReviewOnly() || !AHCM3Recording::IsVS2GameplayCaptureRequested()
            || !FParse::Value(FCommandLine::Get(), TEXT("M5VS2RecordSeconds="), RecordSeconds) || RecordSeconds != 30.f
            || !FParse::Value(FCommandLine::Get(), TEXT("M5VS2RecordDelay="), RecordDelay) || RecordDelay != 3.f
            || !ActorHasTag(TEXT("HarborCity_M5VS2_GaitSoleCapture"))
            || !FPaths::IsUnderDirectory(SoleSelectionEvidence, TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/research"))
            || !IFileManager::Get().FileExists(*SoleSelectionEvidence)) return false;
        if (!UHCM5VS2SoleDiagnostics::ValidateSelection(Character->GetMesh(), SolePoints, Failure)) return false;
        for (TActorIterator<AHCM3Recording> Existing(GetWorld()); Existing; ++Existing)
        { Failure = TEXT("Sole capture spawns its recorder only after settled shader warmup; pre-existing recorder rejected"); return false; }
        if (!FSlateApplication::IsInitialized())
        { Failure = TEXT("Sole capture requires real Slate activation stop observer"); return false; }
        Failure.Reset();
    }
    if (Phases.Num() < 1 || Phases.Num() > 24 || ObservedBones.Num() < 1 || ObservedBones.Num() > 16
        || ExerciseMaterials.Num() != Character->GetMesh()->GetNumMaterials())
    { Failure = TEXT("Bounded phases/bone probes/material slots are invalid"); return false; }
    if (GaitReviewOnly())
    {
        const bool R2=GaitRevisionTwoOnly();
        const FString WorldName=GetWorld()->GetOutermost()->GetName();
        const bool CorrectMap=R2 ? PrivateGaitR2Map(WorldName)
            : WorldName==TEXT("/Game/HarborCity/M5VS2/HeroExercise/L_SelestiaExercise");
        if (FaceReviewOnly() || !CorrectMap
            || Character->WalkSpeed != 400.f || Character->SprintSpeed != 650.f)
        { Failure = TEXT("Gait review requires exact isolated exercise map and unchanged 400/650 movement"); return false; }
        UBlendSpace* Space=R2 ? ExpectedGaitBlendSpace.Get() : LoadObject<UBlendSpace>(nullptr,GaitSpace);
        if (!Space || Space->GetBlendSamples().Num() != 28)
        { Failure = TEXT("Saved native 28-sample gait candidate must exist before runtime review"); return false; }
        ActiveGaitSpace=Space;
        GaitRunClip=Space->GetBlendSamples()[0].Animation;
        GaitSprintClip=Space->GetBlendSamples()[9].Animation;
        GaitWalkClip=Space->GetBlendSamples()[27].Animation;
        GaitIdleClip=Space->GetBlendSamples()[18].Animation;
        if (R2)
        {
            const auto* Motion=Cast<UHCM5VS2LookAnimInstance>(Character->GetMesh()->GetAnimInstance());
            const FString Parent=FPaths::GetPath(Space->GetOutermost()->GetName());
            const bool UniformClock=FParse::Param(FCommandLine::Get(),TEXT("M5VS2HeroGaitUnifiedClock"));
            // The five-asset clock experiment reuses the three exact, previously
            // measured normalized loops. All other R2 fixtures retain the same-
            // batch requirement. This changes identity validation, not sampling.
            const FString LoopParent=UniformClock
                ? TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_d502bbff7626") : Parent;
            const bool Placement=FootPlacementOnly();
            const USkeletalMesh* PlacementMesh=Character->GetMesh()->GetSkeletalMeshAsset();
            const FString PlacementBatch=PlacementMesh ? FPaths::GetPath(PlacementMesh->GetOutermost()->GetName()) : FString();
            const bool PlacementIdentity=Placement && UniformClock && SoleCaptureOnly()
                && !Space->bAllowMarkerBasedSync
                && Parent==TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_9d25d6562b62")
                && PrivatePlacementObject(PlacementMesh,TEXT("SKM_Placement"))
                && PrivatePlacementObject(PlacementMesh->GetSkeleton(),TEXT("SK_Placement"))
                && PrivatePlacementObject(ExpectedAnimationClass.Get(),TEXT("ABP_Selestia_GASMotion_Placement"))
                && FPaths::GetPath(ExpectedAnimationClass->GetOutermost()->GetName())==PlacementBatch
                && FPaths::GetPath(PlacementMesh->GetSkeleton()->GetOutermost()->GetName())==PlacementBatch
                && PlacementMesh->GetRefSkeleton().GetRawBoneNum()==247
                && PlacementMesh->GetSkeleton()->GetVirtualBones().Num()==4
                && Character->GetMesh()->GetBoneIndex(TEXT("VB VS2_PlacementFloor"))!=INDEX_NONE
                && ExpectedCharacterClass->GetName()==TEXT("BP_HeroGait_Placement_C");
            const bool ClockIdentity=Placement ? PlacementIdentity : !UniformClock || (!Space->bAllowMarkerBasedSync && Parent!=LoopParent
                && ExpectedAnimationClass->GetName()==TEXT("ABP_Selestia_GASMotion_UniformClock_C")
                && ExpectedCharacterClass->GetName()==TEXT("BP_HeroGait_UniformClock_C")
                && FPaths::GetPath(ExpectedAnimationClass->GetOutermost()->GetName())==Parent);
            const bool Paths=ClockIdentity && Parent.StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_"))
                && Space->GetName()==TEXT("BS_Selestia_GASMotion")
                && GaitRunClip && GaitRunClip->GetOutermost()->GetName()==LoopParent/TEXT("Loops/Run_WarpLoop")
                && GaitSprintClip && GaitSprintClip->GetOutermost()->GetName()==LoopParent/TEXT("Loops/Sprint_WarpLoop")
                && GaitWalkClip && GaitWalkClip->GetOutermost()->GetName()==LoopParent/TEXT("Loops/Walk_WarpLoop")
                && GaitIdleClip && GaitIdleClip->GetName()==TEXT("M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS");
            if (!Paths || !Motion || !Motion->bVS2GASMotionEnabled || !Motion->bVS2FlightPosesEnabled
                || !ActorHasTag(TEXT("HarborCity_M5VS2_HeroGaitR2")) || GaitSourceReport.IsEmpty()
                || !ExpectedCharacterClass->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_")))
            { Failure=TEXT("R2 gait requires authored integrated Hero, enabled retained GAS/flight graph and exact new loop identities");return false; }
            for (FName Bone:{FName(TEXT("VB VS2_IKRoot")),FName(TEXT("VB VS2_IKFoot_L")),FName(TEXT("VB VS2_IKFoot_R"))})
                if (Character->GetMesh()->GetBoneIndex(Bone)==INDEX_NONE)
                { Failure=TEXT("R2 final mesh is missing real warping virtual bone: ")+Bone.ToString();return false; }
        }
        for (FName Bone : {FName(TEXT("Foot_L")),FName(TEXT("Foot_R")),FName(TEXT("Toe_L")),FName(TEXT("Toe_R"))})
            if (Character->GetMesh()->GetBoneIndex(Bone) == INDEX_NONE)
            { Failure = TEXT("Actual Selestia gait bone missing: ")+Bone.ToString(); return false; }
        // Only this opt-in run's phase list changes. No map/CDO save or velocity setting.
        Phases.Reset();
        auto Add = [this](const TCHAR* Label,const TCHAR* Action,float Duration,float CaptureAt=-1.f)
        {
            FHCM5VS2ExercisePhase P; P.Label=Label;P.Action=Action;P.Duration=Duration;P.CaptureAt=CaptureAt;Phases.Add(P);
        };
        Add(TEXT("Warmup"),TEXT("Warmup"),10);
        Add(TEXT("Gait_Idle"),TEXT("Rest"),1,.6f);
        // R2 starts replace the low-speed acceleration interval. Add a genuine
        // analog walk segment to observe its retained walk loop, without
        // changing movement settings or the original stance eligibility rules.
        if (R2)
        {
            Add(TEXT("Gait_LowSpeedWalk"),TEXT("GaitAnalogWalk"),1.5f,1.1f);
            Add(TEXT("Gait_LowSpeedStop"),TEXT("Rest"),1.f);
        }
        Add(TEXT("Gait_Forward400"),TEXT("Walk"),2.15f,1.4f);
        Add(TEXT("Gait_Stop400"),TEXT("Rest"),1,.8f);
        Add(TEXT("Gait_Forward650"),TEXT("GaitSprintBack"),2.65f,1.7f);
        Add(TEXT("Gait_Stop650"),TEXT("Rest"),1,.8f);
        Add(TEXT("Gait_WalkReturn"),TEXT("Walk"),1.5f);
        Add(TEXT("Gait_Turn"),TEXT("TurnWalk"),1.1f,.65f);
        Add(TEXT("Gait_Jump"),TEXT("Jump"),2,.28f);
        Add(TEXT("Gait_Recovered"),TEXT("Rest"),2,1.4f);
        // Continuous capture is a separate private recording, not the original
        // nine-shot baseline run. Its original movement phases/eligibility stay intact.
        if (SoleCaptureOnly()) for (auto& Phase : Phases) Phase.CaptureAt = -1.f;
    }
    if (FaceReviewOnly())
    {
        // This changes this transient run's phase list only; the authored map/CDO is not saved.
        Phases.RemoveAll([](const FHCM5VS2ExercisePhase& P) { return P.Action != TEXT("Warmup") && P.Action != TEXT("Emotion"); });
        TSet<FName> Emotions;
        for (const auto& P : Phases) if (P.Action == TEXT("Emotion"))
        {
            if (P.CaptureAt < 0 || !P.bFaceCamera || Emotions.Contains(P.Emotion))
            { Failure = TEXT("Face review requires six unique authored emotion closeups"); return false; }
            Emotions.Add(P.Emotion);
        }
        const TSet<FName> Required = {TEXT("Happy"),TEXT("Surprised"),TEXT("Angry"),TEXT("Sad"),TEXT("Shy"),TEXT("Serious")};
        if (Phases.Num() != 7 || Emotions.Num() != Required.Num())
        { Failure = TEXT("Face review requires original Warmup plus six emotions"); return false; }
        for (FName Name : Required) if (!Emotions.Contains(Name))
        { Failure = TEXT("Face review is missing an authored emotion"); return false; }
    }
    TSet<FString> Actions = {TEXT("Warmup"),TEXT("Rest"),TEXT("Walk"),TEXT("TurnWalk"),TEXT("RunHome"),
        TEXT("Jump"),TEXT("Turn"),TEXT("Emotion"),TEXT("Speech"),TEXT("LookLeft"),TEXT("LookRight")};
    if (GaitReviewOnly()) Actions.Add(TEXT("GaitSprintBack"));
    if (GaitRevisionTwoOnly()) Actions.Add(TEXT("GaitAnalogWalk"));
    float Total = 0;
    for (const auto& Phase : Phases)
    {
        if (!SafeName(Phase.Label) || !Actions.Contains(Phase.Action) || !FMath::IsFinite(Phase.Duration)
            || Phase.Duration <= 0 || Phase.Duration > 15 || !FMath::IsFinite(Phase.CaptureAt)
            || Phase.CaptureAt >= Phase.Duration)
        { Failure = TEXT("Invalid exercise phase"); return false; }
        Total += Phase.Duration;
    }
    if (Total > 90 || Phases[0].Action != TEXT("Warmup"))
    { Failure = TEXT("Exercise must start with Warmup and plan <=90 seconds"); return false; }
    for (const auto& Bone : ObservedBones)
        if (Character->GetMesh()->GetBoneIndex(Bone.Bone) == INDEX_NONE || Character->GetMesh()->GetBoneIndex(Bone.Anchor) == INDEX_NONE)
        { Failure = TEXT("Author-selected bone/anchor is missing: ") + Bone.Bone.ToString(); return false; }
    for (UMaterialInterface* Material : ExerciseMaterials) if (!Material)
    { Failure = TEXT("Missing exercise material"); return false; }
    if (!DirectionalLight || !DirectionalLight->GetLightComponent())
    { Failure = TEXT("Actual stage directional light is missing"); return false; }
    Viewport = GameViewport;
    InputHandle = GameViewport->OnInputKey().AddUObject(this, &AHCM5VS2HeroExerciseDirector::ViewportInput);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &AHCM5VS2HeroExerciseDirector::ScreenshotProcessed);
    if (SoleCaptureOnly())
        SoleActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &AHCM5VS2HeroExerciseDirector::SoleActivation);
    OriginalViewTarget = Controller->GetViewTarget(); Origin = Character->GetActorLocation();
    if (AHUD* HUD = Controller->GetHUD()) { bOriginalHUD = HUD->bShowHUD; HUD->bShowHUD = false; }
    for (int32 I=0; I<ExerciseMaterials.Num(); ++I)
    {
        Originals.Add(Character->GetMesh()->GetMaterial(I));
        if (GaitRevisionTwoOnly())
        {
            if (Originals.Last()!=ExerciseMaterials[I])
            { Failure=TEXT("R2 gait retains exact integrated Hero materials; no SoftToon substitution");return false; }
            RuntimeMaterials.Add(Originals.Last());
            continue;
        }
        auto* Dynamic = UMaterialInstanceDynamic::Create(ExerciseMaterials[I], this);
        if (!Dynamic) { Failure = TEXT("Could not create transient exercise material"); return false; }
        Dynamic->SetVectorParameterValue(TEXT("SunDirection"), FLinearColor(-DirectionalLight->GetActorForwardVector()));
        Dynamic->SetVectorParameterValue(TEXT("LightColor"), DirectionalLight->GetLightComponent()->GetLightColor()
            * FMath::Clamp(DirectionalLight->GetLightComponent()->Intensity / 3.f, .01f, 10.f));
        Dynamic->SetVectorParameterValue(TEXT("AmbientColor"), FLinearColor(.30f,.32f,.38f));
        RuntimeMaterials.Add(Dynamic); Character->GetMesh()->SetMaterial(I, Dynamic);
    }
    FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
    Camera = GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    LookTarget = GetWorld()->SpawnActor<ATargetPoint>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Camera || !LookTarget) { Failure = TEXT("Transient exercise camera/target failed"); return false; }
    Camera->GetCameraComponent()->bConstrainAspectRatio = false;
    Camera->GetCameraComponent()->PostProcessBlendWeight = 0;
    Controller->SetViewTarget(Camera);
    AddTickPrerequisiteComponent(Character->GetMesh());
    MaxBoneAngleDegrees.Init(0, ObservedBones.Num()); MaxBoneDisplacement.Init(0, ObservedBones.Num());
    bInitialized = true;
    return BeginPhase(0, Failure);
}

bool AHCM5VS2HeroExerciseDirector::ValidateBinding(FString& Failure) const
{
    auto* Mesh = Character ? Character->GetMesh() : nullptr;
    if (!Character || !Controller || Controller->GetPawn() != Character || !Mesh || !Expression
        || !ExpectedCharacterClass || Character->GetClass() != ExpectedCharacterClass.Get()
        || Mesh->GetSkeletalMeshAsset() != ExpectedMesh || Mesh->GetAnimClass() != ExpectedAnimationClass.Get()
        || !Mesh->GetAnimInstance() || Mesh->GetAnimInstance()->GetClass() != ExpectedAnimationClass.Get()
        || !Cast<UHCM5VS2LookAnimInstance>(Mesh->GetAnimInstance()) || !Expression->IsFaceReady()
        || Controller->IsFirstPersonPerspective() || Controller->GetPlayerMode() != EHCPlayerMode::OnFoot)
    { Failure = TEXT("Expected new VS2 player/physics-look AnimBP/expression/third-person binding is not active; no substitution permitted"); return false; }
    return true;
}

bool AHCM5VS2HeroExerciseDirector::CheckShaders(FString& Failure)
{
    ShaderState = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Rows;
    const EShaderPlatform Platform = GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel());
    bool Ready = true;
    for (int32 I=0; I<Character->GetMesh()->GetNumMaterials(); ++I)
    {
        auto* Material = Character->GetMesh()->GetMaterial(I);
        auto* Resource = Material ? Material->GetMaterialResource(Platform) : nullptr;
        bool Compiling = Material && Material->IsCompiling(); TArray<TSharedPtr<FJsonValue>> Errors;
#if WITH_EDITOR
        if (Resource)
        {
            Compiling |= !Resource->IsCompilationFinished();
            for (const auto& Error : Resource->GetCompileErrors()) if (Errors.Num()<8) Errors.Add(MakeShared<FJsonValueString>(Error.Left(2048)));
            if (!Resource->IsGameThreadShaderMapComplete() && Errors.IsEmpty()) Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
        }
#endif
        const auto* Map = Resource ? Resource->GetGameThreadShaderMap() : nullptr;
        const bool Usage = Material && Material->GetUsageByFlag(MATUSAGE_SkeletalMesh) && Material->GetUsageByFlag(MATUSAGE_MorphTargets);
        const bool Finalized = Map && Map->IsCompilationFinalized();
        const bool Succeeded = Map && Map->CompiledSuccessfully();
        const bool Valid = Map && Map->IsValidForRendering();
        const bool Complete = Resource && Resource->IsGameThreadShaderMapComplete();
        // Frozen lazy-compilation maps may retain false lifecycle bits in UE 5.8.
        // They are diagnostics, except an explicitly finalized failed compile.
        const bool CompileFailed = !Errors.IsEmpty() || (Map && Finalized && !Succeeded);
        const bool SlotReady = Usage && Resource && !Compiling && !CompileFailed && Valid && Complete;
        Ready &= SlotReady;
        auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("slot"), I);
        Row->SetStringField(TEXT("interface"), GetPathNameSafe(Material)); Row->SetBoolField(TEXT("skeletal_and_morph_usage"), Usage);
        Row->SetBoolField(TEXT("compiling"), Compiling); Row->SetBoolField(TEXT("ready"), SlotReady); Row->SetArrayField(TEXT("errors"), Errors);
        Row->SetBoolField(TEXT("shader_map_exists"), Map != nullptr);
        Row->SetBoolField(TEXT("shader_map_finalized"), Finalized);
        Row->SetBoolField(TEXT("shader_map_compiled_successfully"), Succeeded);
        Row->SetBoolField(TEXT("shader_map_valid_for_rendering"), Valid);
        Row->SetBoolField(TEXT("shader_map_complete"), Complete);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        if (!Usage || (CompileFailed && !Compiling)) Failure = TEXT("Exercise requires skeletal+morph-enabled materials without compile errors");
    }
    ShaderState->SetArrayField(TEXT("slots"), Rows); ShaderState->SetNumberField(TEXT("shader_platform"), int32(Platform));
    ShaderState->SetBoolField(TEXT("game_thread_ready"), Ready);
    ShaderState->SetStringField(TEXT("scope"), TEXT("Game-thread shader maps; render-proxy fallback belongs to separate fair-comparison evidence"));
    return Ready;
}

bool AHCM5VS2HeroExerciseDirector::BeginPhase(int32 Index, FString& Failure)
{
    if (!Phases.IsValidIndex(Index)) { Failure = TEXT("Phase index invalid"); return false; }
    PhaseIndex = Index; PhaseElapsed = 0; bCaptureRequested = false; bJumpReleased = false;
    const auto& Phase = Phases[Index];
    Character->SetSprinting(Phase.Action == TEXT("RunHome") || (GaitReviewOnly() && Phase.Action == TEXT("GaitSprintBack")));
    Expression->StopSpeaking(); Expression->ClearLookTarget();
    if (!Expression->SetEmotion(Phase.Emotion)) { Failure = TEXT("Unsupported authored emotion"); return false; }
    if (Phase.Action == TEXT("Jump")) Character->Jump();
    if (Phase.Action == TEXT("Turn")) Character->FaceBodyYawOnce(0, 1.2f);
    if (Phase.Action == TEXT("LookLeft") || Phase.Action == TEXT("LookRight"))
    {
        const FVector Head = Character->GetMesh()->GetSocketLocation(TEXT("Head"));
        LookTarget->SetActorLocation(Head + Character->GetActorForwardVector()*260.f
            + Character->GetActorRightVector()*(Phase.Action == TEXT("LookLeft") ? -160.f : 160.f));
        Expression->SetLookTarget(LookTarget);
    }
    UE_LOG(LogTemp, Display, TEXT("M5VS2_HERO_EXERCISE_PHASE %s simulation=%.3f frame=%llu"), *Phase.Label, SimulationElapsed, GFrameCounter);
    WriteReport(TEXT("NOT_RUN"), TEXT("Native exercise running; visual acceptance remains USER_REVIEW"));
    return true;
}

void AHCM5VS2HeroExerciseDirector::UpdatePhase(float DeltaSeconds)
{
    const auto& Phase = Phases[PhaseIndex];
    if (Phase.Action == TEXT("Walk")) Character->AddMovementInput(FVector(0,1,0), 1.f);
    if (GaitReviewOnly() && Phase.Action == TEXT("GaitSprintBack")) Character->AddMovementInput(FVector(0,-1,0),1.f);
    if (GaitRevisionTwoOnly() && Phase.Action==TEXT("GaitAnalogWalk")) Character->AddMovementInput(FVector(0,1,0),.38f);
    if (Phase.Action == TEXT("TurnWalk"))
        Character->AddMovementInput(FRotator(0, 90.f + 90.f*float(PhaseElapsed/Phase.Duration), 0).Vector(), 1.f);
    if (Phase.Action == TEXT("RunHome"))
    {
        FVector Direction = Origin - Character->GetActorLocation(); Direction.Z = 0;
        if (Direction.SizeSquared() > 40.f*40.f) Character->AddMovementInput(Direction.GetSafeNormal(), 1.f);
    }
    if (Phase.Action == TEXT("Jump") && !bJumpReleased && PhaseElapsed > .16)
    { Character->StopJumping(); bJumpReleased = true; }
    if (Phase.Action == TEXT("Speech"))
    {
        const FString Text = TEXT("欢迎来到海港，我们沿着街道慢慢走吧。");
        Expression->SetDialogueTextProgress(Text, FMath::Clamp(FMath::FloorToInt(PhaseElapsed*8.),0,Text.Len()), true);
    }
    const FVector Eyes = (Character->GetMesh()->GetSocketLocation(TEXT("LeftEye"))
        + Character->GetMesh()->GetSocketLocation(TEXT("RightEye")))*.5;
    const FVector Target = SoleCaptureOnly() ? Character->GetActorLocation()+FVector(0,0,-60)
        : Phase.bFaceCamera ? Eyes : Character->GetActorLocation()+FVector(0,0,-3);
    // Only the opt-in R2 stop capture: the original +Y offset crosses the
    // retained fixture wall after the 400 cm/s route. Gameplay and gait samples
    // keep the identical route; final PCM viewpoint remains recorded.
    const FVector BodyCameraOffset = SoleCaptureOnly() ? FVector(320,0,-15)
        : GaitRevisionTwoOnly() && Phase.Label == TEXT("Gait_Stop400") ? FVector(520,-160,60) : FVector(520,160,60);
    const FVector Position = Phase.bFaceCamera ? Eyes+FVector(165,0,0) : Character->GetActorLocation()+BodyCameraOffset;
    Camera->SetActorLocationAndRotation(Position, (Target-Position).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(SoleCaptureOnly() || Phase.bFaceCamera ? 40.f : 45.f);
}

bool AHCM5VS2HeroExerciseDirector::PrepareSoleRecording(FString& Failure)
{
    Failure.Reset();
    if (!SoleRecorder)
    {
        FActorSpawnParameters Spawn;
        Spawn.ObjectFlags |= RF_Transient;
        SoleRecorder = GetWorld()->SpawnActor<AHCM3Recording>(Spawn);
        SoleRecorderSpawnWall = FPlatformTime::Seconds();
        if (!SoleRecorder || SoleRecorder->GetCaptureDirectory().IsEmpty())
            Failure = TEXT("Native VS2 recorder failed to initialize its isolated capture directory");
        return false;
    }
    // A stopped recorder is never restarted, including zero-frame early stops.
    if (IFileManager::Get().FileExists(*(SoleRecorder->GetCaptureDirectory()/TEXT("capture.json"))))
    { Failure = TEXT("Native recorder stopped before the sole route completed; inspect its original capture.json"); return false; }
    if (!SoleRecorder->HasCapturedFirstFrame())
    {
        if (FPlatformTime::Seconds()-SoleRecorderSpawnWall > 15.)
            Failure = TEXT("Native recorder first-frame deadline exceeded");
        return false;
    }
    if (SoleFirstFrameWorld < 0) SoleFirstFrameWorld = GetWorld()->GetTimeSeconds();
    // Four actual game seconds of static reference before the unchanged route.
    return GetWorld()->GetTimeSeconds()-SoleFirstFrameWorld >= 4.;
}

bool AHCM5VS2HeroExerciseDirector::ObserveSole(FString& Failure)
{
    const double WorldTime = GetWorld()->GetTimeSeconds();
    if (WorldTime < SoleNextSampleWorld) return true;
    SoleNextSampleWorld = WorldTime + 1./30.; // Skip missed deadlines; never duplicate samples or fixed-step time.
    if (SoleSamples.Num() >= 900)
    { Failure = TEXT("Bounded 900 native sole sample limit"); return false; }
    auto Frame = UHCM5VS2SoleDiagnostics::CaptureSurface(Character->GetMesh(), SolePoints);
    if (!Frame || Frame->GetStringField(TEXT("status")) != TEXT("PASS_CPU_LBS_SAMPLE_ONLY"))
    { Failure = Frame ? Frame->GetStringField(TEXT("error")) : TEXT("Native sole sampling unavailable"); return false; }
    Frame->SetStringField(TEXT("phase"), Phases[PhaseIndex].Label);
    Frame->SetNumberField(TEXT("phase_elapsed_seconds"), PhaseElapsed);
    Frame->SetArrayField(TEXT("actor_world_cm"), VectorValues(Character->GetActorLocation()));
    Frame->SetArrayField(TEXT("actor_velocity_cm_s"), VectorValues(Character->GetVelocity()));
    Frame->SetNumberField(TEXT("actor_yaw_degrees"), Character->GetActorRotation().Yaw);
    Frame->SetBoolField(TEXT("moving_on_ground"), Character->GetCharacterMovement()->IsMovingOnGround());
    Frame->SetNumberField(TEXT("latest_gait_sample_index"), GaitSamples.Num()-1);
    if (!GaitSamples.IsEmpty()) Frame->SetNumberField(TEXT("latest_gait_engine_frame"), GaitSamples.Last()->GetNumberField(TEXT("frame")));
    if (Controller->PlayerCameraManager)
    {
        const auto* Manager = Controller->PlayerCameraManager.Get();
        Frame->SetArrayField(TEXT("final_pcm_location_cm"), VectorValues(Manager->GetCameraLocation()));
        const FRotator Rotation = Manager->GetCameraRotation();
        Frame->SetArrayField(TEXT("final_pcm_rotation_pitch_yaw_roll"), VectorValues(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
        Frame->SetNumberField(TEXT("final_pcm_fov"), Manager->GetFOVAngle());
    }
    SoleSamples.Add(Frame);
    return true;
}

void AHCM5VS2HeroExerciseDirector::StopSoleCapture(const FString& Reason)
{
    if (!SoleCaptureOnly() || bStopped) return;
    bStopped = true; bAutoQuit = false; StopFrame = GFrameCounter;
    // Recorder's own input/activation observers preserve the actual stop reason.
    // No camera restore, forced character velocity, automatic resume or exit.
    WriteReport(TEXT("NOT_RUN"), Reason);
    SetActorTickEnabled(false);
}

void AHCM5VS2HeroExerciseDirector::SoleActivation(bool bActive)
{
    if (!bActive) StopSoleCapture(TEXT("Sole recording application focus lost; native stop latched"));
}

TSharedPtr<FJsonObject> AHCM5VS2HeroExerciseDirector::Observe() const
{
    auto Row = MakeShared<FJsonObject>(); const auto* Mesh = Character->GetMesh();
    const auto* Anim = CastChecked<UHCM5VS2LookAnimInstance>(Mesh->GetAnimInstance());
    Row->SetNumberField(TEXT("frame"), double(GFrameCounter)); Row->SetNumberField(TEXT("wall_seconds"), FPlatformTime::Seconds()-StartedAt);
    Row->SetNumberField(TEXT("simulation_seconds"), SimulationElapsed); Row->SetStringField(TEXT("phase"), Phases[PhaseIndex].Label);
    Row->SetNumberField(TEXT("phase_seconds"), PhaseElapsed); Row->SetStringField(TEXT("character_class"), Character->GetClass()->GetPathName());
    Row->SetStringField(TEXT("animation_class"), Anim->GetClass()->GetPathName()); Row->SetStringField(TEXT("mesh"), GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
    Row->SetArrayField(TEXT("actor_world"), VectorValues(Character->GetActorLocation())); Row->SetNumberField(TEXT("actor_yaw"), Character->GetActorRotation().Yaw);
    Row->SetArrayField(TEXT("velocity_cm_s"), VectorValues(Character->GetVelocity()));
    Row->SetBoolField(TEXT("falling"), Character->GetCharacterMovement()->IsFalling());
    Row->SetNumberField(TEXT("blink_weight"), Expression->GetBlinkWeight()); Row->SetNumberField(TEXT("mouth_weight"), Expression->GetTalkingWeight());
    Row->SetStringField(TEXT("emotion"), Expression->GetEffectiveEmotion().ToString());
    Row->SetNumberField(TEXT("head_look_alpha"), Anim->VS2HeadLookAlpha); Row->SetNumberField(TEXT("eye_look_alpha"), Anim->VS2EyeLookAlpha);
    Row->SetBoolField(TEXT("explicit_look_target"), Anim->bVS2ExplicitLookTarget);
    Row->SetArrayField(TEXT("head_component_quaternion"), QuatValues(Mesh->GetSocketTransform(TEXT("Head"),RTS_Component).GetRotation()));
    Row->SetArrayField(TEXT("left_eye_component_quaternion"), QuatValues(Mesh->GetSocketTransform(TEXT("LeftEye"),RTS_Component).GetRotation()));
    Row->SetArrayField(TEXT("right_eye_component_quaternion"), QuatValues(Mesh->GetSocketTransform(TEXT("RightEye"),RTS_Component).GetRotation()));
    if (Controller->PlayerCameraManager)
    {
        Row->SetArrayField(TEXT("final_camera_world"), VectorValues(Controller->PlayerCameraManager->GetCameraLocation()));
        Row->SetArrayField(TEXT("final_camera_pitch_yaw_roll"), VectorValues(FVector(Controller->PlayerCameraManager->GetCameraRotation().Pitch,
            Controller->PlayerCameraManager->GetCameraRotation().Yaw, Controller->PlayerCameraManager->GetCameraRotation().Roll)));
    }
    TArray<TSharedPtr<FJsonValue>> Bones;
    for (const auto& Probe : ObservedBones)
    {
        const FTransform InAnchor = Mesh->GetSocketTransform(Probe.Bone,RTS_Component).GetRelativeTransform(Mesh->GetSocketTransform(Probe.Anchor,RTS_Component));
        const int32 Index = Mesh->GetBoneIndex(Probe.Bone), Parent = ExpectedMesh->GetRefSkeleton().GetParentIndex(Index);
        const FTransform Local = Parent == INDEX_NONE ? Mesh->GetSocketTransform(Probe.Bone,RTS_Component)
            : Mesh->GetSocketTransform(Probe.Bone,RTS_Component).GetRelativeTransform(Mesh->GetSocketTransform(ExpectedMesh->GetRefSkeleton().GetBoneName(Parent),RTS_Component));
        auto Bone = MakeShared<FJsonObject>(); Bone->SetStringField(TEXT("bone"), Probe.Bone.ToString()); Bone->SetStringField(TEXT("anchor"), Probe.Anchor.ToString());
        Bone->SetStringField(TEXT("family"), Probe.Family); Bone->SetArrayField(TEXT("position_in_anchor_cm"),VectorValues(InAnchor.GetLocation()));
        Bone->SetArrayField(TEXT("rotation_in_anchor_xyzw"),QuatValues(InAnchor.GetRotation()));
        Bone->SetNumberField(TEXT("local_angle_from_bind_degrees"), FMath::RadiansToDegrees(Local.GetRotation().AngularDistance(ExpectedMesh->GetRefSkeleton().GetRefBonePose()[Index].GetRotation())));
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    Row->SetArrayField(TEXT("secondary_bones"),Bones);
    return Row;
}

TSharedPtr<FJsonObject> AHCM5VS2HeroExerciseDirector::ObserveProportions(const FString& MeasurementLabel) const
{
    auto Row=MakeShared<FJsonObject>();
    const auto* Mesh=Character->GetMesh();
    const auto* Capsule=Character->GetCapsuleComponent();
    const auto* Movement=Character->GetCharacterMovement();
    Row->SetStringField(TEXT("label"),MeasurementLabel);
    Row->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.HeroProportionLandmarks.v1"));
    Row->SetNumberField(TEXT("frame"),double(GFrameCounter));
    Row->SetNumberField(TEXT("world_seconds"),GetWorld()->GetTimeSeconds());
    Row->SetStringField(TEXT("phase"),Phases[PhaseIndex].Label);
    Row->SetNumberField(TEXT("phase_seconds"),PhaseElapsed);
    Row->SetStringField(TEXT("character_class"),Character->GetClass()->GetPathName());
    Row->SetStringField(TEXT("mesh"),GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
    Row->SetStringField(TEXT("animation_class"),GetPathNameSafe(Mesh->GetAnimInstance()->GetClass()));
    Row->SetArrayField(TEXT("velocity_cm_s"),VectorValues(Character->GetVelocity()));
    const bool Stationary=Movement->IsMovingOnGround() && Character->GetVelocity().Size()<1.;
    Row->SetBoolField(TEXT("stationary_grounded_under_1_cm_s"),Stationary);
    Row->SetStringField(TEXT("status"),Stationary?TEXT("OBSERVED_STATIONARY_LANDMARKS_NOT_ART_PASS"):TEXT("OBSERVED_NONSTATIONARY_NOT_STANDING_PROPORTION"));
    Row->SetStringField(TEXT("pose_scope"),TEXT("PostUpdateWork after actual mesh tick prerequisite; live evaluated idle pose, not forced reference pose or exact GPU frame"));
    auto Transform=[](const FTransform& Value)
    {
        auto T=MakeShared<FJsonObject>();
        T->SetArrayField(TEXT("location_cm"),VectorValues(Value.GetLocation()));
        T->SetArrayField(TEXT("rotation_xyzw"),QuatValues(Value.GetRotation()));
        T->SetArrayField(TEXT("scale_xyz"),VectorValues(Value.GetScale3D()));
        return T;
    };
    Row->SetObjectField(TEXT("actor_world_transform"),Transform(Character->GetActorTransform()));
    Row->SetObjectField(TEXT("mesh_world_transform"),Transform(Mesh->GetComponentTransform()));
    Row->SetObjectField(TEXT("mesh_relative_transform"),Transform(Mesh->GetRelativeTransform()));
    Row->SetStringField(TEXT("mesh_attach_parent"),GetPathNameSafe(Mesh->GetAttachParent()));
    auto Collision=MakeShared<FJsonObject>();
    Collision->SetObjectField(TEXT("world_transform"),Transform(Capsule->GetComponentTransform()));
    Collision->SetNumberField(TEXT("unscaled_radius_cm"),Capsule->GetUnscaledCapsuleRadius());
    Collision->SetNumberField(TEXT("unscaled_half_height_cm"),Capsule->GetUnscaledCapsuleHalfHeight());
    Collision->SetNumberField(TEXT("scaled_radius_cm"),Capsule->GetScaledCapsuleRadius());
    Collision->SetNumberField(TEXT("scaled_half_height_cm"),Capsule->GetScaledCapsuleHalfHeight());
    Collision->SetStringField(TEXT("scope"),TEXT("Collision dimensions, never naked body or mesh height"));
    Row->SetObjectField(TEXT("capsule"),Collision);
    auto Ground=[this,Capsule](const FVector& Point)
    {
        auto G=MakeShared<FJsonObject>();
        const FVector Start(Point.X,Point.Y,FMath::Max(Point.Z,Capsule->GetComponentLocation().Z)+30.);
        const FVector End(Point.X,Point.Y,Capsule->GetComponentLocation().Z-1000.);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2HeroProportionGround),false,Character);
        FHitResult Hit;
        const bool Found=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params) && Hit.bBlockingHit;
        G->SetStringField(TEXT("query"),TEXT("Visibility simple line, own Character ignored, no collision changes"));
        G->SetArrayField(TEXT("trace_start_cm"),VectorValues(Start));G->SetArrayField(TEXT("trace_end_cm"),VectorValues(End));
        G->SetBoolField(TEXT("blocking_hit"),Found);
        if (Found)
        {
            G->SetArrayField(TEXT("impact_point_cm"),VectorValues(Hit.ImpactPoint));
            G->SetArrayField(TEXT("impact_normal"),VectorValues(Hit.ImpactNormal));
            G->SetStringField(TEXT("actor"),GetPathNameSafe(Hit.GetActor()));
            G->SetStringField(TEXT("component"),GetPathNameSafe(Hit.GetComponent()));
            G->SetBoolField(TEXT("walkable_by_current_movement"),Character->GetCharacterMovement()->IsWalkable(Hit));
            G->SetNumberField(TEXT("point_vertical_above_hit_cm"),Point.Z-Hit.ImpactPoint.Z);
        }
        return G;
    };
    Row->SetObjectField(TEXT("capsule_center_ground_trace"),Ground(Capsule->GetComponentLocation()));
    TArray<TSharedPtr<FJsonValue>> Bones;
    for (FName Name:{FName(TEXT("Hips")),FName(TEXT("Chest")),FName(TEXT("Neck")),FName(TEXT("Head")),
        FName(TEXT("LeftEye")),FName(TEXT("RightEye")),FName(TEXT("Foot_L")),FName(TEXT("Foot_R")),FName(TEXT("Toe_L")),FName(TEXT("Toe_R"))})
    {
        auto Bone=MakeShared<FJsonObject>();const int32 Index=Mesh->GetBoneIndex(Name);
        Bone->SetStringField(TEXT("name"),Name.ToString());Bone->SetNumberField(TEXT("index"),Index);
        Bone->SetBoolField(TEXT("exists"),Index!=INDEX_NONE);
        if (Index!=INDEX_NONE)
        {
            const FTransform World=Mesh->GetSocketTransform(Name,RTS_World);
            Bone->SetObjectField(TEXT("world_transform"),Transform(World));
            Bone->SetObjectField(TEXT("component_transform"),Transform(Mesh->GetSocketTransform(Name,RTS_Component)));
            Bone->SetObjectField(TEXT("ground_trace"),Ground(World.GetLocation()));
        }
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    Row->SetArrayField(TEXT("skeletal_landmarks"),Bones);
    const bool EyesPresent=Mesh->GetBoneIndex(TEXT("LeftEye"))!=INDEX_NONE && Mesh->GetBoneIndex(TEXT("RightEye"))!=INDEX_NONE;
    Row->SetBoolField(TEXT("eye_midpoint_available"),EyesPresent);
    if (EyesPresent)
    {
        const FVector Eyes=(Mesh->GetSocketLocation(TEXT("LeftEye"))+Mesh->GetSocketLocation(TEXT("RightEye")))*.5;
        Row->SetArrayField(TEXT("eye_midpoint_world_cm"),VectorValues(Eyes));
        Row->SetObjectField(TEXT("eye_midpoint_ground_trace"),Ground(Eyes));
    }
    Row->SetNumberField(TEXT("pawn_base_eye_height_cm"),Character->BaseEyeHeight);
    const auto* PCM=Controller->PlayerCameraManager.Get();
    Row->SetBoolField(TEXT("final_camera_available"),PCM!=nullptr);
    if (PCM)
    {
        Row->SetArrayField(TEXT("final_camera_world_cm"),VectorValues(PCM->GetCameraLocation()));
        const FRotator Rotation=PCM->GetCameraRotation();
        Row->SetArrayField(TEXT("final_camera_pitch_yaw_roll"),VectorValues(FVector(Rotation.Pitch,Rotation.Yaw,Rotation.Roll)));
        Row->SetNumberField(TEXT("final_camera_fov_degrees"),PCM->GetFOVAngle());
        Row->SetStringField(TEXT("final_view_target"),GetPathNameSafe(Controller->GetViewTarget()));
    }
    Row->SetStringField(TEXT("camera_scope"),TEXT("Actual PCM viewpoint of this existing exercise camera, not gameplay first-person eye height or an OS mouse test"));
    Row->SetStringField(TEXT("height_boundary"),TEXT("Head bone is not crown; Foot/Toe bones are not shoe sole; no hair, halo or Actor bounds maximum is labelled body height. CPU/GPU surface vertex, naked body/crown/sole height and eight-NPC comparison NOT_RUN by this landmark diagnostic."));
    Row->SetBoolField(TEXT("cpu_skinned_vertex_measurement_performed"),false);
    Row->SetStringField(TEXT("mutation_scope"),TEXT("Read-only transforms/bones and bounded traces; no scale, capsule, pose, camera, physics, materials or asset writes"));
    return Row;
}

bool AHCM5VS2HeroExerciseDirector::ObserveGait(float DeltaSeconds,FString& Failure)
{
    if (GaitSamples.Num() >= 8192) { Failure=TEXT("Bounded 8192 final-pose gait samples exceeded"); return false; }
    auto* Mesh=Character->GetMesh(); auto* Anim=Mesh->GetAnimInstance();
    const double Time=GetWorld()->GetTimeSeconds(), DT=Time-PreviousGaitTime;
    const float Speed=Character->GetVelocity().Size2D(); const double Yaw=Character->GetActorRotation().Yaw;
    const bool Grounded=Character->GetCharacterMovement()->IsMovingOnGround();
    auto Row=MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("frame"),double(GFrameCounter));Row->SetNumberField(TEXT("world_seconds"),Time);
    Row->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-StartedAt);Row->SetNumberField(TEXT("tick_delta_seconds"),DeltaSeconds);
    Row->SetStringField(TEXT("phase"),Phases[PhaseIndex].Label);Row->SetNumberField(TEXT("phase_seconds"),PhaseElapsed);
    Row->SetArrayField(TEXT("actor_world"),VectorValues(Character->GetActorLocation()));Row->SetNumberField(TEXT("actor_yaw"),Yaw);
    Row->SetArrayField(TEXT("velocity_cm_s"),VectorValues(Character->GetVelocity()));Row->SetNumberField(TEXT("speed_cm_s"),Speed);
    Row->SetBoolField(TEXT("moving_on_ground"),Grounded);Row->SetBoolField(TEXT("falling"),Character->GetCharacterMovement()->IsFalling());
    Row->SetNumberField(TEXT("movement_max_walk_speed"),Character->GetCharacterMovement()->MaxWalkSpeed);
    Row->SetNumberField(TEXT("mesh_global_animation_rate"),Mesh->GlobalAnimRateScale);
    Row->SetStringField(TEXT("pose_observation_scope"),TEXT("PostUpdateWork after mesh tick prerequisite; native final bone and evaluated curve readback, not exact GPU screenshot frame"));
    Row->SetNumberField(TEXT("predicted_lod"),Mesh->GetPredictedLODLevel());
    const bool R2=GaitRevisionTwoOnly();
    const auto* Motion=CastChecked<UHCM5VS2LookAnimInstance>(Anim);
    const double BodyDirection=Speed>1 ? FMath::FindDeltaAngleDegrees(Yaw,Character->GetVelocity().Rotation().Yaw) : 0;
    if (R2)
    {
        auto M=MakeShared<FJsonObject>();
        M->SetBoolField(TEXT("eligible"),Motion->bVS2MotionEligible);
        M->SetNumberField(TEXT("pose_index"),Motion->VS2MotionPoseIndex);
        M->SetNumberField(TEXT("pose_elapsed"),Motion->VS2MotionElapsed);
        M->SetNumberField(TEXT("ground_speed_cm_s"),Motion->VS2GroundSpeed);
        M->SetNumberField(TEXT("body_relative_velocity_direction_degrees"),BodyDirection);
        M->SetNumberField(TEXT("sample_direction_degrees"),Motion->VS2SampleDirection);
        M->SetNumberField(TEXT("orientation_warp_degrees"),Motion->VS2OrientationAngle);
        M->SetNumberField(TEXT("warp_alpha_input"),Motion->VS2WarpAlpha);
        M->SetNumberField(TEXT("stride_scale_input"),Motion->VS2StrideScale);
        M->SetNumberField(TEXT("nominal_root_speed_previous_evaluated_curve_world_cm_s"),Motion->VS2NominalRootSpeed);
        M->SetArrayField(TEXT("stride_direction_component_space"),VectorValues(Motion->VS2StrideDirection));
        M->SetBoolField(TEXT("flight_pose_eligible"),Motion->bVS2FlightPoseEligible);
        TArray<TSharedPtr<FJsonValue>> Curves;
        for (FName Name:{FName(TEXT("VS2NominalRootSpeed")),FName(TEXT("enable_strafewarping")),FName(TEXT("enable_warping")),FName(TEXT("enable_footplacement"))})
        {
            float Value=0;const bool Present=Mesh->GetCurveValue(Name,0,Value);
            auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("name"),Name.ToString());
            C->SetBoolField(TEXT("present"),Present);if(Present)C->SetNumberField(TEXT("value"),Value);
            Curves.Add(MakeShared<FJsonValueObject>(C));
        }
        M->SetArrayField(TEXT("final_evaluated_curves"),Curves);
        M->SetStringField(TEXT("scope"),TEXT("Actual AnimInstance inputs plus final evaluated curves; input alpha is not node internal weight or GPU sole proof"));
        Row->SetObjectField(TEXT("r2_motion"),M);
        if (FootPlacementOnly())
        {
            auto Placement=MakeShared<FJsonObject>();
            Placement->SetStringField(TEXT("scope"),TEXT("Pre-lock measurements via final evaluated curves (possibly blend-weighted); final component/world bones after native placement plus existing LegIK. Not GPU shoe proof."));
            TArray<TSharedPtr<FJsonValue>> PlacementCurves;
            for (FName Name:{FName(TEXT("VS2PlacementSpeed_L")),FName(TEXT("VS2PlacementSpeed_R")),
                FName(TEXT("VS2PlacementWorldSpeed_L")),FName(TEXT("VS2PlacementWorldSpeed_R")),
                FName(TEXT("VS2PlacementInputValid")),FName(TEXT("VS2PlacementDisableLock")),
                FName(TEXT("VS2PlacementPlaneZ")),FName(TEXT("VS2PlacementUnitScale")),
                FName(TEXT("VS2PlacementInputHipsX")),FName(TEXT("VS2PlacementInputHipsY")),FName(TEXT("VS2PlacementInputHipsZ")),
                FName(TEXT("VS2PlacementInputGoalLX")),FName(TEXT("VS2PlacementInputGoalLY")),FName(TEXT("VS2PlacementInputGoalLZ")),
                FName(TEXT("VS2PlacementInputGoalRX")),FName(TEXT("VS2PlacementInputGoalRY")),FName(TEXT("VS2PlacementInputGoalRZ"))})
            {
                float Value=0;const bool Present=Mesh->GetCurveValue(Name,0,Value);
                auto Curve=MakeShared<FJsonObject>();Curve->SetStringField(TEXT("name"),Name.ToString());
                Curve->SetBoolField(TEXT("present"),Present);if(Present)Curve->SetNumberField(TEXT("value"),Value);
                PlacementCurves.Add(MakeShared<FJsonValueObject>(Curve));
            }
            Placement->SetArrayField(TEXT("curves"),PlacementCurves);
            TArray<TSharedPtr<FJsonValue>> FinalBones;
            for (FName Name:{FName(TEXT("Hips")),FName(TEXT("VB VS2_PlacementFloor")),
                FName(TEXT("VB VS2_IKFoot_L")),FName(TEXT("VB VS2_IKFoot_R"))})
            {
                auto Bone=MakeShared<FJsonObject>();const int32 Index=Mesh->GetBoneIndex(Name);
                Bone->SetStringField(TEXT("name"),Name.ToString());Bone->SetBoolField(TEXT("present"),Index!=INDEX_NONE);
                if (Index!=INDEX_NONE)
                {
                    const FTransform WorldBone=Mesh->GetBoneTransform(Index);
                    const FTransform LocalBone=WorldBone.GetRelativeTransform(Mesh->GetComponentTransform());
                    Bone->SetArrayField(TEXT("world_cm"),VectorValues(WorldBone.GetLocation()));
                    Bone->SetArrayField(TEXT("component_cm"),VectorValues(LocalBone.GetLocation()));
                    Bone->SetArrayField(TEXT("world_up"),VectorValues(WorldBone.GetRotation().GetUpVector()));
                    Bone->SetArrayField(TEXT("component_up"),VectorValues(LocalBone.GetRotation().GetUpVector()));
                }
                FinalBones.Add(MakeShared<FJsonValueObject>(Bone));
            }
            Placement->SetArrayField(TEXT("final_bones"),FinalBones);
            Row->SetObjectField(TEXT("foot_placement"),Placement);
        }

    }
    TArray<TSharedPtr<FJsonValue>> Players;
    double RunWeight=0,SprintWeight=0,WalkWeight=0,IdleWeight=0;
    bool bTargetSpace=false,bBadSpace=false; double Direction=0;
    auto Record=[&](const FAnimTickRecord& Tick,const FString& Group,bool bMarkerSync,bool bLeader)
    {
        auto Player=MakeShared<FJsonObject>();Player->SetStringField(TEXT("group"),Group);
        Player->SetStringField(TEXT("asset"),GetPathNameSafe(Tick.SourceAsset.Get()));Player->SetBoolField(TEXT("group_marker_sync"),bMarkerSync);
        Player->SetBoolField(TEXT("group_leader"),bLeader);Player->SetNumberField(TEXT("effective_blend_weight"),Tick.EffectiveBlendWeight);
        Player->SetNumberField(TEXT("play_rate_multiplier"),Tick.PlayRateMultiplier);
        if (Tick.TimeAccumulator) Player->SetNumberField(TEXT("time_accumulator"),*Tick.TimeAccumulator);
        if (Tick.DeltaTimeRecord) Player->SetNumberField(TEXT("asset_tick_delta"),Tick.DeltaTimeRecord->Delta);
        const auto* Space=Cast<UBlendSpace>(Tick.SourceAsset.Get());
        if (Space)
        {
            const bool Target=Space==ActiveGaitSpace;
            bBadSpace |= R2 ? (!Target && Tick.EffectiveBlendWeight>1.e-4f) : Space->GetPathName()!=GaitSpace;
            bTargetSpace |= Target;
            Direction=Tick.BlendSpace.BlendSpacePositionX;
            Player->SetBoolField(TEXT("exact_target_blendspace"),Target);
            Player->SetArrayField(TEXT("blend_input"),VectorValues(FVector(Direction,Tick.BlendSpace.BlendSpacePositionY,0)));
            if (Tick.BlendSpace.BlendFilter) Player->SetArrayField(TEXT("filtered_input"),VectorValues(Tick.BlendSpace.BlendFilter->GetFilterLastOutput()));
            TArray<TSharedPtr<FJsonValue>> Weights;
            // The public sync tick record points to the actual node's evaluated
            // cache. Do not recompute weights or retain these pointers past this call.
            if (Tick.BlendSpace.BlendSampleDataCache) for (const FBlendSampleData& Sample:*Tick.BlendSpace.BlendSampleDataCache)
            {
                auto S=MakeShared<FJsonObject>();const UAnimSequence* Clip=Sample.Animation.Get();
                S->SetNumberField(TEXT("index"),Sample.SampleDataIndex);S->SetStringField(TEXT("animation"),GetPathNameSafe(Clip));
                S->SetNumberField(TEXT("weight"),Sample.GetClampedWeight());S->SetNumberField(TEXT("time"),Sample.Time);
                S->SetNumberField(TEXT("previous_time"),Sample.PreviousTime);S->SetNumberField(TEXT("actual_animation_delta"),Sample.DeltaTimeRecord.Delta);
                S->SetBoolField(TEXT("delta_previous_valid"),Sample.DeltaTimeRecord.IsPreviousValid());
                S->SetNumberField(TEXT("sample_play_rate"),Sample.SamplePlayRate);
                if (Clip)
                {
                    S->SetNumberField(TEXT("sequence_rate_scale"),Clip->RateScale);S->SetNumberField(TEXT("sequence_length"),Clip->GetPlayLength());
                    const double Weight=Tick.EffectiveBlendWeight*Sample.GetClampedWeight();
                    S->SetNumberField(TEXT("effective_final_player_sample_weight"),Weight);
                    if (R2)
                    {
                        if(Target && Clip==GaitRunClip)RunWeight+=Weight;
                        if(Target && Clip==GaitSprintClip)SprintWeight+=Weight;
                        if(Target && Clip==GaitWalkClip)WalkWeight+=Weight;
                        if(Target && Clip==GaitIdleClip)IdleWeight+=Weight;
                    }
                    else
                    {
                        if (Clip->GetName()==TEXT("M_Relaxed_Run_Loop_F_InPlace_SelestiaGAS"))RunWeight+=Weight;
                        if (Clip->GetName()==TEXT("M_Relaxed_Sprint_Loop_F_InPlace_SelestiaGAS"))SprintWeight+=Weight;
                        bGaitWalkBlendObserved |= Clip->GetName()==TEXT("M_Relaxed_Walk_Loop_F_InPlace_SelestiaGAS") && Weight>.01;
                    }
                }
                Weights.Add(MakeShared<FJsonValueObject>(S));
            }
            Player->SetArrayField(TEXT("actual_cached_samples"),Weights);
        }
        else if (!R2 && Tick.SourceAsset && Tick.SourceAsset->GetName()==TEXT("M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS"))
            bGaitIdleObserved |= Tick.EffectiveBlendWeight>.95f && Speed<1.f;
        Players.Add(MakeShared<FJsonValueObject>(Player));
    };
    // These public getters synchronize the proxy with any outstanding evaluation.
    for (const auto& Group:Anim->GetSyncGroupMapRead())
        for (int32 I=0;I<Group.Value.ActivePlayers.Num();++I)
            Record(Group.Value.ActivePlayers[I],Group.Key.ToString(),Group.Value.bCanUseMarkerSync,I==Group.Value.GroupLeaderIndex);
    for (const auto& Tick:Anim->GetUngroupedActivePlayersRead()) Record(Tick,TEXT("Ungrouped"),false,false);
    if (bBadSpace) { Failure=TEXT("Actual active BlendSpace is not the newly saved exact GAS candidate"); return false; }
    Row->SetArrayField(TEXT("actual_active_players"),Players);Row->SetBoolField(TEXT("target_blendspace_active"),bTargetSpace);
    if (R2)
    {
        bGaitIdleObserved |= IdleWeight>.95 && Speed<1;
        bGaitWalkBlendObserved |= WalkWeight>.01;
        // The new node intentionally compresses its sample direction for
        // orientation warping. Use the same physical forward-angle criterion
        // as legacy, not that remapped value, when selecting stance intervals.
        Direction=BodyDirection;
    }
    Row->SetNumberField(TEXT("effective_target_run_weight"),RunWeight);
    Row->SetNumberField(TEXT("effective_target_sprint_weight"),SprintWeight);
    Row->SetNumberField(TEXT("effective_target_walk_weight"),WalkWeight);
    Row->SetNumberField(TEXT("effective_target_idle_weight"),IdleWeight);
    Row->SetNumberField(TEXT("stance_eligibility_direction_degrees"),Direction);
    bGaitTurnObserved |= bTargetSpace && Speed>100 && FMath::Abs(Direction)>5;
    const int32 Steady=Grounded && bTargetSpace && FMath::Abs(Direction)<2
        ? (FMath::Abs(Speed-400)<2 && RunWeight>=.95 ? 0 : FMath::Abs(Speed-650)<2 && SprintWeight>=.95 ? 1 : INDEX_NONE) : INDEX_NONE;
    Row->SetNumberField(TEXT("steady_target_cm_s"),Steady==0?400:Steady==1?650:0);
    if (Steady!=INDEX_NONE)++GaitSteadyFrames[Steady];
    const bool Consecutive=PreviousGaitPhase==PhaseIndex && GFrameCounter==PreviousGaitFrame+1 && PreviousGaitTime>=0 && DT>1.e-6 && DT<=.075;
    Row->SetBoolField(TEXT("consecutive_interval_under_75ms"),Consecutive);
    Row->SetNumberField(TEXT("actual_interval_seconds"),PreviousGaitTime>=0?DT:0);
    TArray<TSharedPtr<FJsonValue>> Feet;
    for (int32 Side=0;Side<2;++Side)
    {
        const FName Foot=Side==0?TEXT("Foot_L"):TEXT("Foot_R"),Toe=Side==0?TEXT("Toe_L"):TEXT("Toe_R");
        const FVector Position=Mesh->GetSocketLocation(Toe);float Contact=0;
        const bool CurvePresent=Mesh->GetCurveValue(Side==0?TEXT("contact_l"):TEXT("contact_r"),0,Contact);
        if (Position.ContainsNaN() || !FMath::IsFinite(Contact))
        { Failure=TEXT("Non-finite actual final toe/contact observation"); return false; }
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(M5VS2GaitGround),false,Character);
        const bool GroundHit=GetWorld()->LineTraceSingleByChannel(Hit,Position+FVector(0,0,100),Position-FVector(0,0,200),ECC_Visibility,Query);
        auto FootRow=MakeShared<FJsonObject>();FootRow->SetStringField(TEXT("side"),Side==0?TEXT("L"):TEXT("R"));
        FootRow->SetArrayField(TEXT("foot_bone_world"),VectorValues(Mesh->GetSocketLocation(Foot)));
        if (R2)
        {
            const FName Goal=Side==0?TEXT("VB VS2_IKFoot_L"):TEXT("VB VS2_IKFoot_R");
            FootRow->SetArrayField(TEXT("final_virtual_ik_foot_world"),VectorValues(Mesh->GetSocketLocation(Goal)));
            FootRow->SetArrayField(TEXT("final_virtual_ik_root_world"),VectorValues(Mesh->GetSocketLocation(TEXT("VB VS2_IKRoot"))));
        }
        FootRow->SetArrayField(TEXT("toe_bone_world"),VectorValues(Position));FootRow->SetBoolField(TEXT("contact_curve_present"),CurvePresent);
        if (CurvePresent)FootRow->SetNumberField(TEXT("contact_curve"),Contact);
        FootRow->SetBoolField(TEXT("ground_trace_hit"),GroundHit);
        if (GroundHit)
        {
            FootRow->SetArrayField(TEXT("ground_world"),VectorValues(Hit.ImpactPoint));FootRow->SetArrayField(TEXT("ground_normal"),VectorValues(Hit.ImpactNormal));
            FootRow->SetStringField(TEXT("ground_actor"),GetPathNameSafe(Hit.GetActor()));
            FootRow->SetStringField(TEXT("ground_component"),GetPathNameSafe(Hit.GetComponent()));
            FootRow->SetNumberField(TEXT("toe_bone_ground_gap_cm"),Position.Z-Hit.ImpactPoint.Z);
        }
        const bool Eligible=Consecutive && bPreviousGaitGrounded && Grounded && GroundHit && bPreviousGaitGroundHit[Side]
            && CurvePresent && Contact>=.8f && PreviousGaitContact[Side]>=.8f && Steady!=INDEX_NONE && PreviousGaitSteady==Steady
            && FMath::Abs(FMath::FindDeltaAngleDegrees(PreviousGaitYaw,Yaw))<1;
        FootRow->SetBoolField(TEXT("steady_stance_interval_eligible"),Eligible);
        if (Consecutive)
        {
            const FVector Velocity=(Position-PreviousGaitToes[Side])/DT;
            FootRow->SetArrayField(TEXT("observed_toe_world_velocity_cm_s"),VectorValues(Velocity));
            FootRow->SetNumberField(TEXT("observed_toe_horizontal_speed_cm_s"),Velocity.Size2D());
        }
        if (Eligible)++GaitContactIntervals[Steady][Side];
        PreviousGaitToes[Side]=Position;PreviousGaitContact[Side]=CurvePresent?Contact:0;bPreviousGaitGroundHit[Side]=GroundHit;
        Feet.Add(MakeShared<FJsonValueObject>(FootRow));
    }
    Row->SetArrayField(TEXT("feet"),Feet);GaitSamples.Add(Row);
    PreviousGaitTime=Time;PreviousGaitFrame=GFrameCounter;PreviousGaitPhase=PhaseIndex;PreviousGaitSteady=Steady;
    PreviousGaitYaw=Yaw;bPreviousGaitGrounded=Grounded;
    return true;
}

void AHCM5VS2HeroExerciseDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bEnabled || bStopped) return;
    const double Now = FPlatformTime::Seconds();
    if (bDone)
    {
        if (SoleCaptureOnly() && SoleRecorder && !bSoleAudioFinished)
        {
            if (SoleRecorder->IsVS2AudioExportPending())
            {
                if (Now-FinishedAt > 30.)
                {
                    bSoleAudioFinished = true;
                    SoleFinalStatus = TEXT("FAIL");
                    SoleFinalDetail = TEXT("Native audio export deadline exceeded; captured video/sole evidence preserved, no silent audio success");
                    WriteReport(SoleFinalStatus, SoleFinalDetail);
                    // Automatic review may exit normally with failure. Explicit
                    // Esc/P/focus stops take the separate permanent-stop path.
                    SetActorTickEnabled(bAutoQuit);
                }
                return;
            }
            bSoleAudioFinished = true;
            WriteReport(SoleFinalStatus, SoleFinalDetail);
        }
        if (bAutoQuit && Now-FinishedAt > 2 && !FScreenshotRequest::IsScreenshotRequested())
        {
            bAutoQuit = false; SetActorTickEnabled(false);
            if (SoleCaptureOnly()) FPlatformMisc::RequestExitWithStatus(false, SoleFinalStatus == TEXT("PASS") ? 0 : 1, TEXT("M5VS2 isolated sole capture complete"));
            else FPlatformMisc::RequestExit(false,TEXT("M5VS2 isolated hero exercise complete"));
        }
        return;
    }
    if (Now-StartedAt > 180) { Finish(TEXT("FAIL"),TEXT("Bounded 180 second exercise deadline")); return; }
    FString Failure;
    if (!bInitialized)
    { if (!Initialize(Failure)) Finish(TEXT("BLOCKED"),Failure); return; }
    if (!ValidateBinding(Failure)) { Finish(TEXT("FAIL"),Failure); return; }
    ++BindingChecks;
    if (SoleCaptureOnly())
    {
        if (Controller->IsGameplayFocused()) bSoleHadFocus = true;
        else if (bSoleHadFocus)
        { StopSoleCapture(TEXT("Sole recording gameplay focus lost; no automatic restart")); return; }
        else return;
        if (UGameplayStatics::IsGamePaused(this) || Controller->IsPauseMenuOpen())
        { StopSoleCapture(TEXT("Sole recording pause observed; no automatic restart")); return; }
        if (SoleRecorder && PhaseIndex > 0
            && IFileManager::Get().FileExists(*(SoleRecorder->GetCaptureDirectory()/TEXT("capture.json"))))
        { Finish(TEXT("FAIL"),TEXT("Native recorder stopped before the unchanged gait route completed")); return; }
    }
    if (PendingCapture)
    {
        if (bScreenshotProcessed && !FScreenshotRequest::IsScreenshotRequested() && GFrameCounter > RequestFrame)
        {
            const bool Valid = ValidPNG(PendingPNG);
            PendingCapture->SetStringField(TEXT("status"),Valid ? TEXT("PASS") : TEXT("FAIL"));
            PendingCapture->SetNumberField(TEXT("file_bytes"),IFileManager::Get().FileSize(*PendingPNG));
            Captures.Add(PendingCapture); PendingCapture.Reset();
            if (!Valid) { Finish(TEXT("FAIL"),TEXT("Native PNG is absent or not 1920x1080")); return; }
        }
        else if (Now-ScreenshotRequestedAt > 15) { Finish(TEXT("FAIL"),TEXT("Screenshot timed out")); return; }
    }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (Character->GetActorLocation().ContainsNaN() || FVector::Dist2D(Origin,Character->GetActorLocation()) > 1300
        || Character->GetActorLocation().Z < Origin.Z-100)
    { Finish(TEXT("FAIL"),TEXT("Character left the bounded stage or has invalid position")); return; }
    PhaseElapsed += FMath::Max(0.f,DeltaSeconds); SimulationElapsed += FMath::Max(0.f,DeltaSeconds);
    const auto& Phase = Phases[PhaseIndex];
    UpdatePhase(DeltaSeconds);
    if (Phase.Action == TEXT("Warmup"))
    {
        if (Now >= NextShaderCheck)
        {
            NextShaderCheck = Now+.5;
            const bool Ready = CheckShaders(Failure);
            if (!Failure.IsEmpty()) { Finish(TEXT("FAIL"),Failure); return; }
            if (Ready && ShaderReadyFrame == 0) ShaderReadyFrame = GFrameCounter;
            if (!Ready) ShaderReadyFrame = 0;
        }
        if (PhaseElapsed > 120) { Finish(TEXT("FAIL"),TEXT("Shader readiness deadline")); return; }
        if (ShaderReadyFrame == 0 || GFrameCounter-ShaderReadyFrame < 30) return;
        if (GaitRevisionTwoOnly() && ProportionSamples.IsEmpty() && PhaseElapsed>=1.
            && Character->GetCharacterMovement()->IsMovingOnGround() && Character->GetVelocity().Size()<1.)
            ProportionSamples.Add(ObserveProportions(TEXT("settled_warmup")));
        if (SoleCaptureOnly() && PhaseElapsed >= Phase.Duration && !PrepareSoleRecording(Failure))
        {
            if (!Failure.IsEmpty()) Finish(TEXT("FAIL"), Failure);
            return;
        }
    }
    if (GaitReviewOnly() && Phase.Action!=TEXT("Warmup") && !ObserveGait(DeltaSeconds,Failure))
    { Finish(TEXT("FAIL"),Failure); return; }
    if (SoleCaptureOnly() && SoleRecorder && SoleRecorder->HasCapturedFirstFrame() && !ObserveSole(Failure))
    { Finish(TEXT("FAIL"), Failure); return; }
    if (Now >= NextSampleAt && Phase.Action != TEXT("Warmup"))
    {
        NextSampleAt = Now+.1;
        if (Samples.Num() >= 900) { Finish(TEXT("FAIL"),TEXT("Bounded 900 sample limit")); return; }
        Samples.Add(Observe());
        if (Expression->GetBlinkWeight() >= .9f) ++BlinkClosedSamples;
        if (Character->GetCharacterMovement()->IsFalling()) ++AirborneSamples;
        MaximumSpeed = FMath::Max(MaximumSpeed,float(Character->GetVelocity().Size2D()));
        MaximumTalkWeight = FMath::Max(MaximumTalkWeight,Expression->GetTalkingWeight());
        MaximumLookAlpha = FMath::Max(MaximumLookAlpha,CastChecked<UHCM5VS2LookAnimInstance>(Character->GetMesh()->GetAnimInstance())->VS2HeadLookAlpha);
        for (int32 I=0; I<ObservedBones.Num(); ++I)
        {
            const auto& Bone = ObservedBones[I];
            const FTransform InAnchor = Character->GetMesh()->GetSocketTransform(Bone.Bone,RTS_Component)
                .GetRelativeTransform(Character->GetMesh()->GetSocketTransform(Bone.Anchor,RTS_Component));
            if (InAnchor.ContainsNaN()) { Finish(TEXT("FAIL"),TEXT("Non-finite secondary bone transform")); return; }
            if (FirstBoneInAnchor.Num() <= I) FirstBoneInAnchor.Add(InAnchor);
            MaxBoneAngleDegrees[I] = FMath::Max(MaxBoneAngleDegrees[I],FMath::RadiansToDegrees(InAnchor.GetRotation().AngularDistance(FirstBoneInAnchor[I].GetRotation())));
            MaxBoneDisplacement[I] = FMath::Max(MaxBoneDisplacement[I],FVector::Distance(InAnchor.GetLocation(),FirstBoneInAnchor[I].GetLocation()));
        }
    }
    const bool BlinkShot = Phase.Label == TEXT("Rest_Blink");
    if (!bCaptureRequested && Phase.CaptureAt >= 0 && PhaseElapsed >= Phase.CaptureAt
        && (!BlinkShot || Expression->GetBlinkWeight() >= .9f) && !PendingCapture && !FScreenshotRequest::IsScreenshotRequested()) RequestCapture();
    if (PhaseElapsed >= Phase.Duration && !PendingCapture)
    {
        if (Phase.CaptureAt >= 0 && !bCaptureRequested) { Finish(TEXT("FAIL"),TEXT("Planned phase capture was not observed: ")+Phase.Label); return; }
        if (PhaseIndex+1 < Phases.Num())
        { if (!BeginPhase(PhaseIndex+1,Failure)) Finish(TEXT("FAIL"),Failure); }
        else
        {
            int32 ExpectedShots=0; for (const auto& P : Phases) ExpectedShots += P.CaptureAt >= 0;
            const bool GaitCoverage=GaitSteadyFrames[0]>5 && GaitSteadyFrames[1]>5 && bGaitIdleObserved && bGaitWalkBlendObserved && bGaitTurnObserved
                && GaitContactIntervals[0][0]>1 && GaitContactIntervals[0][1]>1 && GaitContactIntervals[1][0]>1 && GaitContactIntervals[1][1]>1 && AirborneSamples>0;
            const bool Completed = Captures.Num() == ExpectedShots && (GaitReviewOnly()?GaitCoverage:(FaceReviewOnly()
                || (BlinkClosedSamples > 0 && AirborneSamples > 0
                && MaximumSpeed > Character->WalkSpeed*1.2f && MaximumTalkWeight > .05f && MaximumLookAlpha > .8f)));
            Finish(Completed ? TEXT("PASS") : TEXT("FAIL"),Completed
                ? (GaitReviewOnly()?TEXT("Native gait coverage and captures completed; measured toe sliding and visual naturalness require review, no zero-slide assertion")
                    :(FaceReviewOnly() ? TEXT("Six native emotion captures and binding checks completed; morph diagnostics and facial appearance need review; gait/blink/speech/look exercise NOT_RUN in this short mode")
                    : TEXT("Native sequence/captures/binding checks completed; gait, cloth collision and expression beauty remain USER_REVIEW")))
                : TEXT("Sequence ended with missing required coverage/capture; inspect mode-specific recorded values"));
        }
    }
}

void AHCM5VS2HeroExerciseDirector::RequestCapture()
{
    PendingPNG = RunDirectory / FString::Printf(TEXT("%02d_%s.png"),Captures.Num(),*Phases[PhaseIndex].Label);
    if (IFileManager::Get().FileExists(*PendingPNG)) { Finish(TEXT("FAIL"),TEXT("Refusing screenshot overwrite")); return; }
    PendingCapture = Observe(); PendingCapture->SetStringField(TEXT("file"),PendingPNG);
    if (GaitRevisionTwoOnly() && Captures.IsEmpty())
    {
        auto Proportion=ObserveProportions(TEXT("first_native_capture_request"));
        Proportion->SetStringField(TEXT("associated_png"),PendingPNG);
        ProportionSamples.Add(Proportion);
        PendingCapture->SetObjectField(TEXT("proportion_landmarks"),Proportion);
    }
    if (GaitReviewOnly() && !GaitSamples.IsEmpty())
    {
        PendingCapture->SetNumberField(TEXT("gait_sample_index"),GaitSamples.Num()-1);
        PendingCapture->SetStringField(TEXT("gait_frame_relation"),TEXT("Final-mesh observation at native screenshot request; render/callback may occur later, not exact GPU-frame proof"));
    }
    PendingCapture->SetObjectField(TEXT("face_morph_readback"), CaptureFaceMorphReadback(Character->GetMesh()));
    PendingCapture->SetStringField(TEXT("status"),TEXT("NOT_RUN")); PendingCapture->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));
    ScreenshotRequestedAt = FPlatformTime::Seconds(); RequestFrame = GFrameCounter;
    bScreenshotProcessed = false; bCaptureRequested = true;
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}
void AHCM5VS2HeroExerciseDirector::ScreenshotProcessed() { if (!bStopped && PendingCapture) bScreenshotProcessed=true; }
void AHCM5VS2HeroExerciseDirector::ViewportInput(const FInputKeyEventArgs& Event)
{
    if (SoleCaptureOnly() && Event.Event == IE_Pressed && Event.Key == EKeys::P)
    { StopSoleCapture(TEXT("Sole recording P key stop; native capture is not automatically resumed")); return; }
    if (bEnabled && !bStopped && Event.Event == IE_Pressed && Event.Key == EKeys::Escape)
    {
        bStopped=true; bAutoQuit=false; StopFrame=GFrameCounter;
        if (PendingCapture && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
        WriteReport(TEXT("NOT_RUN"),TEXT("Viewport Escape user stop; no subsequent native movement, camera, material, expression, capture, restore or exit"));
        SetActorTickEnabled(false);
        UE_LOG(LogTemp,Display,TEXT("M5VS2_HERO_EXERCISE_USER_STOP frame=%llu"),StopFrame);
    }
}
void AHCM5VS2HeroExerciseDirector::Restore()
{
    if (bRestored || bStopped) return;
    if (Character)
    {
        Character->SetSprinting(false); Character->StopJumping(); Character->StopBodyFacing();
        if (Character->GetMesh()) for (int32 I=0; I<Originals.Num(); ++I) Character->GetMesh()->SetMaterial(I,Originals[I]);
    }
    if (Expression) { Expression->StopSpeaking(); Expression->ClearLookTarget(); Expression->SetEmotion(TEXT("Neutral")); }
    if (Controller)
    { if (OriginalViewTarget.IsValid()) Controller->SetViewTarget(OriginalViewTarget.Get()); if (AHUD* HUD=Controller->GetHUD()) HUD->bShowHUD=bOriginalHUD; }
    if (Camera) Camera->Destroy(); if (LookTarget) LookTarget->Destroy(); bRestored=true;
}
void AHCM5VS2HeroExerciseDirector::Finish(const FString& Status,const FString& Detail)
{
    if (bStopped || bDone) return;
    const bool RequestedAutoQuit = bAutoQuit;
    bDone=true; FinishedAt=FPlatformTime::Seconds(); bAutoQuit &= Status==TEXT("PASS");
    SoleFinalStatus = Status; SoleFinalDetail = Detail;
    if (SoleCaptureOnly())
    {
        bAutoQuit = RequestedAutoQuit;
        if (SoleRecorder)
        {
            SoleRecorder->FinishVS2GameplayCapture();
            FString Text;
            const FString Path = SoleRecorder->GetCaptureDirectory()/TEXT("capture.json");
            if (FFileHelper::LoadFileToString(Text, *Path))
            {
                auto Reader = TJsonReaderFactory<>::Create(Text);
                FJsonSerializer::Deserialize(Reader, SoleRecordingReport);
            }
        }
        FString CaptureStatus, StopReason;
        bool GeometryValid = false, AudioRequested = false;
        double Duration = 0;
        const bool Recorded = SoleRecordingReport
            && SoleRecordingReport->TryGetStringField(TEXT("status"), CaptureStatus)
            && SoleRecordingReport->TryGetStringField(TEXT("stop_reason"), StopReason)
            && SoleRecordingReport->TryGetBoolField(TEXT("capture_geometry_valid"), GeometryValid)
            && SoleRecordingReport->TryGetBoolField(TEXT("audio_export_requested"), AudioRequested)
            && SoleRecordingReport->TryGetNumberField(TEXT("wall_seconds"), Duration)
            && CaptureStatus == TEXT("CAPTURED_PENDING_REVIEW") && StopReason == TEXT("vs2_gameplay_sequence_finished")
            && GeometryValid && AudioRequested && Duration >= 20. && Duration <= 30. && SoleSamples.Num() > 100;
        if (Status == TEXT("PASS") && !Recorded)
        {
            SoleFinalStatus = TEXT("FAIL");
            SoleFinalDetail = TEXT("Original gait coverage completed but bounded native sole/video recording failed; inspect preserved evidence");
        }
        bSoleAudioFinished = !SoleRecorder || !SoleRecorder->IsVS2AudioExportPending();
    }
    if (PendingCapture && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    Restore();
    WriteReport(SoleCaptureOnly() && !bSoleAudioFinished ? TEXT("NOT_RUN") : SoleFinalStatus, SoleFinalDetail);
    SetActorTickEnabled(bAutoQuit || (SoleCaptureOnly() && !bSoleAudioFinished));
    UE_LOG(LogTemp,Display,TEXT("M5VS2_HERO_EXERCISE_%s %s report=%s"),*SoleFinalStatus,*SoleFinalDetail,*RunDirectory);
}
void AHCM5VS2HeroExerciseDirector::WriteReport(const FString& Status,const FString& Detail)
{
    if (RunDirectory.IsEmpty()) return;
    auto Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("status"),Status); Root->SetStringField(TEXT("detail"),Detail);
    Root->SetStringField(TEXT("input_level"),TEXT("NATIVE_FUNCTION_CALLS_NOT_ACTION_OR_OS_INPUT")); Root->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));
    Root->SetStringField(TEXT("exercise_scope"),GaitRevisionTwoOnly()?TEXT("R2_INTEGRATED_GAS_WARP_FINAL_TOE_MEASUREMENT")
        :(GaitReviewOnly()?TEXT("GAS_NATIVE_GAIT_COVERAGE_AND_FOOT_MEASUREMENT")
        :(FaceReviewOnly() ? TEXT("SIX_AUTHORED_EMOTIONS_ONLY") : TEXT("FULL_AUTHORED_HERO_EXERCISE"))));
    Root->SetStringField(TEXT("run_directory"),RunDirectory); Root->SetStringField(TEXT("map"),GetWorld()->GetMapName());
    Root->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-StartedAt); Root->SetNumberField(TEXT("simulation_seconds"),SimulationElapsed);
    Root->SetBoolField(TEXT("user_stop_latched"),bStopped); Root->SetNumberField(TEXT("stop_frame"),double(StopFrame)); Root->SetBoolField(TEXT("restored"),bRestored);
    Root->SetNumberField(TEXT("exact_runtime_binding_checks"),BindingChecks); Root->SetNumberField(TEXT("blink_closed_samples"),BlinkClosedSamples);
    Root->SetNumberField(TEXT("airborne_samples"),AirborneSamples); Root->SetNumberField(TEXT("maximum_speed_cm_s"),MaximumSpeed);
    Root->SetNumberField(TEXT("maximum_mouth_weight"),MaximumTalkWeight); Root->SetNumberField(TEXT("maximum_head_look_alpha"),MaximumLookAlpha);
    Root->SetStringField(TEXT("expected_character"),GetPathNameSafe(ExpectedCharacterClass.Get())); Root->SetStringField(TEXT("expected_animation"),GetPathNameSafe(ExpectedAnimationClass.Get()));
    Root->SetStringField(TEXT("secondary_motion_scope"),TEXT("Native final bone transforms relative to moving Head/Hips anchors; nonzero response is not surface collision/naturalness proof"));
    TArray<TSharedPtr<FJsonValue>> BoneResults;
    for (int32 I=0; I<MaxBoneAngleDegrees.Num(); ++I)
    {
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("bone"),ObservedBones[I].Bone.ToString()); Row->SetStringField(TEXT("anchor"),ObservedBones[I].Anchor.ToString());
        Row->SetNumberField(TEXT("max_anchor_relative_angle_from_first_sample_degrees"),MaxBoneAngleDegrees[I]); Row->SetNumberField(TEXT("max_anchor_relative_displacement_cm"),MaxBoneDisplacement[I]);
        BoneResults.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("secondary_response"),BoneResults);
    if (ShaderState) Root->SetObjectField(TEXT("shader_readiness"),ShaderState);
    TArray<TSharedPtr<FJsonValue>> SampleValues, CaptureValues;
    for (const auto& Row:Samples) SampleValues.Add(MakeShared<FJsonValueObject>(Row));
    for (const auto& Row:Captures) CaptureValues.Add(MakeShared<FJsonValueObject>(Row));
    if (PendingCapture) CaptureValues.Add(MakeShared<FJsonValueObject>(PendingCapture));
    Root->SetArrayField(TEXT("samples_10hz_actual_wall_intervals"),SampleValues); Root->SetArrayField(TEXT("captures"),CaptureValues);
    if (SoleCaptureOnly())
    {
        auto Sole = MakeShared<FJsonObject>();
        Sole->SetStringField(TEXT("scope"), TEXT("Independent private shoe-view recording of unchanged CMC movement phases. Fixed native LOD0 points; CPU LBS before morph/cloth/WPO, not final GPU surface or visible contact approval."));
        Sole->SetStringField(TEXT("comparison_boundary"), TEXT("Original 400/650 speeds, movement route and every-frame gait eligibility retained. Private unobstructed recording stage and camera differ; do not substitute this capture for baseline performance or numeric acceptance."));
        Sole->SetStringField(TEXT("source_selection"), SoleSelectionEvidence);
        Sole->SetStringField(TEXT("surface_acceptance"), TEXT("USER_REVIEW"));
        Sole->SetStringField(TEXT("interval_qualification"), TEXT("CPU samples at no more than 30Hz. Qualify each sole interval only after checking ALL original gait frames between both endpoints for the same side; latest_gait_sample_index alone is not sufficient."));
        Sole->SetStringField(TEXT("camera_scope"), TEXT("Dedicated visible diagnostic camera follows the real actor with fixed world offset (320,0,-15)cm, target offset (0,0,-60)cm, FOV40. It does not drive actor or animation; actual PCM is recorded."));
        Sole->SetNumberField(TEXT("requested_sample_hz"), 30);
        Sole->SetNumberField(TEXT("first_recorded_frame_observed_world_seconds"), SoleFirstFrameWorld);
        Sole->SetBoolField(TEXT("native_audio_export_pending"), SoleRecorder && SoleRecorder->IsVS2AudioExportPending());
        Sole->SetBoolField(TEXT("native_audio_export_finished_observed"), bSoleAudioFinished
            && SoleRecorder && !SoleRecorder->IsVS2AudioExportPending());
        Sole->SetStringField(TEXT("recording_directory"), SoleRecorder ? SoleRecorder->GetCaptureDirectory() : FString());
        if (SoleRecordingReport) Sole->SetObjectField(TEXT("native_capture_report"), SoleRecordingReport);
        TArray<TSharedPtr<FJsonValue>> SurfaceFrames;
        for (const auto& Frame : SoleSamples) SurfaceFrames.Add(MakeShared<FJsonValueObject>(Frame));
        Sole->SetArrayField(TEXT("cpu_surface_samples"), SurfaceFrames);
        Root->SetObjectField(TEXT("sole_capture"), Sole);
    }
    if (GaitRevisionTwoOnly())
    {
        TArray<TSharedPtr<FJsonValue>> Proportions;
        for (const auto& Row:ProportionSamples) Proportions.Add(MakeShared<FJsonValueObject>(Row));
        Root->SetArrayField(TEXT("readonly_proportion_landmarks_max_two"),Proportions);
        Root->SetStringField(TEXT("proportion_acceptance"),TEXT("Measured landmarks only, not naked-body height or user proportion approval; absent entries mean NOT_RUN"));
    }
    if (GaitReviewOnly())
    {
        auto Evidence=MakeShared<FJsonObject>();
        Evidence->SetStringField(TEXT("scope"),TEXT("Actual sync tick/cache values and final component bones/curves; bone point motion is not a skinned sole contact proof"));
        Evidence->SetStringField(TEXT("acceptance"),TEXT("Coverage only; raw measured residuals and native images need review; not zero-sliding PASS"));
        Evidence->SetStringField(TEXT("eligibility"),TEXT("Consecutive observed engine frames, actual world interval <=75ms, same phase, both grounded, both contact>=0.8, ground hit, target speed +/-2cm/s and target active weight>=0.95, direction<2deg, actor yaw delta<1deg"));
        Evidence->SetBoolField(GaitRevisionTwoOnly()?TEXT("genuine_idle_clip_effective_weight_over_95_observed"):TEXT("independent_idle_observed"),bGaitIdleObserved);
        Evidence->SetBoolField(TEXT("low_speed_walk_blend_observed"),bGaitWalkBlendObserved);
        Evidence->SetBoolField(TEXT("nonzero_turn_direction_observed"),bGaitTurnObserved);
        Evidence->SetStringField(TEXT("actual_target_blendspace"),GetPathNameSafe(ActiveGaitSpace));
        Evidence->SetStringField(TEXT("run_clip"),GetPathNameSafe(GaitRunClip));
        Evidence->SetStringField(TEXT("sprint_clip"),GetPathNameSafe(GaitSprintClip));
        Evidence->SetStringField(TEXT("walk_clip"),GetPathNameSafe(GaitWalkClip));
        Evidence->SetStringField(TEXT("idle_clip"),GetPathNameSafe(GaitIdleClip));
        if (GaitRevisionTwoOnly())
        {
            Evidence->SetStringField(TEXT("selected_hero_native_source_report"),GaitSourceReport);
            Evidence->SetStringField(TEXT("measurement_layer"),TEXT("New integrated Hero and GASMotion/FlightPose graph, after actual stride/orientation/LegIK/look/Kawaii evaluation. Full retained character materials/outlines/halo; no old SoftToon replacement."));
            Evidence->SetStringField(TEXT("legacy_comparison_boundary"),TEXT("Same final toe/contact eligibility thresholds and 400/650 targets as old Gait. New clip pointer identities and physical body-relative direction replace old hardcoded names/raw BlendSpace X. Added 0.38 analog segment measures retained low-speed walk after start clips."));
            Evidence->SetStringField(TEXT("not_measured"),TEXT("Skinned sole contact, OS input, packaged runtime, combat, flight pose quality and visual approval remain separate. Warp inputs alone do not establish warp solver contribution."));
        }
        TArray<TSharedPtr<FJsonValue>> Summaries,FrameValues;
        for (int32 Gait=0;Gait<2;++Gait)for(int32 Side=0;Side<2;++Side)
        {
            auto Summary=MakeShared<FJsonObject>();const double Target=Gait==0?400:650;
            Summary->SetNumberField(TEXT("target_speed_cm_s"),Target);Summary->SetStringField(TEXT("side"),Side==0?TEXT("L"):TEXT("R"));
            Summary->SetNumberField(TEXT("steady_frames"),GaitSteadyFrames[Gait]);Summary->SetNumberField(TEXT("eligible_intervals"),GaitContactIntervals[Gait][Side]);
            TArray<double> Speeds;double Duration=0,WeightedSpeed=0,Square=0,Maximum=0;
            for (const auto& Frame:GaitSamples)
            {
                if(Frame->GetNumberField(TEXT("steady_target_cm_s"))!=Target)continue;
                const auto& Foot=Frame->GetArrayField(TEXT("feet"))[Side]->AsObject();
                if(!Foot->GetBoolField(TEXT("steady_stance_interval_eligible")))continue;
                const double Seconds=Frame->GetNumberField(TEXT("actual_interval_seconds"));
                const double Speed=Foot->GetNumberField(TEXT("observed_toe_horizontal_speed_cm_s"));
                Duration+=Seconds;WeightedSpeed+=Seconds*Speed;Square+=Seconds*Speed*Speed;Maximum=FMath::Max(Maximum,Speed);Speeds.Add(Speed);
            }
            Summary->SetNumberField(TEXT("eligible_duration_seconds"),Duration);
            if(Duration>0 && !Speeds.IsEmpty())
            {
                Speeds.Sort();Summary->SetNumberField(TEXT("duration_weighted_mean_horizontal_toe_speed_cm_s"),WeightedSpeed/Duration);
                Summary->SetNumberField(TEXT("duration_weighted_rms_horizontal_toe_speed_cm_s"),FMath::Sqrt(Square/Duration));
                Summary->SetNumberField(TEXT("interval_count_p95_horizontal_toe_speed_cm_s"),Speeds[FMath::Clamp(FMath::CeilToInt(.95*Speeds.Num())-1,0,Speeds.Num()-1)]);
                Summary->SetNumberField(TEXT("max_horizontal_toe_speed_cm_s"),Maximum);
            }
            Summaries.Add(MakeShared<FJsonValueObject>(Summary));
        }
        for (const auto& Frame:GaitSamples)FrameValues.Add(MakeShared<FJsonValueObject>(Frame));
        Evidence->SetArrayField(TEXT("steady_stance_measurements"),Summaries);Evidence->SetArrayField(TEXT("final_pose_frames"),FrameValues);
        Root->SetObjectField(TEXT("gait_runtime_evidence"),Evidence);
    }
    FString JSON; auto Writer=TJsonWriterFactory<>::Create(&JSON); FJsonSerializer::Serialize(Root,Writer);
    FFileHelper::SaveStringToFile(JSON,*(RunDirectory/TEXT("hero_exercise_results.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void AHCM5VS2HeroExerciseDirector::RemoveDelegates()
{
    if (SoleActivationHandle.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(SoleActivationHandle);
    SoleActivationHandle.Reset();
    if (Viewport.IsValid() && InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle);
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    InputHandle.Reset(); ScreenshotHandle.Reset();
}
void AHCM5VS2HeroExerciseDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    RemoveDelegates(); if (!bStopped && !bDone && bEnabled) Finish(TEXT("NOT_RUN"),TEXT("World ended before exercise completed"));
    Super::EndPlay(Reason);
}
