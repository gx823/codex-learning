#include "HCM5VS2GASTransitionAssetsEditor.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "AnimationUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetUserData.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Misc/Paths.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"

namespace
{
const FString SourceDisk=TEXT("E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample/Content/");
const FString SkeletonSource=TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin");
const FString MeshSource=TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin");
const FString CleanSkeleton=TEXT("/Game/HarborCity/M5VS2/GASSourceP0/SK_GAS_UEFN_P0");
const FString CleanMesh=TEXT("/Game/HarborCity/M5VS2/GASSourceP0/SKM_GAS_UEFN_P0");
const TCHAR* Owner=TEXT("HarborCity_M5_VS2_GAS_P0_v1");
const TCHAR* TransitionOwner=TEXT("HarborCity_M5_VS2_GASTransition_v1");
const TArray<FString> SubRoots={TEXT("Characters/UEFN_Mannequin/"),TEXT("Audio/"),TEXT("Blueprints/"),TEXT("Misc/")};
using FRows=TArray<TSharedPtr<FJsonValue>>;
struct FCutSpec { const TCHAR* Suffix; int32 First; int32 Last; };
const TArray<FCutSpec> Cuts={
    {TEXT("Jump/M_Relaxed_Jump_F_Start_Stand_Rfoot"),18,35},
    {TEXT("Jump/M_Relaxed_Jump_F_Start_Run_Rfoot"),10,28},
    {TEXT("Jump/M_Relaxed_Jump_F_Start_Sprint_Rfoot"),8,25},
    {TEXT("Jump/M_Relaxed_Jump_Loop_Fall"),0,100},
    {TEXT("Jump/M_Relaxed_Jump_F_Land_Stand_Light_Rfoot"),15,45},
    {TEXT("Jump/M_Relaxed_Jump_F_Land_Run_Light_Rfoot"),15,30},
    {TEXT("Run/M_Relaxed_Run_Start_F_Lfoot"),0,13},
    {TEXT("Sprint/M_Relaxed_Sprint_Start_F_Lfoot"),0,18},
    {TEXT("Run/M_Relaxed_Run_Stop_F_Lfoot"),43,66},
    {TEXT("Sprint/M_Relaxed_Sprint_Stop_F_Lfoot"),46,66}
};
FString OriginalClip(const FString& S) { return TEXT("/Game/Characters/UEFN_Mannequin/Animations/")+S; }
FString ClipName(const FString& S) { return FPackageName::GetShortName(S); }
FString ObjectPath(const FString& P) { return P+TEXT(".")+FPackageName::GetShortName(P); }
FString LocalFile(const FString& P) { return FPackageName::LongPackageNameToFilename(P,FPackageName::GetAssetPackageExtension()); }
bool SourcePackage(const FString& P) { for(const auto& S:SubRoots) if(P.StartsWith(TEXT("/Game/")+S)) return true; return false; }
FString Finish(const TSharedRef<FJsonObject>& R,const FString& Error=FString())
{
    R->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    if(!Error.IsEmpty()) R->SetStringField(TEXT("error"),Error);
    FString Out; FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Out)); return Out;
}
bool Fresh(const FString& P) { return !FindPackage(nullptr,*P) && !IFileManager::Get().FileExists(*LocalFile(P)); }
TSharedRef<FJsonObject> Summary(UAnimSequence* S)
{
    auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"),S->GetPathName());
    const IAnimationDataModel* M=S->GetDataModel();
    if(!M) { J->SetStringField(TEXT("error"),TEXT("No native data model")); return J; }
    J->SetStringField(TEXT("model_class"),S->GetDataModelInterface().GetObject()->GetClass()->GetPathName());
    J->SetStringField(TEXT("data_guid"),M->GenerateGuid().ToString());
    J->SetNumberField(TEXT("frames"),M->GetNumberOfFrames()); J->SetNumberField(TEXT("keys"),M->GetNumberOfKeys());
    J->SetNumberField(TEXT("fps_numerator"),M->GetFrameRate().Numerator); J->SetNumberField(TEXT("fps_denominator"),M->GetFrameRate().Denominator);
    J->SetNumberField(TEXT("duration_seconds"),M->GetPlayLength()); J->SetNumberField(TEXT("tracks"),M->GetNumBoneTracks());
    J->SetNumberField(TEXT("float_curves"),M->GetNumberOfFloatCurves());
    TArray<TSharedPtr<FJsonValue>> Motion;
    for(FName Bone:{FName(TEXT("root")),FName(TEXT("pelvis")),FName(TEXT("Hips"))})
    {
        if(!M->IsValidBoneTrackName(Bone)) continue;
        TArray<FTransform> Keys; M->GetBoneTrackTransforms(Bone,Keys);
        auto B=MakeShared<FJsonObject>(); B->SetStringField(TEXT("bone"),Bone.ToString()); B->SetNumberField(TEXT("keys"),Keys.Num());
        double XY=0.,MinZ=DBL_MAX,MaxZ=-DBL_MAX;
        for(const FTransform& K:Keys)
        {
            if(!Keys.IsEmpty()) XY=FMath::Max(XY,FVector::Dist2D(K.GetLocation(),Keys[0].GetLocation()));
            MinZ=FMath::Min(MinZ,K.GetLocation().Z); MaxZ=FMath::Max(MaxZ,K.GetLocation().Z);
        }
        B->SetNumberField(TEXT("max_local_xy_displacement_cm"),XY); B->SetNumberField(TEXT("local_z_min_cm"),MinZ); B->SetNumberField(TEXT("local_z_max_cm"),MaxZ);
        Motion.Add(MakeShared<FJsonValueObject>(B));
    }
    J->SetArrayField(TEXT("native_track_motion"),Motion); return J;
}
bool SameData(UAnimSequence* A,UAnimSequence* B,bool bExceptRootXY,FString& Error,double& MaxPosition,double& MaxRotation)
{
    const IAnimationDataModel* X=A->GetDataModel(); const IAnimationDataModel* Y=B->GetDataModel();
    if(!X || !Y || X->GetFrameRate()!=Y->GetFrameRate() || X->GetNumberOfFrames()!=Y->GetNumberOfFrames() || X->GetNumberOfKeys()!=Y->GetNumberOfKeys())
        { Error=TEXT("Exact data-model timing differs"); return false; }
    TArray<FName> NX,NY; X->GetBoneTrackNames(NX); Y->GetBoneTrackNames(NY);
    if(NX!=NY) { Error=TEXT("Exact native track identities differ"); return false; }
    IAnimationDataModel::FGuidGenerationSettings Other; Other.bIncludeBoneData=0;
    if(X->GenerateGuid(Other)!=Y->GenerateGuid(Other)) { Error=TEXT("Curve/attribute/timing data GUID differs"); return false; }
    for(FName N:NX)
    {
        TArray<FTransform> KX,KY; X->GetBoneTrackTransforms(N,KX); Y->GetBoneTrackTransforms(N,KY);
        if(KX.Num()!=X->GetNumberOfKeys() || KY.Num()!=KX.Num()) { Error=TEXT("Incomplete full native track keys: ")+N.ToString(); return false; }
        for(int32 I=0;I<KX.Num();++I)
        {
            FTransform Expected=KX[I];
            if(bExceptRootXY && N==TEXT("root")) { FVector V=Expected.GetLocation(); V.X=KX[0].GetLocation().X; V.Y=KX[0].GetLocation().Y; Expected.SetLocation(V); }
            MaxPosition=FMath::Max(MaxPosition,FVector::Distance(Expected.GetLocation(),KY[I].GetLocation()));
            MaxRotation=FMath::Max(MaxRotation,FMath::RadiansToDegrees(Expected.GetRotation().AngularDistance(KY[I].GetRotation())));
            if(!Expected.GetScale3D().Equals(KY[I].GetScale3D(),1.e-6) || MaxPosition>.0001 || MaxRotation>.0001)
                { Error=FString::Printf(TEXT("Native full-track preservation failed: %s frame %d"),*N.ToString(),I); return false; }
        }
    }
    return true;
}
bool CleanSequenceCopy(UAnimSequence* A,UAnimSequence* B,USkeleton* NewSK,USkeletalMesh* NewMesh,FString& Error)
{
    if(!A->RetargetSource.IsNone()) { Error=TEXT("Unexpected named retarget pose; do not discard it"); return false; }
    B->SetSkeleton(NewSK); B->SetPreviewMesh(NewMesh); B->PreviewPoseAsset=nullptr; B->RefPoseSeq=nullptr;
    B->RetargetSource=NAME_None; B->ClearRetargetSourceAsset();
    // Source Foley notifies are deliberately excluded from the seven-package
    // locomotion copy. Emptying the event array alone leaves their duplicated
    // instanced UObjects inside the new animation package. Detach only those
    // exact copied instances, never the original sample objects or its data.
    TSet<UObject*> DiscardedNotifies;
    for(const FAnimNotifyEvent& Event:B->Notifies)
    {
        if(Event.Notify) DiscardedNotifies.Add(Event.Notify);
        if(Event.NotifyStateClass) DiscardedNotifies.Add(Event.NotifyStateClass);
    }
    for(UObject* Notify:DiscardedNotifies)
        if(!Notify->IsIn(B)) { Error=TEXT("Unexpected non-owned duplicated notify; source untouched"); return false; }
    B->Notifies.Empty();
    // Keep the original editor track indices for authored sync markers.
    // RefreshCacheData rebuilds those marker pointers and requires the tracks.
    for(FAnimNotifyTrack& Track:B->AnimNotifyTracks) Track.Notifies.Empty();
    B->RefreshCacheData(); B->EmptyMetaData();
    for(UObject* Notify:DiscardedNotifies)
    {
        const FName DetachedName=MakeUniqueObjectName(GetTransientPackage(),Notify->GetClass(),Notify->GetFName());
        if(!Notify->Rename(*DetachedName.ToString(),GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional))
            { Error=TEXT("Cannot detach copied unused Foley notify; helper has not saved"); return false; }
        Notify->ClearFlags(RF_Public|RF_Standalone); Notify->SetFlags(RF_Transient);
    }
    B->BoneCompressionSettings=FAnimationUtils::GetDefaultAnimationBoneCompressionSettings();
    B->CurveCompressionSettings=FAnimationUtils::GetDefaultAnimationCurveCompressionSettings();
    if(const auto* Data=B->GetAssetUserDataArray()) { const auto Copy=*Data; for(UAssetUserData* D:Copy) if(D) B->RemoveUserDataOfClass(D->GetClass()); }
    return true;
}
bool CleanHardReferences(const TArray<UObject*>& Copies,FString& Error)
{
    TSet<FString> Allowed; for(UObject* O:Copies) Allowed.Add(O->GetOutermost()->GetName());
    for(UObject* O:Copies)
    {
        bool Good=true;
        ForEachObjectWithPackage(O->GetOutermost(),[&](UObject* Child)
        {
            TArray<UObject*> Refs; FReferenceFinder Finder(Refs,nullptr,false,true,false,true); Finder.FindReferences(Child);
            for(UObject* Ref:Refs) if(Ref)
            {
                const FString P=Ref->GetOutermost()->GetName();
                if(P.StartsWith(TEXT("/Game/")) && !Allowed.Contains(P)) { Error=TEXT("Unclean copied hard reference: ")+Child->GetPathName()+TEXT(" -> ")+Ref->GetPathName(); Good=false; }
            }
            return true;
        },EGetObjectsFlags::IncludeNestedObjects);
        if(!Good) return false;
    }
    return true;
}
bool SaveNew(UObject* O,FString& Error,const TCHAR* AssetOwner=Owner)
{
    UPackage* P=O->GetOutermost(); const FString File=LocalFile(P->GetName());
    if(IFileManager::Get().FileExists(*File)) { Error=TEXT("Refuse to overwrite ")+File; return false; }
    P->GetMetaData().SetValue(O,TEXT("HarborCityOwnedBy"),AssetOwner);
    FAssetRegistryModule::AssetCreated(O); O->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
    if(!UPackage::SavePackage(P,O,*File,Args)) { Error=TEXT("Native save failed: ")+File; return false; }
    return IFileManager::Get().FileExists(*File);
}
struct FScopedGASMounts
{
    FScopedGASMounts() { for(const auto& S:SubRoots) FPackageName::RegisterMountPoint(TEXT("/Game/")+S,SourceDisk+S); }
    ~FScopedGASMounts() { for(const auto& S:SubRoots) FPackageName::UnRegisterMountPoint(TEXT("/Game/")+S,SourceDisk+S); }
};

