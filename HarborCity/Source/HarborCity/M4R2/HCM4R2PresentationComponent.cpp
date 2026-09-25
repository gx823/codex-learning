#include "HCM4R2PresentationComponent.h"
#include "HCM4R2FirstPersonMesh.h"
#include "HCM4R2PistolPresentation.h"
#include "HCM4R2PlayerAnimInstance.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M4/HCM4CombatComponent.h"
#include "M4/HCM4Weapon.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TwoBoneIK.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/Package.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Camera/PlayerCameraManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#endif

namespace
{
// Keep the accepted mannequin names whenever present. Different capitalization
// needs no mapping because bone lookup uses FName; only hierarchy differences do.
FName PresentationBone(const USkinnedMeshComponent* Mesh,FName Legacy,FName Alternate)
{
    return Mesh && Mesh->GetBoneIndex(Legacy)==INDEX_NONE && Mesh->GetBoneIndex(Alternate)!=INDEX_NONE ? Alternate : Legacy;
}
}

UHCM4R2PresentationComponent::UHCM4R2PresentationComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
void UHCM4R2PresentationComponent::BeginPlay()
{
    Super::BeginPlay();Character=Cast<AHCM1Character>(GetOwner());
    if(Character)
    {
        AddTickPrerequisiteComponent(Character->GetMesh());
        if(auto* Combat=Character->GetCombatComponent())
            ShotDelegate=Combat->OnAcceptedShot.AddUObject(this,&ThisClass::OnShot);
    }
}
void UHCM4R2PresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Character && Character->GetCombatComponent())Character->GetCombatComponent()->OnAcceptedShot.Remove(ShotDelegate);
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedDelegate);
    Super::EndPlay(Reason);
}
void UHCM4R2PresentationComponent::SetFirstPersonActive(bool bNewActive)
{
    bActive=bNewActive;
    if(HeadAccessory)HeadAccessory->SetOwnerNoSee(bActive);
    if(!bActive){FlashUntil=0;WallRetraction=0;bPoseReady=false;IdleArmBlend=0;bIdleArmTargetsReady=false;ResetLocomotion();}
}
void UHCM4R2PresentationComponent::UpdateFromFinalView(const FMinimalViewInfo& View)
{ LastView=View;bHasView=true; }
void UHCM4R2PresentationComponent::SetFirstPersonArmsOverride(USkeletalMesh* Arms)
{
    FirstPersonArmsOverride=Arms;
    ResolvedArmsSource=nullptr;
    ResolveFirstPersonArms();
}
void UHCM4R2PresentationComponent::ResolveFirstPersonArms()
{
    USkeletalMesh* Source=Character && Character->GetMesh()?Character->GetMesh()->GetSkeletalMeshAsset():nullptr;
    const FSoftObjectPath OverridePath=FirstPersonArmsOverride.ToSoftObjectPath();
    if(!Source || (ResolvedArmsSource==Source && ResolvedArmsOverride==OverridePath))return;
    ResolvedArmsSource=Source;ResolvedArmsOverride=OverridePath;bArmsPairedToSource=false;
    USkeletalMesh* Arms=nullptr;
    if(!OverridePath.IsNull())Arms=FirstPersonArmsOverride.LoadSynchronous();
    else
    {
        const FString Package=Source->GetOutermost()->GetName()+TEXT("_Arms");
        const FString Object=Package+TEXT(".")+Source->GetName()+TEXT("_Arms");
        Arms=LoadObject<USkeletalMesh>(nullptr,*Object);
    }
    if(Arms && Arms->GetSkeleton()!=Source->GetSkeleton())
    {
        UE_LOG(LogTemp,Warning,TEXT("M5VS1 FP arms rejected: skeleton differs from actual body: %s"),*GetPathNameSafe(Arms));
        Arms=nullptr;
    }
    bArmsPairedToSource=Arms!=nullptr;
    if(!Arms && OverridePath.IsNull())
    {
        // Preserve the accepted M4 body/arms pair. Do not silently dress a new
        // heroine in the legacy geometry merely because the skeleton matches.
        const bool bLegacyBody=Source->GetPathName()==TEXT("/Game/HarborCity/M3/Appearance/SK_M3_Quinn_HarborNavy.SK_M3_Quinn_HarborNavy");
        if(bLegacyBody)
        {
            Arms=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/HarborCity/M4R2/Meshes/SKM_M4R2_FirstPersonArms.SKM_M4R2_FirstPersonArms"));
            bArmsPairedToSource=Arms && Arms->GetSkeleton()==Source->GetSkeleton();
        }
    }
    FirstPersonArmsAsset=Arms;
    IdleArmBlend=0;bIdleArmTargetsReady=false;bPoseReady=false;ResetLocomotion();
    if(!Arms)UE_LOG(LogTemp,Warning,TEXT("M5VS1 matching FP arms missing for %s; supply FirstPersonArmsOverride or sibling _Arms asset"),*GetPathNameSafe(Source));
}
void UHCM4R2PresentationComponent::ResetLocomotion()
{
    LocomotionSpeed=0;LocomotionMoveBlend=0;LocomotionRunBlend=0;SourceStride=0;
    bLocomotionReady=false;bLocomotionGrounded=false;PistolLocomotionOffset=FVector::ZeroVector;
}
void UHCM4R2PresentationComponent::UpdateHeadAccessory()
{
    if(!Character)return;
    USkeletalMeshComponent* Source=Character->GetMesh();
    const FSoftObjectPath Wanted=HeadAccessoryMesh.ToSoftObjectPath();
    if(Wanted.IsNull())
    {
        if(HeadAccessory){HeadAccessory->DestroyComponent();HeadAccessory=nullptr;}
        ResolvedHeadAccessoryPath.Reset();return;
    }
    if(!Source || Source->GetBoneIndex(HeadAccessoryBone)==INDEX_NONE)
    {
        if(HeadAccessory)HeadAccessory->SetVisibility(false);
        return;
    }
    if(!HeadAccessory || ResolvedHeadAccessoryPath!=Wanted)
    {
        USceneComponent* Parent=Source;
        UStaticMesh* Asset=HeadAccessoryMesh.LoadSynchronous();
        if(!Asset)return;
        if(!HeadAccessory)
        {
            HeadAccessory=NewObject<UStaticMeshComponent>(Character,TEXT("OptionalHeadAccessory"));
            Character->AddInstanceComponent(HeadAccessory);
            HeadAccessory->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            HeadAccessory->SetCanEverAffectNavigation(false);
            HeadAccessory->SetCastShadow(false);
            HeadAccessory->RegisterComponent();
        }
        HeadAccessory->SetStaticMesh(Asset);
        HeadAccessory->AttachToComponent(Parent,FAttachmentTransformRules::KeepRelativeTransform,HeadAccessoryBone);
        ResolvedHeadAccessoryPath=Wanted;
    }
    if(HeadAccessory->GetAttachParent()!=Source || HeadAccessory->GetAttachSocketName()!=HeadAccessoryBone)
        HeadAccessory->AttachToComponent(Source,FAttachmentTransformRules::KeepRelativeTransform,HeadAccessoryBone);
    HeadAccessory->SetRelativeTransform(HeadAccessoryLocalTransform);
    HeadAccessory->SetOwnerNoSee(bActive);
    HeadAccessory->SetHiddenInGame(Character->IsHidden());
    HeadAccessory->SetVisibility(!Character->IsHidden());
}
void UHCM4R2PresentationComponent::UpdateLocomotion(float DeltaSeconds)
{
    const UCharacterMovementComponent* Movement=Character->GetCharacterMovement();
    const FVector Velocity=Character->GetVelocity();
    LocomotionSpeed=Velocity.Size2D();
    bLocomotionGrounded=Movement && Movement->IsMovingOnGround();
    const float Walk=FMath::Max(1.f,Character->WalkSpeed);
    const float RunRange=FMath::Max(1.f,Character->SprintSpeed-Walk);
    const float Move=bLocomotionGrounded?FMath::Clamp(LocomotionSpeed/Walk,0.f,1.f):0.f;
    const float Run=bLocomotionGrounded?FMath::Clamp((LocomotionSpeed-Walk)/RunRange,0.f,1.f):0.f;
    float Stride=0;
    const USkeletalMeshComponent* Source=Character->GetMesh();
    if(bLocomotionGrounded && LocomotionSpeed>5.f && Source->DoesSocketExist(TEXT("foot_l")) && Source->DoesSocketExist(TEXT("foot_r")))
    {
        // Read the evaluated leg phase instead of advancing an unrelated sine
        // clock: the left hand advances with the right leg, also while strafing.
        const FVector FootSeparation=Source->GetSocketLocation(TEXT("foot_r"))-Source->GetSocketLocation(TEXT("foot_l"));
        Stride=FMath::Clamp(float(FVector::DotProduct(FootSeparation,Velocity.GetSafeNormal2D())/80.f),-1.f,1.f);
    }
    const float Dt=FMath::Max(0.f,DeltaSeconds);
    LocomotionMoveBlend=bLocomotionReady?FMath::FInterpTo(LocomotionMoveBlend,Move,Dt,10.f):Move;
    LocomotionRunBlend=bLocomotionReady?FMath::FInterpTo(LocomotionRunBlend,Run,Dt,10.f):Run;
    SourceStride=bLocomotionReady?FMath::FInterpTo(SourceStride,Stride,Dt,18.f):Stride;
    bLocomotionReady=true;
}
void UHCM4R2PresentationComponent::EnsureAssets()
{
    if(bAssetsInitialized || !Character || !Character->GetMesh()->GetSkeletalMeshAsset())return;
    bAssetsInitialized=true;
    ResolveFirstPersonArms();
    FirstPersonBody=NewObject<UHCM4R2FirstPersonMesh>(Character,TEXT("M4R2_FirstPersonBody"));
    Character->AddInstanceComponent(FirstPersonBody);
    FirstPersonBody->SetSkinnedAssetAndUpdate(Character->GetMesh()->GetSkeletalMeshAsset());
    for(int32 I=0;I<Character->GetMesh()->GetNumMaterials();++I)FirstPersonBody->SetMaterial(I,Character->GetMesh()->GetMaterial(I));
    FirstPersonBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);FirstPersonBody->SetCanEverAffectNavigation(false);
    FirstPersonBody->SetOnlyOwnerSee(true);FirstPersonBody->SetCastShadow(false);
    FirstPersonBody->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    FirstPersonBody->RegisterComponent();FirstPersonBody->SetVisibility(false);
    // neck_01 is a sibling of the clavicle/arm branches: remove the local neck/head
    // stump without removing either arm or changing the gameplay/world skeleton.
    for(FName Bone:{PresentationBone(FirstPersonBody,TEXT("neck_01"),TEXT("Neck")),
                   PresentationBone(FirstPersonBody,TEXT("thigh_l"),TEXT("UpperLeg_L")),
                   PresentationBone(FirstPersonBody,TEXT("thigh_r"),TEXT("UpperLeg_R"))})FirstPersonBody->HideBoneByName(Bone,PBO_None);
    if(USkeletalMesh* Pistol=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Weapons/Pistol/Meshes/SKM_Pistol.SKM_Pistol")))
    {
        FirstPersonPistol=NewObject<UPoseableMeshComponent>(Character,TEXT("M4R2_FirstPersonPistol"));Character->AddInstanceComponent(FirstPersonPistol);
        FirstPersonPistol->SetSkinnedAssetAndUpdate(Pistol);FirstPersonPistol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FirstPersonPistol->SetCanEverAffectNavigation(false);FirstPersonPistol->SetOnlyOwnerSee(true);FirstPersonPistol->SetCastShadow(false);
        FirstPersonPistol->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);FirstPersonPistol->RegisterComponent();FirstPersonPistol->SetVisibility(false);
    }
    else
    {
        // This remains an explicitly observable incomplete-asset fallback, not a passed FP deliverable.
        FirstPersonFallbackPistol=NewObject<UStaticMeshComponent>(Character,TEXT("M4R2_MissingPistolFallback"));Character->AddInstanceComponent(FirstPersonFallbackPistol);
        FirstPersonFallbackPistol->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/HarborCity/M4/Weapons/SM_M4_Pistol.SM_M4_Pistol")));
        FirstPersonFallbackPistol->SetCollisionEnabled(ECollisionEnabled::NoCollision);FirstPersonFallbackPistol->SetCanEverAffectNavigation(false);
        FirstPersonFallbackPistol->SetOnlyOwnerSee(true);FirstPersonFallbackPistol->SetCastShadow(false);
        FirstPersonFallbackPistol->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);FirstPersonFallbackPistol->RegisterComponent();
        UE_LOG(LogTemp,Warning,TEXT("M4R2 first-person articulated pistol asset unavailable; presentation incomplete"));
    }
    UStaticMesh* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    UMaterialInterface* FlashMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HarborCity/M4R2/Materials/M_M4R2_MuzzleFlash.M_M4R2_MuzzleFlash"));
    if(FlashMaterial)
    {
        auto MakeFlash=[&](FName Name){auto* C=NewObject<UStaticMeshComponent>(Character,Name);Character->AddInstanceComponent(C);C->SetStaticMesh(Plane);C->SetMaterial(0,FlashMaterial);
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetOnlyOwnerSee(true);C->SetCastShadow(false);
            C->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);C->RegisterComponent();C->SetVisibility(false);return C;};
        FlashPlaneA=MakeFlash(TEXT("M4R2_FPFlashA"));FlashPlaneB=MakeFlash(TEXT("M4R2_FPFlashB"));
        if(HCM4R2PistolPresentation::GetFlashCapRotation(Plane,FlashCapLocalRotation))FlashCap=MakeFlash(TEXT("M4R2_FPFlashCap"));
    }
}
void UHCM4R2PresentationComponent::OnShot(const FHCM4AcceptedShot& Shot)
{
    if(Shot.ShotId<=LastShotId)return;
    LastShotId=Shot.ShotId;
    if(bActive){++PresentedShots;FlashUntil=Shot.Time+.055;}
}
void UHCM4R2PresentationComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime,TickType,ThisTick);
    if(!Character)return;
    UpdateHeadAccessory();
    EnsureAssets();UpdatePose(DeltaTime);
    if(!PendingFinalPoseLabel.IsEmpty())SampleRequestedFinalPose();
}
void UHCM4R2PresentationComponent::UpdatePose(float DeltaSeconds)
{
    ResolveFirstPersonArms();
    auto* Combat=Character->GetCombatComponent();
    const bool bShow=bActive && bHasView && !Character->IsHidden() && Combat && Combat->GetPlayerHealth()>0;
    if(Combat && Combat->GetWeaponActor())Combat->GetWeaponActor()->SetLocalFirstPersonView(bShow);
    const bool bPistol=bShow && Combat->GetWeaponMode()==EHCM4WeaponMode::Pistol;
    if(FirstPersonBody)FirstPersonBody->SetVisibility(bShow && (bPistol || FirstPersonArmsAsset));
    if(FirstPersonPistol)FirstPersonPistol->SetVisibility(bPistol);
    if(FirstPersonFallbackPistol)FirstPersonFallbackPistol->SetVisibility(bPistol);
    const bool bFlash=bPistol && GetWorld()->GetTimeSeconds()<FlashUntil;
    if(FlashPlaneA)FlashPlaneA->SetVisibility(bFlash);
    if(FlashPlaneB)FlashPlaneB->SetVisibility(bFlash);
    if(FlashCap)FlashCap->SetVisibility(bFlash);
    if(!bShow || !FirstPersonBody){IdleArmBlend=0;bIdleArmTargetsReady=false;ResetLocomotion();return;}
    const auto* PC=Cast<AHCM1PlayerController>(Character->GetController());
    // Keep the last fully evaluated local pose through a pause. Input clearing may
    // stop a world montage; that is not a request to swing the local arms on screen.
    if(bPoseReady && PC && PC->IsPauseMenuOpen())return;
    UpdateLocomotion(DeltaSeconds);
    USkeletalMeshComponent* Source=Character->GetMesh();
    USkeletalMesh* DisplayAsset=bPistol?Source->GetSkeletalMeshAsset():FirstPersonArmsAsset.Get();
    if(!DisplayAsset)return;
    if(bRelaxedAnatomicalWrists && PalmBasisMesh!=DisplayAsset)
    {
        PalmBasisMesh=DisplayAsset;
        const FReferenceSkeleton& Ref=DisplayAsset->GetRefSkeleton();
        for(int32 Side=0;Side<2;++Side)
        {
            const TCHAR* Suffix=Side==0?TEXT("L"):TEXT("R");
            const int32 Hand=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("Hand_%s"),Suffix)));
            const int32 Middle=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("MiddleProximal_%s"),Suffix)));
            const int32 Index=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("IndexProximal_%s"),Suffix)));
            const int32 Little=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("LittleProximal_%s"),Suffix)));
            bPalmBasisValid[Side]=Hand!=INDEX_NONE && Middle!=INDEX_NONE && Index!=INDEX_NONE && Little!=INDEX_NONE
                && Ref.GetParentIndex(Middle)==Hand && Ref.GetParentIndex(Index)==Hand && Ref.GetParentIndex(Little)==Hand;
            if(!bPalmBasisValid[Side])continue;
            // All three are direct children: translations are measured in the
            // wrist's own axes, independent of this rig's mirrored bone rolls.
            const FVector Long=Ref.GetRefBonePose()[Middle].GetLocation().GetSafeNormal();
            const FVector Across=(Ref.GetRefBonePose()[Index].GetLocation()-Ref.GetRefBonePose()[Little].GetLocation()).GetSafeNormal();
            bPalmBasisValid[Side]=!Long.IsNearlyZero() && !Across.IsNearlyZero() && FMath::Abs(Long.Dot(Across))<.98;
            if(bPalmBasisValid[Side])PalmFrameInHand[Side]=FRotationMatrix::MakeFromXY(Long,Across).ToQuat();
        }
        RelaxedFingerIndices.Reset();RelaxedFingerRotations.Reset();
        UAnimSequence* FingerPose=RelaxedFingerPose.LoadSynchronous();
        if(FingerPose && FingerPose->GetSkeleton()==DisplayAsset->GetSkeleton() && !FingerPose->IsValidAdditive())
        {
            const FReferenceSkeleton& SkeletonRef=FingerPose->GetSkeleton()->GetReferenceSkeleton();
            for(const TCHAR* Side:{TEXT("L"),TEXT("R")})for(const TCHAR* Stem:{TEXT("Thumb"),TEXT("Index"),TEXT("Middle"),TEXT("Ring"),TEXT("Little")})
                for(const TCHAR* Joint:{TEXT("Proximal"),TEXT("Intermediate"),TEXT("Distal")})
                {
                    const FName Bone(*FString::Printf(TEXT("%s%s_%s"),Stem,Joint,Side));
                    const int32 MeshIndex=Ref.FindBoneIndex(Bone),SkeletonIndex=SkeletonRef.FindBoneIndex(Bone);
                    if(MeshIndex==INDEX_NONE || SkeletonIndex==INDEX_NONE)continue;
                    FTransform Pose;
                    FingerPose->GetBoneTransform(Pose,FSkeletonPoseBoneIndex(SkeletonIndex),FAnimExtractContext(.28),false);
                    RelaxedFingerIndices.Add(MeshIndex);RelaxedFingerRotations.Add(Pose.GetRotation().GetNormalized());
                }
            if(RelaxedFingerIndices.Num()!=30){RelaxedFingerIndices.Reset();RelaxedFingerRotations.Reset();}
        }
    }
    if(auto* BoundedMesh=Cast<UHCM4R2FirstPersonMesh>(FirstPersonBody))
        BoundedMesh->SetSkinningVertexRadius(FirstPersonSkinningRadiusCm);
    if(FirstPersonBody->GetSkinnedAsset()!=DisplayAsset)
    {
        FirstPersonBody->SetSkinnedAssetAndUpdate(DisplayAsset);
        FirstPersonBody->EmptyOverrideMaterials();
        // The derived mesh preserves six named M3 clothing slots. Match by name,
        // never apply the current player's material array to unrelated slot indices.
        for(int32 I=0;I<DisplayAsset->GetMaterials().Num();++I)
        {
            const int32 SourceSlot=Source->GetMaterialIndex(DisplayAsset->GetMaterials()[I].MaterialSlotName);
            if(SourceSlot!=INDEX_NONE)FirstPersonBody->SetMaterial(I,Source->GetMaterial(SourceSlot));
        }
        FirstPersonBody->SetForcedLOD(bPistol?0:1); // UE's one-based force: 1 is trimmed LOD0.
    }
    // Clothing may be changed without replacing the skeletal mesh. Keep both
    // views on the source's named slots, including a new heroine's extra slots.
    for(int32 I=0;I<DisplayAsset->GetMaterials().Num();++I)
    {
        const int32 SourceSlot=Source->GetMaterialIndex(DisplayAsset->GetMaterials()[I].MaterialSlotName);
        if(SourceSlot!=INDEX_NONE && FirstPersonBody->GetMaterial(I)!=Source->GetMaterial(SourceSlot))
            FirstPersonBody->SetMaterial(I,Source->GetMaterial(SourceSlot));
    }
    FirstPersonBody->CopyPoseFromSkeletalComponent(Source);
    // PoseableMesh's local->component evaluation does not consume HideBone states.
    // Zero only this display copy's branches after copying the evaluated gameplay pose.
    for(FName Bone:{PresentationBone(FirstPersonBody,TEXT("neck_01"),TEXT("Neck")),
                   PresentationBone(FirstPersonBody,TEXT("thigh_l"),TEXT("UpperLeg_L")),
                   PresentationBone(FirstPersonBody,TEXT("thigh_r"),TEXT("UpperLeg_R"))})
    {
        const int32 Index=FirstPersonBody->GetBoneIndex(Bone);
        if(FirstPersonBody->BoneSpaceTransforms.IsValidIndex(Index))FirstPersonBody->BoneSpaceTransforms[Index].SetScale3D(FVector::ZeroVector);
    }
    const FQuat BodyMeshYaw=FRotator(0,Source->GetComponentRotation().Yaw,0).Quaternion();
    const FVector DisplayScale=Source->GetComponentScale();
    // The native graph rotates spine_03 in component space without translating its
    // pivot. Its descendants (neck and both arms) move around that same pivot.
    // Move ONLY the local display eye anchor by that rotation; a fixed reference
    // eye would leave the camera behind the bent neck at steep downward angles.
    // The actual camera remains capsule anchored and never inherits head animation.
    const FVector EyeInMesh=EyeAnchorInMesh;
    DisplayEyeAnchor=EyeInMesh;
    const FName SpineName=PresentationBone(Source,TEXT("spine_03"),TEXT("Chest"));
    const FTransform SourceSpine=Source->GetSocketTransform(SpineName,RTS_Component);
    FQuat AppliedAim=FQuat::Identity;
    if(const auto* Anim=Cast<UHCM4R2PlayerAnimInstance>(Source->GetAnimInstance()))
    {
        // Match FCSPose::LocalBlendCSBoneTransforms -> Transform::BlendWith:
        // normalized linear quaternion blend, not slerp during a partial alpha.
        AppliedAim=FQuat::FastLerp(FQuat::Identity,Anim->R2UpperBodyRotation.Quaternion(),FMath::Clamp(Anim->R2UpperBodyAlpha,0.f,1.f)).GetNormalized();
    }
    if(!bPistol)
    {
        // Unarmed world locomotion/full-body punches intentionally have no gun aim
        // layer. Give only the FP copy a component-space spine look correction,
        // preserving all evaluated local arm/punch transforms and the world pose.
        // Subtract any source gun aim still blending out instead of stacking it.
        const FVector BaseInMesh=BodyMeshYaw.UnrotateVector(Character->GetActorForwardVector());
        const FVector LookInMesh=BodyMeshYaw.UnrotateVector(LastView.Rotation.Vector());
        const FQuat DesiredAim=FQuat::FindBetweenNormals(BaseInMesh.GetSafeNormal(),LookInMesh.GetSafeNormal());
        const int32 SpineIndex=FirstPersonBody->GetBoneIndex(SpineName);
        const FName ParentName=Source->GetParentBone(SpineName);
        if(FirstPersonBody->BoneSpaceTransforms.IsValidIndex(SpineIndex) && ParentName!=NAME_None)
        {
            FTransform DisplaySpine=SourceSpine;
            DisplaySpine.SetRotation((DesiredAim*AppliedAim.Inverse()*SourceSpine.GetRotation()).GetNormalized());
            FirstPersonBody->BoneSpaceTransforms[SpineIndex]=DisplaySpine.GetRelativeTransform(Source->GetSocketTransform(ParentName,RTS_Component));
            AppliedAim=DesiredAim;
        }
    }
    const FVector Pivot=SourceSpine.GetLocation();
    DisplayEyeAnchor=Pivot+AppliedAim.RotateVector(EyeInMesh-Pivot);
    FVector BodyLocation=LastView.Location-BodyMeshYaw.RotateVector(DisplayEyeAnchor*DisplayScale);
    const float Aim=Combat->GetAimBlend();
    const FQuat ViewQ=LastView.Rotation.Quaternion();
    BodyLocation+=ViewQ.RotateVector(FVector(1.f,4.f*(1.f-Aim)-9.f*Aim,-3.f*(1.f-Aim)));
    FHitResult Wall;FCollisionQueryParams Query(SCENE_QUERY_STAT(M4R2FPWeaponClearance),true,Character);
    if(Combat->GetWeaponActor())Query.AddIgnoredActor(Combat->GetWeaponActor());
    const bool bWall=bPistol && GetWorld()->LineTraceSingleByChannel(Wall,LastView.Location,LastView.Location+LastView.Rotation.Vector()*75.f,ECC_Visibility,Query);
    const float Desired=bWall?FMath::Clamp((75.f-Wall.Distance)/50.f,0.f,1.f):0.f;
    WallRetraction=FMath::FInterpTo(WallRetraction,Desired,DeltaSeconds,15.f);
    BodyLocation+=ViewQ.RotateVector(FVector(-14.f*WallRetraction,0,-5.f*WallRetraction));
    if(bPistol)
    {
        // Display-copy-only weight shift; both hands and their attached pistol
        // travel together. ADS, reload and accepted-shot presentation retain the
        // existing aim/grip pose. Never move the real camera or world muzzle.
        const bool bBusy=Combat->IsReloading() || bFlash || Combat->GetShotPresentationPulse()>.001f;
        const float FreePose=bBusy?0.f:FMath::Square(1.f-Aim);
        const float Breath=.18f*FMath::Sin(float(GetWorld()->GetTimeSeconds())*1.6f)*(1.f-LocomotionMoveBlend);
        const FVector DesiredOffset(.25f*SourceStride*LocomotionMoveBlend,
            .55f*SourceStride*LocomotionMoveBlend,
            -.8f*LocomotionMoveBlend-1.6f*LocomotionRunBlend+.3f*(1.f-FMath::Square(SourceStride))*LocomotionMoveBlend+Breath);
        PistolLocomotionOffset=FMath::VInterpTo(PistolLocomotionOffset,DesiredOffset*FreePose,DeltaSeconds,14.f);
        if(bBusy || Aim>.98f)PistolLocomotionOffset=FVector::ZeroVector;
        BodyLocation+=ViewQ.RotateVector(PistolLocomotionOffset);
    }
    else PistolLocomotionOffset=FVector::ZeroVector;
    FirstPersonBody->SetWorldTransform(FTransform(BodyMeshYaw,BodyLocation,DisplayScale));
    FirstPersonBody->RefreshBoneTransforms();
    SolvedArmCount=0;MaxArmRootShift=0;MaxArmLengthError=0;MaxHandRotationErrorDegrees=0;MaxHandSourceRotationDeltaDegrees=0;
    if(bPistol){IdleArmBlend=0;bIdleArmTargetsReady=false;}
    if(!bPistol)
    {
        // Keep shoulder roots and the verified attack placement. Only relaxed
        // locomotion targets change: bent elbows near the lower side regions,
        // with opposite arms following the evaluated source leg phase. No 46cm
        // forward lock or 4cm clamp on the whole source locomotion displacement.
        const FTransform Mount=FirstPersonBody->GetComponentTransform();
        const FVector TargetDelta=Mount.InverseTransformVector(ViewQ.RotateVector(UnarmedAttackHandOffset));
        const bool bAttacking=Combat->IsAttacking() || Combat->GetWeaponMode()==EHCM4WeaponMode::Sword;
        // Finish releasing idle placement before the existing .12s windup sample.
        // Return more gently after combat ends; neither clock nor attack pose changes.
        IdleArmBlend=bPoseReady?FMath::FInterpConstantTo(IdleArmBlend,bAttacking?0.f:1.f,DeltaSeconds,bAttacking?12.5f:4.f):(bAttacking?0.f:1.f);
        const float IdleWeight=IdleArmBlend*IdleArmBlend*(3.f-2.f*IdleArmBlend);
        const FName UpperNames[2]={TEXT("upperarm_l"),TEXT("upperarm_r")};
        const FName LowerNames[2]={TEXT("lowerarm_l"),TEXT("lowerarm_r")};
        const FName HandNames[2]={TEXT("hand_l"),TEXT("hand_r")};
        FTransform OriginalUpper[2],OriginalLower[2],OriginalHand[2];
        FVector Targets[2];FQuat ExpectedWrist[2];bool Solved[2]={false,false};
        for(int32 Side=0;Side<2;++Side)
        {
            const int32 UpperIndex=FirstPersonBody->GetBoneIndex(UpperNames[Side]);
            const int32 LowerIndex=FirstPersonBody->GetBoneIndex(LowerNames[Side]);
            const int32 HandIndex=FirstPersonBody->GetBoneIndex(HandNames[Side]);
            const FName Parent=FirstPersonBody->GetParentBone(UpperNames[Side]);
            if(!FirstPersonBody->BoneSpaceTransforms.IsValidIndex(UpperIndex)||!FirstPersonBody->BoneSpaceTransforms.IsValidIndex(LowerIndex)||
                !FirstPersonBody->BoneSpaceTransforms.IsValidIndex(HandIndex)||Parent==NAME_None||
                FirstPersonBody->GetParentBone(LowerNames[Side])!=UpperNames[Side]||FirstPersonBody->GetParentBone(HandNames[Side])!=LowerNames[Side])continue;
            FTransform Upper=FirstPersonBody->GetSocketTransform(UpperNames[Side],RTS_Component);
            FTransform Lower=FirstPersonBody->GetSocketTransform(LowerNames[Side],RTS_Component);
            FTransform Hand=FirstPersonBody->GetSocketTransform(HandNames[Side],RTS_Component);
            OriginalUpper[Side]=Upper;OriginalLower[Side]=Lower;OriginalHand[Side]=Hand;
            Targets[Side]=Hand.GetLocation()+TargetDelta;
            if(!FMath::IsNearlyEqual(UnarmedAttackLateralScale,1.f))
            {
                // A full-body hook can leave a narrower FP view sideways. Fit
                // only this display target; retain forward/vertical trajectory,
                // source timing, shoulder roots, bone lengths and world attack.
                FVector TargetView=ViewQ.UnrotateVector(Mount.TransformPosition(Targets[Side])-LastView.Location);
                TargetView.Y*=FMath::Clamp(UnarmedAttackLateralScale,.1f,1.f);
                Targets[Side]=Mount.InverseTransformPosition(LastView.Location+ViewQ.RotateVector(TargetView));
            }
            const float SideSign=Side==0?-1.f:1.f;
            const float Swing=(Side==0?1.f:-1.f)*SourceStride*LocomotionMoveBlend;
            const float SwingScale=FMath::Clamp(RelaxedHandSwingScale,0.f,1.f);
            const float Run=LocomotionRunBlend;
            const float Breath=.25f*FMath::Sin(float(GetWorld()->GetTimeSeconds())*1.6f)*(1.f-LocomotionMoveBlend);
            FVector IdleCamera(
                RelaxedHandCamera.X-2.f*Run+(6.f+3.f*Run)*Swing*SwingScale,
                SideSign*(FMath::Abs(RelaxedHandCamera.Y)-1.5f*Run+.4f*FMath::Abs(Swing)),
                RelaxedHandCamera.Z+(1.5f+1.2f*Run)*Swing*SwingScale+Breath
                    +(bRelaxedAnatomicalWrists?1.2f*(1.f-FMath::Square(SourceStride))*LocomotionMoveBlend:0.f));
            if(bRelaxedAnatomicalWrists)
            {
                // Resting/walking arms hang beside the torso. Only running
                // brings a bent arm intermittently into the lower view corners.
                // Visibility at every instant is not a reason to raise both palms.
                const FVector Walk=FMath::Lerp(RelaxedHandCamera+FVector(-8,0,-8),RelaxedHandCamera,LocomotionMoveBlend);
                const FVector Base=FMath::Lerp(Walk,RelaxedRunHandCamera,Run);
                IdleCamera=FVector(Base.X+(14.f-2.f*Run)*Swing*SwingScale,
                    SideSign*FMath::Abs(Base.Y),
                    Base.Z+(8.f-2.f*Run)*FMath::Max(0.f,Swing)+Breath);
            }
            LocomotionHandCamera[Side]=IdleCamera;
            const FVector IdleTarget=Mount.InverseTransformPosition(LastView.Location+ViewQ.RotateVector(IdleCamera));
            Targets[Side]=FMath::Lerp(Targets[Side],IdleTarget,IdleWeight);
            AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Lower.GetLocation(),Targets[Side],false,1.0,1.0);
            if(bRelaxedAnatomicalWrists && bPalmBasisValid[Side] && IdleWeight>0.f)
            {
                // Neutral wrists extend the forearm instead of retaining the
                // original hanging arm's rotation after its wrist was lifted.
                // Thumb-side is camera-up. The measured local palm basis handles
                // each hand's bone axes; applying the same Euler twist would not.
                const FVector Long=(Hand.GetLocation()-Lower.GetLocation()).GetSafeNormal();
                const FVector Up=Mount.InverseTransformVectorNoScale(ViewQ.GetUpVector()).GetSafeNormal();
                FVector Across=Up-Long*FVector::DotProduct(Up,Long);
                if(!Long.IsNearlyZero() && Across.Normalize())
                {
                    const FQuat Relaxed=(FRotationMatrix::MakeFromXY(Long,Across).ToQuat()*PalmFrameInHand[Side].Inverse()).GetNormalized();
                    Hand.SetRotation(FQuat::Slerp(Hand.GetRotation(),Relaxed,IdleWeight).GetNormalized());
                }
            }
            ExpectedWrist[Side]=Hand.GetRotation();
            // IK preserves the shoulder and lengths. Only this optional idle
            // wrist correction changes orientation; all finger locals stay copied.
            // Convert the solved component transforms to this copy's local bones.
            FirstPersonBody->BoneSpaceTransforms[UpperIndex]=Upper.GetRelativeTransform(FirstPersonBody->GetSocketTransform(Parent,RTS_Component));
            FirstPersonBody->BoneSpaceTransforms[LowerIndex]=Lower.GetRelativeTransform(Upper);
            FirstPersonBody->BoneSpaceTransforms[HandIndex]=Hand.GetRelativeTransform(Lower);
            Solved[Side]=true;
        }
        bIdleArmTargetsReady=Solved[0] && Solved[1];
        if(bRelaxedAnatomicalWrists && IdleWeight>0.f && RelaxedFingerIndices.Num()==30)
        {
            // Partial authored grip, not a procedural fist. Thumb and all finger
            // joints retain their anatomical authored curl. Never enters attack
            // or pistol poses, never changes the source/world animation.
            const float CurlWeight=IdleWeight*FMath::Lerp(.25f,.4f,LocomotionRunBlend);
            for(int32 I=0;I<RelaxedFingerIndices.Num();++I)
            {
                FTransform& Local=FirstPersonBody->BoneSpaceTransforms[RelaxedFingerIndices[I]];
                Local.SetRotation(FQuat::Slerp(Local.GetRotation(),RelaxedFingerRotations[I],CurlWeight).GetNormalized());
            }
        }
        FirstPersonBody->RefreshBoneTransforms();
        for(int32 Side=0;Side<2;++Side)if(Solved[Side])
        {
            const FTransform Upper=FirstPersonBody->GetSocketTransform(UpperNames[Side],RTS_Component);
            const FTransform Lower=FirstPersonBody->GetSocketTransform(LowerNames[Side],RTS_Component);
            const FTransform Hand=FirstPersonBody->GetSocketTransform(HandNames[Side],RTS_Component);
            MaxArmRootShift=FMath::Max(MaxArmRootShift,FVector::Distance(Upper.GetLocation(),OriginalUpper[Side].GetLocation()));
            const double OldUpper=FVector::Distance(OriginalUpper[Side].GetLocation(),OriginalLower[Side].GetLocation());
            const double OldLower=FVector::Distance(OriginalLower[Side].GetLocation(),OriginalHand[Side].GetLocation());
            MaxArmLengthError=FMath::Max(MaxArmLengthError,FMath::Abs(FVector::Distance(Upper.GetLocation(),Lower.GetLocation())-OldUpper));
            MaxArmLengthError=FMath::Max(MaxArmLengthError,FMath::Abs(FVector::Distance(Lower.GetLocation(),Hand.GetLocation())-OldLower));
            MaxHandRotationErrorDegrees=FMath::Max(MaxHandRotationErrorDegrees,FMath::RadiansToDegrees(Hand.GetRotation().AngularDistance(ExpectedWrist[Side])));
            MaxHandSourceRotationDeltaDegrees=FMath::Max(MaxHandSourceRotationDeltaDegrees,FMath::RadiansToDegrees(Hand.GetRotation().AngularDistance(OriginalHand[Side].GetRotation())));
            ArmRootCamera[Side]=ViewQ.UnrotateVector(Mount.TransformPosition(Upper.GetLocation())-LastView.Location);
            ArmTargetCamera[Side]=ViewQ.UnrotateVector(Mount.TransformPosition(Targets[Side])-LastView.Location);
            ArmEndCamera[Side]=ViewQ.UnrotateVector(Mount.TransformPosition(Hand.GetLocation())-LastView.Location);
            ArmTargetError[Side]=FVector::Distance(Hand.GetLocation(),Targets[Side]);
            ++SolvedArmCount;
        }
    }
    DisplayEyeWorld=FirstPersonBody->GetComponentTransform().TransformPosition(DisplayEyeAnchor);
    bPoseReady=true;
    if(!bPistol)return;
    const FTransform Hand=FirstPersonBody->GetSocketTransform(TEXT("hand_r"),RTS_World);
    const FTransform Grip(FRotator(-8.21222212,176.32626190,1.13691458),FVector(-4.63102409,1.82995991,-2.16906953));
    FTransform LegacyGun=Grip*Hand;
    LegacyGun.SetScale3D(FVector::OneVector);
    const bool bOfficialGrip=FirstPersonBody->DoesSocketExist(TEXT("HandGrip_R"));
    // Installed first-person template attaches this same SKM_Pistol directly to HandGrip_R.
    // The mesh's +Y forward is already accounted for by the authored grip socket.
    FTransform Gun=FirstPersonPistol && bOfficialGrip?FirstPersonBody->GetSocketTransform(TEXT("HandGrip_R"),RTS_World):LegacyGun;
    Gun.SetScale3D(FVector::OneVector);
    FVector Muzzle=Gun.TransformPosition(FVector(17.5,0,4.45));
    if(FirstPersonFallbackPistol)FirstPersonFallbackPistol->SetWorldTransform(LegacyGun);
    if(FirstPersonPistol)
    {
        FirstPersonPistol->SetWorldTransform(Gun);
        HCM4R2PistolPresentation::Update(FirstPersonPistol,Combat->GetShotPresentationPulse(),Combat->GetReloadProgress());
        if(FirstPersonPistol->DoesSocketExist(TEXT("Muzzle")))Muzzle=FirstPersonPistol->GetSocketLocation(TEXT("Muzzle"));
    }
    if(FlashPlaneA)
    {
        const FQuat GunQ=FirstPersonPistol?FirstPersonPistol->GetSocketQuaternion(TEXT("Muzzle")):Gun.GetRotation();
        FlashPlaneA->SetWorldTransform(FTransform(GunQ*FRotator(0,0,0).Quaternion(),Muzzle+GunQ.RotateVector(FVector(4,0,0)),FVector(.15f,.07f,1)));
        FlashPlaneB->SetWorldTransform(FTransform(GunQ*FRotator(0,0,90).Quaternion(),Muzzle+GunQ.RotateVector(FVector(4,0,0)),FVector(.15f,.07f,1)));
        // Two lengthwise planes are edge-on along the barrel; this radial cap is
        // perpendicular to the actual socket axis and shares the same shot pulse.
        if(FlashCap)FlashCap->SetWorldTransform(FTransform(GunQ*FlashCapLocalRotation,Muzzle+GunQ.GetForwardVector()*2.f,FVector(.10f)));
    }
}
FString UHCM4R2PresentationComponent::GetPresentationDiagnostics() const
{
    return FString::Printf(TEXT("{\"first_person_active\":%s,\"body_asset\":%s,\"articulated_pistol\":%s,\"flash_asset\":%s,\"radial_flash_cap\":%s,\"presented_shots\":%d,\"last_shot_id\":%d,\"wall_retraction\":%.5f,\"display_eye_mesh\":\"%s\",\"display_eye_world\":\"%s\",\"view_pitch\":%.6f,\"forearms_asset_loaded\":%s,\"display_mesh\":\"%s\",\"forced_lod\":%d}"),
        bActive?TEXT("true"):TEXT("false"),FirstPersonBody?TEXT("true"):TEXT("false"),FirstPersonPistol?TEXT("true"):TEXT("false"),
        FlashPlaneA?TEXT("true"):TEXT("false"),FlashCap?TEXT("true"):TEXT("false"),PresentedShots,LastShotId,WallRetraction,*DisplayEyeAnchor.ToString(),*DisplayEyeWorld.ToString(),LastView.Rotation.Pitch,FirstPersonArmsAsset?TEXT("true"):TEXT("false"),
        FirstPersonBody?*GetPathNameSafe(FirstPersonBody->GetSkinnedAsset()):TEXT("missing"),FirstPersonBody?FirstPersonBody->GetForcedLOD():-1);
}

