#include "HCM5VS2LookdevDirector.h"

#include "M1/HCM1Character.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "DataDrivenShaderPlatformInfo.h"
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
#include "UnrealClient.h"
#include <atomic>

/** Owned by the enqueued render command until completion; no UObject or JSON writes off-thread. */
struct FHCM5VS2RenderMaterialCheck
{
    struct FSlot
    {
        const FMaterialRenderProxy* Proxy = nullptr;
        bool bNoFallbackResource = false;
        bool bShaderMapComplete = false;
        bool bShaderMapValid = false;
        bool bFallbackUsed = true;
        FString ActualResourceName;
    };
    TArray<FSlot> Slots;
    std::atomic<bool> bComplete{false};
};

namespace
{
constexpr double InitialWarmupSeconds = 10.0;
constexpr double BetweenShotSeconds = 2.0;
constexpr double ScreenshotTimeoutSeconds = 15.0;
constexpr double MaximumRunSeconds = 600.0;
constexpr double MaterialReadyTimeoutSeconds = 120.0;
constexpr double MaterialReadyPollSeconds = .5;
constexpr float ComparisonFOV = 40.f;

bool SafeLabel(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 64) return false;
    for (TCHAR C : Value) if (!FChar::IsAlnum(C) && C != TEXT('_') && C != TEXT('-')) return false;
    return true;
}

bool FiniteColor(const FLinearColor& Value)
{
    return FMath::IsFinite(Value.R) && FMath::IsFinite(Value.G)
        && FMath::IsFinite(Value.B) && FMath::IsFinite(Value.A);
}

TArray<TSharedPtr<FJsonValue>> VectorJSON(const FVector& Value)
{
    return { MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z) };
}

TArray<TSharedPtr<FJsonValue>> RotationJSON(const FRotator& Value)
{
    return { MakeShared<FJsonValueNumber>(Value.Pitch), MakeShared<FJsonValueNumber>(Value.Yaw), MakeShared<FJsonValueNumber>(Value.Roll) };
}

TArray<TSharedPtr<FJsonValue>> ColorJSON(const FLinearColor& Value)
{
    return { MakeShared<FJsonValueNumber>(Value.R), MakeShared<FJsonValueNumber>(Value.G), MakeShared<FJsonValueNumber>(Value.B), MakeShared<FJsonValueNumber>(Value.A) };
}

bool ReadNativePNG(const FString& Filename, int32& Width, int32& Height)
{
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Filename));
    if (!File || File->TotalSize() < 33) return false;
    uint8 Header[24];
    File->Serialize(Header, sizeof(Header));
    const uint8 Signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (File->IsError() || FMemory::Memcmp(Header, Signature, 8) != 0
        || FMemory::Memcmp(Header + 12, "IHDR", 4) != 0) return false;
    auto BE32 = [](const uint8* B) { return (uint32(B[0]) << 24) | (uint32(B[1]) << 16) | (uint32(B[2]) << 8) | uint32(B[3]); };
    Width = int32(BE32(Header + 16)); Height = int32(BE32(Header + 20));
    return Width > 0 && Height > 0;
}
}

AHCM5VS2LookdevDirector::AHCM5VS2LookdevDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AHCM5VS2LookdevDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("M5VS2Lookdev"))) return;
    FString EvidenceDirectory;
    if (!FParse::Value(FCommandLine::Get(), TEXT("M5VS2EvidenceDir="), EvidenceDirectory)
        || FPaths::IsRelative(EvidenceDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("M5VS2_LOOKDEV_BLOCKED absolute evidence directory required; no scene changes"));
        return;
    }
    // This user-authorized absolute root also applies to a separately staged executable.
    FString Allowed = TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2");
    EvidenceDirectory = FPaths::ConvertRelativePathToFull(EvidenceDirectory);
    FPaths::NormalizeDirectoryName(Allowed); FPaths::NormalizeDirectoryName(EvidenceDirectory);
    if (!FPaths::CollapseRelativeDirectories(Allowed) || !FPaths::CollapseRelativeDirectories(EvidenceDirectory)
        || !FPaths::IsUnderDirectory(EvidenceDirectory, Allowed))
    {
        UE_LOG(LogTemp, Error, TEXT("M5VS2_LOOKDEV_BLOCKED evidence directory must be within %s; no scene changes"), *Allowed);
        return;
    }
    RunDirectory = EvidenceDirectory / (TEXT("Lookdev_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))
        + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*RunDirectory, true)) return;
    StartedAt = FPlatformTime::Seconds();
    bAutoQuit = FParse::Param(FCommandLine::Get(), TEXT("M5VS2AutoQuit"));
    FString Failure;
    if (!InitializeRun(Failure)) { Finish(TEXT("BLOCKED"), Failure); return; }
    bRunning = true;
    SetActorTickEnabled(true);
    if (!ApplyShot(0, Failure)) { Finish(TEXT("FAIL"), Failure); return; }
    WriteReport(TEXT("NOT_RUN"), TEXT("Running native capture sequence; no completed visual acceptance"));
#endif
}

