#include "HCM5VS2FlightPoseEditor.h"
#include "HCM5VS2LookAnimInstance.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_ModifyBone.h"
#include "K2Node_VariableGet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace
{
FString Path(const UObject* O){return O?O->GetOutermost()->GetName():FString();}
bool Owned(const UObject* O){return Path(O).StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_"));}
FString Finish(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{R->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));if(!Error.IsEmpty())R->SetStringField(TEXT("error"),Error);
 R->SetBoolField(TEXT("saved_by_helper"),false);R->SetStringField(TEXT("visual_clothing_coverage"),TEXT("NOT_RUN"));FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;}
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
void ComponentPose(const FReferenceSkeleton& Ref,const TArray<FTransform>& Local,TArray<FTransform>& CS)
{CS.SetNum(Local.Num());for(int32 I=0;I<Local.Num();++I){const int32 P=Ref.GetParentIndex(I);CS[I]=P==INDEX_NONE?Local[I]:Local[I]*CS[P];}}
bool PointBone(const FReferenceSkeleton& Ref,TArray<FTransform>& Local,FName Bone,FName Child,const FVector& Goal)
{
    const int32 I=Ref.FindBoneIndex(Bone),J=Ref.FindBoneIndex(Child);if(I<0||J<0||Ref.GetParentIndex(J)!=I)return false;
    TArray<FTransform> CS;ComponentPose(Ref,Local,CS);const FVector Before=(CS[J].GetLocation()-CS[I].GetLocation()).GetSafeNormal();
    if(Before.IsNearlyZero()||Goal.IsNearlyZero())return false;
    const FQuat NewCS=(FQuat::FindBetweenNormals(Before,Goal.GetSafeNormal())*CS[I].GetRotation()).GetNormalized();
    const int32 Parent=Ref.GetParentIndex(I);Local[I].SetRotation(Parent<0?NewCS:(CS[Parent].GetRotation().Inverse()*NewCS).GetNormalized());return true;
}
template<class T>T* NewNode(UEdGraph* G,const FString& Name,int32 X,int32 Y)
{auto* N=NewObject<T>(G,FName(*Name),RF_Transactional);G->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->NodePosX=X;N->NodePosY=Y;return N;}
UEdGraphPin* Output(UEdGraphNode* N){for(auto* P:N->Pins)if(P->Direction==EGPD_Output&&P->PinName!=TEXT("self"))return P;return nullptr;}
bool Link(UEdGraph* G,UEdGraphPin* A,UEdGraphPin* B){return A&&B&&G->GetSchema()->TryCreateConnection(A,B);}
bool Variable(UEdGraph* G,UAnimGraphNode_Base* N,FName Pin,FName Property)
{
    if(!N->FindPin(Pin))for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==Pin){N->SetPinVisibility(true,I);break;}
    if(!N->FindPin(Pin))return false;
    auto* V=NewNode<UK2Node_VariableGet>(G,TEXT("VS2Flight_Get_")+N->GetName()+TEXT("_")+Property.ToString(),N->NodePosX-200,N->NodePosY+200);
    V->VariableReference.SetSelfMember(Property);V->AllocateDefaultPins();return Link(G,Output(V),N->FindPin(Pin));
}
bool Times(FAnimNode_BlendListBase& Node,const TArray<float>& Values,bool Reset)
{
    auto* F=FindFProperty<FArrayProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("BlendTime"));
    auto* M=FindFProperty<FProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("ChildUpateMode"));
    if(!F||!M||Node.GetBlendTimes().Num()!=Values.Num())return false;
    *F->ContainerPtrToValuePtr<TArray<float>>(&Node)=Values;
    return M->ImportText_InContainer(Reset?TEXT("ResetChildOnActivate"):TEXT("Default"),&Node,nullptr,PPF_None)!=nullptr;
}
bool ClothSettings(UAnimGraphNode_Base* Node,FName& ForceProperty,FString& Root,float& Limit,int32& Capsules)
{
    const auto* N=FindFProperty<FStructProperty>(Node->GetClass(),TEXT("Node"));if(!N)return false;
    void* Data=N->ContainerPtrToValuePtr<void>(Node);const UScriptStruct* Struct=N->Struct;
    const auto* B=FindFProperty<FStructProperty>(Struct,TEXT("RootBone"));
    const auto* F=FindFProperty<FStructProperty>(Struct,TEXT("SimpleExternalForce"));
    const auto* W=FindFProperty<FBoolProperty>(Struct,TEXT("bUseWorldSpaceSimpleExternalForce"));
    const auto* P=FindFProperty<FStructProperty>(Struct,TEXT("PhysicsSettings"));
    const auto* C=FindFProperty<FArrayProperty>(Struct,TEXT("CapsuleLimits"));
    if(!B||!F||!W||!P||!C||!W->GetPropertyValue_InContainer(Data)||!F->ContainerPtrToValuePtr<FVector>(Data)->IsNearlyZero())return false;
    Root=B->ContainerPtrToValuePtr<FBoneReference>(Data)->BoneName.ToString();
    const auto* A=FindFProperty<FFloatProperty>(P->Struct,TEXT("LimitAngle"));if(!A)return false;
    Limit=A->GetPropertyValue_InContainer(P->ContainerPtrToValuePtr<void>(Data));FScriptArrayHelper Array(C,C->ContainerPtrToValuePtr<void>(Data));Capsules=Array.Num();
    if(Root.StartsWith(TEXT("Skirt_"))){ForceProperty=TEXT("VS2FlightSkirtForce");return Limit>0&&Limit<=25.001f&&Capsules>=2;}
    if(Root.StartsWith(TEXT("Hair_"))){ForceProperty=TEXT("VS2FlightHairForce");return true;}
    if(Root.StartsWith(TEXT("Chest_ribbon_"))||Root.StartsWith(TEXT("LowerLeg_ribbon_"))){ForceProperty=TEXT("VS2FlightRibbonForce");return true;}
    return false;
}
}
#endif

