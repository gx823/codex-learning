#include "HCM5VS2HeroOutlineComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Materials/MaterialInterface.h"
#if WITH_EDITOR
#include "M1/HCM1Character.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#endif

namespace
{
FString ToJSON(const TSharedPtr<FJsonObject>& J)
{
    FString S; FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&S)); return S;
}
}

UHCM5VS2HeroOutlineComponent::UHCM5VS2HeroOutlineComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UHCM5VS2HeroOutlineComponent::BeginPlay()
{
    Super::BeginPlay();
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    Source = Character ? Character->GetMesh() : nullptr;
    if (!Source || !Source->GetSkeletalMeshAsset() || OutlineMaterials.Num() != Source->GetNumMaterials()
        || OutlineMaterials.Contains(nullptr))
    { SetComponentTickEnabled(false); return; }
    Outline = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("VS2SourceOutlinePass"));
    GetOwner()->AddInstanceComponent(Outline);
    Outline->SetupAttachment(Source);
    Outline->SetRelativeTransform(FTransform::Identity);
    Outline->SetSkeletalMeshAsset(Source->GetSkeletalMeshAsset());
    for (int32 I = 0; I < OutlineMaterials.Num(); ++I) Outline->SetMaterial(I, OutlineMaterials[I]);
    Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Outline->SetGenerateOverlapEvents(false);
    Outline->SetCanEverAffectNavigation(false);
    Outline->SetCastShadow(false);
    Outline->bUseAttachParentBound = true;
    // UE5.8 UpdateFollowerComponent copies both animation and explicit morph curves from the leader.
    // No CopyPose graph, second Kawaii simulation, independent blink or skeleton evaluation.
    Outline->SetLeaderPoseComponent(Source, true, false);
    Outline->RegisterComponent();
    AddTickPrerequisiteComponent(Source);
}

void UHCM5VS2HeroOutlineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    if (!Source || !Outline) return;
    const bool Visible = Source->IsVisible() && !Source->bHiddenInGame && !GetOwner()->IsHidden();
    Outline->SetVisibility(Visible);
    Outline->SetHiddenInGame(!Visible);
    Outline->SetOwnerNoSee(Source->bOwnerNoSee);
    Outline->SetOnlyOwnerSee(Source->bOnlyOwnerSee);
}

void UHCM5VS2HeroOutlineComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Outline) { Outline->DestroyComponent(); Outline = nullptr; }
    Source = nullptr;
    Super::EndPlay(Reason);
}

FString UHCM5VS2HeroOutlineComponent::GetOutlineDiagnostics() const
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("source"), GetPathNameSafe(Source));
    J->SetStringField(TEXT("outline"), GetPathNameSafe(Outline));
    J->SetBoolField(TEXT("same_mesh"), Source && Outline && Source->GetSkeletalMeshAsset() == Outline->GetSkeletalMeshAsset());
    J->SetBoolField(TEXT("leader_is_source"), Outline && Outline->LeaderPoseComponent.Get() == Source);
    J->SetBoolField(TEXT("collision_disabled"), Outline && Outline->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    J->SetBoolField(TEXT("visible"), Outline && Outline->IsVisible() && !Outline->bHiddenInGame);
    J->SetNumberField(TEXT("source_active_morph_count"), Source ? Source->ActiveMorphTargets.Num() : 0);
    J->SetNumberField(TEXT("outline_active_morph_count"), Outline ? Outline->ActiveMorphTargets.Num() : 0);
    TArray<TSharedPtr<FJsonValue>> Slots;
    if (Outline) for (int32 I = 0; I < Outline->GetNumMaterials(); ++I)
        Slots.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Outline->GetMaterial(I))));
    J->SetArrayField(TEXT("materials"), Slots);
    return ToJSON(J);
}

FString UHCM5VS2HeroOutlineEditor::ConfigureOutline(UBlueprint* Blueprint, const TArray<UMaterialInterface*>& Materials)
{
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("status"), TEXT("FAIL"));
    J->SetBoolField(TEXT("saved_by_helper"), false);
#if WITH_EDITOR
    if (!Blueprint || !Blueprint->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_"))
        || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AHCM1Character::StaticClass())
        || !Blueprint->SimpleConstructionScript || Materials.Num() != 5 || Materials.Contains(nullptr))
    { J->SetStringField(TEXT("error"), TEXT("Fresh VS2 HeroRev2 Blueprint and five authored outline materials required")); return ToJSON(J); }
    for (const auto* Material : Materials)
        if (!Material->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_")))
        { J->SetStringField(TEXT("error"), TEXT("Outline material outside new review namespace")); return ToJSON(J); }
    for (USCS_Node* Existing : Blueprint->SimpleConstructionScript->GetAllNodes())
        if (Existing->ComponentClass == UHCM5VS2HeroOutlineComponent::StaticClass() || Existing->GetVariableName() == TEXT("VS2HeroOutline"))
        { J->SetStringField(TEXT("error"), TEXT("Existing outline component; refuse duplicate")); return ToJSON(J); }
    Blueprint->Modify(); Blueprint->SimpleConstructionScript->Modify();
    USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(UHCM5VS2HeroOutlineComponent::StaticClass(), TEXT("VS2HeroOutline"));
    auto* Component = Node ? Cast<UHCM5VS2HeroOutlineComponent>(Node->ComponentTemplate) : nullptr;
    if (!Component) return ToJSON(J);
    Blueprint->SimpleConstructionScript->AddNode(Node);
    for (UMaterialInterface* Material : Materials) Component->OutlineMaterials.Add(Material);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) { J->SetStringField(TEXT("error"), TEXT("Blueprint compile failed")); return ToJSON(J); }
    Component = Cast<UHCM5VS2HeroOutlineComponent>(Node->ComponentTemplate);
    if (!Component || Component->OutlineMaterials.Num() != 5) return ToJSON(J);
    Blueprint->MarkPackageDirty();
    J->SetStringField(TEXT("status"), TEXT("PASS"));
    J->SetStringField(TEXT("component"), Component->GetPathName());
    J->SetStringField(TEXT("runtime_visual"), TEXT("NOT_RUN"));
#else
    J->SetStringField(TEXT("error"), TEXT("Editor only"));
#endif
    return ToJSON(J);
}
