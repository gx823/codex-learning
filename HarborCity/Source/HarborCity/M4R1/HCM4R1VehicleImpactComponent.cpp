#include "HCM4R1VehicleImpactComponent.h"
#include "HCM4R1PhysicalReactionComponent.h"
#include "M1/HCM1Vehicle.h"
#include "M3/HCM3NPC.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UHCM4R1VehicleImpactComponent::UHCM4R1VehicleImpactComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UHCM4R1VehicleImpactComponent::BeginPlay()
{
    Super::BeginPlay(); Car = Cast<AHCM1Vehicle>(GetOwner());
    if (!Car.IsValid()) { SetComponentTickEnabled(false); return; }
    Car->GetMesh()->OnComponentHit.AddDynamic(this, &UHCM4R1VehicleImpactComponent::OnVehicleHit);
    Car->GetMesh()->SetNotifyRigidBodyCollision(true);
    if (FPhysScene_Chaos* Scene = GetWorld()->GetPhysicsScene())
        PhysicsStepHandle = Scene->OnPhysSceneStep.AddUObject(this, &UHCM4R1VehicleImpactComponent::ObservePhysicsStep);
    ResetMotionHistory();
}

void UHCM4R1VehicleImpactComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Car.IsValid()) Car->GetMesh()->OnComponentHit.RemoveDynamic(this, &UHCM4R1VehicleImpactComponent::OnVehicleHit);
    if (GetWorld()) if (FPhysScene_Chaos* Scene = GetWorld()->GetPhysicsScene()) Scene->OnPhysSceneStep.Remove(PhysicsStepHandle);
    Contacts.Empty(); PendingPhysicsHits.Empty();
    Super::EndPlay(EndPlayReason);
}

void UHCM4R1VehicleImpactComponent::ObservePhysicsStep(FPhysScene_Chaos* Scene, float PhysicsDeltaSeconds)
{
    // UE broadcasts UseDeltaTime after the scene's max-physics-delta clamp, on the game thread.
    // A costly capture can advance world time .4s while Chaos advances only .033s.
    if (FMath::IsFinite(PhysicsDeltaSeconds) && PhysicsDeltaSeconds > 0)
    { ObservedPhysicsSeconds += PhysicsDeltaSeconds; ++ObservedPhysicsSteps; }
}

void UHCM4R1VehicleImpactComponent::ResetMotionHistory()
{
    if (!Car.IsValid()) Car = Cast<AHCM1Vehicle>(GetOwner());
    bHistoryValid = false; Contacts.Empty(); PendingPhysicsHits.Empty();
    if (!Car.IsValid()) return;
    PreviousTransform = Car->GetMesh()->GetComponentTransform();
    PreviousVelocity = Car->GetMesh()->GetPhysicsLinearVelocity();
    PreviousAngularVelocity = Car->GetMesh()->GetPhysicsAngularVelocityInRadians();
}

void UHCM4R1VehicleImpactComponent::OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
#if !UE_BUILD_SHIPPING
    if (Car.IsValid() && FParse::Param(FCommandLine::Get(),TEXT("M5VS2ReverseTrace")) &&
        NormalImpulse.Size() > 10000.f && FMath::Abs(Hit.ImpactNormal.Z) < .7f)
        UE_LOG(LogTemp,Display,TEXT("VS2ReverseImpact t=%.3f actor=%s component=%s impulse=%.1f position=%s"),
            GetWorld()->GetTimeSeconds(),*GetNameSafe(OtherActor),*GetNameSafe(OtherComp),NormalImpulse.Size(),*Hit.ImpactPoint.ToString());
#endif
    AHCM3NPC* NPC = Cast<AHCM3NPC>(OtherActor);
    if (!NPC || !Car.IsValid() || PendingPhysicsHits.Num() >= 64) return;
    ++PhysicalHitCandidates;
    FCandidate Candidate; Candidate.NPC = NPC; Candidate.Hit = Hit; Candidate.Source = TEXT("PhysicalHit");
    Candidate.CarLocation = Car->GetMesh()->GetComponentLocation();
    PendingPhysicsHits.Add(Candidate);
    // NormalImpulse is deliberately not a gate: sweeps and query overlaps have zero solver impulse.
}

