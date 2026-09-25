#include "HCM5VS2SoleDiagnostics.h"

#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{
    return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)};
}

FString SoleJSON(const TSharedRef<FJsonObject>& Value)
{
    FString Text;
    auto Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Value, Writer);
    return Text;
}

bool Geometry(USkeletalMesh* Mesh, const FSkeletalMeshLODRenderData*& LOD,
    const FSkinWeightVertexBuffer*& Weights, FString& Failure)
{
    Failure = TEXT("Actual bounded LOD0 CPU render positions/skin weights unavailable");
    if (!Mesh || !Mesh->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/"))) return false;
    const auto* Data = Mesh->GetResourceForRendering();
    if (!Data || !Data->LODRenderData.IsValidIndex(0)) return false;
    LOD = &Data->LODRenderData[0];
    Weights = LOD->GetSkinWeightVertexBuffer();
    if (LOD->GetNumVertices() == 0 || LOD->GetNumVertices() > 200000
        || !LOD->StaticVertexBuffers.PositionVertexBuffer.GetVertexData()
        || !Weights || Weights->GetNumVertices() != LOD->GetNumVertices()
        || !Weights->GetDataVertexBuffer()->GetWeightData()
        || Weights->GetDataVertexBuffer()->GetVertexDataSize() == 0
        || (Weights->GetVariableBonesPerVertex()
            && (!Weights->GetLookupVertexBuffer()->GetLookupData()
                || Weights->GetLookupVertexBuffer()->GetNumVertices() != LOD->GetNumVertices()))) return false;
    Failure.Reset();
    return true;
}

bool VertexSection(const FSkeletalMeshLODRenderData& LOD, int32 Vertex, int32& Section)
{
    if (Vertex < 0 || uint32(Vertex) >= LOD.GetNumVertices()) return false;
    int32 LocalVertex = INDEX_NONE;
    LOD.GetSectionFromVertexIndex(Vertex, Section, LocalVertex);
    return LOD.RenderSections.IsValidIndex(Section) && !LOD.RenderSections[Section].bDisabled;
}

TSharedPtr<FJsonObject> Vertex(USkeletalMesh* Mesh, const FSkeletalMeshLODRenderData& LOD,
    const FSkinWeightVertexBuffer& Weights, int32 Index, int32 Section, FName& Side)
{
    const auto& Part = LOD.RenderSections[Section];
    const auto& Ref = Mesh->GetRefSkeleton();
    uint32 Offset = 0, Count = 0;
    Weights.GetVertexInfluenceOffsetCount(Index, Offset, Count);
    if (Count == 0 || Count > 32 || !Mesh->GetMaterials().IsValidIndex(Part.MaterialIndex)) return nullptr;
    uint32 Left = 0, Right = 0, Total = 0;
    TArray<TSharedPtr<FJsonValue>> Influences;
    for (uint32 I = 0; I < Count; ++I)
    {
        const uint16 Weight = Weights.GetBoneWeight(Index, I);
        if (!Weight) continue;
        const uint32 LocalBone = Weights.GetBoneIndex(Index, I);
        if (!Part.BoneMap.IsValidIndex(LocalBone)) return nullptr;
        const int32 BoneIndex = Part.BoneMap[LocalBone];
        if (BoneIndex < 0 || BoneIndex >= Ref.GetNum()) return nullptr;
        const FName Bone = Ref.GetBoneName(BoneIndex);
        if (Bone == TEXT("Foot_L") || Bone == TEXT("Toe_L")) Left += Weight;
        if (Bone == TEXT("Foot_R") || Bone == TEXT("Toe_R")) Right += Weight;
        Total += Weight;
        auto Influence = MakeShared<FJsonObject>();
        Influence->SetStringField(TEXT("bone"), Bone.ToString());
        Influence->SetNumberField(TEXT("skeleton_index"), BoneIndex);
        Influence->SetNumberField(TEXT("section_bone_index"), LocalBone);
        Influence->SetNumberField(TEXT("raw_weight_u16"), Weight);
        Influences.Add(MakeShared<FJsonValueObject>(Influence));
    }
    // Preserve the native quantized sum; do not silently normalize CPU skinning weights.
    if (Total == 0) return nullptr;
    Side = Left * 2 >= Total && Right == 0 ? FName(TEXT("L"))
        : Right * 2 >= Total && Left == 0 ? FName(TEXT("R")) : NAME_None;
    auto Row = MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("render_vertex_index"), Index);
    Row->SetNumberField(TEXT("section_index"), Section);
    Row->SetNumberField(TEXT("material_index"), Part.MaterialIndex);
    const auto& Material = Mesh->GetMaterials()[Part.MaterialIndex];
    Row->SetStringField(TEXT("material_slot"), Material.MaterialSlotName.ToString());
    Row->SetStringField(TEXT("default_material"), GetPathNameSafe(Material.MaterialInterface.Get()));
    Row->SetStringField(TEXT("side"), Side.ToString());
    Row->SetArrayField(TEXT("reference_component_cm"), XYZ(FVector(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index))));
    Row->SetNumberField(TEXT("raw_weight_sum"), Total);
    Row->SetNumberField(TEXT("native_weight_sum"), double(Total) / 65535.0);
    Row->SetArrayField(TEXT("skin_weights"), Influences);
    return Row;
}
}

