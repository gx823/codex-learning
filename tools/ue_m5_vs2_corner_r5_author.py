"""Isolated R5 environment author, unchanged 50 m P0 revision-two boundary.
Root alone runs this in native UE; R4/R3 scripts/layouts and old maps are protected.

CornerVTwoProbe: read-only asset/API/layout inspection (no packages created).
CornerVTwo: unique map, materials and geometry; preserves all previous candidates.
The SHA-pinned first author supplies only explicitly allowlisted definitions,
never its imports, assignments, main block or prior-map mutation code.
"""
from pathlib import Path
import ast
import datetime as dt
import hashlib
import itertools
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
LAYOUT_FILE = DOC / 'research/HARBOR_CORNER_P0_REV2_LAYOUT_R5.json'
SOURCE_PACK = '/Game/Fantastic_Village_Pack'
ROOT = '/Game/HarborCity/M5VS2/WorldRev2'
OWNER = 'HarborCity_M5VS2_HarborCorner_Rev2'
KEY = 'HarborCityOwnedBy'
HERO_ROOT = '/Game/HarborCity/M5VS2/HeroSelestia'
HERO = HERO_ROOT + '/BP_M5VS2_Selestia'
GAME_MODE = HERO_ROOT + '/BP_M5VS2_SelestiaGameMode'
NPC_ROOT = '/Game/HarborCity/M5VS2/NPC'
A = U.EditorAssetLibrary
E = U.MaterialEditingLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)
AT = U.AssetToolsHelpers.get_asset_tools()
CMD = U.SystemLibrary.get_command_line()
HELPER = WORK / 'tools/ue_m5_vs2_harbor_corner_author.py'
HELPER_SHA = 'aecc85af3b331a96ae059d918929b5e86e764a3ca45b0a66e0b83478a836cdea'
assert hashlib.sha256(HELPER.read_bytes()).hexdigest() == HELPER_SHA, 'Original author changed; review helper delta first'
HELPER_NAMES = {
    'argument', 'dump', 'check', 'sha', 'package', 'disk', 'document', 'binding',
    'latest_pass', 'protect_file', 'load', 'generated_class', 'vector', 'xyz',
    'native_transform', 'template_transform', 'transform_record', 'transformed_bounds',
    'place_bounds', 'save_owned', 'mesh_bounds', 'prop', 'box', 'material_node', 'connect',
    'surface_material', 'road_centerline', 'offset_line', 'accept_npc_getup_transition',
    'npc_specimen', 'resolve_selected_materials', 'atmosphere_readback',
}
helper_tree = ast.parse(HELPER.read_text(encoding='utf-8-sig'))
helper_defs = [n for n in helper_tree.body if isinstance(n, ast.FunctionDef) and n.name in HELPER_NAMES]
assert {n.name for n in helper_defs} == HELPER_NAMES
exec(compile(ast.Module(body=helper_defs, type_ignores=[]), str(HELPER), 'exec'), globals())
OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and not (OUT / 'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
PHASE = argument('M5AuthorPhase')
ASSET_ROOT = ROOT + '/Corner_' + hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
MAP = ASSET_ROOT + '/L_AnimeHarbor_Corner_P0_Rev2'
R = dict(schema='HarborCity.M5VS2.CornerVTwo.v1', status='RUNNING', phase=PHASE,
    map=MAP, owner=OWNER, dedicated_assets=ASSET_ROOT, checks=[], assets=[], inputs={},
    actors=[], houses=[], props=[], npc_bindings=[], source_blueprints_spawned=0,
    runtime='NOT_RUN', visual='NOT_RUN_NULLRHI', user_art_approval='USER_REVIEW',
    started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}; CACHE = {}; CREATED = []; COPY_MAP = {}; PROP_SOURCES = {}
BUILDING_MATERIALS = {}; SELECTED_WATER = None; HOUSE_TRANSFORMS = {}
LAND_POLYGON = []


def tag(actor, *names):
    values = list(actor.get_editor_property('tags'))
    actor.set_editor_property('tags', values + [U.Name(n) for n in names if U.Name(n) not in values])


def spawn(label, kind, position=(0, 0, 0), rotation=None, group=''):
    check('bounded revision-two actor count', len(CREATED) < 650)
    actor = ACT.spawn_actor_from_class(kind, vector(position), rotation or U.Rotator(), False)
    check('native actor creation', actor is not None, label)
    actor.set_actor_label('HC_M5VS2_CornerV2_' + label)
    actor.set_editor_property('tags', [U.Name(OWNER), U.Name('CornerGroup_' + group)])
    CREATED.append(actor)
    R['actors'].append(dict(label=actor.get_actor_label(), class_path=actor.get_class().get_path_name(), group=group))
    return actor


def static_mesh(label, mesh, transform, materials=None, collision=True, group=''):
    actor = spawn(label, U.StaticMeshActor, group=group)
    body = actor.get_component_by_class(U.StaticMeshComponent)
    body.set_mobility(U.ComponentMobility.STATIC)
    check('native static mesh binding', body.set_static_mesh(mesh), mesh.get_path_name())
    for i, material in enumerate(materials or []):
        if material is not None:
            body.set_material(i, material)
    body.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    actor.set_actor_transform(transform, False, True)
    error = max(math.dist(xyz(U.MathLibrary.transform_location(transform, vector(p))),
                         xyz(U.MathLibrary.transform_location(actor.get_actor_transform(), vector(p))))
                for p in ((0, 0, 0), (100, 0, 0), (0, 100, 100)))
    check('native placement readback', error < .02, dict(label=label, error_cm=error))
    return actor


def point_segment_distance(p, a, b):
    d = [b[i]-a[i] for i in (0, 1)]
    t = max(0., min(1., sum((p[i]-a[i])*d[i] for i in (0, 1))/sum(v*v for v in d)))
    return math.dist(p, [a[i]+t*d[i] for i in (0, 1)])


def segment_box_distance(a, b, bounds):
    lo, hi = bounds['min'], bounds['max']
    enter, leave = 0., 1.
    for i in (0, 1):
        d = b[i]-a[i]
        if abs(d) < 1.e-9:
            if not lo[i] <= a[i] <= hi[i]:
                enter, leave = 1., 0.; break
        else:
            t0, t1 = sorted(((lo[i]-a[i])/d, (hi[i]-a[i])/d))
            enter, leave = max(enter, t0), min(leave, t1)
    if enter <= leave:
        return 0.
    corner_distance = min(point_segment_distance((x, y), a, b) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]))
    endpoint_distance = min(math.hypot(*(max(lo[i]-p[i], 0., p[i]-hi[i]) for i in (0, 1))) for p in (a, b))
    return min(corner_distance, endpoint_distance)


def point_in_land(point, polygon):
    inside = False
    for a, b in zip(polygon, polygon[1:]+polygon[:1]):
        if point_segment_distance(point, a, b) <= 1.e-5:
            return True
        if (a[1] > point[1]) != (b[1] > point[1]):
            crossing = a[0]+(point[1]-a[1])*(b[0]-a[0])/(b[1]-a[1])
            if point[0] < crossing:
                inside = not inside
    return inside


def segment_in_land(a, b, polygon):
    # Test every interval separated by an actual boundary intersection. Four
    # inside corners alone would miss an inward cove crossing a long wall/road.
    if not point_in_land(a, polygon) or not point_in_land(b, polygon):
        return False
    v = [b[i]-a[i] for i in (0, 1)]; length2 = sum(t*t for t in v)
    if length2 < 1.e-12:
        return True
    cuts = [0., 1.]
    for c, d in zip(polygon, polygon[1:]+polygon[:1]):
        w = [d[i]-c[i] for i in (0, 1)]; q = [c[i]-a[i] for i in (0, 1)]
        denominator = v[0]*w[1]-v[1]*w[0]
        if abs(denominator) > 1.e-10:
            t = (q[0]*w[1]-q[1]*w[0])/denominator
            u = (q[0]*v[1]-q[1]*v[0])/denominator
            if -1.e-9 <= t <= 1+1.e-9 and -1.e-9 <= u <= 1+1.e-9:
                cuts.append(max(0., min(1., t)))
        elif abs(q[0]*v[1]-q[1]*v[0]) < 1.e-7:
            for p in (c, d):
                cuts.append(max(0., min(1., sum((p[i]-a[i])*v[i] for i in (0, 1))/length2)))
    cuts = sorted(set(cuts))
    return all(point_in_land([a[i]+v[i]*(lo+hi)*.5 for i in (0, 1)], polygon)
               for lo, hi in zip(cuts, cuts[1:]) if hi-lo > 1.e-10)


def land_footprint_guard(polygon, footprint, label):
    check('entire footprint lies on actual concave land polygon',
          all(segment_in_land(a, b, polygon) for a, b in zip(footprint, footprint[1:]+footprint[:1])),
          dict(label=label, footprint_vertices=len(footprint), boundary_vertices=len(polygon)))


def polygon_guard(polygon):
    area = sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(polygon,polygon[1:]+polygon[:1]))
    check('positive land winding', area > 500.)
    edges = list(zip(polygon,polygon[1:]+polygon[:1]))
    for i,(a,b) in enumerate(edges):
        for j,(c,d) in enumerate(edges):
            if j <= i+1 or (i == 0 and j == len(edges)-1):
                continue
            def cross(p,q,r):
                return (q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0])
            touching = min(point_segment_distance(p,x,y) for p,x,y in
                           ((a,c,d),(b,c,d),(c,a,b),(d,a,b))) <= 1.e-5
            intersecting = cross(a,b,c)*cross(a,b,d) < 0 and cross(c,d,a)*cross(c,d,b) < 0
            check('simple non-self-intersecting land polygon', not touching and not intersecting, [i,j])


def bounds_footprint(bounds):
    lo, hi = bounds['min'], bounds['max']
    return [[lo[0],lo[1]], [hi[0],lo[1]], [hi[0],hi[1]], [lo[0],hi[1]]]


def actual_actor_bounds(actor):
    origin, extent = actor.get_actor_bounds(False, False)
    origin, extent = xyz(origin), xyz(extent)
    return dict(min=[origin[i]-extent[i] for i in range(3)], max=[origin[i]+extent[i] for i in range(3)])


