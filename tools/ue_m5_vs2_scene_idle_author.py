"""Three original contextual arm gestures in an isolated existing harbor copy."""
from pathlib import Path
import datetime as dt, hashlib, json, re, traceback
import unreal as U
W=Path('D:/科研学习/codex学习'); P=W/'HarborCity'; D=W/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary; B=U.BlueprintEditorLibrary
L=U.get_editor_subsystem(U.LevelEditorSubsystem); E=U.get_editor_subsystem(U.EditorActorSubsystem)
def arg(n):
    v=re.findall(r'(?:^|\s)-'+n+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line()); assert len(v)==1
    return v[0][0] or v[0][1]
O=Path(arg('M5EvidenceDir')); assert O.is_relative_to(D/'editor_runtime') and arg('M5AuthorPhase')=='SceneIdleAuthor'
token=hashlib.sha256(str(O).encode()).hexdigest()[:12]
R=dict(status='RUNNING',phase='SceneIdleAuthor',sources={},assets=[],subjects={},checks=[],runtime='NOT_RUN',visual='USER_REVIEW')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def doc(p): return json.loads(Path(p).read_text('utf-8-sig'))
def pkg(x): return (x.get_path_name() if hasattr(x,'get_path_name') else str(x)).split('.')[0]
def disk(x,ext='.uasset'): return P/'Content'/Path(pkg(x).removeprefix('/Game/')).with_suffix(ext)
def keep(p): R['sources'][str(p)]=sha(p)
def check(n,ok):
    R['checks'].append(dict(name=n,status='PASS' if ok else 'FAIL'))
    if not ok: raise RuntimeError(n)
def duplicate(source,target):
    keep(disk(source)); check('fresh asset '+target,not A.does_asset_exist(target))
    result=A.duplicate_asset(pkg(source),target); check('native duplicate '+target,result is not None); return result
def save(x):
    check('native save '+pkg(x),A.save_loaded_asset(x,False)); R['assets'].append(dict(path=pkg(x),sha256=sha(disk(x))))