bool AHCM5VS2LookdevDirector::InitializeRun(FString& Failure)
{
    Controller = UGameplayStatics::GetPlayerController(this, 0);
    Character = Controller ? Cast<AHCM1Character>(Controller->GetPawn()) : nullptr;
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    ULightComponent* Light = DirectionalLight ? DirectionalLight->GetLightComponent() : nullptr;
    if (!Controller || !Character || !Character->GetMesh() || !Viewport || !Viewport->Viewport
        || !Light || Light->Mobility != EComponentMobility::Movable)
    { Failure = TEXT("Requires live AHCM1Character player, viewport and configured movable directional light"); return false; }
    const int32 Slots = Character->GetMesh()->GetNumMaterials();
    if (Slots < 1 || Slots > 32 || MaterialVariants.IsEmpty() || MaterialVariants.Num() > 8 || Shots.IsEmpty() || Shots.Num() > 96)
    { Failure = TEXT("Bounded plan requires 1-32 mesh slots, 1-8 variants and 1-96 shots"); return false; }
    for (const FHCM5VS2MaterialVariant& Variant : MaterialVariants)
    {
        if (!SafeLabel(Variant.Label) || (!Variant.bUseOriginalMaterials && Variant.Materials.Num() != Slots))
        { Failure = TEXT("Variant labels must be safe and material count must match actual character slots"); return false; }
        if (!Variant.bUseOriginalMaterials)
            for (UMaterialInterface* Material : Variant.Materials)
                if (!IsValid(Material)) { Failure = TEXT("Variant contains a missing material"); return false; }
    }
    for (const FHCM5VS2LookdevShot& Shot : Shots)
    {
        if (!SafeLabel(Shot.Label) || !MaterialVariants.IsValidIndex(Shot.VariantIndex)
            || Shot.CameraLocation.ContainsNaN() || Shot.CameraRotation.ContainsNaN() || Shot.LightDirection.ContainsNaN()
            || !FiniteColor(Shot.LightColor) || !FiniteColor(Shot.AmbientColor)
            || !FMath::IsFinite(Shot.LightIntensity) || Shot.LightIntensity < 0.f || Shot.LightIntensity > 200000.f)
        { Failure = TEXT("Invalid shot label, index or finite camera/light data"); return false; }
    }
    // Observe the multicast input event. This does not consume, replace or replay Esc/P.
    ObservedViewport = Viewport;
    InputHandle = Viewport->OnInputKey().AddUObject(this, &AHCM5VS2LookdevDirector::OnViewportInputKey);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &AHCM5VS2LookdevDirector::OnScreenshotProcessed);
    OriginalViewTarget = Controller->GetViewTarget();
    OriginalLightRotation = DirectionalLight->GetActorRotation();
    OriginalLightColor = Light->GetLightColor(); OriginalLightIntensity = Light->Intensity;
    for (int32 Index = 0; Index < Slots; ++Index) OriginalMaterials.Add(Character->GetMesh()->GetMaterial(Index));
    if (AHUD* HUD = Controller->GetHUD()) bOriginalShowHUD = HUD->bShowHUD;
    FRotator EyeRotation;
    Character->GetActorEyesViewPoint(InitialEyeLocation, EyeRotation);
    AddTickPrerequisiteComponent(Character->GetMesh());
    bSavedState = true;
    RuntimeVariants = MaterialVariants;
    for (FHCM5VS2MaterialVariant& Variant : RuntimeVariants)
    {
        if (Variant.bUseOriginalMaterials) { Variant.Materials = OriginalMaterials; continue; }
        for (TObjectPtr<UMaterialInterface>& Material : Variant.Materials)
        {
            UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material.Get(), this);
            if (!Dynamic) { Failure = TEXT("Failed to create isolated dynamic material instance"); return false; }
            Material = Dynamic;
        }
    }
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    CaptureCamera = GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!CaptureCamera) { Failure = TEXT("Transient comparison camera spawn failed"); return false; }
    CaptureCamera->GetCameraComponent()->SetFieldOfView(ComparisonFOV);
    CaptureCamera->GetCameraComponent()->bConstrainAspectRatio = false;
    CaptureCamera->GetCameraComponent()->PostProcessBlendWeight = 0.f;
    if (bHideHUD) if (AHUD* HUD = Controller->GetHUD()) HUD->bShowHUD = false;
    return true;
}

bool AHCM5VS2LookdevDirector::ApplyShot(int32 Index, FString& Failure)
{
    if (bUserStopped || !Shots.IsValidIndex(Index) || !IsValid(Character) || !IsValid(CaptureCamera)
        || !IsValid(DirectionalLight) || !DirectionalLight->GetLightComponent())
    { Failure = TEXT("Shot runtime dependency is unavailable"); return false; }
    const FHCM5VS2LookdevShot& Shot = Shots[Index];
    FHCM5VS2MaterialVariant& Variant = RuntimeVariants[Shot.VariantIndex];
    DirectionalLight->SetActorRotation(Shot.LightDirection);
    ULightComponent* Light = DirectionalLight->GetLightComponent();
    Light->SetLightColor(Shot.LightColor); Light->SetIntensity(Shot.LightIntensity);
    const FVector TowardSun = -Light->GetForwardVector();
    const FLinearColor ShaderLight = Light->GetLightColor() * .8f;
    for (int32 Slot = 0; Slot < Variant.Materials.Num(); ++Slot)
    {
        if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Variant.Materials[Slot]))
        {
            // The original baseline's existing dynamic interfaces are never changed.
            if (!Variant.bUseOriginalMaterials)
            {
                Dynamic->SetVectorParameterValue(TEXT("SunDirection"), FLinearColor(TowardSun.X, TowardSun.Y, TowardSun.Z, 0.f));
                Dynamic->SetVectorParameterValue(TEXT("LightColor"), ShaderLight);
                Dynamic->SetVectorParameterValue(TEXT("AmbientColor"), Shot.AmbientColor);
            }
        }
        Character->GetMesh()->SetMaterial(Slot, Variant.Materials[Slot]);
    }
    FVector Position = Shot.CameraLocation;
    if (bUsePlayerEyeHeight && Shot.bUsePlayerEyeHeight && bEyeHeightSampled) Position.Z = LockedEyeMidpoint.Z;
    CaptureCamera->SetActorLocationAndRotation(Position, FRotator(0.f, Shot.CameraRotation.Yaw, 0.f));
    Controller->SetViewTarget(CaptureCamera);
    ShotIndex = Index; HoldFrames = 0; PhaseElapsed = 0;
    ShaderWaitStartedAt = FPlatformTime::Seconds(); NextShaderPollAt = 0;
    ShaderReadyFrame = 0; ShaderPollCount = 0; bMaterialsReady = false;
    LastMaterialReadiness.Reset(); PendingRenderMaterialCheck.Reset();
    return true;
}

