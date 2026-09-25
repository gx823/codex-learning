#include "HCM3NPC.h"
#include "HCM3AIController.h"
#include "HCM3NavRegion.h"
#include "HCM3Experience.h"
#include "M4/HCM4Facing.h"
#include "M4/HCM4DamageTypes.h"
#include "M4/HCM4AnimationAppearance.h"
#include "M4/HCM4R1ReactionComponent.h"
#include "M4R1/HCM4R1PhysicalReactionComponent.h"
#include "M4R1/HCM4R1VehicleDamageType.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Vehicle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AHCM3NPC::AHCM3NPC()
{
    Reaction = CreateDefaultSubobject<UHCM4R1ReactionComponent>(TEXT("M4R1Reaction"));
    PhysicalReaction = CreateDefaultSubobject<UHCM4R1PhysicalReactionComponent>(TEXT("PhysicalReaction"));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    AIControllerClass = AHCM3AIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->InitCapsuleSize(32, 90);
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    GetCapsuleComponent()->SetNotifyRigidBodyCollision(true);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -89));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetMesh()->bEnableUpdateRateOptimizations = true;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> TemplateMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    if (TemplateMesh.Succeeded()) GetMesh()->SetSkeletalMesh(TemplateMesh.Object);
    if (Anim.Succeeded()) GetMesh()->SetAnimInstanceClass(Anim.Class);
    UCharacterMovementComponent* Move = GetCharacterMovement();
    Move->MaxWalkSpeed = WalkSpeed;
    Move->MaxAcceleration = 500;
    Move->BrakingDecelerationWalking = 600;
    Move->RotationRate = FRotator(0, 150, 0);
    Move->bOrientRotationToMovement = true;
    Move->bUseRVOAvoidance = false;
    Move->bEnablePhysicsInteraction = false;
    Move->GetNavMovementProperties()->bUseAccelerationForPaths = true;
    Move->NavAgentProps.bCanJump = false;
    Move->NavAgentProps.bCanSwim = false;
    Move->MaxDepenetrationWithPawn = 20;
    Move->MaxDepenetrationWithGeometry = 60;
}

void AHCM3NPC::BeginPlay()
{
    Super::BeginPlay();
    // Apply to existing authored instances as well as the native defaults.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(WalkSpeed, 40.0f, 220.0f);
    for (TActorIterator<AHCM3NavRegion> It(GetWorld()); It; ++It) Regions.Add(*It);
    for (TActorIterator<AHCM1Vehicle> It(GetWorld()); It; ++It) Vehicles.Add(*It);
    for (TActorIterator<AHCM3Experience> It(GetWorld()); It; ++It) { Experience = *It; break; }
    bM4Available = Experience.IsValid();
    Health = FMath::Clamp(MaxHealth, 1.f, 1000.f);
    bInitialStationary = bStationary;
    InitialMeshRelativeTransform = GetMesh()->GetRelativeTransform();
    AuthoredState = CaptureState();
    if (bM4Available)
    {
        // New component-level appearance only; old skeletal assets remain unchanged.
        ApplyInitialVisuals();
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
        GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    LastObservedPosition = LastProgressPosition = GetActorLocation();
    LastProgressTime = GetWorld()->GetTimeSeconds();
    NextDecisionTime = LastProgressTime + 0.5 + (GetTypeHash(StableId) % 100) * 0.015;
    ApplyVisibilityAndTicks();
}

bool AHCM3NPC::ApplyInitialVisuals()
{
    return UHCM4AnimationAppearance::ApplyNPCVisuals(this);
}

void AHCM3NPC::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reaction) Reaction->ResetReaction();
    if (PhysicalReaction) PhysicalReaction->ResetForRestore();
    StopNavigation();
    ConversationPartner.Reset();
    PassengerVehicle.Reset();
    Super::EndPlay(Reason);
}

AHCM3AIController* AHCM3NPC::GetNPCAI() const { return Cast<AHCM3AIController>(GetController()); }

void AHCM3NPC::StopNavigation()
{
    bStoppingMove = true;
    if (AHCM3AIController* AI = GetNPCAI()) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    bStoppingMove = false;
}

bool AHCM3NPC::IsPermittedFeetPoint(const FVector& Point, bool bForStanding, const AHCM3NavRegion* RegionOverride) const
{
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const AHCM3NavRegion* Allowed = RegionOverride ? RegionOverride : NavigationRegion.Get();
    bool bInside = IsValid(Allowed) && Allowed->Kind == EHCM3NavRegionKind::Allowed && Allowed->ContainsPoint(Point, Radius);
    // During an actual threat/return only, follow the union of authored safe areas.
    // The existing cafe door reserved box bridges the 60 cm gap between cafe and sidewalk.
    // It can be traversed but is never a standing destination. NavMesh and world collision
    // still determine the real doorway route; no straight-line wall crossing is introduced.
    if (!RegionOverride && (PanicUntil > 0 || (Reaction && Reaction->IsCountering()) || bReturningToPost))
        for (const auto& Ref : Regions)
            if (const AHCM3NavRegion* R = Ref.Get())
                if ((R->Kind == EHCM3NavRegionKind::Allowed && R->ContainsPoint(Point, Radius)) ||
                    (!bForStanding && R->StableId == TEXT("M3_Nav_CafeDoorStandClear") && R->ContainsPoint(Point, 0))) bInside = true;
    if (!bInside) return false;
    for (const TWeakObjectPtr<AHCM3NavRegion>& Ref : Regions)
        if (const AHCM3NavRegion* Region = Ref.Get())
            if ((Region->Kind == EHCM3NavRegionKind::Forbidden ||
                (bForStanding && Region->Kind == EHCM3NavRegionKind::Reserved)) && Region->ContainsPoint(Point, -Radius)) return false;
    return true;
}

AHCM3NavRegion* AHCM3NPC::FindRegionById(FName Id) const
{
    AHCM3NavRegion* Found = nullptr;
    if (Id.IsNone()) return nullptr;
    for (const TWeakObjectPtr<AHCM3NavRegion>& Ref : Regions)
        if (AHCM3NavRegion* Region = Ref.Get())
            if (Region->StableId == Id && Region->Kind == EHCM3NavRegionKind::Allowed)
            { if (Found) return nullptr; Found = Region; }
    return Found;
}

AHCM3NavRegion* AHCM3NPC::FindUniqueRegionAt(const FVector& Point) const
{
    AHCM3NavRegion* Found = nullptr;
    for (const TWeakObjectPtr<AHCM3NavRegion>& Ref : Regions)
        if (AHCM3NavRegion* Region = Ref.Get())
            if (Region->Kind == EHCM3NavRegionKind::Allowed && IsPermittedFeetPoint(Point, true, Region))
            { if (Found) return nullptr; Found = Region; }
    return Found;
}

bool AHCM3NPC::IsAtPermittedLocation() const
{
    const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Projected;
    return Nav && IsPermittedFeetPoint(Feet, false) &&
        Nav->ProjectPointToNavigation(Feet, Projected, FVector(20, 20, 50), &GetNavAgentPropertiesRef()) &&
        FVector::Dist2D(Feet, Projected.Location) < 20 && FMath::Abs(Feet.Z - Projected.Location.Z) < 35;
}