try:
    for letter,activity in [('Q','Vendor'),('R','SeaGaze'),('T','Chat')]:
        files=sorted((D/'editor_runtime').glob('author_*_NPCIdleR2'+letter+'Reload/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
        file=next(f for f in files if doc(f)['status']=='PASS'); prior=doc(file); keep(file)
        for a in prior['assets']:
            check('unchanged relaxed-idle prerequisite',sha(disk(a['path']))==a['sha256']);keep(disk(a['path']))
        root='/Game/HarborCity/M5VS2/NPC/AvatarSample_'+letter+'/IdleR2/Batch_'+token
        seq=duplicate(prior['specimen']['idle_animation'],root+'/A_SceneIdle_'+activity)
        motion=json.loads(U.HCM5VS2NPCIdleEditor.author_scene_idle(A.load_asset(prior['specimen']['idle_animation']),seq,U.Name(activity)))
        check('original bounded motion author '+letter,motion.get('status')=='PASS')
        abp=duplicate(prior['source_animation_blueprint'],root+'/ABP_NPCIdleR2_'+letter)
        bs=duplicate(prior['source_blendspace'],root+'/BS_NPCIdleR2_'+letter)
        graph=json.loads(U.HCM5VS2NPCIdleEditor.configure_private_idle(A.load_asset(prior['source_animation_blueprint']),abp,bs,seq,True))
        check('only idle sample changes '+letter,graph.get('status')=='PASS')
        bp=duplicate(prior['candidate_blueprint'],root+'/BP_NPCIdleR2_'+letter)
        body=U.get_default_object(bp.generated_class()).get_component_by_class(U.SkeletalMeshComponent)
        body.modify();body.set_editor_property('anim_class',abp.generated_class())
        check('compile private NPC '+letter,B.compile_blueprint(bp))
        for asset in (seq,bs,abp,bp):save(asset)
        R['subjects'][letter]=dict(activity=activity,blueprint=pkg(bp),animation_blueprint=pkg(abp),animation=pkg(seq),motion=motion,graph=graph)
    source_file=D/'editor_runtime/author_20260925_144955_324_cb37bf54_PlayablePolishAuthor/author_result.json'
    keep(source_file); prior=doc(source_file);check('actual selected corner author passed',prior['status']=='PASS')
    source=prior['maps']['Afternoon'];keep(disk(source,'.umap'))
    R['map']='/Game/HarborCity/M5VS2/WorldRev2/SceneIdleReview_'+token+'/L_SceneIdleReview'
    check('fresh private map',not A.does_asset_exist(R['map']))
    check('copy selected corner',A.duplicate_asset(source,R['map']) is not None);check('load private corner',L.load_level(R['map']))
    actors=E.get_all_level_actors(); npcs=[a for a in actors if isinstance(a,U.HCM5VS2NPC)]
    check('retained eight independent NPCs',len(npcs)==8)
    by={re.search(r'/AvatarSample_([A-Z])/',n.get_component_by_class(U.SkeletalMeshComponent).get_skeletal_mesh_asset().get_path_name())[1]:n for n in npcs}
    positions={'Q':(-1010,-1020,0),'R':(1530,500,0),'T':(-610,250,90)}
    selected=[]
    for letter,row in R['subjects'].items():
        old=by[letter]; transform=old.get_actor_transform(); oldbody=old.get_component_by_class(U.SkeletalMeshComponent)
        mats=[oldbody.get_material(i) for i in range(oldbody.get_num_materials())]
        actor=E.spawn_actor_from_class(A.load_blueprint_class(row['blueprint']),old.get_actor_location(),old.get_actor_rotation(),False)
        check('create scene actor '+letter,actor is not None);actor.set_actor_transform(transform,False,True)
        for key in ('stable_id','stationary','navigation_region','display_name','role_id','talkable','dialogue_lines'):
            actor.set_editor_property(key,old.get_editor_property(key))
        actor.set_editor_property('stationary',True);actor.set_actor_label('HC_SceneIdle_'+letter+'_'+row['activity'])
        body=actor.get_component_by_class(U.SkeletalMeshComponent)
        for i,mat in enumerate(mats):body.set_material(i,mat)
        x,y,yaw=positions[letter];actor.set_actor_location(U.Vector(x,y,old.get_actor_location().z),False,True);actor.set_actor_rotation(U.Rotator(yaw=yaw),True)
        check('remove only replaced actor in copied map',E.destroy_actor(old));selected.append(actor)
        row['position']=actor.get_actor_transform().export_text()
    listener=by['U'];listener.set_actor_location(U.Vector(-610,440,listener.get_actor_location().z),False,True);listener.set_actor_rotation(U.Rotator(yaw=-90),True)
    suns=[a for a in actors if isinstance(a,U.DirectionalLight) and 'HC_VS2_Rev2_Sun' in [str(t) for t in a.get_editor_property('tags')]]
    check('one actual sun',len(suns)==1)
    director=E.spawn_actor_from_class(U.HCM5VS2NPCReviewDirector,U.Vector(0,0,-100))
    director.set_actor_label('HC_Context_Idle_Review');director.set_editor_property('specimens',selected)
    director.set_editor_property('specimen_ids',[U.Name('Q_Vendor'),U.Name('R_SeaGaze'),U.Name('T_Chat')]);director.set_editor_property('main_light',suns[0])
    check('save private scene',L.save_current_level());R['assets'].append(dict(path=R['map'],sha256=sha(disk(R['map'],'.umap'))))
    R.update(status='PASS',source_map=source,scope='Three original small arm gestures over retargeted relaxed idle; private existing street. Saved graph retains locomotion/head-look/hit/getup links. Visual quality and gameplay remain unapproved.')
except Exception:
    R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:
    R['changed_sources']=[p for p,h in R['sources'].items() if sha(p)!=h]
    if R['changed_sources']:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
if R['status']!='PASS':raise RuntimeError('Scene idle author failed')
