#include "HCM5VS2FlightVisualComponent.h"
#include "HCM5VS2FlightComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

namespace
{
FString Encode(const TSharedRef<FJsonObject>& J)
{FString S;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S));return S;}
void Visible(UStaticMeshComponent* C,bool Show)
{if(C){C->SetVisibility(Show);C->SetHiddenInGame(!Show);}}
}

UHCM5VS2FlightVisualComponent::UHCM5VS2FlightVisualComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;}

UStaticMeshComponent* UHCM5VS2FlightVisualComponent::MakePiece(UStaticMesh* Shape,UMaterialInterface* Material,FName Name)
{
    auto* Part=NewObject<UStaticMeshComponent>(GetOwner(),Name);
    GetOwner()->AddInstanceComponent(Part);Part->SetMobility(EComponentMobility::Movable);
    Part->SetStaticMesh(Shape);Part->SetMaterial(0,Material);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->SetVisibility(false);Part->SetHiddenInGame(true);
    Part->SetOwnerNoSee(true);Part->RegisterComponent();return Part;
}

void UHCM5VS2FlightVisualComponent::BeginPlay()
{
    Super::BeginPlay();
    auto* Character=Cast<AHCM1Character>(GetOwner());Body=Character?Character->GetMesh():nullptr;
    Flight=Character?Character->GetFlightComponent():nullptr;
    if(!Body||!Flight||!Flight->IsFlightAvailable()||Body->GetBoneIndex(TEXT("Chest"))==INDEX_NONE
        || !FeatherMesh||!RibbonMesh||!RuneMesh||!LightMaterial)
    {SetComponentTickEnabled(false);return;}
    WingGlow=UMaterialInstanceDynamic::Create(WingMaterial?WingMaterial.Get():LightMaterial.Get(),this);
    bVS3LayeredWings=GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/"));
    if(bVS3LayeredWings){
        if(auto* Shape=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/HarborCity/M5VS3/Combat/SM_SilkFeather.SM_SilkFeather")))FeatherMesh=Shape;
        if(auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HarborCity/M5VS3/Combat/M_WingSilk.M_WingSilk")))WingGlow=UMaterialInstanceDynamic::Create(Surface,this);
    }
    WindGlow=UMaterialInstanceDynamic::Create(LightMaterial,this);
    RuneGlow=UMaterialInstanceDynamic::Create(LightMaterial,this);
    for(int32 I=0;I<(bVS3LayeredWings?84:20);++I)Feathers.Add(MakePiece(FeatherMesh,WingGlow,*FString::Printf(TEXT("VS2LightFeather%02d"),I)));
    for(int32 I=0;I<8;++I)Ribbons.Add(MakePiece(RibbonMesh,WindGlow,*FString::Printf(TEXT("VS2WindRibbon%02d"),I)));
    Rune=MakePiece(RuneMesh,RuneGlow,TEXT("VS2TakeoffRune"));
    AddTickPrerequisiteComponent(Body);AddTickPrerequisiteComponent(Flight);
}

void UHCM5VS2FlightVisualComponent::StartRune()
{
    auto* Character=Cast<AHCM1Character>(GetOwner());
    if(!Character||!Flight||!Rune)return;
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(VS2TakeoffRune),false,Character);
    const FVector Origin=Character->GetActorLocation();
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin-FVector(0,0,400),ECC_Visibility,Params))return;
    const auto* Bounds=Flight->GetFlightBounds();
    if(!Bounds||Hit.ImpactPoint.Z<=Bounds->SeaLevelZ+1||Hit.ImpactNormal.Z<.5f)return;
    RuneTransform=FTransform(FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat(),Hit.ImpactPoint+Hit.ImpactNormal*2.5f,FVector(.7f));
    RuneAge=0;
}

