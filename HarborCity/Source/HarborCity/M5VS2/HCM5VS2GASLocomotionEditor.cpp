#include "HCM5VS2GASLocomotionEditor.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
const FString Hero=TEXT("/Game/HarborCity/M5VS2/HeroSelestia");
const FString Old=TEXT("/Game/HarborCity/M5VS1/HeroSelestia");
const FString GAS=Hero+TEXT("/Animation/GAS");
const FString OriginalSpace=Old+TEXT("/Animation/BS_Idle_Walk_Run_Selestia");
const FString CandidateSpace=GAS+TEXT("/BS_M5VS2_GAS_IdleWalkRun");
FString Package(const UObject* O) { return O?O->GetOutermost()->GetName():FString(); }
FString Finish(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{
    if (!Error.IsEmpty()) { R->SetStringField(TEXT("status"),TEXT("FAIL"));R->SetStringField(TEXT("error"),Error); }
    FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;
}
bool InputPin(const UEdGraphNode* N,const FName Name)
{
    for (const UEdGraphPin* P:N->Pins) if (P&&P->Direction==EGPD_Input&&P->PinName==Name)return true;
    return false;
}
bool SprintPath(const FString& Path)
{
    const FString Prefix=GAS+TEXT("/Sprint/Batch_");
    const FString Suffix=TEXT("/Animations/M_Relaxed_Sprint_Loop_F_InPlace_SelestiaGAS");
    if(!Path.StartsWith(Prefix)||!Path.EndsWith(Suffix)||Path.Len()!=Prefix.Len()+12+Suffix.Len())return false;
    const FString Token=Path.Mid(Prefix.Len(),12);
    for(TCHAR C:Token)if(!FChar::IsHexDigit(C)||FChar::ToLower(C)!=C)return false;
    return true;
}
}
#endif

