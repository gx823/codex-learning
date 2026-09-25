#include "HCM5VS2VillageEditor.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallParentFunction.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"

namespace
{
const FString VillageMount = TEXT("/Game/Fantastic_Village_Pack/");
const FString VillageDisk = TEXT("E:/GameDev/Assets/HarborCity/M5_VS2/Environment/HC_VillageSource/Content/Fantastic_Village_Pack/");
const FString VillageDestination = TEXT("/Game/HarborCity/M5VS2/Environment/Village_");
constexpr int32 MaxClosure = 280;
constexpr int64 MaxSourceBytes = 1024LL * 1024 * 1024;

FString Result(const TSharedRef<FJsonObject>& Report, const FString& Error = FString())
{
    Report->SetStringField(TEXT("status"), Error.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    if (!Error.IsEmpty()) Report->SetStringField(TEXT("error"), Error);
    Report->SetStringField(TEXT("source_disk"), VillageDisk);
    Report->SetStringField(TEXT("runtime_visual_collision"), TEXT("NOT_RUN"));
    Report->SetBoolField(TEXT("actors_spawned"), false);
    Report->SetBoolField(TEXT("construction_scripts_executed"), false);
    FString Text; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)); return Text;
}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Input)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const auto& Value : Input) Out.Add(MakeShared<FJsonValueString>(Value));
    return Out;
}
TArray<TSharedPtr<FJsonValue>> Vector(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
}
TSharedRef<FJsonObject> Transform(const FTransform& Value)
{
    auto Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("location_cm"), Vector(Value.GetLocation()));
    const FRotator Rotation = Value.Rotator();
    Out->SetArrayField(TEXT("pitch_yaw_roll"), Vector(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
    Out->SetArrayField(TEXT("scale"), Vector(Value.GetScale3D()));
    const FQuat Q = Value.GetRotation();
    Out->SetArrayField(TEXT("quaternion_xyzw"), {MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W)});
    return Out;
}
TSharedRef<FJsonObject> Bounds(const FBox& Box)
{
    auto Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("valid"), Box.IsValid != 0);
    if (Box.IsValid) { Out->SetArrayField(TEXT("min"), Vector(Box.Min)); Out->SetArrayField(TEXT("max"), Vector(Box.Max)); Out->SetArrayField(TEXT("size"), Vector(Box.GetSize())); }
    return Out;
}
struct FVillageMountScope
{
    bool bMounted = false;
    FString Error;
    FVillageMountScope()
    {
        if (!FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/科研学习/codex学习/HarborCity/")))
        { Error = TEXT("Only the authorized HarborCity project may mount this source"); return; }
        if (IFileManager::Get().DirectoryExists(*(FPaths::ProjectContentDir() / TEXT("Fantastic_Village_Pack"))))
        { Error = TEXT("Project already has this source namespace; refusing mount shadowing"); return; }
        if (!IFileManager::Get().DirectoryExists(*VillageDisk)) { Error = TEXT("Actual Village source folder absent"); return; }
        FPackageName::RegisterMountPoint(VillageMount, VillageDisk); bMounted = true;
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanPathsSynchronous({VillageMount.LeftChop(1)}, true);
    }
    ~FVillageMountScope() { if (bMounted) FPackageName::UnRegisterMountPoint(VillageMount, VillageDisk); }
};
bool AuditGraphs(UBlueprint* Blueprint, const TSharedRef<FJsonObject>& Out)
{
    bool bSafe = Blueprint && Blueprint->ParentClass == AActor::StaticClass();
    Out->SetStringField(TEXT("parent_class"), Blueprint ? GetPathNameSafe(Blueprint->ParentClass) : TEXT("None"));
    TArray<TSharedPtr<FJsonValue>> Nodes;
    if (Blueprint)
    {
        TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs) if (Graph) for (const UEdGraphNode* Node : Graph->Nodes) if (Node)
        {
            auto Row = MakeShared<FJsonObject>();
            const FString Class = Node->GetClass()->GetName();
            Row->SetStringField(TEXT("graph"), Graph->GetName()); Row->SetStringField(TEXT("node_class"), Class);
            Row->SetStringField(TEXT("node"), Node->GetName());
            bool bAllowed = Class == TEXT("K2Node_FunctionEntry") || Class == TEXT("K2Node_FunctionResult")
                || Class == TEXT("K2Node_Event") || Class == TEXT("EdGraphNode_Comment");
            if (const UK2Node_CallParentFunction* ParentCall = Cast<UK2Node_CallParentFunction>(Node))
            {
                const FName Function = ParentCall->GetFunctionName();
                Row->SetStringField(TEXT("function"), Function.ToString());
                bAllowed = Blueprint->ParentClass == AActor::StaticClass() && Function == TEXT("UserConstructionScript");
            }
            Row->SetBoolField(TEXT("allowed_inert_or_actor_parent_construction"), bAllowed); bSafe &= bAllowed;
            Nodes.Add(MakeShared<FJsonValueObject>(Row));
        }
        // Timelines/interfaces imply behavior outside a static component assembly.
        bSafe &= Blueprint->Timelines.IsEmpty() && Blueprint->ImplementedInterfaces.IsEmpty();
    }
    Out->SetArrayField(TEXT("graph_nodes"), Nodes);
    Out->SetBoolField(TEXT("inert_graph_allowlist_pass"), bSafe);
    return bSafe;
}
TSharedRef<FJsonObject> InspectHouse(const FString& Path)
{
    auto Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), Path);
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *(Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path)));
    bool bSafe = AuditGraphs(Blueprint, Out);
    TArray<FString> Rejections;
    auto Reject = [&](const FString& Reason) { bSafe = false; Rejections.AddUnique(Reason); };
    if (!bSafe) Rejections.Add(TEXT("GRAPH_OR_PARENT_CLASS_ALLOWLIST_REJECTED: inspect graph_nodes/parent_class"));
    TArray<TSharedPtr<FJsonValue>> Components;
    FBox FullBox(ForceInit);
    int32 MeshCount = 0;
    int32 OmittedBoundsCount = 0;
    if (!Blueprint || !Blueprint->SimpleConstructionScript) Reject(TEXT("BLUEPRINT_OR_SCS_MISSING"));
    else
    {
        TSet<const USCS_Node*> Seen;
        TFunction<void(USCS_Node*, const FTransform&, const FString&, int32)> Visit;
        Visit = [&](USCS_Node* Node, const FTransform& ParentTransform, const FString& ParentName, int32 Depth)
        {
            if (!Node) { Reject(TEXT("NULL_SCS_NODE")); return; }
            if (Seen.Contains(Node)) { Reject(TEXT("REPEATED_SCS_NODE: ") + Node->GetVariableName().ToString()); return; }
            if (Seen.Num() >= 512 || Depth > 32) { Reject(TEXT("SCS_TRAVERSAL_BOUND_EXCEEDED")); return; }
            Seen.Add(Node);
            const UActorComponent* Template = Node->ComponentTemplate;
            const USceneComponent* Scene = Cast<USceneComponent>(Template);
            if (!Scene) { Reject(TEXT("NON_SCENE_TEMPLATE: ") + Node->GetVariableName().ToString()); return; }
            // Exact native classes only: no child actors, plugin components, or user component scripts.
            const bool bComponentAllowed = Scene->GetClass() == USceneComponent::StaticClass() || Scene->GetClass() == UStaticMeshComponent::StaticClass();
            if (!bComponentAllowed)
            {
                Reject(TEXT("COMPONENT_CLASS_NOT_ALLOWLISTED: ") + Node->GetVariableName().ToString() + TEXT(" ") + Scene->GetClass()->GetPathName());
                ++OmittedBoundsCount;
            }
            if (ParentName.IsEmpty() && !Node->ParentComponentOrVariableName.IsNone())
                Reject(TEXT("EXTERNAL_ROOT_PARENT_UNRESOLVED: ") + Node->GetVariableName().ToString() + TEXT(" -> ") + Node->ParentComponentOrVariableName.ToString());
            const FTransform Local = Scene->GetRelativeTransform();
            const FTransform ToActor = Local * ParentTransform;
            if (Local.ContainsNaN() || ToActor.ContainsNaN()) { Reject(TEXT("NONFINITE_TEMPLATE_TRANSFORM: ") + Node->GetVariableName().ToString()); return; }
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("variable"), Node->GetVariableName().ToString());
            Row->SetStringField(TEXT("template"), Template->GetPathName()); Row->SetStringField(TEXT("class"), Template->GetClass()->GetPathName());
            Row->SetStringField(TEXT("parent_variable"), ParentName);
            Row->SetStringField(TEXT("external_parent_component_or_variable"), Node->ParentComponentOrVariableName.ToString());
            Row->SetStringField(TEXT("external_parent_owner_class"), Node->ParentComponentOwnerClassName.ToString());
            Row->SetBoolField(TEXT("external_parent_is_native"), Node->bIsParentComponentNative);
            Row->SetBoolField(TEXT("component_class_allowlist_pass"), bComponentAllowed);
            Row->SetObjectField(TEXT("relative_transform"), Transform(Local));
            Row->SetObjectField(TEXT("template_to_actor_transform"), Transform(ToActor));
            if (const UChildActorComponent* ChildComponent = Cast<UChildActorComponent>(Scene))
            {
                Row->SetStringField(TEXT("child_actor_class"), GetPathNameSafe(ChildComponent->GetChildActorClass().Get()));
                Row->SetStringField(TEXT("child_actor_template"), GetPathNameSafe(ChildComponent->GetChildActorTemplate()));
                Row->SetStringField(TEXT("child_actor_scope"), TEXT("Class/template path read only; not spawned, recursively executed, or included in geometry bounds"));
            }
            if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Scene))
            {
                const UStaticMesh* MeshAsset = Static->GetStaticMesh();
                Row->SetStringField(TEXT("static_mesh"), GetPathNameSafe(MeshAsset));
                Row->SetStringField(TEXT("collision_profile"), Static->GetCollisionProfileName().ToString());
                Row->SetNumberField(TEXT("collision_enabled"), static_cast<int32>(Static->GetCollisionEnabled()));
                Row->SetNumberField(TEXT("mobility"), static_cast<int32>(Static->Mobility));
                TArray<TSharedPtr<FJsonValue>> Materials;
                for (int32 Slot = 0; Slot < Static->GetNumMaterials(); ++Slot) Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Static->GetMaterial(Slot))));
                Row->SetArrayField(TEXT("effective_materials"), Materials);
                if (MeshAsset)
                {
                    ++MeshCount;
                    const FBox LocalBox = MeshAsset->GetBoundingBox();
                    const FBox ActorBox = LocalBox.TransformBy(ToActor);
                    Row->SetObjectField(TEXT("mesh_local_bounds_cm"), Bounds(LocalBox));
                    Row->SetObjectField(TEXT("template_actor_bounds_cm"), Bounds(ActorBox)); FullBox += ActorBox;
                }
            }
            Components.Add(MakeShared<FJsonValueObject>(Row));
            for (USCS_Node* Child : Node->GetChildNodes()) Visit(Child, ToActor, Node->GetVariableName().ToString(), Depth + 1);
        };
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetRootNodes()) Visit(Node, FTransform::Identity, FString(), 0);
        Out->SetNumberField(TEXT("scs_root_node_count"), Blueprint->SimpleConstructionScript->GetRootNodes().Num());
        Out->SetNumberField(TEXT("scs_all_node_count"), Blueprint->SimpleConstructionScript->GetAllNodes().Num());
        Out->SetNumberField(TEXT("scs_visited_node_count"), Seen.Num());
        if (Seen.Num() != Blueprint->SimpleConstructionScript->GetAllNodes().Num()) Reject(TEXT("SCS_ALL_VS_VISITED_COUNT_MISMATCH"));
    }
    if (MeshCount == 0) Reject(TEXT("NO_STATIC_MESH_GEOMETRY"));
    Out->SetArrayField(TEXT("rejection_reasons"), Strings(Rejections));
    Out->SetNumberField(TEXT("non_allowlisted_component_bounds_omitted"), OmittedBoundsCount);
    Out->SetArrayField(TEXT("components"), Components);
    Out->SetObjectField(TEXT("template_assembly_bounds_cm"), Bounds(FullBox));
    Out->SetNumberField(TEXT("static_mesh_component_count"), MeshCount);
    Out->SetBoolField(TEXT("safe_geometry_blueprint"), bSafe && MeshCount > 0);
    Out->SetStringField(TEXT("bounds_scope"), TEXT("Static-mesh SCS geometry only, composed at identity actor; child actors, particles and light influence excluded. Not complete spawned-Blueprint bounds; no construction, WPO or collision validation"));
    return Out;
}
bool Closure(const TArray<FString>& Roots, TArray<FString>& Packages, TArray<FString>& External, FString& Error)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TSet<FString> Seen;
    TArray<FString> Queue = Roots;
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const FString Current = Queue[Index];
        if (Seen.Contains(Current)) continue;
        Seen.Add(Current);
        if (Current.StartsWith(TEXT("/Engine/")) || Current.StartsWith(TEXT("/Script/"))) { External.AddUnique(Current); continue; }
        if (!Current.StartsWith(VillageMount)) { Error = TEXT("Unexpected external dependency: ") + Current; return false; }
        if (Packages.Num() >= MaxClosure) { Error = TEXT("Selected closure exceeds 280 source packages"); return false; }
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(FName(*Current), Assets, true);
        if (Assets.Num() != 1) { Error = TEXT("Expected exactly one registry asset in ") + Current; return false; }
        const FString Class = Assets[0].AssetClassPath.GetAssetName().ToString();
        // Independent static assemblies only. Do not load/copy any external Blueprint, particle or world dependency.
        const bool bSupported = Class == TEXT("StaticMesh") || Class == TEXT("Material")
            || Class == TEXT("MaterialInstanceConstant") || Class == TEXT("MaterialFunction") || Class == TEXT("MaterialParameterCollection")
            || Class == TEXT("Texture2D") || Class == TEXT("TextureCube");
        if (!bSupported) { Error = TEXT("Unsupported selected dependency class: ") + Class + TEXT(" ") + Current; return false; }
        FString Filename;
        if (!FPackageName::DoesPackageExist(Current, &Filename) || !FPaths::ConvertRelativePathToFull(Filename).StartsWith(VillageDisk))
        { Error = TEXT("Source package resolution escaped exact mounted source: ") + Current; return false; }
        Packages.Add(Current);
        TArray<FName> Dependencies;
        Registry.GetDependencies(FName(*Current), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        for (FName Dependency : Dependencies) Queue.Add(Dependency.ToString());
    }
    Packages.Sort(); External.Sort(); return true;
}
}
#endif