bool UHCM4R2PresentationComponent::HasValidUnarmedArmIK() const
{
    return SolvedArmCount==2 && MaxArmRootShift<.01 && MaxArmLengthError<.01 && MaxHandRotationErrorDegrees<.02
        && (!bRelaxedAnatomicalWrists || (bPalmBasisValid[0] && bPalmBasisValid[1] && RelaxedFingerIndices.Num()==30));
}
FString UHCM4R2PresentationComponent::GetUnarmedArmIKDiagnostics() const
{
    return FString::Printf(TEXT("solved=%d maxRootShiftCm=%.6f maxLengthErrorCm=%.6f maxHandRotationErrorDeg=%.6f L.rootCamera=(%s) L.targetCamera=(%s) L.handCamera=(%s) L.targetResidualCm=%.6f R.rootCamera=(%s) R.targetCamera=(%s) R.handCamera=(%s) R.targetResidualCm=%.6f idleBlend=%.6f locomotionTargetsReady=%d"),
        SolvedArmCount,MaxArmRootShift,MaxArmLengthError,MaxHandRotationErrorDegrees,*ArmRootCamera[0].ToString(),*ArmTargetCamera[0].ToString(),*ArmEndCamera[0].ToString(),ArmTargetError[0],
        *ArmRootCamera[1].ToString(),*ArmTargetCamera[1].ToString(),*ArmEndCamera[1].ToString(),ArmTargetError[1],IdleArmBlend,bIdleArmTargetsReady?1:0);
}

