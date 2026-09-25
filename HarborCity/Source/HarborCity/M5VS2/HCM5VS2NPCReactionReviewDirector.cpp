#include "HCM5VS2NPCReactionReviewDirector.h"
#include "HCM5VS2NPCClothDiagnostics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "HCM5VS2NPC.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
#include "Internationalization/Regex.h"
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
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
enum EReviewPhase { Warmup, HitHold, PreImpact, AwaitRecovery, RecoveredView, Kill, PreDeath, DeathSettle, Restore, RestoreVerify };
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

// Ray visibility is an acquisition guard, not a contact/penetration verdict.
struct FReactionCameraVisibility
{
    TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
    int32 VisibleAndFramed = 0;
    bool bCameraClear = false;
    bool bAllTargetsVisible = false;
};
FReactionCameraVisibility InspectReactionCamera(UWorld* World, AHCM5VS2NPC* NPC,
    const FVector& Origin, const FRotator& Rotation, float HorizontalFOV, double Aspect)
{
    FReactionCameraVisibility Result;
    auto& Evidence=Result.Evidence;
    Evidence->SetArrayField(TEXT("camera_location_cm"),Vec(Origin));
    Evidence->SetNumberField(TEXT("horizontal_fov"),HorizontalFOV);
    Evidence->SetNumberField(TEXT("viewport_aspect"),Aspect);
    Evidence->SetStringField(TEXT("scope"),TEXT("ECC_Visibility complex rays to six real humanoid bone points; camera sphere radius 4cm. Not GPU occlusion, skin penetration, contact, or art acceptance."));
    if (!World || !NPC || !NPC->NPCProfile || !NPC->GetMesh() || Origin.ContainsNaN() || Aspect<=0)
    {
        Evidence->SetStringField(TEXT("status"),TEXT("FAIL_MISSING_CAMERA_OR_PROFILE"));
        return Result;
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M5VS2NPCReviewVisibility),true,NPC);
    Result.bCameraClear=!World->OverlapBlockingTestByChannel(Origin,FQuat::Identity,
        ECC_Visibility,FCollisionShape::MakeSphere(4.f),Query);
    Evidence->SetBoolField(TEXT("camera_sphere_clear"),Result.bCameraClear);
    const FRotationMatrix Basis(Rotation);
    const FVector Forward=Basis.GetUnitAxis(EAxis::X);
    const FVector Right=Basis.GetUnitAxis(EAxis::Y);
    const FVector Up=Basis.GetUnitAxis(EAxis::Z);
    const double TanHorizontal=FMath::Tan(FMath::DegreesToRadians(double(HorizontalFOV)*.5))*.95;
    const double TanVertical=TanHorizontal/Aspect;
    TArray<TSharedPtr<FJsonValue>> Rays;
    for (const TCHAR* Role:{TEXT("hips"),TEXT("head"),TEXT("leftHand"),
        TEXT("rightHand"),TEXT("leftFoot"),TEXT("rightFoot")})
    {
        auto Ray=MakeShared<FJsonObject>();
        Ray->SetStringField(TEXT("role"),Role);
        const FName* Bone=NPC->NPCProfile->HumanoidBones.Find(FName(Role));
        const bool bFound=Bone && NPC->GetMesh()->GetBoneIndex(*Bone)!=INDEX_NONE;
        Ray->SetBoolField(TEXT("bone_found"),bFound);
        if (bFound)
        {
            const FVector Point=NPC->GetMesh()->GetSocketLocation(*Bone);
            const FVector Delta=Point-Origin;
            const double Depth=FVector::DotProduct(Delta,Forward);
            const bool bFinite=!Point.ContainsNaN();
            const bool bFramed=bFinite && Depth>1 &&
                FMath::Abs(FVector::DotProduct(Delta,Right))<=Depth*TanHorizontal &&
                FMath::Abs(FVector::DotProduct(Delta,Up))<=Depth*TanVertical;
            FHitResult Hit;
            const bool bBlocked=bFinite && World->LineTraceSingleByChannel(Hit,Origin,Point,ECC_Visibility,Query);
            Ray->SetStringField(TEXT("bone"),Bone->ToString());
            Ray->SetArrayField(TEXT("target_world_cm"),Vec(Point));
            Ray->SetBoolField(TEXT("finite"),bFinite);
            Ray->SetBoolField(TEXT("inside_95_percent_frame"),bFramed);
            Ray->SetBoolField(TEXT("blocked"),bBlocked);
            if (bBlocked)
            {
                Ray->SetStringField(TEXT("hit_actor"),GetPathNameSafe(Hit.GetActor()));
                Ray->SetStringField(TEXT("hit_component"),GetPathNameSafe(Hit.GetComponent()));
                Ray->SetArrayField(TEXT("hit_point_cm"),Vec(Hit.ImpactPoint));
                Ray->SetBoolField(TEXT("start_penetrating"),Hit.bStartPenetrating);
            }
            if (bFinite && bFramed && !bBlocked) ++Result.VisibleAndFramed;
        }
        Rays.Add(MakeShared<FJsonValueObject>(Ray));
    }
    Result.bAllTargetsVisible=Result.bCameraClear && Result.VisibleAndFramed==6;
    Evidence->SetNumberField(TEXT("visible_and_framed_target_count"),Result.VisibleAndFramed);
    Evidence->SetNumberField(TEXT("required_target_count"),6);
    Evidence->SetArrayField(TEXT("target_rays"),Rays);
    Evidence->SetStringField(TEXT("status"),Result.bAllTargetsVisible?
        TEXT("PASS_RAY_TARGETS_ONLY"):TEXT("FAIL_PARTIAL_OR_OCCLUDED_ACQUISITION"));
    return Result;
}

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

AHCM5VS2NPCReactionReviewDirector::AHCM5VS2NPCReactionReviewDirector()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}