bool TransitionRoot(const FString& Root)
{
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/GASTransitionSource/Batch_");
    if(!Root.StartsWith(Prefix)||Root.Len()!=Prefix.Len()+12) return false;
    for(TCHAR C:Root.Right(12)) if(!FChar::IsHexDigit(C)||FChar::ToLower(C)!=C) return false;
    return true;
}

bool ValidateCutPlan(const FString& Path,FString& Error)
{
    const FString Required=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/research/20260924_90723b8c_GAS_TRANSITION_CUT_PROPOSAL.json");
    FString Text; TSharedPtr<FJsonObject> Plan;
    if(!FPaths::IsSamePath(Path,Required)||!FFileHelper::LoadFileToString(Text,*Path)
        ||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Plan)
        ||Plan->GetStringField(TEXT("status"))!=TEXT("PROPOSAL_NOT_APPLIED"))
    { Error=TEXT("Exact reviewed cut proposal is required"); return false; }
    TSet<FString> Found;
    for(const auto& Value:Plan->GetArrayField(TEXT("selections")))
    {
        const auto Row=Value->AsObject();
        if(!Row.IsValid()) { Error=TEXT("Invalid cut-plan row"); return false; }
        const FString Source=Row->GetStringField(TEXT("source_path"));
        if(Source==OriginalClip(TEXT("Idle/M_Relaxed_Stand_Idle_Break_v02"))) continue; // Reviewed but intentionally not migrated.
        const FCutSpec* Match=Cuts.FindByPredicate([&](const FCutSpec& C){return Source==OriginalClip(C.Suffix);});
        if(!Match || Found.Contains(Source)) { Error=TEXT("Unexpected or duplicate selected transition"); return false; }
        const auto& Frames=Row->GetArrayField(TEXT("original_frame_range_inclusive"));
        if(Frames.Num()!=2 || Frames[0]->AsNumber()!=Match->First || Frames[1]->AsNumber()!=Match->Last
            || Row->GetNumberField(TEXT("original_rate_hz"))!=30)
        { Error=TEXT("Compiled cut boundary differs from reviewed source frames"); return false; }
        Found.Add(Source);
    }
    if(Found.Num()!=Cuts.Num()) { Error=TEXT("Incomplete ten-clip cut proposal"); return false; }
    return true;
}

struct FPreparedCut
{
    TArray<FName> Bones;
    TArray<TArray<FTransform>> Tracks;
    TArray<FFloatCurve> Curves;
    TArray<FAnimSyncMarker> Markers;
    FTransform RootReference;
    double MaxCurveError=0.;
    int32 CurveSamples=0;
    double MaxFloatEulerRoundTrip=0.,MaxNormalizedDoubleEulerRoundTrip=0.;
    FName WorstFloatEulerBone,WorstNormalizedDoubleEulerBone;
    int32 WorstFloatEulerFrame=INDEX_NONE,WorstNormalizedDoubleEulerFrame=INDEX_NONE;
};

// UE 5.8's Sequencer data controller uses two different rotation write paths:
// SetBoneCurveKeys calls FQuat4f::Euler directly, while UpdateBoneCurveKeys
// widens and normalizes the quaternion before double-precision Euler conversion.
// Measure both against the actual source keys; these predictions do not replace
// the strict readback of all transforms after the real controller operation.
void MeasureControllerRotationRoundTrip(const FTransform& Transform,FName Bone,int32 SourceFrame,FPreparedCut& Cut)
{
    const FQuat Expected=Transform.GetRotation();
    const FQuat4f Narrowed(Expected);
    const FQuat FloatEulerResult=FQuat::MakeFromEuler(FVector(Narrowed.Euler())).GetNormalized();
    FQuat Normalized(Narrowed); Normalized.Normalize();
    const FQuat DoubleEulerResult=FQuat::MakeFromEuler(FVector(FVector3f(Normalized.Euler()))).GetNormalized();
    const double OldError=FMath::RadiansToDegrees(Expected.AngularDistance(FloatEulerResult));
    const double NewError=FMath::RadiansToDegrees(Expected.AngularDistance(DoubleEulerResult));
    if(OldError>Cut.MaxFloatEulerRoundTrip)
    { Cut.MaxFloatEulerRoundTrip=OldError; Cut.WorstFloatEulerBone=Bone; Cut.WorstFloatEulerFrame=SourceFrame; }
    if(NewError>Cut.MaxNormalizedDoubleEulerRoundTrip)
    { Cut.MaxNormalizedDoubleEulerRoundTrip=NewError; Cut.WorstNormalizedDoubleEulerBone=Bone; Cut.WorstNormalizedDoubleEulerFrame=SourceFrame; }
}

void AddRotationWriteAudit(const FPreparedCut& Cut,const TSharedRef<FJsonObject>& Audit)
{
    Audit->SetNumberField(TEXT("predicted_float_euler_write_max_error_deg"),Cut.MaxFloatEulerRoundTrip);
    Audit->SetStringField(TEXT("predicted_float_euler_worst_bone"),Cut.WorstFloatEulerBone.ToString());
    Audit->SetNumberField(TEXT("predicted_float_euler_worst_source_frame"),Cut.WorstFloatEulerFrame);
    Audit->SetNumberField(TEXT("predicted_normalized_double_euler_write_max_error_deg"),Cut.MaxNormalizedDoubleEulerRoundTrip);
    Audit->SetStringField(TEXT("predicted_normalized_double_euler_worst_bone"),Cut.WorstNormalizedDoubleEulerBone.ToString());
    Audit->SetNumberField(TEXT("predicted_normalized_double_euler_worst_source_frame"),Cut.WorstNormalizedDoubleEulerFrame);
    Audit->SetStringField(TEXT("controller_write_path"),TEXT("SetBoneTrackKeys clears old range; UpdateBoneTrackKeys restores all selected keys through normalized double Euler"));
}

