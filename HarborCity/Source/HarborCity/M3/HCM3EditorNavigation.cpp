#include "HCM3EditorNavigation.h"
#include "AI/NavigationSystemBase.h"
#include "AI/NavigationSystemConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "UObject/UObjectMarks.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#endif

UHCM3NavigationSystem::UHCM3NavigationSystem(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetPedestrianAgent();
}

void UHCM3NavigationSystem::SetPedestrianAgent()
{
    FNavDataConfig Agent(32.0f, 180.0f);
    Agent.Name = TEXT("M3Pedestrian");
    Agent.bCanWalk = true;
    Agent.bCanJump = false;
    Agent.bCanSwim = false;
    Agent.SetNavDataClass(ARecastNavMesh::StaticClass());
    Agent.SetPreferredNavData(ARecastNavMesh::StaticClass());
    SupportedAgents.Reset();
    SupportedAgents.Add(Agent);
    SupportedAgentsMask.Empty();
    SupportedAgentsMask.Set(0);
    SupportedAgentsMask.MarkInitialized();
    DefaultAgentName = Agent.Name;
}

void UHCM3NavigationSystem::PostInitProperties()
{
    // Config properties have been loaded before PostInitProperties. Restore the
    // class-local contract before Super reads the CDO during agent filtering.
    SetPedestrianAgent();
    Super::PostInitProperties();
}

void UHCM3NavigationSystem::Configure(const UNavigationSystemConfig& Config)
{
    Super::Configure(Config);
    // The accepted map's old config can name the former Default agent. This
    // dedicated class always supplies its own one pedestrian agent in game too.
    SetPedestrianAgent();
}

namespace
{
UWorld* GetM3AuthorWorld(UObject* Context)
{
#if WITH_EDITOR
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    return World && World->WorldType == EWorldType::Editor &&
        (World->GetOutermost()->GetName() == TEXT("/Game/HarborCity/Maps/L_M2_SeafrontStreet") ||
         World->GetOutermost()->GetName() == TEXT("/Game/HarborCity/Maps/L_M5_CyberHarbor")) ? World : nullptr;
#else
    return nullptr;
#endif
}
}

FString UHCM3EditorNavigation::GetConfiguredNavigationClass(UObject* WorldContextObject)
{
    UWorld* World = GetM3AuthorWorld(WorldContextObject);
    const UNavigationSystemConfig* Config = World ? World->GetWorldSettings()->GetNavigationSystemConfig() : nullptr;
    return Config ? Config->NavigationSystemClass.ToString() : FString();
}

bool UHCM3EditorNavigation::ConfigureForAuthoring(UObject* WorldContextObject)
{
#if WITH_EDITOR
    UWorld* World = GetM3AuthorWorld(WorldContextObject);
    if (!World) return false;
    AWorldSettings* Settings = World->GetWorldSettings();
    UNavigationSystemConfig* Config = Settings->GetNavigationSystemConfig();
    // Do not manufacture a missing config or modify a transient override.
    if (!Config || Settings->GetNavigationSystemConfigOverride()) return false;
    const FSoftClassPath Required(UHCM3NavigationSystem::StaticClass());
    const FSoftClassPath Existing = Config->NavigationSystemClass;
    if (Existing != Required && Existing != FSoftClassPath(UNavigationSystemV1::StaticClass())) return false;
    if (Existing != Required)
    {
        Settings->Modify();
        Config->Modify();
        Config->NavigationSystemClass = Required; // Only this serialized map field changes; no config Flush.
    }
    if (World->GetNavigationSystem() && !World->GetNavigationSystem()->IsA<UHCM3NavigationSystem>())
        World->SetNavigationSystem(nullptr); // Calls the existing system's normal CleanUp(CleanupWithWorld).
    UE_LOG(LogTemp, Display, TEXT("M3_AUTHOR_NAV_CONFIG before=%s after=%s"), *Existing.ToString(), *Required.ToString());
    return InitializeForAuthoring(WorldContextObject);
#else
    return false;
#endif
}

bool UHCM3EditorNavigation::InitializeForAuthoring(UObject* WorldContextObject)
{
#if WITH_EDITOR
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || World->WorldType != EWorldType::Editor ||
        (World->GetOutermost()->GetName() != TEXT("/Game/HarborCity/Maps/L_M2_SeafrontStreet") &&
         World->GetOutermost()->GetName() != TEXT("/Game/HarborCity/Maps/L_M5_CyberHarbor") &&
         !((World->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/Part3/Town_")) ||
            World->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS3/Town_"))) &&
           World->GetOutermost()->GetName().EndsWith(TEXT("/L_HarborTown"))))) return false;
    UNavigationSystemV1* Before = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Before) FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
    UNavigationSystemV1* After = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    UE_LOG(LogTemp, Display, TEXT("M3_AUTHOR_NAV_INIT engine=%s before=%s after=%s world=%s"),
        *GEngine->GetClass()->GetPathName(), *GetNameSafe(Before), *GetNameSafe(After), *World->GetPathName());
    return After != nullptr;
#else
    return false;
#endif
}

