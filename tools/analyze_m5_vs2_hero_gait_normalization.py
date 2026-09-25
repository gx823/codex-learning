"""Offline comparison of native toe/contact evidence; never launches Unreal.

All native eligible intervals remain in the measurements, including transient
stride-scale frames. A successful analysis validates arithmetic, not animation
quality, skinned sole contact, clothing coverage, or packaged/OS-input behavior.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys


WORK = Path(__file__).resolve().parents[1]
RUNTIME = WORK / "docs/HarborCity_M5_VS2/editor_runtime"
BASELINES = {
    "legacy_07142dd7": RUNTIME / "20260925_003444_975_07142dd7_herogait_game/HeroExercise_20260925_003502_60CE5499/hero_exercise_results.json",
    "before_fix_98997426": RUNTIME / "20260925_065550_401_98997426_herogait_r2_game/HeroExercise_20260925_065608_513395A4/hero_exercise_results.json",
}
LEGACY_CLIPS = {
    kind: f"M_Relaxed_{name}_Loop_F_InPlace_SelestiaGAS"
    for kind, name in (("run", "Run"), ("sprint", "Sprint"), ("walk", "Walk"))
}
NOMINAL = {"run": 400.0, "sprint": 650.0, "walk": 155.24163818359375}
METRICS = (
    "eligible_duration_seconds",
    "duration_weighted_mean_horizontal_toe_speed_cm_s",
    "duration_weighted_rms_horizontal_toe_speed_cm_s",
    "interval_count_p95_horizontal_toe_speed_cm_s",
    "max_horizontal_toe_speed_cm_s",
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite(value):
    require(isinstance(value, (int, float)) and not isinstance(value, bool)
            and math.isfinite(value), f"Expected finite number: {value!r}")
    return value


def f32(value):
    return struct.unpack("f", struct.pack("f", value))[0]


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def source(path):
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def distribution(values):
    if not values:
        return {"count": 0, "min": None, "mean": None, "p95": None, "max": None}
    values = sorted(map(finite, values))
    return {"count": len(values), "min": values[0], "mean": sum(values) / len(values),
            "p95": values[math.ceil(.95 * len(values)) - 1], "max": values[-1]}


def weights(row, gait):
    result = dict.fromkeys(NOMINAL, 0.0)
    for player in row["actual_active_players"]:
        if "actual_target_blendspace" in gait and player["asset"] != gait["actual_target_blendspace"]:
            continue
        for sample in player.get("actual_cached_samples", []):
            # Native C++ multiplication occurs in float before promotion to double.
            weight = f32(player["effective_blend_weight"] * sample["weight"])
            if "effective_final_player_sample_weight" in sample:
                require(abs(weight - sample["effective_final_player_sample_weight"]) < 1e-9,
                        "Native sample effective weight differs from float product")
            for kind in result:
                match = (sample["animation"] == gait[kind + "_clip"] if kind + "_clip" in gait
                         else sample["animation"].rsplit(".", 1)[-1] == LEGACY_CLIPS[kind])
                if match:
                    result[kind] += weight
    for kind in result:
        key = "effective_target_" + kind + "_weight"
        if key in row:
            require(abs(row[key] - result[kind]) < 1e-9, "Cached sample versus row weight mismatch")
    return result


def direction(row):
    if "stance_eligibility_direction_degrees" in row:
        return row["stance_eligibility_direction_degrees"]
    values = [p["blend_input"][0] for p in row["actual_active_players"] if "blend_input" in p]
    return values[-1] if values else 0.0


def steady_target(row, row_weights):
    if row["moving_on_ground"] and row["target_blendspace_active"] and abs(direction(row)) < 2:
        for target, kind in ((400, "run"), (650, "sprint")):
            if abs(row["speed_cm_s"] - target) < 2 and row_weights[kind] >= .95:
                return target
    return 0


def consecutive(row, previous):
    return bool(previous and row["frame"] == previous["frame"] + 1
                and row["phase"] == previous["phase"]
                and 1e-6 < row["world_seconds"] - previous["world_seconds"] <= .075)


def feet(row):
    result = {f["side"]: f for f in row["feet"]}
    require(set(result) == {"L", "R"} and len(row["feet"]) == 2, "Expected unique L/R feet")
    return result


def summaries(rows):
    result = []
    for target in (400, 650):
        for side in ("L", "R"):
            subset = [r for r in rows if r["steady_target_cm_s"] == target]
            intervals = [(r["actual_interval_seconds"], feet(r)[side]["observed_toe_horizontal_speed_cm_s"])
                         for r in subset if feet(r)[side]["steady_stance_interval_eligible"]]
            duration = sum(dt for dt, _ in intervals)
            item = {"target_speed_cm_s": target, "side": side, "steady_frames": len(subset),
                    "eligible_intervals": len(intervals), "eligible_duration_seconds": duration}
            if intervals:
                speeds = sorted(v for _, v in intervals)
                item.update({METRICS[1]: sum(dt * v for dt, v in intervals) / duration,
                             METRICS[2]: math.sqrt(sum(dt * v * v for dt, v in intervals) / duration),
                             METRICS[3]: speeds[math.ceil(.95 * len(speeds)) - 1], METRICS[4]: max(speeds)})
            result.append(item)
    return result


def analyze_run(path):
    data = read(path)
    require(data["status"] in ("PASS", "FAIL"), "Need completed PASS/FAIL runtime; no RUNNING/abort")
    require(not data["user_stop_latched"], "User-stopped runtime is not a calibration sample")
    gait = data["gait_runtime_evidence"]
    rows = gait["final_pose_frames"]
    require(isinstance(rows, list) and rows, "No actual final pose rows")
    previous = None
    previous_weights = None
    eligible_checks = 0
    raw_residuals, normalized_residuals = [], []
    observed_nominal, curve_weight_sums = [], []
    toe_velocity_error = []
    stride = []
    rates = {}
    for row in rows:
        w = weights(row, gait)
        require(row["steady_target_cm_s"] == steady_target(row, w), "Steady eligibility mismatch")
        linked = consecutive(row, previous)
        require(linked == row["consecutive_interval_under_75ms"], "Consecutive qualification mismatch")
        if previous:
            require(abs((row["world_seconds"] - previous["world_seconds"])
                        - row["actual_interval_seconds"]) < 1e-10, "World interval mismatch")
        for side, foot in feet(row).items():
            prior = feet(previous)[side] if previous else None
            yaw_delta = abs((row["actor_yaw"] - previous["actor_yaw"] + 180) % 360 - 180) if previous else 0
            eligible = bool(linked and previous["moving_on_ground"] and row["moving_on_ground"]
                            and foot["ground_trace_hit"] and prior["ground_trace_hit"]
                            and foot["contact_curve_present"] and foot["contact_curve"] >= f32(.8)
                            and prior["contact_curve"] >= f32(.8)
                            and row["steady_target_cm_s"] != 0
                            and previous["steady_target_cm_s"] == row["steady_target_cm_s"] and yaw_delta < 1)
            require(eligible == foot["steady_stance_interval_eligible"],
                    f"Contact eligibility mismatch at frame {row['frame']} {side}")
            if eligible:
                eligible_checks += 1
                dt = row["actual_interval_seconds"]
                velocity = [(v - p) / dt for v, p in zip(foot["toe_bone_world"], prior["toe_bone_world"])]
                error = abs(math.hypot(*velocity[:2]) - foot["observed_toe_horizontal_speed_cm_s"])
                require(error <= 1e-8, "Native toe velocity does not match consecutive bone displacement")
                toe_velocity_error.append(error)
        motion = row.get("r2_motion")
        if motion:
            if row["steady_target_cm_s"]:
                stride.append(motion["stride_scale_input"])
            if previous and row["frame"] == previous["frame"] + 1:
                raw = sum(previous_weights[k] * NOMINAL[k] for k in NOMINAL)
                total = sum(previous_weights.values())
                # This comparison deliberately does not claim the weight curve was
                # read back: the current native schema records only speed curve.
                normalized = raw / total if total > 1e-8 else 0.0
                observed = motion["nominal_root_speed_previous_evaluated_curve_world_cm_s"]
                observed_nominal.append(observed)
                raw_residuals.append(abs(observed - raw))
                normalized_residuals.append(abs(observed - normalized))
                curve_weight_sums.append(total)
            for player in row["actual_active_players"]:
                for sample in player.get("actual_cached_samples", []):
                    key = sample["animation"]
                    rates.setdefault(key, set()).add((sample["sample_play_rate"], sample["sequence_rate_scale"]))
        previous, previous_weights = row, w
    measured = summaries(rows)
    native = {(s["target_speed_cm_s"], s["side"]): s for s in gait["steady_stance_measurements"]}
    require(len(native) == 4, "Expected four native summaries")
    for item in measured:
        expected = native[(item["target_speed_cm_s"], item["side"])]
        require(item.keys() == expected.keys(), "Native summary schema mismatch")
        for key in item:
            if isinstance(item[key], (int, float)):
                require(math.isclose(item[key], expected[key], rel_tol=1e-12, abs_tol=1e-10),
                        f"Native summary differs: {key}")
            else:
                require(item[key] == expected[key], "Native summary side mismatch")
    launch_path = path.parent.parent / "launch.json"
    launch = read(launch_path) if launch_path.exists() else {}
    if launch:
        require(launch.get("exit_code") is not None, "Launcher process has not exited")
    return {
        "source": source(path), "runtime_status": data["status"], "runtime_detail": data["detail"],
        "launch": {**(source(launch_path) if launch else {}), **{k: launch[k] for k in
                   ("status", "exit_code", "kind", "module_sha256", "author_report", "input_level") if k in launch}},
        "expected_character": data["expected_character"], "expected_animation": data["expected_animation"],
        "native_qualification": gait["eligibility"], "measurement_scope": gait["scope"],
        "legacy_comparison_boundary": gait.get("legacy_comparison_boundary", "Legacy graph; no r2_motion observations"),
        "frame_count": len(rows), "native_arithmetic_and_eligibility_recheck": "PASS",
        "eligible_foot_interval_checks": eligible_checks,
        "max_toe_velocity_recalculation_error_cm_s": max(toe_velocity_error, default=None),
        "measurements": measured,
        "curve_identity": {
            "status": "MEASURED" if raw_residuals else "NOT_AVAILABLE",
            "raw_hypothesis": "Previous effective run*400 + sprint*650 + walk*155.24163818359375",
            "normalized_hypothesis": "Raw hypothesis / sum of previous effective run+sprint+walk; zero if weight<=1e-8",
            "raw_abs_residual_cm_s": distribution(raw_residuals),
            "normalized_abs_residual_cm_s": distribution(normalized_residuals),
            "observed_nominal_world_cm_s": distribution(observed_nominal),
            "previous_effective_curve_weight_sum": distribution(curve_weight_sums),
            "limit": "Hypothesis values use the frozen source nominal speeds and actual previous-frame cache weights. Companion weight curve is not explicitly present in this runtime JSON; its saved author/reload proof is separate. Final speed curve stays diluted by design; normalization is in AnimInstance input.",
        },
        "steady_stride_scale_input": distribution(stride),
        "actual_sample_rates": [{"animation": p, "observed_sample_and_sequence_rate_pairs": sorted(v)}
                                for p, v in sorted(rates.items())],
    }


def compare(candidate, baseline):
    require(candidate["native_qualification"] == baseline["native_qualification"],
            "Native qualification text changed; do not silently compare different conditions")
    result = []
    for new, old in zip(candidate["measurements"], baseline["measurements"]):
        require((new["target_speed_cm_s"], new["side"]) == (old["target_speed_cm_s"], old["side"]),
                "Measurement group mismatch")
        result.append({"target_speed_cm_s": new["target_speed_cm_s"], "side": new["side"],
                       "eligible_intervals_change": new["eligible_intervals"] - old["eligible_intervals"],
                       "deltas_candidate_minus_baseline": {k: new[k] - old[k] if k in new and k in old else None for k in METRICS},
                       "percent_change": {k: 100 * (new[k] / old[k] - 1) if k in new and k in old and old[k] else None for k in METRICS}})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", required=True, type=Path, help="Completed new hero_exercise_results.json")
    parser.add_argument("--output", type=Path, help="New JSON file; omission writes stdout only; existing file refused")
    args = parser.parse_args()
    if args.output:
        require(not args.output.exists(), "Output exists; preserve previous analysis")
    runs = {name: analyze_run(path) for name, path in BASELINES.items()}
    runs["candidate"] = analyze_run(args.runtime.resolve())
    same_source = [name for name in BASELINES if runs[name]["source"]["sha256"] == runs["candidate"]["source"]["sha256"]]
    result = {
        "schema": "HarborCity.M5VS2.GaitNormalizationComparison.v1",
        "status": "ARITHMETIC_SELFCHECK_ONLY" if same_source else "MEASURED_COMPARISON_REQUIRES_REVIEW",
        "analysis_script": source(Path(__file__)), "candidate_identical_to_baseline": same_source,
        "runs": runs,
        "comparisons": {name: compare(runs["candidate"], runs[name]) for name in BASELINES},
        "acceptance_boundaries": [
            "No native eligible intervals were filtered out; speed, contact, effective weight, interval and yaw qualification were independently reconstructed.",
            "p95 is nearest-rank by interval count; mean/RMS are duration-weighted. Finite observed toe bone motion is not a GPU-skinned sole contact measurement.",
            "Same qualification does not synchronize animation phase, contact duty time or frame scheduling across separate runs. No significance or zero-sliding claim.",
            "Only actual JSON fields are measured. Script does not modify animations, movement, tests, clips, camera or clothes.",
            "Art, first-person, airborne clothing coverage, OS mouse input and packaged game validation are not established by these gait data.",
        ],
    }
    encoded = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as handle:
            handle.write(encoded)
        print(json.dumps({"status": result["status"], "output": source(args.output),
                          "candidate_measurements": runs["candidate"]["measurements"],
                          "curve_identity": runs["candidate"]["curve_identity"]}, ensure_ascii=False))
    else:
        print(encoded)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Analysis failed: {error}", file=sys.stderr)
        raise SystemExit(1)
