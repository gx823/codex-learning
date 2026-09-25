#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "HCM5VS2DrivingHandsReviewDirector.generated.h"
class AHCM1Character;
class AHCM1Vehicle;
class AHCM1PlayerController;
class UHCM5VS2DrivingHandsComponent;
class UGameViewportClient;
class UMaterialInterface;
class USkeletalMesh;
class UAnimInstance;
class FJsonObject;
struct FInputKeyEventArgs;
/** Private engine-key review. No pawn/camera/vehicle control setters. */
UCLASS()
class HARBORCITY_API AHCM5VS2DrivingHandsReviewDirector : public AActor
{
    GENERATED_BODY()
public:
    AHCM5VS2DrivingHandsReviewDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere) TSubclassOf<AHCM1Character> ExpectedCharacterClass;
    UPROPERTY(EditAnywhere) TSubclassOf<AHCM1Vehicle> ExpectedVehicleClass;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> ExpectedBody;
    UPROPERTY(EditAnywhere) TSubclassOf<UAnimInstance> ExpectedAnimationClass;
    UPROPERTY(EditAnywhere) TArray<TObjectPtr<UMaterialInterface>> ExpectedMaterials;
    UPROPERTY(EditAnywhere) FName ExpectedPeriod=TEXT("Afternoon");
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Initialize();
    bool ReadLookChain();
    bool BindingValid() const;
    bool MaterialsReady(FString& Failure);
    bool Check(bool Good,const FString& Name,const FString& Detail);
    void Key(const FKey& K,bool Pressed);
    void Pulse(const FKey& K);
    void ReleaseOwned();
    void Look(float Pitch,float Yaw,float Dt);
    void Advance(int32 Next);
    void Capture(const FString& Label,int32 Next);
    void ScreenshotProcessed();
    TSharedPtr<FJsonObject> Observe() const;
    void ObserveInput(const FInputKeyEventArgs& Event);
    void ObserveActivation(bool Active);
    void Finish(const FString& Result,const FString& Reason,bool Stop=false);
    bool Write();
    UPROPERTY(Transient) TObjectPtr<AHCM1PlayerController> PC;
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Character;
    UPROPERTY(Transient) TObjectPtr<AHCM1Vehicle> Vehicle;
    UPROPERTY(Transient) TObjectPtr<UHCM5VS2DrivingHandsComponent> Hands;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    FDelegateHandle InputHandle;
    FDelegateHandle ActivationHandle;
    FDelegateHandle ScreenshotHandle;
    TSet<FKey> Held;
    TArray<FKey> Pulsed;
    TArray<TSharedPtr<FJsonObject>> Captures, Samples, Events, Checks, LookChains;
    TSharedPtr<FJsonObject> Pending;
    TSharedPtr<FJsonObject> ShaderState;
    FString Directory, PendingPNG, Status=TEXT("RUNNING"), Detail;
    FVector VehicleOrigin=FVector::ZeroVector;
    FVector2D MappingScale=FVector2D::ZeroVector;
    FVector2D RawMouseSum=FVector2D::ZeroVector;
    float FootYaw=0,LeftWheel=0,RightWheel=0;
    double StartedWall=0,StageWall=0,ShotWall=0,FinishedWall=0,NextSample=0,NextShader=0;
    uint64 RequestFrame=0,CallbacksBefore=0;
    int32 Stage=0,PendingNext=0;
    bool bEnabled=false,bInitialized=false,bHadFocus=false,bStopped=false,bDone=false,bAutoQuit=false,bWriteFailed=false,bShotProcessed=false,bWarmReady=false,bThrottled=false;
};
