"""Read-only evidence computation; never changes qualification or invokes UE."""
import bisect
import hashlib
import json
import math
from pathlib import Path

WORK = Path(r"D:/科研学习/codex学习")
BASE = WORK / "docs/HarborCity_M5_VS2/editor_runtime"
CAL = BASE / "author_20260924_135152_178_93c29817_GASLocomotionCalibration"
RUN = BASE / "20260925_084241_175_ca956725_herogait_clock_game/HeroExercise_20260925_084258_66448B49/hero_exercise_results.json"
SOURCE = BASE / "probe_20260924_123631_452_88d3fc38_GASGetUpSprintReadOnly/M_Relaxed_Sprint_Loop_F_frames.json"
OUT = WORK / "docs/HarborCity_M5_VS2/research/GAS_CLOCK_RESIDUAL_ca956725.json"


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def record(path):
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def point(animation, seconds, key):
    frames = animation["frames"]
    times = [f["time_seconds"] for f in frames]
    index = max(0, min(len(frames) - 2, bisect.bisect_right(times, seconds) - 1))
    alpha = (seconds - times[index]) / (times[index + 1] - times[index])
    return [a * (1 - alpha) + b * alpha for a, b in zip(
        frames[index]["bones"][key]["position_cm"], frames[index + 1]["bones"][key]["position_cm"])]


def weighted(rows, field, transform=lambda x: x):
    return sum(row["dt"] * transform(row[field]) for row in rows) / sum(row["dt"] for row in rows)


