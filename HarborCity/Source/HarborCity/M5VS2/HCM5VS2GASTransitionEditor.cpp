#include "HCM5VS2GASTransitionEditor.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimStateAliasNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
using FRows=TArray<TSharedPtr<FJsonValue>>;
const FString BlueprintPath=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics.ABP_M5VS2_Selestia_Physics");
const FString SkeletonPath=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SK_Selestia.SK_Selestia");

FString Finish(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{
    if(!Error.IsEmpty()) { R->SetStringField(TEXT("status"),TEXT("FAIL")); R->SetStringField(TEXT("error"),Error); }
    FString Text; FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text)); return Text;
}

TSharedRef<FJsonObject> Identity(const UObject* Object)
{
    auto R=MakeShared<FJsonObject>();
    R->SetStringField(TEXT("path"),GetPathNameSafe(Object));
    R->SetStringField(TEXT("class_name"),Object?Object->GetClass()->GetName():TEXT("None"));
    if(const UAnimStateNode* State=Cast<UAnimStateNode>(Object))
        R->SetStringField(TEXT("state_name"),State->BoundGraph?State->BoundGraph->GetName():TEXT("None"));
    if(const UAnimStateAliasNode* Alias=Cast<UAnimStateAliasNode>(Object))
        R->SetStringField(TEXT("alias_name"),Alias->StateAliasName);
    return R;
}

void ExportProperty(const UObject* Object,const FName Name,const TSharedRef<FJsonObject>& Out)
{
    if(const FProperty* Property=FindFProperty<FProperty>(Object->GetClass(),Name))
    {
        FRows Values;
        for(int32 Index=0;Index<Property->ArrayDim;++Index)
        {
            FString Text; Property->ExportText_InContainer(Index,Text,Object,nullptr,nullptr,PPF_None);
            Values.Add(MakeShared<FJsonValueString>(Text));
        }
        Out->SetArrayField(Name.ToString(),Values);
    }
}

TSharedRef<FJsonObject> NodeReadback(const UEdGraphNode* Node)
{
    auto R=Identity(Node); auto Properties=MakeShared<FJsonObject>();
    // Function/variable references identify actual rule operators rather than inferring them from pin names.
    for(const FName Name:{FName(TEXT("Node")),FName(TEXT("FunctionReference")),FName(TEXT("VariableReference"))})
        ExportProperty(Node,Name,Properties);
    R->SetObjectField(TEXT("reflected_properties"),Properties);
    FRows Pins;
    for(const UEdGraphPin* Pin:Node->Pins)
    {
        if(!Pin) continue;
        auto P=MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"),Pin->PinName.ToString());
        P->SetStringField(TEXT("id"),Pin->PinId.ToString(EGuidFormats::Digits));
        P->SetNumberField(TEXT("direction"),int32(Pin->Direction));
        P->SetBoolField(TEXT("hidden"),Pin->bHidden!=0);
        P->SetStringField(TEXT("default_value"),Pin->DefaultValue);
        P->SetStringField(TEXT("default_object"),GetPathNameSafe(Pin->DefaultObject));
        P->SetStringField(TEXT("default_text"),Pin->DefaultTextValue.ToString());
        FString Type; FEdGraphPinType::StaticStruct()->ExportText(Type,&Pin->PinType,nullptr,nullptr,PPF_None,nullptr);
        P->SetStringField(TEXT("type"),Type);
        TArray<FString> Links;
        for(const UEdGraphPin* Link:Pin->LinkedTo)
            if(Link) Links.Add(GetPathNameSafe(Link->GetOwningNode())+TEXT(":")+Link->PinName.ToString());
        Links.Sort(); FRows LinkRows;
        for(const FString& Link:Links) LinkRows.Add(MakeShared<FJsonValueString>(Link));
        P->SetArrayField(TEXT("links"),LinkRows); Pins.Add(MakeShared<FJsonValueObject>(P));
    }
    R->SetArrayField(TEXT("pins"),Pins);
    if(const auto* Player=Cast<UAnimGraphNode_SequencePlayer>(Node))
        R->SetStringField(TEXT("sequence"),GetPathNameSafe(Player->Node.GetSequence()));
    return R;
}

TSharedRef<FJsonObject> StateReadback(const UAnimStateNode* State)
{
    auto R=Identity(State);
    R->SetStringField(TEXT("bound_graph"),GetPathNameSafe(State->BoundGraph));
    R->SetBoolField(TEXT("always_reset_on_entry"),State->bAlwaysResetOnEntry);
    R->SetNumberField(TEXT("state_type"),int32(State->StateType));
    auto Properties=MakeShared<FJsonObject>();
    for(const FName Name:{FName(TEXT("StateEntered")),FName(TEXT("StateLeft")),FName(TEXT("StateFullyBlended"))})
        ExportProperty(State,Name,Properties);
    R->SetObjectField(TEXT("events"),Properties);
    return R;
}

