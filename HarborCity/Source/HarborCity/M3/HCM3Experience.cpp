#include "HCM3Experience.h"
#include "HCM3NPC.h"
#include "HCM3NavRegion.h"
#include "HCM3Recording.h"
#include "M5VS3/HCM5VS3QA.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1Vehicle.h"
#include "M1/HCM1SaveGame.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

namespace
{
    bool M4PickupBodyBounds(const AHCM1Vehicle* Vehicle, FBox& Bounds)
    {
        if (!IsValid(Vehicle)) return false;
        TInlineComponentArray<UStaticMeshComponent*> Components(Vehicle);
        for (const UStaticMeshComponent* Component : Components)
            if (Component && Component->GetFName() == TEXT("BodyVisual") && Component->GetStaticMesh())
            {
                Bounds = Component->GetStaticMesh()->GetBoundingBox().TransformBy(Component->GetComponentTransform());
                return Bounds.IsValid != 0;
            }
        return false;
    }
}

AHCM3Experience::AHCM3Experience()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    CoffeeParcel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoffeeParcel"));
    CoffeeParcel->SetupAttachment(RootComponent);
    CoffeeParcel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CoffeeParcel->SetCanEverAffectNavigation(false);
    ParkingMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParkingMarker"));
    ParkingMarker->SetupAttachment(RootComponent);
    ParkingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ParkingMarker->SetCanEverAffectNavigation(false);
    ParkingMarker->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Cylinder.Succeeded()) CoffeeParcel->SetStaticMesh(Cylinder.Object);
    CoffeeParcel->SetRelativeScale3D(FVector(.08, .08, .12));
}

void AHCM3Experience::BeginPlay()
{
    Super::BeginPlay();
    for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC))
    {
        InitialNPCStates.Add(NPC->CaptureState());
        NPC->CorpseLifetimeSeconds = FMath::Clamp(CorpseLifetimeSeconds, 1.f, 600.f);
        NPC->CorpseFadeSeconds = FMath::Clamp(CorpseFadeSeconds, .1f, 5.f);
    }
#if !UE_BUILD_SHIPPING
    int32 Enabled = 1;
    if (FParse::Value(FCommandLine::Get(), TEXT("HCM3NPCs="), Enabled))
    {
        // The opt-in benchmark switch must run after placed pawns and their
        // auto-spawned controllers finish BeginPlay/tick registration. Applying
        // it here synchronously left controller ticks enabled in the first
        // packaged off-run (the NPC/mesh/movement ticks were already disabled).
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Enabled]
        {
            SetNPCsEnabled(Enabled != 0);
            UE_LOG(LogTemp, Display, TEXT("M3_NPC_PERF_OVERRIDE_AFTER_BEGINPLAY requested=%d"), Enabled);
        }));
        // One bounded readback after all authored actors have begun play. This is
        // evidence for the opt-in performance comparison, not normal game logging.
        FTimerHandle Readback;
        GetWorldTimerManager().SetTimer(Readback, FTimerDelegate::CreateWeakLambda(this, [this, Enabled]
        {
            int32 Valid=0, Hidden=0, ActorTicks=0, MeshTicks=0, MovementTicks=0, AITicks=0, Disabled=0;
            for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC))
            {
                ++Valid; Hidden += NPC->IsHidden(); ActorTicks += NPC->IsActorTickEnabled();
                MeshTicks += NPC->GetMesh()->IsComponentTickEnabled();
                MovementTicks += NPC->GetCharacterMovement()->IsComponentTickEnabled();
                AITicks += NPC->GetController() && NPC->GetController()->IsActorTickEnabled();
                Disabled += NPC->GetBehaviourState() == EHCM3NPCBehaviour::Disabled;
            }
            UE_LOG(LogTemp, Display, TEXT("M3_NPC_PERF_READBACK requested=%d enabled=%d valid=%d hidden=%d actor_ticks=%d mesh_ticks=%d movement_ticks=%d ai_ticks=%d disabled=%d"),
                Enabled, GetEnabledNPCCount(), Valid, Hidden, ActorTicks, MeshTicks, MovementTicks, AITicks, Disabled);
        }), 2.0f, false);
    }
    float RecordSeconds = 0;
    const bool HasM4Recording = FParse::Value(FCommandLine::Get(), TEXT("HCM4RecordSeconds="), RecordSeconds);
    if (AHCM3Recording::IsVS2GameplayCaptureRequested())
    {
        // One opt-in recorder even if a private review world contains more than
        // one Experience. Legacy recording behavior below remains unchanged.
        TActorIterator<AHCM3Recording> Existing(GetWorld());
        if (!Existing) GetWorld()->SpawnActor<AHCM3Recording>();
    }
    else if ((HasM4Recording || FParse::Value(FCommandLine::Get(), TEXT("HCM3RecordSeconds="), RecordSeconds)) && RecordSeconds > 0)
        GetWorld()->SpawnActor<AHCM3Recording>();
