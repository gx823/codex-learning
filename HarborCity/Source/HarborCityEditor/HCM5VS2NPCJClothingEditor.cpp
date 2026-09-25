#include "HCM5VS2NPCJClothingEditor.h"
#include "ClothingAssetFactory.h"
#include "ClothingAsset.h"
#include "ClothLODData.h"
#include "ClothPhysicalMeshData.h"
#include "ClothVertBoneData.h"
#include "PointWeightMap.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "SkinnedAssetCompiler.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AnimNode_VrmSpringBone.h"
#include "VrmMetaObject.h"

namespace
{
constexpr int32 Sections[3]={10,12,13};
constexpr uint32 ExpectedTriangles[3]={846,452,452};
const TCHAR* const MaterialTokens[3]={TEXT("MI_N00_002_03_Tops_01_CLOTH_01__Instance_"),
    TEXT("MI_N00_002_03_Tops_01_CLOTH_02__Instance_"),TEXT("MI_N00_002_03_Tops_01_CLOTH_03__Instance_")};

FString Finish(const TSharedRef<FJsonObject>& Result,const FString& Error=FString())
{
    Result->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    Result->SetStringField(TEXT("schema"),TEXT("HarborCity.M5VS2.NPCJClothing.Mesh.v1"));
    Result->SetBoolField(TEXT("saved_by_helper"),false);
    Result->SetStringField(TEXT("runtime_and_visual"),TEXT("NOT_RUN"));
    Result->SetStringField(TEXT("inter_asset_cloth_collision"),TEXT("NOT_ASSERTED"));
    if(!Error.IsEmpty())Result->SetStringField(TEXT("error"),Error);
    FString Text;if(!FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text)))
        return TEXT("{\"status\":\"FAIL\",\"error\":\"Native JSON serialization failed\"}");
    return Text;
}

