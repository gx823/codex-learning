"""Read-only native target-foot calibration. Root runs GASLocomotionCalibration.

No object setters, asset creation, saves, gameplay execution or configuration edits.
Produces a proposed forward-only BlendSpace change list, never applies it.
"""
from pathlib import Path
import ast
import copy
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity'
DOC=WORK/'docs/HarborCity_M5_VS2'
ROOT='/Game/HarborCity/M5VS2/HeroSelestia'
GAS=ROOT+'/Animation/GAS'
HERO=ROOT+'/BP_M5VS2_Selestia'
BP=ROOT+'/Animation/ABP_M5VS2_Selestia_Physics'
OLD='/Game/HarborCity/M5VS1/HeroSelestia'
BS=OLD+'/Animation/BS_Idle_Walk_Run_Selestia'
A,L,P=U.EditorAssetLibrary,U.AnimationLibrary,U.AnimPoseExtensions
IS_SPRINT=True  # Reused read-only pose audit branch, not a movement-mode switch.

def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    if len(rows)!=1: raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
assert arg('M5AuthorPhase')=='GASLocomotionCalibration'
OUT.mkdir(parents=True,exist_ok=True)
R=dict(status='RUNNING',phase='GASLocomotionCalibration',checks=[],inputs={},target_audits=[],calibration=[],
    asset_writes=0,config_writes=0,runtime_foot_contact='NOT_RUN',visual='NOT_RUN',candidate_applied=False,
    started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),limitations=[
    'Native single-clip raw target poses with authored contact curves; not runtime BlendSpace output or skin sole contact.',
    'Contact threshold 0.8 and actor-forward straight motion define the calibration window; no turning or contact correction is simulated.',
    'A rate can reduce mean forward mismatch but cannot guarantee zero sliding, natural transitions or planted feet.',
    'Actual actor walk/sprint settings stay 400/650. All non-forward directional samples, combat and FP input remain unchanged by this read-only tool.'])
PROTECTED={}

def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')

def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed));dump()
    if not ok: raise RuntimeError(name+': '+repr(observed))

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()

def package(obj):
    return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]

def disk(obj):
    path=package(obj)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')

def protect(path,digest=None):
    path=disk(path);actual=sha(path)
    check('protected actual package '+str(path),digest is None or actual==digest)
    PROTECTED[str(path)]=actual

def load(path):
    obj=A.load_asset(path);check('load '+path,obj is not None);return obj

def latest(phase):
    for p in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda x:x.stat().st_mtime,reverse=True):
        d=json.loads(p.read_text('utf-8-sig'))
        if d.get('status')=='PASS':return p,d
    raise RuntimeError('No successful native '+phase)

def dirty():
    return sorted(p.get_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages())

def direction_contract(graph):
    event=next(g for g in graph['graphs'] if g['path'].endswith(':EventGraph'))
    nodes={n['path'].rsplit('.',1)[1]:n for n in event['nodes']}
    def pin(node,name):return next(p for p in nodes[node]['pins'] if p['name']==name)
    def edge(node,name,ends):return len(pin(node,name)['links'])==1 and pin(node,name)['links'][0].endswith(ends)
    check('actual Direction selects movement-relative value',
        edge('K2Node_VariableSet_9','Direction','.K2Node_Select_0:ReturnValue') and
        edge('K2Node_Select_0','Index','.K2Node_VariableGet_8:bOrientRotationToMovement') and
        edge('K2Node_Select_0','Option 0','.K2Node_CallFunction_8:ReturnValue') and
        edge('K2Node_Select_0','Option 1','.K2Node_CallFunction_12:ReturnValue') and
        edge('K2Node_CallFunction_8','Velocity','.K2Node_VariableGet_12:Velocity') and
        edge('K2Node_CallFunction_8','BaseRotation','.K2Node_CallFunction_10:ReturnValue') and
        edge('K2Node_CallFunction_10','self','.K2Node_VariableGet_13:Character') and
        edge('K2Node_CallFunction_12','Value','.K2Node_CallFunction_8:ReturnValue') and
        float(pin('K2Node_CallFunction_12','Min')['default_value'])==-45 and
        float(pin('K2Node_CallFunction_12','Max')['default_value'])==45)
    return dict(native_edges_verified=True,source='Velocity relative to Character rotation; ordinary orient-to-movement branch clamps [-45,45], disabled branch retains full angle',
        reachability='Static actual C++ plus native graph: ordinary turning can use nonzero [-45,45]; FP, aiming and attack FaceBodyYawOnce disable orient and permit side/back directions. Runtime directional coverage NOT_RUN.',
        evidence_nodes=[nodes[n] for n in ('K2Node_CallFunction_8','K2Node_CallFunction_10','K2Node_CallFunction_12','K2Node_Select_0','K2Node_VariableSet_9')])

