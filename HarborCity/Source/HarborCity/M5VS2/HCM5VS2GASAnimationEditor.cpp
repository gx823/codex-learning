#include "HCM5VS2GASAnimationEditor.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
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
const FString CleanRoot=TEXT("/Game/HarborCity/M5VS2/GASSourceP0/");
const FString WorkRoot=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/InPlace/");
const FString ResultRoot=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/");
const FString SkeletonSource=TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin");
const FString MeshSource=TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin");
const FString CleanSkeleton=CleanRoot+TEXT("SK_GAS_UEFN_P0");
const FString CleanMesh=CleanRoot+TEXT("SKM_GAS_UEFN_P0");
const TCHAR* Owner=TEXT("HarborCity_M5_VS2_GAS_P0_v1");
const TArray<FString> ClipSuffixes={
    TEXT("Idle/M_Relaxed_Stand_Idle_Loop"), TEXT("Walk/M_Relaxed_Walk_Loop_F"),
    TEXT("Run/M_Relaxed_Run_Loop_F"), TEXT("Idle/M_Relaxed_Stand_Idle_Break_v01"),
    TEXT("Idle/M_Relaxed_Stand_Idle_Break_v02")};
const TArray<FString> SubRoots={TEXT("Characters/UEFN_Mannequin/"),TEXT("Audio/"),TEXT("Blueprints/"),TEXT("Misc/")};

FString OriginalClip(const FString& S) { return TEXT("/Game/Characters/UEFN_Mannequin/Animations/")+S; }
FString ClipName(const FString& S) { return FPackageName::GetShortName(S); }
FString ObjectPath(const FString& P) { return P+TEXT(".")+FPackageName::GetShortName(P); }
FString LocalFile(const FString& P) { return FPackageName::LongPackageNameToFilename(P,FPackageName::GetAssetPackageExtension()); }
bool SourcePackage(const FString& P) { for(const auto& S:SubRoots) if(P.StartsWith(TEXT("/Game/")+S)) return true; return false; }
bool AllowedClip(const FString& P)
{
    for(const auto& S:ClipSuffixes)
        if(P==CleanRoot+TEXT("Animations/")+ClipName(S) || P==WorkRoot+ClipName(S)+TEXT("_InPlace")
           || P==ResultRoot+ClipName(S)+TEXT("_InPlace_SelestiaGAS")) return true;
    return false;
}
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
}
#endif

