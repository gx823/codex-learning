"""Read-only original-frame GAS start/stop/turn/jump selection probe.

Exact local GAS project only. Reuses the already executed native raw-pose reader's
function definitions, never its module author body. Does not save, copy, retarget,
instantiate source Blueprints or change HarborCity gameplay. The 31 reads are a
selection pool, not a request to migrate all 31 assets.
"""
from pathlib import Path
import ast
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
DOC = WORK / 'docs/HarborCity_M5_VS2'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample').resolve()
BASE = '/Game/Characters/UEFN_Mannequin/Animations/'
SKELETON = '/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin'
BONES = ('root', 'pelvis', 'spine_05', 'neck_02', 'head', 'upperarm_l', 'upperarm_r',
         'thigh_l', 'thigh_r', 'calf_l', 'calf_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r')
CURVES = ('contact_l', 'contact_r', 'movedata_speed', 'enable_warping', 'phase')
GROUPS = {
    'ground_start_stop': tuple(
        BASE + gait + '/M_Relaxed_' + gait + '_' + event + '_F_' + side + 'foot'
        for gait in ('Run', 'Sprint') for event in ('Start', 'Stop') for side in ('L', 'R')),
    'stationary_turn_candidates': tuple(
        BASE + 'Idle/M_Relaxed_Stand_Turn_' + angle + '_' + side
        for angle in ('090',) for side in ('L', 'R')),
    'moving_turn_candidates_one_foot_phase': tuple(
        BASE + 'Run/M_Relaxed_Run_Turn_' + side + '_090_Lfoot' for side in ('L', 'R')) + tuple(
        BASE + 'Sprint/M_Relaxed_Sprint_Turn_090_' + side + '_Lfoot' for side in ('L', 'R')),
    'jump_candidates': tuple(
        BASE + 'Jump/M_Relaxed_Jump_F_Start_' + gait + '_' + side + 'foot'
        for gait in ('Stand', 'Run', 'Sprint') for side in ('L', 'R')) + tuple(
        BASE + 'Jump/M_Relaxed_Jump_F_Off_Run_' + side + 'foot' for side in ('L', 'R')) + (
        BASE + 'Jump/M_Relaxed_Jump_Loop_Fall',) + tuple(
        BASE + 'Jump/M_Relaxed_Jump_F_Land_' + gait + '_Light_' + side + 'foot'
        for gait in ('Stand', 'Run', 'Sprint') for side in ('L', 'R')),
    'idle_micro_candidates': tuple(BASE + 'Idle/M_Relaxed_Stand_Idle_Break_v0' + n for n in ('1', '2')),
}
SELECTED = tuple(p for paths in GROUPS.values() for p in paths)
A, L, P = U.EditorAssetLibrary, U.AnimationLibrary, U.AnimPoseExtensions
AR = U.AssetRegistryHelpers.get_asset_registry()
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
if len(args) != 1:
    raise RuntimeError('Exactly one -M5EvidenceDir required')
OUT = Path(args[0][0] or args[0][1]).resolve()
if not OUT.is_relative_to(DOC) or OUT == DOC or (OUT / 'author_result.json').exists():
    raise RuntimeError('Fresh evidence directory inside M5-VS2 docs required')
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase='GAS_TRANSITION_READ_ONLY', checks=[], clips=[], asset_writes=0,
         inspected_paths=list(SELECTED), selection_groups=GROUPS,
         runtime='NOT_RUN', visual='NOT_RUN', retarget='NOT_RUN', migration='NOT_RUN',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         limitations=[
             'Raw source-frame readback only; not target retarget, compressed runtime or naturalness approval.',
             'Names are selection hints; contact curves and actual trajectories determine whether a jump contains anticipation.',
             'Authored contact values are not physical ground measurements.',
             'Source foot/pelvis coordinates do not prove target foot locking or sole contact.',
             'No movement velocity, acceleration, camera, first-person or combat changes.',
             'Start/stop clips must be interruptible and evaluated against actual CharacterMovement acceleration/braking.',
             'Root yaw and translation must not be applied a second time on top of CharacterMovement.',
             'Moving-turn candidates initially inspect the Lfoot phase only; their Rfoot partners are not yet evaluated.',
             '31 source reads are not 31 proposed migrated assets.'])