bool PrivateMesh(const USkeletalMesh* Mesh)
{
    if(!Mesh)return false;
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/GarmentR2/Batch_");
    const FString Path=Mesh->GetOutermost()->GetName();
    if(!Path.StartsWith(Prefix)||Path.Len()<Prefix.Len()+14||Path[Prefix.Len()+12]!='/')return false;
    for(TCHAR C:Path.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return false;
    return Path.Mid(Prefix.Len()+13)==TEXT("SK_NPCJ_Clothing");
}

bool Sibling(const UObject* Object,const USkeletalMesh* Mesh,const FString& Leaf)
{
    if(!Object||!PrivateMesh(Mesh))return false;
    const FString MeshPackage=Mesh->GetOutermost()->GetName();int32 Slash=INDEX_NONE;
    MeshPackage.FindLastChar('/',Slash);
    return Slash!=INDEX_NONE&&Object->GetOutermost()->GetName()==MeshPackage.Left(Slash+1)+Leaf;
}

bool Skirt(const FName Name)
{const FString S=Name.ToString();return S.StartsWith(TEXT("J_Sec_"))&&S.Contains(TEXT("_Skirt"))&&!S.Contains(TEXT("Coat"));}

bool ExactSource(const USkeletalMesh* Mesh,const UPhysicsAsset* BodyPhysics)
{
    // The component's validated runtime PHYS override is deliberately separate
    // from the importer mesh's own physics asset; never substitute the latter.
    return Mesh&&BodyPhysics&&Mesh->GetOutermost()->GetName()==TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Source_8b6562a56a4a/SK_AvatarSample_J_Studio2140")
        &&BodyPhysics->GetOutermost()->GetName()==TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Runtime_0f9dd6fb5a/PHYS_NPC_Humanoid")
        &&BodyPhysics->SkeletalBodySetups.Num()==20&&Mesh->GetMeshClothingAssets().IsEmpty()
        &&Mesh->GetImportedModel()&&Mesh->GetImportedModel()->LODModels.Num()==1;
}

bool GeometryPreserved(const USkeletalMesh* Source,const USkeletalMesh* Candidate)
{
    if(!Source||!Candidate||Source==Candidate||Source->GetSkeleton()!=Candidate->GetSkeleton()
        ||Source->GetPhysicsAsset()!=Candidate->GetPhysicsAsset()
        ||Source->GetMorphTargets().Num()!=Candidate->GetMorphTargets().Num()
        ||Source->GetMaterials().Num()!=Candidate->GetMaterials().Num())return false;
    for(int32 I=0;I<Source->GetMaterials().Num();++I)
    {
        // Only these three material interfaces may differ. The complete exact
        // private parent-chain/graph comparison is required separately below.
        const bool Garment=I==10||I==12||I==13;
        if(!Garment&&Source->GetMaterials()[I].MaterialInterface!=Candidate->GetMaterials()[I].MaterialInterface)return false;
        if(Source->GetMaterials()[I].MaterialSlotName!=Candidate->GetMaterials()[I].MaterialSlotName
            ||Source->GetMaterials()[I].ImportedMaterialSlotName!=Candidate->GetMaterials()[I].ImportedMaterialSlotName)return false;
    }
    const auto* A=Source->GetImportedModel();const auto* B=Candidate->GetImportedModel();
    if(!A||!B||A->LODModels.Num()!=1||B->LODModels.Num()!=1)return false;
    const auto& L=A->LODModels[0];const auto& R=B->LODModels[0];
    if(L.IndexBuffer!=R.IndexBuffer||L.Sections.Num()!=R.Sections.Num())return false;
    for(int32 S=0;S<L.Sections.Num();++S)
    {
        const auto& X=L.Sections[S];const auto& Y=R.Sections[S];
        if(X.MaterialIndex!=Y.MaterialIndex||X.NumTriangles!=Y.NumTriangles||X.BaseIndex!=Y.BaseIndex
            ||X.SoftVertices.Num()!=Y.SoftVertices.Num()||X.BoneMap!=Y.BoneMap)return false;
        for(int32 V=0;V<X.SoftVertices.Num();++V)
        {
            const auto& P=X.SoftVertices[V];const auto& Q=Y.SoftVertices[V];
            if(P.Position!=Q.Position||P.TangentX!=Q.TangentX||P.TangentY!=Q.TangentY
                ||P.TangentZ!=Q.TangentZ||P.Color!=Q.Color)return false;
            for(int32 U=0;U<UE_ARRAY_COUNT(P.UVs);++U)if(P.UVs[U]!=Q.UVs[U])return false;
            for(int32 I=0;I<MAX_TOTAL_INFLUENCES;++I)
                if(P.InfluenceBones[I]!=Q.InfluenceBones[I]||P.InfluenceWeights[I]!=Q.InfluenceWeights[I])return false;
        }
    }
    return true;
}

// Exact existing chain, leaf -> root. No shared plugin material is edited.
const TCHAR* const AncestorNames[4]={TEXT("MI_VrmMToonOptLitOpaqueTwoSided"),TEXT("MI_VrmMToonOptLitOpaque"),
    TEXT("MI_VrmMToonBaseLitOpaque"),TEXT("M_VrmMToonBaseOpaque")};
const TCHAR* const PrivateAncestorNames[4]={TEXT("MI_JClothing_OptLitOpaqueTwoSided"),TEXT("MI_JClothing_OptLitOpaque"),
    TEXT("MI_JClothing_BaseLitOpaque"),TEXT("M_JClothing_BaseOpaque")};

bool MaterialPairs(const USkeletalMesh* Source,const USkeletalMesh* Candidate,
    TArray<UMaterialInterface*>& Originals,TArray<UMaterialInterface*>& Copies,FString& Error)
{
    if(!Source||!PrivateMesh(Candidate)||Source->GetMaterials().Num()!=Candidate->GetMaterials().Num()
        ||Source->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Source_8b6562a56a4a/SK_AvatarSample_J_Studio2140"))
    {Error=TEXT("Exact source and private mesh required for material comparison");return false;}
    for(int32 Slot=0;Slot<Source->GetMaterials().Num();++Slot)
    {
        if(Slot!=10&&Slot!=12&&Slot!=13&&Source->GetMaterials()[Slot].MaterialInterface!=Candidate->GetMaterials()[Slot].MaterialInterface)
        {Error=FString::Printf(TEXT("Non-garment material slot %d changed"),Slot);return false;}
    }
    for(int32 I=0;I<3;++I)
    {
        if(!Source->GetMaterials().IsValidIndex(Sections[I])){Error=TEXT("Required material slot missing");return false;}
        UMaterialInterface* A=Source->GetMaterials()[Sections[I]].MaterialInterface.Get();
        UMaterialInterface* B=Candidate->GetMaterials()[Sections[I]].MaterialInterface.Get();
        if(!A||A->GetName()!=MaterialTokens[I]||!Cast<UMaterialInstanceConstant>(A)||!Cast<UMaterialInstanceConstant>(B)
            ||!Sibling(B,Candidate,FString::Printf(TEXT("MI_JClothing_Section%d"),Sections[I])))
        {Error=TEXT("Exact source/private leaf material identity failed");return false;}
        Originals.Add(A);Copies.Add(B);
        for(int32 Depth=0;Depth<4;++Depth)
        {
            const auto* AI=Cast<UMaterialInstanceConstant>(A);const auto* BI=Cast<UMaterialInstanceConstant>(B);
            if(!AI||!BI){Error=TEXT("Expected MIC parent chain length/type differs");return false;}
            A=AI->Parent;B=BI->Parent;
            const FString Expected=FString(TEXT("/VRM4U/MaterialUtil/UE5/Material/"))+AncestorNames[Depth];
            if(!A||A->GetOutermost()->GetName()!=Expected||!Sibling(B,Candidate,PrivateAncestorNames[Depth])||A->GetClass()!=B->GetClass())
            {Error=FString::Printf(TEXT("Exact ancestor identity failed at depth %d"),Depth);return false;}
            const int32 Found=Originals.Find(A);
            if(Found==INDEX_NONE){Originals.Add(A);Copies.Add(B);}
            else if(Copies[Found]!=B){Error=TEXT("Shared ancestor duplicated inconsistently");return false;}
        }
        if(!Cast<UMaterial>(A)||!Cast<UMaterial>(B)){Error=TEXT("Expected exact root Material");return false;}
    }
    return Originals.Num()==7&&Copies.Num()==7;
}

FString NormalizeMaterialText(FString Text,const TArray<UMaterialInterface*>& Originals,const TArray<UMaterialInterface*>& Copies)
{
    for(int32 I=0;I<Originals.Num();++I)
    {
        const FString Token=FString::Printf(TEXT("$J_MATERIAL_%d"),I);
        Text.ReplaceInline(*(Originals[I]->GetPathName()+TEXT(":")+Originals[I]->GetName()+TEXT("EditorOnlyData")),*(Token+TEXT(":EditorOnlyData")),ESearchCase::CaseSensitive);
        Text.ReplaceInline(*(Copies[I]->GetPathName()+TEXT(":")+Copies[I]->GetName()+TEXT("EditorOnlyData")),*(Token+TEXT(":EditorOnlyData")),ESearchCase::CaseSensitive);
        Text.ReplaceInline(*Originals[I]->GetPathName(),*Token,ESearchCase::CaseSensitive);
        Text.ReplaceInline(*Copies[I]->GetPathName(),*Token,ESearchCase::CaseSensitive);
    }
    return Text;
}

bool EquivalentMaterialObjects(UMaterialInterface* Source,UMaterialInterface* Copy,
    const TArray<UMaterialInterface*>& Originals,const TArray<UMaterialInterface*>& Copies,
    TSharedRef<FJsonObject> Row,FString& Error)
{
    TArray<UObject*> AObjects,BObjects;GetObjectsWithOuter(Source,AObjects);GetObjectsWithOuter(Copy,BObjects);
    AObjects.Insert(Source,0);BObjects.Insert(Copy,0);
    TMap<FString,UObject*> AMap,BMap;
    auto OwnedKey=[](UObject* O,UMaterialInterface* Root)
    {
        if(O==Root)return FString(TEXT("$"));
        const FString Name=O->GetPathName(Root),Prefix=Root->GetName()+TEXT("EditorOnlyData");
        // The engine renames this owned editor-data object with its new material.
        // Still compare its class and every persistent property, including graph.
        return Name==Prefix||Name.StartsWith(Prefix+TEXT("."))?FString(TEXT("EditorOnlyData"))+Name.Mid(Prefix.Len()):Name;
    };
    for(UObject* O:AObjects)if(!O->HasAnyFlags(RF_Transient))AMap.Add(OwnedKey(O,Source),O);
    for(UObject* O:BObjects)if(!O->HasAnyFlags(RF_Transient))BMap.Add(OwnedKey(O,Copy),O);
    if(AMap.Num()!=BMap.Num()){Error=TEXT("Material duplicate owned-object count mismatch");return false;}
    int32 Properties=0;TArray<TSharedPtr<FJsonValue>> Ignored,Differences;
    for(const auto& Pair:AMap)
    {
        UObject* A=Pair.Value;UObject* const* Found=BMap.Find(Pair.Key);UObject* B=Found?*Found:nullptr;
        if(!B||A->GetClass()!=B->GetClass()){Error=TEXT("Material owned-object name/class mismatch: ")+Pair.Key;return false;}
        for(TFieldIterator<FProperty> It(A->GetClass());It;++It)
        {
            FProperty* P=*It;const FName Name=P->GetFName();
            if(P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_Deprecated))continue;
            // Native duplicate/PostEditChange regenerate only these identity /
            // shader-cache GUIDs. All parameter GUIDs, graph links, static
            // switches, texture refs and inactive base overrides are compared.
            const bool CacheGuid=A==Source&&(Name==TEXT("LightingGuid")
                ||(Cast<UMaterial>(A)&&Name==TEXT("StateId"))
                ||(Cast<UMaterialInstanceConstant>(A)&&Name==TEXT("ParameterStateId")));
            const bool ClothingUsage=A==Source&&Cast<UMaterial>(A)&&Name==TEXT("bUsedWithClothing");
            if(CacheGuid||ClothingUsage){Ignored.Add(MakeShared<FJsonValueString>(Pair.Key+TEXT(".")+Name.ToString()));continue;}
            // UMaterialInterface::SetTextureStreamingData derives this marker
            // from the cache array (and PostLoad can discard an old version).
            // Record the marker separately; the actual TextureStreamingData
            // array, every texture reference and graph/parameter remain subject
            // to the exact comparison below. This never mutates the source.
            if(A==Source&&Name==TEXT("TextureStreamingDataVersion"))
            {
                FString Before,After;P->ExportText_InContainer(0,Before,A,nullptr,A,PPF_None);
                P->ExportText_InContainer(0,After,B,nullptr,B,PPF_None);
                auto Cache=MakeShared<FJsonObject>();Cache->SetStringField(TEXT("source"),Before);Cache->SetStringField(TEXT("candidate"),After);
                Cache->SetStringField(TEXT("scope"),TEXT("Derived version marker only; texture streaming entries still compared exactly"));
                Row->SetObjectField(TEXT("derived_texture_streaming_version"),Cache);continue;
            }
            for(int32 Index=0;Index<P->ArrayDim;++Index)
            {
                FString Left,Right;P->ExportText_InContainer(Index,Left,A,nullptr,A,PPF_None);
                P->ExportText_InContainer(Index,Right,B,nullptr,B,PPF_None);
                Left=NormalizeMaterialText(MoveTemp(Left),Originals,Copies);Right=NormalizeMaterialText(MoveTemp(Right),Originals,Copies);
                // UE 5.8 propagates the new root usage into each MIC's resolved
                // BasePropertyOverrides. Normalize exactly that inherited bit;
                // every other base field and usage bit still compares verbatim.
                if(A==Source&&Name==TEXT("BasePropertyOverrides"))
                {
                    const auto* MA=Cast<UMaterialInstanceConstant>(A);const auto* MB=Cast<UMaterialInstanceConstant>(B);
                    if(MA&&MB)
                    {
                        const uint32 Before=MA->BasePropertyOverrides.UsageFlags,After=MB->BasePropertyOverrides.UsageFlags;
                        const uint32 ClothingBit=1u<<uint32(MATUSAGE_Clothing);
                        if((Before&ClothingBit)==0&&After==(Before|ClothingBit))
                            Right.ReplaceInline(*FString::Printf(TEXT("UsageFlags=%u"),After),*FString::Printf(TEXT("UsageFlags=%u"),Before),ESearchCase::CaseSensitive);
                    }
                }
                if(Left!=Right)
                {
                    auto Difference=MakeShared<FJsonObject>();Difference->SetStringField(TEXT("object"),Pair.Key);Difference->SetStringField(TEXT("property"),Name.ToString());
                    Difference->SetStringField(TEXT("source_value_bounded"),Left.Left(2048));Difference->SetStringField(TEXT("candidate_value_bounded"),Right.Left(2048));
                    Differences.Add(MakeShared<FJsonValueObject>(Difference));
                }
                ++Properties;
            }
        }
    }
    Row->SetNumberField(TEXT("compared_owned_objects"),AMap.Num());Row->SetNumberField(TEXT("compared_property_elements"),Properties);
    Row->SetArrayField(TEXT("excluded_exact_root_usage_and_cache_fields"),Ignored);
    Row->SetArrayField(TEXT("authored_property_differences"),Differences);
    if(!Differences.IsEmpty())Error=FString::Printf(TEXT("Material has %d authored property differences; all collected in one pass"),Differences.Num());
    return Differences.IsEmpty();
}

bool CheckMaterials(const USkeletalMesh* Source,const USkeletalMesh* Candidate,TSharedRef<FJsonObject> Result,FString& Error)
{
    TArray<UMaterialInterface*> Originals,Copies;
    if(!MaterialPairs(Source,Candidate,Originals,Copies,Error))return false;
    TArray<TSharedPtr<FJsonValue>> Rows;bool bAllEquivalent=true;
    for(int32 I=0;I<Originals.Num();++I)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("source"),Originals[I]->GetPathName());
        Row->SetStringField(TEXT("candidate"),Copies[I]->GetPathName());Rows.Add(MakeShared<FJsonValueObject>(Row));
        Result->SetArrayField(TEXT("material_compatibility"),Rows);
        if(!EquivalentMaterialObjects(Originals[I],Copies[I],Originals,Copies,Row,Error))bAllEquivalent=false;
        const bool Before=Originals[I]->GetUsageByFlag(MATUSAGE_Clothing),After=Copies[I]->GetUsageByFlag(MATUSAGE_Clothing);
        Row->SetBoolField(TEXT("source_clothing_usage"),Before);Row->SetBoolField(TEXT("candidate_clothing_usage"),After);
        if(Before||!After){Error=TEXT("Exact false -> true Clothing usage readback failed");return false;}
        if(Originals[I]->GetBlendMode()!=Copies[I]->GetBlendMode()||Originals[I]->IsTwoSided()!=Copies[I]->IsTwoSided()
            ||Originals[I]->GetShadingModels()!=Copies[I]->GetShadingModels()
            ||Originals[I]->GetOpacityMaskClipValue()!=Copies[I]->GetOpacityMaskClipValue())
        {Error=TEXT("Resolved appearance properties differ after parent replacement");return false;}
    }
    Result->SetStringField(TEXT("appearance_comparison"),TEXT("Authored root/owned-object properties, texture references, streaming entries and final shading are compared; exact Clothing bit normalized; regenerated GUIDs and derived streaming version marker reported separately. Actual rendered equivalence NOT_RUN."));
    return bAllEquivalent;
}

