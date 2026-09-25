"""Evidence-bound 28-sample forward gait author; root alone runs Unreal.

Phases GASLocomotionForwardDryRun / GASLocomotionForwardApply. Rates and low
walk speed come only from the successful native target-foot calibration.
Native interpolation/compile/save are not runtime planted-foot acceptance.
"""
from pathlib import Path
import ast
import copy
import datetime as dt
import hashlib
import json
import math
import re
import shutil
import struct
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity';CONTENT=PROJECT/'Content';DOC=WORK/'docs/HarborCity_M5_VS2'
ROOT='/Game/HarborCity/M5VS2/HeroSelestia';ANIM=ROOT+'/Animation';GAS=ANIM+'/GAS'
SOURCE_ROOT='/Game/HarborCity/M5VS1/HeroSelestia'
BLUEPRINT=ANIM+'/ABP_M5VS2_Selestia_Physics';HERO=ROOT+'/BP_M5VS2_Selestia'
SOURCE_BS=SOURCE_ROOT+'/Animation/BS_Idle_Walk_Run_Selestia'
TARGET_BS=GAS+'/BS_M5VS2_GAS_IdleWalkRun';SKELETON=SOURCE_ROOT+'/SK_Selestia'
OLD_IDLE=SOURCE_ROOT+'/Animation/MM_Idle_Selestia'
KEY='HarborCityOwnedBy';OWNER='HarborCity_M5_VS2_GASForwardLocomotion_v1'
A=U.EditorAssetLibrary

def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    if len(rows)!=1:raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
assert PHASE in ('GASLocomotionForwardDryRun','GASLocomotionForwardApply')
OUT.mkdir(parents=True,exist_ok=True);APPLY=PHASE.endswith('Apply')
R=dict(status='RUNNING',phase=PHASE,checks=[],inputs={},assets=[],transitions=[],applied=False,
       runtime_foot_contact='NOT_RUN',visual='NOT_RUN',user_naturalness='USER_REVIEW',
       map_writes=0,animation_sequence_writes=0,started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={}

def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')

def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed));dump()
    if not ok:raise RuntimeError(name+': '+repr(observed))

def latest(phase):
    for p in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        d=json.loads(p.read_text('utf-8-sig'))
        if d.get('status')=='PASS':return p,d
    raise RuntimeError('No successful native '+phase)

def f32(value):return struct.unpack('<f',struct.pack('<f',float(value)))[0]
def xyz(v):return [float(v.x),float(v.y),float(v.z)]

def hero_readback():
    cls=U.load_class(None,HERO+'.BP_M5VS2_Selestia_C');check('actual hero class',cls is not None)
    hero=U.get_default_object(cls);body=hero.get_component_by_class(U.SkeletalMeshComponent)
    movement=hero.get_component_by_class(U.CharacterMovementComponent);t=body.get_relative_transform()
    p=t.translation;q=t.rotation;s=t.scale3d
    return dict(walk_speed_cm_s=float(hero.get_editor_property('walk_speed')),
        sprint_speed_cm_s=float(hero.get_editor_property('sprint_speed')),
        body_transform=dict(position_cm=xyz(p),quaternion_xyzw=[float(q.x),float(q.y),float(q.z),float(q.w)],scale=xyz(s)),
        orient_rotation_to_movement=bool(movement.get_editor_property('orient_rotation_to_movement')),
        actor_scale=xyz(hero.get_actor_scale3d()),animation=path_of(body.get_editor_property('anim_class')))

def exact_difference(expected,actual,path='',rows=None):
    rows=[] if rows is None else rows
    if len(rows)>=20:return rows
    if type(expected)!=type(actual):
        # JSON integral values and floating-point 0/1 are semantically identical.
        if isinstance(expected,(float,int)) and isinstance(actual,(float,int)) and expected==actual:return rows
        rows.append(dict(path=path,expected=expected,actual=actual));return rows
    if isinstance(expected,dict):
        for key in sorted(set(expected)|set(actual)):
            if key not in expected or key not in actual:rows.append(dict(path=path+'/'+key,expected=expected.get(key),actual=actual.get(key)))
            else:exact_difference(expected[key],actual[key],path+'/'+key,rows)
            if len(rows)>=20:break
    elif isinstance(expected,list):
        if len(expected)!=len(actual):rows.append(dict(path=path+'/length',expected=len(expected),actual=len(actual)))
        for i,(a,b) in enumerate(zip(expected,actual)):
            exact_difference(a,b,path+'/'+str(i),rows)
            if len(rows)>=20:break
    elif expected!=actual:rows.append(dict(path=path,expected=expected,actual=actual))
    return rows