def bounds_guard(layout, bounds, label, doors=True, corridor=True):
    check('complete instance AABB inside fifty metre land',
          -2500 <= bounds['min'][0] <= bounds['max'][0] <= layout['land_max_x']
          and -2500 <= bounds['min'][1] <= bounds['max'][1] <= 2500,
          dict(label=label, bounds=bounds))
    land_footprint_guard(layout['land_outline_xy'], bounds_footprint(bounds), label)
    if corridor:
        points = road_centerline(layout['road']['points_xy'], layout['road']['samples_per_segment'])
        distance = min(segment_box_distance(a, b, bounds) for a, b in zip(points, points[1:]))
        check('full AABB outside six metre road and two metre sidewalks', distance >= 502.,
              dict(label=label, clearance_cm=distance, required_cm=502.))
    if doors:
        for zone in layout['door_clearance_zones']:
            overlap = all(bounds['max'][i] > zone['min_xy'][i] and bounds['min'][i] < zone['max_xy'][i] for i in (0, 1))
            check('door approach kept clear', not overlap, dict(label=label, door=zone['name']))


def resolve_inputs():
    layout = document(LAYOUT_FILE)
    check('exact new layout identity', layout['owner'] == OWNER and layout['schema_version'] == 2
          and layout['playable_bounds_xy'] == [-2500, -2500, 2500, 2500])
    predecessor = DOC / layout['revision_source']['relative_path']
    protect_file(predecessor, layout['revision_source']['sha256']); protect_file(LAYOUT_FILE)
    check('exact original R2 layout remains byte-identical',
          predecessor.name == 'HARBOR_CORNER_P0_REV2_LAYOUT.json'
          and layout['revision_source']['sha256'] == '2a165dafc37a22e7cccb06195bb67b73d73833151d963af6e288b687d501b48e'
          and layout['art_revision'] == 5)
    previous_r3 = DOC / layout['revision_source_r3']['relative_path']
    protect_file(previous_r3, layout['revision_source_r3']['sha256'])
    previous_author = WORK / 'tools/ue_m5_vs2_corner_v2_author.py'
    protect_file(previous_author, 'c559124fc454dc8c46575d45b9f270dc903119831514cc3b6bb301a21b4aeb05')
    R['inputs']['r3_layout'] = binding(previous_r3)
    R['inputs']['r3_author'] = binding(previous_author)
    r4_layout = DOC / layout['revision_source_r4']['relative_path']
    protect_file(r4_layout, '9c9db0939581c8c16e4051619c5d3501601e1dad5e63d44da1dc99cbc6ed51f2')
    r4_author = WORK / 'tools/ue_m5_vs2_corner_r4_author.py'
    protect_file(r4_author, 'dc45cefbee8b6224f478cdd6a1fa8c77075be07ec0e95a020efaa7969f2f61b8')
    r4_values = document(r4_layout)
    allowed_layout_changes = {'art_revision', 'revision_source_r4', 'cloud_revision', 'horizon_revision', 'r5_review_evidence'}
    check('R5 layout changes restricted to sky and same-footprint distant terrain metadata',
          all(layout.get(k) == r4_values.get(k) for k in set(layout) | set(r4_values)
              if k not in allowed_layout_changes))
    R['inputs']['r4_layout'] = binding(r4_layout)
    R['inputs']['r4_author'] = binding(r4_author)
    R['inputs']['r5_author'] = binding(Path(__file__).resolve())
    LAND_POLYGON[:] = layout['land_outline_xy']
    check('finite inward shoreline stays in original fifty metre boundary',
          8 <= len(LAND_POLYGON) <= 128 and len({tuple(p) for p in LAND_POLYGON}) == len(LAND_POLYGON)
          and all(len(p) == 2 and all(math.isfinite(v) and -2500 <= v <= 2500 for v in p) for p in LAND_POLYGON))
    original_land = document(predecessor)['land_outline_xy']
    polygon_guard(LAND_POLYGON)
    land_footprint_guard(original_land, LAND_POLYGON, 'New shoreline never expands original playable land')
    road = layout['road']; points = road_centerline(road['points_xy'], road['samples_per_segment'])
    full_street = offset_line(points,500)+cap_points(points[-1],500,True)[1:-1]
    full_street += list(reversed(offset_line(points,-500)))+cap_points(points[0],500,False)[1:-1]
    land_footprint_guard(LAND_POLYGON, full_street, 'Full six metre road plus both two metre footways and endcaps')
    for file in (PROJECT/'Content/HarborCity/M5VS2/WorldRev2').rglob('*'):
        if file.is_file() and file.suffix in ('.uasset', '.umap'):
            protect_file(file)
    check('seven complete instances of four existing source houses', len(layout['houses']) == 7
          and {h['house'] for h in layout['houses']} == {2, 6, 9, 14})
    copied_file, copied = latest_pass('author_*Village*', lambda d: d.get('mode') == 'Copy')
    check('native copied geometry input', copied['status'] == 'PASS' and copied['source_unchanged'] is True
          and copied['geometry_paths_state'] == 'SAVED' and copied['selection_kind'] == 'STATIC_GEOMETRY_ONLY'
          and copied['native']['saved_packages_and_reference_remap_verified'] is True)
    for row in copied['assets']:
        protect_file(disk(row['path']), row['sha256']); COPY_MAP[row['source']] = row['path']
    manifest_file = Path(copied['manifest']['path']); protect_file(manifest_file, copied['manifest']['sha256'])
    for row in document(manifest_file)['candidates']:
        PROP_SOURCES[row['role']] = row['path']
    assemblies = {int(a['source_blueprint'].rsplit('_', 1)[1]): a for a in copied['geometry_assemblies']}
    check('exact native template assemblies', set(assemblies) == {2, 6, 9, 14}
          and sum(len(a['components']) for a in assemblies.values()) == 55)
    resolve_selected_materials(layout)
    q, r = npc_specimen('Q'), npc_specimen('R')
    for folder in (PROJECT/'Config', disk(HERO).parent):
        for file in sorted(folder.rglob('*')):
            if file.is_file() and file.suffix.lower() in ('.ini', '.uasset'):
                protect_file(file)
    for file in (PROJECT/'Content').rglob('*.umap'):
        protect_file(file)
    mode = generated_class(GAME_MODE); hero = generated_class(HERO)
    check('existing actual Hero game mode', U.get_default_object(mode).get_editor_property('default_pawn_class') == hero)
    cdo = U.get_default_object(hero); body = cdo.get_component_by_class(U.SkeletalMeshComponent)
    capsule = cdo.get_component_by_class(U.CapsuleComponent)
    R['hero_binding'] = dict(blueprint=HERO, generated_class=hero.get_path_name(), game_mode=mode.get_path_name(),
        mesh=body.get_editor_property('skeletal_mesh_asset').get_path_name(),
        animation=body.get_editor_property('anim_class').get_path_name(),
        materials=[body.get_material(i).get_path_name() for i in range(body.get_num_materials())],
        capsule_half_height_cm=float(capsule.get_unscaled_capsule_half_height()))
    for path in [R['hero_binding']['mesh'], R['hero_binding']['animation']] + R['hero_binding']['materials']:
        protect_file(disk(path))
    R['inputs'].update(layout=binding(LAYOUT_FILE), helpers=binding(HELPER), village_copy=binding(copied_file))
    R['inputs']['original_revision_two_layout'] = binding(predecessor)
    R['art_revision'] = layout['art_revision']; R['limitations'] = layout['limitations']
    for placement in layout['houses']:
        transform = place_bounds(assemblies[placement['house']]['bounds_cm'], placement['center_xy'], 0, placement['yaw'])
        bounds_guard(layout, transformed_bounds(assemblies[placement['house']]['bounds_cm'], transform), placement['role'], doors=False)
    for row in layout['props']+[dict(r,role='cafe_table') for r in layout['table_groups']]:
        source=row.get('source_path') or PROP_SOURCES[row['role']]
        shape=mesh_bounds(load(COPY_MAP[source]));scale=row.get('scale',1)
        scale=(scale,)*3 if isinstance(scale,(int,float)) else scale
        transform=place_bounds(shape,row['center_xy'],row.get('ground_z',0),row.get('yaw',0),scale)
        bounds_guard(layout,transformed_bounds(shape,transform),row['name'],
            doors=row.get('door_guard',True),corridor=row.get('corridor_guard',True))
    return layout, assemblies, {'Q': q, 'R': r}, mode


def material_surface_snapshot(material):
    outputs = {}
    for key in ('BASE_COLOR', 'ROUGHNESS', 'METALLIC', 'SPECULAR', 'EMISSIVE_COLOR', 'OPACITY', 'WORLD_POSITION_OFFSET'):
        field = getattr(U.MaterialProperty, 'MP_' + key)
        node = E.get_material_property_input_node(material, field)
        entry = None
        if node:
            entry = dict(class_path=node.get_class().get_path_name(),
                         output=E.get_material_property_input_node_output_name(material, field))
            if isinstance(node, U.MaterialExpressionConstant):
                entry['constant'] = float(node.get_editor_property('r'))
        outputs[key] = entry
    return dict(path=material.get_path_name(),
                blend_mode=str(material.get_editor_property('blend_mode')),
                shading_model=str(material.get_editor_property('shading_model')),
                two_sided=bool(material.get_editor_property('two_sided')),
                disable_depth_test=bool(material.get_editor_property('disable_depth_test')),
                outputs=outputs)