FString UHCM4R2PresentationComponent::GetGeometryDiagnostics() const
{
    // Called by bounded tests/explicit diagnostics only; never per-frame logging.
    auto R=MakeShared<FJsonObject>();
    R->SetStringField(TEXT("ik"),GetUnarmedArmIKDiagnostics());
    R->SetStringField(TEXT("unarmed_attack_hand_offset_view_cm"),UnarmedAttackHandOffset.ToString());
    R->SetNumberField(TEXT("unarmed_attack_lateral_scale"),UnarmedAttackLateralScale);
    R->SetBoolField(TEXT("relaxed_anatomical_wrists"),bRelaxedAnatomicalWrists);
    R->SetBoolField(TEXT("both_anatomical_palm_bases_valid"),bPalmBasisValid[0] && bPalmBasisValid[1]);
    R->SetNumberField(TEXT("relaxed_hand_swing_scale"),RelaxedHandSwingScale);
    R->SetNumberField(TEXT("relaxed_finger_pose_bones"),RelaxedFingerIndices.Num());
    R->SetStringField(TEXT("relaxed_finger_pose_asset"),RelaxedFingerPose.ToString());
    R->SetNumberField(TEXT("max_wrist_delta_from_source_degrees"),MaxHandSourceRotationDeltaDegrees);
    R->SetStringField(TEXT("wrist_rotation_error_reference"),TEXT("expected final wrist: authored attack, optionally blended anatomical relaxed orientation"));
    R->SetNumberField(TEXT("measured_skinning_radius_cm"),FirstPersonSkinningRadiusCm);
    if(FirstPersonBody)
    {
        R->SetStringField(TEXT("display_component_transform"),FirstPersonBody->GetComponentTransform().ToString());
        R->SetStringField(TEXT("display_bounds_origin"),FirstPersonBody->Bounds.Origin.ToString());
        R->SetStringField(TEXT("display_bounds_extent"),FirstPersonBody->Bounds.BoxExtent.ToString());
        R->SetNumberField(TEXT("display_bounds_radius"),FirstPersonBody->Bounds.SphereRadius);
        R->SetBoolField(TEXT("display_visible"),FirstPersonBody->IsVisible());
        R->SetBoolField(TEXT("display_owner_no_see"),FirstPersonBody->bOwnerNoSee);
        R->SetBoolField(TEXT("display_only_owner_see"),FirstPersonBody->bOnlyOwnerSee);
        R->SetStringField(TEXT("display_physics_asset"),GetPathNameSafe(FirstPersonBody->GetPhysicsAsset()));
        for(FName Bone:{FName("upperarm_l"),FName("lowerarm_l"),FName("hand_l"),FName("upperarm_r"),FName("lowerarm_r"),FName("hand_r")})
        {
            const FVector Point=FirstPersonBody->GetSocketLocation(Bone);
            R->SetStringField(Bone.ToString()+TEXT("_world"),Point.ToString());
            R->SetBoolField(Bone.ToString()+TEXT("_inside_bounds"),FirstPersonBody->Bounds.GetBox().IsInsideOrOn(Point));
        }
    }
    if(Character && Character->GetMesh())
    {
        const FVector Bottom=Character->GetActorLocation()-FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        R->SetStringField(TEXT("capsule_bottom"),Bottom.ToString());
        R->SetStringField(TEXT("actor_location"),Character->GetActorLocation().ToString());
        for(FName Bone:{FName("foot_l"),FName("foot_r")})
        {
            const FTransform Foot=Character->GetMesh()->GetSocketTransform(Bone,RTS_World);
            FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(M5FootDiagnostic),false,Character);
            const bool Found=GetWorld()->LineTraceSingleByChannel(Hit,Foot.GetLocation()+FVector(0,0,80),Foot.GetLocation()-FVector(0,0,220),ECC_Visibility,Query);
            R->SetStringField(Bone.ToString()+TEXT("_world_transform"),Foot.ToString());
            R->SetNumberField(Bone.ToString()+TEXT("_height_above_capsule_bottom"),Foot.GetLocation().Z-Bottom.Z);
            R->SetBoolField(Bone.ToString()+TEXT("_ground_hit"),Found);
            if(Found)
            {
                R->SetNumberField(Bone.ToString()+TEXT("_bone_to_ground_z_cm"),Foot.GetLocation().Z-Hit.ImpactPoint.Z);
                R->SetStringField(Bone.ToString()+TEXT("_ground_actor"),GetNameSafe(Hit.GetActor()));
            }
        }
    }
    R->SetBoolField(TEXT("head_accessory_exists"),HeadAccessory!=nullptr);
    if(HeadAccessory)
    {
        R->SetStringField(TEXT("head_accessory_mesh"),GetPathNameSafe(HeadAccessory->GetStaticMesh()));
        R->SetStringField(TEXT("head_accessory_world"),HeadAccessory->GetComponentTransform().ToString());
        R->SetStringField(TEXT("head_accessory_socket"),HeadAccessory->GetAttachSocketName().ToString());
        R->SetBoolField(TEXT("head_accessory_owner_no_see"),HeadAccessory->bOwnerNoSee);
        R->SetBoolField(TEXT("head_accessory_hidden"),HeadAccessory->bHiddenInGame);
    }
    FString Result;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Result));return Result;
}