bool UHCM4R1VehicleImpactComponent::IsOccluded(const FCandidate& Candidate) const
{
    AHCM3NPC* NPC = Candidate.NPC.Get();
    if (!NPC || !Car.IsValid()) return true;
    const FVector Target = NPC->GetPhysicalReaction() && NPC->GetPhysicalReaction()->IsLivingRagdollActive() ?
        NPC->GetPhysicalReaction()->GetPhysicalLocation() : NPC->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(M4R1ImpactOcclusion), false, Car.Get());
    Query.AddIgnoredActor(NPC);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FHitResult Obstruction;
    return GetWorld()->LineTraceSingleByObjectType(Obstruction, Candidate.CarLocation, Target, Objects, Query);
}

void UHCM4R1VehicleImpactComponent::Consider(const FCandidate& Candidate, const FVector& CurrentVelocity,
    const FVector& CurrentAngular, double Now)
{
    AHCM3NPC* NPC = Candidate.NPC.Get();
    if (!NPC || !Car.IsValid() || NPC->IsDead() || !NPC->IsNPCEnabled() || NPC->GetIsPassenger() || NPC->IsHidden()) return;
    UHCM4R1PhysicalReactionComponent* Reaction = NPC->GetPhysicalReaction();
    if (!Reaction) return;
    if (IsOccluded(Candidate)) { ++OcclusionRejected; return; }
    FContactEpisode* Existing = Contacts.Find(NPC);
    if (!Existing)
    {
        FContactEpisode Episode; Episode.Serial = ++ContactSerial;
        Contacts.Add(NPC, Episode); ++AcceptedContacts;
        auto Transition=MakeShared<FJsonObject>();Transition->SetStringField(TEXT("kind"),TEXT("begin_actual_shape_contact"));
        Transition->SetStringField(TEXT("npc_id"),NPC->StableId.ToString());Transition->SetNumberField(TEXT("serial"),Episode.Serial);
        Transition->SetNumberField(TEXT("world_seconds"),Now);Transition->SetNumberField(TEXT("physics_seconds"),ObservedPhysicsSeconds);
        Transition->SetStringField(TEXT("component"),GetNameSafe(Candidate.Hit.GetComponent()));
        if(RecentContactTransitions.Num()>=64)RecentContactTransitions.RemoveAt(0);
        RecentContactTransitions.Add(MakeShared<FJsonValueObject>(Transition));
    }
    FContactEpisode& Episode = Contacts.FindChecked(NPC); Episode.Touch(ObservedPhysicsSeconds);
    if (Episode.bApplied) { ++DuplicateCandidates; return; }
    const FVector NPCPoint = Reaction->IsLivingRagdollActive() ? Reaction->GetPhysicalLocation() : NPC->GetActorLocation();
    FVector Outward = -Candidate.Hit.ImpactNormal.GetSafeNormal2D();
    if (Outward.IsNearlyZero() || Candidate.Hit.bStartPenetrating)
        Outward = (NPCPoint - Candidate.CarLocation).GetSafeNormal2D();
    if (Outward.IsNearlyZero()) return;
    const FVector ImpactPoint = Candidate.Hit.ImpactPoint.IsNearlyZero() ? NPCPoint : Candidate.Hit.ImpactPoint;
    const FVector CurrentPointVelocity = CurrentVelocity + FVector::CrossProduct(CurrentAngular,
        ImpactPoint - Car->GetMesh()->GetComponentLocation());
    const FVector PreviousPointVelocity = PreviousVelocity + FVector::CrossProduct(PreviousAngularVelocity,
        ImpactPoint - PreviousTransform.GetLocation());
    const FVector NPCVelocity = Reaction->IsLivingRagdollActive() ? NPC->GetMesh()->GetPhysicsLinearVelocity(NPC->GetPhysicalRootBone()) : NPC->GetVelocity();
    // The larger closing measurement preserves the velocity immediately before a solver stop.
    // It does not manufacture speed from HUD, displacement teleports, or impulse magnitude.
    const FVector CurrentRelative = CurrentPointVelocity - NPCVelocity;
    const FVector PreviousRelative = PreviousPointVelocity - NPCVelocity;
    const bool bUsePrevious = FVector::DotProduct(PreviousRelative, Outward) > FVector::DotProduct(CurrentRelative, Outward);
    const FVector Relative = bUsePrevious ? PreviousRelative : CurrentRelative;
    const float Closing = FMath::Max(0.f, float(FVector::DotProduct(Relative, Outward))); LastClosing = Closing;
    if (Closing < FMath::Max(100.f, Reaction->DamageThresholdCmS))
    { if (!Episode.bLowRecorded) { ++LowClosingContacts; Episode.bLowRecorded = true; } return; }
    const float Applied = Reaction->ReceiveVehicleImpact(Car.Get(), Candidate.Hit, Relative, Outward, Closing,
        Episode.Serial, Candidate.Source);
    if (Applied <= 0) return;
    Episode.bApplied = true; ++DamageContacts;
    TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
    Event->SetNumberField(TEXT("world_seconds"), Now); Event->SetStringField(TEXT("npc_id"), NPC->StableId.ToString());
    Event->SetNumberField(TEXT("observed_physics_seconds"), ObservedPhysicsSeconds);
    Event->SetNumberField(TEXT("contact_serial"), Episode.Serial); Event->SetStringField(TEXT("source"), Candidate.Source.ToString());
    Event->SetNumberField(TEXT("closing_cm_s"), Closing); Event->SetStringField(TEXT("relative_cm_s"), Relative.ToCompactString());
    Event->SetBoolField(TEXT("used_previous_frame_velocity"), bUsePrevious);
    Event->SetStringField(TEXT("impact_point_cm"), ImpactPoint.ToCompactString());
    Event->SetStringField(TEXT("outward_normal"), Outward.ToCompactString());
    Event->SetNumberField(TEXT("damage"), Applied); Event->SetBoolField(TEXT("dead"), NPC->IsDead());
    Event->SetBoolField(TEXT("living_ragdoll"), Reaction->IsLivingRagdollActive());
    Event->SetStringField(TEXT("hit_component"), GetNameSafe(Candidate.Hit.GetComponent()));
    Event->SetStringField(TEXT("hit_bone"), Candidate.Hit.BoneName.ToString());
    Event->SetNumberField(TEXT("npc_capsule_vehicle_response"), int32(NPC->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Vehicle)));
    Event->SetNumberField(TEXT("car_pawn_response"), int32(Car->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn)));
    if (RecentEvents.Num() >= 64) RecentEvents.RemoveAt(0);
    RecentEvents.Add(MakeShared<FJsonValueObject>(Event));
}

