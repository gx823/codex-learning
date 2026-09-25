"""Offline sparse CPU shoe-surface analysis. Never upgrades visual acceptance.

Every original native gait eligibility record is preserved. A lower-rate CPU
interval is qualified only when ALL intervening original intervals qualified
for the same side/target/phase; no curve or speed threshold is relaxed.
"""
from pathlib import Path
import argparse
import collections
import hashlib
import itertools
import json
import math

DOC = Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2').resolve()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def vector(value):
    assert len(value) == 3 and all(math.isfinite(float(v)) for v in value)
    return [float(v) for v in value]


def sub(a, b):
    return [a[i]-b[i] for i in range(3)]


def center(points):
    assert len(points) == 3
    return [sum(vector(row['cpu_lbs_world_cm'])[i] for row in points)/3 for i in range(3)]


def speed(a, b, seconds):
    return [v/seconds for v in sub(a, b)]


def percentile(values, fraction):
    values = sorted(values)
    return values[max(0, math.ceil(fraction*len(values))-1)] if values else None


def metrics(rows, key):
    if not rows:
        return dict(interval_count=0, qualified_duration_seconds=0, measurement='NOT_RUN')
    duration = sum(row['seconds'] for row in rows)
    return dict(interval_count=len(rows), qualified_duration_seconds=duration,
                duration_weighted_mean=sum(row[key]*row['seconds'] for row in rows)/duration,
                interval_count_p95=percentile([row[key] for row in rows], .95),
                maximum=max(row[key] for row in rows))


def support(points):
    valid = all(row['ground_trace_hit'] and not row.get('trace_start_penetrating', True)
                and row.get('ground_normal', [0, 0, 0])[2] >= .999 for row in points)
    result = dict(flat_ground_trace_valid=valid)
    if valid:
        gaps = [row['signed_surface_plane_distance_cm'] for row in points]
        result.update(signed_gap_min_cm=min(gaps), signed_gap_mean_cm=sum(gaps)/3, signed_gap_max_cm=max(gaps),
                      all_three_within_one_cm_descriptive=all(abs(gap) <= 1 for gap in gaps),
                      ground_actors=sorted({row['ground_actor'] for row in points}))
    return result