// Derive an allowance from this actual cloth surface. Zero skirt weight is
// pinned. Shortest edge-path from pins is in mesh centimeters, not world Z.
bool MakeMaxDistance(UClothingAssetCommon* Asset,TArray<float>& Values,TSharedRef<FJsonObject> Row)
{
    if(!Asset||Asset->LodData.Num()!=1)return false;
    const auto& Physical=Asset->LodData[0].PhysicalMeshData;const int32 N=Physical.Vertices.Num();
    if(N<3||N>2000||Physical.BoneData.Num()!=N||Physical.Indices.Num()%3)return false;
    TArray<float> Weights;Weights.Init(0.f,N);TArray<double> Distance;Distance.Init(TNumericLimits<double>::Max(),N);
    TArray<bool> Visited;Visited.Init(false,N);TArray<TArray<int32>> Edges;Edges.SetNum(N);int32 Fixed=0,Moving=0;
    for(int32 V=0;V<N;++V)
    {
        double Sum=0,Cloth=0;const auto& B=Physical.BoneData[V];
        for(int32 I=0;I<FClothVertBoneData::MaxTotalInfluences;++I)
        {
            const float W=B.BoneWeights[I];if(W<=0)continue;
            if(!FMath::IsFinite(W)||!Asset->UsedBoneNames.IsValidIndex(B.BoneIndices[I]))return false;
            Sum+=W;if(Skirt(Asset->UsedBoneNames[B.BoneIndices[I]]))Cloth+=W;
        }
        if(Sum<.99||Sum>1.01||!FMath::IsFinite(Sum))return false;
        Weights[V]=float(Cloth/Sum);
        if(Weights[V]<=1.e-5f){Distance[V]=0.;++Fixed;}else ++Moving;
    }
    for(int32 I=0;I<Physical.Indices.Num();I+=3)
    {
        for(int32 E=0;E<3;++E)
        {
            const int32 A=Physical.Indices[I+E],B=Physical.Indices[I+(E+1)%3];
            if(!Edges.IsValidIndex(A)||!Edges.IsValidIndex(B)||A==B)return false;
            Edges[A].AddUnique(B);Edges[B].AddUnique(A);
        }
    }
    if(!Fixed||!Moving)return false;
    // O(N^2) is bounded by 2000 author-time points; no gameplay solver here.
    for(int32 Pass=0;Pass<N;++Pass)
    {
        int32 Pick=INDEX_NONE;double Best=TNumericLimits<double>::Max();
        for(int32 V=0;V<N;++V)if(!Visited[V]&&Distance[V]<Best){Pick=V;Best=Distance[V];}
        if(Pick==INDEX_NONE)break;Visited[Pick]=true;
        for(int32 V:Edges[Pick])Distance[V]=FMath::Min(Distance[V],Best+double(FVector3f::Distance(Physical.Vertices[V],Physical.Vertices[Pick])));
    }
    Values.SetNum(N);double Maximum=0.;
    for(int32 V=0;V<N;++V)
    {
        if(!Visited[V]||!FMath::IsFinite(Distance[V]))return false; // Every island must have an anchor.
        Values[V]=float(2.*Distance[V]*Weights[V]); // Full drape within twice attachment distance; tethers constrain stretch.
        if(!FMath::IsFinite(Values[V])||Values[V]<0||Values[V]>150.f)return false;
        Maximum=FMath::Max(Maximum,double(Values[V]));
    }
    Row->SetNumberField(TEXT("simulation_vertices"),N);Row->SetNumberField(TEXT("simulation_triangles"),Physical.Indices.Num()/3);
    Row->SetNumberField(TEXT("fixed_vertices"),Fixed);Row->SetNumberField(TEXT("moving_vertices"),Moving);
    Row->SetNumberField(TEXT("max_distance_cm"),Maximum);
    Row->SetStringField(TEXT("mask_rule"),TEXT("2 * shortest surface-edge distance to zero-Skirt-weight pinned vertex * total Skirt skin fraction; centimeters; every island anchored"));
    return true;
}

