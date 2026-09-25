#include "HCM5VS2GASMotionEditor.h"
#include "HCM5VS2LookAnimInstance.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Engine/SkeletalMesh.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LegIK.h"
#include "AnimGraphNode_CopyBone.h"
#include "AnimGraph/AnimGraphNode_StrideWarping.h"
#include "AnimGraph/AnimGraphNode_OrientationWarping.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
const FString CandidateRoot=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_");
FString Package(const UObject* O) { return O ? O->GetOutermost()->GetName() : FString(); }
bool Owned(const UObject* O) { return O && Package(O).StartsWith(CandidateRoot); }
FString Finish(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{
    R->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    if (!Error.IsEmpty()) R->SetStringField(TEXT("error"),Error);
    R->SetBoolField(TEXT("saved_by_helper"),false); R->SetStringField(TEXT("runtime_sliding"),TEXT("NOT_RUN"));
    FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;
}
bool SameRawRef(const FReferenceSkeleton& A,const FReferenceSkeleton& B)
{
    if (A.GetRawBoneNum()!=B.GetRawBoneNum()) return false;
    for (int32 I=0;I<A.GetRawBoneNum();++I)
        if (A.GetRawRefBoneInfo()[I].Name!=B.GetRawRefBoneInfo()[I].Name
            || A.GetRawRefBoneInfo()[I].ParentIndex!=B.GetRawRefBoneInfo()[I].ParentIndex
            || !A.GetRawRefBonePose()[I].Equals(B.GetRawRefBonePose()[I],0.)) return false;
    return true;
}
template<class T> T* Node(UEdGraph* G,const TCHAR* Name,int32 X,int32 Y)
{
    T* N=NewObject<T>(G,FName(Name),RF_Transactional);
    G->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->NodePosX=X;N->NodePosY=Y;return N;
}
UEdGraphPin* Output(UEdGraphNode* N)
{
    UEdGraphPin* Result=nullptr;
    for (UEdGraphPin* P:N->Pins) if(P->Direction==EGPD_Output && P->PinName!=TEXT("self"))
    { if(Result)return nullptr;Result=P; }
    return Result;
}
bool Expose(UAnimGraphNode_Base* N,FName Property)
{
    if(N->FindPin(Property))return true;
    for(int32 I=0;I<N->ShowPinForProperties.Num();++I)
        if(N->ShowPinForProperties[I].PropertyName==Property)
        { N->SetPinVisibility(true,I);return N->FindPin(Property)!=nullptr; }
    return false;
}
bool Link(UEdGraph* G,UEdGraphPin* A,UEdGraphPin* B)
{ return A && B && G->GetSchema()->TryCreateConnection(A,B); }
bool Variable(UEdGraph* G,UAnimGraphNode_Base* Destination,FName Pin,FName Property,int32 Y)
{
    if(!Expose(Destination,Pin))return false;
    const FString Name=TEXT("VS2Motion_Get_")+Property.ToString()+TEXT("_")+Destination->GetName();
    auto* N=Node<UK2Node_VariableGet>(G,*Name,Destination->NodePosX-250,Y);
    N->VariableReference.SetSelfMember(Property);N->AllocateDefaultPins();
    return Link(G,Output(N),Destination->FindPin(Pin));
}
bool BlendSettings(FAnimNode_BlendListBase& N,const TArray<float>& Times,bool Reset)
{
    auto* A=FindFProperty<FArrayProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("BlendTime"));
    FProperty* Mode=FindFProperty<FProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("ChildUpateMode"));
    if(!A || !Mode || N.GetBlendTimes().Num()!=Times.Num())return false;
    *A->ContainerPtrToValuePtr<TArray<float>>(&N)=Times;
    return Mode->ImportText_InContainer(Reset?TEXT("ResetChildOnActivate"):TEXT("Default"),&N,nullptr,PPF_None)!=nullptr;
}
}
#endif

