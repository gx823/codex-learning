#include "HCM5VS2HeroModestyComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Package.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "M1/HCM1Character.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "BoneWeights.h"
#include "SkinnedAssetCompiler.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#endif
namespace
{
FString ModestyJSON(const TSharedPtr<FJsonObject>& J){FString Text;FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));return Text;}
TArray<TSharedPtr<FJsonValue>> ModestyXYZ(const FVector3f& P){return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};}
bool PrivateModesty(const UObject* Object)
{
    if(!Object)return false;const FString Path=Object->GetOutermost()->GetName();const FString Prefix=TEXT("/Game/HarborCity/M5VS2/HeroModesty/Review_");
    if(!Path.StartsWith(Prefix)||Path.Len()<Prefix.Len()+14||Path[Prefix.Len()+12]!=TCHAR('/'))return false;
    for(TCHAR C:Path.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return false;return true;
}
#if WITH_EDITOR
using namespace UE::AnimationCore;
constexpr float LowerCut=56.f,UpperCut=81.f,InnerOffset=.45f,OuterOffset=.60f;
struct FClothVertex{FVector3f P=FVector3f::ZeroVector,N=FVector3f::ZeroVector;FBoneWeights W;};
struct FClothEdge{int32 A=0,B=0,Count=0;bool SameDirection=false;};
uint64 EdgeKey(int32 A,int32 B){return(uint64(uint32(FMath::Min(A,B)))<<32)|uint32(FMath::Max(A,B));}
using FClothTriangle=TStaticArray<int32,3>;
FBoneWeightsSettings WeightSettings(){FBoneWeightsSettings S;S.SetMaxWeightCount(12);S.SetWeightThreshold(1.f/65535.f);return S;}
float WeightDifference(const FBoneWeights& A,const FBoneWeights& B)
{
    TMap<int32,float> Difference;for(const auto& W:A)Difference.FindOrAdd(W.GetBoneIndex())+=W.GetWeight();for(const auto& W:B)Difference.FindOrAdd(W.GetBoneIndex())-=W.GetWeight();
    float Max=0;for(const auto& P:Difference)Max=FMath::Max(Max,FMath::Abs(P.Value));return Max;
}
FClothVertex Blend(const FClothVertex& A,const FClothVertex& B,float T)
{FClothVertex V;V.P=FMath::Lerp(A.P,B.P,T);V.N=FMath::Lerp(A.N,B.N,T).GetSafeNormal();V.W=FBoneWeights::Blend(A.W,B.W,T,WeightSettings());return V;}
TArray<FClothVertex> Clip(const TArray<FClothVertex>& Polygon,float Plane,bool Above)
{
    TArray<FClothVertex> Out;for(int32 I=0;I<Polygon.Num();++I)
    {
        const auto& A=Polygon[I];const auto& B=Polygon[(I+1)%Polygon.Num()];const bool IA=Above?A.P.Z>=Plane:A.P.Z<=Plane;const bool IB=Above?B.P.Z>=Plane:B.P.Z<=Plane;
        if(IA)Out.Add(A);if(IA!=IB){auto V=Blend(A,B,(Plane-A.P.Z)/(B.P.Z-A.P.Z));V.P.Z=Plane;Out.Add(V);}
    }return Out;
}
void AddEdge(TMap<uint64,FClothEdge>& Edges,int32 A,int32 B)
{auto& E=Edges.FindOrAdd(EdgeKey(A,B));if(E.Count==0){E.A=A;E.B=B;}else if(E.A==A&&E.B==B)E.SameDirection=true;++E.Count;}
TMap<uint64,FClothEdge> MeshEdges(const TArray<FClothTriangle>& Faces)
{TMap<uint64,FClothEdge> Edges;for(const auto& T:Faces)for(int32 I=0;I<3;++I)AddEdge(Edges,T[I],T[(I+1)%3]);return Edges;}
TArray<TArray<int32>> Connected(const TMap<int32,TArray<int32>>& Adj)
{
    TArray<TArray<int32>> Groups;TSet<int32> Seen;
    for(const auto& P:Adj)if(!Seen.Contains(P.Key))
    {TArray<int32> Stack{P.Key},Group;while(!Stack.IsEmpty()){int32 I=Stack.Pop(EAllowShrinking::No);if(Seen.Contains(I))continue;Seen.Add(I);Group.Add(I);if(const auto* Neighbours=Adj.Find(I))Stack.Append(*Neighbours);}Groups.Add(Group);}return Groups;
}
uint32 GeometryCRC(const FMeshDescription& D)
{
    FSkeletalMeshConstAttributes A(D);uint32 CRC=0;auto Hash=[&](const auto& V){CRC=FCrc::MemCrc32(&V,sizeof(V),CRC);};
    for(FVertexID V:D.Vertices().GetElementIDs()){Hash(V.GetValue());Hash(A.GetVertexPositions()[V]);for(const auto& W:A.GetVertexSkinWeights().Get(V)){Hash(W.GetBoneIndex());Hash(W.GetRawWeight());}}
    for(FTriangleID T:D.Triangles().GetElementIDs()){Hash(T.GetValue());for(FVertexID V:D.GetTriangleVertices(T))Hash(V.GetValue());}
    return CRC;
}
bool BodySource(USkeletalMesh* Source,const FMeshDescription*& D,int32& Start,int32& Count,TSharedRef<FJsonObject> J,FString& Error)
{
    if(!Source||!Source->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_"))||Source->GetName()!=TEXT("SKM_Selestia_Warp"))
    {Error=TEXT("Exact current native GASMotion Selestia body required");return false;}
    TArray<USkinnedAsset*> Pending{Source};FSkinnedAssetCompilingManager::Get().FinishCompilation(Pending);D=Source->GetMeshDescription(0);
    if(Source->GetLODNum()!=1||!D||!Source->GetSkeleton()){Error=TEXT("One native LOD and source skeleton/description required");return false;}
    const FSkeletalMeshConstAttributes A(*D);const auto Raw=D->VertexAttributes().GetAttributesRef<int32>(MeshAttribute::Vertex::ImportPointIndex);
    if(!A.HasSourceGeometryParts()||!Raw.IsValid()||!A.GetVertexSkinWeights().IsValid()){Error=TEXT("Native raw import provenance and skinning missing");return false;}
    int32 Matches=0;for(FSourceGeometryPartID P:A.SourceGeometryParts().GetElementIDs())
    {
        const FName N=A.GetSourceGeometryPartNames()[P];if(N==TEXT("Vert.006")||N==TEXT("Vert_006"))
        {const auto Range=A.GetSourceGeometryPartVertexOffsetAndCounts()[P];if(Range.Num()!=2){Error=TEXT("Invalid body range");return false;}Start=Range[0];Count=Range[1];++Matches;}
    }
    if(Matches!=1||Count!=15309){Error=TEXT("Source Selestia_body Vert.006 must be the exact 15309-control-point part");return false;}
    FVector3f Min(FLT_MAX),Max(-FLT_MAX);TSet<int32> Points;int32 BodyTriangles=0;
    for(FVertexID V:D->Vertices().GetElementIDs())if(Raw[V]>=Start&&Raw[V]<Start+Count)
    {const auto P=A.GetVertexPositions()[V];Min=Min.ComponentMin(P);Max=Max.ComponentMax(P);Points.Add(Raw[V]);}
    for(FTriangleID T:D->Triangles().GetElementIDs())
    {bool All=true;for(FVertexID V:D->GetTriangleVertices(T))All&=Raw[V]>=Start&&Raw[V]<Start+Count;if(All)++BodyTriangles;}
    J->SetStringField(TEXT("source_mesh"),Source->GetPathName());J->SetStringField(TEXT("body_part"),TEXT("Vert.006 / Selestia_body"));J->SetNumberField(TEXT("body_raw_point_count"),Points.Num());J->SetNumberField(TEXT("body_triangles"),BodyTriangles);J->SetNumberField(TEXT("raw_start"),Start);J->SetArrayField(TEXT("body_bounds_min_cm"),ModestyXYZ(Min));J->SetArrayField(TEXT("body_bounds_max_cm"),ModestyXYZ(Max));
    J->SetNumberField(TEXT("source_geometry_crc"),GeometryCRC(*D));J->SetNumberField(TEXT("lower_cut_mesh_z_cm"),LowerCut);J->SetNumberField(TEXT("upper_cut_mesh_z_cm"),UpperCut);J->SetNumberField(TEXT("inner_offset_cm"),InnerOffset);J->SetNumberField(TEXT("outer_offset_cm"),OuterOffset);
    if(Points.Num()!=15309||BodyTriangles!=30516||Min.Z<0||Min.Z>10||Max.Z<110||Max.Z>117||FMath::Abs(Min.X)>65||FMath::Abs(Max.X)>65)
    {Error=TEXT("Native part topology/cm Z-up identity differs from audited source; do not guess axes/scale");return false;}
    return true;
}
bool BuildClipped(const FMeshDescription& D,int32 Start,int32 Count,TArray<FClothVertex>& Vertices,TArray<FClothTriangle>& Faces,TSharedRef<FJsonObject> J,FString& Error)
{
    const FSkeletalMeshConstAttributes A(D);const auto Raw=D.VertexAttributes().GetAttributesRef<int32>(MeshAttribute::Vertex::ImportPointIndex);
    TMap<FIntVector,int32> Weld;float MaxWeightDifference=0;int32 DroppedDegenerate=0;
    auto Add=[&](const FClothVertex& V)->int32
    {
        const FIntVector Key(FMath::RoundToInt(V.P.X*10000.f),FMath::RoundToInt(V.P.Y*10000.f),FMath::RoundToInt(V.P.Z*10000.f));
        if(const int32* Existing=Weld.Find(Key))
        {auto& E=Vertices[*Existing];MaxWeightDifference=FMath::Max(MaxWeightDifference,WeightDifference(E.W,V.W));E.N=(E.N+V.N).GetSafeNormal();return *Existing;}
        int32 Index=Vertices.Add(V);Weld.Add(Key,Index);return Index;
    };
    for(FTriangleID T:D.Triangles().GetElementIDs())
    {
        bool All=true,Any=false;for(FVertexID V:D.GetTriangleVertices(T)){const bool IsBody=Raw[V]>=Start&&Raw[V]<Start+Count;All&=IsBody;Any|=IsBody;}
        if(Any&&!All){Error=TEXT("Polygon crosses body/source part boundary");return false;}if(!All)continue;
        TArray<FClothVertex> P;for(FVertexInstanceID Instance:D.GetTriangleVertexInstances(T))
        {const FVertexID V=D.GetVertexInstanceVertex(Instance);FClothVertex C;C.P=A.GetVertexPositions()[V];C.N=A.GetVertexInstanceNormals()[Instance].GetSafeNormal();auto Settings=WeightSettings();Settings.SetNormalizeType(EBoneWeightNormalizeType::None);C.W=FBoneWeights::Create(A.GetVertexSkinWeights().Get(V),Settings);P.Add(C);}
        P=Clip(P,LowerCut,true);if(!P.IsEmpty())P=Clip(P,UpperCut,false);if(P.Num()<3)continue;
        TArray<int32> Ids;for(const auto& V:P)Ids.Add(Add(V));
        for(int32 I=1;I+1<Ids.Num();++I)
        {FClothTriangle F{Ids[0],Ids[I],Ids[I+1]};if(F[0]==F[1]||F[1]==F[2]||F[2]==F[0]){++DroppedDegenerate;continue;}const auto Cross=FVector3f::CrossProduct(Vertices[F[1]].P-Vertices[F[0]].P,Vertices[F[2]].P-Vertices[F[0]].P);if(Cross.SizeSquared()<1.e-12f){++DroppedDegenerate;continue;}Faces.Add(F);}
    }
    if(Vertices.Num()<1000||Vertices.Num()>2400||Faces.Num()<2500||Faces.Num()>4500||MaxWeightDifference>4.f/65535.f){Error=TEXT("Unexpected native clipping count or incompatible coincident skin weights");return false;}
    const auto Edges=MeshEdges(Faces);TMap<int32,TArray<int32>> Boundary,AllAdj;int32 BadEdges=0;
    for(const auto& P:Edges)
    {const auto& E=P.Value;BadEdges+=E.Count>2||E.SameDirection;AllAdj.FindOrAdd(E.A).Add(E.B);AllAdj.FindOrAdd(E.B).Add(E.A);if(E.Count==1){Boundary.FindOrAdd(E.A).Add(E.B);Boundary.FindOrAdd(E.B).Add(E.A);}}
    int32 BadDegree=0;for(const auto& P:Boundary)BadDegree+=P.Value.Num()!=2;const auto Loops=Connected(Boundary);const auto Components=Connected(AllAdj);
    TArray<TSharedPtr<FJsonValue>> LoopRows;int32 Waist=0,Legs=0;TArray<float> LegCenters;
    for(const auto& Loop:Loops)
    {
        FVector3f Min(FLT_MAX),Max(-FLT_MAX);for(int32 I:Loop){Min=Min.ComponentMin(Vertices[I].P);Max=Max.ComponentMax(Vertices[I].P);}
        const bool Top=FMath::Abs(Min.Z-UpperCut)<.002f&&FMath::Abs(Max.Z-UpperCut)<.002f;
        const bool Bottom=FMath::Abs(Min.Z-LowerCut)<.002f&&FMath::Abs(Max.Z-LowerCut)<.002f;Waist+=Top;Legs+=Bottom;if(Bottom)LegCenters.Add((Min.X+Max.X)*.5f);
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("vertices"),Loop.Num());R->SetArrayField(TEXT("min_cm"),ModestyXYZ(Min));R->SetArrayField(TEXT("max_cm"),ModestyXYZ(Max));R->SetStringField(TEXT("role"),Top?TEXT("waist"):Bottom?TEXT("leg"):TEXT("unexpected_hole"));LoopRows.Add(MakeShared<FJsonValueObject>(R));
    }
    J->SetNumberField(TEXT("clipped_vertices"),Vertices.Num());J->SetNumberField(TEXT("clipped_triangles"),Faces.Num());J->SetNumberField(TEXT("max_welded_weight_difference"),MaxWeightDifference);J->SetNumberField(TEXT("discarded_zero_area_clip_triangles"),DroppedDegenerate);J->SetNumberField(TEXT("body_surface_components"),Components.Num());J->SetArrayField(TEXT("actual_cut_boundary_loops"),LoopRows);
    if(BadEdges||BadDegree||Components.Num()!=1||Loops.Num()!=3||Waist!=1||Legs!=2||LegCenters.Num()!=2||LegCenters[0]*LegCenters[1]>=0)
    {Error=TEXT("Expected connected closed-crotch pants surface with exactly waist + left/right leg boundaries; no extra anatomical holes");return false;}
    return true;
}
#endif
}
UHCM5VS2HeroModestyComponent::UHCM5VS2HeroModestyComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;}
void UHCM5VS2HeroModestyComponent::BeginPlay()
{
    Super::BeginPlay();const auto* Character=Cast<ACharacter>(GetOwner());Body=Character?Character->GetMesh():nullptr;
    if(!Body||!Body->GetSkeletalMeshAsset()||!SafetyShorts||!ClothMaterial||SafetyShorts->GetSkeleton()!=Body->GetSkeletalMeshAsset()->GetSkeleton()||ClothMaterial->GetBlendMode()!=BLEND_Opaque)
    {UE_LOG(LogTemp,Error,TEXT("VS2_MODESTY_INIT_FAILED; candidate not safe for acceptance"));SetComponentTickEnabled(false);return;}
    // SCS already instantiates the owner component as VS2SafetyShorts under
    // this Actor. The runtime mesh must have a distinct explicit UObject name.
    Garment=NewObject<USkeletalMeshComponent>(GetOwner(),TEXT("VS2SafetyShortsMesh"));GetOwner()->AddInstanceComponent(Garment);
    Garment->SetupAttachment(Body);Garment->SetRelativeTransform(FTransform::Identity);Garment->SetSkeletalMeshAsset(SafetyShorts);Garment->SetMaterial(0,ClothMaterial);
    Garment->SetCollisionEnabled(ECollisionEnabled::NoCollision);Garment->SetGenerateOverlapEvents(false);Garment->SetCanEverAffectNavigation(false);Garment->SetCastShadow(true);Garment->bUseAttachParentBound=true;
    Garment->SetForcedLOD(1);Garment->SetLeaderPoseComponent(Body,true,false);Garment->RegisterComponent();AddTickPrerequisiteComponent(Body);
}
void UHCM5VS2HeroModestyComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime,TickType,Function);if(!Body||!Garment)return;
    const bool Visible=Body->IsVisible()&&!Body->bHiddenInGame&&!GetOwner()->IsHidden();Garment->SetVisibility(Visible);Garment->SetHiddenInGame(!Visible);Garment->SetOwnerNoSee(Body->bOwnerNoSee);Garment->SetOnlyOwnerSee(Body->bOnlyOwnerSee);
}
void UHCM5VS2HeroModestyComponent::EndPlay(const EEndPlayReason::Type Reason)
{if(Garment){Garment->DestroyComponent();Garment=nullptr;}Body=nullptr;Super::EndPlay(Reason);}
FString UHCM5VS2HeroModestyComponent::GetModestyDiagnostics() const
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("body"),GetPathNameSafe(Body));J->SetStringField(TEXT("garment"),GetPathNameSafe(Garment));J->SetStringField(TEXT("mesh"),GetPathNameSafe(SafetyShorts));J->SetStringField(TEXT("material"),GetPathNameSafe(ClothMaterial));
    J->SetBoolField(TEXT("allocated"),Garment!=nullptr);J->SetBoolField(TEXT("leader_is_actual_body"),Garment&&Garment->LeaderPoseComponent.Get()==Body);J->SetBoolField(TEXT("same_skeleton"),Garment&&Body&&Garment->GetSkeletalMeshAsset()->GetSkeleton()==Body->GetSkeletalMeshAsset()->GetSkeleton());
    J->SetBoolField(TEXT("opaque"),ClothMaterial&&ClothMaterial->GetBlendMode()==BLEND_Opaque);J->SetBoolField(TEXT("visible"),Garment&&Garment->IsVisible()&&!Garment->bHiddenInGame);J->SetBoolField(TEXT("no_collision"),Garment&&Garment->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    J->SetStringField(TEXT("coverage_acceptance"),TEXT("NOT_ESTABLISHED_BY_TOPOLOGY; evaluate actual skinned low-angle frames"));return ModestyJSON(J);
}
FString UHCM5VS2HeroModestyEditor::ProbeBody(USkeletalMesh* Source)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));J->SetBoolField(TEXT("asset_mutated"),false);
#if WITH_EDITOR
    const FMeshDescription* D=nullptr;int32 Start=0,Count=0;FString Error;TArray<FClothVertex> V;TArray<FClothTriangle> F;
    if(BodySource(Source,D,Start,Count,J,Error)&&BuildClipped(*D,Start,Count,V,F,J,Error))J->SetStringField(TEXT("status"),TEXT("PASS"));else J->SetStringField(TEXT("error"),Error);
