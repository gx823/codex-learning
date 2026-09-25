#include "HCM1TestRunner.h"
#include "HCM1PlayerController.h"
#include "HCM1Character.h"
#include "HCM1Vehicle.h"
#include "HCM1LightSwitch.h"
#include "HCM1SaveGame.h"
#include "M3/HCM3NPC.h"
#include "M3/HCM3Experience.h"
#include "M4/HCM4CombatComponent.h"
#include "M4R2/HCM4R2Navigation.h"
#include "M4R2/HCM4R2ViewPreferences.h"
#include "M4R2/HCM4R2PresentationComponent.h"
#include "M5/HCM5StoryDirector.h"
#include "M5/HCM5StoryTerminal.h"
#include "M5/HCM5MusicComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#include "SkeletalRenderPublic.h"
#endif

namespace
{
struct FM5SequenceState
{
    TWeakObjectPtr<AHCM5StoryDirector> Story;
    TWeakObjectPtr<AHCM3NPC> Target;
    int32 BeforeShots = 0, BeforeHits = 0;
    int32 TargetDamageHitsStart = 0;
    bool bAttemptShot = false;
    FVector MovementStart = FVector::ZeroVector;
    uint64 PlacementFrame = 0;
    FString PlacementDiagnostics;
};
AActor* M5Target(const UWorld* World, FName Id)
{
    for (TActorIterator<AHCM3NPC> It(World); It; ++It) if (It->StableId == Id) return *It;
    for (TActorIterator<AHCM5StoryTerminal> It(World); It; ++It) if (It->StableId == Id) return *It;
    return nullptr;
}
FString M5InteractionReadback(AHCM1PlayerController* PC, AHCM1Character* Character, AActor* Target)
{
    if (!PC || !Character || !Target) return TEXT("missing placement actor");
    FVector View; FRotator ViewRotation; PC->GetPlayerViewPoint(View, ViewRotation);
    const FVector Start = PC->IsFirstPersonPerspective() ? View : Character->GetActorLocation()+FVector(0,0,25);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M5PlacementReadback), false, Character);
    FHitResult Hit;
    const bool bHit=Character->GetWorld()->LineTraceSingleByChannel(Hit,Start,Target->GetActorLocation(),ECC_Visibility,Query);
    return FString::Printf(TEXT("frame=%llu fp=%d player=%s view=%s target=%s distance=%.3f hit=%s reachable=%d"),
        GFrameCounter,PC->IsFirstPersonPerspective(),*Character->GetActorLocation().ToString(),*View.ToString(),
        *Target->GetActorLocation().ToString(),FVector::Dist(Character->GetActorLocation(),Target->GetActorLocation()),
        bHit?*GetNameSafe(Hit.GetActor()):TEXT("none"),PC->CanReachInteraction(Target));
}
}

