#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HCM5VS2DrivingHandsComponent.generated.h"
class AHCM1Vehicle;
class AHCM1Character;
class AHCM1PlayerController;
class USkeletalMesh;
class UAnimSequence;
class UBlueprint;
class UStaticMeshComponent;
class UHCM4R2FirstPersonMesh;

/** Opt-in private vehicle visual. No vehicle physics, input or camera authority. */
UCLASS(ClassGroup=(HarborCity),meta=(BlueprintSpawnableComponent))
class HARBORCITY_API UHCM5VS2DrivingHandsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHCM5VS2DrivingHandsComponent();
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<USkeletalMesh> ArmsOverride;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> GripFingerPose;
    UFUNCTION(BlueprintCallable) FString GetDrivingHandsDiagnostics() const;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Tick) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool Resolve(AHCM1PlayerController* PC,FString& Error);
    bool UpdateHands(float DeltaTime,FString& Error);
    void HideAndReset();
    UPROPERTY(Transient) TObjectPtr<AHCM1Vehicle> Vehicle;
    UPROPERTY(Transient) TObjectPtr<AHCM1Character> Driver;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Wheel;
    UPROPERTY(Transient) TObjectPtr<UHCM4R2FirstPersonMesh> Display;
    TArray<int32> FingerIndices;
    TArray<FQuat> FingerRotations;
    FQuat PalmFrame[2];
    FVector PalmContact[2];
    FVector ShoulderWorld[2];
    FVector ElbowWorld[2];
    FVector WristWorld[2];
    FVector ContactWorld[2];
    FVector TargetContactWorld[2];
    double UpperLength[2]={0,0};
    double LowerLength[2]={0,0};
    double ContactError[2]={0,0};
    double ArmLengthError[2]={0,0};
    double WheelLocalAngle[2]={0,0};
    double SeatAngle[2]={-80,80};
    bool Sliding[2]={false,false};
    float FittedShoulderForwardCm=0;
    float WheelAngle=0;
    float RegripFrom=0;
    float RegripAge=0;
    int32 Regripping=INDEX_NONE;
    int32 RegripCount=0;
    uint64 PoseFrame=0;
    bool bActive=false;
    bool bGeometryReady=false;
    bool bFitted=false;
    bool bGripInitialized=false;
    FString Failure;
};

UCLASS()
class HARBORCITY_API UHCM5VS2DrivingHandsEditor : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="HarborCity|Editor")
    static FString ProbeDriverSource(AHCM1Character* Character,USkeletalMesh* Arms,UAnimSequence* FingerPose);
    UFUNCTION(BlueprintCallable,Category="HarborCity|Editor")
    static FString ConfigurePrivateVehicle(UBlueprint* Blueprint,USkeletalMesh* Arms,UAnimSequence* FingerPose);
};
