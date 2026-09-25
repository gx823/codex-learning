"""Evidence-bound single new J/T/U/V/W/X getup CDO configuration only.

Root runs NPCGetUpBindingDryRun, then NPCGetUpBindingApply with the existing
author_run wrapper. Eight target sequences and existing NPC inputs are read-only.
Only the one exact new cast Blueprint may be saved, after private byte backups.
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
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
NPC_ROOT='/Game/HarborCity/M5VS2/NPC'
KEY='HarborCityOwnedBy';OWNER='HarborCity_M5_VS2_NPC_Integration'
A,L,P=U.EditorAssetLibrary,U.AnimationLibrary,U.AnimPoseExtensions

def arg(name):
    matches=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    if len(matches)!=1:raise RuntimeError('Exactly one '+name+' required')
    return matches[0][0] or matches[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
matched=re.fullmatch(r'NPCGetUpR2([JTUVWX])(DryRun|Apply)',PHASE)
assert matched is not None
LETTER=matched.group(1)
APPLY=PHASE.endswith('Apply');OUT.mkdir(parents=True,exist_ok=True)
R=dict(status='RUNNING',phase=PHASE,checks=[],inputs={},plans=[],assets=[],transitions=[],
    map_writes=0,animation_writes=0,physics_writes=0,applied=False,runtime='NOT_RUN',visual='NOT_RUN',
    started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={};ALLOW_CHANGED={};OBJECTS={}

def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')

def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed));dump()
    if not ok:raise RuntimeError(name+': '+repr(observed))

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()

def package(obj):return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def binding(path):return dict(path=str(path),sha256=sha(path))
def disk(obj):
    path=package(obj);assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')

def protect_file(path,expected=None):
    path=Path(path);actual=sha(path)
    check('exact protected input '+str(path),expected is None or actual==expected)
    PROTECTED[str(path)]=actual

def latest_pass(pattern,predicate=lambda d:True):
    for file in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        data=json.loads(file.read_text('utf-8-sig'))
        if data.get('status')=='PASS' and predicate(data):return file,data
    raise RuntimeError('No successful '+pattern)

def load(path):
    obj=A.load_asset(path);check('native load '+package(path),obj is not None);return obj

def generated_class(path):
    path=package(path);c=U.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C')
    check('native generated class '+path,c is not None);return c

def vector(values):return U.Vector(*[float(v) for v in values])
def xyz(v):return [float(v.x),float(v.y),float(v.z)]

def near(a,b,tolerance=.002):
    if isinstance(a,dict):return isinstance(b,dict) and a.keys()==b.keys() and all(near(a[k],b[k],tolerance) for k in a)
    if isinstance(a,list):return isinstance(b,list) and len(a)==len(b) and all(near(x,y,tolerance) for x,y in zip(a,b))
    if isinstance(a,(int,float)) and isinstance(b,(int,float)):return math.isfinite(a) and math.isfinite(b) and abs(a-b)<tolerance
    return a==b

def unchanged_snapshot(cdo,body,reaction):
    # Explicitly cover the existing native and authored settings touched by this
    # subsystem; byte backups cover a complete reversal of the two BP packages.
    return dict(identity={k:str(cdo.get_editor_property(k)) for k in ('stable_id','display_name','role_id','talkable','stationary','dialogue_lines','walk_speed','panic_speed')},
        profile=package(cdo.get_editor_property('npc_profile')),body_mesh=package(body.get_editor_property('skeletal_mesh_asset')),
        body_animation=str(body.get_editor_property('anim_class').get_path_name()),body_physics=package(body.get_editor_property('physics_asset_override')),
        body_transform=transform(body.get_relative_transform()),actor_scale=xyz(cdo.get_actor_scale3d()),
        original_physical_parameters={k:float(reaction.get_editor_property(k)) for k in ('damage_threshold_cm_s','knockdown_threshold_cm_s','maximum_delta_v_cm_s','minimum_down_seconds','stable_seconds','recovery_blend_seconds','emergency_recovery_seconds','get_up_maximum_alignment_error_cm')})

def getup_readback(component):
    rows=[]
    for row in component.get_editor_property('get_up_clips'):
        direction=next((k for k in ('SUPINE','PRONE','LEFT','RIGHT') if row.get_editor_property('direction')==getattr(U.HCM4R1GetUpDirection,k)),None)
        animation=row.get_editor_property('animation')
        check('getup soft animation resolves after native load',isinstance(animation,U.AnimSequence))
        rows.append(dict(direction=direction,animation=package(animation),**{k:xyz(row.get_editor_property(k)) for k in
            ('first_hips_component','first_chest_forward_component','first_chest_right_component','end_hips_component')}))
    return dict(use_authored_get_up=bool(component.get_editor_property('use_authored_get_up')),
        get_up_pose_data_verified=bool(component.get_editor_property('get_up_pose_data_verified')),get_up_clips=rows,
        get_up_chest_bone=str(component.get_editor_property('get_up_chest_bone')),
        **{k:xyz(component.get_editor_property(k)) for k in ('get_up_chest_forward_bone_local','get_up_chest_right_bone_local','get_up_authored_component_scale')})

def verify_clip(audit,mesh):
    seq=load(audit['path']);model=seq.get_editor_property('data_model_interface')
    check('actual independent getup skeleton and root flags',seq.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
        and seq.get_editor_property('enable_root_motion') and seq.get_editor_property('force_root_lock')
        and seq.get_editor_property('root_motion_root_lock')==U.RootMotionRootLock.REF_POSE)
    check('actual original-frame getup duration',model.get_number_of_keys()==audit['keys']==151 and abs(float(model.get_play_length())-5)<1e-6)
    file=Path(audit['trajectory']['path']).resolve()
    check('target trajectory stays in actual retarget evidence',file.parent.parent==DOC/'editor_runtime' and sha(file)==audit['trajectory']['sha256'])
    data=json.loads(file.read_text('utf-8'));bones=data['bone_mapping'];basis=data['reference_anatomical_basis'];refs=data['reference_bones']
    check('all 151 original target frame records',len(data['frames'])==151 and data['path']==audit['path'])
    raw_options=options(mesh);max_position_error=0.;max_rotation_deg=0.
    for frame in data['frames']:
        pose=P.get_anim_pose_at_frame(seq,frame['frame'],raw_options)
        if not P.is_valid(pose):raise RuntimeError('Invalid native target raw frame')
        for key in ('root','hips','chest'):
            actual=transform(P.get_bone_pose(pose,U.Name(bones[key]),U.AnimPoseSpaces.WORLD));expected=frame['bones'][key]
            max_position_error=max(max_position_error,length(sub(actual['position_cm'],expected['position_cm'])))
            qa,qb=actual['quaternion_xyzw'],expected['quaternion_xyzw'];norm=math.sqrt(sum(x*x for x in qa)*sum(x*x for x in qb))
            max_rotation_deg=max(max_rotation_deg,math.degrees(2*math.acos(min(1,abs(sum(a*b for a,b in zip(qa,qb)))/norm))))
    check('full original-frame raw root/hips/chest are still target audit',max_position_error<.002 and max_rotation_deg<.02,
        dict(path=audit['path'],max_position_error_cm=max_position_error,max_rotation_error_deg=max_rotation_deg))
    locked=[]
    for endpoint in audit['root_extracted_locked_endpoints']:
        pose=P.get_anim_pose_at_frame(seq,endpoint['frame'],options(mesh,True));check('native effective endpoint valid',P.is_valid(pose))
        actual={key:transform(P.get_bone_pose(pose,U.Name(name),U.AnimPoseSpaces.WORLD)) for key,name in bones.items()}
        check('actual root-extracted/locked complete endpoint matches saved data',near(actual,endpoint['bones']))
        chest=anatomical_axes(actual['chest'],refs['chest'],basis)
        check('actual locked chest anatomical axes',near(chest,endpoint['anatomical']['chest'],.0001))
        check('root extraction uses actual reference root',length(sub(actual['root']['position_cm'],refs['root']['position_cm']))<.001)
        locked.append(dict(bones=actual,chest=chest))
    check('exact first and last endpoint domain',[x['frame'] for x in audit['root_extracted_locked_endpoints']]==[0,150])
    local={k:qrotate(qinverse(refs['chest']['quaternion_xyzw']),v) for k,v in basis.items()}
    check('local anatomical axes independently reconstructed',near(local,audit['chest_anatomical_bone_local'],.0001))
    return locked,local

try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty maps or content',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    # Load only read-only function definitions from existing tested authors.
    helper=WORK/'tools/ue_m5_vs2_gas_recovery_retarget_author.py'
    names={'options','add','sub','mul','dot','length','normalized','cross','qrotate','qinverse','qmul','vec','transform','heading','anatomical_axes'}
    tree=ast.parse(helper.read_text('utf-8'));nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact native-pose helper subset',{n.name for n in nodes}==names)
    exec(compile(ast.fix_missing_locations(ast.Module(body=nodes,type_ignores=[])),str(helper),'exec'),globals())
    R['inputs']['pose_helper']=binding(helper)
    check('unchanged validated pose helper',sha(helper)=='d6914b02db146d05da1515a8344109e5722a7844999d22c672650c0eb2ee00d6')
    for file in (PROJECT/'Content/HarborCity/M5VS2/NPC').rglob('*.uasset'):protect_file(file)

    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([NPC_ROOT],force_rescan=True)
    for file in (PROJECT/'Config').glob('*.ini'):protect_file(file)
    for letter in (LETTER,):
        integration_file,integration=latest_pass('author_*_NPCIntegrationR2'+letter)
        check('actual single-model integration PASS',integration.get('sample')==letter and integration.get('preservation',{}).get('unexpected_changed_files')==[])
        spec=integration['specimen'];bp_path=package(spec['blueprint'])
        check('exact approved model namespace',bp_path.startswith(NPC_ROOT+'/AvatarSample_'+letter+'/Runtime_'))
        for path,row in {package(a['path']):a for a in integration['assets']}.items():protect_file(disk(path),row['sha256'])
        R['inputs']['NPC_'+letter]=dict(**binding(integration_file),specimen=spec)
        bp=load(bp_path)

        check('existing exact VS2 Blueprint owner',A.get_metadata_tag(bp,KEY)==OWNER)
        cdo=U.get_default_object(generated_class(bp_path));body=cdo.get_component_by_class(U.SkeletalMeshComponent)
        reaction=cdo.get_component_by_class(U.HCM4R1PhysicalReactionComponent)
        check('existing physical component is owned by exact new BP',reaction is not None and package(reaction)==bp_path)
        mesh=body.get_editor_property('skeletal_mesh_asset');check('exact integrated body mesh',package(mesh)==package(spec['mesh']))
        skeleton=mesh.get_editor_property('skeleton');protect_file(disk(skeleton))
        check('new optin starts disabled',not reaction.get_editor_property('use_authored_get_up') and not reaction.get_editor_property('get_up_pose_data_verified') and len(reaction.get_editor_property('get_up_clips'))==0)
        scale=xyz(body.get_editor_property('relative_scale3d'));check('actual positive uniform body scale',min(scale)>0 and max(scale)-min(scale)<1e-6)
        check('actor and every component parent have unit scale',near(xyz(cdo.get_actor_scale3d()),[1,1,1]))
        parent=body.get_attach_parent();parent_count=0
        while parent:
            parent_count+=1;check('bounded unit-scaled attachment parent',parent_count<8 and near(xyz(parent.get_editor_property('relative_scale3d')),[1,1,1]))
            parent=parent.get_attach_parent()
        rt_file,rt=latest_pass('author_*_NPCGetUpRetargetR2'+letter)
        check('exact isolated retarget identity and four actions',rt.get('phase')=='NPCGetUpRetargetR2'+letter and len(rt['target_audits'])==4 and rt['preservation']['changed_files']==[])
        R['inputs']['retarget_'+letter]=binding(rt_file)
        for asset in rt['assets']:protect_file(disk(asset['path']),asset['sha256'])
        rows=[];common=None
        for suffix,direction in zip(('B','F','L','R'),('SUPINE','PRONE','LEFT','RIGHT')):
            audits=[a for a in rt['target_audits'] if a['path'].endswith('/M_ragdoll_getup_stand_'+suffix+'_NPC'+letter+'GetUp')]
            check('exact target action '+letter+suffix,len(audits)==1);audit=audits[0]
            check('actual mesh CDO transform matches retarget input',near(transform(body.get_relative_transform()),audit['component_relative_transform']))
            ends,local=verify_clip(audit,mesh)
            chest=ends[0]['chest'];semantic={'B':chest['forward_dot_up'],'F':-chest['forward_dot_up'],'L':chest['right_dot_up'],'R':-chest['right_dot_up']}[suffix]
            check('actual directional first pose '+direction,semantic>.8,semantic)
            current=dict(get_up_chest_bone=audit['chest_bone'],get_up_chest_forward_bone_local=local['forward'],get_up_chest_right_bone_local=local['right'],get_up_authored_component_scale=scale)
            check('four clips share exact target semantic frame',common is None or near(current,common,.0001));common=current
            rows.append(dict(direction=direction,animation=audit['path'],first_hips_component=ends[0]['bones']['hips']['position_cm'],
                first_chest_forward_component=chest['forward'],first_chest_right_component=chest['right'],end_hips_component=ends[1]['bones']['hips']['position_cm']))
        # Existing verified physics repair includes the same simulated chest body.
        # This model received its own PHYS during the new integration; Q/R and other models were untouched.
        phys=integration['individual_physics_apply']
        check('same models exact native PHYS apply',phys.get('status')=='PASS' and phys.get('applied') is True
            and phys.get('native_candidate_readback_checked') is True and package(phys.get('physics',''))==package(spec['physics']))
        check('actual simulated chest mapped in this model',common['get_up_chest_bone'].casefold() in {b['bone'].casefold() for b in phys['bodies']})
        plan=dict(sample=letter,blueprint=bp_path,before_sha256=sha(disk(bp)),integration=R['inputs']['NPC_'+letter]['path'],
            integration_sha256=R['inputs']['NPC_'+letter]['sha256'],retarget=R['inputs']['retarget_'+letter],
            original_settings=unchanged_snapshot(cdo,body,reaction),before_getup=getup_readback(reaction),
            after_getup=dict(use_authored_get_up=True,get_up_pose_data_verified=True,get_up_clips=rows,**common))
        R['plans'].append(plan);OBJECTS[letter]=(bp,cdo,body,reaction);dump()
    check('one exact isolated new NPC plan',len(R['plans'])==1 and R['plans'][0]['sample']==LETTER)
    if APPLY:
        dry_file,dry=latest_pass('author_*_NPCGetUpR2'+LETTER+'DryRun')
        check('apply bound to complete actual dry run',dry['plans']==R['plans'] and dry['inputs']==R['inputs'] and not dry['applied'])
        R['dry_run']=binding(dry_file)
        backup_root=Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/OUT.name
        check('fresh private backup directory',not backup_root.exists());backup_root.mkdir(parents=True,exist_ok=False)
        # Back up BOTH packages before touching either CDO.
        for plan in R['plans']:
            file=disk(plan['blueprint']);dest=backup_root/file.name
            shutil.copy2(file,dest);check('complete original BP byte backup',sha(dest)==plan['before_sha256'])
            plan['backup']=binding(dest)
        dump()
        for plan in R['plans']:
            bp,cdo,body,component=OBJECTS[plan['sample']];bp.modify();cdo.modify();component.modify()
            wanted=plan['after_getup'];clips=[]
            for row in wanted['get_up_clips']:
                value=U.HCM4R1GetUpClip();value.set_editor_property('direction',getattr(U.HCM4R1GetUpDirection,row['direction']))
                value.set_editor_property('animation',load(row['animation']))
                for key in ('first_hips_component','first_chest_forward_component','first_chest_right_component','end_hips_component'):value.set_editor_property(key,vector(row[key]))
                clips.append(value)
            component.set_editor_property('get_up_clips',clips)
            component.set_editor_property('get_up_chest_bone',U.Name(wanted['get_up_chest_bone']))
            for key in ('get_up_chest_forward_bone_local','get_up_chest_right_bone_local','get_up_authored_component_scale'):component.set_editor_property(key,vector(wanted[key]))
            inactive=copy.deepcopy(wanted);inactive['use_authored_get_up']=inactive['get_up_pose_data_verified']=False
            check('all target data reads back before enabling',near(getup_readback(component),inactive,.0001))
            component.set_editor_property('get_up_pose_data_verified',True);component.set_editor_property('use_authored_get_up',True)
            check('compile exact modified Blueprint',U.BlueprintEditorLibrary.compile_blueprint(bp))
            cdo=U.get_default_object(generated_class(plan['blueprint']));body=cdo.get_component_by_class(U.SkeletalMeshComponent);component=cdo.get_component_by_class(U.HCM4R1PhysicalReactionComponent)
            check('compiled CDO exact new optin configuration',near(getup_readback(component),wanted,.0001))
            check('original NPC and physical gameplay settings unchanged',unchanged_snapshot(cdo,body,component)==plan['original_settings'])
            check('original integration metadata retained',A.get_metadata_tag(bp,KEY)==OWNER)
            ALLOW_CHANGED[str(disk(bp))]=plan['before_sha256']
            check('native save only exact VS2 BP',A.save_loaded_asset(bp,False))
            after=sha(disk(bp));check('saved BP actually contains new bytes',after!=plan['before_sha256'])
            transition=dict(sample=plan['sample'],path=plan['blueprint'],class_name='Blueprint',before_sha256=plan['before_sha256'],after_sha256=after,
                backup=plan['backup'],integration_evidence_sha256=plan['integration_sha256'],retarget_evidence=plan['retarget'],
                changed_scope='PhysicalReaction GetUp fields only; existing NPC settings snapshot unchanged',native_cdo_readback=getup_readback(component))
            R['transitions'].append(transition);R['assets'].append(dict(path=plan['blueprint'],sha256=after,class_name='Blueprint'));dump()
        check('exact one native BP save',len(R['transitions'])==1);R['applied']=True
    check('no maps dirtied',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc()
finally:
    changed=[p for p,digest in PROTECTED.items() if p not in ALLOW_CHANGED and (not Path(p).is_file() or sha(p)!=digest)]
    R['preservation']=dict(files_checked=len(PROTECTED),allowed_blueprint_files=list(ALLOW_CHANGED),unexpected_changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
    if APPLY and R['status']=='PASS':
        manifest=dict(schema='HarborCity.M5VS2.NPCCastR2.GetUpBinding.v1',sample=LETTER,status='PASS',phase=PHASE,evidence=binding(OUT/'author_result.json'),
            dry_run=R['dry_run'],transitions=R['transitions'],runtime='NOT_RUN',visual='NOT_RUN')
        (OUT/'getup_binding_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('Getup binding failed; retain evidence/private backups; no unreviewed overwrite retry')
