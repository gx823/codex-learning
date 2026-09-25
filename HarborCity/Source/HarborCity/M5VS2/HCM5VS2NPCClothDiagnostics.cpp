#include "HCM5VS2NPCClothDiagnostics.h"

#include "AnimNode_VrmSpringBone.h"
#include "VrmMetaObject.h"
#include "Animation/AnimInstance.h"
#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "ClothingSystemRuntimeTypes.h"
#include "ClothingAssetBase.h"
#include "ClothingAsset.h"
#include "ClothPhysicalMeshData.h"
#include "PointWeightMap.h"
#include "ClothingSimulationInstance.h"
#include "ClothingSimulationInterface.h"
#include "Engine/OverlapResult.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace HCM5VS2JClothPrivate
{
constexpr int32 MaxAnchors = 384;
const TCHAR* JMesh = TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Source_8b6562a56a4a/SK_AvatarSample_J_Studio2140");
const TCHAR* JPhysics = TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Runtime_0f9dd6fb5a/PHYS_NPC_Humanoid");
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; }
FString ClothJSON(const TSharedRef<FJsonObject>& R)
{ FString S; auto W=TJsonWriterFactory<>::Create(&S); FJsonSerializer::Serialize(R,W); return S; }
bool GarmentBone(const FString& N)
{ return N.StartsWith(TEXT("J_Sec_")) && (N.Contains(TEXT("Skirt")) || N.Contains(TEXT("Coat")) || N.Contains(TEXT("Sleeve"))); }
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> A; for(const auto& V:Values) A.Add(MakeShared<FJsonValueString>(V)); return A; }
TSharedPtr<FJsonObject> Meta(const UVrmMetaObject* M)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),M ? TEXT("PASS_READBACK") : TEXT("NOT_AVAILABLE"));
    R->SetStringField(TEXT("path"),GetPathNameSafe(M)); if(!M) return R;
    R->SetNumberField(TEXT("version"),M->GetVRMVersion());
    R->SetNumberField(TEXT("total_spring_groups"),M->VRMSpringMeta.Num());
    TArray<TSharedPtr<FJsonValue>> Groups,Colliders;
    for(int32 I=0;I<M->VRMSpringMeta.Num();++I)
    {
        const auto& G=M->VRMSpringMeta[I]; bool IsClothes=false;
        for(const auto& Name:G.boneNames) IsClothes|=GarmentBone(Name);
        if(!IsClothes) continue;
        auto V=MakeShared<FJsonObject>(); V->SetNumberField(TEXT("index"),I);
        V->SetArrayField(TEXT("bone_names"),Strings(G.boneNames));
        V->SetNumberField(TEXT("stiffness"),G.stiffness); V->SetNumberField(TEXT("gravity_power"),G.gravityPower);
        V->SetArrayField(TEXT("gravity_direction"),XYZ(G.gravityDir)); V->SetNumberField(TEXT("drag_force"),G.dragForce);
        V->SetNumberField(TEXT("hit_radius_source_m"),G.hitRadius);
        V->SetNumberField(TEXT("world_trace_radius_cm_before_component_scale"),G.hitRadius*100.0);
        TArray<TSharedPtr<FJsonValue>> IDs; for(int32 ID:G.ColliderIndexArray) IDs.Add(MakeShared<FJsonValueNumber>(ID));
        V->SetArrayField(TEXT("collider_group_indices"),IDs); Groups.Add(MakeShared<FJsonValueObject>(V));
    }
    for(int32 I=0;I<M->VRMColliderMeta.Num();++I)
    {
        const auto& C=M->VRMColliderMeta[I]; auto V=MakeShared<FJsonObject>();
        V->SetNumberField(TEXT("index"),I); V->SetStringField(TEXT("bone"),C.boneName);
        TArray<TSharedPtr<FJsonValue>> Spheres;
        for(const auto& C0:C.collider)
        { auto S=MakeShared<FJsonObject>(); S->SetArrayField(TEXT("offset_source_units"),XYZ(C0.offset)); S->SetNumberField(TEXT("radius_source_units"),C0.radius); Spheres.Add(MakeShared<FJsonValueObject>(S)); }
        V->SetArrayField(TEXT("spheres"),Spheres); Colliders.Add(MakeShared<FJsonValueObject>(V));
    }
    R->SetArrayField(TEXT("garment_groups"),Groups); R->SetArrayField(TEXT("collider_groups"),Colliders);
    return R;
}
TSharedPtr<FJsonObject> Nodes(USkeletalMeshComponent* Mesh,bool Runtime)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),TEXT("NOT_AVAILABLE"));
    R->SetBoolField(TEXT("postprocess_disabled"),Mesh->GetDisablePostProcessBlueprint());
    UAnimInstance* Instance=Mesh->GetPostProcessInstance();
    R->SetStringField(TEXT("runtime_instance"),GetPathNameSafe(Instance));
    const auto Class=Mesh->GetPostProcessAnimBPClassToBeUsed();
    R->SetStringField(TEXT("selected_postprocess_class"),GetPathNameSafe(Class.Get()));
    if(!Instance && !Runtime && Class) Instance=Class.GetDefaultObject();
    R->SetStringField(TEXT("read_scope"),Instance && !Instance->HasAnyFlags(RF_ClassDefaultObject)
        ? TEXT("ACTUAL_RUNTIME_INSTANCE") : TEXT("CDO_DEFAULTS_ONLY_NOT_RUNTIME"));
    if(!Instance) return R;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(TFieldIterator<FStructProperty> P(Instance->GetClass(),EFieldIteratorFlags::IncludeSuper);P;++P)
    {
        if(P->Struct!=FAnimNode_VrmSpringBone::StaticStruct()) continue;
        const auto* N=P->ContainerPtrToValuePtr<FAnimNode_VrmSpringBone>(Instance);
        auto V=MakeShared<FJsonObject>(); V->SetStringField(TEXT("property"),P->GetName());
        V->SetBoolField(TEXT("auto_search_meta"),N->EnableAutoSearchMetaData);
        V->SetBoolField(TEXT("ignore_physics_collision"),N->bIgnorePhysicsCollision);
        V->SetBoolField(TEXT("ignore_vrm_collision"),N->bIgnoreVRMCollision);
        V->SetBoolField(TEXT("ignore_physics_reset_on_teleport"),N->bIgnorePhysicsResetOnTeleport);
        V->SetBoolField(TEXT("ignore_wind_directional_source"),N->bIgnoreWindDirectionalSource);
        V->SetNumberField(TEXT("loop_count"),N->loopc); V->SetNumberField(TEXT("collision_check_loop_count"),N->collisionCheckLoopCount);
        V->SetNumberField(TEXT("gravity_scale"),N->gravityScale); V->SetArrayField(TEXT("gravity_add"),XYZ(N->gravityAdd));
        V->SetNumberField(TEXT("stiffness_scale"),N->stiffnessScale); V->SetNumberField(TEXT("stiffness_add"),N->stiffnessAdd);
        V->SetNumberField(TEXT("wind_scale"),N->windScale); V->SetNumberField(TEXT("random_wind_range"),N->randomWindRange);
        V->SetNumberField(TEXT("current_delta_time"),N->CurrentDeltaTime); V->SetBoolField(TEXT("spring_initialized"),N->IsSpringInit());
        V->SetStringField(TEXT("explicit_meta"),GetPathNameSafe(N->VrmMetaObject));
        V->SetStringField(TEXT("resolved_meta_soft_path"),N->VrmMetaObject_Internal.ToSoftObjectPath().ToString());
        const auto* Resolved=N->VrmMetaObject_Internal.Get();
        V->SetObjectField(TEXT("actual_resolved_meta"),Meta(Resolved));
        V->SetObjectField(TEXT("explicit_meta_readback"),Meta(N->VrmMetaObject));
        Rows.Add(MakeShared<FJsonValueObject>(V));
    }
    R->SetNumberField(TEXT("spring_node_count"),Rows.Num()); R->SetArrayField(TEXT("nodes"),Rows);
    R->SetStringField(TEXT("status"),Rows.Num()>0 ? TEXT("PASS_READBACK") : TEXT("NOT_AVAILABLE_NO_SPRING_NODE")); return R;
}
bool Scope(USkeletalMeshComponent* Mesh,TSharedRef<FJsonObject> R)
{
    if(!IsInGameThread() || !Mesh || !Mesh->GetSkeletalMeshAsset()
        || Mesh->GetSkeletalMeshAsset()->GetOutermost()->GetName()!=JMesh
        || !Mesh->GetPhysicsAsset() || Mesh->GetPhysicsAsset()->GetOutermost()->GetName()!=JPhysics)
    { R->SetStringField(TEXT("error"),TEXT("Exact J source mesh and original private humanoid PHYS required on game thread")); return false; }
    TArray<FString> Expected={TEXT("J_Bip_C_Hips"),TEXT("J_Bip_C_Spine"),TEXT("J_Bip_C_Chest"),TEXT("J_Bip_C_UpperChest"),TEXT("J_Bip_C_Neck"),TEXT("J_Bip_C_Head")};
    for(const FString Side:{FString(TEXT("L")),FString(TEXT("R"))})
        for(const FString Part:{FString(TEXT("Shoulder")),FString(TEXT("UpperArm")),FString(TEXT("LowerArm")),FString(TEXT("Hand")),FString(TEXT("UpperLeg")),FString(TEXT("LowerLeg")),FString(TEXT("Foot"))})
            Expected.Add(TEXT("J_Bip_")+Side+TEXT("_")+Part);
    TArray<FString> Actual; for(const auto& B:Mesh->GetPhysicsAsset()->SkeletalBodySetups) if(B) Actual.Add(B->BoneName.ToString());
    Expected.Sort(); Actual.Sort(); R->SetArrayField(TEXT("actual_original_humanoid_bodies"),Strings(Actual));
    R->SetBoolField(TEXT("original_twenty_body_names_exact"),Actual==Expected);
    if(Actual!=Expected) { R->SetStringField(TEXT("error"),TEXT("Original exact 20 body names changed; no cloth probe")); return false; }
    R->SetStringField(TEXT("mesh"),GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
    R->SetStringField(TEXT("physics"),GetPathNameSafe(Mesh->GetPhysicsAsset()));
    if(Mesh->IsRunningParallelEvaluation()) { R->SetStringField(TEXT("error"),TEXT("NOT_AVAILABLE: parallel animation still in flight; probe does not block or alter evaluation")); return false; }
    return true;
}
struct FAnchor { int32 Vertex=INDEX_NONE,Section=INDEX_NONE,Bone=INDEX_NONE; FVector Reference=FVector::ZeroVector; };
bool Anchors(USkeletalMeshComponent* Mesh,const FSkeletalMeshLODRenderData*& LOD,const FSkinWeightVertexBuffer*& Weights,
    TArray<FAnchor>& Out,TSharedRef<FJsonObject> R)
{
    const auto* Data=Mesh->GetSkeletalMeshAsset()->GetResourceForRendering();
    if(!Data || !Data->LODRenderData.IsValidIndex(0)) return false;
    LOD=&Data->LODRenderData[0]; Weights=LOD->GetSkinWeightVertexBuffer();
    if(!LOD->GetNumVertices() || LOD->GetNumVertices()>200000 || !LOD->StaticVertexBuffers.PositionVertexBuffer.GetVertexData()
        || !Weights || Weights->GetNumVertices()!=LOD->GetNumVertices() || !Weights->GetDataVertexBuffer()->GetWeightData()
        || Weights->GetDataVertexBuffer()->GetVertexDataSize()==0
        || (Weights->GetVariableBonesPerVertex() && (!Weights->GetLookupVertexBuffer()->GetLookupData()
            || Weights->GetLookupVertexBuffer()->GetNumVertices()!=LOD->GetNumVertices()))) return false;
    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    TMap<int32,TArray<FAnchor>> Extremes; int32 CandidateCount=0;
    for(int32 S=0;S<LOD->RenderSections.Num();++S)
    {
        const auto& Part=LOD->RenderSections[S]; if(Part.bDisabled) continue;
        if(Part.BaseVertexIndex+Part.NumVertices>LOD->GetNumVertices()) return false;
        for(uint32 V=Part.BaseVertexIndex;V<Part.BaseVertexIndex+Part.NumVertices;++V)
        {
            uint32 Offset=0,Count=0,Total=0,Cloth=0,Highest=0; int32 Dominant=INDEX_NONE;
            Weights->GetVertexInfluenceOffsetCount(V,Offset,Count); if(Count>32 || !Count) return false;
            for(uint32 I=0;I<Count;++I)
            {
                const uint16 W=Weights->GetBoneWeight(V,I); if(!W) continue;
                const uint32 B=Weights->GetBoneIndex(V,I); if(!Part.BoneMap.IsValidIndex(B)) return false;
                const int32 Bone=Part.BoneMap[B]; if(Bone<0 || Bone>=Ref.GetNum()) return false;
                Total+=W; if(!GarmentBone(Ref.GetBoneName(Bone).ToString())) continue; Cloth+=W;
                if(W>Highest) {Highest=W;Dominant=Bone;}
            }
            if(!Total || Dominant==INDEX_NONE || Cloth*2<Total) continue;
            FAnchor A; A.Vertex=V; A.Section=S; A.Bone=Dominant;
            A.Reference=FVector(LOD->StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V)); if(A.Reference.ContainsNaN()) return false;
            ++CandidateCount; auto& Six=Extremes.FindOrAdd(Dominant);
            if(Six.IsEmpty()) Six.Init(A,6);
            for(int32 Axis=0;Axis<3;++Axis)
            { if(A.Reference[Axis]<Six[Axis*2].Reference[Axis]) Six[Axis*2]=A;
              if(A.Reference[Axis]>Six[Axis*2+1].Reference[Axis]) Six[Axis*2+1]=A; }
        }
    }
    TArray<int32> Keys; Extremes.GetKeys(Keys); Keys.Sort(); TSet<int32> Seen;
    for(int32 B:Keys) for(const auto& A:Extremes[B]) if(!Seen.Contains(A.Vertex)) {Seen.Add(A.Vertex);Out.Add(A);}
    R->SetNumberField(TEXT("all_weighted_candidate_count"),CandidateCount); R->SetNumberField(TEXT("cloth_dominant_bone_count"),Keys.Num());
    R->SetNumberField(TEXT("selected_sparse_anchor_count"),Out.Num());
    R->SetStringField(TEXT("selection_rule"),TEXT("LOD0 vertices with >= half total raw skin weight from J_Sec Skirt/Coat/Sleeve bones; six reference-space XYZ extrema per strongest garment bone, de-duplicated and ordered by bone. Fixed deterministic sparse anchors, NOT the whole garment or approved contact surface."));
    return Out.Num()>0 && Out.Num()<=MaxAnchors;
}
TSharedPtr<FJsonObject> Surface(USkeletalMeshComponent* Mesh,bool Runtime)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),TEXT("NOT_AVAILABLE"));
    R->SetStringField(TEXT("scope"),TEXT("Current component RefToLocal CPU LBS at screenshot request, after usual gameplay tick. Excludes morph deltas, cloth simulation and material WPO/GPU deformation. Signed trace-plane gap is sparse, not whole-clothing visual acceptance."));
    const FSkeletalMeshLODRenderData* LOD=nullptr; const FSkinWeightVertexBuffer* SourceWeights=nullptr; TArray<FAnchor> Points;
    if(!Anchors(Mesh,LOD,SourceWeights,Points,R)) {R->SetStringField(TEXT("error"),TEXT("Bounded native garment render positions/weights unavailable or anchor count outside 1..384"));return R;}
    TArray<FMatrix44f> Matrices; const FSkinWeightVertexBuffer* ActualWeights=SourceWeights;
    FSkinWeightVertexBuffer* RuntimeWeights=nullptr;
    if(Runtime)
    {
        if(!Mesh->GetWorld() || Mesh->GetPredictedLODLevel()!=0) {R->SetStringField(TEXT("error"),TEXT("Runtime world/LOD0 required"));return R;}
        RuntimeWeights=Mesh->GetSkinWeightBuffer(0); ActualWeights=RuntimeWeights;
        if(!ActualWeights || ActualWeights->GetNumVertices()!=LOD->GetNumVertices() || !ActualWeights->GetDataVertexBuffer()->GetWeightData()
            || ActualWeights->GetDataVertexBuffer()->GetVertexDataSize()==0
            || (ActualWeights->GetVariableBonesPerVertex() && (!ActualWeights->GetLookupVertexBuffer()->GetLookupData()
                || ActualWeights->GetLookupVertexBuffer()->GetNumVertices()!=LOD->GetNumVertices()))) return R;
        Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
    }
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<int32> Selected;
    for(const auto& A:Points)
    {
        auto V=MakeShared<FJsonObject>(); Selected.Add(A.Vertex);
        V->SetNumberField(TEXT("render_vertex_index"),A.Vertex); V->SetNumberField(TEXT("section"),A.Section);
        V->SetStringField(TEXT("dominant_garment_bone"),Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(A.Bone).ToString());
        V->SetArrayField(TEXT("reference_component_cm"),XYZ(A.Reference));
        const auto& Part=LOD->RenderSections[A.Section]; V->SetNumberField(TEXT("material_index"),Part.MaterialIndex);
        V->SetStringField(TEXT("actual_material"),GetPathNameSafe(Mesh->GetMaterial(Part.MaterialIndex)));
        uint32 Offset=0,Count=0,ActualOffset=0,ActualCount=0; SourceWeights->GetVertexInfluenceOffsetCount(A.Vertex,Offset,Count);
        ActualWeights->GetVertexInfluenceOffsetCount(A.Vertex,ActualOffset,ActualCount); if(Count!=ActualCount) return R;
        TArray<TSharedPtr<FJsonValue>> InfluenceRows;
        for(uint32 I=0;I<Count;++I)
        {
            const uint16 W=SourceWeights->GetBoneWeight(A.Vertex,I); const uint32 B=SourceWeights->GetBoneIndex(A.Vertex,I);
            if(W!=ActualWeights->GetBoneWeight(A.Vertex,I) || B!=ActualWeights->GetBoneIndex(A.Vertex,I)) return R;
            if(!W) continue; const int32 BI=Part.BoneMap[B];
            if(Runtime && (!Matrices.IsValidIndex(BI) || Matrices[BI].ContainsNaN())) return R;
            auto I0=MakeShared<FJsonObject>(); I0->SetStringField(TEXT("bone"),Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BI).ToString());
            I0->SetNumberField(TEXT("raw_weight_u16"),W); InfluenceRows.Add(MakeShared<FJsonValueObject>(I0));
        }
        V->SetArrayField(TEXT("skin_weights"),InfluenceRows);
        if(Runtime)
        {
            const FVector Local(USkinnedMeshComponent::GetSkinnedVertexPosition(Mesh,A.Vertex,*LOD,*RuntimeWeights,Matrices));
            const FVector World=Mesh->GetComponentTransform().TransformPosition(Local); if(Local.ContainsNaN() || World.ContainsNaN()) return R;
            V->SetArrayField(TEXT("cpu_lbs_world_cm"),XYZ(World));
            FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(HCM5VS2JGarment),false,Mesh->GetOwner());
            const bool Found=Mesh->GetWorld()->LineTraceSingleByChannel(Hit,World+FVector(0,0,150),World-FVector(0,0,500),ECC_WorldStatic,Query);
            V->SetBoolField(TEXT("ground_trace_hit"),Found);
            if(Found)
            {
                V->SetStringField(TEXT("ground_actor"),GetPathNameSafe(Hit.GetActor())); V->SetStringField(TEXT("ground_component"),GetPathNameSafe(Hit.GetComponent()));
                V->SetArrayField(TEXT("ground_world_cm"),XYZ(Hit.ImpactPoint)); V->SetArrayField(TEXT("ground_normal"),XYZ(Hit.ImpactNormal));
                V->SetBoolField(TEXT("trace_start_penetrating"),Hit.bStartPenetrating);
                V->SetNumberField(TEXT("signed_surface_plane_gap_cm"),FVector::DotProduct(World-Hit.ImpactPoint,Hit.ImpactNormal));
                V->SetBoolField(TEXT("upward_support_plane"),!Hit.bStartPenetrating && Hit.ImpactNormal.Z>=.5);
            }
        }
        Rows.Add(MakeShared<FJsonValueObject>(V));
    }
    R->SetArrayField(TEXT("points"),Rows);
    if(Runtime)
    {
        bool Known=true,Affects=false; TArray<TSharedPtr<FJsonValue>> Morphs;
        for(const auto& Active:Mesh->ActiveMorphTargets)
        {
            if(!Active.Key || !Mesh->MorphTargetWeights.IsValidIndex(Active.Value)) {Known=false;continue;}
            const float W=Mesh->MorphTargetWeights[Active.Value]; if(FMath::Abs(W)<.0001 || !Active.Key->HasDataForLOD(0)) continue;
            const auto& ML=Active.Key->GetMorphLODModels()[0]; if(ML.Vertices.IsEmpty()) Known=false;
            auto M=MakeShared<FJsonObject>(); M->SetStringField(TEXT("name"),Active.Key->GetName()); M->SetNumberField(TEXT("weight"),W);
            TArray<TSharedPtr<FJsonValue>> Overlap;
            for(const auto& D:ML.Vertices) if(Selected.Contains(D.SourceIdx) && !D.PositionDelta.IsNearlyZero()) {Affects=true;Overlap.Add(MakeShared<FJsonValueNumber>(D.SourceIdx));}
            M->SetArrayField(TEXT("selected_vertex_overlap"),Overlap); Morphs.Add(MakeShared<FJsonValueObject>(M));
        }
        R->SetArrayField(TEXT("active_morphs"),Morphs); R->SetBoolField(TEXT("active_morph_overlap_known"),Known); R->SetBoolField(TEXT("active_morph_affects_selected_point"),Affects);
    }
    R->SetStringField(TEXT("status"),Runtime ? TEXT("PASS_CPU_LBS_SAMPLE_ONLY") : TEXT("PASS_REFERENCE_ANCHORS_ONLY")); return R;
}
TSharedRef<FJsonObject> Read(USkeletalMeshComponent* Mesh,bool Runtime)
{
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),TEXT("NOT_AVAILABLE"));
    R->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.NPCJGarment.ReadOnly.v1"));
    R->SetBoolField(TEXT("assets_or_physics_changed"),false); R->SetStringField(TEXT("visual_acceptance"),TEXT("NOT_ASSERTED"));
    if(!Scope(Mesh,R)) return R;
    R->SetStringField(TEXT("component_transform"),Mesh->GetComponentTransform().ToHumanReadableString());
    R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    if(Runtime && Mesh->GetWorld()) R->SetNumberField(TEXT("world_seconds"),Mesh->GetWorld()->GetTimeSeconds());
    R->SetObjectField(TEXT("post_process"),Nodes(Mesh,Runtime)); R->SetObjectField(TEXT("garment_surface"),Surface(Mesh,Runtime));
    R->SetStringField(TEXT("status"),TEXT("READBACK_COMPLETE_CHECK_CHILD_STATUS")); return R;
}
}

