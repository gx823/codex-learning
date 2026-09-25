#include "HCM5VS2CornerTimeDirector.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PostProcessVolume.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
template<class T> T* FindUnique(UWorld* World, const FName Tag)
{
    T* Result = nullptr;
    for (TActorIterator<T> It(World); It; ++It) if (It->ActorHasTag(Tag))
    { if (Result) return nullptr; Result = *It; }
    return Result;
}
TArray<TSharedPtr<FJsonValue>> RGBA(const FLinearColor& C)
{ return {MakeShared<FJsonValueNumber>(C.R),MakeShared<FJsonValueNumber>(C.G),MakeShared<FJsonValueNumber>(C.B),MakeShared<FJsonValueNumber>(C.A)}; }
TArray<TSharedPtr<FJsonValue>> Angles(const FRotator& R)
{ return {MakeShared<FJsonValueNumber>(R.Pitch),MakeShared<FJsonValueNumber>(R.Yaw),MakeShared<FJsonValueNumber>(R.Roll)}; }
}

AHCM5VS2CornerTimeDirector::AHCM5VS2CornerTimeDirector() { PrimaryActorTick.bCanEverTick = false; }
void AHCM5VS2CornerTimeDirector::BeginPlay()
{ Super::BeginPlay(); if (BindWorld()) SetTimeOfDay(InitialPeriod); }

bool AHCM5VS2CornerTimeDirector::BindWorld()
{
    if (bBound) return true;
    Sun = FindUnique<ADirectionalLight>(GetWorld(),TEXT("HC_VS2_Rev2_Sun"));
    Moon = FindUnique<ADirectionalLight>(GetWorld(),TEXT("HC_VS2_Rev2_Moon"));
    Sky = FindUnique<ASkyLight>(GetWorld(),TEXT("HC_VS2_Rev2_SkyLight"));
    Post = FindUnique<APostProcessVolume>(GetWorld(),TEXT("HC_VS2_Rev2_PostProcess"));
    auto* Dome = FindUnique<AStaticMeshActor>(GetWorld(),TEXT("HC_VS2_Rev2_SkyDome"));
    if (!Sun || !Moon || !Sky || !Post || !Dome || !Dome->GetStaticMeshComponent()->GetMaterial(0)) return false;
    NightLights.Reset(); NightIntensities.Reset();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("HC_VS2_Rev2_NightLight")) || It->ActorHasTag(TEXT("HC_VS2_Rev2_WindowLight")))
        {
            auto* Light = It->FindComponentByClass<UPointLightComponent>();
            float Value = -1;
            for (FName Tag : It->Tags)
            {
                FString S = Tag.ToString();
                if (S.RemoveFromStart(TEXT("HC_VS2_Rev2_OnIntensity=")))
                { if (!LexTryParseString(Value,*S)) return false; }
            }
            if (!Light || !FMath::IsFinite(Value) || Value < 0 || Value > 10000) return false;
            NightLights.Add(Light); NightIntensities.Add(Value);
        }
    if (NightLights.IsEmpty()) return false;
    DomeMaterial = Dome->GetStaticMeshComponent()->CreateDynamicMaterialInstance(0);
    bBound = DomeMaterial != nullptr;
    return bBound;
}

bool AHCM5VS2CornerTimeDirector::SetTimeOfDay(FName Period)
{
    if (Period != TEXT("Afternoon") && Period != TEXT("Dusk") && Period != TEXT("Night")) return false;
    if (!BindWorld()) return false;
    const bool Dusk = Period == TEXT("Dusk"), Night = Period == TEXT("Night");
    Sun->SetActorRotation(Dusk ? FRotator(-8,155,0) : FRotator(-32,138,0));
    Sun->GetLightComponent()->SetIntensity(Night ? 0.f : Dusk ? 3.5f : 6.f);
    Sun->GetLightComponent()->SetLightColor(Dusk ? FLinearColor(1,.39f,.19f) : FLinearColor(1,.97f,.91f),true);
    Moon->SetActorRotation(FRotator(-38,-25,0));
    Moon->GetLightComponent()->SetIntensity(Night ? .7f : 0.f);
    Moon->GetLightComponent()->SetLightColor(FLinearColor(.50f,.65f,1.f),true);
    Sky->GetLightComponent()->SetIntensity(Night ? .8f : Dusk ? .75f : 1.1f);
    Sky->GetLightComponent()->SetLightColor(Night ? FLinearColor(.65f,.72f,1.f) : Dusk ? FLinearColor(.65f,.53f,.70f) : FLinearColor::White);
    FPostProcessSettings& P = Post->Settings;
    P.bOverride_AutoExposureMethod = true; P.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    P.bOverride_AutoExposureBias = true; P.AutoExposureBias = Night ? .7f : .35f;
    P.bOverride_AutoExposureApplyPhysicalCameraExposure = true; P.AutoExposureApplyPhysicalCameraExposure = false;
    P.bOverride_BloomIntensity = true; P.BloomIntensity = Night ? .20f : Dusk ? .13f : .08f;
    DomeMaterial->SetVectorParameterValue(TEXT("SkyZenith"), Night ? FLinearColor(.008f,.015f,.045f) : Dusk ? FLinearColor(.17f,.13f,.36f) : FLinearColor(.18f,.47f,.82f));
    DomeMaterial->SetVectorParameterValue(TEXT("SkyHorizon"), Night ? FLinearColor(.035f,.055f,.12f) : Dusk ? FLinearColor(1.f,.32f,.19f) : FLinearColor(.68f,.82f,1.f));
    DomeMaterial->SetVectorParameterValue(TEXT("CloudLight"), Night ? FLinearColor(.11f,.16f,.29f) : Dusk ? FLinearColor(1.f,.65f,.40f) : FLinearColor(1.f,.97f,.90f));
    DomeMaterial->SetVectorParameterValue(TEXT("CloudShade"), Night ? FLinearColor(.025f,.04f,.095f) : Dusk ? FLinearColor(.40f,.22f,.37f) : FLinearColor(.58f,.71f,.89f));
    // This authored shader uses the parameter as cloud opacity, not covered sky area.
    DomeMaterial->SetScalarParameterValue(TEXT("CloudCoverage"), 1.f);
    DomeMaterial->SetScalarParameterValue(TEXT("StarStrength"), Night ? 1.f : 0.f);
    DomeMaterial->SetScalarParameterValue(TEXT("MoonStrength"), Night ? 1.f : 0.f);
    for (int32 I = 0; I < NightLights.Num(); ++I) NightLights[I]->SetIntensity((Dusk || Night) ? NightIntensities[I] : 0.f);
    Sky->GetLightComponent()->RecaptureSky();
    CurrentPeriod = Period;
    return true;
}