// A boundary inside an unweighted cubic is split using its actual Hermite derivative.
// Weighted segments whose boundaries are not keys require Bezier splitting; reject them explicitly.
bool BoundaryKey(const FRichCurve& Curve,float Time,FRichCurveKey& Out,FString& Error)
{
    const auto& Keys=Curve.GetConstRefOfKeys();
    for(const FRichCurveKey& Key:Keys) if(Key.Time==Time) { Out=Key; return true; }
    Out=FRichCurveKey(Time,Curve.Eval(Time)); Out.TangentMode=RCTM_Break;
    if(Keys.IsEmpty()) return true;
    if(Keys.Num()==1) { Out.InterpMode=RCIM_Constant; return true; }
    if(Time<Keys[0].Time || Time>Keys.Last().Time)
    {
        const bool Before=Time<Keys[0].Time;
        const auto Extrap=Before?Curve.PreInfinityExtrap:Curve.PostInfinityExtrap;
        if(Extrap!=RCCE_Constant && Extrap!=RCCE_Linear)
        { Error=TEXT("Unsupported cyclic curve extrapolation at cut boundary"); return false; }
        Out.InterpMode=Extrap==RCCE_Constant?RCIM_Constant:RCIM_Linear;
        if(Extrap==RCCE_Linear)
        {
            const FRichCurveKey& A=Before?Keys[0]:Keys[Keys.Num()-2];
            const FRichCurveKey& B=Before?Keys[1]:Keys.Last();
            const float DT=B.Time-A.Time;
            Out.ArriveTangent=Out.LeaveTangent=DT>UE_SMALL_NUMBER?(B.Value-A.Value)/DT:0.f;
        }
        return true;
    }
    for(int32 I=0;I+1<Keys.Num();++I)
    {
        const FRichCurveKey& A=Keys[I]; const FRichCurveKey& B=Keys[I+1];
        if(!(A.Time<Time && Time<B.Time)) continue;
        Out.InterpMode=A.InterpMode;
        if(A.InterpMode==RCIM_Constant) return true;
        const double H=double(B.Time)-A.Time;
        if(H<=0) { Error=TEXT("Non-increasing source curve keys"); return false; }
        if(A.InterpMode==RCIM_Linear)
        { Out.ArriveTangent=Out.LeaveTangent=float((B.Value-A.Value)/H); return true; }
        if(A.InterpMode!=RCIM_Cubic)
        { Error=TEXT("Unknown boundary interpolation mode"); return false; }
        if(A.TangentWeightMode==RCTWM_WeightedLeave || A.TangentWeightMode==RCTWM_WeightedBoth
            || B.TangentWeightMode==RCTWM_WeightedArrive || B.TangentWeightMode==RCTWM_WeightedBoth)
        { Error=TEXT("Cut crosses a weighted cubic segment; no guessed tangent conversion"); return false; }
        const double T=(Time-A.Time)/H, T2=T*T;
        const double Slope=((6*T2-6*T)*A.Value+(3*T2-4*T+1)*H*A.LeaveTangent
            +(-6*T2+6*T)*B.Value+(3*T2-2*T)*H*B.ArriveTangent)/H;
        Out.ArriveTangent=Out.LeaveTangent=float(Slope); return true;
    }
    Error=TEXT("Could not locate curve cut boundary"); return false;
}

bool CutCurve(const FRichCurve& Source,float First,float Last,FRichCurve& Output,
    double& MaxError,int32& SampleCount,FString& Error)
{
    Output=Source;
    const auto& Original=Source.GetConstRefOfKeys();
    if(Original.IsEmpty()) return true; // Preserve actual empty/default curve, not invented zero keys.
    TArray<FRichCurveKey> Keys; FRichCurveKey Left,Right;
    if(!BoundaryKey(Source,First,Left,Error)||!BoundaryKey(Source,Last,Right,Error)) return false;
    Keys.Add(Left);
    for(const auto& K:Original) if(K.Time>First && K.Time<Last) Keys.Add(K);
    Keys.Add(Right);
    for(auto& K:Keys)
    {
        K.Time-=First;
        // SetKeys calls AutoSetTangents. Freezing the actual numeric tangents avoids
        // changing retained curve segments simply because a neighbour was cropped.
        if(K.InterpMode==RCIM_Cubic || K.TangentMode==RCTM_Auto || K.TangentMode==RCTM_SmartAuto)
            K.TangentMode=RCTM_Break;
    }
    Output.SetKeys(Keys);
    TArray<float> Times={First,Last};
    for(const auto& K:Original) if(K.Time>=First && K.Time<=Last) Times.AddUnique(K.Time);
    // Include dense samples plus interior points in every retained curve segment.
    const int32 Steps=FMath::Max(1,FMath::CeilToInt((Last-First)*240.f));
    for(int32 I=1;I<Steps;++I) Times.Add(First+(Last-First)*float(I)/Steps);
    for(int32 I=0;I+1<Keys.Num();++I) for(int32 J=1;J<8;++J)
        Times.Add(First+FMath::Lerp(Keys[I].Time,Keys[I+1].Time,float(J)/8));
    for(float T:Times)
    {
        const double Expected=Source.Eval(T),Actual=Output.Eval(T-First),Difference=FMath::Abs(Expected-Actual);
        MaxError=FMath::Max(MaxError,Difference); ++SampleCount;
        if(!FMath::IsFinite(Expected)||!FMath::IsFinite(Actual)||Difference>1.e-4+FMath::Abs(Expected)*2.e-6)
        { Error=FString::Printf(TEXT("Curve function changed at %.9g: %.9g -> %.9g"),T,Expected,Actual); return false; }
    }
    return true;
}

bool PrepareCut(UAnimSequence* Source,const FCutSpec& Spec,FPreparedCut& Cut,FString& Error)
{
    const IAnimationDataModel* M=Source?Source->GetDataModel():nullptr;
    if(!M || M->GetFrameRate()!=FFrameRate(30,1) || Spec.First<0 || Spec.First>=Spec.Last
        || Spec.Last>=M->GetNumberOfKeys() || M->GetNumberOfKeys()>10000 || M->GetNumBoneTracks()>400
        || M->GetNumberOfFloatCurves()>64 || M->GetNumberOfTransformCurves()!=0 || M->GetNumberOfAttributes()!=0
        || Source->RateScale!=1 || !Source->GetSkeleton())
    { Error=TEXT("Source timing/schema exceeds exact cut support (transform curves/attributes are not discarded)"); return false; }
    const FReferenceSkeleton& Ref=Source->GetSkeleton()->GetReferenceSkeleton();
    const int32 Root=Ref.FindBoneIndex(TEXT("root"));
    if(Root!=0 || Ref.GetParentIndex(Root)!=INDEX_NONE)
    { Error=TEXT("Expected independent floor-root at index zero"); return false; }
    Cut.RootReference=Ref.GetRefBonePose()[Root];
    if(Cut.RootReference.ContainsNaN()||!Cut.RootReference.GetScale3D().Equals(FVector::OneVector,1.e-6))
    { Error=TEXT("Unexpected source root reference scale"); return false; }
    M->GetBoneTrackNames(Cut.Bones);
    if(!Cut.Bones.Contains(TEXT("root"))||!Cut.Bones.Contains(TEXT("pelvis")))
    { Error=TEXT("Missing independent root or pelvis track"); return false; }
    for(FName Bone:Cut.Bones)
    {
        TArray<FTransform> Original; M->GetBoneTrackTransforms(Bone,Original);
        if(Original.Num()!=M->GetNumberOfKeys()) { Error=TEXT("Incomplete native bone keys"); return false; }
        TArray<FTransform> Selected;
        for(int32 I=Spec.First;I<=Spec.Last;++I)
        {
            FTransform T=Original[I];
            if(T.ContainsNaN()) { Error=TEXT("Nonfinite source bone transform"); return false; }
            if(Bone==TEXT("root"))
            {
                if(!T.GetScale3D().Equals(FVector::OneVector,1.e-6)) { Error=TEXT("Animated root scale is not supported"); return false; }
                T.SetTranslation(Cut.RootReference.GetTranslation()); T.SetRotation(Cut.RootReference.GetRotation());
            }
            MeasureControllerRotationRoundTrip(T,Bone,I,Cut);
            Selected.Add(T);
        }
        Cut.Tracks.Add(MoveTemp(Selected));
    }
    const float First=float(Spec.First)/30.f,Last=float(Spec.Last)/30.f;
    for(const FFloatCurve& Curve:M->GetFloatCurves())
    {
        FFloatCurve Selected=Curve;
        if(!CutCurve(Curve.FloatCurve,First,Last,Selected.FloatCurve,Cut.MaxCurveError,Cut.CurveSamples,Error))
        { Error=Curve.GetName().ToString()+TEXT(": ")+Error; return false; }
        Cut.Curves.Add(MoveTemp(Selected));
    }
    for(const FAnimSyncMarker& Marker:Source->AuthoredSyncMarkers)
    {
        if(Marker.Time<First || Marker.Time>Last) continue;
        if(Marker.TrackIndex<0 || Marker.TrackIndex>=Source->AnimNotifyTracks.Num())
        { Error=TEXT("Source marker has invalid editor track"); return false; }
        FAnimSyncMarker Selected=Marker; Selected.Time-=First; Cut.Markers.Add(Selected);
    }
    return true;
}

