#include "HCM5VS2NPCGameplayReviewDirector.h"
#include "HCM5VS2NPC.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Vehicle.h"
#include "M3/HCM3Experience.h"
#include "M4R1/HCM4R1PhysicalReactionComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "Chaos/CollisionFilterData.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
enum EReviewPhase { Warmup, HitHold, AwaitRecovery, RecoveredView, Kill, DeathSettle, Restore, RestoreVerify };
TArray<TSharedPtr<FJsonValue>> Vec(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; }
TSharedPtr<FJsonObject> ParseDiagnostics(const FString& Text)
{
    TSharedPtr<FJsonObject> R;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) || !R) R=MakeShared<FJsonObject>();
    return R;
}
double Number(const TSharedPtr<FJsonObject>& R, const TCHAR* Key)
{ double Value=-1; if (R) R->TryGetNumberField(Key,Value); return Value; }
TArray<TSharedPtr<FJsonValue>> Objects(const TArray<TSharedPtr<FJsonObject>>& Rows)
{ TArray<TSharedPtr<FJsonValue>> R; for (const auto& Row:Rows) R.Add(MakeShared<FJsonValueObject>(Row)); return R; }
bool ReadPNG(const FString& Path,int32& W,int32& H)
{
    TUniquePtr<FArchive> F(IFileManager::Get().CreateFileReader(*Path));
    if (!F || F->TotalSize()<33) return false;
    uint8 B[24]; F->Serialize(B,24); const uint8 Magic[]={137,80,78,71,13,10,26,10};
    if (F->IsError() || FMemory::Memcmp(B,Magic,8) || FMemory::Memcmp(B+12,"IHDR",4)) return false;
    auto Big=[](const uint8* P){return int32(uint32(P[0])<<24|uint32(P[1])<<16|uint32(P[2])<<8|uint32(P[3]));};
    W=Big(B+16); H=Big(B+20); return W>=640 && H>=360;
}
}

AHCM5VS2NPCGameplayReviewDirector::AHCM5VS2NPCGameplayReviewDirector()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}

void AHCM5VS2NPCGameplayReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCGameplayReview"))) return;
    if (GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/M5VS2/NPC/GameplayReview/L_NPCGameplay")) return;
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root) || FPaths::IsRelative(Root)) return;
    Root=FPaths::ConvertRelativePathToFull(Root); FPaths::NormalizeDirectoryName(Root);
    if (!FPaths::CollapseRelativeDirectories(Root) || !FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2"))) return;
    Directory=Root/(TEXT("NPCGameplay_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true)) return;
    StartedWall=LastTickWall=FPlatformTime::Seconds(); StartedGame=GetWorld()->GetTimeSeconds();
    bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    bActive=true; SetActorTickEnabled(true); ListenForStop();
    Write(TEXT("RUNNING"),TEXT("Waiting at most ten unpaused seconds for actual controller/pawn/viewport readiness; no gameplay mutation yet"));
#endif
}

void AHCM5VS2NPCGameplayReviewDirector::ListenForStop()
{
    if (InputHandle.IsValid()) return;
    if (UGameViewportClient* Live=GetWorld()->GetGameViewport())
    { Viewport=Live; InputHandle=Live->OnInputKey().AddUObject(this,&AHCM5VS2NPCGameplayReviewDirector::Input); }
}

bool AHCM5VS2NPCGameplayReviewDirector::Start(FString& Error)
{
    Controller=UGameplayStatics::GetPlayerController(this,0);
    UGameViewportClient* Live=GetWorld()->GetGameViewport();
    if (!Cast<AHCM1PlayerController>(Controller) || !Controller->GetPawn() || !Live || !Live->Viewport)
    { Error=TEXT("Require live rendered M1 controller/pawn and viewport"); return false; }
    ListenForStop();
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2NPCGameplayReviewDirector::Processed);
    if (Specimens.Num()!=2 || !IsValid(Experience) || !IsValid(Vehicle) || Vehicle->HasDriver())
    { Error=TEXT("Require rendered M1 controller/pawn, exact two NPCs, preplaced Experience and unpossessed vehicle"); return false; }
    int32 ExperienceCount=0, VehicleCount=0, NPCCount=0, DirectorCount=0;
    for (TActorIterator<AHCM3Experience> It(GetWorld());It;++It) ++ExperienceCount;
    for (TActorIterator<AHCM1Vehicle> It(GetWorld());It;++It) ++VehicleCount;
    for (TActorIterator<AHCM3NPC> It(GetWorld());It;++It) ++NPCCount;
    for (TActorIterator<AHCM5VS2NPCGameplayReviewDirector> It(GetWorld());It;++It) ++DirectorCount;
    if (ExperienceCount!=1 || VehicleCount!=1 || NPCCount!=2 || DirectorCount!=1 || Experience->NPCs.Num()!=2 ||
        Experience->PlayerAppearanceMesh || Experience->PlayerAnimationClass)
    { Error=TEXT("Isolated map must contain exactly one Experience/vehicle/director and two NPCs, without player appearance override"); return false; }
    for (int32 Index=0;Index<2;++Index)
    {
        AHCM5VS2NPC* NPC=Specimens[Index];
        if (!IsValid(NPC) || NPC->StableId!=FName(Index==0?TEXT("M5VS2_Q"):TEXT("M5VS2_R")) ||
            !Experience->NPCs.Contains(NPC) || !NPC->NavigationRegion || !NPC->bStationary || !NPC->GetPhysicalReaction())
        { Error=TEXT("Q/R identity, Experience membership, stationary and navigation/physical reaction references required"); return false; }
        if (FVector::Dist2D(NPC->GetActorLocation(),Vehicle->GetActorLocation())<700 ||
            FVector::Dist2D(NPC->GetActorLocation(),Controller->GetPawn()->GetActorLocation())<500)
        { Error=TEXT("Vehicle/player obstruct the isolated NPC recovery area"); return false; }
        AddTickPrerequisiteComponent(NPC->GetMesh());
    }
    if (FVector::Dist2D(Specimens[0]->GetActorLocation(),Specimens[1]->GetActorLocation())<1200)
    { Error=TEXT("Q/R require at least 1200 cm separation for bounded independent exercises"); return false; }
    OriginalView=Controller->GetViewTarget();
    FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
    ReviewCamera=GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!ReviewCamera) { Error=TEXT("Review camera creation failed"); return false; }
    ReviewCamera->GetCameraComponent()->SetFieldOfView(48);
    ReviewCamera->GetCameraComponent()->bConstrainAspectRatio=false;
    ReviewCamera->GetCameraComponent()->PostProcessBlendWeight=0;
    Controller->SetViewTarget(ReviewCamera); AimCamera();
    return true;
}

