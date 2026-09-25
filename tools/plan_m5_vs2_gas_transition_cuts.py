"""Derive a bounded transition proposal from actual native source/runtime evidence.

Ordinary Python only, no Unreal import. Does not create or modify game assets.
Proposed rates are trial settings; target retarget/contact/visual tests remain NOT_RUN.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math

DOC = Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2').resolve()
PROBE = DOC / 'editor_runtime/probe_20260924_153126_945_90723b8c_GASTransitionReadOnly/author_result.json'
RUNTIME = DOC / 'editor_runtime/20260924_145159_021_c7c01976_herogait_game/HeroExercise_20260924_145215_2F74E524/hero_exercise_results.json'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample/Content')
# Inclusive ORIGINAL 30-Hz keys; no resampling implied. Original sources stay intact.
CUTS = (
    ('launch_standing', 'M_Relaxed_Jump_F_Start_Stand_Rfoot', 18, 35, 1., 'Jump grounded-edge entry: recorded launch speed near zero'),
    ('launch_running', 'M_Relaxed_Jump_F_Start_Run_Rfoot', 10, 28, 1., 'Jump grounded-edge entry: moving at normal gait'),
    ('launch_sprinting', 'M_Relaxed_Jump_F_Start_Sprint_Rfoot', 8, 25, 1., 'Jump grounded-edge entry: sprinting'),
    ('fall_loop', 'M_Relaxed_Jump_Loop_Fall', 0, 100, 1., 'Existing Fall Loop state'),
    ('land_standing', 'M_Relaxed_Jump_F_Land_Stand_Light_Rfoot', 15, 45, 1., 'Actual IsFalling falling-edge; near-zero horizontal speed'),
    ('land_moving', 'M_Relaxed_Jump_F_Land_Run_Light_Rfoot', 15, 30, 1., 'Actual landing while moving; input remains immediately effective'),
    ('idle_break', 'M_Relaxed_Stand_Idle_Break_v02', 0, 165, 1., 'Idle-only variation, interrupted by movement/combat/FP; head glances do not add a competing target'),
    ('start_400_trial', 'M_Relaxed_Run_Start_F_Lfoot', 0, 13, 2.15, 'Forward grounded start from rest; trial for target calibration, not approved foot locking'),
    ('start_650_trial', 'M_Relaxed_Sprint_Start_F_Lfoot', 0, 18, 1.9, 'Forward grounded sprint from rest; trial for target calibration'),
    ('stop_400_recovery_only', 'M_Relaxed_Run_Stop_F_Lfoot', 43, 66, 1., 'Only after actual speed reaches near zero; cancel on renewed input'),
    ('stop_650_recovery_only', 'M_Relaxed_Sprint_Stop_F_Lfoot', 46, 66, 1., 'Only after actual speed reaches near zero; remember pre-release sprint state'),
)


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def read(p):
    return json.loads(Path(p).read_text('utf-8-sig'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    out = Path(parser.parse_args().output).resolve()
    if not out.is_relative_to(DOC / 'research') or out.exists() or out.suffix != '.json':
        raise RuntimeError('A fresh JSON file under the VS2 research directory is required')
    probe, runtime = read(PROBE), read(RUNTIME)
    assert probe['status'] == runtime['status'] == 'PASS' and probe['asset_writes'] == 0
    assert len(probe['clips']) == 31
    by_name = {c['path'].rsplit('/', 1)[1]: c for c in probe['clips']}
    result = dict(status='PROPOSAL_NOT_APPLIED', native_probe=dict(path=str(PROBE), sha256=sha(PROBE)),
        runtime_evidence=dict(path=str(RUNTIME), sha256=sha(RUNTIME)), selections=[],
        source_writes=0, asset_writes=0, target_calibration='NOT_RUN', runtime_validation='NOT_RUN', visual_acceptance='PENDING',
        rate_scope='Source-cut trial rates only; no target contact result or accepted artistic quality implied',
        root_processing='Separate clean source copies: hold independent floor root translation XYZ and rotation at reference; preserve all other local bone tracks. Then native retarget. Never zero target Selestia Hips Z.',
        preserved=['CharacterMovement 400/650 speed, actual acceleration/braking and immediate Jump',
            'Current 28-sample BlendSpace and 16 side/back motion samples',
            'Weapon, damage, first-person input/camera, combat Slots, secondary physics'],
        explicit_limits=['Stop clips below are post-stop recovery, not a faithful source braking trajectory.',
            'Start trial rates match observed acceleration duration only; target final-pose foot movement still needs measurement.',
            'Moving-land clip needs a short interruptible state/blend without blocking movement; existing ShouldMove exit currently can bypass Land.',
            'Only one idle-break is selected; v01 head excursion is about 85 degrees and was not accepted as a subtle idle.',
            'Only Lfoot ground-start/stop variants selected initially; phase-matched mirrored partners remain unbound.',
            'Root-motion sample flags are not permission to apply root motion to the player capsule.'],
        excluded={'Jump_F_Off_Run': 'Measured zero/upward-free rootZ followed by decline; walking off an edge, not an active jump.',
            'FullStartAndStop': 'Source acceleration/deceleration durations substantially exceed frozen CharacterMovement response.',
            'Turns': 'Measured 90-degree source turn spans 0.50-0.83s. Current player yaw can rotate much faster. Requires separately agreed visual yaw compensation; do not double actor and root yaw.'})
    for role, name, first, last, rate, trigger in CUTS:
        c = by_name[name]
        file = SOURCE / Path(c['path'].removeprefix('/Game/')).with_suffix('.uasset')
        assert sha(file) == c['source_sha256']
        trajectory_path = Path(c['trajectory_file']['path'])
        assert trajectory_path.is_relative_to(PROBE.parent) and sha(trajectory_path) == c['trajectory_file']['sha256']
        t = read(trajectory_path)
        assert t['original_frame_rate'] == {'numerator': 30, 'denominator': 1}
        frames = t['frames']
        assert 0 <= first < last < len(frames)
        a, z = frames[first], frames[last]
        qa = a['bones']['root']['quaternion_xyzw']
        root_angles = []
        for frame in frames[first:last+1]:
            q = frame['bones']['root']['quaternion_xyzw']
            angle = math.degrees(2*math.acos(min(1., abs(sum(x*y for x, y in zip(qa, q))))))
            root_angles.append(angle)
        delta = [z['bones']['root']['position_cm'][i]-a['bones']['root']['position_cm'][i] for i in range(3)]
        result['selections'].append(dict(role=role, source_path=c['path'], source_sha256=c['source_sha256'],
            original_frame_range_inclusive=[first, last], original_rate_hz=30,
            original_time_range_seconds=[first/30, last/30], output_keys=last-first+1,
            proposed_rate=rate, proposed_duration_seconds=(last-first)/30/rate,
            original_root_delta_cm=delta, source_root_max_rotation_from_cut_start_deg=max(root_angles), trigger=trigger,
            available_contact_curves={side: 'contact_'+side in c['float_curve_names'] for side in ('l', 'r')},
            endpoint_hips_relative_root_z_cm=[f['bones']['pelvis']['position_cm'][2]-f['bones']['root']['position_cm'][2] for f in (a,z)]))
    actual = runtime['gait_runtime_evidence']['final_pose_frames']
    result['actual_acceleration_samples'] = []
    for phase, speed in [('Gait_Forward400', 400), ('Gait_Forward650', 650)]:
        frames = [f for f in actual if f['phase'] == phase]
        zero, end = frames[0], next(f for f in frames if abs(f['speed_cm_s']-speed) < 2)
        result['actual_acceleration_samples'].append(dict(phase=phase,
            first_phase_sample_seconds=zero['phase_seconds'], first_target_sample_seconds=end['phase_seconds'],
            time_between_samples_seconds=end['world_seconds']-zero['world_seconds'],
            displacement_cm=math.dist(zero['actor_world'],end['actor_world']),
            scope='Native sampled interval; excludes initial phase latency, not exact hardware input-to-motion latency'))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    print(json.dumps(dict(output=str(out), sha256=sha(out), selections=len(result['selections']), status=result['status'])))


if __name__ == '__main__':
    main()