bool AHCM5VS2LookdevDirector::PollMaterialReadiness(double Now, FString& Failure)
{
    const bool PassiveObservation=FParse::Param(FCommandLine::Get(),TEXT("M5VS2PassiveMaterialObservation"));
    const bool SkipExplicitGTSubmission=PassiveObservation || FParse::Param(FCommandLine::Get(),TEXT("M5VS2NoExplicitGTSubmit"));
    const bool PassiveRTObservation=PassiveObservation || FParse::Param(FCommandLine::Get(),TEXT("M5VS2PassiveRTObservation"));
    if (bUserStopped || !IsValid(Character) || !Character->GetMesh()) return false;
    if (GUsingNullRHI)
    { Failure = TEXT("Material readiness requires a real rendering RHI; NullRHI is not shader/render evidence"); return false; }
    // Consume a finished render-thread observation without ever blocking the game/input thread.
    if (PendingRenderMaterialCheck)
    {
        if (!PendingRenderMaterialCheck->bComplete.load(std::memory_order_acquire))
        {
            if (Now - ShaderWaitStartedAt > MaterialReadyTimeoutSeconds)
                Failure = TEXT("Material render-proxy readiness observation exceeded bounded 120 seconds");
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>& Rows = LastMaterialReadiness->GetArrayField(TEXT("slots"));
        bool Ready = LastMaterialReadiness->GetBoolField(TEXT("game_thread_ready"));
        for (int32 I = 0; I < PendingRenderMaterialCheck->Slots.Num(); ++I)
        {
            const auto& Observed = PendingRenderMaterialCheck->Slots[I];
            const auto Row = Rows[I]->AsObject();
            Row->SetBoolField(TEXT("render_no_fallback_resource_exists"), Observed.bNoFallbackResource);
            Row->SetBoolField(TEXT("render_shader_map_complete"), Observed.bShaderMapComplete);
            Row->SetBoolField(TEXT("render_shader_map_valid"), Observed.bShaderMapValid);
            Row->SetBoolField(TEXT("render_fallback_used"), Observed.bFallbackUsed);
            Row->SetStringField(TEXT("render_actual_resource_name"), Observed.ActualResourceName);
            const bool RenderReady = Observed.bNoFallbackResource && Observed.bShaderMapComplete
                && Observed.bShaderMapValid && !Observed.bFallbackUsed;
            Row->SetBoolField(TEXT("ready"), Row->GetBoolField(TEXT("game_thread_ready")) && RenderReady);
            Ready &= RenderReady;
        }
        PendingRenderMaterialCheck.Reset();
        LastMaterialReadiness->SetBoolField(TEXT("render_thread_observation_complete"), true);
        LastMaterialReadiness->SetStringField(TEXT("status"), Ready ? TEXT("READY") : TEXT("NOT_READY"));
        LastMaterialReadiness->SetNumberField(TEXT("observed_frame"), double(GFrameCounter));
        LastMaterialReadiness->SetNumberField(TEXT("wait_seconds"), Now - ShaderWaitStartedAt);
        if (LastMaterialReadiness->GetBoolField(TEXT("definite_compile_or_usage_failure")))
        {
            LastMaterialReadiness->SetStringField(TEXT("status"), TEXT("FAIL"));
            Failure = TEXT("Actual material compile error or missing skeletal/morph usage; see per-slot material_readiness, no comparison screenshot accepted");
            return false;
        }
        if (Ready && !bMaterialsReady) { ShaderReadyFrame = GFrameCounter; HoldFrames = 0; }
        if (!Ready) ShaderReadyFrame = 0;
        bMaterialsReady = Ready;
        WriteReport(TEXT("NOT_RUN"), Ready ? TEXT("Actual shaders ready including render proxies; waiting normal stabilization frames")
            : TEXT("Actual shaders/render proxies not ready; native frames and Escape/P remain active"));
    }
    if (!bMaterialsReady && Now - ShaderWaitStartedAt > MaterialReadyTimeoutSeconds)
    {
        if (LastMaterialReadiness) LastMaterialReadiness->SetStringField(TEXT("status"), TEXT("NOT_READY_TIMEOUT"));
        Failure = TEXT("Bounded 120-second material shader readiness timeout; not proof of compile failure; see actual resource/errors/pending state");
        return false;
    }
    if (Now < NextShaderPollAt) return bMaterialsReady;
    NextShaderPollAt = Now + MaterialReadyPollSeconds;
    ++ShaderPollCount;
    const EShaderPlatform Platform = GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel());
    const ERHIFeatureLevel::Type FeatureLevel = GetWorld()->GetFeatureLevel();
    LastMaterialReadiness = MakeShared<FJsonObject>();
    LastMaterialReadiness->SetBoolField(TEXT("passive_material_observation"),PassiveObservation);
    LastMaterialReadiness->SetBoolField(TEXT("skip_explicit_gt_submission"),SkipExplicitGTSubmission);
    LastMaterialReadiness->SetBoolField(TEXT("passive_rt_observation"),PassiveRTObservation);
    LastMaterialReadiness->SetBoolField(TEXT("normal_gt_cache_finalization"),!PassiveObservation);
    LastMaterialReadiness->SetStringField(TEXT("status"), TEXT("RENDER_QUERY_PENDING"));
    LastMaterialReadiness->SetNumberField(TEXT("shot_index"), ShotIndex);
    LastMaterialReadiness->SetNumberField(TEXT("shader_platform"), int32(Platform));
    LastMaterialReadiness->SetStringField(TEXT("shader_platform_name"), FDataDrivenShaderPlatformInfo::GetName(Platform).ToString());
    LastMaterialReadiness->SetNumberField(TEXT("feature_level"), int32(FeatureLevel));
    LastMaterialReadiness->SetBoolField(TEXT("null_rhi"), false);
    LastMaterialReadiness->SetNumberField(TEXT("poll_count"), ShaderPollCount);
    LastMaterialReadiness->SetBoolField(TEXT("render_thread_observation_complete"), false);
#if WITH_EDITOR
    LastMaterialReadiness->SetNumberField(TEXT("global_jobs_remaining"), GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0);
    LastMaterialReadiness->SetBoolField(TEXT("global_compiler_active"), GShaderCompilingManager && GShaderCompilingManager->IsCompiling());
#endif
    const auto State = MakeShared<FHCM5VS2RenderMaterialCheck, ESPMode::ThreadSafe>();
    // Match USkinnedMeshComponent::UpdateMorphMaterialUsageOnProxy: only interfaces
    // on sections actually used by active morphs need the morph permutation.
    // A historical neutral baseline with no active morphs is never rewritten.
    TSet<UMaterialInterface*> MorphMaterials;
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    if (const FSkeletalMeshRenderData* RenderData = Mesh->GetSkeletalMeshRenderData())
    {
        for (const auto& Active : Mesh->ActiveMorphTargets)
        {
            if (!Active.Key) continue;
            const auto& MorphLODs = Active.Key->GetMorphLODModels();
            for (int32 LOD = 0; LOD < FMath::Min(RenderData->LODRenderData.Num(), MorphLODs.Num()); ++LOD)
                for (int32 Section : MorphLODs[LOD].SectionIndices)
                    if (RenderData->LODRenderData[LOD].RenderSections.IsValidIndex(Section))
                        MorphMaterials.Add(Mesh->GetMaterial(RenderData->LODRenderData[LOD].RenderSections[Section].MaterialIndex));
        }
    }
    LastMaterialReadiness->SetNumberField(TEXT("active_morph_count"), Mesh->ActiveMorphTargets.Num());
    LastMaterialReadiness->SetNumberField(TEXT("materials_requiring_morph_usage"), MorphMaterials.Num());
    bool AllReady = true, DefiniteFailure = false;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < Character->GetMesh()->GetNumMaterials(); ++I)
    {
        UMaterialInterface* Material = Character->GetMesh()->GetMaterial(I);
        UMaterial* Base = Material ? Material->GetMaterial() : nullptr;
        // UE 5.8's deprecated feature-level overload returns null. Use the actual shader platform.
        FMaterialResource* Resource = Material ? Material->GetMaterialResource(Platform) : nullptr;
        const bool Skeletal = Base && Base->GetUsageByFlag(MATUSAGE_SkeletalMesh);
        const bool Morph = Base && Base->GetUsageByFlag(MATUSAGE_MorphTargets);
        const bool RequiresMorph = MorphMaterials.Contains(Material);
        bool Compiling = Material && Material->IsCompiling();
        TArray<TSharedPtr<FJsonValue>> Errors;
#if WITH_EDITOR
        if (Resource)
        {
            if (!PassiveObservation) Compiling |= !Resource->IsCompilationFinished();
            for (const FString& Error : Resource->GetCompileErrors())
                if (Errors.Num() < 8) Errors.Add(MakeShared<FJsonValueString>(Error.Left(2048)));
        }
#endif
        // IsCompilationFinished may finalize an already-ready cache request and change this pointer.
        FMaterialShaderMap* Map = Resource ? Resource->GetGameThreadShaderMap() : nullptr;
        const bool Finalized = Map && Map->IsCompilationFinalized();
        const bool Succeeded = Map && Map->CompiledSuccessfully();
        const bool Valid = Map && Map->IsValidForRendering();
        // Use the engine-maintained public completeness state; FMaterialShaderMap::IsComplete
        // is not exported from the Engine DLL in this installed UE version.
        const bool Complete = Resource && Resource->IsGameThreadShaderMapComplete();
#if WITH_EDITOR
        // -game may prepare lazy shader maps without submitting jobs until first use.
        // This only submits already-prepared jobs asynchronously; it cannot change
        // usage flags, static parameters, asset bytes or a material instance's map ID.
        if (!SkipExplicitGTSubmission && Resource && !Complete && Errors.IsEmpty()) Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
#endif
        // UE 5.8 can publish a frozen lazy-compilation clone whose lifecycle bits
        // are both false. IsValidForRendering deliberately does not require them.
        // Require actual completeness/validity and the render-proxy check below;
        // a finalized failed compile remains a failure, not a lazy-ready map.
        const bool CompileFailed = !Errors.IsEmpty() || (Map && Finalized && !Succeeded);
        const bool Ready = Material && Resource && Skeletal && (!RequiresMorph || Morph) && !Compiling
            && !CompileFailed && Valid && Complete;
        AllReady &= Ready;
        DefiniteFailure |= !Material || !Base || !Skeletal || (RequiresMorph && !Morph) || (CompileFailed && !Compiling);
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("slot"), I);
        Row->SetStringField(TEXT("actual_interface"), GetPathNameSafe(Material));
        Row->SetStringField(TEXT("base_material"), GetPathNameSafe(Base));
        const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
        Row->SetStringField(TEXT("actual_parent"), GetPathNameSafe(Instance ? Instance->Parent.Get() : nullptr));
        TArray<TSharedPtr<FJsonValue>> ParentPaths;
        TSet<const UMaterialInterface*> SeenParents;
        const UMaterialInterface* Parent = Instance ? Instance->Parent.Get() : nullptr;
        while (Parent && ParentPaths.Num() < 8 && !SeenParents.Contains(Parent))
        {
            SeenParents.Add(Parent);
            ParentPaths.Add(MakeShared<FJsonValueString>(Parent->GetPathName()));
            const UMaterialInstance* ParentInstance = Cast<UMaterialInstance>(Parent);
            Parent = ParentInstance ? ParentInstance->Parent.Get() : nullptr;
        }
        Row->SetArrayField(TEXT("actual_parent_chain_bounded"), ParentPaths);
        Row->SetBoolField(TEXT("actual_parent_chain_complete"), Parent == nullptr);
        UTexture* DiffuseTexture = nullptr;
        const bool DiffuseFound = Material && Material->GetTextureParameterValue(
            FMaterialParameterInfo(TEXT("gltf_tex_diffuse")), DiffuseTexture);
        Row->SetBoolField(TEXT("gltf_tex_diffuse_parameter_found"), DiffuseFound);
        Row->SetStringField(TEXT("gltf_tex_diffuse_effective_texture"), GetPathNameSafe(DiffuseTexture));
        Row->SetStringField(TEXT("texture_readback_scope"), TEXT("Game-thread resolved parameter including parent inheritance; not a GPU uniform-buffer capture"));
        Row->SetBoolField(TEXT("used_with_skeletal_mesh"), Skeletal);
        Row->SetBoolField(TEXT("used_with_morph_targets"), Morph);
        Row->SetBoolField(TEXT("morph_usage_required_by_active_sections"), RequiresMorph);
        Row->SetBoolField(TEXT("material_resource_exists"), Resource != nullptr);
        Row->SetStringField(TEXT("resource_material_interface"), Resource ? GetPathNameSafe(Resource->GetMaterialInterface()) : TEXT("None"));
        Row->SetNumberField(TEXT("resource_quality_level"), Resource ? int32(Resource->GetQualityLevel()) : -1);
        Row->SetBoolField(TEXT("compiling"), Compiling);
        Row->SetArrayField(TEXT("compile_errors_bounded"), Errors);
        Row->SetStringField(TEXT("game_thread_reason"), !Material || !Base ? TEXT("MISSING_MATERIAL")
            : !Skeletal || (RequiresMorph && !Morph) ? TEXT("MISSING_REQUIRED_USAGE")
            : Compiling ? TEXT("COMPILATION_PENDING")
            : CompileFailed ? TEXT("COMPILE_FAILURE")
            : !Resource || !Map ? TEXT("SHADER_RESOURCE_MISSING")
            : !Valid || !Complete ? TEXT("SHADER_RESOURCE_NOT_READY") : TEXT("READY"));
        Row->SetBoolField(TEXT("shader_map_exists"), Map != nullptr);
        Row->SetBoolField(TEXT("shader_map_finalized"), Finalized);
        Row->SetBoolField(TEXT("shader_map_compiled_successfully"), Succeeded);
        Row->SetBoolField(TEXT("shader_map_valid_for_rendering"), Valid);
        Row->SetBoolField(TEXT("shader_map_complete"), Complete);
        Row->SetBoolField(TEXT("game_thread_ready"), Ready);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        FHCM5VS2RenderMaterialCheck::FSlot& Render = State->Slots.AddDefaulted_GetRef();
        Render.Proxy = Material ? Material->GetRenderProxy() : nullptr;
    }
    LastMaterialReadiness->SetArrayField(TEXT("slots"), Rows);
    LastMaterialReadiness->SetBoolField(TEXT("game_thread_ready"), AllReady);
    LastMaterialReadiness->SetBoolField(TEXT("definite_compile_or_usage_failure"), DefiniteFailure);
    PendingRenderMaterialCheck = State;
    ENQUEUE_RENDER_COMMAND(HCM5VS2ObserveMaterialFallback)([State, FeatureLevel, PassiveRTObservation](FRHICommandListImmediate& RHICmdList)
    {
        for (auto& Slot : State->Slots)
        {
            if (!Slot.Proxy) continue;
            const FMaterial* Direct = Slot.Proxy->GetMaterialNoFallback(FeatureLevel);
            Slot.bNoFallbackResource = Direct != nullptr;
            Slot.bShaderMapComplete = Direct && Direct->IsRenderingThreadShaderMapComplete();
            const FMaterialShaderMap* ShaderMap = Direct ? Direct->GetRenderingThreadShaderMap() : nullptr;
            Slot.bShaderMapValid = ShaderMap && ShaderMap->IsValidForRendering();
            if (PassiveRTObservation)
            {
                // Diagnostic only: avoid this observer's lazy RT submission inside
                // GetMaterialWithFallback. Renderer itself retains normal compilation.
                const FMaterialRenderProxy* Proxy=Slot.Proxy;
                const FMaterial* Actual=Direct;
                int32 Depth=0;
                while ((!Actual || !Actual->IsRenderingThreadShaderMapComplete()) && Proxy && Depth++<8)
                { const auto* Next=Proxy->GetFallback(FeatureLevel);if(Next==Proxy)break;Proxy=Next;Actual=Proxy?Proxy->GetMaterialNoFallback(FeatureLevel):nullptr; }
                Slot.bFallbackUsed=Proxy!=Slot.Proxy || !Actual || !Actual->IsRenderingThreadShaderMapComplete();
                Slot.ActualResourceName=Actual?Actual->GetFriendlyName():TEXT("None");
            }
            else
            {
                const FMaterialRenderProxy* Fallback = nullptr;
                const FMaterial& Actual = Slot.Proxy->GetMaterialWithFallback(FeatureLevel, Fallback);
                Slot.bFallbackUsed = Fallback != nullptr;
                Slot.ActualResourceName = Actual.GetFriendlyName();
            }
        }
        State->bComplete.store(true, std::memory_order_release);
    });
    return false;
}

