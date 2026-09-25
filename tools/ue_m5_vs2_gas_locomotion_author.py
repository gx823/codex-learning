"""Bounded GAS locomotion Probe / evidence-bound Apply; root owns execution.

ue_m5_vs2_author_run.ps1 -Phase GASLocomotionProbe -ScriptPath this_file
ue_m5_vs2_author_run.ps1 -Phase GASLocomotionApply -ScriptPath this_file

Apply uses the newest actual PASS Probe only if its inputs and readback still
match. Only three sample animation references change in a new VS2 BlendSpace;
one existing VS2 AnimBP BlendSpace reference changes. No graph rewiring, speed,
rate, combat, FP arms, source clip, skeleton, VS1, map or C++ writes.
"""
from pathlib import Path
import copy
import datetime as dt
import hashlib
import json
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
CONTENT = PROJECT / 'Content'
ROOT = '/Game/HarborCity/M5VS2/HeroSelestia'
ANIM = ROOT + '/Animation'
BLUEPRINT = ANIM + '/ABP_M5VS2_Selestia_Physics'
SOURCE_ROOT = '/Game/HarborCity/M5VS1/HeroSelestia'
SOURCE_BS = SOURCE_ROOT + '/Animation/BS_Idle_Walk_Run_Selestia'
TARGET_BS = ANIM + '/GAS/BS_M5VS2_GAS_IdleWalkRun'
SKELETON = SOURCE_ROOT + '/SK_Selestia'
HERO = ROOT + '/BP_M5VS2_Selestia'
OWNER = 'HarborCity_M5_VS2_GASLocomotion'
KEY = 'HarborCityOwnedBy'
A = U.EditorAssetLibrary
CMD = U.SystemLibrary.get_command_line()


def argument(name):
    rows = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', CMD)
    if len(rows) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return rows[0][0] or rows[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
PHASE = argument('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT / 'author_result.json').exists()
assert PHASE in ('GASLocomotionProbe', 'GASLocomotionApply')
OUT.mkdir(parents=True, exist_ok=True)
APPLY = PHASE == 'GASLocomotionApply'
R = dict(status='RUNNING', phase=PHASE, checks=[], assets=[], inputs={},
    candidate_valid=False, started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
    runtime='NOT_RUN', rendered_pose='NOT_RUN', user_naturalness='USER_REVIEW', map_writes=0,
    scope='Three animation-reference substitutions in a new VS2 copy of the existing 2D BlendSpace; exactly one existing VS2 AnimBP reference. Existing axes/sample positions/rates/directions/graph wiring remain unchanged.',
    limitations=[
        'Native binding does not establish natural gait, planted feet, clean transitions or collision-free secondary motion.',
        'Source relaxed walk travel is approximately 165cm/s; relaxed run approximately 375cm/s. Character CDO speeds are read back separately and never altered. No automatic rate multiplier hides this mismatch.',
        'Fourteen other directional clips stay as the existing Selestia clips. Forward GAS changes must be reviewed through directional transitions and held-weapon poses.',
        'Two GAS idle-break clips remain isolated review candidates, not connected to the default state machine by this script.',
        'Any separate legacy idle SequencePlayer is explicitly reported and remains unchanged; changing it requires a separately scoped decision after Probe.'])
PROTECTED = {}
REPLACEMENTS = {
    SOURCE_ROOT + '/Animation/MM_Idle_Selestia': ANIM + '/GAS/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS',
    SOURCE_ROOT + '/Animation/MF_Unarmed_Walk_Fwd_Selestia': ANIM + '/GAS/Retargeted/M_Relaxed_Walk_Loop_F_InPlace_SelestiaGAS',
    SOURCE_ROOT + '/Animation/MF_Unarmed_Jog_Fwd_Selestia': ANIM + '/GAS/Retargeted/M_Relaxed_Run_Loop_F_InPlace_SelestiaGAS',
}


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed)); dump()
    if not ok:
        raise RuntimeError(name + ': ' + repr(observed))


def package(path):
    return str(path).split('.')[0]


def disk(path):
    path = package(path)
    assert path.startswith('/Game/') and '..' not in path
    return CONTENT / Path(path.removeprefix('/Game/')).with_suffix('.uasset')


def sha(file):
    digest = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def input_file(file):
    return dict(path=str(file), sha256=sha(file))


def path_of(obj):
    return obj.get_path_name() if obj else None


