#include "HCM5VS2NPCIdleEditor.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LookAt.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace HCSceneIdle
{
void CS(const FReferenceSkeleton& Ref,const TArray<FTransform>& Local,TArray<FTransform>& Out)
{Out.SetNum(Local.Num());for(int32 I=0;I<Local.Num();++I){const int32 P=Ref.GetParentIndex(I);Out[I]=P<0?Local[I]:Local[I]*Out[P];}}
bool Aim(const FReferenceSkeleton& Ref,TArray<FTransform>& Pose,const TCHAR* Bone,const TCHAR* Child,const FVector& Direction)
{
    const int32 I=Ref.FindBoneIndex(Bone),J=Ref.FindBoneIndex(Child);if(I<0||J<0||Ref.GetParentIndex(J)!=I)return false;
    TArray<FTransform> World;CS(Ref,Pose,World);
    const FVector Old=(World[J].GetLocation()-World[I].GetLocation()).GetSafeNormal();if(Old.IsNearlyZero())return false;
    const FQuat New=(FQuat::FindBetweenNormals(Old,Direction.GetSafeNormal())*World[I].GetRotation()).GetNormalized();
    Pose[I].SetRotation((World[Ref.GetParentIndex(I)].GetRotation().Inverse()*New).GetNormalized());return true;
}
}