bool AHCM5VS2LookdevDirector::LockAnimatedEyeHeight(FString& Failure)
{
    if (bUserStopped || bEyeHeightSampled || !IsValid(Character) || !IsValid(CaptureCamera))
    { Failure = TEXT("Animated eye-height sample is unavailable or already locked"); return false; }
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    const FName LeftBone(TEXT("LeftEye")), RightBone(TEXT("RightEye"));
    LeftEyeBoneIndex = Mesh ? Mesh->GetBoneIndex(LeftBone) : INDEX_NONE;
    RightEyeBoneIndex = Mesh ? Mesh->GetBoneIndex(RightBone) : INDEX_NONE;
    if (LeftEyeBoneIndex == INDEX_NONE || RightEyeBoneIndex == INDEX_NONE)
    { Failure = TEXT("Actual animated LeftEye and RightEye bones are required; no CDO eye-height fallback"); return false; }
    SampledLeftEye = Mesh->GetBoneLocation(LeftBone, EBoneSpaces::WorldSpace);
    SampledRightEye = Mesh->GetBoneLocation(RightBone, EBoneSpaces::WorldSpace);
    if (SampledLeftEye.ContainsNaN() || SampledRightEye.ContainsNaN())
    { Failure = TEXT("Animated eye-bone world positions are not finite"); return false; }
    LockedEyeMidpoint = (SampledLeftEye + SampledRightEye) * .5;
    EyeSampleFrame = GFrameCounter;
    EyeSampleWarmupSeconds = PhaseElapsed;
    bEyeHeightSampled = true;
    const FHCM5VS2LookdevShot& Shot = Shots[ShotIndex];
    if (bUsePlayerEyeHeight && Shot.bUsePlayerEyeHeight)
    {
        FVector Position = CaptureCamera->GetActorLocation();
        Position.Z = LockedEyeMidpoint.Z;
        CaptureCamera->SetActorLocation(Position);
    }
    // Leave animation/time/materials/light untouched and allow 30 live frames at the corrected camera.
    HoldFrames = 0;
    WriteReport(TEXT("NOT_RUN"), TEXT("Animated eye height locked after warmup; waiting 30 normal camera-stabilization frames"));
    return true;
}

