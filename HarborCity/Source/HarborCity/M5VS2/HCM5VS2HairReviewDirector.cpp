#include "HCM5VS2HairReviewDirector.h"
#include "HCM5VS2ExpressionComponent.h"
#include "HCM5VS2LookAnimInstance.h"
#include "HCM5VS2HeroOutlineComponent.h"
#include "M1/HCM1Character.h"
#include "M1/HCM1PlayerController.h"
#include "M4/HCM4CombatComponent.h"
#include "Animation/MorphTarget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Materials/MaterialRenderProxy.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include <atomic>
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
struct FHCM5VS2HairOutlineMaterialCheck
{
    struct FSlot { const FMaterialRenderProxy* Proxy=nullptr; bool Ready=false; FString Actual; };
    TArray<FSlot> Slots;
    std::atomic<bool> Complete{false};
};
namespace
{
    const TCHAR* Labels[]=
    {
        TEXT("Idle_PhysicsOn"),TEXT("Idle_PhysicsOff"),TEXT("Idle_PhysicsResetOn"),TEXT("LookLeft"),TEXT("Walk"),TEXT("Stop"),TEXT("Rear_Idle")
    }
    ;
    const double Durations[]=
    {
        3.,2.5,3.,2.5,2.5,2.5,2.5
    }
    ;
    const TCHAR* OutlineLabels[]={TEXT("Front_OutlineOn"),TEXT("Front_HairOutlineOff"),TEXT("Front_SourceAlphaMask"),
        TEXT("Rear_OutlineOn"),TEXT("Rear_HairOutlineOff"),TEXT("Rear_SourceAlphaMask")};
    const TCHAR* Roots[]=
    {
        TEXT("Hair_back_long1_L"),TEXT("Hair_back_long1_R"),TEXT("Hair_back_long2_L"),TEXT("Hair_back_long2_R"),TEXT("Hair_back_long3_L"),TEXT("Hair_back_long3_R"),TEXT("Hair_side2_L"),TEXT("Hair_side2_R")
    }
    ;
    TArray<TSharedPtr<FJsonValue>> V(const FVector& P)
    {
        return
        {
            MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)
        }
        ;
    }
    TArray<TSharedPtr<FJsonValue>> Q(const FQuat& P)
    {
        return
        {
            MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z),MakeShared<FJsonValueNumber>(P.W)
        }
        ;
    }
    TArray<TSharedPtr<FJsonValue>> Rows(const TArray<TSharedPtr<FJsonObject>>& A)
    {
        TArray<TSharedPtr<FJsonValue>> R;
        for(const auto& J:A)R.Add(MakeShared<FJsonValueObject>(J));
        return R;
    }
    IConsoleVariable* Switch()
    {
        return IConsoleManager::Get().FindConsoleVariable(TEXT("a.AnimNode.KawaiiPhysics.Enable"));
    }
    bool PNG(const FString& P)
    {
        TArray<uint8> B;
        if(!FFileHelper::LoadFileToArray(B,*P)||B.Num()<24)return false;
        const uint8 Magic[]=
        {
            137,80,78,71,13,10,26,10
        }
        ;
        if(FMemory::Memcmp(B.GetData(),Magic,8))return false;
        auto U=[&](int I)
        {
            return (uint32(B[I])<<24)|(uint32(B[I+1])<<16)|(uint32(B[I+2])<<8)|B[I+3];
        }
        ;
        return U(16)==1920&&U(20)==1080;
    }
    uint16 Mask(FName Name)
    {
        const FString N=Name.ToString();
        for(int32 I=0;I<8;++I)if(N==Roots[I]||N.StartsWith(FString(Roots[I])+TEXT("_")))return uint16(1<<I);
        return 0;
    }
    TSharedPtr<FJsonObject> Stats(TArray<double> A)
    {
        auto J=MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("count"),A.Num());
        if(!A.IsEmpty())
        {
            A.Sort();
            J->SetNumberField(TEXT("min"),A[0]);
            J->SetNumberField(TEXT("median"),A[A.Num()/2]);
            J->SetNumberField(TEXT("p95_nearest_rank"),A[FMath::Clamp(FMath::CeilToInt(A.Num()*.95)-1,0,A.Num()-1)]);
            J->SetNumberField(TEXT("max"),A.Last());
        }
        return J;
    }
}
// Opt-in project diagnostic only. No plugin type linkage or private-field access.
namespace
{
    bool HCSubstepSetting(bool& Value)
    {
        UClass* C=FindObject<UClass>(nullptr,TEXT("/Script/KawaiiPhysics.KawaiiPhysicsDeveloperSettings"));
        const FBoolProperty* P=C?FindFProperty<FBoolProperty>(C,TEXT("bUseFixedSubstepping")):nullptr;
        if(!P)return false;
        Value=P->GetPropertyValue_InContainer(C->GetDefaultObject()); return true;
    }
    bool HCReadVector(const UStruct* S,const void* Data,const TCHAR* Name,FVector& Out)
    {
        const FStructProperty* P=FindFProperty<FStructProperty>(S,Name);
        if(!P||P->Struct!=TBaseStructure<FVector>::Get())return false;
        Out=*P->ContainerPtrToValuePtr<FVector>(Data); return !Out.ContainsNaN();
    }
    FName HCReadBone(const UStruct* S,const void* Data,const TCHAR* Field)
    {
        const FStructProperty* P=FindFProperty<FStructProperty>(S,Field);
        const FNameProperty* N=P?FindFProperty<FNameProperty>(P->Struct,TEXT("BoneName")):nullptr;
        return N?N->GetPropertyValue_InContainer(P->ContainerPtrToValuePtr<void>(Data)):NAME_None;
    }
}
TSharedPtr<FJsonObject> AHCM5VS2HairReviewDirector::SubstepObservation(float Dt) const
{
    auto R=MakeShared<FJsonObject>();
    R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    R->SetNumberField(TEXT("actual_world_delta_seconds"),Dt);
    R->SetStringField(TEXT("phase"),PhaseLabel());
    R->SetNumberField(TEXT("active_seconds"),Active);
    R->SetNumberField(TEXT("phase_seconds"),Age);
    R->SetBoolField(TEXT("kawaii_evaluation_enabled"),Switch()&&Switch()->GetInt()!=0);
    bool Fixed=false; const bool Settings=HCSubstepSetting(Fixed);
    R->SetBoolField(TEXT("actual_fixed_substepping"),Fixed);
    R->SetBoolField(TEXT("settings_readback_valid"),Settings);
    R->SetStringField(TEXT("NumSteps"),TEXT("NOT_EXPOSED_FUNCTION_LOCAL_NOT_MEASURED"));
    R->SetStringField(TEXT("scope"),TEXT("Read-only reflected runtime node fields after mesh tick. Solver Location/PoseLocation are component-space only; private accumulator and NumSteps are not read or inferred. This is not a performance sample."));
    auto* Mesh=Character?Character->GetMesh():nullptr;
    auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
    if(!Mesh||!Anim||Mesh->IsRunningParallelEvaluation())
    { R->SetStringField(TEXT("status"),TEXT("NOT_RUN_PARALLEL_EVALUATION_OR_MISSING_MESH")); return R; }
    TArray<TSharedPtr<FJsonValue>> Nodes;
    int32 Selected=0; bool Good=Settings&&(int32(Fixed)==ExpectedFixedSubstep);
    for(TFieldIterator<FStructProperty> It(Anim->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
    {
        const FStructProperty* NP=*It;
        if(NP->Struct->GetPathName()!=TEXT("/Script/KawaiiPhysics.AnimNode_KawaiiPhysics"))continue;
        const void* Data=NP->ContainerPtrToValuePtr<void>(Anim);
        const FName Root=HCReadBone(NP->Struct,Data,TEXT("RootBone"));
        if(Root!=FName(Roots[0])&&Root!=FName(Roots[6]))continue;
        auto N=MakeShared<FJsonObject>(); N->SetStringField(TEXT("node_property"),NP->GetName());
        N->SetStringField(TEXT("primary_root"),Root.ToString());
        const auto* Anchor=FindFProperty<FBoolProperty>(NP->Struct,TEXT("bAnchorFixedStepOutputToCurrentPose"));
        const bool Anchored=Anchor&&Anchor->GetPropertyValue_InContainer(Data);
        N->SetBoolField(TEXT("anchor_fixed_step_output_to_current_pose"),Anchored);
        Good&=Anchor&&Anchored==bRootAnchorCandidate;
        const auto* Target=FindFProperty<FIntProperty>(NP->Struct,TEXT("TargetFramerate"));
        const auto* Delta=FindFProperty<FFloatProperty>(NP->Struct,TEXT("DeltaTime"));
        const auto* Space=FindFProperty<FEnumProperty>(NP->Struct,TEXT("SimulationSpace"));
        const auto* Subdiv=FindFProperty<FIntProperty>(NP->Struct,TEXT("BoneSubdivisionCount"));
        const auto* CollisionOnly=FindFProperty<FBoolProperty>(NP->Struct,TEXT("bBoneSubdivisionCollisionOnly"));
        const auto* Array=FindFProperty<FArrayProperty>(NP->Struct,TEXT("ModifyBones"));
        const auto* Inner=Array?CastField<FStructProperty>(Array->Inner):nullptr;
        if(!Target||!Delta||!Space||!Subdiv||!CollisionOnly||!Inner)
        { Good=false; N->SetStringField(TEXT("error"),TEXT("Exact public reflection schema unavailable")); Nodes.Add(MakeShared<FJsonValueObject>(N)); continue; }
        const int64 SpaceValue=Space->GetUnderlyingProperty()->GetSignedIntPropertyValue(Space->ContainerPtrToValuePtr<void>(Data));
        N->SetNumberField(TEXT("simulation_space_enum"),double(SpaceValue));
        N->SetNumberField(TEXT("target_framerate"),Target->GetPropertyValue_InContainer(Data));
        N->SetNumberField(TEXT("node_delta_seconds"),Delta->GetPropertyValue_InContainer(Data));
        N->SetNumberField(TEXT("bone_subdivision_count"),Subdiv->GetPropertyValue_InContainer(Data));
        N->SetBoolField(TEXT("collision_only_subdivision"),CollisionOnly->GetPropertyValue_InContainer(Data));
        Good&=SpaceValue==0;
        FScriptArrayHelper Helper(Array,Array->ContainerPtrToValuePtr<void>(Data));
        TArray<TSharedPtr<FJsonValue>> BoneRows;
        for(int32 Index=0;Index<Helper.Num();++Index)
        {
            const void* BoneData=Helper.GetRawPtr(Index);
            const FName Name=HCReadBone(Inner->Struct,BoneData,TEXT("BoneRef"));
            bool Keep=false;
            for(int32 I=0;I<8;++I)
                Keep|=Name==FName(Roots[I])||Name==FName(*(FString(Roots[I])+TEXT("_001")))||Name==FName(*FString::Printf(TEXT("%s_%03d"),Roots[I],I<6?4:6));
            if(!Keep)continue;
            FVector Location,Pose,Previous;
            if(!HCReadVector(Inner->Struct,BoneData,TEXT("Location"),Location)||!HCReadVector(Inner->Struct,BoneData,TEXT("PoseLocation"),Pose)||!HCReadVector(Inner->Struct,BoneData,TEXT("PrevLocation"),Previous))
            { Good=false; continue; }
            auto B=MakeShared<FJsonObject>(); B->SetStringField(TEXT("bone"),Name.ToString());
            B->SetArrayField(TEXT("solver_location_cs"),V(Location));
            B->SetArrayField(TEXT("solver_pose_location_cs"),V(Pose));
            B->SetArrayField(TEXT("solver_previous_location_cs"),V(Previous));
            B->SetArrayField(TEXT("final_location_cs"),V(Mesh->GetSocketTransform(Name,RTS_Component).GetLocation()));
            const int32 BoneId=Mesh->GetBoneIndex(Name),ParentId=ExpectedMesh->GetRefSkeleton().GetParentIndex(BoneId);
            const FName Parent=ExpectedMesh->GetRefSkeleton().GetBoneName(ParentId);
            const auto Final=Mesh->GetSocketTransform(Name,RTS_Component),FinalParent=Mesh->GetSocketTransform(Parent,RTS_Component);
            B->SetArrayField(TEXT("final_parent_location_cs"),V(FinalParent.GetLocation()));
            B->SetArrayField(TEXT("final_rotation_xyzw"),Q(Final.GetRotation()));
            B->SetArrayField(TEXT("final_scale"),V(Final.GetScale3D()));
            B->SetNumberField(TEXT("reference_segment_cm"),ExpectedMesh->GetRefSkeleton().GetRefBonePose()[BoneId].GetTranslation().Size());
            BoneRows.Add(MakeShared<FJsonValueObject>(B)); ++Selected;
        }
        N->SetArrayField(TEXT("bones"),BoneRows); Nodes.Add(MakeShared<FJsonValueObject>(N));
    }
    R->SetArrayField(TEXT("runtime_nodes"),Nodes); R->SetNumberField(TEXT("selected_real_bones"),Selected);
    Good&=Nodes.Num()==2&&Selected==24;
    R->SetStringField(TEXT("status"),Good?TEXT("PASS_READBACK_ONLY"):TEXT("FAIL_READBACK_SCHEMA_OR_BINDING"));
    return R;
}
void AHCM5VS2HairReviewDirector::StopSubstepAudit(const FString& Reason)
{
    bStopped=true; bActive=bAutoQuit=bExit=false; StopFrame=GFrameCounter;
    if(Character&&Character->GetCharacterMovement())Character->GetCharacterMovement()->StopMovementImmediately();
    if(Pending&&FScreenshotRequest::IsScreenshotRequested()&&FScreenshotRequest::GetFilename()==PendingPNG)FScreenshotRequest::Reset();
    UE_LOG(LogTemp,Warning,TEXT("M5VS2_HAIR_SUBSTEP_STOP frame=%llu reason=%s; no restore/refocus/autoquit"),StopFrame,*Reason);
    Write(TEXT("NOT_RUN"),Reason); SetActorTickEnabled(false);
}

AHCM5VS2HairReviewDirector::AHCM5VS2HairReviewDirector()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}
void AHCM5VS2HairReviewDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING && ENABLE_ANIM_DEBUG
    if(!FParse::Param(FCommandLine::Get(),TEXT("M5VS2HairReview")))return;
    if(bOutlineMaskComparison!=FParse::Param(FCommandLine::Get(),TEXT("M5VS2HairOutlineReview")))return;
    const FString Map=GetWorld()->GetOutermost()->GetName(),Prefix=TEXT("/Game/HarborCity/M5VS2/HairReview/Run_");
    if(!Map.StartsWith(Prefix)||Map!=Prefix+Map.Mid(Prefix.Len(),12)+TEXT("/L_HairReview"))return;
    for(TCHAR C:Map.Mid(Prefix.Len(),12))if(!FChar::IsHexDigit(C))return;
    FString R;
    if(!FParse::Value(FCommandLine::Get(),TEXT("M5VS2EvidenceDir="),R)||FPaths::IsRelative(R))return;
    R=FPaths::ConvertRelativePathToFull(R);
    FPaths::NormalizeDirectoryName(R);
    if(!FPaths::CollapseRelativeDirectories(R)||!FPaths::IsUnderDirectory(R,TEXT("D:/科研学习/codex学习/docs/HarborCity_M5_VS2")))return;
    Directory=R/(TEXT("HairReview_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    if(!IFileManager::Get().MakeDirectory(*Directory,true))return;
    bSubstepAudit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2HairSubstepAudit"));
    bRootAnchorCandidate=FParse::Param(FCommandLine::Get(),TEXT("M5VS2HairRootAnchorCandidate"));
    if(bRootAnchorCandidate&&!bSubstepAudit)return;
    if(bSubstepAudit)
    {
        if(bOutlineMaskComparison||!FParse::Value(FCommandLine::Get(),TEXT("M5VS2FixedSubstepExpected="),ExpectedFixedSubstep)||(ExpectedFixedSubstep!=0&&ExpectedFixedSubstep!=1))return;
    }
    Started=FPlatformTime::Seconds();
    bActive=true;
    bAutoQuit=FParse::Param(FCommandLine::Get(),TEXT("M5VS2AutoQuit"));
    SetActorTickEnabled(true);
    Write(TEXT("RUNNING"),TEXT("Waiting for actual current candidate and focused viewport"));
#endif
}
bool AHCM5VS2HairReviewDirector::Start(FString& Error)
{
    Error=TEXT("Waiting for exact current candidate, third person, focused 1080p viewport");
    PC=Cast<AHCM1PlayerController>(UGameplayStatics::GetPlayerController(this,0));
    Character=PC?Cast<AHCM1Character>(PC->GetPawn()):nullptr;
    if(!PC||!Character||!Viewport.IsValid()||!Viewport->Viewport||!PC->IsGameplayFocused())return false;
    auto* Mesh=Character->GetMesh();
    auto* Anim=Cast<UHCM5VS2LookAnimInstance>(Mesh->GetAnimInstance());
    Expression=Character->FindComponentByClass<UHCM5VS2ExpressionComponent>();
    if(Character->GetClass()!=ExpectedCharacterClass||Mesh->GetSkeletalMeshAsset()!=ExpectedMesh||!Anim||Anim->GetClass()!=ExpectedAnimationClass||!Expression||!Expression->IsFaceReady()||!Switch()
    ||PC->IsFirstPersonPerspective()||PC->GetPlayerMode()!=EHCPlayerMode::OnFoot||Viewport->Viewport->GetSizeXY()!=FIntPoint(1920,1080))return false;
    if(!PC->GetCombatComponent()||PC->GetCombatComponent()->GetWeaponMode()!=EHCM4WeaponMode::Unarmed)return false;
    Names.Reset();
    Indices.Reset();
    Parents.Reset();
    BindLengths.Reset();
    Edges.Reset();
    HairVertices.Reset();
    const FReferenceSkeleton& Ref=ExpectedMesh->GetRefSkeleton();
    for(int32 I=0;I<8;++I)for(int32 J=0;J<(I<6?5:7);++J)
    {
        const FName N(J?*FString::Printf(TEXT("%s_%03d"),Roots[I],J):Roots[I]);
        const int32 Id=Ref.FindBoneIndex(N);
        if(Id==INDEX_NONE||Mesh->GetBoneIndex(N)!=Id)
        {
            Error=TEXT("Missing exact full long-hair hierarchy");
            return false;
        }
        Names.Add(N);
        Indices.Add(Id);
        Parents.Add(Ref.GetParentIndex(Id));
        BindLengths.Add(Ref.GetRefBonePose()[Id].GetTranslation().Size());
    }
    if(Names.Num()!=44)
    {
        Error=TEXT("Expected 44 raw long-hair bones");
        return false;
    }
    if(!PrepareEdges(Error))
    {
        Finish(TEXT("FAIL"),Error);
        return false;
    }
    if(bOutlineMaskComparison)
    {
        if(OutlineComparisonMaterials.Num()!=3||OutlineComparisonMaterials.Contains(nullptr))
        { Error=TEXT("Exactly three private outline comparison materials required"); return false; }
        const FString PackagePrefix=GetWorld()->GetOutermost()->GetName().LeftChop(FString(TEXT("L_HairReview")).Len());
        for(const auto& Material:OutlineComparisonMaterials)
            if(!Material->GetOutermost()->GetName().StartsWith(PackagePrefix))
            { Error=TEXT("Outline candidate is outside this private fixture"); return false; }
        TArray<USkeletalMeshComponent*> Components; Character->GetComponents(Components);
        for(auto* C:Components)if(C->GetFName()==TEXT("VS2SourceOutlinePass"))OutlineMesh=C;
        if(!OutlineMesh||OutlineMesh->GetSkeletalMeshAsset()!=ExpectedMesh||OutlineMesh->LeaderPoseComponent.Get()!=Mesh
            ||OutlineMesh->GetNumMaterials()!=5||Mesh->GetNumMaterials()!=5)
        { Error=TEXT("Actual source-leader outline pass not ready"); return false; }
        OriginalOutlineMaterials.Reset(); OriginalBodyMaterials.Reset();
        for(int32 I=0;I<5;++I)
        { OriginalOutlineMaterials.Add(OutlineMesh->GetMaterial(I)); OriginalBodyMaterials.Add(Mesh->GetMaterial(I)); }
        if(OriginalOutlineMaterials.Contains(nullptr)||OriginalBodyMaterials.Contains(nullptr))
        { Error=TEXT("Missing actual material slot"); return false; }
        AddTickPrerequisiteComponent(OutlineMesh);
    }
    OriginalKawaii=Switch()->GetInt();
    OriginalKawaiiFlags=int32(Switch()->GetFlags());
    OriginalView=PC->GetViewTarget();
    bOriginalGlances=Anim->bVS2IdleGlances;
    if(PC->GetHUD())
    {
        bOriginalHUD=PC->GetHUD()->bShowHUD;
        PC->GetHUD()->bShowHUD=false;
    }
    bSaved=true;
    if(bRootAnchorCandidate)
    {
        if(Mesh->IsRunningParallelEvaluation())
        { Error=TEXT("Cannot edit review instance during parallel evaluation"); Finish(TEXT("FAIL"),Error); return false; }
        int32 Changed=0;
        for(TFieldIterator<FStructProperty> It(Anim->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
        {
            const auto* NP=*It;
            if(NP->Struct->GetPathName()!=TEXT("/Script/KawaiiPhysics.AnimNode_KawaiiPhysics"))continue;
            void* Data=NP->ContainerPtrToValuePtr<void>(Anim);
            const FName Root=HCReadBone(NP->Struct,Data,TEXT("RootBone"));
            if(Root!=FName(Roots[0])&&Root!=FName(Roots[6]))continue;
            const auto* Anchor=FindFProperty<FBoolProperty>(NP->Struct,TEXT("bAnchorFixedStepOutputToCurrentPose"));
            if(!Anchor)continue;
            Anchor->SetPropertyValue_InContainer(Data,true); ++Changed;
        }
        if(Changed!=2){ Error=TEXT("Expected exactly two review hair nodes"); Finish(TEXT("FAIL"),Error); return false; }
    }
    Anim->bVS2IdleGlances=false;
    Expression->StopSpeaking();
    Expression->SetEmotion(TEXT("Neutral"));
    Character->SetSprinting(false);
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    LookTarget=GetWorld()->SpawnActor<ATargetPoint>();
    if(!Camera||!LookTarget)
    {
        Error=TEXT("Native review actors unavailable");
        return false;
    }
    Camera->GetCameraComponent()->SetFieldOfView(40);
    Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
    PC->SetViewTarget(Camera);
    Origin=Character->GetActorLocation();
    AddTickPrerequisiteComponent(Mesh);
    AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&AHCM5VS2HairReviewDirector::Processed);
    return SetPhase(0);
}
bool AHCM5VS2HairReviewDirector::PrepareEdges(FString& Error)
{
    Error=TEXT("Actual LOD0 hair render geometry/CPU skin weights unavailable");
    auto* Mesh=Character->GetMesh();
    auto* Data=ExpectedMesh->GetResourceForRendering();
    auto* Weights=Mesh->GetSkinWeightBuffer(0);
    if(!Data||!Data->LODRenderData.IsValidIndex(0)||!Weights)return false;
    const auto& LOD=Data->LODRenderData[0];
    if(LOD.GetNumVertices()==0||LOD.GetNumVertices()>200000||Weights->GetNumVertices()!=LOD.GetNumVertices())return false;
    TArray<uint32> Triangles;
    LOD.MultiSizeIndexContainer.GetIndexBuffer(Triangles);
    TSet<uint64> Seen;
    TArray<uint16> Masks;
    Masks.Init(0,LOD.GetNumVertices());
    int32 Sections=0,Cross=0,Degenerate=0;
    for(const auto& S:LOD.RenderSections)
    {
        if(S.MaterialIndex!=0||S.bDisabled)continue;
        ++Sections;
        if(S.BaseVertexIndex+S.NumVertices>LOD.GetNumVertices()||S.BaseIndex+S.NumTriangles*3>uint32(Triangles.Num()))return false;
        for(uint32 I=S.BaseVertexIndex;I<S.BaseVertexIndex+S.NumVertices;++I)
        {
            HairVertices.Add(I);
            for(uint32 W=0;W<Weights->GetMaxBoneInfluences();++W)if(Weights->GetBoneWeight(I,W)>655)
            {
                const uint32 B=Weights->GetBoneIndex(I,W);
                if(!S.BoneMap.IsValidIndex(B))return false;
                Masks[I]|=Mask(ExpectedMesh->GetRefSkeleton().GetBoneName(S.BoneMap[B]));
            }
        }
        for(uint32 I=S.BaseIndex;I<S.BaseIndex+S.NumTriangles*3;I+=3)for(int32 E=0;E<3;++E)
        {
            const uint32 A=FMath::Min(Triangles[I+E],Triangles[I+(E+1)%3]),B=FMath::Max(Triangles[I+E],Triangles[I+(E+1)%3]);
            if(A>=LOD.GetNumVertices()||B>=LOD.GetNumVertices())return false;
            const uint64 Key=(uint64(A)<<32)|B;
            if(Seen.Contains(Key))continue;
            Seen.Add(Key);
            const uint16 Chains=Masks[A]|Masks[B];
            if(!Chains)continue;
            const double Length=FVector3f::Distance(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(A),LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(B));
            if(Length<.0001)
            {
                ++Degenerate;
                continue;
            }
            Edges.Add(
            {
                A,B,Chains,Length
            }
            );
            if(Chains&(Chains-1))++Cross;
        }
    }
    if(!Sections||Edges.IsEmpty()||Edges.Num()>60000||Cross==0)return false;
    MeshAudit=MakeShared<FJsonObject>();
    MeshAudit->SetNumberField(TEXT("hair_material_slot"),0);
    MeshAudit->SetStringField(TEXT("actual_hair_material"),GetPathNameSafe(Mesh->GetMaterial(0)));
    MeshAudit->SetNumberField(TEXT("hair_sections"),Sections);
    MeshAudit->SetNumberField(TEXT("hair_vertices"),HairVertices.Num());
    MeshAudit->SetNumberField(TEXT("native_unique_longhair_edges"),Edges.Num());
    MeshAudit->SetNumberField(TEXT("edges_with_multiple_longhair_chain_influences"),Cross);
    MeshAudit->SetNumberField(TEXT("degenerate_edges_excluded"),Degenerate);
    MeshAudit->SetStringField(TEXT("index_scope"),TEXT("Current native merged LOD0 render vertex IDs; NOT original FBX control point IDs. Chain labels use per-section BoneMap and weights >1 percent."));
    return true;
}
bool AHCM5VS2HairReviewDirector::SetPhase(int32 Index)
{
    if(Index<0||Index>=(bOutlineMaskComparison?6:7))return false;
    Phase=Index;
    Age=0;
    bCaptured=false;
    if(bOutlineMaskComparison)
    {
        // Change only the hair slot of the second pass. Main hair/cape and all other outline slots remain untouched.
        OutlineMesh->SetMaterial(0,OutlineComparisonMaterials[Index%3]);
        bOutlineMaterialsReady=false; NextOutlinePoll=0; LastOutlineReadiness.Reset();
        auto Event=MakeShared<FJsonObject>(); Event->SetStringField(TEXT("phase"),PhaseLabel());
        Event->SetStringField(TEXT("hair_outline_material"),GetPathNameSafe(OutlineMesh->GetMaterial(0)));
        Event->SetNumberField(TEXT("frame"),double(GFrameCounter)); Event->SetBoolField(TEXT("same_pose_locked"),bPoseFrozen);
        Events.Add(Event); return true;
    }
    const int32 Value=Index==1?0:1;
    Switch()->Set(Value,ECVF_SetByCode);
    if(Switch()->GetInt()!=Value)
    {
        Finish(TEXT("FAIL"),TEXT("Transient Kawaii switch could not take effect"));
        return false;
    }
    // Kawaii IsValidToEvaluate bypass does not reset its stale particle history.
    // Its ResetDynamics records ResetPhysics and clears the substep accumulator.
    if(Index<=2)Character->GetMesh()->ResetAnimInstanceDynamics(ETeleportType::ResetPhysics);
    auto J=MakeShared<FJsonObject>();
    J->SetStringField(TEXT("phase"),Labels[Phase]);
    J->SetNumberField(TEXT("frame"),double(GFrameCounter));
    J->SetNumberField(TEXT("kawaii"),Value);
    J->SetStringField(TEXT("reset"),Index<=2?TEXT("ResetAnimInstanceDynamics(ResetPhysics), then phase settle; no asset/config write"):TEXT("None; retain continuous particle history through head turn/walk/stop"));
    Events.Add(J);
    return true;
}
TSharedPtr<FJsonObject> AHCM5VS2HairReviewDirector::Bones() const
{
    auto R=MakeShared<FJsonObject>();
    const auto* Mesh=Character->GetMesh();
    R->SetStringField(TEXT("phase"),PhaseLabel());
    R->SetNumberField(TEXT("frame"),double(GFrameCounter));
    R->SetNumberField(TEXT("active_seconds"),Active);
    R->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-Started);
    R->SetNumberField(TEXT("phase_seconds"),Age);
    R->SetNumberField(TEXT("kawaii"),Switch()->GetInt());
    R->SetNumberField(TEXT("actual_lod"),Mesh->GetPredictedLODLevel());
    R->SetArrayField(TEXT("mesh_world_scale"),V(Mesh->GetComponentScale()));
    R->SetArrayField(TEXT("actor_world"),V(Character->GetActorLocation()));
    R->SetArrayField(TEXT("velocity"),V(Character->GetVelocity()));
    TArray<TSharedPtr<FJsonValue>> A;
    const auto& Ref=ExpectedMesh->GetRefSkeleton();
    for(int32 I=0;I<Names.Num();++I)
    {
        const auto T=Mesh->GetSocketTransform(Names[I],RTS_Component),P=Mesh->GetSocketTransform(Ref.GetBoneName(Parents[I]),RTS_Component);
        auto J=MakeShared<FJsonObject>();
        J->SetStringField(TEXT("bone"),Names[I].ToString());
        J->SetStringField(TEXT("parent"),Ref.GetBoneName(Parents[I]).ToString());
        J->SetArrayField(TEXT("cs_position_cm"),V(T.GetLocation()));
        J->SetArrayField(TEXT("parent_cs_position_cm"),V(P.GetLocation()));
        J->SetArrayField(TEXT("cs_quaternion"),Q(T.GetRotation()));
        J->SetArrayField(TEXT("cs_scale"),V(T.GetScale3D()));
        J->SetNumberField(TEXT("reference_local_segment_cm"),BindLengths[I]);
        J->SetNumberField(TEXT("actual_cs_segment_cm"),FVector::Distance(T.GetLocation(),P.GetLocation()));
        J->SetNumberField(TEXT("length_ratio_to_reference"),BindLengths[I]>.0001?FVector::Distance(T.GetLocation(),P.GetLocation())/BindLengths[I]:0);
        A.Add(MakeShared<FJsonValueObject>(J));
    }
    R->SetArrayField(TEXT("bones"),A);
    if(PC->PlayerCameraManager)
    {
        R->SetArrayField(TEXT("final_camera_world"),V(PC->PlayerCameraManager->GetCameraLocation()));
        const FRotator T=PC->PlayerCameraManager->GetCameraRotation();
        R->SetArrayField(TEXT("camera_pitch_yaw_roll"),V(FVector(T.Pitch,T.Yaw,T.Roll)));
    }
    return R;
}
TSharedPtr<FJsonObject> AHCM5VS2HairReviewDirector::Skin()
{
    auto J=Bones();
    auto* Mesh=Character->GetMesh();
    const auto& LOD=ExpectedMesh->GetResourceForRendering()->LODRenderData[0];
    auto* Weights=Mesh->GetSkinWeightBuffer(0);
    TArray<TSharedPtr<FJsonValue>> Morphs;
    for(const UMorphTarget* Morph:ExpectedMesh->GetMorphTargets())if(FMath::Abs(Mesh->GetMorphTarget(Morph->GetFName()))>.0001&&Morph->HasDataForLOD(0))
    {
        bool Hair=false;
        for(const auto& D:Morph->GetMorphLODModels()[0].Vertices)if(HairVertices.Contains(D.SourceIdx)&&!D.PositionDelta.IsNearlyZero())
        {
            Hair=true;
            break;
        }
        if(Hair)Morphs.Add(MakeShared<FJsonValueString>(Morph->GetName()));
    }
    J->SetArrayField(TEXT("active_hair_position_morphs"),Morphs);
    J->SetStringField(TEXT("scope"),TEXT("Native ComputeSkinnedPositions from actual current reference-to-local matrices and skin weights; no render-state rebuild. Excludes material WPO/alpha and GPU skin-cache differences; native edge IDs are not source FBX IDs. Not an FPS sample."));
    if(!Morphs.IsEmpty())
    {
        J->SetStringField(TEXT("status"),TEXT("NOT_RUN"));
        J->SetStringField(TEXT("detail"),TEXT("Active hair morph position deltas require separate morph-aware skin readback; not silently omitted"));
        return J;
    }
    const double Start=FPlatformTime::Seconds();
    TArray<FMatrix44f> Matrices;
    TArray<FVector3f> Positions;
    Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
    USkinnedMeshComponent::ComputeSkinnedPositions(Mesh,Positions,Matrices,LOD,*Weights);
    if(Positions.Num()!=int32(LOD.GetNumVertices()))
    {
        J->SetStringField(TEXT("status"),TEXT("FAIL"));
        return J;
    }
    TArray<double> Ratios,Cross,Lengths,OffRatios;
    TArray<int32> Order;
    for(int32 I=0;I<Edges.Num();++I)
    {
        const auto& E=Edges[I];
        const double L=FVector3f::Distance(Positions[E.A],Positions[E.B]);
        if(!FMath::IsFinite(L))
        {
            J->SetStringField(TEXT("status"),TEXT("FAIL"));
            return J;
        }
        Lengths.Add(L);
        Ratios.Add(L/E.ReferenceLength);
        Order.Add(I);
        if(E.Chains&(E.Chains-1))Cross.Add(Ratios.Last());
        if(OffLengths.IsValidIndex(I)&&OffLengths[I]>.0001)OffRatios.Add(L/OffLengths[I]);
    }
    if(!bOutlineMaskComparison&&Phase==1)OffLengths=Lengths;
    Order.Sort([&](int32 A,int32 B)
    {
        return Ratios[A]>Ratios[B];
    }
    );
    TArray<TSharedPtr<FJsonValue>> Top;
    for(int32 K=0;K<FMath::Min(30,Order.Num());++K)
    {
        const int32 I=Order[K];
        const auto& E=Edges[I];
        auto R=MakeShared<FJsonObject>();
        R->SetNumberField(TEXT("vertex_a"),E.A);
        R->SetNumberField(TEXT("vertex_b"),E.B);
        R->SetNumberField(TEXT("reference_cm"),E.ReferenceLength);
        R->SetNumberField(TEXT("posed_cm"),Lengths[I]);
        R->SetNumberField(TEXT("ratio"),Ratios[I]);
        if(OffLengths.IsValidIndex(I)&&OffLengths[I]>.0001)R->SetNumberField(TEXT("ratio_to_off"),Lengths[I]/OffLengths[I]);
        TArray<TSharedPtr<FJsonValue>> C;
        for(int32 B=0;B<8;++B)if(E.Chains&(1<<B))C.Add(MakeShared<FJsonValueString>(Roots[B]));
        R->SetArrayField(TEXT("influential_chains"),C);
        FVector2D P;
        if(UGameplayStatics::ProjectWorldToScreen(PC,Mesh->GetComponentTransform().TransformPosition(FVector(Positions[E.A])),P))R->SetArrayField(TEXT("vertex_a_screen_xy"),
        {
            MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)
        }
        );
        Top.Add(MakeShared<FJsonValueObject>(R));
    }
    J->SetObjectField(TEXT("all_longhair_edge_ratios"),Stats(Ratios));
    J->SetObjectField(TEXT("multiple_chain_edge_ratios"),Stats(Cross));
    J->SetObjectField(TEXT("ratios_to_physics_off"),Stats(OffRatios));
    J->SetArrayField(TEXT("largest_30_reference_ratios"),Top);
    J->SetNumberField(TEXT("cpu_readback_wall_ms"),(FPlatformTime::Seconds()-Start)*1000);
    J->SetStringField(TEXT("status"),TEXT("PASS"));
    J->SetStringField(TEXT("interpretation"),TEXT("PASS means finite readback only; edge stretch, naturalness and visible thin-line cause remain USER_REVIEW"));
    return J;
}
const TCHAR* AHCM5VS2HairReviewDirector::PhaseLabel() const
{
    return bOutlineMaskComparison?OutlineLabels[Phase]:Labels[Phase];
}
bool AHCM5VS2HairReviewDirector::FreezeOutlinePose()
{
    auto* Mesh=Character->GetMesh();
    Character->GetCharacterMovement()->StopMovementImmediately();
    bOriginalMeshTick=Mesh->IsComponentTickEnabled();
    bOriginalExpressionTick=Expression->IsComponentTickEnabled();
    bOriginalCharacterTick=Character->IsActorTickEnabled();
    bOriginalMovementTick=Character->GetCharacterMovement()->IsComponentTickEnabled();
    // Retain the actual settled evaluated pose; no reference-pose substitution and no bone edits.
    Mesh->SetComponentTickEnabled(false); Expression->SetComponentTickEnabled(false);
    Character->SetActorTickEnabled(false); Character->GetCharacterMovement()->SetComponentTickEnabled(false);
    FrozenMeshTransform=Mesh->GetComponentTransform(); FrozenBones.Reset(); FrozenMorphs.Reset();
    FrozenPoseEvidence=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> BoneRows,MorphRows;
    const auto& Ref=ExpectedMesh->GetRefSkeleton();
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const FName Name=Ref.GetBoneName(I); const FTransform T=Mesh->GetSocketTransform(Name,RTS_Component);
        FrozenBones.Add(T); auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("bone"),Name.ToString());
        Row->SetArrayField(TEXT("component_position_cm"),V(T.GetLocation())); Row->SetArrayField(TEXT("component_rotation_xyzw"),Q(T.GetRotation()));
        Row->SetArrayField(TEXT("component_scale"),V(T.GetScale3D())); BoneRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    for(const UMorphTarget* Morph:ExpectedMesh->GetMorphTargets())
    {
        const float Value=Mesh->GetMorphTarget(Morph->GetFName()); FrozenMorphs.Add(Morph->GetFName(),Value);
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"),Morph->GetName()); Row->SetNumberField(TEXT("weight"),Value);
        MorphRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    FrozenPoseEvidence->SetArrayField(TEXT("all_bones"),BoneRows); FrozenPoseEvidence->SetArrayField(TEXT("all_morphs"),MorphRows);
    FrozenPoseEvidence->SetNumberField(TEXT("frozen_at_frame"),double(GFrameCounter));
    FrozenPoseEvidence->SetNumberField(TEXT("kawaii_at_freeze"),Switch()->GetInt());
    FrozenPoseEvidence->SetStringField(TEXT("method"),TEXT("After actual idle settle, suspend body/expression/character/movement ticks; outline remains source LeaderPose. No mesh/physics/animation asset mutation."));
    bPoseFrozen=true; return !FrozenBones.IsEmpty()&&!FrozenMorphs.IsEmpty();
}
TSharedPtr<FJsonObject> AHCM5VS2HairReviewDirector::OutlineObservation() const
{
    auto J=MakeShared<FJsonObject>(); auto* Mesh=Character->GetMesh(); const auto& Ref=ExpectedMesh->GetRefSkeleton();
    double Position=0,Angle=0,Scale=0,MorphDelta=0; bool Same=bPoseFrozen&&FrozenBones.Num()==Ref.GetNum();
    if(Same)for(int32 I=0;I<Ref.GetNum();++I)
    {
        const auto T=Mesh->GetSocketTransform(Ref.GetBoneName(I),RTS_Component);
        Position=FMath::Max(Position,FVector::Distance(T.GetLocation(),FrozenBones[I].GetLocation()));
        Angle=FMath::Max(Angle,FMath::RadiansToDegrees(T.GetRotation().AngularDistance(FrozenBones[I].GetRotation())));
        Scale=FMath::Max(Scale,FVector::Distance(T.GetScale3D(),FrozenBones[I].GetScale3D()));
    }
    for(const auto& Pair:FrozenMorphs)MorphDelta=FMath::Max(MorphDelta,double(FMath::Abs(Mesh->GetMorphTarget(Pair.Key)-Pair.Value)));
    Same&=Position<.0001&&Angle<.001&&Scale<.00001&&MorphDelta<.000001&&Mesh->GetComponentTransform().Equals(FrozenMeshTransform,.00001);
    bool Materials=OutlineMesh&&OriginalBodyMaterials.Num()==5&&OriginalOutlineMaterials.Num()==5;
    if(Materials)for(int32 I=0;I<5;++I)
        Materials&=Mesh->GetMaterial(I)==OriginalBodyMaterials[I]&&OutlineMesh->GetMaterial(I)==(I==0?OutlineComparisonMaterials[Phase%3]:OriginalOutlineMaterials[I]);
    const auto* PCM=PC->PlayerCameraManager.Get();
    const bool CameraMatch=PCM&&PC->GetViewTarget()==Camera&&PCM->GetCameraLocation().Equals(Camera->GetActorLocation(),.1)
        &&PCM->GetCameraRotation().Equals(Camera->GetActorRotation(),.01)&&FMath::Abs(PCM->GetFOVAngle()-40.f)<.01f;
    J->SetBoolField(TEXT("same_pose"),Same); J->SetNumberField(TEXT("all_bones_checked"),FrozenBones.Num());
    J->SetNumberField(TEXT("max_bone_position_delta_cm"),Position); J->SetNumberField(TEXT("max_bone_rotation_delta_degrees"),Angle);
    J->SetNumberField(TEXT("max_bone_scale_delta"),Scale); J->SetNumberField(TEXT("all_morphs_checked"),FrozenMorphs.Num()); J->SetNumberField(TEXT("max_morph_weight_delta"),MorphDelta);
    J->SetBoolField(TEXT("only_expected_outline_slot_changed"),Materials); J->SetBoolField(TEXT("final_camera_matches_request"),CameraMatch);
    J->SetArrayField(TEXT("requested_camera_world"),V(Camera->GetActorLocation()));
    J->SetArrayField(TEXT("requested_camera_pitch_yaw_roll"),V(FVector(Camera->GetActorRotation().Pitch,Camera->GetActorRotation().Yaw,Camera->GetActorRotation().Roll)));
    if(PCM)J->SetNumberField(TEXT("final_fov"),PCM->GetFOVAngle());
    J->SetBoolField(TEXT("outline_is_source_leader"),OutlineMesh&&OutlineMesh->LeaderPoseComponent.Get()==Mesh);
    TArray<TSharedPtr<FJsonValue>> Components;
    for(const auto* Component:{Mesh,OutlineMesh.Get()})if(Component)
    {
        auto C=MakeShared<FJsonObject>(); C->SetStringField(TEXT("component"),Component->GetPathName());
        C->SetNumberField(TEXT("translucency_sort_priority"),Component->TranslucencySortPriority);
        C->SetNumberField(TEXT("translucency_sort_distance_offset"),Component->TranslucencySortDistanceOffset);
        C->SetBoolField(TEXT("render_custom_depth"),Component->bRenderCustomDepth); C->SetBoolField(TEXT("visible"),Component->IsVisible()&&!Component->bHiddenInGame);
        TArray<TSharedPtr<FJsonValue>> Slots;
        for(int32 I=0;I<Component->GetNumMaterials();++I)
        {
            const auto* M=Component->GetMaterial(I); const auto* Base=M?M->GetMaterial():nullptr; auto S=MakeShared<FJsonObject>();
            S->SetNumberField(TEXT("slot"),I); S->SetStringField(TEXT("material"),GetPathNameSafe(M)); S->SetStringField(TEXT("base_material"),GetPathNameSafe(Base));
            if(M)
            {
                const EBlendMode Blend=M->GetBlendMode();
                S->SetNumberField(TEXT("blend_mode_enum_value"),int32(Blend));
                // GetBlendModeString has no ENGINE_API export in this installed 5.8 binary.
                S->SetStringField(TEXT("blend_mode"),Blend==BLEND_Masked?TEXT("BLEND_Masked"):
                    Blend==BLEND_Translucent?TEXT("BLEND_Translucent"):Blend==BLEND_Opaque?TEXT("BLEND_Opaque"):TEXT("OTHER_SEE_ENUM_VALUE"));
                S->SetNumberField(TEXT("opacity_mask_clip"),M->GetOpacityMaskClipValue()); S->SetBoolField(TEXT("two_sided"),M->IsTwoSided());
                float Width=0; const bool Found=M->GetScalarParameterValue(FMaterialParameterInfo(TEXT("SourceOutlineWidthCm")),Width);
                S->SetBoolField(TEXT("has_outline_width_parameter"),Found); if(Found)S->SetNumberField(TEXT("source_outline_width_world_cm"),Width);
            }
            if(Base)
            { S->SetBoolField(TEXT("base_disable_depth_test"),Base->bDisableDepthTest); S->SetBoolField(TEXT("base_allow_translucent_custom_depth_writes"),Base->AllowTranslucentCustomDepthWrites); }
            Slots.Add(MakeShared<FJsonValueObject>(S));
        }
        C->SetArrayField(TEXT("slots"),Slots); Components.Add(MakeShared<FJsonValueObject>(C));
    }
    J->SetArrayField(TEXT("actual_material_blend_depth_sort"),Components);
    J->SetStringField(TEXT("depth_scope"),TEXT("Public native component/material state; no GPU depth-buffer readback. Main hair/cape blend state is observed, not altered."));
    J->SetStringField(TEXT("status"),Same&&Materials&&CameraMatch?TEXT("PASS"):TEXT("FAIL")); return J;
}
bool AHCM5VS2HairReviewDirector::PollOutlineMaterials(FString& Error)
{
    if(PendingOutlineMaterialCheck)
    {
        if(!PendingOutlineMaterialCheck->Complete.load(std::memory_order_acquire))return false;
        bool All=LastOutlineReadiness->GetBoolField(TEXT("game_thread_ready"));
        const auto& Rows=LastOutlineReadiness->GetArrayField(TEXT("slots"));
        for(int32 I=0;I<PendingOutlineMaterialCheck->Slots.Num();++I)
        {
            auto Row=Rows[I]->AsObject(); const auto& S=PendingOutlineMaterialCheck->Slots[I];
            Row->SetBoolField(TEXT("render_ready_without_fallback"),S.Ready); Row->SetStringField(TEXT("render_actual_resource"),S.Actual); All&=S.Ready;
        }
        PendingOutlineMaterialCheck.Reset(); bOutlineMaterialsReady=All;
        LastOutlineReadiness->SetStringField(TEXT("status"),All?TEXT("READY"):TEXT("NOT_READY"));
        LastOutlineReadiness->SetNumberField(TEXT("observed_frame"),double(GFrameCounter));
        if(LastOutlineReadiness->GetBoolField(TEXT("definite_failure"))) { Error=TEXT("Actual outline/body material usage or compilation failure"); return false; }
    }
    if(bOutlineMaterialsReady)return true;
    const double Now=FPlatformTime::Seconds(); if(Now-Started>120) { Error=TEXT("Bounded outline shader readiness timeout"); return false; }
    if(Now<NextOutlinePoll)return false; NextOutlinePoll=Now+.5;
    CheckedOutlineMaterials.Reset(); LastOutlineReadiness=MakeShared<FJsonObject>();
    auto State=MakeShared<FHCM5VS2HairOutlineMaterialCheck,ESPMode::ThreadSafe>();
    const auto FeatureLevel=GetWorld()->GetFeatureLevel(); const auto Platform=GetFeatureLevelShaderPlatform_Checked(FeatureLevel);
    bool All=true,Failed=false; TArray<TSharedPtr<FJsonValue>> Rows;
    for(auto* Body:{Character->GetMesh(),OutlineMesh.Get()})
    {
        TSet<UMaterialInterface*> MorphMaterials;
        if(const auto* Data=Body->GetSkeletalMeshRenderData())for(const auto& ActiveMorph:Body->ActiveMorphTargets)
        {
            if(!ActiveMorph.Key)continue; const auto& LODs=ActiveMorph.Key->GetMorphLODModels();
            for(int32 L=0;L<FMath::Min(Data->LODRenderData.Num(),LODs.Num());++L)for(int32 S:LODs[L].SectionIndices)
                if(Data->LODRenderData[L].RenderSections.IsValidIndex(S))MorphMaterials.Add(Body->GetMaterial(Data->LODRenderData[L].RenderSections[S].MaterialIndex));
        }
        for(int32 I=0;I<Body->GetNumMaterials();++I)
        {
            auto* M=Body->GetMaterial(I); auto* Base=M?M->GetMaterial():nullptr; auto* Resource=M?M->GetMaterialResource(Platform):nullptr;
            bool Compiling=M&&M->IsCompiling(); TArray<TSharedPtr<FJsonValue>> Errors;
#if WITH_EDITOR
            if(Resource) { Compiling|=!Resource->IsCompilationFinished(); for(const auto& E:Resource->GetCompileErrors())if(Errors.Num()<8)Errors.Add(MakeShared<FJsonValueString>(E.Left(2048))); }
#endif
            auto* Map=Resource?Resource->GetGameThreadShaderMap():nullptr;
            const bool Usage=Base&&Base->GetUsageByFlag(MATUSAGE_SkeletalMesh)&&(!MorphMaterials.Contains(M)||Base->GetUsageByFlag(MATUSAGE_MorphTargets));
            const bool Complete=Resource&&Resource->IsGameThreadShaderMapComplete();
            const bool CompileFailed=!Errors.IsEmpty()||(Map&&Map->IsCompilationFinalized()&&!Map->CompiledSuccessfully());
#if WITH_EDITOR
            if(Resource&&!Complete&&!CompileFailed)Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
#endif
            const bool Ready=M&&Resource&&Usage&&!Compiling&&!CompileFailed&&Complete&&Map&&Map->IsValidForRendering();
            All&=Ready; Failed|=!M||!Usage||(CompileFailed&&!Compiling);
            auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("component"),Body->GetPathName()); Row->SetNumberField(TEXT("slot"),I);
            Row->SetStringField(TEXT("actual_material"),GetPathNameSafe(M)); Row->SetBoolField(TEXT("required_usage"),Usage);
            Row->SetBoolField(TEXT("game_thread_ready"),Ready); Row->SetBoolField(TEXT("compiling"),Compiling); Row->SetArrayField(TEXT("compile_errors"),Errors);
            Rows.Add(MakeShared<FJsonValueObject>(Row)); CheckedOutlineMaterials.Add(M); State->Slots.AddDefaulted_GetRef().Proxy=M?M->GetRenderProxy():nullptr;
        }
    }
    LastOutlineReadiness->SetStringField(TEXT("status"),TEXT("RENDER_QUERY_PENDING")); LastOutlineReadiness->SetArrayField(TEXT("slots"),Rows);
    LastOutlineReadiness->SetBoolField(TEXT("game_thread_ready"),All&&Rows.Num()==10); LastOutlineReadiness->SetBoolField(TEXT("definite_failure"),Failed||Rows.Num()!=10);
    PendingOutlineMaterialCheck=State;
    ENQUEUE_RENDER_COMMAND(HCM5VS2HairOutlineMaterialReadback)([State,FeatureLevel](FRHICommandListImmediate& RHICmdList)
    {
        for(auto& S:State->Slots)if(S.Proxy)
        {
            const FMaterial* Direct=S.Proxy->GetMaterialNoFallback(FeatureLevel); const auto* Map=Direct?Direct->GetRenderingThreadShaderMap():nullptr;
            const FMaterialRenderProxy* Fallback=nullptr; const FMaterial& Actual=S.Proxy->GetMaterialWithFallback(FeatureLevel,Fallback); S.Actual=Actual.GetFriendlyName();
            S.Ready=Direct&&Direct->IsRenderingThreadShaderMapComplete()&&Map&&Map->IsValidForRendering()&&!Fallback;
        }
        State->Complete.store(true,std::memory_order_release);
    });
    return false;
}
void AHCM5VS2HairReviewDirector::TickOutlineComparison(float Dt)
{
    FString Error; const bool Ready=PollOutlineMaterials(Error);
    if(!Error.IsEmpty()) { Finish(TEXT("FAIL"),Error); return; }
    auto* Mesh=Character->GetMesh();
    if(!bPoseFrozen)
    {
        LookTarget->SetActorLocation(Mesh->GetSocketLocation(TEXT("Head"))+Character->GetActorForwardVector()*300);
        Expression->SetLookTarget(LookTarget);
    }
    const FVector Target=Origin+FVector(0,0,28),Position=Target+FVector(Phase<3?320:-320,0,18);
    Camera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());
    if(!Ready) { Age=0; return; }
    if(!bPoseFrozen)
    {
        Warmup+=FMath::Max(Dt,0.f); if(Warmup<3)return;
        if(!FreezeOutlinePose()) { Finish(TEXT("FAIL"),TEXT("Unable to freeze actual evaluated native pose")); return; }
        Age=0; return;
    }
    if(!Pending) { Age+=FMath::Max(Dt,0.f); Active+=FMath::Max(Dt,0.f); }
    if(Age>=1.5&&!bCaptured&&!Pending&&!FScreenshotRequest::IsScreenshotRequested())
    {
        auto Observation=OutlineObservation();
        if(Observation->GetStringField(TEXT("status"))!=TEXT("PASS"))
        { FrozenPoseEvidence->SetObjectField(TEXT("failed_observation"),Observation); Finish(TEXT("FAIL"),TEXT("Frozen pose/material/camera invariant changed")); return; }
        Capture();
    }
    if(bCaptured&&!Pending)
    {
        if(Phase==5)Finish(Captures.Num()==6?TEXT("PASS"):TEXT("FAIL"),TEXT("Same-pose native hair-outline comparison collected; original design/physics/alpha contribution remains visual review"));
        else SetPhase(Phase+1);
    }
}
void AHCM5VS2HairReviewDirector::Capture()
{
    if(bOutlineMaskComparison||Phase==0||Phase==1||Phase==2||Phase==5)SkinSamples.Add(Skin());
    PendingPNG=Directory/FString::Printf(TEXT("%02d_%s.png"),Phase,PhaseLabel());
    if(IFileManager::Get().FileExists(*PendingPNG))
    {
        Finish(TEXT("FAIL"),TEXT("Screenshot overwrite refused"));
        return;
    }
    Pending=Bones();
    if(bOutlineMaskComparison)
    {
        Pending->SetObjectField(TEXT("outline_comparison"),OutlineObservation());
        Pending->SetObjectField(TEXT("material_readiness"),LastOutlineReadiness);
    }
    Pending->SetStringField(TEXT("file"),PendingPNG);
    RequestAt=FPlatformTime::Seconds();
    RequestFrame=GFrameCounter;
    bCaptured=true;
    bPendingProcessed=false;
    FScreenshotRequest::RequestScreenshot(PendingPNG,false,false,false,FIntRect(),true);
}
void AHCM5VS2HairReviewDirector::Processed()
{
    if(!bStopped&&Pending)bPendingProcessed=true;
}
void AHCM5VS2HairReviewDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bStopped)return;
    const double Now=FPlatformTime::Seconds();
    if(bExit)
    {
        if(Now-Finished>2)
        {
            bExit=false;
            SetActorTickEnabled(false);
            FPlatformMisc::RequestExit(false,TEXT("M5VS2HairReview complete"));
        }
        return;
    }
    if(!bActive)return;
    if(!InputHandle.IsValid())if(auto* Vp=GetWorld()->GetGameViewport())
    {
        Viewport=Vp;
        InputHandle=Vp->OnInputKey().AddUObject(this,&AHCM5VS2HairReviewDirector::Input);
    }
    if(bSubstepAudit&&bReady&&(!PC||!PC->IsGameplayFocused()||UGameplayStatics::IsGamePaused(this)||!FSlateApplication::IsInitialized()||!FSlateApplication::Get().IsActive()))
    { StopSubstepAudit(TEXT("Pause/focus lost: permanent audit stop; no auto-resume")); return; }
    if(Now-Started>150)
    {
        Finish(TEXT("NOT_RUN"),TEXT("Bounded wall deadline"));
        return;
    }
    if(!bReady)
    {
        FString Error;
        if(Start(Error))
        {
            bReady=true;
            Write(TEXT("RUNNING"),TEXT("Actual candidate bound; finite warmup starts"));
        }
        else if(bActive&&(bSaved||Now-Started>15))Finish(TEXT("FAIL"),Error);
        return;
    }
    if(Pending)
    {
        if(bPendingProcessed&&!FScreenshotRequest::IsScreenshotRequested()&&GFrameCounter>RequestFrame)
        {
            const bool Good=PNG(PendingPNG);
            Pending->SetStringField(TEXT("status"),Good?TEXT("PASS"):TEXT("FAIL"));
            Captures.Add(Pending);
            Pending.Reset();
            if(!Good)
            {
                Finish(TEXT("FAIL"),TEXT("Native PNG missing or not 1920x1080"));
                return;
            }
        }
        else if(Now-RequestAt>15)
        {
            Finish(TEXT("FAIL"),TEXT("Bounded PNG timeout"));
            return;
        }
    }
    if(!PC->IsGameplayFocused()||UGameplayStatics::IsGamePaused(this))return;
    const auto* Mesh=Character->GetMesh();
    if(Mesh->GetSkeletalMeshAsset()!=ExpectedMesh||!Mesh->GetAnimInstance()||Mesh->GetAnimInstance()->GetClass()!=ExpectedAnimationClass||PC->IsFirstPersonPerspective()||Character->IsHidden()||FVector::Dist2D(Character->GetActorLocation(),Origin)>1500)
    {
        Finish(TEXT("FAIL"),TEXT("Runtime binding, visibility or bounded fixture changed"));
        return;
    }
    if(bOutlineMaskComparison) { TickOutlineComparison(Dt); return; }
    bool Shaders=false;
