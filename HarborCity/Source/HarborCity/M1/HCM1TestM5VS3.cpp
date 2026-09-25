#include "HCM1TestRunner.h"
#include "HCM1Character.h"
#include "HCM1Vehicle.h"
#include "HCM1PlayerController.h"
#include "HCM1SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "M3/HCM3NPC.h"
#include "M3/HCM3Experience.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "M4/HCM4CombatComponent.h"
#include "M5VS3/HCM5VS3Abilities.h"
#include "M5VS2/HCM5VS2FlightComponent.h"
#include "M5VS2/HCM5VS2FlightVisualComponent.h"
#include "M5VS2/HCM5VS2CornerTimeDirector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void AHCM1TestRunner::AddM5VS3Tests()
{
    const bool Demo=Mode==TEXT("m5_vs3_demo");
    auto Shot=[this](FString Name){
        AddStep(Name+TEXT(" capture"),.3,[this,Name]{
            const FString File=RunDirectory/(Name+TEXT(".png"));M3CaptureFiles.AddUnique(File);M3LastCaptureFrame=GFrameCounter;
            FScreenshotRequest::RequestScreenshot(File,true,false,false,FIntRect(),true);
        });AddM3CaptureCompletion(Name);
    };
    AddStep(TEXT("VS3 actual runtime components"),2,[]{},[this]{
        auto* A=Character->FindComponentByClass<UHCM5VS3Abilities>();
        Check(TEXT("VS3 abilities enabled on current world"),A&&A->IsEnabled(),TEXT("enabled"),GetWorld()->GetOutermost()->GetName(),TEXT("A-runtime"));
        Check(TEXT("VS3 initial stamina"),FMath::IsNearlyEqual(Character->GetFlightComponent()->GetStamina(),100),TEXT("100"),Character->GetFlightComponent()->GetFlightDiagnostics(),TEXT("A-runtime"));
    });
    if(Mode==TEXT("m5_vs3_ride_probe")||Mode==TEXT("m5_vs3_ride_resume")){
        if(Mode==TEXT("m5_vs3_ride_resume"))Tap(TEXT("Load"),1.);
        AddM5VS3SideQuests();Tap(TEXT("Save"),.8);
        AddStep(TEXT("Ride checkpoint actual file"),.2,[]{},[this]{
            const auto* Saved=Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0));
            Check(TEXT("Both side quests serialized complete"),Saved&&Saved->CoffeeStage==EHCM3QuestStage::Complete&&Saved->RideStage==EHCM3QuestStage::Complete,
                TEXT("actual F5 file contains both completed side quests"),PC->GetStatusMessage(),TEXT("B-Action + A-file readback"));
        });Tap(TEXT("Load"),1.);Shot(TEXT("RideCheckpoint"));return;
    }
    if(Mode==TEXT("m5_vs3_art")){
        AddStep(TEXT("A clear-plaza arcane view fixture"),3,[this]{PlaceCharacter(FVector(0,-6800,110),90);PC->SetControlRotation(FRotator(-8,90,0));});
        for(int32 View=0;View<2;++View){
            if(View)Tap(TEXT("Perspective"),1);
            for(int32 I=0;I<4;++I){
                Tap(FName(*FString::Printf(TEXT("Spell%d"),I+1)),.2);
                Shot(FString::Printf(TEXT("Arcane_%s_%d"),View?TEXT("FP"):TEXT("TP"),I+1));
                AddStep(TEXT("Natural spell recovery for next art view"),I==1?10:7,[]{});
            }
        }
        return;
    }
    if(Mode==TEXT("m5_vs3_combat")||Demo){
        for(int32 I=0;I<2;++I){
            AddStep(TEXT("B normalize previous weapon using Q"),.05,[this]{if(PC->GetCombatComponent()->GetWeaponMode()!=EHCM4WeaponMode::Unarmed)Hold(TEXT("ToggleWeapon"),FInputActionValue(true));},[this]{Release(TEXT("ToggleWeapon"));});
            AddStep(TEXT("Allow actual weapon transition"),1.,[]{});
        }
        const auto Target=MakeShared<TWeakObjectPtr<AHCM3NPC>>();
        auto Approach=[this,Target](int32 Index,float Distance){
            AddStep(TEXT("A adult combat approach fixture"),.8,[this,Target,Index,Distance]{
                for(TActorIterator<AHCM3NPC> It(GetWorld());It;++It)if(It->StableId==FName(*FString::Printf(TEXT("VS2_Citizen%02d"),Index))){*Target=*It;break;}
                if(Target->IsValid()&&!Target->Get()->IsDead()){
                    auto* N=Target->Get();const FVector P=N->GetActorLocation()-FVector(Distance,0,0);
                    PlaceCharacter(P,0);PC->SetControlRotation(FRotator(-8,0,0));
                }
            },[this,Target]{if(Target->IsValid())PC->SetControlRotation((Target->Get()->GetActorLocation()+FVector(0,0,25)-PC->PlayerCameraManager->GetCameraLocation()).Rotation());});
        };
        Approach(0,115);Tap(TEXT("Attack"),.8);Approach(0,110);Tap(TEXT("Attack"),.8);Approach(0,110);Tap(TEXT("Attack"),1);
        Shot(TEXT("PunchImpact"));
        AddStep(TEXT("B punch outcome"),3,[]{},[this,Target]{Check(TEXT("Actual punch damages target"),Target->IsValid()&&Target->Get()->GetHealth()<100,TEXT("health reduced by Attack Action"),Target->IsValid()?Target->Get()->GetCombatDiagnostics():TEXT("missing"),TEXT("B-Action + A placement"));});
        Tap(TEXT("ToggleWeapon"),1.1);
        AddStep(TEXT("B sword selection readback"),.2,[]{},[this]{Check(TEXT("Q selects sword"),PC->GetCombatComponent()->GetWeaponMode()==EHCM4WeaponMode::Sword,TEXT("Sword"),PC->GetCombatComponent()->GetHUDText(),TEXT("B-Action"));});
        Shot(TEXT("SwordDraw"));
        for(int32 I=0;I<3;++I){Approach(1,125);Tap(TEXT("Attack"),.65);}
        Shot(TEXT("SwordCombo"));
        Approach(1,125);AddStep(TEXT("B held sword heavy attack"),.8,[this]{Hold(TEXT("Attack"),FInputActionValue(true));},[this]{Release(TEXT("Attack"));});
        AddStep(TEXT("B sword damage readback"),1,[]{},[this,Target]{Check(TEXT("Blade sweep damages adult target"),Target->IsValid()&&Target->Get()->GetHealth()<100,TEXT("health reduced via input-driven blade sweep"),Target->IsValid()?Target->Get()->GetCombatDiagnostics():TEXT("missing"),TEXT("B-Action + A placement"));});
        Tap(TEXT("Perspective"),.7);Tap(TEXT("Attack"),.2);Shot(TEXT("SwordFirstPerson"));Tap(TEXT("Perspective"),.8);
        Tap(TEXT("ToggleWeapon"),1.1);Approach(2,350);Tap(TEXT("Attack"),.5);Tap(TEXT("Attack"),.5);Shot(TEXT("MagicGun"));
        AddStep(TEXT("B magic gun outcome"),1,[]{},[this,Target]{Check(TEXT("Magic gun uses existing firearm damage"),Target->IsValid()&&Target->Get()->GetHealth()<100&&PC->GetCombatComponent()->GetWeaponMode()==EHCM4WeaponMode::Pistol,TEXT("Pistol damage authority"),PC->GetCombatComponent()->GetHUDText(),TEXT("B-Action"));});
        Tap(TEXT("ToggleWeapon"),1.1);
        for(int32 I=0;I<4;++I){
            Approach(3+I,160);
            const auto Before=MakeShared<float>();
            AddStep(TEXT("A MP readback"),.1,[this,Before]{*Before=Character->FindComponentByClass<UHCM5VS3Abilities>()->GetMP();});
            Tap(FName(*FString::Printf(TEXT("Spell%d"),I+1)),.10);
            AddStep(TEXT("B spell cost/cooldown"),.05,[]{},[this,I,Before]{auto* A=Character->FindComponentByClass<UHCM5VS3Abilities>();const float Cost[]={15,35,30,25};Check(FString::Printf(TEXT("Spell %d MP cost once"),I+1),FMath::Abs(*Before-A->GetMP()-Cost[I])<1,TEXT("configured cost, no duplicate binding"),FString::Printf(TEXT("before %.2f after %.2f cooldown %.2f"),*Before,A->GetMP(),A->GetCooldown(I)),TEXT("B-Action"));});
            Shot(FString::Printf(TEXT("Spell%dRune"),I+1));
            AddStep(TEXT("Observe spell and regenerate"),I==1?10:7,[]{});
        }
        // Pause belongs to the non-recorded regression. The recorder correctly
        // stops on pause/focus loss; keep its safeguards intact for the demo.
        if(!Demo){Tap(TEXT("Pause"),.5);Tap(TEXT("Pause"),.5);}
    }
    if(Mode==TEXT("m5_vs3_flight")||Demo){
        AddStep(TEXT("A open ground departure"),2,[this]{PlaceCharacter(FVector(0,-6800,110),90);PC->SetControlRotation(FRotator(-8,90,0));});
        Tap(TEXT("FlightToggle"),1);
        AddStep(TEXT("B ascend"),1.5,[this]{Hold(TEXT("Jump"),FInputActionValue(true));},[this]{Release(TEXT("Jump"));});
        const auto Before=MakeShared<float>();
        AddStep(TEXT("A stamina readback"),.05,[this,Before]{*Before=Character->GetFlightComponent()->GetStamina();});
        AddStep(TEXT("B hover stamina consumption"),4,[]{},[this,Before]{Check(TEXT("Normal flight consumes 5 per second"),FMath::Abs((*Before-Character->GetFlightComponent()->GetStamina())-20)<1.2,TEXT("20 over four simulated seconds"),Character->GetFlightComponent()->GetFlightDiagnostics(),TEXT("B-Action flight + A readback"));});
        Shot(TEXT("FlightStamina"));
        if(Demo){Tap(TEXT("Perspective"),.8);Shot(TEXT("FlightFirstPerson"));Tap(TEXT("Perspective"),.6);}
        Tap(TEXT("Spell1"),.5);Tap(TEXT("ToggleWeapon"),.3);Tap(TEXT("Attack"),.3);
        AddStep(TEXT("B flight action restrictions"),.2,[]{},[this]{Check(TEXT("Flight keeps hands empty and MP full"),PC->GetCombatComponent()->GetWeaponMode()==EHCM4WeaponMode::Unarmed&&!Character->FindComponentByClass<UHCM5VS3Abilities>()->IsBusy(),TEXT("no weapon/cast/attack"),PC->GetCombatComponent()->GetHUDText(),TEXT("B-Action"));});
        AddStep(TEXT("B stamina exhaust and automatic landing"),19,[]{});
        Shot(TEXT("StaminaAutoLanding"));
        AddStep(TEXT("B exhaustion landing readback"),1,[]{},[this]{Check(TEXT("Stamina auto landing reaches ground"),!PC->IsFlying(),TEXT("grounded, no manual landing input"),Character->GetFlightComponent()->GetFlightDiagnostics(),TEXT("B-Action"));});
        AddStep(TEXT("Ground recovery"),10,[]{},[this]{Check(TEXT("Ground stamina recovery"),Character->GetFlightComponent()->GetStamina()>99,TEXT("100 after ten seconds"),Character->GetFlightComponent()->GetFlightDiagnostics(),TEXT("A-runtime"));});
    }
    if(Demo){
        // Same current town, three original feather arrangements. Camera placement
        // is an A review fixture; hover/forward/boost are actual movement Actions.
        for(const TCHAR* Period:{TEXT("Afternoon"),TEXT("Night")})for(int32 Style=0;Style<3;++Style){
            AddStep(TEXT("A wing style and time fixture"),2,[this,Style,Period]{
                for(TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld());It;++It)It->SetTimeOfDay(Period);
                Character->FindComponentByClass<UHCM5VS2FlightVisualComponent>()->SetWingStyle(Style);
                PlaceCharacter(FVector(0,-6800,110),90);PC->SetControlRotation(FRotator(-8,90,0));
            });
            Tap(TEXT("FlightToggle"),.6);AddStep(TEXT("B wing climb"),1.2,[this]{Hold(TEXT("Jump"),FInputActionValue(true));},[this]{Release(TEXT("Jump"));});
            Shot(FString::Printf(TEXT("Wing%d_%s_Hover"),Style+1,Period));
            AddStep(TEXT("B wing forward"),.6,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));});Shot(FString::Printf(TEXT("Wing%d_%s_Forward"),Style+1,Period));
            AddStep(TEXT("B wing boost"),.5,[this]{Hold(TEXT("Sprint"),FInputActionValue(true));});Shot(FString::Printf(TEXT("Wing%d_%s_Boost"),Style+1,Period));
            AddStep(TEXT("Release movement"),.1,[this]{Release(TEXT("Sprint"));Release(TEXT("Move"));});Tap(TEXT("FlightToggle"),8);AddStep(TEXT("Rest after wing comparison"),10,[]{});
        }
        for(const FVector P:{FVector(-4700,-5000,110),FVector(5800,-5000,110)}){
            AddStep(TEXT("A completed facade viewpoint"),2,[this,P]{PlaceCharacter(P-FVector(0,700,0),90);PC->SetControlRotation(FRotator(-8,90,0));});
            AddStep(TEXT("B walk into furnished interior"),3,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Move"));});AddStep(TEXT("Observe interior"),5,[]{});
        }
        AddStep(TEXT("A open plaza save demonstration fixture"),2,[this]{PlaceCharacter(FVector(0,-6800,110),90);});
        Tap(TEXT("Save"),.8);
        const auto SavedPoint=MakeShared<FVector>();
        const auto HasSave=MakeShared<bool>(false);
        AddStep(TEXT("A actual demonstration slot readback"),.2,[this,SavedPoint,HasSave]{
            const auto* Saved=Cast<UHCM1SaveGame>(UGameplayStatics::LoadGameFromSlot(PC->GetSaveSlotName(),0));
            *HasSave=Saved!=nullptr;if(Saved)*SavedPoint=Saved->PlayerTransform.GetLocation();
            Check(TEXT("Demo F5 writes actual isolated slot"),*HasSave,TEXT("readable save after Save action"),PC->GetStatusMessage(),TEXT("B-Action + A-file readback"));
        });
        AddStep(TEXT("B move away before F9"),1,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Move"));});
        AddStep(TEXT("B stand before F9"),1,[]{});Tap(TEXT("Load"),1);
        AddStep(TEXT("A demonstration restored position"),.2,[]{},[this,SavedPoint,HasSave]{
            Check(TEXT("Demo F9 restores actual saved position"),*HasSave&&FVector::Dist2D(Character->GetActorLocation(),*SavedPoint)<5,
                TEXT("saved position within 5 cm horizontally"),PC->GetStatusMessage()+TEXT(" ")+Character->GetActorLocation().ToString(),TEXT("B-Action + A-readback"));
        });Shot(TEXT("DemoSaveLoad"));
    }
}