def main():
    data = load(RUN)
    gait = data["gait_runtime_evidence"]
    frames = gait["final_pose_frames"]
    result = {"runtime": record(RUN), "runtime_intervals": {}, "offline_source_target": {}}
    result["limits"] = [
        "Every native qualified interval is retained, including the start-to-loop crossfade. No acceptance changes.",
        "RAW prediction uses linearly interpolated component bone positions, not exact quaternion evaluation or compression. It models only the dominant clip, excluding other blend contributions and controls.",
        "Ankle and toe bone movement is not heel/sole skin motion; no zero-slip or visual PASS.",
        "Offline 30Hz contact windows are a separate comparison, not replacement for runtime qualification.",
    ]

    def sample(row, kind):
        player = next(p for p in row["actual_active_players"] if p["asset"] == gait["actual_target_blendspace"])
        return next(s for s in player["actual_cached_samples"] if s["animation"] == gait[kind + "_clip"])

    for speed, kind in [(400, "run"), (650, "sprint")]:
        target_path = CAL / f"M_Relaxed_{kind.title()}_Loop_F_InPlace_SelestiaGAS_target_frames.json"
        target = load(target_path)
        for side in ("L", "R"):
            rows = []
            for previous, row in zip(frames, frames[1:]):
                foot = next(f for f in row["feet"] if f["side"] == side)
                prior_foot = next(f for f in previous["feet"] if f["side"] == side)
                if row["steady_target_cm_s"] != speed or not foot["steady_stance_interval_eligible"]:
                    continue
                dt = row["actual_interval_seconds"]
                clip, previous_clip = sample(row, kind), sample(previous, kind)
                toe, prior_toe = point(target, clip["time"], "ball_" + side.lower()), point(target, previous_clip["time"], "ball_" + side.lower())
                raw_velocity = [(a - b) * 1.25 / dt for a, b in zip(toe[:2], prior_toe[:2])]
                yaw = math.radians(row["actor_yaw"] - 90)
                c, s = math.cos(yaw), math.sin(yaw)
                actor_velocity = [(a - b) / dt for a, b in zip(row["actor_world"][:2], previous["actor_world"][:2])]
                predicted = [actor_velocity[0] + c * raw_velocity[0] - s * raw_velocity[1], actor_velocity[1] + s * raw_velocity[0] + c * raw_velocity[1]]
                toe_velocity = foot["observed_toe_world_velocity_cm_s"][:2]
                ankle_velocity = [(a - b) / dt for a, b in zip(foot["foot_bone_world"][:2], prior_foot["foot_bone_world"][:2])]
                relative = [a - b for a, b in zip(toe_velocity, ankle_velocity)]
                forward = [v / speed for v in row["velocity_cm_s"][:2]]
                right = [forward[1], -forward[0]]
                dot = lambda a, b: sum(x * y for x, y in zip(a, b))
                rows.append({"frame": row["frame"], "dt": dt, "phase_seconds": row["phase_seconds"],
                             "clip_time": clip["time"], "clip_actual_rate": clip["actual_animation_delta"] / dt,
                             "contact": foot["contact_curve"], "stride_scale": row["r2_motion"]["stride_scale_input"],
                             "toe_speed": math.hypot(*toe_velocity), "single_raw_predicted_speed": math.hypot(*predicted),
                             "raw_prediction_vector_error_speed": math.dist(toe_velocity, predicted),
                             "toe_lateral": dot(toe_velocity, right), "ankle_lateral": dot(ankle_velocity, right),
                             "toe_relative_ankle_lateral": dot(relative, right),
                             "toe_forward": dot(toe_velocity, forward), "ankle_forward": dot(ankle_velocity, forward),
                             "toe_relative_ankle_forward": dot(relative, forward)})
            stats = {"eligible_count": len(rows), "eligible_seconds": sum(r["dt"] for r in rows),
                     "target_raw": record(target_path), "rows": rows,
                     "actual_rate_min": min(r["clip_actual_rate"] for r in rows),
                     "actual_rate_max": max(r["clip_actual_rate"] for r in rows)}
            for field in ("toe_speed", "single_raw_predicted_speed", "raw_prediction_vector_error_speed", "toe_forward", "ankle_forward", "toe_relative_ankle_forward"):
                stats["duration_weighted_" + field] = weighted(rows, field)
            for field in ("toe_lateral", "ankle_lateral", "toe_relative_ankle_lateral"):
                stats["duration_weighted_absolute_" + field] = weighted(rows, field, abs)
            result["runtime_intervals"][f"{speed}_{side}"] = stats

    source = load(SOURCE)
    target_path = CAL / "M_Relaxed_Sprint_Loop_F_InPlace_SelestiaGAS_target_frames.json"
    target = load(target_path)
    result["source_sprint"] = record(SOURCE)
    assert len(source["frames"]) == len(target["frames"]) == 69
    result["offline_contact_criterion"] = "Consecutive original 30Hz frames, both actual evaluated contact curves >=0.8; all such intervals retained. Different sampling layer from runtime."
    for side in ("l", "r"):
        for label, animation, scale, rate, speed, subtract_root in [
            ("source_original585", source, 1, 1, 0, False),
            ("source_inplace_normalized650", source, 1, 650 / 585, 650, True),
            ("target_actual650", target, 1.25, 1.1508945262961796, 650, False),
        ]:
            rows = []
            for a, b in zip(animation["frames"], animation["frames"][1:]):
                if min(a["curves"]["contact_" + side], b["curves"]["contact_" + side]) < .8:
                    continue
                dt = b["time_seconds"] - a["time_seconds"]
                velocities = []
                for bone in ("foot_" + side, "ball_" + side):
                    v = [(b["bones"][bone]["position_cm"][k] - a["bones"][bone]["position_cm"][k]) / dt for k in range(3)]
                    if subtract_root:
                        v = [v[k] - (b["bones"]["root"]["position_cm"][k] - a["bones"]["root"]["position_cm"][k]) / dt for k in range(3)]
                    v = [x * scale * rate for x in v]
                    v[1] += speed
                    velocities.append(v)
                ankle, toe = velocities
                rows.append({"dt": dt, "time": a["time_seconds"], "toe_speed": math.hypot(*toe[:2]),
                             "toe_lateral": toe[0], "ankle_lateral": ankle[0], "relative_lateral": toe[0] - ankle[0]})
            result["offline_source_target"][label + "_" + side] = {
                "interval_count": len(rows), "mean_toe_xy_speed": weighted(rows, "toe_speed"),
                "mean_absolute_toe_lateral": weighted(rows, "toe_lateral", abs),
                "mean_absolute_ankle_lateral": weighted(rows, "ankle_lateral", abs),
                "mean_absolute_relative_lateral": weighted(rows, "relative_lateral", abs), "rows": rows}
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(record(OUT), ensure_ascii=False))


if __name__ == "__main__":
    main()
