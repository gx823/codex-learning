"""Private sole-recording fixture for a completed FootPlacement core candidate.

Explicit input: ABReload for Author, FixtureAuthor for Reload. No latest search.
Preserves prior route, camera, lighting, geometry and qualification logic.
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
BASE = DOC/'editor_runtime/author_20260925_101813_171_d15e10ec_GaitSoleCaptureAuthor/author_result.json'
ORIGINAL_ARMS = '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia_Arms'
OWNER = 'HarborCity_M5VS2_FootPlacementAB'
A = U.EditorAssetLibrary
LEVEL, ACT = U.get_editor_subsystem(U.LevelEditorSubsystem), U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    rows = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(rows) == 1, 'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT, PHASE = Path(argument('M5EvidenceDir')).resolve(), argument('M5AuthorPhase')
assert PHASE in ('FootPlacementFixtureAuthor', 'FootPlacementFixtureReload')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/HeroGaitR2/Run_'+TOKEN
HERO = '/Game/HarborCity/M5VS2/HeroRev2/Review_'+TOKEN+'/BP_HeroGait_Placement'
ARMS = '/Game/HarborCity/M5VS2/FootPlacementAB/Batch_'+TOKEN+'/SKM_PlacementArms'
R = dict(schema='HarborCity.M5VS2.FootPlacementFixture.Author.v1', phase=PHASE, status='RUNNING', checks=[], assets=[],
         expected_screenshots=0, runtime='NOT_RUN', visible_sliding='USER_REVIEW',
         input_level='NATIVE_FUNCTION_CALLS_NOT_ACTION_OR_OS_INPUT',
         scope='Private copy of actual sole fixture; unchanged route, camera, floor, lighting, 400/650 and original contact eligibility.',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def sha(path):
    with Path(path).open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()


def doc(path): return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, okay, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if okay else 'FAIL', observed=observed)); dump()
    if not okay: raise RuntimeError(name+': '+repr(observed))


def package(value):
    return (value.get_path_name() if hasattr(value, 'get_path_name') else str(value)).split('.')[0]


def disk(value, map_asset=False):
    value = package(value)
    assert value.startswith('/Game/HarborCity/') and '..' not in value
    return PROJECT/'Content'/Path(value.removeprefix('/Game/')).with_suffix('.umap' if map_asset else '.uasset')


def protect(path, expected=None):
    path = Path(path).resolve(); value = sha(path)
    check('source hash '+str(path), path.is_relative_to(WORK) and (expected is None or value == expected.lower()))
    PROTECTED[str(path)] = value


def completed(path, phase):
    path = Path(path).resolve()
    check('owned actual native report', path.is_relative_to(DOC/'editor_runtime') and path.name == 'author_result.json')
    protect(path); protect(path.parent/'commandlet.json')
    result, command = doc(path), doc(path.parent/'commandlet.json')
    check('completed '+phase, result.get('status') == 'PASS' and result.get('phase') == phase
          and command.get('status') == 'PASS' and command.get('phase') == phase and command.get('exit_code') == 0)
    return result


def protect_assets(report):
    for row in report['assets']: protect(disk(row['path'], row['path'] == report.get('map')), row['sha256'])


def load(value, kind=None):
    obj = A.load_asset(package(value)); check('load '+package(value), obj is not None and (kind is None or isinstance(obj, kind)))
    return obj


def value(item):
    if item is None or isinstance(item, (str, bool, int, float)): return item
    if isinstance(item, U.Vector): return [item.x, item.y, item.z]
    if isinstance(item, U.Rotator): return [item.pitch, item.yaw, item.roll]
    if hasattr(item, 'get_path_name'): return item.get_path_name()
    if isinstance(item, (list, tuple, U.Array)): return [value(x) for x in item]
    if isinstance(item, U.StructBase): return item.export_text()
    raise TypeError(type(item))


def props(obj, names): return {key: value(obj.get_editor_property(key)) for key in names}


def objects(bp):
    hero = U.get_default_object(bp.generated_class())
    rows = [hero, hero.get_editor_property('mesh'), hero.get_component_by_class(U.HCM4CombatComponent),
            hero.get_component_by_class(U.HCM4R2PresentationComponent)]
    check('native private Hero components', all(x is not None and package(x) == package(bp) for x in rows))
    return rows


def preserved(bp):
    hero, body, combat, presentation = objects(bp)
    return dict(hero=props(hero, ('walk_speed', 'sprint_speed', 'use_controller_rotation_yaw', 'base_eye_height')),
        movement=props(hero.get_component_by_class(U.CharacterMovementComponent), ('max_walk_speed', 'max_acceleration',
            'braking_deceleration_walking', 'gravity_scale', 'rotation_rate', 'orient_rotation_to_movement')),
        body=props(body, ('relative_location', 'relative_rotation', 'relative_scale3d', 'override_materials',
            'physics_asset_override', 'lighting_channels', 'enable_update_rate_optimizations')),
        combat=props(combat, ('aim_look_scale', 'ads_magnification', 'aim_transition_seconds', 'attack_movement_scale',
            'body_damage', 'head_damage', 'melee_damage', 'punch_animations', 'pistol_idle_animation', 'pistol_aim_animation')),
        presentation=props(presentation, ('eye_anchor_in_mesh', 'relaxed_hand_camera', 'relaxed_run_hand_camera',
            'relaxed_hand_swing_scale', 'relaxed_finger_pose')),
        flight=props(hero.get_component_by_class(U.HCM5VS2FlightComponent), ('enable_flight', 'cruise_speed', 'boost_speed',
            'vertical_speed', 'flight_acceleration', 'flight_braking')))


def shape(mesh):
    sub = U.get_editor_subsystem(U.SkeletalMeshEditorSubsystem)
    lods = sub.get_lod_count(mesh); bounds = mesh.get_bounds()
    check('bounded original arms LODs', 0 < lods < 12)
    return dict(lods=[dict(vertices=sub.get_num_verts(mesh, i), sections=sub.get_num_sections(mesh, i)) for i in range(lods)],
                materials=[dict(slot=str(x.get_editor_property('material_slot_name')), material=package(x.get_editor_property('material_interface')))
                           for x in mesh.get_editor_property('materials')],
                bounds=dict(origin=value(bounds.origin), extent=value(bounds.box_extent), sphere=bounds.sphere_radius),
                morphs=[x.get_name() for x in mesh.get_editor_property('morph_targets')])


def environment():
    rows = []
    for actor in ACT.get_all_level_actors():
        row = dict(label=actor.get_actor_label(), cls=actor.get_class().get_path_name(),
                   location=value(actor.get_actor_location()), rotation=value(actor.get_actor_rotation()), scale=value(actor.get_actor_scale3d()))
        mesh = actor.get_component_by_class(U.StaticMeshComponent)
        if mesh: row.update(mesh=package(mesh.get_editor_property('static_mesh')), materials=[package(m) for m in mesh.get_materials()],
                            collision=str(mesh.get_collision_profile_name()))
        if isinstance(actor, U.HCM5VS2HeroExerciseDirector):
            row['director_preserved'] = props(actor, ('phases', 'observed_bones', 'exercise_materials', 'sole_points', 'expected_gait_blend_space'))
            row['directional_light_label'] = actor.get_editor_property('directional_light').get_actor_label()
        light = actor.get_component_by_class(U.LightComponent)
        if light: row['light'] = props(light, ('intensity', 'light_color', 'cast_shadows'))
        rows.append(row)
    return sorted(rows, key=lambda item: (item['label'], item['cls']))


def sole_geometry(mesh):
    actual = json.loads(U.HCM5VS2SoleDiagnostics.inspect_shoe_geometry(mesh))
    check('actual native shoe geometry', actual.get('status') == 'PASS')
    actual.pop('mesh')
    return actual


def bindings(candidate, candidate_paths):
    match = re.fullmatch(r'/Game/HarborCity/M5VS2/HeroGaitR2/Run_([0-9a-f]{12})/L_HeroGaitR2', candidate_paths['map'])
    check('strict private fixture namespace', match is not None)
    token = match[1]
    check('exact four private package names', candidate_paths == dict(
        map='/Game/HarborCity/M5VS2/HeroGaitR2/Run_'+token+'/L_HeroGaitR2',
        game_mode='/Game/HarborCity/M5VS2/HeroGaitR2/Run_'+token+'/BP_HeroGaitR2GameMode',
        character='/Game/HarborCity/M5VS2/HeroRev2/Review_'+token+'/BP_HeroGait_Placement',
        arms='/Game/HarborCity/M5VS2/FootPlacementAB/Batch_'+token+'/SKM_PlacementArms'))
    hero, body, combat, presentation = objects(load(candidate_paths['character'], U.Blueprint))
    check('actual private Hero body and combat ABP', package(body.get_editor_property('skeletal_mesh_asset')) == candidate['mesh']
          and package(body.get_editor_property('anim_class')) == candidate['animation']
          and package(combat.get_editor_property('player_animation_blueprint')) == candidate['animation']
          and package(presentation.get_editor_property('first_person_arms_override')) == candidate_paths['arms'])
    arms = load(candidate_paths['arms'], U.SkeletalMesh)
    arm_native = json.loads(U.HCM5VS2FootPlacementEditor.bind_private_arms(load(ORIGINAL_ARMS, U.SkeletalMesh), arms,
                                                                            load(candidate['mesh'], U.SkeletalMesh), False))
    check('actual complete private arms skeleton', arm_native.get('status') == 'PASS', arm_native)
    check('arms shape preserved', shape(arms) == shape(load(ORIGINAL_ARMS, U.SkeletalMesh)))
    directors = [a for a in ACT.get_all_level_actors() if isinstance(a, U.HCM5VS2HeroExerciseDirector)]
    check('one private diagnostic director', len(directors) == 1)
    director = directors[0]
    expected = dict(expected_character_class=candidate_paths['character'], expected_animation_class=candidate['animation'],
                    expected_mesh=candidate['mesh'], expected_gait_blend_space=R['binding']['blendspace'])
    check('saved diagnostic references', all(package(director.get_editor_property(key)) == val for key, val in expected.items())
          and director.actor_has_tag(U.Name(OWNER)) and director.get_editor_property('sole_selection_evidence') == R['selection']['path'])
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    gm_class = world.get_world_settings().get_editor_property('default_game_mode')
    check('saved actual GameMode default pawn', package(gm_class) == candidate_paths['game_mode']
          and package(U.get_default_object(gm_class).get_editor_property('default_pawn_class')) == candidate_paths['character'])
    return arm_native


try:
    check('actual project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    descriptor_path = PROJECT/'HarborCity.uproject'
    protect(descriptor_path)
    descriptor_modules = [m for m in doc(descriptor_path)['Modules'] if m['Name'] == 'HarborCityEditor']
    check('custom graph module loads in uncooked game only', len(descriptor_modules) == 1
          and descriptor_modules[0]['Type'] == 'UncookedOnly' and descriptor_modules[0]['LoadingPhase'] == 'Default')
    R['project_descriptor'] = dict(path=str(descriptor_path), sha256=sha(descriptor_path),
                                   graph_module_type='UncookedOnly', graph_module_loading_phase='Default')

    check('no PIE or dirty packages', not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    check('compiled private fixture arms bridge', hasattr(U.HCM5VS2FootPlacementEditor, 'bind_private_arms'))
    given_path = Path(argument('M5FootPlacementFixtureInput')).resolve()
    prior = completed(given_path, 'FootPlacementFixtureAuthor') if PHASE == 'FootPlacementFixtureReload' else None
    core_reload_path = Path(prior['core_reload']['path']) if prior else given_path
    core_reload = completed(core_reload_path, 'FootPlacementABReload')
    core_author_path = Path(core_reload['author_report'])
    core = completed(core_author_path, 'FootPlacementABAuthor')
    check('core actual author/reload use this descriptor', core.get('project_descriptor') == R['project_descriptor']
          and core_reload.get('project_descriptor') == R['project_descriptor'])
    check('explicit completed core provenance', core_reload.get('fresh_process_disk_readback') == 'PASS'
          and core_reload['author_sha256'] == sha(core_author_path) and core_reload['assets'] == core['assets'])
    protect_assets(core)
    for path, digest in core['source_preservation']['sha256_before'].items():
        if Path(path).suffix.lower() in ('.uasset', '.umap'): protect(path, digest)
    base = completed(BASE, 'GaitSoleCaptureAuthor'); protect_assets(base)
    candidate = core['candidate']
    native = json.loads(U.HCM5VS2FootPlacementEditor.configure_candidate(load(base['binding']['animation'], U.AnimBlueprint),
        load(base['binding']['mesh'], U.SkeletalMesh), load(candidate['animation'], U.AnimBlueprint),
        load(candidate['skeleton'], U.Skeleton), load(candidate['mesh'], U.SkeletalMesh), False))
    check('current module native core readback exact', native == core['native'])
    R.update(core_reload=dict(path=str(core_reload_path), sha256=sha(core_reload_path)), core_candidate=candidate,
             source_binding=base['binding'], native_core=native, source_map=base['map'])
    R['modules'] = [dict(path=str(PROJECT/'Binaries/Win64'/name), sha256=sha(PROJECT/'Binaries/Win64'/name))
                    for name in ('UnrealEditor-HarborCity.dll', 'UnrealEditor-HarborCityEditor.dll')]
    source_hero = load(base['binding']['character'], U.Blueprint); original_arms = load(ORIGINAL_ARMS, U.SkeletalMesh)
    protect(disk(source_hero)); protect(disk(original_arms))
    source_arms = objects(source_hero)[3].get_editor_property('first_person_arms_override')
    check('source actual FP arms shape retains original', isinstance(source_arms, U.SkeletalMesh)
          and shape(source_arms) == shape(original_arms))
    protect(disk(source_arms))
    original_keep = preserved(source_hero)
    check('source actual 400/650', original_keep['hero']['walk_speed'] == 400 and original_keep['hero']['sprint_speed'] == 650)
    check('exact unchanged candidate shoe topology/weights', sole_geometry(load(candidate['mesh'], U.SkeletalMesh))
          == sole_geometry(load(base['binding']['mesh'], U.SkeletalMesh)))
    if prior:
        check('same descriptor as fixture author', prior.get('project_descriptor') == R['project_descriptor'])
        check('same frozen fixture author and compiled modules', sha(Path(__file__)) == doc(given_path.parent/'commandlet.json')['script_sha256'].lower()
              and R['modules'] == prior['modules'])
        protect_assets(prior); protect(prior['selection']['path'], prior['selection']['sha256'])
        for path, digest in prior['source_preservation']['sha256_before'].items(): protect(path, digest)
        paths = prior['paths']; R.update(paths=paths, binding=prior['binding'], selection=prior['selection'], map=prior['map'])
        check('unchanged exact saved binding', R['binding'] == dict(base['binding'], character=paths['character'],
                                                                  mesh=candidate['mesh'], animation=candidate['animation']))
        check('fresh private fixture load', LEVEL.load_level(prior['map']))
        check('exact retained fixture environment', environment() == prior['environment'])
        check('exact preserved Hero behavior', preserved(load(paths['character'])) == original_keep)
        arm_native = bindings(candidate, paths)
        check('saved arms bridge exact', arm_native == prior['native_arms'])
        R.update(assets=prior['assets'], environment=prior['environment'], native_arms=arm_native, author_report=str(given_path),
                 author_sha256=sha(given_path), fresh_process_disk_readback='PASS')
    else:
        paths = dict(character=HERO, arms=ARMS, game_mode=ROOT+'/BP_HeroGaitR2GameMode', map=ROOT+'/L_HeroGaitR2')
        R['paths'], R['map'] = paths, paths['map']
        R['binding'] = dict(base['binding'], character=HERO, mesh=candidate['mesh'], animation=candidate['animation'])
        check('exact source sole fixture load', LEVEL.load_level(base['map']))
        original_environment = environment()
        world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        source_gm = load(world.get_world_settings().get_editor_property('default_game_mode'), U.Blueprint)
        protect(disk(source_gm))
        check('source actual default pawn', package(U.get_default_object(source_gm.generated_class()).get_editor_property('default_pawn_class')) == base['binding']['character'])
        for path in paths.values(): check('unique private output '+path, not A.does_asset_exist(path) and not disk(path, path == paths['map']).exists())
        arms = A.duplicate_asset(ORIGINAL_ARMS, ARMS)
        check('fresh arms copy', isinstance(arms, U.SkeletalMesh))
        arm_native = json.loads(U.HCM5VS2FootPlacementEditor.bind_private_arms(original_arms, arms, load(candidate['mesh']), True))
        check('private arms native apply', arm_native.get('status') == 'PASS', arm_native)
        bp = A.duplicate_asset(package(source_hero), HERO)
        check('fresh private Hero copy', isinstance(bp, U.Blueprint))
        hero, body, combat, presentation = objects(bp)
        for obj in (hero, body, combat, presentation): obj.modify()
        body.set_skeletal_mesh_asset(load(candidate['mesh']))
        body.set_editor_property('anim_class', load(candidate['animation']).generated_class())
        combat.set_editor_property('player_animation_blueprint', load(candidate['animation']).generated_class())
        presentation.set_first_person_arms_override(arms)
        check('native private Hero compile', U.BlueprintEditorLibrary.compile_blueprint(bp))
        check('Hero gameplay and material overrides unchanged', preserved(bp) == original_keep)
        gm = A.duplicate_asset(package(source_gm), paths['game_mode'])
        check('fresh private GameMode', isinstance(gm, U.Blueprint))
        U.get_default_object(gm.generated_class()).set_editor_property('default_pawn_class', bp.generated_class())
        check('native private GameMode compile', U.BlueprintEditorLibrary.compile_blueprint(gm))
        selection_path = DOC/'research'/('GAS_FOOTPLACEMENT_SOLE_SELECTION_'+TOKEN+'.json')
        check('unique native point selection record', not selection_path.exists())
        protect(base['selection']['path'], base['selection']['sha256'])
        selection = doc(base['selection']['path'])
        selection.update(mesh=candidate['mesh'], mesh_sha256=sha(disk(candidate['mesh'])),
                         inherited_selection=dict(path=base['selection']['path'], sha256=base['selection']['sha256']),
                         actual_candidate_geometry='Entire native shoe candidate list/weights/reference bones exact except mesh path')
        selection_path.write_text(json.dumps(selection, ensure_ascii=False, indent=2), encoding='utf-8')
        R['selection'] = dict(path=str(selection_path), sha256=sha(selection_path), points=selection['points'])
        check('copy actual sole fixture with unchanged scene', LEVEL.new_level_from_template(paths['map'], base['map']))
        check('native copy preserves fixture transforms/materials/lights/phases/point list', environment() == original_environment)
        world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property('default_game_mode', gm.generated_class())
        directors = [a for a in ACT.get_all_level_actors() if isinstance(a, U.HCM5VS2HeroExerciseDirector)]
        check('one copied sole director', len(directors) == 1)
        director = directors[0]
        for key, item in dict(expected_character_class=bp.generated_class(), expected_animation_class=load(candidate['animation']).generated_class(),
                              expected_mesh=load(candidate['mesh']), sole_selection_evidence=str(selection_path)).items():
            director.set_editor_property(key, item)
        director.set_editor_property('tags', list(director.get_editor_property('tags'))+[U.Name(OWNER)])
        R['environment'] = environment()
        check('all measured scene/phases/points unchanged after rebinding', R['environment'] == original_environment)
        R['native_arms'] = bindings(candidate, paths)
        for obj in (arms, bp, gm):
            A.set_metadata_tag(obj, 'HarborCityOwnedBy', OWNER)
            check('native save '+package(obj), A.save_loaded_asset(obj, False))
            R['assets'].append(dict(path=package(obj), sha256=sha(disk(obj))))
        A.set_metadata_tag(world, 'HarborCityOwnedBy', OWNER)
        check('native save private fixture', LEVEL.save_current_level())
        R['assets'].append(dict(path=paths['map'], sha256=sha(disk(paths['map'], True))))
    check('exact four private fixture packages', len(R['assets']) == 4 and {row['path'] for row in R['assets']} == set(R['paths'].values()))
    R['status'] = 'PASS'
except Exception as exc:
    R.update(status='FAIL', error=repr(exc), traceback=traceback.format_exc()); U.log_error(R['traceback'])
finally:
    changed = [p for p, h in PROTECTED.items() if not Path(p).is_file() or sha(p) != h]
    R['source_preservation'] = dict(sha256_before=PROTECTED, changed=changed)
    if changed: R.update(status='FAIL', error='Protected source changed')
    R['finished_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS': raise RuntimeError('Private FootPlacement fixture failed; preserve failed evidence/assets')
