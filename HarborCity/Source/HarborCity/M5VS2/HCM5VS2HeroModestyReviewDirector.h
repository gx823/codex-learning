#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2HeroModestyReviewDirector.generated.h"

class AHCM1Character;
class AHCM1PlayerController;
class ACameraActor;
class UAnimInstance;
class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;
class UGameViewportClient;
class UHCM5VS2HeroModestyComponent;
class FJsonObject;
struct FInputKeyEventArgs;

/** Private four-view clothing diagnostic. Fixed cameras are NOT player mouse evidence. */
UCLASS(Blueprintable)
class HARBORCITY_API AHCM5VS2HeroModestyReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2HeroModestyReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AHCM1Character> ExpectedCharacterClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UAnimInstance> ExpectedAnimationClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> ExpectedBody;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> ExpectedShorts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UMaterialInterface> ExpectedCloth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UMaterialInterface>> ExpectedBodyMaterials;
    /** Authored Front/Rear/Left/Right, fixed entire run; no camera movement. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<ACameraActor>> LowCameras;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Initialize();
    bool BindingValid() const;
    bool ShadersReady(FString& Failure);
    bool FinalViewValid() const;
    void SetView(int32 Index);
    void JumpKey(bool Pressed);
    void Capture(bool Jump);
    void ScreenshotProcessed();
    void ObserveInput(const FInputKeyEventArgs& Event);
    void ObserveActivation(bool Active);
    TSharedPtr<FJsonObject> Observe() const;
    void Finish(const FString& Status,const FString& Detail,bool UserStop=false);
    bool Write();
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Garment;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2HeroModestyComponent> Modesty;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle,ActivationHandle,ScreenshotHandle;
    TArray<FTransform> CameraTransforms;
    TArray<TSharedPtr<FJsonObject>> Captures,KeyEvents,Samples;
    TSharedPtr<FJsonObject> ShaderState,Pending;
    FString Directory,PendingPNG,Status=TEXT("RUNNING"),Detail;
    FVector Origin=FVector::ZeroVector;
    double StartedWall=0,FinishedWall=0,StageStartedWorld=0,ShotWall=0,NextShaderWall=0,NextSampleWorld=0;
    uint64 ReadyFrame=0,RequestFrame=0;
    int32 ViewIndex=INDEX_NONE,Stage=0,JumpCount=0;
    bool bEnabled=false,bInitialized=false,bHadFocus=false,bJumpHeld=false,bShotProcessed=false;
    bool bDone=false,bStopped=false,bAutoQuit=false,bWriteFailed=false,bAirborne=false;
};