def equal(name,expected,actual):
    diff=exact_difference(expected,actual)
    check(name,not diff,diff)

def sample_changed(row,animation,position=None,rate=None,index=None):
    result=replace_serialized(copy.deepcopy(row),package(row['animation']),animation)
    if index is not None:result['index']=index
    if position is not None:
        result['position']=list(position)
        result['reflected_sample']['SampleValue']=['(X=%.6f,Y=%.6f,Z=%.6f)'%tuple(position)]
    if rate is not None:
        result['rate_scale']=rate
        result['reflected_sample']['RateScale']=['%.6f'%rate]
    return result

def expected_blendspace(before,plan):
    result=replace_serialized(copy.deepcopy(before),SOURCE_BS,TARGET_BS)
    for i in plan['replace_all_zero_speed_idle_indices']:
        result['samples'][i]=sample_changed(result['samples'][i],plan['idle_sequence'])
    walk,run,sprint=plan['forward_samples']
    for i,row in ((plan['forward_run_index'],run),(plan['forward_sprint_index'],sprint)):
        result['samples'][i]=sample_changed(result['samples'][i],row['animation'],row['position'],row['rate_scale'])
    result['samples'].append(sample_changed(before['samples'][plan['forward_run_index']],walk['animation'],walk['position'],walk['rate_scale'],27))
    result['axes'][1]['Max']=['650.000000'];result['axes'][1]['bSnapToGrid']=['']
    result['authored_blendspace_properties']['BlendParameters'][1]='(DisplayName="Speed",Max=650.000000,GridNum=4)'
    return result

def check_calibration(cal):
    check('actual native calibration scope',cal.get('phase')=='GASLocomotionCalibration' and not cal.get('candidate_applied')
        and cal.get('preservation',{}).get('changed_files')==[] and all(c['status']=='PASS' for c in cal['checks']))
    for name,row in cal['inputs'].items():
        check('calibration input unchanged '+name,Path(row['path']).is_file() and sha(row['path'])==row['sha256'])
    p=copy.deepcopy(cal['proposed_binding_only'])
    check('precise approved forward topology',p['expected_new_sample_count']==28 and p['speed_axis_maximum']==650
        and p['speed_axis_snap_to_grid'] is False and len(p['forward_samples'])==3
        and len(p['preserve_other_direction_sample_indices'])==16 and len(p['replace_all_zero_speed_idle_indices'])==9)
    metrics=cal['calibration'];check('exact measured gait ordering',[x['gait'] for x in metrics]==['Walk','Run','Sprint'])
    for i,(row,metric) in enumerate(zip(p['forward_samples'],metrics)):
        check('actual positive native foot calibration '+metric['gait'],metric['path']==row['animation']
            and metric['sequence_rate_scale']==1 and metric['contact_threshold']==.8 and metric['stance_interval_count']>=10
            and math.isfinite(metric['measured_toe_backward_cm_s_rate1']) and metric['measured_toe_backward_cm_s_rate1']>30)
        equal('calibration formula '+metric['gait'],metric['mean_forward_matching_playrate'],
            metric['proposed_sample_speed_cm_s']/metric['measured_toe_backward_cm_s_rate1'])
        check('sample uses actual measured result '+metric['gait'],row['rate_scale']==metric['mean_forward_matching_playrate']
            and row['position']==[0,metric['proposed_sample_speed_cm_s'],0])
        row['position']=[0.,f32(row['position'][1]),0.];row['rate_scale']=f32(row['rate_scale'])
    check('gameplay speed and measured low-speed Walk remain separate',p['forward_samples'][0]['rate_scale']==1
        and 20<p['forward_samples'][0]['position'][1]<350 and p['forward_samples'][1]['position'][1]==400
        and p['forward_samples'][2]['position'][1]==650)
    before=cal['native_readback']['blendspace']['samples']
    p['forward_run_index']=next(x['index'] for x in before if x['position']==[0,300,0])
    p['forward_sprint_index']=next(x['index'] for x in before if x['position']==[0,600,0])
    p['parameters_precision']='Native float32 inputs; source measurement retained in calibration report.'
    return p