TSharedRef<FJsonObject> CurveKeyAudit(const FRichCurveKey& Key)
{
    auto J=MakeShared<FJsonObject>();
    const auto Number=[&](const TCHAR* Name,float Value)
    { if(FMath::IsFinite(Value)) J->SetNumberField(Name,Value); else J->SetStringField(Name,TEXT("NON_FINITE")); };
    Number(TEXT("time_seconds"),Key.Time); Number(TEXT("value"),Key.Value);
    Number(TEXT("arrive_tangent"),Key.ArriveTangent); Number(TEXT("leave_tangent"),Key.LeaveTangent);
    Number(TEXT("arrive_tangent_weight"),Key.ArriveTangentWeight); Number(TEXT("leave_tangent_weight"),Key.LeaveTangentWeight);
    J->SetNumberField(TEXT("interpolation"),Key.InterpMode); J->SetNumberField(TEXT("tangent_mode"),Key.TangentMode);
    J->SetNumberField(TEXT("tangent_weight_mode"),Key.TangentWeightMode); return J;
}

uint64 FloatUlpDistance(float A,float B)
{
    if(A==B) return 0; // Includes equivalent positive/negative zero.
    if(!FMath::IsFinite(A)||!FMath::IsFinite(B)) return MAX_uint64;
    uint32 X,Y; FMemory::Memcpy(&X,&A,sizeof(X)); FMemory::Memcpy(&Y,&B,sizeof(Y));
    X=(X&0x80000000u)?~X:(X|0x80000000u); Y=(Y&0x80000000u)?~Y:(Y|0x80000000u);
    return X>Y?uint64(X)-Y:uint64(Y)-X;
}

// The Sequencer helper is private/non-exported. Predict its actual UE 5.8
// RichCurve -> float-channel -> RichCurve representation here, without changing
// what is written. Arithmetic order follows AnimSequencerHelpers.cpp 85-229:
// forward seconds-delta/division is float, inverse timing ratio is double.
// The later evaluated-function guard still compares against the prepared cut,
// so this representation check cannot legitimize a changed curve shape.
bool PredictNativeCurveKeys(const TArray<FRichCurveKey>& Keys,const FFrameRate& Rate,
    TArray<FRichCurveKey>& Predicted,FString& Error)
{
    Predicted=Keys; TArray<FFrameNumber> Frames; TArray<double> Seconds;
    for(const auto& K:Keys)
    {
        if(!FMath::IsFinite(K.Time)||!FMath::IsFinite(K.Value)||!FMath::IsFinite(K.ArriveTangent)
            || !FMath::IsFinite(K.LeaveTangent)||!FMath::IsFinite(K.ArriveTangentWeight)||!FMath::IsFinite(K.LeaveTangentWeight))
        { Error=TEXT("Nonfinite prepared curve key"); return false; }
        const FFrameNumber Frame=Rate.AsFrameTime(K.Time).RoundToFrame();
        if(!Frames.IsEmpty() && Frame.Value<=Frames.Last().Value)
        { Error=TEXT("Curve keys collide or reverse after native frame quantization"); return false; }
        Frames.Add(Frame); Seconds.Add(Rate.AsSeconds(Frame));
    }
    for(int32 I=0;I<Keys.Num();++I)
    {
        const auto& K=Keys[I]; auto& P=Predicted[I]; P.Time=float(Seconds[I]);
        const int32 L=I>0?I-1:I,R=I+1<Keys.Num()?I+1:I;
        const bool Neighbours=L!=R;
        const float SourceSecondsDelta=Neighbours?Keys[R].Time-Keys[L].Time:1.f;
        const int32 FrameDelta=Neighbours?Frames[R].Value-Frames[L].Value:1;
        const double NativeSecondsDelta=Neighbours?Seconds[R]-Seconds[L]:1.;
        // Keep the intermediate float division used by the actual converter.
        const double WriteRatio=Neighbours?double(SourceSecondsDelta/FrameDelta):1.;
        const double ReadRatio=Neighbours?double(FrameDelta)/NativeSecondsDelta:1.;
        const double TangentWriteRatio=K.TangentWeightMode==RCTWM_WeightedNone?WriteRatio:Rate.AsInterval();
        float Arrive=float(K.ArriveTangent*TangentWriteRatio),Leave=float(K.LeaveTangent*TangentWriteRatio);
        if(K.InterpMode==RCIM_Linear)
        {
            P.TangentWeightMode=RCTWM_WeightedNone;
            if(I>0) Arrive=0.f;
            if(I+1<Keys.Num()) Leave=float((Keys[I+1].Value-K.Value)/FMath::Max<double>(KINDA_SMALL_NUMBER,Frames[I+1].Value-Frames[I].Value));
        }
        else if(K.InterpMode==RCIM_Cubic && I>0 && Keys[I-1].InterpMode==RCIM_Linear)
        {
            P.TangentWeightMode=(K.TangentWeightMode==RCTWM_WeightedBoth||K.TangentWeightMode==RCTWM_WeightedLeave)?RCTWM_WeightedLeave:RCTWM_WeightedNone;
            P.TangentMode=RCTM_Break;
            Arrive=float((K.Value-Keys[I-1].Value)/FMath::Max<double>(KINDA_SMALL_NUMBER,Frames[I].Value-Frames[I-1].Value));
        }
        P.ArriveTangent=float(Arrive*ReadRatio); P.LeaveTangent=float(Leave*ReadRatio);
    }
    return true;
}

