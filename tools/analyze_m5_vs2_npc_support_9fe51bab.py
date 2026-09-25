"""Offline, evidence-only analysis. No UE, asset writes, or acceptance threshold changes."""
import bisect
import hashlib
import json
import math
from pathlib import Path

WORK = Path(r"D:/科研学习/codex学习")
BASE = WORK / "docs/HarborCity_M5_VS2/editor_runtime"
RUN = BASE / "20260925_083631_733_9fe51bab_npcreaction_Open_JT_Backward_game/NPCReactionR2_20260925_083650_2C67D5E0"
REPORT = RUN / "npc_reaction_review.json"
OUTPUT = WORK / "docs/HarborCity_M5_VS2/research/NPC_JT_SUPPORT_9fe51bab_ANALYSIS.json"
TARGETS = {
    "J": ("F", "author_20260925_050103_282_a9858935_NPCGetUpRetargetR2J"),
    "T": ("B", "author_20260925_052332_803_a7855ea8_NPCGetUpRetargetR2T"),
}


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def xyz(transform):
    return [float(v) for v in transform.split("|")[0].split(",")]


def main():
    original_hash = sha(REPORT)
    runtime = read(REPORT)
    raw = runtime["getup_support_samples"]
    assert len(raw) == 25
    result = {
        "scope": "Offline analysis of existing native engine function diagnostic; not OS or true vehicle contact, not skin-surface measurement.",
        "source": {"path": str(REPORT), "sha256": original_hash},
        "raw_sample_count": len(raw),
        "all_raw_samples_preserved": raw,
        "characters": {},
    }
    for who, (direction, folder) in TARGETS.items():
        rows = [r for r in raw if r["stable_id"] == "M5VS2_" + who]
        target_path = BASE / folder / f"M_ragdoll_getup_stand_{direction}_NPC{who}GetUp_target_frames.json"
        target = read(target_path)
        frames = target["frames"]
        times = [f["time_seconds"] for f in frames]
        all_points = [p for row in rows for p in row["bone_points"]]
        body = {
            "actual_clip": rows[0]["getup"]["getup_clip"],
            "target_pose_source": {"path": str(target_path), "sha256": sha(target_path)},
            "sample_count": len(rows),
            "all_13_points_found_finite": all(p["bone_found"] and p["finite"] for p in all_points),
            "all_ground_traces_hit_flat_z0_without_start_penetration": all(
                p["static_trace_hit"] and p["hit_normal"] == [0, 0, 1]
                and p["hit_point_cm"][2] == 0 and not p["start_penetrating"] for p in all_points),
            "montage_weights": sorted(set(r["active_montage_weight"] for r in rows)),
            "root_motion_modes": sorted(set(r["root_motion_mode"] for r in rows)),
            "mesh_world_z_cm": sorted(set(xyz(r["mesh_world_transform"])[2] for r in rows)),
            "maximum_sample_gap_seconds": max(b["game_seconds"] - a["game_seconds"] for a, b in zip(rows, rows[1:])),
            "actor_xy_displacement_cm": math.dist(xyz(rows[0]["actor_transform"])[:2], xyz(rows[-1]["actor_transform"])[:2]),
            "bone_height_prediction": {},
            "motion_rows": [],
        }
        for role, bone in [("hips", "hips"), ("leftFoot", "foot_l"), ("rightFoot", "foot_r"), ("leftToes", "ball_l"), ("rightToes", "ball_r")]:
            errors = []
            for row in rows:
                seconds = row["active_montage_position_s"]
                i = max(0, min(len(frames) - 2, bisect.bisect_right(times, seconds) - 1))
                alpha = (seconds - times[i]) / (times[i + 1] - times[i])
                predicted = sum(weight * frames[index]["bones"][bone]["position_cm"][2]
                                for weight, index in [(1 - alpha, i), (alpha, i + 1)])
                predicted += xyz(row["mesh_world_transform"])[2]
                observed = next(p["world_cm"][2] for p in row["bone_points"] if p["role"] == role)
                errors.append(observed - predicted)
            body["bone_height_prediction"][role] = {
                "method": "Actual montage seconds, linear interpolation of existing 30Hz target RAW component-space bone Z + measured mesh Z; not interpolation of quaternion pose or exact compression identity.",
                "min_error_cm": min(errors), "max_error_cm": max(errors),
                "max_absolute_error_cm": max(map(abs, errors)),
            }
        for index, row in enumerate(rows):
            entry = {"active_montage_seconds": row["active_montage_position_s"], "game_seconds": row["game_seconds"], "bones": {}}
            previous = rows[index - 1] if index else None
            for point in row["bone_points"]:
                metric = {"world_cm": point["world_cm"], "bone_ground_gap_cm": point["bone_minus_hit_z_cm"]}
                if previous:
                    previous_point = next(p for p in previous["bone_points"] if p["role"] == point["role"])
                    metric["interval_xy_speed_cm_s"] = math.dist(point["world_cm"][:2], previous_point["world_cm"][:2]) / (row["game_seconds"] - previous["game_seconds"])
                entry["bones"][point["role"]] = metric
            body["motion_rows"].append(entry)
        result["characters"][who] = body
    assert sha(REPORT) == original_hash
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(OUTPUT), "sha256": sha(OUTPUT), "raw_rows": len(raw)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
