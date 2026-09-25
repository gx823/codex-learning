#include "HCM5VS2NPCEditor.h"
#include "HCM5VS2NPCFaceComponent.h"
#include "HCM5VS2NPCAnimInstance.h"
#include "Animation/Skeleton.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimPose.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Animation/BlendSpace1D.h"
#include "Engine/SkeletalMesh.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LookAt.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Math/RotationMatrix.h"
#include "Rendering/SkeletalMeshModel.h"

namespace
{
bool Owned(const UObject* Asset)
{ return Asset && Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/")); }
FString Finish(const TSharedPtr<FJsonObject>& R, const FString& Error = FString())
{
    R->SetStringField(TEXT("status"), Error.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    if (!Error.IsEmpty()) R->SetStringField(TEXT("error"), Error);
    R->SetStringField(TEXT("runtime_visual_physics"), TEXT("NOT_RUN"));
    R->SetBoolField(TEXT("saved_by_helper"), false);
    FString Out; FJsonSerializer::Serialize(R.ToSharedRef(), TJsonWriterFactory<>::Create(&Out)); return Out;
}
template<class T> T* NewNode(UEdGraph* Graph, FName Name, int32 X, int32 Y = 0)
{
    T* N = NewObject<T>(Graph, Name, RF_Transactional);
    Graph->AddNode(N, false, false); N->CreateNewGuid(); N->PostPlacedNewNode();
    N->NodePosX = X; N->NodePosY = Y; return N;
}
UEdGraphPin* Output(UEdGraphNode* N)
{
    UEdGraphPin* Found = nullptr;
    for (auto* P : N->Pins) if (P->Direction == EGPD_Output) { if (Found) return nullptr; Found = P; }
    return Found;
}
bool Expose(UAnimGraphNode_Base* N, FName Name)
{
    if (N->FindPin(Name)) return true;
    for (int32 I = 0; I < N->ShowPinForProperties.Num(); ++I)
        if (N->ShowPinForProperties[I].PropertyName == Name)
        { N->SetPinVisibility(true, I); return N->FindPin(Name) != nullptr; }
    return false;
}
template<class T> T* FreshAsset(const FString& Path)
{
    if (!Path.StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/")) || !FPackageName::IsValidLongPackageName(Path)
        || FPackageName::DoesPackageExist(Path) || FindObject<UPackage>(nullptr, *Path)) return nullptr;
    UPackage* Package = CreatePackage(*Path);
    T* Asset = NewObject<T>(Package, *FPackageName::GetLongPackageAssetName(Path), RF_Public | RF_Standalone | RF_Transactional);
    FAssetRegistryModule::AssetCreated(Asset); Asset->MarkPackageDirty(); return Asset;
}
}
#endif

bool UHCM5VS2NPCEditor::CopyNPCSlots(USkeleton* Source, USkeleton* Target)
{
#if WITH_EDITOR
    if (!Source || !Owned(Target) || Source == Target) return false;
    Target->Modify();
    for (const FAnimSlotGroup& Group : Source->GetSlotGroups())
        for (FName Slot : Group.SlotNames) { Target->RegisterSlotNode(Slot); Target->SetSlotGroupName(Slot, Group.GroupName); }
    Target->MarkPackageDirty(); return Target->ContainsSlotName(TEXT("FullBody"));
#else
    return false;
#endif
}

FString UHCM5VS2NPCEditor::RepairNPCHorizontalRootTravel(UAnimSequence* Source, UAnimSequence* Target,
    USkeletalMesh* TargetMesh, UIKRetargeter* Retargeter, bool bApply)
{
#if WITH_EDITOR
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("applied"), false);
    const TSet<FString> AllowedSources = {
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"),
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"),
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd")};
    if (!Source || !Target || !TargetMesh || !Retargeter || !Source->GetSkeleton()
        || !TargetMesh->GetSkeleton() || Target->GetSkeleton() != TargetMesh->GetSkeleton()
        || !AllowedSources.Contains(Source->GetOutermost()->GetName())
        || Source->GetSkeleton()->GetPathName() != TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin")
        || Source->GetSkeleton() == Target->GetSkeleton()) return Finish(R, TEXT("Only exact original Quinn/approved official eight-cast locomotion pairs are allowed"));
    const FString TargetPackage = Target->GetOutermost()->GetName();
    const FString Runtime = FPackageName::GetLongPackagePath(FPackageName::GetLongPackagePath(TargetPackage));
    FString Letter;
    for (const TCHAR* Candidate : {TEXT("Q"), TEXT("R"), TEXT("J"), TEXT("T"), TEXT("U"), TEXT("V"), TEXT("W"), TEXT("X")})
    {
        const FString Prefix = FString(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_")) + Candidate + TEXT("/Runtime_");
        if (Runtime.StartsWith(Prefix) && Runtime.Len() == Prefix.Len() + 10)
        {
            bool Hex = true;
            for (TCHAR C : Runtime.Right(10)) Hex &= FChar::IsHexDigit(C);
            if (Hex) Letter = Candidate;
        }
    }
    const FString ImportRoot = FString(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_")) + Letter + TEXT("/Source_");
    if (Letter.IsEmpty() || TargetPackage != Runtime + TEXT("/Animation/") + Source->GetName() + TEXT("_NPC") + Letter
        || Retargeter->GetOutermost()->GetName() != Runtime + TEXT("/RTG_QuinnToVRoid")
        || !TargetMesh->GetOutermost()->GetName().StartsWith(ImportRoot)
        || !Target->GetSkeleton()->GetOutermost()->GetName().StartsWith(ImportRoot)
        || Source->GetAdditiveAnimType() != AAT_None || Target->GetAdditiveAnimType() != AAT_None
        || Target->bEnableRootMotion || Target->bForceRootLock)
        return Finish(R, TEXT("Confined runtime paths, independent imported skeleton and non-additive unlocked target required"));
    auto XYZ = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
    { return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; };
    R->SetStringField(TEXT("source"), Source->GetPathName()); R->SetStringField(TEXT("target"), Target->GetPathName());
    R->SetStringField(TEXT("retargeter"), Retargeter->GetPathName());
    R->SetStringField(TEXT("method"), TEXT("Full native batch replay versus identical replay with only source-root XY displacement removed from source component poses; edit only the measured local translation contribution"));
    USkeletalMesh* SourceMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/HarborCity/M3/Appearance/SK_M3_Quinn_HarborNavy.SK_M3_Quinn_HarborNavy"));
    const IAnimationDataModel* SM = Source->GetDataModel(); IAnimationDataModel* TM = Target->GetDataModel();
    if (!SourceMesh || SourceMesh->GetSkeleton() != Source->GetSkeleton() || !SM || !TM
        || TM->GetNumberOfKeys() < 2 || TM->GetNumberOfKeys() > 6000 || TM->GetNumBoneTracks() > 1024
        || Source->GetNumberOfSampledKeys() != TM->GetNumberOfKeys() || SM->GetFrameRate() != TM->GetFrameRate()
        || !FMath::IsNearlyEqual(SM->GetPlayLength(), TM->GetPlayLength(), .0001))
        return Finish(R, TEXT("Native source mesh/data model/sample-rate/duration bounds failed"));
    const FReferenceSkeleton& SR = SourceMesh->GetRefSkeleton();
    const FReferenceSkeleton& TR = TargetMesh->GetRefSkeleton();
    const FName Hips(TEXT("J_Bip_C_Hips")); const int32 HipsIndex = TR.FindBoneIndex(Hips);
    if (SR.FindBoneIndex(TEXT("root")) != 0 || TR.FindBoneIndex(TEXT("Root")) != 0
        || HipsIndex == INDEX_NONE || TR.GetParentIndex(HipsIndex) != 0)
        return Finish(R, TEXT("Expected measured Q/R Root -> J_Bip_C_Hips hierarchy; no guessed hips lock"));
    TArray<FName> Names; TM->GetBoneTrackNames(Names);
    TMap<FName, TArray<FTransform>> Before;
    for (FName Name : Names)
    {
        TM->GetBoneTrackTransforms(Name, Before.Add(Name));
        if (TR.FindBoneIndex(Name) == INDEX_NONE || Before[Name].Num() != TM->GetNumberOfKeys())
            return Finish(R, TEXT("Supported GetBoneTrackTransforms did not return all target frames"));
        for (const FTransform& T : Before[Name]) if (T.ContainsNaN()) return Finish(R, TEXT("Nonfinite original track"));
    }
    if (!Before.Contains(Hips) || !Before.Contains(TEXT("Root"))) return Finish(R, TEXT("Missing measured root/hips tracks"));
    FRetargetInitParameters Init; Init.SourceSkeletalMesh = SourceMesh; Init.TargetSkeletalMesh = TargetMesh; Init.RetargeterAsset = Retargeter;
    FIKRetargetProcessor OriginalProcessor, InPlaceProcessor;
    OriginalProcessor.Initialize(Init); InPlaceProcessor.Initialize(Init);
    const auto* Pelvis = OriginalProcessor.GetFirstRetargetOpOfType<FIKRetargetPelvisMotionOp>();
    const auto* FK = OriginalProcessor.GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>();
    if (!OriginalProcessor.IsInitialized() || !InPlaceProcessor.IsInitialized() || !Pelvis || !FK
        || OriginalProcessor.GetRetargetOps().Num() != 2
        || Pelvis->Settings.SourcePelvisBone.BoneName != TEXT("pelvis") || Pelvis->Settings.TargetPelvisBone.BoneName != Hips)
        return Finish(R, TEXT("Only initialized original pelvis+FK two-op retargeter is supported"));
    FAnimPoseEvaluationOptions Evaluation; Evaluation.OptionalSkeletalMesh = SourceMesh;
    Evaluation.bExtractRootMotion = false; Evaluation.bIncorporateRootMotionIntoPose = true;
    FRetargetProfile Profile; Profile.FillProfileWithAssetSettings(Retargeter);
    OriginalProcessor.OnPlaybackReset(); InPlaceProcessor.OnPlaybackReset();
    TMap<FName, TArray<FVector>> Contributions;
    for (FName Name : Names) Contributions.Add(Name).SetNum(TM->GetNumberOfKeys());
    FVector FirstSourceRoot = FVector::ZeroVector;
    double OriginalError = 0, InPlaceError = 0, RotationError = 0, ScaleError = 0;
    double SourceTravel = 0, NonHorizontalContribution = 0, OtherTrackContribution = 0;
    double HipsBeforeRadius = 0, HipsAfterRadius = 0, RootTravel = 0;
    TArray<TSharedPtr<FJsonValue>> Samples;
    for (int32 Frame = 0; Frame < TM->GetNumberOfKeys(); ++Frame)
    {
        FAnimPose SourcePose; UAnimPoseExtensions::GetAnimPoseAtFrame(Source, Frame, Evaluation, SourcePose);
        if (!SourcePose.IsValid()) return Finish(R, TEXT("Native source pose evaluation failed"));
        TArray<FTransform> Input; Input.SetNum(SR.GetNum());
        for (int32 Bone = 0; Bone < SR.GetNum(); ++Bone)
        {
            Input[Bone] = UAnimPoseExtensions::GetBonePose(SourcePose, SR.GetBoneName(Bone), EAnimPoseSpaces::World);
            Input[Bone].SetScale3D(FVector::OneVector);
            if (Input[Bone].ContainsNaN()) return Finish(R, TEXT("Nonfinite source evaluation"));
        }
        if (Frame == 0) FirstSourceRoot = Input[0].GetTranslation();
        FVector Delta = Input[0].GetTranslation() - FirstSourceRoot; Delta.Z = 0;
        SourceTravel = FMath::Max(SourceTravel, Delta.Size2D());
        TArray<FTransform> WithoutTravel = Input;
        for (FTransform& T : WithoutTravel) T.AddToTranslation(-Delta);
        OriginalProcessor.ApplySourceScaleToPose(Input); InPlaceProcessor.ApplySourceScaleToPose(WithoutTravel);
        const float Time = Source->GetTimeAtFrame(Frame);
        OriginalProcessor.UpdateOpsFromAnimSequence(Source, Time); InPlaceProcessor.UpdateOpsFromAnimSequence(Source, Time);
        FRetargetRunParameters Params; Params.Profile = &Profile;
        Params.DeltaTime = Frame ? Time - Source->GetTimeAtFrame(Frame - 1) : Time;
        Params.SourceGlobalPose = &Input;
        const TArray<FTransform> Original = OriginalProcessor.RunRetargeter(Params);
        Params.SourceGlobalPose = &WithoutTravel;
        const TArray<FTransform> InPlace = InPlaceProcessor.RunRetargeter(Params);
        if (Original.Num() != TR.GetNum() || InPlace.Num() != TR.GetNum()) return Finish(R, TEXT("Native replay output incomplete"));
        for (FName Name : Names)
        {
            const int32 Bone = TR.FindBoneIndex(Name), Parent = TR.GetParentIndex(Bone);
            const FTransform A = Parent == INDEX_NONE ? Original[Bone] : Original[Bone].GetRelativeTransform(Original[Parent]);
            const FTransform B = Parent == INDEX_NONE ? InPlace[Bone] : InPlace[Bone].GetRelativeTransform(InPlace[Parent]);
            const FTransform& Actual = Before[Name][Frame];
            if (A.ContainsNaN() || B.ContainsNaN()) return Finish(R, TEXT("Nonfinite native replay"));
            OriginalError = FMath::Max(OriginalError, (Actual.GetTranslation() - A.GetTranslation()).Size());
            InPlaceError = FMath::Max(InPlaceError, (Actual.GetTranslation() - B.GetTranslation()).Size());
            RotationError = FMath::Max(RotationError, FMath::RadiansToDegrees(Actual.GetRotation().AngularDistance(A.GetRotation())));
            RotationError = FMath::Max(RotationError, FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())));
            ScaleError = FMath::Max(ScaleError, (Actual.GetScale3D() - A.GetScale3D()).Size());
            ScaleError = FMath::Max(ScaleError, (A.GetScale3D() - B.GetScale3D()).Size());
            const FVector Contribution = A.GetTranslation() - B.GetTranslation();
            Contributions[Name][Frame] = Contribution;
            NonHorizontalContribution = FMath::Max(NonHorizontalContribution, FMath::Abs(Contribution.Z));
            if (Name != Hips && Name != TEXT("Root")) OtherTrackContribution = FMath::Max(OtherTrackContribution, Contribution.Size());
        }
        HipsBeforeRadius = FMath::Max(HipsBeforeRadius, Original[HipsIndex].GetTranslation().Size2D());
        HipsAfterRadius = FMath::Max(HipsAfterRadius, InPlace[HipsIndex].GetTranslation().Size2D());
        RootTravel = FMath::Max(RootTravel, (Original[0].GetTranslation() - TR.GetRefBonePose()[0].GetTranslation()).Size2D());
        if (Frame == 0 || Frame == TM->GetNumberOfKeys() - 1 || Frame % FMath::Max(1, (TM->GetNumberOfKeys() - 1) / 8) == 0)
        {
            auto S = MakeShared<FJsonObject>(); S->SetNumberField(TEXT("frame"), Frame); S->SetNumberField(TEXT("seconds"), Time);
            S->SetArrayField(TEXT("source_root_xy_delta_cm"), XYZ(Delta));
            S->SetArrayField(TEXT("native_target_root_cm"), XYZ(Original[0].GetTranslation()));
            S->SetArrayField(TEXT("native_hips_before_cm"), XYZ(Original[HipsIndex].GetTranslation()));
            S->SetArrayField(TEXT("native_hips_in_place_cm"), XYZ(InPlace[HipsIndex].GetTranslation()));
            Samples.Add(MakeShared<FJsonValueObject>(S));
        }
    }
    R->SetNumberField(TEXT("frames_checked"), TM->GetNumberOfKeys()); R->SetNumberField(TEXT("tracks_checked"), Names.Num());
    R->SetNumberField(TEXT("original_batch_position_error_max_cm"), OriginalError);
    R->SetNumberField(TEXT("in_place_batch_position_error_max_cm"), InPlaceError);
    R->SetNumberField(TEXT("replay_rotation_error_max_degrees"), RotationError); R->SetNumberField(TEXT("replay_scale_error_max"), ScaleError);
    R->SetNumberField(TEXT("source_root_xy_travel_max_cm"), SourceTravel);
    R->SetNumberField(TEXT("target_root_xy_travel_max_cm"), RootTravel);
    R->SetNumberField(TEXT("hips_component_xy_radius_before_cm"), HipsBeforeRadius);
    R->SetNumberField(TEXT("hips_component_xy_radius_in_place_cm"), HipsAfterRadius);
    R->SetNumberField(TEXT("non_horizontal_contribution_max_cm"), NonHorizontalContribution);
    R->SetNumberField(TEXT("non_root_hips_local_contribution_max_cm"), OtherTrackContribution);
    R->SetArrayField(TEXT("samples"), Samples);
    if (RotationError > .01 || ScaleError > .00001 || NonHorizontalContribution > .00001 || OtherTrackContribution > .001
        || FMath::Min(OriginalError, InPlaceError) > .05)
        return Finish(R, TEXT("Full-frame replay did not isolate a horizontal root/hips-only contribution; no mutation"));
    const bool AlreadyInPlace = OriginalError > .05 && InPlaceError <= .05;
    const bool NeedsRepair = SourceTravel > .1 && !AlreadyInPlace;
    R->SetBoolField(TEXT("already_in_place"), AlreadyInPlace); R->SetBoolField(TEXT("needs_repair"), NeedsRepair);
    TArray<FName> ChangedNames;
    for (FName Name : {FName(TEXT("Root")), Hips})
        for (const FVector& Delta : Contributions[Name])
            if (Delta.Size2D() > .001) { ChangedNames.Add(Name); break; }
    TArray<TSharedPtr<FJsonValue>> ChangedRows;
    for (FName Name : ChangedNames) ChangedRows.Add(MakeShared<FJsonValueString>(Name.ToString()));
    R->SetArrayField(TEXT("measured_translation_tracks"), ChangedRows);
    if (NeedsRepair && ChangedNames.IsEmpty()) return Finish(R, TEXT("Source travel has no measured target track; no mutation"));
    if (bApply && NeedsRepair)
    {
        Target->Modify(); auto& Controller = Target->GetController();
        Controller.OpenBracket(FText::FromString(TEXT("Q/R in-place locomotion: remove measured source-root XY only")), false);
        bool Updated = true;
        for (FName Name : ChangedNames)
        {
            TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
            for (int32 K = 0; K < Before[Name].Num(); ++K)
            {
                const FTransform& T = Before[Name][K]; FVector P = T.GetTranslation();
                P.X -= Contributions[Name][K].X; P.Y -= Contributions[Name][K].Y;
                Positions.Add(FVector3f(P)); Rotations.Add(FQuat4f(T.GetRotation())); Scales.Add(FVector3f(T.GetScale3D()));
            }
            Updated &= Controller.SetBoneTrackKeys(Name, Positions, Rotations, Scales, false);
        }
        Controller.CloseBracket(false);
        if (!Updated) return Finish(R, TEXT("Native track update failed; helper has not saved"));
        TArray<FName> AfterNames; TM->GetBoneTrackNames(AfterNames);
        if (AfterNames != Names) return Finish(R, TEXT("Track membership/order changed; helper has not saved"));
        double ReadbackPositionError = 0, ReadbackRotationError = 0;
        for (FName Name : Names)
        {
            TArray<FTransform> After; TM->GetBoneTrackTransforms(Name, After);
            if (After.Num() != Before[Name].Num()) return Finish(R, TEXT("Track key count changed; helper has not saved"));
            for (int32 K = 0; K < After.Num(); ++K)
            {
                const FTransform& A = Before[Name][K]; const FTransform& B = After[K];
                if (!ChangedNames.Contains(Name))
                { if (!A.Equals(B, 0.)) return Finish(R, TEXT("Unselected local track changed; helper has not saved")); continue; }
                if (A.GetTranslation().Z != B.GetTranslation().Z || A.GetScale3D() != B.GetScale3D())
                    return Finish(R, TEXT("Selected track Z/scale changed; helper has not saved"));
                FVector Expected = A.GetTranslation() - Contributions[Name][K]; Expected.Z = A.GetTranslation().Z;
                ReadbackPositionError = FMath::Max(ReadbackPositionError, (B.GetTranslation() - Expected).Size());
                ReadbackRotationError = FMath::Max(ReadbackRotationError, FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())));
            }
        }
        R->SetNumberField(TEXT("readback_xy_error_max_cm"), ReadbackPositionError);
        R->SetNumberField(TEXT("readback_rotation_error_max_degrees"), ReadbackRotationError);
        if (ReadbackPositionError > .0001 || ReadbackRotationError > .0001)
            return Finish(R, TEXT("Track readback exceeded roundtrip tolerance; helper has not saved"));
        Target->MarkPackageDirty(); R->SetBoolField(TEXT("applied"), true);
        R->SetBoolField(TEXT("all_unselected_tracks_exactly_preserved"), true);
        R->SetBoolField(TEXT("selected_tracks_z_scale_exactly_preserved"), true);
    }
    return Finish(R);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2NPCEditor::InspectOrRepairNPCPhysics(UHCM5VS2NPCProfile* Profile, UPhysicsAsset* Physics, bool bApply)
{
#if WITH_EDITOR
    auto R = MakeShared<FJsonObject>(); R->SetNumberField(TEXT("schema"), 3); R->SetBoolField(TEXT("applied"), false);
    USkeletalMesh* Mesh = Profile ? Profile->Mesh.Get() : nullptr;
    if (!Owned(Profile) || !Owned(Physics) || !Owned(Mesh) || !Mesh->GetSkeleton())
        return Finish(R, TEXT("Owned approved official cast profile, mesh and physics asset required"));
    const FString Runtime = FPackageName::GetLongPackagePath(Profile->GetOutermost()->GetName());
    bool Scope = false;
    for (const TCHAR* Letter : {TEXT("Q"), TEXT("R"), TEXT("J"), TEXT("T"), TEXT("U"), TEXT("V"), TEXT("W"), TEXT("X")})
    {
        const FString Prefix = FString(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_")) + Letter + TEXT("/Runtime_");
        bool Hex = Runtime.StartsWith(Prefix) && Runtime.Len() == Prefix.Len() + 10;
        for (TCHAR C : Runtime.Right(10)) Hex &= FChar::IsHexDigit(C);
        Scope |= Hex && Mesh->GetOutermost()->GetName().StartsWith(FString(TEXT("/Game/HarborCity/M5VS2/NPC/AvatarSample_")) + Letter + TEXT("/Source_"));
    }
    if (!Scope || Profile->GetName() != TEXT("DA_NPCProfile")
        || Physics->GetOutermost()->GetName() != Runtime + TEXT("/PHYS_NPC_Humanoid")
        || Physics->SkeletalBodySetups.Num() < 12 || Physics->SkeletalBodySetups.Num() > 22
        || Physics->ConstraintSetup.Num() != Physics->SkeletalBodySetups.Num() - 1)
        return Finish(R, TEXT("Exact approved cast runtime humanoid physics/profile paths and bounded tree required"));
    auto XYZ = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
    { return {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)}; };
    auto TransformJSON = [&XYZ](const FTransform& T)
    {
        auto J = MakeShared<FJsonObject>(); J->SetArrayField(TEXT("translation_cm"), XYZ(T.GetTranslation()));
        const FQuat Q = T.GetRotation(); J->SetArrayField(TEXT("rotation_xyzw"), {MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W)});
        J->SetArrayField(TEXT("scale"), XYZ(T.GetScale3D())); J->SetArrayField(TEXT("twist_axis_x"), XYZ(T.GetUnitAxis(EAxis::X)));
        J->SetArrayField(TEXT("axis_y"), XYZ(T.GetUnitAxis(EAxis::Y))); J->SetArrayField(TEXT("axis_z"), XYZ(T.GetUnitAxis(EAxis::Z))); return J;
    };
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton(); TArray<FTransform> CS; CS.SetNum(Ref.GetNum());
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    { const int32 P = Ref.GetParentIndex(I); CS[I] = P < 0 ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * CS[P]; }
    auto RoleIndex = [&](const TCHAR* Role) { const FName* Bone = Profile->HumanoidBones.Find(FName(Role)); return Bone ? Ref.FindBoneIndex(*Bone) : INDEX_NONE; };
    for (const TCHAR* Role : {TEXT("hips"),TEXT("head"),TEXT("leftUpperLeg"),TEXT("rightUpperLeg"),TEXT("leftLowerLeg"),TEXT("rightLowerLeg"),TEXT("leftFoot"),TEXT("rightFoot"),TEXT("leftToes"),TEXT("rightToes")})
        if (RoleIndex(Role) == INDEX_NONE) return Finish(R, TEXT("Reference pelvis/head/feet/toes must be mapped"));
    const FVector Up = (CS[RoleIndex(TEXT("head"))].GetTranslation() - CS[RoleIndex(TEXT("hips"))].GetTranslation()).GetSafeNormal();
    FVector Forward = CS[RoleIndex(TEXT("leftToes"))].GetTranslation() - CS[RoleIndex(TEXT("leftFoot"))].GetTranslation()
        + CS[RoleIndex(TEXT("rightToes"))].GetTranslation() - CS[RoleIndex(TEXT("rightFoot"))].GetTranslation();
    Forward = (Forward - Up * FVector::DotProduct(Forward, Up)).GetSafeNormal();
    if (Up.IsNearlyZero() || Forward.IsNearlyZero()) return Finish(R, TEXT("Degenerate actual reference anatomy basis"));
    const FVector LeftHip = CS[RoleIndex(TEXT("leftUpperLeg"))].GetTranslation();
    const FVector RightHip = CS[RoleIndex(TEXT("rightUpperLeg"))].GetTranslation();
    const double HipSpan = (LeftHip - RightHip).Size();
    if (!FMath::IsFinite(HipSpan) || HipSpan < 8 || HipSpan > 35) return Finish(R, TEXT("Measured hip-root separation outside Q/R humanoid bounds"));
    R->SetStringField(TEXT("profile"), Profile->GetPathName()); R->SetStringField(TEXT("physics"), Physics->GetPathName());
    R->SetStringField(TEXT("mesh"), Mesh->GetPathName()); R->SetStringField(TEXT("skeleton"), Mesh->GetSkeleton()->GetPathName());
    R->SetArrayField(TEXT("anatomical_up_component"), XYZ(Up)); R->SetArrayField(TEXT("anatomical_forward_from_toes"), XYZ(Forward));
    R->SetNumberField(TEXT("measured_left_right_upper_leg_root_distance_cm"), HipSpan);
    R->SetStringField(TEXT("shape_fit_scope"), TEXT("All humanoid bodies: LOD0 Body_00_SKIN/Face_00_SKIN dominant weights plus actual reference bone segments. Clothing, hair, ears, tail excluded. Missing hidden skin has explicit bounded anatomical fallback. Inner collision envelope, not full surface coverage or visual acceptance."));
    auto Solver = MakeShared<FJsonObject>();
    Solver->SetNumberField(TEXT("position_iterations"), Physics->SolverSettings.PositionIterations);
    Solver->SetNumberField(TEXT("velocity_iterations"), Physics->SolverSettings.VelocityIterations);
    Solver->SetNumberField(TEXT("projection_iterations"), Physics->SolverSettings.ProjectionIterations);
    Solver->SetNumberField(TEXT("cull_distance"), Physics->SolverSettings.CullDistance);
    Solver->SetNumberField(TEXT("max_depenetration_velocity"), Physics->SolverSettings.MaxDepenetrationVelocity);
    Solver->SetNumberField(TEXT("fixed_time_step"), Physics->SolverSettings.FixedTimeStep);
    Solver->SetBoolField(TEXT("use_linear_joint_solver"), Physics->SolverSettings.bUseLinearJointSolver);
    Solver->SetStringField(TEXT("scope"), TEXT("Read only. These PhysicsAsset fields are documented RBAN settings, not proof of the world rigid-body solver's effective iteration counts."));
    R->SetObjectField(TEXT("physics_asset_solver_settings"), Solver);
    R->SetStringField(TEXT("body_iteration_getter_scope"),TEXT("GetPosition/VelocitySolverIterationCount returns -1 when bOverrideIterationCounts is false; -1 means project defaults, not a negative live iteration count. No solver settings are modified."));
    // The two imported source models have real skin sections, independently
    // audited against their local VRMs. Never fit to sleeves, coat, skirt or tail.
    const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
    if (!Model || Model->LODModels.IsEmpty()) return Finish(R, TEXT("LOD0 imported skin data unavailable; no guessed full-body fit"));
    TMap<FString,TArray<FVector>> SkinPoints; TArray<TSharedPtr<FJsonValue>> SkinSections;
    int32 SkinPointCount = 0;
    for (const FSkelMeshSection& Section : Model->LODModels[0].Sections)
    {
        if (!Mesh->GetMaterials().IsValidIndex(Section.MaterialIndex)) return Finish(R,TEXT("Invalid LOD0 section material"));
        const FSkeletalMaterial& Material = Mesh->GetMaterials()[Section.MaterialIndex];
        const FString Slot = Material.MaterialSlotName.ToString() + TEXT("|") + Material.ImportedMaterialSlotName.ToString();
        const bool BodySkin = Slot.Contains(TEXT("Body_00_SKIN")), FaceSkin = Slot.Contains(TEXT("Face_00_SKIN"));
        if (!BodySkin && !FaceSkin) continue;
        auto S = MakeShared<FJsonObject>(); S->SetStringField(TEXT("slot"),Slot); S->SetNumberField(TEXT("material_index"),Section.MaterialIndex);
        S->SetNumberField(TEXT("soft_vertex_count"),Section.SoftVertices.Num()); SkinSections.Add(MakeShared<FJsonValueObject>(S));
        for (const FSoftSkinVertex& V : Section.SoftVertices)
        {
            int32 MaxInfluence = 0;
            for (int32 J=1; J<MAX_TOTAL_INFLUENCES; ++J) if (V.InfluenceWeights[J]>V.InfluenceWeights[MaxInfluence]) MaxInfluence=J;
            if (!V.InfluenceWeights[MaxInfluence] || !Section.BoneMap.IsValidIndex(V.InfluenceBones[MaxInfluence])) continue;
            const int32 SkinBone = Section.BoneMap[V.InfluenceBones[MaxInfluence]];
            if (!CS.IsValidIndex(SkinBone)) return Finish(R,TEXT("Invalid skin bone index"));
            const FName* Mapped = Profile->HumanoidBones.FindKey(Ref.GetBoneName(SkinBone));
            if (!Mapped) continue; // Includes secondary bust/accessory bones; no new body on these chains.
            FString Role = Mapped->ToString();
            if (Role.EndsWith(TEXT("Toes"))) Role=Role.Replace(TEXT("Toes"),TEXT("Foot"));
            if (FaceSkin && Role!=TEXT("head")) continue;
            const FVector P(V.Position); if (P.ContainsNaN()) return Finish(R,TEXT("Nonfinite source skin point"));
            SkinPoints.FindOrAdd(Role).Add(P);
            if (++SkinPointCount>100000) return Finish(R,TEXT("Unexpected skin vertex count"));
        }
    }
    R->SetArrayField(TEXT("skin_sections_read_only"),SkinSections); R->SetNumberField(TEXT("mapped_skin_point_count"),SkinPointCount);
    if (SkinSections.Num()<2 || SkinPointCount<2000) return Finish(R,TEXT("Actual Body/Face skin section coverage missing; no mutation"));
    const FVector Lateral=(LeftHip-RightHip).GetSafeNormal();
    auto Quantile=[](TArray<double> Values, double Fraction) { Values.Sort(); return Values[FMath::Clamp(FMath::FloorToInt(Fraction*(Values.Num()-1)),0,Values.Num()-1)]; };
    auto Bounds=[&](const TArray<FVector>& Points,const FVector& X,const FVector& Y,const FVector& Z,FVector& Lo,FVector& Hi)
    {
        if (Points.Num()<24) return false;
        TArray<double> A,B,C; for (const FVector& P:Points) { A.Add(FVector::DotProduct(P,X)); B.Add(FVector::DotProduct(P,Y)); C.Add(FVector::DotProduct(P,Z)); }
        Lo=FVector(Quantile(A,.02),Quantile(B,.02),Quantile(C,.02)); Hi=FVector(Quantile(A,.98),Quantile(B,.98),Quantile(C,.98)); return true;
    };
    auto Radial=[&](const TArray<FVector>& Points,const FVector& Start,const FVector& End,int32& Used)
    {
        const FVector Axis=(End-Start).GetSafeNormal(); const double Length=(End-Start).Size(); TArray<double> Distances;
        for (const FVector& P:Points) { const double T=FVector::DotProduct(P-Start,Axis); if (T>=.15*Length && T<=.85*Length) Distances.Add((P-Start-Axis*T).Size()); }
        Used=Distances.Num(); return Used>=16 ? Quantile(Distances,.90) : -1.0;
    };
    TArray<TSharedPtr<FJsonValue>> Problems, Bodies, Joints, Pairs;
    auto Problem = [&](const FString& S) { Problems.Add(MakeShared<FJsonValueString>(S)); };
    struct FCapsule { FVector A, B; double Radius; };
    TArray<TArray<FCapsule>> Capsules; Capsules.SetNum(Physics->SkeletalBodySetups.Num());
    TArray<TArray<FCapsule>> CandidateCapsules; CandidateCapsules.SetNum(Physics->SkeletalBodySetups.Num());
    struct FBodyShapePlan { bool Replace = false; FKSphylElem Capsule; };
    TArray<FBodyShapePlan> ShapePlans; ShapePlans.SetNum(Physics->SkeletalBodySetups.Num());
    TArray<FString> Roles; TArray<double> MassWeights; double TotalMass = 0, TotalWeight = 0;
    const TMap<FString, double> Weights = {{TEXT("hips"),.18},{TEXT("spine"),.08},{TEXT("chest"),.10},{TEXT("upperChest"),.12},
        {TEXT("neck"),.015},{TEXT("head"),.08},{TEXT("Shoulder"),.006},{TEXT("UpperArm"),.027},{TEXT("LowerArm"),.016},
        {TEXT("Hand"),.006},{TEXT("UpperLeg"),.105},{TEXT("LowerLeg"),.045},{TEXT("Foot"),.015}};
    for (int32 I = 0; I < Physics->SkeletalBodySetups.Num(); ++I)
    {
        USkeletalBodySetup* Body = Physics->SkeletalBodySetups[I]; if (!Body) return Finish(R, TEXT("Null body"));
        const int32 Bone = Ref.FindBoneIndex(Body->BoneName); if (Bone == INDEX_NONE) return Finish(R, TEXT("Body bone absent from actual mesh"));
        FString Role; for (const auto& Entry : Profile->HumanoidBones) if (Entry.Value == Body->BoneName) { if (!Role.IsEmpty()) Problem(TEXT("Ambiguous role for ") + Body->BoneName.ToString()); Role = Entry.Key.ToString(); }
        Roles.Add(Role); FString WeightRole = Role;
        if (WeightRole.StartsWith(TEXT("left"))) WeightRole.RightChopInline(4);
        else if (WeightRole.StartsWith(TEXT("right"))) WeightRole.RightChopInline(5);
        const double* Weight = Weights.Find(WeightRole); MassWeights.Add(Weight ? *Weight : 0); TotalWeight += MassWeights.Last();
        if (!Weight) Problem(TEXT("No bounded mass/joint role for ") + Role);
        if (!CS[Bone].GetScale3D().Equals(FVector::OneVector, .00001)) Problem(TEXT("Non-unit reference body scale: ") + Role);
        TotalMass += Body->DefaultInstance.GetMassOverride();
        auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("index"), I); Row->SetStringField(TEXT("bone"), Body->BoneName.ToString()); Row->SetStringField(TEXT("role"), Role);
        Row->SetObjectField(TEXT("reference_component"), TransformJSON(CS[Bone])); Row->SetObjectField(TEXT("reference_bone_local"), TransformJSON(Ref.GetRefBonePose()[Bone]));
        Row->SetNumberField(TEXT("mass_override_kg"), Body->DefaultInstance.GetMassOverride());
        Row->SetNumberField(TEXT("linear_damping"), Body->DefaultInstance.LinearDamping); Row->SetNumberField(TEXT("angular_damping"), Body->DefaultInstance.AngularDamping);
        Row->SetNumberField(TEXT("physics_type"), int32(Body->PhysicsType)); Row->SetNumberField(TEXT("collision_enabled"), int32(Body->DefaultInstance.GetCollisionEnabled()));
        Row->SetBoolField(TEXT("override_iteration_counts"),Body->DefaultInstance.GetPositionSolverIterationCount()>=0);
        Row->SetNumberField(TEXT("position_solver_iteration_count"),Body->DefaultInstance.GetPositionSolverIterationCount());
        Row->SetNumberField(TEXT("velocity_solver_iteration_count"),Body->DefaultInstance.GetVelocitySolverIterationCount());
        Row->SetNumberField(TEXT("primitive_count"), Body->AggGeom.GetElementCount());
        Row->SetNumberField(TEXT("convex_count"), Body->AggGeom.ConvexElems.Num()); Row->SetNumberField(TEXT("other_shape_count"), Body->AggGeom.GetElementCount() - Body->AggGeom.SphylElems.Num() - Body->AggGeom.SphereElems.Num() - Body->AggGeom.BoxElems.Num());
        TArray<TSharedPtr<FJsonValue>> Shapes;
        for (const FKSphylElem& Shape : Body->AggGeom.SphylElems)
        {
            if (!FMath::IsFinite(Shape.Radius) || !FMath::IsFinite(Shape.Length) || Shape.Radius <= 0 || Shape.Length < 0 || Shape.GetTransform().ContainsNaN())
                return Finish(R, TEXT("Invalid capsule dimensions/transform; no mutation"));
            const FTransform Global = Shape.GetTransform() * CS[Bone]; const FVector Half = Global.GetUnitAxis(EAxis::Z) * Shape.Length * .5;
            Capsules[I].Add({Global.GetTranslation() - Half, Global.GetTranslation() + Half, Shape.Radius});
            auto S = MakeShared<FJsonObject>(); S->SetStringField(TEXT("type"), TEXT("capsule")); S->SetObjectField(TEXT("bone_local"), TransformJSON(Shape.GetTransform())); S->SetObjectField(TEXT("reference_component"), TransformJSON(Global));
            S->SetNumberField(TEXT("radius_cm"), Shape.Radius); S->SetNumberField(TEXT("cylinder_length_cm"), Shape.Length); S->SetNumberField(TEXT("total_length_cm"), Shape.Length + 2 * Shape.Radius); Shapes.Add(MakeShared<FJsonValueObject>(S));
        }
        for (const FKSphereElem& Shape : Body->AggGeom.SphereElems)
        {
            if (!FMath::IsFinite(Shape.Radius) || Shape.Radius <= 0 || Shape.Center.ContainsNaN())
                return Finish(R, TEXT("Invalid sphere dimensions/transform; no mutation"));
            const FVector Center = CS[Bone].TransformPosition(Shape.Center); Capsules[I].Add({Center, Center, Shape.Radius});
            auto S = MakeShared<FJsonObject>(); S->SetStringField(TEXT("type"), TEXT("sphere")); S->SetArrayField(TEXT("bone_local_center_cm"), XYZ(Shape.Center)); S->SetArrayField(TEXT("reference_center_cm"), XYZ(Center)); S->SetNumberField(TEXT("radius_cm"), Shape.Radius); Shapes.Add(MakeShared<FJsonValueObject>(S));
        }
        for (const FKBoxElem& Shape : Body->AggGeom.BoxElems)
        {
            auto S = MakeShared<FJsonObject>(); S->SetStringField(TEXT("type"), TEXT("box")); S->SetObjectField(TEXT("bone_local"), TransformJSON(Shape.GetTransform())); S->SetArrayField(TEXT("size_cm"), XYZ(FVector(Shape.X, Shape.Y, Shape.Z))); Shapes.Add(MakeShared<FJsonValueObject>(S));
        }
        if (Capsules[I].IsEmpty() || Capsules[I].Num() != Body->AggGeom.GetElementCount()) Problem(TEXT("Candidate overlap check supports only measured capsules/spheres: ") + Role);
        CandidateCapsules[I] = Capsules[I];
        FBodyShapePlan& ShapePlan = ShapePlans[I];
        if (Body->AggGeom.SphylElems.Num()!=1 || Body->AggGeom.GetElementCount()!=1)
            Problem(TEXT("Full-body fit requires one original capsule per evidenced humanoid body: ")+Role);
        else
        {
            // Keep primitive identity/settings, replace only geometric dimensions.
            ShapePlan.Capsule=Body->AggGeom.SphylElems[0];
            const TArray<FVector>& Points=SkinPoints.FindOrAdd(Role);
            FVector Center=CS[Bone].GetTranslation(),LongAxis=Up,Lo=FVector::ZeroVector,Hi=FVector::ZeroVector;
            double Radius=0,TotalLength=0; FString BasisDescription,Distal; bool Fallback=false; int32 RadialCount=0;
            const bool Trunk=Role==TEXT("hips") || Role==TEXT("spine") || Role==TEXT("chest") || Role==TEXT("upperChest");
            const bool HasBounds=Bounds(Points,Lateral,Forward,Up,Lo,Hi);
            Row->SetNumberField(TEXT("source_skin_point_count"),Points.Num());
            if (HasBounds) { Row->SetArrayField(TEXT("skin_bounds_2percent_anatomical_min"),XYZ(Lo)); Row->SetArrayField(TEXT("skin_bounds_98percent_anatomical_max"),XYZ(Hi)); }
            if (Trunk || Role==TEXT("head"))
            {
                if (!HasBounds) Problem(TEXT("Missing measured trunk/head skin: ")+Role);
                else if (Role==TEXT("upperChest") && Points.Num()<80)
                {
                    // R has only a small front patch under the coat. Do not use
                    // its one-sided bounds as a complete torso cross-section.
                    FVector ChestLo,ChestHi;
                    const int32 Neck=RoleIndex(TEXT("neck")),LA=RoleIndex(TEXT("leftUpperArm")),RA=RoleIndex(TEXT("rightUpperArm"));
                    if (!Bounds(SkinPoints.FindOrAdd(TEXT("chest")),Lateral,Forward,Up,ChestLo,ChestHi) || Neck==INDEX_NONE || LA==INDEX_NONE || RA==INDEX_NONE)
                        Problem(TEXT("Sparse upper chest requires measured chest skin/shoulder landmarks"));
                    else
                    {
                        Fallback=true; LongAxis=Lateral; const double ShoulderSpan=(CS[LA].GetTranslation()-CS[RA].GetTranslation()).Size();
                        Center=CS[Bone].GetTranslation()+Up*(.15*(CS[Neck].GetTranslation()-CS[Bone].GetTranslation()).Size());
                        Radius=FMath::Min(.45*HipSpan,.45*(ChestHi.Y-ChestLo.Y));
                        TotalLength=FMath::Min(.90*ShoulderSpan,FMath::Max(.95*(ChestHi.X-ChestLo.X),.80*ShoulderSpan));
                        BasisDescription=TEXT("Sparse/hidden upper-chest skin: measured same-character chest depth and shoulder-root width; radius=min(.45*hipSpan,.45*chestDepth); width limited to .90*shoulderSpan. Center from upperChest/neck landmarks. Explicit fallback, not complete skin coverage.");
                    }
                }
                else
                {
                    Center=Lateral*((Lo.X+Hi.X)*.5)+Forward*((Lo.Y+Hi.Y)*.5)+Up*((Lo.Z+Hi.Z)*.5);
                    if (Trunk)
                    { LongAxis=Lateral; Radius=FMath::Min(.50*HipSpan,.45*FMath::Min(Hi.Y-Lo.Y,Hi.Z-Lo.Z)); TotalLength=.96*(Hi.X-Lo.X); }
                    else
                    { LongAxis=Up; Radius=.46*FMath::Min(Hi.X-Lo.X,Hi.Y-Lo.Y); TotalLength=.96*(Hi.Z-Lo.Z); }
                    BasisDescription=TEXT("Dominant-weight Body/Face SKIN points only, 2%-98% anatomical-axis bounds. Inner transverse torso/vertical head capsule; no hair/ear outlier enclosure, clothing or accessory fit.");
                }
            }
            else if (Role.EndsWith(TEXT("Foot")))
            {
                // Bare foot and toe skin define the sole, not long coat/shoe decoration.
                const FVector SoleUp=FVector::UpVector, SoleForward=(Forward-SoleUp*FVector::DotProduct(Forward,SoleUp)).GetSafeNormal();
                const FVector SoleLateral=FVector::CrossProduct(SoleForward,SoleUp).GetSafeNormal(); FVector FootLo,FootHi;
                if (FVector::DotProduct(Up,SoleUp)<.98 || !Bounds(Points,SoleLateral,SoleForward,SoleUp,FootLo,FootHi))
                    Problem(TEXT("Foot requires measured upright reference skin/sole: ")+Role);
                else
                {
                    LongAxis=SoleForward; Radius=FMath::Min(.24*HipSpan,.46*(FootHi.X-FootLo.X)); TotalLength=.98*(FootHi.Y-FootLo.Y);
                    Center=SoleLateral*((FootLo.X+FootHi.X)*.5)+SoleForward*((FootLo.Y+FootHi.Y)*.5)+SoleUp*(FootLo.Z+Radius);
                    BasisDescription=TEXT("Foot plus toes SKIN, 2%-98% bounds; longitudinal inner capsule, radius <= .24*hipSpan and .46*footWidth. Bottom at measured skin sole; no oversized shoe/garment envelope.");
                    Row->SetArrayField(TEXT("measured_foot_sole_bounds_min"),XYZ(FootLo)); Row->SetArrayField(TEXT("measured_foot_sole_bounds_max"),XYZ(FootHi));
                }
            }
            else
            {
                double RadiusCap=0;
                if (Role.EndsWith(TEXT("UpperArm"))) { Distal=Role.Replace(TEXT("UpperArm"),TEXT("LowerArm")); RadiusCap=.24*HipSpan; }
                else if (Role.EndsWith(TEXT("LowerArm"))) { Distal=Role.Replace(TEXT("LowerArm"),TEXT("Hand")); RadiusCap=.22*HipSpan; }
                else if (Role.EndsWith(TEXT("UpperLeg"))) { Distal=Role.Replace(TEXT("UpperLeg"),TEXT("LowerLeg")); RadiusCap=.40*HipSpan; }
                else if (Role.EndsWith(TEXT("LowerLeg"))) { Distal=Role.Replace(TEXT("LowerLeg"),TEXT("Foot")); RadiusCap=.28*HipSpan; }
                else if (Role.EndsWith(TEXT("Shoulder"))) { Distal=Role.Replace(TEXT("Shoulder"),TEXT("UpperArm")); RadiusCap=.20*HipSpan; }
                else if (Role.EndsWith(TEXT("Hand"))) { Distal=Role.Replace(TEXT("Hand"),TEXT("MiddleProximal")); RadiusCap=.16*HipSpan; }
                else if (Role==TEXT("neck")) { Distal=TEXT("head"); RadiusCap=.22*HipSpan; }
                else Problem(TEXT("Unsupported full-body fit role: ")+Role);
                const int32 EndIndex=RoleIndex(*Distal);
                if (EndIndex==INDEX_NONE) Problem(TEXT("Missing distal fit landmark: ")+Distal);
                else
                {
                    const FVector Start=CS[Bone].GetTranslation(), End=CS[EndIndex].GetTranslation();
                    LongAxis=(End-Start).GetSafeNormal(); TotalLength=(End-Start).Size(); Center=(Start+End)*.5;
                    double Measured=Radial(Points,Start,End,RadialCount);
                    if (Measured<0 && Role.EndsWith(TEXT("UpperArm")))
                    {
                        const FString Forearm=Role.Replace(TEXT("UpperArm"),TEXT("LowerArm")), Hand=Role.Replace(TEXT("UpperArm"),TEXT("Hand"));
                        const int32 ForearmIndex=RoleIndex(*Forearm),HandIndex=RoleIndex(*Hand); int32 OtherCount=0;
                        if (ForearmIndex!=INDEX_NONE && HandIndex!=INDEX_NONE)
                            Measured=1.2*Radial(SkinPoints.FindOrAdd(Forearm),CS[ForearmIndex].GetTranslation(),CS[HandIndex].GetTranslation(),OtherCount);
                        Fallback=true; Row->SetNumberField(TEXT("fallback_forearm_radial_samples"),OtherCount);
                    }
                    if (Measured<0 && Role.EndsWith(TEXT("Shoulder"))) { Measured=FMath::Min(.20*HipSpan,.30*TotalLength); Fallback=true; }
                    if (Measured<=0) Problem(TEXT("No trustworthy skin radial samples or allowed hidden-arm fallback: ")+Role);
                    else
                    {
                        Radius=FMath::Min3(.90*Measured,RadiusCap,.48*TotalLength);
                        BasisDescription=Fallback ? TEXT("Hidden shoulder/upper-arm skin: actual bone segment; same-character measured forearm radius*1.2 for upper arm or min(.20*hipSpan,.30*shoulderSegment) for clavicle. Explicit inner-envelope fallback, not fabricated source coverage.")
                            : TEXT("Actual bone segment; skin-only radial 90th percentile over middle 15%-85% of segment, multiplied .90 and capped by anatomical hip-span ratio. Palm excludes finger/thumb skin; no sleeve or trouser envelope.");
                        Row->SetNumberField(TEXT("measured_skin_radial_p90_cm"),Measured); Row->SetNumberField(TEXT("anatomical_radius_cap_cm"),RadiusCap);
                    }
                    Row->SetNumberField(TEXT("measured_bone_segment_cm"),TotalLength);
                }
            }
            Row->SetBoolField(TEXT("shape_uses_explicit_fallback"),Fallback); Row->SetNumberField(TEXT("skin_radial_sample_count"),RadialCount);
            if (Radius>0 && FMath::IsFinite(Radius) && TotalLength>=2*Radius && !LongAxis.IsNearlyZero() && !Center.ContainsNaN())
            {
                ShapePlan.Replace=true; const double CylinderLength=TotalLength-2*Radius;
                const FTransform Global(FRotationMatrix::MakeFromZ(LongAxis).ToQuat(), Center);
                ShapePlan.Capsule.SetTransform(Global.GetRelativeTransform(CS[Bone]));
                ShapePlan.Capsule.Radius = float(Radius); ShapePlan.Capsule.Length = float(CylinderLength);
                // Calculate collision decisions from the same float-backed authored primitive.
                const FTransform AuthoredGlobal = ShapePlan.Capsule.GetTransform() * CS[Bone];
                const FVector Half = AuthoredGlobal.GetUnitAxis(EAxis::Z) * ShapePlan.Capsule.Length * .5;
                CandidateCapsules[I] = {{AuthoredGlobal.GetTranslation()-Half, AuthoredGlobal.GetTranslation()+Half, ShapePlan.Capsule.Radius}};
                auto CandidateShape = MakeShared<FJsonObject>(); CandidateShape->SetStringField(TEXT("type"), TEXT("capsule"));
                CandidateShape->SetObjectField(TEXT("bone_local"), TransformJSON(ShapePlan.Capsule.GetTransform()));
                CandidateShape->SetObjectField(TEXT("reference_component"), TransformJSON(AuthoredGlobal));
                CandidateShape->SetNumberField(TEXT("radius_cm"), ShapePlan.Capsule.Radius); CandidateShape->SetNumberField(TEXT("cylinder_length_cm"), ShapePlan.Capsule.Length);
                CandidateShape->SetNumberField(TEXT("total_length_cm"), ShapePlan.Capsule.Length+2*ShapePlan.Capsule.Radius);
                CandidateShape->SetStringField(TEXT("basis"), BasisDescription); Row->SetObjectField(TEXT("candidate_shape"), CandidateShape);
            }
            else Problem(TEXT("Invalid or incomplete measured full-body capsule: ")+Role);
        }
        Row->SetBoolField(TEXT("candidate_rebuilds_geometry"), ShapePlan.Replace);
        Row->SetArrayField(TEXT("shapes"), Shapes); Bodies.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (!FMath::IsFinite(TotalMass) || TotalMass < 10 || TotalMass > 200 || TotalWeight <= 0) Problem(TEXT("Existing total mass outside bounded humanoid range"));
    for (int32 I = 0; I < Bodies.Num(); ++I) Bodies[I]->AsObject()->SetNumberField(TEXT("candidate_mass_kg"), TotalWeight > 0 ? TotalMass * MassWeights[I] / TotalWeight : 0);
    struct FJointPlan { FTransform Child, Parent; double Swing1 = 0, Swing2 = 0, Twist = 0; bool Hinge = false; };
    TArray<FJointPlan> Plans; TSet<FRigidBodyIndexPair> Adjacent;
    for (UPhysicsConstraintTemplate* Template : Physics->ConstraintSetup)
    {
        if (!Template) return Finish(R, TEXT("Null constraint"));
        const FConstraintInstance& C = Template->DefaultInstance;
        if (!Template->ProfileHandles.IsEmpty()) Problem(TEXT("Unexpected named joint profiles: ") + C.JointName.ToString());
        const int32 Child = Ref.FindBoneIndex(C.ConstraintBone1), Parent = Ref.FindBoneIndex(C.ConstraintBone2);
        const int32 CI = Physics->FindBodyIndex(C.ConstraintBone1), ParentBodyIndex = Physics->FindBodyIndex(C.ConstraintBone2);
        if (Child == INDEX_NONE || Parent == INDEX_NONE || !Roles.IsValidIndex(CI) || !Roles.IsValidIndex(ParentBodyIndex)) return Finish(R, TEXT("Constraint body/bone mapping invalid"));
        Adjacent.Add(FRigidBodyIndexPair(CI, ParentBodyIndex)); const FString& Role = Roles[CI];
        int32 Nearest = Ref.GetParentIndex(Child); while (Nearest != INDEX_NONE && Physics->FindBodyIndex(Ref.GetBoneName(Nearest)) == INDEX_NONE) Nearest = Ref.GetParentIndex(Nearest);
        if (Nearest != Parent) Problem(TEXT("Constraint is not nearest physical ancestor: ") + C.JointName.ToString());
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("joint"), C.JointName.ToString()); Row->SetStringField(TEXT("child_bone"), C.ConstraintBone1.ToString()); Row->SetStringField(TEXT("parent_bone"), C.ConstraintBone2.ToString()); Row->SetStringField(TEXT("child_role"), Role);
        Row->SetObjectField(TEXT("frame1_child_local"), TransformJSON(C.GetRefFrame(EConstraintFrame::Frame1))); Row->SetObjectField(TEXT("frame2_parent_local"), TransformJSON(C.GetRefFrame(EConstraintFrame::Frame2)));
        const FTransform World1 = C.GetRefFrame(EConstraintFrame::Frame1) * CS[Child], World2 = C.GetRefFrame(EConstraintFrame::Frame2) * CS[Parent];
        Row->SetObjectField(TEXT("frame1_reference_component"), TransformJSON(World1)); Row->SetObjectField(TEXT("frame2_reference_component"), TransformJSON(World2));
        Row->SetNumberField(TEXT("reference_anchor_separation_cm"), (World1.GetTranslation() - World2.GetTranslation()).Size());
        Row->SetArrayField(TEXT("linear_motion_xyz"), {MakeShared<FJsonValueNumber>(int32(C.GetLinearXMotion())),MakeShared<FJsonValueNumber>(int32(C.GetLinearYMotion())),MakeShared<FJsonValueNumber>(int32(C.GetLinearZMotion()))});
        Row->SetArrayField(TEXT("angular_motion_swing1_swing2_twist"), {MakeShared<FJsonValueNumber>(int32(C.GetAngularSwing1Motion())),MakeShared<FJsonValueNumber>(int32(C.GetAngularSwing2Motion())),MakeShared<FJsonValueNumber>(int32(C.GetAngularTwistMotion()))});
        Row->SetArrayField(TEXT("angular_limits_degrees"), XYZ(FVector(C.GetAngularSwing1Limit(),C.GetAngularSwing2Limit(),C.GetAngularTwistLimit())));
        Row->SetArrayField(TEXT("angular_offset_pitch_yaw_roll"), XYZ(FVector(C.AngularRotationOffset.Pitch,C.AngularRotationOffset.Yaw,C.AngularRotationOffset.Roll)));
        Row->SetBoolField(TEXT("constraint_disables_pair_collision"), C.IsCollisionDisabled()); Row->SetBoolField(TEXT("projection_enabled"), C.ProfileInstance.bEnableProjection);
        Row->SetBoolField(TEXT("soft_swing_limit"), C.ProfileInstance.ConeLimit.bSoftConstraint); Row->SetBoolField(TEXT("soft_twist_limit"), C.ProfileInstance.TwistLimit.bSoftConstraint);
        Row->SetNumberField(TEXT("named_profile_count"), Template->ProfileHandles.Num());
        FJointPlan Plan; FVector Axis = Up, Secondary = Forward; double CenterAngle = 0, ParentSwingBias = 0;
        FString Distal;
        if (Role.EndsWith(TEXT("UpperArm"))) { Distal = Role.Replace(TEXT("UpperArm"), TEXT("LowerArm")); Plan.Swing1=45; Plan.Swing2=100; Plan.Twist=45; ParentSwingBias=20; }
        else if (Role.EndsWith(TEXT("UpperLeg"))) { Distal = Role.Replace(TEXT("UpperLeg"), TEXT("LowerLeg")); Plan.Swing1=55; Plan.Swing2=35; Plan.Twist=25; }
        else if (Role.EndsWith(TEXT("LowerArm"))) { Distal = Role.Replace(TEXT("LowerArm"), TEXT("Hand")); Plan.Hinge=true; Plan.Twist=69; CenterAngle=66; }
        else if (Role.EndsWith(TEXT("LowerLeg"))) { Distal = Role.Replace(TEXT("LowerLeg"), TEXT("Foot")); Plan.Hinge=true; Plan.Twist=64; CenterAngle=61; }
        else if (Role.EndsWith(TEXT("Hand"))) { Distal = Role.Replace(TEXT("Hand"), TEXT("MiddleProximal")); Plan.Swing1=25; Plan.Swing2=20; Plan.Twist=25; }
        else if (Role.EndsWith(TEXT("Foot"))) { Distal = Role.Replace(TEXT("Foot"), TEXT("Toes")); Plan.Swing1=20; Plan.Swing2=15; Plan.Twist=10; }
        else if (Role.EndsWith(TEXT("Shoulder"))) { Distal = Role.Replace(TEXT("Shoulder"), TEXT("UpperArm")); Plan.Swing1=20; Plan.Swing2=20; Plan.Twist=15; }
        else if (Role==TEXT("head")) { Plan.Swing1=25; Plan.Swing2=25; Plan.Twist=40; }
        else if (Role==TEXT("neck")) { Plan.Swing1=15; Plan.Swing2=15; Plan.Twist=20; }
        else if (Role==TEXT("spine") || Role==TEXT("chest") || Role==TEXT("upperChest")) { Plan.Swing1=10; Plan.Swing2=10; Plan.Twist=12; }
        else Problem(TEXT("Unsupported anatomical joint role: ") + Role);
        if (!Distal.IsEmpty())
        {
            const int32 End = RoleIndex(*Distal);
            if (End == INDEX_NONE) Problem(TEXT("Missing distal reference role: ") + Distal);
            else Axis = (CS[End].GetTranslation() - CS[Child].GetTranslation()).GetSafeNormal();
        }
        if (Plan.Hinge)
        {
            const FVector Bend = Role.EndsWith(TEXT("LowerLeg")) ? -Forward : Forward;
            const FVector Limb = Axis; Axis = FVector::CrossProduct(Limb, Bend).GetSafeNormal(); Secondary = Limb;
        }
        else if (FMath::Abs(FVector::DotProduct(Axis, Secondary)) > .95) Secondary = Up;
        if (Role.EndsWith(TEXT("UpperLeg")))
        {
            const double RefPitch=FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Axis,Forward),FVector::DotProduct(Axis,-Up)));
            ParentSwingBias=40-RefPitch; Row->SetNumberField(TEXT("reference_thigh_forward_pitch_degrees"),RefPitch);
        }
        if (Axis.IsNearlyZero() || FVector::CrossProduct(Axis, Secondary).Size() < .1)
        { Problem(TEXT("Degenerate measured joint frame: ") + Role); Axis = Up; Secondary = Forward; }
        const FQuat Basis = FRotationMatrix::MakeFromXY(Axis, Secondary).ToQuat();
        const FVector Anchor = CS[Child].GetTranslation();
        // Bias child joint X by -center: symmetric twist limits become the stated
        // anatomical flexion interval, while reference position remains valid.
        const FTransform ChildWorld(Basis * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-CenterAngle)), Anchor);
        // Parent local-Z swing bias is distinct from the unchanged child local-X
        // hinge bias above. Pure sagittal hip range becomes [-15,+95] degrees.
        const FTransform ParentWorld(Basis * FQuat(FVector::UpVector,FMath::DegreesToRadians(ParentSwingBias)), Anchor);
        Plan.Child = ChildWorld.GetRelativeTransform(CS[Child]); Plan.Parent = ParentWorld.GetRelativeTransform(CS[Parent]);
        auto Candidate = MakeShared<FJsonObject>(); Candidate->SetObjectField(TEXT("frame1_child_local"), TransformJSON(Plan.Child)); Candidate->SetObjectField(TEXT("frame2_parent_local"), TransformJSON(Plan.Parent));
        Candidate->SetArrayField(TEXT("angular_limits_degrees"), XYZ(FVector(Plan.Swing1,Plan.Swing2,Plan.Twist))); Candidate->SetBoolField(TEXT("twist_hinge_swing_locked"), Plan.Hinge);
        if (Plan.Hinge) { Candidate->SetNumberField(TEXT("flexion_min_degrees"), CenterAngle-Plan.Twist); Candidate->SetNumberField(TEXT("flexion_max_degrees"), CenterAngle+Plan.Twist); }
        Candidate->SetNumberField(TEXT("parent_local_z_swing_bias_degrees"),ParentSwingBias);
        if (Role.EndsWith(TEXT("UpperLeg"))) Candidate->SetStringField(TEXT("sagittal_anatomical_candidate"),TEXT("[-15,+95] degrees from actual reference forward pitch; coupled cone and runtime constraint error still require validation"));
        if (Role.EndsWith(TEXT("UpperArm"))) Candidate->SetStringField(TEXT("reference_horizontal_candidate"),TEXT("[-25,+65] degrees around local Z; swing2=100 preserves arm lowering; not the clavicle Shoulder joint"));
        Candidate->SetStringField(TEXT("scope"), TEXT("Initial engineering limits derived from actual reference axes; not visual/clinical validation"));
        Row->SetObjectField(TEXT("candidate"), Candidate); Plans.Add(Plan); Joints.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<FRigidBodyIndexPair> EnablePairs; int32 DisabledCount=0, OverlapExceptions=0;
    for (int32 A = 0; A < Capsules.Num(); ++A) for (int32 B = A+1; B < Capsules.Num(); ++B)
    {
        const FRigidBodyIndexPair Pair(A,B); double Gap = TNumericLimits<double>::Max(), CandidateGap = TNumericLimits<double>::Max();
        for (const FCapsule& One : Capsules[A]) for (const FCapsule& Two : Capsules[B])
        { FVector P,Q; FMath::SegmentDistToSegmentSafe(One.A,One.B,Two.A,Two.B,P,Q); Gap=FMath::Min(Gap,(P-Q).Size()-One.Radius-Two.Radius); }
        for (const FCapsule& One : CandidateCapsules[A]) for (const FCapsule& Two : CandidateCapsules[B])
        { FVector P,Q; FMath::SegmentDistToSegmentSafe(One.A,One.B,Two.A,Two.B,P,Q); CandidateGap=FMath::Min(CandidateGap,(P-Q).Size()-One.Radius-Two.Radius); }
        const bool Disabled = Physics->CollisionDisableTable.Contains(Pair); DisabledCount += Disabled;
        const bool Enable = !Adjacent.Contains(Pair) && CandidateGap != TNumericLimits<double>::Max() && CandidateGap > .25;
        if (Enable) EnablePairs.Add(Pair); else if (!Adjacent.Contains(Pair)) ++OverlapExceptions;
        auto Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("a"),A); Row->SetNumberField(TEXT("b"),B); Row->SetStringField(TEXT("role_a"),Roles[A]); Row->SetStringField(TEXT("role_b"),Roles[B]);
        Row->SetBoolField(TEXT("disabled_before"),Disabled); Row->SetBoolField(TEXT("adjacent"),Adjacent.Contains(Pair)); Row->SetBoolField(TEXT("candidate_enabled"),Enable);
        if (Gap != TNumericLimits<double>::Max()) Row->SetNumberField(TEXT("reference_capsule_surface_gap_cm"),Gap);
        if (CandidateGap != TNumericLimits<double>::Max()) Row->SetNumberField(TEXT("candidate_reference_capsule_surface_gap_cm"),CandidateGap);
        Row->SetStringField(TEXT("candidate_reason"),Adjacent.Contains(Pair)?TEXT("Adjacent bodies stay collision-disabled"):Enable?TEXT("Separated non-adjacent bodies collide"):TEXT("Rest overlap/near-contact retained disabled; shape review remains required")); Pairs.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("bodies"),Bodies); R->SetArrayField(TEXT("constraints"),Joints); R->SetArrayField(TEXT("collision_pairs"),Pairs);
    R->SetNumberField(TEXT("body_count"),Bodies.Num()); R->SetNumberField(TEXT("constraint_count"),Joints.Num()); R->SetNumberField(TEXT("disabled_pair_count_before"),DisabledCount);
    R->SetNumberField(TEXT("candidate_nonadjacent_collision_pairs"),EnablePairs.Num()); R->SetNumberField(TEXT("nonadjacent_rest_overlap_exceptions"),OverlapExceptions);
    R->SetNumberField(TEXT("total_mass_kg_preserved"),TotalMass); R->SetArrayField(TEXT("candidate_blockers"),Problems); R->SetBoolField(TEXT("candidate_valid"),Problems.IsEmpty());
    R->SetStringField(TEXT("geometry_scope"),TEXT("All existing humanoid capsules rebuilt from skin-only points and actual reference landmarks. No new bodies, garment/tail fitting, source geometry changes or solver setting changes. Collision decisions use all candidate shapes; remaining reference overlaps are explicit exceptions, not natural-pose acceptance."));
    if (bApply)
    {
        if (!Problems.IsEmpty()) return Finish(R,TEXT("Candidate blocked by actual native probe; no mutation"));
        Physics->Modify();
        for (int32 I=0; I<Physics->SkeletalBodySetups.Num(); ++I)
        {
            auto* Body=Physics->SkeletalBodySetups[I].Get(); Body->Modify();
            if (ShapePlans[I].Replace)
            { Body->AggGeom.SphylElems[0]=ShapePlans[I].Capsule; Body->InvalidatePhysicsData(); Body->CreatePhysicsMeshes(); }
            Body->DefaultInstance.SetMassOverride(TotalMass*MassWeights[I]/TotalWeight,true);
        }
        for (int32 I=0; I<Physics->ConstraintSetup.Num(); ++I)
        {
            auto* Template=Physics->ConstraintSetup[I].Get(); Template->Modify(); FConstraintInstance& C=Template->DefaultInstance; const FJointPlan& P=Plans[I];
            C.AngularRotationOffset=FRotator::ZeroRotator; C.SetRefFrame(EConstraintFrame::Frame1,P.Child); C.SetRefFrame(EConstraintFrame::Frame2,P.Parent);
            C.SetLinearLimits(LCM_Locked,LCM_Locked,LCM_Locked,0);
            C.SetAngularSwing1Limit(P.Hinge?ACM_Locked:ACM_Limited,P.Swing1); C.SetAngularSwing2Limit(P.Hinge?ACM_Locked:ACM_Limited,P.Swing2); C.SetAngularTwistLimit(ACM_Limited,P.Twist);
            C.ProfileInstance.ConeLimit.bSoftConstraint=false; C.ProfileInstance.TwistLimit.bSoftConstraint=false;
            C.SetDisableCollision(true); C.SetLinearBreakable(false,0); C.SetAngularBreakable(false,0); Template->SetDefaultProfile(C);
        }
        for (int32 A=0; A<Capsules.Num(); ++A) for (int32 B=A+1; B<Capsules.Num(); ++B)
            if (EnablePairs.Contains(FRigidBodyIndexPair(A,B))) Physics->EnableCollision(A,B); else Physics->DisableCollision(A,B);
        Physics->PostEditChange(); Physics->MarkPackageDirty(); R->SetBoolField(TEXT("applied"),true);
        TSharedPtr<FJsonObject> Readback; const FString Text=InspectOrRepairNPCPhysics(Profile,Physics,false);
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Readback) || !Readback || Readback->GetStringField(TEXT("status"))!=TEXT("PASS")) return Finish(R,TEXT("Native post-apply readback failed; helper has not saved"));
        R->SetObjectField(TEXT("after"),Readback);
        for (int32 I=0; I<Bodies.Num(); ++I)
        {
            if (!FMath::IsNearlyEqual(Physics->SkeletalBodySetups[I]->DefaultInstance.GetMassOverride(),float(TotalMass*MassWeights[I]/TotalWeight),.0001f)) return Finish(R,TEXT("Mass readback mismatch; helper has not saved"));
            if (ShapePlans[I].Replace && !(Physics->SkeletalBodySetups[I]->AggGeom.SphylElems[0] == ShapePlans[I].Capsule))
                return Finish(R,TEXT("Measured body shape readback mismatch; helper has not saved"));
            if (!ShapePlans[I].Replace)
            {
                FString BeforeShapes, AfterShapes;
                FJsonSerializer::Serialize(Bodies[I]->AsObject()->GetArrayField(TEXT("shapes")),TJsonWriterFactory<>::Create(&BeforeShapes));
                FJsonSerializer::Serialize(Readback->GetArrayField(TEXT("bodies"))[I]->AsObject()->GetArrayField(TEXT("shapes")),TJsonWriterFactory<>::Create(&AfterShapes));
                if (BeforeShapes != AfterShapes) return Finish(R,TEXT("Unselected body geometry changed; helper has not saved"));
            }
        }
        for (int32 I=0; I<Plans.Num(); ++I)
        {
            const auto& C=Physics->ConstraintSetup[I]->DefaultInstance; const auto& P=Plans[I];
            if (!C.GetRefFrame(EConstraintFrame::Frame1).Equals(P.Child,.0001) || !C.GetRefFrame(EConstraintFrame::Frame2).Equals(P.Parent,.0001)
                || C.GetLinearXMotion()!=LCM_Locked || C.GetLinearYMotion()!=LCM_Locked || C.GetLinearZMotion()!=LCM_Locked
                || C.GetAngularSwing1Motion()!=(P.Hinge?ACM_Locked:ACM_Limited) || C.GetAngularSwing2Motion()!=(P.Hinge?ACM_Locked:ACM_Limited)
                || C.GetAngularTwistMotion()!=ACM_Limited || !FMath::IsNearlyEqual(C.GetAngularSwing1Limit(),float(P.Swing1),.0001f)
                || !FMath::IsNearlyEqual(C.GetAngularSwing2Limit(),float(P.Swing2),.0001f) || !FMath::IsNearlyEqual(C.GetAngularTwistLimit(),float(P.Twist),.0001f)
                || C.ProfileInstance.ConeLimit.bSoftConstraint || C.ProfileInstance.TwistLimit.bSoftConstraint || !C.IsCollisionDisabled())
                return Finish(R,TEXT("Joint readback mismatch; helper has not saved"));
        }
        for (int32 A=0; A<Capsules.Num(); ++A) for (int32 B=A+1; B<Capsules.Num(); ++B)
            if (Physics->CollisionDisableTable.Contains(FRigidBodyIndexPair(A,B))==EnablePairs.Contains(FRigidBodyIndexPair(A,B))) return Finish(R,TEXT("Collision-table readback mismatch; helper has not saved"));
        R->SetBoolField(TEXT("native_candidate_readback_checked"),true);
    }
    return Finish(R);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2NPCEditor::BuildNPCPresentation(UHCM5VS2NPCProfile* Profile, UAnimBlueprint* Blueprint,
    UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, const FString& Destination)
{
#if WITH_EDITOR
    auto R = MakeShared<FJsonObject>();
    USkeletalMesh* Mesh = Profile ? Profile->Mesh.Get() : nullptr;
    if (!Owned(Profile) || !Owned(Mesh) || !Owned(Blueprint) || !Mesh->GetSkeleton()
        || Blueprint->TargetSkeleton != Mesh->GetSkeleton() || Blueprint->ParentClass != UHCM5VS2NPCAnimInstance::StaticClass()
        || !Destination.StartsWith(TEXT("/Game/HarborCity/M5VS2/NPC/"))) return Finish(R, TEXT("New matching NPC profile, mesh and empty NPC AnimBP required"));
    for (UAnimSequence* Sequence : {Idle, Walk, Run})
        if (!Owned(Sequence) || Sequence->GetSkeleton() != Mesh->GetSkeleton()) return Finish(R, TEXT("Three independently retargeted NPC locomotion sequences required"));
    if (!Mesh->GetSkeleton()->ContainsSlotName(TEXT("FullBody"))) return Finish(R, TEXT("Copy source reaction slots first"));
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    const FName Roles[] = {TEXT("head"), TEXT("leftEye"), TEXT("rightEye"), TEXT("hips")};
    TArray<FName> Bones;
    for (FName Role : Roles)
    {
        const FName* Bone = Profile->HumanoidBones.Find(Role);
        if (!Bone || Ref.FindBoneIndex(*Bone) == INDEX_NONE) return Finish(R, TEXT("Verified humanoid head, eyes and hips required"));
        Bones.Add(*Bone);
    }
    TArray<UAnimGraphNode_Root*> Roots; FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Roots);
    UAnimGraphNode_Root* Root = nullptr;
    for (auto* Node : Roots) if (Node->GetGraph()->GetName() == TEXT("AnimGraph")) { if (Root) return Finish(R, TEXT("Ambiguous graph")); Root = Node; }
    if (!Root || !Root->FindPin(TEXT("Result")) || !Root->FindPin(TEXT("Result"))->LinkedTo.IsEmpty()
        || Root->GetGraph()->Nodes.Num() != 1) return Finish(R, TEXT("Only a fresh empty AnimGraph may be authored"));

    UBlendSpace1D* Space = FreshAsset<UBlendSpace1D>(Destination + TEXT("/BS_NPC_IdleWalkRun"));
    UPhysicsAsset* Physics = FreshAsset<UPhysicsAsset>(Destination + TEXT("/PHYS_NPC_Humanoid"));
    if (!Space || !Physics) return Finish(R, TEXT("Output already exists; preserve it and use a fresh namespace"));
    Space->SetSkeleton(Mesh->GetSkeleton());
    FStructProperty* AxisProperty = FindFProperty<FStructProperty>(Space->GetClass(), TEXT("BlendParameters"));
    if (!AxisProperty || AxisProperty->Struct != FBlendParameter::StaticStruct() || AxisProperty->ArrayDim != 3)
        return Finish(R, TEXT("Unexpected native BlendParameters schema"));
    FBlendParameter* Axis = AxisProperty->ContainerPtrToValuePtr<FBlendParameter>(Space, 0);
    Axis->DisplayName = TEXT("Speed cm/s"); Axis->Min = 0; Axis->Max = 280; Axis->GridNum = 4;
    Space->AddSample(Idle, FVector(0, 0, 0)); Space->AddSample(Walk, FVector(140, 0, 0)); Space->AddSample(Run, FVector(280, 0, 0));
    Space->ValidateSampleData(); Space->ResampleData(); Space->PostEditChange();
    if (Space->GetNumberOfBlendSamples() != 3) return Finish(R, TEXT("Native BlendSpace samples incomplete"));

    FPhysAssetCreateParams Params; Params.MinBoneSize = 8.f; Params.bIncludeChildBones = false;
    Params.GeomType = EFG_Sphyl; Params.bCreateConstraints = true; Params.bDisableCollisionsByDefault = true;
    FText Error;
    if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Physics, Mesh, Params, Error, false, false)) return Finish(R, Error.ToString());
    TSet<FName> BodyBones;
    for (FName Role : {FName(TEXT("hips")), FName(TEXT("spine")), FName(TEXT("chest")), FName(TEXT("upperChest")),
        FName(TEXT("neck")), FName(TEXT("head")), FName(TEXT("leftShoulder")), FName(TEXT("rightShoulder")),
        FName(TEXT("leftUpperArm")), FName(TEXT("leftLowerArm")), FName(TEXT("leftHand")),
        FName(TEXT("rightUpperArm")), FName(TEXT("rightLowerArm")), FName(TEXT("rightHand")),
        FName(TEXT("leftUpperLeg")), FName(TEXT("leftLowerLeg")), FName(TEXT("leftFoot")),
        FName(TEXT("rightUpperLeg")), FName(TEXT("rightLowerLeg")), FName(TEXT("rightFoot"))})
        if (const FName* Bone = Profile->HumanoidBones.Find(Role)) BodyBones.Add(*Bone);
    for (int32 I = Physics->SkeletalBodySetups.Num() - 1; I >= 0; --I)
        if (!Physics->SkeletalBodySetups[I] || !BodyBones.Contains(Physics->SkeletalBodySetups[I]->BoneName)) FPhysicsAssetUtils::DestroyBody(Physics, I);
    Physics->UpdateBodySetupIndexMap(); Physics->UpdateBoundsBodiesArray();
    if (Physics->FindBodyIndex(Bones[3]) == INDEX_NONE || Physics->SkeletalBodySetups.Num() < 8 || Physics->ConstraintSetup.Num() < 7)
        return Finish(R, TEXT("Automatic humanoid physics lacks hips/body coverage; inspect before binding"));
    TArray<TSharedPtr<FJsonValue>> BodyRows;
    for (USkeletalBodySetup* Body : Physics->SkeletalBodySetups)
    {
        Body->DefaultInstance.SetMassOverride(65.f / Physics->SkeletalBodySetups.Num(), true);
        Body->DefaultInstance.LinearDamping = .8f; Body->DefaultInstance.AngularDamping = 2.f;
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("bone"), Body->BoneName.ToString());
        Row->SetNumberField(TEXT("primitive_count"), Body->AggGeom.GetElementCount()); BodyRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Physics->SetPreviewMesh(Mesh); Physics->MarkPackageDirty();

    UEdGraph* Graph = Root->GetGraph(); Blueprint->Modify(); Graph->Modify();
    auto Connect = [Graph](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Graph->GetSchema()->TryCreateConnection(A, B); };
    auto* Speed = NewNode<UK2Node_VariableGet>(Graph, TEXT("NPC_GroundSpeed"), -1300, 300);
    Speed->VariableReference.SetSelfMember(TEXT("NPCGroundSpeed")); Speed->AllocateDefaultPins();
    auto* Player = NewNode<UAnimGraphNode_BlendSpacePlayer>(Graph, TEXT("NPC_Locomotion"), -1100);
    Player->Node.SetBlendSpace(Space); Player->AllocateDefaultPins();
    if (!Expose(Player, TEXT("X")) || !Connect(Output(Speed), Player->FindPin(TEXT("X")))) return Finish(R, TEXT("Native locomotion speed pin wiring failed"));
    auto* Slot = NewNode<UAnimGraphNode_Slot>(Graph, TEXT("NPC_FullBody"), -850);
    Slot->Node.SlotName = TEXT("FullBody"); Slot->Node.bAlwaysUpdateSourcePose = true; Slot->AllocateDefaultPins();
    auto* ToComponent = NewNode<UAnimGraphNode_LocalToComponentSpace>(Graph, TEXT("NPC_ToComponent"), -620); ToComponent->AllocateDefaultPins();
    if (!Connect(Output(Player), Slot->FindPin(TEXT("Source"))) || !Connect(Output(Slot), ToComponent->FindPin(TEXT("LocalPose")))) return Finish(R, TEXT("Native reaction slot wiring failed"));
    auto* Target = NewNode<UK2Node_VariableGet>(Graph, TEXT("NPC_LookTarget"), -500, 350);
    Target->VariableReference.SetSelfMember(TEXT("NPCLookTarget")); Target->AllocateDefaultPins();
    auto* HeadAlpha = NewNode<UK2Node_VariableGet>(Graph, TEXT("NPC_HeadAlpha"), -500, 450);
    HeadAlpha->VariableReference.SetSelfMember(TEXT("NPCHeadLookAlpha")); HeadAlpha->AllocateDefaultPins();
    auto* EyeAlpha = NewNode<UK2Node_VariableGet>(Graph, TEXT("NPC_EyeAlpha"), -500, 550);
    EyeAlpha->VariableReference.SetSelfMember(TEXT("NPCEyeLookAlpha")); EyeAlpha->AllocateDefaultPins();
    TArray<FTransform> CS; CS.SetNum(Ref.GetNum());
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    { const int32 Parent = Ref.GetParentIndex(I); CS[I] = Parent < 0 ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * CS[Parent]; }
    UEdGraphPin* Prior = Output(ToComponent);
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Look = NewNode<UAnimGraphNode_LookAt>(Graph, *FString::Printf(TEXT("NPC_Look_%d"), I), -380 + I * 220);
        Look->Node.BoneToModify.BoneName = Bones[I];
        const FQuat Rotation = CS[Ref.FindBoneIndex(Bones[I])].GetRotation();
        // Q and R's imported eye/toe positions confirm +Y as the mesh forward basis.
        Look->Node.LookAt_Axis = FAxis(Rotation.UnrotateVector(FVector(0, 1, 0)).GetSafeNormal());
        Look->Node.LookUp_Axis = FAxis(Rotation.UnrotateVector(FVector::UpVector).GetSafeNormal());
        Look->Node.bUseLookUpAxis = true; Look->Node.LookAtClamp = I == 0 ? 30.f : 10.f;
        Look->Node.Alpha = 0; Look->Node.InterpolationTime = 0; Look->AllocateDefaultPins();
        if (!Expose(Look, TEXT("LookAtLocation")) || !Expose(Look, TEXT("Alpha"))
            || !Connect(Prior, Look->FindPin(TEXT("ComponentPose"))) || !Connect(Output(Target), Look->FindPin(TEXT("LookAtLocation")))
            || !Connect(Output(I == 0 ? HeadAlpha : EyeAlpha), Look->FindPin(TEXT("Alpha")))) return Finish(R, TEXT("Mapped look node wiring failed"));
        Prior = Output(Look);
    }
    auto* ToLocal = NewNode<UAnimGraphNode_ComponentToLocalSpace>(Graph, TEXT("NPC_ToLocal"), 360); ToLocal->AllocateDefaultPins();
    if (!Connect(Prior, ToLocal->FindPin(TEXT("ComponentPose"))) || !Connect(Output(ToLocal), Root->FindPin(TEXT("Result")))) return Finish(R, TEXT("Final native pose wiring failed"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return Finish(R, TEXT("NPC AnimBlueprint compile failed"));
    Blueprint->MarkPackageDirty();
    R->SetStringField(TEXT("blendspace"), Space->GetPathName()); R->SetStringField(TEXT("anim_blueprint"), Blueprint->GetPathName());
    R->SetStringField(TEXT("physics_asset"), Physics->GetPathName()); R->SetArrayField(TEXT("physics_bodies"), BodyRows);
    R->SetNumberField(TEXT("physics_constraints"), Physics->ConstraintSetup.Num());
    R->SetStringField(TEXT("physics_scope"), TEXT("Capsules fitted to humanoid weighted vertices; accessories/fingers excluded. Runtime stability and visual fit require real testing."));
    R->SetStringField(TEXT("spring_scope"), TEXT("Imported mesh post-process VRM spring graph retained; its runtime motion is not verified by graph authoring."));
    return Finish(R);
#else
    return TEXT("{\"status\":\"NOT_RUN\",\"error\":\"Editor only\"}");
#endif
}