void UHCM4R1VehicleImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!Car.IsValid() || DeltaTime <= 0 || GetWorld()->IsPaused()) return;
    USkeletalMeshComponent* Mesh = Car->GetMesh();
    const FTransform Current = Mesh->GetComponentTransform();
    const FVector Velocity = Mesh->GetPhysicsLinearVelocity(), Angular = Mesh->GetPhysicsAngularVelocityInRadians();
    if (!bHistoryValid)
    {
        PreviousTransform = Current; PreviousVelocity = Velocity; PreviousAngularVelocity = Angular;
        bHistoryValid = true; PendingPhysicsHits.Empty(); return;
    }
    const float Distance = FVector::Distance(PreviousTransform.GetLocation(), Current.GetLocation());
    const float RotationDegrees = FMath::RadiansToDegrees(PreviousTransform.GetRotation().AngularDistance(Current.GetRotation()));
    // Validated reset has an explicit hook. This also rejects an unknown editor/save teleport.
    const float PlausibleDistance = FMath::Max(200.f, float(FMath::Max(PreviousVelocity.Size(), Velocity.Size()) * DeltaTime * 1.8 + 50));
    if (Current.ContainsNaN() || Distance > PlausibleDistance || RotationDegrees > 100.f || DeltaTime > .5f)
    {
        ++InvalidMotionFrames;
        if (DeltaTime > .5f && !Current.ContainsNaN() && Distance <= PlausibleDistance && RotationDegrees <= 100.f)
        {
            // A hitch skips uncertain swept travel but does not rearm an ongoing contact.
            for (auto& Pair : Contacts) Pair.Value.SkipUnobservedInterval();
        }
        else Contacts.Empty();
        PendingPhysicsHits.Empty();
        PreviousTransform = Current; PreviousVelocity = Velocity; PreviousAngularVelocity = Angular; return;
    }
    ++SweepFrames; LastSweepStart = PreviousTransform.GetLocation(); LastSweepEnd = Current.GetLocation();
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto& Pair : Contacts) Pair.Value.BeginQuery();
    for (const FCandidate& Candidate : PendingPhysicsHits) Consider(Candidate, Velocity, Angular, Now);
    PendingPhysicsHits.Empty();
    const int32 Steps = FMath::Clamp(FMath::Max(FMath::CeilToInt(Distance / 100.f), FMath::CeilToInt(RotationDegrees / 5.f)), 1, 24);
    FComponentQueryParams Query(TEXT("M4R1ActualVehicleShapes"), Car.Get());
    Query.bTraceComplex = false; Query.bFindInitialOverlaps = true;
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        const float A = float(Step) / Steps, B = float(Step + 1) / Steps;
        const FVector Start = FMath::Lerp(PreviousTransform.GetLocation(), Current.GetLocation(), A);
        const FVector End = FMath::Lerp(PreviousTransform.GetLocation(), Current.GetLocation(), B);
        const FQuat Rotation = FQuat::Slerp(PreviousTransform.GetRotation(), Current.GetRotation(), (A + B) * .5f);
        TArray<FHitResult> Hits;
        GetWorld()->ComponentSweepMultiByChannel(Hits, Mesh, Start, End, Rotation, ECC_Vehicle, Query); ++SweepQueries;
        for (const FHitResult& Hit : Hits)
        {
            AHCM3NPC* NPC = Cast<AHCM3NPC>(Hit.GetActor());
            if (!NPC) continue;
            ++SweepCandidates;
            FCandidate Candidate; Candidate.NPC = NPC; Candidate.Hit = Hit; Candidate.Source = TEXT("PhysicsAssetSweep");
            Candidate.CarLocation = FMath::Lerp(Start, End, FMath::Clamp(Hit.Time, 0.f, 1.f));
            Candidate.Time = FMath::Lerp(A, B, Hit.Time);
            Consider(Candidate, Velocity, Angular, Now);
        }
    }
    // Only completed geometry queries with no touch establish separation. A 250ms
    // frame that still touches the NPC is not a new contact; Hit and every shape share this episode.
    for (auto It = Contacts.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || It.Value().EndQuery(ObservedPhysicsSeconds))
        {
            auto Transition=MakeShared<FJsonObject>();Transition->SetStringField(TEXT("kind"),TEXT("end_observed_shape_separation"));
            Transition->SetStringField(TEXT("npc_id"),It.Key().IsValid()?It.Key()->StableId.ToString():TEXT("invalid"));
            Transition->SetNumberField(TEXT("serial"),It.Value().Serial);Transition->SetNumberField(TEXT("world_seconds"),Now);
            Transition->SetNumberField(TEXT("physics_seconds"),ObservedPhysicsSeconds);
            Transition->SetNumberField(TEXT("last_touch_physics_seconds"),It.Value().LastTouchPhysicsSeconds);
            Transition->SetNumberField(TEXT("absent_since_physics_seconds"),It.Value().AbsentSince);
            if(RecentContactTransitions.Num()>=64)RecentContactTransitions.RemoveAt(0);
            RecentContactTransitions.Add(MakeShared<FJsonValueObject>(Transition));It.RemoveCurrent();
        }
    PreviousTransform = Current; PreviousVelocity = Velocity; PreviousAngularVelocity = Angular;
}