bool VerifyCurveReadback(const FFloatCurve& Expected,const FFloatCurve* Actual,const FFrameRate& ModelRate,
    float Duration,const TSharedRef<FJsonObject>& Audit,FString& Error)
{
    Audit->SetStringField(TEXT("curve"),Expected.GetName().ToString()); Audit->SetBoolField(TEXT("actual_exists"),Actual!=nullptr);
    const auto Fail=[&](const FString& Field)
    { Audit->SetStringField(TEXT("status"),TEXT("FAIL")); Audit->SetStringField(TEXT("failed_field"),Field);
      Error=TEXT("Native curve readback differs: ")+Expected.GetName().ToString()+TEXT(" / ")+Field; return false; };
    if(!Actual) return Fail(TEXT("missing curve"));
    const FRichCurve& E=Expected.FloatCurve; const FRichCurve& A=Actual->FloatCurve;
    const auto& EK=E.GetConstRefOfKeys(); const auto& AK=A.GetConstRefOfKeys();
    Audit->SetNumberField(TEXT("expected_flags"),Expected.GetCurveTypeFlags()); Audit->SetNumberField(TEXT("actual_flags"),Actual->GetCurveTypeFlags());
    if(FMath::IsFinite(E.DefaultValue)) Audit->SetNumberField(TEXT("expected_default"),E.DefaultValue); else Audit->SetStringField(TEXT("expected_default"),TEXT("NON_FINITE"));
    if(FMath::IsFinite(A.DefaultValue)) Audit->SetNumberField(TEXT("actual_default"),A.DefaultValue); else Audit->SetStringField(TEXT("actual_default"),TEXT("NON_FINITE"));
    Audit->SetNumberField(TEXT("expected_pre_extrapolation"),E.PreInfinityExtrap); Audit->SetNumberField(TEXT("actual_pre_extrapolation"),A.PreInfinityExtrap);
    Audit->SetNumberField(TEXT("expected_post_extrapolation"),E.PostInfinityExtrap); Audit->SetNumberField(TEXT("actual_post_extrapolation"),A.PostInfinityExtrap);
    Audit->SetNumberField(TEXT("expected_keys"),EK.Num()); Audit->SetNumberField(TEXT("actual_keys"),AK.Num());
    if(Actual->GetCurveTypeFlags()!=Expected.GetCurveTypeFlags()) return Fail(TEXT("flags"));
    if(!FMath::IsFinite(E.DefaultValue)||!FMath::IsFinite(A.DefaultValue)||A.DefaultValue!=E.DefaultValue) return Fail(TEXT("default"));
    if(A.PreInfinityExtrap!=E.PreInfinityExtrap || A.PostInfinityExtrap!=E.PostInfinityExtrap) return Fail(TEXT("extrapolation"));
    if(AK.Num()!=EK.Num()) return Fail(TEXT("key count"));
    for(const auto& K:EK) if(!FMath::IsFinite(K.Time)) return Fail(TEXT("nonfinite prepared time"));
    // Match the actual UE 5.8 controller conversion, not a broad source-value
    // epsilon or half-animation-frame allowance. All discrete fields must match
    // the predicted engine representation; tangent rounding is bounded in ULPs.
    const bool OffFrame=EK.ContainsByPredicate([ModelRate](const FRichCurveKey& Key)
    {
        const FFrameTime T=ModelRate.AsFrameTime(Key.Time);
        return !(FMath::IsNearlyZero(T.GetSubFrame(),KINDA_SMALL_NUMBER)||FMath::IsNearlyEqual(T.GetSubFrame(),1.f,KINDA_SMALL_NUMBER));
    });
    const FFrameRate ChannelRate=OffFrame?FFrameRate(240000,1):ModelRate;
    TArray<FRichCurveKey> NativeKeys; FString PredictionError;
    if(!PredictNativeCurveKeys(EK,ChannelRate,NativeKeys,PredictionError)) return Fail(PredictionError);
    Audit->SetNumberField(TEXT("predicted_native_channel_hz"),ChannelRate.AsDecimal());
    Audit->SetStringField(TEXT("time_rule"),TEXT("Exact float(ChannelRate.AsSeconds(ChannelRate.AsFrameTime(input).RoundToFrame())); no general tolerance"));
    Audit->SetStringField(TEXT("tangent_rule"),TEXT("Cubic tangents within 4 float32 ULP of UE5.8 predicted channel roundtrip, not source tangent. Native linear-neighbour mode normalization predicted explicitly. Values and weight values exact; original function tolerance unchanged."));
    double MaxTimeError=0.,MaxTangentRepresentationError=0.; uint64 MaxTangentPredictionUlps=0; int32 TimeChanges=0; FRows ChangedKeys;
    for(int32 I=0;I<EK.Num();++I)
    {
        const auto& K=EK[I]; const auto& V=AK[I];
        if(!FMath::IsFinite(K.Time)||!FMath::IsFinite(V.Time)) return Fail(TEXT("nonfinite time"));
        const FFrameNumber Frame=ChannelRate.AsFrameTime(K.Time).RoundToFrame();
        const auto& Canonical=NativeKeys[I]; const float CanonicalTime=Canonical.Time;
        const double Difference=FMath::Abs(double(K.Time)-V.Time); MaxTimeError=FMath::Max(MaxTimeError,Difference);
        if(K.Time!=V.Time) ++TimeChanges;
        // RichCurveKey::operator== omits weights and cannot account for numeric
        // roundtrip error. Check each active field instead; inactive noncubic
        // tangents are generated by the native controller and do not drive Eval.
        const bool WeightsEqual=Canonical.ArriveTangentWeight==V.ArriveTangentWeight && Canonical.LeaveTangentWeight==V.LeaveTangentWeight;
        const uint64 TangentUlps=FMath::Max(FloatUlpDistance(Canonical.ArriveTangent,V.ArriveTangent),FloatUlpDistance(Canonical.LeaveTangent,V.LeaveTangent));
        const bool Cubic=Canonical.InterpMode==RCIM_Cubic;
        if(Cubic && FMath::IsFinite(V.ArriveTangent) && FMath::IsFinite(V.LeaveTangent))
        {
            MaxTangentPredictionUlps=FMath::Max(MaxTangentPredictionUlps,TangentUlps);
            MaxTangentRepresentationError=FMath::Max(MaxTangentRepresentationError,FMath::Max(FMath::Abs(double(K.ArriveTangent)-V.ArriveTangent),FMath::Abs(double(K.LeaveTangent)-V.LeaveTangent)));
        }
        const bool Matches=Canonical.Time==V.Time && Canonical.Value==V.Value && Canonical.InterpMode==V.InterpMode
            && Canonical.TangentMode==V.TangentMode && Canonical.TangentWeightMode==V.TangentWeightMode
            && (!Cubic || TangentUlps<=4) && WeightsEqual && FMath::IsFinite(K.Value)
            && FMath::IsFinite(K.ArriveTangent) && FMath::IsFinite(K.LeaveTangent)
            && FMath::IsFinite(K.ArriveTangentWeight) && FMath::IsFinite(K.LeaveTangentWeight) && FMath::IsFinite(V.Value)
            && FMath::IsFinite(V.ArriveTangent) && FMath::IsFinite(V.LeaveTangent)
            && FMath::IsFinite(V.ArriveTangentWeight) && FMath::IsFinite(V.LeaveTangentWeight)
            && (I==0 || V.Time>AK[I-1].Time);
        if((K!=V || !WeightsEqual) && ChangedKeys.Num()<8)
        {
            auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("key_index"),I);
            J->SetObjectField(TEXT("prepared_cut"),CurveKeyAudit(K)); J->SetObjectField(TEXT("actual_readback"),CurveKeyAudit(V));
            J->SetObjectField(TEXT("predicted_native_key"),CurveKeyAudit(Canonical));
            if(Cubic && TangentUlps!=MAX_uint64) J->SetNumberField(TEXT("native_tangent_prediction_error_ulps"),double(TangentUlps));
            J->SetNumberField(TEXT("predicted_native_frame"),Frame.Value); J->SetNumberField(TEXT("predicted_native_time_seconds"),CanonicalTime);
            J->SetNumberField(TEXT("time_error_seconds"),Difference); J->SetBoolField(TEXT("matches_guarded_fields"),Matches);
            ChangedKeys.Add(MakeShared<FJsonValueObject>(J));
        }
        Audit->SetArrayField(TEXT("changed_keys_first_eight"),ChangedKeys);
        Audit->SetNumberField(TEXT("maximum_time_representation_error_seconds"),MaxTimeError);
        Audit->SetNumberField(TEXT("maximum_cubic_tangent_representation_error"),MaxTangentRepresentationError);
        Audit->SetNumberField(TEXT("maximum_cubic_tangent_prediction_error_ulps"),double(MaxTangentPredictionUlps));
        Audit->SetNumberField(TEXT("time_keys_canonicalized"),TimeChanges);
        if(!Matches)
        {
            Audit->SetNumberField(TEXT("failed_key_index"),I); Audit->SetObjectField(TEXT("failed_expected_key"),CurveKeyAudit(Canonical));
            Audit->SetObjectField(TEXT("failed_actual_key"),CurveKeyAudit(V)); return Fail(TEXT("key fields or order"));
        }
    }
    // Independently test the resulting function, including original and native
    // key times and seven interior points per segment. Do not accept changed
    // curve behaviour just because its time representation is explainable.
    TArray<float> Times={0.f,Duration};
    for(const auto& K:EK) Times.AddUnique(K.Time);
    for(const auto& K:AK) Times.AddUnique(K.Time);
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(Duration*240.f));
    for(int32 I=1;I<Steps;++I) Times.Add(Duration*float(I)/Steps);
    for(int32 I=0;I+1<EK.Num();++I) for(int32 J=1;J<8;++J) Times.Add(FMath::Lerp(EK[I].Time,EK[I+1].Time,float(J)/8));
    double MaxError=0.; int32 Samples=0;
    for(float T:Times)
    {
        const double EV=E.Eval(T),AV=A.Eval(T),Difference=FMath::Abs(EV-AV); ++Samples;
        if(FMath::IsFinite(Difference)) MaxError=FMath::Max(MaxError,Difference);
        Audit->SetNumberField(TEXT("maximum_native_curve_function_error"),MaxError); Audit->SetNumberField(TEXT("native_curve_function_samples"),Samples);
        if(!FMath::IsFinite(EV)||!FMath::IsFinite(AV)||Difference>1.e-4+FMath::Abs(EV)*2.e-6)
        { Audit->SetNumberField(TEXT("failed_function_sample_seconds"),T); return Fail(TEXT("evaluated function")); }
    }
    Audit->SetStringField(TEXT("status"),TEXT("PASS")); return true;
}