def contact_metrics(audit,requested):
    data=json.loads(Path(audit['trajectory']['path']).read_text('utf-8'))
    frames=data['frames'];scale=data['component_relative_transform']['scale'][0]
    q=data['component_relative_transform']['quaternion_xyzw'];samples=[]
    for side in ('l','r'):
        for a,b in zip(frames,frames[1:]):
            ca,cb=a['curves']['contact_'+side],b['curves']['contact_'+side]
            if ca is None or cb is None or min(ca,cb)<.8:continue
            seconds=b['time_seconds']-a['time_seconds']
            v=qrotate(q,mul(sub(b['bones']['ball_'+side]['position_cm'],a['bones']['ball_'+side]['position_cm']),scale/seconds))
            samples.append(dict(side=side,frame0=a['frame'],dt=seconds,velocity_cm_s=v))
    check('actual bilateral authored toe stance windows',all(sum(x['side']==s for x in samples)>=2 for s in ('l','r')))
    duration=sum(x['dt'] for x in samples)
    mean_back=-sum(x['dt']*x['velocity_cm_s'][0] for x in samples)/duration
    check('target actor-forward toe motion is backward in stance',mean_back>30,mean_back)
    speed=mean_back if requested is None else requested
    rate=speed/mean_back
    def residual(playrate):
        values=[(x['dt'],math.hypot(speed+playrate*x['velocity_cm_s'][0],playrate*x['velocity_cm_s'][1])) for x in samples]
        return dict(mean_cm_s=sum(w*v for w,v in values)/duration,rms_cm_s=math.sqrt(sum(w*v*v for w,v in values)/duration),max_cm_s=max(v for w,v in values))
    denominator=sum(x['dt']*(x['velocity_cm_s'][0]**2+x['velocity_cm_s'][1]**2) for x in samples)
    ls_rate=-speed*sum(x['dt']*x['velocity_cm_s'][0] for x in samples)/denominator
    return dict(path=audit['path'],bilateral_stance_duration_seconds=duration,stance_interval_count=len(samples),
        contact_threshold=.8,measured_toe_backward_cm_s_rate1=mean_back,proposed_sample_speed_cm_s=speed,
        mean_forward_matching_playrate=rate,horizontal_least_squares_rate_comparison=ls_rate,
        predicted_horizontal_world_toe_residual_cm_s=dict(rate1=residual(1),mean_forward_matching=residual(rate),least_squares=residual(ls_rate)),
        measurement='Single clip native raw, final Selestia bone axes, actual CDO uniform scale and rotation; NOT runtime foot planting',samples=samples)

