#include "HCM5VS2NPCReviewDirector.h"
#include "HCM5VS2NPC.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "HCM5VS2NPCAnimInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "Chaos/CollisionFilterData.h"
#include "Materials/MaterialInterface.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
bool QSkinComparison() { return FParse::Param(FCommandLine::Get(),TEXT("M5VS2QSkinComparison")); }
bool SceneIdleReview() { return FParse::Param(FCommandLine::Get(),TEXT("M5VS2SceneIdleReview")); }
TArray<TSharedPtr<FJsonValue>> V3(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; }
const FName Emotions[] = {TEXT("Neutral"), TEXT("Blink"), TEXT("Happy"), TEXT("Surprised"), TEXT("Angry"), TEXT("Sad"), TEXT("Shy"), TEXT("Serious")};
const FName CastProbeEmotions[] = {TEXT("Happy"), TEXT("Angry"), TEXT("Sad")};
const FName CastProbeGroups[] = {TEXT("Joy"), TEXT("Angry"), TEXT("Sorrow")};
const double PhysicsCaptureTimes[] = {.02, .1, .25, .5, 1., 2., 3.5, 5., 5.5, 6.};
bool PNG(const FString& Path, int32& W, int32& H)
{
    TUniquePtr<FArchive> F(IFileManager::Get().CreateFileReader(*Path));
    if (!F || F->TotalSize() < 33) return false;
    uint8 B[24]; F->Serialize(B, 24); const uint8 Magic[] = {137,80,78,71,13,10,26,10};
    if (F->IsError() || FMemory::Memcmp(B, Magic, 8) || FMemory::Memcmp(B+12, "IHDR", 4)) return false;
    auto Big = [](const uint8* P) { return int32(uint32(P[0])<<24 | uint32(P[1])<<16 | uint32(P[2])<<8 | uint32(P[3])); };
    W=Big(B+16); H=Big(B+20); return W>0 && H>0;
}
}

