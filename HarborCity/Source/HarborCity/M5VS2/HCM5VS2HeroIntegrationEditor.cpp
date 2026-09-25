#include "HCM5VS2HeroIntegrationEditor.h"
#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
FString HeroIntegrationPackage(const UObject* Value){return Value?Value->GetOutermost()->GetName():FString();}
bool HeroIntegrationSameRaw(const FReferenceSkeleton& A,const FReferenceSkeleton& B)
{
    if(A.GetRawBoneNum()!=B.GetRawBoneNum())return false;
    for(int32 I=0;I<A.GetRawBoneNum();++I)
        if(A.GetRawRefBoneInfo()[I].Name!=B.GetRawRefBoneInfo()[I].Name
            ||A.GetRawRefBoneInfo()[I].ParentIndex!=B.GetRawRefBoneInfo()[I].ParentIndex
            ||!A.GetRawRefBonePose()[I].Equals(B.GetRawRefBonePose()[I],0.))return false;
    return true;
}
FString HeroIntegrationFinish(const TSharedRef<FJsonObject>& Result,const FString& Error=FString())
{
    Result->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("PASS"):TEXT("FAIL"));
    if(!Error.IsEmpty())Result->SetStringField(TEXT("error"),Error);
    Result->SetBoolField(TEXT("saved_by_helper"),false);
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));return Text;
}
}
#endif

FString UHCM5VS2HeroIntegrationEditor::BindPrivateArmsSkeleton(USkeletalMesh* SourceArms,USkeletalMesh* PrivateArms,USkeletalMesh* BodyMesh,bool Apply)
{
#if WITH_EDITOR
    auto Result=MakeShared<FJsonObject>();
    const FString SourcePath=HeroIntegrationPackage(SourceArms),PrivatePath=HeroIntegrationPackage(PrivateArms),BodyPath=HeroIntegrationPackage(BodyMesh);
    const FString MotionPrefix=TEXT("/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_");
    if(!SourceArms||!PrivateArms||!BodyMesh||SourceArms==PrivateArms||PrivateArms==BodyMesh
        ||SourcePath!=TEXT("/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia_Arms")
        ||!PrivatePath.StartsWith(TEXT("/Game/HarborCity/M5VS2/HeroRev2/Review_"))
        ||!PrivatePath.EndsWith(TEXT("/SKM_HeroRev2_FirstPersonArms"))
        ||!BodyPath.StartsWith(MotionPrefix)||!BodyMesh->GetSkeleton()
        ||!HeroIntegrationPackage(BodyMesh->GetSkeleton()).StartsWith(MotionPrefix))
        return HeroIntegrationFinish(Result,TEXT("Only the original source arms, isolated HeroRev2 arms and completed GAS body skeleton are permitted"));
    USkeleton* TargetSkeleton=BodyMesh->GetSkeleton();
    const auto& SourceRef=SourceArms->GetRefSkeleton();const auto& BodyRef=BodyMesh->GetRefSkeleton();
    if(SourceRef.GetRawBoneNum()!=247||!HeroIntegrationSameRaw(SourceRef,PrivateArms->GetRefSkeleton())
        ||!HeroIntegrationSameRaw(SourceRef,BodyRef)||!HeroIntegrationSameRaw(SourceRef,TargetSkeleton->GetReferenceSkeleton())
        ||TargetSkeleton->GetVirtualBones().Num()!=3)
        return HeroIntegrationFinish(Result,TEXT("Actual 247 raw names/parents/zero-tolerance reference transforms or completed 3 virtual bones differ"));
    for(const FName Bone:{FName(TEXT("VB VS2_IKRoot")),FName(TEXT("VB VS2_IKFoot_L")),FName(TEXT("VB VS2_IKFoot_R"))})
        if(BodyRef.FindBoneIndex(Bone)==INDEX_NONE)return HeroIntegrationFinish(Result,TEXT("Body lacks the verified GAS virtual bone"));
    if(Apply)
    {
        // No virtual bone is added and no source skeleton is edited. Match the
        // actual GAS native mesh operation, then prove the complete ref data.
        PrivateArms->Modify();PrivateArms->SetSkeleton(TargetSkeleton);
        PrivateArms->GetRefSkeleton().RebuildRefSkeleton(TargetSkeleton,true);
        PrivateArms->MarkPackageDirty();
    }
    const auto& ArmsRef=PrivateArms->GetRefSkeleton();
    if(PrivateArms->GetSkeleton()!=TargetSkeleton||!HeroIntegrationSameRaw(SourceRef,ArmsRef)
        ||ArmsRef.GetNum()!=BodyRef.GetNum()||ArmsRef.GetNum()!=250)
        return HeroIntegrationFinish(Result,TEXT("Native arms skeleton/full reference readback does not match the actual completed body"));
    for(int32 I=0;I<ArmsRef.GetNum();++I)
        if(ArmsRef.GetBoneName(I)!=BodyRef.GetBoneName(I)||ArmsRef.GetParentIndex(I)!=BodyRef.GetParentIndex(I)
            ||!ArmsRef.GetRefBonePose()[I].Equals(BodyRef.GetRefBonePose()[I],0.))
            return HeroIntegrationFinish(Result,TEXT("Full names/parents/reference transforms including virtual bones differ"));
    Result->SetStringField(TEXT("source_arms"),SourcePath);Result->SetStringField(TEXT("private_arms"),PrivatePath);
    Result->SetStringField(TEXT("body_mesh"),BodyPath);Result->SetStringField(TEXT("skeleton"),HeroIntegrationPackage(TargetSkeleton));
    Result->SetNumberField(TEXT("raw_bones_exact"),ArmsRef.GetRawBoneNum());Result->SetNumberField(TEXT("full_ref_bones_exact"),ArmsRef.GetNum());
    Result->SetNumberField(TEXT("existing_virtual_bones"),TargetSkeleton->GetVirtualBones().Num());Result->SetBoolField(TEXT("apply"),Apply);
    return HeroIntegrationFinish(Result);
#else
    return TEXT("{\"status\":\"EDITOR_ONLY\"}");
#endif
}