void UHCM5VS2FlightVisualComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime,TickType,ThisTick);
    if(!Body||!Flight||Feathers.Num()!=(bVS3LayeredWings?84:20)||Ribbons.Num()!=8||!Rune||!FMath::IsFinite(DeltaTime)||DeltaTime<=0)return;
    const auto* Character=Cast<AHCM1Character>(GetOwner());
    const auto* PC=Character?Cast<AHCM1PlayerController>(Character->GetController()):nullptr;
    // Explicit PC gate also covers FP full-body meshes that remain owner-visible.
    bFirstPersonHidden=!PC||PC->IsFirstPersonPerspective()||PC->GetPlayerMode()!=EHCPlayerMode::OnFoot;
    const bool IsFlying=Flight->IsFlying();
    if(IsFlying&&!bWasFlying)StartRune();
    bWasFlying=IsFlying;
    if(PC&&PC->IsPauseMenuOpen())return;
    VisualSeconds=FMath::Fmod(VisualSeconds+DeltaTime,60.f);RuneAge+=DeltaTime;
    WingAlpha=FMath::FInterpTo(WingAlpha,IsFlying?1.f:0.f,DeltaTime,IsFlying?5.f:9.f);
    const float Speed=Character?Character->GetVelocity().Size():0;
    WindAlpha=FMath::FInterpTo(WindAlpha,IsFlying&&Flight->IsBoosting()?FMath::Clamp((Speed-800.f)/1700.f,0.f,1.f):0.f,DeltaTime,6.f);
    const bool CanShow=!bFirstPersonHidden&&!GetOwner()->IsHidden()&&Body->IsVisible()&&!Body->bHiddenInGame;
    WingGlow->SetScalarParameterValue(TEXT("GlowGain"),Flight->IsBoosting()?3.5f:2.3f);
    WingGlow->SetScalarParameterValue(TEXT("OpacityGain"),WingAlpha*.82f);
    WindGlow->SetScalarParameterValue(TEXT("GlowGain"),1.8f);WindGlow->SetScalarParameterValue(TEXT("OpacityGain"),WindAlpha*.45f);
    const FVector Anchor=Body->GetSocketLocation(TEXT("Chest"));
    const FQuat ActorYaw(FRotator(0,GetOwner()->GetActorRotation().Yaw,0));
    const FQuat Bank(FVector::ForwardVector,FMath::DegreesToRadians(Flight->VisualBankDegrees));
    const FQuat Frame=ActorYaw*Bank;
    const float Size=FMath::Clamp(float(Body->GetComponentScale().GetAbsMax())/1.25f,.5f,2.f);
    const FName State=Flight->GetFlightPresentationState();
    const bool TakingOff=State==TEXT("Takeoff"),Boost=Flight->IsBoosting();
    const float FlapAmplitude=TakingOff?22.f:Boost?3.f:10.f;
    const float Flap=FMath::Sin(VisualSeconds*(TakingOff?8.f:4.2f))*FlapAmplitude;
    VisibleFeathers=0;VisibleRibbons=0;
    if(bVS3LayeredWings)for(int32 SideIndex=0;SideIndex<2;++SideIndex){
        const float Side=SideIndex==0?-1.f:1.f;
        for(int32 Row=0;Row<3;++Row)for(int32 I=0;I<14;++I){
            const float T=I/13.f;const float Breadth=WingStyle==1?1.16f:WingStyle==2?.85f:1.f;
            // A shoulder, elbow and wrist curve with three overlapping feather rows.
            FVector Base(-19.f-Row*3,Side*(16+T*88*Breadth),18+48*FMath::Sin(T*PI*.8f)-Row*9);
            const float Length=(Row==0?70+45*T:Row==1?36+25*T:18+13*T)*(WingStyle==2?1.2f:1.f);
            FVector Axis(-(Boost?.85f:.22f)-T*.2f,Side*(.35f+.9f*T),(WingStyle==1?.35f:.05f)-.9f*T);
            Axis.Normalize();FVector Tip=Base+Axis*Length;
            if(WingStyle==2)Tip.Z+=28*FMath::Square(T);
            const FQuat Flapping(FVector::ForwardVector,FMath::DegreesToRadians(Side*(Flap+2*FMath::Sin(VisualSeconds*3-T*2))));
            Base=Flapping.RotateVector(Base);Tip=Flapping.RotateVector(Tip);
            const FVector Delta=Tip-Base;const auto Rotation=FRotationMatrix::MakeFromXZ(Frame.RotateVector(Delta.GetSafeNormal()),Frame.RotateVector(FVector::BackwardVector)).ToQuat();
            auto* Feather=Feathers[SideIndex*42+Row*14+I].Get();const float Spread=FMath::Max(.02f,WingAlpha);
            Feather->SetWorldTransform(FTransform(Rotation,Anchor+Frame.RotateVector(Base)*Size,FVector(Delta.Size()/100*Size*Spread,(Row==0?1.65f:Row==1?1.25f:.8f)*Size*Spread,Size)));
            Feather->SetOwnerNoSee(false);const bool Show=CanShow&&WingAlpha>.01f;Visible(Feather,Show);VisibleFeathers+=Show?1:0;
        }
    }
    else for(int32 SideIndex=0;SideIndex<2;++SideIndex)
    {
        const float Side=SideIndex==0?-1.f:1.f;
        for(int32 I=0;I<10;++I)
        {
            const float T=I/9.f;
            const FVector Base(-20.f,Side*(14.f+18.f*T),15.f+8.f*FMath::Sin(T*PI));
            FVector Tip(-30.f-(Boost?75.f:8.f)*T,Side*(54.f+88.f*T),115.f-122.f*T);
            const FQuat FlapRotation(FVector::ForwardVector,FMath::DegreesToRadians(Side*(Flap+3.f*FMath::Sin(VisualSeconds*3.f+I*.3f))));
            Tip=Base+FlapRotation.RotateVector(Tip-Base);
            const FVector Delta=Tip-Base,Direction=Frame.RotateVector(Delta.GetSafeNormal());
            const FQuat Rotation=FRotationMatrix::MakeFromXZ(Direction,Frame.RotateVector(FVector::BackwardVector)).ToQuat();
            const float Spread=FMath::Clamp(WingAlpha,.02f,1.f);
            auto* Feather=Feathers[SideIndex*10+I].Get();
            Feather->SetWorldTransform(FTransform(Rotation,Anchor+Frame.RotateVector(Base)*Size,
                FVector(Delta.Size()/100.f*Size*Spread,Size*(1.f-.15f*T)*Spread,Size)));
            Feather->SetOwnerNoSee(false);const bool Show=CanShow&&WingAlpha>.01f;
            Visible(Feather,Show);VisibleFeathers+=Show?1:0;
        }
    }
    const FVector Velocity=Character?Character->GetVelocity():FVector::ZeroVector;
    const FVector Back=Velocity.IsNearlyZero()?-ActorYaw.GetForwardVector():-Velocity.GetSafeNormal();
    const FQuat WindFrame=FRotationMatrix::MakeFromX(Back).ToQuat();
    for(int32 I=0;I<8;++I)
    {
        const float Cycle=FMath::Fmod(VisualSeconds*2.2f+I*.137f,1.f),Angle=I*(2.f*PI/8.f);
        const FVector Radial=WindFrame.RotateVector(FVector(0,FMath::Cos(Angle),FMath::Sin(Angle)))*(50.f+8.f*(I%3))*Size;
        const FVector Position=Anchor+Radial+Back*((Cycle-.2f)*200.f*Size);
        auto* Ribbon=Ribbons[I].Get();Ribbon->SetWorldTransform(FTransform(WindFrame,Position,FVector((.7f+Cycle)*Size,.6f*Size,.6f*Size)));
        Ribbon->SetOwnerNoSee(false);const bool Show=CanShow&&WindAlpha>.02f;Visible(Ribbon,Show);VisibleRibbons+=Show?1:0;
    }
    const float RuneAlpha=RuneAge<1.2f?FMath::Sin(FMath::Clamp(RuneAge/1.2f,0.f,1.f)*PI):0.f;
    FTransform RuneNow=RuneTransform;RuneNow.SetScale3D(FVector(.7f+FMath::Clamp(RuneAge,0.f,1.2f)*.35f));
    Rune->SetWorldTransform(RuneNow);Rune->SetOwnerNoSee(false);
    RuneGlow->SetScalarParameterValue(TEXT("GlowGain"),2.2f);RuneGlow->SetScalarParameterValue(TEXT("OpacityGain"),RuneAlpha*.8f);
    Visible(Rune,CanShow&&RuneAlpha>.01f);
}