AHCM5VS2NPCReviewDirector::AHCM5VS2NPCReviewDirector()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bTickEvenWhenPaused = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AHCM5VS2NPCReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(), TEXT("M5VS2NPCReview"))) return;
    FString Root;
    if (!FParse::Value(FCommandLine::Get(), TEXT("M5VS2EvidenceDir="), Root) || FPaths::IsRelative(Root)) return;
    Root=FPaths::ConvertRelativePathToFull(Root); FPaths::NormalizeDirectoryName(Root);
    const FString Allowed=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2");
    if (!FPaths::CollapseRelativeDirectories(Root) || !FPaths::IsUnderDirectory(Root, Allowed)) return;
    Directory=Root/(TEXT("NPCReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if (!IFileManager::Get().MakeDirectory(*Directory,true)) return;
    Started=FPlatformTime::Seconds(); Exercises=MakeShared<FJsonObject>();
    bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    bPhysicsOnly=FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCPhysicsReview"));
    bPhysicsSettledOnly=bPhysicsOnly && FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCPhysicsSettledOnly"));
    bPhysicsDisablePostProcess=bPhysicsOnly && FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCPhysicsNoPostProcess"));
    FString Error;
    if (!Start(Error)) { Finish(TEXT("FAIL"),Error); return; }
    bActive=true; SetActorTickEnabled(true); Write(TEXT("RUNNING"),TEXT("Ten seconds of normal rendered warmup; no completed review yet"));
#endif
}

bool AHCM5VS2NPCReviewDirector::Start(FString& Error)
{
    Controller=UGameplayStatics::GetPlayerController(this,0);
    UGameViewportClient* LiveViewport=GetWorld()->GetGameViewport();
    if (!Controller || !LiveViewport || !LiveViewport->Viewport || Specimens.Num()!=(SceneIdleReview()?3:(QSkinComparison()?1:(bCastPortraits?8:2))) || !MainLight)
    { Error=TEXT("Live rendered viewport, mode-specific exact specimen count and main light required"); return false; }
    if (QSkinComparison() && (bCastPortraits || bPhysicsOnly || !GetWorld()->GetPackage()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/WorldRev2/QSkinReview_"))))
    { Error=TEXT("Q skin comparison requires its isolated harbor map and portrait-only mode");return false; }
    if (SceneIdleReview() && (QSkinComparison() || bCastPortraits || bPhysicsOnly || SpecimenIds.Num()!=3
        || !GetWorld()->GetPackage()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/WorldRev2/SceneIdleReview_"))))
    { Error=TEXT("Scene idle review requires its isolated three-subject harbor map");return false; }
    if (bCastPortraits)
    {
        const TSet<FName> Expected={TEXT("Q"),TEXT("R"),TEXT("J"),TEXT("T"),TEXT("U"),TEXT("V"),TEXT("W"),TEXT("X")};
        TSet<FName> Actual; for (FName Id:SpecimenIds) Actual.Add(Id);
        if (!FParse::Param(FCommandLine::Get(),TEXT("M5VS2NPCCastReview")) || bPhysicsOnly
            || !GetWorld()->GetPackage()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/CastReview/Run_"))
            || SpecimenIds.Num()!=8 || Actual.Num()!=8 || Actual.Difference(Expected).Num()!=0)
        { Error=TEXT("Cast review requires explicit flag, unique isolated map and exactly Q/R/J/T/U/V/W/X"); return false; }
        TSet<FString> Sources;
        for (const AHCM5VS2NPC* NPC:Specimens) if (NPC && NPC->NPCProfile) Sources.Add(NPC->NPCProfile->SourceSHA256);
        if (Sources.Num()!=8 || Sources.Contains(FString()))
        { Error=TEXT("Eight genuinely independent source identities required"); return false; }
        CastPeakBlink.Init(0.f,8);
    }
    for (AHCM5VS2NPC* NPC:Specimens)
        if (!IsValid(NPC) || !NPC->GetMesh() || !NPC->NPCProfile || !NPC->NPCFace)
        { Error=TEXT("Invalid NPC/profile/face reference"); return false; }
    Viewport=LiveViewport;
    InputHandle=LiveViewport->OnInputKey().AddUObject(this,&AHCM5VS2NPCReviewDirector::Input);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2NPCReviewDirector::Processed);
    OriginalView=Controller->GetViewTarget();
    if (AHUD* HUD=Controller->GetHUD()) { bSavedHUD=HUD->bShowHUD; HUD->bShowHUD=false; }
    FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient;
    ReviewCamera=GetWorld()->SpawnActor<ACameraActor>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!ReviewCamera) { Error=TEXT("Native camera creation failed"); return false; }
    ReviewCamera->GetCameraComponent()->SetFieldOfView(40.f);
    ReviewCamera->GetCameraComponent()->bConstrainAspectRatio=false;
    ReviewCamera->GetCameraComponent()->PostProcessBlendWeight=0;
    for (AHCM5VS2NPC* NPC:Specimens)
    {
        AddTickPrerequisiteComponent(NPC->GetMesh()); OriginalActorTicks.Add(NPC->IsActorTickEnabled());
        OriginalBlinks.Add(NPC->NPCFace->bAutomaticBlink); NPC->NPCFace->SetEmotion(TEXT("Neutral"));
    }
    return true;
}

TSharedPtr<FJsonObject> AHCM5VS2NPCReviewDirector::Snapshot(AHCM5VS2NPC* NPC) const
{
    auto R=MakeShared<FJsonObject>(); USkeletalMeshComponent* Body=NPC->GetMesh(); UHCM5VS2NPCFaceComponent* Face=NPC->NPCFace.Get();
    R->SetStringField(TEXT("actor"),NPC->GetPathName()); R->SetStringField(TEXT("blueprint_class"),NPC->GetClass()->GetPathName());
    R->SetStringField(TEXT("profile"),GetPathNameSafe(NPC->NPCProfile)); R->SetStringField(TEXT("source_sha256"),NPC->NPCProfile->SourceSHA256);
    R->SetStringField(TEXT("metadata"),NPC->NPCProfile->MetadataAssetPath); R->SetStringField(TEXT("mesh"),GetPathNameSafe(Body->GetSkeletalMeshAsset()));
    R->SetStringField(TEXT("anim_instance"),GetPathNameSafe(Body->GetAnimInstance()));
    R->SetStringField(TEXT("postprocess_instance"),GetPathNameSafe(Body->GetPostProcessInstance()));
    R->SetBoolField(TEXT("postprocess_disabled"),Body->GetDisablePostProcessBlueprint());
    R->SetStringField(TEXT("physics_asset"),GetPathNameSafe(Body->GetPhysicsAsset()));
    R->SetBoolField(TEXT("visual_profile_ready"),NPC->IsVisualProfileReady()); R->SetBoolField(TEXT("face_ready"),Face->IsFaceReady());
    R->SetStringField(TEXT("emotion"),Face->GetEffectiveEmotion().ToString()); R->SetNumberField(TEXT("blink_weight"),Face->GetBlinkWeight());
    R->SetArrayField(TEXT("actor_location_cm"),V3(NPC->GetActorLocation())); R->SetArrayField(TEXT("velocity_cm_s"),V3(NPC->GetVelocity()));
    R->SetBoolField(TEXT("physics_simulating"),Body->IsSimulatingPhysics());
    auto ComponentFilter=MakeShared<FJsonObject>();
    ComponentFilter->SetStringField(TEXT("profile"),Body->GetCollisionProfileName().ToString());
    ComponentFilter->SetNumberField(TEXT("collision_enabled"),int32(Body->GetCollisionEnabled()));
    ComponentFilter->SetNumberField(TEXT("object_channel"),int32(Body->GetCollisionObjectType()));
    ComponentFilter->SetNumberField(TEXT("physics_body_channel"),int32(ECC_PhysicsBody));
    ComponentFilter->SetNumberField(TEXT("response_to_physics_body"),int32(Body->GetCollisionResponseToChannel(ECC_PhysicsBody)));
    ComponentFilter->SetBoolField(TEXT("blocks_physics_body"),Body->GetCollisionResponseToChannel(ECC_PhysicsBody)==ECR_Block);
    R->SetObjectField(TEXT("component_collision_filter"),ComponentFilter);
    if (const auto* Anim=Cast<UHCM5VS2NPCAnimInstance>(Body->GetAnimInstance())) R->SetNumberField(TEXT("animation_ground_speed"),Anim->NPCGroundSpeed);
    auto Bones=MakeShared<FJsonObject>(); auto Positions=MakeShared<FJsonObject>(); auto Morphs=MakeShared<FJsonObject>();
    for (const auto& Pair:NPC->NPCProfile->HumanoidBones)
    { Bones->SetStringField(Pair.Key.ToString(),Pair.Value.ToString()); Positions->SetArrayField(Pair.Key.ToString(),V3(Body->GetSocketLocation(Pair.Value))); }
    for (const auto& Group:NPC->NPCProfile->FaceGroups) for (const auto& Bind:Group.Binds)
        Morphs->SetNumberField(Bind.Morph.ToString(),Body->GetMorphTarget(Bind.Morph));
    R->SetObjectField(TEXT("humanoid_bones"),Bones); R->SetObjectField(TEXT("bone_world_positions_cm"),Positions); R->SetObjectField(TEXT("actual_morph_weights"),Morphs);
    auto RigidBodies=MakeShared<FJsonObject>();
    for (const auto& Pair:NPC->NPCProfile->HumanoidBones)
        if (const FBodyInstance* Instance=Body->GetBodyInstance(Pair.Value))
        {
            auto Row=MakeShared<FJsonObject>();
            const FTransform World=Instance->GetUnrealWorldTransform();
            Row->SetArrayField(TEXT("physics_position_cm"),V3(World.GetLocation()));
            Row->SetBoolField(TEXT("awake"),Instance->IsInstanceAwake());
            Row->SetArrayField(TEXT("linear_velocity_cm_s"),V3(Instance->GetUnrealWorldVelocity()));
            Row->SetArrayField(TEXT("angular_velocity_rad_s"),V3(Instance->GetUnrealWorldAngularVelocityInRadians()));
            const FQuat Rotation=World.GetRotation();
            Row->SetArrayField(TEXT("physics_rotation_xyzw"),{MakeShared<FJsonValueNumber>(Rotation.X),MakeShared<FJsonValueNumber>(Rotation.Y),MakeShared<FJsonValueNumber>(Rotation.Z),MakeShared<FJsonValueNumber>(Rotation.W)});
            Row->SetNumberField(TEXT("bone_physics_position_distance_cm"),FVector::Dist(Body->GetSocketLocation(Pair.Value),World.GetLocation()));
            Row->SetNumberField(TEXT("bone_physics_rotation_difference_degrees"),FMath::RadiansToDegrees(Body->GetSocketQuaternion(Pair.Value).AngularDistance(World.GetRotation())));
            RigidBodies->SetObjectField(Pair.Key.ToString(),Row);
        }
    R->SetObjectField(TEXT("actual_rigid_bodies"),RigidBodies);
    TArray<TSharedPtr<FJsonValue>> Joints;
    for (const FConstraintInstance* Joint:Body->Constraints)
    {
        if (!Joint) continue;
        auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("joint"),Joint->JointName.ToString());
        Row->SetStringField(TEXT("child"),Joint->ConstraintBone1.ToString());
        Row->SetStringField(TEXT("parent"),Joint->ConstraintBone2.ToString());
        Row->SetBoolField(TEXT("valid"),Joint->IsValidConstraintInstance());
        Row->SetBoolField(TEXT("use_linear_joint_solver"),Joint->ProfileInstance.bUseLinearJointSolver);
        Row->SetArrayField(TEXT("engine_current_swing1_swing2_twist_radians"),V3(FVector(Joint->GetCurrentSwing1(),Joint->GetCurrentSwing2(),Joint->GetCurrentTwist())));
        Row->SetArrayField(TEXT("limits_swing1_swing2_twist_degrees"),V3(FVector(Joint->GetAngularSwing1Limit(),Joint->GetAngularSwing2Limit(),Joint->GetAngularTwistLimit())));
        Row->SetArrayField(TEXT("motion_swing1_swing2_twist"),V3(FVector(int32(Joint->GetAngularSwing1Motion()),int32(Joint->GetAngularSwing2Motion()),int32(Joint->GetAngularTwistMotion()))));
        const FBodyInstance* Child=Body->GetBodyInstance(Joint->ConstraintBone1);
        const FBodyInstance* Parent=Body->GetBodyInstance(Joint->ConstraintBone2);
        if (Child && Parent)
        {
            const FTransform A=Joint->GetRefFrame(EConstraintFrame::Frame1)*Child->GetUnrealWorldTransform();
            const FTransform B=Joint->GetRefFrame(EConstraintFrame::Frame2)*Parent->GetUnrealWorldTransform();
            Row->SetNumberField(TEXT("world_anchor_separation_cm"),FVector::Dist(A.GetLocation(),B.GetLocation()));
            const FQuat Q=B.GetRotation().Inverse()*A.GetRotation();
            Row->SetArrayField(TEXT("child_in_parent_frame_rotation_xyzw"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});
        }
        Joints.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("live_constraints"),Joints);
    // Analytic capsule data for diagnosis only: not a claim about rendered skin contact.
    TArray<TSharedPtr<FJsonValue>> Shapes, Contacts, ActualShapeFilters;
    struct FWorldCapsule { int32 Index; FName Bone; FVector A,B; float Radius; };
    TArray<FWorldCapsule> WorldCapsules;
    if (const UPhysicsAsset* Asset=Body->GetPhysicsAsset())
    {
        for (int32 Index=0;Index<Asset->SkeletalBodySetups.Num();++Index)
        {
            const USkeletalBodySetup* Setup=Asset->SkeletalBodySetups[Index];
            const FBodyInstance* Instance=Setup?Body->GetBodyInstance(Setup->BoneName):nullptr;
            auto FilterRow=MakeShared<FJsonObject>();
            FilterRow->SetNumberField(TEXT("physics_asset_body_index"),Index);
            FilterRow->SetStringField(TEXT("bone"),Setup?Setup->BoneName.ToString():TEXT("None"));
            FilterRow->SetBoolField(TEXT("body_instance_present"),Instance!=nullptr);
            TArray<TSharedPtr<FJsonValue>> FilterShapes;
            int32 SimulationShapeCount=0, BlockingPhysicsBodyShapeCount=0;
            bool bReadExecuted=false;
            if (Instance)
            {
                FilterRow->SetNumberField(TEXT("body_instance_object_channel"),int32(Instance->GetObjectType()));
                FilterRow->SetNumberField(TEXT("body_instance_response_to_physics_body"),int32(Instance->GetResponseToChannel(ECC_PhysicsBody)));
                // Read the real actor shapes under the engine scene read lock, not the asset's intended filter.
                bReadExecuted=FPhysicsCommand::ExecuteRead(Instance->GetPhysicsActor(),[&](const FPhysicsActorHandle& Actor)
                {
                    TArray<FPhysicsShapeHandle> ActorShapes;
                    FPhysicsInterface::GetAllShapes_AssumedLocked(Actor,ActorShapes);
                    FilterRow->SetNumberField(TEXT("actor_shape_count"),ActorShapes.Num());
                    for (int32 ShapeIndex=0;ShapeIndex<ActorShapes.Num();++ShapeIndex)
                    {
                        const FPhysicsShapeHandle& Shape=ActorShapes[ShapeIndex];
                        auto ShapeRow=MakeShared<FJsonObject>();
                        ShapeRow->SetNumberField(TEXT("actor_shape_index"),ShapeIndex);
                        ShapeRow->SetBoolField(TEXT("shape_handle_valid"),Shape.IsValid());
                        if (Shape.IsValid())
                        {
                            const bool bOwned=Instance->GetOriginalBodyInstance(Shape)==Instance;
                            const bool bSimulation=FPhysicsInterface::IsSimulationShape(Shape);
                            const auto Combined=FPhysicsInterface::GetCombinedShapeFilterData(Shape);
                            const auto& Filter=Combined.GetShapeFilterData();
                            const uint64 BlockMask=Filter.GetBlockChannels();
                            const bool bBlocksPhysicsBody=(BlockMask&(uint64(1)<<uint32(ECC_PhysicsBody)))!=0;
                            ShapeRow->SetBoolField(TEXT("owned_by_body_instance"),bOwned);
                            ShapeRow->SetBoolField(TEXT("simulation_enabled"),bSimulation);
                            ShapeRow->SetBoolField(TEXT("query_enabled"),FPhysicsInterface::IsQueryShape(Shape));
                            ShapeRow->SetBoolField(TEXT("combined_filter_valid"),Combined.IsValid());
                            ShapeRow->SetBoolField(TEXT("simulation_filter_valid"),Filter.IsSimValid());
                            ShapeRow->SetNumberField(TEXT("object_channel"),Filter.GetCollisionChannelIndex());
                            // Strings preserve every bit of the 64-bit masks in JSON consumers.
                            ShapeRow->SetStringField(TEXT("object_channel_mask_hex"),FString::Printf(TEXT("0x%016llX"),Filter.GetCollisionChannelMask()));
                            ShapeRow->SetStringField(TEXT("block_channel_mask_hex"),FString::Printf(TEXT("0x%016llX"),BlockMask));
                            ShapeRow->SetStringField(TEXT("overlap_channel_mask_hex"),FString::Printf(TEXT("0x%016llX"),Filter.GetOverlapChannels()));
                            ShapeRow->SetNumberField(TEXT("mask_filter"),Filter.GetMaskFilter());
                            ShapeRow->SetBoolField(TEXT("blocks_physics_body_channel"),bBlocksPhysicsBody);
                            if (bOwned && bSimulation)
                            {
                                ++SimulationShapeCount;
                                if (Filter.IsSimValid() && bBlocksPhysicsBody) ++BlockingPhysicsBodyShapeCount;
                            }
                        }
                        FilterShapes.Add(MakeShared<FJsonValueObject>(ShapeRow));
                    }
                });
            }
            FilterRow->SetBoolField(TEXT("scene_read_executed"),bReadExecuted);
            FilterRow->SetNumberField(TEXT("owned_simulation_shape_count"),SimulationShapeCount);
            FilterRow->SetNumberField(TEXT("owned_simulation_shapes_blocking_physics_body"),BlockingPhysicsBodyShapeCount);
            FilterRow->SetArrayField(TEXT("shapes"),FilterShapes);
            ActualShapeFilters.Add(MakeShared<FJsonValueObject>(FilterRow));
            if (!Instance) continue;
            for (const FKSphylElem& Capsule:Setup->AggGeom.SphylElems)
            {
                const FTransform T=Capsule.GetTransform()*Instance->GetUnrealWorldTransform();
                const FVector Half=T.GetUnitAxis(EAxis::Z)*Capsule.Length*.5;
                const FVector A=T.GetLocation()-Half,B=T.GetLocation()+Half;
                auto Shape=MakeShared<FJsonObject>(); Shape->SetStringField(TEXT("bone"),Setup->BoneName.ToString());
                Shape->SetArrayField(TEXT("a_cm"),V3(A)); Shape->SetArrayField(TEXT("b_cm"),V3(B)); Shape->SetNumberField(TEXT("radius_cm"),Capsule.Radius);
                Shapes.Add(MakeShared<FJsonValueObject>(Shape)); WorldCapsules.Add({Index,Setup->BoneName,A,B,Capsule.Radius});
            }
        }
        for (int32 A=0;A<WorldCapsules.Num();++A) for (int32 B=A+1;B<WorldCapsules.Num();++B)
        {
            const auto& One=WorldCapsules[A]; const auto& Two=WorldCapsules[B];
            if (One.Index==Two.Index) continue;
            FVector P,Q; FMath::SegmentDistToSegmentSafe(One.A,One.B,Two.A,Two.B,P,Q);
            const double Gap=FVector::Dist(P,Q)-One.Radius-Two.Radius;
            if (Gap>.5) continue;
            auto Contact=MakeShared<FJsonObject>(); Contact->SetStringField(TEXT("a"),One.Bone.ToString()); Contact->SetStringField(TEXT("b"),Two.Bone.ToString());
            Contact->SetNumberField(TEXT("analytic_capsule_gap_cm"),Gap);
            Contact->SetBoolField(TEXT("pair_collision_disabled"),Asset->CollisionDisableTable.Contains(FRigidBodyIndexPair(One.Index,Two.Index)));
            Contacts.Add(MakeShared<FJsonValueObject>(Contact));
        }
    }
    R->SetArrayField(TEXT("world_capsules"),Shapes); R->SetArrayField(TEXT("capsule_near_or_overlap_pairs_not_contact_manifolds"),Contacts);
    R->SetArrayField(TEXT("actual_body_shape_filters"),ActualShapeFilters);
    R->SetStringField(TEXT("actual_body_shape_filter_scope"),TEXT("Scene-read-locked native actor shape filters; block-channel bits alone do not prove a contact or bypass disabled constraint/asset pairs"));
    auto Hair=MakeShared<FJsonObject>();
    for (FName Bone:{FName(TEXT("J_Sec_Hair1_01")),FName(TEXT("J_Sec_Hair1_02")),FName(TEXT("J_Sec_Hair1_03"))})
        if (Body->GetBoneIndex(Bone)!=INDEX_NONE) Hair->SetArrayField(Bone.ToString(),V3(Body->GetSocketLocation(Bone)));
    R->SetObjectField(TEXT("spring_hair_world_positions_cm"),Hair);
    TArray<TSharedPtr<FJsonValue>> Mats;
    for (int32 I=0;I<Body->GetNumMaterials();++I) Mats.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Body->GetMaterial(I))));
    R->SetArrayField(TEXT("materials"),Mats); return R;
}