#else
    J->SetStringField(TEXT("error"),TEXT("Editor only"));
#endif
    return ModestyJSON(J);
}
FString UHCM5VS2HeroModestyEditor::BuildSafetyShorts(USkeletalMesh* Source,USkeletalMesh* Target,UMaterialInterface* Material)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));J->SetBoolField(TEXT("saved_by_helper"),false);J->SetBoolField(TEXT("asset_mutated"),false);
    auto Fail=[&](const FString& Error){J->SetStringField(TEXT("error"),Error);return ModestyJSON(J);};
#if WITH_EDITOR
    const FMeshDescription* D=nullptr;int32 Start=0,Count=0;FString Error;
    if(!BodySource(Source,D,Start,Count,J,Error))return Fail(Error);
    if(!Target||Target==Source||!PrivateModesty(Target)||!PrivateModesty(Material)||Target->GetSkeleton()!=Source->GetSkeleton()||Material->GetBlendMode()!=BLEND_Opaque||Target->GetLODNum()!=1)
        return Fail(TEXT("Fresh same-skeleton private mesh and opaque private cloth material required"));
    const uint32 SourceCRC=GeometryCRC(*D);const FReferenceSkeleton OriginalRef=Source->GetRefSkeleton();
    TArray<FClothVertex> V;TArray<FClothTriangle> F;if(!BuildClipped(*D,Start,Count,V,F,J,Error))return Fail(Error);
    const auto OpenEdges=MeshEdges(F);const int32 LayerVertices=V.Num();TArray<FClothTriangle> Closed=F;
    for(const auto& Face:F)Closed.Add(FClothTriangle{Face[2]+LayerVertices,Face[1]+LayerVertices,Face[0]+LayerVertices});
    int32 BoundaryBridges=0;for(const auto& P:OpenEdges)if(P.Value.Count==1)
    {const int32 A=P.Value.A,B=P.Value.B;Closed.Add(FClothTriangle{B,A,A+LayerVertices});Closed.Add(FClothTriangle{B,A+LayerVertices,B+LayerVertices});++BoundaryBridges;}
    const auto ClosedEdges=MeshEdges(Closed);int32 Bad=0;for(const auto& P:ClosedEdges)Bad+=P.Value.Count!=2||P.Value.SameDirection;
    const int32 Euler=LayerVertices*2-ClosedEdges.Num()+Closed.Num();
    if(Bad||Euler!=-2)return Fail(TEXT("Final thick pants must be one watertight manifold with three joined rim tunnels (Euler -2); no open waist/cuffs/crotch geometry"));
    FMeshDescription Out;FSkeletalMeshAttributes A(Out);A.Register();A.GetVertexInstanceUVs().SetNumChannels(1);
    const FSkeletalMeshConstAttributes SourceA(*D);if(!SourceA.HasBones())return Fail(TEXT("Source native bone attributes unavailable"));
    for(FBoneID Bone:SourceA.Bones().GetElementIDs())
    {A.CreateBone(Bone);A.GetBoneNames()[Bone]=SourceA.GetBoneNames()[Bone];A.GetBoneParentIndices()[Bone]=SourceA.GetBoneParentIndices()[Bone];A.GetBonePoses()[Bone]=SourceA.GetBonePoses()[Bone];}
    const FPolygonGroupID Group=Out.CreatePolygonGroup();A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("HC_SafetyShorts_Cloth");
    TArray<FVertexID> IDs;TArray<FVector3f> Positions;
    for(int32 Layer=0;Layer<2;++Layer)for(const auto& Item:V)
    {
        if(Item.P.ContainsNaN()||Item.N.ContainsNaN()||Item.N.IsNearlyZero()||Item.W.Num()==0||Item.W.Num()>12)return Fail(TEXT("Invalid generated position/normal/weights"));
        float Sum=0;for(const auto& Weight:Item.W){Sum+=Weight.GetWeight();if(Weight.GetBoneIndex()>=SourceA.GetNumBones())return Fail(TEXT("Clipped skin index outside exact source bone map"));}
        if(FMath::Abs(Sum-1.f)>.0002f)return Fail(TEXT("Interpolated weights not normalized within integer quantization"));
        const FVertexID ID=Out.CreateVertex();const FVector3f Position=Item.P+Item.N*(Layer==0?OuterOffset:InnerOffset);
        A.GetVertexPositions()[ID]=Position;A.GetVertexSkinWeights().Set(ID,Item.W);IDs.Add(ID);Positions.Add(Position);
    }
    int32 FaceIndex=0;
    for(const auto& Face:Closed)
    {
        const bool Outer=FaceIndex<F.Num(),Inner=FaceIndex>=F.Num()&&FaceIndex<2*F.Num();
        // SkeletalMeshOperations inherits StaticMeshOperations: UE triangle
        // normals use edge2.Cross(edge1). Only rims consume this normal.
        const FVector3f GeometricNormal=FVector3f::CrossProduct(Positions[Face[2]]-Positions[Face[0]],Positions[Face[1]]-Positions[Face[0]]).GetSafeNormal();
        if(GeometricNormal.IsNearlyZero())return Fail(TEXT("Zero-area generated garment triangle"));
        TArray<FVertexInstanceID> Instances;
        for(int32 Index:Face)
        {
            const FVertexInstanceID Instance=Out.CreateVertexInstance(IDs[Index]);const auto& Base=V[Index%LayerVertices];
            const FVector3f Normal=Outer?Base.N:Inner?-Base.N:GeometricNormal;
            FVector3f Tangent=FVector3f(1,0,0)-Normal*Normal.X;if(Tangent.IsNearlyZero())Tangent=FVector3f(0,1,0)-Normal*Normal.Y;
            A.GetVertexInstanceNormals()[Instance]=Normal;A.GetVertexInstanceTangents()[Instance]=Tangent.GetSafeNormal();A.GetVertexInstanceBinormalSigns()[Instance]=1;
            A.GetVertexInstanceColors()[Instance]=FVector4f(1,1,1,1);A.GetVertexInstanceUVs().Set(Instance,0,FVector2f(Base.P.X*.02f,Base.P.Z*.02f));Instances.Add(Instance);
        }
        Out.CreatePolygon(Group,Instances);++FaceIndex;
    }
    const uint32 PlannedCRC=GeometryCRC(Out);Target->Modify();Target->UnregisterAllMorphTarget();Target->SetPhysicsAsset(nullptr);
    Target->GetMaterials().Reset();Target->GetMaterials().Add(FSkeletalMaterial(Material,true,false,TEXT("HC_SafetyShorts_Cloth"),TEXT("HC_SafetyShorts_Cloth")));
    Target->CreateMeshDescription(0,MoveTemp(Out));J->SetBoolField(TEXT("asset_mutated"),true);
    if(!Target->CommitMeshDescription(0))return Fail(TEXT("Native garment description commit failed; do not save"));
    Target->Build();TArray<USkinnedAsset*> Pending{Target};FSkinnedAssetCompilingManager::Get().FinishCompilation(Pending);
    const auto* Actual=Target->GetMeshDescription(0);if(!Actual||GeometryCRC(*Actual)!=PlannedCRC||GeometryCRC(*D)!=SourceCRC)return Fail(TEXT("Build changed planned garment/source geometry; do not save"));
    bool RefSame=Target->GetSkeleton()==Source->GetSkeleton()&&Target->GetRefSkeleton().GetNum()==OriginalRef.GetNum();
    if(RefSame)for(int32 I=0;I<OriginalRef.GetNum();++I)RefSame&=Target->GetRefSkeleton().GetBoneName(I)==OriginalRef.GetBoneName(I)&&Target->GetRefSkeleton().GetParentIndex(I)==OriginalRef.GetParentIndex(I)&&Target->GetRefSkeleton().GetRefBonePose()[I].Equals(OriginalRef.GetRefBonePose()[I],0.f);
    if(!RefSame||Target->GetMorphTargets().Num()!=0||Target->GetPhysicsAsset()!=nullptr)return Fail(TEXT("Private layer bone map changed or inherited facial/physics payload survived"));
    const auto* Render=Target->GetResourceForRendering();if(!Render||Render->LODRenderData.Num()!=1)return Fail(TEXT("One compiled garment LOD required"));
    uint32 RenderTriangles=0;for(const auto& Section:Render->LODRenderData[0].RenderSections){RenderTriangles+=Section.NumTriangles;if(Section.MaterialIndex!=0)return Fail(TEXT("Garment section material mismatch"));}
    if(RenderTriangles!=uint32(Closed.Num()))return Fail(TEXT("Compiled topology does not match closed planned shell"));
    J->SetNumberField(TEXT("geometry_crc"),PlannedCRC);J->SetNumberField(TEXT("garment_vertices"),IDs.Num());J->SetNumberField(TEXT("garment_triangles"),Closed.Num());J->SetNumberField(TEXT("closed_edge_count"),ClosedEdges.Num());J->SetNumberField(TEXT("boundary_bridges"),BoundaryBridges);J->SetNumberField(TEXT("euler_characteristic"),Euler);J->SetNumberField(TEXT("open_or_nonmanifold_edges"),Bad);J->SetNumberField(TEXT("render_triangles"),RenderTriangles);J->SetNumberField(TEXT("render_vertices"),Render->LODRenderData[0].GetNumVertices());
    J->SetBoolField(TEXT("reference_skeleton_preserved"),RefSame);J->SetBoolField(TEXT("source_geometry_preserved"),true);J->SetStringField(TEXT("weight_policy"),TEXT("Original body weights at retained points; barycentric edge interpolation only at exact horizontal cuts; inner/outer/rim share identical copied endpoint weights. No rigid tubes or new skirt simulation."));
    J->SetStringField(TEXT("coverage_boundary"),TEXT("Closed source-derived crotch and thin solid waist/leg rims. Static topology cannot prove no pose-dependent body intersection; low-angle and flight runtime required."));
    Target->MarkPackageDirty();J->SetStringField(TEXT("status"),TEXT("PASS"));return ModestyJSON(J);