def apply(blueprint,source,clips):
    dry_path,dry_report=latest('GASLocomotionForwardDryRun')
    for field in ('inputs','native_readback','hero','plan'):
        equal('unchanged successful DryRun '+field,dry_report[field],R[field])
    R['dry_run']=input_file(dry_path)
    backup=Path('E:/GameDev/Assets/HarborCity/M5_VS2/HeroSelestia/AnimationBackups')/OUT.name/'ABP_M5VS2_Selestia_Physics.uasset'
    check('fresh private ABP backup',not backup.parent.exists());backup.parent.mkdir(parents=True,exist_ok=False)
    shutil.copy2(disk(BLUEPRINT),backup);check('original ABP exact byte backup',sha(backup)==R['inputs']['blueprint']['sha256'])
    R['backup']=input_file(backup);dump()
    target=A.duplicate_asset(SOURCE_BS,TARGET_BS);check('native independent BlendSpace copy',target is not None)
    A.set_metadata_tag(target,KEY,OWNER)
    A.set_metadata_tag(target,'GASLocomotionCalibrationSHA256',R['inputs']['calibration']['sha256'])
    duplicate=blendspace_readback(target)
    equal('complete native copy before any gait edits',replace_serialized(R['native_readback']['blendspace'],SOURCE_BS,TARGET_BS),duplicate)
    p=R['plan'];walk,run,sprint=p['forward_samples']
    R['native_apply']=json.loads(U.HCM5VS2GASLocomotionEditor.apply_forward_locomotion(blueprint,source,target,
        clips[0],clips[1],clips[2],clips[3],walk['position'][1],run['rate_scale'],sprint['rate_scale']))
    check('bounded native 28-sample change and two graph assignments compile',R['native_apply'].get('status')=='APPLIED_COMPILED_NOT_SAVED',R['native_apply'])
    R['after_readback']=dict(blendspace=blendspace_readback(target),blueprint=blueprint_readback(blueprint)[0]);dump()
    expected=expected_blendspace(R['native_readback']['blendspace'],p)
    equal('exact 28 samples and only approved speed axis changes',expected,R['after_readback']['blendspace'])
    graph=replace_serialized(R['native_readback']['blueprint'],SOURCE_BS,TARGET_BS)
    graph=replace_serialized(graph,OLD_IDLE,p['idle_sequence'])
    equal('two sequence references only; all nodes links slots physics remain',graph,R['after_readback']['blueprint'])
    equal('actual hero settings remain',R['hero'],hero_readback())
    equal('old source BlendSpace remains',R['native_readback']['blendspace'],blendspace_readback(source))
    check('save isolated new BlendSpace',A.save_loaded_asset(target,False))
    check('save only backed-up VS2 AnimBP',A.save_loaded_asset(blueprint,False))
    for obj in (target,blueprint):
        path=package(obj.get_path_name());check('real saved package exists '+path,disk(path).is_file())
        R['assets'].append(dict(path=path,class_name=obj.get_class().get_name(),sha256=sha(disk(path)),bytes=disk(path).stat().st_size))
    R['transitions'].append(dict(path=BLUEPRINT,before_sha256=R['inputs']['blueprint']['sha256'],
        after_sha256=sha(disk(BLUEPRINT)),backup=R['backup'],changed_scope='One BlendSpace reference plus one independent Idle sequence; no other graph edits.'))
    PROTECTED.pop(str(disk(BLUEPRINT)))
    equal('after-save target readback',expected,blendspace_readback(target))
    equal('after-save graph readback',graph,blueprint_readback(blueprint)[0])
    R['applied']=True
    R['fresh_process_reload']='NOT_RUN_REQUIRED: load saved BS + ABP and verify 28 samples before runtime gait review.'