void AHCM5VS2LookdevDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bUserStopped) return;
    const double Now = FPlatformTime::Seconds();
    if (bAwaitingAutoQuit)
    {
        // Keep the Esc observer alive during the final grace interval.
        if (Now - FinishedAt >= 2.0 && !FScreenshotRequest::IsScreenshotRequested())
        {
            bAwaitingAutoQuit = false;
            SetActorTickEnabled(false);
            FPlatformMisc::RequestExit(false, TEXT("M5VS2 completed native lookdev capture"));
        }
        return;
    }
    if (!bRunning) return;
    if (Now - StartedAt > MaximumRunSeconds)
    { Finish(TEXT("FAIL"), TEXT("Bounded 600-second capture deadline exceeded; auto-quit withheld")); return; }
    if (bCapturePending)
    {
        if (bScreenshotProcessed && !FScreenshotRequest::IsScreenshotRequested() && GFrameCounter > RequestFrame)
        {
            int32 Width = 0, Height = 0;
            const bool Valid = ReadNativePNG(PendingScreenshot, Width, Height);
            PendingResult->SetStringField(TEXT("status"), Valid && Width == 1920 && Height == 1080 ? TEXT("PASS") : TEXT("FAIL"));
            PendingResult->SetBoolField(TEXT("native_png_exists"), Valid);
            PendingResult->SetNumberField(TEXT("file_bytes"), IFileManager::Get().FileSize(*PendingScreenshot));
            PendingResult->SetNumberField(TEXT("width"), Width); PendingResult->SetNumberField(TEXT("height"), Height);
            PendingResult->SetNumberField(TEXT("processed_after_seconds"), Now - RequestedAt);
            Results.Add(PendingResult); PendingResult.Reset(); bCapturePending = false;
            if (!Valid || Width != 1920 || Height != 1080)
            { Finish(TEXT("FAIL"), TEXT("Native screenshot missing/invalid or not 1920x1080")); return; }
            if (ShotIndex + 1 == Shots.Num()) { Finish(TEXT("PASS"), TEXT("All requested native PNG files verified; visual quality remains USER_REVIEW")); return; }
            // No next material/camera change while P has paused the game.
            PhaseElapsed = -1.0;
            WriteReport(TEXT("NOT_RUN"), TEXT("Native capture sequence in progress; remaining shots NOT_RUN"));
        }
        else if (Now - RequestedAt > ScreenshotTimeoutSeconds)
        { Finish(TEXT("FAIL"), TEXT("Native screenshot processing timed out; auto-quit withheld")); }
        return;
    }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (PhaseElapsed < 0)
    {
        FString Failure;
        if (!ApplyShot(ShotIndex + 1, Failure)) Finish(TEXT("FAIL"), Failure);
        return;
    }
    // Normal GPU frames continue; no paused-world, fixed timestep or high-res screenshot trick.
    PhaseElapsed += FMath::Clamp(double(DeltaSeconds), 0.0, .1);
    ++HoldFrames;
    FString MaterialFailure;
    const bool MaterialsReady = PollMaterialReadiness(Now, MaterialFailure);
    if (!MaterialFailure.IsEmpty()) { Finish(TEXT("FAIL"), MaterialFailure); return; }
    if (!MaterialsReady || ShaderReadyFrame == 0 || GFrameCounter - ShaderReadyFrame < 30) return;
    if (ShotIndex == 0 && !bEyeHeightSampled && PhaseElapsed >= InitialWarmupSeconds && HoldFrames >= 30)
    {
        FString Failure;
        if (!LockAnimatedEyeHeight(Failure)) Finish(TEXT("FAIL"), Failure);
        return;
    }
    const double Required = ShotIndex == 0 ? InitialWarmupSeconds : BetweenShotSeconds;
    if (PhaseElapsed >= Required && HoldFrames >= 30 && !FScreenshotRequest::IsScreenshotRequested()) RequestCurrentShot();
}

