#include "HCM5VS2FootPlacementEditor.h"
#include "AnimGraphNode_HCM5VS2PlacementInput.h"
#include "M5VS2/HCM5VS2LookAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LegIK.h"
#include "AnimGraph/AnimGraphNode_StrideWarping.h"
#include "AnimGraph/AnimGraphNode_FootPlacement.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/SkeletalMesh.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
FString Package(const UObject* Object)
{ return Object ? Object->GetOutermost()->GetName() : FString(); }

FString OwnedBatch(const UObject* Object)
{
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/FootPlacementAB/Batch_");
    const FString Path=Package(Object);
    if (!Path.StartsWith(Prefix) || Path.Len()<Prefix.Len()+14 || Path[Prefix.Len()+12]!='/') return FString();
    const FString Token=Path.Mid(Prefix.Len(),12);
    for (TCHAR Character:Token) if (!FChar::IsHexDigit(Character)) return FString();
    return Prefix+Token;
}

FString Done(const TSharedRef<FJsonObject>& Result,const FString& Error=FString())
{
    Result->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    if (!Error.IsEmpty()) Result->SetStringField(TEXT("error"),Error);
    Result->SetBoolField(TEXT("saved_by_helper"),false);
    Result->SetStringField(TEXT("visible_sliding"),TEXT("NOT_RUN"));
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));return Text;
}

bool SameRaw(const FReferenceSkeleton& A,const FReferenceSkeleton& B)
{
    if (A.GetRawBoneNum()!=247 || B.GetRawBoneNum()!=247) return false;
    for (int32 Index=0;Index<247;++Index)
        if (A.GetRawRefBoneInfo()[Index].Name!=B.GetRawRefBoneInfo()[Index].Name
            || A.GetRawRefBoneInfo()[Index].ParentIndex!=B.GetRawRefBoneInfo()[Index].ParentIndex
            || !A.GetRawRefBonePose()[Index].Equals(B.GetRawRefBonePose()[Index],0.)) return false;
    return true;
}

bool VirtualBonesPreserved(const USkeleton* Source,const USkeleton* Candidate,bool Applied)
{
    const auto& Old=Source->GetVirtualBones();const auto& New=Candidate->GetVirtualBones();
    if (Old.Num()!=3 || New.Num()!=(Applied?4:3)) return false;
    for (int32 Index=0;Index<3;++Index)
        if (Old[Index].SourceBoneName!=New[Index].SourceBoneName || Old[Index].TargetBoneName!=New[Index].TargetBoneName
            || Old[Index].VirtualBoneName!=New[Index].VirtualBoneName) return false;
    return !Applied || (New[3].SourceBoneName==TEXT("Spine") && New[3].TargetBoneName==TEXT("Hips")
        && New[3].VirtualBoneName==TEXT("VB VS2_PlacementFloor"));
}

UEdGraphNode* Named(UAnimBlueprint* Blueprint,FName Name)
{
    UEdGraphNode* Found=nullptr;TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph:Graphs) for (UEdGraphNode* Node:Graph->Nodes)
        if (Node->GetFName()==Name) { if (Found) return nullptr;Found=Node; }
    return Found;
}

UEdGraphPin* Output(UEdGraphNode* Node)
{
    UEdGraphPin* Found=nullptr;
    if (!Node) return nullptr;
    for (UEdGraphPin* Pin:Node->Pins) if (Pin->Direction==EGPD_Output && Pin->PinName!=TEXT("self"))
    { if (Found) return nullptr;Found=Pin; }
    return Found;
}

template<class T> T* Add(UEdGraph* Graph,const TCHAR* Name,int32 X,int32 Y)
{
    T* Node=NewObject<T>(Graph,FName(Name),RF_Transactional);
    Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->PostPlacedNewNode();
    Node->NodePosX=X;Node->NodePosY=Y;return Node;
}

bool Connect(UEdGraph* Graph,UEdGraphPin* A,UEdGraphPin* B)
{ return A && B && Graph->GetSchema()->TryCreateConnection(A,B); }

