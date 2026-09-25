#include "HCM1TestRunner.h"
#include "HCM1PlayerController.h"
#include "HCM1Character.h"
#include "HCM1Vehicle.h"
#include "M3/HCM3NPC.h"
#include "M5VS2/HCM5VS2TownRuntime.h"
#include "M5VS2/HCM5VS2FlightComponent.h"
#include "M5VS2/HCM5VS2CornerTimeDirector.h"
#include "M4R2/HCM4R2Navigation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Misc/Paths.h"

void AHCM1TestRunner::AddM5VS2Tests()
{
    AddStep(TEXT("Town runtime references"),2,[]{},[this]
    {
        int32 Towns=0,Adults=0,Excluded=0;
        for(TActorIterator<AHCM5VS2TownRuntime> It(GetWorld());It;++It)
        {
            ++Towns;
            Check(TEXT("Standard fill and world audio runtime"),It->GetTownDiagnostics().Contains(TEXT("true")),
                TEXT("readback; audible recording verified separately"),It->GetTownDiagnostics(),TEXT("A-runtime"));
            Check(TEXT("Town navigation asset"),It->NavigationMap&&It->NavigationMap->SourceLevel.Contains(TEXT("/Part3/Town_")),
                TEXT("current town map data"),GetNameSafe(It->NavigationMap),TEXT("A-runtime"));
        }
        for(TActorIterator<AHCM3NPC> It(GetWorld());It;++It)
        {
            auto* C=It->FindComponentByClass<USkeletalMeshComponent>();
            const FString Path=C&&C->GetSkeletalMeshAsset()?C->GetSkeletalMeshAsset()->GetPathName():TEXT("");
            if(Path.Contains(TEXT("AvatarSample_Q/"))||Path.Contains(TEXT("AvatarSample_R/"))||Path.Contains(TEXT("AvatarSample_V/")))++Adults;
            if(Path.Contains(TEXT("AvatarSample_J/"))||Path.Contains(TEXT("AvatarSample_T/"))||Path.Contains(TEXT("AvatarSample_U/"))||Path.Contains(TEXT("AvatarSample_W/")))++Excluded;
        }
        Check(TEXT("Rejected young appearances excluded"),Excluded==0&&Adults>=19,TEXT("19 adult-base NPCs; diversity is separate art review"),FString::Printf(TEXT("adult_base=%d excluded=%d"),Adults,Excluded),TEXT("A-runtime"));
        Check(TEXT("Town actor unique"),Towns==1,TEXT("one runtime actor"),FString::FromInt(Towns),TEXT("A-runtime"));
        Check(TEXT("Help panel starts closed"),!PC->IsControlsPanelOpen(),TEXT("closed"),FString::FromInt(PC->IsControlsPanelOpen()),TEXT("A-runtime"));
    });
    if(Mode==TEXT("m5_vs2_demo"))
    {
        // Locations are disclosed shot fixtures. Only the actions move the player
        // within each shot; this is not a continuous walking-route acceptance.
        const TArray<FVector> Spots={{0,-6800,110},{0,900,110},{1800,3300,110},
            {-4500,6700,430},{-4700,-4650,110},{5800,-4650,110}};
        for(const TCHAR* Period:{TEXT("Afternoon"),TEXT("Dusk"),TEXT("Night")})
        {
            AddStep(FString(Period)+TEXT(" A period fixture"),3,[this,Period]
            {for(TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld());It;++It)It->SetTimeOfDay(Period);});
            for(int32 I=0;I<Spots.Num();++I)
            {
                AddStep(TEXT("A district shot fixture"),3,[this,Spots,I]
                {PlaceCharacter(Spots[I],90);PC->SetViewTarget(Character);PC->SetControlRotation(FRotator(-12,90,0));});
                AddStep(TEXT("B district walking"),I<4?3:0.4,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Move"));});
                AddStep(TEXT("B observed idle"),5,[]{});
            }
        }
        AddStep(TEXT("A combat daylight"),2,[this]{for(TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld());It;++It)It->SetTimeOfDay(TEXT("Afternoon"));});
        const auto Target=MakeShared<TWeakObjectPtr<AHCM3NPC>>();
        for(int32 Weapon=0;Weapon<2;++Weapon)
        {
            AddStep(TEXT("A adult target shot fixture"),1,[this,Target,Weapon]
            {
                for(TActorIterator<AHCM3NPC> It(GetWorld());It;++It)if(It->StableId==FName(Weapon?TEXT("VS2_Citizen01"):TEXT("VS2_Citizen00"))){*Target=*It;break;}
            });
            if(Weapon)Tap(TEXT("ToggleWeapon"),.5);
            for(int32 Hit=0;Hit<4;++Hit)
            {
                AddStep(TEXT("A grounded attack approach"),.4,[this,Target,Weapon]
                {
                    if(!Target->IsValid()||Target->Get()->IsDead())return;
                    const auto* N=Target->Get();const FVector P=N->GetActorLocation()-N->GetActorForwardVector()*(Weapon?330:120);
                    PlaceCharacter(P,(N->GetActorLocation()-P).Rotation().Yaw);
                },[this,Target]
                {
                    // Read the final POV only after the explicit placement has settled.
                    if(Target->IsValid()&&!Target->Get()->IsDead())
                        PC->SetControlRotation((Target->Get()->GetActorLocation()+FVector(0,0,25)-PC->PlayerCameraManager->GetCameraLocation()).Rotation());
                });
                Tap(TEXT("Attack"),.7);
            }
            AddStep(TEXT("Observe physical reaction"),6,[]{},[this,Target]
            {Check(TEXT("B actual attack damages adult NPC"),Target->IsValid()&&Target->Get()->GetHealth()<100,TEXT("damage through Attack Action, not direct damage function"),Target->IsValid()?Target->Get()->GetCombatDiagnostics():TEXT("missing"),TEXT("B-Action with A shot placement"));});
        }
        Tap(TEXT("ToggleWeapon"),.5);
        AddStep(TEXT("A approach current parked vehicle"),2,[this]{PlaceCharacter(Car->GetActorLocation()-Car->GetActorRightVector()*180,Car->GetActorRotation().Yaw);PC->SetControlRotation(FRotator(-10,Car->GetActorRotation().Yaw,0));});
        Tap(TEXT("Interact"),1);
        AddStep(TEXT("B driving"),3,[this]{Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Drive"));});
        AddStep(TEXT("B braking and reverse"),3,[this]{Hold(TEXT("Drive"),FInputActionValue(FVector2D(0,-1)));},[this]{Release(TEXT("Drive"));});
        AddStep(TEXT("B handbrake"),2,[this]{Hold(TEXT("Handbrake"),FInputActionValue(true));},[this]{Release(TEXT("Handbrake"));});
        Tap(TEXT("Interact"),2);
        AddStep(TEXT("A flight departure"),2,[this]{if(IsOnFoot()){PlaceCharacter(FVector(0,-6800,110),90);PC->SetControlRotation(FRotator(-10,90,0));}});
        Tap(TEXT("FlightToggle"),1);
        AddStep(TEXT("B climb"),5,[this]{Hold(TEXT("Jump"),FInputActionValue(true));},[this]{Release(TEXT("Jump"));});
        AddStep(TEXT("B hover"),5,[]{});
        AddStep(TEXT("B flight across town"),6,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Move"));});
        AddStep(TEXT("B boosted flight"),2,[this]{Hold(TEXT("Sprint"),FInputActionValue(true));Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Sprint"));Release(TEXT("Move"));});
        Tap(TEXT("Perspective"),4);Tap(TEXT("Perspective"),2);Tap(TEXT("FlightToggle"),12);
        return;
    }
    if(Mode==TEXT("m5_vs2_motion"))
    {
        AddStep(TEXT("A road placement for normal-speed motion review"),2,[this]
        {PlaceCharacter(FVector(0,-10000,110),90);PC->SetControlRotation(FRotator(-10,90,0));});
        AddStep(TEXT("B walk"),5,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Move"));});
        AddStep(TEXT("B stop"),2,[]{});
        AddStep(TEXT("B sprint"),5,[this]{Hold(TEXT("Sprint"),FInputActionValue(true));Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this]{Release(TEXT("Sprint"));Release(TEXT("Move"));});
        AddStep(TEXT("B emergency stop"),2,[]{});Tap(TEXT("Jump"),2);
        Tap(TEXT("FlightToggle"),1);
        AddStep(TEXT("B ascend"),1.5,[this]{Hold(TEXT("Jump"),FInputActionValue(true));},[this]{Release(TEXT("Jump"));});
        AddStep(TEXT("B hover"),3,[]{});
        for(int32 I=0;I<2;++I)
        {
            AddStep(I?TEXT("B boosted forward flight"):TEXT("B forward flight"),I?2:3,[this,I]
            {if(I)Hold(TEXT("Sprint"),FInputActionValue(true));Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));},[this,I]
            {
                auto* Mesh=Character->GetMesh();
                Check(I?TEXT("Boost flight observed"):TEXT("Forward flight observed"),PC->IsFlying()&&Character->GetVelocity().Size2D()>500,
                    TEXT("actual Action movement; pose remains visual review"),
                    Character->GetFlightComponent()->GetFlightDiagnostics()+TEXT(" hips_world=")+Mesh->GetSocketTransform(TEXT("Hips")).GetRotation().Rotator().ToString(),TEXT("B-Action"));
                Release(TEXT("Sprint"));Release(TEXT("Move"));
            });
            AddStep(TEXT("B glide deceleration"),2,[]{});
        }
        Tap(TEXT("Perspective"),2);Tap(TEXT("Perspective"),1);Tap(TEXT("FlightToggle"),5);
        return;
    }
    if(Mode==TEXT("m5_vs2_perf"))
    {
        const auto Sample=[this](const FString& Label,double Duration)
        {
            AddStep(Label,Duration,[this]{FrameTimes.Reset();},[this,Label]
            {
                TArray<double> Sorted=FrameTimes;Sorted.Sort();double Sum=0;
                for(double V:Sorted)Sum+=V;
                const double Fps=Sum>0?1000.*Sorted.Num()/Sum:0;
                const double P99=Sorted.IsEmpty()?0:Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.99)-1,0,Sorted.Num()-1)];
                Check(Label,Fps>=60&&P99<=25,TEXT("1080p uncapped, mean>=60fps p99<=25ms, no capture"),
                    FString::Printf(TEXT("frames=%d mean_fps=%.3f p99_ms=%.3f wall_seconds=%.3f"),Sorted.Num(),Fps,P99,Sum/1000.),TEXT("A wall-frame measurement + B movement"));
            });
        };
        AddStep(TEXT("Warmup route"),15,[this]{PlaceCharacter(FVector(0,-10000,110),90);PC->SetControlRotation(FRotator(-10,90,0));});
        AddStep(TEXT("B start walk"),.1,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));});
        Sample(TEXT("South approach walk"),12);Sample(TEXT("Market approach walk"),12);
        AddStep(TEXT("B stop before flight"),2,[this]{Release(TEXT("Move"));});Tap(TEXT("FlightToggle"),1);
        AddStep(TEXT("B climb"),7,[this]{Hold(TEXT("Jump"),FInputActionValue(true));},[this]{Release(TEXT("Jump"));});
        const bool VS3=GetWorld()->GetOutermost()->GetName().Contains(TEXT("/M5VS3/"));
        Sample(TEXT("High aerial hover"),VS3?4:12);
        AddStep(TEXT("B forward flight"),.1,[this]{Hold(TEXT("Move"),FInputActionValue(FVector2D(0,1)));});
        Sample(TEXT("Town aerial traversal"),VS3?5:12);
        AddStep(TEXT("B end route"),1,[this]{Release(TEXT("Move"));});
        if(VS3){
            AddStep(TEXT("Wait for actual stamina landing"),16,[]{});
            const auto Crowd=MakeShared<TArray<TWeakObjectPtr<AHCM3NPC>>>();
            AddStep(TEXT("A six-adult spell stress fixture"),1,[this,Crowd]{
                PlaceCharacter(FVector(0,-6800,110),90);PC->SetControlRotation(FRotator(-5,90,0));
                for(TActorIterator<AHCM3NPC> It(GetWorld());It&&Crowd->Num()<6;++It)if(It->StableId.ToString().StartsWith(TEXT("VS2_Citizen"))&&!It->IsDead()){
                    const int32 I=Crowd->Num();const FVector P=FVector((I%3-1)*95,-6540+(I/3)*100,110);
                    FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(VS3SpellCrowd),false,*It);Q.AddIgnoredActor(Character);
                    if(GetWorld()->LineTraceSingleByChannel(H,P+FVector(0,0,40),P-FVector(0,0,160),ECC_Visibility,Q)&&H.ImpactNormal.Z>.8){
                        It->SetActorLocation(H.ImpactPoint+FVector(0,0,It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4),false,nullptr,ETeleportType::TeleportPhysics);Crowd->Add(*It);
                    }
                }
                Check(TEXT("Spell stress has six existing adult NPCs"),Crowd->Num()==6,TEXT("six live authored adults, A placement only"),FString::FromInt(Crowd->Num()),TEXT("A-fixture"));
            });
            AddStep(TEXT("Begin measured multiple-spell AI response"),.05,[this]{FrameTimes.Reset();});
            Tap(TEXT("Spell4"),.8);Tap(TEXT("Spell3"),.8);Tap(TEXT("Spell2"),3.8);Tap(TEXT("Spell1"),.8);
            AddStep(TEXT("Measure ordinary NPC reactions to spells"),8,[]{},[this,Crowd]{
                TArray<double> Sorted=FrameTimes;Sorted.Sort();double Sum=0;for(double T:Sorted)Sum+=T;
                const double FPS=Sum>0?1000.*Sorted.Num()/Sum:0,P99=Sorted.IsEmpty()?0:Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num()*.99)-1,0,Sorted.Num()-1)];
                int32 Damaged=0;for(const auto& N:*Crowd)if(N.IsValid()&&N->GetHealth()<100)++Damaged;
                Check(TEXT("Multiple spells and six NPC AI performance"),FPS>=60&&P99<=25,TEXT("uncapped; mean>=60 p99<=25; ordinary NPC reactions, not forced six attackers"),FString::Printf(TEXT("frames=%d mean_fps=%.3f p99_ms=%.3f wall_seconds=%.3f npcs=%d damaged=%d"),Sorted.Num(),FPS,P99,Sum/1000.,Crowd->Num(),Damaged),TEXT("B-Action spells + A crowd placement and wall-frame readback"));
            });
        }
        return;
    }
    if(Mode==TEXT("m5_vs2_beauty"))
    {
        struct FView {const TCHAR* Name;FVector Eye,Target;};
        const TArray<FView> Views={
            {TEXT("Corner"),{0,-6800,350},{0,-3500,150}},
            {TEXT("Guild"),{-4200,-5700,350},{-4700,-4400,140}},
            {TEXT("GuildInside"),{-4700,-4680,155},{-4700,-4130,140}},
            {TEXT("Cafe"),{6500,-5700,350},{5800,-4400,180}},
            {TEXT("CafeInside"),{5800,-4680,155},{5800,-4130,140}},
            {TEXT("Market"),{0,900,280},{0,3400,150}},
            {TEXT("Bridge"),{1800,3300,650},{3600,5000,150}},
            {TEXT("Windmill"),{-4500,7000,800},{-6400,9400,600}},
            {TEXT("Temple"),{-2000,6700,700},{-4800,8000,350}},
            {TEXT("Aerial"),{12000,-14500,6500},{-1000,1500,0}}};
        for(const TCHAR* Period:{TEXT("Afternoon"),TEXT("Dusk"),TEXT("Night")})
        {
            AddStep(FString(Period)+TEXT(" settle lighting"),5,[this,Period]
            {for(TActorIterator<AHCM5VS2CornerTimeDirector> It(GetWorld());It;++It)It->SetTimeOfDay(Period);});
            for(const auto& V:Views)
            {
                const FString Name=FString(Period)+TEXT("_")+V.Name;
                AddStep(Name+TEXT(" framing fixture"),2.5,[this,V]
                {
                    auto* Camera=GetWorld()->SpawnActor<ACameraActor>(V.Eye,(V.Target-V.Eye).Rotation());
                    Camera->GetCameraComponent()->SetFieldOfView(65);Fixtures.Add(Camera);PC->SetViewTarget(Camera);
                },[this,Name]
                {
                    const FString Filename=RunDirectory/(Name+TEXT(".png"));
                    M3CaptureFiles.AddUnique(Filename);M3LastCaptureFrame=GFrameCounter;
                    FScreenshotRequest::RequestScreenshot(Filename,false,false,false,FIntRect(),true);
                });
                AddM3CaptureCompletion(Name);
            }
        }
        return;
    }
    const bool Soak=Mode==TEXT("m5_vs2_soak");
    if(Soak)
    {
        AddStep(TEXT("First idle gesture"),10,[]{},[this]{Capture(TEXT("IdleGesture1"));});
        AddStep(TEXT("Second idle gesture"),7,[]{},[this]{Capture(TEXT("IdleGesture2"));});
    }
    const int32 Cycles=Soak?30:1;
    for(int32 I=0;I<Cycles;++I)
    {
        Tap(TEXT("Help"),.2);Tap(TEXT("TimeOfDay"),.5);Tap(TEXT("Perspective"),.5);
        Tap(TEXT("FlightToggle"),1);
        AddStep(TEXT("Ascend through actual input"),.6,[this]{Hold(TEXT("Jump"),FInputActionValue(true));},[this]{Release(TEXT("Jump"));});
        AddStep(TEXT("Hover with HUD text"),Soak?48:3,[]{},[this]
        {Check(TEXT("Flight started through Action"),PC->IsFlying(),TEXT("flying"),State(),TEXT("B-Action, not OS keys"));});
        Tap(TEXT("Perspective"),.2);Tap(TEXT("FlightToggle"),5);Tap(TEXT("Help"),.2);
        AddStep(TEXT("Landed after input"),1,[]{},[this]
        {Check(TEXT("Landed through Action"),!PC->IsFlying(),TEXT("grounded"),State(),TEXT("B-Action, not OS keys"));});
    }
    if(Soak) AddStep(TEXT("Complete at least 30 minutes"),1,[this]
    {Steps[StepIndex].Duration=FMath::Max(1.0,1810.0-(FPlatformTime::Seconds()-StartedAt));},[this]
    {Check(TEXT("30 minute ordinary light soak"),FPlatformTime::Seconds()-StartedAt>=1800,TEXT("wall >=1800s; no root-cause claim"),FString::SanitizeFloat(FPlatformTime::Seconds()-StartedAt),TEXT("A-runtime"));});
}
