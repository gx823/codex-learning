#include "HCM5VS2NPCFaceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "M3/HCM3NPC.h"

UHCM5VS2NPCFaceComponent::UHCM5VS2NPCFaceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bTickEvenWhenPaused = false;
}

void UHCM5VS2NPCFaceComponent::BeginPlay()
{
    Super::BeginPlay();
    Random.Initialize(int32(GetUniqueID() ^ FPlatformTime::Cycles()));
    BlinkCountdown = Random.FRandRange(2.8f, 5.6f);
    ReinitializeFace();
}

void UHCM5VS2NPCFaceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Restore();
    Super::EndPlay(Reason);
}

void UHCM5VS2NPCFaceComponent::Restore()
{
    if (IsValid(ActiveMesh))
    {
        ActiveMesh->RemoveTickPrerequisiteComponent(this);
        if (ActiveMesh->GetSkeletalMeshAsset() == ActiveAsset)
            for (const auto& Pair : Original) ActiveMesh->SetMorphTarget(Pair.Key, Pair.Value);
    }
    Original.Reset(); Current.Reset(); MissingMorphs.Reset();
    ActiveMesh = nullptr; ActiveAsset = nullptr; bReady = false;
}

bool UHCM5VS2NPCFaceComponent::ReinitializeFace()
{
    Restore();
    USkeletalMeshComponent* Mesh = TargetMesh;
    if (!Mesh) if (const ACharacter* Character = Cast<ACharacter>(GetOwner())) Mesh = Character->GetMesh();
    if (!Profile || !Mesh || !Profile->Mesh || Mesh->GetSkeletalMeshAsset() != Profile->Mesh) return false;
    TSet<FName> Groups, Morphs;
    for (const auto& Pose : Profile->FaceGroups)
    {
        if (Pose.Group.IsNone() || Pose.Binds.IsEmpty() || Groups.Contains(Pose.Group)) return false;
        Groups.Add(Pose.Group);
        for (const auto& Bind : Pose.Binds)
        {
            if (!FMath::IsFinite(Bind.Weight) || Bind.Weight < 0 || Bind.Weight > 1 || Bind.Morph.IsNone()) return false;
            Morphs.Add(Bind.Morph);
            if (!Profile->Mesh->FindMorphTarget(Bind.Morph)) MissingMorphs.AddUnique(Bind.Morph);
        }
    }
    for (FName Required : {FName(TEXT("Blink")), FName(TEXT("A")), FName(TEXT("I")), FName(TEXT("U")),
        FName(TEXT("E")), FName(TEXT("O")), FName(TEXT("Joy")), FName(TEXT("Angry")), FName(TEXT("Sorrow")),
        FName(TEXT("Fun")), FName(TEXT("Surprised"))}) if (!Groups.Contains(Required)) return false;
    if (!MissingMorphs.IsEmpty()) return false;
    ActiveMesh = Mesh; ActiveAsset = Mesh->GetSkeletalMeshAsset();
    for (const FName Morph : Morphs)
    { const float Value = Mesh->GetMorphTarget(Morph); Original.Add(Morph, Value); Current.Add(Morph, Value); }
    ActiveMesh->AddTickPrerequisiteComponent(this);
    bReady = true;
    return true;
}

bool UHCM5VS2NPCFaceComponent::SetEmotion(FName Emotion, float Value)
{
    if (!FMath::IsFinite(Value)) return false;
    if (Emotion.IsNone() || Emotion == TEXT("Calm")) Emotion = TEXT("Neutral");
    if (Emotion == TEXT("Smile") || Emotion == TEXT("Relieved")) Emotion = TEXT("Happy");
    if (Emotion == TEXT("Concerned")) Emotion = TEXT("Sad");
    if (Emotion == TEXT("Determined") || Emotion == TEXT("Wry")) Emotion = TEXT("Serious");
    const TArray<FName> Allowed = {TEXT("Neutral"), TEXT("Happy"), TEXT("Surprised"), TEXT("Angry"), TEXT("Sad"), TEXT("Shy"), TEXT("Serious")};
    if (!Allowed.Contains(Emotion)) return false;
    RequestedEmotion = Emotion; Intensity = FMath::Clamp(Value, 0.f, 1.f); return true;
}

void UHCM5VS2NPCFaceComponent::NotifyHit(float Value)
{
    if (!FMath::IsFinite(Value) || Value <= 0) return;
    HitRemaining = .36f; HitIntensity = FMath::Clamp(Value, 0.f, 1.f);
}

void UHCM5VS2NPCFaceComponent::SetDialogueTextProgress(const FString& Text, int32 Visible, bool bSpeaking)
{
    if (!bSpeaking) { StopSpeaking(); return; }
    if (Text != SpokenText) { SpokenText = Text; PreviousVisibleCharacters = 0; TalkRemaining = 0; }
    const int32 Count = FMath::Clamp(Visible, 0, Text.Len());
    const int32 Added = Count - PreviousVisibleCharacters;
    if (Added > 0 && Added <= 4)
    {
        const TCHAR Last = Text[Count - 1];
        const bool bPunctuation = FChar::IsWhitespace(Last) || FString(TEXT(".,!?;:，。！？；：、…—\"“”‘’（）()[]【】")).Contains(FString::Chr(Last));
        const FName Vowels[] = {TEXT("A"), TEXT("I"), TEXT("U"), TEXT("E"), TEXT("O")};
        TalkGroup = Vowels[uint32(Last) % UE_ARRAY_COUNT(Vowels)];
        TalkRemaining = bPunctuation ? 0.f : .105f;
    }
    else if (Added < 0 || Added > 4) TalkRemaining = 0;
    PreviousVisibleCharacters = Count;
}