void AHCM5VS2NPCReactionReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCReactionReview"))) return;
    const TArray<FString> Sites={TEXT("Open"),TEXT("Wall"),TEXT("Slope"),TEXT("Narrow")};
    if (!Sites.Contains(ProbeSite)) return;
    if(ReviewPair.IsEmpty())
    {
        if(GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/L_NPCReaction_")+ProbeSite)return;
    }
    else
    {
        FRegexMatcher Matcher(FRegexPattern(TEXT("^/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/IdlePairs/Attempt_[0-9a-f]{12}/L_NPCReaction_(Open|Wall|Slope|Narrow)_(QR|JT|UV|WX)$")),GetWorld()->GetOutermost()->GetName());
        FString RequestedPair;
        if(!Matcher.FindNext()||Matcher.GetCaptureGroup(1)!=ProbeSite||Matcher.GetCaptureGroup(2)!=ReviewPair
            ||!FParse::Value(FCommandLine::Get(),TEXT("M5VS2NPCReactionIdlePair="),RequestedPair)||RequestedPair!=ReviewPair)return;
    }
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5NPCFallDirection="),FallDirection) ||
        !TArray<FString>{TEXT("Forward"),TEXT("Backward"),TEXT("Left"),TEXT("Right")}.Contains(FallDirection)) return;
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),Root) || FPaths::IsRelative(Root)) return;
    Root=FPaths::ConvertRelativePathToFull(Root); FPaths::NormalizeDirectoryName(Root);
    if (!FPaths::CollapseRelativeDirectories(Root) || !FPaths::IsUnderDirectory(Root,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2"))) return;
    Directory=Root/(TEXT("NPCReactionR2_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true)) return;
    StartedWall=LastTickWall=FPlatformTime::Seconds(); StartedGame=GetWorld()->GetTimeSeconds();
    bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    bActive=true; SetActorTickEnabled(true); ListenForStop();
    Write(TEXT("RUNNING"),TEXT("Waiting at most ten unpaused seconds for actual controller/pawn/viewport readiness; no gameplay mutation yet"));
#endif
}

void AHCM5VS2NPCReactionReviewDirector::ListenForStop()
{
    if (InputHandle.IsValid()) return;
    if (UGameViewportClient* Live=GetWorld()->GetGameViewport())
    { Viewport=Live; InputHandle=Live->OnInputKey().AddUObject(this,&AHCM5VS2NPCReactionReviewDirector::Input); }
}

bool AHCM5VS2NPCReactionReviewDirector::Start(FString& Error)
{
    Controller=UGameplayStatics::GetPlayerController(this,0);
    UGameViewportClient* Live=GetWorld()->GetGameViewport();
    if (!Cast<AHCM1PlayerController>(Controller) || !Controller->GetPawn() || !Live || !Live->Viewport)
    { Error=TEXT("Require live rendered M1 controller/pawn and viewport"); return false; }
    ListenForStop();
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2NPCReactionReviewDirector::Processed);
    if (Specimens.Num()!=2 || !IsValid(Experience) || !IsValid(Vehicle) || Vehicle->HasDriver())
    { Error=TEXT("Require rendered M1 controller/pawn, exact two NPCs, preplaced Experience and unpossessed vehicle"); return false; }
    int32 ExperienceCount=0, VehicleCount=0, NPCCount=0, DirectorCount=0;
    for (TActorIterator<AHCM3Experience> It(GetWorld());It;++It) ++ExperienceCount;
    for (TActorIterator<AHCM1Vehicle> It(GetWorld());It;++It) ++VehicleCount;
    for (TActorIterator<AHCM3NPC> It(GetWorld());It;++It) ++NPCCount;
    for (TActorIterator<AHCM5VS2NPCReactionReviewDirector> It(GetWorld());It;++It) ++DirectorCount;
    if (ExperienceCount!=1 || VehicleCount!=1 || NPCCount!=2 || DirectorCount!=1 || Experience->NPCs.Num()!=2 ||
        Experience->PlayerAppearanceMesh || Experience->PlayerAnimationClass)
    { Error=TEXT("Isolated map must contain exactly one Experience/vehicle/director and two NPCs, without player appearance override"); return false; }
    if (bRequireRampSupport)
    {
        if (ProbeSite!=TEXT("Slope") || ReviewPair.IsEmpty() || RampSupportActors.Num()!=2)
        { Error=TEXT("Ramp support gate requires a new private slope pair and exactly two ordered ramp references"); return false; }
        for (int32 Index=0;Index<2;++Index)
        {
            const AStaticMeshActor* Ramp=Cast<AStaticMeshActor>(RampSupportActors[Index]);
            const UStaticMeshComponent* Surface=Ramp?Ramp->GetStaticMeshComponent():nullptr;
            const FVector Center(0,Index==0?-750.:750.,60);
            if (!Surface || !Surface->GetStaticMesh() || RampSupportActors[0]==RampSupportActors[1]
                || Surface->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")
                || !Ramp->GetActorLocation().Equals(Center,.001)
                || !Ramp->GetActorScale3D().Equals(FVector(10,10,.4),.001)
                || !Ramp->GetActorRotation().Equals(FRotator(8,0,0),.001)
                || Surface->GetCollisionProfileName()!=FName(TEXT("BlockAll")))
            { Error=TEXT("Private support ramp must retain exact native cube, 8-degree pitch and 500cm half-width"); return false; }
        }
    }
    for (int32 Index=0;Index<2;++Index)
    {
        AHCM5VS2NPC* NPC=Specimens[Index];
        const FString Letter=(ReviewPair.IsEmpty()?FString(TEXT("QR")):ReviewPair).Mid(Index,1);
        if (!IsValid(NPC) || NPC->StableId!=FName(*(TEXT("M5VS2_")+Letter)) ||
            !Experience->NPCs.Contains(NPC) || !NPC->NavigationRegion || !NPC->bStationary || !NPC->GetPhysicalReaction())
        { Error=TEXT("Exact pair identity, Experience membership, stationary and navigation/physical reaction references required"); return false; }
        if(!ReviewPair.IsEmpty())
        {
            const FString Package=NPC->GetClass()->GetOutermost()->GetName();
            FRegexMatcher ClassMatch(FRegexPattern(TEXT("^/Game/HarborCity/M5VS2/NPC/AvatarSample_")+Letter+TEXT("/IdleR2/Batch_[0-9a-f]{12}/BP_NPCIdleR2_")+Letter+TEXT("$")),Package);
            const bool ClothReview=Letter==TEXT("J")&&FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCJChaosClothReview"));
            FRegexMatcher ClothMatch(FRegexPattern(TEXT("^/Game/HarborCity/M5VS2/NPC/AvatarSample_J/GarmentR2/Batch_[0-9a-f]{12}/BP_NPCJ_Clothing$")),Package);
            if(!(ClothReview?ClothMatch.FindNext():ClassMatch.FindNext())){Error=TEXT("Exact private IdleR2 or opted-in J clothing candidate required");return false;}
        }
        if (FVector::Dist2D(NPC->GetActorLocation(),Vehicle->GetActorLocation())<700 ||
            FVector::Dist2D(NPC->GetActorLocation(),Controller->GetPawn()->GetActorLocation())<500)
        { Error=TEXT("Vehicle/player obstruct the isolated NPC recovery area"); return false; }
        AddTickPrerequisiteComponent(NPC->GetMesh());
        if(NPC->StableId==TEXT("M5VS2_J")&&FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCJChaosClothReview")))
        {
            if(!NPC->GetMesh()->bWaitForParallelClothTask){Error=TEXT("J cloth fixture must explicitly wait for its parallel cloth task");return false;}
            PrimaryActorTick.AddPrerequisite(NPC->GetMesh(),NPC->GetMesh()->ClothTickFunction);
        }
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

bool AHCM5VS2NPCReactionReviewDirector::Check(const FString& Name,bool bPass,const FString& Detail)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("check"),Name);
    R->SetStringField(TEXT("status"),bPass?TEXT("PASS"):TEXT("FAIL")); R->SetStringField(TEXT("detail"),Detail);
    R->SetNumberField(TEXT("specimen_index"),SpecimenIndex); R->SetNumberField(TEXT("game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame);
    Checks.Add(R); if (!bPass) Finish(TEXT("FAIL"),Name+TEXT(": ")+Detail); return bPass;
}

void AHCM5VS2NPCReactionReviewDirector::SetPhase(int32 Next)
{ Phase=Next; PhaseStarted=GetWorld()->GetTimeSeconds(); }

bool AHCM5VS2NPCReactionReviewDirector::BeginSpecimen()
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

TSharedPtr<FJsonObject> AHCM5VS2NPCReactionReviewDirector::Snapshot(AHCM5VS2NPC* NPC,bool bDetailed) const
{
    auto R=MakeShared<FJsonObject>(); const auto* Mesh=NPC->GetMesh(); const auto* Reaction=NPC->GetPhysicalReaction();
    R->SetNumberField(TEXT("game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame); R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    R->SetNumberField(TEXT("phase"),Phase); R->SetStringField(TEXT("stable_id"),NPC->StableId.ToString());
    R->SetNumberField(TEXT("health"),NPC->GetHealth()); R->SetBoolField(TEXT("dead"),NPC->IsDead());
    R->SetBoolField(TEXT("corpse_present"),NPC->IsCorpsePresent()); R->SetBoolField(TEXT("mesh_simulating"),Mesh->IsSimulatingPhysics());
    R->SetBoolField(TEXT("living_reaction_active_including_pending"),Reaction && Reaction->IsLivingRagdollActive());
    R->SetNumberField(TEXT("pre_hit_maximum_bone_delta_degrees"),MaximumHitPoseDeltaDegrees);
    R->SetBoolField(TEXT("pre_hit_bone_motion_observed_before_simulation"),bObservedAdvancingPose);
    R->SetStringField(TEXT("requested_impact_direction"),FallDirection);
    R->SetStringField(TEXT("site"),ProbeSite);
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

void AHCM5VS2NPCReactionReviewDirector::SampleGetUpSupport(AHCM5VS2NPC* NPC, const TSharedPtr<FJsonObject>& State)
{
    // Measurement only, within the existing <=10 Hz unpaused sampling branch.
    // Bone points are not a palm/sole surface, contact force, or contact manifold.
    if (GetUpSupportSamples.Num()>=40 || !State || !NPC || !NPC->NPCProfile) return;
    const TSharedPtr<FJsonObject>* GetUp=nullptr;
    if (!State->TryGetObjectField(TEXT("getup"),GetUp) || !GetUp || !GetUp->IsValid()) return;
    const double Position=Number(*GetUp,TEXT("getup_montage_position_s"));
    if (Number(*GetUp,TEXT("getup_stage"))!=2 || Position<0 || Position>1.5) return;
    auto* Mesh=NPC->GetMesh(); if (!Mesh) return;
    auto Row=MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("stable_id"),NPC->StableId.ToString());
    Row->SetNumberField(TEXT("frame"),double(GFrameCounter));
    Row->SetNumberField(TEXT("game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame);
    Row->SetObjectField(TEXT("getup"),*GetUp);
    Row->SetStringField(TEXT("actor_transform"),NPC->GetActorTransform().ToString());
    Row->SetStringField(TEXT("mesh_world_transform"),Mesh->GetComponentTransform().ToString());
    Row->SetBoolField(TEXT("mesh_simulating"),Mesh->IsSimulatingPhysics());
    Row->SetBoolField(TEXT("postprocess_disabled"),Mesh->GetDisablePostProcessBlueprint());
    Row->SetStringField(TEXT("postprocess_class"),Mesh->GetPostProcessInstance()?Mesh->GetPostProcessInstance()->GetClass()->GetPathName():TEXT("None"));
    if (Mesh->GetNumBones()>0)
    {
        const FName RootBone=Mesh->GetBoneName(0);
        Row->SetStringField(TEXT("root_bone"),RootBone.ToString());
        Row->SetStringField(TEXT("root_bone_component_transform"),Mesh->GetSocketTransform(RootBone,RTS_Component).ToString());
    }
    if (const UAnimInstance* Anim=Mesh->GetAnimInstance())
    {
        Row->SetNumberField(TEXT("root_motion_mode"),int32(Anim->RootMotionMode.GetValue()));
        const UAnimMontage* Montage=Anim->GetCurrentActiveMontage();
        Row->SetStringField(TEXT("active_montage"),Montage?Montage->GetPathName():TEXT("None"));
        if (Montage)
        {
            Row->SetNumberField(TEXT("active_montage_position_s"),Anim->Montage_GetPosition(Montage));
            if (const FAnimMontageInstance* Instance=Anim->GetInstanceForMontage(Montage))
            { Row->SetNumberField(TEXT("active_montage_weight"),Instance->GetWeight()); Row->SetNumberField(TEXT("active_montage_play_rate"),Instance->GetPlayRate()); }
        }
    }
    FCollisionObjectQueryParams ObjectsToTrace; ObjectsToTrace.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M5VS2GetUpSupport),false,NPC);
    TArray<TSharedPtr<FJsonValue>> Points;
    for (const TCHAR* HumanoidRole:{TEXT("hips"),TEXT("leftHand"),TEXT("rightHand"),
        TEXT("leftMiddleProximal"),TEXT("rightMiddleProximal"),TEXT("leftMiddleDistal"),TEXT("rightMiddleDistal"),
        TEXT("leftFoot"),TEXT("rightFoot"),TEXT("leftToes"),TEXT("rightToes"),TEXT("leftLowerLeg"),TEXT("rightLowerLeg")})
    {
        auto Point=MakeShared<FJsonObject>(); Point->SetStringField(TEXT("role"),HumanoidRole);
        const FName* Bone=NPC->NPCProfile->HumanoidBones.Find(FName(HumanoidRole));
        const bool bFound=Bone && Mesh->GetBoneIndex(*Bone)!=INDEX_NONE;
        Point->SetBoolField(TEXT("bone_found"),bFound);
        if (bFound)
        {
            const FTransform WorldBone=Mesh->GetSocketTransform(*Bone,RTS_World);
            Point->SetStringField(TEXT("bone"),Bone->ToString());
            Point->SetBoolField(TEXT("finite"),!WorldBone.ContainsNaN());
            if (!WorldBone.ContainsNaN())
            {
                const FVector PositionWorld=WorldBone.GetLocation();
                Point->SetArrayField(TEXT("world_cm"),Vec(PositionWorld));
                Point->SetStringField(TEXT("world_transform"),WorldBone.ToString());
                FHitResult Hit;
                const bool bHit=GetWorld()->LineTraceSingleByObjectType(Hit,PositionWorld+FVector(0,0,20),PositionWorld-FVector(0,0,250),ObjectsToTrace,Query);
                Point->SetBoolField(TEXT("static_trace_hit"),bHit);
                if (bHit)
                {
                    Point->SetStringField(TEXT("hit_actor"),GetPathNameSafe(Hit.GetActor()));
                    Point->SetStringField(TEXT("hit_component"),GetPathNameSafe(Hit.GetComponent()));
                    Point->SetArrayField(TEXT("hit_point_cm"),Vec(Hit.ImpactPoint));
                    Point->SetArrayField(TEXT("hit_normal"),Vec(Hit.ImpactNormal));
                    Point->SetBoolField(TEXT("start_penetrating"),Hit.bStartPenetrating);
                    Point->SetNumberField(TEXT("bone_minus_hit_z_cm"),PositionWorld.Z-Hit.ImpactPoint.Z);
                }
            }
        }
        Points.Add(MakeShared<FJsonValueObject>(Point));
    }
    Row->SetArrayField(TEXT("bone_points"),Points); GetUpSupportSamples.Add(Row);
}

bool AHCM5VS2NPCReactionReviewDirector::CheckRampSupport(AHCM5VS2NPC* NPC,const FString& Label)
{
    if (!bRequireRampSupport) return true;
    if (RampSupportSamples.Num()>=8 || !RampSupportActors.IsValidIndex(SpecimenIndex)
        || !IsValid(RampSupportActors[SpecimenIndex]) || !NPC || !NPC->NPCProfile)
        return Check(TEXT("ramp_support_source"),false,TEXT("Missing bounded private terrain references or actual humanoid mapping"));
    AActor* Expected=RampSupportActors[SpecimenIndex];
    auto Row=MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("stable_id"),NPC->StableId.ToString());
    Row->SetStringField(TEXT("phase_label"),Label);
    Row->SetNumberField(TEXT("frame"),double(GFrameCounter));
    Row->SetNumberField(TEXT("game_seconds"),GetWorld()->GetTimeSeconds());
    Row->SetStringField(TEXT("expected_ramp"),Expected->GetPathName());
    Row->SetStringField(TEXT("expected_ramp_transform"),Expected->GetActorTransform().ToString());
    const FVector ExpectedNormal=Expected->GetActorUpVector();
    Row->SetArrayField(TEXT("expected_normal"),Vec(ExpectedNormal));
    FCollisionObjectQueryParams ObjectsToTrace(ECC_WorldStatic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2NPCReviewRamp),false,NPC);
    TArray<TSharedPtr<FJsonValue>> Points;
    bool bAll=true;
    for (const TCHAR* SupportRole:{TEXT("hips"),TEXT("chest"),TEXT("leftFoot"),TEXT("rightFoot")})
    {
        auto Point=MakeShared<FJsonObject>();
        Point->SetStringField(TEXT("role"),SupportRole);
        const FName* Bone=NPC->NPCProfile->HumanoidBones.Find(FName(SupportRole));
        bool bPass=false;
        if (Bone && NPC->GetMesh()->GetBoneIndex(*Bone)!=INDEX_NONE)
        {
            const FVector Location=FCString::Strcmp(SupportRole,TEXT("hips"))==0
                ?NPC->GetPhysicalReaction()->GetPhysicalLocation():NPC->GetMesh()->GetSocketLocation(*Bone);
            const FVector Start=Location+FVector(0,0,20),End=Location-FVector(0,0,250);
            FHitResult Hit;
            const bool bHit=GetWorld()->LineTraceSingleByObjectType(Hit,Start,End,ObjectsToTrace,Params);
            Point->SetStringField(TEXT("bone"),Bone->ToString());
            Point->SetArrayField(TEXT("bone_world_cm"),Vec(Location));
            Point->SetArrayField(TEXT("trace_start_cm"),Vec(Start));
            Point->SetArrayField(TEXT("trace_end_cm"),Vec(End));
            Point->SetBoolField(TEXT("blocking_hit"),bHit&&Hit.bBlockingHit);
            Point->SetBoolField(TEXT("start_penetrating"),Hit.bStartPenetrating);
            if (bHit)
            {
                const double NormalDot=FVector::DotProduct(Hit.ImpactNormal,ExpectedNormal);
                Point->SetStringField(TEXT("hit_actor"),GetPathNameSafe(Hit.GetActor()));
                Point->SetStringField(TEXT("hit_component"),GetPathNameSafe(Hit.GetComponent()));
                Point->SetArrayField(TEXT("hit_location_cm"),Vec(Hit.ImpactPoint));
                Point->SetArrayField(TEXT("hit_normal"),Vec(Hit.ImpactNormal));
                Point->SetNumberField(TEXT("normal_dot_expected"),NormalDot);
                Point->SetNumberField(TEXT("bone_minus_surface_z_cm"),Location.Z-Hit.ImpactPoint.Z);
                bPass=Hit.bBlockingHit&&!Hit.bStartPenetrating&&Hit.GetActor()==Expected
                    &&NormalDot>=FMath::Cos(FMath::DegreesToRadians(1.));
            }
        }
        Point->SetBoolField(TEXT("expected_ramp_top_query_pass"),bPass);
        bAll&=bPass; Points.Add(MakeShared<FJsonValueObject>(Point));
    }
    Row->SetArrayField(TEXT("bone_surface_queries"),Points);
    if (Label==TEXT("AutomaticRecovery"))
    {
        const FFindFloorResult& Floor=NPC->GetCharacterMovement()->CurrentFloor;
        const bool bFloor=Floor.IsWalkableFloor()&&!Floor.HitResult.bStartPenetrating
            &&Floor.HitResult.GetActor()==Expected
            &&FVector::DotProduct(Floor.HitResult.ImpactNormal,ExpectedNormal)>=FMath::Cos(FMath::DegreesToRadians(1.));
        Row->SetBoolField(TEXT("recovered_cmc_current_floor_is_ramp"),bFloor);
        Row->SetStringField(TEXT("recovered_cmc_floor_actor"),GetPathNameSafe(Floor.HitResult.GetActor()));
        Row->SetArrayField(TEXT("recovered_cmc_floor_normal"),Vec(Floor.HitResult.ImpactNormal));
        bAll&=bFloor;
    }
    Row->SetBoolField(TEXT("terrain_query_pass"),bAll);
    Row->SetStringField(TEXT("scope"),TEXT("Actual WorldStatic surface queries under four mapped bone points; recovery additionally requires CMC CurrentFloor on Ramp. Not skin contact, force, cloth or penetration-depth proof."));
    RampSupportSamples.Add(Row);
    return Check(TEXT("ramp_support_")+Label,bAll,
        TEXT("Fall-down/get-up/recovered surface must be the actual ordered Ramp top; bottom Floor or side face cannot pass"));
}

void AHCM5VS2NPCReactionReviewDirector::AimCamera()
{
    if (!ReviewCamera || !Specimens.IsValidIndex(SpecimenIndex) || !IsValid(Specimens[SpecimenIndex])) return;
    auto* NPC=Specimens[SpecimenIndex].Get();
    const FVector Focus=NPC->GetPhysicalReaction()->GetPhysicalLocation()+FVector(0,0,20);
    const FVector Offset=NPC->GetActorForwardVector()*440+NPC->GetActorRightVector()*-380+FVector(0,0,260);
    if (!bOcclusionAwareReviewCamera || ReviewPair.IsEmpty())
    {
        // Preserve every existing fixture's camera unless it explicitly opts in.
        ReviewCamera->SetActorLocation(Focus+Offset);
        ReviewCamera->SetActorRotation((-Offset).Rotation());
        return;
    }
    double Aspect=16./9.;
    if (Viewport.IsValid() && Viewport->Viewport)
    {
        const FIntPoint Size=Viewport->Viewport->GetSizeXY();
        if (Size.Y>0) Aspect=double(Size.X)/Size.Y;
    }
    // World-axis corridor ends, then high oblique views. Never move/hide the
    // specimen or obstacles, never alter collision, impulse or the phase clock.
    const TArray<FVector> Offsets={Offset,FVector(650,0,360),FVector(-650,0,360),
        FVector(0,650,360),FVector(0,-650,360),FVector(440,0,650),FVector(-440,0,650),
        FVector(0,440,650),FVector(0,-440,650),FVector(0,0,720)};
    if (VisibleCameraSpecimen!=SpecimenIndex || !Offsets.IsValidIndex(VisibleCameraCandidate))
    { VisibleCameraSpecimen=SpecimenIndex; VisibleCameraCandidate=0; }
    auto Inspect=[&](int32 Index)
    {
        auto Result=InspectReactionCamera(GetWorld(),NPC,Focus+Offsets[Index],
            (-Offsets[Index]).Rotation(),ReviewCamera->GetCameraComponent()->FieldOfView,Aspect);
        Result.Evidence->SetNumberField(TEXT("candidate_index"),Index);
        return Result;
    };
    auto Best=Inspect(VisibleCameraCandidate);
    TArray<TSharedPtr<FJsonObject>> Attempts={Best.Evidence};
    if (!Best.bAllTargetsVisible)
    {
        const int32 Previous=VisibleCameraCandidate;
        for (int32 Index=0;Index<Offsets.Num();++Index)
        {
            if (Index==Previous) continue;
            auto Candidate=Inspect(Index); Attempts.Add(Candidate.Evidence);
            if (Candidate.bCameraClear && (!Best.bCameraClear || Candidate.VisibleAndFramed>Best.VisibleAndFramed))
            { Best=Candidate; VisibleCameraCandidate=Index; }
            if (Best.bAllTargetsVisible) break;
        }
    }
    CameraVisibilityDecision=MakeShared<FJsonObject>();
    CameraVisibilityDecision->SetNumberField(TEXT("decision_frame"),double(GFrameCounter));
    CameraVisibilityDecision->SetNumberField(TEXT("selected_candidate"),VisibleCameraCandidate);
    CameraVisibilityDecision->SetArrayField(TEXT("attempts"),Objects(Attempts));
    CameraVisibilityDecision->SetObjectField(TEXT("selected_query"),Best.Evidence);
    ReviewCamera->SetActorLocation(Focus+Offsets[VisibleCameraCandidate]);
    ReviewCamera->SetActorRotation((-Offsets[VisibleCameraCandidate]).Rotation());
}

void AHCM5VS2NPCReactionReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (bStopped) return;
    const double Wall=FPlatformTime::Seconds(), WallStep=FMath::Max(0.,Wall-LastTickWall); LastTickWall=Wall;
    if(!ReviewPair.IsEmpty())if(auto* FocusController=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0)))
        if(!FocusController->IsGameplayFocused())
        {
            // Use the real pause menu and its P resume. Do not keep dealing
            // damage offscreen or automatically steal focus/resume a user's pause.
            if(!FocusController->IsPauseMenuOpen()&&!UGameplayStatics::IsGamePaused(this))
            {FocusController->TogglePauseMenu();++FocusPauseCount;}
            return;
        }
    if (UGameplayStatics::IsGamePaused(this)) return; // P is owned by the real controller; no test work or auto-exit while paused.
    if (bExitPending) { if (Wall-FinishedWall>2 && !FScreenshotRequest::IsScreenshotRequested()) { bExitPending=false; FPlatformMisc::RequestExitWithStatus(false,bFinalEvidenceWriteFailed?1:0,TEXT("M5VS2 NPC gameplay review finished")); } return; }
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
        TArray<TSharedPtr<FJsonValue>> Actors;
        for (AHCM5VS2NPC* NPC:Specimens)
        { const auto State=Snapshot(NPC,false); Actors.Add(MakeShared<FJsonValueObject>(State)); SampleGetUpSupport(NPC,State); }
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
        const FVector Direction=ImpactDirection(); FHitResult Hit(NPC,Mesh,Mesh->GetSocketLocation(NPC->GetPhysicalRootBone()),-Direction);
        Hit.BoneName=NPC->GetPhysicalRootBone(); Hit.bBlockingHit=true; const float Health=NPC->GetHealth();
        HitStartPose=BonePose(); MaximumHitPoseDeltaDegrees=0; bObservedAdvancingPose=false;
        const float Applied=Reaction->ReceiveVehicleImpact(Vehicle,Hit,Direction*450,Direction,450,SpecimenIndex+1,TEXT("GameplayProbe"));
        HealthAfterImpact=NPC->GetHealth(); ImpactStarted=Now; StableSince=-1; bDownCaptured=false; Events.Add(Snapshot(NPC,true));
        if (!Check(TEXT("impact_entry_knockdown"),Applied>0 && FMath::IsNearlyEqual(HealthAfterImpact,Health-Applied,.01f) &&
            !NPC->IsDead() && Reaction->IsLivingRagdollActive() && !Mesh->IsSimulatingPhysics(),
            TEXT("ReceiveVehicleImpact 450 cm/s accepted living delayed hit phase; no instantaneous simulation. Function call, NOT actual contact"))) return;
        SetPhase(PreImpact); return;
    }
    if (Phase==PreImpact)
    {
        if (!ObserveHitPose(false)) return;
        if (Mesh->IsSimulatingPhysics())
        {
            Events.Add(Snapshot(NPC,true));
            if (!Check(TEXT("visible_pose_precedes_living_physics"),bObservedAdvancingPose && MaximumHitPoseDeltaDegrees>1.f,
                FString::Printf(TEXT("Observed non-simulated bone rotation delta %.3f degrees before handoff; animation clock alone is insufficient"),MaximumHitPoseDeltaDegrees))) return;
            SetPhase(AwaitRecovery);
        }
        else if (Now-PhaseStarted>1) Finish(TEXT("FAIL"),TEXT("Delayed living physics did not start within one second"));
        return;
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
        HitStartPose=BonePose(); MaximumHitPoseDeltaDegrees=0; bObservedAdvancingPose=false;
        const auto BeforeLethal=Snapshot(NPC,true); BeforeLethal->SetStringField(TEXT("event"),TEXT("lethal_before_take_damage")); Events.Add(BeforeLethal);
        const FVector Direction=NPC->GetActorForwardVector(); FHitResult Hit(NPC,Mesh,Mesh->GetSocketLocation(NPC->GetPhysicalRootBone()),-Direction);
        Hit.BoneName=NPC->GetPhysicalRootBone(); Hit.bBlockingHit=true; const float Damage=NPC->GetHealth()+1;
        FPointDamageEvent Event(Damage,Hit,Direction,UDamageType::StaticClass()); const float Applied=NPC->TakeDamage(Damage,Event,nullptr,Vehicle);
        const auto AfterLethal=Snapshot(NPC,true); AfterLethal->SetStringField(TEXT("event"),TEXT("lethal_after_take_damage")); Events.Add(AfterLethal);
        if (!Check(TEXT("lethal_damage_entry"),Applied>0 && NPC->IsDead() && NPC->GetHealth()==0 && NPC->IsCorpsePresent() && !NPC->IsRagdollActive() && !NPC->GetController(),
            TEXT("Real TakeDamage -> Die; immediate logical death and AI unpossess, delayed corpse physics"))) return;
        SetPhase(PreDeath); return;
    }
    if (Phase==PreDeath)
    {
        if (!ObserveHitPose(true)) return;
        if (Mesh->IsSimulatingPhysics())
        {
            Events.Add(Snapshot(NPC,true));
            if (!Check(TEXT("visible_pose_precedes_death_physics"),bObservedAdvancingPose && MaximumHitPoseDeltaDegrees>1.f,
                FString::Printf(TEXT("Observed non-simulated bone rotation delta %.3f degrees before corpse handoff"),MaximumHitPoseDeltaDegrees))) return;
            SetPhase(DeathSettle);
        }
        else if (Now-PhaseStarted>1) Finish(TEXT("FAIL"),TEXT("Delayed corpse physics did not start within one second"));
        return;
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
        else Finish(TEXT("PASS"),TEXT("Selected pair native point damage, living impact, ordinary automatic recovery, lethal damage and explicit RestoreState completed; appearance USER_REVIEW"));
    }
}

void AHCM5VS2NPCReactionReviewDirector::Capture(const FString& Label,int32 NextPhase)
{
    if (bStopped || bPending || FScreenshotRequest::IsScreenshotRequested()) return;
    if (Captures.Num()>=10) { Finish(TEXT("FAIL"),TEXT("Ten-image bound exceeded")); return; }
    if (bRequireRampSupport && Label!=TEXT("Death_After5Seconds")
        && !CheckRampSupport(Specimens[SpecimenIndex],Label)) return;
    PendingPNG=Directory/(FString::Printf(TEXT("%02d_%s_%s.png"),Captures.Num(),*Specimens[SpecimenIndex]->StableId.ToString(),*Label));
    if (IFileManager::Get().FileExists(*PendingPNG)) { Finish(TEXT("FAIL"),TEXT("Refuse screenshot overwrite")); return; }
    Pending=MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("label"),Label); Pending->SetStringField(TEXT("file"),PendingPNG);
    Pending->SetNumberField(TEXT("request_frame"),double(GFrameCounter)); Pending->SetObjectField(TEXT("state_at_request"),Snapshot(Specimens[SpecimenIndex],true));
    // Explicit diagnostics only, max five existing J capture requests. No physics/node writes.
    if (FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCClothProbe"))
        && Specimens[SpecimenIndex]->StableId==FName(TEXT("M5VS2_J")))
        Pending->SetObjectField(TEXT("j_cloth_probe_at_request"),UHCM5VS2NPCClothDiagnostics::CaptureJ(Specimens[SpecimenIndex]->GetMesh()));
    if (FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCJChaosClothReview"))
        && Specimens[SpecimenIndex]->StableId==FName(TEXT("M5VS2_J")))
        Pending->SetObjectField(TEXT("j_chaos_cloth_at_request"),UHCM5VS2NPCClothDiagnostics::CaptureChaosJ(Specimens[SpecimenIndex]->GetMesh()));
    if (Controller && Controller->PlayerCameraManager)
    {
        const APlayerCameraManager* Camera=Controller->PlayerCameraManager.Get();
        Pending->SetArrayField(TEXT("actual_camera_location_cm"),Vec(Camera->GetCameraLocation()));
        const FRotator Rot=Camera->GetCameraRotation(); Pending->SetArrayField(TEXT("actual_camera_pitch_yaw_roll"),Vec(FVector(Rot.Pitch,Rot.Yaw,Rot.Roll)));
        Pending->SetNumberField(TEXT("actual_camera_fov"),Camera->GetFOVAngle());
        if (bOcclusionAwareReviewCamera && !ReviewPair.IsEmpty())
        {
            double Aspect=16./9.;
            if (Viewport.IsValid() && Viewport->Viewport)
            {
                const FIntPoint Size=Viewport->Viewport->GetSizeXY();
                if (Size.Y>0) Aspect=double(Size.X)/Size.Y;
            }
            const auto Actual=InspectReactionCamera(GetWorld(),Specimens[SpecimenIndex],
                Camera->GetCameraLocation(),Camera->GetCameraRotation(),Camera->GetFOVAngle(),Aspect);
            Pending->SetObjectField(TEXT("actual_camera_visibility_at_request"),Actual.Evidence);
            if (CameraVisibilityDecision)
                Pending->SetObjectField(TEXT("camera_candidate_decision"),CameraVisibilityDecision);
        }
    }
    Pending->SetStringField(TEXT("alignment_scope"),TEXT("State at screenshot request and processed callback; not a claimed exact GPU/skinned-pose timestamp"));
    bPending=true; bProcessed=false; CaptureFrame=GFrameCounter; RequestUnpausedSeconds=UnpausedWall; AfterCapturePhase=NextPhase;
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}