void UHCM5VS2FlightVisualComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    for(const auto& C:Feathers)if(C)C->DestroyComponent();for(const auto& C:Ribbons)if(C)C->DestroyComponent();
    if(Rune)Rune->DestroyComponent();Feathers.Reset();Ribbons.Reset();Rune=nullptr;
    WingGlow=WindGlow=RuneGlow=nullptr;Body=nullptr;Flight=nullptr;Super::EndPlay(Reason);
}

FString UHCM5VS2FlightVisualComponent::GetFlightVisualDiagnostics() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("allocated_feathers"),Feathers.Num());J->SetNumberField(TEXT("allocated_wind_ribbons"),Ribbons.Num());
    J->SetNumberField(TEXT("visible_feathers"),VisibleFeathers);J->SetNumberField(TEXT("visible_wind_ribbons"),VisibleRibbons);
    J->SetNumberField(TEXT("allocated_runes"),Rune?1:0);J->SetBoolField(TEXT("rune_visible"),Rune&&Rune->IsVisible());
    J->SetBoolField(TEXT("first_person_hidden"),bFirstPersonHidden);J->SetNumberField(TEXT("wing_alpha"),WingAlpha);J->SetNumberField(TEXT("wind_alpha"),WindAlpha);
    J->SetStringField(TEXT("feather_mesh"),GetPathNameSafe(FeatherMesh));J->SetStringField(TEXT("wind_mesh"),GetPathNameSafe(RibbonMesh));
    bool NoCollision=true;for(const auto& C:Feathers)NoCollision&=C&&C->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
    for(const auto& C:Ribbons)NoCollision&=C&&C->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
    J->SetBoolField(TEXT("no_collision"),NoCollision&&(!Rune||Rune->GetCollisionEnabled()==ECollisionEnabled::NoCollision));
    J->SetStringField(TEXT("scope"),TEXT("Original light feathers / bounded ribbon pool / ground ray anchored takeoff rune; no camera or movement writes"));return Encode(J);
}