void AHCM5VS2LookdevDirector::RequestCurrentShot()
{
    if (bUserStopped || !bRunning || !Controller || !Controller->PlayerCameraManager || !Character || !CaptureCamera) return;
    if (!bEyeHeightSampled) { Finish(TEXT("FAIL"), TEXT("Actual animated eye height was not sampled after warmup")); return; }
    if (!bMaterialsReady || PendingRenderMaterialCheck || !LastMaterialReadiness
        || LastMaterialReadiness->GetStringField(TEXT("status")) != TEXT("READY"))
    { Finish(TEXT("FAIL"), TEXT("Comparison capture attempted before actual shader/render readiness")); return; }
    const FHCM5VS2LookdevShot& Shot = Shots[ShotIndex];
    const FHCM5VS2MaterialVariant& Variant = MaterialVariants[Shot.VariantIndex];
    const APlayerCameraManager* Manager = Controller->PlayerCameraManager;
    const FVector ActualLocation = Manager->GetCameraLocation();
    const FRotator ActualRotation = Manager->GetCameraRotation();
    const bool CameraMatches = Controller->GetViewTarget() == CaptureCamera
        && ActualLocation.Equals(CaptureCamera->GetActorLocation(), .5)
        && ActualRotation.Equals(CaptureCamera->GetActorRotation(), .1)
        && FMath::IsNearlyEqual(Manager->GetFOVAngle(), ComparisonFOV, .05f);
    if (!CameraMatches) { Finish(TEXT("FAIL"), TEXT("Actual player camera differs from the fixed comparison camera")); return; }
    PendingScreenshot = RunDirectory / FString::Printf(TEXT("%03d_%s_%s.png"), ShotIndex, *Shot.Label, *Variant.Label);
    if (IFileManager::Get().FileExists(*PendingScreenshot))
    { Finish(TEXT("FAIL"), TEXT("Refusing to overwrite an existing screenshot")); return; }
    PendingResult = MakeShared<FJsonObject>();
    PendingResult->SetNumberField(TEXT("shot_index"), ShotIndex);
    PendingResult->SetStringField(TEXT("label"), Shot.Label);
    PendingResult->SetStringField(TEXT("variant"), Variant.Label);
    PendingResult->SetStringField(TEXT("status"), TEXT("NOT_RUN"));
    PendingResult->SetStringField(TEXT("evidence_level"), TEXT("Native viewport rendering; no OS mouse/keyboard input"));
    PendingResult->SetStringField(TEXT("file"), PendingScreenshot);
    PendingResult->SetObjectField(TEXT("material_readiness"), LastMaterialReadiness);
    PendingResult->SetNumberField(TEXT("frames_after_material_ready"), double(GFrameCounter - ShaderReadyFrame));
    PendingResult->SetArrayField(TEXT("camera_location"), VectorJSON(ActualLocation));
    PendingResult->SetArrayField(TEXT("camera_rotation_pitch_yaw_roll"), RotationJSON(ActualRotation));
    PendingResult->SetNumberField(TEXT("horizontal_fov"), Manager->GetFOVAngle());
    PendingResult->SetArrayField(TEXT("initial_player_eye_location"), VectorJSON(InitialEyeLocation));
    PendingResult->SetArrayField(TEXT("locked_animated_eye_midpoint_world"), VectorJSON(LockedEyeMidpoint));
    PendingResult->SetNumberField(TEXT("eye_height_sample_frame"), double(EyeSampleFrame));
    PendingResult->SetNumberField(TEXT("frames_since_eye_height_sample"), double(GFrameCounter - EyeSampleFrame));
    PendingResult->SetBoolField(TEXT("shot_eye_height_locked"), bUsePlayerEyeHeight && Shot.bUsePlayerEyeHeight);
    PendingResult->SetArrayField(TEXT("character_location"), VectorJSON(Character->GetActorLocation()));
    PendingResult->SetArrayField(TEXT("character_rotation"), RotationJSON(Character->GetActorRotation()));
    PendingResult->SetStringField(TEXT("character_class"), Character->GetClass()->GetPathName());
    PendingResult->SetStringField(TEXT("skeletal_mesh"), GetPathNameSafe(Character->GetMesh()->GetSkeletalMeshAsset()));
    PendingResult->SetStringField(TEXT("animation_class"), GetPathNameSafe(Character->GetMesh()->GetAnimClass()));
    const FLightingChannels& MeshChannels=Character->GetMesh()->LightingChannels;
    PendingResult->SetNumberField(TEXT("mesh_lighting_channel_mask"), (MeshChannels.bChannel0 ? 1 : 0) | (MeshChannels.bChannel1 ? 2 : 0) | (MeshChannels.bChannel2 ? 4 : 0));
    TArray<UPointLightComponent*> CharacterLights;
    Character->GetComponents(CharacterLights);
    TArray<TSharedPtr<FJsonValue>> FillLights;
    for (const UPointLightComponent* Fill:CharacterLights)
    {
        auto Entry=MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("component"),Fill->GetPathName());
        Entry->SetArrayField(TEXT("world_location"),VectorJSON(Fill->GetComponentLocation()));
        Entry->SetArrayField(TEXT("relative_location"),VectorJSON(Fill->GetRelativeLocation()));
        Entry->SetStringField(TEXT("parent"),GetPathNameSafe(Fill->GetAttachParent()));
        Entry->SetNumberField(TEXT("intensity"),Fill->Intensity);
        Entry->SetNumberField(TEXT("intensity_units"),int32(Fill->IntensityUnits));
        Entry->SetNumberField(TEXT("attenuation_radius_cm"),Fill->AttenuationRadius);
        Entry->SetArrayField(TEXT("color_linear"),ColorJSON(Fill->GetLightColor()));
        Entry->SetBoolField(TEXT("visible"),Fill->IsVisible());
        Entry->SetBoolField(TEXT("registered"),Fill->IsRegistered());
        Entry->SetNumberField(TEXT("lighting_channel_mask"),(Fill->LightingChannels.bChannel0 ? 1 : 0) | (Fill->LightingChannels.bChannel1 ? 2 : 0) | (Fill->LightingChannels.bChannel2 ? 4 : 0));
        FillLights.Add(MakeShared<FJsonValueObject>(Entry));
    }
    PendingResult->SetArrayField(TEXT("character_point_lights_actual"),FillLights);
    ULightComponent* Light = DirectionalLight->GetLightComponent();
    PendingResult->SetStringField(TEXT("directional_light"), DirectionalLight->GetPathName());
    PendingResult->SetArrayField(TEXT("light_rotation"), RotationJSON(DirectionalLight->GetActorRotation()));
    PendingResult->SetArrayField(TEXT("light_color_linear"), ColorJSON(Light->GetLightColor()));
    PendingResult->SetNumberField(TEXT("light_intensity"), Light->Intensity);
    PendingResult->SetArrayField(TEXT("shader_sun_direction_world"), VectorJSON(-Light->GetForwardVector()));
    PendingResult->SetArrayField(TEXT("shader_light_color_target"), ColorJSON(Light->GetLightColor() * .8f));
    PendingResult->SetArrayField(TEXT("shader_ambient_color_target"), ColorJSON(Shot.AmbientColor));
    TArray<TSharedPtr<FJsonValue>> Materials;
    for (int32 Slot = 0; Slot < Character->GetMesh()->GetNumMaterials(); ++Slot)
    {
        const auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("slot"), Slot);
        Row->SetStringField(TEXT("actual_interface"), GetPathNameSafe(Character->GetMesh()->GetMaterial(Slot)));
        Row->SetStringField(TEXT("configured_source"), GetPathNameSafe(Variant.bUseOriginalMaterials ? OriginalMaterials[Slot].Get() : Variant.Materials[Slot].Get()));
        if (UMaterialInterface* Actual = Character->GetMesh()->GetMaterial(Slot))
        {
            Row->SetNumberField(TEXT("shading_models_mask"),Actual->GetShadingModels().GetShadingModelField());
            const auto Parameters = MakeShared<FJsonObject>();
            for (const FName Name : { FName(TEXT("SunDirection")), FName(TEXT("LightColor")), FName(TEXT("AmbientColor")) })
            {
                FLinearColor Value;
                const bool Found = Actual->GetVectorParameterValue(FHashedMaterialParameterInfo(Name), Value);
                Parameters->SetBoolField(Name.ToString() + TEXT("_present"), Found);
                if (Found) Parameters->SetArrayField(Name.ToString(), ColorJSON(Value));
            }
            Row->SetObjectField(TEXT("actual_vector_parameter_readback"), Parameters);
        }
        Materials.Add(MakeShared<FJsonValueObject>(Row));
    }
    PendingResult->SetArrayField(TEXT("materials"), Materials);
    PendingResult->SetNumberField(TEXT("render_hold_seconds"), PhaseElapsed);
    PendingResult->SetNumberField(TEXT("render_hold_frames"), HoldFrames);
    PendingResult->SetNumberField(TEXT("request_frame"), double(GFrameCounter));
    PendingResult->SetStringField(TEXT("visual_acceptance"), TEXT("USER_REVIEW"));
    RequestedAt = FPlatformTime::Seconds(); RequestFrame = GFrameCounter;
    bCapturePending = true; bScreenshotProcessed = false;
    FScreenshotRequest::RequestScreenshot(PendingScreenshot, !bHideHUD, false, false, FIntRect(), true);
}