def r4_surface_probe():
    # Read the actual packages used by the seven R4 native images. A neutral
    # source color or a ray through authored geometry is not proof of GPU output.
    path = DOC / 'editor_runtime/author_20260925_072631_409_78941ab7_CornerVTwo/author_result.json'
    protect_file(path, '831d51f263d4aec822c1c6c6f328697d6bb9a6bd3690e6f58f2b7fcc4324d4c7')
    source = document(path)
    check('actual R4 native author baseline', source['status'] == 'PASS' and source['art_revision'] == 4)
    rocks = [row for row in source['original_geometry'] if '_CoastRock_' in row['label']]
    check('all actual R4 coastal rocks inspected', len(rocks) == 15)
    meshes = []
    material_paths = set()
    for row in rocks:
        mesh = load(row['mesh']); protect_file(disk(row['mesh']))
        slots = mesh.get_editor_property('static_materials')
        paths = [slot.get_editor_property('material_interface').get_path_name() for slot in slots]
        material_paths.update(paths)
        meshes.append(dict(mesh=mesh.get_path_name(), material_slots=paths))
    expected = source['dedicated_assets'] + '/Materials/M_R4_CoastalStrata.M_R4_CoastalStrata'
    check('actual R4 rock material slots match saved author', material_paths == {expected}, sorted(material_paths))
    stone = load(expected); protect_file(disk(expected))
    water = load(source['water_revision']['candidate']); protect_file(disk(water.get_path_name()))
    parent = water.get_editor_property('parent'); protect_file(disk(parent.get_path_name()))
    stone_read = material_surface_snapshot(stone)
    water_read = material_surface_snapshot(parent)
    R['r4_surface_probe'] = dict(author=binding(path), rocks=meshes, stone=stone_read,
                                water=water_snapshot(water), water_master=water_read,
                                interpretation='Readback only. Blue aerial rock faces require new native render comparison; no root cause inferred from screenshot alone.')
    check('R4 rock remains opaque and water has no vertex displacement',
          stone.get_editor_property('blend_mode') == U.BlendMode.BLEND_OPAQUE
          and stone_read['outputs']['EMISSIVE_COLOR'] is None
          and stone_read['outputs']['WORLD_POSITION_OFFSET'] is None
          and water_read['outputs']['WORLD_POSITION_OFFSET'] is None)


def probe_apis():
    setter_source=Path('E:/UE_5.8/Engine/Source/Editor/MaterialEditor/Private/MaterialEditingLibrary.cpp')
    setter_sha='96051980458dad86719f195072da4bd34eebd07a80d647ebb50bbbb0626e5565'
    protect_file(setter_source,setter_sha)
    R['water_setter_contract']=dict(source=str(setter_source),sha256=setter_sha,
        scalar_lines=[1485,1494],vector_lines=[1573,1582],
        observation='Both local UE implementations call SetParameterEditorOnly and UpdateMaterialInstance, then return an unchanged false bool. Success must be proven by actual parameter readback, not this return value.')
    calls = {'GeometryScript_MeshEdits': ['append_buffers_to_mesh'],
             'GeometryScript_Normals': ['set_per_vertex_normals'],
             'GeometryScript_AssetUtils': ['copy_mesh_from_static_mesh_v2'],
             'GeometryScript_MeshBooleans': ['apply_mesh_boolean'],
             'GeometryScript_Primitives': ['append_box'],
             'GeometryScript_NewAssetUtils': ['create_new_static_mesh_asset_from_mesh'],
             'PointLightComponent': ['set_attenuation_radius','set_cast_shadows'],
             'StaticMeshComponent': ['set_cast_shadow'], 'Actor': ['get_actor_bounds'],
             'MaterialEditingLibrary': ['get_scalar_parameter_names','get_vector_parameter_names',
                'get_texture_parameter_names','get_static_switch_parameter_names',
                'get_material_instance_scalar_parameter_value','get_material_instance_vector_parameter_value',
                'get_material_instance_texture_parameter_value','get_material_instance_static_switch_parameter_value',
                'set_material_instance_vector_parameter_value','set_material_instance_scalar_parameter_value','update_material_instance']}
    R['api'] = {}
    for class_name, methods in calls.items():
        kind = getattr(U, class_name, None); check('reflected native API class', kind is not None, class_name)
        for method in methods:
            call = getattr(kind, method, None); check('reflected native API method', call is not None, class_name+'.'+method)
            R['api'][class_name+'.'+method] = call.__doc__
    for class_name in ['GeometryScriptSimpleMeshBuffers', 'HCM5VS2FlightBounds', 'MaterialExpressionVectorParameter',
                       'MaterialExpressionScalarParameter', 'PointLight', 'MaterialExpressionCustom']:
        check('required reflected type', hasattr(U, class_name), class_name)
    buffers = U.GeometryScriptSimpleMeshBuffers()
    for field in ('vertices', 'triangles', 'normals', 'uv0', 'vertex_colors'):
        R['api']['buffers.'+field] = str(type(buffers.get_editor_property(field)))
    R['api']['Box']=U.Box.__doc__
    test_box=U.Box(min=U.Vector(-1,-2,-3),max=U.Vector(1,2,3))
    check('native Box constructor validity',bool(test_box.get_editor_property('is_valid'))
          and xyz(test_box.min)==[-1.,-2.,-3.] and xyz(test_box.max)==[1.,2.,3.])
    original_water=water_snapshot(load(SELECTED_WATER))
    check('actual selected water baseline and exact three-field author scope',
          original_water['parameters']['scalar']['Caustics Intensity']==20.
          and original_water['parameters']['scalar']['Fade Distance']==160.
          and original_water['parameters']['scalar']['Water Height']==-150.
          and all(n in original_water['parameters']['vector'] for n in ('Water Color Shallow','Water Color Deep')))
    R['water_source_readback']=original_water
    r4_surface_probe()
    R['pass_scope'] = 'Only reflected APIs, protected native inputs and complete layout AABBs; no packages or actors created.'


def generated_normal_guard(name, dynamic, vertices, triangles, stage):
    # Restrict the fix/check to our own closed meshes. Roads use Unreal's
    # native polygon extrusion, and the deliberately two-sided sail is separate.
    affected = name.startswith(('CoastRock_', 'DistantIsland_')) or name == 'ClosedShoreFoundation'
    if not affected:return
    maximum_z=max(p[2] for p in vertices)
    expected_up=[]
    for tid,tri in enumerate(triangles):
        z=[vertices[i][2] for i in tri]
        if (name.startswith('CoastRock_') and sum(z)/3 > maximum_z*.45
            or name.startswith('DistantIsland_') and min(z)>=-1.e-6
            or name=='ClosedShoreFoundation' and min(z)>maximum_z-.01):
            expected_up.append(tid)
    check('bounded known upper surface normal sample exists',len(expected_up)>10,dict(name=name,count=len(expected_up)))
    eligible=len(expected_up)
    if eligible>128:expected_up=[expected_up[(i*(eligible-1))//127] for i in range(128)]
    minimum=1.;failed=[];overlay_minimum=1.
    for tid in expected_up:
        face,valid=U.GeometryScript_MeshQueries.get_triangle_face_normal(dynamic,tid)
        values=U.GeometryScript_MeshQueries.get_triangle_normals(dynamic,tid)
        if not valid or not values[-1] or float(face.z)<-1.e-5:failed.append(tid)
        if valid:minimum=min(minimum,float(face.z))
        if values[-1]:overlay_minimum=min(overlay_minimum,*(float(v.z) for v in values[1:4]))
    record=dict(name=name,stage=stage,eligible_upper_triangles=eligible,sampled_triangles=len(expected_up),minimum_native_face_normal_z=minimum,
                minimum_overlay_normal_z=overlay_minimum,failed_face_triangles=failed,
                convention='UE VectorUtil::Normal uses (V2-V0).Cross(V1-V0); upper faces must point upward.')
    R.setdefault('generated_normal_readback',[]).append(record)
    check('actual native generated upper-face normals point up',not failed,record)


def native_mesh(name, vertices, triangles, material, colors=None, collision=False, transform=None, group='OriginalGeometry'):
    check('bounded actual original geometry', 4 <= len(vertices) <= 20000 and 2 <= len(triangles) <= 40000)
    check('finite nondegenerate indexed triangles', all(all(math.isfinite(v) for v in p) for p in vertices)
          and all(len(set(t)) == 3 and min(t) >= 0 and max(t) < len(vertices) for t in triangles))
    buffers = U.GeometryScriptSimpleMeshBuffers()
    buffers.set_editor_property('vertices', [vector(p) for p in vertices])
    buffers.set_editor_property('triangles', [U.IntVector(*t) for t in triangles])
    buffers.set_editor_property('uv0', [U.Vector2D(p[0]/200., p[1]/200.) for p in vertices])
    if colors:
        buffers.set_editor_property('vertex_colors', [U.LinearColor(*c, 1) for c in colors])
    dynamic = U.DynamicMesh()
    U.GeometryScript_MeshEdits.append_buffers_to_mesh(dynamic, buffers, 0, False)
    U.GeometryScript_Normals.set_per_vertex_normals(dynamic)
    generated_normal_guard(name,dynamic,vertices,triangles,'PRE_SAVE_NATIVE_DYNAMIC_MESH')
    mesh = save_dynamic(name, dynamic, [material], collision)
    if name.startswith(('CoastRock_', 'DistantIsland_')) or name=='ClosedShoreFoundation':
        saved=U.DynamicMesh()
        copied=U.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,saved,
            U.GeometryScriptCopyMeshFromAssetOptions(),U.GeometryScriptMeshReadLOD(),False)
        check('new saved generated mesh native readback',copied[-1]==U.GeometryScriptOutcomePins.SUCCESS,name)
        # UE may reorder triangles while building the asset, so the saved check
        # derives its upper-face selection from the actual copied vertices.
        names=[n for n in dir(U.GeometryScript_MeshQueries) if n.replace('_','').lower()=='getnumtriangleids']
        check('unique actual triangle count method',len(names)==1,names)
        count=getattr(U.GeometryScript_MeshQueries,names[0])(saved)
        saved_vertices=[];saved_triangles=[];invalid=[]
        for tid in range(count):
            valid,a,b,c=U.GeometryScript_MeshQueries.get_triangle_positions(saved,tid)
            if not valid:invalid.append(tid)
            start=len(saved_vertices);saved_vertices.extend([xyz(a),xyz(b),xyz(c)]);saved_triangles.append((start,start+1,start+2))
        check('saved generated triangle positions valid',not invalid,dict(name=name,invalid=invalid))
        generated_normal_guard(name,saved,saved_vertices,saved_triangles,'SAVED_NATIVE_SOURCE_MODEL')
    actor = static_mesh(name, mesh, transform or native_transform(), [material], collision, group)
    R.setdefault('original_geometry', []).append(dict(label=actor.get_actor_label(), vertices=len(vertices), triangles=len(triangles),
        collision=collision, source='Original deterministic 3D geometry, no downloaded mesh or image', mesh=mesh.get_path_name()))
    return actor


def save_dynamic(name, dynamic, materials, collision=True):
    options = U.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property('enable_recompute_tangents', True)
    options.set_editor_property('enable_collision', collision)
    options.set_editor_property('collision_mode', U.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh, outcome = U.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, ASSET_ROOT+'/Geometry/'+name, options)
    check('native new static mesh asset', outcome == U.GeometryScriptOutcomePins.SUCCESS and mesh is not None, name)
    for i, material in enumerate(materials):
        mesh.set_material(i, material)
    save_owned(mesh)
    return mesh


def strip_mesh(name, polygon, bottom_z, height, material, collision=True, group='StreetStructure'):
    land_footprint_guard(LAND_POLYGON, polygon, name)
    area = sum(polygon[i][0]*polygon[(i+1)%len(polygon)][1]-polygon[(i+1)%len(polygon)][0]*polygon[i][1] for i in range(len(polygon)))
    if area < 0:
        polygon = list(reversed(polygon))
    check('bounded continuous polygon', 4 <= len(polygon) <= 256 and abs(area) > 500)
    dynamic = U.DynamicMesh()
    U.GeometryScript_Primitives.append_simple_extrude_polygon(dynamic, U.GeometryScriptPrimitiveOptions(),
        native_transform((0, 0, bottom_z)), [U.Vector2D(*p) for p in polygon], height, 0, True, U.GeometryScriptPrimitiveOriginMode.BASE)
    query=U.GeometryScript_MeshQueries
    count_name=[n for n in dir(query) if n.replace('_','').lower()=='getnumtriangleids']
    check('native extrusion triangle query exact',len(count_name)==1,count_name)
    count=getattr(query,count_name[0])(dynamic);top_ids=[];normal_z=[];degenerate_top=[];degenerate_normal_errors=[]
    for tid in range(count):
        valid,a,b,c=query.get_triangle_positions(dynamic,tid)
        if valid and all(abs(float(p.z)-(bottom_z+height))<.02 for p in (a,b,c)):
            face,normal_valid=query.get_triangle_face_normal(dynamic,tid)
            e1=[float(getattr(b,k)-getattr(a,k)) for k in ('x','y','z')]
            e2=[float(getattr(c,k)-getattr(a,k)) for k in ('x','y','z')]
            cross=(e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0])
            twice_area_squared=sum(v*v for v in cross)
            if twice_area_squared==0.:
                # UE5.8 AppendSimpleExtrudePolygon with HeightSteps=0 builds
                # Path [0,H,H], so its last side strip has exactly zero area.
                # Such triangles have no facing normal. Preserve and report
                # them separately; never accept a nonzero-area downward face.
                degenerate_top.append(tid)
                if not normal_valid or any(float(getattr(face,k))!=0. for k in ('x','y','z')):
                    degenerate_normal_errors.append(tid)
                continue
            top_ids.append(tid);normal_z.append(float(face.z) if normal_valid else -2.)
    record=dict(name=name,stage='UNCHANGED_ENGINE_POLYGON_EXTRUSION',nonzero_area_top_triangles=len(top_ids),
                exact_zero_area_top_triangles=len(degenerate_top),zero_area_triangle_ids=degenerate_top,
                degenerate_normal_errors=degenerate_normal_errors,
                minimum_native_face_normal_z=min(normal_z) if normal_z else None,triangle_order_modified=False,
                zero_area_scope='Existing engine extrusion has coincident final rings. Recorded, not modified and not counted as a facing surface.')
    R.setdefault('generated_normal_readback',[]).append(record)
    check('unchanged native road and ground nondegenerate top normals point up',
          bool(top_ids) and min(normal_z)>.999 and not degenerate_normal_errors,record)
    mesh = save_dynamic(name, dynamic, [material], collision)
    return static_mesh(name, mesh, native_transform(), [material], collision, group)