void AHCM5VS2NPCReactionReviewDirector::Processed() { if (!bStopped && bPending) bProcessed=true; }

void AHCM5VS2NPCReactionReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if (Event.Event!=IE_Pressed || bStopped || (!bActive && !bExitPending)) return;
    if (Event.Key==EKeys::P) { ++PauseKeyPresses; return; } // Observe only. The controller handles P.
    if (Event.Key!=EKeys::Escape) return;
    bStopped=true; bActive=false; bExitPending=false; bAutoQuit=false; StopFrame=GFrameCounter;
    // Independent marker remains observable if publishing the JSON itself fails.
    UE_LOG(LogTemp,Warning,TEXT("M5VS2_NPC_REACTION_R2_USER_ABORTED frame=%llu; capture/test/autoexit permanently stopped"),static_cast<unsigned long long>(StopFrame));
    if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    bPending=false; SetActorTickEnabled(false);
    Write(TEXT("USER_ABORTED"),TEXT("Escape permanently latched. No subsequent test damage/camera/capture/restore/exit. Event not consumed; ordinary game remains under user control."));
}

void AHCM5VS2NPCReactionReviewDirector::Finish(const FString& Status,const FString& Detail)
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
    if (!Write(Status,Detail))
    {
        bFinalEvidenceWriteFailed=true;
        UE_LOG(LogTemp,Error,TEXT("M5VS2_NPC_REACTION_R2_FINAL_EVIDENCE_WRITE_FAILED intended_status=%s; final result is FAIL, exit code 1 when autoquit is enabled; %s"),*Status,*Directory);
    }
    else UE_LOG(LogTemp,Display,TEXT("M5VS2_NPC_REACTION_R2_%s %s"),*Status,*Directory);
}