bool AHCM5VS2NPCReviewDirector::BeginPhase(int32 Index,FString& Error)
{
    if (bStopped || !ReviewCamera) return false;
    RestoreExercise(); Phase=Index; PhaseSeconds=0; Frames=0;
    for (AHCM5VS2NPC* NPC:Specimens) { NPC->NPCFace->SetEmotion(TEXT("Neutral")); NPC->NPCFace->bAutomaticBlink=bCastPortraits; }
    if (SceneIdleReview())
    {
        if (Index<0 || Index>5) {Error=TEXT("Invalid bounded scene idle phase");return false;}
        auto* NPC=Specimens[Index/2].Get();auto* Body=NPC->GetMesh();
        FBox Box=Body->Bounds.GetBox();
        if (const auto* Asset=Body->GetSkeletalMeshAsset())Box+=Asset->GetBounds().GetBox().TransformBy(Body->GetComponentTransform());
        const FVector Focus(NPC->GetActorLocation().X,NPC->GetActorLocation().Y,(Box.Min.Z+Box.Max.Z)*.5);
        const FVector Offset=Index/2==0?NPC->GetActorForwardVector()*500+FVector(0,0,70)
            :(NPC->GetActorForwardVector()*.82+NPC->GetActorRightVector()*.57).GetSafeNormal()*570+FVector(0,0,40);
        PhaseLabel=SpecimenIds[Index/2].ToString()+FString::Printf(TEXT("_Context_%d"),Index%2);
        ReviewCamera->SetActorLocation(Focus+Offset);ReviewCamera->SetActorRotation((-Offset).Rotation());
        Controller->SetViewTarget(ReviewCamera);Write(TEXT("RUNNING"),PhaseLabel);return true;
    }
    if (QSkinComparison())
    {
        if (Index<0 || Index>1 || EyeSamples.Num()!=1) {Error=TEXT("Invalid Q skin comparison phase");return false;}
        auto* NPC=Specimens[0].Get();auto* Body=NPC->GetMesh();
        if (!Body->GetSkeletalMeshAsset()->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_Q/Source_")))
        {Error=TEXT("Q source mesh required");return false;}
        FString CandidateRoot=TEXT("/Game/HarborCity/M5VS2/NPCSkinCandidates/Q_af19e4166fd9");
        FParse::Value(FCommandLine::Get(),TEXT("M5QSkinCandidate="),CandidateRoot);
        if (!CandidateRoot.StartsWith(TEXT("/Game/HarborCity/M5VS2/NPCSkinCandidates/Q_")) || CandidateRoot.Contains(TEXT("..")))
        {Error=TEXT("Private Q skin candidate root required");return false;}
        for (int32 Part=0;Part<2;++Part)
        {
            const FString Name=Part==0?TEXT("Face"):TEXT("Body");
            const FString Path=Index==0
                ? TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_Q/Source_36c131cef1f2/MI_N00_000_00_")+Name+TEXT("_00_SKIN__Instance_")
                : CandidateRoot+TEXT("/MI_Q_")+Name+TEXT("_WarmY");
            auto* Material=LoadObject<UMaterialInterface>(nullptr,*Path);
            if (!Material) {Error=TEXT("Actual saved Q comparison material unavailable");return false;}
            Body->SetMaterial(Part==0?3:7,Material);
        }
        // Same warmed pose and camera; this isolated art probe changes no source asset or light.
        NPC->SetActorTickEnabled(false);Body->bPauseAnims=true;
        PhaseLabel=Index==0?TEXT("Q_Harbor_Original"):TEXT("Q_Harbor_WarmSkinCandidate");
        const FVector Offset=NPC->GetActorForwardVector()*150.f;
        ReviewCamera->SetActorLocation(EyeSamples[0]+Offset);ReviewCamera->SetActorRotation((-Offset).Rotation());
        Controller->SetViewTarget(ReviewCamera);Write(TEXT("RUNNING"),PhaseLabel);return true;
    }
    if (bCastPortraits)
    {
        if (!Specimens.IsValidIndex(Phase/2)) { Error=TEXT("Invalid bounded cast portrait index"); return false; }
        AHCM5VS2NPC* NPC=Specimens[Phase/2];
        const bool Face=Phase%2==0;
        PhaseLabel=SpecimenIds[Phase/2].ToString()+(Face?TEXT("_Face_Neutral"):TEXT("_FullBody"));
        FVector Focus=EyeSamples[Phase/2], Offset=NPC->GetActorForwardVector()*150.f;
        if (!Face)
        {
            // Physics-derived component bounds omit decorative ears/hair. Include
            // the imported render bounds so a full-body portrait keeps them in frame.
            auto* PortraitBody=NPC->GetMesh();
            FBox Box=PortraitBody->Bounds.GetBox();
            if (const auto* PortraitMesh=PortraitBody->GetSkeletalMeshAsset())
                Box+=PortraitMesh->GetBounds().GetBox().TransformBy(PortraitBody->GetComponentTransform());
            const float Height=Box.GetSize().Z;
            if (!Box.IsValid || !FMath::IsFinite(Height) || Height<100.f || Height>280.f)
            { Error=TEXT("Actual full-body bounds outside bounded adult specimen range"); return false; }
            Focus=FVector(NPC->GetActorLocation().X,NPC->GetActorLocation().Y,(Box.Min.Z+Box.Max.Z)*.5);
            // 40-degree horizontal FOV at 16:9: fit actual animated height with 15% head/foot room.
            const float Distance=(Height*.575f)/(FMath::Tan(FMath::DegreesToRadians(20.f))/(16.f/9.f));
            Offset=(NPC->GetActorForwardVector()*.94f+NPC->GetActorRightVector()*.342f).GetSafeNormal()*Distance;
        }
        ReviewCamera->SetActorLocation(Focus+Offset);ReviewCamera->SetActorRotation((-Offset).Rotation());
        Controller->SetViewTarget(ReviewCamera);Write(TEXT("RUNNING"),PhaseLabel);return true;
    }
    if (Phase<16)
    {
        const int32 Person=Phase/8, Pose=Phase%8; AHCM5VS2NPC* NPC=Specimens[Person];
        PhaseLabel=FString::Printf(TEXT("%s_Face_%s"),Person==0?TEXT("Q"):TEXT("R"),*Emotions[Pose].ToString());
        if (Pose==1) NPC->NPCFace->bAutomaticBlink=true; else NPC->NPCFace->SetEmotion(Emotions[Pose]);
        const FVector Front=NPC->GetActorForwardVector();
        ReviewCamera->SetActorLocation(EyeSamples[Person]+Front*120.f);
        ReviewCamera->SetActorRotation(FRotator(0,NPC->GetActorRotation().Yaw+180.f,0));
    }
    else
    {
        const FVector Center=(Specimens[0]->GetActorLocation()+Specimens[1]->GetActorLocation())*.5;
        ReviewCamera->SetActorLocation(FVector(Center.X+620,Center.Y,100)); ReviewCamera->SetActorRotation(FRotator(0,180,0));
        if (Phase==16) PhaseLabel=TEXT("Group_FullHeight");
        if (Phase==17)
        {
            PhaseLabel=TEXT("Group_CharacterMovement_FunctionExercise"); MoveStarts.Reset(); PeakSpeed=0; bMoving=true;
            ReviewCamera->SetActorLocation(FVector(Center.X+900,Center.Y,100));
            for (AHCM5VS2NPC* NPC:Specimens) { MoveStarts.Add(NPC->GetActorLocation()); NPC->SetActorTickEnabled(false); }
        }
        if (Phase==18 || Phase==19)
        {
            PhaseLabel=Phase==18?TEXT("Q_Physics_FunctionExercise"):TEXT("R_Physics_FunctionExercise");
            if (!BeginPhysics(Phase-18,Error)) return false;
            if (bPhysicsOnly)
            {
                const FVector Focus(Specimens[Phase-18]->GetActorLocation().X+70,Specimens[Phase-18]->GetActorLocation().Y,75);
                const FVector CameraPosition=Focus+FVector(490,-260,180);
                ReviewCamera->SetActorLocation(CameraPosition); ReviewCamera->SetActorRotation((Focus-CameraPosition).Rotation());
            }
        }
    }
    Controller->SetViewTarget(ReviewCamera); Write(TEXT("RUNNING"),PhaseLabel); return true;
}

bool AHCM5VS2NPCReviewDirector::BeginPhysics(int32 Index,FString& Error)
{
    AHCM5VS2NPC* NPC=Specimens[Index]; USkeletalMeshComponent* Body=NPC->GetMesh();
    if (!Body->GetPhysicsAsset() || Body->GetPhysicsAsset()->FindBodyIndex(NPC->GetPhysicalRootBone())==INDEX_NONE)
    { Error=TEXT("Mapped humanoid root body missing"); return false; }
    PhysicsIndex=Index; PhysicsMeshRelative=Body->GetRelativeTransform(); PhysicsMeshCollision=Body->GetCollisionEnabled();
    PhysicsCaptureIndex=bPhysicsSettledOnly?7:0; PhysicsSimulationStart=GetWorld()->GetTimeSeconds(); PhysicsFrameSeconds.Reset();
    PhysicsViewCaptureIndex=INDEX_NONE; PhysicsViewChangedFrame=0;
    bPhysicsOriginalPostProcessDisabled=Body->GetDisablePostProcessBlueprint();
    if (bPhysicsDisablePostProcess) Body->SetDisablePostProcessBlueprint(true);
    PhysicsObjectType=Body->GetCollisionObjectType(); PhysicsResponses=Body->GetCollisionResponseToChannels();
    PhysicsCapsuleCollision=NPC->GetCapsuleComponent()->GetCollisionEnabled();
    NPC->SetActorTickEnabled(false); NPC->GetCharacterMovement()->StopMovementImmediately(); NPC->GetCharacterMovement()->DisableMovement();
    NPC->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCollisionObjectType(ECC_PhysicsBody); Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    if (NPC->ShouldBlockPhysicsBodiesDuringRagdoll()) Body->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block); Body->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Body->SetSimulatePhysics(true); Body->SetAllBodiesPhysicsBlendWeight(1); Body->WakeAllRigidBodies();
    if (!Body->IsSimulatingPhysics()) { Error=TEXT("Native humanoid bodies did not start simulation"); return false; }
    FString Case=TEXT("HipImpulse");
    if (bPhysicsOnly) FParse::Value(FCommandLine::Get(),TEXT("M5VS2NPCPhysicsCase="),Case);
    if (Case==TEXT("HipImpulse")) Body->AddImpulse(FVector(110,0,40),NPC->GetPhysicalRootBone(),true);
    else if (Case==TEXT("DistributedFront") || Case==TEXT("DistributedSide"))
    {
        // Repeatable component-level impulses only; not an actual vehicle-contact test.
        const FVector Direction=Case==TEXT("DistributedFront")?-NPC->GetActorForwardVector():NPC->GetActorRightVector();
        Body->SetAllPhysicsLinearVelocity(Direction*260.f+FVector(0,0,90));
    }
    else { Error=TEXT("Unrecognized bounded physics case"); return false; }
    Exercises->SetStringField(TEXT("physics_case"),Case);
    Exercises->SetStringField(TEXT("physics_case_scope"),TEXT("Direct component simulation and initial velocity; not vehicle-contact or input acceptance"));
    return true;
}

