#include "HCM4R2Navigation.h"
#include "M5VS2/HCM5VS2TownRuntime.h"
#include "EngineUtils.h"
#include "HCM4R2NavigationAuthor.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1Vehicle.h"
#include "M3/HCM3Experience.h"
#include "M3/HCM3NPC.h"
#include "M5/HCM5StoryDirector.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& P)
    { return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}; }

    bool ClosestGraphEdge(const UHCM4R2MapData* Data, const FVector& Point, FIntPoint& Edge, FVector& Projection, double& Distance)
    {
        Distance = TNumericLimits<double>::Max();
        if (!Data) return false;
        for (const FIntPoint& Candidate : Data->RoadEdges)
        {
            if (!Data->RoadNodes.IsValidIndex(Candidate.X) || !Data->RoadNodes.IsValidIndex(Candidate.Y)) continue;
            const FVector A = Data->RoadNodes[Candidate.X], B = Data->RoadNodes[Candidate.Y];
            const FVector FlatPoint(Point.X,Point.Y,0), FlatA(A.X,A.Y,0), FlatB(B.X,B.Y,0);
            const FVector FlatProjection = FMath::ClosestPointOnSegment(FlatPoint,FlatA,FlatB);
            const double D = FVector::DistSquared2D(Point,FlatProjection);
            if (D >= Distance) continue;
            Distance = D; Edge = Candidate;
            const double Alpha = FVector::Dist2D(A,B) > .01 ? FVector::Dist2D(A,FlatProjection)/FVector::Dist2D(A,B) : 0.;
            Projection = FMath::Lerp(A,B,Alpha);
        }
        Distance = FMath::Sqrt(Distance);
        return Distance < TNumericLimits<double>::Max()*.1;
    }
}

UHCM4R2NavigationComponent::UHCM4R2NavigationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .25f;
    MapAsset = TSoftObjectPtr<UHCM4R2MapData>(FSoftObjectPath(TEXT("/Game/HarborCity/M4R2/Map/DA_SeafrontNavigation.DA_SeafrontNavigation")));
}

void UHCM4R2NavigationComponent::BeginPlay()
{
    Super::BeginPlay();
    Controller = Cast<AHCM1PlayerController>(GetOwner());
    if (AHCM5StoryDirector::Find(GetWorld()))
        MapAsset=TSoftObjectPtr<UHCM4R2MapData>(FSoftObjectPath(TEXT("/Game/HarborCity/M5/Map/DA_CyberHarborNavigation.DA_CyberHarborNavigation")));
    MapData = MapAsset.LoadSynchronous();
    // The new saved town supplies its own baked background and road graph.
    // Historical maps keep their original data; no global map asset is overwritten.
    for (TActorIterator<AHCM5VS2TownRuntime> It(GetWorld()); It; ++It)
        if (It->NavigationMap) { MapData = It->NavigationMap; break; }
    if (!MapData) UE_LOG(LogTemp, Warning, TEXT("M4R2 navigation baked map not installed; routes remain unavailable."));
}

void UHCM4R2NavigationComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaTime,TickType,Tick);
    if (bMinimapEnabled) RefreshGuidance();
}

bool UHCM4R2NavigationComponent::IsDriving() const
{ return Controller.IsValid() && Controller->GetPlayerMode() == EHCPlayerMode::Driving && IsValid(Controller->GetActiveVehicle()); }

FVector UHCM4R2NavigationComponent::GetPosition() const
{
    if (!Controller.IsValid()) return FVector::ZeroVector;
    const APawn* Pawn = IsDriving() ? static_cast<APawn*>(Controller->GetActiveVehicle()) : Controller->GetControlledCharacter();
    return IsValid(Pawn) ? Pawn->GetActorLocation() : FVector::ZeroVector;
}

float UHCM4R2NavigationComponent::GetHeadingDegrees() const
{
    if (!Controller.IsValid()) return 0;
    const APawn* Pawn = IsDriving() ? static_cast<APawn*>(Controller->GetActiveVehicle()) : Controller->GetControlledCharacter();
    return IsValid(Pawn) ? Pawn->GetActorRotation().Yaw : 0;
}

