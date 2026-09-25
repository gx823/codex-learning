"""Root-scheduled HarborCity native GAS P0 preparation; four explicit phases.

Use existing ue_m5_vs2_author_run.ps1 with GASSourceDryRun/GASSourceApply or
GASInPlaceDryRun/GASInPlaceApply. Each phase uses a fresh editor process. Never
execute this in the sample project; original sample assets are read only.
"""
from pathlib import Path
import datetime
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
DOC = WORK / 'docs/HarborCity_M5_VS2'
CONTENT = WORK / 'HarborCity/Content'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample')
PROBE = DOC / 'editor_runtime/probe_20260924_000021_400_446619eb_GASReadOnly/author_result.json'
ROOT = '/Game/HarborCity/M5VS2/GASSourceP0'
INPLACE = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/InPlace'
OWNER = 'HarborCity_M5_VS2_GAS_P0_v1'
A = U.EditorAssetLibrary


def arg(name):
    values = re.findall(r'(?:^|\s)-' + name + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(values) != 1:
        raise RuntimeError('Exactly one -' + name + ' required')
    return values[0][0] or values[0][1]


OUT = Path(arg('M5EvidenceDir')).resolve()
PHASE = arg('M5AuthorPhase')
if not OUT.is_relative_to(DOC.resolve()) or (OUT / 'author_result.json').exists():
    raise RuntimeError('Fresh VS2 report directory required')
if PHASE not in ('GASSourceDryRun', 'GASSourceApply', 'GASInPlaceDryRun', 'GASInPlaceApply'):
    raise RuntimeError('Unexpected GAS native source phase')
OUT.mkdir(parents=True, exist_ok=True)
IS_SOURCE = PHASE.startswith('GASSource')
APPLY = PHASE.endswith('Apply')
MANIFEST = DOC / 'research' / ('GAS_P0_SOURCE_MANIFEST.json' if IS_SOURCE else 'GAS_P0_INPLACE_MANIFEST.json')
R = dict(status='RUNNING', phase=PHASE, started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         checks=[], assets=[], source_protection={}, visual='NOT_RUN', retarget='NOT_RUN',
         gameplay_binding='NOT_RUN', protected_model_source='Native IAnimationDataModel exact track data, not resampled AnimPose output')
protected = {}


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, valid, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if valid else 'FAIL', observed=observed))
    dump()
    if not valid:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def disk(package, content=CONTENT):
    return content / Path(package.removeprefix('/Game/')).with_suffix('.uasset')


try:
    actual = Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()
    check('only current HarborCity project', actual == (WORK / 'HarborCity').resolve(), str(actual))
    check('real new native capability available', hasattr(U, 'HCM5VS2GASAnimationEditor'))
    check('unique stable manifest not overwritten', not APPLY or not MANIFEST.exists(), str(MANIFEST))
    probe = json.loads(PROBE.read_text(encoding='utf-8-sig'))
    check('actual successful source probe', probe['status'] == 'PASS' and len(probe['candidate_inspections']) == 5)
    names = [p['path'].rsplit('/', 1)[1] for p in probe['candidate_inspections']]
    expected_source = [ROOT + '/SK_GAS_UEFN_P0', ROOT + '/SKM_GAS_UEFN_P0'] + [ROOT + '/Animations/' + n for n in names]
    expected = expected_source if IS_SOURCE else [INPLACE + '/' + n + '_InPlace' for n in names]
    if IS_SOURCE:
        deps = probe['migration_dependency_preview']['package_paths']
        check('fixed complete original dependency read scope', len(deps) == 301 and probe['migration_dependency_preview']['status'] == 'COMPLETE_METADATA_ONLY')
        protected = {str(disk(p, SOURCE / 'Content')): sha(disk(p, SOURCE / 'Content')) for p in deps}
        for p in probe['candidate_inspections']:
            check('selected original clip hash still matches native probe ' + p['path'],
                  protected[str(disk(p['path'], SOURCE / 'Content'))] == p['source_sha256_after'])
    else:
        source_manifest = DOC / 'research/GAS_P0_SOURCE_MANIFEST.json'
        src = json.loads(source_manifest.read_text(encoding='utf-8-sig'))
        check('actual seven-package source preparation completed', src['status'] == 'PASS' and {x['path'] for x in src['assets']} == set(expected_source))
        protected = {str(disk(x['path'])): x['sha256'] for x in src['assets']}
        check('clean native source bytes match saved manifest', all(sha(p) == h for p, h in protected.items()))
    # Guard existing target skeleton/mesh and historical animation data without
    # saving or changing any of those objects.
    for folder in ('HarborCity/M5VS1/HeroSelestia', 'HarborCity/M4R2/Animation', 'HarborCity/M4/Animation'):
        for p in (CONTENT / folder).rglob('*.uasset'):
            protected[str(p)] = sha(p)
    R['source_protection']['files'] = len(protected)
    R['source_protection']['before_sha256_file'] = str(OUT / 'protected_hashes.json')
    (OUT / 'protected_hashes.json').write_text(json.dumps(protected, ensure_ascii=False, indent=2), encoding='utf-8')
    check('new output paths absent on disk', all(not disk(p).exists() for p in expected), expected)
    if IS_SOURCE:
        native = U.HCM5VS2GASAnimationEditor.prepare_source_p0(str(PROBE), APPLY)
    else:
        native = U.HCM5VS2GASAnimationEditor.create_in_place_p0(APPLY)
    R['native'] = json.loads(native)
    check('native exact P0 operation completed', R['native'].get('status') == 'PASS', R['native'].get('error'))
    if APPLY:
        U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT, INPLACE], force_rescan=True)
        for package in expected:
            obj = A.load_asset(package)
            check('saved P0 asset reload and ownership ' + package,
                  obj is not None and disk(package).is_file() and A.get_metadata_tag(obj, 'HarborCityOwnedBy') == OWNER)
            R['assets'].append(dict(path=package, class_name=obj.get_class().get_name(), bytes=disk(package).stat().st_size, sha256=sha(disk(package))))
        check('exact bounded output count', len(R['assets']) == (7 if IS_SOURCE else 5))
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
finally:
    changed = [p for p, h in protected.items() if not Path(p).is_file() or sha(p) != h]
    R['source_protection']['changed_files'] = changed
    R['source_protection']['status'] = 'PASS' if not changed and protected else 'FAIL_OR_NOT_ESTABLISHED'
    if changed:
        R['status'] = 'FAIL'
    R['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    dump()
    if R['status'] == 'PASS' and APPLY:
        MANIFEST.write_text(json.dumps(dict(status='PASS', phase=PHASE, evidence=str(OUT), assets=R['assets'],
                         source_probe_sha256=sha(PROBE)), ensure_ascii=False, indent=2), encoding='utf-8')
if R['status'] != 'PASS':
    raise RuntimeError('GAS P0 native operation failed; preserve the report and any new attempt packages')