FString UHCM5VS2GASAnimationEditor::PrepareSourceP0(const FString& ProbeJsonPath,bool bApply)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("apply"),bApply); R->SetStringField(TEXT("scope"),TEXT("exact seven fresh packages; original source submounts are scoped read-only"));
    const FString ActualProject=FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
    if(!FPaths::IsSamePath(ActualProject,TEXT("D:/科研学习/codex学习/HarborCity/HarborCity.uproject"))) return Finish(R,TEXT("Wrong destination project"));
    FString ProbeText; TSharedPtr<FJsonObject> Probe;
    const FString RequiredProbe=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/probe_20260924_000021_400_446619eb_GASReadOnly/author_result.json");
    if(!FPaths::IsSamePath(ProbeJsonPath,RequiredProbe) || !FFileHelper::LoadFileToString(ProbeText,*ProbeJsonPath)
       || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProbeText),Probe) || Probe->GetStringField(TEXT("status"))!=TEXT("PASS"))
        return Finish(R,TEXT("Require the actual fixed successful GAS source probe"));
    TSet<FString> Known;
    for(auto V:Probe->GetObjectField(TEXT("migration_dependency_preview"))->GetArrayField(TEXT("package_paths"))) Known.Add(V->AsString());
    if(Known.Num()!=301) return Finish(R,TEXT("Actual bounded source closure changed; re-audit first"));
    for(const auto& S:SubRoots)
    {
        TArray<FString> Files; IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectContentDir()+S),TEXT("*"),true,false);
        if(!Files.IsEmpty()) return Finish(R,TEXT("Destination source-mount subtree is not empty: ")+S);
    }
    for(const FString& P:Known)
        if(!SourcePackage(P) || FindPackage(nullptr,*P) || IFileManager::Get().FileExists(*LocalFile(P))) return Finish(R,TEXT("Unsafe source closure collision: ")+P);
    TArray<FString> Outputs={CleanSkeleton,CleanMesh};
    for(const auto& S:ClipSuffixes) Outputs.Add(CleanRoot+TEXT("Animations/")+ClipName(S));
    for(const auto& P:Outputs) if(!Fresh(P)) return Finish(R,TEXT("Fresh outputs required; previous attempts are preserved: ")+P);
    FScopedGASMounts Mounts;
    TArray<UObject*> Originals;
    auto* SK=LoadObject<USkeleton>(nullptr,*ObjectPath(SkeletonSource));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*ObjectPath(MeshSource));
    if(!SK || !Mesh || Mesh->GetSkeleton()!=SK) return Finish(R,TEXT("Source mesh/skeleton load or binding failed"));
    Originals.Add(SK); Originals.Add(Mesh);
    TArray<TSharedPtr<FJsonValue>> Audits;
    for(const auto& S:ClipSuffixes)
    {
        auto* Seq=LoadObject<UAnimSequence>(nullptr,*ObjectPath(OriginalClip(S)));
        if(!Seq || Seq->GetSkeleton()!=SK || Seq->AdditiveAnimType!=AAT_None || !Seq->GetDataModel()
           || !Seq->GetDataModel()->IsValidBoneTrackName(TEXT("root")) || !Seq->GetDataModel()->IsValidBoneTrackName(TEXT("pelvis")))
            return Finish(R,TEXT("Actual P0 source clip/skeleton/additive/raw track mismatch: ")+S);
        for(FName N:{FName(TEXT("root")),FName(TEXT("pelvis"))})
        {
            TArray<FTransform> Keys; Seq->GetDataModel()->GetBoneTrackTransforms(N,Keys);
            if(Keys.Num()!=Seq->GetDataModel()->GetNumberOfKeys()) return Finish(R,TEXT("Incomplete modern data-model track keys"));
        }
        Originals.Add(Seq); Audits.Add(MakeShared<FJsonValueObject>(Summary(Seq)));
    }
    R->SetArrayField(TEXT("source_native_full_track_audit"),Audits);
    for(UObject* O:Originals)
    {
        const FString Expected=SourceDisk+O->GetOutermost()->GetName().Mid(6)+TEXT(".uasset");
        if(!FPaths::IsSamePath(O->GetOutermost()->GetLoadedPath().GetLocalFullPath(),Expected)) return Finish(R,TEXT("Loaded source is not the authorized physical file: ")+O->GetPathName());
    }
    for(TObjectIterator<UPackage> It;It;++It)
        if(SourcePackage(It->GetName()) && !Known.Contains(It->GetName())) return Finish(R,TEXT("Unexpected source dependency loaded: ")+It->GetName());
    R->SetNumberField(TEXT("known_read_only_dependency_packages"),Known.Num());
    if(!bApply) return Finish(R);
    TArray<UObject*> Copies; TMap<UObject*,UObject*> Replacements;
    for(int32 I=0;I<Originals.Num();++I)
    {
        UPackage* P=CreatePackage(*Outputs[I]);
        UObject* C=StaticDuplicateObject(Originals[I],P,*FPackageName::GetShortName(Outputs[I]));
        if(!C) return Finish(R,TEXT("Native duplication failed: ")+Outputs[I]);
        C->SetFlags(RF_Public|RF_Standalone); Copies.Add(C); Replacements.Add(Originals[I],C);
    }
    // Copies retain actual native data models. Replace references only inside the
    // new objects, including private nested FK rigs; no reflection write bypass.
    for(UObject* C:Copies) { FArchiveReplaceObjectRef<UObject> Replace(C,Replacements,EArchiveReplaceObjectFlags::IgnoreOuterRef|EArchiveReplaceObjectFlags::IgnoreArchetypeRef); }
    auto* NewSK=CastChecked<USkeleton>(Copies[0]); auto* NewMesh=CastChecked<USkeletalMesh>(Copies[1]);
    for(auto Compatible:TArray<TSoftObjectPtr<USkeleton>>(NewSK->GetCompatibleSkeletons())) NewSK->RemoveCompatibleSkeleton(Compatible);
    NewSK->SetUseRetargetModesFromCompatibleSkeleton(false);
    NewSK->SetPreviewMesh(NewMesh); NewSK->SetAdditionalPreviewSkeletalMeshes(nullptr);
    NewSK->PreviewAttachedAssetContainer.ClearAllAttachedObjects();
    NewSK->AnimRetargetSources.Empty();
    NewMesh->SetSkeleton(NewSK); NewMesh->SetPhysicsAsset(nullptr); NewMesh->SetShadowPhysicsAsset(nullptr);
    NewMesh->SetPostProcessAnimBlueprint(nullptr); NewMesh->SetNodeMappingData({}); NewMesh->SetLODSettings(nullptr);
    auto Materials=Mesh->GetMaterials(); for(auto& M:Materials) M.MaterialInterface=UMaterial::GetDefaultMaterial(MD_Surface); NewMesh->SetMaterials(Materials);
    if(const auto* Data=NewMesh->GetAssetUserDataArray()) { const auto Copy=*Data; for(UAssetUserData* D:Copy) if(D) NewMesh->RemoveUserDataOfClass(D->GetClass()); }
    // Public array includes editor-only data in this non-cook editor commandlet;
    // RemoveUserDataOfClass removes from both arrays (SkeletalMesh.cpp 6362+).
    if(const auto* Remaining=NewMesh->GetAssetUserDataArray(); Remaining && !Remaining->IsEmpty())
        return Finish(R,TEXT("Public API did not remove all copied mesh user data"));
    TArray<TSharedPtr<FJsonValue>> Checks;
    FString Error;
    for(int32 I=2;I<Copies.Num();++I)
    {
        auto* A=CastChecked<UAnimSequence>(Originals[I]); auto* B=CastChecked<UAnimSequence>(Copies[I]);
        if(!CleanSequenceCopy(A,B,NewSK,NewMesh,Error)) return Finish(R,Error);
        double Position=0.,Rotation=0.; if(!SameData(A,B,false,Error,Position,Rotation)) return Finish(R,Error);
        const auto& ReferenceA=A->GetRetargetTransforms(); const auto& ReferenceB=B->GetRetargetTransforms();
        if(ReferenceA.Num()!=ReferenceB.Num()) return Finish(R,TEXT("Source retarget-reference basis changed during clean copy"));
        for(int32 Bone=0;Bone<ReferenceA.Num();++Bone)
            if(!ReferenceA[Bone].Equals(ReferenceB[Bone],.000001)) return Finish(R,TEXT("Source retarget-reference transform changed"));
        auto C=Summary(B); C->SetNumberField(TEXT("max_raw_position_error_cm"),Position); C->SetNumberField(TEXT("max_raw_rotation_error_degrees"),Rotation);
        C->SetStringField(TEXT("non_bone_data_guard"),TEXT("native GUID includes curves, attributes and exact timing")); Checks.Add(MakeShared<FJsonValueObject>(C));
    }
    R->SetArrayField(TEXT("copied_native_full_track_checks"),Checks);
    R->SetStringField(TEXT("copy_only_dependency_changes"),TEXT("sample notifies/editor metadata/additive-unused reference removed; neutral preview mesh, no postprocess/physics; engine default compression profiles; raw tracks/curves/attributes retained"));
    if(!CleanHardReferences(Copies,Error)) return Finish(R,Error);
    TArray<FString> Files;
    for(UObject* C:Copies) { if(!SaveNew(C,Error)) return Finish(R,Error); Files.Add(LocalFile(C->GetOutermost()->GetName())); }
    IAssetRegistry& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get(); AR.ScanFilesSynchronous(Files,true);
    TSet<FString> Allowed; for(const FString& P:Outputs) Allowed.Add(P);
    TArray<TSharedPtr<FJsonValue>> Saved;
    for(UObject* C:Copies)
    {
        TArray<FName> Deps; AR.GetDependencies(C->GetOutermost()->GetFName(),Deps);
        for(FName Dep:Deps) if(Dep.ToString().StartsWith(TEXT("/Game/")) && !Allowed.Contains(Dep.ToString()))
            return Finish(R,TEXT("Saved copy still has a source/framework dependency; preserve failed attempt: ")+Dep.ToString());
        Saved.Add(MakeShared<FJsonValueString>(C->GetOutermost()->GetName()));
    }
    R->SetArrayField(TEXT("saved_packages"),Saved); R->SetNumberField(TEXT("saved_count"),Copies.Num());
    R->SetStringField(TEXT("source_pose_preservation"),TEXT("full native Track data; not raw-pose resampling"));
    return Finish(R);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2GASAnimationEditor::CreateInPlaceP0(bool bApply)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("apply"),bApply); FString Error;
    TArray<TSharedPtr<FJsonValue>> Audits; TArray<UAnimSequence*> Sources;
    for(const auto& S:ClipSuffixes)
    {
        const FString Source=CleanRoot+TEXT("Animations/")+ClipName(S), Dest=WorkRoot+ClipName(S)+TEXT("_InPlace");
        auto* A=LoadObject<UAnimSequence>(nullptr,*ObjectPath(Source));
        if(!A || !A->GetSkeleton() || A->GetSkeleton()->GetOutermost()->GetName()!=CleanSkeleton || !A->GetDataModel() || A->AdditiveAnimType!=AAT_None || !Fresh(Dest))
            return Finish(R,TEXT("Exact clean-source/fresh-working-clip guard failed: ")+Source);
        TArray<FTransform> Keys; A->GetDataModel()->GetBoneTrackTransforms(TEXT("root"),Keys);
        if(Keys.Num()!=A->GetDataModel()->GetNumberOfKeys() || Keys.IsEmpty()) return Finish(R,TEXT("Native root track incomplete"));
        for(const FTransform& K:Keys) if(K.ContainsNaN() || !K.GetScale3D().Equals(FVector::OneVector,.00001) || K.GetRotation().AngularDistance(Keys[0].GetRotation())>.00001)
            return Finish(R,TEXT("P0 has unexpected root rotation/scale; turn clips require a separate plan"));
        Sources.Add(A); Audits.Add(MakeShared<FJsonValueObject>(Summary(A)));
    }
    R->SetArrayField(TEXT("source_audits"),Audits); if(!bApply) return Finish(R);
    TArray<TSharedPtr<FJsonValue>> Results;
    for(int32 I=0;I<Sources.Num();++I)
    {
        auto* A=Sources[I]; const FString Dest=WorkRoot+ClipName(ClipSuffixes[I])+TEXT("_InPlace");
        auto* B=DuplicateObject<UAnimSequence>(A,CreatePackage(*Dest),*FPackageName::GetShortName(Dest));
        if(!B) return Finish(R,TEXT("Native in-place working-copy duplication failed"));
        B->SetFlags(RF_Public|RF_Standalone);
        TArray<FTransform> Keys; A->GetDataModel()->GetBoneTrackTransforms(TEXT("root"),Keys);
        TArray<FVector3f> P,S; TArray<FQuat4f> Q;
        for(const FTransform& K:Keys) { FVector V=K.GetLocation(); V.X=Keys[0].GetLocation().X; V.Y=Keys[0].GetLocation().Y; P.Add(FVector3f(V)); Q.Add(FQuat4f(K.GetRotation())); S.Add(FVector3f(K.GetScale3D())); }
        if(!B->GetController().SetBoneTrackKeys(TEXT("root"),P,Q,S,false)) return Finish(R,TEXT("Native root-only write rejected"));
        B->bEnableRootMotion=false; B->bForceRootLock=false;
        double Pos=0.,Rot=0.; if(!SameData(A,B,true,Error,Pos,Rot)) return Finish(R,Error);
        if(!SaveNew(B,Error)) return Finish(R,Error);
        auto J=Summary(B); J->SetNumberField(TEXT("all_track_max_error_cm_except_planned_root_xy"),Pos); J->SetNumberField(TEXT("all_track_max_rotation_error_degrees"),Rot);
        Results.Add(MakeShared<FJsonValueObject>(J));
    }
    R->SetArrayField(TEXT("saved_working_clips"),Results); return Finish(R);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}