bool AHCM3NPC::RequestWalk(const FVector& FeetGoal, bool bSafetyMove, float AcceptanceRadius)
{
    AHCM3AIController* AI = GetNPCAI();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Goal;
    if (!AI || !Nav || !bNPCEnabled || bPassenger || bDead || IsPhysicalReactionActive() ||
        !Nav->ProjectPointToNavigation(FeetGoal, Goal, FVector(60, 60, 150), &GetNavAgentPropertiesRef()) ||
        FVector::Dist2D(Goal.Location, FeetGoal) > 80 || !IsPermittedFeetPoint(Goal.Location, true)) return false;
    FAIMoveRequest Request(Goal.Location);
    Request.SetAcceptanceRadius(FMath::Clamp(AcceptanceRadius, 5.f, 35.f));
    Request.SetReachTestIncludesAgentRadius(false);
    Request.SetAllowPartialPath(false);
    Request.SetUsePathfinding(true);
    Request.SetProjectGoalLocation(false);
    FNavPathSharedPtr Path;
    const FPathFollowingRequestResult Result = AI->MoveTo(Request, &Path);
    if (Result.Code == EPathFollowingRequestResult::Failed) return false;
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful && (!Path.IsValid() || Path->IsPartial()))
    { StopNavigation(); return false; }
    // Validate every traversed segment, not only its endpoint: disconnected safe zones
    // must never be joined by a shortest path through the road or a reserved car bay.
    if (Path.IsValid())
    {
        const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            const FVector A = Points[Index - 1].Location, B = Points[Index].Location;
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A, B) / 60));
            for (int32 Step = 0; Step <= Steps; ++Step)
                if (!IsPermittedFeetPoint(FMath::Lerp(A, B, float(Step) / Steps), false) ||
                    !IsPanicPathAllowed(FMath::Lerp(A, B, float(Step) / Steps))) { StopNavigation(); return false; }
        }
    }
    Behaviour = Result.Code == EPathFollowingRequestResult::AlreadyAtGoal ? EHCM3NPCBehaviour::Idle :
        (bSafetyMove ? EHCM3NPCBehaviour::Yielding : EHCM3NPCBehaviour::Walking);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    AI->ClearFocus(EAIFocusPriority::Gameplay);
    LastProgressTime = GetWorld()->GetTimeSeconds();
    LastProgressPosition = GetActorLocation();
    return true;
}

void AHCM3NPC::OnNavigationMoveFinished(bool bSuccess)
{
    if (bStoppingMove || !bNPCEnabled || bPassenger || bDead || IsPhysicalReactionActive() || IsConversationActive()) return;
    if (Reaction && Reaction->IsCountering()) { Behaviour = EHCM3NPCBehaviour::CounterApproach; return; }
    if (PanicUntil > 0) { Behaviour = EHCM3NPCBehaviour::Panic; NextPanicMove = GetWorld()->GetTimeSeconds() + .5; return; }
    if (!bSuccess) ++PathFailures;
    if (Behaviour == EHCM3NPCBehaviour::Walking && bSuccess && !PatrolPoints.IsEmpty()) PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
    Behaviour = EHCM3NPCBehaviour::Idle;
    NextDecisionTime = GetWorld()->GetTimeSeconds() + FMath::Max(PatrolWaitSeconds, 0.5f);
}

bool AHCM3NPC::TryYieldFrom(AActor* Obstacle, bool bVehicle)
{
    if (!IsValid(Obstacle) || bPassenger || !bNPCEnabled || bDead || IsPhysicalReactionActive()) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < SafetyCooldownUntil) return true;
    // A vehicle safety event ends conversation before any step-away movement.
    EndConversation();
    StopNavigation();
    const FVector Away = (GetActorLocation() - Obstacle->GetActorLocation()).GetSafeNormal2D();
    const FVector Side(-Away.Y, Away.X, 0);
    const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const float Step = bVehicle ? 250 : 115;
    ++SafetyYields;
    SafetyCooldownUntil = Now + (bVehicle ? 1.2 : 0.8);
    NextDecisionTime = SafetyCooldownUntil;
    for (const FVector& Offset : {Away * Step, Side * Step, -Side * Step, (Away + Side).GetSafeNormal() * Step})
    {
        FTransform Stand(GetActorRotation(), Feet + Offset + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
        FTransform Resolved;
        if (ResolveStandTransform(Stand, Resolved) && RequestWalk(Resolved.GetLocation() -
            FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), true)) return true;
    }
    // Anticipatory avoidance itself causes no damage. Real vehicle impact is
    // handled independently by its contact/sweep component, even if retreat fails.
    Behaviour = EHCM3NPCBehaviour::Yielding;
    return true;
}

bool AHCM3NPC::CheckNearbySafety()
{
    const FVector Here = GetActorLocation();
    for (const TWeakObjectPtr<AHCM1Vehicle>& Ref : Vehicles)
    {
        AHCM1Vehicle* Car = Ref.Get();
        if (!Car) continue;
        const FVector Velocity = Car->GetVelocity();
        const FVector Center = Car->GetMesh()->Bounds.Origin;
        const double SpeedSquared = Velocity.SizeSquared2D();
        const double AheadTime = SpeedSquared > 100 ? FMath::Clamp(FVector::DotProduct(Here - Center, Velocity) / SpeedSquared, 0.0, 1.0) : 0;
        const FBoxSphereBounds Body = Car->GetMesh()->CalcBounds(FTransform::Identity);
        const FTransform BodyTransform = Car->GetMesh()->GetComponentTransform();
        const FVector Local = BodyTransform.InverseTransformPosition(Here - Velocity * AheadTime) - Body.Origin;
        const FVector Padding = FVector(GetCapsuleComponent()->GetScaledCapsuleRadius() + 35) /
            BodyTransform.GetScale3D().GetAbs().ComponentMax(FVector(0.001));
        if (FMath::Abs(Local.X) < Body.BoxExtent.X + Padding.X && FMath::Abs(Local.Y) < Body.BoxExtent.Y + Padding.Y &&
            FMath::Abs(Local.Z) < Body.BoxExtent.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) return TryYieldFrom(Car, true);
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M3NPCNeighbours), false, this);
    TArray<FOverlapResult> Neighbours;
    GetWorld()->OverlapMultiByObjectType(Neighbours, Here, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(145), Query);
    for (const FOverlapResult& Hit : Neighbours)
    {
        ACharacter* Other = Cast<ACharacter>(Hit.GetActor());
        if (!Other || Other == this || Other->IsHidden()) continue;
        const FVector ToOther = (Other->GetActorLocation() - Here).GetSafeNormal2D();
        if (GetVelocity().SizeSquared2D() < 25 || FVector::DotProduct(GetVelocity().GetSafeNormal2D(), ToOther) < 0.25) continue;
        if (AHCM3NPC* NPC = Cast<AHCM3NPC>(Other))
            if (StableId.ToString().Compare(NPC->StableId.ToString()) < 0) continue;
        return TryYieldFrom(Other, false);
    }
    return false;
}

