#include "HCM5VS3Authoring.h"
#include "M3/HCM3NPC.h"
#include "M3/HCM3NavRegion.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#endif

FString UHCM5VS3Authoring::RepairStandingStarts(UObject* Context)
{
    auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("status"),TEXT("FAIL"));
#if WITH_EDITOR
    UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);
    if(!World || World->WorldType!=EWorldType::Editor || !World->GetOutermost()->GetName().StartsWith(TEXT("/Game/HarborCity/M5VS3/")))return TEXT("{\"status\":\"FAIL\",\"error\":\"VS3 editor map required\"}");
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    TArray<TSharedPtr<FJsonValue>> Rows;TArray<FVector> Planned;bool All=Nav!=nullptr;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VS3AuthorStarts),false);
    for(TActorIterator<APawn> It(World);It;++It)Query.AddIgnoredActor(*It);
    for(TActorIterator<AHCM3NPC> It(World);It;++It)
    {
        AHCM3NPC* NPC=*It;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),NPC->StableId.ToString());
        const FVector Old=NPC->GetActorLocation();Row->SetStringField(TEXT("old"),Old.ToString());
        const float Half=NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=NPC->GetCapsuleComponent()->GetScaledCapsuleRadius();
        bool Found=false;FString FirstReason;
        for(int32 I=0;Nav && I<161;++I)
        {
            const float Dist=I==0?0:50.f*(1+(I-1)/16);
            const FVector Feet=Old-FVector(0,0,Half)+FRotator(0,(I-1)%16*22.5f,0).Vector()*Dist;
            FNavLocation Projected;
            auto Reject=[&](const FString& Why){if(I==0)FirstReason=Why;};
            if(!Nav->ProjectPointToNavigation(Feet,Projected,FVector(50,50,160),&NPC->GetNavAgentPropertiesRef())){Reject(TEXT("no matching pedestrian nav polygon"));continue;}
            if(FVector::Dist2D(Feet,Projected.Location)>75){Reject(TEXT("nav projection too far"));continue;}
            if(!NPC->NavigationRegion || !NPC->NavigationRegion->ContainsPoint(Projected.Location,Radius)){Reject(TEXT("outside assigned standing region"));continue;}
            bool Forbidden=false;
            for(TActorIterator<AHCM3NavRegion> R(World);R;++R)
                if(R->Kind!=EHCM3NavRegionKind::Allowed && R->ContainsPoint(Projected.Location,-Radius))Forbidden=true;
            if(Forbidden){Reject(TEXT("inside forbidden/reserved region"));continue;}
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,Projected.Location+FVector(0,0,55),Projected.Location-FVector(0,0,80),ECC_Visibility,Query)||Hit.ImpactNormal.Z<.8){Reject(TEXT("no safe ground"));continue;}
            const FVector Center=Hit.ImpactPoint+FVector(0,0,Half+3);
            if(World->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius+2,Half),Query)){Reject(TEXT("standing capsule overlaps geometry"));continue;}
            bool Occupied=false;for(const FVector& Other:Planned)if(FVector::Dist2D(Center,Other)<2*Radius+15 && FMath::Abs(Center.Z-Other.Z)<2*Half)Occupied=true;
            if(Occupied){Reject(TEXT("overlaps another planned NPC"));continue;}
            NPC->Modify();NPC->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics);Planned.Add(Center);
            Row->SetStringField(TEXT("new"),Center.ToString());Row->SetNumberField(TEXT("displacement_cm"),FVector::Dist(Center,Old));Found=true;break;
        }
        Row->SetStringField(TEXT("initial_rejection"),FirstReason);Row->SetBoolField(TEXT("valid_start"),Found);Rows.Add(MakeShared<FJsonValueObject>(Row));All&=Found;
    }
    Result->SetArrayField(TEXT("npcs"),Rows);Result->SetStringField(TEXT("status"),All?TEXT("PASS"):TEXT("FAIL"));
#endif
    FString Out;auto Writer=TJsonWriterFactory<>::Create(&Out);FJsonSerializer::Serialize(Result,Writer);return Out;
}

