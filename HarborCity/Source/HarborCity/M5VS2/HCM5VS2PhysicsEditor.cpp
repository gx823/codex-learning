#include "HCM5VS2PhysicsEditor.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "JsonObjectConverter.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace
{
using FRows = TArray<TSharedPtr<FJsonValue>>;

FString Finish(const TSharedPtr<FJsonObject>& Report, const FString& Error = FString())
{
    if (!Error.IsEmpty())
    {
        Report->SetStringField(TEXT("status"), TEXT("FAIL"));
        Report->SetStringField(TEXT("error"), Error);
    }
    FString Result;
    FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}

FRows XYZ(const FVector& V)
{
    return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y),
        MakeShared<FJsonValueNumber>(V.Z)};
}

TSharedPtr<FJsonObject> TransformRow(const FTransform& T)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetArrayField(TEXT("translation_cm"), XYZ(T.GetTranslation()));
    Row->SetArrayField(TEXT("scale"), XYZ(T.GetScale3D()));
    const FQuat Q = T.GetRotation();
    Row->SetArrayField(TEXT("quaternion_xyzw"), {MakeShared<FJsonValueNumber>(Q.X),
        MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z),
        MakeShared<FJsonValueNumber>(Q.W)});
    return Row;
}

bool InReadScope(const UObject* Object, const TCHAR* BaselinePath)
{
    return Object && (Object->GetPathName() == BaselinePath
        || Object->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/")));
}

// The plugin's source has been checked, but no Kawaii C++ type is linked here.
// Reflection lets the probe report the exact installed schema without changing
// Build.cs. Absent classes are reported, not synthesized or silently loaded.
TSharedPtr<FJsonObject> ReadFields(const UStruct* Struct, const void* Data,
    const TArray<FName>& Names)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("struct"), Struct ? Struct->GetPathName() : TEXT("NOT_LOADED"));
    FRows Fields;
    for (FName Name : Names)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Name.ToString());
        const FProperty* Property = Struct ? FindFProperty<FProperty>(Struct, Name) : nullptr;
        Row->SetBoolField(TEXT("present"), Property != nullptr);
        if (Property)
        {
            Row->SetStringField(TEXT("cpp_type"), Property->GetCPPType());
            if (Data)
            {
                FString Value;
                Property->ExportText_InContainer(0, Value, Data, nullptr, nullptr, PPF_None);
                Row->SetBoolField(TEXT("value_truncated"), Value.Len() > 8192);
                Row->SetStringField(TEXT("value"), Value.Left(8192));
            }
        }
        Fields.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("fields"), Fields);
    return Result;
}

const TArray<FName>& NodeFields()
{
    static const TArray<FName> Fields = {TEXT("RootBone"), TEXT("AdditionalRootBones"),
        TEXT("ExcludeBones"), TEXT("PhysicsSettings"), TEXT("SimulationSpace"),
        TEXT("SimulationBaseBone"), TEXT("BoneForwardAxis"), TEXT("DummyBoneLength"),
        TEXT("TargetFramerate"), TEXT("BoneSubdivisionCount"), TEXT("TeleportDistanceThreshold"),
        TEXT("TeleportRotationThreshold"), TEXT("Gravity"), TEXT("bUseLegacyGravity"),
        TEXT("bUseDefaultGravityZProjectSetting"), TEXT("bUseWorldSpaceGravity"),
        TEXT("bEnableWind"), TEXT("bAllowWorldCollision"), TEXT("bUseSharedCollision"),
        TEXT("SphericalLimits"), TEXT("CapsuleLimits"), TEXT("PhysicsAssetForLimits"),
        TEXT("LimitsDataAsset"), TEXT("DampingCurveData"), TEXT("StiffnessCurveData"),
        TEXT("RadiusCurveData"), TEXT("LimitAngleCurveData"), TEXT("Alpha"), TEXT("LODThreshold")};
    return Fields;
}

TSharedPtr<FJsonObject> MapCandidate(const FString& Source, const FReferenceSkeleton& Ref,
    int32& OutIndex)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("source_bone"), Source);
    OutIndex = Ref.FindBoneIndex(FName(*Source));
    FString State = TEXT("EXACT_NAME");
    if (OutIndex == INDEX_NONE)
    {
        // FBX import can sanitize '.', but only the actual imported name is evidence.
        const FString Candidate = Source.Replace(TEXT("."), TEXT("_"));
        OutIndex = Ref.FindBoneIndex(FName(*Candidate));
        State = OutIndex == INDEX_NONE ? TEXT("UNRESOLVED") : TEXT("PUNCTUATION_CANDIDATE_ONLY");
    }
    Row->SetStringField(TEXT("match"), State);
    Row->SetNumberField(TEXT("ue_index"), OutIndex);
    if (OutIndex != INDEX_NONE)
    {
        Row->SetStringField(TEXT("ue_bone"), Ref.GetBoneName(OutIndex).ToString());
        Row->SetNumberField(TEXT("ue_parent_index"), Ref.GetParentIndex(OutIndex));
    }
    return Row;
}

bool InSecondaryScope(const FString& Root)
{
    return Root.StartsWith(TEXT("Hair_")) || Root.StartsWith(TEXT("Skirt_"))
        || Root.StartsWith(TEXT("Chest_ribbon_")) || Root.StartsWith(TEXT("LowerLeg_ribbon_"));
}

const TCHAR* PhysicsBlueprintPath = TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics.ABP_M5VS2_Selestia_Physics");

UEdGraphPin* PhysicsOutput(UEdGraphNode* Node)
{
    if (Node) for (UEdGraphPin* Pin : Node->Pins) if (Pin->Direction == EGPD_Output) return Pin;
    return nullptr;
}

// Validate every provided key, including nested structs/array elements. Missing
// fields deliberately keep native defaults; JsonConverter strict mode requires
// all fields, including transient implementation fields, and is unsuitable here.
bool CheckJsonProperty(FProperty* Property, const TSharedPtr<FJsonValue>& Value, FString& Error);
bool CheckJsonStruct(const UStruct* Struct, const TSharedPtr<FJsonObject>& Object, FString& Error)
{
    if (!Struct || !Object) return false;
    for (const auto& Pair : Object->Values)
    {
        const FString Key(*Pair.Key);
        FProperty* P = FindFProperty<FProperty>(Struct, FName(*Key));
        if (!P) { Error = TEXT("Unknown reflected property: ") + Key; return false; }
        if (!CheckJsonProperty(P, Pair.Value, Error)) return false;
    }
    return true;
}
bool CheckJsonProperty(FProperty* Property, const TSharedPtr<FJsonValue>& Value, FString& Error)
{
    if (!Value) { Error = TEXT("Null configuration value"); return false; }
    if (auto* S = CastField<FStructProperty>(Property); S && Value->Type == EJson::Object)
        return CheckJsonStruct(S->Struct, Value->AsObject(), Error);
    if (auto* A = CastField<FArrayProperty>(Property); A && Value->Type == EJson::Array)
        for (const auto& V : Value->AsArray()) if (!CheckJsonProperty(A->Inner, V, Error)) return false;
    return true;
}

bool JsonVector(const TSharedPtr<FJsonObject>& Row, const TCHAR* Name, FVector& Out)
{
    const FRows* A = nullptr;
    if (!Row->TryGetArrayField(Name, A) || A->Num() != 3) return false;
    for (const auto& V : *A) if (!V || V->Type != EJson::Number || !FMath::IsFinite(V->AsNumber())) return false;
    Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
    return true;
}