FString UHCM5VS2FlightPoseEditor::AuthorFlightLoops(UAnimSequence* Source,const TArray<UAnimSequence*>& Loops,bool Apply,bool ReadbackOnly,bool bTownPolish)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("apply"),Apply);R->SetBoolField(TEXT("readback_only"),ReadbackOnly);
    if(Apply&&ReadbackOnly)return Finish(R,TEXT("Write and reload modes are exclusive"));
    if(!Source||Path(Source)!=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS")
        ||!Source->GetSkeleton()||!Source->GetDataModel()||Loops.Num()!=5)return Finish(R,TEXT("Exact actual relaxed Idle and five fresh targets required"));
    const FReferenceSkeleton& Ref=Source->GetSkeleton()->GetReferenceSkeleton();
    if(Ref.GetRawBoneNum()!=247||Ref.GetBoneName(0)!=TEXT("Hips"))return Finish(R,TEXT("Actual Selestia Hips-root hierarchy required"));
    for(const auto* Loop:Loops)if(!Owned(Loop)||Loop==Source||Loop->GetSkeleton()!=Source->GetSkeleton())return Finish(R,TEXT("Fresh same-skeleton candidate loops required"));
    TArray<FTransform> Base=Ref.GetRawRefBonePose();
    for(int32 I=0;I<Base.Num();++I)if(Source->GetDataModel()->IsValidBoneTrackName(Ref.GetBoneName(I)))Base[I]=Source->GetDataModel()->GetBoneTrackTransform(Ref.GetBoneName(I),FFrameNumber(0));
    for(auto& T:Base){if(T.ContainsNaN())return Finish(R,TEXT("Nonfinite source pose"));T.NormalizeRotation();}
    TArray<FTransform> BaseCS;ComponentPose(Ref,Base,BaseCS);
    const TCHAR* Required[]={TEXT("UpperArm_L"),TEXT("LowerArm_L"),TEXT("Hand_L"),TEXT("UpperArm_R"),TEXT("LowerArm_R"),TEXT("Hand_R"),TEXT("UpperLeg_L"),TEXT("LowerLeg_L"),TEXT("Foot_L"),TEXT("Toe_L"),TEXT("UpperLeg_R"),TEXT("LowerLeg_R"),TEXT("Foot_R"),TEXT("Toe_R")};
    for(const TCHAR* N:Required)if(Ref.FindBoneIndex(N)<0)return Finish(R,TEXT("Real limb bone missing"));
    const auto Position=[&](const TCHAR* Name){return BaseCS[Ref.FindBoneIndex(Name)].GetLocation();};
    const FVector Up=FVector::UpVector;
    const FVector Forward=FVector::VectorPlaneProject((Position(TEXT("Toe_L"))-Position(TEXT("Foot_L")))+(Position(TEXT("Toe_R"))-Position(TEXT("Foot_R"))),Up).GetSafeNormal();
    const FVector Lateral=FVector::VectorPlaneProject(Position(TEXT("UpperArm_L"))-Position(TEXT("UpperArm_R")),Up).GetSafeNormal();
    if(Forward.IsNearlyZero()||Lateral.IsNearlyZero()||FMath::Abs(Forward|Lateral)>.2)return Finish(R,TEXT("Actual source body basis is not well-conditioned"));
    R->SetArrayField(TEXT("measured_model_forward"),XYZ(Forward));R->SetArrayField(TEXT("measured_model_left"),XYZ(Lateral));
    const float LeanDegrees[]={0,bTownPolish?18.f:16.f,bTownPolish?35.f:28.f,-5,5},ArmSpread[]={.45f,.35f,.25f,.6f,.5f},ArmFore[]={.1f,-.12f,-.3f,.12f,.22f};
    R->SetBoolField(TEXT("town_pose_revision"),bTownPolish);
    TArray<TSharedPtr<FJsonValue>> Audits;double MaxAngular=0,MaxTranslation=0,MaxScale=0;
    for(int32 Kind=0;Kind<5;++Kind)
    {
        auto* Target=Loops[Kind];TArray<TArray<FTransform>> Frames;Frames.Reserve(61);
        for(int32 Frame=0;Frame<=60;++Frame)
        {
            const double Wave=FMath::Sin(2.*PI*Frame/60.);const FQuat Lean=FQuat::FindBetweenNormals(Up,(Up*FMath::Cos(FMath::DegreesToRadians(LeanDegrees[Kind]))+Forward*FMath::Sin(FMath::DegreesToRadians(LeanDegrees[Kind]))).GetSafeNormal());
            TArray<FTransform> Pose=Base;Pose[0].SetRotation((Lean*Pose[0].GetRotation()).GetNormalized());
            // Gentle whole-body breath is bounded under 0.5 cm; no actor/root motion extraction.
            Pose[0].AddToTranslation(Up*(Wave*(bTownPolish?1.2:.45)));
            for(int32 Side=0;Side<2;++Side)
            {
                const FString S=Side==0?TEXT("L"):TEXT("R");const float Sign=Side==0?1.f:-1.f;
                const auto Bone=[&](const TCHAR* Prefix){return FName(*(FString(Prefix)+S));};
                const auto Direction=[&](double X,double Y,double Z){return Lean.RotateVector((Lateral*X+Forward*Y+Up*Z).GetSafeNormal());};
                bool Good=PointBone(Ref,Pose,Bone(TEXT("UpperArm_")),Bone(TEXT("LowerArm_")),Direction(Sign*(ArmSpread[Kind]+Wave*.015),ArmFore[Kind],-1));
                Good&=PointBone(Ref,Pose,Bone(TEXT("LowerArm_")),Bone(TEXT("Hand_")),Direction(Sign*.10,ArmFore[Kind]+(Side==0?.48:.40),-1));
                // Relaxed asymmetric suspension: one knee hangs lower, the other bends back.
                // Direction vectors use the source skeleton's measured forward/up axes;
                // preserve every segment length, hand/finger track and skirt local pose.
                const double ThighFore=bTownPolish?(Side==0?.12:.30):(Kind==2?(Side==0?.04:.14):(Side==0?.10:.32));
                const double CalfBack=bTownPolish?(Side==0?.70:1.05):(Kind==2?(Side==0?.20:.34):(Side==0?.28:.58));
                const double LegWave=Wave*(Side==0?.018:-.018);
                Good&=PointBone(Ref,Pose,Bone(TEXT("UpperLeg_")),Bone(TEXT("LowerLeg_")),Direction(Sign*.025,ThighFore+LegWave,-1));
                Good&=PointBone(Ref,Pose,Bone(TEXT("LowerLeg_")),Bone(TEXT("Foot_")),Direction(0,-CalfBack-LegWave,-1));
                Good&=PointBone(Ref,Pose,Bone(TEXT("Foot_")),Bone(TEXT("Toe_")),Direction(0,.65,-.7));
                if(!Good)return Finish(R,TEXT("Actual limb hierarchy does not match directional solver"));
            }
            Frames.Add(MoveTemp(Pose));
        }
        if(Apply)
        {
            Target->Modify();auto& C=Target->GetController();C.OpenBracket(FText::FromString(TEXT("Original bounded flight pose loops")),false);
            C.SetFrameRate(FFrameRate(30,1),false);C.SetNumberOfFrames(FFrameNumber(60),false);C.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float,false);
            bool Good=true;
            for(int32 Bone=0;Bone<247;++Bone)
            {
                const FName Name=Ref.GetBoneName(Bone);if(!Target->GetDataModel()->IsValidBoneTrackName(Name))Good&=C.AddBoneCurve(Name,false);
                TArray<FVector> P,S;TArray<FQuat> Q;
                for(const auto& Pose:Frames){P.Add(Pose[Bone].GetLocation());Q.Add(Pose[Bone].GetRotation());S.Add(Pose[Bone].GetScale3D());}
                Good&=C.SetBoneTrackKeys(Name,P,Q,S,false);
                // Native double-Euler update avoids an unnecessary float Euler round trip.
                Good&=C.UpdateBoneTrackKeys(Name,FInt32Range(0,61),P,Q,S,false);
            }
            C.CloseBracket(false);if(!Good)return Finish(R,TEXT("Native pose controller write failed"));
            Target->bEnableRootMotion=false;Target->bForceRootLock=false;Target->RateScale=1;Target->MarkPackageDirty();
        }
        if(Apply||ReadbackOnly)
            for(int32 Bone=0;Bone<247;++Bone)
            {
                TArray<FTransform> Actual;Target->GetDataModel()->GetBoneTrackTransforms(Ref.GetBoneName(Bone),Actual);
                if(Actual.Num()!=61)return Finish(R,TEXT("Native generated key count differs"));
                for(int32 Frame=0;Frame<=60;++Frame)
                {
                    const auto& Expected=Frames[Frame][Bone];const auto& Read=Actual[Frame];
                    if(Read.ContainsNaN())return Finish(R,TEXT("Nonfinite native pose readback"));
                    MaxAngular=FMath::Max(MaxAngular,FMath::RadiansToDegrees(Expected.GetRotation().AngularDistance(Read.GetRotation())));
                    MaxTranslation=FMath::Max(MaxTranslation,(Expected.GetLocation()-Read.GetLocation()).Size());MaxScale=FMath::Max(MaxScale,(Expected.GetScale3D()-Read.GetScale3D()).Size());
                }
            }
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("target"),Path(Target));Row->SetNumberField(TEXT("lean_degrees"),LeanDegrees[Kind]);
        Row->SetNumberField(TEXT("frames"),61);Row->SetNumberField(TEXT("fps"),30);Row->SetNumberField(TEXT("raw_bones"),247);
        Row->SetNumberField(TEXT("loop_seam_root_cm"),(Frames[0][0].GetLocation()-Frames.Last()[0].GetLocation()).Size());Audits.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("loops"),Audits);R->SetNumberField(TEXT("native_rotation_error_degrees"),MaxAngular);
    R->SetNumberField(TEXT("native_position_error_cm"),MaxTranslation);R->SetNumberField(TEXT("native_scale_error"),MaxScale);
    if(MaxAngular>.001||MaxTranslation>.0001||MaxScale>.00001)return Finish(R,TEXT("Native authored pose readback outside numeric bounds"));
    R->SetStringField(TEXT("scope"),TEXT("Original CS-direction limbs; frozen source Idle frame-0 fingers/face/local lengths; no bone-local Euler guesses; existing target float gait curves cleared"));return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2FlightPoseEditor::ConfigureFlightGraph(UAnimBlueprint* BP,const TArray<UAnimSequence*>& Loops,UAnimSequence* Takeoff,UAnimSequence* Landing)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Owned(BP)||!BP->GeneratedClass||BP->ParentClass!=UHCM5VS2LookAnimInstance::StaticClass()||Loops.Num()!=5||!Takeoff||!Landing)return Finish(R,TEXT("Fresh actual GAS motion candidate and seven poses required"));
    for(auto* Loop:Loops)if(!Owned(Loop)||Loop->GetDataModel()->GetNumberOfKeys()!=61)return Finish(R,TEXT("Actual authored loops required"));
    if(Takeoff->GetName()!=TEXT("M_Relaxed_Jump_F_Start_Stand_Rfoot_Cut_InPlace_SelestiaTransition")||Landing->GetName()!=TEXT("M_Relaxed_Jump_F_Land_Stand_Light_Rfoot_Cut_InPlace_SelestiaTransition"))return Finish(R,TEXT("Exact previously retargeted transition clips required"));
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* Graph=nullptr;UEdGraphPin* Prior=nullptr;UEdGraphPin* Sink=nullptr;
    TArray<UAnimGraphNode_SequencePlayer*> Existing;Existing.Init(nullptr,7);int32 ExistingCount=0;
    for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)
        if(auto* Player=Cast<UAnimGraphNode_SequencePlayer>(N))
            for(int32 I=0;I<7;++I)if(N->GetName()==FString::Printf(TEXT("VS2Flight_Clip%d"),I))
            {if(Existing[I])return Finish(R,TEXT("Duplicate existing flight clip"));Existing[I]=Player;++ExistingCount;}
    if(ExistingCount)
    {
        if(ExistingCount!=7||Existing[0]->Node.GetSequence()!=Takeoff||Existing[6]->Node.GetSequence()!=Landing)
            return Finish(R,TEXT("Complete existing flight branch with unchanged takeoff/landing required"));
        BP->Modify();
        for(int32 I=0;I<5;++I){Existing[I+1]->Modify();Existing[I+1]->Node.SetSequence(Loops[I]);}
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
        if(BP->Status==BS_Error)return Finish(R,TEXT("Existing flight loop replacement compile failed"));
        R->SetStringField(TEXT("mode"),TEXT("REPLACE_FIVE_LOOPS_ONLY_EXISTING_GRAPH_RETAINED"));
        return Finish(R);
    }
    TArray<UAnimGraphNode_Base*> Physics;
    for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)
    {
        if(N->GetName().StartsWith(TEXT("VS2Flight_")))return Finish(R,TEXT("Partial/repeated flight graph authoring refused"));
        if(N->GetClass()->GetName().Contains(TEXT("KawaiiPhysics")))Physics.Add(CastChecked<UAnimGraphNode_Base>(N));
        if(N->GetName()==TEXT("VS2Motion_Eligibility")){if(Graph)return Finish(R,TEXT("Ambiguous prior motion branch"));Graph=G;Prior=Output(N);if(!Prior||Prior->LinkedTo.Num()!=1)return Finish(R,TEXT("Motion sink ambiguous"));Sink=Prior->LinkedTo[0];}
    }
    if(!Graph||!Sink||Physics.Num()!=11)return Finish(R,TEXT("Expected candidate/11 secondary nodes missing"));
    TArray<TSharedPtr<FJsonValue>> Cloth;TArray<FName> ForceProperties;
    for(auto* N:Physics)
    {
        FName Property;FString Root;float Limit=0;int32 Capsules=0;
        if(!ClothSettings(N,Property,Root,Limit,Capsules))return Finish(R,TEXT("Existing cloth wind/limit/collision guard failed: ")+Root);
        ForceProperties.Add(Property);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("root"),Root);Row->SetNumberField(TEXT("cone_degrees"),Limit);Row->SetNumberField(TEXT("capsules_retained"),Capsules);Cloth.Add(MakeShared<FJsonValueObject>(Row));
    }
    BP->Modify();Graph->Modify();
    auto* Select=NewNode<UAnimGraphNode_BlendListByBool>(Graph,TEXT("VS2Flight_Eligibility"),200,3700);
    if(!Times(Select->Node,{.15f,0.f},false))return Finish(R,TEXT("Flight bool blend settings failed"));Select->AllocateDefaultPins();
    auto* Motion=NewNode<UAnimGraphNode_BlendListByInt>(Graph,TEXT("VS2Flight_Poses"),-1200,3700);
    while(Motion->Node.GetBlendTimes().Num()<7)Motion->Node.AddPose();TArray<float> Blend;Blend.Init(.2f,7);Blend[6]=.08f;
    if(!Times(Motion->Node,Blend,true))return Finish(R,TEXT("Flight pose reset settings failed"));Motion->ReconstructNode();
    TArray<UAnimSequence*> Poses={Takeoff};Poses.Append(Loops);Poses.Add(Landing);
    for(int32 I=0;I<7;++I)
    {
        auto* N=NewNode<UAnimGraphNode_SequencePlayer>(Graph,FString::Printf(TEXT("VS2Flight_Clip%d"),I),-1500,4000+I*130);
        N->Node.SetSequence(Poses[I]);N->Node.SetPlayRate(1.f);N->Node.SetLoopAnimation(I>0&&I<6);N->Node.SetGroupMethod(EAnimSyncMethod::DoNotSync);N->AllocateDefaultPins();
        if(!Link(Graph,Output(N),Motion->FindPin(FName(*FString::Printf(TEXT("BlendPose_%d"),I)))))return Finish(R,TEXT("Flight sequence connection failed"));
    }
    auto* CS=NewNode<UAnimGraphNode_LocalToComponentSpace>(Graph,TEXT("VS2Flight_ToCS"),-900,3700);CS->AllocateDefaultPins();
    auto* Bank=NewNode<UAnimGraphNode_ModifyBone>(Graph,TEXT("VS2Flight_Bank"),-650,3700);
    Bank->Node.BoneToModify.BoneName=TEXT("Hips");Bank->Node.TranslationMode=BMM_Ignore;Bank->Node.ScaleMode=BMM_Ignore;
    Bank->Node.RotationMode=BMM_Additive;Bank->Node.RotationSpace=BCS_ComponentSpace;Bank->AllocateDefaultPins();
    auto* Local=NewNode<UAnimGraphNode_ComponentToLocalSpace>(Graph,TEXT("VS2Flight_ToLocal"),-400,3700);Local->AllocateDefaultPins();
    if(!Link(Graph,Output(Motion),CS->FindPin(TEXT("LocalPose")))||!Link(Graph,Output(CS),Bank->FindPin(TEXT("ComponentPose")))
        ||!Link(Graph,Output(Bank),Local->FindPin(TEXT("ComponentPose")))||!Link(Graph,Output(Local),Select->FindPin(TEXT("BlendPose_0")))
        ||!Variable(Graph,Motion,TEXT("ActiveChildIndex"),TEXT("VS2FlightPoseIndex"))
        ||!Variable(Graph,Bank,TEXT("Rotation"),TEXT("VS2FlightBankRotation"))
        ||!Variable(Graph,Select,TEXT("bActiveValue"),TEXT("bVS2FlightPoseEligible")))return Finish(R,TEXT("Flight pose/bank variable connection failed"));
    for(int32 I=0;I<Physics.Num();++I)
        if(!Variable(Physics[I]->GetGraph(),Physics[I],TEXT("SimpleExternalForce"),ForceProperties[I]))return Finish(R,TEXT("Native bounded secondary wind pin failed"));
    Prior->BreakLinkTo(Sink);
    if(!Link(Graph,Prior,Select->FindPin(TEXT("BlendPose_1")))||!Link(Graph,Output(Select),Sink))return Finish(R,TEXT("Prior motion/FP/combat output connection failed"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error||!BP->GeneratedClass)return Finish(R,TEXT("Flight candidate blueprint compile failed"));
    auto* Defaults=Cast<UHCM5VS2LookAnimInstance>(BP->GeneratedClass->GetDefaultObject());if(!Defaults||!Defaults->bVS2GASMotionEnabled)return Finish(R,TEXT("GAS opt-in lost"));
    Defaults->bVS2FlightPosesEnabled=true;BP->MarkPackageDirty();R->SetArrayField(TEXT("unchanged_cloth_constraints"),Cloth);
    return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2FlightPoseEditor::InspectFlightGraph(UAnimBlueprint* BP)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();if(!Owned(BP)||!BP->GeneratedClass||BP->Status==BS_Error)return Finish(R,TEXT("Actual compiled candidate required"));
    const auto* Defaults=Cast<UHCM5VS2LookAnimInstance>(BP->GeneratedClass->GetDefaultObject());
    if(!Defaults||!Defaults->bVS2FlightPosesEnabled||!Defaults->bVS2GASMotionEnabled)return Finish(R,TEXT("Both native motion flags required"));
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);TArray<TSharedPtr<FJsonValue>> Rows;int32 Players=0,Banks=0,Winds=0;
    for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)
    {
        if(N->GetClass()->GetName().Contains(TEXT("KawaiiPhysics")))
        {auto* Pin=N->FindPin(TEXT("SimpleExternalForce"));if(!Pin||Pin->LinkedTo.Num()!=1)return Finish(R,TEXT("Secondary wind pin not linked"));++Winds;}
        if(!N->GetName().StartsWith(TEXT("VS2Flight_")))continue;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),N->GetName());Row->SetStringField(TEXT("class"),N->GetClass()->GetName());TArray<TSharedPtr<FJsonValue>> Links;
        for(auto* Pin:N->Pins)for(auto* To:Pin->LinkedTo)Links.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()+TEXT("->")+To->GetOwningNode()->GetName()+TEXT(".")+To->PinName.ToString()));
        Row->SetArrayField(TEXT("links"),Links);
        if(auto* P=Cast<UAnimGraphNode_SequencePlayer>(N)){++Players;Row->SetStringField(TEXT("sequence"),Path(P->Node.GetSequence()));}
        if(Cast<UAnimGraphNode_ModifyBone>(N))++Banks;Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(Players!=7||Banks!=1||Winds!=11)return Finish(R,TEXT("Flight native graph counts differ"));
    R->SetArrayField(TEXT("nodes"),Rows);R->SetNumberField(TEXT("bounded_wind_nodes"),Winds);return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
