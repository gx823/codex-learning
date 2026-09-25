#include "HCM5VS2NPCGameplayEditor.h"

#if WITH_EDITOR
#include "M3/HCM3EditorNavigation.h"
#include "M3/HCM3NavRegion.h"
#include "HCM5VS2NPC.h"
#include "AI/NavigationSystemBase.h"
#include "AI/NavigationSystemConfig.h"
#include "AssetCompilingManager.h"
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Regex.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
const TCHAR* MapPath=TEXT("/Game/HarborCity/M5VS2/NPC/GameplayReview/L_NPCGameplay");
FString IdlePairForMap(const FString& Name)
{
    FRegexMatcher Matcher(FRegexPattern(TEXT("^/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/IdlePairs/Attempt_[0-9a-f]{12}/L_NPCReaction_(Open|Wall|Slope|Narrow)_(QR|JT|UV|WX)$")),Name);
    return Matcher.FindNext()?Matcher.GetCaptureGroup(2):FString();
}
bool IsReactionMap(const FString& Name)
{
    for (const TCHAR* Site:{TEXT("Open"),TEXT("Wall"),TEXT("Slope"),TEXT("Narrow")})
        if (Name==FString(TEXT("/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/L_NPCReaction_"))+Site) return true;
    return !IdlePairForMap(Name).IsEmpty();
}
FName OwnerFor(UWorld* World)
{ return IsReactionMap(World->GetOutermost()->GetName())?FName(TEXT("HarborCity_M5_VS2_NPCReactionReviewR2")):FName(TEXT("HarborCity_M5_VS2_NPCGameplayReview")); }
using FRows=TArray<TSharedPtr<FJsonValue>>;
FRows XYZ(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
FString Result(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{
    R->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    if (!Error.IsEmpty()) R->SetStringField(TEXT("error"),Error);
    FString JSON; FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON)); return JSON;
}
UWorld* ExactWorld(UObject* Context)
{
    UWorld* W=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
    return W && W->WorldType==EWorldType::Editor && (W->GetOutermost()->GetName()==MapPath || IsReactionMap(W->GetOutermost()->GetName()))?W:nullptr;
}
bool Initialize(UWorld* W)
{
    const UNavigationSystemConfig* Config=W->GetWorldSettings()->GetNavigationSystemConfig();
    if (!Config || W->GetWorldSettings()->GetNavigationSystemConfigOverride()
        || Config->NavigationSystemClass!=FSoftClassPath(UHCM3NavigationSystem::StaticClass())) return false;
    if (!W->GetNavigationSystem()) FNavigationSystem::AddNavigationSystemToWorld(*W,FNavigationSystemRunMode::EditorMode);
    return FNavigationSystem::GetCurrent<UHCM3NavigationSystem>(W)!=nullptr;
}
FString Inspect(UWorld* W,const TSharedRef<FJsonObject>& R)
{
    if (!Initialize(W)) return Result(R,TEXT("Saved exact M3 pedestrian navigation config required"));
    UHCM3NavigationSystem* Nav=FNavigationSystem::GetCurrent<UHCM3NavigationSystem>(W);
    R->SetStringField(TEXT("map"),W->GetOutermost()->GetName());
    R->SetStringField(TEXT("navigation_class"),Nav->GetClass()->GetPathName());
    R->SetNumberField(TEXT("remaining_tasks"),Nav->GetNumRemainingBuildTasks());
    R->SetBoolField(TEXT("build_in_progress"),Nav->IsNavigationBuildInProgress());
    if (Nav->GetSupportedAgents().Num()!=1) return Result(R,TEXT("Exactly one existing M3 pedestrian agent required"));
    const FNavDataConfig& Agent=Nav->GetSupportedAgents()[0];
    R->SetNumberField(TEXT("agent_radius_cm"),Agent.AgentRadius); R->SetNumberField(TEXT("agent_height_cm"),Agent.AgentHeight);
    bool Valid=FMath::IsNearlyEqual(Agent.AgentRadius,32.f)&&FMath::IsNearlyEqual(Agent.AgentHeight,180.f);
    int32 BoundsCount=0,DataCount=0,RegionCount=0;
    for (TActorIterator<ANavMeshBoundsVolume> It(W);It;++It)
    { ++BoundsCount; Valid &= It->ActorHasTag(OwnerFor(W)); }
    AHCM3NavRegion* Region=nullptr;
    for (TActorIterator<AHCM3NavRegion> It(W);It;++It)
    { ++RegionCount; Region=*It; Valid &= It->ActorHasTag(OwnerFor(W))&&It->Kind==EHCM3NavRegionKind::Allowed; }
    FRows DataRows;
    for (TActorIterator<ARecastNavMesh> It(W);It;++It)
    {
        ++DataCount; const FNavDataConfig& C=It->GetConfig(); auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"),It->GetPathName()); Row->SetNumberField(TEXT("radius_cm"),C.AgentRadius);
        Row->SetNumberField(TEXT("height_cm"),C.AgentHeight);
        const bool Static=It->GetRuntimeGenerationMode()==ERuntimeGenerationType::Static;
        Row->SetBoolField(TEXT("static"),Static); DataRows.Add(MakeShared<FJsonValueObject>(Row));
        Valid &= Static&&FMath::IsNearlyEqual(C.AgentRadius,32.f)&&FMath::IsNearlyEqual(C.AgentHeight,180.f);
    }
    R->SetArrayField(TEXT("navigation_data"),DataRows); R->SetNumberField(TEXT("bounds_count"),BoundsCount);
    R->SetNumberField(TEXT("allowed_region_count"),RegionCount);
    if (!Valid || BoundsCount!=1 || DataCount!=1 || RegionCount!=1 || !Region
        || Nav->GetNumRemainingBuildTasks()!=0 || Nav->IsNavigationBuildInProgress())
        return Result(R,TEXT("Navigation class, one static data, bounds/region or completion mismatch"));
    FRows Samples; TSet<FName> IDs;
    for (TActorIterator<AHCM5VS2NPC> It(W);It;++It)
    {
        AHCM5VS2NPC* NPC=*It; IDs.Add(NPC->StableId); auto Row=MakeShared<FJsonObject>();
        const FVector Feet=NPC->GetActorLocation()-FVector(0,0,NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FVector Goal=Feet+FVector(400,0,0);
        FNavLocation StartProjected,EndProjected;
        const bool StartOK=Nav->ProjectPointToNavigation(Feet,StartProjected,FVector(80,80,180));
        const bool EndOK=Nav->ProjectPointToNavigation(Goal,EndProjected,FVector(80,80,180));
        UNavigationPath* Path=StartOK&&EndOK?UNavigationSystemV1::FindPathToLocationSynchronously(W,StartProjected.Location,EndProjected.Location,NPC):nullptr;
        const bool PathOK=Path&&Path->IsValid()&&!Path->IsPartial()&&Path->PathPoints.Num()>=2;
        const bool SampleOK=NPC->ActorHasTag(OwnerFor(W))&&NPC->bStationary&&NPC->NavigationRegion==Region
            &&StartOK&&EndOK&&FVector::Dist2D(Feet,StartProjected.Location)<10
            &&Region->ContainsPoint(Feet,35)&&Region->ContainsPoint(Goal,35)&&PathOK;
        Valid &= SampleOK;
        Row->SetStringField(TEXT("stable_id"),NPC->StableId.ToString()); Row->SetArrayField(TEXT("feet_cm"),XYZ(Feet));
        Row->SetBoolField(TEXT("feet_projected"),StartOK); Row->SetBoolField(TEXT("goal_projected"),EndOK);
        if (StartOK) Row->SetArrayField(TEXT("projected_feet_cm"),XYZ(StartProjected.Location));
        if (EndOK) Row->SetArrayField(TEXT("projected_goal_cm"),XYZ(EndProjected.Location));
        Row->SetBoolField(TEXT("nonpartial_400cm_path"),PathOK); Row->SetBoolField(TEXT("passed"),SampleOK);
        Samples.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("specimen_paths"),Samples);
    FString Pair=IdlePairForMap(W->GetOutermost()->GetName());if(Pair.IsEmpty())Pair=TEXT("QR");
    R->SetStringField(TEXT("review_pair"),Pair);
    if (!Valid||Samples.Num()!=2||IDs.Num()!=2||!IDs.Contains(FName(*(TEXT("M5VS2_")+Pair.Mid(0,1))))||!IDs.Contains(FName(*(TEXT("M5VS2_")+Pair.Mid(1,1)))))
        return Result(R,TEXT("Exact two selected identities, authored feet and 400cm paths must project on saved navigation"));
    return Result(R);
}
}
#endif

