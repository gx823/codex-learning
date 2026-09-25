#include "HCM4R2PlayerCameraManager.h"
#include "HCM4R2PresentationComponent.h"
#include "M1/HCM1PlayerController.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1Vehicle.h"
#include "M2/HCM2SceneSettings.h"
#include "M4/HCM4CombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "M3/HCM3NPC.h"

void AHCM4R2PlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
    Super::ProcessViewRotation(DeltaTime, OutViewRotation, OutDeltaRot);
    const auto* PC = Cast<AHCM1PlayerController>(PCOwner);
    if (PC && PC->GetPlayerMode() == EHCPlayerMode::OnFoot && PC->IsFirstPersonPerspective())
        LimitViewPitch(OutViewRotation, -FootFirstPersonPitchLimit, FootFirstPersonPitchLimit);
}

void AHCM4R2PlayerCameraManager::ResetPerspectiveState()
{
    CockpitLook = FRotator::ZeroRotator;
    CockpitIdleSeconds = 0;
    GunShoulderBlend = 0;
    bResetFilter = true;
    SetGameCameraCutThisFrame();
}

void AHCM4R2PlayerCameraManager::AddCockpitLook(const FVector2D& RawMouse)
{
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(PCOwner);
    if (!PC || RawMouse.ContainsNaN() || RawMouse.IsNearlyZero()) return;
    const FVector2D Gain = PC->GetDrivingLookDegreesPerActionUnit();
    // Displacement input, same effective signed baseline and .75 exactly once.
    CockpitLook.Yaw = FMath::Clamp(CockpitLook.Yaw + RawMouse.X * Gain.X, -115.f, 115.f);
    CockpitLook.Pitch = FMath::Clamp(CockpitLook.Pitch + RawMouse.Y * Gain.Y, -55.f, 45.f);
    CockpitIdleSeconds = 0;
}

