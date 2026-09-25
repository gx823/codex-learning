#include "HCM5VS3Abilities.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M3/HCM3NPC.h"
#include "M4/HCM4CombatComponent.h"
#include "M4/HCM4DamageTypes.h"
#include "M4/HCM4Weapon.h"
#include "M4R1/HCM4R1PhysicalReactionComponent.h"
#include "M4R2/HCM4R2PresentationComponent.h"
#include "M5/HCM5MusicComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace {
const TCHAR* AssetRoot=TEXT("/Game/HarborCity/M5VS3/Combat/");
const TCHAR* RuneNames[]={TEXT("SM_RuneBolt"),TEXT("SM_RuneWave"),TEXT("SM_RuneHeal"),TEXT("SM_RuneShield")};
void PlayMagicSound(AHCM1Character* Hero,int32 Kind){const FString N=FString::Printf(TEXT("SW_VS3_Magic%d"),Kind);if(auto* S=LoadObject<USoundBase>(nullptr,*(FString(AssetRoot)+N+TEXT(".")+N)))UGameplayStatics::PlaySoundAtLocation(Hero,S,Hero->GetActorLocation(),.55f);}
UStaticMesh* MeshAsset(const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*(FString(AssetRoot)+Name+TEXT(".")+Name));}
UMaterialInterface* MagicMaterial(int32 Kind){const FString N=FString::Printf(TEXT("M_Magic%d"),FMath::Clamp(Kind,0,3));return LoadObject<UMaterialInterface>(nullptr,*(FString(AssetRoot)+N+TEXT(".")+N));}
UStaticMeshComponent* AddMesh(AActor* Owner,const TCHAR* Name){auto* M=NewObject<UStaticMeshComponent>(Owner,FName(Name));Owner->AddInstanceComponent(M);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetGenerateOverlapEvents(false);M->SetCanEverAffectNavigation(false);M->SetupAttachment(Owner->GetRootComponent());M->RegisterComponent();return M;}
FName Hand(USkinnedMeshComponent* Mesh){return Mesh->GetBoneIndex(TEXT("Hand_R"))!=INDEX_NONE?TEXT("Hand_R"):TEXT("hand_r");}
}
UHCM5VS3Abilities::UHCM5VS3Abilities(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;}
void UHCM5VS3Abilities::BeginPlay(){
    Super::BeginPlay();Hero=Cast<AHCM1Character>(GetOwner());
    bEnabled=Hero&&GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/"));
    if(!bEnabled){SetComponentTickEnabled(false);return;}
    Combat=Hero->GetCombatComponent();Combat->OnAcceptedShot.AddUObject(this,&ThisClass::AcceptedShot);
    AddTickPrerequisiteComponent(Hero->GetMesh());AddTickPrerequisiteComponent(Hero->GetR2PresentationComponent());
    Sword=AddMesh(Hero,TEXT("VS3Sword"));Sword->SetStaticMesh(MeshAsset(TEXT("SM_Sword")));
    FirstSword=AddMesh(Hero,TEXT("VS3FirstSword"));FirstSword->SetStaticMesh(Sword->GetStaticMesh());FirstSword->SetOnlyOwnerSee(true);FirstSword->SetCastShadow(false);
    FirstSword->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    MagicGun=AddMesh(Hero,TEXT("VS3MagicGun"));MagicGun->SetStaticMesh(MeshAsset(TEXT("SM_MagicGun")));
    FirstMagicGun=AddMesh(Hero,TEXT("VS3FirstMagicGun"));FirstMagicGun->SetStaticMesh(MagicGun->GetStaticMesh());FirstMagicGun->SetOnlyOwnerSee(true);FirstMagicGun->SetCastShadow(false);FirstMagicGun->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    CastCircle=AddMesh(Hero,TEXT("VS3CastingCircle"));CastCircle->SetCastShadow(false);CastCircle->SetVisibility(false);
    for(int32 Layer=0;Layer<3;++Layer){auto* M=Layer==2?CastCircle.Get():AddMesh(Hero,*FString::Printf(TEXT("VS3RuneLayer%d"),Layer));M->SetCastShadow(false);M->SetVisibility(false);RuneLayers.Add(M);}
    for(const TCHAR* Kind:{TEXT("Bolt"),TEXT("Wave"),TEXT("Heal"),TEXT("Shield")})
        for(const TCHAR* Layer:{TEXT("Outer"),TEXT("Inner"),TEXT("Core")})RuneMeshes.Add(MeshAsset(*FString::Printf(TEXT("SM_Arcane%s_%s"),Kind,Layer)));
    for(int32 Kind=0;Kind<4;++Kind){const FString N=FString::Printf(TEXT("M_ArcaneFiligree%d"),Kind);RuneSurfaces.Add(LoadObject<UMaterialInterface>(nullptr,*(FString(AssetRoot)+N+TEXT(".")+N)));}
}
void UHCM5VS3Abilities::EndPlay(const EEndPlayReason::Type Reason){if(Combat)Combat->OnAcceptedShot.RemoveAll(this);Super::EndPlay(Reason);}
void UHCM5VS3Abilities::ResetTransient(){bHeld=false;SwingTime=0;CastKind=-1;RuneKind=-1;bBladeSample=false;if(Hero)Hero->SetCombatMovementScale(1);for(const auto& M:RuneLayers)if(M)M->SetVisibility(false);}
void UHCM5VS3Abilities::PlayPose(const TCHAR* Name,float Duration){
    auto* Seq=LoadObject<UAnimSequence>(nullptr,*(FString(AssetRoot)+Name+TEXT(".")+Name));
    if(Seq&&Hero->GetMesh()->GetAnimInstance())SwingMontage=Hero->GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Seq,TEXT("FullBody"),.10f,.14f,Seq->GetPlayLength()/Duration);
}
void UHCM5VS3Abilities::WeaponChanged(){if(!bEnabled)return;ResetTransient();PlayPose(Combat->GetWeaponMode()==EHCM4WeaponMode::Sword?TEXT("A_SwordDraw"):TEXT("A_SwordSheath"),.55f);}
bool UHCM5VS3Abilities::PressSword(){
    if(!bEnabled||!Combat->CanUseCombat()||IsBusy()||Combat->IsReloading())return false;
    bHeld=true;HoldTime=0;IdleMP=0;Hero->SetCombatMovementScale(.4f);Hero->FaceBodyYawOnce(Hero->GetControlRotation().Yaw,1.5f);PlayPose(TEXT("A_SwordCharge"),1.f);return true;
}
void UHCM5VS3Abilities::ReleaseSword(){if(!bHeld)return;bHeld=false;if(Combat->CanUseCombat())StartSwing(HoldTime>=.55f);else ResetTransient();}
void UHCM5VS3Abilities::StartSwing(bool Heavy){
    PlayMagicSound(Hero,4);
    bHeavy=Heavy;Combo=Heavy?2:LastSwingAge<1.2f?(Combo+1)%3:0;SwingDuration=Heavy?1.05f:.72f;SwingTime=SwingDuration;LastSwingAge=0;SwingHits.Reset();bBladeSample=false;bContact=false;
    const FString N=Heavy?TEXT("A_SwordHeavy"):FString::Printf(TEXT("A_Sword%d"),Combo+1);PlayPose(*N,SwingDuration);
}
void UHCM5VS3Abilities::SwordTrace(){
    if(!Sword||!Sword->GetStaticMesh())return;
    const FTransform T=Sword->GetComponentTransform();const FVector Base=T.TransformPosition(FVector(14,0,0)),Tip=T.TransformPosition(FVector(102,0,0));
    if(!bBladeSample){PreviousBase=Base;PreviousTip=Tip;bBladeSample=true;return;}
    FCollisionQueryParams Q(SCENE_QUERY_STAT(VS3Sword),false,Hero);Q.bReturnPhysicalMaterial=true;
    // Six samples cover the whole actual blade and its displacement, never an aim-ray shortcut.
    for(int32 I=0;I<=5&&!bContact;++I){const float A=I/5.f;FHitResult H;
        if(GetWorld()->SweepSingleByChannel(H,FMath::Lerp(PreviousBase,PreviousTip,A),FMath::Lerp(Base,Tip,A),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(7),Q)){
            bContact=true;
            if(auto* NPC=Cast<AHCM3NPC>(H.GetActor());NPC&&!SwingHits.Contains(NPC)){
                SwingHits.Add(NPC);UGameplayStatics::ApplyPointDamage(NPC,bHeavy?55.f:Combo==2?35.f:24.f,(Tip-PreviousTip).GetSafeNormal(),H,Hero->GetController(),Hero,UHCM4MeleeDamageType::StaticClass());
            }
            // Stop the advance at first surface; immediate blend to recovery avoids follow-through through torsos/walls.
            if(auto* Anim=Hero->GetMesh()->GetAnimInstance();Anim&&SwingMontage)Anim->Montage_Stop(.10f,SwingMontage);
            SwingTime=FMath::Min(SwingTime,.15f);
        }
    }
    if(!bContact)AHCM5VS3SpellFX::Spawn(Hero,Tip,FVector::ZeroVector,0,.12f,0,false);
    PreviousBase=Base;PreviousTip=Tip;
}
bool UHCM5VS3Abilities::CastSpell(int32 Kind){
    const float Cost[]={15,35,30,25};
    if(!bEnabled||Kind<0||Kind>3||!Combat->CanUseCombat()||IsBusy()||Combat->IsAttacking()||Combat->IsReloading())return false;
    auto* PC=Cast<AHCM1PlayerController>(Hero->GetController());
    if(Cooldown[Kind]>0||MP<Cost[Kind]){if(PC)PC->ShowStatusMessage(TEXT("魔力不足或技能冷却中。"),1.5f);return false;}
    MP-=Cost[Kind];IdleMP=0;CastKind=Kind;CastTime=Kind==1?.85f:.7f;Combat->SetAimHeld(false);Hero->SetCombatMovementScale(.4f);
    RuneKind=Kind;RuneAge=0;RuneCastDuration=CastTime;RuneInstances.Reset();
    for(int32 I=0;I<3;++I){RuneLayers[I]->SetStaticMesh(RuneMeshes[Kind*3+I]);auto* MID=UMaterialInstanceDynamic::Create(RuneSurfaces[Kind],this);RuneInstances.Add(MID);RuneLayers[I]->SetMaterial(0,MID);MID->SetScalarParameterValue(TEXT("Reveal"),0);RuneLayers[I]->SetVisibility(true);}
    PlayPose(TEXT("A_Cast"),CastTime+.25f);return true;
}
void UHCM5VS3Abilities::ReleaseSpell(){
    const float CD[]={1,6,10,12};const int32 Kind=CastKind;Cooldown[Kind]=CD[Kind];CastKind=-1;IdleMP=0;Hero->SetCombatMovementScale(1);
    auto* PC=Cast<AHCM1PlayerController>(Hero->GetController());if(!PC)return;
    PlayMagicSound(Hero,Kind);
    const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(),Dir=PC->PlayerCameraManager->GetCameraRotation().Vector();
    if(Kind==0){
        FCollisionQueryParams Q(SCENE_QUERY_STAT(VS3MagicAim),false,Hero);FHitResult H;const FVector Far=Eye+Dir*10000;
        GetWorld()->LineTraceSingleByChannel(H,Eye,Far,ECC_Visibility,Q);const FVector Start=Hero->GetActorLocation()+FVector(0,0,45)+Hero->GetActorForwardVector()*45;
        AHCM5VS3SpellFX::Spawn(Hero,Start,((H.bBlockingHit?H.ImpactPoint:Far)-Start).GetSafeNormal()*1800,30,5,0);
    } else if(Kind==1){
        const FVector Center=Hero->GetActorLocation()+FRotator(0,Hero->GetControlRotation().Yaw,0).Vector()*150;
        AHCM5VS3SpellFX::Spawn(Hero,Center-FVector(0,0,75),FVector::ZeroVector,0,.7f,1,false);
        for(TActorIterator<AHCM3NPC> It(GetWorld());It;++It){if(FVector::Dist(It->GetActorLocation(),Center)>230)continue;
            FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(VS3AOEOcclusion),false,Hero);
            if(GetWorld()->LineTraceSingleByChannel(Hit,Center+FVector(0,0,40),It->GetActorLocation(),ECC_Visibility,Q)&&Hit.GetActor()!=*It)continue;
            const FVector Away=(It->GetActorLocation()-Center).GetSafeNormal2D();
            UGameplayStatics::ApplyDamage(*It,28,PC,Hero,UHCM4MeleeDamageType::StaticClass());
            if(!It->IsDead() && !It->GetPhysicalReaction()->TryAnimatedKnockdown(Center))It->LaunchCharacter(Away*280+FVector(0,0,140),false,false);
        }
    } else if(Kind==2)HealRemaining=3;else ShieldRemaining=5;
    UE_LOG(LogTemp,Display,TEXT("VS3_SPELL_RELEASE kind=%d mp=%.2f"),Kind,MP);
}
void UHCM5VS3Abilities::AcceptedShot(const FHCM4AcceptedShot& Shot){
    IdleMP=0;const FVector Delta=Shot.AimPoint-Shot.WorldMuzzle;AHCM5VS3SpellFX::Spawn(Hero,Shot.WorldMuzzle,Delta.GetSafeNormal()*3000,0,FMath::Min(1.f,Delta.Size()/3000.f),0);
    AHCM5VS3SpellFX::Spawn(Hero,Shot.WorldMuzzle,FVector::ZeroVector,0,.1f,0,false);
}
void UHCM5VS3Abilities::UpdateVisuals(float Dt){
    auto* PC=Cast<AHCM1PlayerController>(Hero->GetController());if(!PC)return;const bool Equipped=Combat->GetWeaponMode()==EHCM4WeaponMode::Sword;
    const bool Foot=PC->GetPlayerMode()==EHCPlayerMode::OnFoot;const bool FP=PC->IsFirstPersonPerspective();
    auto* Body=Hero->GetMesh();FTransform Mount;
    if(Equipped){Mount=Body->GetSocketTransform(Body->DoesSocketExist(TEXT("HandGrip_R"))?FName(TEXT("HandGrip_R")):Hand(Body));Mount.SetScale3D(FVector(1));Mount=FTransform(FRotator(70,0,0))*Mount;}
    else {Mount=FTransform(FRotator(0,Hero->GetActorRotation().Yaw+50,145),Hero->GetActorLocation()-Hero->GetActorForwardVector()*22+FVector(0,0,20));}
    Sword->SetWorldTransform(Mount);Sword->SetVisibility(Foot&&!FP);
    auto* Arms=Hero->GetR2PresentationComponent()->GetFirstPersonMesh();FirstSword->SetVisibility(Foot&&FP&&Equipped&&Arms);
    if(Arms){FTransform FM=FTransform(FRotator(70,0,0))*Arms->GetSocketTransform(Arms->DoesSocketExist(TEXT("HandGrip_R"))?FName(TEXT("HandGrip_R")):Hand(Arms));FM.SetScale3D(FVector(1));FirstSword->SetWorldTransform(FM);}
    const bool Gun=Foot&&Combat->GetWeaponMode()==EHCM4WeaponMode::Pistol;
    MagicGun->SetVisibility(Gun&&!FP);FirstMagicGun->SetVisibility(Gun&&FP);
    if(auto* Weapon=Combat->GetWeaponActor()){
        MagicGun->SetWorldTransform(Weapon->GetActorTransform());Weapon->GetWeaponMesh()->SetVisibility(false);
        if(Weapon->GetArticulatedMesh())Weapon->GetArticulatedMesh()->SetVisibility(false);
    }
    if(auto* FPWeapon=Hero->GetR2PresentationComponent()->GetFirstPersonPistol()){
        FirstMagicGun->SetWorldTransform(FPWeapon->GetComponentTransform());FPWeapon->SetVisibility(false);
    }
    if(RuneKind>=0){
        RuneAge+=Dt;const FRotator V=PC->PlayerCameraManager->GetCameraRotation();const float Fade=1-FMath::SmoothStep(RuneCastDuration,RuneCastDuration+.38f,RuneAge);
        for(int32 I=0;I<3;++I){
            const float Reveal=FMath::SmoothStep(I*.045f,I*.045f+.17f,RuneAge)*Fade;
            // Eye-space casting ornament: fit the actual FOV, with a clear aiming
            // corridor. This does not change the gameplay camera or world traces.
            const float Speed[]={7,-16,23};
            const float Depth=75+I*1.5f;
            const float HalfWidth=Depth*FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5f));
            const FVector Offset(Depth,HalfWidth*.40f,-HalfWidth*.06f);
            RuneLayers[I]->SetWorldLocation(PC->PlayerCameraManager->GetCameraLocation()+V.RotateVector(Offset));
            RuneLayers[I]->SetWorldRotation(V+FRotator(0,0,RuneAge*Speed[I]));
            RuneLayers[I]->SetWorldScale3D(FVector(HalfWidth*.30f/58.f*FMath::Lerp(.65f,1.f,FMath::SmoothStep(0.f,.2f,RuneAge))));
            RuneInstances[I]->SetScalarParameterValue(TEXT("Reveal"),Reveal*.92f);
            RuneInstances[I]->SetScalarParameterValue(TEXT("Glow"),(I==2?2.f:1.3f)+.35f*FMath::Sin(RuneAge*8));
            RuneLayers[I]->SetVisibility(Reveal>.005f);
        }
        if(Fade<=0)RuneKind=-1;
    }
}
void UHCM5VS3Abilities::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick){
    Super::TickComponent(Dt,Type,Tick);if(!bEnabled||!Hero||!Combat)return;
    if(!Combat->CanUseCombat()){ResetTransient();UpdateVisuals(Dt);return;}
    for(float& C:Cooldown)C=FMath::Max(0.f,C-Dt);if(bHeld||SwingTime>0||CastKind>=0)IdleMP=0;else IdleMP+=Dt;if(IdleMP>2)MP=FMath::Min(100.f,MP+5*Dt);
    ShieldRemaining=FMath::Max(0.f,ShieldRemaining-Dt);
    if(HealRemaining>0){const float HealDt=FMath::Min(Dt,HealRemaining);Combat->HealPlayer(40.f/3*HealDt);HealRemaining-=HealDt;}
    if(bHeld)HoldTime=FMath::Min(1.2f,HoldTime+Dt);LastSwingAge+=Dt;
    UpdateVisuals(Dt);
    if(SwingTime>0){SwingTime=FMath::Max(0.f,SwingTime-Dt);const float T=1-SwingTime/SwingDuration;if(T>.22f&&T<.78f&&!bContact)SwordTrace();if(SwingTime==0)Hero->SetCombatMovementScale(1);}
    if(CastKind>=0){CastTime-=Dt;if(CastTime<=0)ReleaseSpell();}
}
FString UHCM5VS3Abilities::GetHUDText() const {return FString::Printf(TEXT("1 ◇ 魔力弹 %.1f   2 ◎ 冲击 %.1f\n3 ✚ 治疗 %.1f   4 ⬡ 护盾 %.1f%s"),Cooldown[0],Cooldown[1],Cooldown[2],Cooldown[3],ShieldRemaining>0?TEXT("  生效中"):TEXT(""));}
AHCM5VS3SpellFX::AHCM5VS3SpellFX(){PrimaryActorTick.bCanEverTick=true;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Magic"));SetRootComponent(Mesh);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCanEverAffectNavigation(false);Mesh->SetCastShadow(false);}
AHCM5VS3SpellFX* AHCM5VS3SpellFX::Spawn(AHCM1Character* Source,const FVector& Where,const FVector& Velocity,float Damage,float Life,int32 Kind,bool Projectile){
    if(!Source)return nullptr;int32 Count=0;for(TActorIterator<AHCM5VS3SpellFX> It(Source->GetWorld());It;++It)if(++Count>=32)return nullptr;
    FActorSpawnParameters P;P.Owner=Source;P.Instigator=Source;auto* FX=Source->GetWorld()->SpawnActor<AHCM5VS3SpellFX>(Where,Velocity.Rotation(),P);if(FX)FX->Initialize(Source,Velocity,Damage,Life,Kind,Projectile);return FX;
}
void AHCM5VS3SpellFX::Initialize(AHCM1Character* Source,const FVector& Velocity,float Damage,float Life,int32 Kind,bool Projectile){Hero=Source;Travel=Velocity;HitDamage=Damage;Remaining=Life;Style=Kind;bTravel=Projectile;Mesh->SetStaticMesh(MeshAsset(Projectile?TEXT("SM_MagicBolt"):RuneNames[Kind]));Mesh->SetMaterial(0,MagicMaterial(Kind));if(!Projectile)SetActorRotation(FRotator(90,0,0));}
void AHCM5VS3SpellFX::Tick(float Dt){Super::Tick(Dt);Remaining-=Dt;Age+=Dt;if(Remaining<=0||!Hero.IsValid()){Destroy();return;}
    if(bTravel){const FVector Start=GetActorLocation(),End=Start+Travel*Dt;FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(VS3MagicBolt),false,Hero.Get());
        if(HitDamage>0&&GetWorld()->SweepSingleByChannel(H,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8),Q)){
            UGameplayStatics::ApplyPointDamage(H.GetActor(),HitDamage,Travel.GetSafeNormal(),H,Hero->GetController(),Hero.Get(),UHCM4PistolDamageType::StaticClass());Destroy();return;}
        SetActorLocation(End);
    } else {const float Scale=Style==1?1+Age*4:.35f;SetActorScale3D(FVector(Scale));AddActorLocalRotation(FRotator(0,0,Dt*100));}
}