void AHCM5VS2NPCReviewDirector::RestoreExercise()
{
    if (bStopped) return;
    if (bMoving)
    {
        TArray<TSharedPtr<FJsonValue>> Distances;
        for (int32 I=0;I<Specimens.Num();++I)
        { Distances.Add(MakeShared<FJsonValueNumber>(FVector::Dist2D(MoveStarts[I],Specimens[I]->GetActorLocation()))); Specimens[I]->GetCharacterMovement()->StopMovementImmediately(); Specimens[I]->SetActorTickEnabled(OriginalActorTicks[I]); }
        Exercises->SetArrayField(TEXT("character_movement_distance_cm"),Distances); Exercises->SetNumberField(TEXT("peak_speed_cm_s"),PeakSpeed);
        Exercises->SetStringField(TEXT("movement_scope"),TEXT("Native AddMovementInput/CharacterMovement component exercise; NPC decision tick suspended; not OS input or navigation/pathfinding acceptance."));
        bMoving=false;
    }
    if (PhysicsIndex!=INDEX_NONE)
    {
        AHCM5VS2NPC* NPC=Specimens[PhysicsIndex]; USkeletalMeshComponent* Body=NPC->GetMesh();
        Exercises->SetObjectField(PhysicsIndex==0?TEXT("Q_physics_final"):TEXT("R_physics_final"),Snapshot(NPC));
        auto Timing=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Durations;
        for (double Seconds:PhysicsFrameSeconds) Durations.Add(MakeShared<FJsonValueNumber>(Seconds));
        Timing->SetArrayField(TEXT("game_tick_delta_seconds_before_first_capture"),Durations);
        Timing->SetStringField(TEXT("scope"),TEXT("Game tick deltas while freely falling before first capture, not Chaos substep telemetry."));
        Exercises->SetObjectField(PhysicsIndex==0?TEXT("Q_fall_frame_timing"):TEXT("R_fall_frame_timing"),Timing);
        Body->SetSimulatePhysics(false); Body->SetAllBodiesPhysicsBlendWeight(0);
        if (bPhysicsDisablePostProcess) Body->SetDisablePostProcessBlueprint(bPhysicsOriginalPostProcessDisabled);
        Body->AttachToComponent(NPC->GetCapsuleComponent(),FAttachmentTransformRules::KeepRelativeTransform); Body->SetRelativeTransform(PhysicsMeshRelative);
        Body->SetCollisionObjectType(PhysicsObjectType); Body->SetCollisionResponseToChannels(PhysicsResponses); Body->SetCollisionEnabled(PhysicsMeshCollision);
        NPC->GetCapsuleComponent()->SetCollisionEnabled(PhysicsCapsuleCollision); NPC->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        NPC->SetActorTickEnabled(OriginalActorTicks[PhysicsIndex]); PhysicsIndex=INDEX_NONE;
        Exercises->SetStringField(TEXT("physics_scope"),TEXT("Native simulation/impulse on saved humanoid asset, then explicit test restoration. Vehicle contact, damage, automatic recovery, death and respawn NOT_RUN."));
    }
}

