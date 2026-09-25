#include "HCM5VS2ExpressionEditor.h"
#include "HCM5VS2ExpressionComponent.h"
#include "HCM5VS2LookAnimInstance.h"

#if WITH_EDITOR
#include "M1/HCM1Character.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "AnimGraphNode_LookAt.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/Package.h"

namespace
{
const TCHAR* HeroPackage = TEXT("/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia");
const TCHAR* AnimPackage = TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics");

FString Finish(const TSharedPtr<FJsonObject>& Report, const FString& Error = FString())
{
    Report->SetStringField(TEXT("status"), Error.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    if (!Error.IsEmpty()) Report->SetStringField(TEXT("error"), Error);
    Report->SetStringField(TEXT("runtime_visual"), TEXT("NOT_RUN"));
    Report->SetBoolField(TEXT("saved_by_helper"), false);
    FString Result;
    FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}

template<class T> T* NewNode(UEdGraph* Graph, FName Name, int32 X, int32 Y)
{
    T* Node = NewObject<T>(Graph, Name, RF_Transactional);
    Graph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->PostPlacedNewNode();
    Node->NodePosX = X; Node->NodePosY = Y;
    return Node;
}

UEdGraphPin* OutputPin(UEdGraphNode* Node)
{
    UEdGraphPin* Result = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
        if (Pin->Direction == EGPD_Output) { if (Result) return nullptr; Result = Pin; }
    return Result;
}

bool Expose(UAnimGraphNode_LookAt* Node, FName Property)
{
    if (Node->FindPin(Property)) return true;
    for (int32 I = 0; I < Node->ShowPinForProperties.Num(); ++I)
        if (Node->ShowPinForProperties[I].PropertyName == Property)
        { Node->SetPinVisibility(true, I); return Node->FindPin(Property) != nullptr; }
    return false;
}
}
#endif

FString UHCM5VS2ExpressionEditor::InstallHeroExpressionComponent(UBlueprint* Blueprint)
{
#if WITH_EDITOR
    auto Report = MakeShared<FJsonObject>();
    if (!Blueprint || Blueprint->GetOutermost()->GetName() != HeroPackage || !Blueprint->SimpleConstructionScript
        || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(AHCM1Character::StaticClass()))
        return Finish(Report, TEXT("Exact new VS2 character Blueprint with native character parent required"));
    USCS_Node* Found = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node->GetVariableName() == TEXT("M5VS2Expression")
            || Node->ComponentClass == UHCM5VS2ExpressionComponent::StaticClass())
        {
            if (Found || Node->GetVariableName() != TEXT("M5VS2Expression")
                || Node->ComponentClass != UHCM5VS2ExpressionComponent::StaticClass())
                return Finish(Report, TEXT("Unexpected expression component/name collision; no duplicate installed"));
            Found = Node;
        }
    }
    Blueprint->Modify(); Blueprint->SimpleConstructionScript->Modify();
    if (!Found)
    {
        Found = Blueprint->SimpleConstructionScript->CreateNode(UHCM5VS2ExpressionComponent::StaticClass(), TEXT("M5VS2Expression"));
        if (!Found) return Finish(Report, TEXT("Native SCS component creation failed"));
        Blueprint->SimpleConstructionScript->AddNode(Found);
    }
    auto* Face = Cast<UHCM5VS2ExpressionComponent>(Found->ComponentTemplate);
    if (!Face) return Finish(Report, TEXT("Native expression component template missing"));
    Face->Modify(); Face->bExpressionsEnabled = true; Face->bAutomaticBlink = true;
    Face->bReadOwnerCombatState = true; Face->BlinkClosedSeconds = .12f;
    Face->TargetMesh = nullptr; // Instance resolves its own ACharacter::GetMesh, never a CDO pointer.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return Finish(Report, TEXT("New character failed native Blueprint compilation"));
    Blueprint->MarkPackageDirty();
    Report->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    Report->SetStringField(TEXT("component"), Found->ComponentTemplate->GetPathName());
    Report->SetNumberField(TEXT("closed_hold_seconds"), Face->BlinkClosedSeconds);
    Report->SetStringField(TEXT("dialogue_integration"), TEXT("Explicit speaker Emotion/text-progress calls still required"));
    return Finish(Report);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2ExpressionEditor::ConfigureLookGraph(UAnimBlueprint* Blueprint, USkeletalMesh* Mesh)
{
#if WITH_EDITOR
    auto Report = MakeShared<FJsonObject>();
    if (!Blueprint || Blueprint->GetOutermost()->GetName() != AnimPackage || !Mesh || !Mesh->GetSkeleton()
        || Blueprint->TargetSkeleton != Mesh->GetSkeleton() || !Blueprint->ParentClass
        || !Blueprint->ParentClass->IsChildOf(UHCM4R2PlayerAnimInstance::StaticClass()))
        return Finish(Report, TEXT("Exact new Physics ABP with compatible base state and matching Selestia mesh required"));
    if (!Mesh->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroSelestia/"))
        && Mesh->GetOutermost()->GetName() != TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia"))
        return Finish(Report, TEXT("Only verified Selestia baseline or new VS2 mesh allowed"));
    const FName Bones[] = {TEXT("Head"), TEXT("LeftEye"), TEXT("RightEye")};
    const FName NodeNames[] = {TEXT("M5VS2_LookHead"), TEXT("M5VS2_LookLeftEye"), TEXT("M5VS2_LookRightEye")};
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    TArray<FTransform> CS; CS.SetNum(Ref.GetNum());
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        const int32 Parent = Ref.GetParentIndex(I);
        if (Parent >= I || Ref.GetRefBonePose()[I].ContainsNaN()) return Finish(Report, TEXT("Invalid reference hierarchy"));
        CS[I] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * CS[Parent];
    }
    for (FName Bone : Bones)
        if (Ref.FindBoneIndex(Bone) == INDEX_NONE) return Finish(Report, TEXT("Actual Head/LeftEye/RightEye required"));
    if (Ref.GetParentIndex(Ref.FindBoneIndex(Bones[1])) != Ref.FindBoneIndex(Bones[0])
        || Ref.GetParentIndex(Ref.FindBoneIndex(Bones[2])) != Ref.FindBoneIndex(Bones[0]))
        return Finish(Report, TEXT("Unexpected eye parent hierarchy"));

    TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
    UEdGraph* Graph = nullptr; UEdGraphPin* Prior = nullptr; UEdGraphPin* FirstPhysicsInput = nullptr;
    int32 Existing = 0;
    for (UEdGraph* G : Graphs)
        for (UEdGraphNode* Node : G->Nodes)
        {
            for (FName Name : NodeNames) if (Node->GetFName() == Name) ++Existing;
            if (Node->GetClass()->GetName() != TEXT("AnimGraphNode_KawaiiPhysics")
                || !Node->GetName().StartsWith(TEXT("M5VS2_Physics_"))) continue;
            UEdGraphPin* Input = Node->FindPin(TEXT("ComponentPose"));
            if (Input && Input->LinkedTo.Num() == 1 && Cast<UAnimGraphNode_LocalToComponentSpace>(Input->LinkedTo[0]->GetOwningNode())
                && Input->LinkedTo[0]->GetOwningNode()->GetFName() == TEXT("M5VS2_ToComponent"))
            {
                if (FirstPhysicsInput) return Finish(Report, TEXT("Ambiguous first Kawaii chain"));
                Graph = G; Prior = Input->LinkedTo[0]; FirstPhysicsInput = Input;
            }
        }
    if (Existing)
    {
        if (Existing != 3 || Blueprint->ParentClass != UHCM5VS2LookAnimInstance::StaticClass())
            return Finish(Report, TEXT("Partial/colliding look graph found; refuse duplicate"));
        TArray<UAnimGraphNode_LookAt*> Nodes; FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Nodes);
        int32 Verified = 0;
        for (int32 I = 0; I < 3; ++I)
            for (auto* Node : Nodes)
                if (Node->GetFName() == NodeNames[I] && Node->Node.BoneToModify.BoneName == Bones[I])
                {
                    const UEdGraphPin* Target = Node->FindPin(TEXT("LookAtLocation"));
                    const UEdGraphPin* Alpha = Node->FindPin(TEXT("Alpha"));
                    if (Target && Target->LinkedTo.Num() == 1 && Alpha && Alpha->LinkedTo.Num() == 1) ++Verified;
                }
        if (Verified != 3 || Blueprint->Status == BS_Error) return Finish(Report, TEXT("Existing look nodes are not fully bound"));
        Report->SetNumberField(TEXT("look_node_count"), Verified);
        Report->SetBoolField(TEXT("already_present"), true);
        return Finish(Report);
    }
    if (!Graph || !Prior || !FirstPhysicsInput) return Finish(Report, TEXT("Author Kawaii graph first; expected LocalToComponent directly before first Kawaii"));
    const FName FirstPhysicsNodeName = FirstPhysicsInput->GetOwningNode()->GetFName();
    Blueprint->Modify(); Graph->Modify(); Blueprint->ParentClass = UHCM5VS2LookAnimInstance::StaticClass();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return Finish(Report, TEXT("Compatible look subclass failed compilation before graph change"));
    // Regeneration can reconstruct pins. Reacquire them rather than reuse pre-compile pin pointers.
    Prior = nullptr; FirstPhysicsInput = nullptr;
    Graphs.Reset(); Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* G : Graphs)
        for (UEdGraphNode* Node : G->Nodes)
            if (Node->GetFName() == FirstPhysicsNodeName)
            {
                UEdGraphPin* Input = Node->FindPin(TEXT("ComponentPose"));
                if (Input && Input->LinkedTo.Num() == 1
                    && Input->LinkedTo[0]->GetOwningNode()->GetFName() == TEXT("M5VS2_ToComponent"))
                { Graph = G; FirstPhysicsInput = Input; Prior = Input->LinkedTo[0]; }
            }
    if (!Prior || !FirstPhysicsInput) return Finish(Report, TEXT("Physics entry changed during class regeneration; do not save"));
    auto* Location = NewNode<UK2Node_VariableGet>(Graph, TEXT("M5VS2_LookTarget"), Prior->GetOwningNode()->NodePosX, 1000);
    Location->VariableReference.SetSelfMember(TEXT("VS2LookTargetWorld")); Location->AllocateDefaultPins();
    auto* HeadAlpha = NewNode<UK2Node_VariableGet>(Graph, TEXT("M5VS2_LookHeadAlpha"), Location->NodePosX, 1120);
    HeadAlpha->VariableReference.SetSelfMember(TEXT("VS2HeadLookAlpha")); HeadAlpha->AllocateDefaultPins();
    auto* EyeAlpha = NewNode<UK2Node_VariableGet>(Graph, TEXT("M5VS2_LookEyeAlpha"), Location->NodePosX, 1240);
    EyeAlpha->VariableReference.SetSelfMember(TEXT("VS2EyeLookAlpha")); EyeAlpha->AllocateDefaultPins();
    const auto Connect = [Graph](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Graph->GetSchema()->TryCreateConnection(A, B); };
    FirstPhysicsInput->BreakAllPinLinks();
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Node = NewNode<UAnimGraphNode_LookAt>(Graph, NodeNames[I], Prior->GetOwningNode()->NodePosX + 220, Prior->GetOwningNode()->NodePosY);
        Node->Node.BoneToModify.BoneName = Bones[I];
        const FQuat RefRotation = CS[Ref.FindBoneIndex(Bones[I])].GetRotation();
        // Mesh +Y is forward for the verified Selestia mesh at Character mesh yaw -90.
        // Convert that semantic frame into each actual bone's local reference axes.
        const FVector Forward = RefRotation.UnrotateVector(FVector(0, 1, 0)).GetSafeNormal();
        const FVector Up = RefRotation.UnrotateVector(FVector::UpVector).GetSafeNormal();
        Node->Node.LookAt_Axis = FAxis(Forward); Node->Node.LookUp_Axis = FAxis(Up);
        Node->Node.bUseLookUpAxis = true; Node->Node.LookAtClamp = I == 0 ? 32.f : 10.f;
        Node->Node.InterpolationTime = 0; // Runtime input direction already smooths once.
        Node->Node.Alpha = 0; Node->Node.LODThreshold = 1;
        Node->AllocateDefaultPins();
        if (!Expose(Node, TEXT("LookAtLocation")) || !Expose(Node, TEXT("Alpha"))
            || !Connect(Prior, Node->FindPin(TEXT("ComponentPose")))
            || !Connect(OutputPin(Location), Node->FindPin(TEXT("LookAtLocation")))
            || !Connect(OutputPin(I == 0 ? HeadAlpha : EyeAlpha), Node->FindPin(TEXT("Alpha"))))
            return Finish(Report, TEXT("Native look node/getter pin wiring failed; do not save"));
        Prior = OutputPin(Node);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("bone"), Bones[I].ToString());
        Row->SetStringField(TEXT("local_forward"), Forward.ToString()); Row->SetStringField(TEXT("local_up"), Up.ToString());
        Row->SetNumberField(TEXT("clamp_degrees"), Node->Node.LookAtClamp); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (!Connect(Prior, FirstPhysicsInput)) return Finish(Report, TEXT("Look-to-Kawaii connection failed; do not save"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return Finish(Report, TEXT("Look graph failed native Blueprint compile; do not save"));
    Blueprint->MarkPackageDirty();
    Report->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
    Report->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetPathName());
    Report->SetArrayField(TEXT("look_nodes"), Rows);
    Report->SetStringField(TEXT("order"), TEXT("Existing full-body pose -> LocalToComponent -> Head -> LeftEye -> RightEye -> existing Kawaii -> ComponentToLocal"));
    Report->SetStringField(TEXT("combat_fp_driving"), TEXT("Runtime look alphas zero; inherited animation state remains"));
    return Finish(Report);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}
