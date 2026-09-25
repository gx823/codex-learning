"""Prepare six source-bound native corner capture requests; never starts Unreal.

Run with ordinary local Python after an actual HarborCorner author PASS:
  python ue_m5_vs2_corner_review_author.py --evidence-dir <fresh VS2 docs dir>
Optional --corner-author pins its author_result.json; otherwise the latest PASS
HarborCorner report is used. Outputs only corner_capture_plan.json and
author_result.json. No import unreal, subprocess, package writes or screenshots.
Optional --profile Portraits requests three actual eye-locked character cameras
under the same two authored lighting presets. It never re-authors the map.
The small opt-in runtime consumer is implemented/run separately by root.
"""
from pathlib import Path
import argparse
import datetime as dt
import hashlib
import json
import math

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
LAYOUT = DOC / 'research/HARBOR_CORNER_P0_LAYOUT.json'
MAP = '/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0'
OWNER = 'HarborCity_M5_VS2_HarborCorner_P0'
CAMERAS = ('SouthStreet', 'Promenade', 'GuildSquare')
PRESETS = ('Afternoon', 'Dusk')


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()


def bind(file):
    file = Path(file).resolve()
    return dict(path=str(file), sha256=sha(file), bytes=file.stat().st_size)


def read_document(file):
    file = Path(file).resolve()
    if not file.is_relative_to(DOC) or not file.is_file():
        raise RuntimeError('Required input JSON outside VS2 evidence or missing: ' + str(file))
    return json.loads(file.read_text(encoding='utf-8-sig'))


def disk(package, extension='.uasset'):
    package = str(package).split('.')[0]
    if not package.startswith('/Game/HarborCity/') or '..' in package:
        raise RuntimeError('Unexpected project package: ' + package)
    return PROJECT / 'Content' / Path(package.removeprefix('/Game/')).with_suffix(extension)


def resolve_author(explicit):
    if explicit:
        file = Path(explicit).resolve()
        candidates = [(file, read_document(file))]
    else:
        files = sorted((DOC / 'editor_runtime').glob('author_*HarborCorner*/author_result.json'),
                       key=lambda p: p.stat().st_mtime, reverse=True)
        candidates = ((p, read_document(p)) for p in files)
    for file, data in candidates:
        if data.get('status') == 'PASS' and data.get('map') == MAP and data.get('owner') == OWNER:
            return file, data
    raise RuntimeError('No actual PASS author of the exact owned HarborCorner map; no runnable plan can be bound yet')


def camera_rotation(location, target):
    delta = [target[i]-location[i] for i in range(3)]
    if not all(math.isfinite(x) for x in (*location, *target)) or math.dist(location, target) < 1:
        raise RuntimeError('Nonfinite or degenerate review camera')
    return [math.degrees(math.atan2(delta[2], math.hypot(delta[0], delta[1]))),
            math.degrees(math.atan2(delta[1], delta[0])), 0.0]