#endif
#if UE_BUILD_SHIPPING
    // The recorder itself supports Shipping QA; spawn it only for the same
    // explicit local VS3 slot/mode. Plain gameplay has no recording actor.
    float VS3RecordSeconds = 0;
    if (HCM5VS3LocalQA() && FParse::Value(FCommandLine::Get(), TEXT("HCM4RecordSeconds="), VS3RecordSeconds)
        && VS3RecordSeconds > 0 && VS3RecordSeconds <= 360)
    {
        TActorIterator<AHCM3Recording> Existing(GetWorld());
        if (!Existing) GetWorld()->SpawnActor<AHCM3Recording>();
    }
#endif
    UpdateVisuals();
}

AHCM3NPC* AHCM3Experience::FindNPC(FName Id) const
{
    for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC) && NPC->StableId == Id) return NPC;
    return nullptr;
}

int32 AHCM3Experience::GetEnabledNPCCount() const
{
    int32 Count = 0;
    if (bNPCEnabled) for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC) && !NPC->IsDead()) ++Count;
    return Count;
}

void AHCM3Experience::SetNPCsEnabled(bool bEnabled)
{
    bNPCEnabled = bEnabled;
    for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC)) NPC->SetNPCEnabled(bEnabled);
}

void AHCM3Experience::UpdateVisuals()
{
    CoffeeParcel->SetWorldLocation(CoffeeParcelLocation);
    CoffeeParcel->SetVisibility(CoffeeStage == EHCM3QuestStage::Available);
    ParkingMarker->SetWorldLocation(RideParkingCenter + FVector(0, 0, .8));
    ParkingMarker->SetVisibility(RideStage == EHCM3QuestStage::Active);
}

void AHCM3Experience::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateDeathAndRespawn();
    AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (!PC) return;
    AHCM1Character* Character = PC->GetControlledCharacter();
    if (IsValid(Character) && PlayerAppearanceMesh && (!bAppearanceApplied || AppearancePawn.Get() != Character))
    {
        // Same pawn, capsule, relative mesh transform, camera and input; appearance only.
        Character->GetMesh()->SetSkeletalMesh(PlayerAppearanceMesh);
        if (PlayerAnimationClass) Character->GetMesh()->SetAnimInstanceClass(PlayerAnimationClass);
        AppearancePawn = Character;
        bAppearanceApplied = true;
    }
    if (!bNPCEnabled || PC->IsPauseMenuOpen() || !PC->IsGameplayFocused()) return;
    if (RideStage != EHCM3QuestStage::Active) return;
    AHCM3NPC* Passenger = FindNPC(PassengerId);
    AHCM1Vehicle* Vehicle = PC->GetActiveVehicle();
    if (!Passenger || Passenger->IsDead() || !Vehicle || PC->GetPlayerMode() != EHCPlayerMode::Driving)
    { ParkedSeconds = 0; return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Passenger->GetIsPassenger())
    {
        ParkedSeconds = 0;
        if (Vehicle->GetSpeedKmh() <= 1 && Now >= NextBoardAttempt && CanPickupAtCurb(Passenger, Vehicle)
            && FVector::DistSquared2D(Passenger->GetActorLocation(), Vehicle->GetActorLocation()) < FMath::Square(600.f))
        {
            NextBoardAttempt = Now + 2;
            if (Passenger->TryBoardPassenger(Vehicle))
                PC->ShowStatusMessage(TEXT("阿宁已上车。请沿道路开往海滨标记车位，停稳送达。"), 5);
        }
        return;
    }
    const FVector Offset = Vehicle->GetActorLocation() - RideParkingCenter;
    const bool bInside = FMath::Abs(Offset.X) <= RideParkingExtent.X && FMath::Abs(Offset.Y) <= RideParkingExtent.Y
        && FMath::Abs(Offset.Z) <= RideParkingExtent.Z;
    if (!bInside || Vehicle->GetSpeedKmh() > 1 || Vehicle->GetActorUpVector().Z < .8)
    { ParkedSeconds = 0; return; }
    ParkedSeconds += DeltaSeconds;
    if (ParkedSeconds < 1.25) return;
    if (Passenger->TryRestorePassenger(PassengerDropoff, DropoffRegion))
    {
        RideStage = EHCM3QuestStage::Complete;
        Passenger->bStationary = true;
        UpdateVisuals();
        PC->ShowStatusMessage(TEXT("接送完成！阿宁已在海滨步道安全下车。谢谢你。"), 7);
    }
}

