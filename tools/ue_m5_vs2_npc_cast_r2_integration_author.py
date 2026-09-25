"""Native approved six-source cast R2 generator. Root runs NPCIntegrationR2J/T/U/V/W/X in separate processes.

Creates fresh M5VS2/NPC runtime assets, using successful real import reports and
the installed UE 5.8 IK retarget APIs. No map, default-map or old NPC asset edit.
The imported NEW skeleton receives source slot memberships after an E: backup.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
ROOT = '/Game/HarborCity/M5VS2/NPC'
OWNER = 'HarborCity_M5_VS2_NPC_Integration'
KEY = 'HarborCityOwnedBy'
IMPORT_OWNER = 'HarborCity_M5_VS2_NPC_OfficialSamples'
SOURCE_MESH = '/Game/HarborCity/M3/Appearance/SK_M3_Quinn_HarborNavy'
SOURCE_SKELETON = '/Game/Characters/Mannequins/Meshes/SK_Mannequin'
SOURCE_CLIPS = ['/Game/Characters/Mannequins/Anims/Unarmed/' + n for n in
                ('MM_Idle', 'Walk/MF_Unarmed_Walk_Fwd', 'Jog/MF_Unarmed_Jog_Fwd')]
SOURCE_HITS = ['/Game/HarborCity/M4/Animation/AM_M4_Hit' + d for d in ('Front', 'Back', 'Left', 'Right')]
SOURCE_COUNTER = '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01'
A = U.EditorAssetLibrary
AT = U.AssetToolsHelpers.get_asset_tools()
B = U.BlueprintEditorLibrary
CMD = U.SystemLibrary.get_command_line()
matches = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', CMD)
assert len(matches) == 1
OUT = Path(matches[0][0] or matches[0][1]).resolve()
assert OUT.is_relative_to(DOC.resolve()) and not (OUT / 'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
PROTECTED={}
ALLOWED_SKELETON=None
CAST={
    'J':dict(name='绫音',role='HarborShrineKeeper',age=27,reaction='FLEE'),
    'T':dict(name='阿隼',role='HarborDockmaster',age=42,reaction='DEFEND_AGAINST_MELEE'),
    'U':dict(name='露希',role='HarborFlorist',age=29,reaction='FLEE'),
    'V':dict(name='诺亚',role='HarborArchivist',age=36,reaction='DEFEND_AGAINST_MELEE'),
    'W':dict(name='春乃',role='HarborTeaKeeper',age=31,reaction='FLEE'),
    'X':dict(name='纱月',role='HarborFishmonger',age=33,reaction='FLEE'),
}
R = dict(status='RUNNING', checks=[], assets=[], started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         runtime='NOT_RUN', visual='NOT_RUN_NULLRHI', ragdoll='NOT_RUN', dialogue_hook='EXPLICIT_SPEAKER_CALL_REQUIRED',
         scope='One independent adult cast specimen per process; Q/R and all other models preserved. No map placement or NPC replacement; naturalness/adult appearance remain USER_REVIEW.')


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, default=str), encoding='utf-8')


def check(label, ok, observed=None):
    R['checks'].append(dict(name=label, status='PASS' if ok else 'FAIL', observed=observed)); dump()
    if not ok: raise RuntimeError(label + ': ' + str(observed))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''): h.update(chunk)
    return h.hexdigest()


def path_of(obj): return obj.get_path_name().split('.')[0]
def disk(path): return PROJECT / 'Content' / Path(path.split('.')[0].removeprefix('/Game/')).with_suffix('.uasset')
def key(value): return str(value).casefold()


def load(path):
    obj = A.load_asset(path); check('load ' + path, obj is not None); return obj


def create(path, cls, factory):
    check('fresh confined asset ' + path, path.startswith(DEST + '/') and not A.does_asset_exist(path) and not disk(path).exists())
    folder, name = path.rsplit('/', 1)
    obj = AT.create_asset(name, folder, cls, factory)
    check('native create ' + path, obj is not None); A.set_metadata_tag(obj, KEY, OWNER); return obj


def save(obj):
    path = path_of(obj)
    check('save only new runtime namespace', path.startswith(DEST + '/'), path)
    A.set_metadata_tag(obj, KEY, OWNER)
    check('native save ' + path, A.save_loaded_asset(obj, False))
    check('saved native package exists', disk(path).is_file(), path)
    R['assets'].append(dict(path=path, class_name=obj.get_class().get_name(), bytes=disk(path).stat().st_size, sha256=sha(disk(path))))
    dump()


def successful_import(letter):
    for p in sorted((DOC / 'editor_runtime').glob('author_*NPCImport*/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(p.read_text(encoding='utf-8'))
        if data.get('status')!='PASS' or data.get('preservation',{}).get('changed_files')!=[]:continue
        for model in data.get('models', []):
            # Q completed and saved before the later R failure in the combined run.
            if model.get('sample') == letter and model.get('status') == 'PASS' and model.get('assets_saved'):
                check('source VRM hash matches successful native import', sha(model['source']['path']) == model['source']['sha256'])
                check('import morphs and real groups exist', bool(model['native']['mesh']['morph_target_names']) and bool(model['native']['meta']['expression_groups']))
                R['import_evidence'] = str(p); return model
    raise RuntimeError('No saved successful native import for ' + letter)


def repair_locomotion(clips, mesh, rtg):
    # Preflight every frame of all three clips before changing any clip.
    pairs = [(load(p), target) for p, target in zip(SOURCE_CLIPS, clips)]
    R['locomotion_in_place'] = []
    for source, target in pairs:
        result = json.loads(U.HCM5VS2NPCEditor.repair_npc_horizontal_root_travel(source, target, mesh, rtg, False))
        R['locomotion_in_place'].append(dict(target=target.get_path_name(), preflight=result)); dump()
        check('full-frame locomotion preflight '+target.get_name(), result.get('status')=='PASS', result)
    for (source, target), row in zip(pairs, R['locomotion_in_place']):
        if row['preflight']['needs_repair']:
            result = json.loads(U.HCM5VS2NPCEditor.repair_npc_horizontal_root_travel(source, target, mesh, rtg, True))
            row['apply'] = result; dump()
            check('native horizontal-only track repair '+target.get_name(), result.get('status')=='PASS' and result.get('applied'), result)
            save(target)
        result = json.loads(U.HCM5VS2NPCEditor.repair_npc_horizontal_root_travel(source, target, mesh, rtg, False))
        row['after_native_save_or_unchanged'] = result; dump()
        check('all-frame in-place readback '+target.get_name(), result.get('status')=='PASS' and not result.get('needs_repair'), result)
    R['locomotion_scope'] = 'Native all-frame trajectory and track preservation readback; disk saves hashed. Fresh-process movement/stop and visual acceptance NOT_RUN.'


def repair_existing():
    global DEST, ALLOWED_SKELETON
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT.resolve())
    check('no dirty maps', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    reports = sorted((DOC/'editor_runtime').glob('author_*_NPCIntegration'+LETTER+'/author_result.json'), key=lambda p:p.stat().st_mtime, reverse=True)
    evidence = next((p for p in reports if json.loads(p.read_text(encoding='utf-8')).get('status')=='PASS'), None)
    check('successful existing NPC integration evidence', evidence is not None)
    prior = json.loads(evidence.read_text(encoding='utf-8')); DEST = prior['destination']
    check('confined existing runtime', re.fullmatch(re.escape(ROOT+'/AvatarSample_'+LETTER+'/Runtime_')+r'[0-9a-f]{10}',DEST) is not None)
    R.update(destination=DEST, sample=LETTER, integration_evidence=str(evidence), specimen=prior['specimen'])
    expected = {item['path']:item['sha256'] for item in prior['assets']}
    target_paths = [DEST+'/Animation/'+p.rsplit('/',1)[1]+'_NPC'+LETTER for p in SOURCE_CLIPS]
    for path in target_paths+[DEST+'/RTG_QuinnToVRoid']:
        check('existing integration asset hash '+path, path in expected and sha(disk(path))==expected[path])
    clips = [load(p) for p in target_paths]
    rtg = load(DEST+'/RTG_QuinnToVRoid'); mesh = load(prior['specimen']['mesh'])
    for obj in clips+[rtg]: check('integration ownership '+obj.get_name(), A.get_metadata_tag(obj,KEY)==OWNER)
    target_sk = mesh.get_editor_property('skeleton')
    protected = SOURCE_CLIPS+[SOURCE_MESH,SOURCE_SKELETON,path_of(mesh),path_of(target_sk)]
    protected += [p for p in A.list_assets(DEST,recursive=True,include_folder=False) if p.split('.')[0] not in target_paths]
    before = {str(disk(p)):sha(disk(p)) for p in protected}
    config_hash = sha(PROJECT/'Config/DefaultEngine.ini')
    backup = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/OUT.name
    backup.mkdir(parents=True,exist_ok=False)
    R['animation_backups'] = []
    for path in target_paths:
        src = disk(path); dst = backup/src.name; shutil.copy2(src,dst)
        check('exact target animation backup '+src.name,sha(dst)==sha(src))
        R['animation_backups'].append(dict(path=path,backup=str(dst),sha256=sha(dst)))
    dump(); repair_locomotion(clips,mesh,rtg)
    check('all source/skeleton/mesh/other runtime assets preserved',all(sha(p)==h for p,h in before.items()))
    check('map config unchanged',sha(PROJECT/'Config/DefaultEngine.ini')==config_hash)
    check('no maps written',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status']='PASS'; R['pass_scope']='Only evidenced existing Q/R locomotion animations repaired and saved; no skeleton, mesh, graph, Blueprint or map changes. Runtime movement/stop regression NOT_RUN.'


def rig(path, mesh, root, chains, source):
    obj = create(path, U.IKRigDefinition, U.IKRigDefinitionFactory())
    c = U.IKRigController.get_controller(obj)
    check('rig mesh ' + path, c.set_skeletal_mesh(mesh))
    check('rig pelvis ' + path, c.set_retarget_root(root))
    for name, sa, sb, ta, tb in chains:
        start, end = (sa, sb) if source else (ta, tb)
        check('native chain ' + name, key(c.add_retarget_chain(name, start, end, 'None')) == key(name))
        check('chain bone readback ' + name, key(c.get_retarget_chain_start_bone(name)) == key(start) and key(c.get_retarget_chain_end_bone(name)) == key(end))
    save(obj); return obj


def integrate():
    global DEST, ALLOWED_SKELETON
    license_file=DOC/'research/NPC_CAST_R2_LICENSE_LEDGER.json'
    check('exact user decision ledger',sha(license_file)=='30aab61772bb44c0de26c40016d6e1a2b525e714404ee435b96c5a8f17cb55ca')
    license_data=json.loads(license_file.read_text(encoding='utf-8'))
    check('ordinary fictional combat interpretation accepted separately',license_data['user_decision']['status']=='ACCEPTED')
    R['license_ledger']=dict(path=str(license_file),sha256=sha(license_file),user_decision=license_data['user_decision'],source_violence_clause=license_data['fictional_violence'])
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT.resolve())
    check('no dirty maps', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    model = successful_import(LETTER)
    R['original_integration_template']=dict(path=str(WORK/'tools/ue_m5_vs2_npc_integration_author.py'),sha256='b17d382f598ac1a4356b277c1a3fbf39966a2d0968a71d63b3667aea12829eff')
    check('reviewed original generator unchanged',sha(WORK/'tools/ue_m5_vs2_npc_integration_author.py')==R['original_integration_template']['sha256'])
    for base in (PROJECT/'Content/HarborCity/M5VS2/NPC',PROJECT/'Plugins/VRM4U/Content'):
        for file in base.rglob('*.uasset'):PROTECTED[str(file.resolve())]=sha(file)
    for file in (PROJECT/'Config').glob('*.ini'):PROTECTED[str(file.resolve())]=sha(file)
    R['protected_sha256_before']=dict(PROTECTED)
    native=model['native'];human=native['meta']['humanoid']
    reference={b['name']:b['component_translation_cm'] for b in native['mesh']['reference_bones']}
    def role(name):return reference[human[name]]
    def difference(a,b):return [x-y for x,y in zip(role(a),role(b))]
    def unit(v):
        length=math.sqrt(sum(x*x for x in v));check('nondegenerate measured basis',length>.0001,v)
        return [x/length for x in v]
    up=unit(difference('head','hips'))
    toes=[a+b for a,b in zip(difference('leftToes','leftFoot'),difference('rightToes','rightFoot'))]
    forward=unit([toes[0],toes[1],0])
    R['individual_axes']=dict(up=up,toe_forward=forward,
        rationale='Current native head/eye graph and -90 mesh yaw require actual +Y forward / +Z up; no per-model guessed axis.')
    check('actual individual humanoid basis supports current native graph',up[2]>.98 and forward[1]>.98,R['individual_axes'])
    R['license_scope']=dict(source_terms=model['source']['raw_metadata'],original_flags_unchanged=True,
        combat='NOT_RUN_AUTHOR_ONLY; ordinary fictional combat user acceptance recorded separately; metadata is not authorization')
    for row in model['assets_saved']:
        check('exact imported input asset before first integration',sha(disk(row['path']))==row['sha256'],row['path'])

    suffix = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:10]
    DEST = ROOT + '/AvatarSample_' + LETTER + '/Runtime_' + suffix
    R['destination'] = DEST; R['sample'] = LETTER
    check('fresh entire runtime directory', not A.does_directory_exist(DEST) and not disk(DEST + '/placeholder').parent.exists())
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT, '/Game/Characters/Mannequins', '/Game/HarborCity/M4/Animation', '/Game/HarborCity/M3/Appearance'], force_rescan=True)
    mesh = load(model['native']['mesh']['path'])
    target_sk = mesh.get_editor_property('skeleton')
    check('new imported source ownership', A.get_metadata_tag(mesh, KEY) == IMPORT_OWNER and A.get_metadata_tag(target_sk, KEY) == IMPORT_OWNER)
    source_mesh, source_sk = load(SOURCE_MESH), load(SOURCE_SKELETON)
    source_assets = [source_mesh, source_sk] + [load(p) for p in SOURCE_CLIPS + SOURCE_HITS + [SOURCE_COUNTER]]
    old_hashes = {str(disk(path_of(o))): sha(disk(path_of(o))) for o in source_assets}
    old_config = sha(PROJECT / 'Config/DefaultEngine.ini')
    check('independent actual target skeleton', source_sk != target_sk)

    # Retargeted montages require the source slot membership on the NEW imported skeleton.
    # Preserve its bytes first; metadata ownership remains the native-import owner.
    backup = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / OUT.name
    backup.mkdir(parents=True, exist_ok=False)
    target_sk_file = disk(path_of(target_sk))
    shutil.copy2(target_sk_file, backup / target_sk_file.name)
    before_sk = sha(target_sk_file)
    ALLOWED_SKELETON=str(target_sk_file.resolve())
    check('source-slot memberships copied only to new NPC skeleton', U.HCM5VS2NPCEditor.copy_npc_slots(source_sk, target_sk))
    check('new skeleton slot save', A.save_loaded_asset(target_sk, False))
    R['necessary_new_asset_migration'] = dict(skeleton=target_sk.get_path_name(), backup=str(backup / target_sk_file.name),
        before_sha256=before_sk, after_sha256=sha(target_sk_file), change='Copy source animation slot groups; original NPCs and source skeleton unchanged.')

    human = model['native']['meta']['humanoid']
    chains = [('Spine', 'spine_01', 'spine_05', human['spine'], human.get('upperChest', human['chest'])),
              ('Neck', 'neck_01', 'neck_02', human['neck'], human['neck']),
              ('Head', 'head', 'head', human['head'], human['head'])]
    for side in ('left', 'right'):
        low = side[0]; name = side.title()
        chains += [(name+'Clavicle', 'clavicle_'+low, 'clavicle_'+low, human[side+'Shoulder'], human[side+'Shoulder']),
                   (name+'Arm', 'upperarm_'+low, 'hand_'+low, human[side+'UpperArm'], human[side+'Hand']),
                   (name+'Leg', 'thigh_'+low, 'foot_'+low, human[side+'UpperLeg'], human[side+'Foot']),
                   (name+'Toe', 'ball_'+low, 'ball_'+low, human[side+'Toes'], human[side+'Toes'])]
        for sf, tf in (('thumb', 'Thumb'), ('index', 'Index'), ('middle', 'Middle'), ('ring', 'Ring'), ('pinky', 'Little')):
            chains.append((name+tf, sf+'_01_'+low, sf+'_03_'+low, human[side+tf+'Proximal'], human[side+tf+'Distal']))
    source_rig = rig(DEST+'/IK_SourceQuinn', source_mesh, 'pelvis', chains, True)
    target_rig = rig(DEST+'/IK_TargetVRoid', mesh, human['hips'], chains, False)
    rtg = create(DEST+'/RTG_QuinnToVRoid', U.IKRetargeter, U.IKRetargetFactory())
    rc = U.IKRetargeterController.get_controller(rtg)
    rc.remove_all_ops(); src, dst = U.RetargetSourceOrTarget.SOURCE, U.RetargetSourceOrTarget.TARGET
    rc.set_ik_rig(src, source_rig); rc.set_ik_rig(dst, target_rig)
    rc.set_preview_mesh(src, source_mesh); rc.set_preview_mesh(dst, mesh)
    pelvis = rc.add_retarget_op('/Script/IKRig.IKRetargetPelvisMotionOp')
    fk = rc.add_retarget_op('/Script/IKRig.IKRetargetFKChainsOp')
    check('native pelvis and FK retarget ops', pelvis >= 0 and fk >= 0)
    for index in (pelvis, fk): rc.run_op_initial_setup(index)
    pc = rc.get_op_controller(pelvis); pc.set_source_pelvis_bone('pelvis'); pc.set_target_pelvis_bone(human['hips'])
    for name, *_ in chains: check('explicit FK map '+name, rc.set_source_chain(name, name, rc.get_op_name(fk)))
    fc = rc.get_op_controller(fk); settings = fc.get_settings(); values = list(settings.get_editor_property('chains_to_retarget'))
    for chain in values:
        chain.set_editor_property('enable_fk', True)
        chain.set_editor_property('rotation_mode', U.FKChainRotationMode.INTERPOLATED)
        chain.set_editor_property('translation_mode', U.FKChainTranslationMode.NONE)
    settings.set_editor_property('chains_to_retarget', values); fc.set_settings(settings)
    pose = 'VRoid_QuinnAlignment'; rc.create_retarget_pose(pose, dst)
    check('target retarget pose selected', rc.set_current_retarget_pose(pose, dst))
    rc.auto_align_all_bones(dst, U.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(rtg)
    inputs = U.IKRetargetBatchOperationInputs()
    fields = dict(assets_to_retarget=[A.find_asset_data(p) for p in SOURCE_CLIPS+SOURCE_HITS+[SOURCE_COUNTER]], source_mesh=source_mesh,
        target_mesh=mesh, ik_retarget_asset=rtg, target_path=DEST+'/Animation', suffix='_NPC'+LETTER,
        use_source_path=False, include_referenced_assets=True, overwrite_existing_files=False, retain_additive_flags=True)
    for name, value in fields.items(): inputs.set_editor_property(name, value)
    results = list(U.IKRetargetBatchOperation.run_batch_retarget(inputs))
    check('native retarget outputs', len(results) >= 8, len(results))
    for item in results:
        obj = item.get_asset(); check('retargeted animation skeleton', obj and obj.get_editor_property('skeleton') == target_sk)
        if isinstance(obj, U.AnimSequence):
            obj.set_editor_property('enable_root_motion', False); obj.set_editor_property('force_root_lock', False)
        save(obj)
    clips = [load(DEST+'/Animation/'+p.rsplit('/',1)[1]+'_NPC'+LETTER) for p in SOURCE_CLIPS]
    repair_locomotion(clips, mesh, rtg)
    hits = [load(DEST+'/Animation/'+p.rsplit('/',1)[1]+'_NPC'+LETTER) for p in SOURCE_HITS]
    counter = load(DEST+'/Animation/'+SOURCE_COUNTER.rsplit('/',1)[1]+'_NPC'+LETTER)

    factory = U.DataAssetFactory(); factory.set_editor_property('data_asset_class', U.HCM5VS2NPCProfile)
    profile = create(DEST+'/DA_NPCProfile', U.HCM5VS2NPCProfile, factory)
    profile.set_editor_property('mesh', mesh); profile.set_editor_property('source_sha256', model['source']['sha256'])
    profile.set_editor_property('metadata_asset_path', model['native']['meta']['path'])
    profile.set_editor_property('humanoid_bones', {U.Name(k): U.Name(v) for k,v in human.items()})
    morphs = set(model['native']['mesh']['morph_target_names']); groups = []; omissions = []
    for group in model['native']['meta']['expression_groups']:
        # Neutral is the base geometry. R's zero-delta neutral is legitimately omitted by the native importer.
        if group['name'] == 'Neutral':
            omissions.append(dict(group='Neutral', reason='Neutral uses zero owned weights / base geometry.')); continue
        binds = []
        for bind in group['binds']:
            check('real imported expression morph '+bind['morph_target_name'], bind['morph_target_name'] in morphs)
            value = U.HCM5VS2NPCMorphWeight()
            value.set_editor_property('morph', U.Name(bind['morph_target_name']))
            value.set_editor_property('weight', float(bind['weight']) / 100.)
            binds.append(value)
        value = U.HCM5VS2NPCFacePose(); value.set_editor_property('group', U.Name(group['name'])); value.set_editor_property('binds', binds); groups.append(value)
    profile.set_editor_property('face_groups', groups); save(profile)
    R['face_mapping'] = dict(groups=[g['name'] for g in model['native']['meta']['expression_groups'] if g['name']!='Neutral'], omissions=omissions,
        derived_emotions={'Shy':'Fun x 0.4', 'Serious':'Angry x 0.35', 'Pain':'Angry x hit intensity x 0.7'},
        dialogue='SetDialogueTextProgress called by actual speaker; five group-mapped vowels, no phoneme recognition.')
    af = U.AnimBlueprintFactory(); af.set_editor_property('target_skeleton', target_sk)
    af.set_editor_property('preview_skeletal_mesh', mesh); af.set_editor_property('parent_class', U.HCM5VS2NPCAnimInstance)
    abp = create(DEST+'/ABP_NPC', U.AnimBlueprint, af)
    native = json.loads(U.HCM5VS2NPCEditor.build_npc_presentation(profile, abp, *clips, DEST))
    R['native_presentation'] = native; dump(); check('native NPC animation and humanoid physics author', native.get('status')=='PASS', native)
    physics = load(native['physics_asset']); space = load(native['blendspace'])
    # Fresh asset only: reuse the audited anatomical method with this skeleton's
    # actual skin points, bone landmarks, mass, frames, limits and collision pairs.
    probe=json.loads(U.HCM5VS2NPCEditor.inspect_or_repair_npc_physics(profile,physics,False))
    R['individual_physics_preflight']=probe;dump()
    check('per-model anatomical PHYS plan valid',probe.get('status')=='PASS' and probe.get('candidate_valid') is True,probe)
    applied=json.loads(U.HCM5VS2NPCEditor.inspect_or_repair_npc_physics(profile,physics,True))
    R['individual_physics_apply']=applied;dump()
    check('fresh per-model PHYS native apply and exact readback',applied.get('status')=='PASS' and applied.get('applied') is True
        and applied.get('native_candidate_readback_checked') is True,applied)

    for obj in (space, physics, abp): save(obj)
    bf = U.BlueprintFactory(); bf.set_editor_property('parent_class', U.HCM5VS2NPC)
    bp = create(DEST+'/BP_AvatarSample_'+LETTER, U.Blueprint, bf)
    check('new NPC Blueprint compiles', B.compile_blueprint(bp))
    cdo = U.get_default_object(B.generated_class(bp)); cdo.modify()
    body = cdo.get_component_by_class(U.SkeletalMeshComponent); body.modify()
    check('own NPC mesh component', path_of(body)==path_of(bp), body.get_path_name())
    body.set_skeletal_mesh_asset(mesh)
    body.set_editor_property('animation_mode', U.AnimationMode.ANIMATION_BLUEPRINT)
    body.set_editor_property('anim_class', B.generated_class(abp))
    body.set_physics_asset(physics, True)
    half = max(80., min(98., model['native']['mesh']['reference_height_cm'] / 2.))
    floor_z = model['native']['mesh']['bounds_origin_cm'][2] - model['native']['mesh']['bounds_extent_cm'][2]
    body.set_editor_property('relative_location', U.Vector(0,0,-half-floor_z))
    body.set_editor_property('relative_rotation', U.Rotator(pitch=0,yaw=-90,roll=0))
    body.set_editor_property('relative_scale3d', U.Vector(1,1,1))
    capsule = cdo.get_component_by_class(U.CapsuleComponent); capsule.modify(); capsule.set_capsule_size(32.,half,False)
    cdo.set_editor_property('npc_profile', profile)
    cdo.set_editor_property('stable_id', U.Name('M5VS2_'+LETTER))
    cdo.set_editor_property('display_name', CAST[LETTER]['name'])
    cdo.set_editor_property('role_id', U.Name(CAST[LETTER]['role']))
    cdo.set_editor_property('talkable', True); cdo.set_editor_property('stationary', True)
    cdo.set_editor_property('dialogue_lines', ['海风停的时候，港町也会慢下来。'])
    cdo.set_editor_property('walk_speed',140.); cdo.set_editor_property('panic_speed',280.)
    reaction = cdo.get_component_by_class(U.HCM4R1ReactionComponent); reaction.modify()
    check('own NPC reaction component', path_of(reaction)==path_of(bp), reaction.get_path_name())
    reaction.set_editor_property('counter_animation', counter)
    enum_class = type(reaction.get_editor_property('reaction_preference'))
    choice = CAST[LETTER]['reaction']
    check('native reflected reaction preference', hasattr(enum_class, choice), choice)
    reaction.set_editor_property('reaction_preference', getattr(enum_class, choice))
    for direction, montage in zip(('front','back','left','right'), hits): cdo.set_editor_property('hit_'+direction+'_montage',montage)
    # GetPhysicsAsset is native C++ only in UE 5.8; the serialized reflected override is readable in Python.
    check('NPC component physics binding', body.get_editor_property('physics_asset_override')==physics)
    check('NPC profile CDO binding', cdo.get_editor_property('npc_profile')==profile)
    save(bp)
    R['specimen'] = dict(blueprint=bp.get_path_name(), generated_class=B.generated_class(bp).get_path_name(),
        mesh=mesh.get_path_name(), profile=profile.get_path_name(), anim_blueprint=abp.get_path_name(), physics=physics.get_path_name(),
        capsule_half_height_cm=half, mesh_z_cm=-half-floor_z, fictional_age=CAST[LETTER]['age'],
        age_scope='Project characterization, not an official sample age claim', spring='Original imported post-process AnimBP retained',
        counter_animation=counter.get_path_name(), reaction_preference=choice,
        placement='NOT_RUN; Blueprint defaults stationary, map author must supply actual region/patrol for walking navigation.')
    check('old animation/mesh/skeleton bytes preserved', all(sha(path)==value for path,value in old_hashes.items()))
    check('default map config unchanged', sha(PROJECT/'Config/DefaultEngine.ini')==old_config)
    check('no maps written', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status']='PASS'
    R['pass_scope']='Native author/save, mapped face profile, compatible retargeted clips/montages and compiled graph only. Native runtime facial changes, adult visual review, collisions, spring motion, ragdoll recovery and dialogue remain NOT_RUN.'

try:
    phase = re.findall(r'(?:^|\s)-M5AuthorPhase=(\S+)', CMD)
    check('run a single specimen per native process', len(phase)==1 and phase[0] in tuple('NPCIntegrationR2'+k for k in CAST),phase)
    LETTER = phase[0][-1]
    integrate()
except Exception:
    R['status']='FAIL'; R['error']=traceback.format_exc(); U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if p!=ALLOWED_SKELETON and (not Path(p).is_file() or sha(p)!=h)]
    R['preservation']=dict(files_checked=len(PROTECTED),allowed_skeleton=ALLOWED_SKELETON,unexpected_changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status']!='PASS': raise RuntimeError('NPC specimen author failed; see author_result.json')