bool AHCM5VS2NPCGameplayReviewDirector::Check(const FString& Name,bool bPass,const FString& Detail)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("check"),Name);
    R->SetStringField(TEXT("status"),bPass?TEXT("PASS"):TEXT("FAIL")); R->SetStringField(TEXT("detail"),Detail);
    R->SetNumberField(TEXT("specimen_index"),SpecimenIndex); R->SetNumberField(TEXT("game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame);
    Checks.Add(R); if (!bPass) Finish(TEXT("FAIL"),Name+TEXT(": ")+Detail); return bPass;
}

void AHCM5VS2NPCGameplayReviewDirector::SetPhase(int32 Next)
{ Phase=Next; PhaseStarted=GetWorld()->GetTimeSeconds(); }

bool AHCM5VS2NPCGameplayReviewDirector::BeginSpecimen()
{
    AHCM5VS2NPC* NPC=Specimens[SpecimenIndex]; FTransform Stand;
    if (!Check(TEXT("settled_unpossessed_vehicle"),IsValid(Vehicle) && !Vehicle->HasDriver() &&
        FMath::IsFinite(Vehicle->GetSpeedKmh()) && Vehicle->GetSpeedKmh()<=1.f,
        TEXT("Vehicle settling allowed during five-second warmup; actual parked speed checked before damage"))) return false;
    if (!Check(TEXT("initial_restore_and_ground_nav"),NPC->CanRestoreState(InitialStates[SpecimenIndex]) &&
        NPC->ResolvePhysicalRecoveryStand(NPC->GetMesh()->GetSocketLocation(NPC->GetPhysicalRootBone()),Stand),
        TEXT("Actual CanRestoreState and ResolvePhysicalRecoveryStand; no fallback teleport"))) return false;
    if (!Check(TEXT("initial_living_state"),NPC->IsVisualProfileReady() && NPC->IsCombatEnabled() && !NPC->IsDead() &&
        !NPC->IsPhysicalReactionActive() && !NPC->GetIsPassenger() && FMath::IsNearlyEqual(NPC->GetHealth(),NPC->MaxHealth,.01f),
        TEXT("Verified current living full-health authored specimen"))) return false;
    Events.Add(Snapshot(NPC,true));
    const auto Before=ParseDiagnostics(NPC->GetCombatDiagnostics());
    FHitResult Hit(NPC,NPC->GetMesh(),NPC->GetMesh()->GetSocketLocation(NPC->GetPhysicalRootBone()),-NPC->GetActorForwardVector());
    Hit.BoneName=NPC->GetPhysicalRootBone(); Hit.bBlockingHit=true;
    const float Health=NPC->GetHealth(); FPointDamageEvent Event(10.f,Hit,NPC->GetActorForwardVector(),UDamageType::StaticClass());
    const float Applied=NPC->TakeDamage(10.f,Event,nullptr,Vehicle);
    const auto After=ParseDiagnostics(NPC->GetCombatDiagnostics()); Events.Add(Snapshot(NPC,true));
    if (!Check(TEXT("point_damage_entry"),FMath::IsNearlyEqual(Applied,10.f,.01f) &&
        FMath::IsNearlyEqual(NPC->GetHealth(),Health-10.f,.01f) && Number(After,TEXT("damage_events"))==Number(Before,TEXT("damage_events"))+1 && !NPC->IsDead(),
        TEXT("Real FPointDamageEvent/TakeDamage, vehicle actor causer; no firearm trace or OS input"))) return false;
    SetPhase(HitHold); return true;
}

TSharedPtr<FJsonObject> AHCM5VS2NPCGameplayReviewDirector::Snapshot(AHCM5VS2NPC* NPC,bool bDetailed) const
{
    auto R=MakeShared<FJsonObject>(); const auto* Mesh=NPC->GetMesh(); const auto* Reaction=NPC->GetPhysicalReaction();
    R->SetNumberField(TEXT("game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame); R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    R->SetNumberField(TEXT("phase"),Phase); R->SetStringField(TEXT("stable_id"),NPC->StableId.ToString());
    R->SetNumberField(TEXT("health"),NPC->GetHealth()); R->SetBoolField(TEXT("dead"),NPC->IsDead());
    R->SetBoolField(TEXT("corpse_present"),NPC->IsCorpsePresent()); R->SetBoolField(TEXT("mesh_simulating"),Mesh->IsSimulatingPhysics());
    R->SetBoolField(TEXT("living_physics_active"),Reaction && Reaction->IsLivingRagdollActive());
    R->SetBoolField(TEXT("recovering"),Reaction && Reaction->IsRecovering()); R->SetNumberField(TEXT("behaviour"),int32(NPC->GetBehaviourState()));
    R->SetArrayField(TEXT("actor_location_cm"),Vec(NPC->GetActorLocation())); R->SetArrayField(TEXT("root_location_cm"),Vec(Reaction->GetPhysicalLocation()));
    R->SetArrayField(TEXT("actor_velocity_cm_s"),Vec(NPC->GetVelocity()));
    const FRotator ActorRotation=NPC->GetActorRotation();
    R->SetArrayField(TEXT("actor_pitch_yaw_roll"),Vec(FVector(ActorRotation.Pitch,ActorRotation.Yaw,ActorRotation.Roll)));
    R->SetBoolField(TEXT("capsule_actual_body_valid"),NPC->GetCapsuleComponent()->BodyInstance.IsValidBodyInstance());
    R->SetBoolField(TEXT("mesh_attached_to_capsule"),Mesh->GetAttachParent()==NPC->GetCapsuleComponent());
    R->SetBoolField(TEXT("controller_present"),NPC->GetController()!=nullptr);
    R->SetNumberField(TEXT("movement_mode"),int32(NPC->GetCharacterMovement()->MovementMode));
    R->SetBoolField(TEXT("movement_tick"),NPC->GetCharacterMovement()->IsComponentTickEnabled());
    float MaxLinear=0,MaxAngular=0; int32 SimBodies=0;
    for (const FBodyInstance* Body:Mesh->Bodies) if (Body && Body->IsInstanceSimulatingPhysics())
    { ++SimBodies; MaxLinear=FMath::Max(MaxLinear,float(Body->GetUnrealWorldVelocity().Size())); MaxAngular=FMath::Max(MaxAngular,float(Body->GetUnrealWorldAngularVelocityInRadians().Size())); }
    R->SetNumberField(TEXT("simulating_bodies"),SimBodies); R->SetNumberField(TEXT("max_body_speed_cm_s"),MaxLinear); R->SetNumberField(TEXT("max_body_angular_rad_s"),MaxAngular);
    const auto PhysicalDiagnostics=ParseDiagnostics(Reaction->GetDiagnostics());
    auto GetUp=MakeShared<FJsonObject>();
    for (const TCHAR* Key:{TEXT("authored_getup_enabled"),TEXT("authored_getup_pose_data_verified"),
        TEXT("getup_stage"),TEXT("getup_result"),TEXT("getup_clip"),TEXT("getup_montage_position_s"),
        TEXT("getup_direction_score"),TEXT("getup_alignment_error_cm"),TEXT("getup_end_hips_error_cm"),
        TEXT("authored_getup_completions"),TEXT("recovery_interruptions"),TEXT("blocked_recovery_checks"),
        TEXT("last_recovery_reason"),TEXT("recovery_reference_body_drift_cm")})
        if (const auto* Value=PhysicalDiagnostics->Values.Find(Key)) GetUp->SetField(Key,*Value);
    R->SetObjectField(TEXT("getup"),GetUp);
    if (!bDetailed) return R;
    R->SetStringField(TEXT("class"),NPC->GetClass()->GetPathName()); R->SetStringField(TEXT("physical_root_bone"),NPC->GetPhysicalRootBone().ToString());
    R->SetBoolField(TEXT("death_velocity_initialization_opt_in"),NPC->ShouldInitializeDeathRagdollVelocity());
    R->SetStringField(TEXT("mesh_relative_transform"),Mesh->GetRelativeTransform().ToString());
    R->SetStringField(TEXT("mesh_world_transform"),Mesh->GetComponentTransform().ToString());
    R->SetObjectField(TEXT("combat"),ParseDiagnostics(NPC->GetCombatDiagnostics()));
    R->SetObjectField(TEXT("physical_reaction"),PhysicalDiagnostics);
    R->SetNumberField(TEXT("component_object_channel"),int32(Mesh->GetCollisionObjectType()));
    R->SetNumberField(TEXT("component_response_to_physics_body"),int32(Mesh->GetCollisionResponseToChannel(ECC_PhysicsBody)));
    TArray<TSharedPtr<FJsonValue>> Filters; int32 SimShapes=0,BlockingShapes=0,ExpectedBodies=0,BlockingBodies=0;
    if (const UPhysicsAsset* Asset=Mesh->GetPhysicsAsset()) for (const USkeletalBodySetup* Setup:Asset->SkeletalBodySetups)
    {
        ++ExpectedBodies; if (!Setup) continue; const FBodyInstance* Body=Mesh->GetBodyInstance(Setup->BoneName);
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("bone"),Setup->BoneName.ToString());
        TArray<TSharedPtr<FJsonValue>> Shapes; bool bRead=false; int32 BodySimShapes=0,BodyBlockingShapes=0;
        if (Body) bRead=FPhysicsCommand::ExecuteRead(Body->GetPhysicsActor(),[&](const FPhysicsActorHandle& Actor)
        {
            // Kinematic bodies retain real V/W in Chaos; excluding them hides the
            // animation-to-ragdoll transition that the lethal before/after events inspect.
            Row->SetBoolField(TEXT("kinematic"),FPhysicsInterface::IsKinematic_AssumesLocked(Actor));
            Row->SetBoolField(TEXT("simulating"),Body->IsInstanceSimulatingPhysics());
            const FVector Linear=FPhysicsInterface::GetLinearVelocity_AssumesLocked(Actor);
            const FVector Angular=FPhysicsInterface::GetAngularVelocity_AssumesLocked(Actor);
            const FTransform WorldPose=FPhysicsInterface::GetTransform_AssumesLocked(Actor,true);
            const bool bFinite=!Linear.ContainsNaN() && !Angular.ContainsNaN() && !WorldPose.ContainsNaN();
            Row->SetBoolField(TEXT("kinematics_finite"),bFinite);
            if (bFinite)
            {
                Row->SetArrayField(TEXT("linear_velocity_cm_s"),Vec(Linear));
                Row->SetArrayField(TEXT("angular_velocity_rad_s"),Vec(Angular));
                Row->SetStringField(TEXT("world_transform"),WorldPose.ToString());
            }
            TArray<FPhysicsShapeHandle> Handles; FPhysicsInterface::GetAllShapes_AssumedLocked(Actor,Handles);
            for (const auto& Shape:Handles)
            {
                if (!Shape.IsValid() || Body->GetOriginalBodyInstance(Shape)!=Body || !FPhysicsInterface::IsSimulationShape(Shape)) continue;
                ++SimShapes; ++BodySimShapes; const auto Combined=FPhysicsInterface::GetCombinedShapeFilterData(Shape);
                auto Entry=MakeShared<FJsonObject>(); const bool bValid=Combined.IsValid() && Combined.GetShapeFilterData().IsSimValid();
                Entry->SetBoolField(TEXT("simulation_filter_valid"),bValid);
                if (bValid)
                {
                    const auto Filter=Combined.GetShapeFilterData(); const uint64 Mask=uint64(Filter.GetBlockChannels());
                    const bool bBlocks=(Mask&(uint64(1)<<uint32(ECC_PhysicsBody)))!=0;
                    Entry->SetNumberField(TEXT("object_channel"),Filter.GetCollisionChannelIndex());
                    Entry->SetStringField(TEXT("block_mask_hex"),FString::Printf(TEXT("0x%016llX"),(unsigned long long)Mask));
                    Entry->SetBoolField(TEXT("blocks_physics_body"),bBlocks);
                    if (bBlocks && Filter.GetCollisionChannelIndex()==uint32(ECC_PhysicsBody)) { ++BlockingShapes; ++BodyBlockingShapes; }
                }
                Shapes.Add(MakeShared<FJsonValueObject>(Entry));
            }
        });
        if (bRead && BodySimShapes>0 && BodyBlockingShapes==BodySimShapes) ++BlockingBodies;
        Row->SetBoolField(TEXT("scene_read_executed"),bRead); Row->SetArrayField(TEXT("simulation_shapes"),Shapes); Filters.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("actual_body_shape_filters"),Filters); R->SetNumberField(TEXT("owned_simulation_shapes"),SimShapes);
    R->SetNumberField(TEXT("owned_simulation_shapes_blocking_physics_body"),BlockingShapes);
    R->SetNumberField(TEXT("expected_physics_bodies"),ExpectedBodies); R->SetNumberField(TEXT("bodies_with_blocking_simulation_shapes"),BlockingBodies);
    R->SetBoolField(TEXT("all_bodies_have_blocking_simulation_shapes"),ExpectedBodies>0 && BlockingBodies==ExpectedBodies);
    R->SetStringField(TEXT("shape_scope"),TEXT("Actual native shape filters; collision pair exclusions and contact manifolds are separate"));
    return R;
}

void AHCM5VS2NPCGameplayReviewDirector::AimCamera()
{
    if (!ReviewCamera || !Specimens.IsValidIndex(SpecimenIndex) || !IsValid(Specimens[SpecimenIndex])) return;
    const auto* NPC=Specimens[SpecimenIndex].Get(); const FVector Focus=NPC->GetPhysicalReaction()->GetPhysicalLocation()+FVector(0,0,20);
    const FVector Offset=NPC->GetActorForwardVector()*440+NPC->GetActorRightVector()*-380+FVector(0,0,260);
    ReviewCamera->SetActorLocation(Focus+Offset); ReviewCamera->SetActorRotation((-Offset).Rotation());
}

void AHCM5VS2NPCGameplayReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (bStopped) return;
    const double Wall=FPlatformTime::Seconds(), WallStep=FMath::Max(0.,Wall-LastTickWall); LastTickWall=Wall;
    if (UGameplayStatics::IsGamePaused(this)) return; // P is owned by the real controller; no test work or auto-exit while paused.
    if (bExitPending) { if (Wall-FinishedWall>2 && !FScreenshotRequest::IsScreenshotRequested()) { bExitPending=false; FPlatformMisc::RequestExit(false,TEXT("M5VS2 NPC gameplay review finished")); } return; }
    if (!bActive) return; UnpausedWall+=WallStep;
    const double Now=GetWorld()->GetTimeSeconds();
    if (UnpausedWall>120 || Now-StartedGame>120) { Finish(TEXT("FAIL"),TEXT("120 second unpaused process bound exceeded")); return; }
    if (!bReady)
    {
        ListenForStop(); Controller=UGameplayStatics::GetPlayerController(this,0);
        const bool bRuntimeReady=Cast<AHCM1PlayerController>(Controller) && Controller->HasActorBegunPlay() &&
            Controller->GetPawn() && Controller->GetPawn()->HasActorBegunPlay() && Viewport.IsValid() && Viewport->Viewport;
        if (!bRuntimeReady)
        {
            if (UnpausedWall>=10) Finish(TEXT("FAIL"),TEXT("Controller/pawn/rendered viewport unavailable after ten unpaused seconds"));
            return;
        }
        FString Error;
        if (!Start(Error)) { Finish(TEXT("FAIL"),Error); return; }
        bReady=true; SetPhase(Warmup); return;
    }
    for (AHCM5VS2NPC* NPC:Specimens) if (!IsValid(NPC) || !NPC->GetMesh() || !NPC->GetPhysicalReaction()) { Finish(TEXT("FAIL"),TEXT("Specimen disappeared")); return; }
    if (Now>=NextSample && Samples.Num()<1201)
    {
        auto Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("game_seconds"),Now-StartedGame);
        TArray<TSharedPtr<FJsonValue>> Actors; for (AHCM5VS2NPC* NPC:Specimens) Actors.Add(MakeShared<FJsonValueObject>(Snapshot(NPC,false)));
        Row->SetArrayField(TEXT("specimens"),Actors); Samples.Add(Row); NextSample=Now+.1; // Actual samples only; never backfill missing frames.
    }
    if (Now>=NextWrite) { Write(TEXT("RUNNING"),TEXT("Real entry-point exercise in progress")); NextWrite=Now+5; }
    AimCamera();
    if (bPending)
    {
        if (bProcessed && !FScreenshotRequest::IsScreenshotRequested() && GFrameCounter>CaptureFrame)
        {
            int32 W=0,H=0; const bool bValid=ReadPNG(PendingPNG,W,H);
            Pending->SetStringField(TEXT("status"),bValid?TEXT("PASS"):TEXT("FAIL")); Pending->SetNumberField(TEXT("width"),W); Pending->SetNumberField(TEXT("height"),H);
            Pending->SetNumberField(TEXT("file_bytes"),IFileManager::Get().FileSize(*PendingPNG));
            Pending->SetObjectField(TEXT("state_when_processed"),Snapshot(Specimens[SpecimenIndex],false));
            Captures.Add(Pending); Pending.Reset(); bPending=false;
            if (!Check(TEXT("native_png"),bValid,TEXT("Actual native PNG header and nonempty file; visual quality USER_REVIEW"))) return;
            if (AfterCapturePhase!=Phase) SetPhase(AfterCapturePhase);
        }
        else if (UnpausedWall-RequestUnpausedSeconds>15) Finish(TEXT("FAIL"),TEXT("Screenshot processing exceeded fifteen unpaused seconds"));
        return;
    }
    if (Phase==Warmup)
    {
        if (Now-PhaseStarted<5) return;
#if WITH_EDITOR
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
        for (AHCM5VS2NPC* NPC:Specimens) { InitialStates.Add(NPC->CaptureState()); InitialMeshRelative.Add(NPC->GetMesh()->GetRelativeTransform()); }
        BeginSpecimen(); return;
    }
    AHCM5VS2NPC* NPC=Specimens[SpecimenIndex]; auto* Reaction=NPC->GetPhysicalReaction(); auto* Mesh=NPC->GetMesh();
    if (NPC->GetActorLocation().ContainsNaN() || Reaction->GetPhysicalLocation().ContainsNaN() ||
        FVector::Dist2D(Reaction->GetPhysicalLocation(),InitialStates[SpecimenIndex].Transform.GetLocation())>650)
    { Finish(TEXT("FAIL"),TEXT("NPC exceeded finite 650 cm authored-area bound")); return; }
    if (Phase==HitHold && Now-PhaseStarted>=.4)
    {
        const auto Before=ParseDiagnostics(Reaction->GetDiagnostics()); RecoveryBefore=Number(Before,TEXT("recovery_events")); EmergencyBefore=Number(Before,TEXT("emergency_recoveries"));
        const FVector Direction=NPC->GetActorForwardVector().GetSafeNormal2D(); FHitResult Hit(NPC,Mesh,Mesh->GetSocketLocation(NPC->GetPhysicalRootBone()),-Direction);
        Hit.BoneName=NPC->GetPhysicalRootBone(); Hit.bBlockingHit=true; const float Health=NPC->GetHealth();
        const float Applied=Reaction->ReceiveVehicleImpact(Vehicle,Hit,Direction*450,Direction,450,SpecimenIndex+1,TEXT("GameplayProbe"));
        HealthAfterImpact=NPC->GetHealth(); ImpactStarted=Now; StableSince=-1; bDownCaptured=false; Events.Add(Snapshot(NPC,true));
        if (!Check(TEXT("impact_entry_knockdown"),Applied>0 && FMath::IsNearlyEqual(HealthAfterImpact,Health-Applied,.01f) &&
            !NPC->IsDead() && Reaction->IsLivingRagdollActive() && Mesh->IsSimulatingPhysics(),
            TEXT("ReceiveVehicleImpact 450 cm/s, source GameplayProbe; direct gameplay entry, NOT actual vehicle contact"))) return;
        SetPhase(AwaitRecovery); return;
    }
    if (Phase==AwaitRecovery)
    {
        const auto D=ParseDiagnostics(Reaction->GetDiagnostics());
        if (Number(D,TEXT("emergency_recoveries"))>EmergencyBefore) { Check(TEXT("ordinary_auto_recovery"),false,TEXT("Emergency recovery occurred; cannot count as ordinary recovery")); return; }
        if (!Reaction->IsLivingRagdollActive())
        {
            Events.Add(Snapshot(NPC,true));
            if (!Check(TEXT("ordinary_auto_recovery"),Number(D,TEXT("recovery_events"))==RecoveryBefore+1 &&
                Number(D,TEXT("emergency_recoveries"))==EmergencyBefore && !Reaction->IsRecovering() && !NPC->IsDead() &&
                FMath::IsNearlyEqual(NPC->GetHealth(),HealthAfterImpact,.01f) && !Mesh->IsSimulatingPhysics() &&
                Mesh->GetAttachParent()==NPC->GetCapsuleComponent() && Mesh->GetRelativeTransform().Equals(InitialMeshRelative[SpecimenIndex],.1f) &&
                NPC->GetCapsuleComponent()->BodyInstance.IsValidBodyInstance() && NPC->GetController() && NPC->GetCharacterMovement()->IsComponentTickEnabled() &&
                NPC->GetCharacterMovement()->MovementMode==MOVE_Walking,
                TEXT("Observed recovery event, no emergency, preserved health, real capsule, mesh reattachment and walking/AI controller"))) return;
            if (!bDownCaptured) { Check(TEXT("settled_down_capture"),false,TEXT("Automatic recovery preceded a stable down capture; no staged/fabricated down image")); return; }
            if (Reaction->bUseAuthoredGetUp && !Check(TEXT("authored_getup_completed"),
                Number(D,TEXT("authored_getup_completions"))>=1 && Number(D,TEXT("getup_montage_position_s"))>=4.5,
                TEXT("Authored branch completed a real five-second root-motion montage; normal recovery above remains required"))) return;
            SetPhase(RecoveredView); return;
        }
        if (Now-ImpactStarted>35) { Check(TEXT("ordinary_auto_recovery"),false,TEXT("No normal recovery within 35 seconds")); return; }
        if (Reaction->bUseAuthoredGetUp && Number(D,TEXT("getup_stage"))==2)
        {
            const double Position=Number(D,TEXT("getup_montage_position_s"));
            for (int32 Half=0; Half<2; ++Half)
            {
                const FString Label=Half==0?TEXT("AuthoredGetUp_FirstHalf"):TEXT("AuthoredGetUp_SecondHalf");
                const bool Recorded=Captures.ContainsByPredicate([&](const TSharedPtr<FJsonObject>& Shot)
                { return Shot->GetStringField(TEXT("label"))==Label &&
                    Shot->GetObjectField(TEXT("state_at_request"))->GetStringField(TEXT("stable_id"))==NPC->StableId.ToString(); });
                if (!Recorded && Position>=(Half==0?1.:3.)) { Capture(Label,AwaitRecovery); return; }
            }
        }
        const auto State=Snapshot(NPC,false);
        const bool bStable=Number(State,TEXT("max_body_speed_cm_s"))<55 && Number(State,TEXT("max_body_angular_rad_s"))<1.5;
        if (!bStable) StableSince=-1; else if (StableSince<0) StableSince=Now;
        // Wait for all bodies to settle, but do not wait past the gameplay component's minimum-down recovery check.
        if (!bDownCaptured && !Reaction->IsRecovering() && Now-ImpactStarted>=.75 && StableSince>=0 && Now-StableSince>=.2)
        {
            const auto Detailed=Snapshot(NPC,true); Events.Add(Detailed);
            if (!Check(TEXT("living_shape_physics_body_filter"),Detailed->GetBoolField(TEXT("all_bodies_have_blocking_simulation_shapes")),
                TEXT("Actual native owned simulation shapes all block PhysicsBody"))) return;
            Capture(TEXT("LivingDown_Stable"),AwaitRecovery); bDownCaptured=bPending;
        }
        return;
    }
    if (Phase==RecoveredView && Now-PhaseStarted>=.25) { Capture(TEXT("AutomaticRecovery"),Kill); return; }
    if (Phase==Kill)
    {
        const auto BeforeLethal=Snapshot(NPC,true); BeforeLethal->SetStringField(TEXT("event"),TEXT("lethal_before_take_damage")); Events.Add(BeforeLethal);
        const FVector Direction=NPC->GetActorForwardVector(); FHitResult Hit(NPC,Mesh,Mesh->GetSocketLocation(NPC->GetPhysicalRootBone()),-Direction);
        Hit.BoneName=NPC->GetPhysicalRootBone(); Hit.bBlockingHit=true; const float Damage=NPC->GetHealth()+1;
        FPointDamageEvent Event(Damage,Hit,Direction,UDamageType::StaticClass()); const float Applied=NPC->TakeDamage(Damage,Event,nullptr,Vehicle);
        const auto AfterLethal=Snapshot(NPC,true); AfterLethal->SetStringField(TEXT("event"),TEXT("lethal_after_take_damage")); Events.Add(AfterLethal);
        if (!Check(TEXT("lethal_damage_entry"),Applied>0 && NPC->IsDead() && NPC->GetHealth()==0 && NPC->IsCorpsePresent() && NPC->IsRagdollActive() && !NPC->GetController(),
            TEXT("Real TakeDamage -> Die; health zero, corpse physics and AI unpossessed"))) return;
        SetPhase(DeathSettle); return;
    }
    if (Phase==DeathSettle && Now-PhaseStarted>=5)
    {
        const auto Detailed=Snapshot(NPC,true); Events.Add(Detailed);
        if (!Check(TEXT("death_shape_physics_body_filter"),NPC->IsDead() && NPC->IsCorpsePresent() && NPC->IsRagdollActive() &&
            Detailed->GetBoolField(TEXT("all_bodies_have_blocking_simulation_shapes")),
            TEXT("Actual corpse retained for five seconds, native simulation shapes block PhysicsBody"))) return;
        Capture(TEXT("Death_After5Seconds"),Restore); return;
    }
    if (Phase==Restore)
    {
        if (!Check(TEXT("restore_state_call"),NPC->CanRestoreState(InitialStates[SpecimenIndex]) && NPC->RestoreState(InitialStates[SpecimenIndex]),
            TEXT("Actual validated RestoreState after death; this is explicit cleanup, not the earlier automatic recovery"))) return;
        SetPhase(RestoreVerify); return;
    }
    if (Phase==RestoreVerify && Now-PhaseStarted>=.25)
    {
        Events.Add(Snapshot(NPC,true));
        if (!Check(TEXT("restored_living_actor"),!NPC->IsDead() && !NPC->IsCorpsePresent() && !NPC->IsPhysicalReactionActive() &&
            FMath::IsNearlyEqual(NPC->GetHealth(),NPC->MaxHealth,.01f) && FVector::Dist(NPC->GetActorLocation(),InitialStates[SpecimenIndex].Transform.GetLocation())<5 &&
            !Mesh->IsSimulatingPhysics() && Mesh->GetAttachParent()==NPC->GetCapsuleComponent() &&
            Mesh->GetRelativeTransform().Equals(InitialMeshRelative[SpecimenIndex],.1f) && NPC->GetCapsuleComponent()->BodyInstance.IsValidBodyInstance() && NPC->GetController(),
            TEXT("Restored health/location/mesh attachment/capsule and controller; no save/load or timed respawn assertion"))) return;
        if (++SpecimenIndex<2) BeginSpecimen();
        else Finish(TEXT("PASS"),TEXT("Q/R native point damage, living impact, ordinary automatic recovery, lethal damage and explicit RestoreState completed; appearance USER_REVIEW"));
    }
}