void AHCM5VS2LookdevDirector::OnScreenshotProcessed()
{
    if (!bUserStopped && bCapturePending) bScreenshotProcessed = true;
}

void AHCM5VS2LookdevDirector::OnViewportInputKey(const FInputKeyEventArgs& Event)
{
    if ((bRunning || bAwaitingAutoQuit) && !bUserStopped && Event.Event == IE_Pressed && Event.Key == EKeys::Escape) StopForUser();
}

void AHCM5VS2LookdevDirector::StopForUser()
{
    bUserStopped = true; bRunning = false; bAwaitingAutoQuit = false; bAutoQuit = false;
    StopFrame = GFrameCounter;
    if (bCapturePending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename() == PendingScreenshot)
        FScreenshotRequest::Reset();
    bCapturePending = false;
    WriteReport(TEXT("NOT_RUN"), TEXT("Viewport Escape user stop; permanent latch, no subsequent camera/material/light changes, captures, automatic resume or exit"));
    SetActorTickEnabled(false);
    // Do not restore here: restoration is itself a camera/material change after user stop.
    // The original gameplay Esc/P route remains untouched and the event is not consumed.
    UE_LOG(LogTemp, Display, TEXT("M5VS2_LOOKDEV_USER_STOP frame=%llu report=%s"), StopFrame, *RunDirectory);
}