bool Variable(UEdGraph* Graph,UAnimGraphNode_Base* Target,FName Pin,const TCHAR* Name)
{
    if (!Target->FindPin(Pin))
        for (int32 Index=0;Index<Target->ShowPinForProperties.Num();++Index)
            if (Target->ShowPinForProperties[Index].PropertyName==Pin)
            { Target->SetPinVisibility(true,Index);break; }
    if (!Target->FindPin(Pin)) return false;
    auto* Get=Add<UK2Node_VariableGet>(Graph,Name,Target->NodePosX-180,Target->NodePosY+260);
    Get->VariableReference.SetSelfMember(TEXT("VS2WarpAlpha"));Get->AllocateDefaultPins();
    return Connect(Graph,Output(Get),Target->FindPin(Pin));
}

// The actual native render vertices selected by probe 1d92a26d. This derives
// the reference origin from the shoe, never from an arbitrary Hips offset.
bool SurfaceZ(USkeletalMesh* Mesh,float& Height)
{
    const auto* Data=Mesh->GetResourceForRendering();
    if (!Data || !Data->LODRenderData.IsValidIndex(0)) return false;
    const auto& LOD=Data->LODRenderData[0];
    if (!LOD.StaticVertexBuffers.PositionVertexBuffer.GetVertexData()) return false;
    const int32 Indices[]={62683,62708,63171,62574,62779,62781,64330,64296,64334,64152,64351,64353};
    double Sum=0;
    for (int32 Index:Indices)
    {
        if (uint32(Index)>=LOD.GetNumVertices()) return false;
        int32 Section=INDEX_NONE,Local=INDEX_NONE;LOD.GetSectionFromVertexIndex(Index,Section,Local);
        if (Section!=4 || !LOD.RenderSections.IsValidIndex(Section) || LOD.RenderSections[Section].bDisabled) return false;
        const FVector3f Position=LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index);
        if (Position.ContainsNaN() || Position.Z < -1.25f || Position.Z > -1.f) return false;
        Sum+=Position.Z;
    }
    Height=float(Sum/12.);return true;
}

bool Direct(UEdGraphPin* A,UEdGraphPin* B)
{ return A && B && A->LinkedTo.Num()==1 && B->LinkedTo.Num()==1 && A->LinkedTo[0]==B && B->LinkedTo[0]==A; }

// Everything except the exact replaced edge and four authored nodes must retain
// identical node classes, input defaults, and graph links after compilation.
FString RetainedWiring(UAnimBlueprint* Blueprint)
{
    TArray<FString> Lines;TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph:Graphs) for (UEdGraphNode* Node:Graph->Nodes)
    {
        if (Node->GetName().StartsWith(TEXT("VS2Placement_"))) continue;
        const FString Key=Graph->GetName()+TEXT("|")+Node->GetName()+TEXT("|")+Node->GetClass()->GetName();
        Lines.Add(Key);
        for (UEdGraphPin* Pin:Node->Pins)
        {
            Lines.Add(Key+TEXT("|")+Pin->PinName.ToString()+TEXT("=")+Pin->DefaultValue);
            for (UEdGraphPin* Link:Pin->LinkedTo)
            {
                const FString Other=Link->GetOwningNode()->GetName();
                if (Other.StartsWith(TEXT("VS2Placement_"))) continue;
                if ((Node->GetName()==TEXT("VS2Motion_Stride") && Other==TEXT("VS2Motion_LegIK"))
                    || (Node->GetName()==TEXT("VS2Motion_LegIK") && Other==TEXT("VS2Motion_Stride"))) continue;
                Lines.Add(Key+TEXT("|")+Pin->PinName.ToString()+TEXT("->")+Other+TEXT(".")+Link->PinName.ToString());
            }
        }
    }
    Lines.Sort();return FString::Join(Lines,TEXT("\n"));
}
}