FString AHCM5VS2CornerTimeDirector::GetLightingDiagnostics() const
{
    auto J = MakeShared<FJsonObject>();
    J->SetBoolField(TEXT("bound"), bBound); J->SetStringField(TEXT("period"),CurrentPeriod.ToString());
    if (Sun) { J->SetNumberField(TEXT("sun_intensity"),Sun->GetLightComponent()->Intensity); J->SetArrayField(TEXT("sun_color"),RGBA(Sun->GetLightComponent()->GetLightColor())); J->SetArrayField(TEXT("sun_rotation"),Angles(Sun->GetActorRotation())); }
    if (Moon) { J->SetNumberField(TEXT("moon_intensity"),Moon->GetLightComponent()->Intensity); J->SetArrayField(TEXT("moon_color"),RGBA(Moon->GetLightComponent()->GetLightColor())); J->SetArrayField(TEXT("moon_rotation"),Angles(Moon->GetActorRotation())); }
    if (Sky) { J->SetNumberField(TEXT("sky_intensity"),Sky->GetLightComponent()->Intensity); J->SetArrayField(TEXT("sky_color"),RGBA(Sky->GetLightComponent()->GetLightColor())); J->SetBoolField(TEXT("sky_realtime"),Sky->GetLightComponent()->IsRealTimeCaptureEnabled()); }
    if (Post) { J->SetNumberField(TEXT("exposure_bias"),Post->Settings.AutoExposureBias); J->SetNumberField(TEXT("exposure_method"),int32(Post->Settings.AutoExposureMethod)); J->SetBoolField(TEXT("physical_exposure"),Post->Settings.AutoExposureApplyPhysicalCameraExposure); J->SetNumberField(TEXT("bloom"),Post->Settings.BloomIntensity); }
    J->SetStringField(TEXT("sky_material"),GetPathNameSafe(DomeMaterial));
    if (DomeMaterial)
    {
        auto Parameters=MakeShared<FJsonObject>();
        bool Complete=true;
        for (const TCHAR* Name : {TEXT("SkyZenith"),TEXT("SkyHorizon"),TEXT("CloudLight"),TEXT("CloudShade")})
        { FLinearColor Value; const bool Found=DomeMaterial->GetVectorParameterValue(FMaterialParameterInfo(FName(Name)),Value); Complete &= Found; if (Found) Parameters->SetArrayField(Name,RGBA(Value)); }
        for (const TCHAR* Name : {TEXT("CloudCoverage"),TEXT("StarStrength"),TEXT("MoonStrength")})
        { float Value=0; const bool Found=DomeMaterial->GetScalarParameterValue(FMaterialParameterInfo(FName(Name)),Value); Complete &= Found; if (Found) Parameters->SetNumberField(Name,Value); }
        J->SetBoolField(TEXT("sky_parameters_complete"),Complete); J->SetObjectField(TEXT("sky_parameters"),Parameters);
    }
    TArray<TSharedPtr<FJsonValue>> Lights;
    for (const auto& L : NightLights) { auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("component"),L->GetPathName()); Row->SetNumberField(TEXT("intensity"),L->Intensity); Row->SetNumberField(TEXT("units"),int32(L->IntensityUnits)); Lights.Add(MakeShared<FJsonValueObject>(Row)); }
    J->SetArrayField(TEXT("night_lights"),Lights);
    FString S; FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S)); return S;
}