FString UHCM4R1VehicleImpactComponent::GetDiagnostics() const
{
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("detector"), TEXT("UE_ComponentSweepMultiByChannel_actual_Chaos_convex_shapes"));
    Data->SetStringField(TEXT("contact_rearm"), TEXT("completed_geometry_queries_absent_0.2_observed_physics_seconds; UE_scene_UseDeltaTime_after_clamp; touched_long_frame_preserves_episode; unobserved_hitch_resets_absence"));
    Data->SetNumberField(TEXT("observed_physics_seconds"),ObservedPhysicsSeconds);
    Data->SetNumberField(TEXT("observed_physics_steps"),double(ObservedPhysicsSteps));
    Data->SetBoolField(TEXT("actual_physics_step_clock_bound"),PhysicsStepHandle.IsValid());
    Data->SetStringField(TEXT("physics_asset"), Car.IsValid() ? GetPathNameSafe(Car->GetMesh()->GetPhysicsAsset()) : FString());
    Data->SetNumberField(TEXT("sweep_frames"), SweepFrames); Data->SetNumberField(TEXT("shape_sweep_queries"), SweepQueries);
    Data->SetNumberField(TEXT("physical_hit_candidates"), PhysicalHitCandidates); Data->SetNumberField(TEXT("sweep_candidates"), SweepCandidates);
    Data->SetNumberField(TEXT("contact_episodes"), AcceptedContacts); Data->SetNumberField(TEXT("damage_contacts"), DamageContacts);
    Data->SetNumberField(TEXT("duplicate_candidates_rejected"), DuplicateCandidates); Data->SetNumberField(TEXT("occlusion_rejected"), OcclusionRejected);
    Data->SetNumberField(TEXT("low_closing_contacts"), LowClosingContacts); Data->SetNumberField(TEXT("invalid_motion_frames"), InvalidMotionFrames);
    Data->SetNumberField(TEXT("active_contact_episodes"), Contacts.Num()); Data->SetNumberField(TEXT("last_closing_cm_s"), LastClosing);
    Data->SetStringField(TEXT("last_sweep_start_cm"), LastSweepStart.ToCompactString());
    Data->SetStringField(TEXT("last_sweep_end_cm"), LastSweepEnd.ToCompactString());
    Data->SetArrayField(TEXT("recent_damage_contacts_bounded_64"), RecentEvents);
    Data->SetArrayField(TEXT("recent_contact_transitions_bounded_64"), RecentContactTransitions);
    FString Output; const auto Writer = TJsonWriterFactory<>::Create(&Output); FJsonSerializer::Serialize(Data, Writer); return Output;
}