FString UHCM5VS2FootPlacementEditor::ConfigureCandidate(UAnimBlueprint* Source,USkeletalMesh* SourceMesh,
    UAnimBlueprint* Candidate,USkeleton* Skeleton,USkeletalMesh* Mesh,bool bApply)
{
    const auto Result=MakeShared<FJsonObject>();
    const FString Batch=OwnedBatch(Candidate);
    if (!Source || !SourceMesh || !Candidate || !Skeleton || !Mesh || Batch.IsEmpty()
        || OwnedBatch(Skeleton)!=Batch || OwnedBatch(Mesh)!=Batch || Candidate==Source || SourceMesh==Mesh
        || Package(Source)!=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_9d25d6562b62/ABP_Selestia_GASMotion_UniformClock")
        || Package(SourceMesh)!=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_1d6685a10359/SKM_Selestia_Warp")
        || !Source->TargetSkeleton || Source->TargetSkeleton!=SourceMesh->GetSkeleton() || Skeleton==Source->TargetSkeleton
        || Source->ParentClass!=UHCM5VS2LookAnimInstance::StaticClass() || Candidate->ParentClass!=Source->ParentClass
        || !SameRaw(SourceMesh->GetRefSkeleton(),Mesh->GetRefSkeleton())
        || !SameRaw(Source->TargetSkeleton->GetReferenceSkeleton(),Skeleton->GetReferenceSkeleton())
        || !VirtualBonesPreserved(Source->TargetSkeleton,Skeleton,!bApply))
        return Done(Result,TEXT("Exact approved Clock source, private fresh duplicates and unchanged 247 raw bones required"));
    auto* SourceLoop=Cast<UAnimGraphNode_BlendSpacePlayer>(Named(Source,TEXT("VS2Motion_Loop")));
    auto* Loop=Cast<UAnimGraphNode_BlendSpacePlayer>(Named(Candidate,TEXT("VS2Motion_Loop")));
    auto* Stride=Cast<UAnimGraphNode_StrideWarping>(Named(Candidate,TEXT("VS2Motion_Stride")));
    auto* Legs=Cast<UAnimGraphNode_LegIK>(Named(Candidate,TEXT("VS2Motion_LegIK")));
    if (!SourceLoop || !Loop || !Stride || !Legs || !SourceLoop->Node.GetBlendSpace()
        || Loop->Node.GetBlendSpace()!=SourceLoop->Node.GetBlendSpace()
        || Loop->Node.GetBlendSpace()->GetBlendSamples().Num()!=28
        || RetainedWiring(Source)!=RetainedWiring(Candidate))
        return Done(Result,TEXT("Unchanged graph and identical 28-sample BlendSpace reference required"));
    float GroundZ=0,CandidateZ=0;
    if (!SurfaceZ(SourceMesh,GroundZ) || !SurfaceZ(Mesh,CandidateZ) || GroundZ!=CandidateZ)
        return Done(Result,TEXT("Actual source/candidate twelve shoe vertices or reference surface changed"));
    UEdGraph* Graph=Stride->GetGraph();
    auto* Prep=Cast<UAnimGraphNode_HCM5VS2PlacementInput>(Named(Candidate,TEXT("VS2Placement_Prepare")));
    auto* Placement=Cast<UAnimGraphNode_FootPlacement>(Named(Candidate,TEXT("VS2Placement_Native")));
    if (bApply)
    {
        if (Prep || Placement || !Direct(Output(Stride),Legs->FindPin(TEXT("ComponentPose")))
            || Candidate->TargetSkeleton!=Source->TargetSkeleton || Mesh->GetSkeleton()!=SourceMesh->GetSkeleton())
            return Done(Result,TEXT("Apply accepts fresh unbound copies only"));
        Skeleton->Modify();Mesh->Modify();Candidate->Modify();Graph->Modify();
        // UE rejects an existing source/target pair even under a new VB name.
        // Hips->Hips is already VS2_IKRoot. This unique Spine->Hips reference
        // is only a storage slot; Prep replaces its full component transform.
        if (Skeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("Spine"))==INDEX_NONE
            || Skeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("Hips"))==INDEX_NONE)
            return Done(Result,TEXT("Actual Spine/Hips reference pair is absent"));
        if (!Skeleton->AddNewNamedVirtualBone(TEXT("Spine"),TEXT("Hips"),TEXT("VB VS2_PlacementFloor")))
            return Done(Result,TEXT("Independent virtual reference creation failed"));
        Skeleton->AddCompatibleSkeleton(Source->TargetSkeleton);
        Mesh->SetSkeleton(Skeleton);Mesh->GetRefSkeleton().RebuildRefSkeleton(Skeleton,true);
        Skeleton->SetPreviewMesh(Mesh);Candidate->TargetSkeleton=Skeleton;Candidate->SetPreviewMesh(Mesh);
        Prep=Add<UAnimGraphNode_HCM5VS2PlacementInput>(Graph,TEXT("VS2Placement_Prepare"),-1430,1700);
        Prep->Node.FloorReference.BoneName=TEXT("VB VS2_PlacementFloor");
        Prep->Node.FootL.BoneName=TEXT("Foot_L");Prep->Node.FootR.BoneName=TEXT("Foot_R");
        Prep->Node.BallL.BoneName=TEXT("Toe_L");Prep->Node.BallR.BoneName=TEXT("Toe_R");
        Prep->Node.GoalL.BoneName=TEXT("VB VS2_IKFoot_L");Prep->Node.GoalR.BoneName=TEXT("VB VS2_IKFoot_R");
        Prep->Node.ReferenceSurfaceZ=GroundZ;Prep->Node.Alpha=1.f;Prep->AllocateDefaultPins();
        Placement=Add<UAnimGraphNode_FootPlacement>(Graph,TEXT("VS2Placement_Native"),-1250,1700);
        Placement->Node.PlantSpeedMode=EWarpingEvaluationMode::Manual;
        Placement->Node.IKFootRootBone.BoneName=TEXT("VB VS2_PlacementFloor");
        Placement->Node.PelvisBone.BoneName=TEXT("Hips");
        Placement->Node.PlantSettings.LockType=EFootPlacementLockType::PivotAroundBall;
        // compact0 is Hips. Native smooth-root would emit Hips twice; leave off.
        Placement->Node.InterpolationSettings.bSmoothRootBone=false;
        for (const TCHAR* Side:{TEXT("L"),TEXT("R")})
        {
            FFootPlacemenLegDefinition Leg;
            Leg.FKFootBone.BoneName=FName(*(FString(TEXT("Foot_"))+Side));
            Leg.BallBone.BoneName=FName(*(FString(TEXT("Toe_"))+Side));
            Leg.IKFootBone.BoneName=FName(*(FString(TEXT("VB VS2_IKFoot_"))+Side));
            Leg.NumBonesInLimb=2;Leg.SpeedCurveName=FName(*(FString(TEXT("VS2PlacementSpeed_"))+Side));
            Leg.DisableLockCurveName=TEXT("VS2PlacementDisableLock");Placement->Node.LegDefinitions.Add(Leg);
        }
        Placement->AllocateDefaultPins();
        Output(Stride)->BreakLinkTo(Legs->FindPin(TEXT("ComponentPose")));
        if (!Connect(Graph,Output(Stride),Prep->FindPin(TEXT("ComponentPose")))
            || !Connect(Graph,Output(Prep),Placement->FindPin(TEXT("ComponentPose")))
            || !Connect(Graph,Output(Placement),Legs->FindPin(TEXT("ComponentPose")))
            || !Variable(Graph,Prep,TEXT("ActivityAlpha"),TEXT("VS2Placement_GetActivity"))
            || !Variable(Graph,Placement,TEXT("Alpha"),TEXT("VS2Placement_GetAlpha")))
            return Done(Result,TEXT("Private placement graph connection failed"));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Candidate);
        FKismetEditorUtilities::CompileBlueprint(Candidate);
        Candidate->MarkPackageDirty();Skeleton->MarkPackageDirty();Mesh->MarkPackageDirty();
    }
    if (!Prep || !Placement || Candidate->Status==BS_Error || !Candidate->GeneratedClass
        || Candidate->TargetSkeleton!=Skeleton || Mesh->GetSkeleton()!=Skeleton
        || !SameRaw(SourceMesh->GetRefSkeleton(),Mesh->GetRefSkeleton())
        || !SameRaw(Source->TargetSkeleton->GetReferenceSkeleton(),Skeleton->GetReferenceSkeleton())
        || !VirtualBonesPreserved(Source->TargetSkeleton,Skeleton,true)
        || RetainedWiring(Source)!=RetainedWiring(Candidate)
        || Prep->Node.ReferenceSurfaceZ!=GroundZ || Prep->Node.Alpha!=1.f
        || Placement->Node.PlantSettings.LockType!=EFootPlacementLockType::PivotAroundBall
        || Placement->Node.InterpolationSettings.bSmoothRootBone
        || Placement->Node.IKFootRootBone.BoneName!=TEXT("VB VS2_PlacementFloor")
        || Placement->Node.PlantSpeedMode!=EWarpingEvaluationMode::Manual
        || Placement->Node.LegDefinitions.Num()!=2
        || !Direct(Output(Stride),Prep->FindPin(TEXT("ComponentPose")))
        || !Direct(Output(Prep),Placement->FindPin(TEXT("ComponentPose")))
        || !Direct(Output(Placement),Legs->FindPin(TEXT("ComponentPose"))))
        return Done(Result,TEXT("Compiled/reloaded graph, hierarchy or configuration guard failed"));
    for (const FBlendSample& Sample:Loop->Node.GetBlendSpace()->GetBlendSamples())
        if (!Sample.Animation || !Skeleton->IsCompatibleForEditor(Sample.Animation->GetSkeleton()))
            return Done(Result,TEXT("Source clip is not compatible with candidate skeleton"));
    const auto* Defaults=Cast<UHCM5VS2LookAnimInstance>(Candidate->GeneratedClass->GetDefaultObject());
    if (!Defaults || !Defaults->bVS2GASMotionEnabled || !Defaults->bVS2FlightPosesEnabled)
        return Done(Result,TEXT("GAS/Flight CDO flags regressed"));
    Result->SetStringField(TEXT("candidate_blueprint"),Package(Candidate));
    Result->SetStringField(TEXT("candidate_mesh"),Package(Mesh));
    Result->SetStringField(TEXT("candidate_skeleton"),Package(Skeleton));
    Result->SetStringField(TEXT("unchanged_blendspace"),Package(Loop->Node.GetBlendSpace()));
    Result->SetStringField(TEXT("reference_virtual_source"),TEXT("Spine"));
    Result->SetStringField(TEXT("reference_virtual_target"),TEXT("Hips"));
    Result->SetNumberField(TEXT("reference_surface_z_mesh_cm"),GroundZ);
    Result->SetStringField(TEXT("reference_axis"),TEXT("mesh component +Z; XYZ origin=(0,0,mean of frozen twelve reference shoe vertices); not Hips plane or world altitude"));
    Result->SetNumberField(TEXT("raw_bones_exact"),247);Result->SetNumberField(TEXT("old_virtual_bones_preserved"),3);
    Result->SetNumberField(TEXT("compatible_unchanged_samples"),28);
    Result->SetBoolField(TEXT("root_smoothing"),false);
    Result->SetStringField(TEXT("foot_speed_source"),TEXT("current pre-lock IK ball world displacement / true update dt / verified uniform mesh scale; no final-pose feedback"));
    Result->SetNumberField(TEXT("native_speed_threshold_mesh_cm_s"),Placement->Node.PlantSettings.SpeedThreshold);
    Result->SetNumberField(TEXT("native_unalignment_speed_mesh_cm_s"),Placement->Node.PlantSettings.UnalignmentSpeedThreshold);
    Result->SetNumberField(TEXT("native_unplant_radius_mesh_cm"),Placement->Node.PlantSettings.UnplantRadius);
    Result->SetStringField(TEXT("scope"),TEXT("isolated FootPlacement A/B, native defaults with PivotAroundBall; runtime/visual evidence pending"));
    return Done(Result);
}