bool UHCM3EditorNavigation::BuildForAuthoring(UObject* WorldContextObject)
{
#if WITH_EDITOR
    if (!InitializeForAuthoring(WorldContextObject)) return false;
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    // A Python commandlet does not run the editor idle loop. Finish actual asset
    // work, then give UE's own 2-second / 16-tick delayed-unlock callback time to
    // run. Never clear a lock or alter the editor auto-update preference here.
    FAssetCompilingManager::Get().FinishAllCompilation();
    FAssetCompilingManager::Get().ProcessAsyncTasks(false);
    const double WaitStarted = FPlatformTime::Seconds();
    double LastTick = WaitStarted;
    while (Nav->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock)
        && FPlatformTime::Seconds() - WaitStarted < 5.0)
    {
        FPlatformProcess::Sleep(0.05f);
        FAssetCompilingManager::Get().ProcessAsyncTasks(false);
        const double Now = FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().Tick(float(Now - LastTick));
        LastTick = Now;
    }
    UE_LOG(LogTemp, Display, TEXT("M3_AUTHOR_NAV_IDLE waited=%.3f assets=%d async=%d initial=%d custom=%d pie=%d"),
        FPlatformTime::Seconds() - WaitStarted, FAssetCompilingManager::Get().GetNumRemainingAssets(),
        Nav->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock),
        Nav->IsNavigationBuildingLocked(ENavigationBuildLock::InitialLock),
        Nav->IsNavigationBuildingLocked(ENavigationBuildLock::Custom),
        Nav->IsNavigationBuildingLocked(ENavigationBuildLock::NoUpdateInPIE));
    // The duplicated VS3 world can retain the source world's async-load lock
    // after its delayed-unlock registration is gone. Only this explicit offline
    // authoring path may retire that one lock, after compilation was drained and
    // the normal callback received its full idle grace period. Other locks stay.
    if (IsRunningCommandlet() && World->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS3/Town_"))
        && FPlatformTime::Seconds()-WaitStarted >= 5.0
        && FAssetCompilingManager::Get().GetNumRemainingAssets()==0
        && Nav->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock))
    {
        UE_LOG(LogTemp,Display,TEXT("VS3_AUTHOR_NAV retire stale offline AsyncLoadLock after drained compilation"));
        Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock, UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
    }
    const bool bLocked = Nav->IsNavigationBuildingLocked(uint8(~uint8(ENavigationBuildLock::NoUpdateInEditor)));
    UE_LOG(LogTemp, Display, TEXT("M3_AUTHOR_NAV_BUILD pre locked_excluding_editor_auto_update=%d remaining=%d"), bLocked, Nav->GetNumRemainingBuildTasks());
    if (bLocked) return false;
    // Feed the same public bounds notifications used by the editor, then drain
    // pending bounds / octree registration before the synchronous manual build.
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
        Nav->OnNavigationBoundsUpdated(*It);
    Nav->Tick(0.0f);
    Nav->Build(); // UE's synchronous method calls EnsureBuildCompletion on every nav data.
    const bool bComplete = Nav->GetNumRemainingBuildTasks() == 0 && !Nav->IsNavigationBuildInProgress();
    UE_LOG(LogTemp, Display, TEXT("M3_AUTHOR_NAV_BUILD post complete=%d remaining=%d"), bComplete, Nav->GetNumRemainingBuildTasks());
    return bComplete;
#else
    return false;
#endif
}

bool UHCM3EditorNavigation::ValidateSavedAgentForAuthoring(UObject* WorldContextObject)
{
#if WITH_EDITOR
    UWorld* World = GetM3AuthorWorld(WorldContextObject);
    UHCM3NavigationSystem* Nav = World ? FNavigationSystem::GetCurrent<UHCM3NavigationSystem>(World) : nullptr;
    if (!Nav || Nav->GetSupportedAgents().Num() != 1) return false;
    const FNavDataConfig& Supported = Nav->GetSupportedAgents()[0];
    int32 Count = 0;
    bool bValid = FMath::IsNearlyEqual(Supported.AgentRadius, 32.0f) && FMath::IsNearlyEqual(Supported.AgentHeight, 180.0f);
    for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
    {
        ++Count;
        const FNavDataConfig& Config = It->GetConfig();
        const bool bStatic = It->GetRuntimeGenerationMode() == ERuntimeGenerationType::Static;
        bValid &= bStatic && FMath::IsNearlyEqual(Config.AgentRadius, 32.0f) && FMath::IsNearlyEqual(Config.AgentHeight, 180.0f);
        UE_LOG(LogTemp, Display, TEXT("M3_AUTHOR_NAV_AGENT supported=%.2f/%.2f navdata=%.2f/%.2f static=%d"),
            Supported.AgentRadius, Supported.AgentHeight, Config.AgentRadius, Config.AgentHeight, bStatic);
    }
    // No direct AgentRadius or RuntimeGeneration edit/INI flush. Inconsistent
    // engine defaults fail here before saving instead of disguising a mismatch.
    return bValid && Count == 1;
#else
    return false;
#endif
}

bool UHCM3EditorNavigation::ResetTextExportMarksForAuthoring(UObject* WorldContextObject)
{
#if WITH_EDITOR
    if (!GetM3AuthorWorld(WorldContextObject)) return false;
    // Same export-bookkeeping reset as ULevelExporterT3D::ExportText and
    // DumpObjectToString. It does not modify actor properties or brush geometry.
    // UObjectExporterT3D alone otherwise leaves TagImp set on inline brush Models,
    // so a later independent actor export omits their real polygon block.
    UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));
    return true;
#else
    return false;
#endif
}