bool UHCM4R1VehicleImpactComponent::CheckContactEpisodeLogic(FString& OutDetails)
{
    auto Data = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Rows; bool Passed = true;
    const auto Record = [&Rows, &Passed](const TCHAR* Name, bool Actual)
    { auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("case"),Name);Row->SetBoolField(TEXT("pass"),Actual);Rows.Add(MakeShared<FJsonValueObject>(Row));Passed&=Actual; };
    FContactEpisode Continuous; Continuous.Serial=1; Continuous.bApplied=true;
    for (double T : {0., .25, .5, .9})
    { Continuous.BeginQuery();Continuous.Touch();Record(TEXT("touch_after_200_to_500ms_keeps_applied_serial1"),!Continuous.EndQuery(T)&&Continuous.Serial==1&&Continuous.bApplied); }
    Continuous.BeginQuery();Continuous.Touch();Continuous.Touch();Continuous.Touch();
    Record(TEXT("same_frame_Hit_plus_multiple_shapes_same_episode"),!Continuous.EndQuery(1.)&&Continuous.bApplied&&Continuous.Serial==1);
    FContactEpisode Separated;Separated.Serial=1;Separated.bApplied=true;
    Separated.BeginQuery();Record(TEXT("first_absent_query_starts_interval"),!Separated.EndQuery(.1));
    Separated.BeginQuery();Record(TEXT("150ms_proven_absence_does_not_rearm"),!Separated.EndQuery(.25));
    Separated.BeginQuery();Record(TEXT("210ms_proven_absence_rearms"),Separated.EndQuery(.31));
    FContactEpisode Retouched;Retouched.bApplied=true;
    Retouched.BeginQuery();Retouched.EndQuery(.1);Retouched.BeginQuery();Retouched.Touch();
    Record(TEXT("retouch_cancels_pending_separation"),!Retouched.EndQuery(.25)&&Retouched.AbsentSince<0&&Retouched.bApplied);
    Retouched.BeginQuery();Retouched.EndQuery(.3);Retouched.SkipUnobservedInterval();Retouched.BeginQuery();
    Record(TEXT("unqueried_hitch_cannot_count_as_separation"),!Retouched.EndQuery(1.3)&&Retouched.AbsentSince==1.3);
    FContactEpisode SlowPhysics;SlowPhysics.bApplied=true;
    SlowPhysics.BeginQuery();SlowPhysics.EndQuery(.016);
    SlowPhysics.BeginQuery();Record(TEXT("world400ms_but_physics33ms_absence_not_expired"),!SlowPhysics.EndQuery(.049));
    SlowPhysics.BeginQuery();SlowPhysics.Touch();Record(TEXT("capsule_to_mesh_short_physics_gap_same_contact"),!SlowPhysics.EndQuery(.082)&&SlowPhysics.bApplied);
    Data->SetStringField(TEXT("evidence_level"),TEXT("A_pure_production_episode_state_logic; no real OS or 250ms Chaos hitch simulation"));
    Data->SetArrayField(TEXT("checks"),Rows);Data->SetBoolField(TEXT("pass"),Passed);
    const auto Writer=TJsonWriterFactory<>::Create(&OutDetails);FJsonSerializer::Serialize(Data,Writer);return Passed;
}
