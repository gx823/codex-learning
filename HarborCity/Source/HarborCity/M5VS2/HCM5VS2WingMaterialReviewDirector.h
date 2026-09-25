#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2WingMaterialReviewDirector.generated.h"
class AHCM1Character; class AHCM1PlayerController; class ACameraActor;
class UHCM5VS2FlightComponent; class UHCM5VS2FlightVisualComponent;
class UMaterialInterface; class UMaterialInstanceDynamic; class UStaticMeshComponent;
class UGameViewportClient; class FJsonObject; struct FInputKeyEventArgs;
struct FHCM5VS2WingMaterialReadiness;

/** Private opt-in material fixture only; four unretouched same-pose viewport PNGs. */
UCLASS()
class HARBORCITY_API AHCM5VS2WingMaterialReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2WingMaterialReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere) TSubclassOf<AHCM1Character> ReferenceHeroClass;
    UPROPERTY(EditAnywhere) TSubclassOf<AHCM1Character> CandidateHeroClass;
    UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> ReferenceMaterial;
    UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> CandidateMaterial;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void BindViewport(); void Input(const FInputKeyEventArgs& Event); void Drawn(); void Processed();
    bool Start(); bool Ready(); bool Freeze(); void Unfreeze(); bool Invariants() const;
    bool ApplyShot(); void Capture(); void Finish(const FString& Status,const FString& Detail,bool Stop=false);
    bool Write(const FString& Status,const FString& Detail); TSharedPtr<FJsonObject> Snapshot() const;
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Hero;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2FlightComponent> Flight;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2FlightVisualComponent> Visual;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Feathers;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MaterialA;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MaterialB;
    TArray<TWeakObjectPtr<UActorComponent>> FrozenComponents;
    TArray<bool> FrozenTickStates;
    TArray<FTransform> FrozenFeathers,FrozenPose;
    FTransform FrozenHero;
    FVector CameraLocation=FVector::ZeroVector; FRotator CameraRotation=FRotator::ZeroRotator;
    TSharedPtr<FHCM5VS2WingMaterialReadiness,ESPMode::ThreadSafe> PendingReadiness;
    TSharedPtr<FJsonObject> Readiness,PendingShot;
    TArray<TSharedPtr<FJsonObject>> Captures,Transitions;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle,DrawHandle,ScreenshotHandle;
    FString Directory,PendingPNG,FinalStatus;
    double Started=0,StageStarted=0,ShotStarted=0,Requested=0,Finished=0,StartZ=0;
    uint64 DrawCount=0,MaterialReadyDraw=0,RequestFrame=0,StopFrame=0;
    int32 Stage=0,ShotIndex=0,ShaderJobs=0,StreamingWanted=0;
    bool bActive=false,bStopped=false,bHadFocus=false,bFrozen=false,bSavedActorTick=false;
    bool bProcessed=false,bShaderReady=false,bAutoQuit=false,bExitPending=false,bReportFailure=false;
};