FString UHCM5VS2FlightVisualEditor::ConfigureFlightVisual(UBlueprint* BP,UStaticMesh* Feather,UStaticMesh* Ribbon,UStaticMesh* Rune,UMaterialInterface* Material)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));J->SetBoolField(TEXT("saved_by_helper"),false);
#if WITH_EDITOR
    if(!BP||!BP->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_"))||!BP->GeneratedClass
        ||!BP->GeneratedClass->IsChildOf(AHCM1Character::StaticClass())||!BP->SimpleConstructionScript||!Feather||!Ribbon||!Rune||!Material)return Encode(J);
    for(const UObject* O:{static_cast<UObject*>(Feather),static_cast<UObject*>(Ribbon),static_cast<UObject*>(Rune),static_cast<UObject*>(Material)})
        if(!O->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS2/FlightVisual/Batch_")))return Encode(J);
    for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->ComponentClass==UHCM5VS2FlightVisualComponent::StaticClass())return Encode(J);
    BP->Modify();BP->SimpleConstructionScript->Modify();
    auto* Node=BP->SimpleConstructionScript->CreateNode(UHCM5VS2FlightVisualComponent::StaticClass(),TEXT("VS2FlightLightWings"));
    auto* C=Node?Cast<UHCM5VS2FlightVisualComponent>(Node->ComponentTemplate):nullptr;if(!C)return Encode(J);
    BP->SimpleConstructionScript->AddNode(Node);C->FeatherMesh=Feather;C->RibbonMesh=Ribbon;C->RuneMesh=Rune;C->LightMaterial=Material;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error)return Encode(J);BP->MarkPackageDirty();J->SetStringField(TEXT("status"),TEXT("PASS"));
    J->SetNumberField(TEXT("maximum_render_components"),29);J->SetStringField(TEXT("runtime"),TEXT("NOT_RUN"));
#endif
    return Encode(J);
}