void AHCM5VS2NPCReviewDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (bStopped) return;
    const double Now=FPlatformTime::Seconds();
    if (bExitPending)
    { if (Now-Finished>=2 && !FScreenshotRequest::IsScreenshotRequested()) { bExitPending=false; FPlatformMisc::RequestExit(false,TEXT("M5VS2 NPC review finished")); } return; }
    if (!bActive) return;
    if (Now-Started>240) { Finish(TEXT("FAIL"),TEXT("Bounded 240 second review deadline")); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    if (bPending)
    {
        if (bProcessed && !FScreenshotRequest::IsScreenshotRequested() && GFrameCounter>CaptureFrame)
        {
            int32 W=0,H=0; const bool Valid=PNG(PendingPNG,W,H) && W==1920 && H==1080;
            Pending->SetStringField(TEXT("status"),Valid?TEXT("PASS"):TEXT("FAIL")); Pending->SetNumberField(TEXT("width"),W); Pending->SetNumberField(TEXT("height"),H);
            Pending->SetNumberField(TEXT("file_bytes"),IFileManager::Get().FileSize(*PendingPNG)); Rows.Add(Pending); Pending.Reset(); bPending=false;
            if (!Valid) { Finish(TEXT("FAIL"),TEXT("Missing or invalid native 1920x1080 PNG")); return; }
            if (bPhysicsOnly && ++PhysicsCaptureIndex<UE_ARRAY_COUNT(PhysicsCaptureTimes)) return;
            if (bCastPortraits && Phase==15)
            {
                bool Blinked=CastExpressionRows.Num()==24;
                for (float Peak:CastPeakBlink) Blinked&=Peak>=.95f;
                Finish(Blinked?TEXT("PASS"):TEXT("FAIL"),Blinked?TEXT("16 actual face/full-body PNGs for eight independent sources, 3 native expressions and automatic blink per specimen; visual art approval remains USER_REVIEW")
                    :TEXT("Cast expression or automatic blink coverage incomplete"));return;
            }
            if (!bCastPortraits && Phase==19) { Finish(TEXT("PASS"),bPhysicsOnly?(bPhysicsSettledOnly?TEXT("6 settled raw physics views after uninterrupted fall; physical pose quality requires image review"):TEXT("20 raw physics progression/multiple-view captures completed; physical pose quality requires image review")):TEXT("20 raw viewport captures and bounded native component exercises completed; visual quality remains USER_REVIEW")); return; }
            if (QSkinComparison() && Phase==1) {Finish(TEXT("PASS"),TEXT("Two native harbor Q skin captures; same actor, warmed pose, camera and lighting; art approval remains USER_REVIEW"));return;}
            if (SceneIdleReview() && Phase==5) {Finish(TEXT("PASS"),TEXT("Six native contextual idle samples; saved animation graph playback, not gameplay or art approval"));return;}
            FString Error; if (!BeginPhase(Phase+1,Error)) Finish(TEXT("FAIL"),Error);
        }
        else if (Now-Requested>15) Finish(TEXT("FAIL"),TEXT("Native screenshot processing timeout"));
        return;
    }
    if (!bWarm)
    {
        if (Now-Started<10) return;
#if WITH_EDITOR
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
        for (AHCM5VS2NPC* NPC:Specimens)
        {
            if (!NPC->IsVisualProfileReady() || !NPC->NPCFace->IsFaceReady()) { Finish(TEXT("FAIL"),TEXT("Actual NPC visual/profile/face readiness failed after warmup")); return; }
            const FName Left=NPC->NPCFace->GetHumanoidBone(TEXT("leftEye")), Right=NPC->NPCFace->GetHumanoidBone(TEXT("rightEye"));
            if (Left.IsNone() || Right.IsNone() || NPC->GetMesh()->GetBoneIndex(Left)==INDEX_NONE || NPC->GetMesh()->GetBoneIndex(Right)==INDEX_NONE)
            { Finish(TEXT("FAIL"),TEXT("Actual mapped eye bones missing")); return; }
            const FVector Eyes=(NPC->GetMesh()->GetSocketLocation(Left)+NPC->GetMesh()->GetSocketLocation(Right))*.5;
            if (Eyes.ContainsNaN()) { Finish(TEXT("FAIL"),TEXT("Invalid animated eye sample")); return; } EyeSamples.Add(Eyes);
        }
        SampleFrame=GFrameCounter; bWarm=true;
        if (bCastPortraits)
        {
            PhaseSeconds=0;Frames=0;
            for (AHCM5VS2NPC* NPC:Specimens) { NPC->NPCFace->bAutomaticBlink=false;NPC->NPCFace->SetEmotion(CastProbeEmotions[0]); }
            return;
        }
        FString Error; if (!BeginPhase(bPhysicsOnly?18:0,Error)) Finish(TEXT("FAIL"),Error); return;
    }
    PhaseSeconds+=FMath::Max(0.f,DeltaSeconds); ++Frames;
    if (bCastPortraits)
    {
        if (!bCastProbesDone)
        {
            if (PhaseSeconds<.75 || Frames<5) return;
            FString Error;if (!ReadCastExpression(Error)) { Finish(TEXT("FAIL"),Error);return; }
            ++CastProbeIndex;PhaseSeconds=0;Frames=0;
            if (CastProbeIndex<UE_ARRAY_COUNT(CastProbeEmotions))
                for (AHCM5VS2NPC* NPC:Specimens) NPC->NPCFace->SetEmotion(CastProbeEmotions[CastProbeIndex]);
            else { bCastProbesDone=true;if (!BeginPhase(0,Error)) Finish(TEXT("FAIL"),Error); }
            return;
        }
        for (int32 I=0;I<Specimens.Num();++I) CastPeakBlink[I]=FMath::Max(CastPeakBlink[I],Specimens[I]->NPCFace->GetBlinkWeight());
        if (PhaseSeconds<2 || Frames<30 || FScreenshotRequest::IsScreenshotRequested()) return;
        if (Specimens[Phase/2]->NPCFace->GetBlinkWeight()>.05f) return;
        if (!Controller->PlayerCameraManager || FVector::Dist(Controller->PlayerCameraManager->GetCameraLocation(),ReviewCamera->GetActorLocation())>.5f
            || FMath::RadiansToDegrees(Controller->PlayerCameraManager->GetCameraRotation().Quaternion().AngularDistance(ReviewCamera->GetActorQuat()))>.1f)
        { Finish(TEXT("FAIL"),TEXT("Actual PlayerCameraManager is not the requested cast camera"));return; }
        Capture();return;
    }
    if (PhysicsIndex!=INDEX_NONE && bPhysicsSettledOnly && PhysicsCaptureIndex==7 && PhysicsFrameSeconds.Num()<1000) PhysicsFrameSeconds.Add(DeltaSeconds);
    if (bMoving)
    {
        for (int32 I=0;I<Specimens.Num();++I)
        {
            AHCM5VS2NPC* NPC=Specimens[I]; const float Distance=FVector::Dist2D(MoveStarts[I],NPC->GetActorLocation());
            if (Distance>350 || NPC->GetActorLocation().ContainsNaN()) { Finish(TEXT("FAIL"),TEXT("Movement exercise exceeded 350 cm bound")); return; }
            if (PhaseSeconds<2) NPC->AddMovementInput(NPC->GetActorForwardVector(),1.f,true);
            PeakSpeed=FMath::Max(PeakSpeed,float(NPC->GetVelocity().Size2D()));
        }
    }
    if (PhysicsIndex!=INDEX_NONE)
    {
        AHCM5VS2NPC* NPC=Specimens[PhysicsIndex]; const FVector Root=NPC->GetMesh()->GetSocketLocation(NPC->GetPhysicalRootBone());
        if (Root.ContainsNaN() || FVector::Dist(Root,NPC->GetActorLocation())>500)
        { Finish(TEXT("FAIL"),TEXT("Physics exercise exceeded finite 500 cm bound")); return; }
    }
    if (bPhysicsOnly)
    {
        if (GetWorld()->GetTimeSeconds()-PhysicsSimulationStart<PhysicsCaptureTimes[PhysicsCaptureIndex] || FScreenshotRequest::IsScreenshotRequested()) return;
        if (PhysicsCaptureIndex>=8)
        {
            if (PhysicsViewCaptureIndex!=PhysicsCaptureIndex)
            {
                const auto* NPC=Specimens[PhysicsIndex].Get(); const auto* Mesh=NPC->GetMesh();
                const FVector Focus=(Mesh->GetSocketLocation(NPC->GetPhysicalRootBone())+Mesh->GetSocketLocation(NPC->NPCProfile->HumanoidBones.FindChecked(TEXT("head"))))*.5;
                const FVector Offset=PhysicsCaptureIndex==8?FVector(320,-340,170):FVector(-320,340,110);
                ReviewCamera->SetActorLocation(Focus+Offset); ReviewCamera->SetActorRotation((-Offset).Rotation());
                PhysicsViewCaptureIndex=PhysicsCaptureIndex; PhysicsViewChangedFrame=GFrameCounter;
                return; // PlayerCameraManager has already updated this frame.
            }
            if (GFrameCounter<=PhysicsViewChangedFrame+1) return;
        }
        Capture(); return;
    }
    if (Frames<30 || PhaseSeconds<2 || FScreenshotRequest::IsScreenshotRequested()) return;
    if (!QSkinComparison() && !SceneIdleReview() && Phase<16 && Phase%8==1)
    {
        if (PhaseSeconds>12) { Finish(TEXT("FAIL"),TEXT("Automatic blink did not reach closed hold within bounded interval")); return; }
        if (Specimens[Phase/8]->NPCFace->GetBlinkWeight()<.95f) return;
    }
    if (Phase==17 && PeakSpeed<30) { Finish(TEXT("FAIL"),TEXT("CharacterMovement produced no meaningful actual velocity")); return; }
    Capture();
}