bool VerifyCut(UAnimSequence* Target,const FCutSpec& Spec,const FPreparedCut& Cut,
    const TSharedRef<FJsonObject>& Audit,FString& Error)
{
    const IAnimationDataModel* M=Target?Target->GetDataModel():nullptr;
    if(!M || M->GetFrameRate()!=FFrameRate(30,1) || M->GetNumberOfFrames()!=Spec.Last-Spec.First
        || M->GetNumberOfKeys()!=Spec.Last-Spec.First+1 || Target->RateScale!=1
        || Target->bEnableRootMotion || Target->bForceRootLock || !Target->Notifies.IsEmpty())
    { Error=TEXT("Cut time/rate/root extraction/notifies mismatch"); return false; }
    TArray<FName> Names; M->GetBoneTrackNames(Names);
    if(Names!=Cut.Bones || M->GetNumberOfFloatCurves()!=Cut.Curves.Num()
        || M->GetNumberOfTransformCurves()!=0 || M->GetNumberOfAttributes()!=0)
    { Error=TEXT("Cut data-model schema mismatch"); return false; }
    double MaxPosition=0.,MaxAngle=0.;
    for(int32 Bone=0;Bone<Names.Num();++Bone)
    {
        TArray<FTransform> Actual; M->GetBoneTrackTransforms(Names[Bone],Actual);
        if(Actual.Num()!=Cut.Tracks[Bone].Num()) { Error=TEXT("Cut bone key count mismatch"); return false; }
        for(int32 I=0;I<Actual.Num();++I)
        {
            const FTransform& Expected=Cut.Tracks[Bone][I];
            MaxPosition=FMath::Max(MaxPosition,FVector::Distance(Expected.GetLocation(),Actual[I].GetLocation()));
            MaxAngle=FMath::Max(MaxAngle,FMath::RadiansToDegrees(Expected.GetRotation().AngularDistance(Actual[I].GetRotation())));
            if(MaxPosition>.0001 || MaxAngle>.0001 || !Expected.GetScale3D().Equals(Actual[I].GetScale3D(),1.e-6))
            {
                auto Failure=MakeShared<FJsonObject>();
                Failure->SetStringField(TEXT("bone"),Names[Bone].ToString());
                Failure->SetNumberField(TEXT("source_frame"),Spec.First+I); Failure->SetNumberField(TEXT("cut_frame"),I);
                Failure->SetNumberField(TEXT("position_error_cm"),FVector::Distance(Expected.GetLocation(),Actual[I].GetLocation()));
                Failure->SetNumberField(TEXT("rotation_error_deg"),FMath::RadiansToDegrees(Expected.GetRotation().AngularDistance(Actual[I].GetRotation())));
                Failure->SetNumberField(TEXT("scale_max_component_error"),(Expected.GetScale3D()-Actual[I].GetScale3D()).GetAbsMax());
                Failure->SetNumberField(TEXT("maximum_position_error_so_far_cm"),MaxPosition);
                Failure->SetNumberField(TEXT("maximum_rotation_error_so_far_deg"),MaxAngle);
                const auto AddTransform=[&](const TCHAR* Prefix,const FTransform& Transform)
                {
                    FRows Q; for(double V:{Transform.GetRotation().X,Transform.GetRotation().Y,Transform.GetRotation().Z,Transform.GetRotation().W}) Q.Add(MakeShared<FJsonValueNumber>(V));
                    Failure->SetArrayField(FString(Prefix)+TEXT("_quaternion_xyzw"),Q);
                    Failure->SetNumberField(FString(Prefix)+TEXT("_quaternion_norm_squared"),Transform.GetRotation().SizeSquared());
                    Failure->SetStringField(FString(Prefix)+TEXT("_position"),Transform.GetLocation().ToString());
                    Failure->SetStringField(FString(Prefix)+TEXT("_scale"),Transform.GetScale3D().ToString());
                };
                AddTransform(TEXT("expected"),Expected); AddTransform(TEXT("actual"),Actual[I]);
                Audit->SetObjectField(TEXT("failed_transform"),Failure); AddRotationWriteAudit(Cut,Audit);
                Error=FString::Printf(TEXT("Unplanned cut bone transform change: %s source frame %d, position %.12g cm, angle %.12g deg"),
                    *Names[Bone].ToString(),Spec.First+I,MaxPosition,MaxAngle); return false;
            }
        }
    }
    Audit->SetNumberField(TEXT("maximum_unplanned_bone_position_error_cm"),MaxPosition);
    Audit->SetNumberField(TEXT("maximum_unplanned_bone_rotation_error_deg"),MaxAngle); AddRotationWriteAudit(Cut,Audit);
    FRows CurveChecks;
    for(const FFloatCurve& Expected:Cut.Curves)
    {
        const auto* Actual=M->FindFloatCurve(FAnimationCurveIdentifier(Expected.GetName(),ERawCurveTrackTypes::RCT_Float));
        auto CurveAudit=MakeShared<FJsonObject>();
        const bool Good=VerifyCurveReadback(Expected,Actual,M->GetFrameRate(),float(Spec.Last-Spec.First)/30.f,CurveAudit,Error);
        CurveChecks.Add(MakeShared<FJsonValueObject>(CurveAudit)); Audit->SetArrayField(TEXT("curve_readback_checks"),CurveChecks);
        if(!Good) return false;
    }
    if(Target->AuthoredSyncMarkers.Num()!=Cut.Markers.Num()) { Error=TEXT("Cut marker count mismatch"); return false; }
    for(int32 I=0;I<Cut.Markers.Num();++I)
    {
        const auto& A=Target->AuthoredSyncMarkers[I]; const auto& B=Cut.Markers[I];
        if(A.MarkerName!=B.MarkerName || A.TrackIndex!=B.TrackIndex || A.Time!=B.Time)
        { Error=TEXT("Cut sync marker changed beyond window/time shift"); return false; }
    }
    Audit->SetNumberField(TEXT("maximum_unplanned_bone_position_error_cm"),MaxPosition);
    Audit->SetNumberField(TEXT("maximum_unplanned_bone_rotation_error_deg"),MaxAngle);
    Audit->SetNumberField(TEXT("maximum_curve_function_error"),Cut.MaxCurveError);
    Audit->SetNumberField(TEXT("curve_function_samples"),Cut.CurveSamples);
    Audit->SetNumberField(TEXT("retained_sync_markers"),Cut.Markers.Num());
    Audit->SetBoolField(TEXT("curve_numeric_tangents_frozen_for_cropping"),true);
    AddRotationWriteAudit(Cut,Audit);
    return true;
}

bool ApplyCut(UAnimSequence* Target,const FCutSpec& Spec,const FPreparedCut& Cut,FString& Error)
{
    IAnimationDataController& Controller=Target->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Exact GAS transition crop")),false);
    Controller.SetNumberOfFrames(FFrameNumber(Spec.Last-Spec.First),false);
    bool Good=true;
    for(int32 B=0;B<Cut.Bones.Num();++B)
    {
        TArray<FVector3f> Positions,Scales; TArray<FQuat4f> Rotations;
        for(const FTransform& T:Cut.Tracks[B])
        { Positions.Add(FVector3f(T.GetLocation())); Rotations.Add(FQuat4f(T.GetRotation())); Scales.Add(FVector3f(T.GetScale3D())); }
        // Set first removes every old/out-of-window key. Its float Euler path can
        // lose precision near a rotation singularity; update the complete exact
        // new range with the controller's normalized double Euler path. No
        // tolerances are relaxed, and every resulting key is read back below.
        bool Written=Controller.SetBoneTrackKeys(Cut.Bones[B],Positions,Rotations,Scales,false);
        if(Written) Written=Controller.UpdateBoneTrackKeys(Cut.Bones[B],FInt32Range(0,Cut.Tracks[B].Num()),Positions,Rotations,Scales,false);
        Good&=Written;
    }
    for(const auto& Curve:Cut.Curves)
        Good&=Controller.SetCurveKeys(FAnimationCurveIdentifier(Curve.GetName(),ERawCurveTrackTypes::RCT_Float),Curve.FloatCurve.GetConstRefOfKeys(),false);
    Controller.CloseBracket(false);
    if(!Good) { Error=TEXT("Native data controller rejected a crop key write"); return false; }
    // SetNumberOfFrames can clamp old markers. Rebuild from the original window after all model notifications.
    Target->AuthoredSyncMarkers=Cut.Markers; Target->RefreshCacheData();
    Target->RateScale=1.f; Target->bEnableRootMotion=false; Target->bForceRootLock=false;
    return true;
}
}
#endif