TSharedPtr<FJsonObject> PhysicsReadback(UAnimBlueprint* Blueprint)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("status"), TEXT("FAIL"));
    if (!Blueprint || Blueprint->GetPathName() != PhysicsBlueprintPath) return Report;
    FRows Rows;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs) for (UEdGraphNode* N : Graph->Nodes)
    {
        if (!N->GetName().StartsWith(TEXT("M5VS2_Physics_"))) continue;
        auto* P = FindFProperty<FStructProperty>(N->GetClass(), TEXT("Node"));
        if (!P) continue;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), N->GetName());
        Row->SetStringField(TEXT("class"), N->GetClass()->GetPathName());
        const void* Data = P->ContainerPtrToValuePtr<void>(N);
        auto Values = MakeShared<FJsonObject>();
        TArray<FName> Fields = NodeFields();
        Fields.Append({TEXT("bBoneSubdivisionCollisionOnly"), TEXT("BoneSubdivisionCount"),
            TEXT("TeleportDistanceThreshold"), TEXT("TeleportRotationThreshold")});
        for (FName Name : Fields)
            if (FProperty* Field = FindFProperty<FProperty>(P->Struct, Name))
                Values->SetField(Name.ToString(), FJsonObjectConverter::UPropertyToJsonValue(Field,
                    Field->ContainerPtrToValuePtr<void>(Data)));
        Row->SetObjectField(TEXT("node_properties"), Values);
        FRows CollisionAxes;
        for (FName LimitName : {FName(TEXT("SphericalLimits")), FName(TEXT("CapsuleLimits"))})
        {
            auto* Array = FindFProperty<FArrayProperty>(P->Struct, LimitName);
            auto* Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
            auto* Rot = Inner ? FindFProperty<FStructProperty>(Inner->Struct, TEXT("OffsetRotation")) : nullptr;
            if (!Array || !Inner || !Rot || Rot->Struct != TBaseStructure<FRotator>::Get()) continue;
            FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Data));
            for (int32 I = 0; I < Helper.Num(); ++I)
            {
                const FRotator& Value = *Rot->ContainerPtrToValuePtr<FRotator>(Helper.GetRawPtr(I));
                auto Axis = MakeShared<FJsonObject>(); Axis->SetStringField(TEXT("array"), LimitName.ToString());
                Axis->SetNumberField(TEXT("index"), I); Axis->SetArrayField(TEXT("axis_local"), XYZ(Value.Quaternion().GetAxisZ()));
                CollisionAxes.Add(MakeShared<FJsonValueObject>(Axis));
            }
        }
        Row->SetArrayField(TEXT("collision_axes_local"), CollisionAxes);
        FRows Links;
        if (auto* Input = N->FindPin(TEXT("ComponentPose"))) for (auto* Link : Input->LinkedTo)
            Links.Add(MakeShared<FJsonValueString>(Link->GetOwningNode()->GetName()));
        Row->SetArrayField(TEXT("input_nodes"), Links);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    Report->SetArrayField(TEXT("nodes"), Rows);
    Report->SetNumberField(TEXT("node_count"), Rows.Num());
    Report->SetStringField(TEXT("runtime_swing_collision_visual"), TEXT("NOT_RUN"));
    Report->SetStringField(TEXT("status"), Rows.Num() == 11 ? TEXT("READBACK_COMPLETE") : TEXT("FAIL"));
    return Report;
}
}
#endif

FString UHCM5VS2PhysicsEditor::ReadPhysicsSetup(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
    return Finish(PhysicsReadback(Blueprint));
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

#if WITH_EDITOR
namespace
{
const FString LocomotionSource=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/Animation/BS_Idle_Walk_Run_Selestia");
const FString LocomotionTarget=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun");
const TCHAR* OldLocomotionNames[]={TEXT("MM_Idle_Selestia"),TEXT("MF_Unarmed_Walk_Fwd_Selestia"),TEXT("MF_Unarmed_Jog_Fwd_Selestia")};
const TCHAR* NewLocomotionNames[]={TEXT("M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS"),TEXT("M_Relaxed_Walk_Loop_F_InPlace_SelestiaGAS"),TEXT("M_Relaxed_Run_Loop_F_InPlace_SelestiaGAS")};
FString AssetPackage(const UObject* Object) { return Object?Object->GetOutermost()->GetName():FString(); }

// Protected native fields remain visible to FProperty. Do not use Python's
// editor-property table to traverse UBlueprint or BlendSpace fixed arrays.
TSharedPtr<FJsonObject> LocomotionFields(const UStruct* Struct, const void* Data, bool bBlendSpaceOnly=false)
{
    auto O=MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Struct);It;++It)
    {
        const FProperty* P=*It;
        if (P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient)) continue;
        if (bBlendSpaceOnly && (!P->GetOwnerStruct()->IsChildOf(UBlendSpace::StaticClass())
            || !P->HasAnyPropertyFlags(CPF_Edit) || P->GetFName()==TEXT("SampleData"))) continue;
        FRows Values;
        for (int32 I=0;I<P->ArrayDim;++I)
        { FString Value; P->ExportText_InContainer(I,Value,Data,nullptr,nullptr,PPF_None); Values.Add(MakeShared<FJsonValueString>(Value)); }
        O->SetArrayField(P->GetName(),Values);
    }
    return O;
}
FRows LocomotionInputPins(const UEdGraphNode* Node)
{
    FRows Names;
    for (const UEdGraphPin* Pin:Node->Pins)
        if (Pin && Pin->Direction==EGPD_Input) Names.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));
    return Names;
}
bool HasBlendSpaceInput(const UEdGraphNode* Node)
{
    for (const UEdGraphPin* Pin:Node->Pins)
        if (Pin && Pin->Direction==EGPD_Input && Pin->PinName==TEXT("BlendSpace")) return true;
    return false;
}
TSharedPtr<FJsonObject> LocomotionGraph(UAnimBlueprint* Blueprint)
{
    auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("status"),TEXT("FAIL"));
    if (!Blueprint || Blueprint->GetPathName()!=PhysicsBlueprintPath)
    { O->SetStringField(TEXT("error"),TEXT("Exact VS2 physics AnimBlueprint required")); return O; }
    const bool bDirty=Blueprint->GetOutermost()->IsDirty();
    O->SetStringField(TEXT("path"),Blueprint->GetPathName());
    O->SetStringField(TEXT("parent_class"),GetPathNameSafe(Blueprint->ParentClass.Get()));
    O->SetStringField(TEXT("target_skeleton"),GetPathNameSafe(Blueprint->TargetSkeleton.Get()));
    FRows GraphRows, Players, Sequences; TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs); Graphs.Remove(nullptr);
    if (Graphs.Num()>100) { O->SetStringField(TEXT("error"),TEXT("More than 100 native graphs")); return O; }
    Graphs.Sort([](const UEdGraph& A,const UEdGraph& B) { return A.GetPathName()<B.GetPathName(); });
    TSet<const UEdGraph*> Seen; int32 TotalNodes=0;
    for (UEdGraph* Graph:Graphs)
    {
        if (!Graph || Seen.Contains(Graph)) continue; Seen.Add(Graph);
        TArray<UEdGraphNode*> Nodes; for (UEdGraphNode* Node:Graph->Nodes) if (Node) Nodes.Add(Node);
        TotalNodes+=Nodes.Num(); if (TotalNodes>4096) { O->SetStringField(TEXT("error"),TEXT("More than 4096 native nodes")); return O; }
        Nodes.Sort([](const UEdGraphNode& A,const UEdGraphNode& B) { return A.GetPathName()<B.GetPathName(); });
        FRows NodeRows;
        for (UEdGraphNode* Node:Nodes)
        {
            auto N=MakeShared<FJsonObject>(); N->SetStringField(TEXT("path"),Node->GetPathName());
            N->SetStringField(TEXT("class_name"),Node->GetClass()->GetName());
            N->SetArrayField(TEXT("position"),{MakeShared<FJsonValueNumber>(Node->GetNodePosX()),MakeShared<FJsonValueNumber>(Node->GetNodePosY())});
            FString Runtime;
            if (const FStructProperty* P=FindFProperty<FStructProperty>(Node->GetClass(),TEXT("Node")))
            {
                P->ExportText_InContainer(0,Runtime,Node,nullptr,nullptr,PPF_None);
                N->SetStringField(TEXT("runtime_node"),Runtime);
            }
            FRows Pins;
            for (const UEdGraphPin* Pin:Node->Pins)
            {
                if (!Pin) continue;
                auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"),Pin->PinName.ToString());
                P->SetNumberField(TEXT("direction"),int32(Pin->Direction)); P->SetBoolField(TEXT("hidden"),Pin->bHidden);
                P->SetStringField(TEXT("default_value"),Pin->DefaultValue); P->SetStringField(TEXT("default_object"),GetPathNameSafe(Pin->DefaultObject));
                P->SetStringField(TEXT("default_text"),Pin->DefaultTextValue.ToString());
                FString PinType; FEdGraphPinType::StaticStruct()->ExportText(PinType,&Pin->PinType,nullptr,nullptr,PPF_None,nullptr);
                P->SetStringField(TEXT("type"),PinType); TArray<FString> Links;
                for (const UEdGraphPin* Link:Pin->LinkedTo) if (Link) Links.Add(Link->GetOwningNode()->GetPathName()+TEXT(":")+Link->PinName.ToString());
                Links.Sort(); FRows Linked; for (const FString& Link:Links) Linked.Add(MakeShared<FJsonValueString>(Link));
                P->SetArrayField(TEXT("links"),Linked); Pins.Add(MakeShared<FJsonValueObject>(P));
            }
            N->SetArrayField(TEXT("pins"),Pins);
            if (const auto* Player=Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
            {
                auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("path"),Node->GetPathName()); P->SetStringField(TEXT("graph"),Graph->GetPathName());
                P->SetStringField(TEXT("blendspace"),Player->Node.GetBlendSpace()?Player->Node.GetBlendSpace()->GetPathName():FString());
                P->SetArrayField(TEXT("exposed_property_pins"),LocomotionInputPins(Node)); P->SetStringField(TEXT("runtime_node"),Runtime);
                Players.Add(MakeShared<FJsonValueObject>(P));
            }
            if (const auto* Player=Cast<UAnimGraphNode_SequencePlayer>(Node))
            {
                auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("path"),Node->GetPathName()); P->SetStringField(TEXT("graph"),Graph->GetPathName());
                P->SetStringField(TEXT("sequence"),Player->Node.GetSequence()?Player->Node.GetSequence()->GetPathName():FString());
                P->SetStringField(TEXT("runtime_node"),Runtime); Sequences.Add(MakeShared<FJsonValueObject>(P));
            }
            NodeRows.Add(MakeShared<FJsonValueObject>(N));
        }
        auto G=MakeShared<FJsonObject>(); G->SetStringField(TEXT("path"),Graph->GetPathName()); G->SetNumberField(TEXT("node_count"),Nodes.Num());
        G->SetArrayField(TEXT("nodes"),NodeRows); GraphRows.Add(MakeShared<FJsonValueObject>(G));
    }
    O->SetArrayField(TEXT("graphs"),GraphRows); O->SetArrayField(TEXT("blendspace_players"),Players); O->SetArrayField(TEXT("sequence_players"),Sequences);
    O->SetObjectField(TEXT("physics"),PhysicsReadback(Blueprint));
    const bool bUnchanged=Blueprint->GetOutermost()->IsDirty()==bDirty;
    O->SetBoolField(TEXT("package_dirty_state_unchanged"),bUnchanged);
    O->SetStringField(TEXT("status"),bUnchanged?TEXT("READBACK_COMPLETE"):TEXT("FAIL")); return O;
}
}
#endif

