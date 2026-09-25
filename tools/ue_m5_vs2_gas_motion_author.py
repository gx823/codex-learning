"""New unbound VS2 start/stop/jump and native warping candidate; root executes UE.

Phases GASMotionCandidate -> GASMotionReload. No live Hero/map writes.
Native graph construction is in HCM5VS2GASMotionEditor, not guessed Python pins.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
DOC, PROJECT = WORK/'docs/HarborCity_M5_VS2', WORK/'HarborCity'
OLD = '/Game/HarborCity/M5VS1/HeroSelestia'
HERO_ROOT = '/Game/HarborCity/M5VS2/HeroSelestia'
SOURCE_BP = HERO_ROOT+'/Animation/ABP_M5VS2_Selestia_Physics'
SOURCE_BS = HERO_ROOT+'/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun'
ROOT = HERO_ROOT+'/Animation/GASMotion'
OWNER = 'HarborCity_M5_VS2_GASMotion_v1'
CLIPS = ['Run_Start_F_Lfoot','Sprint_Start_F_Lfoot','Run_Stop_F_Lfoot','Sprint_Stop_F_Lfoot',
         'Jump_F_Start_Stand_Rfoot','Jump_F_Start_Run_Rfoot','Jump_F_Start_Sprint_Rfoot',
         'Jump_Loop_Fall','Jump_F_Land_Stand_Light_Rfoot','Jump_F_Land_Run_Light_Rfoot']
A, L, P = U.EditorAssetLibrary, U.AnimationLibrary, U.AnimPoseExtensions


def arg(name):
    rows = re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(rows) != 1: raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]


OUT, PHASE = Path(arg('M5EvidenceDir')).resolve(), arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
assert PHASE in ('GASMotionCandidate','GASMotionReload')
OUT.mkdir(parents=True, exist_ok=True)
DEST = ROOT+'/Batch_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
R = dict(status='RUNNING', phase=PHASE, destination=DEST, checks=[], assets=[], inputs={},
         gameplay_binding='NOT_RUN', runtime='NOT_RUN', planted_foot_improvement='NOT_RUN',
         visual='NOT_RUN', user_naturalness='USER_REVIEW', map_writes=0, source_skeleton_writes=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), limitations=[
             'Native source-speed curves are a bounded stride proxy fitted to the existing measured mean foot speed; they do not prove planted feet.',
             'Start, stop, jump and native warping require actual game-state, final-bone and rendered tests before binding to the live Hero.',
             'New movement branch is TP unarmed only. Existing FP/combat branches and downstream slots, look and secondary physics remain.',
             'Flight read-only animation variables are present in the class; this graph does not yet add a flying pose.'])
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')


def check(label, good, observed=None):
    R['checks'].append(dict(name=label,status='PASS' if good else 'FAIL',observed=observed));dump()
    if not good: raise RuntimeError(label+': '+repr(observed))


def package(obj):
    return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]


def disk(obj):
    path=package(obj)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()


def protect(obj, expected=None):
    file=disk(obj);value=sha(file)
    check('source hash '+package(obj),expected is None or value==expected)
    PROTECTED[str(file)]=value


def latest(phase):
    for file in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        data=json.loads(file.read_text('utf-8-sig'))
        if data.get('status')=='PASS' and data.get('phase')==phase:
            R['inputs'][phase]=dict(path=str(file),sha256=sha(file));return data
    raise RuntimeError('No actual PASS '+phase)


def load(path):
    obj=A.load_asset(path);check('load '+package(path),obj is not None);return obj


def native(label, result):
    data=json.loads(result);check(label,data.get('status')=='PASS',data.get('error'));return data


def duplicate(source, leaf):
    dest=DEST+'/'+leaf
    check('fresh candidate '+dest,not A.does_asset_exist(dest) and not disk(dest).exists())
    obj=A.duplicate_asset(package(source),dest);check('native duplicate '+dest,obj is not None);return obj


def save(obj):
    path=package(obj);check('private save namespace',path.startswith(DEST+'/'))
    A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER)
    check('native save '+path,A.save_loaded_asset(obj,False) and disk(path).is_file())
    R['assets'].append(dict(path=path,sha256=sha(disk(path)),bytes=disk(path).stat().st_size,class_name=obj.get_class().get_name()));dump()


def sample_pose_hash(seq, mesh):
    # Exact raw bone/curve preservation for copied loops, all authored frames.
    # Exclude only the new scalar speed curve by not requesting curves.
    options=U.AnimPoseEvaluationOptions()
    for name,value in dict(evaluation_type=U.AnimDataEvalType.RAW,should_retarget=True,
            optional_skeletal_mesh=mesh,extract_root_motion=False,
            incorporate_root_motion_into_pose=True,evaluate_curves=False).items():options.set_editor_property(name,value)
    count=int(seq.get_editor_property('data_model_interface').get_number_of_keys())
    h=hashlib.sha256();bone_count=0
    for frame in range(count):
        pose=P.get_anim_pose_at_frame(seq,frame,options)
        if not P.is_valid(pose):raise RuntimeError('Invalid loop raw pose '+seq.get_name())
        names=P.get_bone_names(pose);bone_count=len(names)
        for name in names:
            t=P.get_bone_pose(pose,name,U.AnimPoseSpaces.LOCAL)
            values=[t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,t.scale3d.x,t.scale3d.y,t.scale3d.z]
            if not all(math.isfinite(x) for x in values):raise RuntimeError('Nonfinite loop raw transform')
            h.update(json.dumps([str(name),values],separators=(',',':'),allow_nan=False).encode())
    return dict(keys=count,bones=bone_count,all_local_bones_all_keys_sha256=h.hexdigest())


def curve_hashes(seq, ignore=()):
    result={}
    for name in L.get_animation_curve_names(seq,U.RawCurveTrackTypes.RCT_FLOAT):
        if str(name) in ignore:continue
        times,values=L.get_float_keys(seq,name)
        # Include midpoints so equal key values alone do not hide tangent changes.
        samples=sorted(set(list(times)+[(a+b)/2 for a,b in zip(times,times[1:])]))
        result[str(name)]=dict(keys=len(times),times=list(times),values=list(values),
                              mid_functions=[float(L.get_float_value_at_time(seq,name,t)) for t in samples])
    return result


try:
    check('actual HarborCity project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    retarget=latest('GASTransitionRetargetReload')
    check('completed exact retarget batch',len(retarget['assets'])==11 and len(retarget['target_audits'])==10
          and re.fullmatch(HERO_ROOT+r'/Animation/GASTransitions/Batch_[0-9a-f]{12}',retarget['destination']) is not None)
    for row in retarget['assets']:protect(row['path'],row['sha256'])
    transitions=[load(retarget['destination']+'/Animations/M_Relaxed_'+name+'_Cut_InPlace_SelestiaTransition') for name in CLIPS]
    recovery=latest('GASRecoverySourceApply')
    check('saved five-source recovery set',len(recovery['assets'])==5)
    for row in recovery['assets']:protect(row['path'],row['sha256'])
    calibration=latest('GASLocomotionCalibration')
    scale=calibration['hero']['body_transform']['scale']
    check('actual original calibration uniform scale',len(scale)==3 and max(scale)==min(scale) and scale[0]>0,scale)
    source_bp,source_bs,source_skeleton=load(SOURCE_BP),load(SOURCE_BS),load(OLD+'/SK_Selestia')
    hero_class=U.load_class(None,HERO_ROOT+'/BP_M5VS2_Selestia.BP_M5VS2_Selestia_C')
    check('actual hero generated class',hero_class is not None)
    hero=U.get_default_object(hero_class);body=hero.get_component_by_class(U.SkeletalMeshComponent)
    source_mesh=body.get_editor_property('skeletal_mesh_asset')
    check('current hero raw skeleton baseline',source_mesh is not None and source_mesh.get_editor_property('skeleton')==source_skeleton)
    check('frozen original gameplay speeds',hero.get_editor_property('walk_speed')==400. and hero.get_editor_property('sprint_speed')==650.)
    check('actual hero uses source ABP',package(body.get_editor_property('anim_class'))==SOURCE_BP)
    R['hero_baseline']=dict(mesh=package(source_mesh),animation=SOURCE_BP,walk_speed=400,sprint_speed=650,
                           material_paths=[package(body.get_material(i)) if body.get_material(i) else None for i in range(body.get_num_materials())])
    for root in (OLD,HERO_ROOT):
        for file in disk(root+'/unused').parent.rglob('*.uasset'):PROTECTED[str(file)]=sha(file)
    for file in (PROJECT/'Config').glob('*.ini'):PROTECTED[str(file)]=sha(file)
    for file in (PROJECT/'Content').rglob('*.umap'):PROTECTED[str(file)]=sha(file)
    baseline=json.loads(U.HCM5VS2PhysicsEditor.read_locomotion_blend_space(source_bs))
    check('native current 28-sample baseline',baseline.get('status')=='READBACK_COMPLETE' and len(baseline['samples'])==28)
    R['original_blendspace']=baseline
    indices=[0,9,27];gaits=['Run','Sprint','Walk']
    originals=[load(baseline['samples'][i]['animation']) for i in indices]
    clean=[load('/Game/HarborCity/M5VS2/GASSourceP0/Animations/M_Relaxed_Run_Loop_F'),
           load(recovery['destination']+'/Animations/M_Relaxed_Sprint_Loop_F'),
           load('/Game/HarborCity/M5VS2/GASSourceP0/Animations/M_Relaxed_Walk_Loop_F')]
    p0=json.loads((DOC/'research/GAS_P0_SOURCE_MANIFEST.json').read_text('utf-8-sig'))
    check('actual original P0 source manifest',p0['status']=='PASS')
    for seq in clean[:1]+clean[2:]:
        row=next(row for row in p0['assets'] if package(row['path'])==package(seq));protect(seq,row['sha256'])
    for i,gait,original in zip(indices,gaits,originals):
        measured=next(row for row in calibration['calibration'] if row['gait']==gait)
        sample=baseline['samples'][i]
        check('real calibrated '+gait+' sample retained',package(original)==package(measured['path'])
              and abs(sample['position'][1]-measured['proposed_sample_speed_cm_s'])<1e-4
              and abs(sample['rate_scale']-measured['mean_forward_matching_playrate'])<1e-6
              and sample['position'][0]==0 and sample['position'][2]==0)
    if PHASE.endswith('Reload'):
        prior=latest('GASMotionCandidate');DEST=prior['destination'];R['destination']=DEST
        check('exact saved private candidate',re.fullmatch(ROOT+r'/Batch_[0-9a-f]{12}',DEST) is not None and len(prior['assets'])==7)
        for row in prior['assets']:protect(row['path'],row['sha256'])
        skeleton=load(DEST+'/SK_Selestia_Warp');mesh=load(DEST+'/SKM_Selestia_Warp')
        bp=load(DEST+'/ABP_Selestia_GASMotion');space=load(DEST+'/BS_Selestia_GASMotion')
        loops=[load(DEST+'/Loops/'+gait+'_WarpLoop') for gait in gaits]
        R['assets']=prior['assets'];R['candidate']=prior['candidate']
        R['native_inspection']=native('native candidate reload',U.HCM5VS2GASMotionEditor.inspect_motion_candidate(bp,mesh,source_bs,space))
        check('saved native graph and pins match author',R['native_inspection']==prior['native_inspection'])
    else:
        check('fresh unique candidate directory',not A.does_directory_exist(DEST) and not disk(DEST+'/placeholder').parent.exists())
        skeleton=duplicate(source_skeleton,'SK_Selestia_Warp');mesh=duplicate(source_mesh,'SKM_Selestia_Warp')
        R['warp_skeleton']=native('native virtual skeleton',U.HCM5VS2GASMotionEditor.prepare_warp_skeleton(source_skeleton,skeleton,mesh))
        space=duplicate(source_bs,'BS_Selestia_GASMotion');bp=duplicate(source_bp,'ABP_Selestia_GASMotion')
        loops=[];R['speed_curves']=[]
        for i,gait,original,source in zip(indices,gaits,originals,clean):
            loop=duplicate(original,'Loops/'+gait+'_WarpLoop');loops.append(loop)
            result=native('native measured '+gait+' speed curve',U.HCM5VS2GASMotionEditor.add_root_speed_curve(source,loop,baseline['samples'][i]['position'][1],scale[0]))
            result.update(gait=gait,source=package(source),target=package(loop),calibration=R['inputs']['GASLocomotionCalibration'])
            R['speed_curves'].append(result)
        R['graph_author']=native('native transitions and warping graph',U.HCM5VS2GASMotionEditor.configure_motion_graph(bp,mesh,source_bs,space,loops,transitions))
        R['native_inspection']=native('native candidate inspection',U.HCM5VS2GASMotionEditor.inspect_motion_candidate(bp,mesh,source_bs,space))
        R['candidate']=dict(mesh=package(mesh),skeleton=package(skeleton),blueprint=package(bp),blendspace=package(space),
                            live_hero_unchanged=True,material_overrides_from_hero=R['hero_baseline']['material_paths'])
    R['loop_preservation']=[]
    for original,loop in zip(originals,loops):
        before,after=sample_pose_hash(original,source_mesh),sample_pose_hash(loop,source_mesh)
        check('all raw loop bone keys unchanged '+loop.get_name(),before==after,after)
        check('old curve functions retained '+loop.get_name(),curve_hashes(original)==curve_hashes(loop,('VS2NominalRootSpeed',)))
        R['loop_preservation'].append(dict(source=package(original),candidate=package(loop),audit=after))
    if not PHASE.endswith('Reload'):
        for obj in [skeleton,mesh]+loops+[space,bp]:save(obj)
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['exception']=traceback.format_exc()
finally:
    changed=[path for path,value in PROTECTED.items() if not Path(path).is_file() or sha(path)!=value]
    R['source_preservation']=dict(files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('GAS motion author failed; preserve candidate and evidence')
