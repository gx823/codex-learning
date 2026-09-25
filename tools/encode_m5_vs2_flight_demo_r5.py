"""Encode one bound R5 VS2 native FlightDemo capture, preserving actual frame times.

Read-only source frames/WAV/evidence; new unique output directory only. Reuses
the existing M4-R2 PPM, PCM and timeline helpers, not its delay-zero entrypoint.
Diagnostic mode never upgrades a failed/stopped run. No UE or desktop control,
interpolation, speed adjustment, replacement audio, captions or synthetic shots.
Original native silent PCM requires --allow-native-silent-mix and both native
silent-rendering/restoration flags. Missing or incomplete WAV is always rejected.
"""
from pathlib import Path
import argparse
import csv
import datetime as dt
import hashlib
import importlib.util
import json
import math
import re
import subprocess
import sys
import uuid

sys.dont_write_bytecode = True

ROOT = Path('D:/科研学习/codex学习')
DOCS = ROOT / 'docs/HarborCity_M5_VS2'
RECORDINGS = DOCS / 'recordings'
BASE = ROOT / 'tools/encode_m4_r2_playthrough.py'
BASE_SHA256 = '4ec810ca1782a4ab5359ec76edfe2b0e417626580402eabc7d2d9820b65dbd8b'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load_encoder():
    require(BASE.is_file() and sha(BASE) == BASE_SHA256, 'Existing encoder changed; review it before using this wrapper')
    spec = importlib.util.spec_from_file_location('hc_vs2_existing_pts_audio_encoder', BASE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def scoped_file(value, root, name=None):
    path = Path(value).resolve()
    require(path.is_relative_to(root.resolve()) and path != root.resolve(), 'Input escapes explicit VS2 scope')
    require(path.is_file() and (name is None or path.name == name), 'Missing or wrong evidence filename: ' + str(path))
    return path


def read_json(path, tracked):
    before = sha(path)
    result = json.loads(path.read_text(encoding='utf-8-sig'))
    require(sha(path) == before, 'Input changed while reading JSON')
    tracked[path] = before
    return result


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def timestamp(value):
    result = dt.datetime.fromisoformat(value)
    require(result.tzinfo is not None, 'Launcher timestamp has no timezone')
    return result.astimezone(dt.timezone.utc)


def validate_frame_rows(data):
    rows = data['frames']
    require(isinstance(rows, list) and 1 < len(rows) <= 3601, 'Capture must contain 2..3601 bounded native frames')
    for index, row in enumerate(rows):
        require(row.get('file') == f'frame_{index:06d}.ppm', 'Frame filename/order is not the native contiguous sequence')
        require(finite(row.get('seconds')) and row['seconds'] >= 0, 'Nonfinite/negative native timestamp')
    require(all(b['seconds'] > a['seconds'] for a, b in zip(rows, rows[1:])), 'Native timestamps are not strictly increasing')
    return rows


def validate_native(data):
    require(data.get('milestone') == 'M5_VS2', 'Capture is not M5_VS2')
    require(data.get('status') in ('CAPTURED_PENDING_REVIEW', 'USER_ABORTED'), 'Native capture itself failed')
    require(data.get('write_failures') == 0 and data.get('capture_geometry_valid') is True, 'Incomplete pixels or invalid viewport geometry')
    require(data.get('framegrabber_latency') == 0 and data.get('requested_fps') == 30, 'Unverified grabber latency or frame request rate')
    require(data.get('audio_file') == 'game_audio.wav' and data.get('audio_export_requested') is True, 'Actual native master-submix export required')
    require(data.get('automatic_restart') is False, 'Unexpected automatic capture restart')
    require(data.get('configured_delay_seconds') == 12 and data.get('requested_duration_seconds') == 60, 'FlightDemo native delay/duration contract changed')
    require(type(data.get('user_stop_latched')) is bool, 'Missing native stop state')
    require(finite(data.get('wall_seconds')) and 0 < data['wall_seconds'] <= 120, 'Invalid capture wall duration')
    size = dict(width=1920, height=1080)
    require(data.get('width') == 1920 and data.get('height') == 1080, 'Native FlightDemo must actually be 1920x1080')
    require(data.get('requested_size') == size and data.get('output_size') == size, 'Native requested/output sizes differ')
    rect = data['capture_rect']; window = data['window_size']
    require(all(type(rect[k]) is int for k in ('min_x', 'min_y', 'max_x', 'max_y')), 'Invalid native capture rectangle')
    require(rect['max_x'] - rect['min_x'] == 1920 and rect['max_y'] - rect['min_y'] == 1080, 'Capture rectangle differs from PPM output')
    require(0 <= rect['min_x'] < rect['max_x'] <= window['width'] and 0 <= rect['min_y'] < rect['max_y'] <= window['height'], 'Capture rectangle escapes native game window')
    for key in ('requested', 'dropped'):
        require(type(data.get(key)) is int and data[key] >= 0, 'Invalid native frame counter: ' + key)
    rows = validate_frame_rows(data)
    require(data['requested'] >= len(rows), 'Saved frames exceed actual requests')
    return rows


def log_capture_binding(log_path, capture, launch):
    text = log_path.read_text(encoding='utf-8-sig', errors='replace')
    events = []
    for number, line in enumerate(text.splitlines(), 1):
        for label, pattern in (
            ('STARTED', r'\bM3_RECORDING_STARTED (.+?) rect='),
            ('FINISHED', r'\bM3_RECORDING_FINISHED (.+?)\s*$')):
            match = re.search(pattern, line)
            if not match:
                continue
            stamp = re.match(r'^\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]', line)
            require(stamp is not None, 'Recording log event lacks native UTC timestamp')
            when = dt.datetime.strptime(stamp[1], '%Y.%m.%d-%H.%M.%S:%f').replace(tzinfo=dt.timezone.utc)
            require(Path(match[1].strip().strip('"')).resolve() == capture.parent, 'Log refers to another recording')
            events.append(dict(event=label, line=number, utc=when.isoformat()))
    require([x['event'] for x in events] == ['STARTED', 'FINISHED'], 'Require one complete actual native capture start/finish pair')
    lo, hi = timestamp(launch['started_at']), timestamp(launch['ended_at'])
    a, b = (timestamp(x['utc']) for x in events)
    require(lo <= a <= b <= hi, 'Capture log events outside actual process lifetime')
    return events, text


def validate_binding(args, capture, data, tracked, report):
    launch_path = scoped_file(args.launch, DOCS, 'launch.json')
    result_path = scoped_file(args.run_results, DOCS, 'flight_demo.json')
    require(launch_path.parent.name.endswith('_flight_demo_r5_game') and result_path.parent.parent == launch_path.parent, 'FlightDemo run and launch directories do not match')
    require(re.fullmatch(r'FlightDemo_[A-Za-z0-9_]+', result_path.parent.name), 'Unexpected native FlightDemo directory')
    launch = read_json(launch_path, tracked); result = read_json(result_path, tracked)
    require(launch.get('schema') == 'HarborCity.M5VS2.FlightDemoR5.Launch.v1' and launch.get('status') == 'EXITED', 'Require the actual waited, completed launch')
    require(type(launch.get('exit_code')) is int, 'Actual process exit code is missing')
    require(result.get('schema') == 'HarborCity.M5VS2.FlightDemoR5.v1', 'Unexpected native demo schema')
    require(launch.get('kind') == 'EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED' and result.get('packaged_runtime') == 'NOT_RUN_EDITOR_GAME', 'Do not relabel editor-game as packaged runtime')
    require(launch.get('input_scope') == result.get('input_scope') == 'ENGINE_INPUT_NOT_OS' and result.get('os_input') == launch.get('os_input_result') == 'NOT_RUN', 'Input evidence scope mismatch')
    require(launch.get('auto_engine_input') is True and launch.get('auto_camera') is False, 'Unexpected demo input/camera route')
    require(scoped_file(launch['result_file'], DOCS, 'flight_demo.json') == result_path and launch.get('result_sha256', '').lower() == tracked[result_path], 'Launcher result path/hash does not match')
    require(launch.get('demo_result') == result.get('status') and launch.get('recording_result') == data.get('status'), 'Launcher observed a different final native result')
    require(Path(launch['recording_directory']).resolve() == Path(result['native_capture_directory']).resolve() == capture.parent, 'Capture directory not bound to this actual run')
    require(result.get('actual_native_capture') == data, 'Final FlightDemo embedded capture differs from disk JSON')
    require(result.get('audio_export_pending') is False, 'Native audio export remains pending')
    map_name = data.get('map', '')
    require(re.fullmatch(r'/Game/HarborCity/M5VS2/FlightDemoR5/Run_[0-9a-f]{12}/L_FlightDemoR5', map_name), 'Unexpected private demo map')
    require(map_name == result.get('map') == launch.get('map'), 'Native map binding mismatch')
    slot = data.get('actual_save_slot', '')
    require(re.fullmatch(r'HarborCity_VS2_FlightDemo_R5_[A-Za-z0-9_]+', slot) and len(slot) <= 100, 'Invalid private demo save slot')
    require(slot == data.get('requested_save_slot') == result.get('private_save_slot') == launch.get('save_slot'), 'Native private-slot binding mismatch')
    require(launch.get('record_delay') == 12 and launch.get('record_seconds') == 60, 'Launch capture configuration mismatch')
    for field in ('scripted_teleports', 'camera_pose_setters', 'retries'):
        require(result.get(field) == 0, 'Unexpected scripted camera/teleport/retry')
    require(type(result.get('stop_latched')) is bool, 'Missing native demo stop latch')
    require(result.get('status') in ('PASS_ENGINE_INPUT_ONLY', 'FAIL', 'USER_ABORTED', 'NOT_RUN'), 'Run is unfinished or has an unknown result')
    if launch.get('polish_author'):
        author_path = scoped_file(launch['polish_author'], DOCS, 'author_result.json')
        author = read_json(author_path, tracked)
        require(tracked[author_path] == launch.get('polish_author_sha256', '').lower(), 'Polish author binding differs')
        command = read_json(scoped_file(author_path.parent / 'commandlet.json', DOCS, 'commandlet.json'), tracked)
        require(author.get('status') == 'PASS' and author.get('phase') == 'PlayablePolishAuthor' and not author.get('changed_sources'), 'Polish native author failed')
        require(command.get('status') == 'PASS' and command.get('exit_code') == 0 and command.get('phase') == 'PlayablePolishAuthor', 'Polish commandlet did not finish')
        require(author['maps']['Flight'] == map_name and author['runtime_module']['sha256'].lower() == launch.get('module_sha256', '').lower(), 'Polish map/module identity differs')
        require(all(x['status'] == 'PASS' for x in author['checks']), 'Polish author contains failed checks')
        for row in author['assets']:
            require(Path(row['path']).resolve().is_relative_to((ROOT / 'HarborCity/Content').resolve()) and sha(Path(row['path'])) == row['sha256'].lower(), 'Native saved polish asset changed')
        report['polish_author_binding'] = dict(author=str(author_path), hero=author['hero'], vehicle=author['vehicle'], scope='Native saved assets, then actual separate game load. No invented Reload phase; art remains USER_REVIEW.')
    else:
        manifest_path = scoped_file(launch['manifest'], DOCS, 'flight_demo_r5_manifest.json')
        manifest = read_json(manifest_path, tracked)
        require(tracked[manifest_path] == launch.get('manifest_sha256', '').lower(), 'Launch manifest hash differs')
        require(manifest.get('schema') == 'HarborCity.M5VS2.FlightDemoR5.Author.v1' and manifest.get('status') == 'READY_FOR_ENGINE_INPUT_DEMO_NOT_RUNTIME_TESTED', 'Missing native-author manifest')
        require(manifest.get('map') == map_name and manifest.get('expected_npcs') == 8 and manifest.get('expected_vehicles') == 1, 'Authored scene identity differs')
        reload_path = scoped_file(launch['reload'], DOCS, 'author_result.json')
        require(reload_path.parent == manifest_path.parent and Path(manifest['reload_report']).resolve() == reload_path, 'Manifest/reload pairing differs')
        reload = read_json(reload_path, tracked)
        require(tracked[reload_path] == launch.get('reload_sha256', '').lower(), 'Launch reload evidence hash differs')
        command_path = scoped_file(reload_path.parent / 'commandlet.json', DOCS, 'commandlet.json')
        command = read_json(command_path, tracked)
        require(reload.get('status') == 'PASS' and reload.get('phase') == 'FlightDemoR5Reload' and reload.get('fresh_process_disk_readback') == 'PASS', 'Native fresh Reload did not pass')
        require(command.get('status') == 'PASS' and command.get('exit_code') == 0, 'Native author process did not complete successfully')
        require(reload['launch_manifest']['sha256'].lower() == tracked[manifest_path] and reload['launch_manifest']['bytes'] == manifest_path.stat().st_size, 'Reload manifest proof differs')
        require(not reload['preservation']['changed_files'] and all(x.get('status') == 'PASS' for x in reload['checks']), 'Native Reload preservation/check failed')
        modules = [x for x in manifest.get('source_bindings', []) if x.get('kind') == 'module']
        require(len(modules) == 1 and modules[0]['sha256'].lower() == launch.get('module_sha256', '').lower(), 'Recorded module identity differs from native manifest')
        validate_r5_author_chain(manifest, reload, reload_path, command, tracked, report)
    log_path = scoped_file(launch_path.parent / 'game.log', DOCS, 'game.log')
    tracked[log_path] = sha(log_path)
    events, log_text = log_capture_binding(log_path, capture, launch)
    logged_span = (timestamp(events[1]['utc']) - timestamp(events[0]['utc'])).total_seconds()
    require(abs(logged_span - data['wall_seconds']) <= 2, 'Capture wall duration differs from its actual native log span')
    stopped_in_log = 'M5VS2_FLIGHT_DEMO_USER_STOP' in log_text
    evidence_failure = 'M5VS2_FLIGHT_DEMO_EVIDENCE_WRITE_FAILED' in log_text or 'M5VS2_FLIGHT_DEMO_FINAL_EVIDENCE_FAILED' in log_text
    require(not evidence_failure, 'Actual native demo final evidence publication failed')
    check_rows = result.get('checks', [])
    require(isinstance(check_rows, list), 'Native checks are not an array')
    reasons = []
    if launch['exit_code'] != 0: reasons.append('actual_process_exit_nonzero')
    if result['status'] != 'PASS_ENGINE_INPUT_ONLY': reasons.append('native_demo_' + result['status'])
    if result['stop_latched'] or data['user_stop_latched'] or stopped_in_log: reasons.append('user_stop_latched_or_logged')
    if data['status'] != 'CAPTURED_PENDING_REVIEW': reasons.append('capture_' + data['status'])
    if data.get('stop_reason') != 'vs2_gameplay_sequence_finished': reasons.append('capture_stop_' + str(data.get('stop_reason')))
    if not check_rows or any(x.get('status') != 'PASS' for x in check_rows): reasons.append('native_checks_missing_or_failed')
    if args.known_visual_failure: reasons.append('operator_reported_visual_failure')
    complete_checks = {x.get('name') for x in check_rows if x.get('status') == 'PASS'}
    required_checks = {'actual_bounded_demo_sequence_completed', 'actual_boost_dive_then_pullup', 'normal_V_first_person_observed', 'normal_V_third_person_restored', 'actual_return_dry_floor'}
    if not required_checks.issubset(complete_checks): reasons.append('required_demo_stages_not_observed')
    if not (finite(result.get('actual_orbit_angle_degrees')) and result['actual_orbit_angle_degrees'] >= 360): reasons.append('orbit_not_complete')
    report['source_run'] = dict(status=result['status'], reason=result.get('reason'), exit_code=launch['exit_code'], stop_latched=result['stop_latched'], capture_status=data['status'], capture_stop_reason=data['stop_reason'], formal_disqualifiers=reasons)
    report['evidence_binding'] = dict(status='BOUND_ACTUAL_NATIVE_LOG_AND_JSON', launch=str(launch_path), results=str(result_path), capture_events=events, map=map_name, private_save_slot=slot,
        module_sha256=launch['module_sha256'], hash_scope='Recorded manifest/module identity; later working-tree/DLL changes do not rewrite historical evidence. No current binary compatibility claim.')
    if not args.diagnostic:
        require(not reasons, 'Formal recording disqualified: ' + ', '.join(reasons))
        require(30 <= data['wall_seconds'] <= 60 and finite(result.get('sequence_wall_seconds')) and 30 <= result['sequence_wall_seconds'] <= 60, 'Formal native sequence must actually last 30..60 seconds')


def validate_audio_policy(allow_silent, capture, audio):
    """Does not validate or synthesize PCM; caller must first inspect the real WAV."""
    require(audio.get('riff_complete') is True and audio.get('bits') == 16
            and finite(audio.get('duration_seconds')) and audio['duration_seconds'] > 0
            and type(audio.get('sample_frames')) is int and audio['sample_frames'] > 0,
            'Actual complete nonempty PCM16 native WAV required, including silent mix')
    require(type(audio.get('signal_present')) is bool, 'Actual measured native audio signal state missing')
    has_signal = audio['signal_present']
    if not has_signal:
        require(allow_silent is True, 'Silent native WAV requires explicit --allow-native-silent-mix')
        require(capture.get('audio_silent_submix_rendering_during_capture') is True
                and capture.get('audio_submix_override_restored') is True,
                'Silent native WAV requires actual capture submix rendering and restored-state proof')
    return dict(classification='NATIVE_SIGNAL_PRESENT' if has_signal else 'NATIVE_SILENT_MIX',
        allow_native_silent_mix=allow_silent, original_wav_required=True, synthetic_audio=False,
        audio_track_policy='ENCODE_ORIGINAL_NATIVE_WAV_TO_ONE_AAC_TRACK',
        native_silent_rendering=capture.get('audio_silent_submix_rendering_during_capture'),
        native_submix_restored=capture.get('audio_submix_override_restored'),
        measured_nonzero_samples=audio.get('nonzero_samples'), measured_rms_dbfs=audio.get('rms_dbfs'),
        measured_peak_normalized=audio.get('peak_normalized'),
        silence_scope='Zero or below the existing 1e-5 RMS signal threshold; actual PCM statistics retained, not a no-audio-track fallback.',
        auditory_quality='NOT_ASSESSED')


def validate_r5_author_chain(manifest, reload, reload_path, command, tracked, report):
    """Hash-bind real author/Reload and R5/safety provenance, without comparing old DLL to current DLL."""
    require(manifest.get('art_revision') == reload.get('art_revision') == 5, 'R5 author revision missing')
    proof = manifest.get('source_corner_proof', {})
    safety = manifest.get('hero_safety', {})
    require(proof.get('art_revision') == 5 and safety.get('status') == 'NATIVE_RELOAD_SELECTED_NOT_ALL_ANGLE_ART_APPROVED', 'R5 environment/safety-layer provenance missing')
    require(safety.get('blueprint') == manifest.get('selected_hero') == reload.get('selected_hero')
            and re.fullmatch(r'/Game/HarborCity/M5VS2/HeroModesty/Review_[0-9a-f]{12}/BP_HeroSafetyShorts', safety.get('blueprint', '')), 'R5 selected Hero differs from native safety candidate')
    require(reload.get('hero_safety') == safety and reload.get('source_corner_proof') == proof, 'Reload/manifest proof chain differs')
    observed = []

    def bound_report(reference, phase, schema=None, expected_script=None):
        file = scoped_file(reference['path'], DOCS, 'author_result.json')
        data = read_json(file, tracked)
        require(tracked[file] == reference['sha256'].lower(), 'Bound author report SHA differs: ' + phase)
        cmd = read_json(scoped_file(file.parent / 'commandlet.json', DOCS, 'commandlet.json'), tracked)
        require(data.get('status') == 'PASS' and data.get('phase') == phase
                and (schema is None or data.get('schema') == schema)
                and all(row.get('status') == 'PASS' for row in data.get('checks', [])), 'Native author phase failed: ' + phase)
        require(cmd.get('status') == 'PASS' and cmd.get('exit_code') == 0
                and cmd.get('phase') == phase and cmd.get('author_status') == 'PASS', 'Native author process failed: ' + phase)
        if expected_script:
            script = scoped_file(cmd['script'], ROOT / 'tools', expected_script)
            digest = sha(script); tracked[script] = digest
            require(digest == cmd['script_sha256'].lower(), 'Author script differs from actually executed SHA: ' + phase)
            bindings = [row for row in manifest['source_bindings'] if row.get('kind') == 'script'
                        and Path(row.get('path', '')).resolve() == script]
            require(len(bindings) == 1 and bindings[0]['sha256'].lower() == digest, 'Executed author script missing/different in actual manifest: ' + phase)
        observed.append(dict(phase=phase, path=str(file), sha256=tracked[file], process_exit_code=cmd['exit_code'],
                             executed_author_script_sha256=cmd.get('script_sha256')))
        return data

    flight_schema = 'HarborCity.M5VS2.FlightDemoR5.Author.v1'
    live_schema = 'HarborCity.M5VS2.CornerPlayableR5.v1'
    # Own Reload command is checked both by the original wrapper and here for
    # exact script identity; paths/hashes are actual produced data, not a guess.
    own_ref = dict(path=str(reload_path), sha256=tracked[reload_path])
    bound_report(own_ref, 'FlightDemoR5Reload', flight_schema, 'ue_m5_vs2_flight_demo_r5_author.py')
    demo = bound_report(reload['source_author'], 'FlightDemoR5', flight_schema, 'ue_m5_vs2_flight_demo_r5_author.py')
    require(demo.get('map') == manifest['map'] and demo.get('hero_safety') == safety
            and demo.get('source_corner_proof') == proof, 'Actual Flight Apply differs from Reload proof')
    live = bound_report(demo['inputs']['live_reload'], 'CornerPlayableR5Reload', live_schema, 'ue_m5_vs2_corner_playable_r5_author.py')
    require(live.get('fresh_process_disk_readback') == 'PASS' and live.get('hero_safety') == safety
            and live.get('source_corner_proof') == proof, 'R5 playable Reload differs from flight source')
    live_apply = bound_report(live['source_author'], 'CornerPlayableR5', live_schema, 'ue_m5_vs2_corner_playable_r5_author.py')
    require(live_apply.get('hero_safety') == safety and live_apply.get('source_corner_proof') == proof, 'R5 playable Apply proof differs')
    corner = bound_report(proof['author'], 'CornerVTwo')
    require(corner.get('art_revision') == 5 and corner.get('map') == proof['map']
            and corner.get('owner') == 'HarborCity_M5VS2_HarborCorner_Rev2', 'Source is not actual R5 corner')
    layout_path = scoped_file(proof['layout']['path'], DOCS)
    layout = read_json(layout_path, tracked)
    require(tracked[layout_path] == proof['layout']['sha256'].lower() and layout.get('art_revision') == 5
            and layout.get('playable_bounds_xy') == [-2500, -2500, 2500, 2500], 'Actual R5 layout bytes/bounds differ')
    modesty = bound_report(safety['reload'], 'HeroModestyReload', 'HarborCity.M5VS2.HeroModesty.v1')
    modesty_apply = bound_report(safety['apply'], 'HeroModestyApply', 'HarborCity.M5VS2.HeroModesty.v1')
    require(modesty.get('fresh_process_disk_readback') == 'PASS' and modesty.get('blueprint') == safety['blueprint']
            and modesty.get('source_blueprint') == safety['source_blueprint']
            and modesty['source_baseline'] == modesty_apply['source_baseline'] == safety['baseline']
            and modesty['native_readback']['component'] == safety['component'], 'Original Modesty baseline/Reload differs')
    selection_path = scoped_file(safety['selection']['path'], DOCS, 'CORNER_REV2_HERO_REVIEW_SELECTION.json')
    selection = read_json(selection_path, tracked)
    require(tracked[selection_path] == safety['selection']['sha256'].lower()
            and selection.get('blueprint') == safety['source_blueprint'], 'Original Hero selection proof differs')
    report['r5_author_chain'] = dict(status='HASH_BOUND_ACTUAL_PRIOR_EVIDENCE', reports=observed,
        art_revision=5, selected_hero=safety['blueprint'], source_hero=safety['source_blueprint'],
        current_asset_compatibility='NOT_ASSERTED_BY_ENCODER',
        author_script_policy='R5 executed author bytes must still match commandlet SHA and manifest; historic DLL compared only to recorded run identity.',
        safety_art_approval='NOT_ASSERTED')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True)
    parser.add_argument('--launch', required=True)
    parser.add_argument('--run-results', required=True)
    parser.add_argument('--allow-native-silent-mix', action='store_true', help='Allow original valid silent PCM only with native capture silent-rendering and restore proof; never creates audio or permits missing WAV')
    parser.add_argument('--diagnostic', action='store_true', help='Unique diagnostic draft only; preserves actual failed/aborted source status')
    parser.add_argument('--known-visual-failure', action='append', default=[])
    parser.add_argument('--validate-only', action='store_true', help='Validate/hash read-only inputs and write a new validation report; no FFmpeg/ffprobe or encoding')
    args = parser.parse_args(argv)
    encoder = load_encoder()
    capture = scoped_file(args.capture, RECORDINGS, 'capture.json')
    require(capture.parent.parent == RECORDINGS.resolve() and re.fullmatch(r'\d{8}_\d{6}_[0-9A-Fa-f]{32}', capture.parent.name), 'Use one exact native VS2 capture directory')
    evidence = DOCS / 'video_encoding' / (dt.datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '_' + uuid.uuid4().hex[:8])
    require(evidence.resolve().is_relative_to(DOCS.resolve()), 'Output evidence directory escapes VS2 docs')
    evidence.mkdir(parents=True, exist_ok=False)
    output = evidence / ('M5_VS2_R5_FLIGHT_DIAGNOSTIC.mp4' if args.diagnostic else 'M5_VS2_R5_FLIGHT_DEMO.mp4')
    report = dict(schema='HarborCity.M5VS2.FlightVideoEncodingR5.v1', milestone='M5_VS2', status='RUNNING', capture=str(capture), output=str(output),
        encoding='NOT_RUN', input_scope='ENGINE_INPUT_NOT_OS', os_input='NOT_RUN', packaged_runtime='NOT_RUN_EDITOR_GAME',
        visual_acceptance='USER_REVIEW', listening='NOT_RUN', normal_speed_viewing='NOT_RUN', mode='DIAGNOSTIC_DRAFT' if args.diagnostic else 'FORMAL_NATIVE_DEMO',
        runtime_acceptance='NOT_UPGRADED_BY_ENCODING', known_visual_failures=args.known_visual_failure, commands=[], reusable_encoder=dict(path=str(BASE), sha256=BASE_SHA256))
    tracked = {BASE: BASE_SHA256, Path(__file__).resolve(): sha(__file__)}
    report['encoder_self'] = dict(path=str(Path(__file__).resolve()), sha256=sha(__file__))

    def save():
        (evidence / 'verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    def run(command, label, timeout=600):
        report['commands'].append(dict(name=label, argv=command, timeout_seconds=timeout)); save()
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
        (evidence / (label + '.stdout')).write_bytes(result.stdout)
        (evidence / (label + '.stderr')).write_bytes(result.stderr)
        report['commands'][-1]['exit_code'] = result.returncode; save()
        require(result.returncode == 0, label + ' failed; partial output and original evidence retained')
        return result.stdout

    def unchanged():
        require(all(path.is_file() and sha(path) == digest for path, digest in tracked.items()), 'Input/encoder changed during validation or encoding')

    try:
        data = read_json(capture, tracked); rows = validate_native(data)
        validate_binding(args, capture, data, tracked, report)
        audio_path = scoped_file(capture.parent / 'game_audio.wav', RECORDINGS, 'game_audio.wav')
        require(audio_path.parent == capture.parent, 'Audio escaped the exact capture directory')
        audio = encoder.inspect_pcm_wav(audio_path); tracked[audio_path] = audio['sha256']
        report['native_audio_policy'] = validate_audio_policy(args.allow_native_silent_mix, data, audio)
        timeline = encoder.make_timeline(data, audio)
        if not args.diagnostic:
            require(30 <= timeline['duration_seconds'] <= 60, 'Actual observed video, not configured duration, must be 30..60 seconds')
        stamps = timeline['timestamps']; relative = timeline['relative_microseconds']
        report['source_audio'] = audio
        report['timeline'] = {k: v for k, v in timeline.items() if k not in ('timestamps', 'relative_microseconds')}
        report['timestamp_scope'] = data['timestamp_scope']
        frame_files = []; sequence = ['ffconcat version 1.0']; inventory = []
        for index, row in enumerate(rows):
            path = scoped_file(capture.parent / row['file'], RECORDINGS)
            require(path.parent == capture.parent, 'Frame escaped exact capture directory')
            encoder.validate_ppm(path, 1920, 1080)
            digest = sha(path); tracked[path] = digest
            inventory.append(dict(index=index, file=row['file'], seconds=row['seconds'], bytes=path.stat().st_size, sha256=digest))
            frame_files.append(path)
            duration_us = relative[index + 1] - relative[index] if index + 1 < len(rows) else 1000
            sequence.extend(["file '" + path.as_posix().replace("'", "'\\''") + "'", 'option framerate 1000', f'duration {duration_us / 1_000_000:.6f}'])
        actual_ppms = {p.resolve() for p in capture.parent.glob('*.ppm')}
        require(actual_ppms == set(frame_files), 'Unlisted/extra native PPM files; capture evidence not complete')
        (evidence / 'source_frame_manifest.json').write_text(json.dumps(inventory, ensure_ascii=False, indent=2), encoding='utf-8')
        report['source_evidence_hashes'] = [dict(path=str(path), sha256=digest) for path, digest in tracked.items() if path not in set(frame_files)]
        report['frames'] = dict(count=len(rows), width=1920, height=1080, requested_fps=30,
            actual_mean_capture_fps=(len(rows) - 1) / (stamps[-1] - stamps[0]), dropped=data['dropped'],
            max_observed_gap_seconds=max(b - a for a, b in zip(stamps, stamps[1:])),
            scope='All native frames exactly once at measured request-time PTS. Holds across real capture gaps remain visible; no interpolation, 30fps resampling, speed change or artificial terminal footage.')
        concat = evidence / 'realtime.ffconcat'; concat.write_text('\n'.join(sequence) + '\n', encoding='utf-8')
        ffmpeg = encoder.FFMPEG; ffprobe = ffmpeg.with_name('ffprobe.exe')
        require(ffmpeg.is_file() and ffprobe.is_file(), 'Existing project FFmpeg/ffprobe unavailable')
        report['tools'] = [dict(path=str(p), sha256=sha(p)) for p in (ffmpeg, ffprobe)]
        af = f"atrim=start_sample={timeline['audio_trim_start_sample']}:end_sample={timeline['audio_trim_end_sample']},asetpts=PTS-STARTPTS"
        if timeline['audio_delay_samples']:
            af += f",adelay={timeline['audio_delay_samples']}S:all=1"
        command = [str(ffmpeg), '-hide_banner', '-loglevel', 'warning', '-nostdin', '-n', '-f', 'concat', '-safe', '0', '-i', str(concat), '-i', str(audio_path),
            '-map', '0:v:0', '-map', '1:a:0', '-fps_mode:v', 'vfr', '-enc_time_base:v', '1:1000', '-c:v', 'libx264', '-preset', 'ultrafast', '-crf', '22', '-threads', '2', '-pix_fmt', 'yuv420p',
            '-af', af, '-c:a', 'aac', '-b:a', '192k', '-ar', str(audio['sample_rate']), '-t', f"{timeline['duration_seconds']:.6f}", '-movflags', '+faststart', str(output)]
        report['planned_encode_command'] = command
        unchanged()
        if args.validate_only:
            report['status'] = 'DIAGNOSTIC_INPUT_VALIDATED_ENCODING_NOT_RUN' if args.diagnostic else 'INPUT_VALIDATED_ENCODING_NOT_RUN'
            save(); print(evidence / 'verification.json'); return 0
        run(command, 'encode', 1200)
        metadata = json.loads(run([str(ffprobe), '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(output)], 'ffprobe', 120))
        report['ffprobe'] = metadata
        video = [x for x in metadata['streams'] if x['codec_type'] == 'video']; sounds = [x for x in metadata['streams'] if x['codec_type'] == 'audio']
        require(len(video) == 1 and video[0]['codec_name'] == 'h264' and video[0]['width'] == 1920 and video[0]['height'] == 1080, 'Output video codec/size changed')
        require(len(sounds) == 1 and sounds[0]['codec_name'] == 'aac' and int(sounds[0]['channels']) == audio['channels'] and int(sounds[0]['sample_rate']) == audio['sample_rate'], 'Output native audio stream missing/different')
        timing = json.loads(run([str(ffprobe), '-v', 'error', '-select_streams', 'v:0', '-count_frames', '-show_frames', '-show_entries', 'frame=pts_time,duration_time:stream=nb_read_frames', '-of', 'json', str(output)], 'frame_probe', 600))
        decoded = timing.get('frames', [])
        require(len(decoded) == len(rows) and int(timing['streams'][0]['nb_read_frames']) == len(rows), 'Encoded native frames duplicated or dropped')
        pts = [float(x['pts_time']) for x in decoded]
        require(all(math.isfinite(x) for x in pts) and all(b > a for a, b in zip(pts, pts[1:])), 'Invalid output PTS')
        errors = [(t - pts[0]) - (stamps[i] - stamps[0]) for i, t in enumerate(pts)]
        with (evidence / 'frame_timing.csv').open('w', newline='', encoding='utf-8') as stream:
            writer = csv.writer(stream); writer.writerow(['index', 'file', 'capture_seconds', 'encoded_pts', 'error_ms'])
            writer.writerows((i, rows[i]['file'], stamps[i], pts[i], errors[i] * 1000) for i in range(len(rows)))
        require(max(map(abs, errors)) <= .0015, 'Output timing changed the real capture speed')
        duration = float(metadata['format']['duration'])
        require(abs(duration - timeline['duration_seconds']) <= .10, 'Output duration was extended/truncated')
        decoded_wav = evidence / 'decoded_game_audio.wav'
        run([str(ffmpeg), '-hide_banner', '-loglevel', 'error', '-nostdin', '-n', '-i', str(output), '-map', '0:a:0', '-c:a', 'pcm_s16le', str(decoded_wav)], 'decoded_audio', 300)
        rendered = encoder.inspect_pcm_wav(decoded_wav); report['decoded_audio'] = rendered
        require(rendered['signal_present'] if report['native_audio_policy']['classification'] == 'NATIVE_SIGNAL_PRESENT' else not rendered['signal_present'], 'Encoded signal class differs from original native WAV')
        require(abs(rendered['duration_seconds'] - timeline['audio_expected_output_seconds']) <= .10 and abs(float(sounds[0].get('start_time', '0'))) <= .025, 'Encoded native audio timing differs from aligned WAV')
        unchanged()
        report.update(status='DIAGNOSTIC_ENCODED_SOURCE_STATUS_RETAINED' if args.diagnostic else 'ENCODED_PENDING_VISUAL_AND_AUDITORY_REVIEW',
            encoding='PASS_TECHNICAL_ONLY', audio_encoding='PASS_TECHNICAL_ORIGINAL_NATIVE_WAV', audio_content=report['native_audio_policy']['classification'], output_sha256=sha(output), output_bytes=output.stat().st_size,
            encoded_duration_seconds=duration, frame_pts_max_error_seconds=max(map(abs, errors)), frame_count_matches=True, normal_speed_pts_matches=True)
        save(); print(evidence / 'verification.json'); return 0
    except Exception as error:
        report.update(status='FAIL', error=repr(error), retention='Every original capture/frame/WAV/run record and any partial new output is retained. No automatic retry, silent fallback or source-status upgrade.')
        save(); print(evidence / 'verification.json', file=sys.stderr); raise


if __name__ == '__main__':
    raise SystemExit(main())