bool ConfigExact(const UChaosClothConfig* C)
{
    return C&&!C->bUseGravityOverride&&C->GravityScale==1.f&&C->bUseCCD
        &&C->CollisionThickness==1.f&&C->DampingCoefficient==.01f&&C->LocalDampingCoefficient==.15f
        &&C->BendingStiffnessWeighted.Low==.15f&&C->BendingStiffnessWeighted.High==.15f
        &&C->bUseSelfCollisions&&C->SelfCollisionThickness==.4f;
}
}

FString UHCM5VS2NPCJClothingEditor::ProbeSource(USkeletalMesh* SourceMesh,UPhysicsAsset* BodyPhysics)
{
    const auto Result=MakeShared<FJsonObject>();
    if(!ExactSource(SourceMesh,BodyPhysics))return Finish(Result,TEXT("Exact J source mesh and active component 20-body PHYS override required"));
    const TStrongObjectPtr<USkeletalMesh> KeepSource(SourceMesh);
    const TStrongObjectPtr<UPhysicsAsset> KeepPhysics(BodyPhysics);
    TArray<TSharedPtr<FJsonValue>> Rows;bool AllUsage=true;
    const auto& L=SourceMesh->GetImportedModel()->LODModels[0];
    for(int32 I=0;I<3;++I)
    {
        const int32 S=Sections[I];
        if(!L.Sections.IsValidIndex(S)||L.Sections[S].NumTriangles!=ExpectedTriangles[I]
            ||!SourceMesh->GetMaterials().IsValidIndex(L.Sections[S].MaterialIndex))
            return Finish(Result,TEXT("Native source garment section/triangle identity failed"));
        const auto& Sec=L.Sections[S];const auto* M=SourceMesh->GetMaterials()[Sec.MaterialIndex].MaterialInterface.Get();
        if(!M||M->GetName()!=MaterialTokens[I])return Finish(Result,TEXT("Native source garment material identity failed"));
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("section"),S);
        Row->SetNumberField(TEXT("material_index"),Sec.MaterialIndex);Row->SetStringField(TEXT("material"),M->GetPathName());
        Row->SetNumberField(TEXT("vertices"),Sec.SoftVertices.Num());Row->SetNumberField(TEXT("triangles"),Sec.NumTriangles);
        const bool Usage=M->GetUsageByFlag(MATUSAGE_Clothing);AllUsage&=Usage;
        Row->SetBoolField(TEXT("clothing_usage_already_enabled"),Usage);
        Row->SetStringField(TEXT("usage_query"),TEXT("GetUsageByFlag only; does not enable or compile source material usage"));
        TArray<TSharedPtr<FJsonValue>> Chain;const UMaterialInterface* Current=M;
        for(int32 Depth=0;Depth<5&&Current;++Depth)
        {
            auto Link=MakeShared<FJsonObject>();Link->SetStringField(TEXT("path"),Current->GetPathName());
            Link->SetStringField(TEXT("class"),Current->GetClass()->GetPathName());
            Chain.Add(MakeShared<FJsonValueObject>(Link));
            const auto* MI=Cast<UMaterialInstanceConstant>(Current);Current=MI?MI->Parent.Get():nullptr;
        }
        Row->SetArrayField(TEXT("material_chain_leaf_to_root"),Chain);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("sections"),Rows);Result->SetBoolField(TEXT("all_source_materials_support_clothing"),AllUsage);
    Result->SetStringField(TEXT("mesh"),SourceMesh->GetPathName());Result->SetStringField(TEXT("active_body_physics_override"),BodyPhysics->GetPathName());
    Result->SetStringField(TEXT("importer_mesh_physics"),GetPathNameSafe(SourceMesh->GetPhysicsAsset()));
    Result->SetStringField(TEXT("next"),AllUsage?TEXT("Private asset author may proceed; cloth runtime remains NOT_RUN"):TEXT("BLOCKED_APPLY: private visually identical usage-compatible material copies needed; originals must remain unchanged"));
    return Finish(Result);
}

