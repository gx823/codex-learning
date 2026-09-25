#include "HCM4R2NavigationAuthor.h"
#include "M3/HCM3NavRegion.h"
#include "M3/HCM3EditorNavigation.h"
#include "AI/NavigationSystemConfig.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavModifierComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UHCM4R2NavigationSystem::UHCM4R2NavigationSystem(const FObjectInitializer& Initializer):Super(Initializer)
{ SetMapAgents(); }

void UHCM4R2NavigationSystem::SetMapAgents()
{
    // NPCs use ordinary PathFollowing plus CharacterMovement avoidance, not
    // DetourCrowd. Do not instantiate an unused manager before nav data loads.
    CrowdManagerClass.Reset();
    SupportedAgents.Reset();
    for(int32 I=0;I<2;++I)
    {
        FNavDataConfig Agent(I==0?32.f:35.f,180.f);
        Agent.Name=I==0?TEXT("M3Pedestrian"):TEXT("M4R2PlayerRoute");
        Agent.bCanWalk=true;Agent.bCanJump=false;Agent.bCanSwim=false;
        // UE IsEquivalent defaults to 5 cm. Names alone do not distinguish
        // our actual 32/35 cm agents, so use its supported class discriminator.
        const TSubclassOf<AActor> DataClass=I==0?ARecastNavMesh::StaticClass():AHCM4R2PlayerRecastNavMesh::StaticClass();
        Agent.SetNavDataClass(DataClass.Get());Agent.SetPreferredNavData(DataClass);
        SupportedAgents.Add(Agent);
    }
    SupportedAgentsMask.Empty();SupportedAgentsMask.Set(0);SupportedAgentsMask.Set(1);SupportedAgentsMask.MarkInitialized();
    DefaultAgentName=TEXT("M3Pedestrian");
}

void UHCM4R2NavigationSystem::PostInitProperties()
{ SetMapAgents();Super::PostInitProperties(); }

void UHCM4R2NavigationSystem::Configure(const UNavigationSystemConfig& Config)
{ Super::Configure(Config);SetMapAgents(); }

UHCM4R2PlayerRoadArea::UHCM4R2PlayerRoadArea(const FObjectInitializer& Initializer):Super(Initializer) {}

TSubclassOf<UNavAreaBase> UHCM4R2PlayerRoadArea::PickAreaClassForAgent(const AActor&,const FNavAgentProperties& Agent) const
{
    // Do not use UNavAreaMeta_SwitchByAgent: its implementation reads the global
    // default navigation class instead of this map's dedicated supported agents.
    return FMath::IsNearlyEqual(Agent.AgentRadius,35.f,.01f)&&FMath::IsNearlyEqual(Agent.AgentHeight,180.f,.01f)
        ? UNavArea_Default::StaticClass() : UNavArea_Null::StaticClass();
}