TArray<FString> AHCM3Experience::GetDialogueLines(AHCM3NPC* NPC) const
{
    if (!IsValid(NPC) || NPC->IsDead()) return {};
    if ((NPC->StableId == CoffeeGiverId || NPC->StableId == CoffeeRecipientId) && CoffeeStage == EHCM3QuestStage::Failed)
        return {TEXT("这次送咖啡的委托已经失败。"), TEXT("请等相关人员恢复后再来接取，进度会从头开始。")};
    if (NPC->StableId == PassengerId && RideStage == EHCM3QuestStage::Failed)
        return {TEXT("这次接送已经失败。"), TEXT("请等阿宁恢复后再重新接取。")};
    if (NPC->StableId == CoffeeGiverId)
    {
        if (CoffeeStage == EHCM3QuestStage::Available)
            return {TEXT("你好，我是林夏，欢迎来到潮汐咖啡。"), TEXT("能帮我把这杯咖啡送给海边长椅旁的陈伯吗？"),
                TEXT("他在海滨步道西侧。请步行送过去，过马路时留意车辆。"), TEXT("接过咖啡就可以出发；现在不方便，也可以先结束对话。")};
        if (CoffeeStage == EHCM3QuestStage::Active)
            return {TEXT("咖啡已经交给你了。"), TEXT("陈伯在海滨步道西侧的长椅旁，找到他后按 E 交谈。"), TEXT("不着急，路上注意安全。")};
        return {TEXT("陈伯收到咖啡了，谢谢你帮忙。"), TEXT("有空再来坐坐，海风大的时候这里很暖和。")};
    }
    if (NPC->StableId == CoffeeRecipientId)
    {
        if (CoffeeStage == EHCM3QuestStage::Active && bHasCoffee)
            return {TEXT("你好，这杯是林夏托你带来的咖啡吧？"), TEXT("正好，我刚在海边走了一圈。"), TEXT("辛苦你了。我收下咖啡，谢谢！")};
        if (CoffeeStage == EHCM3QuestStage::Complete)
            return {TEXT("咖啡收到了，很香。"), TEXT("这里看海最舒服，慢慢逛吧。")};
        return {TEXT("你好，我姓陈，常来这里散步。"), TEXT("商业街的潮汐咖啡不错，林夏就在店里。"), TEXT("黄昏再来看看，灯亮起来很漂亮。")};
    }
    if (NPC->StableId == PassengerId)
    {
        if (RideStage == EHCM3QuestStage::Available)
            return {TEXT("你好，我叫阿宁，第一次来这条海滨街。"), TEXT("可以搭你的车去海滨停车区吗？"),
                TEXT("接下委托后，把车开到我旁边靠边停稳，我会从安全的一侧上车。"), TEXT("到了海滨标记车位，停稳一会儿就好。现在不方便也没关系。")};
        if (RideStage == EHCM3QuestStage::Active)
            return {TEXT("我在这里等你，请把车靠边停稳。"), TEXT("上车后请开往海滨标记车位。路不远，慢慢开。")};
        return {TEXT("已经到海边了，谢谢你送我过来。"), TEXT("我想沿着步道看看风景，再见！")};
    }
    return NPC->DialogueLines;
}

FString AHCM3Experience::GetDialogueFinishLabel(AHCM3NPC* NPC) const
{
    if (NPC && NPC->StableId == CoffeeGiverId && CoffeeStage == EHCM3QuestStage::Available) return TEXT("E 接过咖啡");
    if (NPC && NPC->StableId == CoffeeRecipientId && CoffeeStage == EHCM3QuestStage::Active && bHasCoffee) return TEXT("E 交付咖啡");
    if (NPC && NPC->StableId == PassengerId && RideStage == EHCM3QuestStage::Available) return TEXT("E 接下接送委托");
    return TEXT("E 结束对话");
}

void AHCM3Experience::CompleteDialogue(AHCM3NPC* NPC, AHCM1PlayerController* PC)
{
    if (!IsValid(NPC) || NPC->IsDead() || NPC->IsPanicking() || !PC || !bNPCEnabled || !PC->CanReachInteraction(NPC)) return;
    if (NPC->StableId == CoffeeGiverId && CoffeeStage == EHCM3QuestStage::Available)
    {
        CoffeeStage = EHCM3QuestStage::Active; bHasCoffee = true;
        CoffeeAcceptedOrder = FMath::Max(CoffeeAcceptedOrder, RideAcceptedOrder) + 1;
        TrackedQuestId = TEXT("Coffee");
        PC->ShowStatusMessage(TEXT("已接取「一杯海风」：把咖啡送到海滨西侧长椅旁的陈伯。"), 6);
    }
    else if (NPC->StableId == CoffeeRecipientId && CoffeeStage == EHCM3QuestStage::Active && bHasCoffee)
    {
        CoffeeStage = EHCM3QuestStage::Complete; bHasCoffee = false; bCoffeeDelivered = true;
        PC->ShowStatusMessage(TEXT("送咖啡完成！陈伯已收下咖啡。"), 7);
    }
    else if (NPC->StableId == PassengerId && RideStage == EHCM3QuestStage::Available)
    {
        RideStage = EHCM3QuestStage::Active;
        RideAcceptedOrder = FMath::Max(CoffeeAcceptedOrder, RideAcceptedOrder) + 1;
        TrackedQuestId = TEXT("Ride");
        PC->ShowStatusMessage(TEXT("已接取「顺路看海」：上车后开到阿宁身旁，靠边停稳接人。"), 6);
    }
    UpdateVisuals();
}

