"""Exact five new GAS actions, then one separate Sprint in-place working copy.

Root runs GASRecoverySourceDryRun -> GASRecoverySourceApply -> GASSprintInPlace.
No source/sample BP execution, maps, NPC/hero binding, or old-asset save.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample')
PROBE = DOC / 'editor_runtime/probe_20260924_123631_452_88d3fc38_GASGetUpSprintReadOnly/author_result.json'
P0 = '/Game/HarborCity/M5VS2/GASSourceP0'
NAMES = tuple('M_ragdoll_getup_stand_' + d for d in ('B', 'F', 'L', 'R')) + ('M_Relaxed_Sprint_Loop_F',)
OWNER = 'HarborCity_M5_VS2_GAS_Recovery_v1'
A = U.EditorAssetLibrary


def arg(name):
    match = re.findall(r'(?:^|\s)-' + name + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(match) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return match[0][0] or match[0][1]


OUT, PHASE = Path(arg('M5EvidenceDir')).resolve(), arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and OUT != DOC and not (OUT / 'author_result.json').exists()
assert PHASE in ('GASRecoverySourceDryRun', 'GASRecoverySourceApply', 'GASSprintInPlace')
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase=PHASE, checks=[], assets=[], source_writes=0, source_blueprint_execution=False,
         runtime='NOT_RUN', visual='NOT_RUN', binding='NOT_RUN', started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
protected = {}


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    dump()
    if not ok:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda: f.read(1024*1024), b''):
            h.update(data)
    return h.hexdigest()


def disk(path, content=PROJECT/'Content'):
    assert path.startswith('/Game/') and '..' not in path
    return content / Path(path.removeprefix('/Game/').split('.')[0]).with_suffix('.uasset')


def latest(phase):
    for file in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'), key=lambda p:p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text('utf-8-sig'))
        if data.get('status') == 'PASS' and data.get('phase') == phase:
            return file, data
    raise RuntimeError('No successful native ' + phase)


try:
    check('exact HarborCity project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no dirty maps', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('actual original-frame source probe', (probe := json.loads(PROBE.read_text('utf-8-sig')))['status'] == 'PASS'
          and [c['path'].rsplit('/',1)[1] for c in probe['clips']] == list(NAMES))
    for c in probe['clips']:
        file = disk(c['path'], SOURCE/'Content')
        check('original source still matches native probe '+c['path'], sha(file) == c['source_sha256'])
        protected[str(file)] = c['source_sha256']
    p0 = json.loads((DOC/'research/GAS_P0_SOURCE_MANIFEST.json').read_text('utf-8-sig'))
    check('real saved clean P0 dependencies', p0['status'] == 'PASS')
    for row in p0['assets']:
        check('old clean P0 bytes preserved '+row['path'], sha(disk(row['path'])) == row['sha256'])
        protected[str(disk(row['path']))] = row['sha256']
    for file in (PROJECT/'Config').glob('*.ini'):
        protected[str(file)] = sha(file)
    prior_path = ''
    if PHASE == 'GASRecoverySourceDryRun':
        root = '/Game/HarborCity/M5VS2/GASRecoverySource/Batch_' + hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
    else:
        dependency = 'GASRecoverySourceDryRun' if PHASE == 'GASRecoverySourceApply' else 'GASRecoverySourceApply'
        file, prior = latest(dependency)
        root = prior['destination']; prior_path = str(file)
        R['prerequisite'] = dict(path=str(file), sha256=sha(file))
        check('exact unique owned batch format', re.fullmatch(r'/Game/HarborCity/M5VS2/GASRecoverySource/Batch_[0-9a-f]{12}', root) is not None)
        if PHASE == 'GASRecoverySourceApply':
            check('actual native dryrun only', not prior['native']['apply'] and prior['native']['destination'] == root)
            closure = prior['native']['source_dependency_packages']
            check('bounded discovered closure', 7 <= len(closure) <= 1000 and len(set(closure)) == len(closure))
            for package in closure:
                check('source closure stays in inspected submounts', package.startswith(tuple('/Game/'+p for p in ('Characters/UEFN_Mannequin/', 'Audio/', 'Blueprints/', 'Misc/'))), package)
                file = disk(package, SOURCE/'Content'); protected[str(file)] = sha(file)
        else:
            check('five exact clean-source outputs', {r['path'] for r in prior['assets']} == {root+'/Animations/'+n for n in NAMES})
            for row in prior['assets']:
                check('saved new source bytes '+row['path'], sha(disk(row['path'])) == row['sha256'])
                protected[str(disk(row['path']))] = row['sha256']
    R['destination'] = root
    expected = ([root+'/Working/M_Relaxed_Sprint_Loop_F_InPlace'] if PHASE == 'GASSprintInPlace'
                else [root+'/Animations/'+n for n in NAMES])
    check('fresh output packages only', all(not disk(p).exists() for p in expected), expected)
    (OUT/'protected_hashes.json').write_text(json.dumps(protected,ensure_ascii=False,indent=2),encoding='utf-8')
    if PHASE == 'GASSprintInPlace':
        R['preflight'] = json.loads(U.HCM5VS2GASAnimationEditor.create_sprint_in_place(root, False))
        check('Sprint original root preflight', R['preflight'].get('status') == 'PASS', R['preflight'].get('error'))
        native = U.HCM5VS2GASAnimationEditor.create_sprint_in_place(root, True)
    else:
        native = U.HCM5VS2GASAnimationEditor.prepare_recovery_source(str(PROBE), root, prior_path, PHASE.endswith('Apply'))
    R['native'] = json.loads(native)
    check('native bounded source operation', R['native'].get('status') == 'PASS', R['native'].get('error'))
    if PHASE != 'GASRecoverySourceDryRun':
        U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root], force_rescan=True)
        for package in expected:
            asset = A.load_asset(package)
            check('native saved sequence and ownership '+package, isinstance(asset,U.AnimSequence) and disk(package).is_file()
                  and A.get_metadata_tag(asset,'HarborCityOwnedBy') == OWNER)
            check('clean P0 skeleton binding', asset.get_editor_property('skeleton').get_path_name().split('.')[0] == P0+'/SK_GAS_UEFN_P0')
            if PHASE == 'GASSprintInPlace':
                check('working Sprint root-motion extraction disabled', not asset.get_editor_property('enable_root_motion') and not asset.get_editor_property('force_root_lock'))
            else:
                check('original getup/Sprint root flags retained', asset.get_editor_property('enable_root_motion') and asset.get_editor_property('force_root_lock'))
            R['assets'].append(dict(path=package,sha256=sha(disk(package)),bytes=disk(package).stat().st_size))
    check('no map save requested', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc()
finally:
    changed = [p for p,h in protected.items() if not Path(p).is_file() or sha(p) != h]
    R['source_preservation'] = dict(files_checked=len(protected), changed_files=changed)
    if changed:
        R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('GAS recovery source author failed; preserve unique outputs and evidence')
