#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2CornerRevTwoReviewDirector.generated.h"
class ACameraActor; class ADirectionalLight; class ASkyLight; class APostProcessVolume;
class APlayerController; class UGameViewportClient; class FJsonObject;
class AHCM5VS2CornerTimeDirector;
class ACharacter; class UMaterialInterface;
struct FHCM5VS2RevTwoRenderMaterialCheck;
struct FInputKeyEventArgs;
/** Opt-in seven raw environment viewport captures using the playable time API. Never an OS input test. */
UCLASS()
class HARBORCITY_API AHCM5VS2CornerRevTwoReviewDirector : public AActor
{
 GENERATED_BODY()
public:
 AHCM5VS2CornerRevTwoReviewDirector();
 virtual void Tick(float DeltaSeconds) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 bool LoadPlan(FString& Error); bool Start(FString& Error,bool& bRetryable);
 void TryStart(); void ObserveViewport(); bool BeginShot(int32 Index,FString& Error);
 FVector ExpectedCameraLocation() const; FRotator ExpectedCameraRotation() const;
 bool CameraMatchesRequest() const; bool LightingMatchesRequest() const; bool Ready();
 bool BindHero(FString& Error); bool StageHero(int32 Index,FString& Error);
 bool LockHeroCamera(FString& Error); bool PollHeroMaterials(FString& Error);
 TSharedPtr<FJsonObject> Snapshot() const;
 void Capture(); void Processed(); void Drawn(); void Input(const FInputKeyEventArgs& Event);
 void Restore(); void Finish(const FString& Status,const FString& Detail);
 bool Write(const FString& Status,const FString& Detail); void RemoveDelegates();
 UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
 UPROPERTY(Transient) TObjectPtr<ADirectionalLight> MainLight;
 UPROPERTY(Transient) TObjectPtr<ASkyLight> SkyLight;
 UPROPERTY(Transient) TObjectPtr<APostProcessVolume> PostProcess;
 UPROPERTY(Transient) TObjectPtr<APlayerController> Controller;
 UPROPERTY(Transient) TObjectPtr<AHCM5VS2CornerTimeDirector> TimeDirector;
 UPROPERTY(Transient) TObjectPtr<ACharacter> Hero;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> CheckedMaterials;
 TSharedPtr<FHCM5VS2RevTwoRenderMaterialCheck,ESPMode::ThreadSafe> PendingMaterialCheck;
 TSharedPtr<FJsonObject> LastMaterialReadiness,HeroCameraLocks[2];
 TWeakObjectPtr<AActor> OriginalView;
 TWeakObjectPtr<UGameViewportClient> Viewport;
 FDelegateHandle InputHandle,ScreenshotHandle,DrawHandle;
 TSharedPtr<FJsonObject> Plan,OriginalState,Pending,FirstStartupReadback,LastStartupReadback;
 TArray<TSharedPtr<FJsonObject>> Shots,Results;
 FString Directory,PlanFile,PlanSHA1,PendingPNG,ExitReason,RequestedLightingReadback;
 FString Profile=TEXT("Environment");
 bool bHeroProfile=false,bHeroStateSaved=false,bMaterialsReady=false;
 FTransform OriginalHeroTransform;
 FVector HeroCameraLocations[2]; FRotator HeroCameraRotations[2];
 double CameraLockedAt=0,NextMaterialPoll=0;
 uint64 CameraLockDraw=0;
 FName SavedPeriod;
 int32 ShotIndex=INDEX_NONE,ReadyDraws=0,WantingResources=0,ShaderJobs=0,StartupAttempts=0;
 uint64 DrawCount=0,ShotDrawStart=0,RequestFrame=0,ProcessedFrame=0,StopFrame=0;
 double Started=0,ShotStarted=0,Requested=0,Finished=0;
 bool bStarting=false,bActive=false,bStopped=false,bPending=false,bProcessed=false;
 bool bReady=false,bStateSaved=false,bSavedHUD=true,bHUDSaved=false,bAutoQuit=false,bExitPending=false;
};