FString UHCM5VS2PhysicsEditor::ReadLocomotionGraph(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
    return Finish(LocomotionGraph(Blueprint));
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2PhysicsEditor::ReadLocomotionBlendSpace(UBlendSpace* BlendSpace)
{
#if WITH_EDITOR
    auto O=MakeShared<FJsonObject>();
    if (!BlendSpace || (AssetPackage(BlendSpace)!=LocomotionSource && AssetPackage(BlendSpace)!=LocomotionTarget))
        return Finish(O,TEXT("Exact original or isolated VS2 locomotion BlendSpace required"));
    const bool bDirty=BlendSpace->GetOutermost()->IsDirty();
    const auto& Samples=BlendSpace->GetBlendSamples();
    if (Samples.Num()<3 || Samples.Num()>128) return Finish(O,TEXT("BlendSpace sample count outside 3..128"));
    O->SetStringField(TEXT("path"),BlendSpace->GetPathName()); O->SetStringField(TEXT("class_name"),BlendSpace->GetClass()->GetName());
    O->SetStringField(TEXT("skeleton"),GetPathNameSafe(BlendSpace->GetSkeleton())); FRows Rows;
    for (int32 I=0;I<Samples.Num();++I)
    {
        const auto& Sample=Samples[I]; auto S=MakeShared<FJsonObject>();
        S->SetNumberField(TEXT("index"),I); S->SetStringField(TEXT("animation"),Sample.Animation?Sample.Animation->GetPathName():FString());
        S->SetArrayField(TEXT("position"),XYZ(Sample.SampleValue)); S->SetNumberField(TEXT("rate_scale"),Sample.RateScale);
        S->SetObjectField(TEXT("reflected_sample"),LocomotionFields(FBlendSample::StaticStruct(),&Sample));
        Rows.Add(MakeShared<FJsonValueObject>(S));
    }
    O->SetArrayField(TEXT("samples"),Rows); FRows Axes, Interpolation;
    for (int32 I=0;I<3;++I)
    {
        Axes.Add(MakeShared<FJsonValueObject>(LocomotionFields(FBlendParameter::StaticStruct(),&BlendSpace->GetBlendParameter(I))));
        Interpolation.Add(MakeShared<FJsonValueObject>(LocomotionFields(FInterpolationParameter::StaticStruct(),&BlendSpace->InterpolationParam[I])));
    }
    O->SetArrayField(TEXT("axes"),Axes); O->SetArrayField(TEXT("input_interpolation"),Interpolation);
    O->SetNumberField(TEXT("target_weight_interpolation_speed_per_sec"),BlendSpace->TargetWeightInterpolationSpeedPerSec);
    O->SetObjectField(TEXT("authored_blendspace_properties"),LocomotionFields(BlendSpace->GetClass(),BlendSpace,true));
    O->SetStringField(TEXT("sync_markers_scope"),TEXT("Native ValidateSampleData recomputes derived length/marker caches on Apply; authored coordinates, rates, mirror/frame flags, axes and settings must remain unchanged. Rendered transitions NOT_RUN."));
    const bool bUnchanged=BlendSpace->GetOutermost()->IsDirty()==bDirty;
    O->SetBoolField(TEXT("package_dirty_state_unchanged"),bUnchanged);
    O->SetStringField(TEXT("status"),bUnchanged?TEXT("READBACK_COMPLETE"):TEXT("FAIL")); return Finish(O);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2PhysicsEditor::ApplyLocomotionSamples(UAnimBlueprint* Blueprint,UBlendSpace* Source,UBlendSpace* Target,const FString& ChangesJson)
{
#if WITH_EDITOR
    auto O=MakeShared<FJsonObject>(); O->SetBoolField(TEXT("saved_by_helper"),false);
    if (!Blueprint || Blueprint->GetPathName()!=PhysicsBlueprintPath || !Source || !Target || Source==Target
        || AssetPackage(Source)!=LocomotionSource || AssetPackage(Target)!=LocomotionTarget || Source->GetClass()!=Target->GetClass()
        || !Source->GetSkeleton() || Source->GetSkeleton()!=Target->GetSkeleton() || Blueprint->TargetSkeleton!=Source->GetSkeleton()
        || Source->GetSkeleton()->GetPathName()!=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SK_Selestia.SK_Selestia"))
        return Finish(O,TEXT("Exact VS2 AnimBP, independent source/candidate BlendSpaces and original Selestia skeleton required"));
    if (ChangesJson.Len()>16384) return Finish(O,TEXT("Oversized change list"));
    FRows Changes;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ChangesJson),Changes) || Changes.Num()!=3)
        return Finish(O,TEXT("Exactly three explicit sample changes required"));
    const auto Before=Source->GetBlendSamples();
    if (Before.Num()<3 || Before.Num()>128 || Target->GetBlendSamples().Num()!=Before.Num()) return Finish(O,TEXT("Candidate is not the intact source copy"));
    for (int32 I=0;I<Before.Num();++I) if (!(Before[I]==Target->GetBlendSamples()[I])) return Finish(O,TEXT("Candidate sample differs before Apply"));
    TArray<int32> Indices; TArray<UAnimSequence*> Clips; TSet<int32> Roles;
    for (const auto& Value:Changes)
    {
        const auto Row=Value && Value->Type==EJson::Object?Value->AsObject():nullptr;
        int32 Index=INDEX_NONE; FString OldPath, NewPath; FVector Position; double Rate=0;
        if (!Row || !Row->TryGetNumberField(TEXT("index"),Index) || !Before.IsValidIndex(Index) || Indices.Contains(Index)
            || !Row->TryGetStringField(TEXT("before"),OldPath) || !Row->TryGetStringField(TEXT("after"),NewPath)
            || AssetPackage(Before[Index].Animation.Get())!=OldPath || !JsonVector(Row,TEXT("unchanged_position"),Position)
            || !Before[Index].SampleValue.Equals(Position,.000001) || !Row->TryGetNumberField(TEXT("unchanged_rate_scale"),Rate)
            || !FMath::IsFinite(Rate) || !FMath::IsNearlyEqual(double(Before[Index].RateScale),Rate,.000001))
            return Finish(O,TEXT("Sample index/source/coordinates/rate mismatch"));
        int32 Role=INDEX_NONE;
        for (int32 I=0;I<3;++I)
            if (OldPath==FString(TEXT("/Game/HarborCity/M5VS1/HeroSelestia/Animation/"))+OldLocomotionNames[I]
                && NewPath==FString(TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/"))+NewLocomotionNames[I]) Role=I;
        if (Role==INDEX_NONE || Roles.Contains(Role)) return Finish(O,TEXT("Only exact idle/forward-walk/forward-jog replacements are allowed"));
        UAnimSequence* Clip=LoadObject<UAnimSequence>(nullptr,*(NewPath+TEXT(".")+NewLocomotionNames[Role]));
        if (!Clip || Clip->GetSkeleton()!=Source->GetSkeleton() || Clip->HasRootMotion() || Clip->bForceRootLock || !Target->ValidateAnimationSequence(Clip))
            return Finish(O,TEXT("GAS clip skeleton/root-motion/additive validation failed"));
        int32 Matches=0; for (const auto& Sample:Before) if (AssetPackage(Sample.Animation.Get())==OldPath) ++Matches;
        if (Matches!=1) return Finish(O,TEXT("Each replaced source clip must occur in exactly one sample"));
        Roles.Add(Role); Indices.Add(Index); Clips.Add(Clip);
    }
    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs); TSet<UAnimGraphNode_BlendSpacePlayer*> Players;
    for (UEdGraph* Graph:Graphs) if (Graph) for (UEdGraphNode* Node:Graph->Nodes)
        if (auto* Player=Cast<UAnimGraphNode_BlendSpacePlayer>(Node);Player && Player->Node.GetBlendSpace()==Source) Players.Add(Player);
    if (Players.Num()!=1) return Finish(O,TEXT("Exactly one original locomotion BlendSpace player required"));
    UAnimGraphNode_BlendSpacePlayer* Player=*Players.CreateConstIterator();
    if (HasBlendSpaceInput(Player)) return Finish(O,TEXT("BlendSpace graph pin exists; refuse to alter pin wiring or defaults"));
    const bool bSourceDirty=Source->GetOutermost()->IsDirty();
    Target->Modify();
    for (int32 I=0;I<3;++I) if (!Target->ReplaceSampleAnimation(Indices[I],Clips[I])) return Finish(O,TEXT("Native sample replacement failed; do not save"));
    Target->ValidateSampleData(); // Derived marker/length caches must describe the new animations.
    auto Expected=Before; for (int32 I=0;I<3;++I) Expected[Indices[I]].Animation=Clips[I];
    if (Target->GetBlendSamples().Num()!=Expected.Num()) return Finish(O,TEXT("Validation changed sample count; do not save"));
    for (int32 I=0;I<Expected.Num();++I)
        if (!(Target->GetBlendSamples()[I]==Expected[I]) || !Target->GetBlendSamples()[I].bIsValid)
            return Finish(O,TEXT("Validation changed authored sample data or produced invalid sample; do not save"));
    Blueprint->Modify(); Player->Modify();
    if (!Player->Node.SetBlendSpace(Target)) return Finish(O,TEXT("Native BlendSpace player assignment failed; do not save"));
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint); FKismetEditorUtilities::CompileBlueprint(Blueprint);
    O->SetNumberField(TEXT("blueprint_compile_status"),int32(Blueprint->Status));
    if (Blueprint->Status!=BS_UpToDate && Blueprint->Status!=BS_UpToDateWithWarnings)
        return Finish(O,TEXT("VS2 AnimBP compile did not become up to date; do not save"));
    if (Source->GetOutermost()->IsDirty()!=bSourceDirty) return Finish(O,TEXT("Source dirty flag changed unexpectedly; do not save"));
    Target->MarkPackageDirty(); Blueprint->MarkPackageDirty();
    O->SetStringField(TEXT("node"),Player->GetPathName()); O->SetNumberField(TEXT("changed_samples"),3);
    O->SetStringField(TEXT("derived_cache_policy"),TEXT("Native ValidateSampleData recomputed sample validity, duration and marker caches; no rate/axis/directional/graph wiring edits"));
    O->SetStringField(TEXT("status"),TEXT("APPLIED_COMPILED_NOT_SAVED")); return Finish(O);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2PhysicsEditor::ApplyPhysicsSetup(USkeletalMesh* Target, UAnimBlueprint* Blueprint,
    const FString& ConfigJson)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("status"), TEXT("FAIL"));
    Report->SetStringField(TEXT("runtime_swing_collision_visual"), TEXT("NOT_RUN"));
    if (!Target || !Blueprint || Blueprint->GetPathName() != PhysicsBlueprintPath
        || Target->GetPathName() != TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia.SKM_Selestia")
        || Blueprint->TargetSkeleton != Target->GetSkeleton())
        return Finish(Report, TEXT("Exact isolated VS2 ABP and original Selestia reference mesh required"));
    if (ConfigJson.Len() > 2 * 1024 * 1024) return Finish(Report, TEXT("Oversized physics config"));
    TSharedPtr<FJsonObject> Config;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConfigJson), Config) || !Config)
        return Finish(Report, TEXT("Invalid physics config"));
    if (Config->GetIntegerField(TEXT("schema")) != 1 || Config->GetIntegerField(TEXT("root_count")) != 42)
        return Finish(Report, TEXT("Expected reviewed schema 1 and 42 roots"));
    UClass* KawaiiClass = FindObject<UClass>(nullptr, TEXT("/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics"));
    FStructProperty* NodeProperty = KawaiiClass ? FindFProperty<FStructProperty>(KawaiiClass, TEXT("Node")) : nullptr;
    if (!KawaiiClass || !KawaiiClass->IsChildOf(UEdGraphNode::StaticClass()) || !NodeProperty)
        return Finish(Report, TEXT("Installed Kawaii editor node unavailable"));
    const FReferenceSkeleton& Ref = Target->GetRefSkeleton();
    TArray<FTransform> ComponentPose;
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        const int32 Parent = Ref.GetParentIndex(I);
        ComponentPose.Add(Parent == INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * ComponentPose[Parent]);
    }
    const FRows* Expected = nullptr;
    if (!Config->TryGetArrayField(TEXT("expected_reference_bones"), Expected) || Expected->Num() < 100)
        return Finish(Report, TEXT("Reference-pose gate missing"));
    TSet<FName> AllowedBones;
    double MaxReferenceError = 0;
    for (const auto& V : *Expected)
    {
        const auto Row = V->AsObject(); const FName Name(*Row->GetStringField(TEXT("name")));
        const int32 I = Ref.FindBoneIndex(Name); FVector P, Scale;
        if (I == INDEX_NONE || !JsonVector(Row, TEXT("translation_cm"), P) || !JsonVector(Row, TEXT("scale"), Scale)
            || Ref.GetParentIndex(I) != Row->GetIntegerField(TEXT("parent_index")))
            return Finish(Report, TEXT("Reference name/parent gate failed"));
        const double Error = FVector::Distance(ComponentPose[I].GetTranslation(), P);
        MaxReferenceError = FMath::Max(MaxReferenceError, Error);
        if (Error > .01 || !ComponentPose[I].GetScale3D().Equals(Scale, 1.e-5))
            return Finish(Report, TEXT("Reference pose changed since measured Unity conversion"));
        const FRows* Q = nullptr;
        if (!Row->TryGetArrayField(TEXT("quaternion_xyzw"), Q) || Q->Num() != 4
            || ComponentPose[I].GetRotation().AngularDistance(FQuat((*Q)[0]->AsNumber(), (*Q)[1]->AsNumber(), (*Q)[2]->AsNumber(), (*Q)[3]->AsNumber())) > 1.e-4)
            return Finish(Report, TEXT("Reference rotation changed since measured collider conversion"));
        AllowedBones.Add(Name);
    }
    UAnimGraphNode_Slot* Full = nullptr;
    TArray<UAnimGraphNode_Slot*> Slots; FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Slots);
    for (auto* Slot : Slots) if (Slot->GetFName() == TEXT("M4_FullBody")) Full = Slot;
    UEdGraph* Graph = Full ? Full->GetGraph() : nullptr;
    UEdGraphPin* Prior = PhysicsOutput(Full);
    if (!Graph || !Prior || Prior->LinkedTo.Num() != 1 || Prior->LinkedTo[0]->PinName != TEXT("Result"))
        return Finish(Report, TEXT("Expected intact M4_FullBody to Result connection; refusing repeat or other graph"));
    for (UEdGraphNode* N : Graph->Nodes) if (N->GetName().StartsWith(TEXT("M5VS2_")) || N->GetClass() == KawaiiClass)
        return Finish(Report, TEXT("Target already has VS2 or Kawaii nodes; explicit readback required, no overwrite"));
    UEdGraphPin* ResultPin = Prior->LinkedTo[0];
    const FRows* Groups = nullptr;
    if (!Config->TryGetArrayField(TEXT("groups"), Groups) || Groups->Num() != 11)
        return Finish(Report, TEXT("Expected 11 reviewed physics groups"));
    TArray<TUniquePtr<FStructOnScope>> Prepared;
    TArray<FString> Names; TSet<FName> UsedRoots; TSet<FName> ClaimedBones;
    for (const auto& V : *Groups)
    {
        auto G = V->AsObject(); const FString Name = G->GetStringField(TEXT("name"));
        if (Name.IsEmpty() || Names.Contains(Name) || Name.Contains(TEXT("/")) || Name.Len() > 32)
            return Finish(Report, TEXT("Invalid or repeated group name"));
        Names.Add(Name);
        const auto& Roots = G->GetArrayField(TEXT("roots"));
        if (Roots.IsEmpty()) return Finish(Report, TEXT("Empty root list"));
        for (const auto& Root : Roots)
        {
            const FName RootName(*Root->AsString()); const int32 RootIndex = Ref.FindBoneIndex(RootName);
            if (!InSecondaryScope(RootName.ToString()) || RootIndex == INDEX_NONE || UsedRoots.Contains(RootName))
                return Finish(Report, TEXT("Unapproved, missing or duplicate secondary root"));
            UsedRoots.Add(RootName);
            for (int32 I = 0; I < Ref.GetNum(); ++I)
            {
                int32 Ancestor = I; while (Ancestor != INDEX_NONE && Ancestor != RootIndex) Ancestor = Ref.GetParentIndex(Ancestor);
                if (Ancestor != RootIndex) continue;
                const FName Bone = Ref.GetBoneName(I);
                if (!AllowedBones.Contains(Bone) || ClaimedBones.Contains(Bone))
                    return Finish(Report, TEXT("Overlapping root or unexpected native descendant"));
                ClaimedBones.Add(Bone);
            }
        }
        auto Props = G->GetObjectField(TEXT("node_properties"));
        // Drive the actual root fields from the validated roots, not a second untrusted list.
        auto RootObject = MakeShared<FJsonObject>(); RootObject->SetStringField(TEXT("BoneName"), Roots[0]->AsString());
        Props->SetObjectField(TEXT("RootBone"), RootObject);
        FRows Additional;
        for (int32 I = 1; I < Roots.Num(); ++I)
        {
            auto B = MakeShared<FJsonObject>(); B->SetStringField(TEXT("BoneName"), Roots[I]->AsString());
            auto A = MakeShared<FJsonObject>(); A->SetObjectField(TEXT("RootBone"), B); Additional.Add(MakeShared<FJsonValueObject>(A));
        }
        Props->SetArrayField(TEXT("AdditionalRootBones"), Additional);
        FRows Spheres, Capsules;
        for (const auto& CValue : G->GetArrayField(TEXT("colliders")))
        {
            auto C = CValue->AsObject(); const FString Bone = C->GetStringField(TEXT("driving_bone"));
            FVector Offset, Axis;
            const double Radius = C->GetNumberField(TEXT("radius_cm")), Length = C->GetNumberField(TEXT("length_cm"));
            if (Ref.FindBoneIndex(FName(*Bone)) == INDEX_NONE || !JsonVector(C, TEXT("offset_cm"), Offset)
                || !JsonVector(C, TEXT("axis_local"), Axis) || !Axis.IsNormalized() || Offset.Size() > 100
                || !FMath::IsFinite(Radius) || !FMath::IsFinite(Length) || Radius <= 0 || Radius > 30 || Length < 0 || Length > 100)
                return Finish(Report, TEXT("Invalid converted collider"));
            auto Limit = MakeShared<FJsonObject>(); auto Driving = MakeShared<FJsonObject>();
            Driving->SetStringField(TEXT("BoneName"), Bone); Limit->SetObjectField(TEXT("DrivingBone"), Driving);
            auto Location = MakeShared<FJsonObject>(); Location->SetNumberField(TEXT("X"), Offset.X);
            Location->SetNumberField(TEXT("Y"), Offset.Y); Location->SetNumberField(TEXT("Z"), Offset.Z);
            Limit->SetObjectField(TEXT("OffsetLocation"), Location);
            const FRotator Rot = FQuat::FindBetweenNormals(FVector::UpVector, Axis).Rotator();
            auto Rotation = MakeShared<FJsonObject>(); Rotation->SetNumberField(TEXT("Pitch"), Rot.Pitch);
            Rotation->SetNumberField(TEXT("Yaw"), Rot.Yaw); Rotation->SetNumberField(TEXT("Roll"), Rot.Roll);
            Limit->SetObjectField(TEXT("OffsetRotation"), Rotation); Limit->SetNumberField(TEXT("Radius"), Radius);
            Limit->SetBoolField(TEXT("bEnable"), true);
            if (Length > 0) { Limit->SetNumberField(TEXT("Length"), Length); Capsules.Add(MakeShared<FJsonValueObject>(Limit)); }
            else { Limit->SetStringField(TEXT("LimitType"), TEXT("Outer")); Spheres.Add(MakeShared<FJsonValueObject>(Limit)); }
        }
        Props->SetArrayField(TEXT("SphericalLimits"), Spheres); Props->SetArrayField(TEXT("CapsuleLimits"), Capsules);
        FString Error;
        if (!CheckJsonStruct(NodeProperty->Struct, Props, Error)) return Finish(Report, Error);
        auto Data = MakeUnique<FStructOnScope>(NodeProperty->Struct);
        NodeProperty->CopyCompleteValue(Data->GetStructMemory(), NodeProperty->ContainerPtrToValuePtr<void>(KawaiiClass->GetDefaultObject()));
        FText ImportError;
        if (!FJsonObjectConverter::JsonObjectToUStruct(Props.ToSharedRef(), NodeProperty->Struct, Data->GetStructMemory(), 0, 0, false, &ImportError))
            return Finish(Report, TEXT("Native node property import failed: ") + ImportError.ToString());
        Prepared.Add(MoveTemp(Data));
    }
    if (UsedRoots.Num() != 42 || ClaimedBones.Num() != AllowedBones.Num())
        return Finish(Report, TEXT("Not all reviewed roots/reference bones covered"));
    Blueprint->Modify(); Graph->Modify();
    auto Add = [Graph](UEdGraphNode* Node, int32 X)
    {
        Graph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->PostPlacedNewNode();
        Node->NodePosX = X; Node->NodePosY = 1000; Node->AllocateDefaultPins();
    };
    auto* ToComponent = NewObject<UAnimGraphNode_LocalToComponentSpace>(Graph, TEXT("M5VS2_ToComponent"), RF_Transactional);
    Add(ToComponent, 0);
    auto* ToLocal = NewObject<UAnimGraphNode_ComponentToLocalSpace>(Graph, TEXT("M5VS2_ToLocal"), RF_Transactional);
    Add(ToLocal, 3000);
    TArray<UEdGraphNode*> NewNodes;
    for (int32 I = 0; I < Prepared.Num(); ++I)
    {
        auto* N = NewObject<UEdGraphNode>(Graph, KawaiiClass, FName(*(TEXT("M5VS2_Physics_") + Names[I])), RF_Transactional);
        NodeProperty->CopyCompleteValue(NodeProperty->ContainerPtrToValuePtr<void>(N), Prepared[I]->GetStructMemory());
        Add(N, (I + 1) * 230); NewNodes.Add(N);
    }
    const UEdGraphSchema* Schema = Graph->GetSchema();
    auto Connect = [Schema](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Schema->TryCreateConnection(A, B); };
    Prior->BreakLinkTo(ResultPin);
    bool Linked = Connect(Prior, ToComponent->FindPin(TEXT("LocalPose")));
    UEdGraphPin* Last = PhysicsOutput(ToComponent);
    for (auto* N : NewNodes) { Linked &= Connect(Last, N->FindPin(TEXT("ComponentPose"))); Last = PhysicsOutput(N); }
    Linked &= Connect(Last, ToLocal->FindPin(TEXT("ComponentPose")));
    Linked &= Connect(PhysicsOutput(ToLocal), ResultPin);
    if (!Linked) return Finish(Report, TEXT("New graph connection failed; caller must not save target"));
    Blueprint->SetPreviewMesh(Target);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return Finish(Report, TEXT("New VS2 ABP compile failed; caller must not save"));
    Blueprint->MarkPackageDirty();
    Report = PhysicsReadback(Blueprint);
    Report->SetNumberField(TEXT("root_count"), UsedRoots.Num());
    Report->SetNumberField(TEXT("reference_max_error_cm"), MaxReferenceError);
    Report->SetStringField(TEXT("status"), TEXT("AUTHOR_CONFIGURED_NOT_RUNTIME_ACCEPTANCE"));
    Report->SetBoolField(TEXT("saved_by_helper"), false);
    return Finish(Report);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}