namespace
{
FString IdlePackage(const UObject* Obj){return Obj?Obj->GetOutermost()->GetName():FString();}
FString IdleJson(const TSharedRef<FJsonObject>& Result)
{FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));return Text;}
FString IdleFinish(const TSharedRef<FJsonObject>& Result,const FString& Error=FString())
{
    Result->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    Result->SetBoolField(TEXT("saved_by_helper"),false);
    if(!Error.IsEmpty())Result->SetStringField(TEXT("error"),Error);
    return IdleJson(Result);
}
bool IdleScope(const UObject* Obj,FString& Letter,FString& AvatarRoot)
{
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_"),Path=IdlePackage(Obj);
    if(!Obj||!Path.StartsWith(Prefix)||Path.Len()<=Prefix.Len()+2)return false;
    Letter=Path.Mid(Prefix.Len(),1);AvatarRoot=Prefix+Letter;
    return FString(TEXT("QRJTUVWX")).Contains(Letter)&&Path.Mid(Prefix.Len()+1,1)==TEXT("/");
}
UAnimGraphNode_BlendSpacePlayer* IdlePlayer(UAnimBlueprint* BP,TSharedRef<FJsonObject> Result,FString& Error)
{
    FString Letter,AvatarRoot;
    if(!IdleScope(BP,Letter,AvatarRoot)||(!IdlePackage(BP).StartsWith(AvatarRoot+TEXT("/Runtime_"))
        &&!IdlePackage(BP).StartsWith(AvatarRoot+TEXT("/IdleR2/Batch_"))))
    {Error=TEXT("Only an existing NPC runtime or isolated IdleR2 animation blueprint may be read");return nullptr;}
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* Graph=nullptr;
    for(UEdGraph* Candidate:Graphs)if(Candidate&&Candidate->GetFName()==TEXT("AnimGraph"))
    {if(Graph){Error=TEXT("Ambiguous AnimGraph");return nullptr;}Graph=Candidate;}
    if(!Graph){Error=TEXT("Missing AnimGraph");return nullptr;}
    UAnimGraphNode_BlendSpacePlayer* Player=nullptr;int32 LookCount=0,SlotCount=0;
    TArray<UEdGraphNode*> Nodes;for(UEdGraphNode* Node:Graph->Nodes)if(Node)Nodes.Add(Node);
    Nodes.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.GetName()<B.GetName();});
    TArray<TSharedPtr<FJsonValue>> Rows;TSet<FString> Names;
    for(UEdGraphNode* Node:Nodes)
    {
        if(Names.Contains(Node->GetName())){Error=TEXT("Duplicate native graph node name");return nullptr;}Names.Add(Node->GetName());
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Node->GetName());Row->SetStringField(TEXT("class_name"),Node->GetClass()->GetName());
        TArray<FString> Pins;
        for(const UEdGraphPin* Pin:Node->Pins)if(Pin)
        {
            TArray<FString> Links;
            for(const UEdGraphPin* Link:Pin->LinkedTo)if(Link&&Link->GetOwningNode())Links.Add(Link->GetOwningNode()->GetName()+TEXT(":")+Link->PinName.ToString());
            Links.Sort();
            Pins.Add(FString::Printf(TEXT("%s|%d|%s|%s|%s|%s|%s"),*Pin->PinName.ToString(),int32(Pin->Direction),*Pin->PinType.PinCategory.ToString(),
                *Pin->DefaultValue,*GetPathNameSafe(Pin->DefaultObject),*Pin->DefaultTextValue.ToString(),*FString::Join(Links,TEXT(","))));
        }
        Pins.Sort();TArray<TSharedPtr<FJsonValue>> PinRows;for(const FString& Pin:Pins)PinRows.Add(MakeShared<FJsonValueString>(Pin));
        Row->SetArrayField(TEXT("pins"),PinRows);Rows.Add(MakeShared<FJsonValueObject>(Row));
        if(auto* Found=Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
        {if(Player||Found->GetFName()!=TEXT("NPC_Locomotion")){Error=TEXT("Not one exact NPC_Locomotion player");return nullptr;}Player=Found;}
        if(Cast<UAnimGraphNode_LookAt>(Node))++LookCount;
        if(auto* Slot=Cast<UAnimGraphNode_Slot>(Node))
        {if(Slot->GetFName()!=TEXT("NPC_FullBody")||Slot->Node.SlotName!=TEXT("FullBody")||!Slot->Node.bAlwaysUpdateSourcePose)
            {Error=TEXT("Original FullBody slot contract differs");return nullptr;}++SlotCount;}
    }
    if(!Player||LookCount!=3||SlotCount!=1){Error=TEXT("Original locomotion/head/eyes/FullBody graph is incomplete");return nullptr;}
    auto* Space=Cast<UBlendSpace1D>(Player->Node.GetBlendSpace());
    if(!Space||!BP->TargetSkeleton||Space->GetSkeleton()!=BP->TargetSkeleton||Space->GetNumberOfBlendSamples()!=3)
    {Error=TEXT("Missing compatible three-sample BlendSpace1D");return nullptr;}
    TArray<TSharedPtr<FJsonValue>> Samples;
    for(int32 I=0;I<3;++I)
    {
        const FBlendSample& Sample=Space->GetBlendSample(I);
        if(!Sample.Animation||Sample.Animation->GetSkeleton()!=BP->TargetSkeleton||Sample.SampleValue!=FVector(I*140.,0,0))
        {Error=TEXT("Original three speed coordinates or compatible sample skeletons differ");return nullptr;}
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("animation"),IdlePackage(Sample.Animation));
        Row->SetArrayField(TEXT("sample_value"),{MakeShared<FJsonValueNumber>(Sample.SampleValue.X),MakeShared<FJsonValueNumber>(Sample.SampleValue.Y),MakeShared<FJsonValueNumber>(Sample.SampleValue.Z)});
        Row->SetNumberField(TEXT("rate_scale"),Sample.RateScale);Row->SetBoolField(TEXT("mirror"),Sample.bMirror);
        Row->SetBoolField(TEXT("use_single_frame_for_blending"),Sample.bUseSingleFrameForBlending);Row->SetNumberField(TEXT("frame_index_to_sample"),Sample.FrameIndexToSample);
        Samples.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetStringField(TEXT("blueprint"),IdlePackage(BP));Result->SetStringField(TEXT("blendspace"),IdlePackage(Space));
    Result->SetStringField(TEXT("skeleton"),IdlePackage(BP->TargetSkeleton));Result->SetArrayField(TEXT("nodes"),Rows);Result->SetArrayField(TEXT("samples"),Samples);
    return Player;
}
bool IdleSameGraph(const TSharedRef<FJsonObject>& A,const TSharedRef<FJsonObject>& B)
{
    auto AV=MakeShared<FJsonObject>(),BV=MakeShared<FJsonObject>();
    AV->SetArrayField(TEXT("nodes"),A->GetArrayField(TEXT("nodes")));BV->SetArrayField(TEXT("nodes"),B->GetArrayField(TEXT("nodes")));
    return IdleJson(AV)==IdleJson(BV);
}
}
#endif