FString UHCM5VS2SoleDiagnostics::InspectShoeGeometry(USkeletalMesh* Mesh)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("status"), TEXT("FAIL"));
    Result->SetStringField(TEXT("scope"), TEXT("Actual LOD0 render vertex IDs, positions, per-section BoneMap and CPU weights. IDs are NOT source FBX indices. Candidates are NOT automatically approved heel/forefoot skin; no asset writes."));
    Result->SetStringField(TEXT("mesh"), GetPathNameSafe(Mesh));
    const FSkeletalMeshLODRenderData* LOD = nullptr;
    const FSkinWeightVertexBuffer* Weights = nullptr;
    FString Failure;
    if (!Geometry(Mesh, LOD, Weights, Failure))
    {
        Result->SetStringField(TEXT("error"), Failure);
        return SoleJSON(Result);
    }
    const auto& Ref = Mesh->GetRefSkeleton();
    TArray<FTransform> Global = Ref.GetRefBonePose();
    for (int32 I = 0; I < Global.Num(); ++I)
        if (Ref.GetParentIndex(I) != INDEX_NONE) Global[I] *= Global[Ref.GetParentIndex(I)];
    TArray<TSharedPtr<FJsonValue>> Bones, Candidates;
    FVector Feet[2], Toes[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FName Foot = Side == 0 ? TEXT("Foot_L") : TEXT("Foot_R");
        const FName Toe = Side == 0 ? TEXT("Toe_L") : TEXT("Toe_R");
        const int32 FootIndex = Ref.FindBoneIndex(Foot), ToeIndex = Ref.FindBoneIndex(Toe);
        if (!Global.IsValidIndex(FootIndex) || !Global.IsValidIndex(ToeIndex))
        {
            Result->SetStringField(TEXT("error"), TEXT("Actual Selestia Foot/Toe reference bones missing"));
            return SoleJSON(Result);
        }
        Feet[Side] = Global[FootIndex].GetLocation();
        Toes[Side] = Global[ToeIndex].GetLocation();
        auto Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("side"), Side == 0 ? TEXT("L") : TEXT("R"));
        Bone->SetArrayField(TEXT("foot_reference_component_cm"), XYZ(Feet[Side]));
        Bone->SetArrayField(TEXT("toe_reference_component_cm"), XYZ(Toes[Side]));
        Bone->SetNumberField(TEXT("foot_to_toe_length_cm"), FVector::Distance(Feet[Side], Toes[Side]));
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    for (int32 Section = 0; Section < LOD->RenderSections.Num(); ++Section)
    {
        const auto& Part = LOD->RenderSections[Section];
        if (Part.bDisabled) continue;
        if (Part.BaseVertexIndex + Part.NumVertices > LOD->GetNumVertices())
        {
            Result->SetStringField(TEXT("error"), TEXT("Render section vertex range invalid"));
            return SoleJSON(Result);
        }
        for (uint32 I = Part.BaseVertexIndex; I < Part.BaseVertexIndex + Part.NumVertices; ++I)
        {
            FName Side;
            auto Row = Vertex(Mesh, *LOD, *Weights, I, Section, Side);
            if (!Row)
            {
                Result->SetStringField(TEXT("error"), TEXT("Invalid native skin-weight/BoneMap data"));
                return SoleJSON(Result);
            }
            if (Side.IsNone()) continue;
            const int32 Which = Side == TEXT("L") ? 0 : 1;
            const FVector Position(LOD->StaticVertexBuffers.PositionVertexBuffer.VertexPosition(I));
            const double Length = FVector::Distance(Feet[Which], Toes[Which]);
            // Generous, disclosed candidate envelope; classification is reviewed later.
            if (Length < 1 || Position.Z > Feet[Which].Z + 2 || FVector::Distance(Position, Feet[Which]) > Length * 2.5) continue;
            Candidates.Add(MakeShared<FJsonValueObject>(Row));
            if (Candidates.Num() > 12000)
            {
                Result->SetStringField(TEXT("error"), TEXT("Foot candidate geometry exceeded bounded 12000 vertices"));
                return SoleJSON(Result);
            }
        }
    }
    Result->SetNumberField(TEXT("lod"), 0);
    Result->SetNumberField(TEXT("total_render_vertices"), LOD->GetNumVertices());
    Result->SetNumberField(TEXT("candidate_count"), Candidates.Num());
    Result->SetStringField(TEXT("candidate_rule"), TEXT("At least half raw skin weight from same-side Foot/Toe, zero opposite-foot weight; position no higher than ankle+2cm and within 2.5 actual Foot-Toe lengths. Includes possible hidden body/garment geometry; material/surface review required."));
    Result->SetArrayField(TEXT("reference_bones"), Bones);
    Result->SetArrayField(TEXT("candidates"), Candidates);
    Result->SetStringField(TEXT("status"), Candidates.Num() >= 12 ? TEXT("PASS") : TEXT("FAIL"));
    return SoleJSON(Result);
}

