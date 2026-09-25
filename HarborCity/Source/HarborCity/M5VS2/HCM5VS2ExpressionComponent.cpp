#include "HCM5VS2ExpressionComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "M4/HCM4CombatComponent.h"

namespace
{
    const FName BlinkMorph(TEXT("vrc_blink (3_0)"));
    const FName SpeechMorphs[] = {TEXT("vrc_v_aa"), TEXT("vrc_v_e"), TEXT("vrc_v_oh")};

    bool IsEyePose(FName Name)
    {
        const FString S = Name.ToString();
        // During a blink fade conflicting eyelid expression shapes, not pupil/iris size.
        return S.StartsWith(TEXT("eye_")) && Name != TEXT("eye_small");
    }

    bool IsMouthPose(FName Name)
    {
        return Name.ToString().StartsWith(TEXT("mouth_")) || Name == TEXT("jaw_morph_swell");
    }
}

UHCM5VS2ExpressionComponent::UHCM5VS2ExpressionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bTickEvenWhenPaused = false;
}

void UHCM5VS2ExpressionComponent::BeginPlay()
{
    Super::BeginPlay();
    BlinkRandom.Initialize(BlinkRandomSeed != 0 ? BlinkRandomSeed : int32(GetUniqueID() ^ FPlatformTime::Cycles()));
    BlinkCountdown = NextBlinkInterval();
    Combat = GetOwner() ? GetOwner()->FindComponentByClass<UHCM4CombatComponent>() : nullptr;
    PreviousHealth = Combat ? Combat->GetPlayerHealth() : -1.f;
    ReinitializeFace();
}

void UHCM5VS2ExpressionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreOwnedMorphs();
    Super::EndPlay(Reason);
}

bool UHCM5VS2ExpressionComponent::CanonicalEmotion(FName In, FName& Out)
{
    if (In.IsNone() || In == TEXT("Neutral") || In == TEXT("Calm")) Out = TEXT("Neutral");
    else if (In == TEXT("Happy") || In == TEXT("Smile") || In == TEXT("Relieved") || In == TEXT("开心")) Out = TEXT("Happy");
    else if (In == TEXT("Surprised") || In == TEXT("惊讶")) Out = TEXT("Surprised");
    else if (In == TEXT("Angry") || In == TEXT("生气")) Out = TEXT("Angry");
    else if (In == TEXT("Sad") || In == TEXT("Concerned") || In == TEXT("难过")) Out = TEXT("Sad");
    else if (In == TEXT("Shy") || In == TEXT("害羞")) Out = TEXT("Shy");
    else if (In == TEXT("Serious") || In == TEXT("Determined") || In == TEXT("Wry") || In == TEXT("认真")) Out = TEXT("Serious");
    else return false;
    return true;
}

bool UHCM5VS2ExpressionComponent::SetEmotion(FName Emotion, float Intensity)
{
    FName Canonical;
    if (!CanonicalEmotion(Emotion, Canonical) || !FMath::IsFinite(Intensity)) return false;
    RequestedEmotion = Canonical;
    RequestedIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
    return true;
}

