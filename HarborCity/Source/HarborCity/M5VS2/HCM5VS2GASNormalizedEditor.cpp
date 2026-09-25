#include "HCM5VS2GASNormalizedEditor.h"
#include "HCM5VS2LookAnimInstance.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
FString Package(const UObject* O) { return O?O->GetOutermost()->GetName():FString(); }
const FString Prefix=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_");
FString Batch(const UObject* O)
{
    const FString P=Package(O);
    if(!P.StartsWith(Prefix)||P.Len()<Prefix.Len()+14||P[Prefix.Len()+12]!='/')return FString();
    const FString Token=P.Mid(Prefix.Len(),12);
    for(TCHAR C:Token)if(!FChar::IsHexDigit(C))return FString();
    return Prefix+Token;
}
FString Done(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{
    R->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    if(!Error.IsEmpty())R->SetStringField(TEXT("error"),Error);
    R->SetBoolField(TEXT("saved_by_helper"),false);
    FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;
}
UAnimGraphNode_BlendSpacePlayer* MotionLoop(UAnimBlueprint* BP)
{
    UAnimGraphNode_BlendSpacePlayer* Result=nullptr;
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
    for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)
        if(N->GetName()==TEXT("VS2Motion_Loop"))
        {if(Result)return nullptr;Result=Cast<UAnimGraphNode_BlendSpacePlayer>(N);if(!Result)return nullptr;}
    return Result;
}
FString Wiring(UAnimBlueprint* BP)
{
    TArray<FString> Rows;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
    for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)
    {
        const FString Key=G->GetName()+TEXT("|")+N->GetName()+TEXT("|")+N->GetClass()->GetName();Rows.Add(Key);
        for(UEdGraphPin* Pin:N->Pins)
        {
            Rows.Add(Key+TEXT("|")+Pin->PinName.ToString()+TEXT("=")+Pin->DefaultValue);
            for(UEdGraphPin* Link:Pin->LinkedTo)
                Rows.Add(Key+TEXT("|")+Pin->PinName.ToString()+TEXT("->")+Link->GetOwningNode()->GetName()+TEXT(".")+Link->PinName.ToString());
        }
    }
    Rows.Sort();return FString::Join(Rows,TEXT("\n"));
}
bool ConstantWeight(const FFloatCurve* Curve,float Length)
{
    if(!Curve||Curve->FloatCurve.GetNumKeys()!=2)return false;
    const auto& Keys=Curve->FloatCurve.GetConstRefOfKeys();
    if(Keys[0].Time!=0||FMath::Abs(Keys[1].Time-Length)>1.e-5)return false;
    for(const FRichCurveKey& K:Keys)
        if(K.Value!=1||K.InterpMode!=RCIM_Constant)return false;
    for(int32 I=0;I<=16;++I)if(Curve->FloatCurve.Eval(Length*I/16.f)!=1)return false;
    return true;
}
}
#endif