FString UHCM5VS2NPCJClothingEditor::ValidatePrivateMaterials(USkeletalMesh* SourceMesh,USkeletalMesh* CandidateMesh)
{
    const auto Result=MakeShared<FJsonObject>();FString Error;
    if(!CheckMaterials(SourceMesh,CandidateMesh,Result,Error))return Finish(Result,Error.IsEmpty()?TEXT("Private material comparison failed"):Error);
    return Finish(Result);
}

FString UHCM5VS2NPCJClothingEditor::CopyPrivateStreamingData(USkeletalMesh* SourceMesh,USkeletalMesh* CandidateMesh)
{
    const auto Result=MakeShared<FJsonObject>();FString Error;
    TArray<UMaterialInterface*> Originals,Copies;
    if(!MaterialPairs(SourceMesh,CandidateMesh,Originals,Copies,Error))return Finish(Result,Error);
    int32 Entries=0;
    for(int32 I=0;I<Originals.Num();++I)
    {
        Copies[I]->Modify();
        Copies[I]->SetTextureStreamingData(Originals[I]->GetTextureStreamingData());
        Copies[I]->MarkPackageDirty();Entries+=Originals[I]->GetTextureStreamingData().Num();
    }
    Result->SetNumberField(TEXT("private_materials"),Copies.Num());
    Result->SetNumberField(TEXT("copied_streaming_entries"),Entries);
    Result->SetStringField(TEXT("scope"),TEXT("Only seven verified private copies; native streaming setter; unchanged mesh UVs and authored graph still require strict ValidatePrivateMaterials"));
    return Finish(Result);
}