FString UHCM5VS2GASAnimationEditor::InspectSequenceP0(UAnimSequence* Sequence)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>();
    if(!Sequence || !AllowedClip(Sequence->GetOutermost()->GetName()) || !Sequence->GetDataModel()) return Finish(R,TEXT("Only exact five P0 source/working/target clips"));
    R->SetObjectField(TEXT("sequence"),Summary(Sequence)); return Finish(R);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}

#if WITH_EDITOR
namespace
{
const TCHAR* RecoveryOwner=TEXT("HarborCity_M5_VS2_GAS_Recovery_v1");
const TArray<FString> RecoverySuffixes={TEXT("Ragdoll/M_ragdoll_getup_stand_B"),TEXT("Ragdoll/M_ragdoll_getup_stand_F"),
    TEXT("Ragdoll/M_ragdoll_getup_stand_L"),TEXT("Ragdoll/M_ragdoll_getup_stand_R"),TEXT("Sprint/M_Relaxed_Sprint_Loop_F")};
bool RecoveryRoot(const FString& Root)
{
    const FString Prefix=TEXT("/Game/HarborCity/M5VS2/GASRecoverySource/Batch_");
    if(!Root.StartsWith(Prefix) || Root.Len()!=Prefix.Len()+12) return false;
    for(TCHAR C:Root.Right(12)) if(!((C>='0'&&C<='9')||(C>='a'&&C<='f'))) return false;
    return true;
}
}
#endif

