"""Private Clock shoe-view fixture. Root only; never executes gameplay.

Requires an actual native geometry Probe and independently reviewed fixed-point
selection. No placeholder vertices, new animation, character or movement edits.
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
CLOCK_AUTHOR = DOC/'editor_runtime/author_20260925_084107_824_49de361c_GASUnifiedClockAuthor/author_result.json'
CLOCK_RELOAD = DOC/'editor_runtime/author_20260925_084154_358_cc64de19_GASUnifiedClockReload/author_result.json'
OWNER, SOLE_OWNER = 'HarborCity_M5VS2_HeroGaitR2', 'HarborCity_M5VS2_GaitSoleCapture'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
LEVEL, ACT = U.get_editor_subsystem(U.LevelEditorSubsystem), U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    values = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(values) == 1, 'Exactly one '+name+' required'
    return values[0][0] or values[0][1]


OUT, PHASE = Path(argument('M5EvidenceDir')).resolve(), argument('M5AuthorPhase')
assert PHASE in ('GaitSoleCaptureAuthor', 'GaitSoleCaptureReload')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/HeroGaitR2/Run_'+TOKEN
MAP, MATERIAL = ROOT+'/L_HeroGaitR2', ROOT+'/M_SoleGroundReference'
R = dict(schema='HarborCity.M5VS2.GaitSoleCapture.Author.v1', phase=PHASE, status='RUNNING',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), checks=[], assets=[], inputs={},
         map=MAP, runtime='NOT_RUN', visual_acceptance='USER_REVIEW', expected_screenshots=0,
         expected_native_video_game_seconds=20.9,
         scope='Independent native shoe-view recording fixture; original 400/650 route/qualification retained. '
               'Copied noncolliding backdrops removed and private ground reference material added; not the original performance/baseline scene.')
PROTECTED = {}


def sha(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            value.update(block)
    return value.hexdigest()


def document(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, okay, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if okay else 'FAIL', observed=observed))
    dump()
    if not okay:
        raise RuntimeError(name+': '+repr(observed))


def protect(path, expected=None):
    path = Path(path).resolve()
    check('owned existing input '+str(path), path.is_relative_to(WORK) and path.is_file())
    value = sha(path)
    check('unchanged input '+str(path), expected is None or value == expected.lower())
    PROTECTED[str(path)] = value


def package(value):
    return (value.get_path_name() if hasattr(value, 'get_path_name') else str(value)).split('.')[0]


def asset_file(value, is_map=False):
    value = package(value)
    assert value.startswith('/Game/HarborCity/M5VS2/') and '..' not in value
    return PROJECT/'Content'/Path(value.removeprefix('/Game/')).with_suffix('.umap' if is_map else '.uasset')


def completed(path, phase):
    path = Path(path).resolve()
    check('owned native report', path.is_relative_to(DOC/'editor_runtime'))
    protect(path)
    command = path.parent/'commandlet.json'
    protect(command)
    data, launch = document(path), document(command)
    check('completed '+phase, data.get('phase') == phase and data.get('status') == 'PASS'
          and launch.get('phase') == phase and launch.get('status') == 'PASS' and launch.get('exit_code') == 0)
    R['inputs'][phase] = dict(path=str(path), sha256=sha(path), commandlet=str(command), commandlet_sha256=sha(command))
    return data


def xyz(value):
    return [value.x, value.y, value.z]


def actor_snapshot(actor):
    component = actor.get_component_by_class(U.StaticMeshComponent)
    result = dict(label=actor.get_actor_label(), class_path=actor.get_class().get_path_name(),
                  location=xyz(actor.get_actor_location()), scale=xyz(actor.get_actor_scale3d()))
    if component:
        result.update(mesh=package(component.get_editor_property('static_mesh')),
                      collision_profile=str(component.get_collision_profile_name()),
                      materials=[package(item) for item in component.get_materials()])
    return result


def point_values(point):
    return dict(side=str(point.get_editor_property('side')), region=str(point.get_editor_property('region')),
                vertex_index=point.get_editor_property('vertex_index'), section_index=point.get_editor_property('section_index'),
                reference_position=xyz(point.get_editor_property('reference_position')))


def material():
    obj = U.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL.rsplit('/', 1)[1], ROOT, U.Material, U.MaterialFactoryNew())
    check('new private ground-reference material', obj is not None)
    world = E.create_material_expression(obj, U.MaterialExpressionWorldPosition, -550, 0)
    custom = E.create_material_expression(obj, U.MaterialExpressionCustom, -250, 0)
    custom.set_editor_property('output_type', U.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'Native world-space 100cm reference cells, no WPO or UV motion')
    pin = U.CustomInput()
    pin.set_editor_property('input_name', 'WorldPosition')
    custom.set_editor_property('inputs', [pin])
    custom.set_editor_property('code', 'float2 cell=floor(WorldPosition.xy/100.0); float parity=abs(fmod(cell.x+cell.y,2.0)); '
                               'float2 phase=frac(WorldPosition.xy/100.0); float edge=max(step(0.99,phase.x),step(0.99,phase.y)); '
                               'return lerp(lerp(float3(0.18,0.18,0.18),float3(0.24,0.24,0.24),parity),float3(0.07,0.07,0.07),edge);')
    rough = E.create_material_expression(obj, U.MaterialExpressionConstant, -250, 200)
    rough.set_editor_property('r', .8)
    check('world-position to reference grid', E.connect_material_expressions(world, '', custom, 'WorldPosition'))
    check('reference grid base color', E.connect_material_property(custom, '', U.MaterialProperty.MP_BASE_COLOR))
    check('reference ground roughness', E.connect_material_property(rough, '', U.MaterialProperty.MP_ROUGHNESS))
    E.recompile_material(obj)
    A.set_metadata_tag(obj, 'HarborCityOwnedBy', SOLE_OWNER)
    check('native private material save', A.save_loaded_asset(obj, False))
    R['assets'].append(dict(path=MATERIAL, sha256=sha(asset_file(MATERIAL))))
    return obj


def selected_points(selection):
    check('exact twelve authored points', len(selection.get('points', [])) == 12)
    points = []
    for value in selection['points']:
        row = U.HCM5VS2SolePoint()
        for key in ('side', 'region'):
            row.set_editor_property(key, U.Name(value[key]))
        for key in ('vertex_index', 'section_index'):
            row.set_editor_property(key, value[key])
        row.set_editor_property('reference_position', U.Vector(*value['reference_position']))
        points.append(row)
    return points


try:
    check('actual project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE or dirty packages', not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    director_defaults = U.get_default_object(U.HCM5VS2HeroExerciseDirector.static_class())
    check('compiled opt-in sole director properties before any writes',
          len(director_defaults.get_editor_property('sole_points')) == 0
          and director_defaults.get_editor_property('sole_selection_evidence') == '')
    author, reload = completed(CLOCK_AUTHOR, 'GASUnifiedClockAuthor'), completed(CLOCK_RELOAD, 'GASUnifiedClockReload')
    check('exact completed Clock provenance', len(author['assets']) == 5 and reload['author_sha256'] == sha(CLOCK_AUTHOR)
          and reload.get('fresh_process_disk_readback') == 'PASS' and reload['assets'] == author['assets'])
    for row in author['assets']:
        protect(asset_file(row['path'], package(row['path']) == package(author['map'])), row['sha256'])
    for path, digest in author['source_preservation']['sha256_before'].items():
        if Path(path).suffix.lower() in ('.uasset', '.umap'):
            protect(path, digest)
    if PHASE == 'GaitSoleCaptureReload':
        previous = []
        for path in sorted((DOC/'editor_runtime').glob('author_*_GaitSoleCaptureAuthor/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
            command = path.parent/'commandlet.json'
            if document(path).get('status') == 'PASS' and command.is_file() and document(command).get('status') == 'PASS' and document(command).get('exit_code') == 0:
                previous.append(path)
                break
        check('one completed sole author', len(previous) == 1)
        prior_path = previous[0]
        prior = completed(prior_path, 'GaitSoleCaptureAuthor')
        check('exact two private recording packages', re.fullmatch(r'/Game/HarborCity/M5VS2/HeroGaitR2/Run_[0-9a-f]{12}/L_HeroGaitR2', prior['map']) is not None
              and sorted(row['path'] for row in prior['assets']) == sorted([prior['map'], prior['map'].rsplit('/', 1)[0]+'/M_SoleGroundReference']))
        check('same module and author script', sha(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll') == prior['module_sha256']
              and sha(Path(__file__)) == document(prior_path.parent/'commandlet.json')['script_sha256'].lower())
        for row in prior['assets']:
            protect(asset_file(row['path'], row['path'] == prior['map']), row['sha256'])
        selection_path = Path(prior['selection']['path'])
        protect(selection_path, prior['selection']['sha256'])
    else:
        selections = sorted((DOC/'research').glob('GAS_CLOCK_SOLE_SELECTION_????????.json'), key=lambda p: p.stat().st_mtime, reverse=True)
        check('actual fixed-point selection exists before any asset write', len(selections) > 0)
        selection_path = selections[0]
        protect(selection_path)
    selection = document(selection_path)
    check('reviewed native point selection schema', selection.get('schema') == 'HarborCity.M5VS2.SoleSelection.v1'
          and selection.get('status') == 'READY_FOR_NATIVE_SAMPLING'
          and selection.get('mesh') == package(author['binding']['mesh']))
    probe_path = Path(selection['probe_report'])
    probe = completed(probe_path, 'GaitSoleGeometryProbe')
    check('selection tied to actual native probe', sha(probe_path) == selection['probe_sha256']
          and probe['binding']['mesh_sha256'] == selection['mesh_sha256'])
    geometry_path = Path(probe['native_geometry']['path'])
    protect(geometry_path, probe['native_geometry']['sha256'])
    candidates = {row['render_vertex_index']: row for row in document(geometry_path)['candidates']}
    points = selected_points(selection)
    for point in selection['points']:
        candidate = candidates.get(point['vertex_index'])
        check('fixed point from native geometry '+str(point['vertex_index']), candidate is not None
              and point['side'] == candidate['side'] and point['section_index'] == candidate['section_index']
              and point['reference_position'] == candidate['reference_component_cm'])
    mesh = A.load_asset(selection['mesh'])
    protect(asset_file(selection['mesh']), selection['mesh_sha256'])
    check('fresh selected-mesh native read', json.loads(U.HCM5VS2SoleDiagnostics.inspect_shoe_geometry(mesh)) == document(geometry_path))
    R['selection'] = dict(path=str(selection_path), sha256=sha(selection_path), points=selection['points'], probe=str(probe_path))
    R['binding'] = author['binding']
    R['clock'] = author['clock']
    R['module_sha256'] = sha(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll')
    if PHASE == 'GaitSoleCaptureReload':
        check('fresh native private level', LEVEL.load_level(prior['map']))
        directors = [actor for actor in ACT.get_all_level_actors() if isinstance(actor, U.HCM5VS2HeroExerciseDirector)]
        check('one native director and fixed point roundtrip', len(directors) == 1
              and directors[0].actor_has_tag(U.Name(SOLE_OWNER))
              and package(directors[0].get_editor_property('expected_mesh')) == selection['mesh']
              and package(directors[0].get_editor_property('expected_character_class')) == author['clock']['candidate_hero']
              and package(directors[0].get_editor_property('expected_animation_class')) == author['clock']['candidate_blueprint']
              and package(directors[0].get_editor_property('expected_gait_blend_space')) == author['clock']['candidate_space']
              and [point_values(point) for point in directors[0].get_editor_property('sole_points')] == selection['points']
              and directors[0].get_editor_property('sole_selection_evidence') == str(selection_path))
        actors = {a.get_actor_label(): a for a in ACT.get_all_level_actors()}
        check('fresh copied wall removal', not any('HC_M5VS2_Lookdev_Backdrop'+direction in actors for direction in ('West', 'East', 'North', 'South')))
        floor = actor_snapshot(actors['HC_M5VS2_Lookdev_Floor'])
        check('exact saved flat ground and private reference material', floor == prior['recording_fixture']['floor_after'])
        R.update(map=prior['map'], assets=prior['assets'], recording_fixture=prior['recording_fixture'],
                 author_report=str(prior_path), author_sha256=sha(prior_path), fresh_process_disk_readback='PASS')
    else:
        check('new private namespace', not A.does_directory_exist(ROOT) and not asset_file(MAP, True).parent.exists())
        check('native private copy of frozen Clock map', LEVEL.new_level_from_template(MAP, author['map']))
        world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        check('exact private world', package(world) == MAP)
        actors = ACT.get_all_level_actors()
        check('no auto-spawning Experience or recorder', not any(isinstance(a, (U.HCM3Experience, U.HCM3Recording)) for a in actors))
        directors = [a for a in actors if isinstance(a, U.HCM5VS2HeroExerciseDirector)]
        check('one copied Clock director', len(directors) == 1)
        director = directors[0]
        check('copied Clock bindings', package(director.get_editor_property('expected_mesh')) == selection['mesh']
              and package(director.get_editor_property('expected_character_class')) == author['clock']['candidate_hero']
              and package(director.get_editor_property('expected_animation_class')) == author['clock']['candidate_blueprint']
              and package(director.get_editor_property('expected_gait_blend_space')) == author['clock']['candidate_space'])
        removed = []
        backgrounds = [(direction, [a for a in actors if a.get_actor_label() == 'HC_M5VS2_Lookdev_Backdrop'+direction])
                       for direction in ('West', 'East', 'North', 'South')]
        for direction, match in backgrounds:
            check('one copied background '+direction, len(match) == 1 and match[0].actor_has_tag(U.Name(OWNER)))
            before = actor_snapshot(match[0])
            check('visual-only copied background '+direction, before['collision_profile'] == 'NoCollision')
            check('remove private copied background '+direction, ACT.destroy_actor(match[0]))
            removed.append(before)
        floors = [a for a in ACT.get_all_level_actors() if a.get_actor_label() == 'HC_M5VS2_Lookdev_Floor']
        check('one copied floor', len(floors) == 1 and floors[0].actor_has_tag(U.Name(OWNER)))
        floor = floors[0]
        before = actor_snapshot(floor)
        check('original flat 30m floor preserved', before['location'] == [0, 0, -10] and before['scale'] == [30, 30, .2]
              and before['mesh'] == '/Engine/BasicShapes/Cube' and before['collision_profile'] == 'BlockAll')
        floor.get_component_by_class(U.StaticMeshComponent).set_material(0, material())
        director.set_editor_property('tags', [U.Name(OWNER), U.Name(SOLE_OWNER)])
        director.set_editor_property('sole_points', points)
        director.set_editor_property('sole_selection_evidence', str(selection_path))
        check('native twelve-point property roundtrip', [point_values(p) for p in director.get_editor_property('sole_points')] == selection['points'])
        A.set_metadata_tag(world, 'HarborCityOwnedBy', SOLE_OWNER)
        R['recording_fixture'] = dict(removed_visual_only_backgrounds=removed, floor_before=before, floor_after=actor_snapshot(floor),
                                     frozen_previous_route_cm=dict(x=[-372.1479526350529, 0], y=[-559.2227522397668, 1067.989227106667]),
                                     route_source='ca956725 native final_pose_frames; historical observation, not predicted current success')
        check('native save private sole map', LEVEL.save_current_level())
        R['assets'].append(dict(path=MAP, sha256=sha(asset_file(MAP, True))))
    check('only private map and reference material authored', len(R['assets']) == 2)
    R['status'] = 'PASS'
except Exception as exc:
    R['status'], R['error'], R['traceback'] = 'FAIL', repr(exc), traceback.format_exc()
    U.log_error(R['traceback'])
finally:
    changed = [path for path, digest in PROTECTED.items() if not Path(path).is_file() or sha(path) != digest]
    R['source_preservation'] = dict(sha256_before=PROTECTED, changed=changed)
    if changed:
        R['status'], R['error'] = 'FAIL', 'Protected source changed'
    R['finished_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
    U.log('M5VS2_GAIT_SOLE_AUTHOR_'+R['status'])
