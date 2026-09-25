#include "HCM5VS2DrivingHandsComponent.h"
#include "M1/HCM1Vehicle.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M4R2/HCM4R2CockpitComponent.h"
#include "M4R2/HCM4R2PresentationComponent.h"
#include "M4R2/HCM4R2FirstPersonMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "TwoBoneIK.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif
namespace
{
constexpr double RimRadius=16.3,RimTubeRadius=1.65;
constexpr double Home[2]={-80,80};
const FName Upper[2]={TEXT("UpperArm_L"),TEXT("UpperArm_R")};
const FName Lower[2]={TEXT("LowerArm_L"),TEXT("LowerArm_R")};
const FName Hand[2]={TEXT("Hand_L"),TEXT("Hand_R")};
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& V){return{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
FString JSON(const TSharedRef<FJsonObject>& J){FString S;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S));return S;}
bool PrivateVehicle(const UObject* O)
{
    if(!O)return false;const FString Path=O->GetOutermost()->GetName(),Prefix=TEXT("/Game/HarborCity/M5VS2/DrivingHands/Review_");
    if(!Path.StartsWith(Prefix)||Path.Len()<=Prefix.Len()+13||Path[Prefix.Len()+12]!=TCHAR('/'))return false;
    for(TCHAR C:Path.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return false;return true;
}
FVector RimPoint(double Angle,double Lift=0)
{const double R=FMath::DegreesToRadians(Angle);return FVector(-RimTubeRadius-Lift,RimRadius*FMath::Sin(R),RimRadius*FMath::Cos(R));}
// Only these locally sampled finger rotations cross the source skeleton pointer.
// Their named parent and local reference basis must be identical first.
bool SourceGeometry(AHCM1Character* Character,USkeletalMesh* Arms,UAnimSequence* Pose,TArray<int32>& Fingers,TArray<FQuat>& Rotations,FQuat* Palm,FVector* Contact,TSharedRef<FJsonObject> J,FString& Error)
{
    auto Fail=[&](const FString& Text){Error=Text;return false;};Fingers.Reset();Rotations.Reset();
    auto* Body=Character?Character->GetMesh():nullptr;auto* Source=Body?Body->GetSkeletalMeshAsset():nullptr;auto* Presentation=Character?Character->GetR2PresentationComponent():nullptr;
    if(!Source||!Arms||!Pose||!Presentation||Arms==Source||Arms->GetSkeleton()!=Source->GetSkeleton())return Fail(TEXT("Actual matching trimmed Selestia Arms and Body skeleton required"));
    if(!Arms->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_"))||Arms->GetName()!=TEXT("SKM_HeroRev2_FirstPersonArms"))return Fail(TEXT("Only current privately validated Selestia arms accepted"));
    if(Presentation->FirstPersonArmsOverride.LoadSynchronous()!=Arms)return Fail(TEXT("Configured arms are not actual selected character arms"));
    if(!Pose->GetSkeleton()||Pose->IsValidAdditive()||Pose->GetPlayLength()<.28||Pose->GetOutermost()->GetName()!=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/Animation/MM_Attack_01_Selestia"))return Fail(TEXT("Exact already imported named finger pose and sample time required"));
    const FReferenceSkeleton& Ref=Arms->GetRefSkeleton();const FReferenceSkeleton& PRef=Pose->GetSkeleton()->GetReferenceSkeleton();double MaxBasisError=0;
    auto Match=[&](FName Bone)
    {
        const int32 A=Ref.FindBoneIndex(Bone),B=PRef.FindBoneIndex(Bone);if(A==INDEX_NONE||B==INDEX_NONE)return false;
        const int32 AP=Ref.GetParentIndex(A),BP=PRef.GetParentIndex(B);if(AP==INDEX_NONE||BP==INDEX_NONE||Ref.GetBoneName(AP)!=PRef.GetBoneName(BP))return false;
        const auto& AT=Ref.GetRefBonePose()[A];const auto& BT=PRef.GetRefBonePose()[B];MaxBasisError=FMath::Max(MaxBasisError,FVector::Distance(AT.GetLocation(),BT.GetLocation()));
        return AT.Equals(BT,.0001);
    };
    TArray<TSharedPtr<FJsonValue>> ArmRows;
    for(int32 Side=0;Side<2;++Side)
    {
        const TCHAR* S=Side==0?TEXT("L"):TEXT("R");const int32 UI=Ref.FindBoneIndex(Upper[Side]),LI=Ref.FindBoneIndex(Lower[Side]),HI=Ref.FindBoneIndex(Hand[Side]);
        const int32 MI=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("MiddleProximal_%s"),S))),II=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("IndexProximal_%s"),S))),TI=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("LittleProximal_%s"),S)));
        if(UI==INDEX_NONE||LI==INDEX_NONE||HI==INDEX_NONE||MI==INDEX_NONE||II==INDEX_NONE||TI==INDEX_NONE||Ref.GetParentIndex(LI)!=UI||Ref.GetParentIndex(HI)!=LI||Ref.GetParentIndex(MI)!=HI||Ref.GetParentIndex(II)!=HI||Ref.GetParentIndex(TI)!=HI||!Match(Hand[Side]))return Fail(TEXT("Actual anatomical arm and palm hierarchy/reference basis invalid"));
        const FVector M=Ref.GetRefBonePose()[MI].GetLocation(),I=Ref.GetRefBonePose()[II].GetLocation(),T=Ref.GetRefBonePose()[TI].GetLocation();
        const FVector Long=M.GetSafeNormal(),Across=(I-T).GetSafeNormal();if(Long.IsNearlyZero()||Across.IsNearlyZero()||FMath::Abs(Long.Dot(Across))>.97)return Fail(TEXT("Degenerate measured palm basis"));
        Palm[Side]=FRotationMatrix::MakeFromXY(Long,Across).ToQuat();Contact[Side]=(M+I+T)/3.*.75;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("side"),S);Row->SetNumberField(TEXT("upper_reference_cm"),Ref.GetRefBonePose()[LI].GetTranslation().Size());Row->SetNumberField(TEXT("lower_reference_cm"),Ref.GetRefBonePose()[HI].GetTranslation().Size());Row->SetArrayField(TEXT("palm_contact_proxy_in_hand_cm"),XYZ(Contact[Side]));ArmRows.Add(MakeShared<FJsonValueObject>(Row));
        for(const TCHAR* Stem:{TEXT("Thumb"),TEXT("Index"),TEXT("Middle"),TEXT("Ring"),TEXT("Little")})for(const TCHAR* Joint:{TEXT("Proximal"),TEXT("Intermediate"),TEXT("Distal")})
        {
            const FName Bone(*FString::Printf(TEXT("%s%s_%s"),Stem,Joint,S));if(!Match(Bone))return Fail(TEXT("Finger source local basis differs: ")+Bone.ToString());
            FTransform Sample;Pose->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(PRef.FindBoneIndex(Bone)),FAnimExtractContext(.28),false);
            if(Sample.ContainsNaN())return Fail(TEXT("Nonfinite source finger sample"));Fingers.Add(Ref.FindBoneIndex(Bone));Rotations.Add(Sample.GetRotation().GetNormalized());
        }
    }
    TArray<TSharedPtr<FJsonValue>> Slots;
    for(const FSkeletalMaterial& M:Arms->GetMaterials())
    {
        const int32 Slot=Body->GetMaterialIndex(M.MaterialSlotName);if(Slot==INDEX_NONE||!Body->GetMaterial(Slot))return Fail(TEXT("Actual selected body is missing arm material slot ")+M.MaterialSlotName.ToString());
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("slot_name"),M.MaterialSlotName.ToString());R->SetStringField(TEXT("current_body_material"),GetPathNameSafe(Body->GetMaterial(Slot)));Slots.Add(MakeShared<FJsonValueObject>(R));
    }
    J->SetStringField(TEXT("source_body"),GetPathNameSafe(Source));J->SetStringField(TEXT("arms"),GetPathNameSafe(Arms));J->SetStringField(TEXT("arms_skeleton"),GetPathNameSafe(Arms->GetSkeleton()));J->SetStringField(TEXT("finger_pose"),GetPathNameSafe(Pose));J->SetStringField(TEXT("finger_pose_skeleton"),GetPathNameSafe(Pose->GetSkeleton()));J->SetBoolField(TEXT("pose_skeleton_pointer_equal"),Pose->GetSkeleton()==Arms->GetSkeleton());J->SetBoolField(TEXT("named_finger_local_reference_basis_exact"),true);J->SetNumberField(TEXT("finger_reference_translation_max_error_cm"),MaxBasisError);J->SetNumberField(TEXT("finger_count"),Fingers.Num());J->SetNumberField(TEXT("finger_sample_seconds"),.28);J->SetArrayField(TEXT("arms_measured"),ArmRows);J->SetArrayField(TEXT("material_slots"),Slots);J->SetStringField(TEXT("contact_measurement_scope"),TEXT("Palm landmark proxy from native finger bases; visual skin-surface/rim intersections remain USER_REVIEW."));return Fingers.Num()==30;
}
}
UHCM5VS2DrivingHandsComponent::UHCM5VS2DrivingHandsComponent()
{
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
    for(int32 Side=0;Side<2;++Side)
    {PalmFrame[Side]=FQuat::Identity;PalmContact[Side]=ShoulderWorld[Side]=ElbowWorld[Side]=WristWorld[Side]=ContactWorld[Side]=TargetContactWorld[Side]=FVector::ZeroVector;}
}
void UHCM5VS2DrivingHandsComponent::BeginPlay()
{
    Super::BeginPlay();Vehicle=Cast<AHCM1Vehicle>(GetOwner());
    if(!Vehicle||!PrivateVehicle(Vehicle->GetClass())){SetComponentTickEnabled(false);return;}
    if(auto* Cockpit=Vehicle->GetCockpitComponent())AddTickPrerequisiteComponent(Cockpit);
}
void UHCM5VS2DrivingHandsComponent::HideAndReset()
{
    if(Display){Display->SetVisibility(false);Display->SetHiddenInGame(true);}bActive=false;bGripInitialized=false;bFitted=false;Regripping=INDEX_NONE;RegripAge=0;
}
bool UHCM5VS2DrivingHandsComponent::Resolve(AHCM1PlayerController* PC,FString& Error)
{
    auto* C=PC?PC->GetControlledCharacter():nullptr;if(!C)return false;
    auto* Cockpit=Vehicle->GetCockpitComponent();Wheel=Cockpit?Cockpit->GetSteeringWheelMesh():nullptr;
    if(!Wheel||!Wheel->GetStaticMesh())return false;
    if(Wheel->GetStaticMesh()->GetPathName()!=TEXT("/Game/HarborCity/M4R2/Interior/Meshes/SM_M4R2_SteeringWheel.SM_M4R2_SteeringWheel")){Error=TEXT("Unknown steering-wheel geometry; do not guess grip radius");return false;}
    if(Driver!=C||!bGeometryReady)
    {
        HideAndReset();Driver=C;auto Proof=MakeShared<FJsonObject>();bGeometryReady=SourceGeometry(C,ArmsOverride,GripFingerPose,FingerIndices,FingerRotations,PalmFrame,PalmContact,Proof,Error);if(!bGeometryReady)return false;
        AddTickPrerequisiteComponent(Driver->GetMesh());
        if(!Display)
        {
            Display=NewObject<UHCM4R2FirstPersonMesh>(Vehicle,TEXT("VS2DrivingHandsMesh"));Vehicle->AddInstanceComponent(Display);Display->SetupAttachment(Vehicle->GetMesh());
            Display->SetSkinnedAssetAndUpdate(ArmsOverride);Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);Display->SetGenerateOverlapEvents(false);Display->SetCanEverAffectNavigation(false);Display->SetCastShadow(false);Display->SetOnlyOwnerSee(true);Display->SetVisibility(false);Display->SetHiddenInGame(true);Display->SetForcedLOD(1);
            // World projection and vehicle mount: never camera-relative FP scale.
            Display->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);Display->RegisterComponent();
        }
        Display->SetSkinningVertexRadius(Driver->GetR2PresentationComponent()->FirstPersonSkinningRadiusCm);
    }
    return bGeometryReady&&Display&&Driver->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()==ArmsOverride->GetSkeleton();
}
void UHCM5VS2DrivingHandsComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaTime,TickType,Tick);if(!Vehicle)return;
    auto* PC=Cast<AHCM1PlayerController>(Vehicle->GetController());
    const bool Wanted=PC&&PC->GetActiveVehicle()==Vehicle&&PC->GetPlayerMode()==EHCPlayerMode::Driving&&PC->IsFirstPersonPerspective();
    if(!Wanted){HideAndReset();return;}if(PC->IsPauseMenuOpen()||!PC->IsGameplayFocused())return;
    FString Error;if(!Resolve(PC,Error)||!UpdateHands(FMath::Clamp(DeltaTime,0.f,.1f),Error))
    {HideAndReset();if(!Error.IsEmpty()&&Failure!=Error){Failure=Error;UE_LOG(LogTemp,Warning,TEXT("VS2_DRIVING_HANDS_CANDIDATE_INVALID %s"),*Error);}return;}
    Failure.Reset();bActive=true;Display->SetOwnerNoSee(false);Display->SetOnlyOwnerSee(true);Display->SetHiddenInGame(false);Display->SetVisibility(true);PoseFrame=GFrameCounter;
}
bool UHCM5VS2DrivingHandsComponent::UpdateHands(float Dt,FString& Error)
{
    auto Fail=[&](const FString& S){Error=S;return false;};auto* Source=Driver->GetMesh();auto* P=Driver->GetR2PresentationComponent();
    if(!Source||!P||!Display||!Wheel||FingerIndices.Num()!=30)return Fail(TEXT("Incomplete validated driving hand source"));
    const FVector Scale=Source->GetComponentScale();if(Scale.ContainsNaN()||FMath::Abs(Scale.X-Scale.Y)>.0001||FMath::Abs(Scale.X-Scale.Z)>.0001)return Fail(TEXT("Uniform actual source body scale required"));
    for(int32 I=0;I<ArmsOverride->GetMaterials().Num();++I)
    {const int32 SI=Source->GetMaterialIndex(ArmsOverride->GetMaterials()[I].MaterialSlotName);if(SI==INDEX_NONE||!Source->GetMaterial(SI))return Fail(TEXT("Actual arm/body named material pairing changed"));if(Display->GetMaterial(I)!=Source->GetMaterial(SI))Display->SetMaterial(I,Source->GetMaterial(SI));}
    Display->CopyPoseFromSkeletalComponent(Source);
    // CopyPose only copies local atoms and marks the pose dirty in UE 5.8.
    // Read the new pose's component transforms, not the prior frame's IK result.
    Display->RefreshBoneTransforms();
    const FQuat VehicleRotation=Vehicle->GetActorQuat();const FQuat MeshRotation=(VehicleRotation*Source->GetRelativeRotation().Quaternion()).GetNormalized();
    const FVector Eye=Vehicle->GetActorTransform().TransformPosition(Vehicle->GetDriverEyeLocal());
    const FVector CarForward=VehicleRotation.GetForwardVector(),CarRight=VehicleRotation.GetRightVector(),CarUp=VehicleRotation.GetUpVector();
    auto MountFor=[&](float Lean){return FTransform(MeshRotation,Eye-MeshRotation.RotateVector(P->EyeAnchorInMesh*Scale)+CarForward*Lean,Scale);};
    WheelAngle=Vehicle->GetCockpitComponent()->GetSteeringWheelAngleDegrees();if(!FMath::IsFinite(WheelAngle))return Fail(TEXT("Invalid actual wheel angle"));
    const FTransform WheelWorld=Wheel->GetComponentTransform();
    if(!bGripInitialized){for(int32 Side=0;Side<2;++Side){WheelLocalAngle[Side]=Home[Side]+WheelAngle;SeatAngle[Side]=Home[Side];}bGripInitialized=true;}
    for(int32 Side=0;Side<2;++Side)
    {
        Sliding[Side]=false;if(Side==Regripping)continue;
        const double Raw=WheelLocalAngle[Side]-WheelAngle;const double Bounded=Side==0?FMath::Clamp(Raw,-125.,-35.):FMath::Clamp(Raw,35.,125.);
        Sliding[Side]=FMath::Abs(Raw-Bounded)>.001;SeatAngle[Side]=Bounded;
        // The supporting hand may slide along the rim; it never leaves contact
        // or crosses the center while the other hand is regripping.
        if(Sliding[Side])WheelLocalAngle[Side]=WheelAngle+Bounded;
    }
    if(Regripping==INDEX_NONE)
    {
        const double Left=FMath::Abs(SeatAngle[0]-Home[0]),Right=FMath::Abs(SeatAngle[1]-Home[1]);const int32 Pick=Left>=Right?0:1;
        if(FMath::Max(Left,Right)>30){Regripping=Pick;RegripAge=0;RegripFrom=float(SeatAngle[Pick]);++RegripCount;}
    }
    double Lift[2]={0,0},Release[2]={0,0};
    if(Regripping!=INDEX_NONE)
    {
        const int32 Side=Regripping;RegripAge+=Dt;const double T=FMath::Clamp(double(RegripAge)/.30,0.,1.);const double Ease=T*T*(3.-2.*T);
        SeatAngle[Side]=FMath::Lerp(double(RegripFrom),Home[Side],Ease);WheelLocalAngle[Side]=WheelAngle+SeatAngle[Side];Lift[Side]=4.*FMath::Sin(T*PI);Release[Side]=FMath::Sin(T*PI);
        if(T>=1)Regripping=INDEX_NONE;
    }
    FQuat WristTargetWorld[2];
    auto GripTarget=[&](int32 Side,double Seat,double LiftAmount,FQuat& Rotation,FVector& Contact)
    {
        const double LocalAngle=WheelAngle+Seat;
        const double Theta=FMath::DegreesToRadians(LocalAngle);const FVector Radial(0,FMath::Sin(Theta),FMath::Cos(Theta));
        const FVector Axis=WheelWorld.GetUnitAxis(EAxis::X);const FVector Tangent=WheelWorld.TransformVectorNoScale(FVector::CrossProduct(FVector::ForwardVector,Radial)).GetSafeNormal();
        const FVector ThumbDirection=Tangent*(Side==0?-1.f:1.f);Rotation=(FRotationMatrix::MakeFromXY(Axis,ThumbDirection).ToQuat()*PalmFrame[Side].Inverse()).GetNormalized();
        Contact=WheelWorld.TransformPosition(RimPoint(LocalAngle,LiftAmount));
    };
    for(int32 Side=0;Side<2;++Side)GripTarget(Side,SeatAngle[Side],Lift[Side],WristTargetWorld[Side],TargetContactWorld[Side]);
    // Fit one fixed modest seated shoulder lean from actual limb reach. Never
    // stretch bones or animate the camera/seat. 15cm is a rejection bound, not
    // permission to invent a body proportion when the fit fails.
    if(!bFitted)
    {
        bool Found=false;for(float Lean=0;Lean<=15.001f;Lean+=.5f)
        {
            const FTransform Mount=MountFor(Lean);bool Fits=true;
            for(int32 Side=0;Side<2;++Side)
            {
                const FTransform U=Display->GetSocketTransform(Upper[Side],RTS_Component),L=Display->GetSocketTransform(Lower[Side],RTS_Component),H=Display->GetSocketTransform(Hand[Side],RTS_Component);
                const double Length=(FVector::Distance(U.GetLocation(),L.GetLocation())+FVector::Distance(L.GetLocation(),H.GetLocation()))*Scale.X;
                const FVector Target=TargetContactWorld[Side]-WristTargetWorld[Side].RotateVector(PalmContact[Side]*Scale);
                Fits&=FVector::Distance(Mount.TransformPosition(U.GetLocation()),Target)<Length*.97;
            }
            if(Fits){FittedShoulderForwardCm=Lean;Found=true;break;}
        }
        if(!Found)return Fail(TEXT("Original arms cannot reach measured wheel within bounded seated shoulder fit; no stretching fallback"));bFitted=true;
    }
    const FTransform Mount=MountFor(FittedShoulderForwardCm);Display->SetWorldTransform(Mount);
    // The rim sector is limited by this frame's real arm reach, not a fixed
    // angular limit that can send the wrist beyond an unstretched arm. Keep the
    // supporting hand on the rim; release/regrip before running out of reach.
    for(int32 Side=0;Side<2;++Side)
    {
        const FVector Shoulder=Display->GetSocketTransform(Upper[Side],RTS_Component).GetLocation();
        const FVector Elbow=Display->GetSocketTransform(Lower[Side],RTS_Component).GetLocation();
        const FVector Wrist=Display->GetSocketTransform(Hand[Side],RTS_Component).GetLocation();
        const double Reach=(FVector::Distance(Shoulder,Elbow)+FVector::Distance(Elbow,Wrist))*.985;
        auto Reachable=[&](double Seat)
        {
            FQuat Rotation;FVector Contact;GripTarget(Side,Seat,Lift[Side],Rotation,Contact);
            const FVector Target=Mount.InverseTransformPosition(Contact-Rotation.RotateVector(PalmContact[Side]*Scale));
            return FVector::Distance(Shoulder,Target)<=Reach;
        };
        if(Reachable(SeatAngle[Side]))continue;
        if(!Reachable(Home[Side]))return Fail(TEXT("Seated home grip outside actual unstretched arm reach"));
        double Inside=Home[Side],Outside=SeatAngle[Side];
        for(int32 I=0;I<10;++I){const double Mid=(Inside+Outside)*.5;if(Reachable(Mid))Inside=Mid;else Outside=Mid;}
        SeatAngle[Side]=Inside;WheelLocalAngle[Side]=WheelAngle+Inside;Sliding[Side]=true;
        GripTarget(Side,Inside,Lift[Side],WristTargetWorld[Side],TargetContactWorld[Side]);
        if(Regripping==INDEX_NONE){Regripping=Side;RegripAge=0;RegripFrom=float(Inside);++RegripCount;}
    }
    for(int32 Side=0;Side<2;++Side)
    {
        const int32 UI=Display->GetBoneIndex(Upper[Side]),LI=Display->GetBoneIndex(Lower[Side]),HI=Display->GetBoneIndex(Hand[Side]);const FName Parent=Display->GetParentBone(Upper[Side]);
        if(!Display->BoneSpaceTransforms.IsValidIndex(UI)||!Display->BoneSpaceTransforms.IsValidIndex(LI)||!Display->BoneSpaceTransforms.IsValidIndex(HI)||Parent==NAME_None)return Fail(TEXT("Runtime three-bone arm chain missing"));
        FTransform U=Display->GetSocketTransform(Upper[Side],RTS_Component),L=Display->GetSocketTransform(Lower[Side],RTS_Component),H=Display->GetSocketTransform(Hand[Side],RTS_Component);
        const FVector OriginalRoot=U.GetLocation();UpperLength[Side]=FVector::Distance(U.GetLocation(),L.GetLocation());LowerLength[Side]=FVector::Distance(L.GetLocation(),H.GetLocation());
        const FVector WristWorldTarget=TargetContactWorld[Side]-WristTargetWorld[Side].RotateVector(PalmContact[Side]*Scale);
        const FVector Target=Mount.InverseTransformPosition(WristWorldTarget);const FVector Shoulder=Mount.TransformPosition(U.GetLocation());
        const FVector PoleWorld=Shoulder+(-CarUp+CarRight*(Side==0?-.65f:.65f)-CarForward*.20f).GetSafeNormal()*float(UpperLength[Side]*Scale.X);
        AnimationCore::SolveTwoBoneIK(U,L,H,Mount.InverseTransformPosition(PoleWorld),Target,false,1.0,1.0);
        H.SetRotation((Mount.GetRotation().Inverse()*WristTargetWorld[Side]).GetNormalized());
        ArmLengthError[Side]=FMath::Max(FMath::Abs(FVector::Distance(U.GetLocation(),L.GetLocation())-UpperLength[Side]),FMath::Abs(FVector::Distance(L.GetLocation(),H.GetLocation())-LowerLength[Side]));
        if(FVector::Distance(OriginalRoot,U.GetLocation())>.001||ArmLengthError[Side]>.001||FVector::Distance(H.GetLocation(),Target)>.4)
            return Fail(FString::Printf(TEXT("IK reach invalid side=%d wheel=%.3f seat=%.3f lean=%.3f rootShift=%.6f lengthError=%.6f residual=%.6f reach=%.3f upper=%.3f lower=%.3f"),Side,WheelAngle,SeatAngle[Side],FittedShoulderForwardCm,FVector::Distance(OriginalRoot,U.GetLocation()),ArmLengthError[Side],FVector::Distance(H.GetLocation(),Target),FVector::Distance(OriginalRoot,Target),UpperLength[Side],LowerLength[Side]));
        Display->BoneSpaceTransforms[UI]=U.GetRelativeTransform(Display->GetSocketTransform(Parent,RTS_Component));Display->BoneSpaceTransforms[LI]=L.GetRelativeTransform(U);Display->BoneSpaceTransforms[HI]=H.GetRelativeTransform(L);
        for(int32 I=Side*15;I<Side*15+15;++I)
        {FTransform& Local=Display->BoneSpaceTransforms[FingerIndices[I]];Local.SetRotation(FQuat::Slerp(ArmsOverride->GetRefSkeleton().GetRefBonePose()[FingerIndices[I]].GetRotation(),FingerRotations[I],float(.68*(1.-Release[Side]))).GetNormalized());}
    }
    Display->RefreshBoneTransforms();
    for(int32 Side=0;Side<2;++Side)
    {
        ShoulderWorld[Side]=Display->GetSocketLocation(Upper[Side]);ElbowWorld[Side]=Display->GetSocketLocation(Lower[Side]);WristWorld[Side]=Display->GetSocketLocation(Hand[Side]);
        const FTransform ActualHand=Display->GetSocketTransform(Hand[Side],RTS_World);ContactWorld[Side]=ActualHand.TransformPosition(PalmContact[Side]);ContactError[Side]=FVector::Distance(ContactWorld[Side],TargetContactWorld[Side]);
        if(!FMath::IsFinite(ContactError[Side])||ContactError[Side]>.5)return Fail(TEXT("Final evaluated palm proxy missed wheel target"));
    }
    if(FVector::Distance(ContactWorld[0],ContactWorld[1])<15)return Fail(TEXT("Hand contact proxies too close; no intersecting-center fallback"));return true;
}
FString UHCM5VS2DrivingHandsComponent::GetDrivingHandsDiagnostics() const
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("active"),bActive);J->SetBoolField(TEXT("allocated"),Display!=nullptr);J->SetBoolField(TEXT("source_geometry_ready"),bGeometryReady);J->SetBoolField(TEXT("seated_fit_ready"),bFitted);J->SetStringField(TEXT("failure"),Failure);J->SetStringField(TEXT("vehicle"),GetPathNameSafe(Vehicle));J->SetStringField(TEXT("driver"),GetPathNameSafe(Driver));J->SetStringField(TEXT("arms"),GetPathNameSafe(ArmsOverride));J->SetStringField(TEXT("finger_pose"),GetPathNameSafe(GripFingerPose));J->SetNumberField(TEXT("pose_frame"),double(PoseFrame));J->SetNumberField(TEXT("actual_wheel_degrees"),WheelAngle);J->SetNumberField(TEXT("seated_shoulder_fit_cm"),FittedShoulderForwardCm);J->SetNumberField(TEXT("regripping_hand"),Regripping);J->SetNumberField(TEXT("regrip_count"),RegripCount);J->SetNumberField(TEXT("max_simultaneous_regripping_hands"),Regripping==INDEX_NONE?0:1);
    if(Wheel)J->SetStringField(TEXT("actual_wheel_world"),Wheel->GetComponentTransform().ToString());
    if(Display){J->SetBoolField(TEXT("visible"),Display->IsVisible()&&!Display->bHiddenInGame);J->SetBoolField(TEXT("only_owner_see"),Display->bOnlyOwnerSee);J->SetStringField(TEXT("display_world"),Display->GetComponentTransform().ToString());TArray<TSharedPtr<FJsonValue>> M;for(int32 I=0;I<Display->GetNumMaterials();++I)M.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Display->GetMaterial(I))));J->SetArrayField(TEXT("actual_materials"),M);}
    TArray<TSharedPtr<FJsonValue>> H;for(int32 Side=0;Side<2;++Side){auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("side"),Side);R->SetStringField(TEXT("state"),Regripping==Side?TEXT("Regrip"):Sliding[Side]?TEXT("SupportingSlide"):TEXT("Grip"));R->SetNumberField(TEXT("seat_angle_degrees"),SeatAngle[Side]);R->SetNumberField(TEXT("wheel_local_grip_angle_degrees"),WheelLocalAngle[Side]);R->SetArrayField(TEXT("shoulder_world_cm"),XYZ(ShoulderWorld[Side]));R->SetArrayField(TEXT("elbow_world_cm"),XYZ(ElbowWorld[Side]));R->SetArrayField(TEXT("wrist_world_cm"),XYZ(WristWorld[Side]));R->SetArrayField(TEXT("palm_contact_proxy_world_cm"),XYZ(ContactWorld[Side]));R->SetArrayField(TEXT("wheel_surface_target_world_cm"),XYZ(TargetContactWorld[Side]));R->SetNumberField(TEXT("palm_proxy_error_cm"),ContactError[Side]);R->SetNumberField(TEXT("arm_length_error_mesh_cm"),ArmLengthError[Side]);H.Add(MakeShared<FJsonValueObject>(R));}J->SetArrayField(TEXT("hands"),H);J->SetStringField(TEXT("surface_contact_art"),TEXT("USER_REVIEW; proxy/IK success is not proof of finger skin surface contact or naturalness"));J->SetBoolField(TEXT("vehicle_input_physics_camera_modified"),false);return JSON(J);
}
void UHCM5VS2DrivingHandsComponent::EndPlay(const EEndPlayReason::Type Reason)
{if(Display){Display->DestroyComponent();Display=nullptr;}Driver=nullptr;Wheel=nullptr;Vehicle=nullptr;Super::EndPlay(Reason);}
FString UHCM5VS2DrivingHandsEditor::ProbeDriverSource(AHCM1Character* Character,USkeletalMesh* Arms,UAnimSequence* FingerPose)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));TArray<int32> Indices;TArray<FQuat> Rotations;FQuat Palm[2];FVector Contact[2];FString Error;
    if(SourceGeometry(Character,Arms,FingerPose,Indices,Rotations,Palm,Contact,J,Error))J->SetStringField(TEXT("status"),TEXT("PASS"));else J->SetStringField(TEXT("error"),Error);J->SetBoolField(TEXT("assets_mutated"),false);return JSON(J);
}
FString UHCM5VS2DrivingHandsEditor::ConfigurePrivateVehicle(UBlueprint* Blueprint,USkeletalMesh* Arms,UAnimSequence* FingerPose)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("FAIL"));J->SetBoolField(TEXT("saved_by_helper"),false);
#if WITH_EDITOR
    if(!Blueprint||!PrivateVehicle(Blueprint)||Blueprint->ParentClass!=AHCM1Vehicle::StaticClass()||!Blueprint->SimpleConstructionScript||!Arms||!FingerPose){J->SetStringField(TEXT("error"),TEXT("New exact private direct native vehicle child and validated source assets required"));return JSON(J);}
    for(auto* N:Blueprint->SimpleConstructionScript->GetAllNodes())if(N->ComponentClass==UHCM5VS2DrivingHandsComponent::StaticClass()||N->GetVariableName()==TEXT("VS2DrivingHands")){J->SetStringField(TEXT("error"),TEXT("Existing driving hands; refuse duplicate binding"));return JSON(J);}
    Blueprint->Modify();Blueprint->SimpleConstructionScript->Modify();auto* Node=Blueprint->SimpleConstructionScript->CreateNode(UHCM5VS2DrivingHandsComponent::StaticClass(),TEXT("VS2DrivingHands"));auto* C=Node?Cast<UHCM5VS2DrivingHandsComponent>(Node->ComponentTemplate):nullptr;if(!C)return JSON(J);
    C->ArmsOverride=Arms;C->GripFingerPose=FingerPose;Blueprint->SimpleConstructionScript->AddNode(Node);FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if(Blueprint->Status==BS_Error||!Blueprint->GeneratedClass){J->SetStringField(TEXT("error"),TEXT("Private vehicle Blueprint compile failed"));return JSON(J);}J->SetStringField(TEXT("status"),TEXT("PASS"));J->SetStringField(TEXT("blueprint"),Blueprint->GetPathName());J->SetStringField(TEXT("arms"),GetPathNameSafe(Arms));J->SetStringField(TEXT("finger_pose"),GetPathNameSafe(FingerPose));
#else
    J->SetStringField(TEXT("error"),TEXT("Editor-only private author helper"));
#endif
    return JSON(J);
}