void AHCM3NPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double CombatNow = GetWorld()->GetTimeSeconds();
    if (bDead) { UpdateCorpse(CombatNow); return; }
    if (!bNPCEnabled || bPassenger) return;
    if (IsPhysicalReactionActive()) return;
    if (PanicUntil > 0 || bReturningToPost || (Reaction && Reaction->IsCountering()))
        if (AHCM3NavRegion* Here = FindUniqueRegionAt(GetActorLocation() - FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))) NavigationRegion = Here;
    if (HitStunUntil > CombatNow)
    {
        SetActorTickInterval(0.f);
        GetCharacterMovement()->bOrientRotationToMovement = false;
        GetCharacterMovement()->bUseControllerDesiredRotation = false;
        HCM4Facing::TurnBodyToward(this, HitFacingTarget, DeltaSeconds);
        return;
    }
    if (PanicUntil > 0)
    {
        SetActorTickInterval(.05f);
        UpdatePanic(CombatNow);
        return;
    }
    if (Reaction && Reaction->IsCountering()) { SetActorTickInterval(.05f); return; }
    ++DecisionTicks;
    const double Now = GetWorld()->GetTimeSeconds();
    TravelDistance += FVector::Dist2D(LastObservedPosition, GetActorLocation());
    LastObservedPosition = GetActorLocation();
    const APlayerController* Player = GetWorld()->GetFirstPlayerController();
    const APawn* PlayerPawn = Player ? Player->GetPawn() : nullptr;
    const float Distance = PlayerPawn ? FVector::Dist2D(GetActorLocation(), PlayerPawn->GetActorLocation()) : 0;
    SetActorTickInterval(Distance < 3500 ? 0.1f : Distance < 6000 ? 0.35f : 0.75f);
    if (Distance > 6000 && !IsConversationActive())
    {
        if (Behaviour != EHCM3NPCBehaviour::Dormant) StopNavigation();
        Behaviour = EHCM3NPCBehaviour::Dormant;
        GetMesh()->bPauseAnims = true;
        return;
    }
    GetMesh()->bPauseAnims = false;
    if (Behaviour == EHCM3NPCBehaviour::Dormant) { Behaviour = EHCM3NPCBehaviour::Idle; NextDecisionTime = Now + 0.3; }
    if (Behaviour == EHCM3NPCBehaviour::Conversation && !ConversationPartner.IsValid()) EndConversation();
    if (ConversationPartner.IsValid())
    {
        if (FVector::Dist2D(GetActorLocation(), ConversationPartner->GetActorLocation()) > 350) EndConversation();
        else
        {
            SetActorTickInterval(0.f);
            // Interval ticks can include idle time from before the dialogue.
            // Only the current world frame advances the visible conversation turn.
            HCM4Facing::TurnBodyToward(this, ConversationPartner->GetActorLocation(), GetWorld()->GetDeltaSeconds());
        }
    }
    if (CheckNearbySafety() || IsConversationActive()) return;
    if (!IsAtPermittedLocation())
    {
        StopNavigation();
        if (Now >= NextDecisionTime) { ++PathFailures; NextDecisionTime = Now + 2; }
        Behaviour = EHCM3NPCBehaviour::Idle;
        return;
    }
    if (Behaviour == EHCM3NPCBehaviour::Walking || Behaviour == EHCM3NPCBehaviour::Yielding)
    {
        if (FVector::DistSquared2D(LastProgressPosition, GetActorLocation()) > FMath::Square(20))
        { LastProgressPosition = GetActorLocation(); LastProgressTime = Now; }
        if (Now - LastProgressTime > 8)
        { StopNavigation(); ++PathFailures; Behaviour = EHCM3NPCBehaviour::Idle; NextDecisionTime = Now + 2; }
        if (Behaviour == EHCM3NPCBehaviour::Yielding && Now >= SafetyCooldownUntil && GetVelocity().IsNearlyZero()) Behaviour = EHCM3NPCBehaviour::Idle;
        return;
    }
    if (!bReturningToPost && !bStationary && !PatrolPoints.IsEmpty() && Now >= NextDecisionTime)
    {
        PatrolIndex = FMath::Clamp(PatrolIndex, 0, PatrolPoints.Num() - 1);
        // A passenger delivered into another authored region keeps a fixed post.
        // Its original commercial-street patrol must not be retried across the road.
        if (!IsPermittedFeetPoint(PatrolPoints[PatrolIndex], true)) { ++PathFailures; NextDecisionTime = Now + 2; return; }
        if (!RequestWalk(PatrolPoints[PatrolIndex])) { ++PathFailures; NextDecisionTime = Now + 2; }
        else if (Behaviour == EHCM3NPCBehaviour::Idle) { PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num(); NextDecisionTime = Now + PatrolWaitSeconds; }
    }
    else if (bReturningToPost && Now >= NextDecisionTime)
    {
        bReturningToPost = FVector::Dist2D(GetActorLocation(), AuthoredState.Transform.GetLocation()) > 50;
        if (bReturningToPost) RequestWalk(AuthoredState.Transform.GetLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
        NextDecisionTime = Now + 2;
    }
}

bool AHCM3NPC::BeginConversation(AActor* Partner)
{
    if (!bNPCEnabled || bPassenger || bDead || IsPhysicalReactionActive() || (Reaction && Reaction->IsCountering()) || PanicUntil > 0 || !bTalkable || !IsValid(Partner) || DialogueLines.IsEmpty() ||
        FVector::Dist2D(GetActorLocation(), Partner->GetActorLocation()) > 300) return false;
    StopNavigation();
    ConversationPartner = Partner;
    Behaviour = EHCM3NPCBehaviour::Conversation;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    PrimaryActorTick.UpdateTickIntervalAndCoolDown(0.f);
    if (AHCM3AIController* AI = GetNPCAI()) AI->ClearFocus(EAIFocusPriority::Gameplay);
    return true;
}

void AHCM3NPC::EndConversation()
{
    ConversationPartner.Reset();
    if (AHCM3AIController* AI = GetNPCAI()) AI->ClearFocus(EAIFocusPriority::Gameplay);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    if (Behaviour == EHCM3NPCBehaviour::Conversation) Behaviour = EHCM3NPCBehaviour::Idle;
    if (GetWorld()) NextDecisionTime = GetWorld()->GetTimeSeconds() + 1;
}

bool AHCM3NPC::ResolveStandTransform(const FTransform& Requested, FTransform& Out, const AHCM3NavRegion* RegionOverride, bool bSavedStateValidation) const
{
    auto Reject=[&](const TCHAR* Reason){if(bSavedStateValidation && FParse::Param(FCommandLine::Get(),TEXT("M5VS3SaveDiagnostics")))UE_LOG(LogTemp,Display,TEXT("VS3_SAVE_STAND_REJECT id=%s reason=%s pose=%s"),*StableId.ToString(),Reason,*Requested.GetLocation().ToString());return false;};
    if (Requested.ContainsNaN() || !GetWorld()) return false;
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Feet = Requested.GetLocation() - FVector(0, 0, Half);
    FNavLocation Projected;
    if(!Nav)return Reject(TEXT("nav system missing"));
    if(!Nav->ProjectPointToNavigation(Feet,Projected,FVector(65,65,160),&GetNavAgentPropertiesRef()))return Reject(TEXT("no matching nav polygon"));
    if(FVector::Dist2D(Feet,Projected.Location)>75)return Reject(TEXT("projection distance"));
    if(!IsPermittedFeetPoint(Projected.Location,true,RegionOverride))return Reject(TEXT("forbidden/reserved or outside allowed region"));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M3NPCStand), false, this);
    // Whole-save preflight runs before any actor moves. Ignore transient current
    // pawn positions here; the experience validates the future saved layout.
    if (bSavedStateValidation)
        for (TActorIterator<APawn> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, Projected.Location + FVector(0, 0, 55),
        Projected.Location - FVector(0, 0, 80), ECC_Visibility, Query) || Ground.ImpactNormal.Z < 0.8 || Cast<APawn>(Ground.GetActor())) return Reject(TEXT("ground trace or slope"));
    FVector Center(Ground.ImpactPoint.X, Ground.ImpactPoint.Y, Ground.ImpactPoint.Z + Half + 2);
    // Fit the same capsule used by the overlap preflight against the actual floor.
    // A centre ray alone can hit the recessed face of a bevelled floor mesh while
    // the wider capsule already touches its raised edge. Never ignore that floor.
    FHitResult CapsuleGround;
    if (GetWorld()->SweepSingleByChannel(CapsuleGround, Center + FVector(0,0,8),
        Center - FVector(0,0,4), FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius + 2, Half), Query)
        && !CapsuleGround.bStartPenetrating && CapsuleGround.ImpactNormal.Z >= .8f)
        Center.Z = FMath::Max(Center.Z, CapsuleGround.Location.Z + 2.f);
    if (GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius + 2, Half), Query)) {
        if(bSavedStateValidation&&FParse::Param(FCommandLine::Get(),TEXT("M5VS3SaveDiagnostics"))){
            TArray<FOverlapResult> Hits;GetWorld()->OverlapMultiByChannel(Hits,Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius+2,Half),Query);
            int32 Count=0;for(const auto& H:Hits)if(H.bBlockingHit&&Count++<4)UE_LOG(LogTemp,Display,TEXT("VS3_SAVE_BLOCKER id=%s actor=%s component=%s center=%s radius=%.3f half=%.3f ground=%s"),*StableId.ToString(),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*Center.ToString(),Radius,Half,*GetNameSafe(Ground.GetComponent()));
        }
        return Reject(TEXT("capsule overlaps geometry"));
    }
    Out = FTransform(FRotator(0, Requested.Rotator().Yaw, 0), Center, FVector::OneVector);
    return true;
}

bool AHCM3NPC::ResolvePassengerDoorStand(const FTransform& Requested, FTransform& Out) const
{
    return ResolveStandTransform(Requested, Out);
}