FString AHCM3Experience::GetQuestHUD(const AHCM1PlayerController* PC) const
{
    if (!bNPCEnabled) return FString();
    const auto Distance = [PC](const FVector& P)
    { return PC && PC->GetPawn() ? FVector::Dist2D(PC->GetPawn()->GetActorLocation(), P) / 100. : 0.; };
    FString Coffee;
    if (CoffeeStage == EHCM3QuestStage::Available) Coffee = TEXT("一杯海风 · 未接取\n到潮汐咖啡与林夏交谈");
    else if (CoffeeStage == EHCM3QuestStage::Complete) Coffee = TEXT("一杯海风 · 已完成\n咖啡已交给陈伯");
    else if (CoffeeStage == EHCM3QuestStage::Failed)
        Coffee = FString::Printf(TEXT("一杯海风 · 失败\n关键人物死亡，恢复后可重接（约 %.0f 秒）"), FMath::CeilToFloat(FMath::Max(GetNamedRespawnRemaining(CoffeeGiverId), GetNamedRespawnRemaining(CoffeeRecipientId))));
    else if (const AHCM3NPC* NPC = FindNPC(CoffeeRecipientId))
        Coffee = FString::Printf(TEXT("一杯海风 · 进行中\n携带咖啡 → 海滨西侧陈伯  %.0f m"), Distance(NPC->GetActorLocation()));
    FString Ride;
    AHCM3NPC* Passenger = FindNPC(PassengerId);
    if (RideStage == EHCM3QuestStage::Available) Ride = TEXT("顺路看海 · 未接取\n到商业街人行道与阿宁交谈");
    else if (RideStage == EHCM3QuestStage::Complete) Ride = TEXT("顺路看海 · 已完成\n阿宁已抵达海滨步道");
    else if (RideStage == EHCM3QuestStage::Failed)
        Ride = FString::Printf(TEXT("顺路看海 · 失败\n阿宁死亡，恢复后可重接（约 %.0f 秒）"), FMath::CeilToFloat(GetNamedRespawnRemaining(PassengerId)));
    else if (Passenger && !Passenger->GetIsPassenger())
        Ride = FString::Printf(TEXT("顺路看海 · 进行中\n开车到阿宁身旁，靠边停稳  %.0f m"), Distance(Passenger->GetActorLocation()));
    else Ride = FString::Printf(TEXT("顺路看海 · 进行中\n海滨西侧标记车位，停稳送达  %.0f m"), Distance(RideParkingCenter));
    return Coffee + TEXT("\n\n") + Ride;
}

FName AHCM3Experience::GetTrackedQuestId() const
{
    if (!bNPCEnabled) return NAME_None;
    if (TrackedQuestId == TEXT("Coffee") && CoffeeStage == EHCM3QuestStage::Active) return TrackedQuestId;
    if (TrackedQuestId == TEXT("Ride") && RideStage == EHCM3QuestStage::Active) return TrackedQuestId;
    if (CoffeeStage == EHCM3QuestStage::Active && RideStage == EHCM3QuestStage::Active)
        return CoffeeAcceptedOrder > RideAcceptedOrder ? FName(TEXT("Coffee")) : FName(TEXT("Ride"));
    if (CoffeeStage == EHCM3QuestStage::Active) return TEXT("Coffee");
    if (RideStage == EHCM3QuestStage::Active) return TEXT("Ride");
    return NAME_None;
}

bool AHCM3Experience::GetNavigationTarget(FHCM3QuestNavigationTarget& Out) const
{
    Out = FHCM3QuestNavigationTarget();
    Out.QuestId = GetTrackedQuestId();
    if (Out.QuestId == TEXT("Coffee"))
    {
        const AHCM3NPC* Recipient = FindNPC(CoffeeRecipientId);
        if (!IsValid(Recipient) || Recipient->IsDead() || !bHasCoffee) return false;
        Out.StageId = TEXT("Coffee.Deliver"); Out.TargetId = CoffeeRecipientId;
        Out.Location = Recipient->GetNavAgentLocation(); Out.Label = TEXT("一杯海风 · 交给陈伯");
        return true;
    }
    if (Out.QuestId == TEXT("Ride"))
    {
        const AHCM3NPC* Passenger = FindNPC(PassengerId);
        if (!IsValid(Passenger) || Passenger->IsDead()) return false;
        Out.bPickupPassenger = !Passenger->GetIsPassenger();
        Out.bVehicleDestination = !Out.bPickupPassenger;
        Out.StageId = Out.bPickupPassenger ? TEXT("Ride.Pickup") : TEXT("Ride.Dropoff");
        Out.TargetId = Out.bPickupPassenger ? PassengerId : FName(TEXT("M3_RideParking"));
        Out.Location = Out.bPickupPassenger ? Passenger->GetNavAgentLocation() : RideParkingCenter;
        Out.Label = Out.bPickupPassenger ? TEXT("顺路看海 · 靠边接阿宁") : TEXT("顺路看海 · 海滨车位停稳");
        return true;
    }
    return false;
}

void AHCM3Experience::CycleTrackedQuest()
{
    const FName Current = GetTrackedQuestId();
    if (Current == TEXT("Coffee") && RideStage == EHCM3QuestStage::Active) TrackedQuestId = TEXT("Ride");
    else if (Current == TEXT("Ride") && CoffeeStage == EHCM3QuestStage::Active) TrackedQuestId = TEXT("Coffee");
    else TrackedQuestId = Current;
}

FString AHCM3Experience::GetTrackingLabel() const
{
    const FName Current = GetTrackedQuestId();
    if (Current == TEXT("Coffee")) return TEXT("追踪：一杯海风");
    if (Current == TEXT("Ride")) return TEXT("追踪：顺路看海");
    return TEXT("追踪：暂无进行中任务");
}