FString UHCM5VS2NPCJClothingEditor::ConfigurePrivateSpring(USkeletalMesh* CandidateMesh,UVrmMetaObject* SourceMeta,
    UVrmMetaObject* CandidateMeta,UAnimBlueprint* SourcePost,UAnimBlueprint* CandidatePost,bool bApply)
{
    const auto Result=MakeShared<FJsonObject>();
    if(!PrivateMesh(CandidateMesh)||!SourceMeta||!SourcePost||SourceMeta==CandidateMeta||SourcePost==CandidatePost
        ||!Sibling(CandidateMeta,CandidateMesh,TEXT("VM_NPCJ_Clothing"))
        ||!Sibling(CandidatePost,CandidateMesh,TEXT("ABP_NPCJ_ClothingPost"))
        ||SourceMeta->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Source_8b6562a56a4a/VM_AvatarSample_J_Studio2140_VrmMeta")
        ||SourcePost->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_J/Source_8b6562a56a4a/ABP_Post_AvatarSample_J_Studio2140")
        ||SourceMeta->Version!=0||!SourceMeta->SkeletalMesh||SourceMeta->VRMSpringMeta.Num()!=21||CandidateMesh->GetMeshClothingAssets().Num()!=3
        ||SourceMeta->SkeletalMesh->GetSkeleton()!=CandidateMesh->GetSkeleton())
        return Finish(Result,TEXT("Exact original J metadata/postprocess and complete private cloth siblings required"));
    const TStrongObjectPtr<USkeletalMesh> KeepMesh(CandidateMesh);
    const TStrongObjectPtr<UVrmMetaObject> KeepSourceMeta(SourceMeta),KeepCandidateMeta(CandidateMeta);
    const TStrongObjectPtr<UAnimBlueprint> KeepSourcePost(SourcePost),KeepCandidatePost(CandidatePost);
    TArray<FVRMSpringMeta> Expected;TArray<TSharedPtr<FJsonValue>> Removed;
    for(int32 I=0;I<SourceMeta->VRMSpringMeta.Num();++I)
    {
        const auto& Group=SourceMeta->VRMSpringMeta[I];const bool Remove=I==2||I==4||I==5;
        bool AnySkirt=false,OnlySkirt=!Group.boneNames.IsEmpty();
        for(const FString& Bone:Group.boneNames){const bool S=Skirt(FName(*Bone));AnySkirt|=S;OnlySkirt&=S;}
        if(Remove!=AnySkirt||(Remove&&!OnlySkirt))return Finish(Result,TEXT("Skirt group indices do not match actual named roots; refuse to remove other springs"));
        if(Remove)Removed.Add(MakeShared<FJsonValueNumber>(I));else Expected.Add(Group);
    }
    for(TFieldIterator<FProperty> It(UVrmMetaObject::StaticClass());It;++It)
    {
        const FName Name=It->GetFName();
        if(Name!=TEXT("VRMSpringMeta")&&Name!=TEXT("SkeletalMesh")&&!It->Identical_InContainer(SourceMeta,CandidateMeta))
            return Finish(Result,FString::Printf(TEXT("Non-skirt metadata property changed: %s"),*Name.ToString()));
    }
    if(bApply)
    {
        CandidateMeta->VRMSpringMeta=Expected;CandidateMeta->SkeletalMesh=CandidateMesh;CandidateMeta->MarkPackageDirty();
        TArray<UEdGraph*> Graphs;CandidatePost->GetAllGraphs(Graphs);int32 Changed=0;
        for(UEdGraph* Graph:Graphs)if(Graph)for(UEdGraphNode* Node:Graph->Nodes)if(Node)
            for(TFieldIterator<FStructProperty> It(Node->GetClass());It;++It)
                if(It->Struct==FAnimNode_VrmSpringBone::StaticStruct())
                {
                    auto* Spring=It->ContainerPtrToValuePtr<FAnimNode_VrmSpringBone>(Node);
                    if(!Spring->bIgnorePhysicsCollision)return Finish(Result,TEXT("Unexpected original world-collision flag; do not activate dormant coordinate bug"));
                    Spring->VrmMetaObject=CandidateMeta;Spring->EnableAutoSearchMetaData=false;++Changed;
                }
        if(Changed!=1)return Finish(Result,TEXT("Expected one real VRM0 spring node in private postprocess graph"));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CandidatePost);
        FKismetEditorUtilities::CompileBlueprint(CandidatePost);
        if(CandidatePost->Status==BS_Error||!CandidatePost->GeneratedClass
            ||!CandidatePost->GeneratedClass->IsChildOf(UAnimInstance::StaticClass()))
            return Finish(Result,TEXT("Native private postprocess compilation failed"));
        CandidateMesh->SetPostProcessAnimBlueprint(TSubclassOf<UAnimInstance>(CandidatePost->GeneratedClass.Get()));
        CandidateMesh->MarkPackageDirty();CandidatePost->MarkPackageDirty();
    }
    if(CandidateMeta->VRMSpringMeta.Num()!=Expected.Num()||CandidateMeta->SkeletalMesh!=CandidateMesh
        ||CandidateMesh->GetPostProcessAnimBlueprint()!=CandidatePost->GeneratedClass)
        return Finish(Result,TEXT("Native candidate metadata or mesh postprocess binding mismatch"));
    for(int32 I=0;I<Expected.Num();++I)
        if(!FVRMSpringMeta::StaticStruct()->CompareScriptStruct(&Expected[I],&CandidateMeta->VRMSpringMeta[I],0))
            return Finish(Result,TEXT("Retained spring group no longer exactly matches source"));
    UObject* CDO=CandidatePost->GeneratedClass?CandidatePost->GeneratedClass->GetDefaultObject():nullptr;int32 Found=0;
    if(CDO)for(TFieldIterator<FStructProperty> It(CDO->GetClass());It;++It)
        if(It->Struct==FAnimNode_VrmSpringBone::StaticStruct())
        {
            const auto* Spring=It->ContainerPtrToValuePtr<FAnimNode_VrmSpringBone>(CDO);
            if(Spring->VrmMetaObject!=CandidateMeta||Spring->EnableAutoSearchMetaData||!Spring->bIgnorePhysicsCollision)
                return Finish(Result,TEXT("Compiled spring CDO does not exclusively use private metadata and original collision policy"));
            ++Found;
        }
    if(Found!=1)return Finish(Result,TEXT("Expected one compiled VRM spring node"));
    Result->SetArrayField(TEXT("removed_skirt_group_indices"),Removed);Result->SetNumberField(TEXT("original_spring_groups"),21);
    Result->SetNumberField(TEXT("retained_identical_spring_groups"),Expected.Num());Result->SetNumberField(TEXT("compiled_spring_nodes"),Found);
    Result->SetStringField(TEXT("private_metadata"),CandidateMeta->GetPathName());Result->SetStringField(TEXT("private_postprocess"),CandidatePost->GetPathName());
    Result->SetBoolField(TEXT("automatic_metadata_search"),false);Result->SetBoolField(TEXT("original_ignore_spring_world_collision"),true);
    Result->SetStringField(TEXT("scope"),TEXT("Only three actual Skirt spring groups removed; head, hair, ears, tail, colliders and source VA identity retained. Native Chaos computes cloth after body physics."));
    return Finish(Result);
}