bool UHCM4R2NavigationAuthor::ConfigurePlayerNavigation(UObject* Context,ANavMeshBoundsVolume* Bounds)
{
#if WITH_EDITOR
    UWorld* World=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
    const bool bM5=World && (World->GetOutermost()->GetName()==TEXT("/Game/HarborCity/Maps/L_M5_CyberHarbor") ||
        (World->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/Part3/Town_")) &&
         World->GetOutermost()->GetName().EndsWith(TEXT("/L_HarborTown"))));
    if(!World||World->WorldType!=EWorldType::Editor||(!bM5 && World->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/Maps/L_M2_SeafrontStreet"))||
        !IsValid(Bounds)||Bounds->GetWorld()!=World||!Bounds->ActorHasTag(TEXT("HarborCity_M4_R2_Navigation")))return false;
    UNavigationSystemConfig* Config=World->GetWorldSettings()->GetNavigationSystemConfig();
    if(!Config||World->GetWorldSettings()->GetNavigationSystemConfigOverride())return false;
    const FSoftClassPath Old=Config->NavigationSystemClass,Required(UHCM4R2NavigationSystem::StaticClass());
    if(Old!=Required&&Old!=FSoftClassPath(UHCM3NavigationSystem::StaticClass()) &&
        !(bM5 && Old==FSoftClassPath(UNavigationSystemV1::StaticClass())))return false;
    World->GetWorldSettings()->Modify();Config->Modify();Config->NavigationSystemClass=Required;
    Bounds->Modify();Bounds->SupportedAgents.Empty();Bounds->SupportedAgents.Set(1);Bounds->SupportedAgents.MarkInitialized();
    if(World->GetNavigationSystem()&&!World->GetNavigationSystem()->IsA<UHCM4R2NavigationSystem>())World->SetNavigationSystem(nullptr);
    if(!World->GetNavigationSystem())FNavigationSystem::AddNavigationSystemToWorld(*World,FNavigationSystemRunMode::EditorMode);
    UHCM4R2NavigationSystem* Nav=FNavigationSystem::GetCurrent<UHCM4R2NavigationSystem>(World);
    if(!Nav||Nav->GetSupportedAgents().Num()!=2)return false;
    int32 Changed=0;
    for(TActorIterator<AHCM3NavRegion> It(World);It;++It)
    {
        const FName Id=It->StableId;
        if(It->Kind==EHCM3NavRegionKind::Forbidden&&(Id.ToString().StartsWith(TEXT("M3_Nav_Road_"))||
            Id==TEXT("M3_Nav_InitialParking")||Id==TEXT("M3_Nav_CoastParking")))
        {
            It->Modify();It->NavModifier->Modify();
            It->NavModifier->SetAreaClass(UHCM4R2PlayerRoadArea::StaticClass());
            It->NavModifier->RefreshNavigationModifiers();++Changed;
        }
    }
    Nav->OnNavigationBoundsUpdated(Bounds);
    UE_LOG(LogTemp,Display,TEXT("M4R2_PLAYER_NAV_CONFIG agents=2 npc_radius=32 player_radius=35 height=180 road_parking_meta_count=%d"),Changed);
    return Changed>2;
#else
    return false;
#endif
}

FString UHCM4R2NavigationAuthor::InspectNavigation(UObject* Context)
{
    UWorld* World=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
    TSharedRef<FJsonObject> Data=MakeShared<FJsonObject>();
    const UNavigationSystemV1* Nav=World?FNavigationSystem::GetCurrent<UNavigationSystemV1>(World):nullptr;
    Data->SetStringField(TEXT("navigation_class"),Nav?Nav->GetClass()->GetPathName():FString());
    TArray<TSharedPtr<FJsonValue>> Agents,NavData;
    if(Nav)for(const FNavDataConfig& A:Nav->GetSupportedAgents())
    {
        TSharedRef<FJsonObject> Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"),A.Name.ToString());Row->SetNumberField(TEXT("radius"),A.AgentRadius);Row->SetNumberField(TEXT("height"),A.AgentHeight);
        Row->SetStringField(TEXT("preferred_nav_class"),A.PreferredNavData.ToString());
        Agents.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(World)for(TActorIterator<ARecastNavMesh> It(World);It;++It)
    {
        const FNavDataConfig& A=It->GetConfig();TSharedRef<FJsonObject> Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actor"),It->GetPathName());Row->SetStringField(TEXT("agent"),A.Name.ToString());
        Row->SetNumberField(TEXT("radius"),A.AgentRadius);Row->SetNumberField(TEXT("height"),A.AgentHeight);
        Row->SetBoolField(TEXT("static_bake"),It->GetRuntimeGenerationMode()==ERuntimeGenerationType::Static);
        Row->SetStringField(TEXT("nav_class"),It->GetClass()->GetPathName());
        Row->SetBoolField(TEXT("registered"),It->IsRegistered());
        Row->SetBoolField(TEXT("in_nav_data_set"),Nav&&Nav->NavDataSet.Contains(*It));
        Row->SetNumberField(TEXT("agent_index"),Nav?Nav->GetSupportedAgentIndex(*It):INDEX_NONE);
        Row->SetNumberField(TEXT("active_tiles"),It->GetNumActiveTiles());
        NavData.Add(MakeShared<FJsonValueObject>(Row));
    }
    Data->SetArrayField(TEXT("agents"),Agents);Data->SetArrayField(TEXT("nav_data"),NavData);
    FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Data,Writer);return Out;
}