void AHCM5VS2NPCGameplayReviewDirector::Capture(const FString& Label,int32 NextPhase)
{
    if (bStopped || bPending || FScreenshotRequest::IsScreenshotRequested()) return;
    if (Captures.Num()>=10) { Finish(TEXT("FAIL"),TEXT("Ten-image bound exceeded")); return; }
    PendingPNG=Directory/(FString::Printf(TEXT("%02d_%s_%s.png"),Captures.Num(),*Specimens[SpecimenIndex]->StableId.ToString(),*Label));
    if (IFileManager::Get().FileExists(*PendingPNG)) { Finish(TEXT("FAIL"),TEXT("Refuse screenshot overwrite")); return; }
    Pending=MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("label"),Label); Pending->SetStringField(TEXT("file"),PendingPNG);
    Pending->SetNumberField(TEXT("request_frame"),double(GFrameCounter)); Pending->SetObjectField(TEXT("state_at_request"),Snapshot(Specimens[SpecimenIndex],true));
    if (Controller && Controller->PlayerCameraManager)
    {
        const APlayerCameraManager* Camera=Controller->PlayerCameraManager.Get();
        Pending->SetArrayField(TEXT("actual_camera_location_cm"),Vec(Camera->GetCameraLocation()));
        const FRotator Rot=Camera->GetCameraRotation(); Pending->SetArrayField(TEXT("actual_camera_pitch_yaw_roll"),Vec(FVector(Rot.Pitch,Rot.Yaw,Rot.Roll)));
        Pending->SetNumberField(TEXT("actual_camera_fov"),Camera->GetFOVAngle());
    }
    Pending->SetStringField(TEXT("alignment_scope"),TEXT("State at screenshot request and processed callback; not a claimed exact GPU/skinned-pose timestamp"));
    bPending=true; bProcessed=false; CaptureFrame=GFrameCounter; RequestUnpausedSeconds=UnpausedWall; AfterCapturePhase=NextPhase;
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}