FString UHCM5VS2NPCJClothingEditor::BuildPrivateClothing(USkeletalMesh* SourceMesh,USkeletalMesh* CandidateMesh,UPhysicsAsset* BodyPhysics,bool bApply)
{
    const auto Result=MakeShared<FJsonObject>();
    if(!ExactSource(SourceMesh,BodyPhysics)||!PrivateMesh(CandidateMesh)||!GeometryPreserved(SourceMesh,CandidateMesh)
        ||CandidateMesh->GetMeshClothingAssets().Num()!=(bApply?0:3))
        return Finish(Result,TEXT("Exact source J, original 20-body PHYS, unchanged geometry/materials and fresh private copy required"));
    FString MaterialError;
    if(!CheckMaterials(SourceMesh,CandidateMesh,Result,MaterialError))return Finish(Result,MaterialError);
    const TStrongObjectPtr<USkeletalMesh> KeepSource(SourceMesh),KeepCandidate(CandidateMesh);
    const TStrongObjectPtr<UPhysicsAsset> KeepPhysics(BodyPhysics);
    for(int32 I=0;I<3;++I)
    {
        const auto& L=SourceMesh->GetImportedModel()->LODModels[0];const int32 S=Sections[I];
        if(!L.Sections.IsValidIndex(S)||L.Sections[S].NumTriangles!=ExpectedTriangles[I]
            ||!SourceMesh->GetMaterials().IsValidIndex(L.Sections[S].MaterialIndex)
            ||!SourceMesh->GetMaterials()[L.Sections[S].MaterialIndex].MaterialInterface
            ||SourceMesh->GetMaterials()[L.Sections[S].MaterialIndex].MaterialInterface->GetName()!=MaterialTokens[I])
            return Finish(Result,TEXT("Actual J garment section identity/triangle guard failed"));
        if(!CandidateMesh->GetMaterials()[L.Sections[S].MaterialIndex].MaterialInterface->GetUsageByFlag(MATUSAGE_Clothing))
            return Finish(Result,TEXT("Private material lacks Clothing usage; refuse runtime fallback"));
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 I=0;I<3;++I)
    {
        auto Row=MakeShared<FJsonObject>();const int32 S=Sections[I];UClothingAssetCommon* Asset=nullptr;
        Row->SetNumberField(TEXT("section"),S);Row->SetStringField(TEXT("source_material"),MaterialTokens[I]);
        Row->SetStringField(TEXT("compatible_material"),CandidateMesh->GetMaterials()[S].MaterialInterface->GetPathName());
        Rows.Add(MakeShared<FJsonValueObject>(Row));Result->SetArrayField(TEXT("clothing"),Rows);
        if(bApply)
        {
            FSkeletalMeshClothBuildParams Params;Params.AssetName=FString::Printf(TEXT("Cloth_J_Skirt_%02d"),I+1);
            Params.LodIndex=0;Params.SourceSection=S;Params.bRemoveFromMesh=false;Params.PhysicsAsset=BodyPhysics;
            const TStrongObjectPtr<UClothingAssetFactory> Factory(NewObject<UClothingAssetFactory>());
            Asset=Cast<UClothingAssetCommon>(Factory->CreateFromSkeletalMesh(CandidateMesh,Params));
            if(!Asset)return Finish(Result,TEXT("Native cloth factory failed; preserve partial private candidate, never retry in place"));
            // AddClothingAsset owns the new asset before any further editor operation.
            CandidateMesh->AddClothingAsset(Asset);
        }
        else Asset=Cast<UClothingAssetCommon>(CandidateMesh->GetSectionClothingAsset(0,S));
        if(!Asset||Asset->GetOuter()!=CandidateMesh||Asset->PhysicsAsset!=BodyPhysics)
            return Finish(Result,TEXT("Exact private cloth outer/body collision reference failed"));
        TArray<float> Expected;if(!MakeMaxDistance(Asset,Expected,Row))
            return Finish(Result,TEXT("Cloth topology, skin influence, connected anchor or maximum distance guard failed"));
        auto& Lod=Asset->LodData[0];
        if(bApply)
        {
            FPointWeightMap* Mask=nullptr;
            for(auto& M:Lod.PointWeightMaps)if(M.CurrentTarget==uint8(EWeightMapTargetCommon::MaxDistance))
            {if(Mask)return Finish(Result,TEXT("Multiple max-distance masks"));Mask=&M;}
            if(!Mask)return Finish(Result,TEXT("Factory max-distance mask absent"));
            Mask->Name=TEXT("J_Garment_AnchoredSurface_cm");Mask->bEnabled=true;Mask->Values=Expected;
            UChaosClothConfig* Config=Asset->GetClothConfig<UChaosClothConfig>();
            if(!Config)return Finish(Result,TEXT("Loaded Chaos factory did not provide native config"));
            Config->bUseGravityOverride=false;Config->GravityScale=1.f;Config->bUseCCD=true;
            Config->CollisionThickness=1.f;Config->DampingCoefficient=.01f;Config->LocalDampingCoefficient=.15f;
            Config->BendingStiffnessWeighted.Low=.15f;Config->BendingStiffnessWeighted.High=.15f;
            Config->bUseSelfCollisions=true;Config->SelfCollisionThickness=.4f;
            Lod.bSmoothTransition=true;
            Asset->ApplyParameterMasks(true,true);Asset->InvalidateAllCachedData();
            // BindToSkeletalMesh triggers PostEditChange. Persist the user-section
            // binding first so that rebuild cannot restore its old unbound data.
            auto& Section=CandidateMesh->GetImportedModel()->LODModels[0].Sections[S];
            auto& User=CandidateMesh->GetImportedModel()->LODModels[0].UserSectionsData.FindOrAdd(Section.OriginalDataSectionIndex);
            User.CorrespondClothAssetIndex=static_cast<int16>(CandidateMesh->GetClothingAssetIndex(Asset->GetAssetGuid()));
            User.ClothingData.AssetGuid=Asset->GetAssetGuid();User.ClothingData.AssetLodIndex=0;
            if(!Asset->BindToSkeletalMesh(CandidateMesh,0,S,0))return Finish(Result,TEXT("Native cloth binding failed"));
            TArray<USkinnedAsset*> Pending{CandidateMesh};FSkinnedAssetCompilingManager::Get().FinishCompilation(Pending);
        }
        const FPointWeightMap* Actual=Asset->LodData[0].PhysicalMeshData.FindWeightMap(EWeightMapTargetCommon::MaxDistance);
        const auto* Config=Asset->GetClothConfig<UChaosClothConfig>();
        Row->SetBoolField(TEXT("weight_map_present"),Actual!=nullptr);
        Row->SetBoolField(TEXT("weight_values_exact"),Actual&&Actual->Values==Expected);
        Row->SetBoolField(TEXT("config_exact"),ConfigExact(Config));
        Row->SetStringField(TEXT("render_section_clothing_asset"),GetPathNameSafe(CandidateMesh->GetSectionClothingAsset(0,S)));
        Row->SetBoolField(TEXT("asset_guid_valid"),Asset->GetAssetGuid().IsValid());
        if(!Actual||Actual->Values!=Expected||!ConfigExact(Config)
            ||CandidateMesh->GetSectionClothingAsset(0,S)!=Asset||!Asset->GetAssetGuid().IsValid())
            return Finish(Result,TEXT("Native clothing readback does not match actual candidate mask/config/binding"));
        Row->SetStringField(TEXT("asset"),Asset->GetPathName());Row->SetStringField(TEXT("guid"),Asset->GetAssetGuid().ToString());
        Row->SetStringField(TEXT("physics"),Asset->PhysicsAsset->GetPathName());Row->SetBoolField(TEXT("ccd"),Config->bUseCCD);
        Row->SetNumberField(TEXT("world_gravity_scale"),Config->GravityScale);
        Row->SetNumberField(TEXT("collision_thickness_cm"),Config->CollisionThickness);
        Row->SetNumberField(TEXT("global_damping"),Config->DampingCoefficient);Row->SetNumberField(TEXT("local_damping"),Config->LocalDampingCoefficient);
        Row->SetNumberField(TEXT("bending_stiffness"),Config->BendingStiffnessWeighted.Low);
        Row->SetNumberField(TEXT("self_collision_thickness_cm"),Config->SelfCollisionThickness);
    }
    if(!GeometryPreserved(SourceMesh,CandidateMesh)||CandidateMesh->GetMeshClothingAssets().Num()!=3)
        return Finish(Result,TEXT("Final geometry/material/body preservation failed"));
    if(bApply){CandidateMesh->PostEditChange();CandidateMesh->MarkPackageDirty();}
    Result->SetArrayField(TEXT("clothing"),Rows);Result->SetStringField(TEXT("mesh"),CandidateMesh->GetPathName());
    Result->SetBoolField(TEXT("original_body_physics_pointer_preserved"),true);
    Result->SetStringField(TEXT("active_body_physics_override"),BodyPhysics->GetPathName());
    Result->SetBoolField(TEXT("original_render_vertices_weights_triangles_preserved"),true);
    Result->SetStringField(TEXT("materials"),TEXT("Only slots 10/12/13 use exact private visually equivalent Clothing-usage copies; all other material interfaces unchanged"));
    Result->SetStringField(TEXT("candidate_parameters"),TEXT("First private prototype, not validated artistic values. Existing Chaos defaults otherwise retained."));
    Result->SetStringField(TEXT("remaining_rig_authoring"),TEXT("Private Meta remove actual Skirt groups2/4/5; private PP explicit meta; private Profile.Mesh+BP mesh; bCollideWithEnvironment=true; current helper only builds mesh clothing"));
    return Finish(Result);
}