FString UHCM5VS2FootPlacementEditor::BindPrivateArms(USkeletalMesh* SourceArms,
    USkeletalMesh* CandidateArms,USkeletalMesh* CandidateBody,bool bApply)
{
    const auto Result=MakeShared<FJsonObject>();
    const FString ArmsBatch=OwnedBatch(CandidateArms),BodyBatch=OwnedBatch(CandidateBody);
    if (!SourceArms || !CandidateArms || !CandidateBody || SourceArms==CandidateArms
        || CandidateArms==CandidateBody || ArmsBatch.IsEmpty() || BodyBatch.IsEmpty()
        || Package(SourceArms)!=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia_Arms")
        || Package(CandidateArms)!=ArmsBatch/TEXT("SKM_PlacementArms")
        || Package(CandidateBody)!=BodyBatch/TEXT("SKM_Placement")
        || !CandidateBody->GetSkeleton()
        || Package(CandidateBody->GetSkeleton())!=BodyBatch/TEXT("SK_Placement"))
        return Done(Result,TEXT("Exact original arms and private placement arms/body packages required"));
    USkeleton* Skeleton=CandidateBody->GetSkeleton();
    const auto& BodyRef=CandidateBody->GetRefSkeleton();
    const auto& OriginalRef=SourceArms->GetRefSkeleton();
    if (Skeleton->GetVirtualBones().Num()!=4 || BodyRef.GetNum()!=251
        || !SameRaw(OriginalRef,BodyRef) || !SameRaw(OriginalRef,CandidateArms->GetRefSkeleton()))
        return Done(Result,TEXT("Arms must retain original 247 raw bones and completed placement body must have four virtual bones"));
    for (FName Bone:{FName(TEXT("VB VS2_IKRoot")),FName(TEXT("VB VS2_IKFoot_L")),
        FName(TEXT("VB VS2_IKFoot_R")),FName(TEXT("VB VS2_PlacementFloor"))})
        if (BodyRef.FindBoneIndex(Bone)==INDEX_NONE)
            return Done(Result,TEXT("Completed placement body virtual reference absent"));
    if (bApply)
    {
        if (CandidateArms->GetSkeleton()!=SourceArms->GetSkeleton())
            return Done(Result,TEXT("Apply accepts a fresh original-arms copy only"));
        CandidateArms->Modify();CandidateArms->SetSkeleton(Skeleton);
        CandidateArms->GetRefSkeleton().RebuildRefSkeleton(Skeleton,true);
        CandidateArms->MarkPackageDirty();
    }
    const auto& Ref=CandidateArms->GetRefSkeleton();
    if (CandidateArms->GetSkeleton()!=Skeleton || Ref.GetNum()!=251 || !SameRaw(OriginalRef,Ref))
        return Done(Result,TEXT("Private arms skeleton or raw reference readback differs"));
    for (int32 Index=0;Index<251;++Index)
        if (Ref.GetBoneName(Index)!=BodyRef.GetBoneName(Index)
            || Ref.GetParentIndex(Index)!=BodyRef.GetParentIndex(Index)
            || !Ref.GetRefBonePose()[Index].Equals(BodyRef.GetRefBonePose()[Index],0.))
            return Done(Result,TEXT("Full body/arms reference transforms differ"));
    Result->SetStringField(TEXT("source_arms"),Package(SourceArms));
    Result->SetStringField(TEXT("private_arms"),Package(CandidateArms));
    Result->SetStringField(TEXT("body_mesh"),Package(CandidateBody));
    Result->SetStringField(TEXT("skeleton"),Package(Skeleton));
    Result->SetNumberField(TEXT("raw_bones_exact"),247);
    Result->SetNumberField(TEXT("full_bones_exact"),251);
    return Done(Result);
}
