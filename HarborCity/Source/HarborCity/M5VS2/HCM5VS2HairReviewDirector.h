#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2HairReviewDirector.generated.h"
class AHCM1Character; class AHCM1PlayerController; class ACameraActor; class ATargetPoint;
class USkeletalMesh; class UAnimInstance; class UGameViewportClient; class UHCM5VS2ExpressionComponent;
class FJsonObject; struct FInputKeyEventArgs;
class USkeletalMeshComponent; class UMaterialInterface;
struct FHCM5VS2HairOutlineMaterialCheck;
struct FHCM5VS2HairEdge { uint32 A=0,B=0; uint16 Chains=0; double ReferenceLength=0; };
/** Bounded opt-in hair diagnosis in a private fixture; no persistent settings or gameplay changes. */
UCLASS()
class HARBORCITY_API AHCM5VS2HairReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2HairReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere) TSubclassOf<AHCM1Character> ExpectedCharacterClass;
    UPROPERTY(EditAnywhere) TSubclassOf<UAnimInstance> ExpectedAnimationClass;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> ExpectedMesh;
    UPROPERTY(EditAnywhere) FString SourceReport;
    /** Private diagnostic only; also requires -M5VS2HairOutlineReview. Original seven-shot mode is unchanged. */
    UPROPERTY(EditAnywhere) bool bOutlineMaskComparison=false;
    UPROPERTY(EditAnywhere) TArray<TObjectPtr<UMaterialInterface>> OutlineComparisonMaterials;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Start(FString& Error); bool PrepareEdges(FString& Error); bool SetPhase(int32 Index);
    bool PollOutlineMaterials(FString& Error); bool FreezeOutlinePose(); void TickOutlineComparison(float Dt);
    TSharedPtr<FJsonObject> OutlineObservation() const;
    TSharedPtr<FJsonObject> SubstepObservation(float Dt) const;
    void StopSubstepAudit(const FString& Reason);
    bool bSubstepAudit=false; int32 ExpectedFixedSubstep=-1;
    bool bRootAnchorCandidate=false;
    TArray<TSharedPtr<FJsonObject>> SubstepFrames;
    const TCHAR* PhaseLabel() const;
    TSharedPtr<FJsonObject> Bones() const; TSharedPtr<FJsonObject> Skin();
    void Capture(); void Processed(); void Input(const FInputKeyEventArgs& Event);
    void Finish(const FString& Status,const FString& Detail); void Restore(); void Write(const FString& Status,const FString& Detail);
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2ExpressionComponent> Expression;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    UPROPERTY(Transient) TObjectPtr<ATargetPoint> LookTarget;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> OutlineMesh;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OriginalOutlineMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OriginalBodyMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> CheckedOutlineMaterials;
    TSharedPtr<FHCM5VS2HairOutlineMaterialCheck,ESPMode::ThreadSafe> PendingOutlineMaterialCheck;
    TSharedPtr<FJsonObject> LastOutlineReadiness,FrozenPoseEvidence;
    TArray<FTransform> FrozenBones;
    TMap<FName,float> FrozenMorphs;
    FTransform FrozenMeshTransform;
    double NextOutlinePoll=0;
    bool bOutlineMaterialsReady=false,bPoseFrozen=false;
    bool bOriginalMeshTick=false,bOriginalExpressionTick=false,bOriginalCharacterTick=false,bOriginalMovementTick=false;
    TWeakObjectPtr<AActor> OriginalView;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle,ScreenshotHandle;
    TArray<FName> Names; TArray<int32> Indices,Parents; TArray<double> BindLengths;
    TArray<FHCM5VS2HairEdge> Edges; TArray<double> OffLengths; TSet<uint32> HairVertices;
    TArray<TSharedPtr<FJsonObject>> Samples,Captures,SkinSamples,Events;
    TSharedPtr<FJsonObject> Pending,MeshAudit;
    FString Directory,PendingPNG;
    FVector Origin=FVector::ZeroVector;
    double Started=0,LastSample=0,Age=0,Active=0,RequestAt=0,Finished=0,Warmup=0;
    uint64 RequestFrame=0,StopFrame=0;
    int32 Phase=INDEX_NONE,OriginalKawaii=1,OriginalKawaiiFlags=0;
    bool bActive=false,bReady=false,bStopped=false,bExit=false,bAutoQuit=false,bPendingProcessed=false;
    bool bCaptured=false,bSaved=false,bOriginalHUD=true,bOriginalGlances=true;
};