FVector2D UHCM4R2NavigationComponent::WorldToMap(const FVector& P) const
{
    const FVector2D Min = MapData ? MapData->WorldMinimum : FVector2D(-7500,-7500);
    const FVector2D Max = MapData ? MapData->WorldMaximum : FVector2D(7500,7500);
    return FVector2D((P.X-Min.X)/FMath::Max(1.,Max.X-Min.X),(Max.Y-P.Y)/FMath::Max(1.,Max.Y-Min.Y));
}

void UHCM4R2NavigationComponent::CycleTrackedQuest()
{
    if (Controller.IsValid())
    {
        if (AHCM5StoryDirector* Story=Controller->GetM5Story())
        {
            Story->SetTrackMainStory(!Story->IsTrackingMainStory());
            if (!Story->IsTrackingMainStory()) if (AHCM3Experience* E=Controller->GetM3Experience()) E->CycleTrackedQuest();
        }
        else if (AHCM3Experience* E = Controller->GetM3Experience()) E->CycleTrackedQuest();
    }
    NextPlanAt = 0; bInitialized = false; RefreshGuidance();
}

void UHCM4R2NavigationComponent::SetMinimapEnabled(bool bEnabled)
{ bMinimapEnabled = bEnabled; if (bEnabled) { NextPlanAt = 0; bInitialized = false; RefreshGuidance(); } }

FString UHCM4R2NavigationComponent::GetTrackingButtonText() const
{
    if (Controller.IsValid()) if (const AHCM5StoryDirector* Story=Controller->GetM5Story())
        return Story->IsTrackingMainStory()?TEXT("追踪：主线 · 切换至支线"):TEXT("追踪：支线 · 切换至主线");
    const AHCM3Experience* E = Controller.IsValid() ? Controller->GetM3Experience() : nullptr;
    return E ? E->GetTrackingLabel() : TEXT("追踪：暂无进行中任务");
}

FString UHCM4R2NavigationComponent::GetNavigationText() const
{
    if (!bMinimapEnabled) return FString();
    if (!bTargetValid) return Controller.IsValid() && Controller->GetM5Story()
        ? TEXT("北向上 · 夜航港城\n暂无有效追踪目标\nP 暂停菜单切换主线／支线") : TEXT("北向上 · 海滨街\n暂无有效追踪目标\n接取委托后显示路线；暂停菜单切换追踪");
    if (!bRouteValid) return TargetLabel + FString::Printf(TEXT("\n方向提示 · 直线 %.0f m\n%s"),StraightDistanceMeters,*RouteMessage);
    return TargetLabel + FString::Printf(TEXT("\n路线 %.0f m · 直线 %.0f m\n%s"),RouteDistanceMeters,StraightDistanceMeters,*RouteMessage);
}