FString UHCM5VS2GASAnimationEditor::PrepareRecoverySource(const FString& ProbeJsonPath,
    const FString& DestinationRoot,const FString& PriorDryRunJsonPath,bool bApply)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("apply"),bApply); R->SetStringField(TEXT("destination"),DestinationRoot);
    if(!FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),TEXT("D:/科研学习/codex学习/HarborCity/HarborCity.uproject"))
       || !RecoveryRoot(DestinationRoot)) return Finish(R,TEXT("Exact project and fresh Recovery Batch_<12hex> namespace required"));
    const FString ExpectedProbe=TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/probe_20260924_123631_452_88d3fc38_GASGetUpSprintReadOnly/author_result.json");
    FString Text; TSharedPtr<FJsonObject> Probe;
    if(!FPaths::IsSamePath(ProbeJsonPath,ExpectedProbe) || !FFileHelper::LoadFileToString(Text,*ProbeJsonPath)
       || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Probe) || Probe->GetStringField(TEXT("status"))!=TEXT("PASS"))
        return Finish(R,TEXT("Require fixed real getup/sprint original-frame probe"));
    TSet<FString> Probed;
    for(const auto& V:Probe->GetArrayField(TEXT("clips")))
    {
        const auto C=V->AsObject(); if(!C.IsValid() || C->GetStringField(TEXT("status"))!=TEXT("PASS")) return Finish(R,TEXT("Incomplete source probe"));
        Probed.Add(C->GetStringField(TEXT("path")));
    }
    if(Probed.Num()!=5) return Finish(R,TEXT("Only four standing getups and one Relaxed Sprint"));
    for(const auto& S:RecoverySuffixes) if(!Probed.Contains(OriginalClip(S))) return Finish(R,TEXT("Probe selection mismatch"));
    for(const auto& S:SubRoots)
    {
        TArray<FString> Files; IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectContentDir()+S),TEXT("*"),true,false);
        if(!Files.IsEmpty()) return Finish(R,TEXT("Do not shadow a destination subtree: ")+S);
    }
    for(TObjectIterator<UPackage> It;It;++It) if(SourcePackage(It->GetName())) return Finish(R,TEXT("Source submount package already loaded: ")+It->GetName());
    TArray<FString> Outputs; for(const auto& S:RecoverySuffixes) Outputs.Add(DestinationRoot+TEXT("/Animations/")+ClipName(S));
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
    TArray<FString> Queue={SkeletonSource,MeshSource}; for(const auto& S:RecoverySuffixes) Queue.Add(OriginalClip(S));
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
    TArray<UAnimSequence*> Sources; TArray<TSharedPtr<FJsonValue>> Audits;
    for(const auto& S:RecoverySuffixes)
    {
        auto* Seq=LoadObject<UAnimSequence>(nullptr,*ObjectPath(OriginalClip(S)));
        if(!Seq || Seq->GetSkeleton()!=SK || Seq->AdditiveAnimType!=AAT_None || !Seq->GetDataModel()
           || !Seq->bEnableRootMotion || !Seq->bForceRootLock || Seq->RootMotionRootLock!=ERootMotionRootLock::RefPose)
            return Finish(R,TEXT("Observed original action flags/skeleton changed: ")+S);
        Sources.Add(Seq); Audits.Add(MakeShared<FJsonValueObject>(Summary(Seq)));
    }
    R->SetArrayField(TEXT("source_audits"),Audits);
    // Disk registry dependencies can omit the serialized DataAsset references
    // reached while its Blueprint-generated class is loaded. Discover ONLY
    // packages causally reachable from the seven roots through native object
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
           || Prior->GetStringField(TEXT("status"))!=TEXT("PASS") || Prior->GetStringField(TEXT("phase"))!=TEXT("GASRecoverySourceDryRun"))
            return Finish(R,TEXT("Apply requires actual prior successful dry-run report"));
        const auto N=Prior->GetObjectField(TEXT("native")); TSet<FString> PriorKnown;
        for(const auto& V:N->GetArrayField(TEXT("source_dependency_packages"))) PriorKnown.Add(V->AsString());
        if(N->GetStringField(TEXT("destination"))!=DestinationRoot || N->GetBoolField(TEXT("apply")) || PriorKnown.Num()!=Known.Num())
            return Finish(R,TEXT("Dry-run destination/closure mismatch"));
        for(const auto& P:Known) if(!PriorKnown.Contains(P)) return Finish(R,TEXT("Source closure changed after dry run: ")+P);
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
            return Finish(R,TEXT("Getup/Sprint source root flags altered"));
        auto J=Summary(B); J->SetNumberField(TEXT("max_position_error_cm"),Pos); J->SetNumberField(TEXT("max_rotation_error_deg"),Rot); Checks.Add(MakeShared<FJsonValueObject>(J));
    }
    R->SetArrayField(TEXT("copy_full_track_checks"),Checks);
    if(!CleanHardReferences(Allowed,Error)) return Finish(R,Error);
    TArray<FString> Files; for(auto* B:Copies) { if(!SaveNew(B,Error,RecoveryOwner)) return Finish(R,Error); Files.Add(LocalFile(B->GetOutermost()->GetName())); }
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