PROTECTED = {}

# The exact native reader passed in the source project. Only top-level function
# definitions are compiled; its SELECTED list and try/finally author are not run.
READER = WORK / 'tools/ue_m5_vs2_gas_getup_sprint_probe.py'
READER_SHA = '34ce89ad85b956ca448c921859dccb6780516b6f58a97ed158f07484b8a3f7d8'
if hashlib.sha256(READER.read_bytes()).hexdigest() != READER_SHA:
    raise RuntimeError('Verified read-only native reader changed; inspect before reuse')
tree = ast.parse(READER.read_text('utf-8-sig'), filename=str(READER))
function_nodes = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
exec(compile(ast.Module(body=function_nodes, type_ignores=[]), str(READER), 'exec'), globals())
BASE_SUMMARIZE = summarize


def unwrap_degrees(values):
    if not values or any(v is None for v in values):
        return None
    result = [float(values[0])]
    for previous, current in zip(values, values[1:]):
        result.append(result[-1] + ((current - previous + 180.) % 360.) - 180.)
    return result


def intervals_where(frames, predicate):
    windows, first = [], None
    for i, frame in enumerate(frames):
        enabled = predicate(frame)
        if enabled and first is None:
            first = i
        if first is not None and (not enabled or i == len(frames)-1):
            last = i if enabled else i-1
            windows.append(dict(first_frame=first, last_frame=last,
                start_seconds=frames[first]['time_seconds'], end_seconds=frames[last]['time_seconds']))
            first = None
    return windows


def summarize(frames, forward):
    result = BASE_SUMMARIZE(frames, forward)
    roots = [f['bones']['root'] for f in frames]
    q0 = roots[0]['quaternion_xyzw']
    # Full four-component quaternion dot and sign-invariant shortest angle.
    result['root_max_rotation_from_first_deg'] = max(math.degrees(2.*math.acos(min(1., abs(
        sum(q0[i]*root['quaternion_xyzw'][i] for i in range(4)))))) for root in roots)
    yaw = unwrap_degrees([heading(qrotate(root['quaternion_xyzw'], forward)) for root in roots])
    root_rows = []
    for i, frame in enumerate(frames):
        root = roots[i]['position_cm']
        previous = max(0, i-1)
        delta_t = frame['time_seconds'] - frames[previous]['time_seconds']
        velocity = mul(sub(root, roots[previous]['position_cm']), 1./delta_t) if delta_t else None
        root_rows.append(dict(frame=i, time_seconds=frame['time_seconds'],
            delta_from_first_cm=sub(root, roots[0]['position_cm']), velocity_cm_s=velocity,
            forward_velocity_cm_s=dot(velocity, forward) if velocity is not None else None,
            yaw_delta_unwrapped_deg=(yaw[i]-yaw[0]) if yaw is not None else None,
            pelvis_above_root_z_cm=frame['bones']['pelvis']['position_cm'][2]-root[2],
            left_toe_above_root_z_cm=frame['bones']['ball_l']['position_cm'][2]-root[2],
            right_toe_above_root_z_cm=frame['bones']['ball_r']['position_cm'][2]-root[2]))
    result['original_frame_motion_profile'] = root_rows
    result['root_signed_yaw_delta_deg'] = yaw[-1]-yaw[0] if yaw is not None else None
    result['both_authored_contacts_below_point2_windows'] = intervals_where(frames,
        lambda f: all(f['curves'].get('contact_'+side) is not None and f['curves']['contact_'+side] < .2 for side in ('l', 'r')))
    result['either_authored_contact_above_point8_windows'] = intervals_where(frames,
        lambda f: any(f['curves'].get('contact_'+side) is not None and f['curves']['contact_'+side] >= .8 for side in ('l', 'r')))
    result['root_z_range_cm'] = [min(t['position_cm'][2] for t in roots), max(t['position_cm'][2] for t in roots)]
    result['pelvis_relative_root_z_range_cm'] = [min(x['pelvis_above_root_z_cm'] for x in root_rows), max(x['pelvis_above_root_z_cm'] for x in root_rows)]
    result['velocity_windows'] = {}
    end = frames[-1]['time_seconds']
    for name, lo, hi in (('first_0_25_seconds', 0., min(.25, end)), ('last_0_25_seconds', max(0., end-.25), end)):
        rows = [x for x in root_rows if x['velocity_cm_s'] is not None and lo <= x['time_seconds'] <= hi]
        result['velocity_windows'][name] = dict(first_time_seconds=rows[0]['time_seconds'] if rows else None,
            last_time_seconds=rows[-1]['time_seconds'] if rows else None, original_frame_samples=len(rows),
            mean_forward_cm_s=sum(x['forward_velocity_cm_s'] for x in rows)/len(rows) if rows else None,
            max_xy_cm_s=max(math.hypot(*x['velocity_cm_s'][:2]) for x in rows) if rows else None)
    for body in ('pelvis', 'chest'):
        values = unwrap_degrees([f['anatomical'][body]['forward_heading_deg'] for f in frames])
        result[body+'_signed_yaw_delta_deg'] = values[-1]-values[0] if values is not None else None
    return result