void UHCM4R2NavigationComponent::RefreshGuidance()
{
    AHCM1PlayerController* PC = Controller.Get();
    AHCM3Experience* E = PC ? PC->GetM3Experience() : nullptr;
    FHCM3QuestNavigationTarget Target;
    AHCM5StoryDirector* Story=PC?PC->GetM5Story():nullptr;
    const bool bMain=Story && Story->IsTrackingMainStory();
    if (bMain ? !Story->GetNavigationTarget(Target) : (!E || !E->GetNavigationTarget(Target)))
    {
        bTargetValid = bRouteValid = false; RoutePoints.Reset(); StageId = QuestId = NAME_None; TargetLabel.Empty();
        RouteMessage.Empty(); RouteDistanceMeters = StraightDistanceMeters = 0; bInitialized = false; return;
    }
    const bool bDriving = IsDriving();
    const FVector Position = GetPosition();
    FVector RouteGoal = Target.Location;
    FName CurrentStage = Target.StageId;
    bWalkAfterParking = false;
    TargetLabel = Target.Label;
    if (bMain && bDriving && !Target.bVehicleDestination)
    {
        FIntPoint Edge; FVector Projection; double Distance;
        if (ClosestGraphEdge(MapData,Target.Location,Edge,Projection,Distance))
        {
            RouteGoal=Projection; CurrentStage=FName(*(Target.StageId.ToString()+TEXT(".Park")));
            bWalkAfterParking=true; TargetLabel+=TEXT(" · 停稳后下车");
        }
    }
    else if (Target.QuestId == TEXT("Ride") && !bDriving)
    {
        AHCM1Vehicle* Vehicle = PC->GetActiveVehicle();
        if (!IsValid(Vehicle)) for (TActorIterator<AHCM1Vehicle> It(GetWorld());It;++It) { Vehicle=*It;break; }
        if (IsValid(Vehicle))
        {
            FTransform SafeDoor;
            AHCM1Character* Character = PC->GetControlledCharacter();
            if (Character && Vehicle->FindSafeExitTransform(Character, SafeDoor))
                RouteGoal = SafeDoor.GetLocation() - FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            else RouteGoal = Vehicle->GetInteractionLocation() - FVector(0,0,75);
            CurrentStage=TEXT("Ride.GetVehicle"); TargetLabel=TEXT("顺路看海 · 先到车边上车");
        }
    }
    else if (bDriving && Target.bPickupPassenger)
    {
        // The actual curb and actual body half width determine a legal pickup bay.
        double BodyHalfWidth = 110;
        TInlineComponentArray<UStaticMeshComponent*> Parts(PC->GetActiveVehicle());
        for (const UStaticMeshComponent* Part : Parts)
            if (Part && Part->GetFName()==TEXT("BodyVisual") && Part->GetStaticMesh())
                BodyHalfWidth = Part->GetStaticMesh()->GetBoundingBox().GetExtent().Y * Part->GetComponentScale().Y;
        RouteGoal = FVector(FMath::Clamp(Target.Location.X,-3800.,3800.),E->PickupCurbY+BodyHalfWidth+30.,10);
    }
    else if (bDriving && !Target.bVehicleDestination)
    {
        // This map's coffee recipient is in the pedestrian-only west promenade.
        // Drive to the real adjoining parking opening, never onto the pedestrian NavMesh.
        RouteGoal = E->RideParkingCenter; CurrentStage=TEXT("Coffee.ParkThenWalk"); bWalkAfterParking=true;
        TargetLabel=TEXT("一杯海风 · 海滨停车后步行交付");
    }
    const bool bChanged = !bInitialized || bDriving != bLastDriving || CurrentStage != StageId ||
        Target.QuestId != QuestId || FVector::DistSquared2D(RouteGoal,PlannedTarget)>FMath::Square(120.);
    bTargetValid = !RouteGoal.ContainsNaN();
    TargetLocation = RouteGoal; StageId=CurrentStage; QuestId=Target.QuestId;
    StraightDistanceMeters=FVector::Dist2D(Position,TargetLocation)/100.;
    const double Now=GetWorld()->GetTimeSeconds();
    const bool bDeviation = bRouteValid && DistanceToRoute(Position)>(bDriving?250.f:120.f);
    if (bTargetValid && (bChanged || (Now>=NextPlanAt && (bDeviation || !bRouteValid || FVector::DistSquared2D(Position,PlannedStart)>FMath::Square(180.)))))
    {
        NextPlanAt=Now+.75; ++PlanCount;
        RoutePoints.Reset(); bRouteValid=false;
        PlannedTarget=RouteGoal; PlannedStart=Position; bLastDriving=bDriving; bInitialized=true;
        if (MapData) bRouteValid = bDriving ? PlanDriving(Position,RouteGoal) : PlanWalking(Position,RouteGoal);
        else RouteMessage=TEXT("地图资源不可用，路线未生成");
    }
    if (bRouteValid)
    {
        // Remaining length from the closest route segment, rather than stale total at planning time.
        double Best=TNumericLimits<double>::Max(),Remaining=0;
        int32 Nearest=0; FVector Projected=Position;
        for (int32 I=0;I+1<RoutePoints.Num();++I)
        {
            const FVector A(RoutePoints[I].X,RoutePoints[I].Y,0),B(RoutePoints[I+1].X,RoutePoints[I+1].Y,0);
            const FVector P=FMath::ClosestPointOnSegment(FVector(Position.X,Position.Y,0),A,B);
            const double D=FVector::DistSquared2D(P,Position);
            if(D<Best){Best=D;Nearest=I;Projected=P;}
        }
        if(RoutePoints.IsValidIndex(Nearest+1))Remaining=FVector::Dist2D(Projected,RoutePoints[Nearest+1]);
        for(int32 I=Nearest+1;I+1<RoutePoints.Num();++I)Remaining+=FVector::Dist2D(RoutePoints[I],RoutePoints[I+1]);
        RouteDistanceMeters=float(Remaining/100.);
        if(bDriving) RouteMessage=bWalkAfterParking?TEXT("沿道路行驶 · 停车后下车步行"):Target.bPickupPassenger?TEXT("沿道路行驶 · 平行路缘靠边停稳"):TEXT("沿道路行驶 · 标记车位内停稳");
        else RouteMessage=TEXT("步行可达路线 · 到达后按 E 互动");
    }
}