bool AHCM5VS2NPCReactionReviewDirector::Write(const FString& Status,const FString& Detail)
{
    if (Directory.IsEmpty()) return false;
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),Status); R->SetStringField(TEXT("detail"),Detail);
    R->SetStringField(TEXT("map"),GetWorld()->GetOutermost()->GetName()); R->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));
    R->SetBoolField(TEXT("actual_vehicle_contact_tested"),false); R->SetBoolField(TEXT("os_input_used"),false); R->SetBoolField(TEXT("save_load_tested"),false);
    R->SetStringField(TEXT("site"),ProbeSite); R->SetStringField(TEXT("requested_impact_direction"),FallDirection);
    R->SetStringField(TEXT("roster_version"),ReviewPair.IsEmpty()?TEXT("LEGACY_QR"):TEXT("FINAL_IDLE_R2_PAIR"));
    R->SetStringField(TEXT("review_pair"),ReviewPair.IsEmpty()?TEXT("QR"):ReviewPair);
    R->SetNumberField(TEXT("focus_pause_count"),FocusPauseCount);
    R->SetStringField(TEXT("focus_pause_behavior"),ReviewPair.IsEmpty()?TEXT("Legacy behavior unchanged; P pauses"):TEXT("Focus loss opens real pause menu; user P resumes after returning; no forced focus"));
    TArray<TSharedPtr<FJsonValue>> Roster;
    for(const AHCM5VS2NPC* NPC:Specimens)if(IsValid(NPC))
    {auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stable_id"),NPC->StableId.ToString());Row->SetStringField(TEXT("blueprint"),NPC->GetClass()->GetOutermost()->GetName());Roster.Add(MakeShared<FJsonValueObject>(Row));}
    R->SetArrayField(TEXT("roster"),Roster);
    R->SetStringField(TEXT("four_direction_scope"),TEXT("Requested impact direction is NOT a claimed get-up direction. Read each actual getup_clip; aggregate B/F/L/R coverage separately. No pose staging or rotation forcing."));
    R->SetStringField(TEXT("recovery_scope"),TEXT("Actual per-specimen physical diagnostics report whether legacy blend or authored root-motion get-up ran; montage progress and completion are measured, not inferred from configuration"));
    R->SetStringField(TEXT("sampling_scope"),TEXT("At most 10 Hz actual post-update samples, no synthetic backfill; screenshot stalls remain visible in timestamps"));
    R->SetNumberField(TEXT("maximum_unpaused_seconds"),120); R->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-StartedWall);
    R->SetNumberField(TEXT("elapsed_unpaused_wall_seconds"),UnpausedWall); R->SetNumberField(TEXT("elapsed_game_seconds"),GetWorld()->GetTimeSeconds()-StartedGame);
    R->SetBoolField(TEXT("user_stop_latched"),bStopped); R->SetNumberField(TEXT("stop_frame"),double(StopFrame));
    R->SetNumberField(TEXT("observed_pause_key_presses"),PauseKeyPresses); R->SetBoolField(TEXT("explicit_autoquit"),bAutoQuit);
    R->SetArrayField(TEXT("checks"),Objects(Checks)); R->SetArrayField(TEXT("events"),Objects(Events));
    R->SetArrayField(TEXT("process_samples"),Objects(Samples)); R->SetArrayField(TEXT("captures"),Objects(Captures));
    R->SetArrayField(TEXT("getup_support_samples"),Objects(GetUpSupportSamples));
    R->SetBoolField(TEXT("require_ramp_support"),bRequireRampSupport);
    R->SetArrayField(TEXT("ramp_support_samples"),Objects(RampSupportSamples));
    const bool bTerrainFailed=RampSupportSamples.ContainsByPredicate([](const TSharedPtr<FJsonObject>& Row)
        {return !Row->GetBoolField(TEXT("terrain_query_pass"));});
    R->SetStringField(TEXT("ramp_terrain_coverage"),!bRequireRampSupport?TEXT("NOT_RUN"):
        bTerrainFailed?TEXT("FAIL"):RampSupportSamples.Num()==8?TEXT("PASS_SURFACE_QUERIES_ONLY"):TEXT("INCOMPLETE"));
    R->SetStringField(TEXT("getup_support_scope"),TEXT("Only actual Playing montage positions 0..1.5 seconds; existing <=10 Hz sampling; <=40 total rows. Mapped bone points and +20/-250 cm world-static ray hits, NOT skinned palm/sole clearance, contact force, or contact manifolds. No physics/animation mutation; no root-motion consumption; missing frames are not reconstructed."));
    FString Text;
    if (!FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text)))
    { UE_LOG(LogTemp,Error,TEXT("M5VS2_NPC_REACTION_R2_EVIDENCE_WRITE_FAILED stage=serialize")); return false; }
    const FString Destination=Directory/TEXT("npc_reaction_review.json");
    const FString Temporary=Directory/(TEXT("npc_reaction_review_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp"));
    // Write and close a new file. Never truncate the published snapshot while a
    // monitor is reading it. A failed publish preserves both old evidence and temp.
    if (!FFileHelper::SaveStringToFile(Text,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        const uint32 Error=FPlatformMisc::GetLastError();
        UE_LOG(LogTemp,Error,TEXT("M5VS2_NPC_REACTION_R2_EVIDENCE_WRITE_FAILED stage=temp_write error=%u file=%s"),Error,*Temporary);
        return false;
    }
#if PLATFORM_WINDOWS
    // Local UE 5.8 IFileManager::Move(Replace=true) deletes Dest then uses
    // MoveFileW, and can retry. Use one same-directory replace call instead.
    // Extended absolute Windows paths also cover long unique evidence names.
    const FString From=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Temporary).Replace(TEXT("/"),TEXT("\\"));
    const FString To=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Destination).Replace(TEXT("/"),TEXT("\\"));
    if (!::MoveFileExW(*From,*To,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    {
        const uint32 Error=FPlatformMisc::GetLastError();
        UE_LOG(LogTemp,Error,TEXT("M5VS2_NPC_REACTION_R2_EVIDENCE_WRITE_FAILED stage=replace error=%u from=%s to=%s"),Error,*Temporary,*Destination);
        return false;
    }
    return true;
#else
    UE_LOG(LogTemp,Error,TEXT("M5VS2_NPC_REACTION_R2_EVIDENCE_WRITE_FAILED stage=unsupported_platform temp=%s"),*Temporary);
    return false;
#endif
}

void AHCM5VS2NPCReactionReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bActive && !bStopped) Write(TEXT("NOT_RUN"),TEXT("World ended before the bounded gameplay sequence completed"));
    if (Viewport.IsValid() && InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle);
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    // No actor restoration here, particularly after Escape.
    Super::EndPlay(Reason);
}

