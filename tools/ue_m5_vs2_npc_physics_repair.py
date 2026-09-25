"""Bounded Q/R humanoid physics probe, then separately evidence-bound repair.

Run existing author wrapper with phase NPCPhysicsProbe first. NPCPhysicsRepair
requires that successful probe, unchanged source/target hashes and native DLL.
Only the two evidenced PHYS packages can be saved, after private byte backups.
No mesh, skeleton, animation, post-process graph, Blueprint or map is edited.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
ROOT = '/Game/HarborCity/M5VS2/NPC'
OWNER = 'HarborCity_M5_VS2_NPC_Integration'
A = U.EditorAssetLibrary
CMD = U.SystemLibrary.get_command_line()


def argument(name):
    found = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', CMD)
    if len(found) != 1: raise RuntimeError('Exactly one '+name+' is required')
    return found[0][0] or found[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
PHASE = argument('M5AuthorPhase')
assert OUT.is_relative_to((DOC/'editor_runtime').resolve())
assert PHASE in ('NPCPhysicsProbe', 'NPCPhysicsRepair')
assert not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
APPLY = PHASE == 'NPCPhysicsRepair'
R = dict(status='RUNNING', phase=PHASE, checks=[], assets=[],
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         scope='Only Q/R existing PHYS_NPC_Humanoid. Detailed native probe; optional evidence-bound anatomical constraints, mass, collision pairs and all existing humanoid capsules fitted to skin-only source points and actual reference landmarks. Explicit hidden-skin fallbacks; solver settings read only.',
         runtime='NOT_RUN', visual='NOT_RUN_NULLRHI', visual_acceptance='USER_REVIEW',
         previous_visual_result='FAIL: 85151f8c severe folding and ae876cae unnatural backwards hips/arms and tightly folded legs. Both retained; neither superseded by this author.',
         fresh_process_reload='NOT_RUN')


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(label, ok, observed=None):
    R['checks'].append(dict(name=label, status='PASS' if ok else 'FAIL', observed=observed)); dump()
    if not ok: raise RuntimeError(label+': '+str(observed))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()


def package(path): return str(path).split('.')[0]


def disk(path):
    path = package(path)
    if not path.startswith(ROOT+'/'): raise RuntimeError('Outside Q/R NPC asset scope')
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')


def dirty():
    return sorted(p.get_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def native(profile, physics, apply=False):
    return json.loads(U.HCM5VS2NPCEditor.inspect_or_repair_npc_physics(profile, physics, apply))


def load_integration(letter):
    reports = sorted((DOC/'editor_runtime').glob('author_*_NPCIntegration'+letter+'/author_result.json'),
                     key=lambda p: p.stat().st_mtime, reverse=True)
    for file in reports:
        data = json.loads(file.read_text(encoding='utf-8'))
        if data.get('status') == 'PASS': return file, data
    raise RuntimeError('Missing successful NPC integration '+letter)


try:
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT.resolve())
    check('no dirty maps at start', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    code_files = [WORK/'tools/ue_m5_vs2_npc_physics_repair.py',
                  PROJECT/'Source/HarborCity/M5VS2/HCM5VS2NPCEditor.h',
                  PROJECT/'Source/HarborCity/M5VS2/HCM5VS2NPCEditor.cpp',
                  PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll']
    R['code_binding'] = {str(p): sha(p) for p in code_files}
    protected = {}; jobs = []
    for letter in ('Q', 'R'):
        evidence, prior = load_integration(letter)
        specimen = prior['specimen']; runtime = prior['destination']
        check('exact owned runtime '+letter, re.fullmatch(re.escape(ROOT+'/AvatarSample_'+letter+'/Runtime_')+r'[0-9a-f]{10}', runtime) is not None)
        paths = {name: package(specimen[name]) for name in ('profile', 'physics', 'mesh')}
        check('exact target paths '+letter, paths['profile'] == runtime+'/DA_NPCProfile'
              and paths['physics'] == runtime+'/PHYS_NPC_Humanoid'
              and paths['mesh'].startswith(ROOT+'/AvatarSample_'+letter+'/Source_'))
        profile = A.load_asset(paths['profile']); physics = A.load_asset(paths['physics']); mesh = A.load_asset(paths['mesh'])
        check('loaded expected native classes '+letter, isinstance(profile, U.HCM5VS2NPCProfile)
              and isinstance(physics, U.PhysicsAsset) and isinstance(mesh, U.SkeletalMesh))
        check('owned runtime metadata '+letter, all(A.get_metadata_tag(o, 'HarborCityOwnedBy') == OWNER for o in (profile, physics)))
        skeleton = mesh.get_editor_property('skeleton')
        check('native source skeleton '+letter, skeleton is not None)
        paths['skeleton'] = package(skeleton.get_path_name())
        check('skeleton remains in matching source namespace '+letter, paths['skeleton'].rsplit('/', 1)[0] == paths['mesh'].rsplit('/', 1)[0])
        binding = {key: dict(path=path, sha256=sha(disk(path))) for key, path in paths.items()}
        for folder in (disk(paths['physics']).parent, disk(paths['mesh']).parent):
            for file in folder.rglob('*.uasset'): protected[str(file)] = sha(file)
        before_dirty = dirty(); result = native(profile, physics)
        row = dict(sample=letter, integration_evidence=str(evidence), integration_evidence_sha256=sha(evidence),
                   binding=binding, preflight=result)
        R['assets'].append(row); dump()
        check('native read-only probe '+letter, result.get('status') == 'PASS' and not result.get('applied'), result.get('error'))
        check('native full-body skin-fit candidate schema '+letter, result.get('schema') == 3)
        check('actual skin sections inspected '+letter, len(result.get('skin_sections_read_only', [])) >= 2
              and result.get('mapped_skin_point_count', 0) >= 2000)
        check('all and only existing humanoid body geometry candidates '+letter,
              len(result['bodies']) == result['body_count']
              and all(b.get('candidate_rebuilds_geometry') and b.get('candidate_shape') for b in result['bodies']))
        check('solver settings recorded without tuning '+letter,
              bool(result.get('physics_asset_solver_settings'))
              and all('override_iteration_counts' in b and 'position_solver_iteration_count' in b
                      and 'velocity_solver_iteration_count' in b for b in result['bodies']))
        check('native inspected exact profile physics and mesh '+letter, all(package(result[key]) == paths[key] for key in paths))
        check('probe dirty state unchanged '+letter, dirty() == before_dirty)
        jobs.append((profile, physics, row))
    R['candidate_valid'] = all(row['preflight'].get('candidate_valid') for _, _, row in jobs)
    check('all probe input bytes unchanged', all(sha(p) == h for p, h in protected.items()), len(protected))
    if APPLY:
        # A prior real probe is mandatory; newest successful evidence is explicit in this report.
        probe_file = None
        for file in sorted((DOC/'editor_runtime').glob('author_*_NPCPhysicsProbe/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
            data = json.loads(file.read_text(encoding='utf-8'))
            if data.get('status') == 'PASS': probe_file = file; probe = data; break
        check('real prior probe exists', probe_file is not None)
        R['probe_evidence'] = dict(path=str(probe_file), sha256=sha(probe_file))
        check('same native DLL and helper source as probe', probe.get('code_binding') == R['code_binding'])
        check('prior probe contains exactly Q/R', [a.get('sample') for a in probe.get('assets', [])] == ['Q', 'R'])
        check('current candidate has no native blockers', R['candidate_valid'],
              {row['sample']:row['preflight'].get('candidate_blockers') for _, _, row in jobs})
        for (_, _, row), previous in zip(jobs, probe['assets']):
            check('exact prior probe asset binding '+row['sample'], previous.get('binding') == row['binding'])
            check('native candidate identical to prior probe '+row['sample'], previous.get('preflight') == row['preflight'])
        backup = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/OUT.name
        backup.mkdir(parents=True, exist_ok=False)
        # Back up both original PHYS packages before either mutation or save.
        for _, _, row in jobs:
            original = disk(row['binding']['physics']['path']); copy = backup/(row['sample']+'_'+original.name)
            shutil.copy2(original, copy)
            check('byte exact private PHYS backup '+row['sample'], sha(copy) == row['binding']['physics']['sha256'])
            row['backup'] = dict(path=str(copy), sha256=sha(copy)); dump()
        for profile, physics, row in jobs:
            row['native_apply'] = native(profile, physics, True); dump()
            check('bounded native candidate apply '+row['sample'], row['native_apply'].get('status') == 'PASS'
                  and row['native_apply'].get('applied') and row['native_apply'].get('native_candidate_readback_checked'),
                  row['native_apply'].get('error'))
        # No target is saved until both native apply/readbacks have passed.
        for profile, physics, row in jobs:
            check('save only evidenced PHYS '+row['sample'], A.save_loaded_asset(physics, False))
            file = disk(row['binding']['physics']['path'])
            row['after_save'] = dict(path=row['binding']['physics']['path'], sha256=sha(file), bytes=file.stat().st_size,
                                     native=native(profile, physics))
            check('native post-save inspection '+row['sample'], row['after_save']['native'].get('status') == 'PASS')
            protected.pop(str(file)); dump()
        R['pass_scope'] = 'Only two PHYS saved after probe binding, backup and native readback. All existing humanoid capsules rebuilt from skin-only points/reference landmarks with explicit hidden-skin fallbacks; parent-frame anatomical limits applied without changing hinge signs or solver settings. Source mesh/material/animation unchanged. Dynamic stability, skin coverage and natural fall pose NOT_RUN.'
    else:
        R['pass_scope'] = 'Detailed native read-only probe and candidate blocker reporting only; no mutation/save. Candidate validity is separate from probe completion.'
    check('all non-target package bytes preserved', all(sha(p) == h for p, h in protected.items()), len(protected))
    check('no maps dirtied', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc(); U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS': raise RuntimeError('Q/R physics probe/repair failed; see author_result.json')
