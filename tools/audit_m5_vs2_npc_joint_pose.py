"""Read-only audit of native NPC physics reports; never loads Unreal or assets.

Quaternion convention is XYZW. Swing coordinates reproduce UE 5.8's
quarter-angle coordinates. The world linear solver uses independent pyramid
limits; the nonlinear solver uses an ellipse. Missing native solver flags do
not establish which model was active.
"""
import argparse
import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path


REPORT_ROOT = Path("D:/科研学习/codex学习/docs/HarborCity_M5_VS2").resolve()


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def sub(a, b):
    return [x - y for x, y in zip(a, b)]


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def unit(a):
    if not all(math.isfinite(x) for x in a):
        raise ValueError("Non-finite native vector/quaternion")
    length = math.sqrt(dot(a, a))
    if length < 1e-10:
        raise ValueError("Degenerate native vector/quaternion")
    return [x / length for x in a]


def qmul(a, b):
    return [a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
            a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
            a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
            a[3]*b[3]-dot(a[:3], b[:3])]


def qinv(q):
    q = unit(q)
    return [-q[0], -q[1], -q[2], q[3]]


def rotate(q, v):
    q = unit(q)
    twice = [2*x for x in cross(q[:3], v)]
    other = cross(q[:3], twice)
    return [v[i] + q[3]*twice[i] + other[i] for i in range(3)]


def acos_deg(x):
    return math.degrees(math.acos(max(-1., min(1., x))))


def asin_deg(x):
    return math.degrees(math.asin(max(-1., min(1., x))))


def chaos_angles(relative):
    q = unit(relative)
    if q[3] < 0:
        q = [-x for x in q]
    length = math.hypot(q[0], q[3])
    if length < 1e-8:
        raise ValueError("Singular 180-degree swing: do not invent a twist axis")
    twist = [q[0]/length, 0., 0., q[3]/length]
    swing = unit(qmul(q, qinv(twist)))
    # Exact coordinates used by GetEllipticalConeAxisErrorLocal in UE 5.8.
    s1 = math.degrees(4 * math.atan2(swing[2], 1 + swing[3]))
    s2 = math.degrees(4 * math.atan2(swing[1], 1 + swing[3]))
    twist_deg = math.degrees(2 * math.atan2(twist[0], twist[3]))
    return s1, s2, twist_deg, acos_deg(rotate(q, [1., 0., 0.])[0])


def package(path):
    return path.split(".", 1)[0]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def native_profiles(probe):
    profiles = {}
    for row in probe["assets"]:
        if "after_save" in row:
            native = row["after_save"]["native"]
            location = "assets[].after_save.native"
        else:
            native = row["preflight"]
            location = "assets[].preflight (existing frames, never candidate frames)"
        if native.get("status") != "PASS":
            raise ValueError("Native physics inspection is not PASS")
        key = package(native["physics"])
        if key in profiles:
            raise ValueError("Duplicate native physics binding")
        profiles[key] = (row["sample"], native, location)
    return profiles