void UHCM5VS2ExpressionComponent::BuildEmotionWeights(FName Emotion, float Intensity, TMap<FName, float>& Out) const
{
    Out.Reset();
    const auto Put = [&Out, Intensity](const TCHAR* Name, float Weight) { Out.Add(FName(Name), Weight * Intensity); };
    // Selected numeric morph weights from licensed original v1.02 facial presets / 100.
    // Angry/Sad mouths and Serious/Pain are project-derived. HeroFace ca8538db
    // verified the old mouth weights and nonzero native deltas, but the captured
    // Angry/Sad/Serious lips still read as smiling. These alternatives require
    // the same-camera visual check; no private .anim payload is embedded.
    if (Emotion == TEXT("Happy"))
    {
        Put(TEXT("eye_joy"), 1); Put(TEXT("mouth_ω"), .5f);
        Put(TEXT("option_cheek 1"), 1); Put(TEXT("brow_joy"), .7f);
    }
    else if (Emotion == TEXT("Surprised"))
    {
        Put(TEXT("eye_open"), 1); Put(TEXT("eye_under_up"), .25f);
        Put(TEXT("mouth_□"), .3f); Put(TEXT("mouth_~□"), 1);
        Put(TEXT("option_sweat 2"), 1); Put(TEXT("eye_small"), .8f); Put(TEXT("brow_surprised"), 1);
    }
    else if (Emotion == TEXT("Angry"))
    {
        Put(TEXT("eye_jito"), .4f); Put(TEXT("eye_angry"), .5f);
        Put(TEXT("mouth_sad"), 1); Put(TEXT("brow_anger"), 1);
    }
    else if (Emotion == TEXT("Serious"))
    {
        Put(TEXT("eye_jito"), .4f * .35f); Put(TEXT("eye_angry"), .5f * .35f);
        Put(TEXT("mouth_straight"), 1); Put(TEXT("brow_anger"), .35f);
    }
    else if (Emotion == TEXT("Sad"))
    {
        Put(TEXT("eye_doya"), .65f); Put(TEXT("eye_under_up 2"), 1); Put(TEXT("eye_angry"), .685f);
        Put(TEXT("eye_sad"), .923f); Put(TEXT("mouth_sad"), 1); Put(TEXT("mouth_narrow"), .15f);
        Put(TEXT("mouth_H"), 0); Put(TEXT("option_cheek 3"), 1); // Retain ownership/reset of the former mouth layer.
        Put(TEXT("option_tear_top 1"), 1); Put(TEXT("option_tear_top 2"), 1); Put(TEXT("option_tear_under"), 1);
        Put(TEXT("jaw_morph_swell"), .7f); Put(TEXT("brow_anger"), 1); Put(TEXT("eye_mukamuka"), 1);
    }
    else if (Emotion == TEXT("Shy"))
    {
        Put(TEXT("eye_close"), .2f); Put(TEXT("eye_jito"), .25f); Put(TEXT("eye_under_up 2"), .5f);
        Put(TEXT("mouth_∧"), .65f); Put(TEXT("option_cheek 3"), 1);
        Put(TEXT("brow_trouble"), .8f); Put(TEXT("brow_tare"), .3f);
    }
    else if (Emotion == TEXT("Pain"))
    {
        Put(TEXT("eye_close"), .72f); Put(TEXT("brow_trouble"), .8f); Put(TEXT("mouth_△"), .35f);
    }
}

void UHCM5VS2ExpressionComponent::RestoreOwnedMorphs()
{
    if (IsValid(ActiveMesh))
    {
        ActiveMesh->RemoveTickPrerequisiteComponent(this);
        if (ActiveMesh->GetSkeletalMeshAsset() == ActiveMeshAsset)
            for (const TPair<FName, float>& Pair : OriginalWeights)
                ActiveMesh->SetMorphTarget(Pair.Key, Pair.Value);
    }
    ActiveMesh = nullptr; ActiveMeshAsset = nullptr;
    OriginalWeights.Reset(); CurrentWeights.Reset(); ManagedMorphs.Reset(); MissingMorphs.Reset();
    bFaceReady = false;
}

bool UHCM5VS2ExpressionComponent::SetTargetMesh(USkeletalMeshComponent* Mesh)
{
    TargetMesh = Mesh;
    return ReinitializeFace();
}