FString UHCM5VS2PhysicsEditor::InspectPhysicsSetup(USkeletalMesh* Target, UAnimBlueprint* Blueprint,
    const FString& SourceAuditJson, const FString& PrefabPath)
{
#if WITH_EDITOR
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("status"), TEXT("FAIL"));
    Report->SetStringField(TEXT("scope"), TEXT("READ_ONLY_REFERENCE_POSE_AND_PLUGIN_SCHEMA"));
    Report->SetStringField(TEXT("mapping_approval"), TEXT("PENDING_ROOT_REVIEW"));
    Report->SetStringField(TEXT("physics_authoring"), TEXT("NOT_RUN"));
    Report->SetStringField(TEXT("runtime_swing_collision_visual"), TEXT("NOT_RUN"));
    if (!InReadScope(Target, TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia.SKM_Selestia"))
        || !InReadScope(Blueprint, TEXT("/Game/HarborCity/M5VS1/HeroSelestia/Animation/ABP_M4R2_Player_Selestia.ABP_M4R2_Player_Selestia"))
        || !Target->GetSkeleton() || Blueprint->TargetSkeleton != Target->GetSkeleton())
        return Finish(Report, TEXT("Expected existing Selestia baseline or VS2 copies with matching skeleton"));
    if (SourceAuditJson.IsEmpty() || SourceAuditJson.Len() > 8 * 1024 * 1024 || PrefabPath.IsEmpty())
        return Finish(Report, TEXT("Supply bounded source audit JSON and one exact prefab path"));

    TSharedPtr<FJsonObject> Audit;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SourceAuditJson), Audit) || !Audit)
        return Finish(Report, TEXT("Invalid source audit JSON"));
    const FRows* Prefabs = nullptr;
    if (!Audit->TryGetArrayField(TEXT("prefabs"), Prefabs) || Prefabs->Num() > 8)
        return Finish(Report, TEXT("Missing or oversized prefabs array"));
    TSharedPtr<FJsonObject> Prefab;
    for (const auto& Value : *Prefabs)
    {
        if (!Value || Value->Type != EJson::Object) return Finish(Report, TEXT("Invalid prefab entry"));
        const auto Candidate = Value->AsObject();
        FString Path;
        if (Candidate->TryGetStringField(TEXT("path"), Path) && Path == PrefabPath)
        {
            if (Prefab) return Finish(Report, TEXT("Ambiguous duplicate prefab path"));
            Prefab = Candidate;
        }
    }
    if (!Prefab) return Finish(Report, TEXT("Exact source prefab not found"));
    const FRows* Chains = nullptr;
    const FRows* Colliders = nullptr;
    if (!Prefab->TryGetArrayField(TEXT("physbones"), Chains) || Chains->Num() > 128
        || !Prefab->TryGetArrayField(TEXT("explicit_physbone_colliders"), Colliders) || Colliders->Num() > 128)
        return Finish(Report, TEXT("Missing or oversized source chain/collider arrays"));

    const bool bMeshDirty = Target->GetOutermost()->IsDirty();
    const bool bBlueprintDirty = Blueprint->GetOutermost()->IsDirty();
    const bool bSkeletonDirty = Target->GetSkeleton()->GetOutermost()->IsDirty();
    Report->SetStringField(TEXT("mesh"), Target->GetPathName());
    Report->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    Report->SetStringField(TEXT("skeleton"), Target->GetSkeleton()->GetPathName());
    Report->SetStringField(TEXT("prefab_path"), PrefabPath);
    Report->SetArrayField(TEXT("bounds_origin_cm"), XYZ(Target->GetBounds().Origin));
    Report->SetArrayField(TEXT("bounds_extent_cm"), XYZ(Target->GetBounds().BoxExtent));

    const FReferenceSkeleton& Ref = Target->GetRefSkeleton();
    const TArray<FTransform>& Local = Ref.GetRefBonePose();
    if (Ref.GetNum() < 1 || Ref.GetNum() > 4096 || Local.Num() != Ref.GetNum())
        return Finish(Report, TEXT("Invalid bounded imported reference skeleton"));
    TArray<FTransform> CS;
    CS.SetNum(Local.Num());
    FRows Bones;
    for (int32 I = 0; I < Local.Num(); ++I)
    {
        const int32 Parent = Ref.GetParentIndex(I);
        if (Parent < INDEX_NONE || Parent >= I || Local[I].ContainsNaN())
            return Finish(Report, TEXT("Invalid reference parent or transform"));
        CS[I] = Parent == INDEX_NONE ? Local[I] : Local[I] * CS[Parent];
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("index"), I);
        Row->SetStringField(TEXT("name"), Ref.GetBoneName(I).ToString());
        Row->SetNumberField(TEXT("parent_index"), Parent);
        Row->SetObjectField(TEXT("local_refpose"), TransformRow(Local[I]));
        Row->SetObjectField(TEXT("component_refpose"), TransformRow(CS[I]));
        if (Parent != INDEX_NONE)
        {
            const FVector Direction = CS[I].GetTranslation() - CS[Parent].GetTranslation();
            Row->SetNumberField(TEXT("parent_distance_cm"), Direction.Size());
            Row->SetArrayField(TEXT("parent_to_child_direction_cs"), XYZ(Direction.GetSafeNormal()));
            Row->SetArrayField(TEXT("direction_in_parent_axes"),
                XYZ(CS[Parent].GetRotation().UnrotateVector(Direction).GetSafeNormal()));
        }
        Bones.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetArrayField(TEXT("imported_reference_bones"), Bones);

    FRows ChainRows;
    TMap<int32, FString> CandidateOwners;
    FRows Overlaps;
    int32 Missing = 0;
    for (int32 I = 0; I < Chains->Num(); ++I)
    {
        const auto& Value = (*Chains)[I];
        if (!Value || Value->Type != EJson::Object) return Finish(Report, TEXT("Invalid chain object"));
        const auto Source = Value->AsObject();
        FString Root;
        const FRows* SourceBones = nullptr;
        if (!Source->TryGetStringField(TEXT("root"), Root)
            || !Source->TryGetArrayField(TEXT("chain_bones"), SourceBones) || SourceBones->Num() > 256)
            return Finish(Report, TEXT("Invalid source chain names"));
        double Enabled = 0;
        if (!Source->TryGetNumberField(TEXT("enabled"), Enabled))
            return Finish(Report, TEXT("Source chain enabled flag missing"));
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("source_chain_index"), I);
        Row->SetStringField(TEXT("source_root"), Root);
        Row->SetBoolField(TEXT("source_enabled"), Enabled != 0);
        Row->SetBoolField(TEXT("requested_secondary_scope"), InSecondaryScope(Root));
        int32 RootIndex = INDEX_NONE;
        Row->SetObjectField(TEXT("root_match"), MapCandidate(Root, Ref, RootIndex));
        FRows Mapped;
        bool bDescendants = RootIndex != INDEX_NONE;
        for (const auto& Bone : *SourceBones)
        {
            if (!Bone || Bone->Type != EJson::String) return Finish(Report, TEXT("Invalid source bone name"));
            int32 Index = INDEX_NONE;
            auto Match = MapCandidate(Bone->AsString(), Ref, Index);
            if (Index == INDEX_NONE) { ++Missing; bDescendants = false; }
            else
            {
                int32 Ancestor = Index;
                while (Ancestor != INDEX_NONE && Ancestor != RootIndex) Ancestor = Ref.GetParentIndex(Ancestor);
                const bool bInRoot = RootIndex != INDEX_NONE && Ancestor == RootIndex;
                Match->SetBoolField(TEXT("is_root_or_descendant"), bInRoot);
                bDescendants &= bInRoot;
                if (InSecondaryScope(Root) && Enabled != 0)
                {
                    if (const FString* Prior = CandidateOwners.Find(Index))
                    {
                        auto Overlap = MakeShared<FJsonObject>();
                        Overlap->SetStringField(TEXT("ue_bone"), Ref.GetBoneName(Index).ToString());
                        Overlap->SetStringField(TEXT("first_source_root"), *Prior);
                        Overlap->SetStringField(TEXT("second_source_root"), Root);
                        Overlaps.Add(MakeShared<FJsonValueObject>(Overlap));
                    }
                    else CandidateOwners.Add(Index, Root);
                }
            }
            Mapped.Add(MakeShared<FJsonValueObject>(Match));
        }
        Row->SetArrayField(TEXT("bone_matches"), Mapped);
        Row->SetBoolField(TEXT("all_candidates_are_root_or_descendants"), bDescendants);
        const TSharedPtr<FJsonObject>* Settings = nullptr;
        if (Source->TryGetObjectField(TEXT("settings"), Settings)) Row->SetObjectField(TEXT("source_settings_unconverted"), *Settings);
        const FRows* ColliderRefs = nullptr;
        if (Source->TryGetArrayField(TEXT("collider_component_ids"), ColliderRefs))
            Row->SetNumberField(TEXT("source_collider_reference_count"), ColliderRefs->Num());
        ChainRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetArrayField(TEXT("source_chains"), ChainRows);
    Report->SetArrayField(TEXT("candidate_chain_overlaps"), Overlaps);
    Report->SetNumberField(TEXT("unresolved_source_bone_occurrences"), Missing);

    FRows ColliderRows;
    for (int32 I = 0; I < Colliders->Num(); ++I)
    {
        const auto& Value = (*Colliders)[I];
        if (!Value || Value->Type != EJson::Object) return Finish(Report, TEXT("Invalid collider object"));
        const auto Source = Value->AsObject();
        FString Root;
        if (!Source->TryGetStringField(TEXT("root"), Root)) return Finish(Report, TEXT("Missing collider root"));
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("source_collider_index"), I);
        int32 Index = INDEX_NONE;
        Row->SetObjectField(TEXT("driving_bone_candidate"), MapCandidate(Root, Ref, Index));
        const TSharedPtr<FJsonObject>* Settings = nullptr;
        if (Source->TryGetObjectField(TEXT("settings"), Settings)) Row->SetObjectField(TEXT("source_settings_unconverted"), *Settings);
        Row->SetStringField(TEXT("ue_conversion"), TEXT("NOT_APPLIED_REQUIRES_SOURCE_TRANSFORM_ALIGNMENT"));
        ColliderRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Report->SetArrayField(TEXT("source_colliders"), ColliderRows);
    Report->SetStringField(TEXT("source_id_boundary"), TEXT("Unity int64 component IDs are not joined through JSON doubles; source array index/path/root identifies these probe rows."));
    Report->SetStringField(TEXT("conversion_next_step"), TEXT("Match Unity full bind transforms with imported UE component reference transforms; solve global unit/basis alignment and per-bone local basis, validate residuals, then transform collider centers and axes. Capsule Length is sphere-center separation along Kawaii local Z. No scalar PhysBone-to-Kawaii identity or full curve reconstruction is assumed."));

    UClass* KawaiiClass = FindObject<UClass>(nullptr, TEXT("/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics"));
    const FStructProperty* NodeProperty = KawaiiClass ? FindFProperty<FStructProperty>(KawaiiClass, TEXT("Node")) : nullptr;
    Report->SetBoolField(TEXT("kawaii_editor_class_loaded"), KawaiiClass != nullptr);
    if (NodeProperty)
    {
        const UObject* Defaults = KawaiiClass->GetDefaultObject();
        const void* NodeData = NodeProperty->ContainerPtrToValuePtr<void>(Defaults);
        Report->SetObjectField(TEXT("kawaii_node_defaults"), ReadFields(NodeProperty->Struct, NodeData, NodeFields()));
        for (FName FieldName : {FName(TEXT("SphericalLimits")), FName(TEXT("CapsuleLimits"))})
        {
            const FArrayProperty* Array = FindFProperty<FArrayProperty>(NodeProperty->Struct, FieldName);
            const FStructProperty* Element = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
            Report->SetObjectField(FieldName.ToString() + TEXT("_schema"),
                ReadFields(Element ? Element->Struct : nullptr, nullptr,
                    {TEXT("DrivingBone"), TEXT("OffsetLocation"), TEXT("OffsetRotation"), TEXT("Radius"), TEXT("Length"), TEXT("LimitType")}));
        }
    }
    FRows Controls;
    FRows Roots;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            const FString ClassName = Node->GetClass()->GetName();
            const bool bKawaii = KawaiiClass && Node->IsA(KawaiiClass);
            if (bKawaii || ClassName.Contains(TEXT("AnimDynamics")) || ClassName.Contains(TEXT("RigidBody")) || ClassName.Contains(TEXT("ControlRig")))
            {
                auto Row = MakeShared<FJsonObject>();
                Row->SetStringField(TEXT("node"), Node->GetPathName());
                Row->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
                if (bKawaii && NodeProperty)
                    Row->SetObjectField(TEXT("settings"), ReadFields(NodeProperty->Struct,
                        NodeProperty->ContainerPtrToValuePtr<void>(Node), NodeFields()));
                Controls.Add(MakeShared<FJsonValueObject>(Row));
            }
            if (ClassName == TEXT("AnimGraphNode_Root"))
            {
                auto Row = MakeShared<FJsonObject>();
                Row->SetStringField(TEXT("node"), Node->GetPathName());
                FRows Pins;
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin || Pin->Direction != EGPD_Input) continue;
                    auto PinRow = MakeShared<FJsonObject>();
                    PinRow->SetStringField(TEXT("pin"), Pin->PinName.ToString());
                    FRows Links;
                    for (const UEdGraphPin* Linked : Pin->LinkedTo)
                        if (Linked) Links.Add(MakeShared<FJsonValueString>(Linked->GetOwningNode()->GetPathName() + TEXT(":") + Linked->PinName.ToString()));
                    PinRow->SetArrayField(TEXT("linked_from"), Links);
                    Pins.Add(MakeShared<FJsonValueObject>(PinRow));
                }
                Row->SetArrayField(TEXT("input_pins"), Pins);
                Roots.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
    }
    Report->SetArrayField(TEXT("existing_secondary_or_control_rig_nodes"), Controls);
    Report->SetArrayField(TEXT("animation_output_roots"), Roots);
    const bool bDirtyStateUnchanged = Target->GetOutermost()->IsDirty() == bMeshDirty
        && Blueprint->GetOutermost()->IsDirty() == bBlueprintDirty
        && Target->GetSkeleton()->GetOutermost()->IsDirty() == bSkeletonDirty;
    Report->SetBoolField(TEXT("package_dirty_flags_unchanged"), bDirtyStateUnchanged);
    if (!bDirtyStateUnchanged) return Finish(Report, TEXT("Unexpected package dirty-state change during read-only probe"));
    Report->SetStringField(TEXT("status"), TEXT("PROBE_COMPLETE_NOT_PHYSICS_ACCEPTANCE"));
    return Finish(Report);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2PhysicsEditor::AnchorPrivateHeroHair(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Blueprint||!Blueprint->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroPolish/")))
        return Finish(R,TEXT("New private HeroPolish AnimBlueprint required"));
    TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
    TArray<UEdGraphNode*> Selected;int32 Total=0;
    for(auto* Graph:Graphs)if(Graph)for(UEdGraphNode* Node:Graph->Nodes)
    {
        if(!Node||Node->GetClass()->GetName()!=TEXT("AnimGraphNode_KawaiiPhysics"))continue;
        ++Total;
        const auto* P=FindFProperty<FStructProperty>(Node->GetClass(),TEXT("Node"));
        const void* Data=P?P->ContainerPtrToValuePtr<void>(Node):nullptr;
        const auto* Root=P?FindFProperty<FStructProperty>(P->Struct,TEXT("RootBone")):nullptr;
        const auto* Name=Root?FindFProperty<FNameProperty>(Root->Struct,TEXT("BoneName")):nullptr;
        if(!Name||!Data)return Finish(R,TEXT("Installed Kawaii RootBone schema missing"));
        const FName N=Name->GetPropertyValue_InContainer(Root->ContainerPtrToValuePtr<void>(Data));
        if(N==TEXT("Hair_back_long1_L")||N==TEXT("Hair_side2_L"))Selected.Add(Node);
    }
    if(Total!=11||Selected.Num()!=2)return Finish(R,TEXT("Expected 11 unchanged chains and exactly two long-hair groups"));
    Blueprint->Modify();
    for(auto* Node:Selected)
    {
        auto* P=FindFProperty<FStructProperty>(Node->GetClass(),TEXT("Node"));
        const auto* Flag=FindFProperty<FBoolProperty>(P->Struct,TEXT("bAnchorFixedStepOutputToCurrentPose"));
        if(!Flag)return Finish(R,TEXT("Compiled project-local anchor field missing"));
        Node->Modify();Flag->SetPropertyValue_InContainer(P->ContainerPtrToValuePtr<void>(Node),true);
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if(Blueprint->Status==BS_Error||!Blueprint->GeneratedClass)return Finish(R,TEXT("Private hair Blueprint compile failed"));
    int32 Bound=0;const auto* Defaults=Blueprint->GeneratedClass->GetDefaultObject();
    for(TFieldIterator<FStructProperty> It(Defaults->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        const auto* P=*It;if(P->Struct->GetName()!=TEXT("AnimNode_KawaiiPhysics"))continue;
        const auto* Flag=FindFProperty<FBoolProperty>(P->Struct,TEXT("bAnchorFixedStepOutputToCurrentPose"));
        if(Flag&&Flag->GetPropertyValue_InContainer(P->ContainerPtrToValuePtr<void>(Defaults)))++Bound;
    }
    if(Bound!=2)return Finish(R,TEXT("Generated native defaults did not retain exactly two root anchors"));
    R->SetStringField(TEXT("status"),TEXT("PASS"));R->SetNumberField(TEXT("anchored_groups"),Bound);
    R->SetBoolField(TEXT("saved_by_helper"),false);return Finish(R);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}