FString UHCM5VS3Authoring::AuthorCombatPoses(UAnimSequence* Source,const TArray<UAnimSequence*>& Targets)
{
#if WITH_EDITOR
    if(!Source||!Source->GetDataModel()||Targets.Num()!=8)return TEXT("{\"status\":\"FAIL\",\"error\":\"source and eight targets required\"}");
    const auto& Ref=Source->GetSkeleton()->GetReferenceSkeleton();const int32 Count=Ref.GetRawBoneNum();
    if(Count!=247)return TEXT("{\"status\":\"FAIL\",\"error\":\"wrong original hierarchy\"}");
    TArray<FTransform> Base=Ref.GetRawRefBonePose();
    for(int32 B=0;B<Count;++B)if(Source->GetDataModel()->IsValidBoneTrackName(Ref.GetBoneName(B)))Base[B]=Source->GetDataModel()->GetBoneTrackTransform(Ref.GetBoneName(B),FFrameNumber(0));
    auto CS=[&](const TArray<FTransform>& P){TArray<FTransform> Out;Out.SetNum(Count);for(int32 I=0;I<Count;++I){int32 Parent=Ref.GetParentIndex(I);Out[I]=Parent<0?P[I]:P[I]*Out[Parent];}return Out;};
    const auto Initial=CS(Base);
    auto Pos=[&](const TCHAR* N){return Initial[Ref.FindBoneIndex(N)].GetLocation();};
    const FVector Forward=FVector::VectorPlaneProject(Pos(TEXT("Toe_R"))-Pos(TEXT("Foot_R")),FVector::UpVector).GetSafeNormal();
    const FVector Left=FVector::VectorPlaneProject(Pos(TEXT("UpperArm_L"))-Pos(TEXT("UpperArm_R")),FVector::UpVector).GetSafeNormal();
    auto Point=[&](TArray<FTransform>& P,const TCHAR* N,const TCHAR* Child,FVector Goal){const int32 B=Ref.FindBoneIndex(N),J=Ref.FindBoneIndex(Child);auto C=CS(P);const FQuat R=(FQuat::FindBetweenNormals((C[J].GetLocation()-C[B].GetLocation()).GetSafeNormal(),Goal.GetSafeNormal())*C[B].GetRotation()).GetNormalized();P[B].SetRotation((C[Ref.GetParentIndex(B)].GetRotation().Inverse()*R).GetNormalized());};
    for(int32 Kind=0;Kind<8;++Kind){UAnimSequence* Target=Targets[Kind];if(!Target||!Target->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS3/Combat/"))||Target->GetSkeleton()!=Source->GetSkeleton())return TEXT("{\"status\":\"FAIL\",\"error\":\"target scope\"}");
        TArray<TArray<FTransform>> Frames;
        for(int32 F=0;F<=36;++F){const float T=F/36.f;TArray<FTransform> P=Base;
            const float Attack=FMath::SmoothStep(.22f,.66f,T),Recover=1-FMath::SmoothStep(.78f,1.f,T);
            const float Side=Kind==1?1.f:-1.f;FVector Upper,Lower;
            if(Kind<=3){const float Arc=FMath::Lerp(-1.2f,1.2f,Attack)*Side;const bool Overhead=Kind>=2;
                Upper=Forward*.7f+Left*(Overhead?-.18f:FMath::Sin(Arc)) + FVector::UpVector*(Overhead?FMath::Lerp(.9f,-.4f,Attack):.15f);
                Lower=Forward*(Overhead?.75f:FMath::Cos(Arc))+Left*(Overhead?-.2f:FMath::Sin(Arc)*1.6f)+FVector::UpVector*(Overhead?FMath::Lerp(1.2f,-.65f,Attack):.18f);
            }else if(Kind==4){Upper=Forward*.4f-Left*.45f+FVector::UpVector*.75f;Lower=-Forward*.1f+FVector::UpVector;}
            else if(Kind==7){Upper=Forward*.9f-Left*.25f-FVector::UpVector*.2f;Lower=Forward+FVector::UpVector*.25f;}
            else {const float Draw=Kind==5?T:1-T;Upper=FMath::Lerp(-Forward*.35f+FVector::UpVector*.8f,Forward*.6f-FVector::UpVector*.4f,Draw)-Left*.4f;Lower=FMath::Lerp(-Forward*.6f+FVector::UpVector*.8f,Forward+FVector::UpVector*.3f,Draw);}
            Point(P,TEXT("UpperArm_R"),TEXT("LowerArm_R"),Upper);Point(P,TEXT("LowerArm_R"),TEXT("Hand_R"),Lower);
            Point(P,TEXT("UpperArm_L"),TEXT("LowerArm_L"),Left*.35f+Forward*.2f-FVector::UpVector*.7f);
            Point(P,TEXT("LowerArm_L"),TEXT("Hand_L"),Forward*.7f+Left*.15f+FVector::UpVector*.2f);
            if(Kind<=3)for(int32 B=0;B<Count;++B)P[B].Blend(Base[B],P[B],Recover);
            Frames.Add(MoveTemp(P));
        }
        Target->Modify();auto& C=Target->GetController();C.OpenBracket(FText::FromString(TEXT("Original VS3 directional sword/cast pose layer")),false);C.SetFrameRate(FFrameRate(30,1),false);C.SetNumberOfFrames(FFrameNumber(36),false);bool OK=true;
        for(int32 B=0;B<Count;++B){const FName N=Ref.GetBoneName(B);if(!Target->GetDataModel()->IsValidBoneTrackName(N))OK&=C.AddBoneCurve(N,false);TArray<FVector> PosKeys,Scale;TArray<FQuat> Rot;for(const auto& P:Frames){PosKeys.Add(P[B].GetLocation());Rot.Add(P[B].GetRotation());Scale.Add(P[B].GetScale3D());}OK&=C.SetBoneTrackKeys(N,PosKeys,Rot,Scale,false);OK&=C.UpdateBoneTrackKeys(N,FInt32Range(0,37),PosKeys,Rot,Scale,false);}
        C.CloseBracket(false);Target->bEnableRootMotion=false;Target->bForceRootLock=false;Target->MarkPackageDirty();if(!OK)return TEXT("{\"status\":\"FAIL\",\"error\":\"pose write failed\"}");
    }
    return TEXT("{\"status\":\"PASS\",\"visual_review\":\"NOT_RUN\",\"source\":\"GAS relaxed idle plus original directional arm poses\"}");
#else
    return TEXT("{\"status\":\"NOT_RUN\"}");
#endif
}

bool UHCM5VS3Authoring::AuthorFallPose(UAnimSequence* GetUp,UAnimSequence* Target)
{
#if WITH_EDITOR
    if(!GetUp||!Target||!Target->GetPathName().StartsWith(TEXT("/Game/HarborCity/M5VS3/NPC/"))||Target->GetSkeleton()!=GetUp->GetSkeleton()||!GetUp->GetDataModel())return false;
    const auto& Ref=GetUp->GetSkeleton()->GetReferenceSkeleton();const auto* Data=GetUp->GetDataModel();const int32 Last=Data->GetNumberOfFrames();
    auto& C=Target->GetController();Target->Modify();C.OpenBracket(FText::FromString(TEXT("Original protective fall into licensed recovery endpoint")),false);C.SetFrameRate(FFrameRate(30,1),false);C.SetNumberOfFrames(FFrameNumber(36),false);bool OK=true;
    for(int32 B=0;B<Ref.GetRawBoneNum();++B){const FName Name=Ref.GetBoneName(B);const bool Exists=Data->IsValidBoneTrackName(Name);const FTransform Stand=Exists?Data->GetBoneTrackTransform(Name,FFrameNumber(Last)):Ref.GetRawRefBonePose()[B];const FTransform Ground=Exists?Data->GetBoneTrackTransform(Name,FFrameNumber(0)):Ref.GetRawRefBonePose()[B];
        if(!Target->GetDataModel()->IsValidBoneTrackName(Name))OK&=C.AddBoneCurve(Name,false);
        TArray<FVector> P,S;TArray<FQuat> Q;
        for(int32 F=0;F<=36;++F){const float T=F/36.f;const float A=FMath::SmoothStep(.10f,.85f,T);FTransform Pose;Pose.Blend(Stand,Ground,A);
            // Impact flinch then controlled descent. Bone segment lengths/scales are preserved.
            if(B==0){FVector V=Pose.GetLocation();V.Z+=FMath::Sin(A*PI)*5.f;Pose.SetLocation(V);}
            P.Add(Pose.GetLocation());Q.Add(Pose.GetRotation());S.Add(Pose.GetScale3D());
        }
        OK&=C.SetBoneTrackKeys(Name,P,Q,S,false);OK&=C.UpdateBoneTrackKeys(Name,FInt32Range(0,37),P,Q,S,false);
    }
    C.CloseBracket(false);Target->bEnableRootMotion=false;Target->bForceRootLock=false;Target->MarkPackageDirty();return OK;
#else
    return false;
#endif
}