FString UHCM5VS2NPCGameplayEditor::BuildNavigation(UObject* WorldContextObject)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("native_build_requested"),true); R->SetBoolField(TEXT("saved_by_helper"),false);
    UWorld* W=ExactWorld(WorldContextObject);
    if (!W) return Result(R,TEXT("Only isolated NPC GameplayReview editor world is allowed"));
    AWorldSettings* Settings=W->GetWorldSettings(); UNavigationSystemConfig* Config=Settings->GetNavigationSystemConfig();
    if (!Config||Settings->GetNavigationSystemConfigOverride()) return Result(R,TEXT("Existing serialized navigation config required"));
    const FSoftClassPath Required(UHCM3NavigationSystem::StaticClass());
    if (Config->NavigationSystemClass!=Required&&Config->NavigationSystemClass!=FSoftClassPath(UNavigationSystemV1::StaticClass()))
        return Result(R,TEXT("Unexpected prior navigation class; no replacement"));
    if (Config->NavigationSystemClass!=Required)
    { Settings->Modify();Config->Modify();Config->NavigationSystemClass=Required; }
    if (W->GetNavigationSystem()&&!W->GetNavigationSystem()->IsA<UHCM3NavigationSystem>()) W->SetNavigationSystem(nullptr);
    if (!Initialize(W)) return Result(R,TEXT("Native pedestrian navigation initialization failed"));
    UHCM3NavigationSystem* Nav=FNavigationSystem::GetCurrent<UHCM3NavigationSystem>(W);
    FAssetCompilingManager::Get().FinishAllCompilation();
    const double Started=FPlatformTime::Seconds(); double LastTick=Started;
    while (Nav->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock)&&FPlatformTime::Seconds()-Started<5.)
    {
        FPlatformProcess::Sleep(.05f); const double Now=FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().Tick(float(Now-LastTick));LastTick=Now;
    }
    R->SetNumberField(TEXT("async_unlock_wait_seconds"),FPlatformTime::Seconds()-Started);
    if (Nav->IsNavigationBuildingLocked(uint8(~uint8(ENavigationBuildLock::NoUpdateInEditor))))
        return Result(R,TEXT("Navigation still locked; no lock override"));
    int32 Count=0;
    for (TActorIterator<ANavMeshBoundsVolume> It(W);It;++It)
    { ++Count;if (!It->ActorHasTag(OwnerFor(W))) return Result(R,TEXT("Unowned navigation bounds"));Nav->OnNavigationBoundsUpdated(*It); }
    if (Count!=1) return Result(R,TEXT("Exactly one owned navigation bounds required"));
    Nav->Tick(0);Nav->Build(); // Engine Build calls EnsureBuildCompletion on each nav data.
    return Inspect(W,R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2NPCGameplayEditor::InspectNavigation(UObject* WorldContextObject)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("native_build_requested"),false);R->SetBoolField(TEXT("saved_by_helper"),false);
    UWorld* W=ExactWorld(WorldContextObject);
    return W?Inspect(W,R):Result(R,TEXT("Only isolated NPC GameplayReview editor world is allowed"));
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