FString UHCM5VS2GASNormalizedEditor::ConfigureNormalizedLoops(UAnimBlueprint* SourceBP,UBlendSpace* SourceSpace,
    UAnimBlueprint* CandidateBP,UBlendSpace* CandidateSpace,const TArray<UAnimSequence*>& Loops,bool bApply)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    const FString Destination=Batch(CandidateBP);
    if(!SourceBP||!SourceSpace||!CandidateBP||!CandidateSpace||Destination.IsEmpty()||Batch(SourceBP).IsEmpty()
        ||Batch(SourceSpace).IsEmpty()||Destination==Batch(SourceBP)||Destination==Batch(SourceSpace)
        ||Batch(CandidateSpace)!=Destination||Loops.Num()!=3||SourceBP==CandidateBP||SourceSpace==CandidateSpace
        ||SourceBP->TargetSkeleton!=CandidateBP->TargetSkeleton||SourceSpace->GetSkeleton()!=CandidateSpace->GetSkeleton()
        ||SourceSpace->GetBlendSamples().Num()!=28||CandidateSpace->GetBlendSamples().Num()!=28
        ||SourceBP->ParentClass!=UHCM5VS2LookAnimInstance::StaticClass()||CandidateBP->ParentClass!=SourceBP->ParentClass)
        return Done(R,TEXT("Exact distinct private GASMotion/FlightPose source and candidate identities required"));
    auto* OriginalNode=MotionLoop(SourceBP);auto* CandidateNode=MotionLoop(CandidateBP);
    if(!OriginalNode||!CandidateNode||OriginalNode->Node.GetBlendSpace()!=SourceSpace
        ||CandidateNode->Node.GetBlendSpace()!=(bApply?SourceSpace:CandidateSpace)
        ||Wiring(SourceBP)!=Wiring(CandidateBP))return Done(R,TEXT("Exact retained graph wiring and unique VS2Motion_Loop required"));
    const int32 Indices[]={0,9,27};const TCHAR* Names[]={TEXT("Run_WarpLoop"),TEXT("Sprint_WarpLoop"),TEXT("Walk_WarpLoop")};
    const FAnimationCurveIdentifier WeightId(TEXT("VS2NominalRootWeight"),ERawCurveTrackTypes::RCT_Float);
    const FAnimationCurveIdentifier SpeedId(TEXT("VS2NominalRootSpeed"),ERawCurveTrackTypes::RCT_Float);
    for(int32 I=0;I<3;++I)
    {
        const UAnimSequence* Source=SourceSpace->GetBlendSamples()[Indices[I]].Animation;
        UAnimSequence* Target=Loops[I];
        if(!Source||!Target||Batch(Target)!=Destination||Package(Target)!=Destination/TEXT("Loops")/Names[I]
            ||Source==Target||!Source->GetDataModel()||!Target->GetDataModel()
            ||Source->GetSkeleton()!=Target->GetSkeleton()||Source->RateScale!=Target->RateScale
            ||Source->GetPlayLength()!=Target->GetPlayLength()||Source->GetNumberOfSampledKeys()!=Target->GetNumberOfSampledKeys()
            ||Source->GetSamplingFrameRate()!=Target->GetSamplingFrameRate()
            ||Source->bEnableRootMotion!=Target->bEnableRootMotion||Source->bForceRootLock!=Target->bForceRootLock
            ||!Source->GetDataModel()->FindFloatCurve(SpeedId)||!Target->GetDataModel()->FindFloatCurve(SpeedId)
            ||Source->GetDataModel()->FindFloatCurve(WeightId))return Done(R,TEXT("Three unchanged source-loop copies without source weight curve required"));
        if(Source->AuthoredSyncMarkers.Num()!=Target->AuthoredSyncMarkers.Num())return Done(R,TEXT("Authored marker count changed"));
        for(int32 Marker=0;Marker<Source->AuthoredSyncMarkers.Num();++Marker)
            if(Source->AuthoredSyncMarkers[Marker].MarkerName!=Target->AuthoredSyncMarkers[Marker].MarkerName
                ||Source->AuthoredSyncMarkers[Marker].Time!=Target->AuthoredSyncMarkers[Marker].Time)
                return Done(R,TEXT("Authored marker name/time changed"));
        const FFloatCurve* Weight=Target->GetDataModel()->FindFloatCurve(WeightId);
        if(bApply?Weight!=nullptr:!ConstantWeight(Weight,Target->GetPlayLength()))return Done(R,TEXT("Fresh apply or exact constant-one reload curve required"));
    }
    for(int32 I=0;I<28;++I)
    {
        const FBlendSample& A=SourceSpace->GetBlendSamples()[I];const FBlendSample& B=CandidateSpace->GetBlendSamples()[I];
        int32 LoopIndex=INDEX_NONE;for(int32 K=0;K<3;++K)if(I==Indices[K])LoopIndex=K;
        UAnimSequence* Expected=!bApply&&LoopIndex!=INDEX_NONE?Loops[LoopIndex]:A.Animation.Get();
        if(A.SampleValue!=B.SampleValue||A.RateScale!=B.RateScale||B.Animation!=Expected)
            return Done(R,TEXT("Sample rate/positions/directional references changed"));
    }
    if(bApply)
    {
        for(int32 I=0;I<3;++I)
        {
            UAnimSequence* Target=Loops[I];Target->Modify();
            TArray<FRichCurveKey> Keys={FRichCurveKey(0,1),FRichCurveKey(Target->GetPlayLength(),1)};
            for(FRichCurveKey& K:Keys)K.InterpMode=RCIM_Constant;
            auto& C=Target->GetController();C.OpenBracket(FText::FromString(TEXT("Private nominal speed weight normalization")),false);
            const bool Added=C.AddCurve(WeightId,AACF_DefaultCurve,false)&&C.SetCurveKeys(WeightId,Keys,false);C.CloseBracket(false);
            if(!Added||!ConstantWeight(Target->GetDataModel()->FindFloatCurve(WeightId),Target->GetPlayLength()))
                return Done(R,TEXT("Native constant-one curve write/readback failed"));
            Target->MarkPackageDirty();
        }
        CandidateSpace->Modify();CandidateBP->Modify();
        for(int32 I=0;I<3;++I)if(!CandidateSpace->ReplaceSampleAnimation(Indices[I],Loops[I]))return Done(R,TEXT("Native forward-loop replacement failed"));
        CandidateSpace->ValidateSampleData();CandidateSpace->ResampleData();
        CandidateNode->Node.SetBlendSpace(CandidateSpace);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CandidateBP);FKismetEditorUtilities::CompileBlueprint(CandidateBP);
        CandidateBP->MarkPackageDirty();CandidateSpace->MarkPackageDirty();
    }
    const auto* Defaults=CandidateBP->GeneratedClass?Cast<UHCM5VS2LookAnimInstance>(CandidateBP->GeneratedClass->GetDefaultObject()):nullptr;
    if(CandidateBP->Status==BS_Error||!Defaults||!Defaults->bVS2GASMotionEnabled||!Defaults->bVS2FlightPosesEnabled
        ||Wiring(SourceBP)!=Wiring(CandidateBP)||MotionLoop(CandidateBP)->Node.GetBlendSpace()!=CandidateSpace)
        return Done(R,TEXT("Retained compiled GAS/flight graph guard failed"));
    for(const FBlendSample& Sample:CandidateSpace->GetBlendSamples())if(!Sample.bIsValid)return Done(R,TEXT("Invalid candidate blend sample"));
    R->SetNumberField(TEXT("samples_preserved"),28);R->SetNumberField(TEXT("side_back_preserved"),16);
    R->SetNumberField(TEXT("constant_one_curves"),3);R->SetBoolField(TEXT("graph_wiring_unchanged"),true);
    R->SetStringField(TEXT("candidate_blueprint"),Package(CandidateBP));R->SetStringField(TEXT("candidate_space"),Package(CandidateSpace));
    return Done(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