def planting_mask_code(regions):
    lines = ['float Coverage=0.0;']
    for j, region in enumerate(regions):
        for k, (a, b) in enumerate(zip(region['points_xy'], region['points_xy'][1:])):
            label = str(j)+'_'+str(k); width = region['width_cm']*.5
            lines += ['float2 A%s=float2(%f,%f), B%s=float2(%f,%f);' % (label,*a,label,*b),
                'float2 V%s=B%s-A%s;' % (label,label,label),
                'float T%s=saturate(dot(XY-A%s,V%s)/dot(V%s,V%s));' % (label,label,label,label,label),
                'float D%s=length(XY-(A%s+V%s*T%s));' % (label,label,label,label),
                'float N%s=0.16*sin(XY.x*.024+sin(XY.y*.018))*sin(XY.y*.031);' % label,
                'Coverage=max(Coverage,1.0-smoothstep(%f,%f,D%s*(1.0+N%s)));' % (width*.42,width,label,label)]
    return '\n'.join(lines+['return Coverage;'])


def cap_points(center, radius, north=True, steps=20):
    angles = [math.pi*(1.-i/steps) if north else -math.pi*i/steps for i in range(steps+1)]
    return [(center[0]+radius*math.cos(a), center[1]+radius*math.sin(a)) for a in angles]


def author_street(layout):
    stone = load(COPY_MAP[PROP_SOURCES['stone_surface']]); road = layout['road']
    gravel = surface_material('M_Rev2_GardenGround', SOURCE_PACK+'/textures/T_ENV_TERRAIN_gravel_BC',
        SOURCE_PACK+'/textures/T_ENV_TERRAIN_gravel_N', 150., planting_islands=layout['planting_bands'],
        base_color_adjustment=layout['gravel_color_adjustment'])
    paving = surface_material('M_Rev2_StoneRoad', SOURCE_PACK+'/textures/T_stonebrick_02_BC',
        SOURCE_PACK+'/textures/T_stonebrick_01_N', 150., normal_flatness=.4)
    walk = surface_material('M_Rev2_Footway', SOURCE_PACK+'/textures/T_stonebrick_01_BC',
        SOURCE_PACK+'/textures/T_stonebrick_01_N', 150., normal_flatness=.45)
    foundation_geometry(layout['land_outline_xy'])
    strip_mesh('ClosedLandSurface', layout['land_outline_xy'], -5, 5, gravel)
    points = road_centerline(road['points_xy'], road['samples_per_segment'])
    left = offset_line(points, 300); right = offset_line(points, -300)
    outer_l = offset_line(points, 500); outer_r = offset_line(points, -500)
    road_polygon = left + cap_points(points[-1],300,True)[1:-1] + list(reversed(right)) + cap_points(points[0],300,False)[1:-1]
    strip_mesh('ContinuousCurvedStoneRoad6m', road_polygon, -5, 6, paving)
    strip_mesh('WestFootway2m', outer_l+list(reversed(left)), -3, 11, walk)
    strip_mesh('EastFootway2m', right+list(reversed(outer_r)), -3, 11, walk)
    for north in (False, True):
        center = points[-1] if north else points[0]
        outer = cap_points(center,500,north); inner = cap_points(center,300,north)
        strip_mesh(('North' if north else 'South')+'RoundedWalkEnd', outer+list(reversed(inner)), -3, 11, walk)
    for label, polygon in [('WestCurb',offset_line(points,310)+list(reversed(left))),
                           ('EastCurb',right+list(reversed(offset_line(points,-310))))]:
        strip_mesh(label,polygon,0,10,stone)
    for row in layout['forecourts']:
        strip_mesh(row['name'],row['polygon_xy'],-2,4,paving)
    R['road_geometry'] = dict(width_cm=600, sidewalks_each_cm=200, sidewalk_z=8, curb_z=10,
        centerline_cm=points, ends='Both curve into closed walking endcaps inside the bounded quay.',
        collision='Native complex-as-simple; actual walking/driving and doorway traversal NOT_RUN')


def simple_material(name, color, vertex_color=False, two_sided=False):
    material = AT.create_asset(name, ASSET_ROOT+'/Materials', U.Material, U.MaterialFactoryNew())
    material.set_editor_property('two_sided',two_sided)
    node = material_node(material,'VertexColor') if vertex_color else material_node(material,'Constant3Vector',constant=U.LinearColor(*color,1))
    connected=E.connect_material_property(node,'',U.MaterialProperty.MP_BASE_COLOR)
    actual=E.get_material_property_input_node(material,U.MaterialProperty.MP_BASE_COLOR)
    actual_output=E.get_material_property_input_node_output_name(material,U.MaterialProperty.MP_BASE_COLOR)
    check('original material base connected and read back',connected is True and actual==node and actual_output=='',
          dict(return_value=connected,input_node=actual.get_path_name() if actual else None,output=actual_output))
    rough = material_node(material,'Constant',r=.9)
    E.connect_material_property(rough,'',U.MaterialProperty.MP_ROUGHNESS)
    E.recompile_material(material); save_owned(material); return material


def foundation_geometry(outline):
    # Same bounded land surface, with an inward weathered cliff rather than an
    # exposed rectangular texture strip. Every side ring is guarded as land.
    polygon = []
    for a,b in zip(outline,outline[1:]+outline[:1]):
        steps = max(1,math.ceil(math.dist(a,b)/150.))
        polygon += [[a[j]+(b[j]-a[j])*i/steps for j in (0,1)] for i in range(steps)]
    center = [-350.,0.]; count = len(polygon)
    vertices=[]; colors=[]; triangles=[]; rings=[]
    for ring in range(3):
        xy=[]
        for i,p in enumerate(polygon):
            scale = 1. if ring == 0 else .986+.007*math.sin(i*1.73+ring)
            v=[center[j]+(p[j]-center[j])*scale for j in (0,1)];xy.append(v)
            z = -2. if ring == 0 else (-130. if ring == 1 else -370.)+16*math.sin(i*.91+ring)
            vertices.append((*v,z))
            tone=.035*math.sin(i*1.17)+.025*math.sin(i*.39+ring)
            colors.append((.24+tone,.265+tone,.265+tone))
        land_footprint_guard(outline,xy,'Weathered foundation ring '+str(ring))
        check('foundation cap center sees every boundary vertex',
              all(segment_in_land(center,p,xy) for p in xy))
        rings.append(xy)
    for ring in range(2):
        a=ring*count;b=a+count
        for i in range(count):
            n=(i+1)%count;triangles.extend([(a+i,b+i,b+n),(a+i,b+n,a+n)])
    for ring,z in ((0,-2.),(2,-390.)):
        middle=len(vertices);vertices.append((*center,z));colors.append((.24,.265,.265))
        a=ring*count
        for i in range(count):
            n=(i+1)%count
            triangles.append((middle,a+i,a+n) if ring == 0 else (middle,a+n,a+i))
    # This local hand-built foundation shares the R4 right-handed triangle
    # order defect; the engine-authored road/footway extrusions do not.
    triangles=[(a,c,b) for a,b,c in triangles]
    material=terrain_detail_material('M_R4_WeatheredQuay')
    native_mesh('ClosedShoreFoundation',vertices,triangles,material,colors,True,group='StreetStructure')
    R['foundation']=dict(land_outline=outline,rings=3,side_segments=count,
        scope='Closed native collision mesh; all side rings remain within original and revised land. Visual/traversal NOT_RUN.')


