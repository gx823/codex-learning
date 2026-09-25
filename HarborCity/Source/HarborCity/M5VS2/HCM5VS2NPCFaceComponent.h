#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "HCM5VS2NPCFaceComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FHCM5VS2NPCMorphWeight
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Morph;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float Weight = 1.f;
};

/** A named VRM group resolved to actual imported morph names. Weights are normalized from VRM percent. */
USTRUCT(BlueprintType)
struct FHCM5VS2NPCFacePose
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Group;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHCM5VS2NPCMorphWeight> Binds;
};

/** Independent Q/R data; no VRM plugin runtime dependency or Selestia morph assumptions. */
UCLASS(BlueprintType)
class HARBORCITY_API UHCM5VS2NPCProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SourceSHA256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MetadataAssetPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> HumanoidBones;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHCM5VS2NPCFacePose> FaceGroups;
};

UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2NPCFaceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2NPCFaceComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="M5VS2|NPC") TObjectPtr<UHCM5VS2NPCProfile> Profile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="M5VS2|NPC") TObjectPtr<USkeletalMeshComponent> TargetMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="M5VS2|NPC") bool bAutomaticBlink = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="M5VS2|NPC", meta=(ClampMin="0.05")) float BlendSeconds = .16f;
    UFUNCTION(BlueprintCallable) bool ReinitializeFace();
    UFUNCTION(BlueprintCallable) bool SetEmotion(FName Emotion, float Intensity = 1.f);
    UFUNCTION(BlueprintCallable) void NotifyHit(float Intensity = 1.f);
    /** Text reveal drives a restrained vowel approximation, not voice or phoneme recognition. */
    UFUNCTION(BlueprintCallable) void SetDialogueTextProgress(const FString& FullText, int32 VisibleCharacters, bool bSpeaking);
    UFUNCTION(BlueprintCallable) void StopSpeaking();
    UFUNCTION(BlueprintCallable) void SetLookTarget(AActor* Actor, FName SocketName = NAME_None);
    UFUNCTION(BlueprintCallable) void ClearLookTarget();
    UFUNCTION(BlueprintPure) bool GetLookTargetLocation(FVector& Location) const;
    UFUNCTION(BlueprintPure) bool IsFaceReady() const { return bReady; }
    UFUNCTION(BlueprintPure) FName GetEffectiveEmotion() const { return EffectiveEmotion; }
    UFUNCTION(BlueprintPure) float GetBlinkWeight() const { return BlinkWeight; }
    UFUNCTION(BlueprintPure) float GetTalkingWeight() const { return TalkingWeight; }
    UFUNCTION(BlueprintPure) TArray<FName> GetMissingMorphNames() const { return MissingMorphs; }
    UFUNCTION(BlueprintPure) FName GetHumanoidBone(FName HumanRole) const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Restore();
    void AddGroup(FName Group, float Weight, TMap<FName, float>& Out) const;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> ActiveMesh;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> ActiveAsset;
    UPROPERTY(Transient) TArray<FName> MissingMorphs;
    TMap<FName, float> Original, Current;
    TWeakObjectPtr<AActor> LookActor;
    FName LookSocket;
    FName RequestedEmotion = TEXT("Neutral"), EffectiveEmotion = TEXT("Neutral"), TalkGroup = TEXT("A");
    FString SpokenText;
    int32 PreviousVisibleCharacters = 0;
    float Intensity = 1.f, BlinkCountdown = 3.f, BlinkElapsed = -1.f, BlinkWeight = 0;
    float TalkRemaining = 0, TalkingWeight = 0, HitRemaining = 0, HitIntensity = 0, PreviousHealth = -1.f;
    FRandomStream Random;
    bool bReady = false;
};