#else
    return Fail(TEXT("Editor only"));
#endif
}
FString UHCM5VS2HeroModestyEditor::InspectSafetyShorts(USkeletalMesh* Source,USkeletalMesh* Target)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));auto Fail=[&](const FString& E){J->SetStringField(TEXT("error"),E);return ModestyJSON(J);};
#if WITH_EDITOR
    if(!Source||!Target||!PrivateModesty(Target)||Target->GetSkeleton()!=Source->GetSkeleton()||Target->GetLODNum()!=1)return Fail(TEXT("Private same-skeleton LOD0 garment required"));
    TArray<USkinnedAsset*> Pending{Target};FSkinnedAssetCompilingManager::Get().FinishCompilation(Pending);const auto* D=Target->GetMeshDescription(0);if(!D)return Fail(TEXT("No native garment description"));
    TArray<FClothTriangle> Faces;TMap<int32,TArray<int32>> Adj;
    for(FTriangleID T:D->Triangles().GetElementIDs()){const auto V=D->GetTriangleVertices(T);Faces.Add(FClothTriangle{V[0].GetValue(),V[1].GetValue(),V[2].GetValue()});}
    const auto Edges=MeshEdges(Faces);int32 Bad=0;for(const auto& P:Edges){Bad+=P.Value.Count!=2||P.Value.SameDirection;Adj.FindOrAdd(P.Value.A).Add(P.Value.B);Adj.FindOrAdd(P.Value.B).Add(P.Value.A);}
    const int32 Euler=D->Vertices().Num()-Edges.Num()+Faces.Num();const int32 Components=Connected(Adj).Num();
    const auto& Materials=Target->GetMaterials();const bool Opaque=Materials.Num()==1&&Materials[0].MaterialInterface&&Materials[0].MaterialInterface->GetBlendMode()==BLEND_Opaque;
    J->SetNumberField(TEXT("geometry_crc"),GeometryCRC(*D));J->SetNumberField(TEXT("vertices"),D->Vertices().Num());J->SetNumberField(TEXT("triangles"),Faces.Num());J->SetNumberField(TEXT("open_or_nonmanifold_edges"),Bad);J->SetNumberField(TEXT("euler_characteristic"),Euler);J->SetNumberField(TEXT("connected_components"),Components);J->SetBoolField(TEXT("opaque"),Opaque);J->SetBoolField(TEXT("collision_payload_absent"),Target->GetPhysicsAsset()==nullptr);J->SetNumberField(TEXT("morph_count"),Target->GetMorphTargets().Num());J->SetStringField(TEXT("skeleton"),GetPathNameSafe(Target->GetSkeleton()));J->SetStringField(TEXT("material"),Materials.Num()==1?GetPathNameSafe(Materials[0].MaterialInterface):TEXT(""));
    if(Bad||Euler!=-2||Components!=1||!Opaque||Target->GetPhysicsAsset()||Target->GetMorphTargets().Num()!=0)return Fail(TEXT("Saved garment closed topology/material isolation failed"));
    J->SetStringField(TEXT("status"),TEXT("PASS"));J->SetStringField(TEXT("runtime"),TEXT("NOT_RUN"));return ModestyJSON(J);