def island_elevation(x,y,r,height,seed):
    # Preserve the entire old XY footprint and height budget. Several unequal
    # narrow ridges have saddles between them instead of one Gaussian shoulder.
    crests = ((-.48,-.10,.64,.22,.43),(-.12,.15,.99,.20,.34),
              (.23,-.19,.76,.19,.40),(.53,.11,.48,.17,.31))
    peaks = 0.
    for cx,cy,amount,sx,sy in crests:
        dy = y-cy-.10*math.sin(x*8.+seed)
        distance = math.hypot((x-cx)/sx,dy/sy)
        peaks = max(peaks,amount*max(0.,1.-distance*.48)**1.55)
    # Small weathered ribs change the actual 3D surface, not just its tint.
    ribs = .027*abs(math.sin(x*33.+y*15.+seed)) + .018*math.sin(y*39.-x*8.)
    return height*min(1.,max(0.,peaks+ribs+.025))*max(0.,1-r**4)


def terrain_detail_material(name):
    # Preserve copied source textures. Vertex colors provide strata/vegetation
    # variation while the same-pack painted gravel supplies fine surface detail.
    material=AT.create_asset(name,ASSET_ROOT+'/Materials',U.Material,U.MaterialFactoryNew())
    position=material_node(material,'WorldPosition')
    samples={}
    for name_,channels in [('SampleXY',(True,True,False)),('SampleXZ',(True,False,True)),('SampleYZ',(False,True,True))]:
        uv=material_node(material,'ComponentMask',r=channels[0],g=channels[1],b=channels[2],a=False)
        divide=material_node(material,'Divide',const_b=240.)
        connect(position,'',uv,'Input');connect(uv,'',divide,'A')
        texture=material_node(material,'TextureSample',texture=load(COPY_MAP[SOURCE_PACK+'/textures/T_ENV_TERRAIN_gravel_BC']))
        connect(divide,'',texture,'UVs');samples[name_]=texture
    vertex=material_node(material,'VertexColor')
    normal=material_node(material,'VertexNormalWS')
    inputs=[]
    for name_ in ('SampleXY','SampleXZ','SampleYZ','NormalWS','Tint'):
        item=U.CustomInput();item.set_editor_property('input_name',name_);inputs.append(item)
    detail=material_node(material,'Custom',inputs=inputs,
        code='float3 w=pow(abs(NormalWS),4.); w/=max(dot(w,float3(1.,1.,1.)),.0001); float3 t=SampleXY*w.z+SampleXZ*w.y+SampleYZ*w.x; float l=dot(t,float3(.2126,.7152,.0722)); return Tint*(.72+saturate(l)*.70);',
        output_type=U.CustomMaterialOutputType.CMOT_FLOAT3)
    for name_,sample in samples.items():connect(sample,'RGB',detail,name_)
    # UE 5.8 VertexColor output 0 is unnamed (RGB mask), unlike TextureSample.
    connect(normal,'',detail,'NormalWS');connect(vertex,'',detail,'Tint')
    rough=material_node(material,'Constant',r=.94)
    check('R5 terrain sampled base output',E.connect_material_property(detail,'',U.MaterialProperty.MP_BASE_COLOR))
    check('R5 terrain roughness output',E.connect_material_property(rough,'',U.MaterialProperty.MP_ROUGHNESS))
    E.recompile_material(material);save_owned(material)
    R.setdefault('r5_terrain_materials',[]).append(dict(path=material.get_path_name(),
        source_texture=COPY_MAP[SOURCE_PACK+'/textures/T_ENV_TERRAIN_gravel_BC'],world_triplanar_period_cm=240.,
        emissive=False,opacity=False,wpo=False,scope='New material only; native render still NOT_RUN.'))
    return material


def island_geometry(size, height, seed):
    segments, rings = 80, 48
    vertices = [(0,0,island_elevation(0,0,0,height,seed))]; colors=[(.25,.34,.29)]; triangles=[]
    for ring in range(1,rings+1):
        r=ring/rings
        for i in range(segments):
            theta=2*math.pi*i/segments
            edge=1.+.085*math.sin(3*theta+seed)+.035*math.sin(7*theta+seed*.3)
            x,y=r*edge*math.cos(theta),r*edge*math.sin(theta)
            z=island_elevation(x,y,r,height,seed)
            vertices.append((size[0]*.5*x,size[1]*.5*y,z))
            tone=.025*math.sin(x*17+seed)*math.sin(y*14-seed)
            # Irregular gray exposed ridges and cooler wooded shoulders prevent
            # one solid green mound. Actual sun/sky still shade every surface.
            rocky=(z>height*.50 and math.sin(x*12+y*8+seed)>.1) or r>.82
            base=(.34,.355,.34) if rocky else (.23,.32,.265)
            if seed==2:base=tuple(.84*c+.16*h for c,h in zip(base,(.44,.51,.56)))
            colors.append(tuple(c+tone for c in base))
    for i in range(segments):
        triangles.append((0,1+i,1+(i+1)%segments))
    for ring in range(rings-1):
        a=1+ring*segments;b=a+segments
        for i in range(segments):
            n=(i+1)%segments; triangles.extend([(a+i,b+i,b+n),(a+i,b+n,a+n)])
    bottom=len(vertices); vertices.append((0,0,-150));colors.append((.18,.19,.17))
    a=1+(rings-1)*segments
    for i in range(segments):
        triangles.append((bottom,a+(i+1)%segments,a+i))
    triangles=[(a,c,b) for a,b,c in triangles]
    return vertices,triangles,colors


def coastal_rock_geometry(size,height,seed):
    # Asymmetric eroded ledges, not a repeated round barrel. Lowest two rings
    # remain submerged; unequal upper shelves break the bead-like aerial line.
    segments=17;vertices=[];colors=[];triangles=[]
    for ring,(radius,z) in enumerate(((.78,-100),(1.,15),(.89,height*.42),(.62,height*.79),(.18,height))):
        for i in range(segments):
            theta=2*math.pi*i/segments
            edge=1.+.17*math.sin(3*theta+seed)+.085*math.sin(5*theta+seed*.4)
            dx=ring*.045*math.cos(seed);dy=ring*.048*math.sin(seed)
            vertices.append((size[0]*.5*(radius*edge*math.cos(theta)+dx),
                             size[1]*.5*(radius*edge*math.sin(theta)+dy),
                              z+height*.08*math.sin(2*theta+seed+ring*.5)))
            tone=.032*math.sin(i*1.73+ring+seed)
            dry=.04*ring
            colors.append((.21+tone+dry,.235+tone+dry,.23+tone+dry))
    for ring in range(4):
        a=ring*segments;b=a+segments
        for i in range(segments):
            n=(i+1)%segments
            triangles.extend([(a+i,a+n,b+n),(a+i,b+n,b+i)])
    for ring,z in ((0,-120),(4,height)):
        middle=len(vertices);vertices.append((size[0]*.048*math.cos(seed),size[1]*.052*math.sin(seed),z))
        colors.append((.255,.27,.255));a=ring*segments
        for i in range(segments):
            n=(i+1)%segments
            triangles.append((middle,a+n,a+i) if ring == 0 else (middle,a+i,a+n))
    triangles=[(a,c,b) for a,b,c in triangles]
    return vertices,triangles,colors


def author_coast_and_horizon(layout):
    material=terrain_detail_material('M_R5_CoastalStrata')
    for i,row in enumerate(layout['shore_rocks']):
        verts,tris,colors=coastal_rock_geometry(row['size_cm'],row['height_cm'],row['seed'])
        native_mesh('CoastRock_%02d'%i,verts,tris,material,colors,True,
            native_transform((*row['center_xy'],-205),yaw=row['yaw']),'RockCoast')
    for i,row in enumerate(layout['distant_islands']):
        verts,tris,colors=island_geometry(row['size_cm'],row['height_cm'],row['seed'])
        native_mesh('DistantIsland_%d'%i,verts,tris,material,colors,False,
            native_transform((*row['center_xy'],layout['water_z']-20),yaw=row['yaw']),'NonPlayableHorizon')
        # Existing copied tree silhouettes planted into actual island geometry;
        # the source mesh and material remain unchanged, no new nav/collision.
        source_tree=load(COPY_MAP[PROP_SOURCES['tree']]); tree_shape=mesh_bounds(source_tree)
        island_transform=native_transform((*row['center_xy'],layout['water_z']-20),yaw=row['yaw'])
        for j in range(layout['horizon_revision']['trees_per_island']):
            theta=j*2.399963+row['seed']; radius=.30+.32*((j*7%17)/17.)
            x=radius*math.cos(theta);y=radius*math.sin(theta)
            local=vector((row['size_cm'][0]*.5*x,row['size_cm'][1]*.5*y,
                          island_elevation(x,y,radius,row['height_cm'],row['seed'])))
            location=xyz(U.MathLibrary.transform_location(island_transform,local))
            scale=.65+.70*((j*11%19)/19.)
            static_mesh('Island%d_Tree%02d'%(i,j),source_tree,
                place_bounds(tree_shape,location[:2],location[2]-30.,j*51+row['yaw'],(scale,scale,scale)),
                collision=False,group='NonPlayableHorizon')
    row=layout['distant_sailboat']; x,y=row['center_xy']; z=layout['water_z']
    prop('moored_rowboat_geometry','DistantBoatHull',(x,y),z-35,yaw=row['yaw'],scale=1.4,collision=False,group='NonPlayableHorizon')
    wood=load(COPY_MAP[SOURCE_PACK+'/materials/MI_wood_05'])
    static_mesh('DistantBoatMast',load('/Engine/BasicShapes/Cylinder'),native_transform((x,y,z+265),scale=(.10,.10,5.2)),[wood],False,'NonPlayableHorizon')
    sail=simple_material('M_OriginalSail',(.76,.69,.49),two_sided=True)
    verts=[];tris=[]
    for v in range(13):
        t=v/12
        for u in range(17):
            s=u/16; width=310*(1-t)+25
            verts.append((math.sin(math.pi*s)*math.sin(math.pi*t)*65,s*width,95+t*390))
    for v in range(12):
        for u in range(16):
            a=v*17+u;tris.extend([(a,a+1,a+18),(a,a+18,a+17)])
    native_mesh('DistantCurvedSail',verts,tris,sail,collision=False,
        transform=native_transform((x,y,z),yaw=row['yaw']),group='NonPlayableHorizon')
    R['horizon'] = dict(islands=layout['distant_islands'],boat=row,collision=False,
        scope='Original three-dimensional scenery only, no new playable land or navigation region.')