FString UHCM5VS2NPCIdleEditor::InspectIdleGraph(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
    auto Result=MakeShared<FJsonObject>();FString Error;IdlePlayer(Blueprint,Result,Error);return IdleFinish(Result,Error);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2NPCIdleEditor::AuthorSceneIdle(UAnimSequence* Source,UAnimSequence* Target,FName Activity)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Source||!Target||Source==Target||!Source->GetSkeleton()||Target->GetSkeleton()!=Source->GetSkeleton()
        ||!IdlePackage(Target).StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_"))
        ||!IdlePackage(Target).Contains(TEXT("/IdleR2/Batch_"))||!Target->GetName().StartsWith(TEXT("A_SceneIdle_"))
        ||(Activity!=TEXT("Vendor")&&Activity!=TEXT("SeaGaze")&&Activity!=TEXT("Chat")))
        return IdleFinish(R,TEXT("Unique native NPC scene idle candidate and known activity required"));
    const auto* Model=Source->GetDataModel();const auto& Ref=Source->GetSkeleton()->GetReferenceSkeleton();
    const int32 Count=Model?Model->GetNumberOfKeys():0;
    if(Count<30||Count>1000||Ref.GetRawBoneNum()>500)return IdleFinish(R,TEXT("Bounded source idle required"));
    const TCHAR* Arms[]={TEXT("J_Bip_L_UpperArm"),TEXT("J_Bip_L_LowerArm"),TEXT("J_Bip_R_UpperArm"),TEXT("J_Bip_R_LowerArm")};
    TArray<FTransform> Base=Ref.GetRawRefBonePose();
    for(int32 I=0;I<Base.Num();++I)if(Model->IsValidBoneTrackName(Ref.GetBoneName(I)))Base[I]=Model->GetBoneTrackTransform(Ref.GetBoneName(I),FFrameNumber(0));
    TArray<FTransform> World;HCSceneIdle::CS(Ref,Base,World);
    auto Pos=[&](const TCHAR* N){const int32 I=Ref.FindBoneIndex(N);return I<0?FVector::ZeroVector:World[I].GetLocation();};
    for(const auto* N:Arms)if(Ref.FindBoneIndex(N)<0||!Model->IsValidBoneTrackName(N))return IdleFinish(R,TEXT("Actual mapped arm tracks required"));
    for(const auto* N:{TEXT("J_Bip_C_Hips"),TEXT("J_Bip_C_Head"),TEXT("J_Bip_L_ToeBase"),TEXT("J_Bip_L_Foot"),TEXT("J_Bip_R_ToeBase"),TEXT("J_Bip_R_Foot")})
        if(Ref.FindBoneIndex(N)<0)return IdleFinish(R,TEXT("Actual humanoid basis bones required"));
    const FVector Up=(Pos(TEXT("J_Bip_C_Head"))-Pos(TEXT("J_Bip_C_Hips"))).GetSafeNormal();
    const FVector Forward=FVector::VectorPlaneProject(Pos(TEXT("J_Bip_L_ToeBase"))-Pos(TEXT("J_Bip_L_Foot"))+Pos(TEXT("J_Bip_R_ToeBase"))-Pos(TEXT("J_Bip_R_Foot")),Up).GetSafeNormal();
    const FVector Left=FVector::VectorPlaneProject(Pos(Arms[0])-Pos(Arms[2]),Up).GetSafeNormal();
    if(Up.IsNearlyZero()||Forward.IsNearlyZero()||Left.IsNearlyZero())return IdleFinish(R,TEXT("Degenerate source basis"));
    TArray<TArray<FQuat>> Rotations;Rotations.SetNum(4);for(auto& V:Rotations)V.Reserve(Count);
    for(int32 Frame=0;Frame<Count;++Frame)
    {
        TArray<FTransform> Pose=Base;
        for(int32 I=0;I<Pose.Num();++I)if(Model->IsValidBoneTrackName(Ref.GetBoneName(I)))Pose[I]=Model->GetBoneTrackTransform(Ref.GetBoneName(I),FFrameNumber(Frame));
        const double Wave=FMath::Sin(2.*PI*Frame/double(Count-1));
        auto Dir=[&](double X,double Y,double Z){return (Left*X+Forward*Y+Up*Z).GetSafeNormal();};bool Good=true;
        if(Activity==TEXT("SeaGaze"))
        {
            Good&=HCSceneIdle::Aim(Ref,Pose,Arms[2],Arms[3],Dir(-.8,.32,-.05+Wave*.02));
            Good&=HCSceneIdle::Aim(Ref,Pose,Arms[3],TEXT("J_Bip_R_Hand"),Dir(.55,.20,.81));
        }
        else
        {
            const bool Vendor=Activity==TEXT("Vendor");
            Good&=HCSceneIdle::Aim(Ref,Pose,Arms[2],Arms[3],Dir(-.20,.38,-1));
            Good&=HCSceneIdle::Aim(Ref,Pose,Arms[3],TEXT("J_Bip_R_Hand"),Dir(Vendor?-.10:.25,1.,(Vendor?-.12:.20)+Wave*.09));
            Good&=HCSceneIdle::Aim(Ref,Pose,Arms[0],Arms[1],Dir(.12,.12,-1));
            Good&=HCSceneIdle::Aim(Ref,Pose,Arms[1],TEXT("J_Bip_L_Hand"),Dir(.12,Vendor?.35:.55,Vendor?-.8:-.55));
        }
        if(!Good)return IdleFinish(R,TEXT("Unexpected source arm hierarchy"));
        for(int32 I=0;I<4;++I)Rotations[I].Add(Pose[Ref.FindBoneIndex(Arms[I])].GetRotation());
    }
    Target->Modify();auto& C=Target->GetController();C.OpenBracket(FText::FromString(TEXT("Original bounded contextual idle arms")),false);
    bool Good=true;double TranslationError=0;
    for(int32 I=0;I<4;++I)
    {
        TArray<FTransform> Original;Model->GetBoneTrackTransforms(Arms[I],Original);TArray<FVector> Positions,Scales;
        for(const auto& T:Original){Positions.Add(T.GetLocation());Scales.Add(T.GetScale3D());}
        Good&=C.SetBoneTrackKeys(Arms[I],Positions,Rotations[I],Scales,false);
    }
    C.CloseBracket(false);if(!Good)return IdleFinish(R,TEXT("Scene idle native track update failed"));
    for(int32 I=0;I<Ref.GetRawBoneNum();++I)
    {
        const FName N=Ref.GetBoneName(I);if(!Model->IsValidBoneTrackName(N))continue;
        TArray<FTransform> Before,After;Model->GetBoneTrackTransforms(N,Before);Target->GetDataModel()->GetBoneTrackTransforms(N,After);
        if(Before.Num()!=After.Num())return IdleFinish(R,TEXT("Source key count changed"));
        const bool Arm=N==Arms[0]||N==Arms[1]||N==Arms[2]||N==Arms[3];
        for(int32 K=0;K<Before.Num();++K)
        {TranslationError=FMath::Max(TranslationError,(Before[K].GetLocation()-After[K].GetLocation()).Size());
            if(!Before[K].GetScale3D().Equals(After[K].GetScale3D(),.0001)||(!Arm&&Before[K].GetRotation().AngularDistance(After[K].GetRotation())>.0001))return IdleFinish(R,TEXT("Non-arm pose was changed"));}
    }
    if(TranslationError>.0001)return IdleFinish(R,TEXT("Segment length changed"));
    Target->PostEditChange();Target->CacheDerivedDataForCurrentPlatform();Target->MarkPackageDirty();
    R->SetStringField(TEXT("activity"),Activity.ToString());R->SetNumberField(TEXT("source_frames"),Count);R->SetNumberField(TEXT("max_translation_error_cm"),TranslationError);
    R->SetStringField(TEXT("scope"),TEXT("Original four-arm-track rotations over existing licensed retargeted idle; legs/root/head/fingers/local lengths unchanged. NOT visual approval."));return IdleFinish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2NPCIdleEditor::ConfigurePrivateIdle(UAnimBlueprint* SourceBlueprint,UAnimBlueprint* PrivateBlueprint,
    UBlendSpace1D* PrivateSpace,UAnimSequence* PrivateIdle,bool Apply)
{
#if WITH_EDITOR
    auto Result=MakeShared<FJsonObject>();FString Letter,AvatarRoot;
    if(!IdleScope(SourceBlueprint,Letter,AvatarRoot)||SourceBlueprint==PrivateBlueprint
        ||!IdlePackage(SourceBlueprint).StartsWith(AvatarRoot+TEXT("/Runtime_"))||!PrivateBlueprint||!PrivateSpace||!PrivateIdle)
        return IdleFinish(Result,TEXT("Original runtime ABP and distinct private idle assets are required"));
    const FString Batch=FPackageName::GetLongPackagePath(IdlePackage(PrivateBlueprint));
    if(!Batch.StartsWith(AvatarRoot+TEXT("/IdleR2/Batch_"))||PrivateBlueprint->GetName()!=TEXT("ABP_NPCIdleR2_")+Letter
        ||IdlePackage(PrivateSpace)!=Batch+TEXT("/BS_NPCIdleR2_")+Letter
        ||(IdlePackage(PrivateIdle)!=Batch+TEXT("/M_Relaxed_Stand_Idle_Loop_InPlace_NPC")+Letter+TEXT("IdleR2")
            &&!(FPackageName::GetLongPackagePath(IdlePackage(PrivateIdle))==Batch&&PrivateIdle->GetName().StartsWith(TEXT("A_SceneIdle_"))))
        ||PrivateBlueprint->TargetSkeleton!=SourceBlueprint->TargetSkeleton||PrivateSpace->GetSkeleton()!=SourceBlueprint->TargetSkeleton
        ||PrivateIdle->GetSkeleton()!=SourceBlueprint->TargetSkeleton||PrivateIdle->IsValidAdditive())
        return IdleFinish(Result,TEXT("Isolated same-batch private ABP/BS/relaxed-loop scope or exact target skeleton mismatch"));
    auto Before=MakeShared<FJsonObject>(),Current=MakeShared<FJsonObject>();FString Error;
    auto* SourcePlayer=IdlePlayer(SourceBlueprint,Before,Error);if(!SourcePlayer)return IdleFinish(Result,Error);
    auto* Player=IdlePlayer(PrivateBlueprint,Current,Error);if(!Player)return IdleFinish(Result,Error);
    auto* SourceSpace=CastChecked<UBlendSpace1D>(SourcePlayer->Node.GetBlendSpace());
    if(!IdleSameGraph(Before,Current)||PrivateSpace==SourceSpace||PrivateSpace->GetNumberOfBlendSamples()!=3)
        return IdleFinish(Result,TEXT("Private duplicate must preserve every original node/pin/default/link and all sample positions"));
    for(int32 I=0;I<3;++I)
    {
        FBlendSample Expected=SourceSpace->GetBlendSample(I);if(!Apply&&I==0)Expected.Animation=PrivateIdle;
        if(!(PrivateSpace->GetBlendSample(I)==Expected))return IdleFinish(Result,TEXT("Unexpected private sample edit before binding/readback"));
    }
    if(Apply)
    {
        if(Player->Node.GetBlendSpace()!=SourceSpace)return IdleFinish(Result,TEXT("Unconfigured private graph must still reference the exact source BlendSpace"));
        PrivateSpace->Modify();
        if(!PrivateSpace->ReplaceSampleAnimation(0,PrivateIdle))return IdleFinish(Result,TEXT("Native sample zero replacement failed"));
        PrivateSpace->ValidateSampleData();PrivateSpace->ResampleData();PrivateSpace->PostEditChange();PrivateSpace->MarkPackageDirty();
        PrivateBlueprint->Modify();Player->Modify();Player->Node.SetBlendSpace(PrivateSpace);
        FBlueprintEditorUtils::MarkBlueprintAsModified(PrivateBlueprint);FKismetEditorUtilities::CompileBlueprint(PrivateBlueprint);
        PrivateBlueprint->MarkPackageDirty();
    }
    if(PrivateBlueprint->Status==BS_Error)return IdleFinish(Result,TEXT("Private animation blueprint compile failed"));
    auto After=MakeShared<FJsonObject>();Player=IdlePlayer(PrivateBlueprint,After,Error);if(!Player)return IdleFinish(Result,Error);
    if(Player->Node.GetBlendSpace()!=PrivateSpace||!IdleSameGraph(Before,After))
        return IdleFinish(Result,TEXT("Native private graph readback or original wiring changed"));
    for(int32 I=0;I<3;++I)
    {FBlendSample Expected=SourceSpace->GetBlendSample(I);if(I==0)Expected.Animation=PrivateIdle;
        if(!(PrivateSpace->GetBlendSample(I)==Expected))return IdleFinish(Result,TEXT("Native final private sample readback differs"));}
    Result->SetObjectField(TEXT("graph"),After);Result->SetBoolField(TEXT("apply"),Apply);
    Result->SetBoolField(TEXT("source_graph_nodes_pins_defaults_links_preserved"),true);
    Result->SetBoolField(TEXT("only_sample_zero_animation_replaced"),true);return IdleFinish(Result);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