FString UHCM5VS2VillageEditor::ProbeVillageHouses()
{
#if WITH_EDITOR
    auto Report = MakeShared<FJsonObject>();
    FVillageMountScope Mount; if (!Mount.Error.IsEmpty()) return Result(Report, Mount.Error);
    TArray<TSharedPtr<FJsonValue>> Houses;
    for (int32 Index = 1; Index <= 14; ++Index)
        Houses.Add(MakeShared<FJsonValueObject>(InspectHouse(VillageMount + FString::Printf(TEXT("blueprints/buildings/BP_BLD_house_%d"), Index))));
    Report->SetArrayField(TEXT("houses"), Houses);
    Report->SetStringField(TEXT("meaning_of_pass"), TEXT("Inspection completed; each house carries its own safety result. Nothing selected or copied."));
    return Result(Report);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2VillageEditor::CopyVillageSelection(const TArray<FString>& Roots, const FString& Destination, bool bApply)
{
#if WITH_EDITOR
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("apply_requested"), bApply);
    Report->SetStringField(TEXT("selection_kind"), TEXT("STATIC_GEOMETRY_ONLY"));
    Report->SetArrayField(TEXT("roots"), Strings(Roots)); Report->SetStringField(TEXT("destination"), Destination);
    FVillageMountScope Mount; if (!Mount.Error.IsEmpty()) return Result(Report, Mount.Error);
    if (Roots.IsEmpty() || Roots.Num() > 64 || !Destination.StartsWith(VillageDestination)
        || Destination.Len() != VillageDestination.Len() + 12 || !FPackageName::IsValidLongPackageName(Destination))
        return Result(Report, TEXT("Require 1-64 explicit roots and a fresh Environment/Village_<12 hex> destination"));
    for (TCHAR Ch : Destination.Right(12)) if (!FChar::IsHexDigit(Ch)) return Result(Report, TEXT("Invalid destination digest"));
    TArray<FString> Packages, External; FString Error;
    if (!Closure(Roots, Packages, External, Error)) return Result(Report, Error);
    TMap<FString, FString> CopyMap;
    TArray<TSharedPtr<FJsonValue>> Entries;
    int64 SourceBytes = 0;
    for (const FString& SourcePackage : Packages)
    {
        const FString TargetPackage = Destination + TEXT("/") + SourcePackage.Mid(VillageMount.Len());
        if (FPackageName::DoesPackageExist(TargetPackage) || FindObject<UPackage>(nullptr, *TargetPackage))
            return Result(Report, TEXT("Destination already exists; no overwrite or partial-run reuse: ") + TargetPackage);
        FString Filename; FPackageName::DoesPackageExist(SourcePackage, &Filename);
        const int64 Size = IFileManager::Get().FileSize(*Filename);
        if (Size <= 0) return Result(Report, TEXT("Unreadable source package: ") + SourcePackage);
        SourceBytes += Size;
        auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("source"), SourcePackage);
        Entry->SetStringField(TEXT("destination"), TargetPackage); Entry->SetStringField(TEXT("source_file"), FPaths::ConvertRelativePathToFull(Filename));
        Entry->SetNumberField(TEXT("source_bytes"), static_cast<double>(Size)); Entries.Add(MakeShared<FJsonValueObject>(Entry));
        CopyMap.Add(SourcePackage, TargetPackage);
    }
    Report->SetArrayField(TEXT("packages"), Entries); Report->SetArrayField(TEXT("external_engine_script_dependencies"), Strings(External));
    Report->SetNumberField(TEXT("package_count"), Packages.Num()); Report->SetNumberField(TEXT("source_bytes"), static_cast<double>(SourceBytes));
    if (SourceBytes > MaxSourceBytes) return Result(Report, TEXT("Closure exceeds 1 GiB; review size before copying"));
    if (!bApply) return Result(Report);
    const bool bCopied = FAssetToolsModule::GetModule().Get().AdvancedCopyPackages(CopyMap, true, false, nullptr, EMessageSeverity::Error);
    Report->SetBoolField(TEXT("native_advanced_copy_returned"), bCopied);
    if (!bCopied) return Result(Report, TEXT("Native AdvancedCopyPackages failed; preserve partial output for diagnosis"));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanPathsSynchronous({Destination}, true);
    for (const auto& Pair : CopyMap)
    {
        if (!FPackageName::DoesPackageExist(Pair.Value)) return Result(Report, TEXT("Native copy did not save expected package: ") + Pair.Value);
        TArray<FName> Dependencies; Registry.GetDependencies(FName(*Pair.Value), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        for (FName Dependency : Dependencies)
        {
            const FString Name = Dependency.ToString();
            if (!Name.StartsWith(Destination + TEXT("/")) && !Name.StartsWith(TEXT("/Engine/")) && !Name.StartsWith(TEXT("/Script/")))
                return Result(Report, TEXT("Copied package retains unexpected dependency: ") + Pair.Value + TEXT(" -> ") + Name);
        }
    }
    Report->SetBoolField(TEXT("saved_packages_and_reference_remap_verified"), true);
    return Result(Report);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}