void AHCM5VS2LookdevDirector::Finish(const FString& Status, const FString& Detail)
{
    if (bUserStopped) return;
    bRunning = false;
    if (bCapturePending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename() == PendingScreenshot)
        FScreenshotRequest::Reset();
    bCapturePending = false;
    if (PendingResult.IsValid())
    {
        PendingResult->SetStringField(TEXT("status"), TEXT("FAIL"));
        PendingResult->SetStringField(TEXT("detail"), Detail);
        Results.Add(PendingResult); PendingResult.Reset();
    }
    RestoreOriginalState();
    FinishedAt = FPlatformTime::Seconds();
    bAwaitingAutoQuit = bAutoQuit && Status == TEXT("PASS");
    WriteReport(Status, Detail);
    SetActorTickEnabled(bAwaitingAutoQuit);
    UE_LOG(LogTemp, Display, TEXT("M5VS2_LOOKDEV_%s %s report=%s"), *Status, *Detail, *RunDirectory);
}

void AHCM5VS2LookdevDirector::RestoreOriginalState()
{
    if (!bSavedState || bRestored || bUserStopped) return;
    if (IsValid(Character) && Character->GetMesh())
        for (int32 Slot = 0; Slot < OriginalMaterials.Num(); ++Slot) Character->GetMesh()->SetMaterial(Slot, OriginalMaterials[Slot]);
    if (IsValid(DirectionalLight) && DirectionalLight->GetLightComponent())
    {
        DirectionalLight->SetActorRotation(OriginalLightRotation);
        DirectionalLight->GetLightComponent()->SetLightColor(OriginalLightColor);
        DirectionalLight->GetLightComponent()->SetIntensity(OriginalLightIntensity);
    }
    if (IsValid(Controller))
    {
        if (OriginalViewTarget.IsValid()) Controller->SetViewTarget(OriginalViewTarget.Get());
        if (AHUD* HUD = Controller->GetHUD()) HUD->bShowHUD = bOriginalShowHUD;
    }
    if (IsValid(CaptureCamera)) CaptureCamera->Destroy();
    bRestored = true;
}

void AHCM5VS2LookdevDirector::WriteReport(const FString& Status, const FString& Detail)
{
    if (RunDirectory.IsEmpty()) return;
    const auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), Status); Root->SetStringField(TEXT("detail"), Detail);
    Root->SetStringField(TEXT("run_directory"), RunDirectory);
    Root->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : TEXT("none"));
    Root->SetStringField(TEXT("visual_acceptance"), TEXT("USER_REVIEW"));
    Root->SetBoolField(TEXT("os_input_used"), false);
    Root->SetBoolField(TEXT("user_stop_latched"), bUserStopped);
    Root->SetNumberField(TEXT("stop_frame"), double(StopFrame));
    Root->SetBoolField(TEXT("original_state_restored"), bRestored);
    Root->SetBoolField(TEXT("auto_quit_pending"), bAwaitingAutoQuit);
    Root->SetBoolField(TEXT("hide_hud_in_test_capture"), bHideHUD);
    Root->SetBoolField(TEXT("eye_height_locked"), bUsePlayerEyeHeight && bEyeHeightSampled);
    Root->SetNumberField(TEXT("per_shot_material_ready_timeout_seconds"), MaterialReadyTimeoutSeconds);
    Root->SetNumberField(TEXT("material_poll_interval_seconds"), MaterialReadyPollSeconds);
    if (LastMaterialReadiness) Root->SetObjectField(TEXT("active_material_readiness"), LastMaterialReadiness);
    const auto EyeSample = MakeShared<FJsonObject>();
    EyeSample->SetStringField(TEXT("status"), bEyeHeightSampled ? TEXT("PASS") : TEXT("NOT_RUN"));
    EyeSample->SetStringField(TEXT("left_bone"), TEXT("LeftEye"));
    EyeSample->SetStringField(TEXT("right_bone"), TEXT("RightEye"));
    EyeSample->SetNumberField(TEXT("left_bone_index"), LeftEyeBoneIndex);
    EyeSample->SetNumberField(TEXT("right_bone_index"), RightEyeBoneIndex);
    EyeSample->SetArrayField(TEXT("left_eye_world"), VectorJSON(SampledLeftEye));
    EyeSample->SetArrayField(TEXT("right_eye_world"), VectorJSON(SampledRightEye));
    EyeSample->SetArrayField(TEXT("locked_midpoint_world"), VectorJSON(LockedEyeMidpoint));
    EyeSample->SetNumberField(TEXT("sample_frame"), double(EyeSampleFrame));
    EyeSample->SetNumberField(TEXT("warmup_seconds_before_sample"), EyeSampleWarmupSeconds);
    EyeSample->SetNumberField(TEXT("additional_stabilization_frames_minimum"), 30);
    EyeSample->SetBoolField(TEXT("cdo_fallback_allowed"), false);
    Root->SetObjectField(TEXT("animated_eye_height_sample"), EyeSample);
    Root->SetNumberField(TEXT("elapsed_wall_seconds"), FPlatformTime::Seconds() - StartedAt);
    Root->SetNumberField(TEXT("warmup_seconds_minimum"), InitialWarmupSeconds);
    Root->SetNumberField(TEXT("between_shot_seconds_minimum"), BetweenShotSeconds);
    Root->SetNumberField(TEXT("planned_shots"), Shots.Num());
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto& Result : Results) Rows.Add(MakeShared<FJsonValueObject>(Result));
    for (int32 Index = Results.Num(); Index < Shots.Num(); ++Index)
    {
        const auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("shot_index"), Index); Row->SetStringField(TEXT("label"), Shots[Index].Label);
        Row->SetStringField(TEXT("status"), TEXT("NOT_RUN"));
        Row->SetStringField(TEXT("detail"), bUserStopped ? TEXT("User stop; no further automation") : TEXT("Not yet captured"));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("shots"), Rows);
    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    if (FJsonSerializer::Serialize(Root, Writer))
        if (!FFileHelper::SaveStringToFile(Serialized, *(RunDirectory / TEXT("lookdev_results.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            UE_LOG(LogTemp, Error, TEXT("M5VS2_LOOKDEV_REPORT_WRITE_FAILED %s"), *RunDirectory);
}

void AHCM5VS2LookdevDirector::RemoveDelegates()
{
    if (ObservedViewport.IsValid() && InputHandle.IsValid()) ObservedViewport->OnInputKey().Remove(InputHandle);
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    InputHandle.Reset(); ScreenshotHandle.Reset();
}

void AHCM5VS2LookdevDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bRunning && !bUserStopped) Finish(TEXT("NOT_RUN"), TEXT("World ended before the native capture sequence completed"));
    RemoveDelegates();
    // User-stop latch also prevents deferred restoration on EndPlay.
    if (!bUserStopped) RestoreOriginalState();
    Super::EndPlay(Reason);
}
