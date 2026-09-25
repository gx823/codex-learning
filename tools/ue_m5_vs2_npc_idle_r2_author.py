"""Eight-source relaxed NPC idle candidates; no old package or map writes.

Native phases NPCIdleR2<QRJTUVWX>Probe / Apply / Reload. Probe is
read-only. Apply retargets one real GAS idle and duplicates BS/ABP/final BP.
Fresh-process Reload is required before any cast author may select it.
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

WORK=Path('D:/科研学习/codex学习').resolve(); PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
NPC_ROOT='/Game/HarborCity/M5VS2/NPC'; P0='/Game/HarborCity/M5VS2/GASSourceP0'
IDLE='/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/InPlace/M_Relaxed_Stand_Idle_Loop_InPlace'
OWNER='HarborCity_M5_VS2_NPCIdleR2'; KEY='HarborCityOwnedBy'
RESOLVER=WORK/'tools/ue_m5_vs2_npc_cast_review_author.py'
RESOLVER_FUNCTIONS=('accept_npc_getup_transition','npc_specimen','cast_specimen')
RESOLVER_SHA='4e96290e80b3c9dd2c82ad7c5662a57f2a256c1acebc243777b38047ed4bb2ef'
A,B,P=U.EditorAssetLibrary,U.BlueprintEditorLibrary,U.AnimPoseExtensions

def arg(name):
    values=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(values)==1,name
    return values[0][0] or values[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve(); PHASE=arg('M5AuthorPhase')
MATCH=re.fullmatch(r'NPCIdleR2([QRJTUVWX])(Probe|Apply|Reload)',PHASE)
assert MATCH and OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
LETTER,MODE=MATCH.groups(); OUT.mkdir(parents=True,exist_ok=True)
TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
DEST=NPC_ROOT+'/AvatarSample_'+LETTER+'/IdleR2/Batch_'+TOKEN
R=dict(schema='HarborCity.M5VS2.NPCIdleR2.v1',status='RUNNING',phase=PHASE,sample=LETTER,mode=MODE,
       destination=DEST,inputs={},checks=[],assets=[],runtime='NOT_RUN',visual='USER_REVIEW',
       fresh_process_disk_readback='NOT_RUN',map_writes=0,skeleton_writes=0,physics_writes=0,
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       limitations=['A real relaxed idle loop, not authored tea-drinking, a seated pose, or a complete scene-idle system.',
                    'Native author/readback does not establish visual quality, clothing collision, or runtime locomotion/getup regression.',
                    'The original walk/run samples, FullBody slot and head/eye graph are retained.'])
PROTECTED={}

def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed))
    if not ok: dump(); raise RuntimeError(name+': '+repr(observed))
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def package(obj): return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def path(obj): return obj.get_path_name() if obj is not None else None
def disk(obj):
    p=package(obj); assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
def document(p): return json.loads(Path(p).read_text('utf-8-sig'))
def binding(p): return dict(path=str(Path(p).resolve()),sha256=sha(p))
def protect_file(p,expected=None):
    p=Path(p).resolve(); check('protected source exists',p.is_file(),str(p)); actual=sha(p)
    check('exact source SHA',expected is None or actual==expected,str(p)); PROTECTED[str(p)]=actual
def latest_pass(pattern,predicate=lambda d:True):
    for p in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda f:f.stat().st_mtime,reverse=True):
        d=document(p)
        if d.get('status')=='PASS' and predicate(d):return p,d
    raise RuntimeError('Missing completed native prerequisite: '+pattern)
def load(p):
    obj=A.load_asset(p); check('exact asset load',obj is not None,p); return obj
def cls(p):
    p=package(p); obj=U.load_class(None,p+'.'+p.rsplit('/',1)[1]+'_C')
    check('exact generated class load',obj is not None,p); return obj
def xyz(v): return [float(v.x),float(v.y),float(v.z)]
def transform(t):
    return dict(position_cm=xyz(t.translation),quaternion_xyzw=[float(t.rotation.x),float(t.rotation.y),float(t.rotation.z),float(t.rotation.w)],scale=xyz(t.scale3d))
def save(obj):
    p=package(obj); check('save only private idle batch',p.startswith(DEST+'/'),p)
    A.set_metadata_tag(obj,KEY,OWNER); check('native save',A.save_loaded_asset(obj,False) and disk(obj).is_file(),p)
    R['assets']=[a for a in R['assets'] if package(a['path'])!=p]+[dict(path=p,sha256=sha(disk(obj)),class_name=obj.get_class().get_name(),bytes=disk(obj).stat().st_size)]
def duplicate(obj,name):
    target=DEST+'/'+name; check('fresh destination asset',not A.does_asset_exist(target),target)
    result=A.duplicate_asset(package(obj),target); check('native private asset duplicate',result is not None,target);return result

def install_frozen_resolvers():
    # Execute only three reviewed read-only functions, never this other author's
    # top-level map/asset operations. Its evolving main body is not imported.
    text=RESOLVER.read_text('utf-8'); module=ast.parse(text)
    nodes=[n for n in module.body if isinstance(n,ast.FunctionDef) and n.name in RESOLVER_FUNCTIONS]
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes)
    check('exact frozen provenance resolver functions',tuple(n.name for n in nodes)==RESOLVER_FUNCTIONS
          and hashlib.sha256(code.encode()).hexdigest()==RESOLVER_SHA)
    R['inputs']['provenance_resolver']=dict(**binding(RESOLVER),function_sha256=RESOLVER_SHA,functions=list(RESOLVER_FUNCTIONS))
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(RESOLVER)+'::frozen_resolvers','exec'),globals())

def getup_snapshot(component):
    rows=[]
    for row in component.get_editor_property('get_up_clips'):
        direction=next((k for k in ('SUPINE','PRONE','LEFT','RIGHT') if row.get_editor_property('direction')==getattr(U.HCM4R1GetUpDirection,k)),None)
        animation=row.get_editor_property('animation'); check('actual getup sequence resolves',isinstance(animation,U.AnimSequence))
        rows.append(dict(direction=direction,animation=package(animation),**{k:xyz(row.get_editor_property(k)) for k in
            ('first_hips_component','first_chest_forward_component','first_chest_right_component','end_hips_component')}))
    return dict(use_authored_get_up=bool(component.get_editor_property('use_authored_get_up')),
        get_up_pose_data_verified=bool(component.get_editor_property('get_up_pose_data_verified')),get_up_clips=rows,
        get_up_chest_bone=str(component.get_editor_property('get_up_chest_bone')),
        **{k:xyz(component.get_editor_property(k)) for k in ('get_up_chest_forward_bone_local','get_up_chest_right_bone_local','get_up_authored_component_scale')})

def cdo_snapshot(bp):
    cdo=U.get_default_object(B.generated_class(bp)); body=cdo.get_component_by_class(U.SkeletalMeshComponent)
    physical=cdo.get_component_by_class(U.HCM4R1PhysicalReactionComponent); reaction=cdo.get_component_by_class(U.HCM4R1ReactionComponent)
    cap=cdo.get_component_by_class(U.CapsuleComponent)
    check('actual required NPC CDO components',all(x is not None for x in (cdo,body,physical,reaction,cap)))
    getup=getup_snapshot(physical)
    check('completed original four-direction getup',getup['use_authored_get_up'] and getup['get_up_pose_data_verified']
          and len(getup['get_up_clips'])==4 and {r['direction'] for r in getup['get_up_clips']}=={'SUPINE','PRONE','LEFT','RIGHT'})
    return dict(identity={k:str(cdo.get_editor_property(k)) for k in ('stable_id','display_name','role_id','talkable','stationary','dialogue_lines','walk_speed','panic_speed')},
        profile=package(cdo.get_editor_property('npc_profile')),mesh=package(body.get_editor_property('skeletal_mesh_asset')),
        anim_class=path(body.get_editor_property('anim_class')),physics=package(body.get_editor_property('physics_asset_override')),
        body_transform=transform(body.get_relative_transform()),actor_scale=xyz(cdo.get_actor_scale3d()),
        materials=[path(body.get_material(i)) for i in range(body.get_num_materials())],
        capsule=[float(cap.get_unscaled_capsule_radius()),float(cap.get_unscaled_capsule_half_height())],
        hits={k:package(cdo.get_editor_property('hit_'+k+'_montage')) for k in ('front','back','left','right')},
        counter_animation=package(reaction.get_editor_property('counter_animation')),reaction_preference=str(reaction.get_editor_property('reaction_preference')),
        getup=getup,physical_parameters={k:float(physical.get_editor_property(k)) for k in ('damage_threshold_cm_s','knockdown_threshold_cm_s','maximum_delta_v_cm_s',
            'minimum_down_seconds','stable_seconds','recovery_blend_seconds','emergency_recovery_seconds','get_up_maximum_alignment_error_cm')})

def graph_player(abp):
    # The actual first probe proved AnimationGraph.Nodes is protected in Python.
    # Use the compiled bounded native reader, never disable reflection protection.
    result=json.loads(U.HCM5VS2NPCIdleEditor.inspect_idle_graph(abp))
    check('native actual graph/player/samples readback',result.get('status')=='PASS',result)
    return result,load(result['blendspace']),result['nodes']

def samples_snapshot(native):
    rows=copy.deepcopy(native['samples'])
    check('exact original three speed coordinates',len(rows)==3 and [x['sample_value'] for x in rows]==[[0.,0.,0.],[140.,0.,0.],[280.,0.,0.]],rows)
    return rows

def completed_process(report_file):
    file=report_file.parent/'commandlet.json';data=document(file)
    check('actual completed native process',data.get('status')=='PASS' and data.get('exit_code')==0,dict(path=str(file),status=data.get('status'),exit_code=data.get('exit_code')))
    return binding(file)

def select_warm_q(original):
    file,data=latest_pass('author_*_966823a4_QSkinCandidate')
    check('explicit reviewed-input Q warm candidate provenance',data.get('schema')=='HarborCity.M5VS2.QSkinCandidate.v1'
          and data.get('phase')=='QSkinCandidate' and package(data['source_blueprint'])==package(original['blueprint'])
          and data.get('preservation',{}).get('changed_source_files')==[] and data.get('preservation',{}).get('source_tree_changes')==[]
          and len(data['assets'])==3 and len(data['candidates'])==2)
    original_file=str(disk(original['blueprint']).resolve())
    protect_file(original_file,data['preservation']['source_sha256_before'][original_file])
    for row in data['assets']:protect_file(disk(row['path']),row['sha256'])
    before=cdo_snapshot(load(original['blueprint']));after=cdo_snapshot(load(data['candidate_blueprint']))
    expected=copy.deepcopy(before)
    check('only original two skin slots selected',{int(row['slot']) for row in data['candidates']}=={3,7})
    for row in data['candidates']:expected['materials'][int(row['slot'])]=path(load(row['candidate']))
    check('warm source keeps full original CDO/getup except two skin slots',after==expected)
    R['inputs']['Q_skin_candidate']=dict(**binding(file),commandlet=completed_process(file))
    R['source_transition']=dict(kind='independent_Q_warm_BP',source_blueprint=package(original['blueprint']),
        source_sha256=sha(disk(original['blueprint'])),candidate_blueprint=package(data['candidate_blueprint']),
        candidate_sha256=sha(disk(data['candidate_blueprint'])),source_cdo=before,candidate_cdo=after,
        art_status='USER_REVIEW; this is the selected native warm-material input, not art approval')
    return dict(original,blueprint=data['candidate_blueprint'],generated_class=data['candidate_generated_class'])

def input_assets(spec):
    mf=DOC/'research/GAS_P0_INPLACE_MANIFEST.json';m=document(mf)
    check('actual GAS in-place source manifest',m.get('status')=='PASS' and m.get('phase')=='GASInPlaceApply')
    source=next(a for a in m['assets'] if package(a['path'])==IDLE)
    protect_file(disk(IDLE),source['sha256']);R['inputs']['idle_inplace_manifest']=binding(mf)
    source_manifest=DOC/'research/GAS_P0_SOURCE_MANIFEST.json';sm=document(source_manifest)
    check('actual P0 source manifest',sm.get('status')=='PASS')
    for key in ('SKM_GAS_UEFN_P0','SK_GAS_UEFN_P0'):
        row=next(a for a in sm['assets'] if package(a['path'])==P0+'/'+key);protect_file(disk(row['path']),row['sha256'])
    R['inputs']['gas_source_manifest']=binding(source_manifest)
    phase=('GASRecoveryRetarget' if LETTER in ('Q','R') else 'NPCGetUpRetargetR2')+LETTER
    file,retarget=latest_pass('author_*_'+phase)
    check('retarget matches exact current integration specimen',retarget['inputs']['npc_integration']['specimen']==R['baseline_specimen']
          and retarget['inputs']['npc_integration']['sha256']==R['inputs']['NPC_'+LETTER]['sha256'])
    rt_rows=[a for a in retarget['assets'] if a['class_name']=='IKRetargeter']
    check('one preserved successful GAS target retargeter',len(rt_rows)==1 and retarget.get('preservation',{}).get('changed_files')==[])
    row=rt_rows[0];protect_file(disk(row['path']),row['sha256']);R['inputs']['getup_retarget']=binding(file)
    idle=load(IDLE);source_mesh=load(P0+'/SKM_GAS_UEFN_P0');mesh=load(spec['mesh']);rt=load(row['path'])
    rc=U.IKRetargeterController.get_controller(rt);side=U.RetargetSourceOrTarget
    check('existing retargeter actual source and target',rc.get_preview_mesh(side.SOURCE)==source_mesh and rc.get_preview_mesh(side.TARGET)==mesh
          and idle.get_editor_property('skeleton')==source_mesh.get_editor_property('skeleton')
          and mesh.get_editor_property('skeleton')!=idle.get_editor_property('skeleton'))
    for which,expected_mesh in ((side.SOURCE,source_mesh),(side.TARGET,mesh)):
        rig=rc.get_ik_rig(which);check('real rig mesh matches',rig is not None and U.IKRigController.get_controller(rig).get_skeletal_mesh()==expected_mesh)
        protect_file(disk(rig))
    protect_file(disk(mesh.get_editor_property('skeleton')))
    return idle,source_mesh,mesh,rt

def pose_audit(seq,source,mesh,profile):
    model=seq.get_editor_property('data_model_interface');source_model=source.get_editor_property('data_model_interface')
    count=int(model.get_number_of_keys());rate=model.get_frame_rate();src_rate=source_model.get_frame_rate()
    check('full original relaxed-loop timing',count==int(source_model.get_number_of_keys())==301
          and int(model.get_number_of_frames())==300 and float(model.get_play_length())==float(source_model.get_play_length())==10.
          and (rate.numerator,rate.denominator)==(src_rate.numerator,src_rate.denominator)==(30,1))
    check('compatible non-additive non-root-motion idle',seq.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
          and seq.get_editor_property('additive_anim_type')==U.AdditiveAnimationType.AAT_NONE
          and not seq.get_editor_property('enable_root_motion') and not seq.get_editor_property('force_root_lock'))
    human={str(k):str(v) for k,v in profile.get_editor_property('humanoid_bones').items()}
    bones=dict(root='Root',hips=human['hips'],head=human['head'],hand_l=human['leftHand'],hand_r=human['rightHand'],
               foot_l=human['leftFoot'],foot_r=human['rightFoot'])
    opts=U.AnimPoseEvaluationOptions()
    for k,v in dict(evaluation_type=U.AnimDataEvalType.RAW,should_retarget=True,optional_skeletal_mesh=mesh,
        extract_root_motion=False,incorporate_root_motion_into_pose=True,retrieve_additive_as_full_pose=True,evaluate_curves=True).items():opts.set_editor_property(k,v)
    rows=[]
    for frame in range(count):
        pose=P.get_anim_pose_at_frame(seq,frame,opts)
        if not P.is_valid(pose):raise RuntimeError('Invalid relaxed idle pose '+str(frame))
        vals={k:transform(P.get_bone_pose(pose,U.Name(v),U.AnimPoseSpaces.WORLD)) for k,v in bones.items()}
        if not all(math.isfinite(x) for t in vals.values() for a in t.values() for x in a):raise RuntimeError('Non-finite idle transform')
        rows.append(dict(frame=frame,bones=vals))
    roots=[r['bones']['root']['position_cm'] for r in rows]
    drift=max(math.dist(p,roots[0]) for p in roots)
    check('independent root remains planted through entire idle',drift<.01,drift)
    result=dict(path=package(seq),source=package(source),frames=count,frame_rate=[rate.numerator,rate.denominator],duration_seconds=10.,
        root_max_displacement_cm=drift,bone_mapping=bones,first=rows[0],last=rows[-1],
        component_space_position_range_cm={k:[max(r['bones'][k]['position_cm'][i] for r in rows)-min(r['bones'][k]['position_cm'][i] for r in rows) for i in range(3)] for k in bones})
    trajectory=OUT/'idle_pose_samples.json';trajectory.write_text(json.dumps(dict(**result,samples=rows),ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
    result['trajectory']=binding(trajectory);return result

def preserve_cdo(before,after,new_abp):
    expected=copy.deepcopy(before);expected['anim_class']=path(B.generated_class(new_abp))
    check('only final body anim class changes in original CDO contract',after==expected)

try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty original maps/content',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    check('bounded native graph bridge compiled',hasattr(U,'HCM5VS2NPCIdleEditor'))
    install_frozen_resolvers()
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([NPC_ROOT,P0,'/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS'],force_rescan=True)
    if LETTER!='J':
        proof_file,proof=latest_pass('author_*_NPCIdleR2JReload')
        check('J prototype actually passed before other source candidates',proof.get('fresh_process_disk_readback')=='PASS'
              and proof.get('preservation',{}).get('changed_source_files')==[] and len(proof.get('assets',[]))==4)
        R['inputs']['J_prototype']=dict(**binding(proof_file),commandlet=completed_process(proof_file))
    spec=npc_specimen(LETTER) if LETTER in ('Q','R') else cast_specimen(LETTER)
    R['baseline_specimen']=copy.deepcopy(spec)
    if LETTER=='Q':spec=select_warm_q(spec)
    R['source_specimen']=copy.deepcopy(spec)
    bp=load(spec['blueprint']);abp=load(spec['anim_blueprint']);profile=load(spec['profile'])
    before=cdo_snapshot(bp);node,space,graph=graph_player(abp);samples=samples_snapshot(node)
    check('source CDO uses actual source ABP',before['anim_class']==path(B.generated_class(abp)))
    check('source sample zero is original MM_Idle',samples[0]['animation'].endswith('/MM_Idle_NPC'+LETTER))
    idle,source_mesh,mesh,rt=input_assets(spec)
    check('three source samples all use target skeleton',all(load(row['animation']).get_editor_property('skeleton')==mesh.get_editor_property('skeleton') for row in samples))
    R.update(source_blueprint=package(bp),source_sha256=sha(disk(bp)),source_cdo=before,source_graph=graph,source_samples=samples,
             source_blendspace=package(space),source_animation_blueprint=package(abp),retargeter=package(rt),idle_source=package(idle))
    for file in disk(spec['mesh']).parents[1].rglob('*.uasset'):protect_file(file)
    for obj in (bp,abp,space,profile):protect_file(disk(obj))
    for file in (PROJECT/'Config').glob('*.ini'):protect_file(file)
    for file in (PROJECT/'Content/HarborCity').rglob('*.umap'):protect_file(file)
    if MODE=='Probe':
        R['reflection_probe']=dict(status='PASS',graph_nodes='native bounded bridge',blendspace_player_node=True,blend_space_property=True,sample_data=True,
            write_access='NOT_RUN; source objects never assigned during probe',retargeter_source_target='PASS')
        R['destination']=None
    else:
        if MODE=='Reload':
            file,prior=latest_pass('author_*_NPCIdleR2'+LETTER+'Apply')
            check('exact candidate schema and four saved assets',prior.get('schema')==R['schema'] and prior.get('sample')==LETTER
                  and len(prior['assets'])==4 and prior['preservation']['changed_source_files']==[])
            R['inputs']['candidate_evidence']=binding(file);DEST=prior['destination'];R['destination']=DEST
            check('exact frozen original CDO and input identities',prior['source_cdo']==before and prior['source_sha256']==R['source_sha256']
                  and prior['source_specimen']==spec and prior['source_graph']==graph and prior['source_samples']==samples)
            for row in prior['assets']:protect_file(disk(row['path']),row['sha256'])
            candidate=load(prior['candidate_blueprint']);new_abp=load(prior['specimen']['anim_blueprint'])
            new_space=load(prior['specimen']['blendspace']);new_idle=load(prior['specimen']['idle_animation'])
            R['assets']=prior['assets']
        else:
            file,probe=latest_pass('author_*_NPCIdleR2'+LETTER+'Probe')
            check('successful exact source reflection probe first',probe.get('reflection_probe',{}).get('status')=='PASS'
                  and probe['source_sha256']==R['source_sha256'] and probe['source_specimen']==spec and probe['source_samples']==samples)
            R['inputs']['probe_evidence']=binding(file)
            check('fresh private idle batch',not A.does_directory_exist(DEST))
            params=U.IKRetargetBatchOperationInputs();suffix='_NPC'+LETTER+'IdleR2'
            for k,v in dict(assets_to_retarget=[A.find_asset_data(IDLE)],source_mesh=source_mesh,target_mesh=mesh,ik_retarget_asset=rt,
                target_path=DEST,suffix=suffix,use_source_path=False,include_referenced_assets=False,overwrite_existing_files=False,retain_additive_flags=True).items():params.set_editor_property(k,v)
            outputs=[a.get_asset() for a in U.IKRetargetBatchOperation.run_batch_retarget(params)]
            check('one actual new target idle',len(outputs)==1 and isinstance(outputs[0],U.AnimSequence)
                  and package(outputs[0])==DEST+'/'+idle.get_name()+suffix)
            new_idle=outputs[0];new_idle.set_editor_property('enable_root_motion',False);new_idle.set_editor_property('force_root_lock',False)
            save(new_idle)
            new_space=duplicate(space,'BS_NPCIdleR2_'+LETTER)
            new_abp=duplicate(abp,'ABP_NPCIdleR2_'+LETTER);player,copied_space,copied_graph=graph_player(new_abp)
            check('duplicate retains original graph and source BS before edit',copied_graph==graph and copied_space==space)
            R['native_binding']=json.loads(U.HCM5VS2NPCIdleEditor.configure_private_idle(abp,new_abp,new_space,new_idle,True))
            check('native sample replacement and unchanged original graph wiring',R['native_binding'].get('status')=='PASS',R['native_binding'])
            save(new_space);save(new_abp)
            candidate=duplicate(bp,'BP_NPCIdleR2_'+LETTER);cdo=U.get_default_object(B.generated_class(candidate));body=cdo.get_component_by_class(U.SkeletalMeshComponent)
            cdo.modify();body.modify();body.set_editor_property('anim_class',B.generated_class(new_abp))
            check('compile private NPC blueprint',B.compile_blueprint(candidate));save(candidate)
        actual_node,actual_space,actual_graph=graph_player(new_abp);actual_samples=samples_snapshot(actual_node)
        R['native_binding_readback']=json.loads(U.HCM5VS2NPCIdleEditor.configure_private_idle(abp,new_abp,new_space,new_idle,False))
        check('native no-write complete graph/sample preservation readback',R['native_binding_readback'].get('status')=='PASS',R['native_binding_readback'])
        expected_samples=copy.deepcopy(samples);expected_samples[0]['animation']=package(new_idle)
        check('only sample zero animation changes',actual_samples==expected_samples)
        check('exact preserved graph inventory and new BS reference',actual_graph==graph and actual_space==new_space)
        after=cdo_snapshot(candidate);preserve_cdo(before,after,new_abp)
        check('new compatible ABP and BS skeletons',new_abp.get_editor_property('target_skeleton')==mesh.get_editor_property('skeleton')
              and new_space.get_editor_property('skeleton')==mesh.get_editor_property('skeleton'))
        R['idle_audit']=pose_audit(new_idle,idle,mesh,profile)
        R['specimen']=dict(spec,blueprint=package(candidate),generated_class=path(B.generated_class(candidate)),anim_blueprint=package(new_abp),
                           blendspace=package(new_space),idle_animation=package(new_idle))
        R.update(candidate_blueprint=package(candidate),candidate_generated_class=path(B.generated_class(candidate)),candidate_cdo=after,
                 candidate_graph=actual_graph,candidate_samples=actual_samples)
        if MODE=='Reload':
            check('fresh process exact saved candidate CDO/graph/samples',after==prior['candidate_cdo'] and actual_graph==prior['candidate_graph']
                  and actual_samples==prior['candidate_samples'] and R['specimen']==prior['specimen'])
            check('fresh process same raw idle sample trajectory',R['idle_audit']['trajectory']['sha256']==prior['idle_audit']['trajectory']['sha256'])
            R['fresh_process_disk_readback']='PASS'
        check('exact four private saved assets',len(R['assets'])==4 and all(package(a['path']).startswith(DEST+'/') for a in R['assets']))
    check('no original map writes',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(source_sha256_before=PROTECTED,files_checked=len(PROTECTED),changed_source_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('NPC idle candidate failed; preserve native failure and unique batch')