#else
    return Fail(TEXT("Editor only"));
#endif
}
FString UHCM5VS2HeroModestyEditor::ConfigureModesty(UBlueprint* Blueprint,USkeletalMesh* Shorts,UMaterialInterface* Material)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));J->SetBoolField(TEXT("saved_by_helper"),false);
#if WITH_EDITOR
    if(!PrivateModesty(Blueprint)||!PrivateModesty(Shorts)||!PrivateModesty(Material)||!Blueprint->GeneratedClass||!Blueprint->GeneratedClass->IsChildOf(AHCM1Character::StaticClass())||!Blueprint->SimpleConstructionScript||Material->GetBlendMode()!=BLEND_Opaque)return ModestyJSON(J);
    for(USCS_Node* Node:Blueprint->SimpleConstructionScript->GetAllNodes())if(Node->ComponentClass==UHCM5VS2HeroModestyComponent::StaticClass()||Node->GetVariableName()==TEXT("VS2SafetyShorts")){J->SetStringField(TEXT("error"),TEXT("Existing garment; no double binding"));return ModestyJSON(J);}
    Blueprint->Modify();Blueprint->SimpleConstructionScript->Modify();USCS_Node* Node=Blueprint->SimpleConstructionScript->CreateNode(UHCM5VS2HeroModestyComponent::StaticClass(),TEXT("VS2SafetyShorts"));auto* Component=Node?Cast<UHCM5VS2HeroModestyComponent>(Node->ComponentTemplate):nullptr;if(!Component)return ModestyJSON(J);
    Component->SafetyShorts=Shorts;Component->ClothMaterial=Material;Blueprint->SimpleConstructionScript->AddNode(Node);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);FKismetEditorUtilities::CompileBlueprint(Blueprint);if(Blueprint->Status==BS_Error)return ModestyJSON(J);
    Blueprint->MarkPackageDirty();J->SetStringField(TEXT("status"),TEXT("PASS"));J->SetStringField(TEXT("component"),Component->GetPathName());J->SetStringField(TEXT("runtime"),TEXT("NOT_RUN"));
#else
    J->SetStringField(TEXT("error"),TEXT("Editor only"));
#endif
    return ModestyJSON(J);
}