def build_plan(author_file, author, layout, profile='Base'):
    if profile not in ('Base', 'Portraits'):
        raise RuntimeError('Only Base and Portraits review profiles are supported')
    if layout.get('schema_version') != 1 or layout.get('map') != MAP or layout.get('owner') != OWNER:
        raise RuntimeError('Unexpected layout identity')
    if author['inputs']['layout']['sha256'] != sha(LAYOUT):
        raise RuntimeError('Layout changed after native map author; re-author/review the actual map first')
    # Bind real native package bytes. Merely having a proposed map path is not
    # sufficient to create a runtime-ready capture plan.
    packages = []
    for asset in author['assets']:
        file = disk(asset['path'], '.umap' if asset['path'] == MAP else '.uasset')
        if not file.is_file() or sha(file) != asset['sha256']:
            raise RuntimeError('Saved map/dedicated asset no longer matches author evidence: ' + asset['path'])
        packages.append(dict(package=asset['path'], **bind(file)))
    if not any(p['package'] == MAP for p in packages):
        raise RuntimeError('Native map package missing from author evidence')
    copy_binding = author['inputs']['village_copy']
    copy_file = Path(copy_binding['path']).resolve()
    copied = read_document(copy_file)
    if (sha(copy_file) != copy_binding['sha256'] or copied.get('status') != 'PASS'
            or copied.get('mode') != 'Copy' or copied.get('geometry_paths_state') != 'SAVED'
            or copied.get('source_unchanged') is not True):
        raise RuntimeError('Actual saved Village Copy evidence mismatch')
    saved_cameras = {row['name']: row for row in author['capture_camera_plan']}
    layout_cameras = {row['name']: row for row in layout['cameras']}
    actor_rows = {a['label']: a for a in author['actors']}
    main_label = author['lighting']['main_light_actor']
    bindings = dict(main_light=dict(label=main_label, class_path='/Script/Engine.DirectionalLight'),
        sky_light=dict(label='HC_M5VS2_Corner_SkyLight', class_path='/Script/Engine.SkyLight'),
        post_process=dict(label='HC_M5VS2_Corner_FixedExposure', class_path='/Script/Engine.PostProcessVolume'))
    for row in bindings.values():
        if row['label'] not in actor_rows or actor_rows[row['label']]['class_path'] != row['class_path']:
            raise RuntimeError('Expected native lighting Actor missing from saved author report: ' + row['label'])
        row['owner_tag'] = OWNER
        row['runtime_selector'] = 'Exactly one Actor with native class and owner tag; editor label is diagnostic, not a shipping-only dependency.'
    if author['lighting']['saved_preset'] != 'Afternoon' or author['lighting']['presets'] != layout['lighting_presets']:
        raise RuntimeError('Saved lighting declaration differs from layout')
    sky_policy = author['lighting'].get('sky_policy')
    if not isinstance(sky_policy, str) or not sky_policy.strip():
        raise RuntimeError('Successful corner author must declare its actual authored sky policy')
    shots = []
    for preset_name in PRESETS:
        preset = layout['lighting_presets'][preset_name]
        for camera_name in CAMERAS:
            camera = layout_cameras[camera_name]
            saved = saved_cameras[camera_name]
            if any(saved[k] != camera[k] for k in ('location_cm', 'target_cm', 'fov')):
                raise RuntimeError('Saved camera declaration differs from layout: ' + camera_name)
            if actor_rows.get(saved['actor'], {}).get('class_path') != '/Script/Engine.CameraActor':
                raise RuntimeError('Expected native camera missing: ' + camera_name)
            index = len(shots)
            label = '%02d_%s_%s' % (index, preset_name, camera_name)
            shots.append(dict(index=index, label=label, expected_png=label+'.png', capture_status='NOT_RUN',
                camera_actor_label=saved['actor'], camera_name=camera_name,
                camera_location_cm=camera['location_cm'], camera_rotation_pitch_yaw_roll=camera_rotation(camera['location_cm'], camera['target_cm']),
                camera_fov_degrees=float(camera['fov']), light_preset=preset_name,
                sun_rotation_pitch_yaw_roll=[preset['sun_pitch'], preset['sun_yaw'], 0.0],
                sun_color_linear_rgba=preset['sun_color']+[1.0], sun_intensity=preset['sun_intensity'],
                sky_intensity=preset['sky_intensity'], exposure_bias=preset['exposure_bias'], bloom_intensity=preset['bloom_intensity'],
                lighting_origin=('AUTHOR_DECLARED_SAVED_DEFAULT_REQUIRES_RUNTIME_READBACK' if preset_name == 'Afternoon'
                                 else 'EXPLICIT_RUNTIME_LIGHT_OVERRIDE_NOT_SAVED_MAP_DEFAULT'),
                sky_material_policy=sky_policy,
                minimum_settle_seconds=10.0 if index in (0, 3) else 2.0,
                minimum_settle_rendered_frames=30,
                same_camera_comparison_index=index+3 if index < 3 else index-3))
    plan = dict(schema_version=1, plan_type='HARBOR_CORNER_NATIVE_VIEWPORT', status='READY_FOR_RUNTIME_NOT_CAPTURED',
        map=MAP, owner=OWNER, expected_screenshots=6, expected_resolution=[1920, 1080],
        authoring_scope='Disk/evidence-bound plan only. No UE execution, native runtime defaults observation, screenshot generation or art acceptance.',
        sources=dict(layout=bind(LAYOUT), corner_author=bind(author_file), village_copy=bind(copy_file),
                     plan_script=bind(Path(__file__)), map_and_dedicated_packages=packages),
        actor_bindings=bindings, shots=shots, sky_policy_authoritative=sky_policy,
        atmosphere_author_readback=author['lighting'].get('saved_atmosphere_actual'),
        runtime_report_compatibility_note='This source-bound plan and the actual saved author atmosphere fields supersede the older CornerReview consumer top-level fixed day-sky policy text. That legacy text is not an observation; native screenshots still require visual review.',
        runtime_contract=dict(
            consumer_state='NOT_IMPLEMENTED_BY_THIS_SCRIPT; root owns the small native opt-in consumer',
            opt_in_flag='M5VS2CornerReview', plan_argument='M5VS2CornerPlan', evidence_argument='M5VS2EvidenceDir',
            evidence_root=str(DOC), maximum_duration_seconds=120, initial_warmup_seconds=10,
            minimum_between_shots_seconds=2, use_real_rendered_frames=True,
            screenshot_api='FScreenshotRequest::RequestScreenshot; OnScreenshotRequestProcessed plus exact PNG file/size verification',
            hide_hud_only_during_review=True, preserve_pawn_animation_and_input=True,
            character_material_overrides=False, pose_or_position_overrides=False,
            observe_viewport_escape_without_consuming=True, escape_permanently_stops_camera_light_capture_restore_and_autoquit=True,
            keep_existing_p_pause_route=True, autoquit_requires_explicit_flag='M5VS2AutoQuit',
            autoquit_on_success_requires_all_six_verified=True, autoquit_on_non_esc_fail=True,
            startup_readiness_deadline_seconds=10, restore_original_view_hud_lighting_on_normal_completion=True,
            sample_actual_defaults_before_first_override=True,
            reject_different_map_or_duplicate_actor_bindings=True,
            capture_failure_policy='Record FAIL/NOT_RUN with real reason; never substitute a reference, prior PNG or generated image.'),
        required_runtime_evidence=[
            'Plan SHA256 and actual world package path; input map SHA256 verified by launcher/consumer before launch.',
            'Original actual directional light transform/color/intensity, SkyLight intensity, post-process exposure/bloom and current view/HUD before any override.',
            'For each shot: actual PlayerCameraManager camera location/rotation/FOV, main light/SkyLight/post-process values and original-vs-override label.',
            'For each shot: current Hero BP/AnimBP/material interfaces and Q/R Blueprint/profile paths; no silent lookdev material replacement.',
            'Actual shader/streaming readiness and elapsed real rendered frames; pending compile or fallback must not be called visual PASS.',
            'Actual requested/completed frame, exact raw PNG absolute path, PNG dimensions and nonzero file size; total real file count.',
            'Viewport Esc stop frame; all remaining shots NOT_RUN and no auto-exit or late restore after Esc.'
        ],
        review_notes=[
            'Three unique saved camera configurations repeated under two presets yield six raw viewport captures.',
            'Afternoon becomes an actual default capture only if the consumer first verifies the saved runtime values; the author declaration alone is insufficient.',
            'Dusk is explicitly a staged light comparison; inspect the source-bound sky_policy and actual saved atmosphere. No completed playable time/weather system is claimed.',
            'PlayerStart is authored on the adjacent sidewalk, outside the SouthStreet camera foreground. Characters remain visible and are never hidden or moved by the capture Director; record actual occlusion.',
            'Lights/colors stored in native components may be quantized; record actual effective values, not only these requested parameters.',
            'No interiors, input acceptance, driving, navigation, ragdoll quality or naturalness is established by static screenshots.'
        ],
        capture_summary=dict(status='NOT_RUN', actual_png_count=0, visual_acceptance='USER_REVIEW'))
    if profile == 'Portraits':
        hero = author.get('hero_binding', {})
        hero_class = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia.BP_M5VS2_Selestia_C'
        if hero.get('generated_class') != hero_class:
            raise RuntimeError('Portraits require the actual saved VS2 Hero class binding')
        subjects = [dict(id='Hero', blueprint_class=hero_class, left_eye_bone='LeftEye', right_eye_bone='RightEye',
                         camera_distance_cm=130.0, source='corner_author.hero_binding; runtime named eye bones required')]
        for sample in ('Q', 'R'):
            matches = [n for n in author.get('npc_bindings', []) if n.get('sample') == sample]
            if len(matches) != 1:
                raise RuntimeError('Exactly one saved NPC binding required for portrait ' + sample)
            npc = matches[0]
            blueprint = npc['blueprint']
            prefix = '/Game/HarborCity/M5VS2/NPC/AvatarSample_' + sample + '/Runtime_'
            name = 'BP_AvatarSample_' + sample
            if (not blueprint.startswith(prefix) or '..' in blueprint or not blueprint.endswith('/'+name)
                    or not npc['profile'].startswith(prefix) or '..' in npc['profile']
                    or not npc['profile'].endswith('/DA_NPCProfile.DA_NPCProfile')):
                raise RuntimeError('Unexpected exact source NPC Blueprint/profile binding')
            bones = npc.get('humanoid_bones', {})
            if bones.get('leftEye') != 'J_Adj_L_FaceEye' or bones.get('rightEye') != 'J_Adj_R_FaceEye':
                raise RuntimeError('Saved NPC actual eye-bone mapping is missing or unexpected')
            class_path = blueprint + '.' + name + '_C'
            if actor_rows.get(npc['actor'], {}).get('class_path') != class_path:
                raise RuntimeError('Saved NPC actor class differs from portrait subject')
            subjects.append(dict(id=sample, blueprint_class=class_path, npc_profile=npc['profile'],
                                 left_eye_bone=bones['leftEye'], right_eye_bone=bones['rightEye'], camera_distance_cm=150.0,
                                 source='corner_author.npc_bindings actual humanoid_bones and actor class'))
        for index, shot in enumerate(shots):
            subject = subjects[index % 3]['id']
            camera_name = subject + 'Portrait'
            label = '%02d_%s_%s' % (index, shot['light_preset'], camera_name)
            # No guessed/static eye height is serialized as an actual camera request.
            for key in ('camera_actor_label', 'camera_location_cm', 'camera_rotation_pitch_yaw_roll'):
                del shot[key]
            shot.update(label=label, expected_png=label+'.png', camera_name=camera_name, camera_fov_degrees=40.0,
                        portrait_subject=subject, camera_mode='LOCK_ACTUAL_ANIMATED_EYES_ON_FIRST_USE')
        plan.update(profile='Portraits', portrait_subjects=subjects,
                    portrait_camera_policy='After real warmup/readiness, sample each actual eye midpoint once; position along current Actor yaw at 130/150 cm. Lock and reuse this exact camera for both light presets. No CDO fallback, pose freeze, actor rotation or light changes beyond the existing two presets.',
                    portrait_material_validation='NOT_RUN: existing global shader/streaming gate does not establish individual render-proxy fallback completeness.')
        plan['review_notes'][0] = 'Three actual eye-locked portraits (Hero/Q/R), each repeated under the same authored Afternoon/Dusk presets, yield six raw viewport captures.'
        plan['review_notes'][3] = 'Characters stay at their authored posts and keep live animation/materials. Camera sampling does not change eye/head pose or Actor transform. Record actual occlusion; do not add a portrait fill light.'
        plan['required_runtime_evidence'].append('Portraits: exact unique Hero/Q/R class/profile; first-use actual left/right eye bone names, indices and world midpoint; locked cameras; current subject transforms/materials and PlayerCameraManager request/end-draw match. No OS input test.')
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence-dir', required=True)
    parser.add_argument('--corner-author')
    parser.add_argument('--profile', choices=('Base', 'Portraits'), default='Base')
    args = parser.parse_args()
    out = Path(args.evidence_dir).resolve()
    if out == DOC or not out.is_relative_to(DOC) or (out/'author_result.json').exists() or (out/'corner_capture_plan.json').exists():
        raise RuntimeError('Fresh output directory beneath VS2 evidence required')
    out.mkdir(parents=True, exist_ok=True)
    result = dict(status='RUNNING', phase='CornerReviewPlan', started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
                  assets_modified=0, ue_processes_started=0, screenshots_created=0, runtime='NOT_RUN', visual='NOT_RUN')
    try:
        author_file, author = resolve_author(args.corner_author)
        plan = build_plan(author_file, author, read_document(LAYOUT), args.profile)
        target = out/'corner_capture_plan.json'
        target.write_text(json.dumps(plan, ensure_ascii=False, indent=2), encoding='utf-8')
        result.update(status='PASS', pass_scope='Exact source-bound six-shot plan written only; native captures NOT_RUN.',
                      capture_plan=bind(target), expected_screenshots=6,
                      runtime_arguments=[MAP, '-game', '-M5VS2CornerReview', '-M5VS2CornerPlan="'+str(target)+'"',
                          '-M5VS2EvidenceDir=<fresh absolute VS2 docs directory>', '-M5VS2AutoQuit'])
        print(json.dumps(dict(status='PASS_PLAN_ONLY', plan=str(target), screenshots='NOT_RUN'), ensure_ascii=False))
    except Exception as exc:
        result.update(status='FAIL', error=str(exc))
        raise
    finally:
        result['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
        (out/'author_result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