void AHCM5VS2NPCGameplayReviewDirector::Processed() { if (!bStopped && bPending) bProcessed=true; }

void AHCM5VS2NPCGameplayReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if (Event.Event!=IE_Pressed || bStopped || (!bActive && !bExitPending)) return;
    if (Event.Key==EKeys::P) { ++PauseKeyPresses; return; } // Observe only. The controller handles P.
    if (Event.Key!=EKeys::Escape) return;
    bStopped=true; bActive=false; bExitPending=false; bAutoQuit=false; StopFrame=GFrameCounter;
    if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    bPending=false; SetActorTickEnabled(false);
    Write(TEXT("USER_ABORTED"),TEXT("Escape permanently latched. No subsequent test damage/camera/capture/restore/exit. Event not consumed; ordinary game remains under user control."));
}

void AHCM5VS2NPCGameplayReviewDirector::Finish(const FString& Status,const FString& Detail)
{
    if (bStopped) return; bActive=false;
    if (bReady) for (AHCM5VS2NPC* NPC:Specimens)
        if (IsValid(NPC) && NPC->GetMesh() && NPC->GetPhysicalReaction())
        {
            auto Actual=Snapshot(NPC,true); Actual->SetStringField(TEXT("event"),TEXT("final_runtime_observation")); Events.Add(Actual);
        }
    if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    bPending=false; // Never force a physical recovery on failure or hide it by restoring actors.
    if (Controller && OriginalView.IsValid()) Controller->SetViewTarget(OriginalView.Get());
    FinishedWall=FPlatformTime::Seconds(); bExitPending=bAutoQuit; SetActorTickEnabled(bExitPending);
    Write(Status,Detail); UE_LOG(LogTemp,Display,TEXT("M5VS2_NPC_GAMEPLAY_%s %s"),*Status,*Directory);
}