void AHCM4R2PlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
    Super::UpdateViewTargetInternal(OutVT, DeltaTime);
    AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(PCOwner);
    if (!PC) return;
    AHCM1Character* Character = PC->GetControlledCharacter();
    AHCM1Vehicle* Vehicle = PC->GetActiveVehicle();
    const bool bFoot = PC->GetPlayerMode() == EHCPlayerMode::OnFoot && OutVT.Target == Character;
    const bool bDriving = PC->GetPlayerMode() == EHCPlayerMode::Driving && OutVT.Target == Vehicle;
    if (!bFoot && !bDriving) return; // Existing safe entry/exit blend remains authoritative.
    const bool bFirst = PC->IsFirstPersonPerspective();
    OutVT.POV.bUseFirstPersonParameters = false;
    if (bFoot && Character)
    {
        const UHCM4CombatComponent* Combat = PC->GetCombatComponent();
        const float Aim = Combat ? Combat->GetAimBlend() : 0.f;
        const bool bPistol = Combat && Combat->GetWeaponMode()==EHCM4WeaponMode::Pistol && Combat->GetPlayerHealth()>0;
        GunShoulderBlend = FMath::FInterpConstantTo(GunShoulderBlend,bPistol?1.f:0.f,DeltaTime,5.f);
        LastBaseWorldFOV = Character->GetFollowCamera()->FieldOfView;
        if (!bFirst) OutVT.POV.Location = Character->GetCameraBoom()->GetUnfixedCameraPosition();
        if (bFirst)
        {
            // Eye-height anchored to the physical capsule, not noisy animated head roll.
            OutVT.POV.Location = Character->GetActorLocation() + FVector(0,0,Character->BaseEyeHeight);
            OutVT.POV.Rotation = PC->GetControlRotation();
            OutVT.POV.Rotation.Roll = 0;
            OutVT.POV.bUseFirstPersonParameters = true;
            OutVT.POV.FirstPersonFOV = 75.f;
            OutVT.POV.FirstPersonScale = .65f;
            OutVT.POV.PerspectiveNearClipPlane = 3.f;
        }
        else if (GunShoulderBlend > KINDA_SMALL_NUMBER || Aim > KINDA_SMALL_NUMBER)
        {
            const FRotator View = PC->GetControlRotation();
            const FVector Pivot = Character->GetActorLocation() + FVector(0,0,Character->BaseEyeHeight - 8.f);
            // The original hip boom sat below and directly behind the gun, so the
            // back occluded both hands. Keep a wider real shoulder view when armed;
            // ADS closes toward its existing pose without changing hip FOV or input.
            const FVector Shoulder = Pivot + View.RotateVector(FMath::Lerp(FVector(-230,115,5),FVector(-170,65,5),Aim));
            FCollisionQueryParams Query(SCENE_QUERY_STAT(M4R2ADSShoulder), false, Character);
            for(TActorIterator<APawn> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
            FHitResult Hit;
            const FVector Desired = FMath::Lerp(OutVT.POV.Location, Shoulder, FMath::Max(GunShoulderBlend,Aim));
            const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity,
                ECC_Camera, FCollisionShape::MakeSphere(12.f), Query);
            OutVT.POV.Location = bBlocked ? Hit.Location : Desired;
            OutVT.POV.Rotation = View;
        }
        // World projection magnification; no weapon/viewport image scaling.
        if (!bFirst)
        {
            // Sweep from eye level. Never orbit below the waist while looking up.
            const FVector Pivot = Character->GetActorLocation()+FVector(0,0,Character->BaseEyeHeight);
            FVector Desired=OutVT.POV.Location; Desired.Z=FMath::Max(Desired.Z,Pivot.Z);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(VS3FootCamera),false,Character);
            for(TActorIterator<APawn> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
            FHitResult Hit;
            OutVT.POV.Location=GetWorld()->SweepSingleByChannel(Hit,Pivot,Desired,FQuat::Identity,
                ECC_Camera,FCollisionShape::MakeSphere(12.f),Query)?Hit.Location:Desired;
        }
        LastMagnification = Combat ? Combat->GetCurrentADSMagnification() : 1.f;
        OutVT.POV.FOV = FMath::RadiansToDegrees(2.f * FMath::Atan(
            FMath::Tan(FMath::DegreesToRadians(LastBaseWorldFOV * .5f)) / LastMagnification));
    }
    else if (bDriving && Vehicle)
    {
        LastBaseWorldFOV = Vehicle->DrivingCamera->FieldOfView;
        LastMagnification = 1.f;
        if (bFirst)
        {
            if (LastVehicle != Vehicle || LastVehicleReset != Vehicle->GetCameraResetSerial())
            {
                LastVehicle = Vehicle;
                LastVehicleReset = Vehicle->GetCameraResetSerial();
                ResetPerspectiveState();
            }
            const FRotator Chassis = Vehicle->GetActorRotation();
            const FRotator Target(FMath::Clamp(FRotator::NormalizeAxis(Chassis.Pitch),-15.f,15.f)*.35f,
                Chassis.Yaw, FMath::Clamp(FRotator::NormalizeAxis(Chassis.Roll),-15.f,15.f)*.2f);
            FilteredChassis = bResetFilter ? Target : FMath::RInterpTo(FilteredChassis,Target,DeltaTime,7.f);
            // Heading follows actual vehicle immediately; only high-frequency pitch/roll are filtered.
            FilteredChassis.Yaw = Chassis.Yaw;
            bResetFilter = false;
            if (!PC->IsPauseMenuOpen() && PC->IsGameplayFocused())
            {
                CockpitIdleSeconds += DeltaTime;
                if (CockpitIdleSeconds >= 1.5f && Vehicle->GetSignedSpeed() > 100.f)
                    CockpitLook.Yaw *= FMath::Exp(-3.f*DeltaTime);
            }
            OutVT.POV.Location = Vehicle->GetActorTransform().TransformPosition(Vehicle->GetDriverEyeLocal());
            // Fitted driver eye: a mild downward rest angle frames the real wheel
            // and road together. Mouse pitch remains a separate signed offset.
            OutVT.POV.Rotation = FilteredChassis + CockpitLook + FRotator(-6.f,0,0);
            OutVT.POV.FOV = LastBaseWorldFOV;
            OutVT.POV.PerspectiveNearClipPlane = 3.f;
            // The default 10--90 percentile meters mostly the dark cabin and
            // boosted the measured view exposure ~50x above the same street.
            // Meter the brighter window/road range only for this cockpit POV.
            // A window can leave the histogram entirely when looking at a door
            // or roof. Bound adaptation to the current authored daylight preset,
            // then recover interior detail with local exposure, not brighter roads.
            OutVT.POV.PostProcessBlendWeight = 1.f;
            OutVT.POV.PostProcessSettings.bOverride_AutoExposureLowPercent = true;
            OutVT.POV.PostProcessSettings.AutoExposureLowPercent = 80.f;
            OutVT.POV.PostProcessSettings.bOverride_AutoExposureHighPercent = true;
            OutVT.POV.PostProcessSettings.AutoExposureHighPercent = 95.f;
            const bool bDusk = PC->GetSceneSettings() && PC->GetSceneSettings()->IsDusk();
            OutVT.POV.PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
            const bool bCyber=PC->GetSceneSettings() && PC->GetSceneSettings()->SceneId==TEXT("M5_CyberHarbor");
            OutVT.POV.PostProcessSettings.AutoExposureMinBrightness = bCyber ? 1.5f : bDusk ? 6.f : 10.f;
            OutVT.POV.PostProcessSettings.bOverride_LocalExposureShadowContrastScale = true;
            OutVT.POV.PostProcessSettings.LocalExposureShadowContrastScale = .45f;
        }
    }
}