void AHCM5VS2NPCReviewDirector::Capture()
{
    if (bStopped) return;
    PendingPNG=Directory/(bPhysicsOnly?FString::Printf(TEXT("%02d_%02d_%s.png"),Phase,PhysicsCaptureIndex,*PhaseLabel):FString::Printf(TEXT("%02d_%s.png"),Phase,*PhaseLabel));
    if (IFileManager::Get().FileExists(*PendingPNG)) { Finish(TEXT("FAIL"),TEXT("Refuse screenshot overwrite")); return; }
    Pending=MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("label"),PhaseLabel); Pending->SetStringField(TEXT("file"),PendingPNG);
    Pending->SetNumberField(TEXT("request_frame"),double(GFrameCounter)); Pending->SetNumberField(TEXT("eye_sample_frame"),double(SampleFrame));
    if (SceneIdleReview())
    {
        auto* NPC=Specimens[Phase/2].Get();auto* Body=NPC->GetMesh();
        Pending->SetStringField(TEXT("subject"),SpecimenIds[Phase/2].ToString());
        Pending->SetStringField(TEXT("animation_instance"),GetPathNameSafe(Body->GetAnimInstance()));
        Pending->SetArrayField(TEXT("left_hand_world_cm"),V3(Body->GetSocketLocation(TEXT("J_Bip_L_Hand"))));
        Pending->SetArrayField(TEXT("right_hand_world_cm"),V3(Body->GetSocketLocation(TEXT("J_Bip_R_Hand"))));
    }
    if (bCastPortraits)
    {
        Pending->SetStringField(TEXT("subject"),SpecimenIds[Phase/2].ToString());
        Pending->SetStringField(TEXT("framing"),Phase%2==0?TEXT("FACE"):TEXT("FULL_BODY"));
        Pending->SetStringField(TEXT("per_material_render_proxy_readiness"),TEXT("NOT_RUN; global shader compiler idle and actual material references only"));
        Pending->SetArrayField(TEXT("final_camera_location_cm"),V3(Controller->PlayerCameraManager->GetCameraLocation()));
        const auto ViewRot=Controller->PlayerCameraManager->GetCameraRotation();
        Pending->SetArrayField(TEXT("final_camera_pitch_yaw_roll"),V3(FVector(ViewRot.Pitch,ViewRot.Yaw,ViewRot.Roll)));
        Pending->SetNumberField(TEXT("final_camera_fov"),Controller->PlayerCameraManager->GetFOVAngle());
    }
    if (PhysicsIndex!=INDEX_NONE)
    {
        Pending->SetNumberField(TEXT("actual_physics_simulation_seconds"),GetWorld()->GetTimeSeconds()-PhysicsSimulationStart);
        Pending->SetStringField(TEXT("time_measurement_scope"),TEXT("World game time since simulation enabled, not accumulated Chaos solver time; screenshot stalls affect frame spacing."));
        if (bPhysicsOnly) Pending->SetNumberField(TEXT("requested_physics_simulation_seconds"),PhysicsCaptureTimes[PhysicsCaptureIndex]);
    }
    Pending->SetArrayField(TEXT("camera_location_cm"),V3(ReviewCamera->GetActorLocation())); const FRotator Rot=ReviewCamera->GetActorRotation();
    Pending->SetArrayField(TEXT("camera_pitch_yaw_roll"),V3(FVector(Rot.Pitch,Rot.Yaw,Rot.Roll))); Pending->SetNumberField(TEXT("fov"),ReviewCamera->GetCameraComponent()->FieldOfView);
    TArray<TSharedPtr<FJsonValue>> NPCs; for (AHCM5VS2NPC* NPC:Specimens) NPCs.Add(MakeShared<FJsonValueObject>(Snapshot(NPC))); Pending->SetArrayField(TEXT("specimens"),NPCs);
    if (MainLight && MainLight->GetLightComponent())
    { Pending->SetArrayField(TEXT("light_forward"),V3(MainLight->GetActorForwardVector())); Pending->SetStringField(TEXT("light_linear_color"),MainLight->GetLightComponent()->GetLightColor().ToString()); Pending->SetNumberField(TEXT("light_intensity"),MainLight->GetLightComponent()->Intensity); }
    bPending=true; bProcessed=false; CaptureFrame=GFrameCounter; Requested=FPlatformTime::Seconds();
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}