TSharedRef<FJsonObject> TransitionReadback(const UAnimStateTransitionNode* Transition)
{
    auto R=Identity(Transition);
    R->SetStringField(TEXT("rule_graph"),GetPathNameSafe(Transition->BoundGraph));
    R->SetStringField(TEXT("custom_blend_graph"),GetPathNameSafe(Transition->CustomTransitionGraph));
    R->SetObjectField(TEXT("previous_node"),Identity(Transition->GetPreviousState()));
    R->SetObjectField(TEXT("next_node"),Identity(Transition->GetNextState()));
    R->SetNumberField(TEXT("priority_order"),Transition->PriorityOrder);
    R->SetNumberField(TEXT("crossfade_seconds"),Transition->CrossfadeDuration);
    R->SetNumberField(TEXT("blend_mode"),int32(Transition->BlendMode));
    R->SetNumberField(TEXT("logic_type"),int32(Transition->LogicType));
    R->SetBoolField(TEXT("automatic_rule"),Transition->bAutomaticRuleBasedOnSequencePlayerInState);
    R->SetNumberField(TEXT("automatic_rule_trigger_time_seconds"),Transition->AutomaticRuleTriggerTime);
    R->SetNumberField(TEXT("minimum_time_before_reentry_seconds"),Transition->MinTimeBeforeReentry);
    R->SetStringField(TEXT("required_sync_group"),Transition->SyncGroupNameToRequireValidMarkersRule.ToString());
    R->SetBoolField(TEXT("disabled"),Transition->bDisabled);
    R->SetBoolField(TEXT("bidirectional"),Transition->Bidirectional);
    R->SetBoolField(TEXT("only_evaluate_when_active"),Transition->bOnlyEvaluateWhenActive);
    R->SetBoolField(TEXT("allow_inertialization_for_self_transitions"),Transition->bAllowInertializationForSelfTransitions);
    auto Properties=MakeShared<FJsonObject>();
    // Preserve enum spellings, blend-profile interface and shared-rule/event metadata verbatim.
    for(const FName Name:{FName(TEXT("BlendMode")),FName(TEXT("LogicType")),FName(TEXT("CustomBlendCurve")),
        FName(TEXT("BlendProfileWrapper")),FName(TEXT("bSharedRules")),FName(TEXT("bSharedCrossfade")),
        FName(TEXT("SharedRulesName")),FName(TEXT("SharedRulesGuid")),FName(TEXT("SharedCrossfadeName")),
        FName(TEXT("SharedCrossfadeGuid")),FName(TEXT("SharedCrossfadeIdx")),FName(TEXT("TransitionStart")),
        FName(TEXT("TransitionEnd")),FName(TEXT("TransitionInterrupt"))}) ExportProperty(Transition,Name,Properties);
    R->SetObjectField(TEXT("reflected_metadata"),Properties);
    return R;
}

TSharedRef<FJsonObject> AliasReadback(const UAnimStateAliasNode* Alias)
{
    auto R=Identity(Alias); R->SetBoolField(TEXT("global_alias"),Alias->bGlobalAlias);
    FRows Members; TArray<const UAnimStateNodeBase*> ValidMembers; int32 Invalid=0;
    for(const TWeakObjectPtr<UAnimStateNodeBase>& Member:Alias->GetAliasedStates())
    {
        if(const UAnimStateNodeBase* State=Member.Get()) ValidMembers.Add(State);
        else ++Invalid;
    }
    ValidMembers.Sort([](const UAnimStateNodeBase& A,const UAnimStateNodeBase& B){return A.GetPathName()<B.GetPathName();});
    for(const UAnimStateNodeBase* State:ValidMembers) Members.Add(MakeShared<FJsonValueObject>(Identity(State)));
    R->SetArrayField(TEXT("explicit_members"),Members);
    R->SetNumberField(TEXT("invalid_explicit_member_count"),Invalid);
    R->SetStringField(TEXT("global_scope_note"),TEXT("When global_alias is true the UE compiler maps all baked states; explicit_members alone is not its effective scope."));
    return R;
}
}
#endif