bool UHCM5VS2ExpressionComponent::ReinitializeFace()
{
    RestoreOwnedMorphs();
    if (!bExpressionsEnabled) return false;
    USkeletalMeshComponent* SelectedMesh = TargetMesh;
    if (!SelectedMesh)
        if (ACharacter* Character = Cast<ACharacter>(GetOwner())) SelectedMesh = Character->GetMesh();
    if (!IsValid(SelectedMesh) || !SelectedMesh->GetSkeletalMeshAsset()) return false;
    TSet<FName> Names;
    for (const FName Emotion : {FName(TEXT("Happy")), FName(TEXT("Surprised")), FName(TEXT("Angry")),
        FName(TEXT("Sad")), FName(TEXT("Shy")), FName(TEXT("Serious")), FName(TEXT("Pain"))})
    {
        TMap<FName, float> Weights;
        BuildEmotionWeights(Emotion, 1.f, Weights);
        for (const TPair<FName, float>& Pair : Weights) Names.Add(Pair.Key);
    }
    Names.Add(BlinkMorph);
    for (const FName Morph : SpeechMorphs) Names.Add(Morph);
    // A different NPC mesh needs its own verified profile, not partial silently broken morph calls.
    for (const FName Name : Names)
        if (!SelectedMesh->GetSkeletalMeshAsset()->FindMorphTarget(Name)) MissingMorphs.Add(Name);
    MissingMorphs.Sort(FNameLexicalLess());
    if (!MissingMorphs.IsEmpty()) return false;
    ActiveMesh = SelectedMesh; ActiveMeshAsset = SelectedMesh->GetSkeletalMeshAsset();
    ManagedMorphs = Names.Array(); ManagedMorphs.Sort(FNameLexicalLess());
    for (const FName Name : ManagedMorphs)
    {
        const float Weight = ActiveMesh->GetMorphTarget(Name);
        OriginalWeights.Add(Name, Weight); CurrentWeights.Add(Name, Weight);
    }
    // Expression precedes animation evaluation. Do not depend on Combat's PostPhysics tick:
    // Combat already depends on the mesh, and that additional edge would make a cycle.
    ActiveMesh->AddTickPrerequisiteComponent(this);
    bFaceReady = true;
    return true;
}

void UHCM5VS2ExpressionComponent::SetExpressionEnabled(bool bEnabled)
{
    if (bExpressionsEnabled == bEnabled) return;
    bExpressionsEnabled = bEnabled;
    if (bEnabled) ReinitializeFace();
    else
    {
        RestoreOwnedMorphs(); StopSpeaking();
        BlinkWeight = 0; BlinkElapsed = -1; BlinkCountdown = NextBlinkInterval();
        HitRemaining = 0;
    }
}

void UHCM5VS2ExpressionComponent::SetCombatExpression(bool bInCombat) { bExplicitCombat = bInCombat; }

void UHCM5VS2ExpressionComponent::NotifyHit(float Intensity)
{
    if (!FMath::IsFinite(Intensity) || Intensity <= 0) return;
    HitRemaining = .36f; HitIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
    ++ObservedHitCount;
}

void UHCM5VS2ExpressionComponent::SetDialogueTextProgress(const FString& FullText, int32 VisibleCharacters, bool bSpeaking)
{
    if (!bSpeaking) { StopSpeaking(); return; }
    if (FullText != SpokenText)
    {
        SpokenText = FullText; PreviousVisibleCharacters = 0; TalkRemaining = 0;
    }
    const int32 Count = FMath::Clamp(VisibleCharacters, 0, FullText.Len());
    const int32 NewlyVisible = Count - PreviousVisibleCharacters;
    if (NewlyVisible > 0 && NewlyVisible <= 4)
    {
        const TCHAR Last = FullText[Count - 1];
        const bool bPunctuation = FChar::IsWhitespace(Last) || FString(TEXT(".,!?;:，。！？；：、…—\"“”‘’（）()[]【】" )).Contains(FString::Chr(Last));
        if (bPunctuation) TalkRemaining = 0;
        else
        {
            TalkingMorph = SpeechMorphs[uint32(Last) % UE_ARRAY_COUNT(SpeechMorphs)];
            TalkRemaining = .105f;
        }
    }
    else if (NewlyVisible != 0) TalkRemaining = 0; // Full-line skip/reset is not sustained speech.
    PreviousVisibleCharacters = Count;
}

void UHCM5VS2ExpressionComponent::StopSpeaking()
{
    SpokenText.Reset(); PreviousVisibleCharacters = 0; TalkRemaining = 0;
}

float UHCM5VS2ExpressionComponent::NextBlinkInterval()
{
    const float Min = FMath::Max(.5f, BlinkIntervalMin);
    return BlinkRandom.FRandRange(Min, FMath::Max(Min, BlinkIntervalMax));
}