void AHCM5VS2NPCReviewDirector::Processed() { if (!bStopped && bPending) bProcessed=true; }

void AHCM5VS2NPCReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if ((bActive || bExitPending) && !bStopped && Event.Event==IE_Pressed && Event.Key==EKeys::Escape)
    {
        bStopped=true; bActive=false; bExitPending=false; bAutoQuit=false; StopFrame=GFrameCounter;
        if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
        bPending=false; SetActorTickEnabled(false);
        Write(TEXT("NOT_RUN"),TEXT("Viewport Escape latched permanently; no subsequent director camera, expression, movement-input, impulse, capture, restoration or auto-exit actions. Event not consumed."));
    }
}

void AHCM5VS2NPCReviewDirector::Finish(const FString& Status,const FString& Detail)
{
    if (bStopped) return;
    bActive=false; RestoreExercise();
    if (bPending && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::GetFilename()==PendingPNG) FScreenshotRequest::Reset();
    bPending=false;
    for (int32 I=0;I<Specimens.Num();++I) if (IsValid(Specimens[I]) && Specimens[I]->NPCFace)
    { Specimens[I]->NPCFace->SetEmotion(TEXT("Neutral")); if (OriginalBlinks.IsValidIndex(I)) Specimens[I]->NPCFace->bAutomaticBlink=OriginalBlinks[I]; }
    if (Controller) { if (OriginalView.IsValid()) Controller->SetViewTarget(OriginalView.Get()); if (AHUD* HUD=Controller->GetHUD()) HUD->bShowHUD=bSavedHUD; }
    Finished=FPlatformTime::Seconds(); bExitPending=bAutoQuit; SetActorTickEnabled(bExitPending);
    Write(Status,Detail); UE_LOG(LogTemp,Display,TEXT("M5VS2_NPC_REVIEW_%s %s"),*Status,*Directory);
}