void AHCM5VS2NPCGameplayReviewDirector::Write(const FString& Status,const FString& Detail)
{
    if (Directory.IsEmpty()) return;
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),Status); R->SetStringField(TEXT("detail"),Detail);
    R->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName()); R->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));
    R->SetBoolField(TEXT("actual_vehicle_contact_tested"),false); R->SetBoolField(TEXT("os_input_used"),false); R->SetBoolField(TEXT("save_load_tested"),false);
    R->SetStringField(TEXT("recovery_scope"),TEXT("Actual per-specimen physical diagnostics report whether legacy blend or authored root-motion get-up ran; montage progress and completion are measured, not inferred from configuration"));
    R->SetStringField(TEXT("sampling_scope"),TEXT("At most 10 Hz actual post-update samples, no synthetic backfill; screenshot stalls remain visible in timestamps"));
    R->SetNumberField(TEXT("maximum_unpaused_seconds"),120); R->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-StartedWall);
    R->SetNumberField(TEXT("elapsed_unpaused_wall_seconds"),UnpausedWall); R->SetNumberField(TEXT("elapsed_game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame);
    R->SetBoolField(TEXT("user_stop_latched"),bStopped); R->SetNumberField(TEXT("stop_frame"),double(StopFrame));
    R->SetNumberField(TEXT("observed_pause_key_presses"),PauseKeyPresses); R->SetBoolField(TEXT("explicit_autoquit"),bAutoQuit);
    R->SetArrayField(TEXT("checks"),Objects(Checks)); R->SetArrayField(TEXT("events"),Objects(Events));
    R->SetArrayField(TEXT("process_samples"),Objects(Samples)); R->SetArrayField(TEXT("captures"),Objects(Captures));
    FString Text; FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));
    if (!FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("npc_gameplay_review.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        UE_LOG(LogTemp,Error,TEXT("M5VS2_NPC_GAMEPLAY_EVIDENCE_WRITE_FAILED"));
}

void AHCM5VS2NPCGameplayReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bActive && !bStopped) Write(TEXT("NOT_RUN"),TEXT("World ended before the bounded gameplay sequence completed"));
    if (Viewport.IsValid() && InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle);
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    // No actor restoration here, particularly after Escape.
    Super::EndPlay(Reason);
}
