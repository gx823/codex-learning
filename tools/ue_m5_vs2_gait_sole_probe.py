"""Read-only native LOD0 shoe candidates for the exact completed Clock Hero.

No asset save, map authoring, vertex selection, animation change or game launch.
Root runs this after compiling the staged native diagnostics helper.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK/'HarborCity'
DOC = WORK/'docs/HarborCity_M5_VS2'
CLOCK_AUTHOR = DOC/'editor_runtime/author_20260925_084107_824_49de361c_GASUnifiedClockAuthor/author_result.json'
CLOCK_RELOAD = DOC/'editor_runtime/author_20260925_084154_358_cc64de19_GASUnifiedClockReload/author_result.json'


def argument(name):
    found = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(found) == 1, 'Exactly one '+name+' required'
    return found[0][0] or found[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
PHASE = argument('M5AuthorPhase')
assert PHASE == 'GaitSoleGeometryProbe'
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
R = dict(schema='HarborCity.M5VS2.GaitSoleGeometry.Probe.v1', phase=PHASE, status='RUNNING',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), checks=[], assets=[], inputs={},
         source_preservation={}, runtime='NOT_RUN', sole_selection='NOT_RUN', visual_acceptance='USER_REVIEW')
PROTECTED = {}


def sha(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for data in iter(lambda: stream.read(1024*1024), b''):
            value.update(data)
    return value.hexdigest()


def document(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, okay, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if okay else 'FAIL', observed=observed))
    dump()
    if not okay:
        raise RuntimeError(name+': '+repr(observed))


def protect(path, expected=None):
    path = Path(path).resolve()
    check('owned existing input '+str(path), path.is_relative_to(WORK) and path.is_file())
    value = sha(path)
    check('unchanged input '+str(path), expected is None or value == expected.lower())
    PROTECTED[str(path)] = value


def package(value):
    return (value.get_path_name() if hasattr(value, 'get_path_name') else str(value)).split('.')[0]


def asset_file(value, is_map=False):
    value = package(value)
    assert value.startswith('/Game/HarborCity/M5VS2/') and '..' not in value
    return PROJECT/'Content'/Path(value.removeprefix('/Game/')).with_suffix('.umap' if is_map else '.uasset')


def completed(path, phase):
    protect(path)
    command_path = path.parent/'commandlet.json'
    protect(command_path)
    data, command = document(path), document(command_path)
    check('completed native '+phase, data.get('status') == 'PASS' and data.get('phase') == phase
          and command.get('status') == 'PASS' and command.get('phase') == phase and command.get('exit_code') == 0)
    R['inputs'][phase] = dict(path=str(path), sha256=sha(path), commandlet=str(command_path), commandlet_sha256=sha(command_path))
    return data


try:
    check('actual project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE or dirty assets/maps', not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    author, reload = completed(CLOCK_AUTHOR, 'GASUnifiedClockAuthor'), completed(CLOCK_RELOAD, 'GASUnifiedClockReload')
    check('exact five-asset Clock source and fresh readback', len(author['assets']) == 5
          and reload['author_sha256'] == sha(CLOCK_AUTHOR)
          and Path(reload['author_report']).resolve() == CLOCK_AUTHOR.resolve()
          and reload.get('fresh_process_disk_readback') == 'PASS' and reload['assets'] == author['assets'])
    for row in author['assets']:
        protect(asset_file(row['path'], package(row['path']) == package(author['map'])), row['sha256'])
    # The diagnostic DLL is necessarily newer than the completed Clock author.
    # Revalidate immutable animation/mesh/map inputs; keep historical source/DLL
    # hashes in that report rather than requiring an obsolete module today.
    for path, digest in author['source_preservation']['sha256_before'].items():
        if Path(path).suffix.lower() in ('.uasset', '.umap'):
            protect(path, digest)
    mesh_path = package(author['binding']['mesh'])
    mesh = U.EditorAssetLibrary.load_asset(mesh_path)
    check('actual candidate mesh', isinstance(mesh, U.SkeletalMesh), mesh_path)
    protect(asset_file(mesh_path))
    hero_path = author['clock']['candidate_hero']
    hero = U.load_class(None, hero_path+'.'+hero_path.rsplit('/', 1)[1]+'_C')
    check('actual Clock Hero generated class', hero is not None)
    body = U.get_default_object(hero).get_editor_property('mesh')
    check('exact native body binding', body.get_editor_property('skeletal_mesh_asset') == mesh
          and package(body.get_editor_property('anim_class')) == author['clock']['candidate_blueprint'])
    geometry = json.loads(U.HCM5VS2SoleDiagnostics.inspect_shoe_geometry(mesh))
    geometry_path = OUT/'actual_lod0_shoe_candidates.json'
    geometry_path.write_text(json.dumps(geometry, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    R['native_geometry'] = dict(path=str(geometry_path), sha256=sha(geometry_path))
    check('native geometry read only', geometry.get('status') == 'PASS' and package(geometry['mesh']) == mesh_path
          and geometry.get('lod') == 0 and 12 <= geometry.get('candidate_count', 0) <= 12000)
    check('native candidate count', len(geometry['candidates']) == geometry['candidate_count'])
    R['binding'] = dict(hero=hero_path, mesh=mesh_path, mesh_sha256=sha(asset_file(mesh_path)),
                        animation=author['clock']['candidate_blueprint'], space=author['clock']['candidate_space'],
                        source_map=author['map'], materials=[package(value) for value in body.get_materials()])
    R['module_sha256'] = sha(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll')
    R['module_relation'] = 'New diagnostic module, original Clock assets byte-verified; old author module retained as historical provenance.'
    R['candidate_count'] = geometry['candidate_count']
    R['scope'] = ('Actual native render IDs, default material sections and quantized weights only. '
                  'Candidate envelope may include hidden skin and garments; fixed heel/forefoot selection, '
                  'runtime CPU LBS, final GPU sole contact and video have NOT_RUN.')
    R['status'] = 'PASS'
except Exception as exc:
    R['status'], R['error'], R['traceback'] = 'FAIL', repr(exc), traceback.format_exc()
    U.log_error(R['traceback'])
finally:
    changed = [path for path, digest in PROTECTED.items() if not Path(path).is_file() or sha(path) != digest]
    R['source_preservation'] = dict(sha256_before=PROTECTED, changed=changed)
    if changed:
        R['status'] = 'FAIL'
        R['error'] = 'Read-only geometry probe changed protected sources'
    R['finished_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
    U.log('M5VS2_GAIT_SOLE_GEOMETRY_'+R['status'])