bool AHCM3Experience::FillSave(UHCM1SaveGame* Save, FString& Error) const
{
    if (!Save) return false;
    Save->M3Version = 1;
    Save->M4Version = 1;
    Save->CoffeeStage = CoffeeStage; Save->RideStage = RideStage;
    Save->bHasCoffee = bHasCoffee; Save->bCoffeeDelivered = bCoffeeDelivered;
    Save->M4R2NavigationVersion = 1;
    Save->M4R2TrackedQuestId = GetTrackedQuestId();
    Save->M4R2CoffeeAcceptedOrder = CoffeeAcceptedOrder;
    Save->M4R2RideAcceptedOrder = RideAcceptedOrder;
    Save->NPCStates.Reset();
    Save->NamedNPCRespawnSeconds.Reset();
    for (AHCM3NPC* NPC : NPCs)
    {
        if (!IsValid(NPC)) { Error = TEXT("路人引用异常，未写入存档。"); return false; }
        Save->NPCStates.Add(NPC->CaptureState());
        if (NPC->IsDead() && IsNamedId(NPC->StableId))
            Save->NamedNPCRespawnSeconds.Add(NPC->StableId, GetNamedRespawnRemaining(NPC->StableId));
    }
    return ValidateSave(Save, Error);
}

bool AHCM3Experience::ValidateSave(const UHCM1SaveGame* Save, FString& Error) const
{
    if (!Save) return false;
    if (Save->M4R2NavigationVersion < 0 || Save->M4R2NavigationVersion > 1 ||
        Save->M4R2CoffeeAcceptedOrder < 0 || Save->M4R2RideAcceptedOrder < 0 ||
        Save->M4R2CoffeeAcceptedOrder > 1000000 || Save->M4R2RideAcceptedOrder > 1000000 ||
        (!Save->M4R2TrackedQuestId.IsNone() && Save->M4R2TrackedQuestId != TEXT("Coffee") && Save->M4R2TrackedQuestId != TEXT("Ride")))
    { Error = TEXT("任务追踪存档字段无效，保留当前游戏。"); return false; }
    const bool bLegacy = Save->M3Version == 0;
    const bool bM4 = Save->M4Version == 1;
    if (Save->M4Version < 0 || Save->M4Version > 1 || (!bM4 && !Save->NamedNPCRespawnSeconds.IsEmpty()))
    { Error = TEXT("战斗存档版本或重生状态不一致，保留当前游戏。"); return false; }
    for (const auto& Cooldown : Save->NamedNPCRespawnSeconds)
        if (!IsNamedId(Cooldown.Key) || !FMath::IsFinite(Cooldown.Value) || Cooldown.Value <= 0 || Cooldown.Value > 1206.f)
        { Error = TEXT("具名角色重生冷却无效，保留当前游戏。"); return false; }
    // Old saves use initial NPCs, but their future layout still needs validation.
    const TArray<FHCM3NPCState>& States = bLegacy ? InitialNPCStates : Save->NPCStates;
    if ((!bLegacy && (Save->M3Version != 1 || uint8(Save->CoffeeStage) > (bM4 ? 3 : 2) || uint8(Save->RideStage) > (bM4 ? 3 : 2)
        || Save->bHasCoffee != (Save->CoffeeStage == EHCM3QuestStage::Active)
        || Save->bCoffeeDelivered != (Save->CoffeeStage == EHCM3QuestStage::Complete)))
        || States.Num() != NPCs.Num())
    { Error = TEXT("任务存档版本或进度不一致，保留当前游戏。"); return false; }
    if ((Save->CoffeeStage == EHCM3QuestStage::Failed &&
        !Save->NamedNPCRespawnSeconds.Contains(CoffeeGiverId) && !Save->NamedNPCRespawnSeconds.Contains(CoffeeRecipientId)) ||
        (Save->RideStage == EHCM3QuestStage::Failed && !Save->NamedNPCRespawnSeconds.Contains(PassengerId)) ||
        (Save->CoffeeStage == EHCM3QuestStage::Active &&
        (Save->NamedNPCRespawnSeconds.Contains(CoffeeGiverId) || Save->NamedNPCRespawnSeconds.Contains(CoffeeRecipientId))) ||
        (Save->RideStage == EHCM3QuestStage::Active && Save->NamedNPCRespawnSeconds.Contains(PassengerId)))
    { Error = TEXT("任务失败与关键人物冷却不一致，保留当前游戏。"); return false; }
    TSet<FName> Seen;
    AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    AHCM1Character* Character = PC ? PC->GetControlledCharacter() : nullptr;
    AHCM1Vehicle* SavedVehicle = nullptr;
    for (TActorIterator<AHCM1Vehicle> It(GetWorld()); It; ++It)
        if (It->StableId == Save->VehicleId) { SavedVehicle = *It; break; }
    for (const FHCM3NPCState& State : States)
    {
        AHCM3NPC* NPC = FindNPC(State.StableId);
        if (!NPC || Seen.Contains(State.StableId) || !NPC->CanRestoreState(State)
            || (State.bPassenger && (State.StableId != PassengerId || Save->RideStage != EHCM3QuestStage::Active)))
        { Error = FString::Printf(TEXT("路人存档状态无效（%s），保留当前游戏。"), *State.StableId.ToString()); return false; }
        Seen.Add(State.StableId);
        if (Save->NamedNPCRespawnSeconds.Contains(State.StableId))
        {
            if (State.bPassenger) { Error = TEXT("死亡乘客不能占用车位，保留当前游戏。"); return false; }
            continue; // Saved named cooldown has no corpse/standing occupancy.
        }
        if (State.bPassenger) continue; // Hidden seat reservation has no standing capsule.
        const float Radius = NPC->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float Half = NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector Position = State.Transform.GetLocation();
        const auto OverlapsCapsule = [&Position, Radius, Half](const FVector& OtherPosition, float OtherRadius, float OtherHalf)
        {
            return FMath::Abs(Position.Z - OtherPosition.Z) < Half + OtherHalf - 2
                && FVector::DistSquared2D(Position, OtherPosition) < FMath::Square(Radius + OtherRadius - 2);
        };
        if (Character && OverlapsCapsule(Save->PlayerTransform.GetLocation(),
            Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))
        { Error = TEXT("存档中的玩家与路人位置重叠，保留当前游戏。"); return false; }
        if (SavedVehicle && SavedVehicle->CalculateComponentsBoundingBoxInLocalSpace().ExpandBy(FVector(Radius, Radius, Half))
            .IsInsideOrOn(Save->VehicleTransform.InverseTransformPosition(Position)))
        { Error = TEXT("存档中的车辆与路人位置冲突，保留当前游戏。"); return false; }
        for (const FHCM3NPCState& Other : States)
        {
            if (Other.StableId == State.StableId || Other.bPassenger || Save->NamedNPCRespawnSeconds.Contains(Other.StableId)) continue;
            if (const AHCM3NPC* OtherNPC = FindNPC(Other.StableId))
                if (OverlapsCapsule(Other.Transform.GetLocation(), OtherNPC->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                    OtherNPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))
                { Error = TEXT("存档中的路人位置重叠，保留当前游戏。"); return false; }
        }
    }
    return true;
}