bool AHCM3NPC::TryBoardPassenger(AHCM1Vehicle* Vehicle)
{
    if (!bNPCEnabled || bPassenger || bDead || IsPhysicalReactionActive() || (Reaction && Reaction->IsCountering()) || PanicUntil > 0 || !IsValid(Vehicle) || Vehicle->GetSpeedKmh() > 1) return false;
    if (Experience.IsValid() && !Experience->CanPickupAtCurb(this, Vehicle)) return false;
    FTransform Door, Stand;
    if (!Vehicle->FindSafeExitTransform(this, Door) || !ResolvePassengerDoorStand(Door, Stand)) return false;
    const double DoorDistance = FVector::Dist2D(GetActorLocation(), Stand.GetLocation());
    // A safety retreat can leave the waiting passenger several metres from the
    // stopped car. Walk back through the already validated sidewalk NavMesh;
    // never make the visibility transition from the expanded approach radius.
    if (FVector::Dist2D(GetActorLocation(), Vehicle->GetActorLocation()) > 600) return false;
    if (DoorDistance > 70)
    {
        RequestWalk(Stand.GetLocation() - FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
        return false;
    }
    EndConversation(); StopNavigation();
    bPassenger = true; PassengerVehicle = Vehicle; Behaviour = EHCM3NPCBehaviour::Passenger;
    ApplyVisibilityAndTicks();
    return true;
}

bool AHCM3NPC::TryRestorePassenger(const FTransform& RequestedTransform, AHCM3NavRegion* DestinationRegion)
{
    if (!bPassenger || bDead) return false;
    if (!DestinationRegion) DestinationRegion = FindUniqueRegionAt(RequestedTransform.GetLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    if (!DestinationRegion) return false;
    FTransform Safe;
    if (!ResolveStandTransform(RequestedTransform, Safe, DestinationRegion)) return false;
    if (!SetActorTransform(Safe, false, nullptr, ETeleportType::TeleportPhysics)) return false;
    NavigationRegion = DestinationRegion;
    bPassenger = false; PassengerVehicle.Reset(); Behaviour = EHCM3NPCBehaviour::Idle;
    LastObservedPosition = LastProgressPosition = GetActorLocation();
    ApplyVisibilityAndTicks();
    NextDecisionTime = GetWorld()->GetTimeSeconds() + 1;
    return true;
}

void AHCM3NPC::ApplyVisibilityAndTicks()
{
    if (bDead)
    {
        SetActorHiddenInGame(bCorpseRemoved || !bNPCEnabled);
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCharacterMovement()->DisableMovement();
        GetCharacterMovement()->SetComponentTickEnabled(false);
        SetActorTickEnabled(!bCorpseRemoved && bNPCEnabled);
        return;
    }
    const bool bActive = bNPCEnabled && !bPassenger;
    SetActorHiddenInGame(!bActive);
    SetActorEnableCollision(bActive);
    SetActorTickEnabled(bActive);
    GetMesh()->SetComponentTickEnabled(bActive);
    GetMesh()->bPauseAnims = !bActive;
    GetCapsuleComponent()->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bM4Available)
    {
        GetMesh()->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
        GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    GetCharacterMovement()->SetComponentTickEnabled(bActive);
    if (bActive) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    else GetCharacterMovement()->DisableMovement();
    if (AHCM3AIController* AI = GetNPCAI())
    {
        AI->SetActorTickEnabled(bActive);
        if (AI->GetPathFollowingComponent()) AI->GetPathFollowingComponent()->SetComponentTickEnabled(bActive);
    }
}

void AHCM3NPC::SetNPCEnabled(bool bEnabled)
{
    if (!bEnabled)
    {
        if (Reaction) Reaction->ResetReaction();
        if (PhysicalReaction) PhysicalReaction->ResetForRestore();
    }
    EndConversation(); StopNavigation();
    bNPCEnabled = bEnabled;
    Behaviour = !bEnabled ? EHCM3NPCBehaviour::Disabled : bPassenger ? EHCM3NPCBehaviour::Passenger : EHCM3NPCBehaviour::Idle;
    LastObservedPosition = LastProgressPosition = GetActorLocation();
    ApplyVisibilityAndTicks();
}

FHCM3NPCState AHCM3NPC::CaptureState() const
{
    // Corpses and pedestrian death are deliberately not persisted. Named death
    // availability is stored separately by stable ID in the experience extension.
    if (bDead || IsPhysicalReactionActive()) return AuthoredState;
    FHCM3NPCState State;
    State.StableId = StableId; State.Transform = GetActorTransform(); State.PatrolIndex = PatrolIndex; State.bPassenger = bPassenger;
    State.RegionId = NavigationRegion ? NavigationRegion->StableId : NAME_None;
    // Flee/counter/return paths may legally traverse a reserved doorway which is
    // not a standing save destination. Keep valid current poses; normalize only
    // invalid transient ones to the already-authored pose after the same checks.
    // Whole-save validation still rejects overlaps with saved player/car/NPCs.
    if (!AuthoredState.StableId.IsNone() && !CanRestoreState(State))
    {
        const bool bTransient = PanicUntil > 0 || bReturningToPost || (Reaction && Reaction->IsCountering());
        const bool bUseAuthored = bTransient && !bPassenger && CanRestoreState(AuthoredState);
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
        if (Now >= NextSaveCaptureDiagnosticTime)
        {
            NextSaveCaptureDiagnosticTime = Now + 5;
            UE_LOG(LogTemp, Warning, TEXT("M4_R1_NPC_SAVE_CAPTURE id=%s region=%s pose=%s transient=%d panic=%d counter=%d returning=%d authored_valid_fallback=%d"),
                *StableId.ToString(), *State.RegionId.ToString(), *State.Transform.ToString(), bTransient,
                PanicUntil > 0, Reaction && Reaction->IsCountering(), bReturningToPost, bUseAuthored);
        }
        if (bUseAuthored) return AuthoredState;
    }
    return State;
}

bool AHCM3NPC::CanRestoreState(const FHCM3NPCState& State) const
{
    if (StableId.IsNone() || State.StableId != StableId || !State.Transform.IsValid() ||
        !State.Transform.GetScale3D().Equals(FVector::OneVector, .001) || State.PatrolIndex < 0 ||
        (!PatrolPoints.IsEmpty() && State.PatrolIndex >= PatrolPoints.Num())) return false;
    FTransform Safe;
    AHCM3NavRegion* SavedRegion = FindRegionById(State.RegionId);
    return SavedRegion && ResolveStandTransform(State.Transform, Safe, SavedRegion, true);
}

bool AHCM3NPC::RestoreState(const FHCM3NPCState& State, AHCM1Vehicle* Vehicle)
{
    if (!CanRestoreState(State) || (State.bPassenger && !IsValid(Vehicle))) return false;
    FTransform Safe;
    AHCM3NavRegion* SavedRegion = FindRegionById(State.RegionId);
    if (!SavedRegion || !ResolveStandTransform(State.Transform, Safe, SavedRegion, true)) return false;
    EndConversation(); StopNavigation();
    ResetCombatForRestore();
    if (!SetActorTransform(Safe, false, nullptr, ETeleportType::TeleportPhysics)) return false;
    PatrolIndex = State.PatrolIndex; bPassenger = State.bPassenger; PassengerVehicle = Vehicle; NavigationRegion = SavedRegion;
    Behaviour = bPassenger ? EHCM3NPCBehaviour::Passenger : EHCM3NPCBehaviour::Idle;
    LastObservedPosition = LastProgressPosition = GetActorLocation();
    LastProgressTime = GetWorld()->GetTimeSeconds();
    ApplyVisibilityAndTicks();
    return true;
}

void AHCM3NPC::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
    bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
    if (Cast<AHCM1Vehicle>(Other) && bNPCEnabled && !bPassenger && !bDead) { ++SafetyContacts; TryYieldFrom(Other, true); }
}

FString AHCM3NPC::GetInteractionText_Implementation(APlayerController* Player) const
{
    return bNPCEnabled && !bPassenger && !bDead && !IsPhysicalReactionActive() && !(Reaction && Reaction->IsCountering()) && PanicUntil <= 0 && bTalkable && !DialogueLines.IsEmpty() ? FString::Printf(TEXT("E 与%s交谈"), *DisplayName) : FString();
}

void AHCM3NPC::Interact_Implementation(APlayerController* Player)
{
    if (AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(Player)) PC->BeginNPCDialogue(this);
}

float AHCM3NPC::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!bM4Available || !IsCombatEnabled() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0 || !GetWorld()) return 0;
    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (!FMath::IsFinite(Applied) || Applied <= 0) return 0;
    FHitResult Hit;
    FVector ImpulseDirection;
    DamageEvent.GetBestHitInfo(this, DamageCauser, Hit, ImpulseDirection);
    FVector Source = EventInstigator && EventInstigator->GetPawn() ? EventInstigator->GetPawn()->GetActorLocation() :
        IsValid(DamageCauser) ? DamageCauser->GetActorLocation() : GetActorLocation() - ImpulseDirection * 100;
    if (Source.ContainsNaN()) Source = GetActorLocation() - GetActorForwardVector() * 100;
    const float SourceYaw = HCM4Facing::YawToTarget(GetActorLocation(), Source, GetActorRotation().Yaw);
    const float RelativeYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, SourceYaw);
    UAnimMontage* HitAnimation = nullptr;
    if (FMath::Abs(RelativeYaw) <= 45) { LastHitDirection = TEXT("Front"); HitAnimation = HitFrontMontage; }
    else if (FMath::Abs(RelativeYaw) >= 135) { LastHitDirection = TEXT("Back"); HitAnimation = HitBackMontage; }
    else if (RelativeYaw > 0) { LastHitDirection = TEXT("Right"); HitAnimation = HitRightMontage; }
    else { LastHitDirection = TEXT("Left"); HitAnimation = HitLeftMontage; }
    LastDamage = Applied; LastHitBone = Hit.BoneName; ++DamageEvents;
    Health = FMath::Max(0.f, Health - Applied);
    EndConversation();
    const bool bVehicleDamage = DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UHCM4R1VehicleDamageType::StaticClass());
    if (Health <= 0) { Die(Hit, ImpulseDirection.GetSafeNormal(), !bVehicleDamage); return Applied; }
    if (PhysicalReaction) PhysicalReaction->NotifySurvivingDamage(Source);
    if(!bVehicleDamage && PhysicalReaction && (DamageEvents%3==0 || (DamageEvents%2==0 && Health<=65.f)) && PhysicalReaction->TryAnimatedKnockdown(Source))return Applied;
    const double Now = GetWorld()->GetTimeSeconds();
    AActor* SourceActor = EventInstigator && EventInstigator->GetPawn() ? EventInstigator->GetPawn() : DamageCauser;
    const bool bMeleeDamage = DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UHCM4MeleeDamageType::StaticClass());
    const bool bCounter = Reaction && Reaction->ReceiveDamageThreat(SourceActor, bMeleeDamage, Source);
    if (!bCounter) BeginPanic(Source, Experience.IsValid() ? Experience->PanicDurationSeconds : 12.f);
    if (IsPhysicalReactionActive()) return Applied;
    if (Now - LastHitReactionTime >= FMath::Max(.1f, HitReactionCooldownSeconds))
    {
        LastHitReactionTime = Now;
        HitStunUntil = Now + FMath::Clamp(HitStunSeconds, .1f, .5f);
        HitFacingTarget = Source;
        Behaviour = EHCM3NPCBehaviour::HitReaction;
        StopNavigation();
        GetMesh()->bPauseAnims = false;
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            if (HitAnimation && Anim->Montage_Play(HitAnimation) > 0) ++ReactionPlays;
            else UE_LOG(LogTemp, Warning, TEXT("M4_NPC_HIT_REACTION_MISSING id=%s direction=%s montage=%s"), *StableId.ToString(), *LastHitDirection.ToString(), *GetNameSafe(HitAnimation));
        }
        SetActorTickInterval(0.f);
        // Only the third melee point-damage stage requests an additional push.
        // Validate its short swept destination first; CharacterMovement performs
        // the actual displacement and collision, never SetActorLocation damage.
        if (DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UHCM4MeleeDamageType::StaticClass()) && Applied >= 40.f)
        {
            const FVector Away = (GetActorLocation() - Source).GetSafeNormal2D();
            const FVector End = GetActorLocation() + Away * 60;
            FTransform Safe;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(M4MeleePush), false, this);
            FHitResult Block;
            if (ResolveStandTransform(FTransform(GetActorRotation(), End), Safe) &&
                !GetWorld()->SweepSingleByChannel(Block, GetActorLocation(), End, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Query))
                GetCharacterMovement()->AddImpulse(Away * 180.f, true);
        }
    }
    return Applied;
}