def load(path):
    obj = A.load_asset(path)
    check('load exact native asset', obj is not None, path)
    return obj


def protect(file, expected=None):
    file = Path(file)
    check('protected package exists', file.is_file(), str(file))
    digest = sha(file)
    if expected:
        check('package matches successful native evidence', digest == expected, str(file))
    PROTECTED[str(file)] = digest


def dirty():
    return sorted(p.get_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def blueprint_readback(blueprint):
    # UBlueprint.ParentClass/graph arrays are UPROPERTY(), not Python editor
    # properties. Native GetAllGraphs and actual node APIs avoid that boundary.
    report = json.loads(U.HCM5VS2PhysicsEditor.read_locomotion_graph(blueprint))
    check('native bounded locomotion graph readback', report.get('status') == 'READBACK_COMPLETE', report.get('error'))
    players = [p for p in report['blendspace_players']
               if p['blendspace'] and package(p['blendspace']) in (SOURCE_BS, TARGET_BS)]
    return report, players


def blendspace_readback(blendspace):
    report = json.loads(U.HCM5VS2PhysicsEditor.read_locomotion_blend_space(blendspace))
    check('native bounded BlendSpace readback', report.get('status') == 'READBACK_COMPLETE', report.get('error'))
    return report


def hero_readback():
    hero_class = U.load_class(None, HERO + '.BP_M5VS2_Selestia_C')
    check('existing new hero generated class', hero_class is not None)
    hero = U.get_default_object(hero_class)
    mesh = hero.get_component_by_class(U.SkeletalMeshComponent)
    movement = hero.get_component_by_class(U.CharacterMovementComponent)
    return dict(blueprint=HERO, animation=path_of(mesh.get_editor_property('anim_class')),
        mesh=path_of(mesh.get_editor_property('skeletal_mesh_asset')),
        walk_speed=float(hero.get_editor_property('walk_speed')), sprint_speed=float(hero.get_editor_property('sprint_speed')),
        character_movement_max_walk_speed=float(movement.get_editor_property('max_walk_speed')),
        materials=[path_of(mesh.get_material(i)) for i in range(mesh.get_num_materials())])


def validate_inputs():
    for label in ('SOURCE', 'INPLACE', 'RETARGET'):
        file = DOC / ('research/GAS_P0_' + label + '_MANIFEST.json')
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        check('actual successful GAS ' + label + ' manifest', data.get('status') == 'PASS')
        R['inputs'][label] = input_file(file)
        for asset in data['assets']:
            protect(disk(asset['path']), asset['sha256'])
    for directory in (disk(SOURCE_ROOT + '/unused').parent, disk(ROOT + '/unused').parent):
        for file in directory.rglob('*.uasset'):
            protect(file)
    for file in (PROJECT / 'Config').glob('*.ini'):
        protect(file)
    for file in CONTENT.rglob('*.umap'):
        protect(file)
    R['inputs']['script'] = input_file(WORK / 'tools/ue_m5_vs2_gas_locomotion_author.py')
    for suffix in ('.h', '.cpp'):
        R['inputs']['native_helper' + suffix] = input_file(PROJECT / ('Source/HarborCity/M5VS2/HCM5VS2PhysicsEditor' + suffix))
    R['inputs']['blueprint'] = dict(path=BLUEPRINT, sha256=sha(disk(BLUEPRINT)))
    R['inputs']['source_blendspace'] = dict(path=SOURCE_BS, sha256=sha(disk(SOURCE_BS)))
    check('new BlendSpace absent in registry and disk', not A.does_asset_exist(TARGET_BS) and not disk(TARGET_BS).exists())
    blueprint = load(BLUEPRINT); source = load(SOURCE_BS); skeleton = load(SKELETON)
    check('exact VS2 Physics AnimBP owner', A.get_metadata_tag(blueprint, KEY) == 'HarborCity_M5_VS2_SelestiaPhysics_v1')
    check('actual shared normalized Selestia skeleton', source.get_editor_property('skeleton') == skeleton
          and blueprint.get_editor_property('target_skeleton') == skeleton)
    clips = {old: load(new) for old, new in REPLACEMENTS.items()}
    for old, clip in clips.items():
        check('GAS target sequence and matching skeleton', isinstance(clip, U.AnimSequence)
              and clip.get_editor_property('skeleton') == skeleton, clip.get_path_name())
        check('movement stays with CharacterMovement', not clip.get_editor_property('enable_root_motion')
              and not clip.get_editor_property('force_root_lock'), clip.get_path_name())
    return blueprint, source, clips


def planned_changes(graph, blendspace):
    rows = [dict(index=s['index'], before=package(s['animation']), after=REPLACEMENTS[package(s['animation'])],
                 unchanged_position=s['position'], unchanged_rate_scale=s['rate_scale'])
            for s in blendspace['samples'] if s['animation'] and package(s['animation']) in REPLACEMENTS]
    blockers = []
    if len(rows) != 3 or {r['before'] for r in rows} != set(REPLACEMENTS):
        blockers.append('Requires exactly one existing sample for each of idle, forward walk and forward jog; no automatic duplicate/directional replacement.')
    players = [p for p in graph['blendspace_players'] if p['blendspace'] and package(p['blendspace']) == SOURCE_BS]
    if len(players) != 1:
        blockers.append('Expected exactly one player of the original locomotion BlendSpace.')
    if any('blendspace' == p.lower().replace('_', '') for player in players for p in player['exposed_property_pins']):
        blockers.append('BlendSpace is exposed as a graph pin; this author will not change links or pin defaults.')
    if graph['physics'].get('status') != 'READBACK_COMPLETE' or len(graph['physics'].get('nodes', [])) != 11:
        blockers.append('Expected intact read-only eleven-node physics setup.')
    legacy_idle = [p for p in graph['sequence_players'] if p['sequence'] and package(p['sequence']) in REPLACEMENTS]
    return dict(candidate_valid=not blockers, blockers=blockers, sample_changes=rows,
        blendspace_reference_changes=[dict(node=p['path'], before=SOURCE_BS, after=TARGET_BS) for p in players],
        unchanged_other_samples=[s for s in blendspace['samples'] if not s['animation'] or package(s['animation']) not in REPLACEMENTS],
        separate_old_sequence_players_unchanged=legacy_idle,
        idle_scope_warning='Separate original SequencePlayers are not replaced by the three-sample patch; inspect the listed graph contexts before deciding Apply timing.' if legacy_idle else None)


def swap_text(text, old, new):
    # Full object path first, then any bare package occurrence. The copied asset
    # has a new object basename, which a package-only replace would miss.
    old_object = old + '.' + old.rsplit('/', 1)[1]
    new_object = new + '.' + new.rsplit('/', 1)[1]
    return text.replace(old_object, new_object).replace(old, new)


def replace_serialized(value, old, new):
    if isinstance(value, str):
        return swap_text(value, old, new)
    if isinstance(value, list):
        return [replace_serialized(v, old, new) for v in value]
    if isinstance(value, dict):
        return {k:replace_serialized(v, old, new) for k,v in value.items()}
    return value


def apply_changes(blueprint, source, before_graph, before_bs):
    probe_file = None
    for file in sorted((DOC / 'editor_runtime').glob('author_*_GASLocomotionProbe/author_result.json'), key=lambda p:p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS':
            probe_file, probe = file, data; break
    check('prior successful native Probe exists', probe_file is not None)
    check('prior Probe is a valid unchanged candidate', probe.get('candidate_valid') is True
          and probe.get('inputs') == R['inputs'] and probe.get('native_readback') == R['native_readback']
          and probe.get('plan') == R['plan'])
    R['probe'] = input_file(probe_file)
    backup = Path('E:/GameDev/Assets/HarborCity/M5_VS2/HeroSelestia/AnimationBackups') / OUT.name / 'ABP_M5VS2_Selestia_Physics.uasset'
    check('fresh private AnimBP backup', not backup.exists()); backup.parent.mkdir(parents=True, exist_ok=False)
    shutil.copy2(disk(BLUEPRINT), backup)
    check('exact VS2 AnimBP byte backup', sha(backup) == R['inputs']['blueprint']['sha256'])
    R['backup'] = input_file(backup)
    target = A.duplicate_asset(SOURCE_BS, TARGET_BS)
    check('native isolated BlendSpace duplicate', target is not None)
    A.set_metadata_tag(target, KEY, OWNER)
    A.set_metadata_tag(target, 'GASLocomotionProbeSHA256', sha(probe_file))
    # Native duplication also relocates any owned analysis subobjects. Preserve
    # their settings while comparing their legitimate new outer/object paths.
    duplicated_bs = replace_serialized(before_bs, SOURCE_BS, TARGET_BS)
    R['duplicate_readback'] = blendspace_readback(target)
    check('native duplicate preserves complete authored BlendSpace data', R['duplicate_readback'] == duplicated_bs)
    R['native_apply'] = json.loads(U.HCM5VS2PhysicsEditor.apply_locomotion_samples(
        blueprint, source, target, json.dumps(R['plan']['sample_changes'])))
    check('bounded native three-sample replacement and VS2 compile',
          R['native_apply'].get('status') == 'APPLIED_COMPILED_NOT_SAVED', R['native_apply'])
    expected_bs = copy.deepcopy(duplicated_bs)
    for row in R['plan']['sample_changes']:
        expected_bs['samples'][row['index']] = replace_serialized(expected_bs['samples'][row['index']], row['before'], row['after'])
    R['after_readback'] = dict(blendspace=blendspace_readback(target))
    check('only three sample animation references changed', R['after_readback']['blendspace'] == expected_bs)
    after_graph, _ = blueprint_readback(blueprint)
    R['after_readback']['blueprint'] = after_graph
    expected_graph = replace_serialized(before_graph, SOURCE_BS, TARGET_BS)
    check('observed graph nodes/physics/slots remain identical except one BlendSpace reference', after_graph == expected_graph)
    check('hero CDO speeds/materials/AnimBP unchanged', hero_readback() == R['native_readback']['hero'])
    check('save only new VS2 BlendSpace', A.save_loaded_asset(target, False))
    check('save only existing VS2 AnimBP', A.save_loaded_asset(blueprint, False))
    for obj in (target, blueprint):
        path = package(obj.get_path_name())
        R['assets'].append(dict(path=path, class_name=obj.get_class().get_name(), sha256=sha(disk(path)), bytes=disk(path).stat().st_size))
    PROTECTED.pop(str(disk(BLUEPRINT)))
    R['after_readback']['blendspace_after_save'] = blendspace_readback(target)
    R['fresh_process_reload'] = 'NOT_RUN_REQUIRED: next native process must load the saved BlendSpace and AnimBP, inspect samples and exercise real gait.'


try:
    dump()
    check('exact HarborCity project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE or dirty maps', not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('compiled native locomotion helper API available', all(hasattr(U.HCM5VS2PhysicsEditor, name) for name in
          ('read_locomotion_graph', 'read_locomotion_blend_space', 'apply_locomotion_samples')),
          'Requires root native Editor rebuild before Probe; no Python property-name fallback')
    blueprint, source, clips = validate_inputs()
    before_dirty = dirty()
    graph, players = blueprint_readback(blueprint)
    blendspace = blendspace_readback(source)
    hero = hero_readback()
    check('new hero uses exact VS2 Physics AnimBP', package(hero['animation']) == BLUEPRINT)
    R['native_readback'] = dict(blueprint=graph, blendspace=blendspace, hero=hero)
    R['plan'] = planned_changes(graph, blendspace)
    R['candidate_valid'] = R['plan']['candidate_valid']
    R['idle_break_review_plan'] = [dict(path=ANIM + '/GAS/Retargeted/M_Relaxed_Stand_Idle_Break_v0%d_InPlace_SelestiaGAS' % i,
        automatic_state_machine_binding='NOT_AUTHORED', isolated_exercise='NOT_RUN') for i in (1, 2)]
    check('read-only native Probe did not dirty additional assets', dirty() == before_dirty)
    check('all Probe input bytes unchanged', all(sha(p) == h for p,h in PROTECTED.items()), len(PROTECTED))
    if APPLY:
        check('bounded native candidate has no blockers', R['candidate_valid'], R['plan']['blockers'])
        apply_changes(blueprint, source, graph, blendspace)
    check('no map writes', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('VS1, all source/GAS clips, skeletons, HeroBP/FP/material assets, maps and config byte-preserved',
          all(sha(p) == h for p,h in PROTECTED.items()), len(PROTECTED))
    R['pass_scope'] = ('Native three-sample BlendSpace copy, exact VS2 AnimBP reference substitution, compile and package save only; real gait and fresh reload NOT_RUN.'
                       if APPLY else 'Read-only actual native graph/sample/CDO inspection; candidate validity is separate from Probe PASS. Nothing saved.')
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc(); U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS':
    raise RuntimeError('GAS locomotion author failed; preserve evidence and any unsaved/new candidate')