def audit_specimen(specimen, native):
    refs = {b["role"]: b["reference_component"] for b in native["bodies"]}
    bone_roles = {b["bone"]: b["role"] for b in native["bodies"]}
    joints = {j["child_bone"]: j for j in native["constraints"]}
    live = specimen["actual_rigid_bodies"]
    forward = unit(native["anatomical_forward_from_toes"])
    up = unit(native["anatomical_up_component"])
    side_axis = unit(cross(forward, up))
    if abs(dot(forward, up)) > 1e-5:
        raise ValueError("Native anatomical basis not orthogonal")
    out = {"joints": [], "hips": {}, "shoulders_relative_upper_chest": {}, "hinges": {}}
    live_by_child = {}
    for observed in specimen["live_constraints"]:
        child = observed["child"]
        stored = joints[child]
        if not observed["valid"] or observed["parent"] != stored["parent_bone"]:
            raise ValueError("Live constraint validity/parent differs from native probe")
        if observed["limits_swing1_swing2_twist_degrees"] != stored["angular_limits_degrees"]:
            raise ValueError("Live limits differ from supplied native probe")
        if observed["motion_swing1_swing2_twist"] != stored["angular_motion_swing1_swing2_twist"]:
            raise ValueError("Live motion modes differ from supplied native probe")
        cr, pr = bone_roles[child], bone_roles[observed["parent"]]
        cq = qmul(live[cr]["physics_rotation_xyzw"], stored["frame1_child_local"]["rotation_xyzw"])
        pq = qmul(live[pr]["physics_rotation_xyzw"], stored["frame2_parent_local"]["rotation_xyzw"])
        reconstructed = unit(qmul(qinv(pq), cq))
        actual = unit(observed["child_in_parent_frame_rotation_xyzw"])
        error = 2 * acos_deg(abs(dot(reconstructed, actual)))
        if error > .002:
            raise ValueError(f"Probe frames do not match runtime: {child}: {error} degrees")
        s1, s2, twist, cone = chaos_angles(actual)
        limits = observed["limits_swing1_swing2_twist_degrees"]
        modes = observed["motion_swing1_swing2_twist"]
        ellipse = ((s1/limits[0])**2 + (s2/limits[1])**2
                   if modes[:2] == [1, 1] and min(limits[:2]) > 0 else None)
        independent_excess = [max(0., abs(angle)-limit) if mode == 1 else None
                              for angle, limit, mode in zip((s1, s2), limits[:2], modes[:2])]
        flag = observed.get("use_linear_joint_solver")
        if flag is not None and type(flag) is not bool:
            raise ValueError("Native use_linear_joint_solver must be a boolean")
        model = ("LINEAR_PYRAMID" if flag else "NONLINEAR_ELLIPSE") if flag is not None else "UNKNOWN_NATIVE_FLAG_MISSING"
        selected_violation = None
        # This joint's two limited swings are the comparable pyramid/ellipse case.
        # Locked/free combinations have different constraint code paths.
        if ellipse is not None and flag is not None:
            selected_violation = (max(independent_excess) > 1e-6 if flag else ellipse > 1. + 1e-8)
        item = {"child_role": cr, "parent_role": pr,
                "frame_reconstruction_error_degrees": error,
                "swing1_Z_swing2_Y_twist_X_degrees": [s1, s2, twist],
                "true_X_axis_cone_angle_degrees": cone,
                "native_use_linear_joint_solver": flag,
                "solver_model_from_runtime_flag": model,
                "selected_swing_model_violation": selected_violation,
                "ellipse_reference_normalized_sum_squared": ellipse,
                "ellipse_reference_inside": ellipse <= 1. if ellipse is not None else None,
                "independent_pyramid_swing_excess_degrees": independent_excess,
                "model_scope": "For two limited swings only. Ellipse data alone is not a linear-solver violation; pyramid data alone is not a nonlinear ellipse test.",
                "twist_symmetric_limit_excess_degrees": max(0., abs(twist)-limits[2]) if modes[2] == 1 else None,
                "limits_swing1_swing2_twist_degrees": limits,
                "motion_swing1_swing2_twist": modes,
                "world_anchor_separation_cm": observed["world_anchor_separation_cm"]}
        out["joints"].append(item)
        live_by_child[cr] = (observed, stored, pq)
    for prefix in ("left", "right"):
        for segment, distal, parent, destination in (
                ("UpperLeg", "LowerLeg", "hips", "hips"),
                ("UpperArm", "LowerArm", "upperChest", "shoulders_relative_upper_chest")):
            role = prefix + segment
            direction = unit(sub(live[prefix+distal]["physics_position_cm"], live[role]["physics_position_cm"]))
            # Current parent rigid rotation removed, actual bind parent rotation restored.
            bind_direction = rotate(refs[parent]["rotation_xyzw"],
                                    rotate(qinv(live[parent]["physics_rotation_xyzw"]), direction))
            if segment == "UpperLeg":
                values = {"sagittal_forward_positive_degrees": math.degrees(math.atan2(dot(bind_direction, forward), -dot(bind_direction, up))),
                          "signed_lateral_degrees": asin_deg(dot(bind_direction, side_axis))}
            else:
                values = {"forward_positive_elevation_degrees": asin_deg(dot(bind_direction, forward)),
                          "up_positive_elevation_degrees": asin_deg(dot(bind_direction, up))}
            out[destination][prefix] = values
        for upper, middle, distal in (("UpperLeg", "LowerLeg", "Foot"), ("UpperArm", "LowerArm", "Hand")):
            a = unit(sub(live[prefix+middle]["physics_position_cm"], live[prefix+upper]["physics_position_cm"]))
            b = unit(sub(live[prefix+distal]["physics_position_cm"], live[prefix+middle]["physics_position_cm"]))
            _, _, parent_frame_q = live_by_child[prefix+middle]
            axis = rotate(parent_frame_q, [1., 0., 0.])
            out["hinges"][prefix+middle] = {
                "three_point_flexion_magnitude_degrees": acos_deg(dot(a, b)),
                "signed_about_parent_hinge_X_degrees": math.degrees(math.atan2(dot(axis, cross(a, b)), dot(a, b))),
                "definition": "0 = straight geometric segments; sign follows actual parent hinge X; includes reference bone bend."}
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--review", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--visual-status", choices=("USER_REVIEW", "FAIL"), default="USER_REVIEW")
    parser.add_argument("--visual-note", default="Numerical diagnostics cannot establish naturalness or user acceptance.")
    parser.add_argument("--corrects", type=Path, help="Preserved earlier audit whose interpretation this fresh report corrects")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(REPORT_ROOT) or output.suffix.lower() != ".json" or output.exists():
        raise ValueError("Output must be a fresh JSON path under docs/HarborCity_M5_VS2")
    review_path, probe_path = args.review.resolve(strict=True), args.probe.resolve(strict=True)
    hashes = {str(p): sha(p) for p in (review_path, probe_path)}
    review = json.loads(review_path.read_text(encoding="utf-8-sig"))
    probe = json.loads(probe_path.read_text(encoding="utf-8-sig"))
    profiles = native_profiles(probe)
    result = {"schema": "HarborCity_M5VS2_NPC_JointPoseAudit_v2",
              "created_utc": datetime.now(timezone.utc).isoformat(),
              "input_sha256": hashes, "tool_sha256": sha(Path(__file__)),
              "scope": "Offline read-only calculation from native evidence; no engine, asset mutation or visual acceptance test.",
              "visual_status": args.visual_status, "visual_note": args.visual_note,
              "interpretation_correction": "v1 ellipse_inside / ellipse_observations_outside were nonlinear ellipse reference tests. They did not prove world-solver violations without the live per-joint solver flag. The linear cached solver constrains Swing1 and Swing2 independently (pyramid), allowing their ellipse sum to approach 2. Anatomical signed angles and reconstructed frames remain valid.",
              "method": {"quaternion": "Native XYZW; parent joint inverse * child joint. Reconstructed from native local frames and live rigid body quaternions before use.",
                         "swing": "q = swing * twistX. S1=4*atan2(swing.z,1+swing.w); S2=4*atan2(swing.y,1+swing.w). Ellipse=(S1/limit1)^2+(S2/limit2)^2.",
                         "solver_selection": "Only the live constraint use_linear_joint_solver flag selects the model. Missing flag = UNKNOWN. PhysicsAsset RBAN settings and engine defaults are not substituted for runtime evidence.",
                         "linear_engine_source": "E:/UE_5.8/Engine/Source/Runtime/Experimental/Chaos/Private/Chaos/Joint/PBDJointCachedSolverGaussSeidel.cpp:1176,1371 (pyramid); 1235 (SIMD same independent angles/limits)",
                         "nonlinear_engine_source": "E:/UE_5.8/Engine/Source/Runtime/Experimental/Chaos/Private/Chaos/Joint/PBDJointSolverGaussSeidel.cpp:1655; PBDJointConstraintUtilities.cpp: GetEllipticalConeAxisErrorLocal",
                         "anatomical": "Project reference basis from actual head/hips and toes; signed hip angle atan2(direction dot Forward, direction dot -Up). Not a clinical ROM measurement.",
                         "limits": "Ellipse and twist are geometric limit checks only; do not measure solver impulses, contacts or visual naturalness."},
              "profiles": {}, "captures": []}
    if args.corrects:
        previous = args.corrects.resolve(strict=True)
        if not previous.is_relative_to(REPORT_ROOT) or previous == output:
            raise ValueError("Correction target must be a preserved VS2 report")
        result["corrects_preserved_report"] = {"path": str(previous), "sha256": sha(previous)}
    for key, (sample, native, source) in profiles.items():
        result["profiles"][key] = {"sample": sample, "native_source": source,
                                   "anatomical_up_component": native["anatomical_up_component"],
                                   "anatomical_forward_component": native["anatomical_forward_from_toes"],
                                   "body_count": native["body_count"], "constraint_count": native["constraint_count"]}
    for capture in review["captures"]:
        for specimen in capture["specimens"]:
            if not specimen["physics_simulating"]:
                continue
            sample, native, _ = profiles[package(specimen["physics_asset"])]
            if package(native["mesh"]) != package(specimen["mesh"]):
                raise ValueError("Live mesh does not match native probe")
            measured = audit_specimen(specimen, native)
            measured.update(sample=sample, image=capture["file"], request_frame=capture["request_frame"],
                            simulation_seconds=capture["actual_physics_simulation_seconds"],
                            source_actor=specimen["actor"])
            result["captures"].append(measured)
    if not result["captures"]:
        raise ValueError("No simulating specimen captures")
    all_joints = [joint for capture in result["captures"] for joint in capture["joints"]]
    result["summary"] = {"capture_count": len(result["captures"]), "joint_observations": len(all_joints),
                         "max_frame_reconstruction_error_degrees": max(j["frame_reconstruction_error_degrees"] for j in all_joints),
                         "max_anchor_separation_cm": max(j["world_anchor_separation_cm"] for j in all_joints),
                         "solver_model_known_joint_observations": sum(j["native_use_linear_joint_solver"] is not None for j in all_joints),
                         "solver_model_unknown_joint_observations": sum(j["native_use_linear_joint_solver"] is None for j in all_joints),
                         "selected_swing_model_violation_observations": sum(j["selected_swing_model_violation"] is True for j in all_joints),
                         "selected_swing_model_classified_observations": sum(j["selected_swing_model_violation"] is not None for j in all_joints),
                         "ellipse_reference_observations_outside": sum(j["ellipse_reference_inside"] is False for j in all_joints),
                         "max_ellipse_reference_normalized_sum_squared": max(j["ellipse_reference_normalized_sum_squared"] or 0 for j in all_joints),
                         "max_independent_pyramid_swing_excess_degrees": max((e or 0) for j in all_joints for e in j["independent_pyramid_swing_excess_degrees"]),
                         "independent_pyramid_observations_excess_over_1_degree": sum(max((e or 0) for e in j["independent_pyramid_swing_excess_degrees"]) > 1 for j in all_joints)}
    if hashes != {str(p): sha(p) for p in (review_path, probe_path)}:
        raise ValueError("Input evidence changed during audit")
    result["input_bytes_unchanged"] = True
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    print(json.dumps({"output": str(output), "summary": result["summary"], "visual_status": args.visual_status}, ensure_ascii=False))


if __name__ == "__main__":
    main()
