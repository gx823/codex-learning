"""Retarget exactly the 10 saved GAS cut clips. New candidates only; no live graph writes.

Phases GASTransitionRetarget / GASTransitionRetargetReload.
Root runs this with the existing UE author launcher, not ordinary Python.
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
SOURCE = '/Game/HarborCity/M5VS2/GASTransitionSource/Batch_a7bd8e3e062d'
GAS = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS'
ROOT = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASTransitions'
OWNER = 'HarborCity_M5_VS2_GASTransitionRetarget_v1'
SUFFIX = '_SelestiaTransition'
NAMES = ['Jump_F_Start_Stand_Rfoot','Jump_F_Start_Run_Rfoot','Jump_F_Start_Sprint_Rfoot',
         'Jump_Loop_Fall','Jump_F_Land_Stand_Light_Rfoot','Jump_F_Land_Run_Light_Rfoot',
         'Run_Start_F_Lfoot','Sprint_Start_F_Lfoot','Run_Stop_F_Lfoot','Sprint_Stop_F_Lfoot']
A, L, P = U.EditorAssetLibrary, U.AnimationLibrary, U.AnimPoseExtensions
AT = U.AssetToolsHelpers.get_asset_tools()


def arg(name):
    result = re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(result) != 1:
        raise RuntimeError('Exactly one '+name+' required')
    return result[0][0] or result[0][1]


OUT, PHASE = Path(arg('M5EvidenceDir')).resolve(), arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
assert PHASE in ('GASTransitionRetarget','GASTransitionRetargetReload')
OUT.mkdir(parents=True, exist_ok=True)
DEST = ROOT+'/Batch_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
R = dict(status='RUNNING',phase=PHASE,destination=DEST,checks=[],assets=[],target_audits=[],inputs={},
         gameplay_binding='NOT_RUN',runtime='NOT_RUN',visual='NOT_RUN',user_naturalness='USER_REVIEW',
         map_writes=0,skeleton_writes=0,started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(label, good, observed=None):
    R['checks'].append(dict(name=label,status='PASS' if good else 'FAIL',observed=observed))
    dump()
    if not good:
        raise RuntimeError(label+': '+repr(observed))


def package(obj):
    return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]


def disk(obj):
    path=package(obj)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''): h.update(block)
    return h.hexdigest()


def protect(obj, expected=None):
    path=disk(obj); actual=sha(path)
    check('protected source '+package(obj),expected is None or actual==expected)
    PROTECTED[str(path)]=actual


def load(path):
    obj=A.load_asset(path); check('load '+package(path),obj is not None); return obj


def latest(phase):
    for path in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        data=json.loads(path.read_text('utf-8-sig'))
        if data.get('status')=='PASS':
            R['inputs'][phase]=dict(path=str(path),sha256=sha(path)); return data
    raise RuntimeError('No actual PASS '+phase)


def save(obj):
    path=package(obj); check('only fresh transition namespace saved',path.startswith(DEST+'/'))
    A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER)
    check('native save '+path,A.save_loaded_asset(obj,False) and disk(path).is_file())
    R['assets'].append(dict(path=path,sha256=sha(disk(path)),bytes=disk(path).stat().st_size,class_name=obj.get_class().get_name()))
    dump()


def evaluate(seq, source, mesh):
    model=seq.get_editor_property('data_model_interface'); smodel=source.get_editor_property('data_model_interface')
    rate=model.get_frame_rate(); count=int(model.get_number_of_keys()); duration=float(model.get_play_length())
    source_rate=smodel.get_frame_rate()
    check('original cut timing '+seq.get_name(),count==int(smodel.get_number_of_keys())
          and rate.numerator==source_rate.numerator and rate.denominator==source_rate.denominator
          and abs(duration-float(smodel.get_play_length()))<1e-6)
    check('in-place target flags '+seq.get_name(),float(seq.get_editor_property('rate_scale'))==1.
          and not seq.get_editor_property('enable_root_motion') and not seq.get_editor_property('force_root_lock'))
    options=U.AnimPoseEvaluationOptions()
    for key,value in dict(evaluation_type=U.AnimDataEvalType.RAW,should_retarget=True,optional_skeletal_mesh=mesh,
                          extract_root_motion=False,incorporate_root_motion_into_pose=True,evaluate_curves=True).items():
        options.set_editor_property(key,value)
    names=['Hips','Chest','Head','UpperLeg_L','LowerLeg_L','Foot_L','Toe_L',
           'UpperLeg_R','LowerLeg_R','Foot_R','Toe_R','Hand_L','Hand_R']
    frames=[]; root0=None; max_root_move=0.; max_root_angle=0.; max_key_step={n:0. for n in names}
    for frame in range(count):
        pose=P.get_anim_pose_at_frame(seq,frame,options)
        check('first native pose '+seq.get_name(),P.is_valid(pose)) if frame==0 else None
        if not P.is_valid(pose): raise RuntimeError('Invalid target pose')
        if frame==0:
            observed=[str(x) for x in P.get_bone_names(pose)]
            missing=[x for x in names if x.casefold() not in {n.casefold() for n in observed}]
            check('required target hierarchy',not missing,dict(required=names,missing=missing,actual_bone_names=observed))
        row={}
        for name in names:
            t=P.get_bone_pose(pose,U.Name(name),U.AnimPoseSpaces.WORLD); q=t.rotation; v=t.translation
            data=dict(position_cm=[v.x,v.y,v.z],quaternion_xyzw=[q.x,q.y,q.z,q.w],scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z])
            if not all(math.isfinite(x) for values in data.values() for x in values): raise RuntimeError('Non-finite '+name)
            if abs(sum(x*x for x in data['quaternion_xyzw'])-1)>1e-3: raise RuntimeError('Invalid quaternion '+name)
            if frames: max_key_step[name]=max(max_key_step[name],math.dist(data['position_cm'],frames[-1]['bones'][name]['position_cm']))
            row[name]=data
        root0=root0 or row['Hips']
        max_root_move=max(max_root_move,math.dist(root0['position_cm'],row['Hips']['position_cm']))
        dot=abs(sum(a*b for a,b in zip(root0['quaternion_xyzw'],row['Hips']['quaternion_xyzw'])))
        max_root_angle=max(max_root_angle,math.degrees(2*math.acos(min(1.,max(0.,dot)))))
        frames.append(dict(frame=frame,time_seconds=frame*rate.denominator/rate.numerator,bones=row))
    # Selestia's actual root is Hips (index 0): keep its authored pelvis motion.
    # The independent GAS source root, not the target pelvis, must be fixed.
    source_options=U.AnimPoseEvaluationOptions()
    for key,value in dict(evaluation_type=U.AnimDataEvalType.RAW,should_retarget=True,optional_skeletal_mesh=sm,
                          extract_root_motion=False,incorporate_root_motion_into_pose=True).items():source_options.set_editor_property(key,value)
    source_root0=None;source_root_max_cm=0.;source_root_max_degrees=0.
    for frame in range(count):
        pose=P.get_anim_pose_at_frame(source,frame,source_options)
        if not P.is_valid(pose):raise RuntimeError('Invalid actual source pose')
        root=P.get_bone_pose(pose,U.Name('root'),U.AnimPoseSpaces.WORLD)
        values=[root.translation.x,root.translation.y,root.translation.z]
        q=[root.rotation.x,root.rotation.y,root.rotation.z,root.rotation.w]
        if source_root0 is None:source_root0=(values,q)
        source_root_max_cm=max(source_root_max_cm,math.dist(values,source_root0[0]))
        dot=abs(sum(a*b for a,b in zip(q,source_root0[1])))
        source_root_max_degrees=max(source_root_max_degrees,math.degrees(2*math.acos(min(1.,dot))))
    check('actual independent GAS root fixed '+source.get_name(),source_root_max_cm<1e-3 and source_root_max_degrees<.05,
          dict(max_cm=source_root_max_cm,max_degrees=source_root_max_degrees))
    source_curves={str(x) for x in L.get_animation_curve_names(source,U.RawCurveTrackTypes.RCT_FLOAT)}
    target_curves={str(x) for x in L.get_animation_curve_names(seq,U.RawCurveTrackTypes.RCT_FLOAT)}
    check('curve identities '+seq.get_name(),source_curves==target_curves)
    curve_max=0.; curve_samples=0
    for name in sorted(source_curves):
        times,_=L.get_float_keys(source,U.Name(name)); actual_times,_=L.get_float_keys(seq,U.Name(name))
        check('curve key count '+name,len(times)==len(actual_times))
        samples=sorted(set([0.,duration]+list(times)+list(actual_times)+[(a+b)/2 for a,b in zip(times,times[1:])]))
        for time in samples:
            expected=float(L.get_float_value_at_time(source,U.Name(name),time)); actual=float(L.get_float_value_at_time(seq,U.Name(name),time))
            error=abs(expected-actual);curve_max=max(curve_max,error);curve_samples+=1
            if not math.isfinite(actual) or error>1e-4+abs(expected)*2e-6: raise RuntimeError('Retarget changed curve function '+name)
    frame_file=OUT/(seq.get_name()+'_target_frames.json')
    frame_file.write_text(json.dumps(frames,ensure_ascii=False,allow_nan=False),encoding='utf-8')
    return dict(path=package(seq),source=package(source),keys=count,duration_seconds=duration,target_root_bone='Hips',
                independent_target_root=False,target_hips_max_translation_from_start_cm=max_root_move,
                target_hips_max_angle_from_start_degrees=max_root_angle,source_root_max_translation_cm=source_root_max_cm,
                source_root_max_angle_degrees=source_root_max_degrees,max_adjacent_frame_bone_step_cm=max_key_step,
                curves=len(source_curves),curve_samples=curve_samples,curve_max_function_error=curve_max,
                target_frames=str(frame_file),target_frames_sha256=sha(frame_file),continuity_visual='NOT_RUN')


try:
    check('actual HarborCity project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    source_manifest=latest('GASTransitionAssetsReload')
    check('fixed completed source batch',source_manifest['destination']==SOURCE and len(source_manifest['assets'])==20)
    for asset in source_manifest['assets']: protect(asset['path'],asset['sha256'])
    source_paths=[SOURCE+'/Working/M_Relaxed_'+name+'_Cut_InPlace' for name in NAMES]
    sources=[load(path) for path in source_paths]
    sm=load('/Game/HarborCity/M5VS2/GASSourceP0/SKM_GAS_UEFN_P0')
    tm=load('/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia')
    reference=json.loads(U.HCM4AnimationEditor.inspect_animation_asset(tm.get_editor_property('skeleton')))['reference_bones']
    check('actual Selestia normalized root identity',len(reference)==247 and reference[0]['name']=='Hips'
          and reference[0]['parent']==-1 and not any(x['name'].casefold()=='root' for x in reference),
          dict(bone_count=len(reference),root=reference[0],all_names=[x['name'] for x in reference]))
    sr=load(GAS+'/Rigs/IK_GAS_UEFN_P0'); tr=load(GAS+'/Rigs/IK_GAS_Selestia_P0')
    for obj in [sm,tm,sr,tr,sm.get_editor_property('skeleton'),tm.get_editor_property('skeleton')]: protect(obj)
    for path in (PROJECT/'Content/HarborCity/M5VS2/HeroSelestia').rglob('*.uasset'): PROTECTED[str(path)]=sha(path)
    check('exact fixed source sequence identities',len(sources)==10 and all(isinstance(x,U.AnimSequence)
          and x.get_editor_property('skeleton')==sm.get_editor_property('skeleton') for x in sources))
    if PHASE.endswith('Reload'):
        prior=latest('GASTransitionRetarget'); DEST=prior['destination'];R['destination']=DEST
        check('private retarget outputs',DEST.startswith(ROOT+'/Batch_') and len(prior['assets'])==11)
        for row in prior['assets']: protect(row['path'],row['sha256'])
        outputs=[load(DEST+'/Animations/'+s.get_name()+SUFFIX) for s in sources]
        R['assets']=prior['assets']
    else:
        check('fresh private namespace',not disk(DEST+'/placeholder').parent.exists() and not A.does_directory_exist(DEST))
        rt=AT.create_asset('RTG_GAS_Transitions',DEST,U.IKRetargeter,U.IKRetargetFactory())
        check('private retargeter created',rt is not None)
        c=U.IKRetargeterController.get_controller(rt);c.remove_all_ops()
        src,dst=U.RetargetSourceOrTarget.SOURCE,U.RetargetSourceOrTarget.TARGET
        c.set_ik_rig(src,sr);c.set_ik_rig(dst,tr);c.set_preview_mesh(src,sm);c.set_preview_mesh(dst,tm)
        pi=c.add_retarget_op('/Script/IKRig.IKRetargetPelvisMotionOp');fi=c.add_retarget_op('/Script/IKRig.IKRetargetFKChainsOp')
        check('native pelvis and FK ops',pi>=0 and fi>=0)
        for index in [pi,fi]:c.run_op_initial_setup(index)
        pc=c.get_op_controller(pi);pc.set_source_pelvis_bone('pelvis');pc.set_target_pelvis_bone('Hips')
        sc,tc=U.IKRigController.get_controller(sr),U.IKRigController.get_controller(tr)
        chains=[str(x.chain_name) for x in tc.get_retarget_chains()]
        check('known exact chain map',len(chains)==21 and set(chains)=={str(x.chain_name) for x in sc.get_retarget_chains()})
        for name in chains:check('map chain '+name,c.set_source_chain(name,name,c.get_op_name(fi)))
        fc=c.get_op_controller(fi);settings=fc.get_settings();mapped=list(settings.chains_to_retarget)
        for chain in mapped:
            chain.enable_fk=True;chain.rotation_mode=U.FKChainRotationMode.INTERPOLATED;chain.translation_mode=U.FKChainTranslationMode.NONE
        settings.chains_to_retarget=mapped;fc.set_settings(settings)
        check('FK does not stretch bones',all(x.translation_mode==U.FKChainTranslationMode.NONE for x in fc.get_settings().chains_to_retarget))
        align='GAS_Transition_Alignment';c.create_retarget_pose(align,dst)
        check('private target alignment selected',c.set_current_retarget_pose(align,dst));c.auto_align_all_bones(dst,U.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
        save(rt)
        inputs=U.IKRetargetBatchOperationInputs()
        for key,value in dict(assets_to_retarget=[A.find_asset_data(package(x)) for x in sources],source_mesh=sm,target_mesh=tm,
            ik_retarget_asset=rt,target_path=DEST+'/Animations',suffix=SUFFIX,use_source_path=False,include_referenced_assets=False,
            overwrite_existing_files=False,retain_additive_flags=True).items(): inputs.set_editor_property(key,value)
        outputs=[x.get_asset() for x in U.IKRetargetBatchOperation.run_batch_retarget(inputs)]
        check('exact 10 target sequences',len(outputs)==10 and {package(x) for x in outputs}=={DEST+'/Animations/'+s.get_name()+SUFFIX for s in sources})
    for source in sources:
        seq=next(x for x in outputs if x.get_name()==source.get_name()+SUFFIX)
        check('unchanged target skeleton',seq.get_editor_property('skeleton')==tm.get_editor_property('skeleton'))
        if not PHASE.endswith('Reload'):
            seq.set_editor_property('enable_root_motion',False);seq.set_editor_property('force_root_lock',False);seq.set_editor_property('rate_scale',1.)
        R['target_audits'].append(evaluate(seq,source,tm))
        if not PHASE.endswith('Reload'):save(seq)
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['exception']=traceback.format_exc()
finally:
    changed=[path for path,value in PROTECTED.items() if not Path(path).is_file() or sha(path)!=value]
    R['source_preservation']=dict(files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Transition retarget failed; keep unique batch and evidence')