FString UHCM5VS2GASMotionEditor::PrepareWarpSkeleton(USkeleton* Source,USkeleton* Candidate,USkeletalMesh* Mesh)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Source || !Owned(Candidate) || !Owned(Mesh) || Candidate==Source
        || Package(Source)!=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SK_Selestia")
        || !SameRawRef(Source->GetReferenceSkeleton(),Candidate->GetReferenceSkeleton())
        || !SameRawRef(Source->GetReferenceSkeleton(),Mesh->GetRefSkeleton()) || Candidate->GetVirtualBones().Num()!=Source->GetVirtualBones().Num())
        return Finish(R,TEXT("Fresh exact raw hierarchy/transform duplicates of Selestia required"));
    // The real normalized Selestia hierarchy has Hips at index 0, no floor Root.
    const FName Root(TEXT("Hips"));
    if(Source->GetReferenceSkeleton().GetBoneName(0)!=Root || Source->GetReferenceSkeleton().GetParentIndex(0)!=INDEX_NONE)
        return Finish(R,TEXT("Selestia actual Hips root identity differs"));
    for(const FName Name:{Root,FName(TEXT("Hips")),FName(TEXT("Foot_L")),FName(TEXT("Foot_R")),FName(TEXT("UpperLeg_L")),FName(TEXT("UpperLeg_R"))})
        if(Candidate->GetReferenceSkeleton().FindBoneIndex(Name)==INDEX_NONE)return Finish(R,TEXT("Required real bone missing"));
    Candidate->Modify();Mesh->Modify();
    if(!Candidate->AddNewNamedVirtualBone(Root,Root,TEXT("VB VS2_IKRoot"))
        || !Candidate->AddNewNamedVirtualBone(TEXT("VB VS2_IKRoot"),TEXT("Foot_L"),TEXT("VB VS2_IKFoot_L"))
        || !Candidate->AddNewNamedVirtualBone(TEXT("VB VS2_IKRoot"),TEXT("Foot_R"),TEXT("VB VS2_IKFoot_R")))
        return Finish(R,TEXT("Unique native virtual bones could not be added"));
    // Compatibility is permitted only after exact raw ref/hierarchy equality;
    // this is not an unrelated skeleton declaration and never edits the source.
    Candidate->AddCompatibleSkeleton(Source);
    Mesh->SetSkeleton(Candidate);Mesh->GetRefSkeleton().RebuildRefSkeleton(Candidate,true);
    if(!SameRawRef(Source->GetReferenceSkeleton(),Candidate->GetReferenceSkeleton())
        || !SameRawRef(Source->GetReferenceSkeleton(),Mesh->GetRefSkeleton()))
        return Finish(R,TEXT("Virtual bone creation changed raw bones"));
    for(const FName Name:{FName(TEXT("VB VS2_IKRoot")),FName(TEXT("VB VS2_IKFoot_L")),FName(TEXT("VB VS2_IKFoot_R"))})
        if(Mesh->GetRefSkeleton().FindBoneIndex(Name)==INDEX_NONE)return Finish(R,TEXT("Virtual bone absent in target mesh ref pose"));
    Candidate->MarkPackageDirty();Mesh->MarkPackageDirty();
    R->SetNumberField(TEXT("raw_bones_exact"),Source->GetReferenceSkeleton().GetRawBoneNum());
    R->SetNumberField(TEXT("virtual_bones_added"),3);R->SetStringField(TEXT("skeleton"),Package(Candidate));
    return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2GASMotionEditor::AddRootSpeedCurve(UAnimSequence* Source,UAnimSequence* Target,float Speed,float Scale)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Source || !Owned(Target) || Source==Target || !FMath::IsFinite(Speed) || !FMath::IsFinite(Scale)
        || Speed<=0 || Scale<=0 || !Source->GetDataModel() || !Target->GetDataModel())
        return Finish(R,TEXT("Valid clean source and separate candidate loop required"));
    const FString SourceName=Source->GetName();
    if(SourceName!=TEXT("M_Relaxed_Walk_Loop_F") && SourceName!=TEXT("M_Relaxed_Run_Loop_F") && SourceName!=TEXT("M_Relaxed_Sprint_Loop_F"))
        return Finish(R,TEXT("Only actual GAS forward loop source names accepted"));
    if((SourceName!=TEXT("M_Relaxed_Sprint_Loop_F") && Package(Source)!=TEXT("/Game/HarborCity/M5VS2/GASSourceP0/Animations/")+SourceName)
        || (SourceName==TEXT("M_Relaxed_Sprint_Loop_F") && !Package(Source).StartsWith(TEXT("/Game/HarborCity/M5VS2/GASRecoverySource/Batch_"))))
        return Finish(R,TEXT("Unapproved clean-source namespace"));
    const IAnimationDataModel* SM=Source->GetDataModel();const IAnimationDataModel* TM=Target->GetDataModel();
    if(SM->GetNumberOfKeys()!=TM->GetNumberOfKeys() || SM->GetFrameRate()!=TM->GetFrameRate())return Finish(R,TEXT("Source/target loop timing differs"));
    const FAnimationCurveIdentifier Id(TEXT("VS2NominalRootSpeed"),ERawCurveTrackTypes::RCT_Float);
    if(TM->FindFloatCurve(Id))return Finish(R,TEXT("Curve already exists; use a fresh loop copy"));
    TArray<FTransform> Roots;SM->GetBoneTrackTransforms(TEXT("root"),Roots);
    if(Roots.Num()!=SM->GetNumberOfKeys() || Roots.Num()<3)return Finish(R,TEXT("Actual source root track unavailable"));
    const double Hz=SM->GetFrameRate().AsDecimal();TArray<double> Speeds;double Mean=0;
    for(int32 I=0;I<Roots.Num()-1;++I)
    { const double V=(Roots[I+1].GetLocation()-Roots[I].GetLocation()).Size2D()*Hz;if(!FMath::IsFinite(V))return Finish(R,TEXT("Nonfinite root delta"));Speeds.Add(V);Mean+=V; }
    Mean/=Speeds.Num();if(Mean<10)return Finish(R,TEXT("Source root speed too small for measured stride calibration"));
    TArray<FRichCurveKey> Keys;
    for(int32 I=0;I<Roots.Num();++I)
    {
        const double Before=Speeds[(I-1+Speeds.Num())%Speeds.Num()],After=Speeds[I%Speeds.Num()];
        FRichCurveKey Key(float(I/Hz),float((Before+After)*.5/Mean*Speed/Scale));Key.InterpMode=RCIM_Linear;Keys.Add(Key);
    }
    IAnimationDataController& C=Target->GetController();C.OpenBracket(FText::FromString(TEXT("VS2 native nominal stride speed")),false);
    const bool Good=C.AddCurve(Id,AACF_DefaultCurve,false) && C.SetCurveKeys(Id,Keys,false);C.CloseBracket(false);
    const FFloatCurve* Read=Target->GetDataModel()->FindFloatCurve(Id);
    if(!Good || !Read || Read->FloatCurve.GetNumKeys()!=Keys.Num())return Finish(R,TEXT("Native curve controller/readback failed"));
    double MaxError=0;
    for(const FRichCurveKey& Key:Keys)
    { const double Error=FMath::Abs(Read->FloatCurve.Eval(Key.Time)-Key.Value);MaxError=FMath::Max(MaxError,Error);if(Error>1.e-3)return Finish(R,TEXT("Root speed curve changed during native write")); }
    Target->MarkPackageDirty();R->SetNumberField(TEXT("source_mean_root_cm_s"),Mean);
    R->SetNumberField(TEXT("calibrated_world_cm_s"),Speed);R->SetNumberField(TEXT("calibration_mesh_scale"),Scale);
    R->SetNumberField(TEXT("native_max_curve_error"),MaxError);R->SetNumberField(TEXT("keys"),Keys.Num());
    return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2GASMotionEditor::ConfigureMotionGraph(UAnimBlueprint* BP,USkeletalMesh* Mesh,UBlendSpace* Original,UBlendSpace* Space,const TArray<UAnimSequence*>& Loops,const TArray<UAnimSequence*>& Clips)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Owned(BP)||!Owned(Mesh)||!Owned(Space)||!Original||Clips.Num()!=10||Loops.Num()!=3||!Mesh->GetSkeleton()
        || !Owned(Mesh->GetSkeleton()) || BP->ParentClass!=UHCM5VS2LookAnimInstance::StaticClass()
        || Package(Original)!=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun")
        || Original->GetBlendSamples().Num()!=28 || Space->GetBlendSamples().Num()!=28)
        return Finish(R,TEXT("Exact fresh VS2 graph/mesh/28-sample candidates and 10 clips required"));
    for(int32 I=0;I<28;++I)
    {
        const auto& A=Original->GetBlendSamples()[I];const auto& B=Space->GetBlendSamples()[I];
        if(A.SampleValue!=B.SampleValue || A.RateScale!=B.RateScale || A.Animation!=B.Animation)
            return Finish(R,TEXT("Original 28 positions/rates and 16 side/back samples must remain intact"));
    }
    const int32 LoopIndices[]={0,9,27};
    for(int32 I=0;I<3;++I)
    {
        const UAnimSequence* PriorLoop=Original->GetBlendSamples()[LoopIndices[I]].Animation;
        if(!Owned(Loops[I]) || !PriorLoop || !Loops[I]->GetDataModel() || Loops[I]->GetSkeleton()!=Original->GetSkeleton()
            || Loops[I]->RateScale!=PriorLoop->RateScale || Loops[I]->GetNumberOfSampledKeys()!=PriorLoop->GetNumberOfSampledKeys()
            || Loops[I]->GetSamplingFrameRate()!=PriorLoop->GetSamplingFrameRate()
            || !Loops[I]->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(TEXT("VS2NominalRootSpeed"),ERawCurveTrackTypes::RCT_Float)))
            return Finish(R,TEXT("Three exact forward loop copies with measured speed curves required (Run,Sprint,Walk)"));
    }
    const TCHAR* Expected[]={TEXT("Run_Start_F_Lfoot"),TEXT("Sprint_Start_F_Lfoot"),TEXT("Run_Stop_F_Lfoot"),TEXT("Sprint_Stop_F_Lfoot"),
        TEXT("Jump_F_Start_Stand_Rfoot"),TEXT("Jump_F_Start_Run_Rfoot"),TEXT("Jump_F_Start_Sprint_Rfoot"),TEXT("Jump_Loop_Fall"),TEXT("Jump_F_Land_Stand_Light_Rfoot"),TEXT("Jump_F_Land_Run_Light_Rfoot")};
    for(int32 I=0;I<10;++I)
        if(!Clips[I] || Clips[I]->GetName()!=FString(TEXT("M_Relaxed_"))+Expected[I]+TEXT("_Cut_InPlace_SelestiaTransition")
            || !Package(Clips[I]).StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASTransitions/Batch_"))
            || Clips[I]->GetSkeleton()!=Original->GetSkeleton()
            || Clips[I]->RateScale!=1.f || Clips[I]->bEnableRootMotion || Clips[I]->bForceRootLock)
            return Finish(R,TEXT("Unexpected clip order/identity/root-motion flags"));
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* Graph=nullptr;UEdGraphPin* Prior=nullptr;UEdGraphPin* Sink=nullptr;
    int32 KawaiiBefore=0,SlotsBefore=0,LookBefore=0;
    for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)
    {
        if(N->GetName().StartsWith(TEXT("VS2Motion_")))return Finish(R,TEXT("Partial/repeated authoring refused"));
        if(N->GetClass()->GetName().Contains(TEXT("KawaiiPhysics")))++KawaiiBefore;
        if(N->GetClass()->GetName()==TEXT("AnimGraphNode_Slot"))++SlotsBefore;
        if(N->GetClass()->GetName()==TEXT("AnimGraphNode_LookAt"))++LookBefore;
        if(auto* Machine=Cast<UAnimGraphNode_StateMachineBase>(N);Machine&&Machine->EditorStateMachineGraph
            && Machine->EditorStateMachineGraph->GetName()==TEXT("Main States"))
        {
            if(Graph)return Finish(R,TEXT("Ambiguous legacy Main States"));
            Graph=G;Prior=Output(Machine);
            if(!Prior || Prior->LinkedTo.Num()!=1)return Finish(R,TEXT("Expected one intact Main States output"));
            Sink=Prior->LinkedTo[0];
        }
    }
    if(!Graph||!Sink||KawaiiBefore!=11||LookBefore!=3||SlotsBefore<3)return Finish(R,TEXT("Expected legacy movement/slot/look/physics graph absent"));
    BP->Modify();BP->TargetSkeleton=Mesh->GetSkeleton();BP->SetPreviewMesh(Mesh);
    Space->Modify();Space->SetSkeleton(Mesh->GetSkeleton());Space->SetPreviewMesh(Mesh);
    for(int32 I=0;I<3;++I)
        if(!Space->ReplaceSampleAnimation(LoopIndices[I],Loops[I]))return Finish(R,TEXT("Forward loop reference substitution failed"));
    Space->ValidateSampleData();Space->ResampleData();
    for(int32 I=0;I<28;++I)
        if(!Space->GetBlendSamples()[I].bIsValid || !Space->ValidateAnimationSequence(Space->GetBlendSamples()[I].Animation))
            return Finish(R,TEXT("Candidate BlendSpace contains an invalid sample"));
    Graph->Modify();
    auto* Select=Node<UAnimGraphNode_BlendListByBool>(Graph,TEXT("VS2Motion_Eligibility"),-200,1800);
    if(!BlendSettings(Select->Node,{.08f,0.f},false))return Finish(R,TEXT("Bool blend native settings unavailable"));
    Select->AllocateDefaultPins();
    auto* Motion=Node<UAnimGraphNode_BlendListByInt>(Graph,TEXT("VS2Motion_Transitions"),-550,1800);
    while(Motion->Node.GetBlendTimes().Num()<11)Motion->Node.AddPose();
    TArray<float> Times;Times.Init(.08f,11);Times[0]=.1f;Times[5]=Times[6]=Times[7]=.05f;
    if(!BlendSettings(Motion->Node,Times,true))return Finish(R,TEXT("Int blend reset/period settings unavailable"));
    Motion->ReconstructNode();
    auto* Loop=Node<UAnimGraphNode_BlendSpacePlayer>(Graph,TEXT("VS2Motion_Loop"),-2300,1400);
    Loop->Node.SetBlendSpace(Space);Loop->Node.SetPlayRate(1.f);Loop->Node.SetGroupName(TEXT("Locomotion"));Loop->Node.SetGroupMethod(EAnimSyncMethod::SyncGroup);Loop->AllocateDefaultPins();
    if(!Variable(Graph,Loop,TEXT("X"),TEXT("VS2SampleDirection"),1250)||!Variable(Graph,Loop,TEXT("Y"),TEXT("VS2GroundSpeed"),1350)
        ||!Variable(Graph,Motion,TEXT("ActiveChildIndex"),TEXT("VS2MotionPoseIndex"),1750)
        ||!Variable(Graph,Select,TEXT("bActiveValue"),TEXT("bVS2MotionEligible"),1650))return Finish(R,TEXT("Native movement variable links failed"));
    auto* ToCS=Node<UAnimGraphNode_LocalToComponentSpace>(Graph,TEXT("VS2Motion_ToCS"),-2050,1400);ToCS->AllocateDefaultPins();
    // Compatible legacy sequences have no new virtual tracks. Seed goals from
    // the evaluated FK pose every frame instead of depending on reference VBs.
    UEdGraphPin* GoalPose=Output(ToCS);
    const TCHAR* GoalSources[]={TEXT("Hips"),TEXT("Foot_L"),TEXT("Foot_R")};
    const TCHAR* GoalTargets[]={TEXT("VB VS2_IKRoot"),TEXT("VB VS2_IKFoot_L"),TEXT("VB VS2_IKFoot_R")};
    for(int32 I=0;I<3;++I)
    {
        const FString Name=FString::Printf(TEXT("VS2Motion_SeedGoal_%d"),I);
        auto* Copy=Node<UAnimGraphNode_CopyBone>(Graph,*Name,-2050+I*220,1000);
        Copy->Node.SourceBone.BoneName=GoalSources[I];Copy->Node.TargetBone.BoneName=GoalTargets[I];
        Copy->Node.bCopyTranslation=true;Copy->Node.bCopyRotation=true;Copy->Node.bCopyScale=false;
        Copy->Node.ControlSpace=BCS_ComponentSpace;Copy->AllocateDefaultPins();
        if(!Link(Graph,GoalPose,Copy->FindPin(TEXT("ComponentPose"))))return Finish(R,TEXT("Virtual goal initialization failed"));
        GoalPose=Output(Copy);
    }
    auto* Orient=Node<UAnimGraphNode_OrientationWarping>(Graph,TEXT("VS2Motion_Orientation"),-1800,1400);
    Orient->Node.Mode=EWarpingEvaluationMode::Manual;Orient->Node.IKFootRootBone.BoneName=TEXT("VB VS2_IKRoot");
    for(const FName Name:{FName(TEXT("VB VS2_IKFoot_L")),FName(TEXT("VB VS2_IKFoot_R"))}){FBoneReference B;B.BoneName=Name;Orient->Node.IKFootBones.Add(B);}
    for(const FName Name:{FName(TEXT("Spine")),FName(TEXT("Chest"))}){FBoneReference B;B.BoneName=Name;Orient->Node.SpineBones.Add(B);}
    Orient->Node.RotationAxis=EAxis::Z;Orient->Node.RotationInterpSpeed=10.f;Orient->AllocateDefaultPins();
    auto* Stride=Node<UAnimGraphNode_StrideWarping>(Graph,TEXT("VS2Motion_Stride"),-1500,1400);
    Stride->Node.Mode=EWarpingEvaluationMode::Manual;Stride->Node.bDisableIfMissingRootMotion=false;
    Stride->Node.PelvisBone.BoneName=TEXT("Hips");Stride->Node.IKFootRootBone.BoneName=TEXT("VB VS2_IKRoot");
    auto* Legs=Node<UAnimGraphNode_LegIK>(Graph,TEXT("VS2Motion_LegIK"),-1200,1400);
    Legs->Node.ReachPrecision=.05f;Legs->Node.MaxIterations=12;Legs->Node.SoftPercentLength=.98f;Legs->Node.SoftAlpha=1.f;
    for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
    {
        FStrideWarpingFootDefinition Foot;Foot.IKFootBone.BoneName=FName(*(FString(TEXT("VB VS2_IKFoot_"))+Side));
        Foot.FKFootBone.BoneName=FName(*(FString(TEXT("Foot_"))+Side));Foot.ThighBone.BoneName=FName(*(FString(TEXT("UpperLeg_"))+Side));Stride->Node.FootDefinitions.Add(Foot);
        FAnimLegIKDefinition Leg;Leg.IKFootBone=Foot.IKFootBone;Leg.FKFootBone=Foot.FKFootBone;Leg.NumBonesInLimb=2;
        // Preserve the observed FK knee plane; do not guess a bone-local hinge axis.
        Leg.bEnableRotationLimit=false;Leg.bEnableKneeTwistCorrection=false;Legs->Node.LegsDefinition.Add(Leg);
    }
    Stride->AllocateDefaultPins();Legs->AllocateDefaultPins();
    auto* ToLocal=Node<UAnimGraphNode_ComponentToLocalSpace>(Graph,TEXT("VS2Motion_ToLocal"),-900,1400);ToLocal->AllocateDefaultPins();
    if(!Link(Graph,Output(Loop),ToCS->FindPin(TEXT("LocalPose")))||!Link(Graph,GoalPose,Orient->FindPin(TEXT("ComponentPose")))
        ||!Link(Graph,Output(Orient),Stride->FindPin(TEXT("ComponentPose")))||!Link(Graph,Output(Stride),Legs->FindPin(TEXT("ComponentPose")))
        ||!Link(Graph,Output(Legs),ToLocal->FindPin(TEXT("ComponentPose")))||!Link(Graph,Output(ToLocal),Motion->FindPin(TEXT("BlendPose_0"))))
        return Finish(R,TEXT("Native component/local warping chain wiring failed"));
    if(!Variable(Graph,Orient,TEXT("OrientationAngle"),TEXT("VS2OrientationAngle"),1450)
        ||!Variable(Graph,Stride,TEXT("StrideScale"),TEXT("VS2StrideScale"),1600)
        ||!Variable(Graph,Stride,TEXT("StrideDirection"),TEXT("VS2StrideDirection"),1700))return Finish(R,TEXT("Warp inputs failed"));
    for(auto* Control:{static_cast<UAnimGraphNode_Base*>(Orient),static_cast<UAnimGraphNode_Base*>(Stride),static_cast<UAnimGraphNode_Base*>(Legs)})
        if(!Variable(Graph,Control,TEXT("Alpha"),TEXT("VS2WarpAlpha"),Control->NodePosY+400))return Finish(R,TEXT("Warp alpha wiring failed"));
    const float Rates[]={2.15f,1.9f,1,1,1,1,1,1,1,1};
    for(int32 I=0;I<10;++I)
    {
        const FString Name=FString::Printf(TEXT("VS2Motion_Clip_%02d"),I+1);
        auto* Player=Node<UAnimGraphNode_SequencePlayer>(Graph,*Name,-950,2200+I*130);
        Player->Node.SetSequence(Clips[I]);Player->Node.SetStartPosition(0);Player->Node.SetPlayRate(Rates[I]);
        Player->Node.SetLoopAnimation(I==7);Player->Node.SetGroupMethod(EAnimSyncMethod::DoNotSync);Player->AllocateDefaultPins();
        if(!Link(Graph,Output(Player),Motion->FindPin(FName(*FString::Printf(TEXT("BlendPose_%d"),I+1)))))return Finish(R,TEXT("Sequence branch pin wiring failed"));
    }
    Prior->BreakLinkTo(Sink);
    if(!Link(Graph,Prior,Select->FindPin(TEXT("BlendPose_1")))||!Link(Graph,Output(Motion),Select->FindPin(TEXT("BlendPose_0")))
        ||!Link(Graph,Output(Select),Sink))return Finish(R,TEXT("Legacy/new eligibility branch wiring failed"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error || !BP->GeneratedClass)return Finish(R,TEXT("Candidate graph native compilation failed"));
    auto* Defaults=Cast<UHCM5VS2LookAnimInstance>(BP->GeneratedClass->GetDefaultObject());
    if(!Defaults)return Finish(R,TEXT("Candidate motion defaults unavailable"));
    Defaults->bVS2GASMotionEnabled=true;BP->MarkPackageDirty();Space->MarkPackageDirty();
    R->SetNumberField(TEXT("side_back_samples_preserved"),16);R->SetNumberField(TEXT("total_samples"),28);
    R->SetNumberField(TEXT("new_sequence_players"),10);R->SetNumberField(TEXT("legacy_kawaii_preserved"),KawaiiBefore);
    R->SetNumberField(TEXT("legacy_slots_preserved"),SlotsBefore);R->SetNumberField(TEXT("legacy_look_preserved"),LookBefore);
    R->SetStringField(TEXT("branch"),TEXT("TP unarmed candidate; immediate original FP/combat fallback before existing slots/look/physics"));
    return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2GASMotionEditor::InspectMotionCandidate(UAnimBlueprint* BP,USkeletalMesh* Mesh,UBlendSpace* Original,UBlendSpace* Space)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Owned(BP)||!Owned(Mesh)||!Owned(Space)||!Original||!Owned(Mesh->GetSkeleton())
        || Package(Original)!=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun")
        || BP->Status==BS_Error || BP->ParentClass!=UHCM5VS2LookAnimInstance::StaticClass()
        || BP->TargetSkeleton!=Mesh->GetSkeleton() || Space->GetSkeleton()!=Mesh->GetSkeleton()
        || !SameRawRef(Original->GetSkeleton()->GetReferenceSkeleton(),Mesh->GetRefSkeleton())
        || !SameRawRef(Original->GetSkeleton()->GetReferenceSkeleton(),Mesh->GetSkeleton()->GetReferenceSkeleton())
        || Space->GetBlendSamples().Num()!=28 || Original->GetBlendSamples().Num()!=28)
        return Finish(R,TEXT("Candidate identity/hierarchy/compiled graph mismatch"));
    const auto* Defaults=BP->GeneratedClass?Cast<UHCM5VS2LookAnimInstance>(BP->GeneratedClass->GetDefaultObject()):nullptr;
    if(!Defaults || !Defaults->bVS2GASMotionEnabled)return Finish(R,TEXT("Saved candidate motion opt-in missing"));
    TArray<TSharedPtr<FJsonValue>> Samples,Virtuals,Nodes;
    for(const FVirtualBone& Bone:Mesh->GetSkeleton()->GetVirtualBones())
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Bone.VirtualBoneName.ToString());
        Row->SetStringField(TEXT("source"),Bone.SourceBoneName.ToString());Row->SetStringField(TEXT("target"),Bone.TargetBoneName.ToString());
        Virtuals.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(Virtuals.Num()!=Original->GetSkeleton()->GetVirtualBones().Num()+3)return Finish(R,TEXT("Expected three private virtual bones"));
    for(int32 I=0;I<28;++I)
    {
        const auto& A=Original->GetBlendSamples()[I];const auto& B=Space->GetBlendSamples()[I];
        if(A.SampleValue!=B.SampleValue || A.RateScale!=B.RateScale || !B.bIsValid || !B.Animation
            || (I!=0 && I!=9 && I!=27 && A.Animation!=B.Animation)
            || ((I==0 || I==9 || I==27) && !Owned(B.Animation)))
            return Finish(R,TEXT("Authored blend positions/rates/directional samples changed"));
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),I);Row->SetStringField(TEXT("animation"),Package(B.Animation));
        Row->SetNumberField(TEXT("x"),B.SampleValue.X);Row->SetNumberField(TEXT("y"),B.SampleValue.Y);Row->SetNumberField(TEXT("z"),B.SampleValue.Z);
        Row->SetNumberField(TEXT("sample_rate"),B.RateScale);Samples.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);TArray<UEdGraphNode*> CandidateNodes;
    int32 Kawaii=0,Look=0,Slots=0,Sequences=0,Copies=0,Orient=0,Stride=0,Leg=0,BlendSpaces=0;
    for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)
    {
        Kawaii+=N->GetClass()->GetName().Contains(TEXT("KawaiiPhysics"))?1:0;
        Look+=N->GetClass()->GetName()==TEXT("AnimGraphNode_LookAt")?1:0;
        Slots+=N->GetClass()->GetName()==TEXT("AnimGraphNode_Slot")?1:0;
        if(N->GetName().StartsWith(TEXT("VS2Motion_")))CandidateNodes.Add(N);
    }
    CandidateNodes.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.GetName()<B.GetName();});
    for(UEdGraphNode* N:CandidateNodes)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),N->GetName());Row->SetStringField(TEXT("class"),N->GetClass()->GetName());
        TArray<TSharedPtr<FJsonValue>> Pins;
        for(UEdGraphPin* Pin:N->Pins)
        {
            auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("name"),Pin->PinName.ToString());
            P->SetStringField(TEXT("default"),Pin->DefaultValue);P->SetStringField(TEXT("default_object"),GetPathNameSafe(Pin->DefaultObject));
            P->SetBoolField(TEXT("output"),Pin->Direction==EGPD_Output);TArray<TSharedPtr<FJsonValue>> Links;
            for(UEdGraphPin* LinkPin:Pin->LinkedTo)Links.Add(MakeShared<FJsonValueString>(LinkPin->GetOwningNode()->GetName()+TEXT(".")+LinkPin->PinName.ToString()));
            P->SetArrayField(TEXT("links"),Links);Pins.Add(MakeShared<FJsonValueObject>(P));
        }
        Row->SetArrayField(TEXT("pins"),Pins);
        if(auto* S=Cast<UAnimGraphNode_SequencePlayer>(N))
        { ++Sequences;Row->SetStringField(TEXT("sequence"),Package(S->Node.GetSequence()));Row->SetNumberField(TEXT("rate"),S->Node.GetPlayRate()); }
        if(auto* S=Cast<UAnimGraphNode_BlendSpacePlayer>(N))
        { ++BlendSpaces;Row->SetStringField(TEXT("blendspace"),Package(S->Node.GetBlendSpace()));if(S->Node.GetBlendSpace()!=Space)return Finish(R,TEXT("Candidate loop reference wrong")); }
        if(auto* C=Cast<UAnimGraphNode_CopyBone>(N))
        { ++Copies;Row->SetStringField(TEXT("source_bone"),C->Node.SourceBone.BoneName.ToString());Row->SetStringField(TEXT("target_bone"),C->Node.TargetBone.BoneName.ToString()); }
        if(auto* C=Cast<UAnimGraphNode_OrientationWarping>(N))
        { ++Orient;if(C->Node.Mode!=EWarpingEvaluationMode::Manual)return Finish(R,TEXT("Orientation mode changed")); }
        if(auto* C=Cast<UAnimGraphNode_StrideWarping>(N))
        { ++Stride;if(C->Node.Mode!=EWarpingEvaluationMode::Manual || C->Node.bDisableIfMissingRootMotion)return Finish(R,TEXT("Stride mode changed")); }
        Leg+=Cast<UAnimGraphNode_LegIK>(N)?1:0;
        if(auto* V=Cast<UK2Node_VariableGet>(N))Row->SetStringField(TEXT("property"),V->VariableReference.GetMemberName().ToString());
        Nodes.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(Kawaii!=11||Look!=3||Slots<3||Sequences!=10||Copies!=3||Orient!=1||Stride!=1||Leg!=1||BlendSpaces!=1)
        return Finish(R,TEXT("Native candidate/legacy graph node counts differ"));
    R->SetStringField(TEXT("blueprint"),Package(BP));R->SetStringField(TEXT("mesh"),Package(Mesh));R->SetStringField(TEXT("skeleton"),Package(Mesh->GetSkeleton()));
    R->SetBoolField(TEXT("motion_enabled"),true);R->SetArrayField(TEXT("samples"),Samples);R->SetArrayField(TEXT("virtual_bones"),Virtuals);R->SetArrayField(TEXT("nodes"),Nodes);
    R->SetNumberField(TEXT("raw_bones_exact"),Mesh->GetRefSkeleton().GetRawBoneNum());R->SetNumberField(TEXT("legacy_kawaii"),Kawaii);
    R->SetNumberField(TEXT("legacy_look"),Look);R->SetNumberField(TEXT("legacy_slots"),Slots);
    return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