FString UHCM5VS2GASAnimationEditor::CreateSprintInPlace(const FString& DestinationRoot,bool bApply)
{
#if WITH_EDITOR
    auto R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("apply"),bApply);
    if(!RecoveryRoot(DestinationRoot)) return Finish(R,TEXT("Only dedicated Recovery batch namespace"));
    const FString Source=DestinationRoot+TEXT("/Animations/M_Relaxed_Sprint_Loop_F");
    const FString Dest=DestinationRoot+TEXT("/Working/M_Relaxed_Sprint_Loop_F_InPlace");
    auto* A=LoadObject<UAnimSequence>(nullptr,*ObjectPath(Source));
    if(!A || !A->GetSkeleton() || A->GetSkeleton()->GetOutermost()->GetName()!=CleanSkeleton || !A->GetDataModel() || !Fresh(Dest)
       || A->GetOutermost()->GetMetaData().GetValue(A,TEXT("HarborCityOwnedBy"))!=FString(RecoveryOwner))
        return Finish(R,TEXT("Require exact owned clean Sprint and absent working copy"));
    TArray<FTransform> Keys; A->GetDataModel()->GetBoneTrackTransforms(TEXT("root"),Keys);
    if(Keys.IsEmpty() || Keys.Num()!=A->GetDataModel()->GetNumberOfKeys()) return Finish(R,TEXT("Incomplete original root keys"));
    for(const auto& K:Keys) if(K.ContainsNaN() || !K.GetScale3D().Equals(FVector::OneVector,.00001) || K.GetRotation().AngularDistance(Keys[0].GetRotation())>.00001)
        return Finish(R,TEXT("Unexpected Sprint root scale or rotation"));
    R->SetObjectField(TEXT("source"),Summary(A)); if(!bApply) return Finish(R);
    auto* B=DuplicateObject<UAnimSequence>(A,CreatePackage(*Dest),*FPackageName::GetShortName(Dest));
    if(!B) return Finish(R,TEXT("Cannot create new Sprint working copy"));
    B->SetFlags(RF_Public|RF_Standalone); TArray<FVector3f> Pos,Scale; TArray<FQuat4f> Rot;
    for(const auto& K:Keys) { FVector V=K.GetLocation(); V.X=Keys[0].GetLocation().X; V.Y=Keys[0].GetLocation().Y; Pos.Add(FVector3f(V)); Scale.Add(FVector3f(K.GetScale3D())); Rot.Add(FQuat4f(K.GetRotation())); }
    if(!B->GetController().SetBoneTrackKeys(TEXT("root"),Pos,Rot,Scale,false)) return Finish(R,TEXT("Sprint root XY native write rejected"));
    B->bEnableRootMotion=false; B->bForceRootLock=false;
    FString Error; double P=0.,Q=0.; if(!SameData(A,B,true,Error,P,Q)) return Finish(R,Error);
    if(!SaveNew(B,Error,RecoveryOwner)) return Finish(R,Error);
    R->SetObjectField(TEXT("working_copy"),Summary(B)); R->SetNumberField(TEXT("max_unplanned_position_error_cm"),P); R->SetNumberField(TEXT("max_rotation_error_degrees"),Q);
    return Finish(R);
#else
    return TEXT("{\"status\":\"FAIL\",\"error\":\"Editor only\"}");
#endif
}