def run(runtime_file, output_file):
    runtime_file, output_file = runtime_file.resolve(), output_file.resolve()
    assert runtime_file.is_relative_to(DOC/'editor_runtime') and runtime_file.name == 'hero_exercise_results.json'
    assert output_file.is_relative_to(DOC/'research') and output_file.suffix == '.json' and not output_file.exists()
    source = json.loads(runtime_file.read_text(encoding='utf-8-sig'))
    sole = source['sole_capture']
    frames = sole['cpu_surface_samples']
    gait = source['gait_runtime_evidence']['final_pose_frames']
    selection_path = Path(sole['source_selection']).resolve()
    assert selection_path.is_relative_to(DOC/'research')
    selection = json.loads(selection_path.read_text(encoding='utf-8-sig'))
    assert selection['status'] == 'READY_FOR_NATIVE_SAMPLING' and len(selection['points']) == 12
    expected = {(p['side'], p['region'], p['vertex_index']): p for p in selection['points']}
    assert len(expected) == 12 and 0 < len(frames) <= 900
    for frame in frames:
        assert frame['status'] == 'PASS_CPU_LBS_SAMPLE_ONLY' and frame['actual_lod'] == 0
        assert len(frame['points']) == 12
        actual = {(p['side'], p['region'], p['render_vertex_index']): p for p in frame['points']}
        assert set(actual) == set(expected)
        for key, point in actual.items():
            assert point['section_index'] == expected[key]['section_index']
            assert point['reference_component_cm'] == expected[key]['reference_position']
            vector(point['cpu_lbs_world_cm'])
    rows, excluded = [], collections.Counter()
    for previous, current in zip(frames, frames[1:]):
        seconds = current['world_seconds']-previous['world_seconds']
        first, last = previous['latest_gait_sample_index'], current['latest_gait_sample_index']
        if not 0 <= first < last < len(gait):
            excluded['no_complete_native_gait_bracket'] += 1
            continue
        if previous['frame'] != gait[first]['frame'] or current['frame'] != gait[last]['frame']:
            excluded['cpu_and_gait_endpoints_not_same_engine_frames'] += 1
            continue
        if not 0 < seconds <= .075:
            excluded['cpu_interval_outside_0_to_75ms'] += 1
            continue
        if abs(current['world_seconds']-gait[last]['world_seconds']) > 1e-6 or abs(previous['world_seconds']-gait[first]['world_seconds']) > 1e-6:
            raise ValueError('CPU and native gait world clocks disagree')
        middle = gait[first+1:last+1]
        if any(row['phase'] != previous['phase'] or row['phase'] != current['phase'] for row in middle):
            excluded['phase_boundary'] += 1
            continue
        target = gait[last]['steady_target_cm_s']
        if target not in (400, 650):
            excluded['outside_native_400_650_target'] += 1
            continue
        yaw = math.radians(current['actor_yaw_degrees'])
        for side_index, side in enumerate(('L', 'R')):
            if not all(row['steady_target_cm_s'] == target and row['feet'][side_index]['steady_stance_interval_eligible'] for row in middle):
                excluded['original_eligibility_rejected_'+side] += 1
                continue
            toe_velocity = speed(gait[last]['feet'][side_index]['toe_bone_world'], gait[first]['feet'][side_index]['toe_bone_world'], seconds)
            patch_centers = {}
            for region in ('Heel', 'Forefoot'):
                before = [p for p in previous['points'] if p['side'] == side and p['region'] == region]
                after = [p for p in current['points'] if p['side'] == side and p['region'] == region]
                old_center, new_center = center(before), center(after)
                patch_centers[region] = (old_center, new_center)
                velocity = speed(new_center, old_center, seconds)
                old_ground, new_ground = support(before), support(after)
                row = dict(side=side, region=region, target_cm_s=target, phase=current['phase'],
                           from_frame=previous['frame'], to_frame=current['frame'], from_gait_index=first, to_gait_index=last,
                           intervening_original_intervals=len(middle), seconds=seconds,
                           from_world_seconds=previous['world_seconds'], to_world_seconds=current['world_seconds'],
                           from_centroid_world_cm=old_center, to_centroid_world_cm=new_center, centroid_velocity_cm_s=velocity,
                           horizontal_speed_cm_s=math.hypot(velocity[0], velocity[1]),
                           signed_forward_cm_s=velocity[0]*math.cos(yaw)+velocity[1]*math.sin(yaw),
                           absolute_lateral_cm_s=abs(-velocity[0]*math.sin(yaw)+velocity[1]*math.cos(yaw)),
                           signed_vertical_cm_s=velocity[2], same_interval_toe_horizontal_speed_cm_s=math.hypot(toe_velocity[0], toe_velocity[1]),
                           ground_before=old_ground, ground_after=new_ground,
                           known_active_morph_clear=(previous['active_morph_cpu_overlap_known'] and current['active_morph_cpu_overlap_known']
                                                    and not previous['active_morph_affects_selected_point'] and not current['active_morph_affects_selected_point']))
                rows.append(row)
            for row in rows[-2:]:
                lines = [sub(patch_centers['Forefoot'][i], patch_centers['Heel'][i]) for i in (0, 1)]
                angles = [math.degrees(math.atan2(line[2], math.hypot(line[0], line[1]))) for line in lines]
                row['heel_to_forefoot_line_elevation_degrees'] = angles
                row['heel_to_forefoot_elevation_change_degrees'] = angles[1]-angles[0]
    summaries = []
    for target, side, region in itertools.product((400, 650), ('L', 'R'), ('Heel', 'Forefoot')):
        group = [r for r in rows if r['target_cm_s'] == target and r['side'] == side and r['region'] == region]
        near_ground = [r for r in group if r['ground_before'].get('all_three_within_one_cm_descriptive')
                       and r['ground_after'].get('all_three_within_one_cm_descriptive') and r['known_active_morph_clear']]
        summaries.append(dict(target_cm_s=target, side=side, region=region,
                              all_original_qualified_centroid_horizontal_cm_s=metrics(group, 'horizontal_speed_cm_s'),
                              all_original_qualified_absolute_lateral_cm_s=metrics(group, 'absolute_lateral_cm_s'),
                              same_interval_native_toe_horizontal_cm_s=metrics(group, 'same_interval_toe_horizontal_speed_cm_s'),
                              descriptive_one_cm_patch_subset_horizontal_cm_s=metrics(near_ground, 'horizontal_speed_cm_s'),
                              descriptive_subset_warning='1cm is a disclosed exploratory surface-proximity band, not a new gait acceptance criterion. Do not replace the full original-qualified series with it.'))
    result = dict(schema='HarborCity.M5VS2.GaitSoleCPU.Analysis.v1', status='ANALYZED_NOT_VISUALLY_ACCEPTED',
                  runtime_status=source['status'], runtime_report=str(runtime_file), runtime_sha256=sha(runtime_file),
                  selection=str(selection_path), selection_sha256=sha(selection_path),
                  input_level=source['input_level'], source_surface_scope=sole['scope'],
                  raw_cpu_sample_count=len(frames), all_original_gait_frame_count=len(gait),
                  original_toe_summaries_unmodified=source['gait_runtime_evidence']['steady_stance_measurements'],
                  qualification=sole['interval_qualification']+' Additionally CPU endpoint dt<=75ms; exclusions explicitly counted.',
                  excluded_interval_reasons=dict(excluded), summaries=summaries, qualified_sparse_intervals=rows,
                  video_observation='NOT_RUN_BY_THIS_SCRIPT', visible_sliding_acceptance='USER_REVIEW',
                  limitation='Sparse CPU LBS excludes morph/cloth/WPO/GPU. Patch centroid motion includes normal rolling; '
                             'consider heel/forefoot ground gaps and line elevation together, not a requirement that every point must have zero velocity.')
    output_file.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    print(json.dumps(dict(output=str(output_file), sha256=sha(output_file), summaries=summaries), ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.runtime, args.output)