try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or preexisting dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False)
        and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    # Reuse only the previously native-tested read-only functions, never execute
    # the obsolete three-sample author module or its mutation function.
    helper=WORK/'tools/ue_m5_vs2_gas_locomotion_author.py'
    names={'package','disk','sha','input_file','path_of','load','protect','dirty','blueprint_readback','blendspace_readback','swap_text','replace_serialized'}
    nodes=[n for n in ast.parse(helper.read_text('utf-8')).body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact shared read-only functions',{n.name for n in nodes}==names)
    exec(compile(ast.fix_missing_locations(ast.Module(body=nodes,type_ignores=[])),str(helper),'exec'),globals())
    R['inputs']['readback_script']=input_file(helper)
    R['inputs']['script']=input_file(WORK/'tools/ue_m5_vs2_gas_locomotion_forward_author.py')
    for name in ('HCM5VS2GASLocomotionEditor','HCM5VS2PhysicsEditor'):
        for suffix in ('.h','.cpp'):R['inputs'][name+suffix]=input_file(PROJECT/('Source/HarborCity/M5VS2/'+name+suffix))
    check('new native apply helper compiled',hasattr(U,'HCM5VS2GASLocomotionEditor')
        and hasattr(U.HCM5VS2GASLocomotionEditor,'apply_forward_locomotion'))
    cal_path,cal=latest('GASLocomotionCalibration');R['inputs']['calibration']=input_file(cal_path)
    R['plan']=check_calibration(cal)
    # Original target frame files are hashed evidence; no skeletal payload is copied.
    for audit in cal['target_audits']:
        row=audit['trajectory'];check('original-frame target calibration file intact',sha(row['path'])==row['sha256'])
    for label in ('SOURCE','INPLACE','RETARGET'):
        file=DOC/('research/GAS_P0_'+label+'_MANIFEST.json');data=json.loads(file.read_text('utf-8-sig'))
        check('native P0 '+label+' PASS',data.get('status')=='PASS')
        for row in data['assets']:protect(disk(row['path']),row['sha256'])
    sprint=json.loads(Path(cal['inputs']['sprint']['path']).read_text('utf-8-sig'))
    for row in sprint['assets']:protect(disk(row['path']),row['sha256'])
    for root in (SOURCE_ROOT,ROOT):
        for file in disk(root+'/unused').parent.rglob('*.uasset'):protect(file)
    for file in (PROJECT/'Config').glob('*.ini'):protect(file)
    for file in CONTENT.rglob('*.umap'):protect(file)
    R['inputs']['protected_files']=dict(PROTECTED)
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT,SOURCE_ROOT],force_rescan=True)
    check('candidate new path absent on disk and registry',not disk(TARGET_BS).exists() and not A.does_asset_exist(TARGET_BS))
    blueprint=load(BLUEPRINT);source=load(SOURCE_BS);skeleton=load(SKELETON)
    R['inputs']['blueprint']=dict(path=BLUEPRINT,sha256=sha(disk(BLUEPRINT)))
    R['inputs']['source_blendspace']=dict(path=SOURCE_BS,sha256=sha(disk(SOURCE_BS)))
    check('existing exact owned VS2 graph',A.get_metadata_tag(blueprint,KEY)=='HarborCity_M5_VS2_SelestiaPhysics_v1')
    paths=[R['plan']['idle_sequence']]+[x['animation'] for x in R['plan']['forward_samples']]
    clips=[load(path) for path in paths]
    for clip in clips:
        check('actual measured sequence flags and rate',isinstance(clip,U.AnimSequence) and clip.get_editor_property('skeleton')==skeleton
            and not clip.get_editor_property('enable_root_motion') and not clip.get_editor_property('force_root_lock')
            and float(U.AnimationLibrary.get_rate_scale(clip))==1,clip.get_path_name())
    R['native_readback']=dict(blueprint=blueprint_readback(blueprint)[0],blendspace=blendspace_readback(source))
    R['hero']=hero_readback()
    equal('actual entire graph and source space match successful calibration',cal['native_readback'],R['native_readback'])
    equal('actual movement scale and orientation match calibration',cal['hero'],R['hero'])
    check('expected 400/650 movement unchanged',R['hero']['walk_speed_cm_s']==400 and R['hero']['sprint_speed_cm_s']==650)
    check('preflight does not dirty packages',not dirty())
    if APPLY:apply(blueprint,source,clips)
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(files_checked=len(PROTECTED),unexpected_changed_files=changed)
    check('old assets all clips skeletons HeroBP maps config unchanged',not changed,changed)
    check('no dirty map packages',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['pass_scope']='Native configuration, compile and saved bytes only; actual stance contact, start/stop, directional blends and naturalness require runtime review.' if APPLY else 'Read-only matching of measured target gait, current graph/CDO and exact guarded 28-sample plan; no asset writes.'
    R['limitations']=['Mean forward toe velocity calibration retains per-frame toe residual; does not claim zero sliding.',
        'Sixteen original side/back moving samples remain unchanged. Direction-dependent transitions need actual review.',
        'GAS Walk at measured low speed is used during acceleration. Gameplay 400 uses GAS Run and 650 uses GAS Sprint.',
        'Start/stop/jump and two idle-break P0 clips are not newly bound by this author.',
        'Sync/length caches are rebuilt by native validation; preserved marker-sync flags do not prove mixed-clip phase alignment.']
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Forward gait author failed; preserve evidence and any unsaved/new candidate.')
if APPLY:
    manifest=dict(schema='HarborCity.M5VS2.GASForwardLocomotion.v1',status='PASS',phase=PHASE,
        evidence=input_file(OUT/'author_result.json'),calibration=R['inputs']['calibration'],dry_run=R['dry_run'],
        transitions=R['transitions'],assets=R['assets'],plan=R['plan'])
    (OUT/'locomotion_binding_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