void AHCM1TestRunner::AddM5Tests()
{
    if (Mode.StartsWith(TEXT("m5_vs3_"))) { AddM5VS3Tests(); return; }
    if (Mode.StartsWith(TEXT("m5_vs2_"))) { AddM5VS2Tests(); return; }
    const auto S = MakeShared<FM5SequenceState>();
    const bool bReview = Mode == TEXT("m5_review");
    // Every relocation is an explicit A fixture, never evidence of navigation,
    // traversal or OS input. All quest transitions below use the input chain.
    auto PlaceNear = [this,S](AActor* Target, float Distance = 180.f) -> bool
    {
        S->PlacementDiagnostics.Reset();
        if (!Target || !IsOnFoot()) return false;
        const float Half = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(M5TestPlacement), false, Character);
        for (int32 I=0; I<16; ++I)
        {
            const FVector XY = Target->GetActorLocation()+FRotator(0,I*22.5f,0).Vector()*Distance;
            FHitResult Ground;
            // Town targets can be indoors: begin below the ceiling, near the
            // target's capsule centre, instead of selecting the roof as ground.
            const float GroundProbeRise=GetWorld()->GetMapName()==TEXT("L_HarborTown")?60.f:240.f;
            if (!GetWorld()->LineTraceSingleByChannel(Ground,XY+FVector(0,0,GroundProbeRise),XY-FVector(0,0,350),ECC_Visibility,Query)
                || Ground.ImpactNormal.Z<.8 || Cast<APawn>(Ground.GetActor()) || Ground.GetActor()==Target) continue;
            const FVector Center = Ground.ImpactPoint+FVector(0,0,Half+3);
            if (GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius+2,Half),Query)) continue;
            FHitResult Sight;
            // This is only geometry selection. FP live interaction uses the final
            // CameraManager POV, which is stale in this same teleport callback.
            // Actual production eligibility is required after ordinary frame ticks.
            const float ViewHeight=PC->IsFirstPersonPerspective()?Character->BaseEyeHeight:25.f;
            const FVector SightDestination=Target->GetActorLocation()+FVector(0,0,Distance>220.f?25.f:0.f);
            if (GetWorld()->LineTraceSingleByChannel(Sight,Center+FVector(0,0,ViewHeight),
                SightDestination,ECC_Visibility,Query) && Sight.GetActor()!=Target) continue;
            PlaceCharacter(Center,(Target->GetActorLocation()-Center).Rotation().Yaw);
            S->PlacementFrame=GFrameCounter;
            S->PlacementDiagnostics=M5InteractionReadback(PC,Character,Target);
            Check(TEXT("M5 grounded relocation fixture"),true,TEXT("A: ground trace, capsule clear; no traversal claim"),
                Target->GetName()+TEXT(" immediate_readback_only=")+S->PlacementDiagnostics,TEXT("A-fixture"));
            return true;
        }
        S->PlacementDiagnostics=TEXT("no grounded, capsule-clear and sight-clear candidate among16 radial samples; ")
            +M5InteractionReadback(PC,Character,Target);
        return false;
    };
    auto RequireSettledInteraction = [this,S](AActor* Target,const FString& Label)
    {
        const bool bFreshFrame=GFrameCounter>S->PlacementFrame;
        RequireCamera(Label,bFreshFrame&&Target&&PC->CanReachInteraction(Target)
            &&!PC->CanReachInteraction(Car)&&!PC->CanReachInteraction(Lamp),
            TEXT("production eligibility after at least one frame; original distance/collision; no competing vehicle or light"),
            TEXT("immediate={")+S->PlacementDiagnostics+TEXT("} settled={")+M5InteractionReadback(PC,Character,Target)+TEXT("}"),TEXT("A-fixture then A-runtime"));
    };
    auto ApproachInteraction = [this,S,PlaceNear,RequireSettledInteraction](FName Id,const FString& Label)
    {
        AddStep(Label+TEXT(" A geometry and camera settle"),.6,[this,S,PlaceNear,Id,Label]
        {
            const bool bPlaced=PlaceNear(M5Target(GetWorld(),Id));
            RequireCamera(Label+TEXT(" grounded placement"),bPlaced,
                TEXT("grounded collision-clear candidate; eligibility checked after frame ticks"),S->PlacementDiagnostics,TEXT("A-fixture"));
        },
        [this,RequireSettledInteraction,Id,Label]{RequireSettledInteraction(M5Target(GetWorld(),Id),Label+TEXT(" settled reachable"));});
    };
    auto Screenshot = [this](const FString& Name)
    {
        AddStep(Name+TEXT(" capture"),.08,[this,Name]
        {
            if(Mode==TEXT("m5_review"))
            {
                const auto Presentation=Character->GetR2PresentationComponent();
                Check(Name+TEXT(" bounded presentation readback"),Presentation!=nullptr,
                    TEXT("diagnostic capture only; visible hands and contact reviewed in PNG"),
                    Presentation?Presentation->GetGeometryDiagnostics()+TEXT(" locomotion=")+Presentation->GetLocomotionPresentationDiagnostics():TEXT("missing"),
                    TEXT("A-runtime diagnostics; not visual acceptance"));
#if WITH_EDITOR
                if(Presentation && (Name.StartsWith(TEXT("m5_fp_punch")) || Name==TEXT("m5_fp_pistol_ads")))
                {
                    Check(Name+TEXT(" queue final pose observation"),Presentation->RequestFinalPoseDiagnostic(Name),
                        TEXT("bounded request sampled after presentation update; correlate processed frame in game log"),
                        TEXT("measurement only; does not assert visible geometry"),TEXT("A-runtime diagnostic request"));
                }
#endif
            }
            Capture(Name);
        });
        AddM3CaptureCompletion(Name);
    };
    auto Stage = [this,S](const FString& Label, FName Expected, FName TargetId)
    {
        AddStep(Label+TEXT(" readback"),.6,[]{},[this,S,Label,Expected,TargetId]
        {
            if (!S->Story.IsValid()) return;
            RequireCamera(Label+TEXT(" stable story stage"),S->Story->GetStageId()==Expected,
                Expected.ToString(),S->Story->GetDiagnostics()+TEXT(" status=")+PC->GetStatusMessage()+TEXT(" vehicle=")+Car->GetPlacementDiagnostic(),TEXT("B-Action then A-readback"));
            if (bFinished) return;
            FHCM3QuestNavigationTarget Target;
            const bool bTarget=S->Story->GetNavigationTarget(Target);
            Check(Label+TEXT(" story navigation target"),TargetId.IsNone() ? !bTarget : bTarget&&Target.TargetId==TargetId,
                TargetId.IsNone()?TEXT("completed story has no target"):TargetId.ToString(),
                Target.TargetId.ToString()+TEXT(" ")+Target.Location.ToString(),TEXT("A-runtime"));
            if (!TargetId.IsNone())
            {
                const auto Nav=PC->GetNavigationComponent();
                Check(Label+TEXT(" minimap receives actual target"),Nav&&Nav->HasTarget()&&FVector::Dist(Nav->GetTargetLocation(),Target.Location)<2,
                    TEXT("navigation component target equals live story actor"),Nav?Nav->GetDiagnostics():TEXT("missing"),TEXT("A-runtime"));
            }
        });
    };
    auto Music = [this,S](const FString& Label, EHCM5MusicMode Expected)
    {
        AddStep(Label+TEXT(" music settle"),2.,[]{},[this,S,Label,Expected]
        {
            const auto M=S->Story.IsValid()?S->Story->GetMusic():nullptr;
            Check(Label+TEXT(" music mode and bounded players"),M&&M->GetRequestedMode()==Expected&&M->GetPlayingChannelCount()>0&&M->GetPlayingChannelCount()<=2,
                FString::Printf(TEXT("mode=%d; 1..2 native playing channels; auditory quality not asserted"),int32(Expected)),
                M?M->GetDiagnostics():TEXT("missing"),TEXT("A-runtime audio state"));
        });
    };
    auto Dialogue = [this,S,ApproachInteraction,Screenshot](FName Id,const FString& Label)
    {
        ApproachInteraction(Id,Label+TEXT(" dialogue approach"));
        Tap(TEXT("Interact"),.25);
        AddStep(Label+TEXT(" E opens"),.1,[]{},[this,Id]
        {
            const auto N=Cast<AHCM3NPC>(M5Target(GetWorld(),Id));
            RequireCamera(TEXT("M5 E selected actual story NPC"),N&&N->IsConversationActive()&&PC->IsDialogueOpen(),Id.ToString(),PC->GetDialogueName(),TEXT("B-Action"));
        });
        // New M5 conversations have five lines. Each first E reveals text; the
        // second advances. No direct CompleteDialogue invocation is used.
        Tap(TEXT("Interact"),.08);
        Screenshot(Label+TEXT("_dialogue"));
        for (int32 I=0;I<9;++I) Tap(TEXT("Interact"),.08);
        AddStep(Label+TEXT(" closes"),.3,[]{},[this]
        { Check(TEXT("M5 E completes and closes dialogue"),!PC->IsDialogueOpen(),TEXT("closed after five authored lines"),PC->GetDialogueName(),TEXT("B-Action")); });
    };
    auto SaveCheck = [this,S](const FString& Label,FName Expected)
    {
        Tap(TEXT("Save"),.6);
        AddStep(Label+TEXT(" serialized checkpoint"),.1,[]{},[this,Expected,Label]
        {
            TStrongObjectPtr<UHCM1SaveGame> Saved(Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0)));
            RequireCamera(Label+TEXT(" actual save file state"),Saved.IsValid()&&Saved->M5Story.Version==1&&Saved->M5Story.StageId==Expected,
                Expected.ToString(),Saved.IsValid()?Saved->M5Story.StageId.ToString():TEXT("no readable slot; ")+PC->GetStatusMessage(),TEXT("B-Action Save + file readback"));
        });
    };

    AddStep(TEXT("M5 required authored actors"),1.,[this,S]
    {
        S->Story=AHCM5StoryDirector::Find(GetWorld());
        bool bTargets=true;
        for (FName Id:{TEXT("M5_Contact"),TEXT("M5_Harbor"),TEXT("M5_InterceptorA"),TEXT("M5_InterceptorB"),TEXT("M5_RelayCache"),TEXT("M5_TowerUplink")})
            bTargets &= M5Target(GetWorld(),Id)!=nullptr;
        RequireCamera(TEXT("M5 scene prerequisites"),S->Story.IsValid()&&bTargets&&PC->GetCombatComponent()&&PC->IsGameplayFocused(),
            TEXT("actual M5 director, six authored targets, combat and real application focus"),State(),TEXT("A-runtime prerequisite"));
        if (bFinished) return;
        const USkeletalMeshComponent* PlayerMesh=Character->GetMesh();
        const USkeletalMesh* Hero=PlayerMesh?PlayerMesh->GetSkeletalMeshAsset():nullptr;
        const UAnimInstance* HeroAnim=PlayerMesh?PlayerMesh->GetAnimInstance():nullptr;
        RequireCamera(TEXT("M5 actual purchased Selestia player mesh"),Hero
            &&Hero->GetPathName()==(GetWorld()->GetMapName()==TEXT("L_HarborTown")
                ?TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_1d6685a10359/SKM_Selestia_Warp.SKM_Selestia_Warp")
                :TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia.SKM_Selestia")),
            TEXT("private Selestia mesh in the real possessed character; rejected procedural heroes excluded"),
            Hero?Hero->GetPathName():TEXT("null mesh"),TEXT("A-runtime asset identity; not visual approval"));
        if (bFinished) return;
        Check(TEXT("M5 active animation matches combat configured class"),HeroAnim
            &&HeroAnim->GetClass()->GetPathName()==(GetWorld()->GetMapName()==TEXT("L_HarborTown")
                ?TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_7a9c9d9d2395/ABP_TownFlight.ABP_TownFlight_C")
                :TEXT("/Game/HarborCity/M5VS1/HeroSelestia/Animation/ABP_M4R2_Player_Selestia.ABP_M4R2_Player_Selestia_C"))
            &&PC->GetCombatComponent()->PlayerAnimationBlueprint.ToSoftObjectPath().ToString()==HeroAnim->GetClass()->GetPathName(),
            TEXT("Selestia AnimBP active without old combat class override"),
            HeroAnim?HeroAnim->GetClass()->GetPathName():TEXT("null anim instance"),TEXT("A-runtime"));
        bool bUnitBinds=true;
        for(const FTransform& Bone:Hero->GetRefSkeleton().GetRefBonePose())bUnitBinds &= Bone.GetScale3D().Equals(FVector::OneVector,.00001);
        Check(TEXT("M5 original proportions and normalized bind units"),bUnitBinds
            &&PlayerMesh->GetRelativeScale3D().Equals(FVector(1.25),.00001)
            &&Hero->GetRefSkeleton().GetNum()==(GetWorld()->GetMapName()==TEXT("L_HarborTown")?250:247),
            TEXT("247 original bones; town adds 3 virtual IK bones; unit bind scales; uniform mesh scale 1.25"),
            FString::Printf(TEXT("scale=%s bones=%d unit_binds=%d"),*PlayerMesh->GetRelativeScale3D().ToString(),Hero->GetRefSkeleton().GetNum(),bUnitBinds),TEXT("A-runtime"));
        Check(TEXT("M5 first-person camera uses original eye height"),FMath::IsNearlyEqual(Character->BaseEyeHeight,57.4561f,.02f),
            TEXT("original eye reference 117.164879cm * 1.25 - mesh offset 89cm"),
            FString::SanitizeFloat(Character->BaseEyeHeight),TEXT("A-runtime; no head-animation camera inheritance"));
        bool bExpectedPerspective=!PC->IsFirstPersonPerspective();
        if(Mode==TEXT("m5_reopen")||Mode==TEXT("m5_route_reopen")||Mode==TEXT("m5_resume_full"))
        {
            TStrongObjectPtr<UHCM4R2ViewPreferences> Views(Cast<UHCM4R2ViewPreferences>(
                UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName()+TEXT("_Views"),0)));
            bExpectedPerspective=Views.IsValid()&&Views->Version==1
                &&PC->IsFirstPersonPerspective()==(Views->OnFoot==EHCM4R2Perspective::FirstPerson);
            Check(TEXT("M5 restart restores the actual saved view preference"),bExpectedPerspective,
                TEXT("compare live perspective to the previous process isolated _Views file; never reset it"),
                FString::Printf(TEXT("file_present=%d live_first_person=%d saved_on_foot=%d"),Views.IsValid(),
                    PC->IsFirstPersonPerspective(),Views.IsValid()?int32(Views->OnFoot):-1),TEXT("A-disk and runtime"));
        }
        RequireCamera(TEXT("M5 fresh isolated initial state"),S->Story->GetStageId()==TEXT("M5.Signal.Contact")&&IsOnFoot()
            &&bExpectedPerspective&&PC->GetCombatComponent()->GetWeaponMode()==EHCM4WeaponMode::Unarmed,
            (Mode==TEXT("m5_reopen")||Mode==TEXT("m5_route_reopen")||Mode==TEXT("m5_resume_full"))?TEXT("fresh Contact and unarmed; persisted view preference retained before Load")
                :TEXT("fresh Contact, TP, unarmed; no test stage reset"),S->Story->GetDiagnostics(),TEXT("A-runtime prerequisite"));
        Check(TEXT("M5 unchanged base look gains"),PC->GetOnFootLookDegreesPerActionUnit().Equals(FVector2D(1.5,1.5),.0001)
            &&PC->GetDrivingLookDegreesPerActionUnit().Equals(FVector2D(1.125,1.125),.0001),
            TEXT("on-foot1.5; driving1.125=0.75 base"),PC->GetOnFootLookDegreesPerActionUnit().ToString()+TEXT(" / ")+PC->GetDrivingLookDegreesPerActionUnit().ToString(),TEXT("A-readback"));
    });

    if (Mode==TEXT("m5_reopen")||Mode==TEXT("m5_route_reopen"))
    {
        const bool bRoute=Mode==TEXT("m5_route_reopen");
        const FName ExpectedStage=bRoute?TEXT("M5.Signal.Ambush"):TEXT("M5.Signal.Complete");
        AddStep(TEXT("M5 previous-process save prerequisite"),.1,[this,bRoute,ExpectedStage]
        {
            TStrongObjectPtr<UHCM1SaveGame> Saved(Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0)));
            RequireCamera(TEXT("M5 disk checkpoint exists before Load"),Saved.IsValid()
                &&Saved->M5Story.Version==1&&Saved->M5Story.StageId==ExpectedStage
                &&Saved->M5Story.DefeatedInterceptorIds.Num()==(bRoute?0:2)
                &&(!bRoute||(Saved->PlayerTransform.GetLocation().Y>10000&&Saved->VehicleTransform.GetLocation().X<-10000)),
                TEXT("actual previous-process checkpoint; route mode requires both actors outside legacy bounds; never synthesized"),PC->GetSaveSlotName(),TEXT("A-disk prerequisite"));
        });
        Tap(TEXT("Load"),1.5);
        Stage(TEXT("Fresh-process restored checkpoint"),ExpectedStage,bRoute?FName(TEXT("M5_InterceptorA")):NAME_None);
        AddStep(TEXT("M5 fresh-process player and NPC readback"),.3,[]{},[this,S,bRoute]
        {
            bool bNoRepeatEnemies=true;
            for(FName Id:{TEXT("M5_InterceptorA"),TEXT("M5_InterceptorB")})
            {
                auto N=Cast<AHCM3NPC>(M5Target(GetWorld(),Id));
                bNoRepeatEnemies &= N&&(N->IsDead()||!N->IsNPCEnabled());
            }
            auto Citizen=Cast<AHCM3NPC>(M5Target(GetWorld(),TEXT("M5_Citizen01")));
            Check(TEXT("M5 restart restored playable checkpoint"),IsOnFoot()&&CountCharacters()==1
                &&PC->GetCombatComponent()->GetPlayerHealth()>0&&S->Story->IsComplete()==!bRoute,
                TEXT("one live player; expected completion flag restored through Load Action"),State(),TEXT("B-Action + A-readback"));
            Check(TEXT("M5 restart retains expected victories"),S->Story->ExportState().DefeatedInterceptorIds.Num()==(bRoute?0:2)
                &&(bRoute||bNoRepeatEnemies),TEXT("route has zero victories; completed checkpoint has two and no repeat enemies"),
                S->Story->GetDiagnostics(),TEXT("B-Action + A-readback"));
            if(bRoute)
            {
                TStrongObjectPtr<UHCM1SaveGame> Saved(Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0)));
                Check(TEXT("M5 restart restores both actors beyond legacy bounds"),Saved.IsValid()
                    &&FVector::Dist(Character->GetActorLocation(),Saved->PlayerTransform.GetLocation())<10
                    &&FVector::Dist(Car->GetActorLocation(),Saved->VehicleTransform.GetLocation())<20,
                    TEXT("player and parked car match prior route save within10/20cm after actual Load Action"),
                    State()+TEXT(" car=")+Car->GetActorLocation().ToString(),TEXT("B-Action + A-disk readback"));
            }
            Check(TEXT("M5 corrected citizen remains restorable after restart"),Citizen&&Citizen->CanRestoreState(Citizen->CaptureState()),
                TEXT("unmodified production NPC stand/save validation passes after disk restore"),Citizen?Citizen->GetActorLocation().ToString():TEXT("missing"),TEXT("A-runtime validation"));
        });
        Music(TEXT("Restored checkpoint ambience"),bRoute?EHCM5MusicMode::Exploration:EHCM5MusicMode::Interior);
        if(!bRoute&&GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/")))AddStep(TEXT("VS3 restored side quests"),.1,[]{},[this]{auto* E=PC->GetM3Experience();Check(TEXT("Both side quests survive fresh-process Load"),E&&E->GetCoffeeStage()==EHCM3QuestStage::Complete&&E->GetRideStage()==EHCM3QuestStage::Complete,TEXT("both Complete"),E?E->GetQuestHUD(PC):TEXT("missing"),TEXT("B-Action Load + A readback"));});
        Screenshot(bRoute?TEXT("m5_reopened_route"):TEXT("m5_reopened_completion"));
        return;
    }

    if (Mode==TEXT("m5_stability"))
    {
        float Seconds=180.f;
        FParse::Value(FCommandLine::Get(),TEXT("M5StabilitySeconds="),Seconds);
        Seconds=FMath::IsFinite(Seconds)?FMath::Clamp(Seconds,60.f,1800.f):180.f;
        Screenshot(TEXT("m5_stability_before"));
        AddStep(TEXT("M5 stability stationary focused sample"),Seconds,[]{},[this,S,Seconds]
        {
            const auto M=S->Story->GetMusic();
            Check(TEXT("M5 stationary stability state"),IsOnFoot()&&CountCharacters()==1&&PC->IsGameplayFocused()
                &&PC->GetCombatComponent()->GetPlayerHealth()>0&&S->Story->GetStageId()==TEXT("M5.Signal.Contact")
                &&M&&M->GetPlayingChannelCount()>0&&M->GetPlayingChannelCount()<=2,
                TEXT("one live controlled player; unchanged quest; 1..2 music channels; no performance threshold inferred"),
                FString::Printf(TEXT("requested_world_seconds=%.1f; %s; %s"),Seconds,*State(),*S->Story->GetDiagnostics()),TEXT("A-runtime sample"));
            Check(TEXT("M5 stability performance source"),true,TEXT("existing m4_r1_performance named phase has wall frame times, focus, CPU/GPU/settings; setup and captures separate"),
                TEXT("Use the stationary phase only for matched profiling; actor ticks are not GPU frame timings."),TEXT("A-scope"));
        });
        Screenshot(TEXT("m5_stability_after"));
        return;
    }

    if (Mode==TEXT("m5_playthrough"))
    {
        struct FNaturalRun
        {
            FVector Last=FVector::ZeroVector;
            double WalkCm=0,DriveCm=0;
            bool bParked=false;
            int32 InitialShots=0,InitialHits=0;
        };
        const auto N=MakeShared<FNaturalRun>();
        // Exactly one initial car placement. The player remains at the authored
        // spawn. After this step every movement/rotation uses ordinary actions.
        AddStep(TEXT("M5 playthrough initial A car staging only"),3.,[this,N]
        {
            PlaceCar(FTransform(FRotator(0,90,0),FVector(-11700,-7000,100)));
            N->InitialShots=PC->GetCombatComponent()->GetShotCount();N->InitialHits=PC->GetCombatComponent()->GetDamageHitCount();
            Check(TEXT("M5 recording fixture boundary"),true,TEXT("initial A car staging onto west road; player remains original spawn; no later teleport/reset/aim setter"),
                TEXT("B-Action continuous excerpt: first two story chapters, driving, coast exploration and weapon handling; complete chain belongs to m5_story"),TEXT("A-fixture disclosure"));
        });
        auto Walk=[this,N](const FString& Label,TFunction<FVector()> Destination,float Seconds,bool bSprint=false)
        {
            const auto Arrived=MakeShared<bool>(false);
            AddStep(Label+TEXT(" start"),.05,[this,N]{N->Last=Character->GetActorLocation();});
            for(int32 Tick=0;Tick<FMath::CeilToInt(Seconds*10.f);++Tick)
                AddStep(Label+TEXT(" B walking"),.1,[this,N,Destination,bSprint,Arrived]
                {
                    if(!IsOnFoot()||!PC->IsGameplayFocused())
                    {Release(TEXT("Move"));Release(TEXT("Look"));Release(TEXT("Sprint"));RequireCamera(TEXT("M5 natural walk input ownership"),false,TEXT("focused on-foot gameplay"),State(),TEXT("B-Action"));return;}
                    const FVector Position=Character->GetActorLocation();N->WalkCm+=FVector::Dist2D(N->Last,Position);N->Last=Position;
                    const FVector Delta=Destination()-Position;const double Distance=Delta.Size2D();
                    if(*Arrived||Distance<30){*Arrived=true;Release(TEXT("Move"));Release(TEXT("Look"));Release(TEXT("Sprint"));return;}
                    const FRotator Control=PC->GetControlRotation();const FVector Direction=Delta.GetSafeNormal2D();
                    const FVector Forward=FRotator(0,Control.Yaw,0).Vector(),Right=FRotator(0,Control.Yaw+90,0).Vector();
                    const double Strength=FMath::Clamp(Distance/180.,.12,.9);
                    Hold(TEXT("Move"),FInputActionValue(FVector2D(FVector::DotProduct(Direction,Right),FVector::DotProduct(Direction,Forward))*Strength));
                    if(bSprint&&Distance>350)Hold(TEXT("Sprint"),FInputActionValue(true));else Release(TEXT("Sprint"));
                    const double Yaw=FMath::FindDeltaAngleDegrees(Control.Yaw,Delta.Rotation().Yaw),Pitch=FMath::FindDeltaAngleDegrees(Control.Pitch,-8.);
                    const FVector2D Gain=PC->GetOnFootLookDegreesPerActionUnit();
                    Hold(TEXT("Look"),FInputActionValue(FVector2D(FMath::Clamp(Yaw*.12/Gain.X,-1.8,1.8),FMath::Clamp(Pitch*.12/Gain.Y,-.8,.8))));
                });
            AddStep(Label+TEXT(" actual arrival"),.4,[this]{Release(TEXT("Move"));Release(TEXT("Look"));Release(TEXT("Sprint"));},[this,N,Destination,Label]
            {
                RequireCamera(Label+TEXT(" actual continuous walk arrived"),IsOnFoot()&&FVector::Dist2D(Character->GetActorLocation(),Destination())<95,
                    TEXT("real movement reaches planned point within95cm; no relocation"),
                    FString::Printf(TEXT("remaining=%.2f walk_total_m=%.2f position=%s"),FVector::Dist2D(Character->GetActorLocation(),Destination()),N->WalkCm/100,*Character->GetActorLocation().ToString()),TEXT("B-Action"));
            });
        };
        auto Look=[this](const FString& Label,double Yaw,double Pitch,float Seconds)
        {
            for(int32 I=0;I<FMath::CeilToInt(Seconds*10.f);++I)
                AddStep(Label+TEXT(" B Look"),.1,[this,Yaw,Pitch]
                {
                    const FRotator R=PC->GetControlRotation();const FVector2D G=PC->GetOnFootLookDegreesPerActionUnit();
                    Hold(TEXT("Look"),FInputActionValue(FVector2D(FMath::Clamp(FMath::FindDeltaAngleDegrees(R.Yaw,Yaw)*.12/G.X,-1.8,1.8),
                        FMath::Clamp(FMath::FindDeltaAngleDegrees(R.Pitch,Pitch)*.12/G.Y,-1.,1.))));
                });
            AddStep(Label+TEXT(" release Look"),.2,[this]{Release(TEXT("Look"));});
        };
        auto Talk=[this,S,Screenshot](FName Id,FName NextStage,const FString& Label)
        {
            AddStep(Label+TEXT(" actual arrival eligibility"),.3,[this,Id]
            {RequireCamera(TEXT("M5 natural dialogue reached"),PC->CanReachInteraction(M5Target(GetWorld(),Id)),TEXT("real walk ended in E interaction range"),PC->GetInteractionPrompt(),TEXT("B-Action arrival"));});
            Tap(TEXT("Interact"),3.2);
            AddStep(Label+TEXT(" live conversation"),.1,[]{},[this]
            {RequireCamera(TEXT("M5 natural E opened dialogue"),PC->IsDialogueOpen(),TEXT("real dialogue opened"),PC->GetDialogueName(),TEXT("B-Action"));});
            Screenshot(Label);
            for(int32 Line=0;Line<5;++Line)Tap(TEXT("Interact"),3.2);
            AddStep(Label+TEXT(" quest submission"),.6,[]{},[this,S,NextStage]
            {RequireCamera(TEXT("M5 natural dialogue advanced story"),!PC->IsDialogueOpen()&&S->Story->GetStageId()==NextStage,NextStage.ToString(),S->Story->GetDiagnostics(),TEXT("B-Action"));});
        };
        Walk(TEXT("Commercial pavement"),[]{return FVector(-6500,-6200,100);},12);
        Walk(TEXT("Night Relay entrance"),[]{return FVector(-6500,-8615,100);},10);
        Talk(TEXT("M5_Contact"),TEXT("M5.Signal.Harbor"),TEXT("m5_play_contact"));
        Walk(TEXT("Leave relay to south pavement"),[]{return FVector(-6500,-8000,100);},7);
        Walk(TEXT("Walk west to parked road"),[]{return FVector(-11100,-8000,100);},18,true);
        Walk(TEXT("Approach actual driver door"),[this]{return Car->GetInteractionLocation()+FVector(100,0,0);},10);
        Tap(TEXT("Interact"),1.2);
        AddStep(TEXT("M5 natural car possession"),3.,[this,N]
        {RequireCamera(TEXT("M5 natural E enters staged car"),IsDriving(),TEXT("real E enter after continuous walk"),State(),TEXT("B-Action"));N->Last=Car->GetActorLocation();});
        Screenshot(TEXT("m5_play_driving_third"));
        Tap(TEXT("Perspective"),.6);
        Screenshot(TEXT("m5_play_driving_first"));
        for(int32 I=0;I<500;++I)
        {
            if(I==250)Tap(TEXT("Perspective"),.5);
            AddStep(TEXT("M5 west-road actual driving"),.1,[this,N]
            {
                if(!IsDriving()||!PC->IsGameplayFocused())
                {Release(TEXT("Drive"));RequireCamera(TEXT("M5 natural driving input ownership"),false,TEXT("focused real car possession"),State(),TEXT("B-Action"));return;}
                const FVector Here=Car->GetActorLocation();N->DriveCm+=FVector::Dist2D(N->Last,Here);N->Last=Here;
                const FVector Delta=FVector(-11700,9500,Here.Z)-Here;
                const double Remaining=Delta.Size2D(),Speed=Car->GetSpeedKmh();
                if(N->bParked || Remaining<500)
                {
                    if(!N->bParked&&Car->GetSignedSpeed()>90)Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,-.8)));
                    else{N->bParked=true;Release(TEXT("Drive"));Hold(TEXT("Handbrake"),FInputActionValue(true));}
                    return;
                }
                const double Angle=FMath::FindDeltaAngleDegrees(Car->GetActorRotation().Yaw,Delta.Rotation().Yaw);
                const double Steering=FMath::Clamp(Angle*.024,-.35,.35);
                const double Desired=Remaining<1700?10.:18.;
                const double Throttle=Speed>Desired+1.5?-.18:Speed<Desired-1.5?.6:.12;
                Release(TEXT("Handbrake"));Hold(TEXT("Drive"),FInputActionValue(FVector2D(Steering,Throttle)));
            });
        }
        AddStep(TEXT("M5 west-road parking settles"),2.,[this]{Release(TEXT("Drive"));Hold(TEXT("Handbrake"),FInputActionValue(true));},[this,N]
        {
            RequireCamera(TEXT("M5 actual drive route and parked finish"),IsDriving()&&N->bParked&&Car->GetSpeedKmh()<1
                &&N->DriveCm>14000&&FVector::Dist2D(Car->GetActorLocation(),FVector(-11700,9500,100))<800,
                TEXT("more than140m real car travel on west road; stopped within8m of planned curb point"),
                FString::Printf(TEXT("drive_m=%.3f %s"),N->DriveCm/100,*Car->GetDriveTelemetry()),TEXT("B-Action"));
        });
        Tap(TEXT("Interact"),1.2);
        AddStep(TEXT("M5 natural exit"),.4,[this]{Release(TEXT("Handbrake"));},[this]
        {RequireCamera(TEXT("M5 E restores same walking player"),IsOnFoot()&&CountCharacters()==1,TEXT("real safe car exit"),State(),TEXT("B-Action"));});
        Walk(TEXT("Harbor witness approach"),[]{return FVector(-8500,9700,100);},15,true);
        Talk(TEXT("M5_Harbor"),TEXT("M5.Signal.Ambush"),TEXT("m5_play_harbor"));
        Tap(TEXT("Perspective"),.5);
        Walk(TEXT("Harbor lane north"),[]{return FVector(-8500,11500,100);},9);
        Walk(TEXT("Cross harbor branch"),[]{return FVector(-8500,14500,100);},13);
        Walk(TEXT("Waterfront promenade"),[]{return FVector(-8500,16800,100);},11);
        Look(TEXT("Seaside safe ground range"),0,-35,3);
        Tap(TEXT("ToggleWeapon"),.6);
        for(int32 View=0;View<2;++View)
        {
            if(View)Tap(TEXT("Perspective"),.5);
            AddStep(TEXT("M5 coast ordinary ADS"),.5,[this]{Hold(TEXT("Aim"),FInputActionValue(true));});
            Tap(TEXT("Attack"),.65);Tap(TEXT("Attack"),.65);
            AddStep(TEXT("M5 coast aim release"),.4,[this]{Release(TEXT("Aim"));});
            Tap(TEXT("Reload"),1.9);
            Screenshot(View?TEXT("m5_play_pistol_third"):TEXT("m5_play_pistol_first"));
        }
        Tap(TEXT("ToggleWeapon"),.6);Tap(TEXT("Attack"),.85);Tap(TEXT("Attack"),.85);
        Look(TEXT("Waterfront city panorama"),-32,-5,3);
        Tap(TEXT("Save"),.8);
        AddStep(TEXT("M5 real excerpt final save and scope"),10.,[]{},[this,S,N]
        {
            TStrongObjectPtr<UHCM1SaveGame> Saved(Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0)));
            const auto C=PC->GetCombatComponent();
            Check(TEXT("M5 excerpt real weapons and no bystander hits"),C->GetShotCount()==N->InitialShots+4&&C->GetDamageHitCount()==N->InitialHits
                &&C->GetWeaponMode()==EHCM4WeaponMode::Unarmed,TEXT("four actual shots/reloads on safe ground, then real melee input; no NPC damage"),C->GetHUDText(),TEXT("B-Action"));
            Check(TEXT("M5 excerpt saved real intermediate quest"),Saved.IsValid()&&Saved->M5Story.StageId==TEXT("M5.Signal.Ambush")
                &&S->Story->GetStageId()==TEXT("M5.Signal.Ambush")&&IsOnFoot(),TEXT("actual first-two-chapters excerpt; Ambush remains next; no complete-story claim"),
                FString::Printf(TEXT("walk_m=%.3f drive_m=%.3f %s"),N->WalkCm/100,N->DriveCm/100,*S->Story->GetDiagnostics()),TEXT("B-Action + A-readback"));
        });
        Screenshot(TEXT("m5_play_excerpt_end"));
        double Planned=0;for(const auto& Step:Steps)Planned+=Step.Duration;
        UE_LOG(LogTemp,Display,TEXT("M5_PLAYTHROUGH_PLAN seconds=%.3f scope=continuous_first_two_chapters_excerpt initial_car_fixture=1 subsequent_relocations=0"),Planned);
        return;
    }

    if (bReview)
    {
        struct FReviewCameraState
        {
            float ArmLength=0;
            FVector SocketOffset=FVector::ZeroVector,TargetOffset=FVector::ZeroVector;
            FRotator ControlRotation=FRotator::ZeroRotator;
            bool bSaved=false,bRestored=false;
        };
        const auto ReviewCamera=MakeShared<FReviewCameraState>();
        auto ReviewPortrait=[this,ReviewCamera,Screenshot](const FString& Label,float Yaw,float ArmLength,float TargetHeight)
        {
            AddStep(Label+TEXT(" A camera fixture"),.1,[this,ReviewCamera,Yaw,ArmLength,TargetHeight]
            {
                auto Boom=Character->GetCameraBoom();
                if(!RequireCamera(TEXT("M5 portrait actual TP unarmed"),IsOnFoot()&&!PC->IsFirstPersonPerspective()
                    &&PC->GetCombatComponent()->GetWeaponMode()==EHCM4WeaponMode::Unarmed&&Boom,
                    TEXT("actual on-foot TP and unarmed; review fixtures only"),State(),TEXT("A-fixture")))return;
                if(!ReviewCamera->bSaved)
                {
                    ReviewCamera->ArmLength=Boom->TargetArmLength;
                    ReviewCamera->SocketOffset=Boom->SocketOffset;ReviewCamera->TargetOffset=Boom->TargetOffset;
                    ReviewCamera->ControlRotation=PC->GetControlRotation();ReviewCamera->bSaved=true;
                    const TWeakObjectPtr<USpringArmComponent> WeakBoom(Boom);
                    const TWeakObjectPtr<AHCM1PlayerController> WeakPC(PC.Get());
                    // The runner executes and clears this once on Finish or a
                    // legal Escape abort. No strong UObject ownership or input
                    // injection is introduced by this restoration callback.
                    M5ReviewCameraCleanup=[ReviewCamera,WeakBoom,WeakPC]
                    {
                        if(!ReviewCamera->bSaved||ReviewCamera->bRestored)return;
                        ReviewCamera->bRestored=true;
                        if(auto SavedBoom=WeakBoom.Get())
                        {
                            SavedBoom->TargetArmLength=ReviewCamera->ArmLength;
                            SavedBoom->SocketOffset=ReviewCamera->SocketOffset;
                            SavedBoom->TargetOffset=ReviewCamera->TargetOffset;
                        }
                        if(auto SavedPC=WeakPC.Get()) SavedPC->SetControlRotation(ReviewCamera->ControlRotation);
                        UE_LOG(LogTemp,Display,TEXT("M5_REVIEW_CAMERA_RESTORED boom_valid=%d controller_valid=%d input_injected=0"),
                            WeakBoom.IsValid(),WeakPC.IsValid());
                    };
                }
                PlaceCharacter(FVector(-8200,-6000,110),0);
                Boom->TargetArmLength=ArmLength;Boom->SocketOffset=FVector::ZeroVector;
                Boom->TargetOffset=FVector(0,0,TargetHeight);PC->SetControlRotation(FRotator(0,Yaw,0));
            });
            struct FReadyState { double StartedAt=0,ClearAt=0; uint64 StartFrame=0; };
            const auto Ready=MakeShared<FReadyState>();
            auto PollReady=[this,Ready,Label](auto Self)->void
            {
                int32 ShaderJobs=0;
                bool bShadersBusy=false;
#if WITH_EDITOR
                if(GShaderCompilingManager)
                {ShaderJobs=GShaderCompilingManager->GetNumRemainingJobs();bShadersBusy=GShaderCompilingManager->IsCompiling();}
#endif
                const double Now=FPlatformTime::Seconds();
                if(bShadersBusy)Ready->ClearAt=0;
                else if(Ready->ClearAt==0)Ready->ClearAt=Now;
                const bool bReady=!bShadersBusy&&Ready->ClearAt>0&&Now-Ready->ClearAt>=2.&&GFrameCounter>Ready->StartFrame+2;
                if(!bReady&&Now-Ready->StartedAt<45.)
                {
                    // Runner moves End out before calling it. Restore a fresh
                    // callback before repeating this bounded wait step.
                    Steps[StepIndex].End=[Self]{Self(Self);};--StepIndex;return;
                }
                const FVector View=PC->PlayerCameraManager->GetCameraLocation();
                RequireCamera(Label+TEXT(" render settled before capture"),bReady,
                    TEXT("no outstanding editor shader jobs, 2s clear and multiple normal camera frames; timeout45s"),
                    FString::Printf(TEXT("jobs=%d wall_wait=%.3f frame=%llu view=%s body=%s"),ShaderJobs,Now-Ready->StartedAt,
                        GFrameCounter,*View.ToString(),*Character->GetActorLocation().ToString()),TEXT("A-render prerequisite; not art approval"));
            };
            AddStep(Label+TEXT(" bounded renderer settle"),.25,[Ready]
            {
                if(Ready->StartedAt==0) {Ready->StartedAt=FPlatformTime::Seconds();Ready->StartFrame=GFrameCounter;}
            },[PollReady]
            {
                PollReady(PollReady);
            });
#if WITH_EDITOR
            if(Label==TEXT("m5_hero_front_fullbody"))
            {
                AddStep(TEXT("M5 one-shot evaluated sole diagnostic"),.5,[this]
                {
                    // One isolated editor review only: expensive actual skinning,
                    // including morphs. Never part of gameplay or a performance sample.
                    const auto Mesh=Character->GetMesh();
                    TArray<FFinalSkinVertex> Vertices;Mesh->GetCPUSkinnedVertices(Vertices,0);
                    FVector Lowest(0,0,TNumericLimits<double>::Max());
                    for(const auto& Vertex:Vertices)
                    {
                        const FVector Position=Mesh->GetComponentTransform().TransformPosition(FVector(Vertex.Position));
                        if(Position.Z<Lowest.Z)Lowest=Position;
                    }
                    FHitResult Ground;FCollisionQueryParams Query(SCENE_QUERY_STAT(M5SoleAudit),false,Character);
                    const bool bGround=Vertices.Num()>0&&GetWorld()->LineTraceSingleByChannel(Ground,
                        Lowest+FVector(0,0,60),Lowest-FVector(0,0,200),ECC_Visibility,Query);
                    Check(TEXT("M5 evaluated sole diagnostic captured"),bGround&&!Lowest.ContainsNaN(),
                        TEXT("measurement only; no automatic foot-contact acceptance"),
                        FString::Printf(TEXT("vertices=%d lowest=%s ground=%s clearance_cm=%.4f footL=%s footR=%s capsule_bottom=%.4f"),
                            Vertices.Num(),*Lowest.ToString(),*Ground.ImpactPoint.ToString(),bGround?Lowest.Z-Ground.ImpactPoint.Z:-999.,
                            *Mesh->GetSocketLocation(TEXT("Foot_L")).ToString(),*Mesh->GetSocketLocation(TEXT("Foot_R")).ToString(),
                            Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
                        TEXT("A-runtime evaluated geometry; single editor sample"));
                });
            }
#endif
            Screenshot(Label);
        };
        ReviewPortrait(TEXT("m5_hero_front_fullbody"),180.f,285.f,0.f);
        ReviewPortrait(TEXT("m5_hero_threequarter_fullbody"),135.f,285.f,0.f);
        ReviewPortrait(TEXT("m5_hero_face_front"),180.f,70.f,65.f);
        AddStep(TEXT("M5 review restore gameplay camera values"),.8,[this]
        {
            if(!M5ReviewCameraCleanup)return;
            TFunction<void()> Cleanup=MoveTemp(M5ReviewCameraCleanup);
            M5ReviewCameraCleanup=nullptr;
            Cleanup();
        },[this,ReviewCamera]
        {
            const auto Boom=Character->GetCameraBoom();
            Check(TEXT("M5 portrait camera fixtures restored"),ReviewCamera->bSaved&&ReviewCamera->bRestored&&!M5ReviewCameraCleanup
                &&FMath::IsNearlyEqual(Boom->TargetArmLength,ReviewCamera->ArmLength)
                &&Boom->SocketOffset.Equals(ReviewCamera->SocketOffset)&&Boom->TargetOffset.Equals(ReviewCamera->TargetOffset)
                &&PC->GetControlRotation().Equals(ReviewCamera->ControlRotation,.01f),
                TEXT("runtime boom and controller values restored; CDO/default gameplay camera unchanged"),
                TEXT("Three portrait PNGs use A camera fixtures, not proof of mouse input or visual acceptance"),TEXT("A-fixture restore"));
        });
        for (int32 View=0;View<2;++View)
        {
            if (View) Tap(TEXT("Perspective"),.5);
            for (int32 Weapon=0;Weapon<2;++Weapon)
            {
                if (Weapon) Tap(TEXT("ToggleWeapon"),.6);
                const FString Prefix=FString::Printf(TEXT("m5_%s_%s"),View?TEXT("fp"):TEXT("tp"),Weapon?TEXT("pistol"):TEXT("unarmed"));
                AddStep(Prefix+TEXT(" A street fixture"),.8,[this]{PlaceCharacter(FVector(-8200,-6000,110),0);});
                Screenshot(Prefix+TEXT("_idle"));
                if(View)
                {
                    const auto SavedLook=MakeShared<FRotator>();
                    AddStep(Prefix+TEXT(" remember review look"),.1,[this,SavedLook]{*SavedLook=PC->GetControlRotation();});
                    for(const float Pitch:{-70.f,70.f})
                    {
                        const FString Label=Prefix+(Pitch<0?TEXT("_look_down"):TEXT("_look_up"));
                        AddStep(Label+TEXT(" A angle fixture"),.7,[this,Pitch,SavedLook]
                            {PC->SetControlRotation(FRotator(Pitch,SavedLook->Yaw,0));});
                        Screenshot(Label);
                    }
                    AddStep(Prefix+TEXT(" restore review look"),.7,[this,SavedLook]{PC->SetControlRotation(*SavedLook);});
                }
                if(Weapon)
                {
                    AddStep(Prefix+TEXT(" real ADS"),.6,[this]{Hold(TEXT("Aim"),FInputActionValue(true));},[this,Prefix]
                    {
                        const float Base=Character->GetFollowCamera()->FieldOfView,Actual=PC->PlayerCameraManager->GetFOVAngle();
                        const double Ratio=FMath::Tan(FMath::DegreesToRadians(Base*.5))/FMath::Tan(FMath::DegreesToRadians(Actual*.5));
                        Check(Prefix+TEXT(" final world ADS magnification retained"),FMath::IsNearlyEqual(Ratio,1.6,.001),
                            TEXT("1.6 from actual final POV; new character must not alter world magnification"),
                            FString::Printf(TEXT("base=%.6f final=%.6f ratio=%.9f"),Base,Actual,Ratio),TEXT("B-Action + A-final POV"));
                    });
                    Screenshot(Prefix+TEXT("_ads"));
                    AddStep(Prefix+TEXT(" release ADS"),.5,[this]{Release(TEXT("Aim"));});
                }
                for (int32 Run=0;Run<2;++Run)
                {
                    AddStep(Prefix+TEXT(" locomotion"),.55,[this,S,Run]
                    { S->MovementStart=Character->GetActorLocation();Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));if(Run)Hold(TEXT("Sprint"),FInputActionValue(true)); },
                    [this,S,Prefix,Run]
                    { Check(Prefix+(Run?TEXT(" run motion"):TEXT(" walk motion")),FVector::Dist2D(S->MovementStart,Character->GetActorLocation())>30,
                        TEXT("actual movement from injected action"),Character->GetVelocity().ToString(),TEXT("B-Action")); });
                    Screenshot(Prefix+(Run?TEXT("_run"):TEXT("_walk")));
                    AddStep(TEXT("M5 locomotion release"),.7,[this]{Release(TEXT("Move"));Release(TEXT("Sprint"));});
                }
                AddStep(Prefix+TEXT(" attack input"),.1,[this]{Hold(TEXT("Attack"),FInputActionValue(true));});
                AddStep(Prefix+TEXT(" attack phase"),Weapon?.02:.17,[this]{Release(TEXT("Attack"));});
                Screenshot(Prefix+(Weapon?TEXT("_fire"):TEXT("_punch")));
                AddStep(TEXT("M5 attack finish"),1.,[]{});
                if(View && !Weapon)
                {
                    // Independent ordinary-input punches sample different phases;
                    // asynchronous screenshot completion must not pick later phases.
                    for(int32 Combo=0;Combo<2;++Combo)for(float Phase:{.14f,.28f,.46f})
                    {
                        const FString Label=FString::Printf(TEXT("m5_fp_punch%d_phase%03d"),Combo+1,FMath::RoundToInt(Phase*1000));
                        AddStep(Label+TEXT(" combo reset wait"),1.3,[]{});
                        if(Combo)
                        {
                            AddStep(Label+TEXT(" first punch input"),.05,[this]{Hold(TEXT("Attack"),FInputActionValue(true));});
                            AddStep(Label+TEXT(" first punch recovery"),.64,[this]{Release(TEXT("Attack"));});
                        }
                        AddStep(Label+TEXT(" ordinary attack input"),.05,[this]{Hold(TEXT("Attack"),FInputActionValue(true));});
                        AddStep(Label+TEXT(" sample phase"),Phase-.05,[this]{Release(TEXT("Attack"));},[this,Combo,Label]
                        {
                            const auto C=PC->GetCombatComponent();
                            Check(Label+TEXT(" actual active combo"),C->IsAttacking()&&C->GetComboIndex()==Combo,
                                TEXT("ordinary B-Action active punch; actual elapsed and montage position recorded"),
                                FString::Printf(TEXT("combo=%d elapsed=%.6f montage_position=%.6f animation_elapsed=%.6f"),
                                    C->GetComboIndex(),C->GetAttackElapsed(),C->GetAnimationPosition(),C->GetAttackAnimationElapsed()),TEXT("B-Action + A-animation readback"));
                        });
                        Screenshot(Label);
                    }
                    AddStep(TEXT("M5 sampled punches settle"),1.3,[]{});
                    // A continuous real-time B-Action sequence for wrist/elbow
                    // motion review. No montage seeking or per-frame posing.
                    AddStep(TEXT("M5 natural hands A open-street start"),1.,[this]
                        {PlaceCharacter(FVector(-8200,-6000,110),0);});
                    AddStep(TEXT("M5 natural hands continuous idle"),2.,[]{});
                    AddStep(TEXT("M5 natural hands continuous walk"),3.,[this]
                        {Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));});
                    AddStep(TEXT("M5 natural hands continuous run"),3.,[this]
                        {Hold(TEXT("Sprint"),FInputActionValue(true));},[this]
                    {
                        const auto P=Character->GetR2PresentationComponent();
                        Check(TEXT("M5 natural hands running solved geometry"),P && P->HasValidUnarmedArmIK(),
                            TEXT("shoulder and length preservation; final wrist matches planned relaxed orientation"),
                            P?P->GetGeometryDiagnostics():TEXT("missing"),TEXT("B-Action + A-final pose; naturalness is visual review"));
                    });
                    AddStep(TEXT("M5 natural hands continuous stop"),2.,[this]
                        {Release(TEXT("Move"));Release(TEXT("Sprint"));});
                    AddStep(TEXT("M5 natural hands recovery ordinary punch"),.05,[this]
                        {Hold(TEXT("Attack"),FInputActionValue(true));});
                    AddStep(TEXT("M5 natural hands recover to idle"),2.,[this]
                        {Release(TEXT("Attack"));});
                }
            }
            Tap(TEXT("ToggleWeapon"),.5); // Back to unarmed for the next view.
        }
        AddStep(TEXT("M5 A parked vehicle approach"),.8,[this]{ApproachCar();});
        Tap(TEXT("Interact"),1.);
        AddStep(TEXT("M5 actual seated TP"),.3,[]{},[this]
        {RequireCamera(TEXT("M5 driving TP"),IsDriving()&&!PC->IsFirstPersonPerspective(),TEXT("actual car possession and TP"),State(),TEXT("B-Action"));});
        Screenshot(TEXT("m5_driving_third"));
        Tap(TEXT("Perspective"),.6);
        AddStep(TEXT("M5 actual seated FP"),.3,[]{},[this]
        {Check(TEXT("M5 driving FP"),IsDriving()&&PC->IsFirstPersonPerspective(),TEXT("actual cabin camera"),State(),TEXT("B-Action"));});
        Screenshot(TEXT("m5_driving_first"));
        Tap(TEXT("Interact"),1.);
        AddStep(TEXT("M5 exit restores foot FP"),.3,[]{},[this]
        {RequireCamera(TEXT("M5 separate foot preference restored"),IsOnFoot()&&PC->IsFirstPersonPerspective(),TEXT("foot FP retained independently"),State(),TEXT("B-Action"));});
        ApproachInteraction(TEXT("M5_Contact"),TEXT("M5 review contact"));
        Tap(TEXT("Interact"),.3);Tap(TEXT("Interact"),.15);
        Screenshot(TEXT("m5_story_dialogue_style"));
        Tap(TEXT("DialogueCancel"),.3);
        ApproachInteraction(TEXT("M5_TowerUplink"),TEXT("M5 review tower"));
        Screenshot(TEXT("m5_tower_terminal"));
        AddStep(TEXT("M5 review evidence boundary"),.1,[this]
        {Check(TEXT("M5 review scope recorded"),true,TEXT("PNG files for human visual review; no OS input or art approval asserted"),TEXT("A-fixtures + B-Action; C=NOT_RUN; visual=USER_REVIEW"),TEXT("A-scope"));});
        return;
    }

    if(Mode==TEXT("m5_resume_full")){
        Tap(TEXT("Load"),1.);
        Stage(TEXT("Resume remaining regression from saved checkpoint"),TEXT("M5.Signal.Harbor"),TEXT("M5_Harbor"));
    }else{
    Stage(TEXT("Initial"),TEXT("M5.Signal.Contact"),TEXT("M5_Contact"));
    Music(TEXT("Street exploration"),EHCM5MusicMode::Exploration);
    Dialogue(TEXT("M5_Contact"),TEXT("m5_chapter1"));
    Stage(TEXT("Accepted delivery"),TEXT("M5.Signal.Harbor"),TEXT("M5_Harbor"));
    Music(TEXT("Contact interior"),EHCM5MusicMode::Interior);
    SaveCheck(TEXT("Harbor checkpoint"),TEXT("M5.Signal.Harbor"));
    if (Mode==TEXT("m5_save_probe")) return;
    Dialogue(TEXT("M5_Harbor"),TEXT("m5_chapter2"));
    Stage(TEXT("First investigation"),TEXT("M5.Signal.Ambush"),TEXT("M5_InterceptorA"));
    Tap(TEXT("Load"),1.);
    Stage(TEXT("Actual earlier checkpoint restored"),TEXT("M5.Signal.Harbor"),TEXT("M5_Harbor"));
    if (Mode==TEXT("m5_load_probe")) return;
    }
    Dialogue(TEXT("M5_Harbor"),TEXT("m5_chapter2_replay"));
    Stage(TEXT("Investigation after load"),TEXT("M5.Signal.Ambush"),TEXT("M5_InterceptorA"));
    Tap(TEXT("ToggleWeapon"),.6);
    if(GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/")))Tap(TEXT("ToggleWeapon"),.6);
    Tap(TEXT("Perspective"),.5);
    AddStep(TEXT("M5 actual ADS projection"),.6,[this]{Hold(TEXT("Aim"),FInputActionValue(true));},[this]
    {
        const double Ratio=FMath::Tan(FMath::DegreesToRadians(Character->GetFollowCamera()->FieldOfView*.5))
            /FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5));
        Check(TEXT("M5 ADS1.6 and reciprocal input"),FMath::IsNearlyEqual(Ratio,1.6,.001)&&FMath::IsNearlyEqual(PC->GetCurrentAimLookMultiplier(),.625,.0001),
            TEXT("real final POV1.6; look0.625"),FString::Printf(TEXT("projection=%.8f input=%.8f"),Ratio,PC->GetCurrentAimLookMultiplier()),TEXT("B-Action + A-camera readback"));
    });
    AddStep(TEXT("M5 ADS release"),.5,[this]{Release(TEXT("Aim"));});
    for (FName Id:{TEXT("M5_InterceptorA"),TEXT("M5_InterceptorB")})
    {
        AddStep(TEXT("M5 A intercept target fixture"),.5,[this,S,Id,PlaceNear]
        {
            S->Target=Cast<AHCM3NPC>(M5Target(GetWorld(),Id));
            S->TargetDamageHitsStart=PC->GetCombatComponent()->GetDamageHitCount();
            if(!RequireCamera(TEXT("M5 real live interceptor"),S->Target.IsValid()&&!S->Target->IsDead()&&PlaceNear(S->Target.Get(),360.f),
                TEXT("existing live authored NPC, player placed near it"),Id.ToString(),TEXT("A-fixture")))return;
            S->Target->GetCharacterMovement()->StopMovementImmediately();
            S->Target->GetCharacterMovement()->SetComponentTickEnabled(false);
            Check(TEXT("M5 stationary ballistic fixture disclosed"),true,TEXT("A: target movement tick suspended; health/collision/damage routes unchanged"),Id.ToString(),TEXT("A-fixture"));
        });
        Music(TEXT("Courtyard combat"),EHCM5MusicMode::Combat);
        AddStep(TEXT("M5 actual ADS for narrow adult target"),.5,[this]{Hold(TEXT("Aim"),FInputActionValue(true));});
        for (int32 Shot=0;Shot<5;++Shot)
        {
            AddStep(TEXT("M5 A aim at actual target"),.35,[this,S]
            {
                if(S->Target.IsValid()&&!S->Target->IsDead()){
                    FVector AimPoint=S->Target->GetActorLocation()+FVector(0,0,25);
                    const auto* Mesh=S->Target->GetMesh();
                    for(FName Bone:{TEXT("J_Bip_C_UpperChest"),TEXT("J_Bip_C_Chest"),TEXT("spine_03"),TEXT("spine_02")})
                        if(Mesh->GetBoneIndex(Bone)!=INDEX_NONE){AimPoint=Mesh->GetBoneLocation(Bone);break;}
                    PC->SetControlRotation((AimPoint-PC->PlayerCameraManager->GetCameraLocation()).Rotation());
                }
            });
            AddStep(TEXT("M5 real Attack action"),.1,[this,S]
            {
                S->BeforeShots=PC->GetCombatComponent()->GetShotCount();S->BeforeHits=PC->GetCombatComponent()->GetDamageHitCount();
                S->bAttemptShot=S->Target.IsValid()&&!S->Target->IsDead();
                if(S->bAttemptShot)Hold(TEXT("Attack"),FInputActionValue(true));
            });
            AddStep(TEXT("M5 release actual Attack"),.45,[this]{Release(TEXT("Attack"));},[this,S]
            {
                if(!S->bAttemptShot)return;
                auto C=PC->GetCombatComponent();
                Check(TEXT("M5 actual shot accepted; ray outcome recorded"),C->GetShotCount()==S->BeforeShots+1,
                    TEXT("one accepted shot; moving/falling targets may be missed; target defeat and actual damage are asserted separately"),
                    FString::Printf(TEXT("shots %d->%d hits %d->%d hit=%s hp=%.1f muzzleBlocked=%d muzzle=%s rayTarget=%s view=%s"),S->BeforeShots,C->GetShotCount(),S->BeforeHits,C->GetDamageHitCount(),
                        *GetNameSafe(C->GetLastHit().GetActor()),S->Target.IsValid()?S->Target->GetHealth():-1.f,C->WasLastMuzzleBlocked(),
                        *C->GetLastAcceptedShot().WorldMuzzle.ToString(),*C->GetLastAcceptedShot().AimPoint.ToString(),*PC->PlayerCameraManager->GetCameraLocation().ToString()),TEXT("B-Action ADS/fire; A current-pose aim fixture"));
            });
        }
        AddStep(TEXT("M5 release combat ADS"),.2,[this]{Release(TEXT("Aim"));});
        AddStep(TEXT("M5 actual interceptor outcome"),.6,[]{},[this,S,Id]
        {
            const bool bDead=S->Target.IsValid()&&S->Target->IsDead();
            if(S->Target.IsValid()&&!bDead)S->Target->GetCharacterMovement()->SetComponentTickEnabled(true);
            RequireCamera(TEXT("M5 actual interceptor defeated ")+Id.ToString(),bDead&&PC->GetCombatComponent()->GetDamageHitCount()>S->TargetDamageHitsStart,
                TEXT("real NPC dead after Attack actions, with actual damage hits recorded"),S->Story->GetDiagnostics(),TEXT("B-Action"));
        });
    }
    Stage(TEXT("Battle complete"),TEXT("M5.Signal.RecoverShard"),TEXT("M5_RelayCache"));
    if(Mode==TEXT("m5_battle_probe"))return;
    ApproachInteraction(TEXT("M5_RelayCache"),TEXT("M5 cache"));
    Tap(TEXT("Interact"),.6);
    Stage(TEXT("Evidence collected"),TEXT("M5.Signal.Tower"),TEXT("M5_TowerUplink"));
    SaveCheck(TEXT("Evidence checkpoint"),TEXT("M5.Signal.Tower"));
    Tap(TEXT("Load"),1.);
    AddStep(TEXT("M5 saved victory does not respawn enemies"),.5,[]{},[this,S]
    {
        bool bNoLiveEnabled=true;
        for(FName Id:{TEXT("M5_InterceptorA"),TEXT("M5_InterceptorB")})
        {auto N=Cast<AHCM3NPC>(M5Target(GetWorld(),Id));bNoLiveEnabled &= N&&(N->IsDead()||!N->IsNPCEnabled());}
        Check(TEXT("M5 defeated IDs persist without repeat enemies"),S->Story->ExportState().DefeatedInterceptorIds.Num()==2&&bNoLiveEnabled,
            TEXT("two persistent victories, no live enabled interceptor"),S->Story->GetDiagnostics(),TEXT("B-Action Load + A-readback"));
    });
    ApproachInteraction(TEXT("M5_TowerUplink"),TEXT("M5 tower approach"));
    Tap(TEXT("Interact"),.4);
    Stage(TEXT("Broadcast started"),TEXT("M5.Signal.Upload"),TEXT("M5_TowerUplink"));
    AddStep(TEXT("M5 A depart broadcast radius"),.4,[this]{PlaceCharacter(FVector(9500,9500,110),0);});
    Stage(TEXT("Leaving interrupts safely"),TEXT("M5.Signal.Tower"),TEXT("M5_TowerUplink"));
    ApproachInteraction(TEXT("M5_TowerUplink"),TEXT("M5 tower return"));Tap(TEXT("Interact"),.4);
    AddStep(TEXT("M5 A restored zero-health failure fixture"),.25,[this]
    {
        TStrongObjectPtr<UHCM1SaveGame> Fixture(NewObject<UHCM1SaveGame>());
        PC->GetCombatComponent()->FillSave(Fixture.Get());Fixture->M4PlayerHealth=0;
        PC->GetCombatComponent()->RestoreSave(Fixture.Get());
        Check(TEXT("M5 failure stimulus explicitly A"),PC->GetCombatComponent()->GetPlayerHealth()==0,
            TEXT("A: zero-health save-restore fixture, not a claimed NPC attack"),TEXT("production timed recovery is then observed"),TEXT("A-fixture"));
    });
    Stage(TEXT("Defeat safely stops broadcast"),TEXT("M5.Signal.Tower"),TEXT("M5_TowerUplink"));
    AddStep(TEXT("M5 actual timed player recovery"),4.,[]{},[this,S]
    {
        RequireCamera(TEXT("M5 production recovery retains evidence"),PC->GetCombatComponent()->GetPlayerHealth()>0&&IsOnFoot()
            &&S->Story->GetStageId()==TEXT("M5.Signal.Tower")&&S->Story->ExportState().DefeatedInterceptorIds.Num()==2,
            TEXT("recovered player, Tower stage, two victories retained"),S->Story->GetDiagnostics(),TEXT("A failure fixture + runtime recovery"));
    });
    ApproachInteraction(TEXT("M5_TowerUplink"),TEXT("M5 tower retry"));Tap(TEXT("Interact"),.3);
    Tap(TEXT("Pause"),.6);
    AddStep(TEXT("M5 paused music inspect"),.2,[]{},[this,S]
    {Check(TEXT("M5 pause halts music"),PC->IsPauseMenuOpen()&&S->Story->GetMusic()->IsMusicPaused(),TEXT("pause menu and explicit audio pause"),S->Story->GetMusic()->GetDiagnostics(),TEXT("B-Action Pause"));});
    Tap(TEXT("Pause"),.4);
    // Pausing can cancel the active upload through the existing interaction
    // boundary. Retry E is idempotent when Upload already remains active.
    Tap(TEXT("Interact"),.3);
    Music(TEXT("Tower decoding interior"),EHCM5MusicMode::Interior);
    AddStep(TEXT("M5 wait real upload duration"),6.3,[]{});
    Stage(TEXT("Public evidence broadcast"),TEXT("M5.Signal.Debrief"),TEXT("M5_Contact"));
    Dialogue(TEXT("M5_Contact"),TEXT("m5_chapter4"));
    Stage(TEXT("First night complete"),TEXT("M5.Signal.Complete"),NAME_None);
    if((Mode==TEXT("m5_full")||Mode==TEXT("m5_resume_full"))&&GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/")))AddM5VS3SideQuests();
    SaveCheck(TEXT("Complete checkpoint"),TEXT("M5.Signal.Complete"));Tap(TEXT("Load"),1.);
    Stage(TEXT("Complete reload"),TEXT("M5.Signal.Complete"),NAME_None);
    AddStep(TEXT("M5 tagged-state negative validation"),.2,[this,S]
    {
        FString Error;auto Data=S->Story->ExportState();const FName Before=S->Story->GetStageId();
        Data.DefeatedInterceptorIds.Add(TEXT("M5_InterceptorA"));
        Check(TEXT("M5 duplicate victory rejected"),!S->Story->ValidateState(Data,Error),TEXT("reject duplicate stable ID"),Error,TEXT("A-validation"));
        Data=S->Story->ExportState();Data.StageId=TEXT("M5.UnknownStage");
        Check(TEXT("M5 unknown stage rejected"),!S->Story->ValidateState(Data,Error),TEXT("reject unknown stage"),Error,TEXT("A-validation"));
        Data=S->Story->ExportState();Data.Version=99;
        Check(TEXT("M5 future version rejected"),!S->Story->ValidateState(Data,Error),TEXT("reject unknown version"),Error,TEXT("A-validation"));
        FHCM5StorySaveData Legacy;
        Check(TEXT("M5 legacy optional field validates without mutation"),S->Story->ValidateState(Legacy,Error)&&S->Story->GetStageId()==Before,
            TEXT("version0 optional extension accepted; Validate does not advance/reset the live quest"),S->Story->GetDiagnostics(),TEXT("A-validation"));
    });
    Screenshot(TEXT("m5_story_complete"));
    if((Mode==TEXT("m5_full")||Mode==TEXT("m5_resume_full"))&&GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/"))){const FString OriginalMode=Mode;Mode=TEXT("m5_vs3_combat");AddM5VS3Tests();Mode=TEXT("m5_vs3_flight");AddM5VS3Tests();Mode=OriginalMode;}
    if((Mode==TEXT("m5_full")||Mode==TEXT("m5_resume_full"))&&GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/"))){
        AddStep(TEXT("Natural private BGM two-track cycle"),1,[this]{Steps[StepIndex].Duration=FMath::Max(1.,435.-GetWorld()->GetTimeSeconds());},[this,S]{
            const FString Diag=S->Story->GetMusic()->GetDiagnostics();TSharedPtr<FJsonObject> J;const auto Reader=TJsonReaderFactory<>::Create(Diag);
            const bool Parsed=FJsonSerializer::Deserialize(Reader,J)&&J.IsValid();
            Check(TEXT("Private BGM naturally alternates back to track one"),Parsed&&J->GetBoolField(TEXT("private_playlist"))&&J->GetNumberField(TEXT("transitions"))>=2&&J->GetNumberField(TEXT("playlist_index"))==0,TEXT("two natural-duration crossfades, index zero; audibility is separate"),Diag,TEXT("A-runtime audio component clocks, no seek/speedup"));
        });
    }
}
