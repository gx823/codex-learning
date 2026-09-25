"""Private five-asset clock A/B; all existing sequence bytes stay untouched.

Root runs this with the native commandlet wrapper, then a fresh-process Reload.
No selected-Hero update, fabricated markers, runtime launch or relaxed gait test.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT, DOC = WORK/'HarborCity', WORK/'docs/HarborCity_M5_VS2'
BASE_AUTHOR = DOC/'editor_runtime/author_20260925_073727_946_eb2086ca_GASNominalWeightAuthor/author_result.json'
BASE_RELOAD = DOC/'editor_runtime/author_20260925_073753_126_8152f213_GASNominalWeightReload/author_result.json'
MARKER_PROBE = DOC/'editor_runtime/author_20260925_080756_982_9ea67845_GASGaitMarkerProbe/author_result.json'
ORIGINAL_SPACE = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun'
OWNER = 'HarborCity_M5VS2_HeroGaitR2'
A = U.EditorAssetLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    rows = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(rows) == 1, 'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
PHASE = argument('M5AuthorPhase')
assert PHASE in ('GASUnifiedClockAuthor', 'GASUnifiedClockReload')
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/HeroGaitR2/Run_'+TOKEN
MAP = ROOT+'/L_HeroGaitR2'
DEST = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_'+TOKEN
HERO_DEST = '/Game/HarborCity/M5VS2/HeroRev2/Review_'+TOKEN
R = dict(schema='HarborCity.M5VS2.GASUnifiedClock.Author.v1', phase=PHASE, status='RUNNING',
         map=MAP, owner=OWNER, checks=[], assets=[], inputs={}, runtime='NOT_RUN',
         visual_acceptance='USER_REVIEW', started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         scope='Single private BlendSpace marker-sync flag; exact prior normalized gait fixture and animation samples.')
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, okay, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if okay else 'FAIL', observed=observed))
    dump()
    if not okay:
        raise RuntimeError(name+': '+repr(observed))


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def document(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def package(value):
    return (value.get_path_name() if hasattr(value, 'get_path_name') else str(value)).split('.')[0]


def disk(value, extension='.uasset'):
    path = package(value)
    assert path.startswith('/Game/HarborCity/') and '..' not in path and extension in ('.uasset', '.umap')
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix(extension)


def protect(path, expected=None):
    path = Path(path).resolve()
    digest = sha(path)
    check('protected source '+str(path), expected is None or digest == expected.lower())
    PROTECTED[str(path)] = digest


def completed_report(path, phase):
    path = Path(path).resolve()
    check('owned report', path.is_relative_to(DOC/'editor_runtime'))
    protect(path)
    command = path.parent/'commandlet.json'
    protect(command)
    data, launch = document(path), document(command)
    check('completed native '+phase, data.get('status') == 'PASS' and data.get('phase') == phase
          and launch.get('phase') == phase and launch.get('status') == 'PASS' and launch.get('exit_code') == 0)
    R['inputs'][phase] = dict(path=str(path), sha256=sha(path), commandlet=str(command), commandlet_sha256=sha(command))
    return data


def protect_assets(data):
    map_path = package(data['map'])
    check('one exact private gait map', re.fullmatch(r'/Game/HarborCity/M5VS2/HeroGaitR2/Run_[0-9a-f]{12}/L_HeroGaitR2', map_path) is not None
          and sum(package(row['path']) == map_path for row in data['assets']) == 1)
    for row in data['assets']:
        protect(disk(row['path'], '.umap' if package(row['path']) == map_path else '.uasset'), row['sha256'])


def load(path, kind=None):
    obj = A.load_asset(package(path))
    check('native load '+package(path), obj is not None and (kind is None or isinstance(obj, kind)))
    return obj


def generated(path):
    path = package(path)
    cls = U.load_class(None, path+'.'+path.rsplit('/', 1)[1]+'_C')
    check('compiled class '+path, cls is not None)
    return cls


def transform_values(value):
    return [value.translation.x, value.translation.y, value.translation.z, value.rotation.x,
            value.rotation.y, value.rotation.z, value.rotation.w, value.scale3d.x, value.scale3d.y, value.scale3d.z]


def save(obj):
    path = package(obj)
    check('private output only', path in (DEST+'/BS_Selestia_GASMotion', DEST+'/ABP_Selestia_GASMotion_UniformClock',
          HERO_DEST+'/BP_HeroGait_UniformClock', ROOT+'/BP_HeroGaitR2GameMode'))
    A.set_metadata_tag(obj, 'HarborCityOwnedBy', OWNER)
    check('native save '+path, A.save_loaded_asset(obj, False))
    R['assets'].append(dict(path=path, sha256=sha(disk(path))))


def clock_read(clock, apply=False):
    return json.loads(U.HCM5VS2GASClockEditor.configure_unified_clock(
        load(clock['source_blueprint'], U.AnimBlueprint), load(clock['source_space'], U.BlendSpace),
        load(clock['candidate_blueprint'], U.AnimBlueprint), load(clock['candidate_space'], U.BlendSpace), apply))


try:
    check('actual project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE or dirty map', not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    if PHASE == 'GASUnifiedClockReload':
        completed = []
        for file in sorted((DOC/'editor_runtime').glob('author_*_GASUnifiedClockAuthor/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
            candidate, command = document(file), file.parent/'commandlet.json'
            if candidate.get('status') == 'PASS' and command.is_file() and document(command).get('status') == 'PASS' and document(command).get('exit_code') == 0:
                completed.append((file, candidate))
                break
        check('one latest completed author', len(completed) == 1)
        prior_file, prior = completed[0]
        prior = completed_report(prior_file, 'GASUnifiedClockAuthor')
        check('same frozen author script', document(prior_file.parent/'commandlet.json')['script_sha256'].lower() == sha(WORK/'tools/ue_m5_vs2_gas_clock_author.py'))
        check('exact five assets', len(prior['assets']) == 5)
        protect_assets(prior)
        for path, digest in prior['source_preservation']['sha256_before'].items():
            protect(path, digest)
        check('same module as author', sha(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll') == prior['module_sha256'])
        clock = prior['clock']
        readback = clock_read(clock)
        check('fresh native single-flag clock readback', readback == clock['native'], readback)
        bp, mesh = load(clock['candidate_blueprint'], U.AnimBlueprint), load(prior['binding']['mesh'], U.SkeletalMesh)
        space = load(clock['candidate_space'], U.BlendSpace)
        gas = json.loads(U.HCM5VS2GASMotionEditor.inspect_motion_candidate(bp, mesh, load(ORIGINAL_SPACE, U.BlendSpace), space))
        flight = json.loads(U.HCM5VS2FlightPoseEditor.inspect_flight_graph(bp))
        check('fresh retained GAS and flight graph', gas.get('status') == 'PASS' and gas['samples'] == prior['native_retained_gas_graph']['samples']
              and flight == prior['native_retained_flight_graph'])
        check('native private map reload', LEVEL.load_level(prior['map']))
        directors = [a for a in ACT.get_all_level_actors() if isinstance(a, U.HCM5VS2HeroExerciseDirector)]
        check('fresh exact fixture binding', len(directors) == 1
              and directors[0].get_editor_property('expected_character_class') == generated(clock['candidate_hero'])
              and directors[0].get_editor_property('expected_animation_class') == generated(clock['candidate_blueprint'])
              and directors[0].get_editor_property('expected_gait_blend_space') == space
              and directors[0].get_editor_property('expected_mesh') == mesh)
        body = U.get_default_object(generated(clock['candidate_hero'])).get_editor_property('mesh')
        check('fresh Hero animation binding', body.get_editor_property('anim_class') == generated(clock['candidate_blueprint'])
              and body.get_editor_property('skeletal_mesh_asset') == mesh)
        R.update(map=prior['map'], assets=prior['assets'], clock=clock, binding=prior['binding'], source=prior['source'],
                 author_report=str(prior_file), author_sha256=sha(prior_file), fresh_process_disk_readback='PASS', status='PASS')
    else:
        base = completed_report(BASE_AUTHOR, 'GASNominalWeightAuthor')
        base_reload = completed_report(BASE_RELOAD, 'GASNominalWeightReload')
        probe = completed_report(MARKER_PROBE, 'GASGaitMarkerProbe')
        check('exact validated normalized source', base_reload.get('fresh_process_disk_readback') == 'PASS'
              and Path(base_reload['author_report']).resolve() == BASE_AUTHOR.resolve()
              and base_reload['author_sha256'].lower() == sha(BASE_AUTHOR) and len(base['assets']) == 8)
        check('actual marker probe used this source', Path(probe['inputs']['normalized_author']['path']).resolve() == BASE_AUTHOR.resolve()
              and probe['inputs']['normalized_author']['sha256'] == sha(BASE_AUTHOR)
              and probe['asset_writes'] == 0 and not probe['source_preservation']['changed_files'])
        protect_assets(base)
        for row in probe['sequences'] + probe['gas_source_copies']:
            protect(disk(row['path']), row['sha256'])
        protect(Path(base['source']['selection']), base['source']['selection_sha256'])
        protect(Path(base['source']['report']), base['source']['report_sha256'])
        for path in [ORIGINAL_SPACE, base['binding']['mesh'], *base['source']['expected']['materials']]:
            protect(disk(path))
        for file in (PROJECT/'Config').glob('*.ini'):
            protect(file)
        for name in ('HCM5VS2HeroExerciseDirector.h', 'HCM5VS2HeroExerciseDirector.cpp', 'HCM5VS2LookAnimInstance.h',
                     'HCM5VS2LookAnimInstance.cpp', 'HCM5VS2GASClockEditor.h', 'HCM5VS2GASClockEditor.cpp'):
            protect(PROJECT/'Source/HarborCity/M5VS2'/name)
        module = PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll'
        protect(module)
        source_bp = load(base['binding']['animation'], U.AnimBlueprint)
        source_space = load(base['binding']['blendspace'], U.BlendSpace)
        mesh = load(base['binding']['mesh'], U.SkeletalMesh)
        gas = json.loads(U.HCM5VS2GASMotionEditor.inspect_motion_candidate(source_bp, mesh, load(ORIGINAL_SPACE, U.BlendSpace), source_space))
        flight = json.loads(U.HCM5VS2FlightPoseEditor.inspect_flight_graph(source_bp))
        check('exact native 28-sample baseline and flight graph', gas.get('status') == 'PASS'
              and gas['samples'] == probe['native_blendspace']['samples'] and flight.get('status') == 'PASS')
        R['native_retained_gas_graph'], R['native_retained_flight_graph'] = gas, flight
        check('actual mixed marker tables', probe['samples'][0]['marker_count'] > 0
              and probe['samples'][9]['marker_count'] == 0 and probe['samples'][27]['marker_count'] > 0)
        for directory in (ROOT, DEST, HERO_DEST):
            check('unique private namespace '+directory, not A.does_directory_exist(directory)
                  and not disk(directory+'/unused').parent.exists())
        new_space = A.duplicate_asset(package(source_space), DEST+'/BS_Selestia_GASMotion')
        new_bp = A.duplicate_asset(package(source_bp), DEST+'/ABP_Selestia_GASMotion_UniformClock')
        check('private native animation copies', isinstance(new_space, U.BlendSpace) and isinstance(new_bp, U.AnimBlueprint))
        clock = dict(source_blueprint=package(source_bp), source_space=package(source_space), source_hero=base['binding']['character'],
                     candidate_blueprint=package(new_bp), candidate_space=package(new_space),
                     candidate_hero=HERO_DEST+'/BP_HeroGait_UniformClock')
        native = clock_read(clock, True)
        check('native clock single-flag apply', native.get('status') == 'PASS', native)
        clock.update(native=native, changed_only='Private BlendSpace bAllowMarkerBasedSync=false and derived cache; private references.',
                     sequence_copies_or_edits=0, original_markers_contact_curves_and_rates_preserved=True,
                     original_selection_unchanged=True, runtime_sliding_improvement='NOT_RUN')
        R['clock'] = clock
        candidate = A.duplicate_asset(clock['source_hero'], clock['candidate_hero'])
        check('private Hero copy', isinstance(candidate, U.Blueprint))
        source_cdo = U.get_default_object(generated(clock['source_hero']))
        source_body = source_cdo.get_editor_property('mesh')
        materials = [load(path) for path in base['source']['expected']['materials']]
        candidate_body = U.get_default_object(candidate.generated_class()).get_editor_property('mesh')
        candidate_body.set_editor_property('anim_class', new_bp.generated_class())
        check('private Hero compile', U.BlueprintEditorLibrary.compile_blueprint(candidate))
        hero_class, anim_class = candidate.generated_class(), new_bp.generated_class()
        candidate_cdo = U.get_default_object(hero_class)
        candidate_body = candidate_cdo.get_editor_property('mesh')
        for label, cdo, body, expected_anim in (('source', source_cdo, source_body, source_bp.generated_class()),
                                                ('candidate', candidate_cdo, candidate_body, anim_class)):
            check(label+' preserves gameplay and mesh binding', body.get_editor_property('skeletal_mesh_asset') == mesh
                  and body.get_editor_property('anim_class') == expected_anim
                  and [body.get_material(i) for i in range(body.get_num_materials())] == materials
                  and cdo.get_editor_property('walk_speed') == 400 and cdo.get_editor_property('sprint_speed') == 650)
        check('body transform unchanged', transform_values(candidate_body.get_relative_transform()) == transform_values(source_body.get_relative_transform()))
        new_gas = json.loads(U.HCM5VS2GASMotionEditor.inspect_motion_candidate(new_bp, mesh, load(ORIGINAL_SPACE, U.BlendSpace), new_space))
        new_flight = json.loads(U.HCM5VS2FlightPoseEditor.inspect_flight_graph(new_bp))
        check('retained final graph and exact samples', new_gas.get('status') == 'PASS' and new_gas['samples'] == gas['samples'] and new_flight == flight)
        for obj in (new_space, new_bp, candidate):
            save(obj)
        source_map = base['map']
        source_gm = source_map.rsplit('/', 1)[0]+'/BP_HeroGaitR2GameMode'
        check('source mode present in exact manifest', sum(row['path'] == source_gm for row in base['assets']) == 1)
        gm = A.duplicate_asset(source_gm, ROOT+'/BP_HeroGaitR2GameMode')
        check('private game mode copy', isinstance(gm, U.Blueprint))
        U.get_default_object(gm.generated_class()).set_editor_property('default_pawn_class', hero_class)
        check('private game mode compile', U.BlueprintEditorLibrary.compile_blueprint(gm))
        save(gm)
        check('copy actual normalized fixture unchanged', LEVEL.new_level_from_template(MAP, source_map))
        world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        check('exact private fixture', package(world) == MAP)
        A.set_metadata_tag(world, 'HarborCityOwnedBy', OWNER)
        world.get_world_settings().set_editor_property('default_game_mode', gm.generated_class())
        directors = [actor for actor in ACT.get_all_level_actors() if isinstance(actor, U.HCM5VS2HeroExerciseDirector)]
        check('one actual source gait director', len(directors) == 1)
        director = directors[0]
        check('retained original director bindings', director.get_editor_property('expected_character_class') == generated(clock['source_hero'])
              and director.get_editor_property('expected_animation_class') == source_bp.generated_class()
              and director.get_editor_property('expected_mesh') == mesh
              and director.get_editor_property('expected_gait_blend_space') == source_space)
        # No movement phases, contacts, speed thresholds, observed bones, lights,
        # geometry or camera values are changed. Only these three references.
        bindings = dict(expected_character_class=hero_class, expected_animation_class=anim_class, expected_gait_blend_space=new_space)
        for name, value in bindings.items():
            director.set_editor_property(name, value)
        check('native map save', LEVEL.save_current_level())
        R['assets'].append(dict(path=MAP, sha256=sha(disk(MAP, '.umap'))))
        check('native private map reload', LEVEL.load_level(MAP))
        directors = [actor for actor in ACT.get_all_level_actors() if isinstance(actor, U.HCM5VS2HeroExerciseDirector)]
        check('persisted exact three new references', len(directors) == 1 and all(directors[0].get_editor_property(k) == v for k, v in bindings.items()))
        check('exact five private packages', len(R['assets']) == 5)
        R.update(source=base['source'], source_map=source_map,
                 binding=dict(character=clock['candidate_hero'], animation=clock['candidate_blueprint'], mesh=base['binding']['mesh'],
                              blendspace=clock['candidate_space'], loop_targets=base['binding']['loop_targets']),
                 module_sha256=sha(module), runtime_source_sha256={str(p): sha(p) for p in
                     (PROJECT/'Source/HarborCity/M5VS2/HCM5VS2HeroExerciseDirector.h', PROJECT/'Source/HarborCity/M5VS2/HCM5VS2HeroExerciseDirector.cpp')},
                 expected_screenshots=9, runtime_flag='-M5VS2HeroGaitR2',
                 measurement_scope=base['measurement_scope'], status='PASS')
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    changed = [path for path, digest in PROTECTED.items() if not Path(path).is_file() or sha(path) != digest]
    R['source_preservation'] = dict(files=len(PROTECTED), changed=changed, sha256_before=PROTECTED)
    if changed:
        R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('Unified clock author failed; preserve source and failed candidate evidence')