bool UHCM5VS2SoleDiagnostics::ValidateSelection(USkeletalMeshComponent* Mesh,
    const TArray<FHCM5VS2SolePoint>& Points, FString& Failure)
{
    Failure = TEXT("Expected exactly 12 verified L/R heel/forefoot render vertices");
    if (!Mesh || Points.Num() != 12 || Mesh->GetPredictedLODLevel() != 0) return false;
    const FSkeletalMeshLODRenderData* LOD = nullptr;
    const FSkinWeightVertexBuffer* Weights = nullptr;
    if (!Geometry(Mesh->GetSkeletalMeshAsset(), LOD, Weights, Failure)) return false;
    Failure = TEXT("Actual component LOD0 skin weights unavailable or differ from the selected native geometry");
    auto* ActualWeights = Mesh->GetSkinWeightBuffer(0);
    if (!ActualWeights || ActualWeights->GetNumVertices() != LOD->GetNumVertices()
        || !ActualWeights->GetDataVertexBuffer()->GetWeightData()
        || ActualWeights->GetDataVertexBuffer()->GetVertexDataSize() == 0
        || (ActualWeights->GetVariableBonesPerVertex()
            && (!ActualWeights->GetLookupVertexBuffer()->GetLookupData()
                || ActualWeights->GetLookupVertexBuffer()->GetNumVertices() != LOD->GetNumVertices()))) return false;
    TSet<int32> Seen;
    int32 Counts[2][2] = {{0, 0}, {0, 0}};
    for (const auto& Point : Points)
    {
        Failure = FString::Printf(TEXT("Selected vertex %d: side/region, section, reference position or native skin-weight mismatch"), Point.VertexIndex);
        const int32 Side = Point.Side == TEXT("L") ? 0 : Point.Side == TEXT("R") ? 1 : INDEX_NONE;
        const int32 Region = Point.Region == TEXT("Heel") ? 0 : Point.Region == TEXT("Forefoot") ? 1 : INDEX_NONE;
        int32 Section = INDEX_NONE;
        if (Side == INDEX_NONE || Region == INDEX_NONE || Seen.Contains(Point.VertexIndex)
            || !VertexSection(*LOD, Point.VertexIndex, Section) || Section != Point.SectionIndex) return false;
        const FVector Reference(LOD->StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Point.VertexIndex));
        if (!Reference.Equals(Point.ReferencePosition, .0001) || Reference.ContainsNaN()) return false;
        FName ActualSide;
        if (!Vertex(Mesh->GetSkeletalMeshAsset(), *LOD, *ActualWeights, Point.VertexIndex, Section, ActualSide)
            || ActualSide != Point.Side) return false;
        uint32 SourceOffset = 0, SourceCount = 0, ActualOffset = 0, ActualCount = 0;
        Weights->GetVertexInfluenceOffsetCount(Point.VertexIndex, SourceOffset, SourceCount);
        ActualWeights->GetVertexInfluenceOffsetCount(Point.VertexIndex, ActualOffset, ActualCount);
        if (SourceCount != ActualCount) return false;
        for (uint32 I = 0; I < SourceCount; ++I)
            if (Weights->GetBoneIndex(Point.VertexIndex, I) != ActualWeights->GetBoneIndex(Point.VertexIndex, I)
                || Weights->GetBoneWeight(Point.VertexIndex, I) != ActualWeights->GetBoneWeight(Point.VertexIndex, I)) return false;
        for (const auto& Prior : Points)
        {
            if (Prior.VertexIndex == Point.VertexIndex) break;
            if (Prior.Side == Point.Side && Prior.Region == Point.Region
                && FVector::Distance(Prior.ReferencePosition, Point.ReferencePosition) < .1) return false;
        }
        Seen.Add(Point.VertexIndex);
        ++Counts[Side][Region];
    }
    Failure = TEXT("Exactly three distinct fixed vertices per side and heel/forefoot region are required");
    if (Counts[0][0] != 3 || Counts[0][1] != 3 || Counts[1][0] != 3 || Counts[1][1] != 3) return false;
    Failure.Reset();
    return true;
}

