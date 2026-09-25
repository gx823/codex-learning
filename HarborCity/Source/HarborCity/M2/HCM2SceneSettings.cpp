#include "HCM2SceneSettings.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1LightSwitch.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InputComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

AHCM2SceneSettings::AHCM2SceneSettings()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneReferences")));
}

bool AHCM2SceneSettings::HasValidSaveIdentity() const
{
    if (SceneId.IsNone() || PlayerId.IsNone() || MapName.IsNone() ||
        MapName.ToString() != UGameplayStatics::GetCurrentLevelName(this, true) ||
        (!SaveSlot.StartsWith(TEXT("HarborCity_M2_")) && !SaveSlot.StartsWith(TEXT("HarborCity_M5_VS1")) && !SaveSlot.StartsWith(TEXT("HarborCity_M5_VS2_"))) || SaveSlot.Len() > 100) return false;
    for (TCHAR C : SaveSlot) if (!FChar::IsAlnum(C) && C != TEXT('_')) return false;
    return true;
}

void AHCM2SceneSettings::BeginPlay()
{
    Super::BeginPlay();
    if (bExternalTimeOfDayController) return;
    SetDuskPreset(bStartAtDusk);
    if (AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        // A separate T-only binding does not modify any approved Look mapping.
        EnableInput(PC);
        FInputKeyBinding& Binding = InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ThisClass::RequestTogglePreset);
        Binding.bConsumeInput = true;
        Binding.bExecuteWhenPaused = false;
    }
}

void AHCM2SceneSettings::EndPlay(const EEndPlayReason::Type Reason)
{
    DisableInput(Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0)));
    Super::EndPlay(Reason);
}

void AHCM2SceneSettings::SetDuskPreset(bool bEnabled)
{
    if (bExternalTimeOfDayController) return;
    bDusk = bEnabled;
    if (IsValid(Sun))
    {
        Sun->SetActorRotation(bDusk ? DuskSunRotation : AfternoonSunRotation);
        ULightComponent* Light = Sun->GetLightComponent();
        Light->SetIntensity(bDusk ? DuskSunIntensity : AfternoonSunIntensity);
        Light->SetLightColor(bDusk ? DuskSunColor : AfternoonSunColor);
    }
    if (IsValid(SkyLight)) SkyLight->GetLightComponent()->SetIntensity(bDusk ? DuskSkyIntensity : AfternoonSkyIntensity);
    for (AActor* Actor : EveningLightActors)
    {
        // The independent, persistent cafe switch is never owned by the preset.
        if (!IsValid(Actor) || Actor == InteriorSwitch || Actor->IsA<AHCM1LightSwitch>() || Actor == Sun || Actor == SkyLight) continue;
        TInlineComponentArray<ULightComponent*> Lights;
        Actor->GetComponents(Lights);
        for (ULightComponent* Light : Lights)
        {
            Light->SetIntensity(bDusk ? DuskStreetIntensity : AfternoonStreetIntensity);
            Light->SetVisibility((bDusk ? DuskStreetIntensity : AfternoonStreetIntensity) > 0);
        }
    }
    for (AActor* Actor : EveningEmissionActors)
    {
        if (!IsValid(Actor)) continue;
        TInlineComponentArray<UMeshComponent*> Meshes;
        Actor->GetComponents(Meshes);
        for (UMeshComponent* Mesh : Meshes)
            Mesh->SetScalarParameterValueOnMaterials(TEXT("LightOutput"),
                (bDusk ? DuskStreetIntensity : AfternoonStreetIntensity) > 0 ? 4.0f : 0.0f);
    }
}

void AHCM2SceneSettings::RequestTogglePreset()
{
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (!PC || !PC->IsGameplayFocused() || PC->IsPauseMenuOpen() ||
        (PC->GetPlayerMode() != EHCPlayerMode::OnFoot && PC->GetPlayerMode() != EHCPlayerMode::Driving)) return;
    SetDuskPreset(!bDusk);
}