def cut_interior_component(component, name, cuts):
    source=load(component['static_mesh']); dynamic=U.DynamicMesh()
    result=U.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(source,dynamic,
        U.GeometryScriptCopyMeshFromAssetOptions(),U.GeometryScriptMeshReadLOD(),False)
    check('native source mesh copied before private interior cut', result[-1]==U.GeometryScriptOutcomePins.SUCCESS,name)
    for center,size in cuts:
        tool=U.DynamicMesh()
        U.GeometryScript_Primitives.append_box(tool,U.GeometryScriptPrimitiveOptions(),native_transform(center),
            *size,0,0,0,U.GeometryScriptPrimitiveOriginMode.CENTER)
        U.GeometryScript_MeshBooleans.apply_mesh_boolean(dynamic,native_transform(),tool,native_transform(),
            U.GeometryScriptBooleanOperation.SUBTRACT,U.GeometryScriptMeshBooleanOptions())
    materials=[load(BUILDING_MATERIALS.get(package(p),p)) if p else None for p in component['effective_materials']]
    check('interior component source material slots explicit',all(m is not None for m in materials))
    mesh=save_dynamic(name,dynamic,materials,True)
    R.setdefault('interior_geometry_derivatives',[]).append(dict(source=source.get_path_name(),target=mesh.get_path_name(),
        cuts=[dict(center_cm=center,size_cm=size) for center,size in cuts],
        scope='Only this new interior instance; source mesh and all other house instances unchanged. Visual/collision opening validation NOT_RUN.'))
    return mesh


def author_houses(layout,assemblies):
    for row in layout['houses']:
        assembly=assemblies[row['house']]
        transform=place_bounds(assembly['bounds_cm'],row['center_xy'],0,row['yaw'])
        HOUSE_TRANSFORMS[row['role']]=transform
        bounds=transformed_bounds(assembly['bounds_cm'],transform)
        components=[]
        for component in assembly['components']:
            mesh=load(component['static_mesh'])
            if row.get('interior'):
                if component['variable']=='SM_BLD_body_v02_01':
                    mesh=cut_interior_component(component,'SM_InnBodyOpenRoom',[
                        ((0,0,168),(400,420,306)),((0,265,140),(176,200,260))])
                elif component['variable']=='SM_BLD_door_v02_01':
                    mesh=cut_interior_component(component,'SM_InnOpenDoorFrame',[((0,0,140),(176,400,260))])
            final=U.MathLibrary.compose_transforms(template_transform(component['template_to_actor_transform']),transform)
            materials=[load(BUILDING_MATERIALS.get(package(p),p)) if p else None for p in component['effective_materials']]
            actor=static_mesh(row['role']+'_'+component['variable'],mesh,final,materials,True,row['role'])
            tag(actor,'HC_VS2_Rev2_Building')
            actual_bounds=actual_actor_bounds(actor)
            bounds_guard(layout,actual_bounds,actor.get_actor_label(),doors=False)
            components.append(dict(label=actor.get_actor_label(),mesh=mesh.get_path_name(),
                transform=transform_record(final),native_actor_bounds_cm=actual_bounds))
        R['houses'].append(dict(role=row['role'],source_house=row['house'],bounds_cm=bounds,components=components,
            source_blueprint_executed=False,interior=bool(row.get('interior'))))


def add_prop(layout,row):
    if 'source_path' in row:
        mesh=load(COPY_MAP[row['source_path']])
        scale=row.get('scale',1);scale=(scale,)*3 if isinstance(scale,(int,float)) else scale
        transform=place_bounds(mesh_bounds(mesh),row['center_xy'],row.get('ground_z',0),row.get('yaw',0),scale)
        actor=static_mesh(row['name'],mesh,transform,collision=row.get('collision',True),group=row.get('group','StreetLife'))
        bounds=transformed_bounds(mesh_bounds(mesh),transform)
        R['props'].append(dict(label=actor.get_actor_label(),mesh=mesh.get_path_name(),bounds_cm=bounds,role=row.get('group','StreetLife')))
    else:
        actor,bounds=prop(row['role'],row['name'],row['center_xy'],row.get('ground_z',0),row.get('yaw',0),
            row.get('scale',1),row.get('group','StreetLife'),row.get('collision',True))
    bounds_guard(layout,bounds,row['name'],doors=row.get('door_guard',True),corridor=row.get('corridor_guard',True))
    actual_bounds=actual_actor_bounds(actor)
    bounds_guard(layout,actual_bounds,row['name']+' native',doors=row.get('door_guard',True),corridor=row.get('corridor_guard',True))
    R.setdefault('native_prop_bounds',[]).append(dict(label=actor.get_actor_label(),bounds_cm=actual_bounds))
    return actor,bounds


def point_light(name,position,color,intensity,tag_name,radius=550,shadows=False,on_lumens=None):
    actor=spawn(name,U.PointLight,position,group='LocalLighting')
    component=actor.get_component_by_class(U.PointLightComponent)
    component.set_mobility(U.ComponentMobility.MOVABLE)
    component.set_light_color(U.LinearColor(*color,1))
    # Layout artistic source values are lumens; native runtime interface uses
    # candelas explicitly, with the omnidirectional 4*pi conversion once here.
    component.set_editor_property('intensity_units',U.LightUnits.CANDELAS)
    intensity_cd=intensity/(4*math.pi)
    on_cd=(intensity if on_lumens is None else on_lumens)/(4*math.pi)
    component.set_intensity(intensity_cd)
    component.set_attenuation_radius(radius)
    component.set_cast_shadows(shadows)
    actual_cd=float(component.get_editor_property('intensity'))
    actual_radius=float(component.get_editor_property('attenuation_radius'))
    actual_shadows=bool(component.get_editor_property('cast_shadows'))
    check('point-light intensity units and fixed radius/shadow readback',
          abs(actual_cd-intensity_cd)<1.e-5 and actual_radius==radius and actual_shadows==shadows
          and component.get_editor_property('intensity_units')==U.LightUnits.CANDELAS,name)
    tag(actor,tag_name,'HC_VS2_Rev2_OnIntensity=%.8f'%on_cd)
    R.setdefault('local_lights',[]).append(dict(label=actor.get_actor_label(),tag=tag_name,position_cm=position,
        intensity_candelas=actual_cd,on_intensity_candelas=on_cd,
        layout_source_lumens=intensity,units='Candelas',radius_cm=actual_radius,cast_shadows=actual_shadows,
        source_linear_color=color))
    return actor


def author_props(layout):
    for row in layout['props']:
        add_prop(layout,row)
    for row in layout['table_groups']:
        _,bounds=add_prop(layout,dict(name=row['name']+'_Table',role='cafe_table',center_xy=row['center_xy'],yaw=row['yaw'],scale=1))
        z=bounds['max'][2]
        for i,delta in enumerate([(-18,-12),(20,13)]):
            prop('cup',row['name']+'_Cup'+str(i),(row['center_xy'][0]+delta[0],row['center_xy'][1]+delta[1]),z,
                scale=.8,collision=False,group='SupportedTableware')
    # Actual local lights occupy the same source lantern heads. They are zero in
    # the saved afternoon and are staged by the root-owned time-of-day director.
    for row in layout['night_lights']:
        point_light(row['name'],row['position_cm'],row['color'],0.,'HC_VS2_Rev2_NightLight',row['radius_cm'],True,420.)
    for row in layout['window_lights']:
        point_light(row['name'],row['position_cm'],row['color'],0.,'HC_VS2_Rev2_WindowLight',row['radius_cm'],False,90.)


def author_interior(layout):
    room=layout['interior']; transform=HOUSE_TRANSFORMS[room['house_role']]
    wall=load(BUILDING_MATERIALS.get(COPY_MAP[SOURCE_PACK+'/materials/MI_wall_02'],COPY_MAP[SOURCE_PACK+'/materials/MI_wall_02']))
    wood=load(BUILDING_MATERIALS.get(COPY_MAP[SOURCE_PACK+'/materials/MI_wood_05'],COPY_MAP[SOURCE_PACK+'/materials/MI_wood_05']))
    # Complete enclosed room fitted into the new inn; 176 cm doorway is an
    # actual subtraction through its source body and decorative door frame.
    boxes=[('Floor',(0,0,13),(396,420,10),wood),('Ceiling',(0,0,325),(400,420,12),wall),
        ('WestWall',(-202,0,168),(12,420,310),wall),('EastWall',(202,0,168),(12,420,310),wall),
        ('BackWall',(0,-214,168),(416,12,310),wall),
        ('FrontLeft',(-147,214,168),(118,12,310),wall),('FrontRight',(147,214,168),(118,12,310),wall),
        ('DoorHeader',(0,214,296),(176,12,54),wall)]
    for name,center,size,mat in boxes:
        local=native_transform(center,scale=[v/100 for v in size])
        actor=static_mesh('InnInterior_'+name,load('/Engine/BasicShapes/Cube'),
            U.MathLibrary.compose_transforms(local,transform),[mat],True,'Interior')
        tag(actor,'HC_VS2_Rev2_Interior')
    # Native furniture, sized from the same library. Footprint does not obstruct
    # the 176 cm clear entrance or the central room standing/turning space.
    for row in room['furniture']:
        source=PROP_SOURCES[row['role']];mesh=load(COPY_MAP[source]);scale=row.get('scale',1.)
        local=place_bounds(mesh_bounds(mesh),row['center_xy'],18,row.get('yaw',0),(scale,)*3)
        static_mesh('Inn_'+row['name'],mesh,U.MathLibrary.compose_transforms(local,transform),collision=True,group='Interior')
    lumens=room['local_light_lumens']
    check('only two room light intensity overrides',set(lumens)=={'Main','Door'}
          and lumens['Main']==240 and lumens['Door']==140)
    for name,local_pos in [('Main',(0,-30,270)),('Door',(-90,150,215))]:
        pos=xyz(U.MathLibrary.transform_location(transform,vector(local_pos)))
        point_light('InteriorWarm_'+name,pos,[1,.69,.38],lumens[name],'HC_VS2_Rev2_InteriorLight',420,True)
    bounds=transformed_bounds(dict(min=[-208,-220,12],max=[208,220,332]),transform)
    R['interior']=dict(house=room['house_role'],world_bounds_cm=bounds,door_clear_width_cm=176,
        door_clear_height_cm=260,source_materials=[wall.get_path_name(),wood.get_path_name()],
        enclosed=True,real_door_subtraction=True,walking_entry='NOT_RUN',visual='NOT_RUN')