#if WITH_EDITOR
    Shaders=GShaderCompilingManager&&GShaderCompilingManager->GetNumRemainingJobs()>0;
#endif
    if(Shaders)
    {
        Warmup=0;
        if(Now-Started>100)Finish(TEXT("NOT_RUN"),TEXT("Shaders not ready within bounded warmup"));
        return;
    }
    Warmup+=Dt;
    const FVector Forward=Character->GetActorForwardVector(),Head=Mesh->GetSocketLocation(TEXT("Head"));
    LookTarget->SetActorLocation(Head+Forward*300+Character->GetActorRightVector()*(Phase==3?-140.f:0.f));
    Expression->SetLookTarget(LookTarget);
    const FVector Target=Character->GetActorLocation()+FVector(0,0,28);
    const FVector Position=Target+FVector(Phase==6?-320:320,0,18);
    Camera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());
    if(Warmup<3)return;
    if(!Pending)
    {
        Age+=FMath::Max(0.f,Dt);
        Active+=FMath::Max(0.f,Dt);
    }
    if(bSubstepAudit)
    {
        if(SubstepFrames.Num()>=4500){Finish(TEXT("NOT_RUN"),TEXT("Per-frame audit reached 4500-row bound"));return;}
        auto Observation=SubstepObservation(Dt); SubstepFrames.Add(Observation);
        if(Observation->GetStringField(TEXT("status"))!=TEXT("PASS_READBACK_ONLY"))
        { Finish(TEXT("FAIL"),TEXT("Substep readback not safe or exact; no guessed solver fields"));return; }
    }
    if(Phase==4&&!bCaptured)Character->AddMovementInput(FVector(1,0,0),1.f);
    if(Now-LastSample>=.1&&Samples.Num()<650)
    {
        Samples.Add(Bones());
        LastSample=Now;
    }
    if(Age>=Durations[Phase]&&!bCaptured&&!Pending&&!FScreenshotRequest::IsScreenshotRequested())Capture();
    if(bCaptured&&!Pending)
    {
        if(Phase==6)Finish(Captures.Num()==7?TEXT("PASS"):TEXT("FAIL"),TEXT("Bounded native diagnosis collected; visible hair cause and art remain USER_REVIEW"));
        else SetPhase(Phase+1);
    }
}
void AHCM5VS2HairReviewDirector::Input(const FInputKeyEventArgs& Event)
{
    if(bSubstepAudit&&(bActive||bExit)&&!bStopped&&Event.Event==IE_Pressed&&(Event.Key==EKeys::P||Event.Key==EKeys::Escape))
    { StopSubstepAudit(Event.Key==EKeys::Escape?TEXT("Esc stop latched"):TEXT("P pause latched diagnostic stop")); return; }
    if((bActive||bExit)&&!bStopped&&Event.Event==IE_Pressed&&Event.Key==EKeys::Escape)
    {
        bStopped=true;
        bActive=bAutoQuit=bExit=false;
        StopFrame=GFrameCounter;
        if(Pending&&FScreenshotRequest::IsScreenshotRequested()&&FScreenshotRequest::GetFilename()==PendingPNG)FScreenshotRequest::Reset();
        Write(TEXT("NOT_RUN"),TEXT("Esc stopped all capture, native movement, camera, reset, restore and autoquit. Temporary CVar remains this process only; process exit discards it."));
        SetActorTickEnabled(false);
    }
}
void AHCM5VS2HairReviewDirector::Restore()
{
    if(bStopped||!bSaved)return;
    bSaved=false;
    if(Switch())
    {
        Switch()->Set(OriginalKawaii,ECVF_SetByCode);
        Switch()->SetFlags(EConsoleVariableFlags(OriginalKawaiiFlags));
    }
    if(Character)
    {
        if(bOutlineMaskComparison)
        {
            if(OutlineMesh&&OriginalOutlineMaterials.Num()==5)OutlineMesh->SetMaterial(0,OriginalOutlineMaterials[0]);
            if(bPoseFrozen)
            {
                Character->GetMesh()->SetComponentTickEnabled(bOriginalMeshTick);
                Character->SetActorTickEnabled(bOriginalCharacterTick);
                Character->GetCharacterMovement()->SetComponentTickEnabled(bOriginalMovementTick);
                if(Expression)Expression->SetComponentTickEnabled(bOriginalExpressionTick);
                bPoseFrozen=false;
            }
        }
        if(bRootAnchorCandidate)
        {
            auto* Mesh=Character->GetMesh();
            auto* Anim=Mesh->GetAnimInstance();
            if(Anim&&!Mesh->IsRunningParallelEvaluation())
                for(TFieldIterator<FStructProperty> It(Anim->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
                {
                    const auto* NP=*It;
                    if(NP->Struct->GetPathName()!=TEXT("/Script/KawaiiPhysics.AnimNode_KawaiiPhysics"))continue;
                    void* Data=NP->ContainerPtrToValuePtr<void>(Anim);
                    const FName Root=HCReadBone(NP->Struct,Data,TEXT("RootBone"));
                    if(Root!=FName(Roots[0])&&Root!=FName(Roots[6]))continue;
                    if(const auto* Anchor=FindFProperty<FBoolProperty>(NP->Struct,TEXT("bAnchorFixedStepOutputToCurrentPose")))
                        Anchor->SetPropertyValue_InContainer(Data,false);
                }
        }
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetMesh()->ResetAnimInstanceDynamics(ETeleportType::ResetPhysics);
        if(auto* A=Cast<UHCM5VS2LookAnimInstance>(Character->GetMesh()->GetAnimInstance()))A->bVS2IdleGlances=bOriginalGlances;
    }
    if(Expression)Expression->ClearLookTarget();
    if(PC)
    {
        if(OriginalView.IsValid())PC->SetViewTarget(OriginalView.Get());
        if(PC->GetHUD())PC->GetHUD()->bShowHUD=bOriginalHUD;
    }
}
void AHCM5VS2HairReviewDirector::Finish(const FString& Status,const FString& Detail)
{
    if(bStopped)return;
    bActive=false;
    Finished=FPlatformTime::Seconds();
    Restore();
    Write(Status,Detail);
    bExit=bAutoQuit;
    SetActorTickEnabled(bExit);
    UE_LOG(LogTemp,Display,TEXT("M5VS2_HAIR_REVIEW_%s %s"),*Status,*Directory);
}
void AHCM5VS2HairReviewDirector::Write(const FString& Status,const FString& Detail)
{
    if(Directory.IsEmpty())return;
    auto J=MakeShared<FJsonObject>();
    J->SetStringField(TEXT("status"),Status);
    J->SetStringField(TEXT("detail"),Detail);
    J->SetStringField(TEXT("visual_acceptance"),TEXT("USER_REVIEW"));
    J->SetStringField(TEXT("runtime_layer"),TEXT("EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED"));
    J->SetStringField(TEXT("input_scope"),TEXT("Native Expression SetLookTarget and Character AddMovementInput; no Action injection or OS input; cameras are diagnostic framing, not player mouse evidence."));
    J->SetStringField(TEXT("sampling_scope"),TEXT("Nominal 10Hz PostUpdateWork after mesh tick; actual frames/times retained. PNG request observation may precede rendered screenshot. Four bounded CPU bone-skin samples exclude material WPO/alpha. No performance claims."));
    J->SetStringField(TEXT("source_report"),SourceReport);
    J->SetStringField(TEXT("character"),GetPathNameSafe(ExpectedCharacterClass));
    J->SetStringField(TEXT("mesh"),GetPathNameSafe(ExpectedMesh));
    J->SetStringField(TEXT("animation"),GetPathNameSafe(ExpectedAnimationClass));
    J->SetBoolField(TEXT("outline_mask_comparison"),bOutlineMaskComparison);
    if(bOutlineMaskComparison)
    {
        J->SetStringField(TEXT("input_scope"),TEXT("Six native fixed-camera screenshots of one settled frozen body pose; not OS mouse or gameplay input evidence."));
        J->SetStringField(TEXT("sampling_scope"),TEXT("Only hair outline slot changes: unchanged clone, mask zero, original main texture Alpha times existing backface mask. Same clip/width/WPO. All body bones and morph weights verified each shot. Front/rear each share final camera. Main hair/cape unchanged; continuous cape sorting not tested."));
        if(FrozenPoseEvidence)J->SetObjectField(TEXT("frozen_pose_reference"),FrozenPoseEvidence);
        if(LastOutlineReadiness)J->SetObjectField(TEXT("last_material_readiness"),LastOutlineReadiness);
    }
    J->SetNumberField(TEXT("elapsed_wall_seconds"),FPlatformTime::Seconds()-Started);
    J->SetNumberField(TEXT("active_seconds"),Active);
    J->SetNumberField(TEXT("longhair_bones"),Names.Num());
    J->SetBoolField(TEXT("os_input_used"),false);
    J->SetBoolField(TEXT("user_stop_latched"),bStopped);
    J->SetNumberField(TEXT("stop_frame"),double(StopFrame));
    J->SetBoolField(TEXT("transient_state_restored"),!bSaved);
    J->SetNumberField(TEXT("original_kawaii"),OriginalKawaii);
    if(Switch())J->SetNumberField(TEXT("current_kawaii"),Switch()->GetInt());
    J->SetArrayField(TEXT("samples"),Rows(Samples));
    J->SetArrayField(TEXT("captures"),Rows(Captures));
    J->SetArrayField(TEXT("cpu_skin_samples"),Rows(SkinSamples));
    J->SetArrayField(TEXT("switch_and_reset_events"),Rows(Events));
    if(MeshAudit)J->SetObjectField(TEXT("native_hair_geometry"),MeshAudit);
    if(bSubstepAudit)
    {
        auto Audit=MakeShared<FJsonObject>(); bool Actual=false;const bool Valid=HCSubstepSetting(Actual);
        Audit->SetBoolField(TEXT("settings_readback_valid"),Valid);
        Audit->SetBoolField(TEXT("actual_fixed_substepping"),Actual);
        Audit->SetBoolField(TEXT("requested_fixed_substepping"),ExpectedFixedSubstep==1);
        Audit->SetBoolField(TEXT("root_anchor_candidate"),bRootAnchorCandidate);
        Audit->SetStringField(TEXT("scope"),TEXT("Process-wide temporary ini override affects ALL Kawaii nodes in this private process, not a per-node override. No config/CDO mutation by Director."));
        Audit->SetStringField(TEXT("NumSteps"),TEXT("NOT_EXPOSED_NOT_MEASURED"));
        Audit->SetArrayField(TEXT("frames"),Rows(SubstepFrames));
        J->SetObjectField(TEXT("substep_audit"),Audit);
    }
    FString Text;
    const bool Serialized=FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));
    if(!bSubstepAudit)
    { FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("hair_review.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM); return; }
    const FString Tmp=Directory/(TEXT("hair_substep_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".tmp"));
    const FString Dest=Directory/TEXT("hair_review.json");
    bool Published=Serialized&&FFileHelper::SaveStringToFile(Text,*Tmp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
#if PLATFORM_WINDOWS
    if(Published)
    {
        const FString From=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Tmp).Replace(TEXT("/"),TEXT("\\"));
        const FString To=FString(TEXT("\\\\?\\"))+FPaths::ConvertRelativePathToFull(Dest).Replace(TEXT("/"),TEXT("\\"));
        Published=::MoveFileExW(*From,*To,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    }
#else
    Published=false;
#endif
    if(!Published)
    {
        UE_LOG(LogTemp,Error,TEXT("M5VS2_HAIR_SUBSTEP_PUBLISH_FAIL error=%u file=%s"),FPlatformMisc::GetLastError(),*Dest);
        bActive=bAutoQuit=bExit=false;
        if(Character&&Character->GetCharacterMovement())Character->GetCharacterMovement()->StopMovementImmediately();
        if(!bStopped)FPlatformMisc::RequestExitWithStatus(false,1,TEXT("Hair substep report publication failed"));
    }
}
void AHCM5VS2HairReviewDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bActive&&!bStopped)Finish(TEXT("NOT_RUN"),TEXT("World ended before diagnostic completed"));
    bExit=false;
    if(Viewport.IsValid())Viewport->OnInputKey().Remove(InputHandle);
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    if(!bStopped)Restore();
    Super::EndPlay(Reason);
}