bool UHCM4R2NavigationComponent::PlanWalking(const FVector& Start,const FVector& Goal)
{
    UNavigationSystemV1* System=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    AHCM1Character* Character=Controller.IsValid()?Controller->GetControlledCharacter():nullptr;
    if(!System||!Character){RouteMessage=TEXT("步行导航数据不可用");return false;}
    const float Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Height=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2;
    FNavAgentProperties Agent(Radius,Height);
    Agent.SetPreferredNavData(AHCM4R2PlayerRecastNavMesh::StaticClass());
    const ANavigationData* Data=System->GetNavDataForProps(Agent,Start);
    FNavLocation From,To;
    if(!Data||!Data->IsA<AHCM4R2PlayerRecastNavMesh>()||!Data->IsRegistered()||
        !System->ProjectPointToNavigation(Character->GetNavAgentLocation(),From,FVector(130,130,180),Data)||
        !System->ProjectPointToNavigation(Goal,To,FVector(160,160,240),Data))
    {RouteMessage=TEXT("路线不可用 · 起点或目标不在可达地面");return false;}
    FPathFindingQuery Query(GetOwner(),*Data,From.Location,To.Location,Data->GetDefaultQueryFilter());
    Query.SetAllowPartialPaths(false);
    const FPathFindingResult Result=System->FindPathSync(Query);
    if(!Result.IsSuccessful()||!Result.Path.IsValid()||Result.IsPartial()||Result.Path->GetPathPoints().Num()<2)
    {RouteMessage=TEXT("路线不可用 · 无完整步行通路");return false;}
    for(const FNavPathPoint& P:Result.Path->GetPathPoints())RoutePoints.Add(P.Location);
    return true;
}

bool UHCM4R2NavigationComponent::DrivingConnectorClear(const FVector& Start,const FVector& End) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M4R2RouteConnector),false,GetOwner());
    if(Controller.IsValid())
    {Query.AddIgnoredActor(Controller->GetControlledCharacter());Query.AddIgnoredActor(Controller->GetActiveVehicle());}
    FHitResult Hit;
    const FVector A(Start.X,Start.Y,FMath::Max(Start.Z,End.Z)+100);
    const FVector B(End.X,End.Y,FMath::Max(Start.Z,End.Z)+100);
    return !GetWorld()->SweepSingleByObjectType(Hit,A,B,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(75),Query);
}