TSharedPtr<FJsonObject> UHCM5VS2SoleDiagnostics::CaptureSurface(USkeletalMeshComponent* Mesh,
    const TArray<FHCM5VS2SolePoint>& Points)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("status"), TEXT("FAIL"));
    Result->SetStringField(TEXT("scope"), TEXT("Actual fixed native LOD0 render vertices evaluated by CPU LBS from current RefToLocal matrices and skin weights. BEFORE morph offsets, cloth and material WPO; NOT final GPU skin/cache or visible sole contact proof. Actual active morph overlap is reported separately."));
    FString Failure;
    if (!ValidateSelection(Mesh, Points, Failure))
    {
        Result->SetStringField(TEXT("error"), Failure);
        return Result;
    }
    if (!Mesh->GetWorld())
    {
        Result->SetStringField(TEXT("error"), TEXT("Actual runtime world unavailable"));
        return Result;
    }
    const auto& LOD = Mesh->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
    auto* Weights = Mesh->GetSkinWeightBuffer(0);
    TArray<FMatrix44f> Matrices;
    Mesh->GetCurrentRefToLocalMatrices(Matrices, 0);
    for (const auto& Point : Points)
    {
        const auto& Part = LOD.RenderSections[Point.SectionIndex];
        uint32 Offset = 0, Count = 0;
        Weights->GetVertexInfluenceOffsetCount(Point.VertexIndex, Offset, Count);
        for (uint32 I = 0; I < Count; ++I)
        {
            if (Weights->GetBoneWeight(Point.VertexIndex, I) == 0) continue;
            const int32 Bone = Part.BoneMap[Weights->GetBoneIndex(Point.VertexIndex, I)];
            if (!Matrices.IsValidIndex(Bone) || Matrices[Bone].ContainsNaN())
            {
                Result->SetStringField(TEXT("error"), TEXT("Current native RefToLocal matrices missing or nonfinite for selected weighted bone"));
                return Result;
            }
        }
    }
    TArray<TSharedPtr<FJsonValue>> Morphs, Rows;
    bool MorphDataKnown = true, MorphAffectsPoint = false;
    TSet<int32> Selected;
    for (const auto& Point : Points) Selected.Add(Point.VertexIndex);
    for (const auto& Active : Mesh->ActiveMorphTargets)
    {
        if (!Active.Key || !Mesh->MorphTargetWeights.IsValidIndex(Active.Value))
        {
            MorphDataKnown = false;
            continue;
        }
        const float Weight = Mesh->MorphTargetWeights[Active.Value];
        if (FMath::Abs(Weight) < .0001 || !Active.Key->HasDataForLOD(0)) continue;
        const auto& MorphLOD = Active.Key->GetMorphLODModels()[0];
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("morph"), Active.Key->GetName());
        Row->SetNumberField(TEXT("actual_weight"), Weight);
        Row->SetNumberField(TEXT("cpu_delta_count"), MorphLOD.Vertices.Num());
        TArray<TSharedPtr<FJsonValue>> Affected;
        if (MorphLOD.Vertices.IsEmpty()) MorphDataKnown = false;
        for (const auto& Delta : MorphLOD.Vertices)
            if (Selected.Contains(Delta.SourceIdx) && !Delta.PositionDelta.IsNearlyZero())
            {
                Affected.Add(MakeShared<FJsonValueNumber>(Delta.SourceIdx));
                MorphAffectsPoint = true;
            }
        Row->SetArrayField(TEXT("selected_vertices_with_position_delta"), Affected);
        Morphs.Add(MakeShared<FJsonValueObject>(Row));
    }
    const FTransform ToWorld = Mesh->GetComponentTransform();
    for (const auto& Point : Points)
    {
        const FVector Local(USkinnedMeshComponent::GetSkinnedVertexPosition(Mesh, Point.VertexIndex, LOD, *Weights, Matrices));
        const FVector World = ToWorld.TransformPosition(Local);
        if (Local.ContainsNaN() || World.ContainsNaN())
        {
            Result->SetStringField(TEXT("error"), TEXT("Nonfinite native LBS position"));
            return Result;
        }
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("side"), Point.Side.ToString());
        Row->SetStringField(TEXT("region"), Point.Region.ToString());
        Row->SetNumberField(TEXT("render_vertex_index"), Point.VertexIndex);
        Row->SetNumberField(TEXT("section_index"), Point.SectionIndex);
        Row->SetArrayField(TEXT("reference_component_cm"), XYZ(Point.ReferencePosition));
        Row->SetArrayField(TEXT("cpu_lbs_component_cm"), XYZ(Local));
        Row->SetArrayField(TEXT("cpu_lbs_world_cm"), XYZ(World));
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HCM5VS2SoleSurface), false, Mesh->GetOwner());
        const bool Found = Mesh->GetWorld()->LineTraceSingleByChannel(Hit, World + FVector(0, 0, 20),
            World - FVector(0, 0, 500), ECC_WorldStatic, Query);
        Row->SetBoolField(TEXT("ground_trace_hit"), Found);
        if (Found)
        {
            Row->SetStringField(TEXT("ground_actor"), GetPathNameSafe(Hit.GetActor()));
            Row->SetStringField(TEXT("ground_component"), GetPathNameSafe(Hit.GetComponent()));
            Row->SetArrayField(TEXT("ground_world_cm"), XYZ(Hit.ImpactPoint));
            Row->SetArrayField(TEXT("ground_normal"), XYZ(Hit.ImpactNormal));
            Row->SetBoolField(TEXT("trace_start_penetrating"), Hit.bStartPenetrating);
            Row->SetNumberField(TEXT("signed_surface_plane_distance_cm"), FVector::DotProduct(World - Hit.ImpactPoint, Hit.ImpactNormal));
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetNumberField(TEXT("frame"), double(GFrameCounter));
    Result->SetNumberField(TEXT("world_seconds"), Mesh->GetWorld()->GetTimeSeconds());
    Result->SetNumberField(TEXT("wall_seconds_absolute"), FPlatformTime::Seconds());
    Result->SetNumberField(TEXT("actual_lod"), Mesh->GetPredictedLODLevel());
    Result->SetStringField(TEXT("mesh"), GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
    Result->SetArrayField(TEXT("mesh_world_translation_cm"), XYZ(ToWorld.GetTranslation()));
    Result->SetArrayField(TEXT("points"), Rows);
    Result->SetArrayField(TEXT("active_morphs"), Morphs);
    Result->SetBoolField(TEXT("active_morph_cpu_overlap_known"), MorphDataKnown);
    Result->SetBoolField(TEXT("active_morph_affects_selected_point"), MorphAffectsPoint);
    Result->SetBoolField(TEXT("cpu_lbs_excludes_no_known_active_point_morph"), MorphDataKnown && !MorphAffectsPoint);
    Result->SetStringField(TEXT("surface_acceptance"), TEXT("USER_REVIEW_NOT_GPU_OR_CONTACT_PASS"));
    Result->SetStringField(TEXT("status"), TEXT("PASS_CPU_LBS_SAMPLE_ONLY"));
    return Result;
}
