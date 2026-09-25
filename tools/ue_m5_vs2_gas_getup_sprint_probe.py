"""Read-only original-frame probe of five exact installed GAS animations.

Run through ue_m5_vs2_gas_getup_sprint_probe_run.ps1, inside the source GAS
project only. No Blueprint evaluation, controller mutation, asset save or copy.
Component space in this report is the animation's space, not a live actor world.
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
DOC = WORK / 'docs/HarborCity_M5_VS2'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample').resolve()
BASE = '/Game/Characters/UEFN_Mannequin/Animations/'
SELECTED = tuple(BASE + 'Ragdoll/M_ragdoll_getup_stand_' + side for side in ('B', 'F', 'L', 'R')) + (
    BASE + 'Sprint/M_Relaxed_Sprint_Loop_F',)
SKELETON = '/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin'
BONES = ('root', 'pelvis', 'spine_05', 'neck_02', 'head', 'upperarm_l', 'upperarm_r',
         'thigh_l', 'thigh_r', 'calf_l', 'calf_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r')
CURVES = ('contact_l', 'contact_r', 'movedata_speed', 'enable_warping', 'phase')
A, L, P = U.EditorAssetLibrary, U.AnimationLibrary, U.AnimPoseExtensions
AR = U.AssetRegistryHelpers.get_asset_registry()
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
if len(args) != 1:
    raise RuntimeError('Exactly one -M5EvidenceDir required')
OUT = Path(args[0][0] or args[0][1]).resolve()
if not OUT.is_relative_to(DOC) or OUT == DOC or (OUT / 'author_result.json').exists():
    raise RuntimeError('Fresh evidence directory inside M5-VS2 docs required')
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase='GAS_GETUP_SPRINT_READ_ONLY', checks=[], clips=[], asset_writes=0,
         inspected_paths=list(SELECTED), target_foot_measurement='NOT_RUN_SOURCE_PROJECT_ONLY',
         runtime='NOT_RUN', visual='NOT_RUN', retarget='NOT_RUN',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         limitations=['Original-frame source evaluation only; not a rendered or compressed runtime pose test.',
             'Component coordinates are not a spawned character world transform.',
             'Suffix B/F/L/R is never used to classify lying direction.',
             'Source contact curves are authored evidence, not ground collision measurements.',
             'Contact threshold 0.8 is an audit window, not a universal foot-lock rule.',
             'Selestia or Q/R final-world stance velocity must be measured after their own retarget and mesh scale.',
             'No source Blueprint or source gameplay framework is evaluated.'])
PROTECTED = {}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def require(name, success, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if success else 'FAIL', observed=observed))
    if not success:
        dump()
        raise RuntimeError(name + ': ' + repr(observed))


def disk(package):
    if not package.startswith('/Game/Characters/UEFN_Mannequin/') or '..' in package:
        raise RuntimeError('Unexpected source asset path: ' + package)
    return SOURCE / 'Content' / Path(package.removeprefix('/Game/')).with_suffix('.uasset')


def protect(path):
    file = Path(path).resolve()
    if not file.is_relative_to(SOURCE) or not file.is_file():
        raise RuntimeError('Invalid protected source file: ' + str(file))
    PROTECTED[str(file)] = sha(file)


def add(a, b):
    return [a[i] + b[i] for i in range(3)]


def sub(a, b):
    return [a[i] - b[i] for i in range(3)]


def mul(a, amount):
    return [x * amount for x in a]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def length(a):
    return math.sqrt(dot(a, a))


def normalized(a):
    size = length(a)
    if size < 1e-7 or not math.isfinite(size):
        raise RuntimeError('Degenerate anatomical axis')
    return mul(a, 1. / size)


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def qrotate(q, value):
    twice_cross = mul(cross(q[:3], value), 2.)
    return add(value, add(mul(twice_cross, q[3]), cross(q[:3], twice_cross)))


def qinverse(q):
    return [-q[0], -q[1], -q[2], q[3]]


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def transform(value):
    q = value.rotation
    result = dict(position_cm=vec(value.translation), quaternion_xyzw=[float(q.x), float(q.y), float(q.z), float(q.w)],
                  scale=vec(value.scale3d))
    flat = [v for values in result.values() for v in values]
    if not all(math.isfinite(v) for v in flat) or abs(sum(v*v for v in result['quaternion_xyzw']) - 1.) > 1e-3:
        raise RuntimeError('Non-finite transform or non-unit quaternion')
    return result


def heading(axis):
    return math.degrees(math.atan2(axis[1], axis[0])) if math.hypot(axis[0], axis[1]) > .05 else None


def anatomical_axes(bone_pose, reference_bone, basis):
    # Carry the *reference anatomical frame* through the actual bone rotation.
    # No hardcoded bone-local X/Y/Z is treated as chest-forward or pelvis-up.
    qi = qinverse(reference_bone['quaternion_xyzw'])
    q = bone_pose['quaternion_xyzw']
    axes = {name: normalized(qrotate(q, qrotate(qi, direction))) for name, direction in basis.items()}
    return dict(**axes, forward_dot_up=axes['forward'][2], right_dot_up=axes['right'][2],
                cranial_dot_up=axes['up'][2], forward_heading_deg=heading(axes['forward']))


def pose_hint(chest):
    f, r, u = chest['forward_dot_up'], chest['right_dot_up'], chest['cranial_dot_up']
    if u > .7:
        return 'UPRIGHT_OR_UPRIGHT_LEAN'
    if abs(u) > .6 or max(abs(f), abs(r)) < .55:
        return 'OBLIQUE_OR_UNCERTAIN_READ_AXIS_VALUES'
    if abs(f) >= abs(r):
        return 'CHEST_UP_SUPINE_CANDIDATE' if f > 0 else 'CHEST_DOWN_PRONE_CANDIDATE'
    return 'RIGHT_SIDE_UP_LEFT_SIDE_DOWN_CANDIDATE' if r > 0 else 'LEFT_SIDE_UP_RIGHT_SIDE_DOWN_CANDIDATE'


def summarize(frames, forward):
    first, last = frames[0], frames[-1]
    roots = [f['bones']['root']['position_cm'] for f in frames]
    delta = sub(roots[-1], roots[0])
    duration = last['time_seconds'] - first['time_seconds']
    pathxy = sum(math.hypot(*(sub(b, a)[:2])) for a, b in zip(roots, roots[1:]))
    result = dict(root_first_cm=roots[0], root_last_cm=roots[-1], root_net_delta_cm=delta,
                  root_xy_path_cm=pathxy, root_xy_path_speed_cm_s=pathxy/duration,
                  root_signed_forward_speed_cm_s=dot(delta, forward)/duration,
                  first_pose_hint=pose_hint(first['anatomical']['chest']), last_pose_hint=pose_hint(last['anatomical']['chest']),
                  first_anatomical=first['anatomical'], last_anatomical=last['anatomical'],
                  first_pelvis_cm=first['bones']['pelvis']['position_cm'], last_pelvis_cm=last['bones']['pelvis']['position_cm'],
                  root_max_rotation_from_first_deg=0., contact_windows={}, foot_metrics={})
    q0 = first['bones']['root']['quaternion_xyzw']
    result['root_max_rotation_from_first_deg'] = max(math.degrees(2.*math.acos(min(1., abs(dot(q0, f['bones']['root']['quaternion_xyzw']))))) for f in frames)
    # Source world-with-root drift and root-subtracted stance speed are both
    # retained. Neither is substituted for final target-mesh world stride.
    for side in ('l', 'r'):
        key = 'contact_' + side
        windows, begin = [], None
        for index, frame in enumerate(frames):
            active = frame['curves'].get(key) is not None and frame['curves'][key] >= .8
            if active and begin is None:
                begin = index
            if begin is not None and (not active or index == len(frames)-1):
                end = index if active else index-1
                windows.append(dict(first_frame=begin, last_frame=end, start_seconds=frames[begin]['time_seconds'], end_seconds=frames[end]['time_seconds']))
                begin = None
        result['contact_windows'][side] = windows
        for bone in ('foot_' + side, 'ball_' + side):
            rows = []
            for a, b in zip(frames, frames[1:]):
                if a['curves'].get(key) is None or b['curves'].get(key) is None or min(a['curves'][key], b['curves'][key]) < .8:
                    continue
                dt_seconds = b['time_seconds'] - a['time_seconds']
                v = mul(sub(b['bones'][bone]['position_cm'], a['bones'][bone]['position_cm']), 1./dt_seconds)
                vr = mul(sub(b['bones']['root']['position_cm'], a['bones']['root']['position_cm']), 1./dt_seconds)
                rows.append(dict(duration=dt_seconds, source_xy_speed=math.hypot(*v[:2]),
                                 root_removed_backward_speed=-dot(sub(v, vr), forward)))
            time = sum(x['duration'] for x in rows)
            result['foot_metrics'][bone] = dict(stance_intervals=len(rows), stance_seconds=time,
                mean_source_with_root_xy_drift_cm_s=sum(x['source_xy_speed']*x['duration'] for x in rows)/time if time else None,
                mean_root_removed_backward_speed_cm_s=sum(x['root_removed_backward_speed']*x['duration'] for x in rows)/time if time else None,
                calibration_scope='SOURCE_REFERENCE_UNIT_SCALE_ONLY; rotational root compensation and final target scale NOT_APPLIED')
    return result


def inspect(path):
    row = dict(path=path, status='RUNNING')
    R['clips'].append(row)
    file = disk(path)
    protect(file)
    sequence = A.load_asset(path)
    require('exact native AnimSequence', isinstance(sequence, U.AnimSequence) and sequence.get_path_name().split('.')[0] == path, path)
    skeleton = sequence.get_editor_property('skeleton')
    require('exact installed UEFN source skeleton', skeleton is not None and skeleton.get_path_name().split('.')[0] == SKELETON, path)
    require('non-additive source action', sequence.get_editor_property('additive_anim_type') == U.AdditiveAnimationType.AAT_NONE, path)
    # UAnimSequenceBase::DataModelInterface is BlueprintReadOnly; Python's
    # FInterfaceProperty conversion exposes its public BlueprintCallable API.
    # Unlike AnimationLibrary.GetNumKeys, these are ORIGINAL data-model counts.
    model = sequence.get_editor_property('data_model_interface')
    require('actual native data-model interface', model is not None, path)
    fps = model.get_frame_rate()
    num, den = int(fps.numerator), int(fps.denominator)
    frame_count, keys = int(model.get_number_of_frames()), int(model.get_number_of_keys())
    duration = float(model.get_play_length())
    require('bounded original frame domain', num > 0 and den > 0 and 1 <= frame_count < 1000 and keys == frame_count+1
            and 0. < duration <= 20. and abs(duration-frame_count*den/num) < 1e-6,
            dict(path=path, numerator=num, denominator=den, frames=frame_count, keys=keys, duration=duration))
    options = U.AnimPoseEvaluationOptions()
    for key, value in dict(evaluation_type=U.AnimDataEvalType.SOURCE, should_retarget=False,
                          extract_root_motion=False, incorporate_root_motion_into_pose=True,
                          retrieve_additive_as_full_pose=True, evaluate_curves=True).items():
        options.set_editor_property(key, value)
    pose = P.get_anim_pose_at_frame(sequence, 0, options)
    require('valid original-frame pose and required real bones', P.is_valid(pose) and set(BONES) <= {str(x) for x in P.get_bone_names(pose)}, path)
    refs = {bone: transform(P.get_ref_bone_pose(pose, U.Name(bone), U.AnimPoseSpaces.WORLD)) for bone in BONES}
    refpos = lambda bone: refs[bone]['position_cm']
    up = normalized(sub(refpos('head'), refpos('pelvis')))
    right = sub(refpos('thigh_r'), refpos('thigh_l'))
    right = normalized(sub(right, mul(up, dot(up, right))))
    forward = normalized(cross(right, up))
    toes = add(sub(refpos('ball_l'), refpos('foot_l')), sub(refpos('ball_r'), refpos('foot_r')))
    toes = normalized(sub(toes, mul(up, dot(up, toes))))
    require('anatomical chirality independently agrees with toes', dot(forward, toes) > .5,
            dict(path=path, forward=forward, projected_toes=toes, dot=dot(forward, toes)))
    basis = dict(forward=forward, right=right, up=up)
    curve_names = [str(x) for x in L.get_animation_curve_names(sequence, U.RawCurveTrackTypes.RCT_FLOAT)]
    active_curves = [name for name in CURVES if name in curve_names]
    curve_keys = {}
    for name in active_curves:
        times, values = L.get_float_keys(sequence, U.Name(name))
        require('curve key arrays match', len(times) == len(values) and len(times) < 5000, dict(path=path, curve=name))
        curve_keys[name] = dict(times_seconds=[float(x) for x in times], values=[float(x) for x in values])
    frames = []
    for index in range(keys):
        pose = P.get_anim_pose_at_frame(sequence, index, options)
        if not P.is_valid(pose):
            raise RuntimeError('Invalid original frame ' + str(index) + ' in ' + path)
        time = index*den/num
        bones = {bone: transform(P.get_bone_pose(pose, U.Name(bone), U.AnimPoseSpaces.WORLD)) for bone in BONES}
        frames.append(dict(frame=index, time_seconds=time, bones=bones,
            anatomical=dict(chest=anatomical_axes(bones['spine_05'], refs['spine_05'], basis), pelvis=anatomical_axes(bones['pelvis'], refs['pelvis'], basis)),
            curves={name: float(L.get_float_value_at_time(sequence, U.Name(name), time)) if name in active_curves else None for name in CURVES}))
    summary = summarize(frames, forward)
    trajectory = dict(schema_version=1, source_path=path, source_sha256=PROTECTED[str(file.resolve())],
        source_skeleton=SKELETON, original_frame_rate=dict(numerator=num, denominator=den), duration_seconds=duration,
        reference_bones=refs, anatomical_reference_basis=basis, float_curve_keys=curve_keys, frames=frames,
        pose_evaluation='AnimPoseExtensions.GetAnimPoseAtFrame: original DataModel frame; SOURCE; retarget false; root extraction false; ignore root lock true',
        coordinate_space='source skeleton component space, centimeters, unit mesh scale; not actor world')
    output = OUT / (path.rsplit('/', 1)[1] + '_frames.json')
    output.write_text(json.dumps(trajectory, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    row.update(status='PASS', skeleton=SKELETON, original_frame_rate=dict(numerator=num, denominator=den), original_frames=frame_count,
        original_keys=keys, sampled_keys=int(L.get_num_keys(sequence)), sampled_frames=int(L.get_num_frames(sequence)),
        duration_seconds=duration, rate_scale=float(L.get_rate_scale(sequence)), enable_root_motion=bool(sequence.get_editor_property('enable_root_motion')),
        force_root_lock=bool(sequence.get_editor_property('force_root_lock')), root_motion_root_lock=str(sequence.get_editor_property('root_motion_root_lock')),
        float_curve_names=curve_names, summary=summary,
        trajectory_file=dict(path=str(output), sha256=sha(output), frames=len(frames)), source_sha256=PROTECTED[str(file.resolve())])
    require('inspected sequence bytes unchanged', sha(file) == row['source_sha256'], path)
    dump()


try:
    require('exact source project directory', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == SOURCE)
    project = SOURCE / 'GameAnimationSample.uproject'
    require('exact locked 5.8 source descriptor', project.is_file() and json.loads(project.read_text('utf-8-sig'))['EngineAssociation'] == '5.8')
    require('actual engine 5.8', U.SystemLibrary.get_engine_version().startswith('5.8'), U.SystemLibrary.get_engine_version())
    protect(project)
    protect(disk(SKELETON))
    R['source_project'] = dict(path=str(project), sha256=sha(project), engine=U.SystemLibrary.get_engine_version())
    R['script_sha256'] = sha(Path(__file__))
    R['dirty_packages_before'] = [p.get_path_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    AR.scan_paths_synchronous(['/Game/Characters/UEFN_Mannequin/Animations/Ragdoll', '/Game/Characters/UEFN_Mannequin/Animations/Sprint'], force_rescan=True)
    for source_path in SELECTED:
        require('exact source asset exists on disk', disk(source_path).is_file() and A.does_asset_exist(source_path), source_path)
        inspect(source_path)
    R['dirty_packages_after'] = [p.get_path_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    require('all explicit source bytes unchanged', all(sha(path) == digest for path, digest in PROTECTED.items()))
    R['protected_source_hashes'] = PROTECTED
    R['target_measurement_contract'] = dict(status='NOT_RUN', required=['Target final saved clip/skeleton/mesh SHA and actual component scale/rotation.',
        'The same original-frame component foot/ball/pelvis transforms and verified copied contact curves.',
        'Exact node/sample play rates and blend weights; apply each rate once.',
        'Final target stance foot world velocity = actor velocity + rotated/scaled component foot derivative.',
        'Report per-foot stance drift at 400, 650 and gait blends; separate mean stride fit from actual planted-foot locking.'])
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('GAS getup/sprint read-only probe failed; keep author_result.json')
