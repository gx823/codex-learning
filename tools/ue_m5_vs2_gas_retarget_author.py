"""Five exact native GAS P0 clips -> existing normalized Selestia skeleton.

No AnimBP/map/gameplay binding, compatibility declaration, old helper mutation,
source animation overwrite, or combat/first-person animation reprocessing.
Prerequisite: actual seven-package clean-source and five in-place manifests.
"""
from pathlib import Path
import datetime
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
CONTENT = WORK / 'HarborCity/Content'
DOC = WORK / 'docs/HarborCity_M5_VS2'
SOURCE_ROOT = '/Game/HarborCity/M5VS2/GASSourceP0'
ROOT = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS'
DEST = ROOT + '/Retargeted'
RIGS = ROOT + '/Rigs'
SOURCE_MESH = SOURCE_ROOT + '/SKM_GAS_UEFN_P0'
SOURCE_SKELETON = SOURCE_ROOT + '/SK_GAS_UEFN_P0'
TARGET_MESH = '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia'
TARGET_SKELETON = '/Game/HarborCity/M5VS1/HeroSelestia/SK_Selestia'
OWNER = 'HarborCity_M5_VS2_GAS_P0_v1'
KEY = 'HarborCityOwnedBy'
MANIFEST = DOC / 'research/GAS_P0_RETARGET_MANIFEST.json'
A = U.EditorAssetLibrary
AT = U.AssetToolsHelpers.get_asset_tools()
matches = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
if len(matches) != 1:
    raise RuntimeError('Exactly one fresh M5EvidenceDir required')
OUT = Path(matches[0][0] or matches[0][1]).resolve()
if not OUT.is_relative_to(DOC.resolve()) or (OUT / 'author_result.json').exists():
    raise RuntimeError('Fresh VS2 evidence required')
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), checks=[], assets=[],
         native_target_audits=[], source_preservation={}, gameplay_binding='NOT_RUN', rendered_pose='NOT_RUN',
         user_naturalness='USER_REVIEW', fresh_process_reload='NOT_RUN', map_writes=0)
protected = {}


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, good, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if good else 'FAIL', observed=observed))
    dump()
    if not good:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for b in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def disk(package):
    return CONTENT / Path(package.removeprefix('/Game/').split('.')[0]).with_suffix('.uasset')


def path(obj):
    return obj.get_path_name().split('.')[0]


def load(package):
    obj = A.load_asset(package)
    check('load actual ' + package, obj is not None)
    return obj


def save(obj):
    package = path(obj)
    check('save only new P0 rigs/output clips', package.startswith((RIGS + '/', DEST + '/')))
    A.set_metadata_tag(obj, KEY, OWNER)
    check('native package save ' + package, A.save_loaded_asset(obj, False))
    check('actual persisted file ' + package, disk(package).is_file())
    R['assets'] = [x for x in R['assets'] if x['path'] != package] + [dict(path=package, class_name=obj.get_class().get_name(), bytes=disk(package).stat().st_size, sha256=sha(disk(package)))]
    dump()


def create(package, cls, factory):
    check('no existing package overwritten ' + package, not A.does_asset_exist(package) and not disk(package).exists())
    folder, name = package.rsplit('/', 1)
    obj = AT.create_asset(name, folder, cls, factory)
    check('native create ' + package, obj is not None)
    return obj


CHAINS = [('Spine', 'spine_01', 'spine_05', 'Spine', 'Chest'),
          ('Neck', 'neck_01', 'neck_02', 'Neck', 'Neck'), ('Head', 'head', 'head', 'Head', 'Head')]
for side, suffix in [('Left', 'L'), ('Right', 'R')]:
    low = suffix.lower()
    CHAINS += [(side + 'Clavicle', 'clavicle_' + low, 'clavicle_' + low, 'Shoulder_' + suffix, 'Shoulder_' + suffix),
               (side + 'Arm', 'upperarm_' + low, 'hand_' + low, 'UpperArm_' + suffix, 'Hand_' + suffix),
               (side + 'Leg', 'thigh_' + low, 'foot_' + low, 'UpperLeg_' + suffix, 'Foot_' + suffix),
               (side + 'Toe', 'ball_' + low, 'ball_' + low, 'Toe_' + suffix, 'Toe_' + suffix)]
    for sf, tf in [('thumb', 'Thumb'), ('index', 'Index'), ('middle', 'Middle'), ('ring', 'Ring'), ('pinky', 'Little')]:
        CHAINS.append((side + tf, sf + '_01_' + low, sf + '_03_' + low, tf + 'Proximal_' + suffix, tf + 'Distal_' + suffix))