FVector AHCM5VS2NPCReactionReviewDirector::ImpactDirection() const
{
    const AHCM5VS2NPC* N=Specimens[SpecimenIndex];
    if (FallDirection==TEXT("Backward")) return -N->GetActorForwardVector().GetSafeNormal2D();
    if (FallDirection==TEXT("Left")) return -N->GetActorRightVector().GetSafeNormal2D();
    if (FallDirection==TEXT("Right")) return N->GetActorRightVector().GetSafeNormal2D();
    return N->GetActorForwardVector().GetSafeNormal2D();
}
TArray<FTransform> AHCM5VS2NPCReactionReviewDirector::BonePose() const
{
    TArray<FTransform> Result;
    const auto* M=Specimens[SpecimenIndex]->GetMesh();
    // Component-space pose excludes whole-actor rotation; only actual evaluated bones count.
    const TArray<FTransform>& Pose=M->GetComponentSpaceTransforms();
    Result.Append(Pose); return Result;
}
bool AHCM5VS2NPCReactionReviewDirector::ObserveHitPose(bool bLethal)
{
    auto* N=Specimens[SpecimenIndex].Get(); auto* M=N->GetMesh();
    if (M->IsSimulatingPhysics()) return true;
    const auto Pose=BonePose();
    if (!Check(TEXT("pre_hit_pose_array_finite"),Pose.Num()==HitStartPose.Num() && Pose.Num()>0 &&
        !Pose.ContainsByPredicate([](const FTransform& T){return T.ContainsNaN();}),TEXT("Actual evaluated component-space bones"))) return false;
    float MaxDelta=0; TArray<TSharedPtr<FJsonValue>> Bones;
    if (!N->NPCProfile) { Finish(TEXT("FAIL"),TEXT("Missing actual humanoid profile")); return false; }
    // Hair/skirt spring bones are excluded: they move even when no hit pose is evaluated.
    for (const TCHAR* BoneRole:{TEXT("spine"),TEXT("chest"),TEXT("leftUpperArm"),TEXT("leftLowerArm"),
        TEXT("rightUpperArm"),TEXT("rightLowerArm"),TEXT("leftUpperLeg"),TEXT("leftLowerLeg"),
        TEXT("rightUpperLeg"),TEXT("rightLowerLeg")})
    {
        const FName* Bone=N->NPCProfile->HumanoidBones.Find(FName(BoneRole));
        const int32 I=Bone?M->GetBoneIndex(*Bone):INDEX_NONE;
        if (!Pose.IsValidIndex(I)) { Finish(TEXT("FAIL"),TEXT("Required mapped reaction bone missing")); return false; }
        const float Delta=FMath::RadiansToDegrees(float(Pose[I].GetRotation().AngularDistance(HitStartPose[I].GetRotation())));
        MaxDelta=FMath::Max(MaxDelta,Delta);
        auto B=MakeShared<FJsonObject>(); B->SetStringField(TEXT("role"),BoneRole); B->SetStringField(TEXT("bone"),Bone->ToString());
        B->SetNumberField(TEXT("component_rotation_delta_degrees"),Delta); Bones.Add(MakeShared<FJsonValueObject>(B));
    }
    MaximumHitPoseDeltaDegrees=FMath::Max(MaximumHitPoseDeltaDegrees,MaxDelta);
    if (N->HasAdvancedPreRagdollReaction() && MaxDelta>1.f) bObservedAdvancingPose=true;
    auto Row=Snapshot(N,true); Row->SetStringField(TEXT("event"),bLethal?TEXT("lethal_pre_physics_pose"):TEXT("living_pre_physics_pose"));
    Row->SetNumberField(TEXT("actual_component_bone_delta_degrees"),MaxDelta);
    Row->SetArrayField(TEXT("humanoid_reaction_bone_deltas"),Bones); Events.Add(Row);
    return true;
}
