#include "HCM5VS2HeroFillEditor.h"

#if WITH_EDITOR
#include "M1/HCM1Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Scene.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Internationalization/Regex.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
FString Finish(const TSharedPtr<FJsonObject>& Report, const FString& Error = FString())
{
    Report->SetStringField(TEXT("status"), Error.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    if (!Error.IsEmpty()) Report->SetStringField(TEXT("error"), Error);
    Report->SetBoolField(TEXT("saved_by_helper"), false);
    Report->SetStringField(TEXT("runtime_visual"), TEXT("NOT_RUN"));
    FString Result;
    FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}

TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V)
{
    return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)};
}

TArray<TSharedPtr<FJsonValue>> Channels(const FLightingChannels& C)
{
    return {MakeShared<FJsonValueBoolean>(C.bChannel0 != 0), MakeShared<FJsonValueBoolean>(C.bChannel1 != 0), MakeShared<FJsonValueBoolean>(C.bChannel2 != 0)};
}
}
#endif

FString UHCM5VS2HeroFillEditor::ConfigureHeroFill(UBlueprint* Blueprint, float Candela)
{
#if WITH_EDITOR
    auto Report = MakeShared<FJsonObject>();
    if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->SimpleConstructionScript
        || !Blueprint->GeneratedClass->IsChildOf(AHCM1Character::StaticClass()))
        return Finish(Report, TEXT("Compiled isolated native Hero Blueprint required"));
    const FString Package = Blueprint->GetOutermost()->GetName();
    const FRegexPattern Pattern(TEXT("^/Game/HarborCity/M5VS2/HeroFill/Review_([0-9a-f]{12})/BP_SelestiaFill_([0-9a-f]{12})_(Zero|Quarter|One)$"));
    FRegexMatcher Match(Pattern, Package);
    if (!Match.FindNext() || Match.GetCaptureGroup(1) != Match.GetCaptureGroup(2))
        return Finish(Report, TEXT("Only exact unique HeroFill review Blueprint paths are writable"));
    const FString Tag = Match.GetCaptureGroup(3);
    const float Expected = Tag == TEXT("Zero") ? 0.f : Tag == TEXT("Quarter") ? .25f : 1.f;
    if (!FMath::IsFinite(Candela) || Candela != Expected)
        return Finish(Report, TEXT("Candidate tag must match exactly 0, 0.25 or 1 candela"));
    AHCM1Character* CDO = Cast<AHCM1Character>(Blueprint->GeneratedClass->GetDefaultObject());
    USkeletalMeshComponent* Body = CDO ? CDO->GetMesh() : nullptr;
    UCapsuleComponent* Capsule = CDO ? CDO->GetCapsuleComponent() : nullptr;
    USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
    if (!Mesh || !Capsule || Body->GetAttachParent() != Capsule
        || Mesh->GetOutermost()->GetName() != TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia")
        || !CDO->GetActorScale3D().Equals(FVector::OneVector, .0001)
        || !Capsule->GetRelativeScale3D().Equals(FVector::OneVector, .0001)
        || Body->GetRelativeTransform().ContainsNaN())
        return Finish(Report, TEXT("Exact normalized Selestia mesh attached to unit-scale native capsule required"));
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    const int32 Head = Ref.FindBoneIndex(TEXT("Head"));
    if (Head == INDEX_NONE) return Finish(Report, TEXT("Actual Selestia Head reference bone missing"));
    TArray<FTransform> CS; CS.SetNum(Ref.GetNum());
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        const int32 Parent = Ref.GetParentIndex(I);
        if (Parent >= I || Parent < INDEX_NONE || Ref.GetRefBonePose()[I].ContainsNaN())
            return Finish(Report, TEXT("Invalid native reference hierarchy"));
        CS[I] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * CS[Parent];
    }
    const FVector HeadCapsule = Body->GetRelativeTransform().TransformPosition(CS[Head].GetLocation());
    if (HeadCapsule.ContainsNaN() || !FMath::IsFinite(HeadCapsule.Z) || FMath::Abs(HeadCapsule.Z) > 300.)
        return Finish(Report, TEXT("Reference Head is outside bounded centimeter character scope"));
    const FVector Offset(90., 25., HeadCapsule.Z);
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    for (USCS_Node* Node : SCS->GetAllNodes())
        if (Node->GetVariableName() == TEXT("M5VS2HeroFill") || Node->ComponentClass->IsChildOf(UPointLightComponent::StaticClass()))
            return Finish(Report, TEXT("Fresh copied Hero must have no point light or fill name collision"));

    Blueprint->Modify(); SCS->Modify();
    USCS_Node* Node = SCS->CreateNode(UPointLightComponent::StaticClass(), TEXT("M5VS2HeroFill"));
    UPointLightComponent* Light = Node ? Cast<UPointLightComponent>(Node->ComponentTemplate) : nullptr;
    if (!Light) return Finish(Report, TEXT("Native SCS point-light creation failed"));
    SCS->AddNode(Node);
    Node->SetParent(Capsule);
    Light->Modify();
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetRelativeLocation(Offset);
    Light->SetLightColor(FLinearColor::White, false);
    Light->SetUseInverseSquaredFalloff(true);
    Light->SetIntensityUnits(ELightUnits::Candelas);
    Light->SetIntensity(Candela);
    Light->SetAttenuationRadius(200.f);
    Light->SetSourceRadius(20.f);
    Light->SetSoftSourceRadius(20.f);
    Light->SetInverseExposureBlend(0.f);
    Light->SetCastShadows(false);
    Light->SetSpecularScale(0.f);
    Light->SetIndirectLightingIntensity(0.f);
    Light->SetVolumetricScatteringIntensity(0.f);
    Light->SetAffectGlobalIllumination(false);
    Light->SetAffectReflection(false);
    Light->SetLightingChannels(false, true, false);
    Light->SetVisibility(true);
    Light->SetHiddenInGame(false);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return Finish(Report, TEXT("Candidate native Blueprint compilation failed"));
    // Compilation can replace the generated CDO. Change only the new CDO's native mesh.
    CDO = Cast<AHCM1Character>(Blueprint->GeneratedClass->GetDefaultObject());
    Body = CDO ? CDO->GetMesh() : nullptr;
    Capsule = CDO ? CDO->GetCapsuleComponent() : nullptr;
    Light = Cast<UPointLightComponent>(Node->ComponentTemplate);
    if (!Body || !Capsule || !Light) return Finish(Report, TEXT("Compiled candidate component readback failed"));
    Body->Modify(); Body->SetLightingChannels(true, true, false);
    const bool Valid = Node->bIsParentComponentNative && Node->ParentComponentOrVariableName == Capsule->GetFName()
        && Light->GetRelativeLocation().Equals(Offset, .0001) && Light->GetMobility() == EComponentMobility::Movable
        && Light->IntensityUnits == ELightUnits::Candelas && Light->Intensity == Expected
        && Light->bUseInverseSquaredFalloff && Light->AttenuationRadius == 200.f
        && Light->SourceRadius == 20.f && Light->SoftSourceRadius == 20.f && Light->InverseExposureBlend == 0.f
        && !Light->CastShadows && Light->SpecularScale == 0.f && Light->IndirectLightingIntensity == 0.f
        && Light->VolumetricScatteringIntensity == 0.f && !Light->bAffectGlobalIllumination && !Light->bAffectReflection
        && Light->GetLightColor().Equals(FLinearColor::White) && Light->IsVisible()
        && !Light->LightingChannels.bChannel0 && Light->LightingChannels.bChannel1 && !Light->LightingChannels.bChannel2
        && Body->LightingChannels.bChannel0 && Body->LightingChannels.bChannel1 && !Body->LightingChannels.bChannel2;
    Report->SetStringField(TEXT("blueprint"), Package);
    Report->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Report->SetStringField(TEXT("head_bone"), TEXT("Head"));
    Report->SetArrayField(TEXT("head_reference_component_cm"), XYZ(CS[Head].GetLocation()));
    Report->SetArrayField(TEXT("head_reference_capsule_cm"), XYZ(HeadCapsule));
    Report->SetStringField(TEXT("parent_native_component"), Node->ParentComponentOrVariableName.ToString());
    Report->SetStringField(TEXT("light_component_template"), Light->GetPathName());
    Report->SetArrayField(TEXT("light_capsule_relative_cm"), XYZ(Light->GetRelativeLocation()));
    Report->SetArrayField(TEXT("light_channels"), Channels(Light->LightingChannels));
    Report->SetArrayField(TEXT("mesh_channels"), Channels(Body->LightingChannels));
    Report->SetNumberField(TEXT("intensity_cd"), Light->Intensity);
    Report->SetNumberField(TEXT("attenuation_radius_cm"), Light->AttenuationRadius);
    Report->SetNumberField(TEXT("source_radius_cm"), Light->SourceRadius);
    Report->SetBoolField(TEXT("compiled_template_readback_valid"), Valid);
    Report->SetStringField(TEXT("scope"), TEXT("Permanent capsule-attached candidate light; original Hero, material assets, gameplay cameras and runtime-created FP meshes are untouched"));
    if (!Valid) return Finish(Report, TEXT("Compiled point light or character channel readback mismatch"));
    Blueprint->MarkPackageDirty();
    return Finish(Report);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}