bool AHCM3Experience::ApplySave(const UHCM1SaveGame* Save, AHCM1Vehicle* Vehicle, FString& Error)
{
    if (!ValidateSave(Save, Error)) return false;
    const TArray<FHCM3NPCState>& States = Save->M3Version == 0 ? InitialNPCStates : Save->NPCStates;
    TArray<FHCM3NPCState> Previous;
    for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC)) Previous.Add(NPC->CaptureState());
    for (const FHCM3NPCState& State : States)
    {
        AHCM3NPC* NPC = FindNPC(State.StableId);
        if (!NPC || !NPC->RestoreState(State, Vehicle))
        {
            for (const FHCM3NPCState& Old : Previous) if (AHCM3NPC* Existing = FindNPC(Old.StableId)) Existing->RestoreState(Old, Vehicle);
            Error = TEXT("路人恢复位置受阻，任务进度未改变。"); return false;
        }
    }
    CoffeeStage = Save->M3Version == 0 ? EHCM3QuestStage::Available : Save->CoffeeStage;
    RideStage = Save->M3Version == 0 ? EHCM3QuestStage::Available : Save->RideStage;
    bHasCoffee = Save->M3Version != 0 && Save->bHasCoffee;
    bCoffeeDelivered = Save->M3Version != 0 && Save->bCoffeeDelivered;
    TrackedQuestId = Save->M4R2NavigationVersion == 1 ? Save->M4R2TrackedQuestId : NAME_None;
    CoffeeAcceptedOrder = Save->M4R2NavigationVersion == 1 ? Save->M4R2CoffeeAcceptedOrder : 0;
    RideAcceptedOrder = Save->M4R2NavigationVersion == 1 ? Save->M4R2RideAcceptedOrder : 0;
    DeathOrder.Reset(); AwaitingCorpseRemoval.Reset(); NamedRespawnAt.Reset();
    const double Now = GetWorld()->GetTimeSeconds();
    for (const auto& Cooldown : Save->NamedNPCRespawnSeconds)
        if (AHCM3NPC* NPC = FindNPC(Cooldown.Key))
        {
            NPC->RestoreNamedDeathCooldown();
            NamedRespawnAt.Add(Cooldown.Key, Now + Cooldown.Value);
        }
    ParkedSeconds = 0; NextBoardAttempt = 0;
    UpdateVisuals();
    return true;
}

bool AHCM3Experience::IsNamedId(FName Id) const
{
    const AHCM3NPC* NPC = FindNPC(Id);
    return NPC && NPC->bTalkable;
}

int32 AHCM3Experience::GetRagdollCount() const
{
    int32 Count = 0;
    for (const auto& Ref : DeathOrder) if (Ref.IsValid() && Ref->IsRagdollActive()) ++Count;
    return Count;
}

