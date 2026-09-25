#include "HCM5VS2HaloComponent.h"
#include "HCM5VS2FlightComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "M1/HCM1Character.h"
#include "M4R2/HCM4R2PresentationComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

namespace { FString JSON(const TSharedPtr<FJsonObject>& J) { FString S;FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S; } }
UHCM5VS2HaloComponent::UHCM5VS2HaloComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork; }
void UHCM5VS2HaloComponent::BeginPlay()
{
    Super::BeginPlay();
    const ACharacter* C=Cast<ACharacter>(GetOwner());Body=C?C->GetMesh():nullptr;
    if (!Body || Body->GetBoneIndex(TEXT("Head"))==INDEX_NONE || Pieces.Num()!=7 || SourceDegreesPerSecond.Num()!=7 || Pieces.Contains(nullptr) || !GlowMaterial)
    {SetComponentTickEnabled(false);return;}
    Glow=UMaterialInstanceDynamic::Create(GlowMaterial,this);
    for(int32 I=0;I<Pieces.Num();++I)
    {
        auto* Part=NewObject<UStaticMeshComponent>(GetOwner(),*FString::Printf(TEXT("VS2HaloPart%d"),I));
        GetOwner()->AddInstanceComponent(Part);Part->SetStaticMesh(Pieces[I]);Part->SetMaterial(0,Glow);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->RegisterComponent();
        RenderPieces.Add(Part);
    }
    AddTickPrerequisiteComponent(Body);
}
void UHCM5VS2HaloComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime,TickType,ThisTick);
    if(!Body || RenderPieces.Num()!=7 || !FMath::IsFinite(DeltaTime) || DeltaTime<=0)return;
    AnimationSeconds=FMath::Fmod(AnimationSeconds+DeltaTime,60.0);
    FTransform Anchor=SourceHeadTransform*Body->GetSocketTransform(TEXT("Head"));
    const FVector Target=Anchor.GetLocation();
    // Bounded secondary follow replaces the source Unity PhysBone spring; this is a UE adaptation,
    // not an invented source bob animation. Reset on teleport or long frame; never drag a halo trail.
    if(!bAnchorInitialized || FVector::DistSquared(SmoothedAnchor,Target)>40000 || DeltaTime>.15f)
    {SmoothedAnchor=Target;bAnchorInitialized=true;}
    else SmoothedAnchor=FMath::Lerp(SmoothedAnchor,Target,1.f-FMath::Exp(-11.f*DeltaTime));
    SmoothedAnchor=Target+(SmoothedAnchor-Target).GetClampedToMaxSize(2.5f);
    Anchor.SetLocation(SmoothedAnchor);
    const auto* Flight=GetOwner()->FindComponentByClass<UHCM5VS2FlightComponent>();
    Brightness=FMath::FInterpTo(Brightness,Flight && Flight->IsFlying()?1.5f:1.f,DeltaTime,5.f);
    Glow->SetScalarParameterValue(TEXT("FlightGlow"),Brightness);
    const bool Visible=Body->IsVisible() && !Body->bHiddenInGame && !GetOwner()->IsHidden();
    for(int32 I=0;I<RenderPieces.Num();++I)
    {
        auto* Part=RenderPieces[I].Get();
        const FTransform Spin(FRotator(0,float(AnimationSeconds)*SourceDegreesPerSecond[I],0));
        Part->SetWorldTransform(Spin*Anchor);
        Part->SetVisibility(Visible);Part->SetHiddenInGame(!Visible);
        Part->SetOwnerNoSee(Body->bOwnerNoSee);Part->SetOnlyOwnerSee(Body->bOnlyOwnerSee);
    }
}
void UHCM5VS2HaloComponent::EndPlay(const EEndPlayReason::Type Reason)
{for(const auto& Part:RenderPieces)if(Part)Part->DestroyComponent();RenderPieces.Reset();Glow=nullptr;Body=nullptr;Super::EndPlay(Reason);}
FString UHCM5VS2HaloComponent::GetHaloDiagnostics() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("piece_count"),RenderPieces.Num());
    J->SetNumberField(TEXT("seconds"),AnimationSeconds);J->SetNumberField(TEXT("glow_gain"),Brightness);
    J->SetStringField(TEXT("float_scope"),TEXT("UE bounded 2.5 cm exponential follow; source PhysBone aesthetic adaptation"));
    J->SetStringField(TEXT("anchor"),SmoothedAnchor.ToString());
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 I=0;I<RenderPieces.Num();++I)
    { auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("mesh"),GetPathNameSafe(RenderPieces[I]->GetStaticMesh()));R->SetNumberField(TEXT("degrees_per_second"),SourceDegreesPerSecond[I]);R->SetStringField(TEXT("world_transform"),RenderPieces[I]->GetComponentTransform().ToString());R->SetBoolField(TEXT("owner_no_see"),RenderPieces[I]->bOwnerNoSee);Rows.Add(MakeShared<FJsonValueObject>(R)); }
    J->SetArrayField(TEXT("pieces"),Rows);return JSON(J);
}
FString UHCM5VS2HaloEditor::ConfigureHalo(UBlueprint* BP,const TArray<UStaticMesh*>& Meshes,const TArray<float>& Rates,UMaterialInterface* Material,const FTransform& Transform)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));J->SetBoolField(TEXT("saved_by_helper"),false);
#if WITH_EDITOR
    if(!BP || !BP->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_")) || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(AHCM1Character::StaticClass()) || !BP->SimpleConstructionScript || Meshes.Num()!=7 || Rates.Num()!=7 || Meshes.Contains(nullptr) || !Material || Transform.ContainsNaN())return JSON(J);
    for(auto* M:Meshes)if(!M->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HaloRev2/Batch_")))return JSON(J);
    for(float R:Rates)if(!FMath::IsFinite(R) || FMath::Abs(R)>12.001f)return JSON(J);
    for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->ComponentClass==UHCM5VS2HaloComponent::StaticClass())return JSON(J);
    BP->Modify();BP->SimpleConstructionScript->Modify();
    auto* Node=BP->SimpleConstructionScript->CreateNode(UHCM5VS2HaloComponent::StaticClass(),TEXT("VS2OriginalHaloMotion"));
    auto* C=Node?Cast<UHCM5VS2HaloComponent>(Node->ComponentTemplate):nullptr;if(!C)return JSON(J);
    BP->SimpleConstructionScript->AddNode(Node);
    for(auto* M:Meshes)C->Pieces.Add(M);C->SourceDegreesPerSecond=Rates;C->GlowMaterial=Material;C->SourceHeadTransform=Transform;
    auto* Hero=Cast<AHCM1Character>(BP->GeneratedClass->GetDefaultObject());
    auto* Old=Hero?Hero->FindComponentByClass<UHCM4R2PresentationComponent>():nullptr;
    if(!Old)return JSON(J);Old->Modify();Old->HeadAccessoryMesh.Reset();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error)return JSON(J);
    Hero=Cast<AHCM1Character>(BP->GeneratedClass->GetDefaultObject());Old=Hero?Hero->FindComponentByClass<UHCM4R2PresentationComponent>():nullptr;
    if(!Old || !Old->HeadAccessoryMesh.IsNull())return JSON(J);
    BP->MarkPackageDirty();J->SetStringField(TEXT("status"),TEXT("PASS"));J->SetStringField(TEXT("runtime"),TEXT("NOT_RUN"));
#endif
    return JSON(J);
}