void AHCM5VS2NPCReviewDirector::Write(const FString& Status,const FString& Detail)
{
    if (Directory.IsEmpty()) return;
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),Status); R->SetStringField(TEXT("detail"),Detail);
    R->SetStringField(TEXT("map"),GetWorld()->GetMapName()); R->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));
    R->SetBoolField(TEXT("os_input_used"),false); R->SetBoolField(TEXT("user_stop_latched"),bStopped); R->SetNumberField(TEXT("stop_frame"),double(StopFrame));
    R->SetBoolField(TEXT("physics_only_progression"),bPhysicsOnly);
    R->SetBoolField(TEXT("cast_portraits"),bCastPortraits);
    R->SetBoolField(TEXT("q_skin_harbor_comparison"),QSkinComparison());
    R->SetBoolField(TEXT("scene_idle_review"),SceneIdleReview());
    if (bCastPortraits)
    {
        TArray<TSharedPtr<FJsonValue>> ExpressionEvidence,Blinks;
        for (const auto& Row:CastExpressionRows) ExpressionEvidence.Add(MakeShared<FJsonValueObject>(Row));
        for (int32 I=0;I<CastPeakBlink.Num();++I)
        { auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("subject"),SpecimenIds[I].ToString());Row->SetNumberField(TEXT("peak_automatic_blink_weight"),CastPeakBlink[I]);Blinks.Add(MakeShared<FJsonValueObject>(Row)); }
        R->SetArrayField(TEXT("three_native_expression_readbacks_per_person"),ExpressionEvidence);
        R->SetArrayField(TEXT("automatic_blink_observations"),Blinks);
        R->SetStringField(TEXT("cast_scope"),TEXT("Eight unique source face/full-body art review in neutral proxy lighting; 3 SetEmotion function probes and natural blink observations. No dialogue, navigation, ragdoll or OS-input acceptance."));
    }
    if (const auto* Cap=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) R->SetNumberField(TEXT("actual_preview_fps_cap"),Cap->GetFloat());
    R->SetBoolField(TEXT("test_disables_postprocess_during_physics"),bPhysicsDisablePostProcess);
    R->SetNumberField(TEXT("eye_sample_frame"),double(SampleFrame)); R->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);
    R->SetStringField(TEXT("test_scope"),TEXT("Native viewport and component function tests. No claim of mouse/keyboard navigation, vehicle contacts, gameplay recovery or street integration."));
    TArray<TSharedPtr<FJsonValue>> Captures; for (const auto& Row:Rows) Captures.Add(MakeShared<FJsonValueObject>(Row)); R->SetArrayField(TEXT("captures"),Captures);
    if (Exercises) R->SetObjectField(TEXT("component_exercises"),Exercises);
    FString Text; FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("npc_review.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool AHCM5VS2NPCReviewDirector::ReadCastExpression(FString& Error)
{
    for (int32 I=0;I<Specimens.Num();++I)
    {
        auto* NPC=Specimens[I].Get();auto Row=MakeShared<FJsonObject>();auto Weights=MakeShared<FJsonObject>();
        bool Found=false,Valid=NPC->NPCFace->GetEffectiveEmotion()==CastProbeEmotions[CastProbeIndex];
        for (const auto& Group:NPC->NPCProfile->FaceGroups) if (Group.Group==CastProbeGroups[CastProbeIndex])
            for (const auto& Bind:Group.Binds)
            {
                const float Actual=NPC->GetMesh()->GetMorphTarget(Bind.Morph);
                Weights->SetNumberField(Bind.Morph.ToString(),Actual);
                if (Bind.Weight>.05f) { Found=true;Valid&=FMath::IsFinite(Actual) && Actual>=Bind.Weight*.8f; }
            }
        Row->SetStringField(TEXT("subject"),SpecimenIds[I].ToString());Row->SetStringField(TEXT("emotion"),CastProbeEmotions[CastProbeIndex].ToString());
        Row->SetStringField(TEXT("mesh"),GetPathNameSafe(NPC->GetMesh()->GetSkeletalMeshAsset()));
        Row->SetObjectField(TEXT("actual_model_morph_weights"),Weights);Row->SetNumberField(TEXT("frame"),double(GFrameCounter));
        Row->SetBoolField(TEXT("actual_bound_group_responded"),Found&&Valid);CastExpressionRows.Add(Row);
        if (!Found || !Valid) { Error=TEXT("Actual model-specific expression group failed: ")+SpecimenIds[I].ToString();return false; }
    }
    return true;
}

void AHCM5VS2NPCReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (!bStopped) RestoreExercise();
    if (Viewport.IsValid() && InputHandle.IsValid()) Viewport->OnInputKey().Remove(InputHandle);
    if (ScreenshotHandle.IsValid()) FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    Super::EndPlay(Reason);
}
