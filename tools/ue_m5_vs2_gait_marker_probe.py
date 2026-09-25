"""Read native authored marker tables for the exact normalized 28-sample BS.

No marker creation, edits, saves, validation mutators or runtime launches.
The GAS-source entries are the previously verified project-owned native copies;
this is not a claim that a second Unreal project was opened by this script.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT, DOC = WORK/'HarborCity', WORK/'docs/HarborCity_M5_VS2'
AUTHOR = DOC/'editor_runtime/author_20260925_073727_946_eb2086ca_GASNominalWeightAuthor/author_result.json'
SOURCE_REPORT = DOC/'editor_runtime/author_20260924_131312_728_48075629_GASRecoverySourceApply/author_result.json'
PHASE = 'GASGaitMarkerProbe'
A, L = U.EditorAssetLibrary, U.AnimationLibrary


def argument(name):
    values = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(values) == 1, 'Exactly one '+name+' required'
    return values[0][0] or values[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert argument('M5AuthorPhase') == PHASE
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
R = dict(schema='HarborCity.M5VS2.GaitMarkerProbe.v1', phase=PHASE, status='RUNNING',
         checks=[], inputs={}, samples=[], sequences=[], gas_source_copies=[], asset_writes=0,
         runtime='NOT_RUN', started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def document(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, value, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if value else 'FAIL', observed=observed))
    dump()
    if not value:
        raise RuntimeError(name+': '+repr(observed))


def package(value):
    return (value.get_path_name() if hasattr(value, 'get_path_name') else str(value)).split('.')[0]


def disk(path, extension='.uasset'):
    path = package(path)
    assert path.startswith('/Game/HarborCity/M5VS') and '..' not in path
    assert extension in ('.uasset', '.umap')
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix(extension)


def protect(path, expected=None):
    path = Path(path).resolve()
    digest = sha(path)
    check('source hash '+str(path), expected is None or digest == expected.lower())
    PROTECTED[str(path)] = digest


def dirty():
    return sorted(str(p.get_path_name()) for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def sequence(path):
    path = package(path)
    protect(disk(path))
    seq = A.load_asset(path)
    check('native animation '+path, isinstance(seq, U.AnimSequence))
    duration = L.get_sequence_length(seq)
    markers = []
    for marker in L.get_animation_sync_markers(seq):
        row = dict(name=str(marker.get_editor_property('marker_name')),
                   time_seconds=float(marker.get_editor_property('time')))
        check('finite authored marker in actual clip', math.isfinite(row['time_seconds'])
              and 0 <= row['time_seconds'] <= duration and bool(row['name']), row)
        markers.append(row)
    # Retain actual returned order, including any real repeated marker names.
    return dict(path=path, sha256=sha(disk(path)), length_seconds=duration,
                rate_scale=float(seq.get_editor_property('rate_scale')),
                marker_count=len(markers), authored_markers=markers,
                marker_track_index='NOT_AVAILABLE: not exposed by this UE 5.8 Python AnimSyncMarker',
                float_curve_names=[str(x) for x in L.get_animation_curve_names(seq, U.RawCurveTrackTypes.RCT_FLOAT)])


try:
    check('correct project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE', not U.EditorLevelLibrary.get_pie_worlds(False))
    R['dirty_content_before'] = dirty()
    check('no dirty maps', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    protect(AUTHOR)
    author = document(AUTHOR)
    check('exact successful author', author.get('status') == 'PASS' and author.get('phase') == 'GASNominalWeightAuthor')
    commandlet = AUTHOR.parent/'commandlet.json'
    protect(commandlet)
    launch = document(commandlet)
    check('author completed normally', launch.get('status') == 'PASS' and launch.get('exit_code') == 0)
    map_path = package(author['map'])
    check('one exact private map in author manifest',
          re.fullmatch(r'/Game/HarborCity/M5VS2/HeroGaitR2/Run_[0-9a-f]{12}/L_HeroGaitR2', map_path) is not None
          and sum(package(row['path']) == map_path for row in author['assets']) == 1)
    for row in author['assets']:
        protect(disk(row['path'], '.umap' if package(row['path']) == map_path else '.uasset'), row['sha256'])
    R['inputs']['normalized_author'] = dict(path=str(AUTHOR), sha256=sha(AUTHOR))
    space = A.load_asset(author['binding']['blendspace'])
    check('native candidate BlendSpace', isinstance(space, U.BlendSpace))
    original_path = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun'
    protect(disk(original_path))
    for key in ('animation', 'mesh'):
        protect(disk(author['binding'][key]))
    snapshot = json.loads(U.HCM5VS2GASMotionEditor.inspect_motion_candidate(
        A.load_asset(author['binding']['animation']), A.load_asset(author['binding']['mesh']),
        A.load_asset(original_path), space))
    check('native 28 sample readback', snapshot.get('status') == 'PASS' and len(snapshot['samples']) == 28)
    R['native_blendspace'] = dict(path=package(space), native_graph_status=snapshot['status'],
                                  samples=snapshot['samples'])
    by_path = {}
    for sample in snapshot['samples']:
        path = package(sample['animation'])
        if path not in by_path:
            by_path[path] = sequence(path)
        R['samples'].append(dict(sample=sample, sequence=path, marker_count=by_path[path]['marker_count']))
    R['sequences'] = list(by_path.values())
    protect(SOURCE_REPORT)
    recovery = document(SOURCE_REPORT)
    check('verified clean GAS source copy report', recovery.get('status') == 'PASS')
    manifest = DOC/'research/GAS_P0_SOURCE_MANIFEST.json'
    protect(manifest)
    p0 = document(manifest)
    check('verified P0 source copy manifest', p0.get('status') == 'PASS')
    expected = {package(x['path']): x['sha256'] for x in p0['assets'] + recovery['assets']}
    sources = ['/Game/HarborCity/M5VS2/GASSourceP0/Animations/M_Relaxed_'+name
               for name in ('Stand_Idle_Loop', 'Walk_Loop_F', 'Run_Loop_F')]
    sources.append(recovery['destination']+'/Animations/M_Relaxed_Sprint_Loop_F')
    for path in sources:
        check('source copy was in verified manifest '+path, path in expected)
        protect(disk(path), expected[path])
        R['gas_source_copies'].append(sequence(path))
    R['dirty_content_after'] = dirty()
    check('read-only content dirty state preserved', R['dirty_content_after'] == R['dirty_content_before'])
    check('no dirty map after probe', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['source_preservation'] = dict(files_checked=len(PROTECTED),
        changed_files=[p for p, digest in PROTECTED.items() if sha(p) != digest])
    check('all disk source hashes unchanged', not R['source_preservation']['changed_files'])
    R['scope'] = 'Native authored marker tables only. No fabricated markers from contact curves. Mixed-table clock causality still requires a separate private candidate runtime.'
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
    U.log('M5_VS2_GAIT_MARKER_PROBE '+R['status']+' '+str(OUT/'author_result.json'))
