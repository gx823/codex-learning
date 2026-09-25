#include "HCM5VS2GASClockEditor.h"
#include "HCM5VS2LookAnimInstance.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

namespace
{
FString Package(const UObject* Object)
{
    return Object ? Object->GetOutermost()->GetName() : FString();
}

FString Batch(const UObject* Object)
{
    const FString Prefix = TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_");
    const FString Path = Package(Object);
    if (!Path.StartsWith(Prefix) || Path.Len() < Prefix.Len() + 14 || Path[Prefix.Len() + 12] != '/')
        return FString();
    const FString Token = Path.Mid(Prefix.Len(), 12);
    for (TCHAR Character : Token)
        if (!FChar::IsHexDigit(Character)) return FString();
    return Prefix + Token;
}

FString Done(const TSharedRef<FJsonObject>& Result, const FString& Error = FString())
{
    Result->SetStringField(TEXT("status"), Error.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    if (!Error.IsEmpty()) Result->SetStringField(TEXT("error"), Error);
    Result->SetBoolField(TEXT("saved_by_helper"), false);
    FString Text;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
    return Text;
}

UAnimGraphNode_BlendSpacePlayer* MotionLoop(UAnimBlueprint* Blueprint)
{
    UAnimGraphNode_BlendSpacePlayer* Result = nullptr;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (Node->GetName() == TEXT("VS2Motion_Loop"))
            {
                if (Result) return nullptr;
                Result = Cast<UAnimGraphNode_BlendSpacePlayer>(Node);
                if (!Result) return nullptr;
            }
    return Result;
}

FString Wiring(UAnimBlueprint* Blueprint)
{
    TArray<FString> Rows;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            const FString Key = Graph->GetName() + TEXT("|") + Node->GetName() + TEXT("|") + Node->GetClass()->GetName();
            Rows.Add(Key);
            for (UEdGraphPin* Pin : Node->Pins)
            {
                Rows.Add(Key + TEXT("|") + Pin->PinName.ToString() + TEXT("=") + Pin->DefaultValue);
                for (UEdGraphPin* Link : Pin->LinkedTo)
                    Rows.Add(Key + TEXT("|") + Pin->PinName.ToString() + TEXT("->")
                        + Link->GetOwningNode()->GetName() + TEXT(".") + Link->PinName.ToString());
            }
        }
    Rows.Sort();
    return FString::Join(Rows, TEXT("\n"));
}

bool SameSamples(const UBlendSpace* Source, const UBlendSpace* Candidate)
{
    if (Source->GetBlendSamples().Num() != 28 || Candidate->GetBlendSamples().Num() != 28) return false;
    for (int32 Index = 0; Index < 28; ++Index)
    {
        const FBlendSample& A = Source->GetBlendSamples()[Index];
        const FBlendSample& B = Candidate->GetBlendSamples()[Index];
        if (A.Animation != B.Animation || A.SampleValue != B.SampleValue || A.RateScale != B.RateScale
            || A.bMirror != B.bMirror || A.bUseSingleFrameForBlending != B.bUseSingleFrameForBlending
            || A.FrameIndexToSample != B.FrameIndexToSample || A.bIncludeInAnalyseAll != B.bIncludeInAnalyseAll)
            return false;
    }
    return true;
}

// Compare every editable BlendSpace setting, including all three elements of
// axis/filter/analysis arrays. Only the intended clock flag and separately
// checked authored sample array are excluded; derived caches are not inputs.
FString SettingsDifference(const UBlendSpace* Source, const UBlendSpace* Candidate, int32& Compared)
{
    Compared = 0;
    for (TFieldIterator<FProperty> It(UBlendSpace::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const FProperty* Property = *It;
        if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->GetFName() == TEXT("bAllowMarkerBasedSync")
            || Property->GetFName() == TEXT("SampleData")) continue;
        for (int32 Element = 0; Element < Property->ArrayDim; ++Element)
        {
            ++Compared;
            if (!Property->Identical_InContainer(Source, Candidate, Element, PPF_DeepComparison))
                return FString::Printf(TEXT("Unexpected editable setting change: %s[%d]"), *Property->GetName(), Element);
        }
    }
    return FString();
}
}
#endif