void UHCM5VS2NPCFaceComponent::StopSpeaking()
{ SpokenText.Reset(); PreviousVisibleCharacters = 0; TalkRemaining = 0; }

void UHCM5VS2NPCFaceComponent::SetLookTarget(AActor* Actor, FName SocketName)
{ LookActor = Actor; LookSocket = SocketName; }
void UHCM5VS2NPCFaceComponent::ClearLookTarget() { LookActor.Reset(); LookSocket = NAME_None; }
bool UHCM5VS2NPCFaceComponent::GetLookTargetLocation(FVector& Location) const
{
    const AActor* Actor = LookActor.Get();
    if (!Actor) return false;
    if (const ACharacter* Character = Cast<ACharacter>(Actor))
    {
        if (!LookSocket.IsNone() && Character->GetMesh()->DoesSocketExist(LookSocket))
            Location = Character->GetMesh()->GetSocketLocation(LookSocket);
        else Location = Character->GetPawnViewLocation();
    }
    else Location = Actor->GetActorLocation();
    return !Location.ContainsNaN();
}

FName UHCM5VS2NPCFaceComponent::GetHumanoidBone(FName Role) const
{
    const FName* Bone = Profile ? Profile->HumanoidBones.Find(Role) : nullptr;
    return Bone ? *Bone : NAME_None;
}

void UHCM5VS2NPCFaceComponent::AddGroup(FName Group, float Weight, TMap<FName, float>& Out) const
{
    if (!Profile || Weight <= 0) return;
    for (const auto& Pose : Profile->FaceGroups) if (Pose.Group == Group)
        for (const auto& Bind : Pose.Binds) Out.FindOrAdd(Bind.Morph) = FMath::Clamp(Out.FindRef(Bind.Morph) + Weight * Bind.Weight, 0.f, 1.f);
}

void UHCM5VS2NPCFaceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(DeltaTime, TickType, ThisTick);
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0 || !bReady || !ActiveMesh || GetOwner()->IsHidden()) return;
    if (ActiveMesh->GetSkeletalMeshAsset() != ActiveAsset) { Restore(); return; }
    const AHCM3NPC* NPC = Cast<AHCM3NPC>(GetOwner());
    if (NPC)
    {
        const float Health = NPC->GetHealth();
        if (PreviousHealth >= 0 && Health < PreviousHealth) NotifyHit(FMath::Clamp((PreviousHealth - Health) / 25.f, .4f, 1.f));
        PreviousHealth = Health;
        if (NPC->IsDead()) { StopSpeaking(); return; }
    }
    if (bAutomaticBlink)
    {
        if (BlinkElapsed < 0)
        { BlinkCountdown -= DeltaTime; if (BlinkCountdown <= 0) BlinkElapsed = 0; }
        else
        {
            BlinkElapsed += DeltaTime;
            BlinkWeight = BlinkElapsed < .065f ? BlinkElapsed / .065f : BlinkElapsed < .185f ? 1.f : 1.f - (BlinkElapsed - .185f) / .10f;
            if (BlinkElapsed >= .285f) { BlinkElapsed = -1; BlinkWeight = 0; BlinkCountdown = Random.FRandRange(2.8f, 5.6f); }
        }
    }
    else { BlinkElapsed = -1; BlinkWeight = 0; }
    TalkingWeight = FMath::FInterpConstantTo(TalkingWeight, TalkRemaining > 0 ? .42f : 0.f, DeltaTime, 5.f);
    TalkRemaining = FMath::Max(0.f, TalkRemaining - DeltaTime);
    HitRemaining = FMath::Max(0.f, HitRemaining - DeltaTime);
    EffectiveEmotion = HitRemaining > 0 ? FName(TEXT("Pain")) : RequestedEmotion;
    FName Group = NAME_None; float EmotionWeight = Intensity;
    if (EffectiveEmotion == TEXT("Happy")) Group = TEXT("Joy");
    else if (EffectiveEmotion == TEXT("Angry")) Group = TEXT("Angry");
    else if (EffectiveEmotion == TEXT("Sad")) Group = TEXT("Sorrow");
    else if (EffectiveEmotion == TEXT("Surprised")) Group = TEXT("Surprised");
    // Deliberate restrained blends of the model's official groups, not additional imported presets.
    else if (EffectiveEmotion == TEXT("Shy")) { Group = TEXT("Fun"); EmotionWeight *= .4f; }
    else if (EffectiveEmotion == TEXT("Serious")) { Group = TEXT("Angry"); EmotionWeight *= .35f; }
    else if (EffectiveEmotion == TEXT("Pain")) { Group = TEXT("Angry"); EmotionWeight = HitIntensity * .7f; }
    TMap<FName, float> Goal;
    AddGroup(Group, EmotionWeight * (1.f - BlinkWeight) * (1.f - TalkingWeight), Goal);
    AddGroup(TEXT("Blink"), BlinkWeight, Goal);
    if (HitRemaining <= 0) AddGroup(TalkGroup, TalkingWeight, Goal);
    for (auto& Pair : Current)
    {
        // The explicit blink envelope already supplies its own close/hold/open timing.
        bool bBlinkMorph = false;
        for (const auto& Pose : Profile->FaceGroups) if (Pose.Group == TEXT("Blink"))
            for (const auto& Bind : Pose.Binds) bBlinkMorph |= Bind.Morph == Pair.Key;
        Pair.Value = bBlinkMorph ? Goal.FindRef(Pair.Key) : FMath::FInterpConstantTo(Pair.Value, Goal.FindRef(Pair.Key), DeltaTime, 1.f / FMath::Max(.05f, BlendSeconds));
        ActiveMesh->SetMorphTarget(Pair.Key, Pair.Value);
    }
}