bool UHCM4R2NavigationComponent::PlanDriving(const FVector& Start,const FVector& Goal)
{
    FIntPoint StartEdge,GoalEdge; FVector StartProjection,GoalProjection;double StartDistance,GoalDistance;
    if(!ClosestGraphEdge(MapData,Start,StartEdge,StartProjection,StartDistance)||
        !ClosestGraphEdge(MapData,Goal,GoalEdge,GoalProjection,GoalDistance)||StartDistance>430||GoalDistance>430||
        !DrivingConnectorClear(Start,StartProjection)||!DrivingConnectorClear(GoalProjection,Goal))
    {RouteMessage=TEXT("路线不可用 · 请先驶入道路或停车通道");return false;}
    TArray<FVector> Nodes=MapData->RoadNodes;
    TArray<FIntPoint> Edges=MapData->RoadEdges;
    const int32 From=Nodes.Add(StartProjection),To=Nodes.Add(GoalProjection);
    Edges.Add(FIntPoint(From,StartEdge.X));Edges.Add(FIntPoint(From,StartEdge.Y));
    Edges.Add(FIntPoint(To,GoalEdge.X));Edges.Add(FIntPoint(To,GoalEdge.Y));
    if(StartEdge==GoalEdge)Edges.Add(FIntPoint(From,To));
    TArray<double> Cost;Cost.Init(TNumericLimits<double>::Max(),Nodes.Num());
    TArray<int32> Previous;Previous.Init(INDEX_NONE,Nodes.Num());
    TArray<bool> Visited;Visited.Init(false,Nodes.Num());Cost[From]=0;
    for(int32 Step=0;Step<Nodes.Num();++Step)
    {
        int32 Current=INDEX_NONE;double Best=TNumericLimits<double>::Max();
        for(int32 I=0;I<Nodes.Num();++I)if(!Visited[I]&&Cost[I]<Best){Current=I;Best=Cost[I];}
        if(Current==INDEX_NONE||Current==To)break;
        Visited[Current]=true;
        for(const FIntPoint& Edge:Edges)
        {
            const int32 Other=Edge.X==Current?Edge.Y:Edge.Y==Current?Edge.X:INDEX_NONE;
            if(!Nodes.IsValidIndex(Other)||Visited[Other])continue;
            const double Next=Cost[Current]+FVector::Dist2D(Nodes[Current],Nodes[Other]);
            if(Next<Cost[Other]){Cost[Other]=Next;Previous[Other]=Current;}
        }
    }
    if(Previous[To]==INDEX_NONE){RouteMessage=TEXT("路线不可用 · 道路节点未连通");return false;}
    TArray<int32> Reverse;int32 Cursor=To;
    while(Cursor!=INDEX_NONE&&Reverse.Num()<=Nodes.Num()){Reverse.Add(Cursor);if(Cursor==From)break;Cursor=Previous[Cursor];}
    if(Reverse.Last()!=From){RouteMessage=TEXT("路线不可用 · 道路搜索失败");return false;}
    RoutePoints.Add(Start);
    for(int32 I=Reverse.Num()-1;I>=0;--I)RoutePoints.Add(Nodes[Reverse[I]]);
    RoutePoints.Add(Goal);
    return true;
}

float UHCM4R2NavigationComponent::DistanceToRoute(const FVector& Position) const
{
    double Best=TNumericLimits<double>::Max();
    for(int32 I=0;I+1<RoutePoints.Num();++I)
    {
        const FVector A(RoutePoints[I].X,RoutePoints[I].Y,0),B(RoutePoints[I+1].X,RoutePoints[I+1].Y,0);
        const FVector P=FMath::ClosestPointOnSegment(FVector(Position.X,Position.Y,0),A,B);
        Best=FMath::Min(Best,FVector::Dist2D(Position,P));
    }
    return float(Best);
}

FString UHCM4R2NavigationComponent::GetDiagnostics() const
{
    TSharedRef<FJsonObject> Data=MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("enabled"),bMinimapEnabled);Data->SetBoolField(TEXT("map_loaded"),IsValid(MapData));
    Data->SetBoolField(TEXT("texture_loaded"),MapData&&IsValid(MapData->Background));
    Data->SetStringField(TEXT("bake_identity"),MapData?MapData->BakeIdentity:FString());
    Data->SetStringField(TEXT("quest_id"),QuestId.ToString());Data->SetStringField(TEXT("stage_id"),StageId.ToString());
    Data->SetBoolField(TEXT("target_valid"),bTargetValid);Data->SetBoolField(TEXT("route_valid"),bRouteValid);
    Data->SetBoolField(TEXT("driving"),IsDriving());Data->SetBoolField(TEXT("walk_after_parking"),bWalkAfterParking);
    Data->SetArrayField(TEXT("position"),XYZ(GetPosition()));Data->SetArrayField(TEXT("target"),XYZ(TargetLocation));
    Data->SetNumberField(TEXT("heading_degrees"),GetHeadingDegrees());Data->SetNumberField(TEXT("route_m"),RouteDistanceMeters);
    Data->SetNumberField(TEXT("straight_m"),StraightDistanceMeters);Data->SetNumberField(TEXT("plan_count"),PlanCount);
    Data->SetStringField(TEXT("message"),GetNavigationText());
    TArray<TSharedPtr<FJsonValue>> Points;
    for(const FVector& P:RoutePoints)Points.Add(MakeShared<FJsonValueArray>(XYZ(P)));
    Data->SetArrayField(TEXT("route"),Points);
    FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Data,Writer);return Out;
}