void AHCM1TestRunner::AddM5VS3SideQuests()
{
    auto Talk=[this](FName Id){
        AddStep(TEXT("A existing side-quest NPC approach"),1,[this,Id]{
            auto* E=PC->GetM3Experience();auto* N=E?E->FindNPC(Id):nullptr;bool Placed=false;
            if(N&&!N->IsDead())for(int32 I=0;I<16&&!Placed;++I){
                const FVector P=N->GetActorLocation()+FRotator(0,I*22.5f,0).Vector()*180;
                FCollisionQueryParams Q(SCENE_QUERY_STAT(VS3QuestApproach),false,Character);Q.AddIgnoredActor(N);FHitResult H;
                if(GetWorld()->LineTraceSingleByChannel(H,P+FVector(0,0,40),P-FVector(0,0,160),ECC_Visibility,Q)&&H.ImpactNormal.Z>.8){
                    const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();const FVector C=H.ImpactPoint+FVector(0,0,Half+5);
                    if(!GetWorld()->OverlapBlockingTestByChannel(C,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(37,Half),Q)){
                        PlaceCharacter(C,(N->GetActorLocation()-C).Rotation().Yaw);Placed=true;
                    }
                }
            }
            Check(TEXT("Side-quest placement found clear ground"),Placed,TEXT("actual authored actor; collision-clear A placement"),Id.ToString(),TEXT("A-fixture"));
        },[this,Id]{auto* E=PC->GetM3Experience();auto* N=E?E->FindNPC(Id):nullptr;
            Check(TEXT("Side-quest NPC reachable after camera settles"),N&&PC->CanReachInteraction(N),TEXT("actual reachable target after normal ticks"),Id.ToString(),TEXT("A-runtime"));});
        Tap(TEXT("Interact"),.2);
        // Reveal/advance only while the current dialogue remains open. Fixed
        // tap counts can leave a typewriter line open or start a new conversation.
        for(int32 I=0;I<16;++I){
            AddStep(TEXT("B advance current side-quest dialogue"),.10,[this]{if(PC->IsDialogueOpen())Hold(TEXT("Interact"),FInputActionValue(true));},[this]{Release(TEXT("Interact"));});
            AddStep(TEXT("B dialogue release frame"),.10,[]{});
        }
    };
    if(Mode!=TEXT("m5_vs3_ride_resume")){Talk(TEXT("M3_LinXia"));Talk(TEXT("M3_ChenBo"));Talk(TEXT("M3_ANing"));}
    AddStep(TEXT("B side-quest acceptance outcomes"),.5,[]{},[this]{auto* E=PC->GetM3Experience();Check(TEXT("Coffee delivered through actual dialogue input"),E&&E->GetCoffeeStage()==EHCM3QuestStage::Complete,TEXT("Complete"),E?E->GetQuestHUD(PC):TEXT("missing"),TEXT("B-Action"));Check(TEXT("Ride accepted through actual dialogue input"),E&&E->GetRideStage()==EHCM3QuestStage::Active,TEXT("Active"),E?E->GetQuestHUD(PC):TEXT("missing"),TEXT("B-Action"));});
    AddStep(TEXT("A parked curb fixture within current town"),2,[this]{
        auto* E=PC->GetM3Experience();auto* N=E?E->FindNPC(E->PassengerId):nullptr;if(!N)return;
        bool Found=false;FString Candidates;
        for(float XOffset:{0.f,-240.f,240.f,-320.f,320.f}){
            PlaceCar(FTransform(FRotator::ZeroRotator,FVector(N->GetActorLocation().X+XOffset,E->PickupCurbY+150,95)));
            TInlineComponentArray<UStaticMeshComponent*> Meshes(Car);for(auto* M:Meshes)if(M&&M->GetFName()==TEXT("BodyVisual")&&M->GetStaticMesh()){
                const FBox B=M->GetStaticMesh()->GetBoundingBox().TransformBy(M->GetComponentTransform());auto T=Car->GetActorTransform();T.AddToTranslation(FVector(0,E->PickupCurbY+25-B.Min.Y,0));PlaceCar(T);break;
            }
            FTransform Door,Stand;const bool Legal=E->CanPickupAtCurb(N,Car),Exit=Car->FindSafeExitTransform(N,Door),Walk=Exit&&N->ResolvePassengerDoorStand(Door,Stand);
            Candidates+=FString::Printf(TEXT("xOffset=%.0f curb=%d exit=%d walk=%d; "),XOffset,Legal,Exit,Walk);
            if(Legal&&Exit&&Walk&&FVector::Dist2D(N->GetActorLocation(),Car->GetActorLocation())<600){Found=true;break;}
        }
        Check(TEXT("Pickup fixture has legal curb and NPC-walkable door"),Found,TEXT("read-only safety queries choose A parking fixture; no condition override"),Candidates,TEXT("A-fixture"));
        PlaceCharacter(Car->GetActorLocation()+FVector(0,200,0),-90);
    });
    Tap(TEXT("Interact"),12);
    AddStep(TEXT("B passenger boards at curb"),.5,[]{},[this]{auto* E=PC->GetM3Experience();auto* N=E?E->FindNPC(E->PassengerId):nullptr;
        FTransform Door=FTransform::Identity;const bool SafeDoor=N&&Car->FindSafeExitTransform(N,Door);FTransform Stand=FTransform::Identity;const bool SafeStand=N&&SafeDoor&&N->ResolvePassengerDoorStand(Door,Stand);
        const FString Details=FString::Printf(TEXT("driving=%d npc=%s car=%s safeDoor=%d door=%s safeNPCStand=%d npcState=%s world=%s"),IsDriving(),N?*N->GetActorLocation().ToString():TEXT("missing"),*Car->GetActorLocation().ToString(),SafeDoor,*Door.GetLocation().ToString(),SafeStand,N?*N->GetCombatDiagnostics():TEXT("missing"),E?*E->GetCombatWorldDiagnostics():TEXT("missing"));
        Check(TEXT("Real passenger boarding on current town curb"),IsDriving()&&N&&N->GetIsPassenger(),TEXT("driving, passenger attached through normal Tick"),Details,TEXT("B-Action Interact + A parking fixture; A-read-only clearance query"));});
    AddStep(TEXT("B driving input and steering"),1,[this]{Hold(TEXT("Drive"),FInputActionValue(FVector2D(.2,1)));},[this]{Release(TEXT("Drive"));});
    AddStep(TEXT("B brake then reverse"),3,[this]{Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,-1)));},[this]{Release(TEXT("Drive"));Check(TEXT("Driving reverse signed velocity"),Car->GetVelocity().Dot(Car->GetActorForwardVector())< -5,TEXT("actual negative forward velocity"),Car->GetVelocity().ToString(),TEXT("B-Action"));});
    AddStep(TEXT("A destination approach fixture"),.5,[this]{auto* E=PC->GetM3Experience();if(E)PlaceCar(FTransform(FRotator(0,90,0),E->RideParkingCenter+FVector(0,-190,90)));});
    AddStep(TEXT("B slow final parking approach"),.3,[this]{Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,.35)));},[this]{Release(TEXT("Drive"));});
    AddStep(TEXT("B stop within destination"),4,[this]{Hold(TEXT("Handbrake"),FInputActionValue(true));},[this]{Release(TEXT("Handbrake"));auto* E=PC->GetM3Experience();Check(TEXT("Ride completed by normal parking/dropoff condition"),E&&E->GetRideStage()==EHCM3QuestStage::Complete,TEXT("Complete; no direct quest mutation"),E?E->GetQuestHUD(PC):TEXT("missing"),TEXT("B-Action + A travel placement; not continuous route proof"));});
    Tap(TEXT("Interact"),2);
}