FString UHCM5VS2GASClockEditor::ConfigureUnifiedClock(UAnimBlueprint* SourceBlueprint, UBlendSpace* SourceSpace,
    UAnimBlueprint* CandidateBlueprint, UBlendSpace* CandidateSpace, bool bApply)
{
#if WITH_EDITOR
    const auto Result = MakeShared<FJsonObject>();
    const FString Destination = Batch(CandidateBlueprint);
    if (!SourceBlueprint || !SourceSpace || !CandidateBlueprint || !CandidateSpace
        || Destination.IsEmpty() || Batch(SourceBlueprint).IsEmpty() || Batch(SourceSpace) != Batch(SourceBlueprint)
        || Batch(CandidateSpace) != Destination || Destination == Batch(SourceBlueprint)
        || SourceBlueprint == CandidateBlueprint || SourceSpace == CandidateSpace
        || SourceSpace->GetClass() != UBlendSpace::StaticClass() || CandidateSpace->GetClass() != SourceSpace->GetClass()
        || SourceBlueprint->TargetSkeleton != CandidateBlueprint->TargetSkeleton
        || SourceSpace->GetSkeleton() != CandidateSpace->GetSkeleton()
        || SourceBlueprint->TargetSkeleton != SourceSpace->GetSkeleton()
        || SourceBlueprint->ParentClass != UHCM5VS2LookAnimInstance::StaticClass()
        || CandidateBlueprint->ParentClass != SourceBlueprint->ParentClass || !SameSamples(SourceSpace, CandidateSpace))
        return Done(Result, TEXT("Distinct private GASMotion copies with exactly retained skeleton, 28 samples and rates required"));

    UAnimGraphNode_BlendSpacePlayer* SourceNode = MotionLoop(SourceBlueprint);
    UAnimGraphNode_BlendSpacePlayer* CandidateNode = MotionLoop(CandidateBlueprint);
    if (!SourceNode || !CandidateNode || SourceNode->Node.GetBlendSpace() != SourceSpace
        || CandidateNode->Node.GetBlendSpace() != (bApply ? SourceSpace : CandidateSpace)
        || Wiring(SourceBlueprint) != Wiring(CandidateBlueprint))
        return Done(Result, TEXT("Exact retained graph wiring and unique VS2Motion_Loop required"));

    const FIntProperty* CacheProperty = FindFProperty<FIntProperty>(UBlendSpace::StaticClass(), TEXT("SampleIndexWithMarkers"));
    if (!CacheProperty || !SourceSpace->bAllowMarkerBasedSync || CandidateSpace->bAllowMarkerBasedSync != bApply)
        return Done(Result, TEXT("Source marker sync must be enabled; candidate must be a fresh copy or disabled reload"));
    const int32 SourceMarkerIndex = CacheProperty->GetPropertyValue_InContainer(SourceSpace);
    if (SourceMarkerIndex == INDEX_NONE || !SourceSpace->GetBlendSamples()[0].Animation
        || SourceSpace->GetBlendSamples()[0].Animation->AuthoredSyncMarkers.IsEmpty()
        || !SourceSpace->GetBlendSamples()[9].Animation
        || !SourceSpace->GetBlendSamples()[9].Animation->AuthoredSyncMarkers.IsEmpty())
        return Done(Result, TEXT("Actual source marker leader and marked Run / markerless Sprint baseline required"));
    int32 SettingsCompared = 0;
    FString Difference = SettingsDifference(SourceSpace, CandidateSpace, SettingsCompared);
    if (!Difference.IsEmpty()) return Done(Result, Difference);

    if (bApply)
    {
        CandidateSpace->Modify();
        CandidateBlueprint->Modify();
        CandidateSpace->bAllowMarkerBasedSync = false;
        // Native validation derives SampleIndexWithMarkers. Do not mutate that
        // cache directly, alter source marker tables, or fabricate contact markers.
        CandidateSpace->ValidateSampleData();
        CandidateSpace->ResampleData();
        CandidateNode->Node.SetBlendSpace(CandidateSpace);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CandidateBlueprint);
        FKismetEditorUtilities::CompileBlueprint(CandidateBlueprint);
        CandidateSpace->MarkPackageDirty();
        CandidateBlueprint->MarkPackageDirty();
    }

    const int32 CandidateMarkerIndex = CacheProperty->GetPropertyValue_InContainer(CandidateSpace);
    const UHCM5VS2LookAnimInstance* Defaults = CandidateBlueprint->GeneratedClass
        ? Cast<UHCM5VS2LookAnimInstance>(CandidateBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    Difference = SettingsDifference(SourceSpace, CandidateSpace, SettingsCompared);
    if (CandidateBlueprint->Status == BS_Error || !Defaults || !Defaults->bVS2GASMotionEnabled
        || !Defaults->bVS2FlightPosesEnabled || !MotionLoop(CandidateBlueprint)
        || MotionLoop(CandidateBlueprint)->Node.GetBlendSpace() != CandidateSpace
        || Wiring(SourceBlueprint) != Wiring(CandidateBlueprint) || !SameSamples(SourceSpace, CandidateSpace)
        || !Difference.IsEmpty() || CandidateSpace->bAllowMarkerBasedSync || CandidateMarkerIndex != INDEX_NONE
        || !SourceSpace->bAllowMarkerBasedSync || CacheProperty->GetPropertyValue_InContainer(SourceSpace) != SourceMarkerIndex)
        return Done(Result, TEXT("Native compiled graph, single flag, or derived marker cache preservation guard failed: ") + Difference);
    for (const FBlendSample& Sample : CandidateSpace->GetBlendSamples())
        if (!Sample.bIsValid) return Done(Result, TEXT("Invalid candidate blend sample"));

    Result->SetNumberField(TEXT("samples_preserved"), 28);
    Result->SetNumberField(TEXT("side_back_preserved"), 16);
    Result->SetNumberField(TEXT("editable_settings_compared"), SettingsCompared);
    Result->SetBoolField(TEXT("source_allow_marker_based_sync"), true);
    Result->SetBoolField(TEXT("candidate_allow_marker_based_sync"), false);
    Result->SetNumberField(TEXT("source_sample_index_with_markers"), SourceMarkerIndex);
    Result->SetNumberField(TEXT("candidate_sample_index_with_markers"), CandidateMarkerIndex);
    Result->SetBoolField(TEXT("all_sample_references_and_rates_unchanged"), true);
    Result->SetBoolField(TEXT("graph_wiring_unchanged"), true);
    Result->SetStringField(TEXT("candidate_blueprint"), Package(CandidateBlueprint));
    Result->SetStringField(TEXT("candidate_space"), Package(CandidateSpace));
    return Done(Result);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