FString UHCM4R2PresentationComponent::GetLocomotionPresentationDiagnostics() const
{
    return FString::Printf(TEXT("{\"profile\":\"M5VS1_source_stride_bent_elbows_v1\",\"speed_cm_s\":%.6f,\"grounded\":%s,\"move_blend\":%.6f,\"run_blend\":%.6f,\"source_leg_phase\":%.6f,\"relaxed_pose_weight\":%.6f,\"left_target_camera\":\"%s\",\"right_target_camera\":\"%s\",\"pistol_offset_camera\":\"%s\",\"arms_match_source\":%s,\"source_mesh\":\"%s\",\"arms_mesh\":\"%s\",\"camera_motion_added\":false}"),
        LocomotionSpeed,bLocomotionGrounded?TEXT("true"):TEXT("false"),LocomotionMoveBlend,LocomotionRunBlend,SourceStride,IdleArmBlend,
        *LocomotionHandCamera[0].ToString(),*LocomotionHandCamera[1].ToString(),*PistolLocomotionOffset.ToString(),bArmsPairedToSource?TEXT("true"):TEXT("false"),
        *GetPathNameSafe(ResolvedArmsSource),*GetPathNameSafe(FirstPersonArmsAsset));
}

bool UHCM4R2PresentationComponent::RequestFinalPoseDiagnostic(const FString& CaptureLabel)
{
#if WITH_EDITOR
    // Explicit test requests only. No ticking/geometry overhead in ordinary play.
    if(!Character || CaptureLabel.IsEmpty() || CaptureLabel.Len()>96 ||
        !PendingFinalPoseLabel.IsEmpty() || FinalPoseRequestCount>=16)return false;
    for(const TCHAR C:CaptureLabel)
        if(!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT('-'))return false;
    ++FinalPoseRequestCount;
    PendingFinalPoseLabel=CaptureLabel;FinalPoseRequestFrame=GFrameCounter;
    FinalPoseSnapshotFrame=0;FinalPoseSnapshot.Reset();LastFinalPoseDiagnostic.Empty();
    ScreenshotProcessedDelegate=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this,&ThisClass::OnDiagnosticScreenshotProcessed);
    return true;