void AHCM3Experience::FailAffectedQuest(AHCM3NPC* NPC)
{
    bool Changed = false;
    FString Message;
    if ((NPC->StableId == CoffeeGiverId || NPC->StableId == CoffeeRecipientId) && CoffeeStage == EHCM3QuestStage::Active)
    {
        CoffeeStage = EHCM3QuestStage::Failed; bHasCoffee = false; bCoffeeDelivered = false;
        Changed = true; Message = TEXT("「一杯海风」失败：关键人物死亡。恢复后可重新接取。");
    }
    if (NPC->StableId == PassengerId && RideStage == EHCM3QuestStage::Active)
    {
        RideStage = EHCM3QuestStage::Failed; ParkedSeconds = 0; NextBoardAttempt = 0;
        Changed = true; Message = TEXT("「顺路看海」失败：阿宁死亡。车辆仍可正常驾驶和安全下车。");
    }
    if (Changed)
    {
        UpdateVisuals();
        if (AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
            PC->ShowStatusMessage(Message, 8);
    }
}

void AHCM3Experience::NotifyNPCDied(AHCM3NPC* NPC, const FVector& Location)
{
    if (!IsValid(NPC)) return;
    DeathOrder.AddUnique(NPC);
    if (IsNamedId(NPC->StableId)) AwaitingCorpseRemoval.Add(NPC->StableId);
    FailAffectedQuest(NPC);
    // Seeing a death requires an unobstructed line; hearing a shot is separate.
    for (AHCM3NPC* Other : NPCs)
    {
        if (!IsValid(Other) || Other == NPC || !Other->IsCombatEnabled() || Other->GetIsPassenger() ||
            FVector::DistSquared(Other->GetActorLocation(), Location) > FMath::Square(DeathWitnessRadius)) continue;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(M4DeathWitness), false, Other); Query.AddIgnoredActor(NPC);
        FHitResult Block;
        if (!GetWorld()->LineTraceSingleByChannel(Block, Other->GetActorLocation() + FVector(0, 0, 45), Location, ECC_Visibility, Query))
            Other->BeginPanic(Location, PanicDurationSeconds);
    }
    int32 Simulating = GetRagdollCount();
    const int32 Cap = FMath::Clamp(MaximumRagdolls, 1, 8);
    for (const auto& Ref : DeathOrder)
        if (Simulating > Cap && Ref.IsValid() && Ref->IsRagdollActive())
        { Ref->BeginCorpseRemoval(); --Simulating; }
}

void AHCM3Experience::NotifyGunshot(const FVector& Location, AActor* InstigatorActor)
{
    if (!bNPCEnabled || Location.ContainsNaN()) return;
    for (AHCM3NPC* NPC : NPCs)
        if (IsValid(NPC) && NPC->IsCombatEnabled() &&
            FVector::DistSquared(NPC->GetActorLocation(), Location) <= FMath::Square(FMath::Clamp(GunshotHearingRadius, 100.f, 5000.f)))
            NPC->BeginPanic(Location, PanicDurationSeconds);
    UE_LOG(LogTemp, Verbose, TEXT("M4_GUNSHOT_HEARING location=%s source=%s"), *Location.ToString(), *GetNameSafe(InstigatorActor));
}

float AHCM3Experience::GetNamedRespawnRemaining(FName Id) const
{
    const AHCM3NPC* NPC = FindNPC(Id);
    if (!NPC || !NPC->IsDead() || !IsNamedId(Id) || !GetWorld()) return 0;
    if (AwaitingCorpseRemoval.Contains(Id))
        return FMath::Max(1.f, NPC->GetSecondsUntilCorpseRemoval() + FMath::Clamp(NamedRespawnCooldownSeconds, 1.f, 600.f));
    if (const double* At = NamedRespawnAt.Find(Id)) return FMath::Max(1.f, float(*At - GetWorld()->GetTimeSeconds()));
    return FMath::Clamp(NamedRespawnCooldownSeconds, 1.f, 600.f);
}

void AHCM3Experience::UpdateDeathAndRespawn()
{
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto It = AwaitingCorpseRemoval.CreateIterator(); It; ++It)
        if (AHCM3NPC* NPC = FindNPC(*It))
        {
            if (!NPC->IsDead()) { It.RemoveCurrent(); continue; }
            if (!NPC->IsCorpsePresent())
            { NamedRespawnAt.Add(*It, Now + FMath::Clamp(NamedRespawnCooldownSeconds, 1.f, 600.f)); It.RemoveCurrent(); }
        }
    for (auto It = NamedRespawnAt.CreateIterator(); It; ++It)
    {
        AHCM3NPC* CurrentNPC = FindNPC(It.Key());
        if (CurrentNPC && !CurrentNPC->IsDead()) { It.RemoveCurrent(); continue; }
        if (Now >= It.Value())
        {
            AHCM3NPC* NPC = FindNPC(It.Key());
            if (NPC && NPC->RespawnAtAuthoredPost())
            {
                UE_LOG(LogTemp, Display, TEXT("M4_NPC_RESPAWNED id=%s"), *It.Key().ToString());
                It.RemoveCurrent();
            }
            else It.Value() = Now + 1; // Occupied post retries safely, never forces a spawn.
        }
    }
    bool Changed = false;
    const AHCM3NPC* Giver = FindNPC(CoffeeGiverId);
    const AHCM3NPC* Recipient = FindNPC(CoffeeRecipientId);
    if (CoffeeStage == EHCM3QuestStage::Failed && Giver && Recipient && !Giver->IsDead() && !Recipient->IsDead())
    { CoffeeStage = EHCM3QuestStage::Available; bHasCoffee = false; bCoffeeDelivered = false; Changed = true; }
    const AHCM3NPC* Passenger = FindNPC(PassengerId);
    if (RideStage == EHCM3QuestStage::Failed && Passenger && !Passenger->IsDead())
    { RideStage = EHCM3QuestStage::Available; Changed = true; }
    if (Changed)
    {
        UpdateVisuals();
        if (AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
            PC->ShowStatusMessage(TEXT("关键人物已恢复，失败的委托可以重新接取。"), 6);
    }
    DeathOrder.RemoveAll([](const TWeakObjectPtr<AHCM3NPC>& Ref) { return !Ref.IsValid() || !Ref->IsDead(); });
}