void AHCM4R2PlayerCameraManager::UpdateCamera(float DeltaTime)
{
    // V / load may enter FP with a TP angle already outside the FP pose range.
    // Clamp the actual control and POV together; input gain and TP limits are unchanged.
    auto* MutablePC = Cast<AHCM1PlayerController>(PCOwner);
    if (MutablePC && MutablePC->GetPlayerMode() == EHCPlayerMode::OnFoot && MutablePC->IsFirstPersonPerspective())
    {
        FRotator Safe = MutablePC->GetControlRotation();
        const float Pitch = FRotator::NormalizeAxis(Safe.Pitch);
        Safe.Pitch = FMath::Clamp(Pitch, -FootFirstPersonPitchLimit, FootFirstPersonPitchLimit);
        if (!FMath::IsNearlyEqual(Pitch, Safe.Pitch)) MutablePC->SetControlRotation(Safe);
    }
    Super::UpdateCamera(DeltaTime);
    if (MutablePC)
    {
        for (const auto& Component : ProximityHidden) MutablePC->HiddenPrimitiveComponents.Remove(Component);
        ProximityHidden.Reset();
        const FVector Eye=GetCameraLocation();
        for(TActorIterator<ACharacter> It(GetWorld());It;++It)
        {
            const bool bHero=*It==MutablePC->GetControlledCharacter();
            if ((!bHero && !Cast<AHCM3NPC>(*It)) || (bHero && MutablePC->IsFirstPersonPerspective())) continue;
            const FVector Delta=Eye-It->GetActorLocation();
            if(Delta.Size2D()> (bHero?125.f:90.f) || FMath::Abs(Delta.Z)>150.f)continue;
            TInlineComponentArray<USkeletalMeshComponent*> Meshes; It->GetComponents(Meshes);
            for(auto* Mesh:Meshes) if(!MutablePC->HiddenPrimitiveComponents.Contains(Mesh))
            {MutablePC->HiddenPrimitiveComponents.Add(Mesh);ProximityHidden.Add(Mesh);}
        }
    }
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(PCOwner);
    if (PC && PC->GetControlledCharacter())
    {
        USkeletalMeshComponent* Mesh = PC->GetControlledCharacter()->GetMesh();
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        const bool bFootFirst = PC->GetPlayerMode() == EHCPlayerMode::OnFoot && PC->IsFirstPersonPerspective();
        Mesh->SetOwnerNoSee(bFootFirst);
        // A seated character is hidden at its former foot position. Only the
        // on-foot FP body should keep a shadow while hidden from its owner.
        Mesh->SetCastHiddenShadow(bFootFirst);
        if (UHCM4R2PresentationComponent* Presentation = PC->GetControlledCharacter()->GetR2PresentationComponent())
        {
            Presentation->SetFirstPersonActive(PC->GetPlayerMode() == EHCPlayerMode::OnFoot && PC->IsFirstPersonPerspective());
            Presentation->UpdateFromFinalView(GetCameraCacheView());
        }
    }
}

FString AHCM4R2PlayerCameraManager::GetPerspectiveDiagnostics() const
{
    return FString::Printf(TEXT("{\"manager\":\"M4R2\",\"base_fov\":%.6f,\"final_fov\":%.6f,\"magnification\":%.6f,\"cockpit_pitch\":%.6f,\"cockpit_yaw\":%.6f}"),
        LastBaseWorldFOV, GetFOVAngle(), LastMagnification, CockpitLook.Pitch, CockpitLook.Yaw);
}