FString UHCM5VS2GASTransitionAssetsEditor::PrepareTransitionAssets(const FString& ProbeJsonPath,const FString& CutPlanJsonPath,
    const FString& DestinationRoot,const FString& PriorDryRunJsonPath,bool bApply)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("apply"),bApply); R->SetStringField(TEXT("destination"),DestinationRoot);
    if(!FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),TEXT("D:/科研学习/codex学习/HarborCity/HarborCity.uproject"))
       || !TransitionRoot(DestinationRoot)) return Finish(R,TEXT("Exact project and fresh Transition Batch_<12hex> namespace required"));
    const FString ExpectedProbe=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/probe_20260924_153126_945_90723b8c_GASTransitionReadOnly/author_result.json");
    FString Text; TSharedPtr<FJsonObject> Probe;
    if(!FPaths::IsSamePath(ProbeJsonPath,ExpectedProbe) || !FFileHelper::LoadFileToString(Text,*ProbeJsonPath)
       || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Probe) || Probe->GetStringField(TEXT("status"))!=TEXT("PASS"))
        return Finish(R,TEXT("Require fixed real 31-clip transition original-frame probe"));
    TSet<FString> Probed;
    for(const auto& V:Probe->GetArrayField(TEXT("clips")))
    {
        const auto C=V->AsObject(); if(!C.IsValid() || C->GetStringField(TEXT("status"))!=TEXT("PASS")) return Finish(R,TEXT("Incomplete source probe"));
        Probed.Add(C->GetStringField(TEXT("path")));
    }
    if(Probed.Num()!=31) return Finish(R,TEXT("Exact 31-clip read-only source selection required"));
    TArray<FString> TransitionSuffixes; for(const auto& Cut:Cuts) TransitionSuffixes.Add(Cut.Suffix);
    FString PlanError; if(!ValidateCutPlan(CutPlanJsonPath,PlanError)) return Finish(R,PlanError);
    for(const auto& S:TransitionSuffixes) if(!Probed.Contains(OriginalClip(S))) return Finish(R,TEXT("Probe selection mismatch"));
    for(const auto& S:SubRoots)
    {
        TArray<FString> Files; IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectContentDir()+S),TEXT("*"),true,false);
        if(!Files.IsEmpty()) return Finish(R,TEXT("Do not shadow a destination subtree: ")+S);
    }
    for(TObjectIterator<UPackage> It;It;++It) if(SourcePackage(It->GetName())) return Finish(R,TEXT("Source submount package already loaded: ")+It->GetName());
    TArray<FString> Outputs; for(const auto& S:TransitionSuffixes) Outputs.Add(DestinationRoot+TEXT("/Animations/")+ClipName(S));
    TArray<FString> WorkingOutputs;
    for(const auto& S:TransitionSuffixes) WorkingOutputs.Add(DestinationRoot+TEXT("/Working/")+ClipName(S)+TEXT("_Cut_InPlace"));
    for(const auto& O:WorkingOutputs) if(!Fresh(O)) return Finish(R,TEXT("Preserve existing/partial working output: ")+O);
    for(const auto& O:Outputs) if(!Fresh(O)) return Finish(R,TEXT("Preserve existing/partial output: ")+O);
    auto* NewSK=LoadObject<USkeleton>(nullptr,*ObjectPath(CleanSkeleton));
    auto* NewMesh=LoadObject<USkeletalMesh>(nullptr,*ObjectPath(CleanMesh));
    if(!NewSK || !NewMesh || NewMesh->GetSkeleton()!=NewSK
       || NewSK->GetOutermost()->GetMetaData().GetValue(NewSK,TEXT("HarborCityOwnedBy"))!=FString(Owner)
       || NewMesh->GetOutermost()->GetMetaData().GetValue(NewMesh,TEXT("HarborCityOwnedBy"))!=FString(Owner))
        return Finish(R,TEXT("Existing independently saved clean P0 skeleton/mesh required"));
    FScopedGASMounts Mounts;
    IAssetRegistry& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FString> Scan; for(const auto& S:SubRoots) Scan.Add(TEXT("/Game/")+S.LeftChop(1));
    AR.ScanPathsSynchronous(Scan,true);
    TArray<FString> Queue={SkeletonSource,MeshSource}; for(const auto& S:TransitionSuffixes) Queue.Add(OriginalClip(S));
    TSet<FString> Known;
    while(!Queue.IsEmpty())
    {
        const FString P=Queue.Pop(EAllowShrinking::No);
        if(!P.StartsWith(TEXT("/Game/")) || Known.Contains(P)) continue;
        if(!SourcePackage(P) || Known.Num()>=1000 || !IFileManager::Get().FileExists(*(SourceDisk+P.Mid(6)+TEXT(".uasset"))))
            return Finish(R,TEXT("Unforeseen or missing source dependency; stop before loads: ")+P);
        Known.Add(P); TArray<FName> Deps; AR.GetDependencies(FName(*P),Deps); for(FName D:Deps) Queue.Add(D.ToString());
    }
    TArray<FString> Sorted=Known.Array(); Sorted.Sort(); TArray<TSharedPtr<FJsonValue>> Closure;
    for(const auto& P:Sorted) Closure.Add(MakeShared<FJsonValueString>(P));
    R->SetArrayField(TEXT("source_dependency_packages"),Closure);
    auto* SK=LoadObject<USkeleton>(nullptr,*ObjectPath(SkeletonSource)); auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*ObjectPath(MeshSource));
    if(!SK || !Mesh || Mesh->GetSkeleton()!=SK) return Finish(R,TEXT("Real source mesh/skeleton failed"));
    TArray<UAnimSequence*> Sources; TArray<TSharedPtr<FJsonValue>> Audits; TArray<FPreparedCut> Prepared;
    for(const auto& S:TransitionSuffixes)
    {
        auto* Seq=LoadObject<UAnimSequence>(nullptr,*ObjectPath(OriginalClip(S)));
        if(!Seq || Seq->GetSkeleton()!=SK || Seq->AdditiveAnimType!=AAT_None || !Seq->GetDataModel()
           || !Seq->bEnableRootMotion || !Seq->bForceRootLock || Seq->RootMotionRootLock!=ERootMotionRootLock::RefPose)
            return Finish(R,TEXT("Observed original action flags/skeleton changed: ")+S);
        FPreparedCut Cut; FString CutError;
        if(!PrepareCut(Seq,Cuts[Sources.Num()],Cut,CutError)) return Finish(R,S+TEXT(": ")+CutError);
        auto Audit=Summary(Seq);
        Audit->SetNumberField(TEXT("first_source_frame"),Cuts[Sources.Num()].First);
        Audit->SetNumberField(TEXT("last_source_frame"),Cuts[Sources.Num()].Last);
        Audit->SetNumberField(TEXT("cut_curve_function_samples"),Cut.CurveSamples);
        Audit->SetNumberField(TEXT("cut_curve_function_max_error"),Cut.MaxCurveError);
        Audit->SetNumberField(TEXT("cut_marker_count"),Cut.Markers.Num());
        AddRotationWriteAudit(Cut,Audit);
        Prepared.Add(MoveTemp(Cut)); Sources.Add(Seq); Audits.Add(MakeShared<FJsonValueObject>(Audit));
    }
    R->SetArrayField(TEXT("source_audits"),Audits);
    // Disk registry dependencies can omit the serialized DataAsset references
    // reached while its Blueprint-generated class is loaded. Discover ONLY
    // packages causally reachable from the twelve roots through native object
    // references, and retain the exact referring object for every added edge.
    // This is not permission for every loaded package or the Audio subtree.
    R->SetArrayField(TEXT("initial_registry_dependency_packages"),Closure);
    TArray<FString> Inspect=Known.Array(); TSet<FString> Inspected;
    TArray<TSharedPtr<FJsonValue>> AddedEdges; FString DiscoveryError;
    while(!Inspect.IsEmpty())
    {
        const FString Package=Inspect.Pop(EAllowShrinking::No);
        if(Inspected.Contains(Package)) continue;
        Inspected.Add(Package);
        auto AddReachable=[&](const FString& Next,const FString& Referrer,const FString& Referent,const TCHAR* Method)
        {
            if(!Next.StartsWith(TEXT("/Game/")) || Known.Contains(Next) || !DiscoveryError.IsEmpty()) return;
            if(!SourcePackage(Next) || Known.Num()>=1000 || !IFileManager::Get().FileExists(*(SourceDisk+Next.Mid(6)+TEXT(".uasset"))))
                { DiscoveryError=TEXT("Unforeseen native reference outside source closure: ")+Referrer+TEXT(" -> ")+Referent; return; }
            Known.Add(Next); Inspect.Add(Next);
            auto Edge=MakeShared<FJsonObject>(); Edge->SetStringField(TEXT("from"),Referrer); Edge->SetStringField(TEXT("to"),Referent);
            Edge->SetStringField(TEXT("package"),Next); Edge->SetStringField(TEXT("method"),Method); AddedEdges.Add(MakeShared<FJsonValueObject>(Edge));
        };
        TArray<FName> Deps; AR.GetDependencies(FName(*Package),Deps);
        for(FName D:Deps) AddReachable(D.ToString(),Package,D.ToString(),TEXT("native_registry_after_load"));
        if(UPackage* Loaded=FindPackage(nullptr,*Package))
            ForEachObjectWithPackage(Loaded,[&](UObject* Object)
            {
                TArray<UObject*> Refs; FReferenceFinder Finder(Refs,nullptr,false,true,false,true); Finder.FindReferences(Object);
                for(UObject* Ref:Refs) if(Ref) AddReachable(Ref->GetOutermost()->GetName(),Object->GetPathName(),Ref->GetPathName(),TEXT("native_serialized_object_reference"));
                return DiscoveryError.IsEmpty();
            },EGetObjectsFlags::IncludeNestedObjects);
        if(!DiscoveryError.IsEmpty()) return Finish(R,DiscoveryError);
    }
    Sorted=Known.Array(); Sorted.Sort(); Closure.Reset(); for(const auto& P:Sorted) Closure.Add(MakeShared<FJsonValueString>(P));
    R->SetArrayField(TEXT("source_dependency_packages"),Closure); R->SetArrayField(TEXT("causal_loaded_dependency_edges"),AddedEdges);
    if(bApply)
    {
        FString PriorText; TSharedPtr<FJsonObject> Prior;
        const FString Reports=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/");
        FString Full=FPaths::ConvertRelativePathToFull(PriorDryRunJsonPath); FPaths::NormalizeFilename(Full);
        if(!Full.StartsWith(Reports) || !FFileHelper::LoadFileToString(PriorText,*Full)
           || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PriorText),Prior)
           || Prior->GetStringField(TEXT("status"))!=TEXT("PASS") || Prior->GetStringField(TEXT("phase"))!=TEXT("GASTransitionAssetsDryRun"))
            return Finish(R,TEXT("Apply requires actual prior successful dry-run report"));
        const auto N=Prior->GetObjectField(TEXT("native")); TSet<FString> PriorKnown;
        for(const auto& V:N->GetArrayField(TEXT("source_dependency_packages"))) PriorKnown.Add(V->AsString());
        if(N->GetStringField(TEXT("destination"))!=DestinationRoot || N->GetBoolField(TEXT("apply")) || PriorKnown.Num()!=Known.Num())
            return Finish(R,TEXT("Dry-run destination/closure mismatch"));
        for(const auto& P:Known) if(!PriorKnown.Contains(P)) return Finish(R,TEXT("Source closure changed after dry run: ")+P);
        FString Before,After;
        auto BeforeObject=MakeShared<FJsonObject>(); BeforeObject->SetArrayField(TEXT("source_audits"),N->GetArrayField(TEXT("source_audits")));
        auto AfterObject=MakeShared<FJsonObject>(); AfterObject->SetArrayField(TEXT("source_audits"),Audits);
        FJsonSerializer::Serialize(BeforeObject,TJsonWriterFactory<>::Create(&Before));
        FJsonSerializer::Serialize(AfterObject,TJsonWriterFactory<>::Create(&After));
        if(Before!=After) return Finish(R,TEXT("Source data/cut preflight changed after dry run"));
    }
    for(TObjectIterator<UPackage> It;It;++It) if(SourcePackage(It->GetName()))
    {
        if(!Known.Contains(It->GetName())) return Finish(R,TEXT("Loaded dependency outside inspected closure: ")+It->GetName());
        if(!FPaths::IsSamePath(It->GetLoadedPath().GetLocalFullPath(),SourceDisk+It->GetName().Mid(6)+TEXT(".uasset")))
            return Finish(R,TEXT("Source resolved to wrong physical file: ")+It->GetName());
    }
    if(!bApply) return Finish(R);
    TArray<UObject*> Allowed={NewSK,NewMesh}; TArray<UAnimSequence*> Copies; FString Error;
    TMap<UObject*,UObject*> Replacements; Replacements.Add(SK,NewSK); Replacements.Add(Mesh,NewMesh);
    for(int32 I=0;I<Sources.Num();++I)
    {
        auto* B=DuplicateObject<UAnimSequence>(Sources[I],CreatePackage(*Outputs[I]),*FPackageName::GetShortName(Outputs[I]));
        if(!B) return Finish(R,TEXT("Native new action duplicate failed"));
        B->SetFlags(RF_Public|RF_Standalone); Copies.Add(B); Allowed.Add(B); Replacements.Add(Sources[I],B);
    }
    TArray<TSharedPtr<FJsonValue>> Checks;
    for(int32 I=0;I<Copies.Num();++I)
    {
        auto* A=Sources[I]; auto* B=Copies[I]; FArchiveReplaceObjectRef<UObject> Replace(B,Replacements,EArchiveReplaceObjectFlags::IgnoreOuterRef|EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
        if(!CleanSequenceCopy(A,B,NewSK,NewMesh,Error)) return Finish(R,Error);
        double Pos=0.,Rot=0.; if(!SameData(A,B,false,Error,Pos,Rot)) return Finish(R,Error);
        const auto& RA=A->GetRetargetTransforms(); const auto& RB=B->GetRetargetTransforms();
        if(RA.Num()!=RB.Num()) return Finish(R,TEXT("Clean skeleton retarget basis length differs"));
        for(int32 Bone=0;Bone<RA.Num();++Bone) if(!RA[Bone].Equals(RB[Bone],.000001)) return Finish(R,TEXT("Clean skeleton retarget basis differs"));
        if(A->bEnableRootMotion!=B->bEnableRootMotion || A->bForceRootLock!=B->bForceRootLock || A->RootMotionRootLock!=B->RootMotionRootLock)
            return Finish(R,TEXT("Clean original source root flags altered"));
        auto J=Summary(B); J->SetNumberField(TEXT("max_position_error_cm"),Pos); J->SetNumberField(TEXT("max_rotation_error_deg"),Rot); Checks.Add(MakeShared<FJsonValueObject>(J));
    }
    R->SetArrayField(TEXT("copy_full_track_checks"),Checks);
    TArray<UAnimSequence*> Working; FRows CutChecks;
    for(int32 I=0;I<Copies.Num();++I)
    {
        auto* B=DuplicateObject<UAnimSequence>(Copies[I],CreatePackage(*WorkingOutputs[I]),*FPackageName::GetShortName(WorkingOutputs[I]));
        if(!B) return Finish(R,TEXT("Native working transition duplicate failed"));
        B->SetFlags(RF_Public|RF_Standalone);
        if(!ApplyCut(B,Cuts[I],Prepared[I],Error)) return Finish(R,Error);
        auto Audit=Summary(B);
        if(!VerifyCut(B,Cuts[I],Prepared[I],Audit,Error))
        {
            R->SetArrayField(TEXT("cut_full_track_checks"),CutChecks);
            R->SetObjectField(TEXT("failed_cut_check"),Audit); return Finish(R,Error);
        }
        Working.Add(B); Allowed.Add(B); CutChecks.Add(MakeShared<FJsonValueObject>(Audit));
    }
    R->SetArrayField(TEXT("cut_full_track_checks"),CutChecks);
    if(!CleanHardReferences(Allowed,Error)) return Finish(R,Error);
    Copies.Append(Working); Outputs.Append(WorkingOutputs);
    TArray<FString> Files; for(auto* B:Copies) { if(!SaveNew(B,Error,TransitionOwner)) return Finish(R,Error); Files.Add(LocalFile(B->GetOutermost()->GetName())); }
    AR.ScanFilesSynchronous(Files,true); TSet<FString> AllowedPackages={CleanSkeleton,CleanMesh}; for(const auto& P:Outputs) AllowedPackages.Add(P);
    for(auto* B:Copies)
    {
        TArray<FName> Deps; AR.GetDependencies(B->GetOutermost()->GetFName(),Deps);
        for(FName D:Deps) if(D.ToString().StartsWith(TEXT("/Game/")) && !AllowedPackages.Contains(D.ToString()))
            return Finish(R,TEXT("Saved sequence still references source framework: ")+D.ToString());
    }
    R->SetNumberField(TEXT("saved_count"),Copies.Num()); return Finish(R);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2GASTransitionAssetsEditor::InspectTransitionAssets(const FString& DestinationRoot)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("read_only"),true); R->SetStringField(TEXT("destination"),DestinationRoot);
    if(!TransitionRoot(DestinationRoot)) return Finish(R,TEXT("Exact transition batch required"));
    FRows Rows;
    for(const FCutSpec& Spec:Cuts)
    {
        const FString Source=DestinationRoot+TEXT("/Animations/")+ClipName(Spec.Suffix);
        const FString Working=DestinationRoot+TEXT("/Working/")+ClipName(Spec.Suffix)+TEXT("_Cut_InPlace");
        if(FindPackage(nullptr,*Source) || FindPackage(nullptr,*Working))
            return Finish(R,TEXT("Reload verifier requires a fresh commandlet; packages must not already be loaded"));
        auto* A=LoadObject<UAnimSequence>(nullptr,*ObjectPath(Source));
        auto* B=LoadObject<UAnimSequence>(nullptr,*ObjectPath(Working));
        if(!A || !B || !A->GetSkeleton() || A->GetSkeleton()!=B->GetSkeleton()
            || A->GetSkeleton()->GetOutermost()->GetName()!=CleanSkeleton || A->RateScale!=1
            || !A->bEnableRootMotion || !A->bForceRootLock
            || A->GetOutermost()->GetMetaData().GetValue(A,TEXT("HarborCityOwnedBy"))!=FString(TransitionOwner)
            || B->GetOutermost()->GetMetaData().GetValue(B,TEXT("HarborCityOwnedBy"))!=FString(TransitionOwner))
            return Finish(R,TEXT("Saved owned source/working skeleton, rate or root flags mismatch"));
        const bool DirtyA=A->GetOutermost()->IsDirty(),DirtyB=B->GetOutermost()->IsDirty();
        FString Error; FPreparedCut Cut;
        if(!PrepareCut(A,Spec,Cut,Error)) return Finish(R,Error);
        auto Row=Summary(B); if(!VerifyCut(B,Spec,Cut,Row,Error)) return Finish(R,Error);
        if(A->GetOutermost()->IsDirty()!=DirtyA || B->GetOutermost()->IsDirty()!=DirtyB)
            return Finish(R,TEXT("Reload readback unexpectedly changed package dirty flags"));
        Row->SetStringField(TEXT("clean_source"),Source); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("saved_cut_checks"),Rows); R->SetNumberField(TEXT("loaded_asset_count"),Cuts.Num()*2);
    return Finish(R);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}