FString UHCM5VS2GASLocomotionEditor::ApplyForwardLocomotion(UAnimBlueprint* Blueprint,UBlendSpace* Source,
    UBlendSpace* Target,UAnimSequence* Idle,UAnimSequence* Walk,UAnimSequence* Run,UAnimSequence* Sprint,
    float WalkSampleSpeed,float RunSampleRate,float SprintSampleRate)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("saved_by_helper"),false);
    if(!Blueprint||Package(Blueprint)!=Hero+TEXT("/Animation/ABP_M5VS2_Selestia_Physics")||!Source||!Target||Source==Target
        ||Package(Source)!=OriginalSpace||Package(Target)!=CandidateSpace||Source->GetClass()!=Target->GetClass()
        ||!Source->GetSkeleton()||Source->GetSkeleton()!=Target->GetSkeleton()||Blueprint->TargetSkeleton!=Source->GetSkeleton()
        ||Package(Source->GetSkeleton())!=Old+TEXT("/SK_Selestia"))
        return Finish(R,TEXT("Exact new VS2 graph, original/candidate spaces and independent Selestia skeleton required"));
    if(Package(Idle)!=GAS+TEXT("/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS")
        ||Package(Walk)!=GAS+TEXT("/Retargeted/M_Relaxed_Walk_Loop_F_InPlace_SelestiaGAS")
        ||Package(Run)!=GAS+TEXT("/Retargeted/M_Relaxed_Run_Loop_F_InPlace_SelestiaGAS")||!SprintPath(Package(Sprint)))
        return Finish(R,TEXT("Exact four separately measured GAS clips required"));
    for(UAnimSequence* Clip:{Idle,Walk,Run,Sprint})
        if(!Clip||Clip->GetSkeleton()!=Source->GetSkeleton()||Clip->HasRootMotion()||Clip->bForceRootLock||Clip->IsValidAdditive()
            ||Clip->RateScale!=1.f||!Target->ValidateAnimationSequence(Clip))
            return Finish(R,TEXT("Target clip skeleton, additive, root extraction or unmeasured RateScale mismatch"));
    if(!FMath::IsFinite(WalkSampleSpeed)||WalkSampleSpeed<=20||WalkSampleSpeed>=350
        ||!FMath::IsFinite(RunSampleRate)||RunSampleRate<.5||RunSampleRate>2
        ||!FMath::IsFinite(SprintSampleRate)||SprintSampleRate<.5||SprintSampleRate>2)
        return Finish(R,TEXT("Out-of-range measured forward gait parameters"));
    const auto Before=Source->GetBlendSamples();
    if(Before.Num()!=27||Target->GetBlendSamples().Num()!=27)return Finish(R,TEXT("Exact intact 27-sample duplicate required"));
    TArray<int32> Idles;int32 ForwardWalk=INDEX_NONE,ForwardRun=INDEX_NONE;
    TSet<FIntPoint> Coordinates;
    for(int32 I=0;I<27;++I)
    {
        const FBlendSample& S=Before[I];
        const FVector V=S.SampleValue;
        if(!(S==Target->GetBlendSamples()[I])||!S.Animation||S.Animation->GetSkeleton()!=Source->GetSkeleton()||S.RateScale!=1.f
            ||V.Z!=0||V.X!=FMath::RoundToInt(V.X)||V.Y!=FMath::RoundToInt(V.Y)
            ||FMath::Abs(V.X)>180||FMath::RoundToInt(V.X)%45!=0||(V.Y!=0&&V.Y!=300&&V.Y!=600))
            return Finish(R,TEXT("Original source topology, rate or duplicate mismatch"));
        const FIntPoint Point(FMath::RoundToInt(V.X),FMath::RoundToInt(V.Y));
        if(Coordinates.Contains(Point))return Finish(R,TEXT("Repeated source coordinates"));Coordinates.Add(Point);
        if(V.Y==0)
        {
            if(Package(S.Animation.Get())!=Old+TEXT("/Animation/MM_Idle_Selestia"))return Finish(R,TEXT("All nine zero-speed samples must be original Idle"));
            Idles.Add(I);
        }
        else if(V.X==0)
        {
            if(V.Y==300&&Package(S.Animation.Get())==Old+TEXT("/Animation/MF_Unarmed_Walk_Fwd_Selestia"))ForwardWalk=I;
            else if(V.Y==600&&Package(S.Animation.Get())==Old+TEXT("/Animation/MF_Unarmed_Jog_Fwd_Selestia"))ForwardRun=I;
            else return Finish(R,TEXT("Original forward clip identity mismatch"));
        }
    }
    if(Coordinates.Num()!=27||Idles.Num()!=9||ForwardWalk==INDEX_NONE||ForwardRun==INDEX_NONE)
        return Finish(R,TEXT("Incomplete original 9 directions by 3 speeds"));
    const FBlendParameter& OldSpeed=Source->GetBlendParameter(1);
    if(OldSpeed.Min!=0||OldSpeed.Max!=600||OldSpeed.GridNum!=4||!OldSpeed.bSnapToGrid)
        return Finish(R,TEXT("Actual source speed axis changed since probe"));
    FArrayProperty* SamplesProperty=FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(),TEXT("SampleData"));
    FStructProperty* AxesProperty=FindFProperty<FStructProperty>(UBlendSpace::StaticClass(),TEXT("BlendParameters"));
    FStructProperty* SampleStruct=SamplesProperty?CastField<FStructProperty>(SamplesProperty->Inner):nullptr;
    if(!SamplesProperty||!SampleStruct||SampleStruct->Struct!=FBlendSample::StaticStruct()||!AxesProperty
        ||AxesProperty->Struct!=FBlendParameter::StaticStruct()||AxesProperty->ArrayDim!=3
        ||!SamplesProperty->HasAnyPropertyFlags(CPF_Edit)||!AxesProperty->HasAnyPropertyFlags(CPF_Edit))
        return Finish(R,TEXT("Actual native editable BlendSpace property schema mismatch"));
    TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
    TArray<UAnimGraphNode_BlendSpacePlayer*> Players;TArray<UAnimGraphNode_SequencePlayer*> IdlePlayers;
    for(UEdGraph* G:Graphs)if(G)for(UEdGraphNode* N:G->Nodes)
    {
        if(auto* P=Cast<UAnimGraphNode_BlendSpacePlayer>(N);P&&P->Node.GetBlendSpace()==Source)Players.Add(P);
        if(auto* P=Cast<UAnimGraphNode_SequencePlayer>(N);P&&Package(P->Node.GetSequence())==Old+TEXT("/Animation/MM_Idle_Selestia"))IdlePlayers.Add(P);
    }
    if(Players.Num()!=1||IdlePlayers.Num()!=1||IdlePlayers[0]->GetGraph()->GetName()!=TEXT("Idle")
        ||InputPin(Players[0],TEXT("BlendSpace"))||InputPin(IdlePlayers[0],TEXT("Sequence")))
        return Finish(R,TEXT("Exactly one unexposed BS player and one independent Idle player required"));
    const bool SourceDirty=Source->GetOutermost()->IsDirty();
    Target->Modify();
    // These are the actual EditAnywhere fields used by Details editing. No
    // const_cast, alternate layout cast, or private method is used.
    FBlendParameter* Speed=AxesProperty->ContainerPtrToValuePtr<FBlendParameter>(Target,1);
    Speed->Max=650.f;Speed->bSnapToGrid=false;
    for(int32 I:Idles)if(!Target->ReplaceSampleAnimation(I,Idle))return Finish(R,TEXT("Native Idle replacement failed; do not save"));
    if(!Target->ReplaceSampleAnimation(ForwardWalk,Run)||!Target->EditSampleValue(ForwardWalk,FVector(0,400,0))
        ||!Target->ReplaceSampleAnimation(ForwardRun,Sprint)||!Target->EditSampleValue(ForwardRun,FVector(0,650,0)))
        return Finish(R,TEXT("Native forward sample edits failed; do not save"));
    const int32 NewWalk=Target->AddSample(Walk,FVector(0,WalkSampleSpeed,0));
    if(NewWalk!=27)return Finish(R,TEXT("Expected precisely one appended low-speed Walk sample; do not save"));
    auto* Samples=SamplesProperty->ContainerPtrToValuePtr<TArray<FBlendSample>>(Target);
    (*Samples)[ForwardWalk].RateScale=RunSampleRate;(*Samples)[ForwardRun].RateScale=SprintSampleRate;
    auto Expected=Before;for(int32 I:Idles)Expected[I].Animation=Idle;
    Expected[ForwardWalk].Animation=Run;Expected[ForwardWalk].SampleValue=FVector(0,400,0);Expected[ForwardWalk].RateScale=RunSampleRate;
    Expected[ForwardRun].Animation=Sprint;Expected[ForwardRun].SampleValue=FVector(0,650,0);Expected[ForwardRun].RateScale=SprintSampleRate;
    Expected.Add((*Samples)[NewWalk]);
    Target->ValidateSampleData();Target->ResampleData();
    if(Target->GetBlendSamples().Num()!=28)return Finish(R,TEXT("Validation altered exact new sample count; do not save"));
    for(int32 I=0;I<28;++I)
        if(!(Target->GetBlendSamples()[I]==Expected[I])||!Target->GetBlendSamples()[I].bIsValid)
            return Finish(R,TEXT("Native validation changed requested or preserved sample data; do not save"));
    TArray<TSharedPtr<FJsonValue>> Weights;
    for(const FVector Point:{FVector(0,WalkSampleSpeed,0),FVector(0,400,0),FVector(0,650,0)})
    {
        TArray<FBlendSampleData> Weighted;int32 Triangle=INDEX_NONE;
        if(!Target->GetSamplesFromBlendInput(Point,Weighted,Triangle,false))return Finish(R,TEXT("Native triangulation failed; do not save"));
        const int32 Required=Point.Y==400?ForwardWalk:Point.Y==650?ForwardRun:NewWalk;
        float Weight=0;for(const auto& W:Weighted)if(W.SampleDataIndex==Required)Weight+=W.GetClampedWeight();
        if(!FMath::IsNearlyEqual(Weight,1.f,.0001f))return Finish(R,TEXT("Exact forward sample did not receive unit native interpolation weight"));
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("speed"),Point.Y);Row->SetNumberField(TEXT("sample_index"),Required);
        Row->SetNumberField(TEXT("weight"),Weight);Weights.Add(MakeShared<FJsonValueObject>(Row));
    }
    Blueprint->Modify();Players[0]->Modify();IdlePlayers[0]->Modify();
    if(!Players[0]->Node.SetBlendSpace(Target)||!IdlePlayers[0]->Node.SetSequence(Idle))
        return Finish(R,TEXT("Native BS/independent Idle assignment failed; do not save"));
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if(Blueprint->Status!=BS_UpToDate&&Blueprint->Status!=BS_UpToDateWithWarnings)
        return Finish(R,TEXT("VS2 compile failed; do not save"));
    if(Source->GetOutermost()->IsDirty()!=SourceDirty)return Finish(R,TEXT("Original space dirty flag changed"));
    Target->MarkPackageDirty();Blueprint->MarkPackageDirty();
    R->SetStringField(TEXT("status"),TEXT("APPLIED_COMPILED_NOT_SAVED"));R->SetNumberField(TEXT("sample_count"),28);
    R->SetNumberField(TEXT("preserved_side_back_samples"),16);R->SetNumberField(TEXT("changed_zero_speed_idle_samples"),9);
    R->SetStringField(TEXT("blendspace_node"),Players[0]->GetPathName());R->SetStringField(TEXT("independent_idle_node"),IdlePlayers[0]->GetPathName());
    R->SetArrayField(TEXT("native_exact_forward_sample_weights"),Weights);
    R->SetStringField(TEXT("runtime_sliding"),TEXT("NOT_RUN; native interpolation weights are not foot contact or visual acceptance"));
    return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