float AHCM3NPC::PreparePreRagdollReaction()
{
    const float Delay = GetPreRagdollReactionSeconds();
    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (Delay <= 0 || !Anim || GetMesh()->IsSimulatingPhysics() || bPassenger || !GetWorld()) return 0;
    UAnimMontage* Montage = LastHitDirection == TEXT("Back") ? HitBackMontage.Get() :
        LastHitDirection == TEXT("Left") ? HitLeftMontage.Get() :
        LastHitDirection == TEXT("Right") ? HitRightMontage.Get() : HitFrontMontage.Get();
    if (!Montage) return 0;
    EndConversation(); StopNavigation();
    if (Reaction) Reaction->ResetReaction();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetMesh()->bPauseAnims = false;
    GetMesh()->SetComponentTickEnabled(true);
    // Reuse the hit just accepted by TakeDamage, instead of restarting it in the same frame.
    if (!Anim->Montage_IsPlaying(Montage) || Anim->Montage_GetPosition(Montage) > .08f)
    {
        if (Anim->Montage_Play(Montage) <= 0) return 0;
        ++ReactionPlays;
    }
    PreRagdollMontage = Montage;
    PreRagdollStarted = GetWorld()->GetTimeSeconds(); PreRagdollStartedFrame = GFrameCounter;
    const float Duration = FMath::Clamp(Delay, .1f, .3f);
    HitStunUntil = PreRagdollStarted + Duration;
    if (!bDead) Behaviour = EHCM3NPCBehaviour::HitReaction;
    SetActorTickEnabled(true); SetActorTickInterval(0.f);
    return Duration;
}

bool AHCM3NPC::HasAdvancedPreRagdollReaction() const
{
    const UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    return Anim && PreRagdollMontage && GFrameCounter > PreRagdollStartedFrame + 1 &&
        Anim->Montage_GetPosition(PreRagdollMontage) >= .1f;
}

bool AHCM3NPC::QueueDeferredCorpseImpulse(const FVector& DeltaV)
{
    if (!bPendingCorpsePhysics || DeltaV.ContainsNaN()) return false;
    DeferredCorpseDeltaV = (DeferredCorpseDeltaV + DeltaV).GetClampedToMaxSize(1800.f);
    return true;
}

bool AHCM3NPC::IsPhysicalReactionActive() const
{
    return PhysicalReaction && PhysicalReaction->IsLivingRagdollActive();
}

void AHCM3NPC::BeginPhysicalReactionState()
{
    EndConversation(); StopNavigation();
    if (Reaction) Reaction->ResetReaction();
    HitStunUntil = 0; PanicUntil = 0;
    if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.08f);
    GetCharacterMovement()->DisableMovement(); GetCharacterMovement()->SetComponentTickEnabled(false);
    Behaviour = EHCM3NPCBehaviour::KnockedDown;
    SetActorTickEnabled(true); SetActorTickInterval(.05f);
}

bool AHCM3NPC::ResolvePhysicalRecoveryStand(const FVector& ActualPelvisLocation, FTransform& Out) const
{
    if (!GetWorld() || ActualPelvisLocation.ContainsNaN()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M4R1PhysicalStandGround), false, this);
    FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, ActualPelvisLocation + FVector(0,0,65),
        ActualPelvisLocation - FVector(0,0,350), ECC_Visibility, Query) || Ground.ImpactNormal.Z < .8f || Cast<APawn>(Ground.GetActor())) return false;
    AHCM3NavRegion* R = FindUniqueRegionAt(Ground.ImpactPoint);
    if (!R) return false;
    const FVector Center = Ground.ImpactPoint + FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
    return ResolveStandTransform(FTransform(FRotator(0,GetActorRotation().Yaw,0), Center), Out, R);
}

bool AHCM3NPC::ResolvePhysicalAuthoredRecoveryStand(FTransform& Out) const
{
    const AHCM3NavRegion* Region = FindRegionById(AuthoredState.RegionId);
    return Region && ResolveStandTransform(AuthoredState.Transform, Out, Region, false);
}

bool AHCM3NPC::ResolvePhysicalGetUpStand(const FTransform& Requested, FTransform& Out) const
{
    if (!SupportsAuthoredGetUp() || Requested.ContainsNaN()) return false;
    const FVector Feet = Requested.GetLocation() - FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const AHCM3NavRegion* Region = FindUniqueRegionAt(Feet);
    return Region && ResolveStandTransform(Requested, Out, Region);
}

