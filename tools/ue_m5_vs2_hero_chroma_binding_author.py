"""Bind the reviewed Chroma_050 candidate to the one existing VS2 Hero BP.

Root runs HeroChromaBindingApply, then HeroChromaBindingVerify in a fresh native
commandlet process. Only Apply saves the exact BP after an E-drive byte backup.
Verify reloads the saved package in a different process; it does not save assets.
Neither phase renders or establishes final user/scene/night visual acceptance.
RecoveryVerify is a one-run, evidence-bound read-only recovery for a915d9f8's
post-save path-sort guard error. It never changes that original FAIL evidence.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import struct
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
TARGET = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia'
CLASS = TARGET + '.BP_M5VS2_Selestia_C'
HERO_OWNER = 'HarborCity_M5_VS2_HeroBinding'
CHROMA_OWNER = 'HarborCity_M5_VS2_HeroLightChroma'
KEY = 'HarborCityOwnedBy'
SELECTED = 'Chroma_050'
SLOTS = ('hair', 'body', 'face', 'option', 'costume')
AUTHOR = DOC / 'editor_runtime/author_20260924_152537_400_9e7e0df0_HeroLightChroma/author_result.json'
AUTHOR_SHA = 'e2ac7bf4e74cf8d766efc6f6766e45ae2bcb12aad6d8e435812d307012c1915e'
RUN = DOC / 'editor_runtime/20260924_152923_200_c9348576_herolightchroma_game'
RUNTIME = RUN / 'Lookdev_20260924_152940_3AE78A95/lookdev_results.json'
RUNTIME_SHA = '32133a15addae33fffdccddbcbcdb329a1c3c1c31cf7f6b0f728cb84342f7e19'
LAUNCH_SHA = 'aad277024b7c79cd3fdc41cd80224f8e7e41a4d973f4d62a37c9abd14a50388e'
BACKUPS = Path('E:/GameDev/Assets/HarborCity/M5_VS2/HeroSelestia/NativeBackups').resolve()
RECOVERY_RUN = DOC / 'editor_runtime/author_20260924_234211_851_a915d9f8_HeroChromaBindingApply'
RECOVERY_REPORT_SHA = '94b7737326862fa5649c54fa2a92d5a982662a8c2b06c09277a7ebbf8dd563c4'
RECOVERY_COMMANDLET_SHA = '0073d7b7b3774d52d2511d61a27f23137e473e0f0b8ebd785ef48c1efa4d4e93'
RECOVERY_SCRIPT_SHA = '85e1e011a40411a630e7817af95b040a607edaafb1d060abdc2bfe0b0f648ce0'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary


def argument(name):
    values = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(values) == 1, 'Exactly one ' + name + ' required'
    return values[0][0] or values[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
PHASE = argument('M5AuthorPhase')
assert OUT.is_relative_to(DOC / 'editor_runtime') and not (OUT / 'author_result.json').exists()
assert PHASE in ('HeroChromaBindingApply', 'HeroChromaBindingVerify', 'HeroChromaBindingRecoveryVerify')
APPLY = PHASE == 'HeroChromaBindingApply'
OUT.mkdir(parents=True, exist_ok=True)
R = dict(schema='HarborCity.M5VS2.HeroChromaBindingAuthor.v1', status='RUNNING', phase=PHASE,
         process_id=os.getpid(), checks=[], inputs={}, assets=[], transitions=[], applied=False,
         target=TARGET, selected_variant=SELECTED, runtime_after_binding='NOT_RUN',
         visual_acceptance='USER_REVIEW', night_proxy_acceptance='NOT_ACCEPTED',
         fresh_process_disk_readback='NOT_RUN', started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         scope='Only five Hero body override materials; no point light, ABP, GAS, movement, gameplay, map or source-material writes.',
         selection_basis='Root reviewed c9348576: Chroma_050 retains dusk warmth while improving pink identity. This is a project art selection, not final user acceptance.')
PROTECTED, TREES, ALLOWED = {}, {}, set()


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    if not ok:
        dump()
        raise RuntimeError(name + ': ' + repr(observed))


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def binding(file):
    file = Path(file).resolve()
    return dict(path=str(file), sha256=sha(file))


def package(obj):
    return (obj.get_path_name() if isinstance(obj, U.Object) else str(obj)).split('.')[0]


def asset_file(path, extension='.uasset'):
    name = package(path)
    assert name.startswith('/Game/HarborCity/') and '..' not in name
    return (PROJECT / 'Content' / (name.removeprefix('/Game/') + extension)).resolve()


def protect(file, expected=None):
    file = Path(file).resolve()
    check('protected input exists', file.is_file(), str(file))
    actual = sha(file)
    check('exact source hash', expected is None or actual == expected, str(file))
    PROTECTED[str(file)] = actual


def tree(folder):
    folder = Path(folder).resolve()
    # pathlib's WindowsPath ordering is case-folded, str ordering is not.
    # Use the same explicit string ordering both here and in the final guard.
    files = sorted((p.resolve() for p in folder.rglob('*') if p.is_file()), key=str)
    check('protected tree exists', folder.is_dir() and bool(files), str(folder))
    TREES[str(folder)] = [str(p) for p in files]
    for file in files:
        protect(file)


def load(path):
    obj = A.load_asset(path)
    check('native asset load', obj is not None, path)
    return obj


def path_of(obj):
    return obj.get_path_name() if obj is not None else None


def value(obj):
    if obj is None or isinstance(obj, (str, bool, int, float)):
        return obj
    if isinstance(obj, U.Object):
        return obj.get_path_name()
    if isinstance(obj, (list, tuple, U.Array)):
        return [value(x) for x in obj]
    if isinstance(obj, U.Vector):
        return [float(obj.x), float(obj.y), float(obj.z)]
    if isinstance(obj, U.Rotator):
        return [float(obj.pitch), float(obj.yaw), float(obj.roll)]
    # UE's StructBase.__str__ includes a memory address; its native export_text
    # explicitly uses PPF_None and is suitable for cross-process equality.
    if isinstance(obj, U.StructBase):
        return obj.export_text()
    # Reflected enum/name values have stable text, without UObject addresses.
    return str(obj)


def properties(obj, names):
    return {name: value(obj.get_editor_property(name)) for name in names}


def dirty():
    return sorted({p.get_path_name() for fn in (
        U.EditorLoadingAndSavingUtils.get_dirty_content_packages,
        U.EditorLoadingAndSavingUtils.get_dirty_map_packages) for p in fn()})


def hero_objects():
    bp = load(TARGET)
    check('exact owned Hero Blueprint', isinstance(bp, U.Blueprint) and A.get_metadata_tag(bp, KEY) == HERO_OWNER)
    cls = U.load_class(None, CLASS)
    check('native Hero generated class', cls is not None)
    cdo = U.get_default_object(cls)
    body = cdo.get_component_by_class(U.SkeletalMeshComponent)
    check('native body exists', body is not None)
    check('no candidate point-light components', not cdo.get_components_by_class(U.PointLightComponent))
    return bp, cdo, body


def snapshot(cdo, body):
    movement = cdo.get_component_by_class(U.CharacterMovementComponent)
    capsule = cdo.get_component_by_class(U.CapsuleComponent)
    combat = cdo.get_component_by_class(U.HCM4CombatComponent)
    presentation = cdo.get_component_by_class(U.HCM4R2PresentationComponent)
    check('existing gameplay components present', all(x is not None for x in (movement, capsule, combat, presentation)))
    mesh = body.get_editor_property('skeletal_mesh_asset')
    slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('materials')]
    check('actual ordered five Selestia slots', [s.removeprefix('Selestia_') for s in slots] == list(SLOTS), slots)
    check('five effective body materials', body.get_num_materials() == 5)
    keep = dict(
        character_class=cdo.get_class().get_path_name(),
        character=properties(cdo, ('walk_speed', 'sprint_speed', 'base_eye_height', 'crouched_eye_height', 'use_controller_rotation_yaw')),
        component_inventory=sorted([x.get_name(), x.get_class().get_path_name()] for x in cdo.get_components_by_class(U.ActorComponent)),
        body=properties(body, ('skeletal_mesh_asset', 'anim_class', 'animation_mode', 'physics_asset_override',
                              'relative_location', 'relative_rotation', 'relative_scale3d', 'lighting_channels')),
        movement=properties(movement, ('max_walk_speed', 'max_acceleration', 'braking_deceleration_walking',
                                      'gravity_scale', 'rotation_rate', 'orient_rotation_to_movement')),
        capsule=properties(capsule, ('capsule_radius', 'capsule_half_height')),
        combat=properties(combat, ('aim_look_scale', 'ads_magnification', 'aim_transition_seconds', 'magazine_capacity',
                                  'initial_reserve', 'hip_spread_degrees', 'aim_spread_degrees', 'shot_range', 'body_damage',
                                  'head_damage', 'melee_sphere_radius', 'attack_movement_scale', 'combo_reset_seconds',
                                  'attack_durations', 'hit_window_starts', 'hit_window_ends', 'combo_window_starts', 'melee_damage',
                                  'punch_animations', 'player_animation_blueprint', 'pistol_idle_animation', 'pistol_aim_animation',
                                  'pistol_fire_animation', 'pistol_reload_animation', 'player_hit_front_montage')),
        first_person=properties(presentation, ('first_person_arms_override', 'eye_anchor_in_mesh', 'first_person_skinning_radius_cm',
                                              'head_accessory_mesh', 'head_accessory_bone', 'head_accessory_local_transform',
                                              'relaxed_hand_camera', 'relaxed_anatomical_wrists', 'relaxed_hand_swing_scale',
                                              'relaxed_run_hand_camera', 'relaxed_finger_pose', 'unarmed_attack_hand_offset',
                                              'unarmed_attack_lateral_scale')))
    return dict(slot_names=slots, materials=[path_of(body.get_material(i)) for i in range(5)],
                override_materials=[path_of(x) for x in body.get_editor_property('override_materials')], preserved=keep)


def source_evidence():
    for file, expected in ((AUTHOR, AUTHOR_SHA), (RUNTIME, RUNTIME_SHA), (RUN / 'launch.json', LAUNCH_SHA)):
        protect(file, expected)
    author = json.loads(AUTHOR.read_text('utf-8-sig'))
    run = json.loads(RUNTIME.read_text('utf-8-sig'))
    launch = json.loads((RUN / 'launch.json').read_text('utf-8-sig'))
    check('exact successful Chroma author', author['status'] == 'PASS' and author['phase'] == 'HeroLightChroma'
          and len(author['assets']) == 24 and author['protected_source_sha256_before'] == author['protected_source_sha256_after'])
    check('exact nine-shot runtime and normal exit', run['status'] == 'PASS' and run['user_stop_latched'] is False
          and run['stop_frame'] == 0 and run['planned_shots'] == len(run['shots']) == 9
          and launch['exit_code'] == 0 and launch['map'] == author['map'] and run['map'] == author['map'].rsplit('/', 1)[1])
    recorded = {a['path']: a for a in author['assets']}
    check('24 distinct source packages', len(recorded) == 24)
    for path, asset in recorded.items():
        check('source candidate namespace', path.startswith(author['target_directory'] + '/'))
        protect(asset_file(path, '.umap' if path == author['map'] else '.uasset'), asset['sha256'])
    # Verify consumes the same source proof while permitting only its own exact
    # before->after Blueprint transition; every other old input remains exact.
    for file, expected in author['protected_source_sha256_before'].items():
        if Path(file).resolve() != asset_file(TARGET):
            protect(file, expected)
    mapping = {x['slot']: x['path'] for x in author['materials'][SELECTED]}
    check('exact selected five slots', set(mapping) == set(SLOTS) and len(mapping) == 5)
    expected = [mapping[s] for s in SLOTS]
    check('all selected sources are exact saved .5 instances', all(p in recorded and '/Materials/Chroma_050/MI_MToon_' in p for p in expected)
          and all(x['strength'] == .5 for x in author['materials'][SELECTED]))
    pngs = []
    for index, (shot, plan) in enumerate(zip(run['shots'], author['shot_plan'])):
        file = Path(shot['file']).resolve()
        check('native shot matches bound plan and has no point lights', shot['status'] == 'PASS'
              and shot['shot_index'] == index and shot['label'] == plan['label'] and shot['variant'] == plan['variant']
              and shot['character_class'] == CLASS and shot['character_point_lights_actual'] == []
              and file.parent == RUNTIME.parent and file.name == plan['expected_png']
              and shot['width'] == 1920 and shot['height'] == 1080 and shot['native_png_exists'] is True)
        with file.open('rb') as stream:
            head = stream.read(24)
        check('actual native PNG exists and dimensions match', head[:8] == b'\x89PNG\r\n\x1a\n'
              and len(head) == 24 and struct.unpack('>II', head[16:24]) == (1920, 1080)
              and file.stat().st_size == shot['file_bytes'])
        pngs.append(binding(file))
        check('actual per-shot material readiness complete', shot['material_readiness']['status'] == 'READY'
              and all(s['ready'] and not s['render_fallback_used'] for s in shot['material_readiness']['slots']))
        if shot['variant'] == SELECTED:
            check('selected source order matches actual runtime slots', [x['slot'] for x in shot['materials']] == list(range(5))
                  and [package(x['configured_source']) for x in shot['materials']] == expected)
    R['inputs'] = dict(author=binding(AUTHOR), runtime=binding(RUNTIME), launch=binding(RUN / 'launch.json'),
                       native_pngs=pngs, script=binding(Path(__file__)))
    R['selected_materials'] = [dict(slot=i, slot_name=s, path=mapping[s], sha256=recorded[mapping[s]]['sha256']) for i, s in enumerate(SLOTS)]
    mats = [load(p) for p in expected]
    for mat in mats:
        check('native candidate MI identity and effective strength', isinstance(mat, U.MaterialInstanceConstant)
              and A.get_metadata_tag(mat, KEY) == CHROMA_OWNER
              and abs(float(E.get_material_instance_scalar_parameter_value(mat, U.Name('HeroLightChroma'))) - .5) < 1e-6)
    before_sha = author['protected_source_sha256_before'][str(asset_file(TARGET))]
    return author, run, mats, before_sha


def actual_apply(mats, before_sha, run):
    protect(asset_file(TARGET), before_sha)
    bp, cdo, body = hero_objects()
    before = snapshot(cdo, body)
    check('current body agrees with actual reviewed mesh/animation', before['preserved']['body']['skeletal_mesh_asset'] == run['shots'][1]['skeletal_mesh']
          and before['preserved']['body']['anim_class'] == run['shots'][1]['animation_class'])
    check('frozen movement speeds retained', before['preserved']['character']['walk_speed'] == 400
          and before['preserved']['character']['sprint_speed'] == 650)
    selected_paths = [m.get_path_name() for m in mats]
    check('single unapplied exact baseline', before['materials'] != selected_paths)
    R['baseline_native_cdo'] = before
    backup_dir = (BACKUPS / OUT.name).resolve()
    check('fresh bounded E-drive backup', backup_dir.is_relative_to(BACKUPS) and not backup_dir.exists())
    backup_dir.mkdir(parents=True, exist_ok=False)
    backup = backup_dir / asset_file(TARGET).name
    shutil.copy2(asset_file(TARGET), backup)
    check('exact pre-edit BP byte backup', sha(backup) == before_sha)
    R['backup'] = binding(backup)
    dump()
    bp.modify(); cdo.modify(); body.modify()
    body.set_editor_property('override_materials', mats)
    check('native Blueprint compile', U.BlueprintEditorLibrary.compile_blueprint(bp))
    bp, cdo, body = hero_objects()
    after = snapshot(cdo, body)
    check('exact five materials on regenerated CDO', after['materials'] == selected_paths and after['override_materials'] == selected_paths)
    check('all non-material native snapshots unchanged', after['preserved'] == before['preserved'] and after['slot_names'] == before['slot_names'])
    check('no unexpected dirty package before save', set(dirty()).issubset({TARGET}), dirty())
    ALLOWED.add(str(asset_file(TARGET)))
    check('native save exact Hero BP only', A.save_loaded_asset(bp, False))
    saved_sha = sha(asset_file(TARGET))
    check('native serialized BP bytes changed', saved_sha != before_sha)
    bp, cdo, body = hero_objects()
    check('post-save CDO readback remains exact', snapshot(cdo, body) == after)
    check('save leaves no dirty packages', not dirty(), dirty())
    transition = dict(path=TARGET, class_name='Blueprint', before_sha256=before_sha, after_sha256=saved_sha,
                      backup=R['backup'], before_native_cdo=before, after_native_cdo=after,
                      selected_materials=R['selected_materials'], changed_scope='Body override_materials[0:5] only',
                      source_author=R['inputs']['author'], reviewed_runtime=R['inputs']['runtime'])
    R['transitions'] = [transition]
    R['assets'] = [dict(path=TARGET, sha256=saved_sha, bytes=asset_file(TARGET).stat().st_size)]
    R['applied'] = True
    R['pass_scope'] = 'Native BP compile/save/CDO readback only; run HeroChromaBindingVerify in a new process for disk reload evidence.'


def actual_verify(mats, before_sha):
    candidates = sorted((DOC / 'editor_runtime').glob('author_*_HeroChromaBindingApply/hero_chroma_binding_manifest.json'),
                        key=lambda p: p.stat().st_mtime, reverse=True)
    check('successful exact binding manifest exists', bool(candidates))
    file = candidates[0]
    manifest = json.loads(file.read_text('utf-8-sig'))
    check('exact binding manifest schema', manifest.get('schema') == 'HarborCity.M5VS2.HeroChromaBinding.v1'
          and manifest.get('status') == 'PASS' and manifest.get('phase') == 'HeroChromaBindingApply' and len(manifest.get('transitions', [])) == 1)
    report_file = Path(manifest['evidence']['path']).resolve()
    check('manifest points to its exact author report', report_file == file.parent / 'author_result.json'
          and sha(report_file) == manifest['evidence']['sha256'])
    applied = json.loads(report_file.read_text('utf-8-sig'))
    check('actual successful Apply from a different process', applied['status'] == 'PASS' and applied['applied'] is True
          and applied['process_id'] != os.getpid() and applied['phase'] == 'HeroChromaBindingApply'
          and applied['transitions'] == manifest['transitions'] and applied['preservation']['unexpected_changed_files'] == []
          and applied['inputs']['author']['sha256'] == AUTHOR_SHA and applied['inputs']['runtime']['sha256'] == RUNTIME_SHA)
    transition = manifest['transitions'][0]
    check('exact authorized one-BP transition', transition['path'] == TARGET and transition['before_sha256'] == before_sha
          and transition['selected_materials'] == R['selected_materials'])
    backup = Path(transition['backup']['path']).resolve()
    check('original backup remains exact and private', backup.is_relative_to(BACKUPS)
          and sha(backup) == transition['backup']['sha256'] == before_sha)
    protect(asset_file(TARGET), transition['after_sha256'])
    bp, cdo, body = hero_objects()
    fresh = snapshot(cdo, body)
    check('fresh process deserialized complete CDO exactly', fresh == transition['after_native_cdo'])
    check('fresh native five MI references', fresh['materials'] == [m.get_path_name() for m in mats]
          and fresh['override_materials'] == fresh['materials'])
    check('read-only verify leaves no dirty packages', not dirty(), dirty())
    R['inputs']['apply_manifest'] = binding(file)
    R['inputs']['apply_report'] = binding(report_file)
    R['fresh_process_disk_readback'] = 'PASS'
    R['native_cdo_readback'] = fresh
    R['transitions'] = [transition]
    R['pass_scope'] = 'Fresh native process read the exact saved BP and all five selected material references; future scene rendering remains NOT_RUN.'


def actual_recovery_verify(mats, before_sha):
    report_file = RECOVERY_RUN / 'author_result.json'
    command_file = RECOVERY_RUN / 'commandlet.json'
    protect(report_file, RECOVERY_REPORT_SHA)
    protect(command_file, RECOVERY_COMMANDLET_SHA)
    applied = json.loads(report_file.read_text('utf-8-sig'))
    command = json.loads(command_file.read_text('utf-8-sig'))
    check('exact failed Apply and original script identity',
          applied['schema'] == 'HarborCity.M5VS2.HeroChromaBindingAuthor.v1'
          and applied['phase'] == 'HeroChromaBindingApply' and applied['status'] == 'FAIL'
          and applied['applied'] is True and 'error' not in applied
          and applied['target'] == TARGET and applied['selected_variant'] == SELECTED
          and applied['process_id'] != os.getpid()
          and applied['inputs']['script']['sha256'] == RECOVERY_SCRIPT_SHA
          and command['script_sha256'].lower() == RECOVERY_SCRIPT_SHA
          and command['phase'] == 'HeroChromaBindingApply' and command['status'] == 'FAIL'
          and command['author_status'] == 'FAIL')
    check('all 725 native Apply checks actually passed', len(applied['checks']) == 725
          and all(c['status'] == 'PASS' for c in applied['checks']))
    expected_folders = (PROJECT / 'Content/HarborCity/M5VS1',
                        PROJECT / 'Content/HarborCity/M5VS2/HeroSelestia', PROJECT / 'Config')
    preservation = applied['preservation']
    target_file = str(asset_file(TARGET))
    expected_order_only_folders = {str(p.resolve()) for p in expected_folders[:2]}
    check('only recorded failure was empty membership differences from ordering',
          preservation['files_checked'] == 279
          and preservation['allowed_changed_files'] == [target_file]
          and preservation['unexpected_changed_files'] == []
          and preservation['old_vs1_all_files_checked'] is True
          and len(preservation['tree_membership_changes']) == 2
          and {x['folder'] for x in preservation['tree_membership_changes']} == expected_order_only_folders
          and all(x['added'] == [] and x['removed'] == [] for x in preservation['tree_membership_changes']))
    check('one exact completed serialized transition', len(applied['transitions']) == 1 and len(applied['assets']) == 1)
    transition = applied['transitions'][0]
    check('transition agrees with reviewed source and exact saved bytes',
          transition['path'] == TARGET and transition['before_sha256'] == before_sha
          and transition['after_sha256'] == '42ecd5b3a74c1d12aceeae5eac8559b4ac552cb931cba1b0e6f30f3bc7128940'
          and transition['selected_materials'] == R['selected_materials']
          and transition['before_native_cdo'] == applied['baseline_native_cdo']
          and transition['before_native_cdo']['preserved'] == transition['after_native_cdo']['preserved']
          and transition['source_author']['sha256'] == AUTHOR_SHA
          and transition['reviewed_runtime']['sha256'] == RUNTIME_SHA
          and applied['assets'][0]['path'] == TARGET
          and applied['assets'][0]['sha256'] == transition['after_sha256'])
    backup = Path(transition['backup']['path']).resolve()
    check('original exact byte backup preserved privately', backup.is_relative_to(BACKUPS)
          and sha(backup) == transition['backup']['sha256'] == before_sha
          and transition['backup'] == applied['backup'])
    protect(backup, before_sha)
    old_protected = applied['protected_source_sha256_before']
    check('all 279 original protected inputs identified', len(old_protected) == 279
          and old_protected[target_file] == before_sha)
    for file, original_hash in old_protected.items():
        protect(file, transition['after_sha256'] if file == target_file else original_hash)
    membership = []
    for folder in expected_folders:
        folder = folder.resolve()
        old_names = {str(Path(p).resolve()) for p in old_protected if Path(p).resolve().is_relative_to(folder)}
        current_names = {str(p.resolve()) for p in folder.rglob('*') if p.is_file()}
        check('exact original protected tree membership sets unchanged', bool(old_names) and current_names == old_names,
              dict(folder=str(folder), expected_count=len(old_names), actual_count=len(current_names),
                   added=sorted(current_names-old_names), removed=sorted(old_names-current_names)))
        membership.append(dict(folder=str(folder), files=len(old_names), status='PASS'))
    bp, cdo, body = hero_objects()
    fresh = snapshot(cdo, body)
    check('fresh process recovered complete saved CDO exactly', fresh == transition['after_native_cdo'])
    check('fresh process recovered exact five MI references', fresh['materials'] == [m.get_path_name() for m in mats]
          and fresh['override_materials'] == fresh['materials'])
    check('read-only recovery leaves no dirty packages', not dirty(), dirty())
    R['inputs']['original_failed_apply_report'] = binding(report_file)
    R['inputs']['original_failed_apply_commandlet'] = binding(command_file)
    R['fresh_process_disk_readback'] = 'PASS'
    R['native_cdo_readback'] = fresh
    R['transitions'] = [transition]
    R['recovery'] = dict(outcome='RECOVERED_AFTER_ORDER_ONLY_GUARD_FAILURE', original_apply_status='FAIL',
                         original_apply_evidence=binding(report_file), original_script_sha256=RECOVERY_SCRIPT_SHA,
                         original_native_checks_passed=725, original_protected_files_verified=279,
                         original_tree_membership_sets=membership, asset_writes=0, blueprint_compile=False,
                         reason='WindowsPath sort was case-folded while final str sort was case-sensitive; both recorded set differences were empty.')
    R['pass_scope'] = 'Read-only recovery of exact a915d9f8 saved BP: 725 original checks, 279 original protected inputs, complete original tree membership, byte backup and fresh-process CDO verified. Original Apply remains FAIL. No new asset writes or scene render.'


try:
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no initial dirty packages', not dirty(), dirty())
    author, run, materials, before_sha = source_evidence()
    for folder in (PROJECT / 'Content/HarborCity/M5VS1', PROJECT / 'Content/HarborCity/M5VS2/HeroSelestia', PROJECT / 'Config'):
        tree(folder)
    R['protected_source_sha256_before'] = dict(PROTECTED)
    dump()
    if APPLY:
        actual_apply(materials, before_sha, run)
    elif PHASE == 'HeroChromaBindingRecoveryVerify':
        actual_recovery_verify(materials, before_sha)
    else:
        actual_verify(materials, before_sha)
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
finally:
    changed = [p for p, h in PROTECTED.items() if p not in ALLOWED and (not Path(p).is_file() or sha(p) != h)]
    tree_changes = []
    for folder, names in TREES.items():
        actual = sorted(str(p.resolve()) for p in Path(folder).rglob('*') if p.is_file())
        if actual != names:
            tree_changes.append(dict(folder=folder, added=sorted(set(actual) - set(names)), removed=sorted(set(names) - set(actual))))
    R['preservation'] = dict(files_checked=len(PROTECTED), allowed_changed_files=sorted(ALLOWED),
                             unexpected_changed_files=changed, tree_membership_changes=tree_changes,
                             old_vs1_all_files_checked=str(PROJECT / 'Content/HarborCity/M5VS1') in TREES)
    if changed or tree_changes:
        R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
    if R['status'] == 'PASS':
        manifest = dict(schema='HarborCity.M5VS2.HeroChromaBinding.v1', status='PASS', phase=PHASE,
                        evidence=binding(OUT / 'author_result.json'), transitions=R['transitions'],
                        fresh_process_disk_readback=R['fresh_process_disk_readback'], runtime_after_binding='NOT_RUN',
                        visual_acceptance='USER_REVIEW', night_proxy_acceptance='NOT_ACCEPTED')
        if 'recovery' in R:
            manifest['recovery'] = R['recovery']
        (OUT / 'hero_chroma_binding_manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
if R['status'] != 'PASS':
    raise RuntimeError('Hero Chroma binding failed; preserve failure evidence and backup; do not overwrite/retry a changed baseline.')