def sky_material(layout):
    mat=AT.create_asset('M_AnimeCloudDome',ASSET_ROOT+'/Materials',U.Material,U.MaterialFactoryNew())
    mat.set_editor_property('shading_model',U.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True);mat.set_editor_property('is_sky',True)
    parameter_defaults=layout['sky_presets']['Afternoon']
    inputs=[]; nodes={}
    for name in ['SkyZenith','SkyHorizon','CloudLight','CloudShade']:
        node=material_node(mat,'VectorParameter',parameter_name=name,default_value=U.LinearColor(*parameter_defaults[name],1))
        item=U.CustomInput();item.set_editor_property('input_name',name);inputs.append(item);nodes[name]=node
    for name in ['CloudCoverage','StarStrength','MoonStrength']:
        node=material_node(mat,'ScalarParameter',parameter_name=name,default_value=parameter_defaults[name])
        item=U.CustomInput();item.set_editor_property('input_name',name);inputs.append(item);nodes[name]=node
    direction=material_node(mat,'Normalize');world=material_node(mat,'WorldPosition');connect(world,'',direction,'Input')
    item=U.CustomInput();item.set_editor_property('input_name','D');inputs.append(item)
    code='''
float h=saturate(D.z); float u=atan2(D.y,D.x)/6.2831853+0.5;
float3 sky=lerp(SkyHorizon,SkyZenith,pow(h,0.55));
float clouds=0.0; float shadeSum=0.0; float shadeWeight=0.0;
// Group coordinates: actual azimuth degrees, base elevation degrees, size.
// The two local tangent axes use the SAME angular metric. R4's normalized
// azimuth and D.z did not, stretching every cloud horizontally by about 2*pi.
const float3 groups[10]={float3(4,23,.80),float3(40,34,.65),
 float3(78,10,.85),float3(114,19,.72),float3(151,9,1.00),
 float3(190,24,.75),float3(229,15,.80),float3(270,29,.95),
 float3(306,47,.80),float3(337,14,.85)};
// Unequal rising lobes, not eight beads on one long horizontal base.
const float4 lobes[7]={float4(-.103,.030,.055,.045),float4(-.052,.083,.070,.078),
 float4(.015,.125,.065,.085),float4(.079,.066,.062,.065),
 float4(.112,.022,.040,.035),float4(.043,.025,.070,.036),float4(-.020,.027,.098,.035)};
for(int g=0;g<10;g++) {
 float az=groups[g].x*.01745329252; float el=groups[g].y*.01745329252;
 float3 center=float3(cos(el)*cos(az),cos(el)*sin(az),sin(el));
 float facing=dot(D,center);
 if(facing<=.90) continue;
 float3 tangent=float3(-sin(az),cos(az),0.);
 float3 up=cross(center,tangent);
 float2 p=float2(dot(D,tangent),dot(D,up))/(facing*groups[g].z);
 float seed=frac(sin((g+1)*13.719)*17341.317);
 p.x*=g%2==0 ? 1. : -1.;
 p.y/=.88+.24*seed;
 float blob=0.; float formSum=0.; float formWeight=0.;
 for(int k=0;k<7;k++) {
  float4 l=lobes[k];
  l.x+=.009*sin(g*1.7+k*2.1); l.y+=.008*sin(g*2.3+k*1.4);
  float2 v=(p-l.xy)/l.zw;
  float ripple=.012*sin(v.x*9.+g*2.1)*sin(v.y*8.+k*1.7);
  float q=length(v)+ripple;
  float lobe=1.-smoothstep(.975,1.015,q);
  blob=max(blob,lobe);
  float depth=sqrt(saturate(1.-dot(v,v)));
  float3 normal=normalize(float3(v,depth+.0001));
  float illumination=.12+.78*saturate(dot(normal,normalize(float3(-.48,.67,.56))));
  // Continuous weighted overlap avoids the R3 winning-lobe hard seams.
  float weight=lobe*(.025+pow(depth,4.)*2.0);
  formSum+=weight*illumination; formWeight+=weight;
 }
 blob*=smoothstep(-.005,.009,p.y)*saturate(CloudCoverage);
 float upper=smoothstep(.008,.078,p.y);
 float groupShade=lerp(.055,formSum/max(formWeight,.00001),upper);
 clouds=max(clouds,blob);
 shadeSum+=blob*groupShade;shadeWeight+=blob;
}
float shade=saturate(shadeSum/max(shadeWeight,0.000001));
// Preserve the authored CloudLight/CloudShade time-of-day interface; retain
// the bottom and inner-lobe midtones rather than saturating them to white.
float3 cloudColor=lerp(CloudShade*.86,CloudLight,shade);
float3 result=lerp(sky,cloudColor,saturate(clouds));
float2 starUV=float2(u*1300,h*750);float2 cell=floor(starUV);
float hash=frac(sin(dot(cell,float2(12.9898,78.233)))*43758.5453);
// Jitter locations within cells: R3 centered every star on the same grid and
// produced visible rows. Density and strength remain bounded.
float2 jitter=.18+.64*frac(sin(float2(dot(cell,float2(39.31,17.11)),dot(cell,float2(11.71,43.17))))*18537.13);
float star=(1.0-smoothstep(.06,.16,length(frac(starUV)-jitter)))*step(.994,hash)*smoothstep(.05,.35,h);
result+=star*StarStrength*(1.-clouds)*float3(.7,.8,1.);
float3 moonDir=normalize(float3(-.42,.78,.46));
float moon=smoothstep(.99982,.9999,dot(D,moonDir))*MoonStrength;
result+=moon*(1.-clouds)*float3(.83,.89,1.);
return max(result,0.0);
'''
    custom=material_node(mat,'Custom',inputs=inputs,code=code,output_type=U.CustomMaterialOutputType.CMOT_FLOAT3)
    for name,node in nodes.items():connect(node,'',custom,name)
    connect(direction,'',custom,'D')
    check('procedural sky emissive output',E.connect_material_property(custom,'',U.MaterialProperty.MP_EMISSIVE_COLOR))
    E.recompile_material(mat);save_owned(mat)
    R['r5_cloud_shader']=dict(material=mat.get_path_name(),hlsl_sha256=hashlib.sha256(code.encode('utf-8')).hexdigest(),
        groups=10,lobes_per_group=7,depth_model='Equal-angle sphere tangent frames with analytic lobe normals; not volumetric cloud scattering',
        changes='Separated 2D lobe stacks at varied elevations, equal horizontal/vertical metric, nonsaturating inner shade; unchanged star/moon interface',
        source_texture=None,render_status='NOT_RUN')
    return mat


def water_snapshot(material):
    values={}
    for kind in ('scalar','vector','texture','static_switch'):
        names=getattr(E,'get_'+kind+'_parameter_names')(material)
        getter=getattr(E,'get_material_instance_'+kind+'_parameter_value')
        check('bounded water parameter count',len(names)<=64,kind)
        values[kind]={}
        for name in names:
            value=getter(material,name)
            if kind=='vector':value=[float(getattr(value,c)) for c in ('r','g','b','a')]
            elif kind=='texture':value=value.get_path_name() if value else None
            elif kind=='scalar':value=float(value)
            else:value=bool(value)
            values[kind][str(name)]=value
    return dict(parent=material.get_editor_property('parent').get_path_name(),parameters=values,
                base_overrides=material.get_editor_property('base_property_overrides').export_text())


def water_candidate(layout):
    source=load(SELECTED_WATER);before=water_snapshot(source)
    target=ASSET_ROOT+'/Materials/MI_WaterBoundedShallow'
    check('unique water derivative absent',not A.does_asset_exist(target))
    water=A.duplicate_asset(source.get_path_name(),target)
    check('private selected-water derivative',water is not None and water != source)
    settings=layout['water_revision'];p=before['parameters']
    shallow=p['vector']['Water Color Shallow'];deep=p['vector']['Water Color Deep']
    weight=settings['shallow_original_weight']
    check('exact bounded water change whitelist',weight==.2
          and settings['scalar_overrides']=={'Caustics Intensity':2.,'Fade Distance':40.})
    desired=[deep[i]+(shallow[i]-deep[i])*weight for i in range(3)]+[shallow[3]]
    vector_return=E.set_material_instance_vector_parameter_value(
        water,U.Name('Water Color Shallow'),U.LinearColor(*desired))
    actual_vector=E.get_material_instance_vector_parameter_value(water,U.Name('Water Color Shallow'))
    readback=[float(getattr(actual_vector,c)) for c in ('r','g','b','a')]
    R['water_setter_readback']=[dict(parameter='Water Color Shallow',native_return=vector_return,
                                   desired=desired,actual=readback)]
    check('native shallow setter actual float32 readback',vector_return is False
          and max(abs(a-b) for a,b in zip(readback,desired))<1.e-7,R['water_setter_readback'][-1])
    for name,value in settings['scalar_overrides'].items():
        scalar_return=E.set_material_instance_scalar_parameter_value(water,U.Name(name),value)
        readback=float(E.get_material_instance_scalar_parameter_value(water,U.Name(name)))
        R['water_setter_readback'].append(dict(parameter=name,native_return=scalar_return,desired=value,actual=readback))
        check('native local water scalar actual exact readback',scalar_return is False and readback==value,R['water_setter_readback'][-1])
    E.update_material_instance(water);save_owned(water)
    after=water_snapshot(water)
    check('water parent and base overrides remain byte-equivalent',before['parent']==after['parent']
          and before['base_overrides']==after['base_overrides'])
    differences=[]
    for kind,entries in p.items():
        check('water parameter names unchanged',entries.keys()==after['parameters'][kind].keys(),kind)
        for name,value in entries.items():
            actual=after['parameters'][kind][name]
            if actual != value:differences.append(kind+':'+name)
            if kind=='scalar' and name in settings['scalar_overrides']:
                check('exact water scalar readback',actual==settings['scalar_overrides'][name],name)
            elif kind=='vector' and name=='Water Color Shallow':
                check('float32 shallow blend readback',max(abs(a-b) for a,b in zip(actual,desired))<1.e-7)
            else:check('unrelated water parameter unchanged',actual==value,kind+':'+name)
    check('exact three water fields changed',set(differences)=={'scalar:Caustics Intensity','scalar:Fade Distance','vector:Water Color Shallow'})
    check('source water object untouched',water_snapshot(source)==before)
    R['water_revision']=dict(source=source.get_path_name(),candidate=water.get_path_name(),before=before,after=after,
        changed_fields=differences,scope='Native material instance readback only; water/shallow/foam visuals NOT_RUN.')
    return water


