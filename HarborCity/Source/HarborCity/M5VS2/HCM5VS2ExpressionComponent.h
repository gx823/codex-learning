#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HCM5VS2ExpressionComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UHCM4CombatComponent;

/** Opt-in face driver for the new Selestia BP. No legacy character creates this component. */
UCLASS(ClassGroup=(HarborCity), meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2ExpressionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2ExpressionComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;

    /** Happy, Surprised, Angry, Sad, Shy, Serious, Neutral; existing story aliases are accepted. */
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") bool SetEmotion(FName Emotion, float Intensity = 1.f);
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") void SetCombatExpression(bool bInCombat);
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") void NotifyHit(float Intensity = 1.f);
    /** Call for the actual speaking character as its text is revealed. Not audio/phoneme recognition. */
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") void SetDialogueTextProgress(const FString& FullText, int32 VisibleCharacters, bool bSpeaking);
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") void StopSpeaking();
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") bool SetTargetMesh(USkeletalMeshComponent* Mesh);
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") bool ReinitializeFace();
    UFUNCTION(BlueprintCallable, Category="M5VS2|Expression") void SetExpressionEnabled(bool bEnabled);

    /** Target storage only. An AnimBP/look-at implementation must explicitly consume this interface. */
    UFUNCTION(BlueprintCallable, Category="M5VS2|LookTarget") void SetLookTarget(AActor* Actor, FName SocketName = NAME_None);
    UFUNCTION(BlueprintCallable, Category="M5VS2|LookTarget") void ClearLookTarget();
    UFUNCTION(BlueprintPure, Category="M5VS2|LookTarget") bool GetLookTargetLocation(FVector& WorldLocation) const;
    UFUNCTION(BlueprintPure, Category="M5VS2|Expression") bool IsFaceReady() const { return bFaceReady; }
    UFUNCTION(BlueprintPure, Category="M5VS2|Expression") FName GetEffectiveEmotion() const { return EffectiveEmotion; }
    UFUNCTION(BlueprintPure, Category="M5VS2|Expression") float GetBlinkWeight() const { return BlinkWeight; }
    UFUNCTION(BlueprintPure, Category="M5VS2|Expression") float GetTalkingWeight() const { return TalkingWeight; }
    UFUNCTION(BlueprintPure, Category="M5VS2|Expression") TArray<FName> GetMissingMorphNames() const { return MissingMorphs; }
    UFUNCTION(BlueprintPure, Category="M5VS2|Expression") int32 GetObservedHitCount() const { return ObservedHitCount; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Expression") bool bExpressionsEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Expression") bool bAutomaticBlink = true;
    /** Reads accepted health changes and real attack/aim/reload state; never applies damage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Expression") bool bReadOwnerCombatState = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Expression", meta=(ClampMin="0.05", ClampMax="2.0")) float ExpressionBlendSeconds = .16f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Blink", meta=(ClampMin="0.5")) float BlinkIntervalMin = 2.8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Blink", meta=(ClampMin="0.5")) float BlinkIntervalMax = 5.6f;
    /** Fully closed hold, separately from closing/opening ramps. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Blink", meta=(ClampMin="0.1", ClampMax="0.15")) float BlinkClosedSeconds = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Blink") int32 BlinkRandomSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="M5VS2|Expression") TObjectPtr<USkeletalMeshComponent> TargetMesh;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void RestoreOwnedMorphs();
    void BuildEmotionWeights(FName Emotion, float Intensity, TMap<FName, float>& Out) const;
    void TickBlink(float DeltaTime);
    float NextBlinkInterval();
    static bool CanonicalEmotion(FName In, FName& Out);

    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> ActiveMesh;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> ActiveMeshAsset;
    UPROPERTY(Transient) TObjectPtr<UHCM4CombatComponent> Combat;
    UPROPERTY(Transient) TArray<FName> MissingMorphs;
    TWeakObjectPtr<AActor> LookActor;
    FName LookSocket;
    TArray<FName> ManagedMorphs;
    TMap<FName, float> OriginalWeights;
    TMap<FName, float> CurrentWeights;
    FRandomStream BlinkRandom;
    FName RequestedEmotion = TEXT("Neutral"), EffectiveEmotion = TEXT("Neutral");
    float RequestedIntensity = 1.f;
    float BlinkCountdown = 3.f, BlinkElapsed = -1.f, BlinkWeight = 0;
    float HitRemaining = 0, HitIntensity = 0;
    float TalkRemaining = 0, TalkingWeight = 0;
    float PreviousHealth = -1;
    FName TalkingMorph = TEXT("vrc_v_aa");
    FString SpokenText;
    int32 PreviousVisibleCharacters = 0, ObservedHitCount = 0;
    bool bExplicitCombat = false, bFaceReady = false;
};