bool AHCM3Experience::CanPickupAtCurb(AHCM3NPC* NPC, AHCM1Vehicle* Vehicle) const
{
    const bool bVS3Town=GetWorld()&&GetWorld()->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS3/Town_"));
    if (!IsValid(NPC) || NPC->IsDead() || !IsValid(Vehicle) || Vehicle->GetSpeedKmh() > 1 ||
        Vehicle->GetActorUpVector().Z < .8 || !NPC->NavigationRegion ||
        NPC->NavigationRegion->StableId != (bVS3Town?FName(TEXT("VS2_Ride")):FName(TEXT("M3_Nav_Commercial")))) return false;
    FBox Body;
    if (!M4PickupBodyBounds(Vehicle, Body)) return false;
    const float Yaw = Vehicle->GetActorRotation().Yaw;
    const float ParallelError = FMath::Min(FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, 0.f)), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, 180.f)));
    const double Gap = Body.Min.Y - PickupCurbY;
    // The saved south-street curb is y=-3750cm (450cm from y=-3300 road
    // centreline). Use the real body, never the camera/whole-actor giant bounds.
    const float PickupMinX=bVS3Town?NPC->NavigationRegion->GetActorLocation().X-NPC->NavigationRegion->HalfExtent.X:-4200;
    const float PickupMaxX=bVS3Town?NPC->NavigationRegion->GetActorLocation().X+NPC->NavigationRegion->HalfExtent.X:4200;
    return ParallelError <= FMath::Clamp(PickupYawToleranceDegrees, 0.f, 30.f) && Gap >= 0 &&
        Gap <= FMath::Clamp(PickupBodyToCurbMaxGap, 0.f, 180.f) && Body.Min.X >= PickupMinX && Body.Max.X <= PickupMaxX;
}

FString AHCM3Experience::GetCombatWorldDiagnostics() const
{
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("ragdolls"), GetRagdollCount());
    Data->SetNumberField(TEXT("ragdoll_cap"), MaximumRagdolls);
    Data->SetNumberField(TEXT("corpse_lifetime_seconds"), CorpseLifetimeSeconds);
    Data->SetNumberField(TEXT("corpse_fade_seconds"), CorpseFadeSeconds);
    Data->SetNumberField(TEXT("named_post_removal_cooldown_seconds"), NamedRespawnCooldownSeconds);
    Data->SetNumberField(TEXT("coffee_stage"), uint8(CoffeeStage)); Data->SetNumberField(TEXT("ride_stage"), uint8(RideStage));
    Data->SetNumberField(TEXT("pickup_curb_y"), PickupCurbY); Data->SetNumberField(TEXT("pickup_max_body_gap_cm"), PickupBodyToCurbMaxGap);
    Data->SetNumberField(TEXT("pickup_parallel_tolerance_degrees"), PickupYawToleranceDegrees);
    AHCM1Vehicle* Vehicle = nullptr;
    for (TActorIterator<AHCM1Vehicle> It(GetWorld()); It; ++It) { Vehicle = *It; break; }
    FBox Body;
    if (M4PickupBodyBounds(Vehicle, Body))
    {
        Data->SetNumberField(TEXT("actual_pickup_body_gap_cm"), Body.Min.Y - PickupCurbY);
        Data->SetStringField(TEXT("actual_body_world_min"), Body.Min.ToString());
        Data->SetStringField(TEXT("actual_body_world_max"), Body.Max.ToString());
        Data->SetBoolField(TEXT("can_pickup_at_curb"), CanPickupAtCurb(FindNPC(PassengerId), Vehicle));
    }
    TSharedRef<FJsonObject> Cooldowns = MakeShared<FJsonObject>();
    for (AHCM3NPC* NPC : NPCs) if (IsValid(NPC) && IsNamedId(NPC->StableId))
        Cooldowns->SetNumberField(NPC->StableId.ToString(), GetNamedRespawnRemaining(NPC->StableId));
    Data->SetObjectField(TEXT("named_respawn_seconds"), Cooldowns);
    FString Output; const auto Writer = TJsonWriterFactory<>::Create(&Output); FJsonSerializer::Serialize(Data, Writer); return Output;
}