def make_rig(package, mesh, pelvis, source, bones):
    rig = create(package, U.IKRigDefinition, U.IKRigDefinitionFactory())
    c = U.IKRigController.get_controller(rig)
    check('rig uses observed mesh', c.set_skeletal_mesh(mesh))
    check('rig uses observed pelvis', pelvis.casefold() in bones and c.set_retarget_root(pelvis))
    for name, sa, sb, ta, tb in CHAINS:
        start, end = (sa, sb) if source else (ta, tb)
        check('real source/target chain bones ' + name, start.casefold() in bones and end.casefold() in bones, [start, end])
        check('native retarget chain ' + name, str(c.add_retarget_chain(name, start, end, 'None')).casefold() == name.casefold())
        check('native chain endpoints read back', str(c.get_retarget_chain_start_bone(name)).casefold() == start.casefold()
              and str(c.get_retarget_chain_end_bone(name)).casefold() == end.casefold())
    save(rig)
    return rig


try:
    check('current HarborCity project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == (WORK / 'HarborCity').resolve())
    check('fresh manifest', not MANIFEST.exists())
    source_manifest = json.loads((DOC / 'research/GAS_P0_SOURCE_MANIFEST.json').read_text(encoding='utf-8-sig'))
    inplace_manifest = json.loads((DOC / 'research/GAS_P0_INPLACE_MANIFEST.json').read_text(encoding='utf-8-sig'))
    check('actual prerequisite passes', source_manifest['status'] == 'PASS' and inplace_manifest['status'] == 'PASS'
          and len(source_manifest['assets']) == 7 and len(inplace_manifest['assets']) == 5)
    for manifest in (source_manifest, inplace_manifest):
        for row in manifest['assets']:
            check('source native manifest bytes ' + row['path'], disk(row['path']).is_file() and sha(disk(row['path'])) == row['sha256'])
            protected[str(disk(row['path']))] = row['sha256']
    for folder in ('HarborCity/M5VS1/HeroSelestia', 'HarborCity/M4R2/Animation', 'HarborCity/M4/Animation'):
        for file in (CONTENT / folder).rglob('*.uasset'):
            protected[str(file)] = sha(file)
    # Current opt-in BP/physics graph also remains untouched by this asset probe.
    for file in (CONTENT / 'HarborCity/M5VS2/HeroSelestia').rglob('*.uasset'):
        protected[str(file)] = sha(file)
    (OUT / 'protected_hashes.json').write_text(json.dumps(protected, ensure_ascii=False, indent=2), encoding='utf-8')
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([SOURCE_ROOT, ROOT, '/Game/HarborCity/M5VS1/HeroSelestia'], force_rescan=True)
    for folder in (RIGS, DEST):
        check('new private rig/output namespace is empty', not list(disk(folder + '/probe').parent.rglob('*.uasset')), folder)
    sm, tm = load(SOURCE_MESH), load(TARGET_MESH)
    ss, ts = load(SOURCE_SKELETON), load(TARGET_SKELETON)
    check('separate real skeleton bindings', sm.get_editor_property('skeleton') == ss and tm.get_editor_property('skeleton') == ts and ss != ts)
    source_ref = json.loads(U.HCM4AnimationEditor.inspect_animation_asset(ss))['reference_bones']
    target_ref = json.loads(U.HCM4AnimationEditor.inspect_animation_asset(ts))['reference_bones']
    check('target bone bind scale stays normalized', all(max(abs(float(v) - 1.) for v in b['local']['scale']) < .00001 for b in target_ref))
    source_names = {b['name'].casefold() for b in source_ref}
    target_names = {b['name'].casefold() for b in target_ref}
    inputs = [load(row['path']) for row in inplace_manifest['assets']]
    source_audits = {}
    for seq in inputs:
        check('exact native source skeleton', isinstance(seq, U.AnimSequence) and seq.get_editor_property('skeleton') == ss)
        audit = json.loads(U.HCM5VS2GASAnimationEditor.inspect_sequence_p0(seq))
        check('exact P0 native source audit', audit.get('status') == 'PASS')
        native = audit['sequence']
        root = [x for x in native['native_track_motion'] if x['bone'].casefold() == 'root']
        check('actual complete in-place floor root', len(root) == 1 and root[0]['keys'] == native['keys'] and root[0]['max_local_xy_displacement_cm'] < .0001)
        source_audits[seq.get_name()] = native
    sr = make_rig(RIGS + '/IK_GAS_UEFN_P0', sm, 'pelvis', True, source_names)
    tr = make_rig(RIGS + '/IK_GAS_Selestia_P0', tm, 'Hips', False, target_names)
    rt = create(RIGS + '/RTG_GAS_UEFN_To_Selestia_P0', U.IKRetargeter, U.IKRetargetFactory())
    rc = U.IKRetargeterController.get_controller(rt)
    rc.remove_all_ops()
    s, t = U.RetargetSourceOrTarget.SOURCE, U.RetargetSourceOrTarget.TARGET
    rc.set_ik_rig(s, sr); rc.set_ik_rig(t, tr)
    rc.set_preview_mesh(s, sm); rc.set_preview_mesh(t, tm)
    pi = rc.add_retarget_op('/Script/IKRig.IKRetargetPelvisMotionOp')
    fi = rc.add_retarget_op('/Script/IKRig.IKRetargetFKChainsOp')
    check('native pelvis and FK only', pi >= 0 and fi >= 0)
    rc.run_op_initial_setup(pi); rc.run_op_initial_setup(fi)
    pc = rc.get_op_controller(pi)
    pc.set_source_pelvis_bone('pelvis'); pc.set_target_pelvis_bone('Hips')
    check('pelvis operation actual binding', str(pc.get_source_pelvis_bone()).casefold() == 'pelvis' and str(pc.get_target_pelvis_bone()).casefold() == 'hips')
    for name, *_ in CHAINS:
        check('explicit native FK map ' + name, rc.set_source_chain(name, name, rc.get_op_name(fi)))
    fc = rc.get_op_controller(fi); settings = fc.get_settings()
    chains = list(settings.get_editor_property('chains_to_retarget'))
    check('all expected target chains', {str(c.target_chain_name).casefold() for c in chains} == {c[0].casefold() for c in CHAINS})
    for chain in chains:
        chain.set_editor_property('enable_fk', True)
        chain.set_editor_property('rotation_mode', U.FKChainRotationMode.INTERPOLATED)
        chain.set_editor_property('translation_mode', U.FKChainTranslationMode.NONE)
    settings.set_editor_property('chains_to_retarget', chains); fc.set_settings(settings)
    check('bone stretching disabled', all(c.translation_mode == U.FKChainTranslationMode.NONE for c in fc.get_settings().chains_to_retarget))
    pose = 'GAS_UEFN_Selestia_P0'
    rc.create_retarget_pose(pose, t)
    check('select separate target alignment pose', rc.set_current_retarget_pose(pose, t))
    rc.auto_align_all_bones(t, U.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    save(rt)
    bi = U.IKRetargetBatchOperationInputs()
    for key, value in dict(assets_to_retarget=[A.find_asset_data(path(x)) for x in inputs], source_mesh=sm, target_mesh=tm,
            ik_retarget_asset=rt, target_path=DEST, suffix='_SelestiaGAS', use_source_path=False,
            include_referenced_assets=False, overwrite_existing_files=False, retain_additive_flags=True).items():
        bi.set_editor_property(key, value)
    outputs = [x.get_asset() for x in U.IKRetargetBatchOperation.run_batch_retarget(bi)]
    expected = {DEST + '/' + x.get_name() + '_SelestiaGAS' for x in inputs}
    check('exactly five native sequence outputs and no framework', len(outputs) == 5 and all(isinstance(x, U.AnimSequence) for x in outputs) and {path(x) for x in outputs} == expected)
    for seq in outputs:
        check('actual target private skeleton', seq.get_editor_property('skeleton') == ts)
        seq.set_editor_property('enable_root_motion', False)
        seq.set_editor_property('force_root_lock', False)
        native = json.loads(U.HCM5VS2GASAnimationEditor.inspect_sequence_p0(seq))
        check('native target raw model available', native.get('status') == 'PASS')
        data = native['sequence']; source = source_audits[seq.get_name().removesuffix('_SelestiaGAS')]
        check('exact target timing preserved', all(data[k] == source[k] for k in ('frames', 'keys', 'fps_numerator', 'fps_denominator')))
        hips = [x for x in data['native_track_motion'] if x['bone'].casefold() == 'hips']
        check('bounded target pelvis without source forward travel', len(hips) == 1 and hips[0]['keys'] == data['keys'] and hips[0]['max_local_xy_displacement_cm'] < 40., hips)
        R['native_target_audits'].append(native)
        save(seq)
    check('only five clips plus three rigs persisted', len(R['assets']) == 8)
    R['status'] = 'PASS'
    R['pass_scope'] = 'Native five-clip IK retarget and persisted new assets only. No locomotion graph binding, foot-contact, finger-quality, packaged-runtime or user naturalness acceptance.'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc()
finally:
    changed = [p for p, h in protected.items() if not Path(p).is_file() or sha(p) != h]
    R['source_preservation'] = dict(files=len(protected), changed_files=changed, status='PASS' if protected and not changed else 'FAIL_OR_NOT_ESTABLISHED')
    if changed:
        R['status'] = 'FAIL'
    R['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    dump()
    if R['status'] == 'PASS':
        MANIFEST.write_text(json.dumps(dict(status='PASS', evidence=str(OUT), assets=R['assets']), ensure_ascii=False, indent=2), encoding='utf-8')
if R['status'] != 'PASS':
    raise RuntimeError('GAS five-clip retarget failed; preserve report and new attempt assets')
