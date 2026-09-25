"""Ten exact GAS transition sources/cuts; no retarget, graph or gameplay binding.

Root alone runs native phases GASTransitionAssetsDryRun, GASTransitionAssetsApply,
then GASTransitionAssetsReload in fresh serial commandlets. Output is 20 small
owned sequences inside the project on D:, reusing the existing clean P0 rig.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT, DOC = WORK/'HarborCity', WORK/'docs/HarborCity_M5_VS2'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample/Content')
PROBE = DOC/'editor_runtime/probe_20260924_153126_945_90723b8c_GASTransitionReadOnly/author_result.json'
PLAN = DOC/'research/20260924_90723b8c_GAS_TRANSITION_CUT_PROPOSAL.json'
P0 = '/Game/HarborCity/M5VS2/GASSourceP0'
OWNER = 'HarborCity_M5_VS2_GASTransition_v1'
SELECTED = (
    ('Jump/M_Relaxed_Jump_F_Start_Stand_Rfoot', 18, 35),
    ('Jump/M_Relaxed_Jump_F_Start_Run_Rfoot', 10, 28),
    ('Jump/M_Relaxed_Jump_F_Start_Sprint_Rfoot', 8, 25),
    ('Jump/M_Relaxed_Jump_Loop_Fall', 0, 100),
    ('Jump/M_Relaxed_Jump_F_Land_Stand_Light_Rfoot', 15, 45),
    ('Jump/M_Relaxed_Jump_F_Land_Run_Light_Rfoot', 15, 30),
    ('Run/M_Relaxed_Run_Start_F_Lfoot', 0, 13),
    ('Sprint/M_Relaxed_Sprint_Start_F_Lfoot', 0, 18),
    ('Run/M_Relaxed_Run_Stop_F_Lfoot', 43, 66),
    ('Sprint/M_Relaxed_Sprint_Stop_F_Lfoot', 46, 66),
)
A = U.EditorAssetLibrary


def arg(name):
    values = re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(values) != 1:
        raise RuntimeError('Exactly one '+name+' required')
    return values[0][0] or values[0][1]


OUT, PHASE = Path(arg('M5EvidenceDir')).resolve(), arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
assert PHASE in ('GASTransitionAssetsDryRun', 'GASTransitionAssetsApply', 'GASTransitionAssetsReload')
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase=PHASE, checks=[], inputs={}, assets=[], source_writes=0,
         source_blueprint_execution=False, graph_writes=0, skeleton_writes=0, map_writes=0,
         retarget='NOT_RUN', binding='NOT_RUN', runtime='NOT_RUN', visual='NOT_RUN',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(label, condition, observed=None):
    R['checks'].append(dict(name=label, status='PASS' if condition else 'FAIL', observed=observed))
    dump()
    if not condition:
        raise RuntimeError(label+': '+repr(observed))


def read(path):
    return json.loads(Path(path).read_text('utf-8-sig'))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda: f.read(1024*1024), b''):
            h.update(data)
    return h.hexdigest()


def disk(package, content=PROJECT/'Content'):
    assert package.startswith('/Game/') and '..' not in package
    return content/Path(package.removeprefix('/Game/').split('.')[0]).with_suffix('.uasset')


def protect(path, expected=None):
    path = Path(path).resolve()
    digest = sha(path)
    check('protected input hash '+path.name, expected is None or digest == expected)
    PROTECTED[str(path)] = digest


def input_file(name, path):
    path = Path(path).resolve()
    R['inputs'][name] = dict(path=str(path), sha256=sha(path))


def latest(phase):
    for p in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'), key=lambda p:p.stat().st_mtime, reverse=True):
        data = read(p)
        if data.get('status') == 'PASS' and data.get('phase') == phase:
            return p, data
    raise RuntimeError('No successful native '+phase)


def originals():
    return [('/Game/Characters/UEFN_Mannequin/Animations/'+suffix, first, last)
            for suffix, first, last in SELECTED]


try:
    check('exact loaded project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no dirty maps', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    probe, plan = read(PROBE), read(PLAN)
    check('native read-only original source proof', probe['status'] == 'PASS' and probe['asset_writes'] == 0 and len(probe['clips']) == 31)
    check('unapplied source-cut proposal', plan['status'] == 'PROPOSAL_NOT_APPLIED')
    input_file('source_probe', PROBE)
    input_file('cut_plan', PLAN)
    input_file('author_script', WORK/'tools/ue_m5_vs2_gas_transition_assets_author.py')
    input_file('native_helper', PROJECT/'Source/HarborCity/M5VS2/HCM5VS2GASTransitionAssetsEditor.cpp')
    input_file('native_header', PROJECT/'Source/HarborCity/M5VS2/HCM5VS2GASTransitionAssetsEditor.h')
    by_path = {c['path']: c for c in probe['clips']}
    planned = {c['source_path']: c for c in plan['selections']}
    for package, first, last in originals():
        row = planned[package]
        check('exact reviewed native frames '+package, row['original_frame_range_inclusive'] == [first, last]
              and row['original_rate_hz'] == 30 and row['source_sha256'] == by_path[package]['source_sha256'])
        protect(disk(package, SOURCE), by_path[package]['source_sha256'])
    p0_manifest = DOC/'research/GAS_P0_SOURCE_MANIFEST.json'
    p0 = read(p0_manifest)
    check('existing saved P0 dependency proof', p0['status'] == 'PASS')
    input_file('p0_manifest', p0_manifest)
    for row in p0['assets']:
        protect(disk(row['path']), row['sha256'])
    for f in (PROJECT/'Config').glob('*.ini'):
        PROTECTED[str(f.resolve())] = sha(f)
    for package in ('/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics',
                    '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun'):
        protect(disk(package))

    prior_path = ''
    if PHASE == 'GASTransitionAssetsDryRun':
        root = '/Game/HarborCity/M5VS2/GASTransitionSource/Batch_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
    else:
        dependency = 'GASTransitionAssetsDryRun' if PHASE == 'GASTransitionAssetsApply' else 'GASTransitionAssetsApply'
        prior_file, prior = latest(dependency)
        check('same native author inputs as prerequisite', prior['inputs'] == R['inputs'])
        root = prior['destination']
        prior_path = str(prior_file)
        R['prerequisite'] = dict(path=prior_path, sha256=sha(prior_file))
        for source_file, digest in prior['source_closure_hashes'].items():
            source_path = Path(source_file).resolve()
            check('prior source closure physical scope', source_path.is_relative_to(SOURCE.resolve()))
            protect(source_path, digest)
        if PHASE == 'GASTransitionAssetsReload':
            check('actual previous 20 asset writes', len(prior['assets']) == 20 and prior['native']['saved_count'] == 20)
            for row in prior['assets']:
                protect(disk(row['path']), row['sha256'])
    check('fresh owned batch syntax', re.fullmatch(r'/Game/HarborCity/M5VS2/GASTransitionSource/Batch_[0-9a-f]{12}', root) is not None)
    R['destination'] = root
    source_packages = [root+'/Animations/'+Path(s).name for s, _, _ in SELECTED]
    working_packages = [root+'/Working/'+Path(s).name+'_Cut_InPlace' for s, _, _ in SELECTED]
    expected = source_packages+working_packages
    if PHASE != 'GASTransitionAssetsReload':
        check('no existing or partial output is overwritten', all(not disk(p).exists() for p in expected))
    check('new native helper exists', hasattr(U, 'HCM5VS2GASTransitionAssetsEditor'))
    if PHASE == 'GASTransitionAssetsReload':
        native_text = U.HCM5VS2GASTransitionAssetsEditor.inspect_transition_assets(root)
    else:
        native_text = U.HCM5VS2GASTransitionAssetsEditor.prepare_transition_assets(str(PROBE), str(PLAN), root, prior_path, PHASE.endswith('Apply'))
    R['native'] = json.loads(native_text)
    check('native bounded operation', R['native'].get('status') == 'PASS', R['native'].get('error'))
    if PHASE == 'GASTransitionAssetsReload':
        R['source_closure_hashes'] = prior['source_closure_hashes']
    else:
        closure = R['native']['source_dependency_packages']
        check('bounded causal source closure', 12 <= len(closure) <= 1000 and len(set(closure)) == len(closure))
        R['source_closure_hashes'] = {}
        for package in closure:
            check('source closure in inspected submounts', package.startswith(tuple('/Game/'+p for p in ('Characters/UEFN_Mannequin/', 'Audio/', 'Blueprints/', 'Misc/'))), package)
            f = disk(package, SOURCE).resolve()
            R['source_closure_hashes'][str(f)] = sha(f)
        if PHASE == 'GASTransitionAssetsApply':
            check('exact dry/apply source closure hashes', R['source_closure_hashes'] == prior['source_closure_hashes'])
    if PHASE != 'GASTransitionAssetsDryRun':
        for package in expected:
            asset = A.load_asset(package)
            check('saved owned sequence '+package, isinstance(asset, U.AnimSequence) and disk(package).is_file()
                  and A.get_metadata_tag(asset, 'HarborCityOwnedBy') == OWNER)
            check('clean skeleton/rate '+package, asset.get_editor_property('skeleton').get_path_name().split('.')[0] == P0+'/SK_GAS_UEFN_P0'
                  and asset.get_editor_property('rate_scale') == 1.0)
            if package in working_packages:
                check('working root extraction disabled '+package, not asset.get_editor_property('enable_root_motion')
                      and not asset.get_editor_property('force_root_lock'))
            R['assets'].append(dict(path=package, sha256=sha(disk(package)), bytes=disk(package).stat().st_size))
    check('maps remain untouched', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
finally:
    changed = [p for p, digest in PROTECTED.items() if not Path(p).is_file() or sha(p) != digest]
    R['source_preservation'] = dict(files_checked=len(PROTECTED), changed_files=changed)
    if changed:
        R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
    U.log('M5VS2 '+PHASE+' '+R['status']+' '+str(OUT))
if R['status'] != 'PASS':
    raise RuntimeError('GAS transition assets operation failed; preserve evidence and any unique partial outputs')