bool AHCM3NPC::BeginPhysicalGetUpMovement()
{
    if (!SupportsAuthoredGetUp() || bDead || !bNPCEnabled || !IsPhysicalReactionActive()) return false;
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    if (!Capsule->BodyInstance.IsValidBodyInstance()) Capsule->RecreatePhysicsState();
    if (!Capsule->BodyInstance.IsValidBodyInstance()) return false;
    StopNavigation();
    if (AHCM3AIController* AI = GetNPCAI())
        if (AI->GetPathFollowingComponent()) AI->GetPathFollowingComponent()->Deactivate();
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->SetComponentTickEnabled(true);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Behaviour = EHCM3NPCBehaviour::Recovering;
    return true;
}

void AHCM3NPC::EndPhysicalReactionState(const FTransform& SafeStand, const FVector& Danger)
{
    if (bDead || !bNPCEnabled) return;
    SetActorEnableCollision(true);
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    // TermBody clears BodyInstance.OwnerComponent when knockdown removes its body.
    // Setting BodyInstance first would change only the enum; the component setter
    // then early-outs on the same enum and never recreates the actual query body.
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    if (!Capsule->BodyInstance.IsValidBodyInstance()) Capsule->RecreatePhysicsState();
    if (!Capsule->BodyInstance.IsValidBodyInstance())
    {
        UE_LOG(LogTemp, Error, TEXT("M4R1_RECOVERY_CAPSULE_BODY_MISSING id=%s"), *StableId.ToString());
        return; // Physical reaction keeps this living NPC down and retries; never resume ghost AI.
    }
    HitStunUntil = 0;
    if (AHCM3NavRegion* R = FindUniqueRegionAt(SafeStand.GetLocation() - FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))) NavigationRegion = R;
    GetCharacterMovement()->SetComponentTickEnabled(true); GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetMesh()->bPauseAnims = false;
    if (AHCM3AIController* AI = GetNPCAI())
    {
        AI->SetActorTickEnabled(true);
        if (AI->GetPathFollowingComponent()) { AI->GetPathFollowingComponent()->Activate(); AI->GetPathFollowingComponent()->SetComponentTickEnabled(true); }
    }
    LastObservedPosition = LastProgressPosition = GetActorLocation(); LastProgressTime = GetWorld()->GetTimeSeconds();
    Behaviour = EHCM3NPCBehaviour::Panic; BeginPanic(Danger, 12.f);
}

bool AHCM3NPC::IsRagdollActive() const
{
    return bDead && !bCorpseRemoved && GetMesh()->IsSimulatingPhysics();
}

void AHCM3NPC::Die(const FHitResult& Hit, const FVector& Direction, bool bApplyDefaultImpulse)
{
    if (bDead) return;
    // Capture before StopNavigation/DisableMovement clear CharacterMovement velocity.
    // An already physical living NPC retains its individual bodies' real momentum.
    const bool bInitializeDeathVelocity = ShouldInitializeDeathRagdollVelocity() && !GetMesh()->IsSimulatingPhysics();
    const FVector DeathInitialVelocity = bInitializeDeathVelocity ? GetVelocity() : FVector::ZeroVector;
    if (Reaction) Reaction->ResetReaction();
    if (PhysicalReaction) PhysicalReaction->OnOwnerDied();
    EndConversation(); StopNavigation();
    const bool WasPassenger = bPassenger;
    AHCM1Vehicle* RidingVehicle = PassengerVehicle.Get();
    bPassenger = false; PassengerVehicle.Reset();
    bDead = true; Health = 0; PanicUntil = 0; HitStunUntil = 0;
    bCorpseRemoved = false; bCorpseFading = false; CorpseOpacity = 1;
    DeathTime = GetWorld()->GetTimeSeconds(); Behaviour = EHCM3NPCBehaviour::Dead;
    if (Experience.IsValid())
    {
        CorpseLifetimeSeconds = FMath::Clamp(Experience->CorpseLifetimeSeconds, 1.f, 600.f);
        CorpseFadeSeconds = FMath::Clamp(Experience->CorpseFadeSeconds, .1f, 5.f);
    }
    if (AHCM3AIController* AI = GetNPCAI())
    {
        DetachedAI = AI;
        if (AI->GetPathFollowingComponent()) AI->GetPathFollowingComponent()->Deactivate();
        AI->UnPossess(); AI->SetActorTickEnabled(false);
    }
    SetCanAffectNavigationGeneration(false, true);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->SetComponentTickEnabled(false);
    // A hidden passenger has actor collision disabled. The component setter
    // checks that owner override and can skip writing an underlying Pawn body.
    // Disable the actual body before re-enabling actor collision for ragdoll.
    GetCapsuleComponent()->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCanBeDamaged(false);
    SetActorEnableCollision(true); SetActorHiddenInGame(false);
    // The logical hidden passenger owns no vehicle control. Its death uses the
    // current vehicle vicinity, without altering possession, input or car physics.
    if (WasPassenger && IsValid(RidingVehicle))
    {
        const FVector Drop = RidingVehicle->GetActorLocation() + RidingVehicle->GetActorRightVector() * 145 + FVector(0, 0, 100);
        SetActorLocation(Drop, false, nullptr, ETeleportType::TeleportPhysics);
    }
    const float ReactionDelay = !WasPassenger && !GetMesh()->IsSimulatingPhysics()
        ? PreparePreRagdollReaction() : 0.f;
    if (ReactionDelay > 0)
    {
        bPendingCorpsePhysics = true; CorpsePhysicsDue = GetWorld()->GetTimeSeconds() + ReactionDelay;
        DeferredCorpseHit = Hit; DeferredCorpseDirection = Direction; DeferredCorpseVelocity = DeathInitialVelocity;
        DeferredCorpseDeltaV = FVector::ZeroVector; bDeferredCorpseDefaultImpulse = bApplyDefaultImpulse;
        UE_LOG(LogTemp, Display, TEXT("M5VS2_DEATH_REACTION_BEGIN id=%s delay=%.3f montage=%s"),
            *StableId.ToString(), ReactionDelay, *GetPathNameSafe(PreRagdollMontage));
    }
    else StartCorpsePhysics(Hit, Direction, DeathInitialVelocity, bInitializeDeathVelocity, bApplyDefaultImpulse);
    if (CorpseMaterials.IsEmpty())
        for (int32 Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index)
            CorpseMaterials.Add(GetMesh()->CreateDynamicMaterialInstance(Index));
    for (UMaterialInstanceDynamic* Material : CorpseMaterials) if (Material) Material->SetScalarParameterValue(TEXT("M4Opacity"), 1.f);
    SetActorTickEnabled(true); SetActorTickInterval(.05f);
    if (Experience.IsValid()) Experience->NotifyNPCDied(this, GetActorLocation());
    UE_LOG(LogTemp, Display, TEXT("M4_NPC_DIED id=%s bone=%s passenger=%d ragdoll=%d lifetime=%.2f"),
        *StableId.ToString(), *LastHitBone.ToString(), WasPassenger, IsRagdollActive(), CorpseLifetimeSeconds);
}

void AHCM3NPC::StartCorpsePhysics(const FHitResult& Hit, const FVector& Direction,
    const FVector& InitialVelocity, bool bInitializeVelocity, bool bApplyDefaultImpulse)
{
    bPendingCorpsePhysics = false;
    if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.1f);
    GetMesh()->bPauseAnims = false;
    GetMesh()->SetComponentTickEnabled(true);
    GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
    GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    if (ShouldBlockPhysicsBodiesDuringRagdoll())
        GetMesh()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->SetAllBodiesPhysicsBlendWeight(1.f);
    if (bInitializeVelocity && GetMesh()->IsSimulatingPhysics())
    {
        if (!InitialVelocity.ContainsNaN())
        {
            // Chaos preserves kinematic V/W on Kinematic->Dynamic. Imported
            // animation/recovery pose deltas are not physical launch velocities.
            GetMesh()->SetAllPhysicsLinearVelocity(InitialVelocity, false);
            GetMesh()->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
            UE_LOG(LogTemp, Display, TEXT("M5VS2_DEATH_INITIAL_VELOCITY id=%s linear=%s angular_reset=1"),
                *StableId.ToString(), *InitialVelocity.ToCompactString());
        }
        else UE_LOG(LogTemp, Error, TEXT("M5VS2_DEATH_INITIAL_VELOCITY_INVALID id=%s"), *StableId.ToString());
    }
    GetMesh()->WakeAllRigidBodies();
    if (GetMesh()->IsSimulatingPhysics()) { if (bApplyDefaultImpulse) GetMesh()->AddImpulse(Direction * 150.f, Hit.BoneName, true); }
    else UE_LOG(LogTemp, Error, TEXT("M4_RAGDOLL_NOT_SIMULATING id=%s physics_asset=%s"), *StableId.ToString(), *GetNameSafe(GetMesh()->GetPhysicsAsset()));
    if (!DeferredCorpseDeltaV.IsNearlyZero())
    {
        for (FBodyInstance* Body : GetMesh()->Bodies)
            if (Body && Body->IsInstanceSimulatingPhysics())
            {
                const float Mass = Body->GetBodyMass();
                if (FMath::IsFinite(Mass) && Mass > 0) Body->AddImpulse(DeferredCorpseDeltaV * Mass, false);
            }
        DeferredCorpseDeltaV = FVector::ZeroVector;
    }
}

