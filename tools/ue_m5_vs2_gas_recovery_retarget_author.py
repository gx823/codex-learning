"""New Q/R getup actions or Selestia Sprint; no automatic gameplay binding.

Phases GASRecoveryRetargetQ, GASRecoveryRetargetR, GASSprintRetarget.
Reuses actual saved clean source and existing source/target IK rigs. Each run
creates a new retargeter and exact output clips in a unique private namespace.
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
PROJECT, DOC = WORK/'HarborCity', WORK/'docs/HarborCity_M5_VS2'
P0 = '/Game/HarborCity/M5VS2/GASSourceP0'
GAS = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS'
OWNER = 'HarborCity_M5_VS2_GAS_RecoveryRetarget_v1'
KEY = 'HarborCityOwnedBy'
A, L, P = U.EditorAssetLibrary, U.AnimationLibrary, U.AnimPoseExtensions
AT = U.AssetToolsHelpers.get_asset_tools()


def arg(name):
    values = re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(values) != 1:
        raise RuntimeError('Exactly one '+name+' required')
    return values[0][0] or values[0][1]


OUT, PHASE = Path(arg('M5EvidenceDir')).resolve(), arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
assert PHASE in ('GASRecoveryRetargetQ','GASRecoveryRetargetR','GASSprintRetarget')
OUT.mkdir(parents=True,exist_ok=True)
IS_SPRINT = PHASE == 'GASSprintRetarget'
LETTER = PHASE[-1] if not IS_SPRINT else None
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
DEST = (GAS+'/Sprint' if IS_SPRINT else '/Game/HarborCity/M5VS2/NPC/AvatarSample_'+LETTER+'/GetUp')+'/Batch_'+TOKEN
SUFFIX = '_SelestiaGAS' if IS_SPRINT else '_NPC'+LETTER+'GetUp'
R = dict(status='RUNNING',phase=PHASE,destination=DEST,checks=[],assets=[],inputs={},target_audits=[],
         runtime='NOT_RUN',visual='NOT_RUN',binding='NOT_RUN',map_writes=0,skeleton_writes=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')


def check(label,good,observed=None):
    R['checks'].append(dict(name=label,status='PASS' if good else 'FAIL',observed=observed)); dump()
    if not good:
        raise RuntimeError(label+': '+repr(observed))


def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for data in iter(lambda:f.read(1024*1024),b''): h.update(data)
    return h.hexdigest()


def package(obj):
    return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]


def disk(path):
    path=package(path)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')


def protect(path,expected=None):
    file=disk(path); actual=sha(file)
    check('source/target dependency SHA '+package(path),expected is None or actual==expected)
    PROTECTED[str(file)]=actual


def latest(phase):
    for file in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        value=json.loads(file.read_text('utf-8-sig'))
        if value.get('status')=='PASS': return file,value
    raise RuntimeError('No native successful '+phase)


def load(path):
    obj=A.load_asset(path); check('load '+package(path),obj is not None); return obj


def save(obj):
    path=package(obj); check('save only this new batch',path.startswith(DEST+'/'))
    A.set_metadata_tag(obj,KEY,OWNER)
    check('native save '+path,A.save_loaded_asset(obj,False) and disk(path).is_file())
    R['assets']=[x for x in R['assets'] if x['path']!=path]+[dict(path=path,sha256=sha(disk(path)),bytes=disk(path).stat().st_size,class_name=obj.get_class().get_name())]
    dump()


def options(mesh,extract=False):
    result=U.AnimPoseEvaluationOptions()
    for key,value in dict(evaluation_type=U.AnimDataEvalType.RAW,should_retarget=True,optional_skeletal_mesh=mesh,
                          extract_root_motion=extract,incorporate_root_motion_into_pose=not extract,
                          retrieve_additive_as_full_pose=True,evaluate_curves=True).items(): result.set_editor_property(key,value)
    return result


# Pure vector/quaternion helpers are copied below from the successful read-only
# original-frame probe; no source probe module is imported or executed.
def add(a, b):
    return [a[i] + b[i] for i in range(3)]

def sub(a, b):
    return [a[i] - b[i] for i in range(3)]

def mul(a, amount):
    return [x * amount for x in a]

def dot(a, b):
    return sum(x * y for x, y in zip(a, b))

def length(a):
    return math.sqrt(dot(a, a))

def normalized(a):
    size = length(a)
    if size < 1e-7 or not math.isfinite(size):
        raise RuntimeError('Degenerate anatomical axis')
    return mul(a, 1. / size)

def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def qrotate(q, value):
    twice_cross = mul(cross(q[:3], value), 2.)
    return add(value, add(mul(twice_cross, q[3]), cross(q[:3], twice_cross)))

def qinverse(q):
    return [-q[0], -q[1], -q[2], q[3]]

def qmul(a, b):
    xyz=add(add(mul(b[:3],a[3]),mul(a[:3],b[3])),cross(a[:3],b[:3]))
    return xyz+[a[3]*b[3]-dot(a[:3],b[:3])]

def vec(value):
    return [float(value.x), float(value.y), float(value.z)]

def transform(value):
    q = value.rotation
    result = dict(position_cm=vec(value.translation), quaternion_xyzw=[float(q.x), float(q.y), float(q.z), float(q.w)],
                  scale=vec(value.scale3d))
    flat = [v for values in result.values() for v in values]
    if not all(math.isfinite(v) for v in flat) or abs(sum(v*v for v in result['quaternion_xyzw']) - 1.) > 1e-3:
        raise RuntimeError('Non-finite transform or non-unit quaternion')
    return result

def heading(axis):
    return math.degrees(math.atan2(axis[1], axis[0])) if math.hypot(axis[0], axis[1]) > .05 else None

def anatomical_axes(bone_pose, reference_bone, basis):
    # Carry the *reference anatomical frame* through the actual bone rotation.
    # No hardcoded bone-local X/Y/Z is treated as chest-forward or pelvis-up.
    qi = qinverse(reference_bone['quaternion_xyzw'])
    q = bone_pose['quaternion_xyzw']
    axes = {name: normalized(qrotate(q, qrotate(qi, direction))) for name, direction in basis.items()}
    return dict(**axes, forward_dot_up=axes['forward'][2], right_dot_up=axes['right'][2],
                cranial_dot_up=axes['up'][2], forward_heading_deg=heading(axes['forward']))

def pose_hint(chest):
    f, r, u = chest['forward_dot_up'], chest['right_dot_up'], chest['cranial_dot_up']
    if u > .7:
        return 'UPRIGHT_OR_UPRIGHT_LEAN'
    if abs(u) > .6 or max(abs(f), abs(r)) < .55:
        return 'OBLIQUE_OR_UNCERTAIN_READ_AXIS_VALUES'
    if abs(f) >= abs(r):
        return 'CHEST_UP_SUPINE_CANDIDATE' if f > 0 else 'CHEST_DOWN_PRONE_CANDIDATE'
    return 'RIGHT_SIDE_UP_LEFT_SIDE_DOWN_CANDIDATE' if r > 0 else 'LEFT_SIDE_UP_RIGHT_SIDE_DOWN_CANDIDATE'


def audit_target(seq,source,mesh,bones,component):
    model=seq.get_editor_property('data_model_interface'); source_model=source.get_editor_property('data_model_interface')
    rate=model.get_frame_rate(); source_rate=source_model.get_frame_rate()
    count=int(model.get_number_of_keys()); duration=float(model.get_play_length())
    check('original timing retained '+seq.get_name(),count==int(source_model.get_number_of_keys()) and count==int(model.get_number_of_frames())+1
          and rate.numerator==source_rate.numerator and rate.denominator==source_rate.denominator and abs(duration-float(source_model.get_play_length()))<1e-6)
    raw=options(mesh); effective=options(mesh,True)
    first=P.get_anim_pose_at_frame(seq,0,raw)
    check('required target bones evaluated',P.is_valid(first) and {x.casefold() for x in bones.values()}<={str(x).casefold() for x in P.get_bone_names(first)})
    refs={k:transform(P.get_ref_bone_pose(first,U.Name(v),U.AnimPoseSpaces.WORLD)) for k,v in bones.items()}
    pos=lambda key:refs[key]['position_cm']
    up=normalized(sub(pos('head'),pos('hips')))
    right=sub(pos('thigh_r'),pos('thigh_l')); right=normalized(sub(right,mul(up,dot(up,right))))
    forward=normalized(cross(right,up))
    toes=add(sub(pos('ball_l'),pos('foot_l')),sub(pos('ball_r'),pos('foot_r')))
    check('target anatomical handedness agrees with toes',dot(forward,normalized(toes))>.5)
    basis=dict(forward=forward,right=right,up=up)
    curves=[str(x) for x in L.get_animation_curve_names(seq,U.RawCurveTrackTypes.RCT_FLOAT)]
    source_curves=[str(x) for x in L.get_animation_curve_names(source,U.RawCurveTrackTypes.RCT_FLOAT)]
    check('copied curve names retained',set(curves)==set(source_curves))
    for name in source_curves:
        ta,va=L.get_float_keys(source,U.Name(name)); tb,vb=L.get_float_keys(seq,U.Name(name))
        check('full authored curve preserved '+name,list(ta)==list(tb) and list(va)==list(vb))
    rel=component.get_relative_transform(); mesh_scale=vec(rel.scale3d); q=rel.rotation
    mesh_q=[float(q.x),float(q.y),float(q.z),float(q.w)]
    check('actual component scale is positive uniform',min(mesh_scale)>0 and max(mesh_scale)-min(mesh_scale)<1e-5,mesh_scale)
    frames=[]
    for index in range(count):
        pose=P.get_anim_pose_at_frame(seq,index,raw)
        if not P.is_valid(pose): raise RuntimeError('Invalid final target frame '+str(index))
        time=index*rate.denominator/rate.numerator
        transforms={k:transform(P.get_bone_pose(pose,U.Name(v),U.AnimPoseSpaces.WORLD)) for k,v in bones.items()}
        frames.append(dict(frame=index,time_seconds=time,bones=transforms,
            anatomical=dict(chest=anatomical_axes(transforms['chest'],refs['chest'],basis),pelvis=anatomical_axes(transforms['hips'],refs['hips'],basis)),
            curves={name:float(L.get_float_value_at_time(seq,U.Name(name),time)) if name in curves else None for name in ('contact_l','contact_r','phase')}))
    locked=[]
    if not IS_SPRINT:
        for index in (0,count-1):
            pose=P.get_anim_pose_at_frame(seq,index,effective)
            check('native root-extracted/locked endpoint',P.is_valid(pose))
            transforms={k:transform(P.get_bone_pose(pose,U.Name(v),U.AnimPoseSpaces.WORLD)) for k,v in bones.items()}
            locked.append(dict(frame=index,time_seconds=index*rate.denominator/rate.numerator,bones=transforms,
                anatomical=dict(chest=anatomical_axes(transforms['chest'],refs['chest'],basis),pelvis=anatomical_axes(transforms['hips'],refs['hips'],basis))))
    roots=[f['bones']['root']['position_cm'] for f in frames]
    travel=sum(math.hypot(*sub(b,a)[:2]) for a,b in zip(roots,roots[1:]))
    root_copy=[]
    if not IS_SPRINT:
        source_eval=options(sm)
        source_roots=[]; source_rotations=[]; source_root_ref=None
        for index in range(count):
            pose=P.get_anim_pose_at_frame(source,index,source_eval)
            check('valid original getup root frame',P.is_valid(pose)) if index in (0,count-1) else None
            if not P.is_valid(pose): raise RuntimeError('Invalid original root frame')
            source_root=transform(P.get_bone_pose(pose,U.Name('root'),U.AnimPoseSpaces.WORLD))
            source_roots.append(source_root['position_cm']);source_rotations.append(source_root['quaternion_xyzw'])
            if index==0: source_root_ref=transform(P.get_ref_bone_pose(pose,U.Name('root'),U.AnimPoseSpaces.WORLD))
        for axis in range(3):
            # UE5.8 CopyRootMotionFromSourceRoot uses reference-root offset,
            # never animation-first-frame offset. Keep nonzero B/F first roots.
            origin=source_root_ref['position_cm'][axis]
            values=[p[axis]-origin for p in source_roots]
            targets=[p[axis]-origin for p in roots]
            energy=sum(v*v for v in values)
            gain=sum(v*t for v,t in zip(values,targets))/energy if energy>1e-8 else None
            residual=max(abs(t-(gain*v if gain is not None else 0.)) for v,t in zip(values,targets))
            check('native root motion preserved as scaled source on axis '+str(axis),residual<.001 and (gain is None or gain>0.),
                  dict(gain=gain,max_residual_cm=residual))
            root_copy.append(dict(axis=axis,source_reference_root_cm=origin,measured_source_to_target_root_scale=gain,max_residual_cm=residual))
        angular_errors=[]
        for f,q in zip(frames,source_rotations):
            expected=qmul(qmul(q,qinverse(source_root_ref['quaternion_xyzw'])),refs['root']['quaternion_xyzw'])
            actual=f['bones']['root']['quaternion_xyzw']
            norm=math.sqrt(sum(x*x for x in expected)*sum(x*x for x in actual))
            cosine=min(1.,abs(sum(a*b for a,b in zip(expected,actual)))/norm)
            angular_errors.append(math.degrees(2.*math.acos(cosine)))
        check('native root relative rotation retained for all original frames',max(angular_errors)<.02,max(angular_errors))
    result=dict(path=package(seq),source=package(source),keys=count,duration_seconds=duration,
        component_relative_transform=transform(rel),root_first_cm=roots[0],root_last_cm=roots[-1],root_xy_path_cm=travel,
        raw_first=frames[0],raw_last=frames[-1],root_extracted_locked_endpoints=locked,root_copy_verification=root_copy,
        first_pose_hint=pose_hint(frames[0]['anatomical']['chest']),last_pose_hint=pose_hint(frames[-1]['anatomical']['chest']),
        chest_bone=bones['chest'],chest_anatomical_bone_local={k:qrotate(qinverse(refs['chest']['quaternion_xyzw']),v) for k,v in basis.items()})
    if not IS_SPRINT:
        result['source_root_reference']=source_root_ref
        result['root_rotation_max_error_deg']=max(angular_errors)
    if IS_SPRINT:
        metrics={}
        for side in ('l','r'):
            for key in ('foot_'+side,'ball_'+side):
                samples=[]
                for a,b in zip(frames,frames[1:]):
                    ca,cb=a['curves']['contact_'+side],b['curves']['contact_'+side]
                    if ca is None or cb is None or min(ca,cb)<.8: continue
                    seconds=b['time_seconds']-a['time_seconds']
                    v=mul(sub(b['bones'][key]['position_cm'],a['bones'][key]['position_cm']),mesh_scale[0]/seconds)
                    actor_v=qrotate(mesh_q,v)
                    samples.append(dict(dt=seconds,backward_cm_s=-actor_v[0],actor_relative_velocity_cm_s=actor_v))
                span=sum(x['dt'] for x in samples)
                backward=sum(x['dt']*x['backward_cm_s'] for x in samples)/span if span else None
                metrics[key]=dict(stance_intervals=len(samples),stance_seconds=span,mean_backward_speed_cm_s_at_rate1=backward,
                    candidate_rate_for_650=650./backward if backward and backward>0 else None,
                    calibration='Target final raw clip + actual CDO component scale/rotation; not runtime blended pose or zero-slip PASS')
        result['target_stance_metrics']=metrics
    trajectory=OUT/(seq.get_name()+'_target_frames.json')
    trajectory.write_text(json.dumps(dict(path=package(seq),source=package(source),bone_mapping=bones,reference_bones=refs,
        reference_anatomical_basis=basis,component_relative_transform=transform(rel),frames=frames),ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
    result['trajectory']=dict(path=str(trajectory),sha256=sha(trajectory))
    return result


try:
    check('current HarborCity project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('new batch directory only',not disk(DEST+'/placeholder').parent.exists() and not A.does_directory_exist(DEST))
    check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    source_file,source_report=latest('GASSprintInPlace' if IS_SPRINT else 'GASRecoverySourceApply')
    R['inputs']['source']=dict(path=str(source_file),sha256=sha(source_file))
    for row in source_report['assets']: protect(row['path'],row['sha256'])
    p0=json.loads((DOC/'research/GAS_P0_SOURCE_MANIFEST.json').read_text('utf-8-sig'))
    p0rt=json.loads((DOC/'research/GAS_P0_RETARGET_MANIFEST.json').read_text('utf-8-sig'))
    check('actual P0 source and rig manifests',p0['status']=='PASS' and p0rt['status']=='PASS')
    for manifest in (p0,p0rt):
        for row in manifest['assets']: protect(row['path'],row['sha256'])
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/HarborCity/M5VS2', '/Game/HarborCity/M5VS1/HeroSelestia'],force_rescan=True)
    sm,ss=load(P0+'/SKM_GAS_UEFN_P0'),load(P0+'/SK_GAS_UEFN_P0')
    sr=load(GAS+'/Rigs/IK_GAS_UEFN_P0')
    if IS_SPRINT:
        tm=load('/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia')
        tr=load(GAS+'/Rigs/IK_GAS_Selestia_P0')
        blueprint='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia'
        human=dict(hips='Hips',chest='Chest',head='Head',leftUpperLeg='UpperLeg_L',rightUpperLeg='UpperLeg_R',
                   leftFoot='Foot_L',rightFoot='Foot_R',leftToes='Toe_L',rightToes='Toe_R')
        source_paths=[source_report['destination']+'/Working/M_Relaxed_Sprint_Loop_F_InPlace']
    else:
        npc_file,npc=latest('NPCIntegration'+LETTER)
        check('actual existing NPC identity',npc.get('sample')==LETTER and npc['destination'].startswith('/Game/HarborCity/M5VS2/NPC/AvatarSample_'+LETTER+'/Runtime_'))
        R['inputs']['npc_integration']=dict(path=str(npc_file),sha256=sha(npc_file),specimen=npc['specimen'])
        tm=load(npc['specimen']['mesh']); tr=load(npc['destination']+'/IK_TargetVRoid')
        for row in npc['assets']:
            if package(row['path']) in (package(tr),package(npc['specimen']['profile'])): protect(row['path'],row['sha256'])
        profile=load(npc['specimen']['profile'])
        human={str(k):str(v) for k,v in profile.get_editor_property('humanoid_bones').items()}
        blueprint=package(npc['specimen']['blueprint'])
        source_paths=[source_report['destination']+'/Animations/M_ragdoll_getup_stand_'+d for d in ('B','F','L','R')]
    ts=tm.get_editor_property('skeleton')
    check('independent target skeleton',ts is not None and ts!=ss and sm.get_editor_property('skeleton')==ss)
    cls=U.load_class(None,blueprint+'.'+blueprint.rsplit('/',1)[1]+'_C')
    check('actual existing character class',cls is not None)
    body=U.get_default_object(cls).get_component_by_class(U.SkeletalMeshComponent)
    check('actual CDO body uses target mesh',body is not None and body.get_editor_property('skeletal_mesh_asset')==tm)
    for obj in (tm,ts,tr,sr): protect(obj)
    protect(blueprint)
    for file in (PROJECT/'Config').glob('*.ini'): PROTECTED[str(file)]=sha(file)
    sources=[load(path) for path in source_paths]
    check('exact source count and skeleton',len(sources)==(1 if IS_SPRINT else 4) and all(isinstance(s,U.AnimSequence) and s.get_editor_property('skeleton')==ss for s in sources))
    sc,tc=U.IKRigController.get_controller(sr),U.IKRigController.get_controller(tr)
    check('reused rigs exact meshes/root',sc.get_skeletal_mesh()==sm and tc.get_skeletal_mesh()==tm
          and str(sc.get_retarget_root()).casefold()=='pelvis' and str(tc.get_retarget_root()).casefold()==human['hips'].casefold())
    source_chains={str(c.get_editor_property('chain_name')) for c in sc.get_retarget_chains()}
    target_chains={str(c.get_editor_property('chain_name')) for c in tc.get_retarget_chains()}
    check('all reused chain names match',{n.casefold() for n in source_chains}=={n.casefold() for n in target_chains}
          and len(source_chains)==len(target_chains)==21,sorted(source_chains))
    rt_path=DEST+'/RTG_GAS_'+('Sprint' if IS_SPRINT else 'GetUp')
    rt=AT.create_asset(rt_path.rsplit('/',1)[1],DEST,U.IKRetargeter,U.IKRetargetFactory())
    check('create new dedicated retargeter',rt is not None)
    rc=U.IKRetargeterController.get_controller(rt); rc.remove_all_ops()
    src,dst=U.RetargetSourceOrTarget.SOURCE,U.RetargetSourceOrTarget.TARGET
    rc.set_ik_rig(src,sr);rc.set_ik_rig(dst,tr);rc.set_preview_mesh(src,sm);rc.set_preview_mesh(dst,tm)
    pi=rc.add_retarget_op('/Script/IKRig.IKRetargetPelvisMotionOp');fi=rc.add_retarget_op('/Script/IKRig.IKRetargetFKChainsOp')
    check('native pelvis/FK operations',pi>=0 and fi>=0)
    for op in (pi,fi): rc.run_op_initial_setup(op)
    pc=rc.get_op_controller(pi);pc.set_source_pelvis_bone('pelvis');pc.set_target_pelvis_bone(human['hips'])
    for name in sorted(target_chains): check('explicit chain '+name,rc.set_source_chain(name,name,rc.get_op_name(fi)))
    fc=rc.get_op_controller(fi);settings=fc.get_settings();chains=list(settings.get_editor_property('chains_to_retarget'))
    for chain in chains:
        chain.set_editor_property('enable_fk',True);chain.set_editor_property('rotation_mode',U.FKChainRotationMode.INTERPOLATED)
        chain.set_editor_property('translation_mode',U.FKChainTranslationMode.NONE)
    settings.set_editor_property('chains_to_retarget',chains);fc.set_settings(settings)
    check('no bone stretching',all(c.translation_mode==U.FKChainTranslationMode.NONE for c in fc.get_settings().chains_to_retarget))
    if not IS_SPRINT:
        ri=rc.add_retarget_op('/Script/IKRig.IKRetargetRootMotionOp');check('native independent floor-root motion op',ri>=0)
        rc.run_op_initial_setup(ri);rcontrol=rc.get_op_controller(ri)
        rcontrol.set_source_root_bone('root');rcontrol.set_target_root_bone('Root');rcontrol.set_target_pelvis_bone(human['hips'])
        rs=rcontrol.get_settings()
        check('root copy actual bindings/defaults',str(rcontrol.get_source_root_bone()).casefold()=='root' and str(rcontrol.get_target_root_bone()).casefold()=='root'
              and str(rcontrol.get_target_pelvis_bone()).casefold()==human['hips'].casefold() and rs.root_motion_source==U.RootMotionSource.COPY_FROM_SOURCE_ROOT
              and rs.root_height_source==U.RootMotionHeightSource.COPY_HEIGHT_FROM_SOURCE)
        offset=transform(rs.get_editor_property('global_offset'))
        check('root motion has no extra offset or suppressed child propagation',length(offset['position_cm'])<1e-8
              and abs(offset['quaternion_xyzw'][3])>1.-1e-8 and max(abs(x-1.) for x in offset['scale'])<1e-8
              and rs.get_editor_property('propagate_to_non_retargeted_children'),offset)
        R['root_motion_operation']=dict(order_after_pelvis_fk=True,source_root=str(rcontrol.get_source_root_bone()),
            target_root=str(rcontrol.get_target_root_bone()),translation_formula='SourceRefRoot+(SourceFrameRoot-SourceRefRoot)*PelvisGlobalScale',
            rotation_formula='SourceFrameRootQuat*inverse(SourceRefRootQuat)*TargetRefRootQuat',global_offset=offset)
    align='GAS_Recovery_Alignment';rc.create_retarget_pose(align,dst)
    check('select private target alignment',rc.set_current_retarget_pose(align,dst));rc.auto_align_all_bones(dst,U.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(rt)
    inputs=U.IKRetargetBatchOperationInputs()
    for key,value in dict(assets_to_retarget=[A.find_asset_data(package(s)) for s in sources],source_mesh=sm,target_mesh=tm,
        ik_retarget_asset=rt,target_path=DEST+'/Animations',suffix=SUFFIX,use_source_path=False,include_referenced_assets=False,
        overwrite_existing_files=False,retain_additive_flags=True).items(): inputs.set_editor_property(key,value)
    outputs=[item.get_asset() for item in U.IKRetargetBatchOperation.run_batch_retarget(inputs)]
    expected={DEST+'/Animations/'+s.get_name()+SUFFIX for s in sources}
    check('exact isolated sequence outputs',len(outputs)==len(sources) and all(isinstance(o,U.AnimSequence) for o in outputs) and {package(o) for o in outputs}==expected)
    bones=dict(root=human['hips'] if IS_SPRINT else 'Root',hips=human['hips'],chest=human.get('upperChest',human['chest']),head=human['head'],
               thigh_l=human['leftUpperLeg'],thigh_r=human['rightUpperLeg'],foot_l=human['leftFoot'],foot_r=human['rightFoot'],
               ball_l=human['leftToes'],ball_r=human['rightToes'])
    for seq in outputs:
        check('target independent skeleton',seq.get_editor_property('skeleton')==ts)
        seq.set_editor_property('enable_root_motion',not IS_SPRINT);seq.set_editor_property('force_root_lock',not IS_SPRINT)
        seq.set_editor_property('root_motion_root_lock',U.RootMotionRootLock.REF_POSE)
        source=next(s for s in sources if s.get_name()+SUFFIX==seq.get_name())
        audit=audit_target(seq,source,tm,bones,body);R['target_audits'].append(audit)
        save(seq)
    check('exact asset budget',len(R['assets'])==len(sources)+1)
    check('no dirty map writes',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['runtime_contract']=dict(sequence_only=True,montages='NOT_CREATED; runtime may create private transient FullBody montage after pose audit',
        direction_mapping=None if IS_SPRINT else {'B':'supine','F':'prone','L':'left_side_down','R':'right_side_down'},
        target_world_alignment='Use this actual target raw/effective endpoints and CDO transform; never substitute source skeleton coordinates.')
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc()
finally:
    changed=[file for file,digest in PROTECTED.items() if not Path(file).is_file() or sha(file)!=digest]
    R['preservation']=dict(files_checked=len(PROTECTED),changed_files=changed)
    if changed: R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':
    raise RuntimeError('New GAS retarget author failed; preserve unique batch and evidence')