try:
    check('exact current project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    before_dirty=dirty();check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    # Reuse only known function definitions from the already native-PASS target
    # audit, not its module-level authoring code or try/finally mutation block.
    helper=WORK/'tools/ue_m5_vs2_gas_recovery_retarget_author.py'
    required={'options','add','sub','mul','dot','length','normalized','cross','qrotate','qinverse','qmul','vec','transform','heading','anatomical_axes','pose_hint','audit_target'}
    tree=ast.parse(helper.read_text('utf-8'));defs=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in required]
    check('exact pure read-only evaluation functions',set(n.name for n in defs)==required)
    exec(compile(ast.fix_missing_locations(ast.Module(body=defs,type_ignores=[])),str(helper),'exec'),globals())
    R['inputs']['evaluation_helper']=dict(path=str(helper),sha256=sha(helper))
    for label in ('SOURCE','INPLACE','RETARGET'):
        p=DOC/('research/GAS_P0_'+label+'_MANIFEST.json');data=json.loads(p.read_text('utf-8-sig'))
        check('actual P0 manifest '+label,data['status']=='PASS')
        R['inputs'][label]=dict(path=str(p),sha256=sha(p))
        for row in data['assets']:protect(row['path'],row['sha256'])
    sp,sprint=latest('GASSprintRetarget');R['inputs']['sprint']=dict(path=str(sp),sha256=sha(sp))
    check('one actual target Sprint',len(sprint['target_audits'])==1)
    for row in sprint['assets']:protect(row['path'],row['sha256'])
    for path in (HERO,BP,BS,OLD+'/SKM_Selestia',OLD+'/SK_Selestia'):protect(path)
    for file in (PROJECT/'Config').glob('*.ini'):PROTECTED[str(file)]=sha(file)
    for suffix in ('HarborCityCharacter.cpp','M1/HCM1Character.cpp','M4/HCM4CombatComponent.cpp'):
        file=PROJECT/'Source/HarborCity'/suffix;R['inputs'][suffix]=dict(path=str(file),sha256=sha(file))
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/HarborCity/M5VS2',OLD],force_rescan=True)
    cls=U.load_class(None,HERO+'.BP_M5VS2_Selestia_C');check('actual hero class',cls is not None)
    hero=U.get_default_object(cls);body=hero.get_component_by_class(U.SkeletalMeshComponent);movement=hero.get_component_by_class(U.CharacterMovementComponent)
    mesh=load(OLD+'/SKM_Selestia');skeleton=load(OLD+'/SK_Selestia')
    check('actual body target',body is not None and body.get_editor_property('skeletal_mesh_asset')==mesh)
    walk=float(hero.get_editor_property('walk_speed'));run=float(hero.get_editor_property('sprint_speed'))
    check('retain current actual gameplay speeds',walk==400 and run==650,[walk,run])
    R['hero']=dict(walk_speed_cm_s=walk,sprint_speed_cm_s=run,body_transform=transform(body.get_relative_transform()),
        orient_rotation_to_movement=bool(movement.get_editor_property('orient_rotation_to_movement')),
        actor_scale=vec(hero.get_actor_scale3d()),animation=str(body.get_editor_property('anim_class').get_path_name()))
    check('CDO actor scale does not add another unit conversion',max(abs(v-1) for v in R['hero']['actor_scale'])<1e-6)
    graph=json.loads(U.HCM5VS2PhysicsEditor.read_locomotion_graph(load(BP)))
    blend=json.loads(U.HCM5VS2PhysicsEditor.read_locomotion_blend_space(load(BS)))
    check('native actual graph and sample readback',graph['status']=='READBACK_COMPLETE' and blend['status']=='READBACK_COMPLETE')
    R['native_readback']=dict(blueprint=graph,blendspace=blend)
    R['direction_contract']=direction_contract(graph)
    samples=blend['samples'];dirs={-180.,-135.,-90.,-45.,0.,45.,90.,135.,180.}
    check('exact original 27 coordinate topology',len(samples)==27 and {(float(s['position'][0]),float(s['position'][1]),float(s['position'][2])) for s in samples}=={(d,v,0.) for d in dirs for v in (0.,300.,600.)})
    check('original sample rate scale',all(abs(s['rate_scale']-1)<1e-6 for s in samples))
    old_idle=OLD+'/Animation/MM_Idle_Selestia'
    idle=[s for s in samples if float(s['position'][1])==0]
    check('nine identical original Idle samples',len(idle)==9 and all(package(s['animation'])==old_idle for s in idle))
    idle_players=[p for p in graph['sequence_players'] if package(p['sequence'])==old_idle]
    check('one actual independent Idle state',len(idle_players)==1 and idle_players[0]['graph'].endswith('.Idle'))
    forward=[s for s in samples if float(s['position'][0])==0 and float(s['position'][1])>0]
    check('two exact forward source clips',[(float(s['position'][1]),package(s['animation'])) for s in sorted(forward,key=lambda x:x['position'][1])]==[(300.,OLD+'/Animation/MF_Unarmed_Walk_Fwd_Selestia'),(600.,OLD+'/Animation/MF_Unarmed_Jog_Fwd_Selestia')])
    bones=dict(root='Hips',hips='Hips',chest='Chest',head='Head',thigh_l='UpperLeg_L',thigh_r='UpperLeg_R',foot_l='Foot_L',foot_r='Foot_R',ball_l='Toe_L',ball_r='Toe_R')
    candidates=[('Walk',GAS+'/Retargeted/M_Relaxed_Walk_Loop_F_InPlace_SelestiaGAS',GAS+'/InPlace/M_Relaxed_Walk_Loop_F_InPlace',None),
        ('Run',GAS+'/Retargeted/M_Relaxed_Run_Loop_F_InPlace_SelestiaGAS',GAS+'/InPlace/M_Relaxed_Run_Loop_F_InPlace',walk),
        ('Sprint',sprint['target_audits'][0]['path'],sprint['target_audits'][0]['source'],run)]
    for label,target,source,speed in candidates:
        protect(source);seq=load(target);src=load(source)
        check('target skeleton and no locomotion root extraction',seq.get_editor_property('skeleton')==skeleton and not seq.get_editor_property('enable_root_motion') and not seq.get_editor_property('force_root_lock'))
        clip_rate=float(L.get_rate_scale(seq));source_rate=float(L.get_rate_scale(src))
        check('actual clip playback adds no unmeasured extra multiplier',clip_rate==source_rate==1.,dict(target=target,target_rate_scale=clip_rate,source_rate_scale=source_rate))
        audit=audit_target(seq,src,mesh,bones,body);R['target_audits'].append(audit)
        metric=contact_metrics(audit,speed);metric['gait']=label;metric['sequence_rate_scale']=clip_rate;R['calibration'].append(metric);dump()
    low,mid,high=R['calibration']
    check('measured gait order supports minimal three-speed forward plan',20<low['proposed_sample_speed_cm_s']<walk<run and .5<mid['mean_forward_matching_playrate']<2 and .5<high['mean_forward_matching_playrate']<2)
    new_idle=GAS+'/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS'
    R['proposed_binding_only']=dict(applied=False,expected_new_sample_count=28,
        preserve_other_direction_sample_indices=[s['index'] for s in samples if s['position'][0]!=0 and s['position'][1]>0],
        replace_all_zero_speed_idle_indices=[s['index'] for s in idle],idle_sequence=new_idle,independent_idle_node=idle_players[0]['path'],
        forward_samples=[dict(animation=m['path'],position=[0,m['proposed_sample_speed_cm_s'],0],rate_scale=m['mean_forward_matching_playrate']) for m in R['calibration']],
        speed_axis_maximum=run,speed_axis_snap_to_grid=False,other_graph_nodes_unchanged=True,
        pending='Requires separately guarded new BlendSpace/native apply helper, then actual steady400/650 forward, accelerating, stopping, +/-45 transitions and FP side/back foot-contact + visual tests. Do not apply the old three-sample helper.')
    check('read-only dirty package state unchanged',dirty()==before_dirty,dict(before=before_dirty,after=dirty()))
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc()
finally:
    changed=[f for f,h in PROTECTED.items() if not Path(f).is_file() or sha(f)!=h]
    R['preservation']=dict(files_checked=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Read-only locomotion calibration failed; no assets written')