float AHCM3NPC::GetSecondsUntilCorpseRemoval() const
{
    if (!IsCorpsePresent() || !GetWorld()) return 0;
    const double Now = GetWorld()->GetTimeSeconds();
    return FMath::Max(0.f, float((bCorpseFading ? FadeStarted : DeathTime + FMath::Clamp(CorpseLifetimeSeconds, 1.f, 600.f)) + FMath::Clamp(CorpseFadeSeconds, .1f, 5.f) - Now));
}

void AHCM3NPC::BeginCorpseRemoval()
{
    if (!IsCorpsePresent() || bCorpseFading) return;
    bPendingCorpsePhysics = false; DeferredCorpseDeltaV = FVector::ZeroVector;
    bCorpseFading = true; FadeStarted = GetWorld()->GetTimeSeconds();
    // Remove simulation immediately when enforcing the cap; visual fade remains bounded.
    GetMesh()->SetSimulatePhysics(false);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->bPauseAnims = true;
}

void AHCM3NPC::UpdateCorpse(double Now)
{
    if (bCorpseRemoved) return;
    if (bPendingCorpsePhysics && !bCorpseFading && Now >= CorpsePhysicsDue)
    {
        const bool bMontageAdvanced = HasAdvancedPreRagdollReaction();
        if (!bMontageAdvanced && Now - PreRagdollStarted < .5) return;
        LastPreRagdollPoseSeconds = GetMesh()->GetAnimInstance() && PreRagdollMontage
            ? GetMesh()->GetAnimInstance()->Montage_GetPosition(PreRagdollMontage) : 0.f;
        UE_LOG(LogTemp, Display, TEXT("M5VS2_DEATH_REACTION_HANDOVER id=%s montage_advanced=%d pose_s=%.3f elapsed_s=%.3f"),
            *StableId.ToString(), bMontageAdvanced, LastPreRagdollPoseSeconds, Now - PreRagdollStarted);
        StartCorpsePhysics(DeferredCorpseHit, DeferredCorpseDirection, DeferredCorpseVelocity,
            ShouldInitializeDeathRagdollVelocity(), bDeferredCorpseDefaultImpulse);
    }
    if (!bCorpseFading && Now - DeathTime >= FMath::Clamp(CorpseLifetimeSeconds, 1.f, 600.f)) BeginCorpseRemoval();
    if (!bCorpseFading) return;
    CorpseOpacity = 1.f - FMath::Clamp(float((Now - FadeStarted) / FMath::Clamp(CorpseFadeSeconds, .1f, 5.f)), 0.f, 1.f);
    for (UMaterialInstanceDynamic* Material : CorpseMaterials) if (Material) Material->SetScalarParameterValue(TEXT("M4Opacity"), CorpseOpacity);
    if (CorpseOpacity <= 0) ClearCorpse();
}

void AHCM3NPC::ClearCorpse()
{
    bPendingCorpsePhysics = false; DeferredCorpseDeltaV = FVector::ZeroVector; PreRagdollMontage = nullptr;
    GetMesh()->SetSimulatePhysics(false);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetComponentTickEnabled(false);
    SetActorEnableCollision(false); SetActorHiddenInGame(true);
    SetActorTickEnabled(false); bCorpseRemoved = true; CorpseOpacity = 0;
    Behaviour = EHCM3NPCBehaviour::RespawnWait;
}

void AHCM3NPC::ResetCombatForRestore()
{
    bPendingCorpsePhysics = false; DeferredCorpseDeltaV = FVector::ZeroVector; PreRagdollMontage = nullptr;
    if (Reaction) Reaction->ResetReaction();
    if (PhysicalReaction) PhysicalReaction->ResetForRestore();
    GetMesh()->SetSimulatePhysics(false);
    GetMesh()->SetAllBodiesPhysicsBlendWeight(0.f);
    GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    GetMesh()->SetRelativeTransform(InitialMeshRelativeTransform);
    GetMesh()->SetCollisionObjectType(ECC_Pawn);
    for (UMaterialInstanceDynamic* Material : CorpseMaterials) if (Material) Material->SetScalarParameterValue(TEXT("M4Opacity"), 1.f);
    CorpseOpacity = 1; bDead = false; bCorpseRemoved = false; bCorpseFading = false;
    Health = FMath::Clamp(MaxHealth, 1.f, 1000.f);
    HitStunUntil = 0; PanicUntil = 0; NextPanicMove = 0; LastHitReactionTime = -1000;
    bReturningToPost = false;
    GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(WalkSpeed, 40.f, 220.f);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    SetCanBeDamaged(true);
    if (!GetController() && DetachedAI.IsValid()) DetachedAI->Possess(this);
    if (!GetController()) SpawnDefaultController();
    if (AHCM3AIController* AI = GetNPCAI())
        if (AI->GetPathFollowingComponent()) AI->GetPathFollowingComponent()->Activate();
}

bool AHCM3NPC::RespawnAtAuthoredPost()
{
    FTransform Safe;
    AHCM3NavRegion* Region = FindRegionById(AuthoredState.RegionId);
    // Reappearing must wait while a real player/car/NPC occupies the post.
    if (!Region || !ResolveStandTransform(AuthoredState.Transform, Safe, Region, false)) return false;
    FHCM3NPCState State = AuthoredState; State.Transform = Safe;
    if (!RestoreState(State)) return false;
    bStationary = bInitialStationary;
    return true;
}

void AHCM3NPC::RestoreNamedDeathCooldown()
{
    if (Reaction) Reaction->ResetReaction();
    if (PhysicalReaction) PhysicalReaction->ResetForRestore();
    EndConversation(); StopNavigation();
    bPassenger = false; PassengerVehicle.Reset(); bDead = true; Health = 0;
    PanicUntil = 0; HitStunUntil = 0;
    if (AHCM3AIController* AI = GetNPCAI())
    {
        DetachedAI = AI;
        if (AI->GetPathFollowingComponent()) AI->GetPathFollowingComponent()->Deactivate();
        AI->UnPossess(); AI->SetActorTickEnabled(false);
    }
    GetCharacterMovement()->DisableMovement(); GetCharacterMovement()->SetComponentTickEnabled(false);
    SetCanAffectNavigationGeneration(false, true); SetCanBeDamaged(false);
    ClearCorpse();
}

bool AHCM3NPC::ResetCombatForTest()
{
#if !UE_BUILD_SHIPPING
    FString TestMode, Slot;
    if (!FParse::Value(FCommandLine::Get(), TEXT("M4Test="), TestMode) || TestMode.IsEmpty() ||
        !FParse::Value(FCommandLine::Get(), TEXT("HCM1SaveSlot="), Slot) ||
        !Slot.StartsWith(TEXT("HarborCity_M2_V1_Test_M4_")) || Slot.Len() > 100) return false;
    for (const TCHAR Letter : Slot) if (!FChar::IsAlnum(Letter) && Letter != '_') return false;
    return RespawnAtAuthoredPost();
#else
    return false;
#endif
}