void UHCM5VS2ExpressionComponent::TickBlink(float DeltaTime)
{
    BlinkWeight = 0;
    if (!bAutomaticBlink) { BlinkElapsed = -1; return; }
    if (BlinkElapsed < 0)
    {
        BlinkCountdown -= DeltaTime;
        if (BlinkCountdown > 0) return;
        BlinkElapsed = 0;
    }
    BlinkElapsed += DeltaTime;
    constexpr float Closing = .04f, Opening = .065f;
    const float Closed = FMath::Clamp(BlinkClosedSeconds, .1f, .15f);
    if (BlinkElapsed < Closing) BlinkWeight = FMath::SmoothStep(0.f, Closing, BlinkElapsed);
    else if (BlinkElapsed < Closing + Closed) BlinkWeight = 1;
    else if (BlinkElapsed < Closing + Closed + Opening)
        BlinkWeight = 1.f - FMath::SmoothStep(Closing + Closed, Closing + Closed + Opening, BlinkElapsed);
    else { BlinkElapsed = -1; BlinkCountdown = NextBlinkInterval(); }
}

void UHCM5VS2ExpressionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    if (!bExpressionsEnabled || !bFaceReady || !IsValid(ActiveMesh) || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0) return;
    if (ActiveMesh->GetSkeletalMeshAsset() != ActiveMeshAsset)
    {
        ReinitializeFace();
        return;
    }
    bool bCombatNow = bExplicitCombat;
    if (bReadOwnerCombatState && IsValid(Combat))
    {
        const float Health = Combat->GetPlayerHealth();
        // Super::TakeDamage broadcasts before ReceiveNPCPunch can reject it; actual health is authoritative.
        if (PreviousHealth >= 0 && Health < PreviousHealth - KINDA_SMALL_NUMBER) NotifyHit(1.f);
        PreviousHealth = Health;
        bCombatNow |= Combat->IsAttacking() || Combat->IsAimHeld() || Combat->IsReloading();
    }
    TickBlink(DeltaTime);
    const bool bPain = HitRemaining > 0;
    EffectiveEmotion = bPain ? FName(TEXT("Pain")) : bCombatNow ? FName(TEXT("Serious")) : RequestedEmotion;
    const float Intensity = bPain ? HitIntensity : bCombatNow ? 1.f : RequestedIntensity;
    TMap<FName, float> Desired;
    BuildEmotionWeights(EffectiveEmotion, Intensity, Desired);
    const float Alpha = 1.f - FMath::Exp(-DeltaTime / FMath::Max(.01f, ExpressionBlendSeconds));
    const float TalkTarget = TalkRemaining > 0 && !bPain ? .32f : 0;
    TalkingWeight = FMath::FInterpTo(TalkingWeight, TalkTarget, DeltaTime, 24.f);
    for (const FName Name : ManagedMorphs)
    {
        if (Name == BlinkMorph) continue;
        float Value = FMath::Lerp(CurrentWeights.FindRef(Name), Desired.FindRef(Name), Alpha);
        CurrentWeights.Add(Name, Value);
        if (IsEyePose(Name)) Value *= 1.f - BlinkWeight;
        if (IsMouthPose(Name)) Value *= 1.f - TalkingWeight / .32f;
        for (const FName Speech : SpeechMorphs)
            if (Name == Speech) Value = Name == TalkingMorph ? TalkingWeight : 0;
        ActiveMesh->SetMorphTarget(Name, FMath::Clamp(Value, 0.f, 1.f));
    }
    ActiveMesh->SetMorphTarget(BlinkMorph, BlinkWeight);
    HitRemaining = FMath::Max(0.f, HitRemaining - DeltaTime);
    TalkRemaining = FMath::Max(0.f, TalkRemaining - DeltaTime);
}

void UHCM5VS2ExpressionComponent::SetLookTarget(AActor* Actor, FName SocketName)
{
    LookActor = Actor; LookSocket = SocketName;
}

void UHCM5VS2ExpressionComponent::ClearLookTarget() { LookActor.Reset(); LookSocket = NAME_None; }

bool UHCM5VS2ExpressionComponent::GetLookTargetLocation(FVector& WorldLocation) const
{
    const AActor* Actor = LookActor.Get();
    if (!Actor) { WorldLocation = FVector::ZeroVector; return false; }
    if (const USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
    {
        const FName Socket = LookSocket.IsNone() ? FName(TEXT("Head")) : LookSocket;
        if (Mesh->DoesSocketExist(Socket)) { WorldLocation = Mesh->GetSocketLocation(Socket); return true; }
    }
    FRotator EyeRotation;
    Actor->GetActorEyesViewPoint(WorldLocation, EyeRotation);
    return true;
}