try:
    require('exact source project directory', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == SOURCE)
    project = SOURCE / 'GameAnimationSample.uproject'
    require('exact locked 5.8 source descriptor', project.is_file() and json.loads(project.read_text('utf-8-sig'))['EngineAssociation'] == '5.8')
    require('actual engine 5.8', U.SystemLibrary.get_engine_version().startswith('5.8'), U.SystemLibrary.get_engine_version())
    require('bounded unique exact selection', len(SELECTED) == 31 and len(set(SELECTED)) == 31)
    protect(project)
    protect(disk(SKELETON))
    R['source_project'] = dict(path=str(project), sha256=sha(project), engine=U.SystemLibrary.get_engine_version())
    R['script_sha256'] = sha(Path(__file__))
    R['read_only_reader'] = dict(path=str(READER), sha256=READER_SHA, executed_scope='Top-level function definitions only')
    R['dirty_packages_before'] = [p.get_path_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    AR.scan_paths_synchronous([BASE + folder for folder in ('Run', 'Sprint', 'Idle', 'Jump')], force_rescan=True)
    # Check all selected real files before evaluating the first pose.
    for path in SELECTED:
        require('exact selected file and registered asset exist', disk(path).is_file() and A.does_asset_exist(path), path)
    for path in SELECTED:
        inspect(path)
        sequence = A.load_asset(path)
        markers = L.get_animation_sync_markers(sequence)
        R['clips'][-1]['authored_sync_markers'] = [dict(name=str(m.get_editor_property('marker_name')),
            time_seconds=float(m.get_editor_property('time'))) for m in markers]
        R['clips'][-1]['selection_group'] = next(k for k, paths in GROUPS.items() if path in paths)
        require('all marker times finite and inside sequence', all(math.isfinite(m['time_seconds']) and
            -.0001 <= m['time_seconds'] <= R['clips'][-1]['duration_seconds']+.0001
            for m in R['clips'][-1]['authored_sync_markers']), path)
        dump()
    R['dirty_packages_after'] = [p.get_path_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    require('loaded content dirty package set unchanged', set(R['dirty_packages_before']) == set(R['dirty_packages_after']),
            dict(before=R['dirty_packages_before'], after=R['dirty_packages_after']))
    require('all explicit source bytes unchanged', all(sha(path) == digest for path, digest in PROTECTED.items()))
    require('native read-only reader remains unchanged', sha(READER) == READER_SHA)
    R['protected_source_hashes'] = PROTECTED
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('GAS transition read-only probe failed; keep author_result.json')