FString UHCM5VS2NPCClothDiagnostics::InspectJComponent(USkeletalMeshComponent* Mesh)
{ return HCM5VS2JClothPrivate::ClothJSON(HCM5VS2JClothPrivate::Read(Mesh,false)); }
TSharedPtr<FJsonObject> UHCM5VS2NPCClothDiagnostics::CaptureJ(USkeletalMeshComponent* Mesh)
{ return HCM5VS2JClothPrivate::Read(Mesh,true); }

TSharedPtr<FJsonObject> UHCM5VS2NPCClothDiagnostics::CaptureChaosJ(USkeletalMeshComponent* Mesh)
{
    using namespace HCM5VS2JClothPrivate;
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),TEXT("NOT_RUN"));
    R->SetStringField(TEXT("scope"),TEXT("Actual final Chaos particle positions at capture request; not CPU LBS, cloth triangle interiors or inter-layer acceptance"));
    const auto* Asset=Mesh?Mesh->GetSkeletalMeshAsset():nullptr;
    const auto* Physics=Mesh?Mesh->GetPhysicsAsset():nullptr;
    if(!Asset||!Mesh->GetWorld()||!Asset->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/GarmentR2/Batch_"))
        ||!Physics||Physics->GetOutermost()->GetName()!=JPhysics||Physics->SkeletalBodySetups.Num()!=20
        ||!Mesh->bWaitForParallelClothTask||Asset->GetMeshClothingAssets().Num()!=3)
    { R->SetStringField(TEXT("detail"),TEXT("Private J cloth mesh, original 20-body PHYS and explicit fixture cloth wait required")); return R; }
    const auto& Data=Mesh->GetCurrentClothingData_GameThread();
    R->SetBoolField(TEXT("component_collide_with_environment"),Mesh->bCollideWithEnvironment);
    R->SetBoolField(TEXT("force_collision_update"),Mesh->bForceCollisionUpdate);
    R->SetArrayField(TEXT("component_bounds_origin"),XYZ(Mesh->Bounds.Origin));
    R->SetArrayField(TEXT("component_bounds_extent"),XYZ(Mesh->Bounds.BoxExtent));
    TArray<TSharedPtr<FJsonValue>> Simulations;
    for(const auto& Instance:Mesh->GetClothingSimulationInstances())
        if(const auto* Simulation=Instance.GetClothingSimulation())
        {
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("kinematic_particles"),Simulation->GetNumKinematicParticles());
            Row->SetNumberField(TEXT("dynamic_particles"),Simulation->GetNumDynamicParticles());
            Row->SetNumberField(TEXT("iterations"),Simulation->GetNumIterations());Row->SetNumberField(TEXT("substeps"),Simulation->GetNumSubsteps());
            Simulations.Add(MakeShared<FJsonValueObject>(Row));
        }
    R->SetArrayField(TEXT("simulation_runtime"),Simulations);
    TArray<FOverlapResult> Overlaps;FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    FCollisionQueryParams EnvironmentQuery(SCENE_QUERY_STAT(HCM5VS2ClothEnvironmentAvailability),false);
    Mesh->GetWorld()->OverlapMultiByObjectType(Overlaps,Mesh->Bounds.Origin,FQuat::Identity,StaticObjects,
        FCollisionShape::MakeBox(Mesh->Bounds.BoxExtent+FVector(2)),EnvironmentQuery);
    R->SetNumberField(TEXT("nearby_static_collision_components"),Overlaps.Num());
    R->SetStringField(TEXT("environment_query_scope"),TEXT("Same bounds/object-type query as environment extraction; availability only, not proof of solver collision ingestion"));
    R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    R->SetStringField(TEXT("mesh"),Asset->GetPathName());
    R->SetNumberField(TEXT("clothing_assets_read"),Data.Num());
    bool Good=Data.Num()==3; TArray<TSharedPtr<FJsonValue>> Rows;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HCM5VS2ChaosClothGround),false,Mesh->GetOwner());
    for(const auto& Pair:Data)
    {
        const auto& C=Pair.Value; auto Row=MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("asset_index"),Pair.Key); Row->SetNumberField(TEXT("particles"),C.Positions.Num());
        Row->SetNumberField(TEXT("lod_index"),C.LODIndex);
        const bool Bound=Asset->GetMeshClothingAssets().IsValidIndex(Pair.Key);
        if(Bound)Row->SetStringField(TEXT("asset"),GetPathNameSafe(Asset->GetMeshClothingAssets()[Pair.Key]));
        const auto* Common=Bound?Cast<UClothingAssetCommon>(Asset->GetMeshClothingAssets()[Pair.Key]):nullptr;
        const auto* Mask=Common&&Common->LodData.IsValidIndex(C.LODIndex)?Common->LodData[C.LODIndex].PhysicalMeshData.FindWeightMap(EWeightMapTargetCommon::MaxDistance):nullptr;
        const bool bMaskMatches=Mask&&Mask->Values.Num()==C.Positions.Num();
        Row->SetBoolField(TEXT("actual_max_distance_map_matches_particle_count"),bMaskMatches);Good&=bMaskMatches;
        Good&=Bound&&!C.Positions.IsEmpty()&&C.Positions.Num()<=4096&&!C.Transform.ContainsNaN();
        double Minimum=TNumericLimits<double>::Max(); int32 Below=0,Traced=0; FVector Worst=FVector::ZeroVector;
        int32 FixedBelow=0,MovingBelow=0;double FixedMinimum=Minimum,MovingMinimum=Minimum;
        if(C.Positions.Num()<=4096)for(int32 PointIndex=0;PointIndex<C.Positions.Num();++PointIndex)
        {
            const auto& P=C.Positions[PointIndex];
            const FVector World=C.Transform.TransformPosition(FVector(P));
            if(World.ContainsNaN()){Good=false;continue;}
            FHitResult Hit;
            if(Mesh->GetWorld()->LineTraceSingleByChannel(Hit,World+FVector(0,0,60),World-FVector(0,0,1000),ECC_Visibility,Query))
            {
                ++Traced; const double Gap=FVector::DotProduct(World-Hit.ImpactPoint,Hit.ImpactNormal);
                Below+=Gap<-.5?1:0;
                if(bMaskMatches)
                {
                    const bool Fixed=Mask->Values[PointIndex]<=0;
                    if(Fixed){FixedBelow+=Gap<-.5?1:0;FixedMinimum=FMath::Min(FixedMinimum,Gap);}
                    else{MovingBelow+=Gap<-.5?1:0;MovingMinimum=FMath::Min(MovingMinimum,Gap);}
                }
                if(Gap<Minimum){Minimum=Gap;Worst=World;}
            }
        }
        Row->SetNumberField(TEXT("ground_queries_hit"),Traced); Row->SetNumberField(TEXT("particles_below_ground_minus_0_5_cm"),Below);
        Row->SetNumberField(TEXT("fixed_particles_below_ground"),FixedBelow);Row->SetNumberField(TEXT("moving_particles_below_ground"),MovingBelow);
        if(FixedMinimum<TNumericLimits<double>::Max())Row->SetNumberField(TEXT("fixed_minimum_gap_cm"),FixedMinimum);
        if(MovingMinimum<TNumericLimits<double>::Max())Row->SetNumberField(TEXT("moving_minimum_gap_cm"),MovingMinimum);
        if(Traced){Row->SetNumberField(TEXT("minimum_ground_gap_cm"),Minimum);Row->SetArrayField(TEXT("minimum_particle_world_cm"),XYZ(Worst));}
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("cloth"),Rows);
    R->SetStringField(TEXT("status"),Good?TEXT("PASS_PARTICLE_READBACK_ONLY"):TEXT("FAIL_CLOTH_DATA"));
    return R;
}