FString UHCM5VS2GASTransitionEditor::ReadTransitionGraph(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("status"),TEXT("FAIL"));
    R->SetBoolField(TEXT("read_only"),true); R->SetBoolField(TEXT("compiled"),false); R->SetBoolField(TEXT("saved"),false);
    if(!Blueprint || Blueprint->GetPathName()!=BlueprintPath || GetPathNameSafe(Blueprint->TargetSkeleton.Get())!=SkeletonPath)
        return Finish(R,TEXT("Exact current VS2 AnimBlueprint and original Selestia skeleton required"));
    if(GetPathNameSafe(Blueprint->ParentClass.Get())!=TEXT("/Script/HarborCity.HCM5VS2LookAnimInstance"))
        return Finish(R,TEXT("Unexpected VS2 animation parent class"));
    const bool bDirty=Blueprint->GetOutermost()->IsDirty();
    R->SetStringField(TEXT("blueprint"),Blueprint->GetPathName());
    R->SetStringField(TEXT("parent_class"),GetPathNameSafe(Blueprint->ParentClass.Get()));
    R->SetStringField(TEXT("skeleton"),GetPathNameSafe(Blueprint->TargetSkeleton.Get()));
    R->SetBoolField(TEXT("package_dirty_before"),bDirty);
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs); Graphs.Remove(nullptr);
    if(Graphs.Num()>64) return Finish(R,TEXT("Graph count exceeds bounded read"));
    Graphs.Sort([](const UEdGraph& A,const UEdGraph& B){return A.GetPathName()<B.GetPathName();});
    TArray<const UAnimGraphNode_StateMachineBase*> Machines;
    for(UEdGraph* Graph:Graphs) if(Graph->GetName()==TEXT("AnimGraph"))
        for(const UEdGraphNode* Node:Graph->Nodes)
            if(const auto* Machine=Cast<UAnimGraphNode_StateMachineBase>(Node)) Machines.Add(Machine);
    if(Machines.Num()!=2) return Finish(R,TEXT("Expected exactly the two existing top-level state machines"));
    Machines.Sort([](const UAnimGraphNode_StateMachineBase& A,const UAnimGraphNode_StateMachineBase& B){return A.GetPathName()<B.GetPathName();});
    FRows MachineRows; int32 TotalNodes=0;
    for(const UAnimGraphNode_StateMachineBase* Machine:Machines)
    {
        const UAnimationStateMachineGraph* MachineGraph=Machine->EditorStateMachineGraph;
        if(!MachineGraph || (MachineGraph->GetName()!=TEXT("Locomotion") && MachineGraph->GetName()!=TEXT("Main States")))
            return Finish(R,TEXT("Only exact current Locomotion and Main States graphs are supported"));
        auto M=Identity(Machine); M->SetStringField(TEXT("name"),MachineGraph->GetName());
        M->SetStringField(TEXT("graph"),MachineGraph->GetPathName());
        auto Settings=MakeShared<FJsonObject>(); ExportProperty(Machine,TEXT("Node"),Settings); M->SetObjectField(TEXT("settings"),Settings);
        FRows StateRows,TransitionRows,AliasRows,GraphRows;
        for(const UEdGraph* Graph:Graphs)
        {
            if(Graph!=MachineGraph && !Graph->IsIn(MachineGraph)) continue;
            TArray<const UEdGraphNode*> Nodes;
            for(const UEdGraphNode* Node:Graph->Nodes) if(Node) Nodes.Add(Node);
            TotalNodes+=Nodes.Num(); if(TotalNodes>1024) return Finish(R,TEXT("Node count exceeds bounded read"));
            Nodes.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.GetPathName()<B.GetPathName();});
            FRows NodeRows;
            for(const UEdGraphNode* Node:Nodes)
            {
                if(Node->Pins.Num()>128) return Finish(R,TEXT("Node pin count exceeds bounded read"));
                NodeRows.Add(MakeShared<FJsonValueObject>(NodeReadback(Node)));
                if(Graph!=MachineGraph) continue;
                if(const auto* State=Cast<UAnimStateNode>(Node)) StateRows.Add(MakeShared<FJsonValueObject>(StateReadback(State)));
                if(const auto* Alias=Cast<UAnimStateAliasNode>(Node)) AliasRows.Add(MakeShared<FJsonValueObject>(AliasReadback(Alias)));
                if(const auto* Transition=Cast<UAnimStateTransitionNode>(Node))
                {
                    if(Transition->Pins.Num()<2 || !Transition->Pins[0] || !Transition->Pins[1])
                        return Finish(R,TEXT("Malformed transition pins; no unsafe endpoint read"));
                    TransitionRows.Add(MakeShared<FJsonValueObject>(TransitionReadback(Transition)));
                }
            }
            auto G=Identity(Graph); G->SetArrayField(TEXT("nodes"),NodeRows); G->SetNumberField(TEXT("node_count"),Nodes.Num());
            GraphRows.Add(MakeShared<FJsonValueObject>(G));
        }
        M->SetArrayField(TEXT("states"),StateRows); M->SetArrayField(TEXT("transitions"),TransitionRows);
        M->SetArrayField(TEXT("aliases"),AliasRows); M->SetArrayField(TEXT("graphs"),GraphRows);
        MachineRows.Add(MakeShared<FJsonValueObject>(M));
    }
    R->SetArrayField(TEXT("state_machines"),MachineRows); R->SetNumberField(TEXT("total_inspected_nodes"),TotalNodes);
    const bool bAfter=Blueprint->GetOutermost()->IsDirty();
    R->SetBoolField(TEXT("package_dirty_after"),bAfter); R->SetBoolField(TEXT("package_dirty_state_unchanged"),bAfter==bDirty);
    if(bAfter!=bDirty) return Finish(R,TEXT("Unexpected package dirty-state change during readback"));
    R->SetStringField(TEXT("scope"),TEXT("Authored graph and metadata readback only; no compiled runtime-state or naturalness claim."));
    R->SetStringField(TEXT("status"),TEXT("READBACK_COMPLETE")); return Finish(R);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