#else
    // CPU render buffers are not assumed to be retained by a cooked mesh.
    return false;
#endif
}

void UHCM4R2PresentationComponent::SampleRequestedFinalPose()
{
    if(GFrameCounter>FinalPoseRequestFrame+3)
    { FinishFinalPoseDiagnostic(false);return; }
    auto R=MakeShared<FJsonObject>();FinalPoseSnapshot=R;FinalPoseSnapshotFrame=GFrameCounter;
    R->SetStringField(TEXT("label"),PendingFinalPoseLabel);
    R->SetNumberField(TEXT("request_frame"),double(FinalPoseRequestFrame));
    R->SetNumberField(TEXT("snapshot_frame"),double(FinalPoseSnapshotFrame));
    R->SetStringField(TEXT("sampling_stage"),TEXT("Presentation TG_PostUpdateWork after UpdatePose/RefreshBoneTransforms"));
    R->SetStringField(TEXT("scope"),TEXT("Current display-pose LBS; excludes morph/WPO, raster occlusion, material visibility and temporal effects. Screenshot frame equality is checked separately."));
    R->SetStringField(TEXT("screenshot_filename"),FScreenshotRequest::GetFilename());
    R->SetBoolField(TEXT("screenshot_pending_at_sample"),FScreenshotRequest::IsScreenshotRequested());
    R->SetBoolField(TEXT("screenshot_name_matches"),FPaths::GetBaseFilename(FScreenshotRequest::GetFilename())==PendingFinalPoseLabel);
    R->SetBoolField(TEXT("display_pose_ready"),bPoseReady);
    R->SetBoolField(TEXT("first_person_active"),bActive);
    R->SetStringField(TEXT("geometry"),GetGeometryDiagnostics());
    R->SetStringField(TEXT("display_view_location"),LastView.Location.ToString());
    R->SetStringField(TEXT("display_view_rotation"),LastView.Rotation.ToString());
    auto* PC=Character?Cast<AHCM1PlayerController>(Character->GetController()):nullptr;
    const FMinimalViewInfo POV=PC && PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraCacheView():LastView;
    int32 Width=0,Height=0;if(PC)PC->GetViewportSize(Width,Height);
    R->SetNumberField(TEXT("viewport_width"),Width);R->SetNumberField(TEXT("viewport_height"),Height);
    R->SetStringField(TEXT("camera_location"),POV.Location.ToString());
    R->SetStringField(TEXT("camera_rotation"),POV.Rotation.ToString());
    R->SetNumberField(TEXT("camera_fov"),POV.FOV);
    R->SetNumberField(TEXT("first_person_fov"),POV.FirstPersonFOV);
    R->SetNumberField(TEXT("first_person_scale"),POV.FirstPersonScale);
    R->SetNumberField(TEXT("display_view_camera_position_delta_cm"),FVector::Dist(POV.Location,LastView.Location));
    R->SetNumberField(TEXT("display_view_camera_rotation_delta_degrees"),FMath::RadiansToDegrees(POV.Rotation.Quaternion().AngularDistance(LastView.Rotation.Quaternion())));
    if(auto* Combat=Character?Character->GetCombatComponent():nullptr)
    {
        R->SetNumberField(TEXT("combo_index"),Combat->GetComboIndex());
        R->SetNumberField(TEXT("attack_elapsed"),Combat->GetAttackElapsed());
        R->SetNumberField(TEXT("attack_animation_elapsed"),Combat->GetAttackAnimationElapsed());
    }
    if(auto* Source=Character?Character->GetMesh():nullptr)
    {
        R->SetStringField(TEXT("source_mesh"),GetPathNameSafe(Source->GetSkeletalMeshAsset()));
        if(auto* Anim=Source->GetAnimInstance())
        {
            auto* Montage=Anim->GetCurrentActiveMontage();
            R->SetStringField(TEXT("anim_class"),GetPathNameSafe(Anim->GetClass()));
            R->SetStringField(TEXT("active_montage"),GetPathNameSafe(Montage));
            if(Montage)R->SetNumberField(TEXT("montage_position"),Anim->Montage_GetPosition(Montage));
        }
    }
    R->SetStringField(TEXT("cpu_skin_status"),TEXT("NOT_RUN"));
#if WITH_EDITOR
    const auto* Asset=FirstPersonBody?Cast<USkeletalMesh>(FirstPersonBody->GetSkinnedAsset()):nullptr;
    const auto* Data=Asset?Asset->GetResourceForRendering():nullptr;
    auto* Weights=FirstPersonBody?FirstPersonBody->GetSkinWeightBuffer(0):nullptr;
    if(bPoseReady && bActive && Data && !Data->LODRenderData.IsEmpty() && Weights && Width>0 && Height>0)
    {
        TArray<FMatrix44f> Matrices;FirstPersonBody->GetCurrentRefToLocalMatrices(Matrices,0);
        TArray<FVector3f> Vertices;
        USkinnedMeshComponent::ComputeSkinnedPositions(FirstPersonBody,Vertices,Matrices,Data->LODRenderData[0],*Weights);
        const double TanHalf=FMath::Tan(FMath::DegreesToRadians(POV.FirstPersonFOV*.5));
        FBox ViewBounds(ForceInit);FBox2D ProjectedBounds(ForceInit);
        int32 Front=0,Inside=0;
        for(const FVector3f& Vertex:Vertices)
        {
            const FVector P=POV.Rotation.UnrotateVector(FirstPersonBody->GetComponentTransform().TransformPosition(FVector(Vertex))-POV.Location);
            ViewBounds+=P;
            if(P.X<=.01 || TanHalf<=0)continue;
            ++Front;const FVector2D NDC(P.Y/(P.X*TanHalf),P.Z/(P.X*TanHalf)*Width/Height);
            ProjectedBounds+=NDC;
            if(FMath::Abs(NDC.X)<=1 && FMath::Abs(NDC.Y)<=1)++Inside;
        }
        R->SetStringField(TEXT("cpu_skin_status"),Vertices.IsEmpty()?TEXT("FAIL_EMPTY"):TEXT("SAMPLED"));
        R->SetStringField(TEXT("display_mesh"),GetPathNameSafe(Asset));
        R->SetNumberField(TEXT("lod"),0);R->SetNumberField(TEXT("ref_to_local_matrices"),Matrices.Num());
        R->SetNumberField(TEXT("vertices"),Vertices.Num());R->SetNumberField(TEXT("vertices_in_front"),Front);
        R->SetNumberField(TEXT("vertices_inside_fp_projection"),Inside);
        if(!Vertices.IsEmpty())
        {
            R->SetStringField(TEXT("view_bounds_min"),ViewBounds.Min.ToString());
            R->SetStringField(TEXT("view_bounds_max"),ViewBounds.Max.ToString());
        }
        if(Front)
        {
            R->SetStringField(TEXT("ndc_bounds_min"),ProjectedBounds.Min.ToString());
            R->SetStringField(TEXT("ndc_bounds_max"),ProjectedBounds.Max.ToString());
        }
        R->SetBoolField(TEXT("display_recently_rendered"),FirstPersonBody->WasRecentlyRendered(.2f));
    }
#endif
}