void AHCM3NPC::BeginPanic(const FVector& DangerLocation, float DurationSeconds)
{
    if (!IsCombatEnabled() || bPassenger || DangerLocation.ContainsNaN() || !FMath::IsFinite(DurationSeconds)) return;
    if (Reaction) Reaction->CancelCounter(TEXT("ThreatRequiresEscape"));
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsPhysicalReactionActive())
    { DangerPoint = DangerLocation; PanicUntil = FMath::Max(PanicUntil, Now + FMath::Clamp(DurationSeconds, 1.f, 60.f)); return; }
    if (PanicUntil <= Now) ++PanicEntries;
    EndConversation();
    if (PanicUntil <= Now) StopNavigation();
    DangerPoint = DangerLocation;
    PanicUntil = FMath::Max(PanicUntil, Now + FMath::Clamp(DurationSeconds, 1.f, 60.f));
    NextPanicMove = Now; Behaviour = EHCM3NPCBehaviour::Panic;
    GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(PanicSpeed, WalkSpeed, 450.f);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetMesh()->bPauseAnims = false;
    SetActorTickEnabled(true); SetActorTickInterval(.05f);
}

bool AHCM3NPC::IsPanicPathAllowed(const FVector& Point) const
{
    // A real doorway route may initially approach the source before going around
    // a counter. Reject unsafe areas in RequestWalk; require escape at the goal,
    // rather than rejecting every locally non-monotonic path segment.
    return !Point.ContainsNaN();
}

void AHCM3NPC::UpdatePanic(double Now)
{
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    if (Now >= PanicUntil)
    {
        PanicUntil = 0; ++PanicRecoveries; StopNavigation();
        GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(WalkSpeed, 40.f, 220.f);
        GetCharacterMovement()->bOrientRotationToMovement = true;
        Behaviour = EHCM3NPCBehaviour::Idle; NextDecisionTime = Now + .5;
        bReturningToPost = bStationary || (NavigationRegion && NavigationRegion->StableId != AuthoredState.RegionId);
        return;
    }
    if (CheckNearbySafety()) return;
    if (Now < NextPanicMove) return;
    NextPanicMove = Now + 1.5;
    if (GetVelocity().SizeSquared2D() > 400 && FVector::Dist2D(LastProgressPosition, GetActorLocation()) > 20)
    { LastProgressPosition = GetActorLocation(); LastProgressTime = Now; return; }
    const FVector Away = (GetActorLocation() - DangerPoint).GetSafeNormal2D();
    const FVector Side(-Away.Y, Away.X, 0);
    const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    TArray<FVector> Goals = {Feet + Away * 650, Feet + (Away + Side).GetSafeNormal() * 550,
        Feet + (Away - Side).GetSafeNormal() * 550, Feet + Side * 500, Feet - Side * 500};
    // Actual cafe occupants can select the sidewalk through the reserved door.
    // Candidate points derive from the existing allowed-region geometry, not a teleport route.
    for (const auto& Ref : Regions)
        if (AHCM3NavRegion* R = Ref.Get())
            if (R->Kind == EHCM3NavRegionKind::Allowed && (R == NavigationRegion ||
                (NavigationRegion && NavigationRegion->StableId == TEXT("M3_Nav_Cafe") && R->StableId == TEXT("M3_Nav_Commercial"))))
                for (float Fraction : {-.75f, 0.f, .75f})
                {
                    FVector G = R->GetActorLocation() + FVector(R->HalfExtent.X * Fraction, 0, 0);
                    G.Z = Feet.Z; Goals.Add(G);
                }
    Goals.Sort([this](const FVector& A, const FVector& B) { return FVector::DistSquared2D(A, DangerPoint) > FVector::DistSquared2D(B, DangerPoint); });
    for (const FVector& Goal : Goals)
    {
        FTransform Safe;
        if (FVector::Dist2D(Goal, DangerPoint) < FVector::Dist2D(Feet, DangerPoint) + 75) continue;
        ++PanicPathAttempts;
        if (ResolveStandTransform(FTransform(GetActorRotation(), Goal + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight())), Safe) &&
            RequestWalk(Safe.GetLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), true))
        { Behaviour = EHCM3NPCBehaviour::Panic; return; }
        ++PanicPathFailures;
    }
    // Bounded safe waiting at a region edge; no teleport or road shortcut.
    Behaviour = EHCM3NPCBehaviour::Panic;
}

FString AHCM3NPC::GetCombatDiagnostics() const
{
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("stable_id"), StableId.ToString());
    Data->SetNumberField(TEXT("health"), Health); Data->SetNumberField(TEXT("max_health"), MaxHealth);
    Data->SetBoolField(TEXT("dead"), bDead); Data->SetBoolField(TEXT("corpse_present"), IsCorpsePresent());
    Data->SetBoolField(TEXT("ragdoll_simulating"), IsRagdollActive());
    Data->SetBoolField(TEXT("pre_death_reaction_pending"), bPendingCorpsePhysics);
    Data->SetNumberField(TEXT("pre_ragdoll_pose_seconds_at_handover"), LastPreRagdollPoseSeconds);
    Data->SetStringField(TEXT("pre_ragdoll_montage"), GetPathNameSafe(PreRagdollMontage));
    Data->SetStringField(TEXT("physics_asset"), GetPathNameSafe(GetMesh()->GetPhysicsAsset()));
    Data->SetNumberField(TEXT("physics_bodies"), GetMesh()->GetPhysicsAsset() ? GetMesh()->GetPhysicsAsset()->SkeletalBodySetups.Num() : 0);
    Data->SetNumberField(TEXT("corpse_lifetime_seconds"), CorpseLifetimeSeconds);
    Data->SetNumberField(TEXT("corpse_remaining_seconds"), GetSecondsUntilCorpseRemoval());
    Data->SetNumberField(TEXT("corpse_opacity_parameter"), CorpseOpacity);
    Data->SetNumberField(TEXT("mesh_collision_enabled"), int32(GetMesh()->GetCollisionEnabled()));
    Data->SetNumberField(TEXT("mesh_pawn_response"), int32(GetMesh()->GetCollisionResponseToChannel(ECC_Pawn)));
    Data->SetNumberField(TEXT("mesh_vehicle_response"), int32(GetMesh()->GetCollisionResponseToChannel(ECC_Vehicle)));
    Data->SetNumberField(TEXT("mesh_visibility_response"), int32(GetMesh()->GetCollisionResponseToChannel(ECC_Visibility)));
    Data->SetNumberField(TEXT("capsule_collision_enabled"), int32(GetCapsuleComponent()->GetCollisionEnabled()));
    Data->SetNumberField(TEXT("capsule_body_collision_enabled_unfiltered"), int32(GetCapsuleComponent()->BodyInstance.GetCollisionEnabled(false)));
    Data->SetBoolField(TEXT("capsule_actual_body_valid"), GetCapsuleComponent()->BodyInstance.IsValidBodyInstance());
    Data->SetBoolField(TEXT("capsule_physics_state_created"), GetCapsuleComponent()->IsPhysicsStateCreated());
    Data->SetBoolField(TEXT("actor_collision_enabled"), GetActorEnableCollision());
    Data->SetBoolField(TEXT("mesh_affects_navigation"), GetMesh()->CanEverAffectNavigation());
    Data->SetBoolField(TEXT("capsule_affects_navigation"), GetCapsuleComponent()->CanEverAffectNavigation());
    Data->SetBoolField(TEXT("controller_present"), GetController() != nullptr);
    Data->SetBoolField(TEXT("controller_tick"), GetController() && GetController()->IsActorTickEnabled());
    Data->SetNumberField(TEXT("panic_entries"), PanicEntries); Data->SetNumberField(TEXT("panic_recoveries"), PanicRecoveries);
    Data->SetNumberField(TEXT("panic_path_attempts"), PanicPathAttempts); Data->SetNumberField(TEXT("panic_path_failures"), PanicPathFailures);
    Data->SetBoolField(TEXT("living_physical_active"), IsPhysicalReactionActive());
    if (Reaction) Data->SetStringField(TEXT("m4_r1_reaction"), Reaction->GetDiagnostics());
    Data->SetNumberField(TEXT("panic_remaining_seconds"), GetWorld() ? FMath::Max(0., PanicUntil - GetWorld()->GetTimeSeconds()) : 0.);
    Data->SetNumberField(TEXT("damage_events"), DamageEvents); Data->SetNumberField(TEXT("reaction_plays"), ReactionPlays);
    Data->SetStringField(TEXT("last_hit_direction"), LastHitDirection.ToString());
    Data->SetStringField(TEXT("last_hit_bone"), LastHitBone.ToString()); Data->SetNumberField(TEXT("last_damage"), LastDamage);
    FString Output; const auto Writer = TJsonWriterFactory<>::Create(&Output); FJsonSerializer::Serialize(Data, Writer); return Output;
}