def author_environment(layout):
    water=water_candidate(layout)
    sea=static_mesh('SeaSurface',load('/Engine/BasicShapes/Plane'),native_transform((0,0,layout['water_z']),scale=(10000,10000,1)),[water],False,'NonPlayableSea')
    sea.get_component_by_class(U.StaticMeshComponent).set_cast_shadow(False)
    dome=static_mesh('AnimeCloudSkyDome',load('/Engine/BasicShapes/Sphere'),native_transform((0,0,0),scale=(20000,20000,20000)),
        [sky_material(layout)],False,'Sky')
    dome.get_component_by_class(U.StaticMeshComponent).set_cast_shadow(False);tag(dome,'HC_VS2_Rev2_SkyDome')
    atmosphere=spawn('SkyAtmosphere',U.SkyAtmosphere,group='Sky')
    component=atmosphere.get_component_by_class(U.SkyAtmosphereComponent)
    for key,value in layout['atmosphere'].items():
        component.set_editor_property(key,U.LinearColor(*value) if key=='sky_luminance_factor' else value)
    preset=layout['lighting_presets']['Afternoon']
    for name,pitch,yaw,color,intensity,index in [('Sun',preset['sun_pitch'],preset['sun_yaw'],preset['sun_color'],preset['sun_intensity'],0),
                                              ('Moon',preset['moon_pitch'],preset['moon_yaw'],preset['moon_color'],0.,1)]:
        actor=spawn(name,U.DirectionalLight,(0,0,2000),U.Rotator(pitch=pitch,yaw=yaw),'Lighting')
        light=actor.get_component_by_class(U.DirectionalLightComponent);light.set_mobility(U.ComponentMobility.MOVABLE)
        light.set_light_color(U.LinearColor(*color,1));light.set_intensity(intensity)
        light.set_editor_property('atmosphere_sun_light',True);light.set_editor_property('atmosphere_sun_light_index',index)
        light.set_editor_property('light_source_angle',1.5 if index==0 else .8)
        tag(actor,'HC_VS2_Rev2_'+name)
    actor=spawn('SkyLight',U.SkyLight,(0,0,2000),group='Lighting');sky=actor.get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE);sky.set_editor_property('real_time_capture',True)
    sky.set_editor_property('lower_hemisphere_is_black',False);sky.set_intensity(preset['sky_intensity']);tag(actor,'HC_VS2_Rev2_SkyLight')
    post=spawn('FixedExposure',U.PostProcessVolume,group='Lighting');post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    for key,value in dict(override_auto_exposure_method=True,auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True,auto_exposure_bias=preset['exposure_bias'],
        override_motion_blur_amount=True,motion_blur_amount=0.,override_bloom_intensity=True,bloom_intensity=.08).items():
        settings.set_editor_property(key,value)
    post.set_editor_property('settings',settings);tag(post,'HC_VS2_Rev2_PostProcess')
    R['lighting']=dict(saved_preset='Afternoon',presets=layout['lighting_presets'],sky_presets=layout['sky_presets'],
        sky_policy='Original procedural anime-cumulus full sphere. Runtime time-of-day MID/light staging is root-owned.',
        atmosphere_actual=atmosphere_readback(component),volume_clouds=0,
        technical_scope='Saved lighting candidate only; actual dusk/night/indoors/air render and performance NOT_RUN')


def author_people_and_cameras(layout,specimens):
    feet=layout['player_start_feet_cm']
    spawn('PlayerStart',U.PlayerStart,(feet[0],feet[1],feet[2]+R['hero_binding']['capsule_half_height_cm']+4),
        U.Rotator(yaw=layout['player_start_yaw']),'Gameplay')
    region=spawn('AllowedLandRegion',U.HCM3NavRegion,(-350,0,150),group='Gameplay')
    region.set_editor_property('stable_id',U.Name('M5VS2_CornerV2_AllowedLand'));region.set_editor_property('half_extent',U.Vector(2000,2300,350))
    for post in layout['npc_posts']:
        spec=specimens[post['sample']];feet=post['feet_cm']
        actor=spawn('NPC_'+post['sample']+'_'+post['role'],generated_class(spec['blueprint']),
            (feet[0],feet[1],feet[2]+float(spec['capsule_half_height_cm'])+2),U.Rotator(yaw=post['yaw']),'NamedNPC')
        actor.set_editor_property('stationary',True);actor.set_editor_property('navigation_region',region)
        actor.set_editor_property('stable_id',U.Name('M5VS2_CornerV2_NPC_'+post['sample']))
        R['npc_bindings'].append(dict(sample=post['sample'],actor=actor.get_actor_label(),blueprint=spec['blueprint'],
            scope='Existing Q/R unchanged; eight-person revision-two cast integration remains separate root work.'))
    R['capture_camera_plan']=[]
    for row in layout['cameras']:
        if row.get('house_local'):
            transform=HOUSE_TRANSFORMS[row['house_local']]
            location=xyz(U.MathLibrary.transform_location(transform,vector(row['location_cm'])))
            target=xyz(U.MathLibrary.transform_location(transform,vector(row['target_cm'])))
        else:location,target=row['location_cm'],row['target_cm']
        delta=[target[i]-location[i] for i in range(3)]
        rotation=U.Rotator(pitch=math.degrees(math.atan2(delta[2],math.hypot(delta[0],delta[1]))),yaw=math.degrees(math.atan2(delta[1],delta[0])))
        actor=spawn('ReviewCamera_'+row['name'],U.CameraActor,location,rotation,'ReviewCamera')
        actor.get_component_by_class(U.CameraComponent).set_field_of_view(row['fov']);tag(actor,'HC_VS2_Rev2_Camera_'+row['name'])
        R['capture_camera_plan'].append(dict(name=row['name'],location_cm=location,target_cm=target,fov=row['fov'],actor=actor.get_actor_label(),screenshot='NOT_RUN'))
    bounds=spawn('FlightBounds',U.HCM5VS2FlightBounds,group='Gameplay')
    fields=dict(enable_flight=True,sea_level_z=layout['water_z'],flight_area_center=U.Vector2D(0,0),
        flight_area_half_extent=U.Vector2D(2500,2500),soft_return_distance=10000.,ceiling_height=10000.,water_clearance=50.)
    for key,value in fields.items():bounds.set_editor_property(key,value)
    interior=R['interior']['world_bounds_cm']
    no_flight=U.Box(min=vector(interior['min']),max=vector(interior['max']))
    check('saved no-takeoff box is valid',bool(no_flight.get_editor_property('is_valid')))
    bounds.set_editor_property('no_takeoff_volumes',[no_flight]);tag(bounds,'HC_VS2_Rev2_FlightBounds')
    R['flight_bounds']=dict(sea_level_z=layout['water_z'],center_xy=[0,0],half_extent_xy=[2500,2500],
        soft_return_distance=10000,ceiling_height=10000,water_clearance=50,no_takeoff_bounds_cm=interior,
        actual_class=bounds.get_class().get_path_name(),runtime='NOT_RUN')


try:
    dump()
    check('exact bounded author phase',PHASE in ('CornerVTwoProbe','CornerVTwo'))
    check('actual HarborCity project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or initially dirty maps',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    layout,assemblies,specimens,mode=resolve_inputs()
    probe_apis()
    if PHASE=='CornerVTwo':
        check('unique native map and asset namespace absent',not A.does_directory_exist(ASSET_ROOT) and not disk(MAP,'.umap').exists())
        check('create unique native map',LEVEL.new_level(MAP,False))
        world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        A.set_metadata_tag(world,KEY,OWNER);world.get_world_settings().set_editor_property('default_game_mode',mode)
        author_street(layout);author_houses(layout,assemblies);author_props(layout)
        author_coast_and_horizon(layout);author_interior(layout);author_environment(layout)
        author_people_and_cameras(layout,specimens)
        director_class=layout.get('review_director_class')
        if director_class:
            check('root-owned review director reflected',hasattr(U,director_class))
            spawn('ReviewDirector',getattr(U,director_class),group='ReviewCamera')
        check('seven complete house instances and two existing NPC bindings',len(R['houses'])==7 and len(R['npc_bindings'])==2)
        labels=sorted(a.get_actor_label() for a in CREATED)
        check('save actual native new map',LEVEL.save_current_level())
        R['assets'].append(dict(path=MAP,sha256=sha(disk(MAP,'.umap'))))
        check('reload saved unique map',LEVEL.load_level(MAP))
        saved=[a for a in ACT.get_all_level_actors() if a.actor_has_tag(U.Name(OWNER))]
        check('every authored actor persisted exactly once',sorted(a.get_actor_label() for a in saved)==labels)
        for name in ('Sun','Moon','SkyLight','PostProcess','SkyDome','FlightBounds'):
            check('single persisted runtime binding',sum(a.actor_has_tag(U.Name('HC_VS2_Rev2_'+name)) for a in saved)==1,name)
        check('old review director absent',not any(a.get_class().get_name()=='HCM5VS2CornerReviewDirector' for a in saved))
        R['actor_count']=len(saved)
        R['pass_scope']='Native authored assets/map and persistence only. Actual rendering, traversal, collisions, flight, shader fallbacks and art acceptance NOT_RUN.'
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['protected_input_file_count']=len(PROTECTED)
    R['source_preservation']=dict(files=len(PROTECTED),changed=changed)
    R['checks'].append(dict(name='all source, Hero, NPC, previous maps and config preserved',
                            status='FAIL' if changed else 'PASS',observed=R['source_preservation']))
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('CornerVTwo failed; all original failure evidence and partial unique packages retained')