void UHCM4R2PresentationComponent::OnDiagnosticScreenshotProcessed()
{
    if(!PendingFinalPoseLabel.IsEmpty())FinishFinalPoseDiagnostic(true);
}

void UHCM4R2PresentationComponent::FinishFinalPoseDiagnostic(bool bScreenshotProcessed)
{
    auto R=FinalPoseSnapshot.IsValid()?FinalPoseSnapshot:MakeShared<FJsonObject>();
    R->SetStringField(TEXT("label"),PendingFinalPoseLabel);
    R->SetNumberField(TEXT("request_frame"),double(FinalPoseRequestFrame));
    R->SetNumberField(TEXT("processed_frame"),double(GFrameCounter));
    R->SetBoolField(TEXT("screenshot_processed"),bScreenshotProcessed);
    const bool bSameFrame=bScreenshotProcessed && FinalPoseSnapshot.IsValid() && FinalPoseSnapshotFrame==GFrameCounter;
    R->SetBoolField(TEXT("snapshot_matches_processed_frame"),bSameFrame);
    R->SetStringField(TEXT("completion"),bSameFrame?TEXT("SAME_FRAME_OBSERVATION"):bScreenshotProcessed?TEXT("FAIL_FRAME_MISMATCH"):TEXT("FAIL_SCREENSHOT_TIMEOUT"));
    // This completion delegate runs after the standard PNG save path, and does
    // not replace it as OnScreenshotCaptured would. It is not proof of pixels.
    LastFinalPoseDiagnostic.Empty();
    FJsonSerializer::Serialize(R,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&LastFinalPoseDiagnostic));
    UE_LOG(LogTemp,Display,TEXT("M5VS1_FINAL_POSE_DIAGNOSTIC %s"),*LastFinalPoseDiagnostic);
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedDelegate);
    ScreenshotProcessedDelegate.Reset();PendingFinalPoseLabel.Empty();FinalPoseSnapshot.Reset();
}
