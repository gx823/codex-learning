"""Author only the isolated 50m VS2 harbor corner from an actual PASS Village Copy.

Root-owned execution:
  ue_m5_vs2_author_run.ps1 -Phase HarborCorner -ScriptPath this_file
The latest successful Village Copy, Q/R integrations and subsequent PHYS repair
are resolved and byte-checked before any native asset or map is created. Optional
-M5VillageCopy=<VS2 author_result.json> pins the copy report explicitly.
No source Blueprint, construction script, demo map, game launch, build or capture.
"""
from pathlib import Path
import datetime as dt
import hashlib
import itertools
import json
import math
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
LAYOUT_FILE = DOC / 'research/HARBOR_CORNER_P0_LAYOUT.json'
SOURCE_PACK = '/Game/Fantastic_Village_Pack'
ROOT = '/Game/HarborCity/M5VS2/World'
MAP = ROOT + '/L_AnimeHarbor_Corner_P0'
OWNER = 'HarborCity_M5_VS2_HarborCorner_P0'
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


def argument(name, default=None):
    values = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', CMD)
    if not values:
        return default
    assert len(values) == 1, 'Duplicate argument ' + name
    return values[0][0] or values[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and not (OUT / 'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
# Every attempt gets new dedicated geometry/material packages. Only the fixed
# owned map can be rebuilt, and only when its last successful bytes still match.
ASSET_ROOT = ROOT + '/CornerP0_' + hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
R = dict(status='RUNNING', scope='NATIVE_50M_CORNER_AUTHOR_ONLY', map=MAP,
         owner=OWNER, dedicated_assets=ASSET_ROOT, checks=[], assets=[], inputs={},
         actors=[], houses=[], props=[], npc_bindings=[], source_blueprints_spawned=0,
         runtime='NOT_RUN', visual='NOT_RUN_NULLRHI', user_art_approval='USER_REVIEW',
         interiors='NOT_RUN_NOT_AUTHORED', navigation='NOT_RUN_NO_NAVMESH_BAKE',
         walking_driving_collision='NOT_RUN', injury_and_vehicle_contact='NOT_RUN',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}
CACHE = {}
CREATED = []
COPY_MAP = {}
PROP_SOURCES = {}
BUILDING_MATERIALS = {}
SELECTED_WATER = None


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, condition, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if condition else 'FAIL', observed=observed))
    dump()
    if not condition:
        raise RuntimeError(name + ': ' + str(observed))


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def package(path):
    return str(path).split('.')[0]


def disk(path, extension='.uasset'):
    path = package(path)
    assert path.startswith('/Game/') and '..' not in path
    return PROJECT / 'Content' / Path(path.removeprefix('/Game/')).with_suffix(extension)


def document(path):
    path = Path(path).resolve()
    check('input document within VS2 evidence', path.is_relative_to(DOC) and path.is_file(), str(path))
    return json.loads(path.read_text(encoding='utf-8-sig'))


def binding(path):
    return dict(path=str(path), sha256=sha(path))


def latest_pass(pattern, predicate=lambda x: True):
    for file in sorted((DOC / 'editor_runtime').glob(pattern + '/author_result.json'),
                       key=lambda p: p.stat().st_mtime, reverse=True):
        value = json.loads(file.read_text(encoding='utf-8-sig'))
        if value.get('status') == 'PASS' and predicate(value):
            return file, value
    raise RuntimeError('No actual PASS report: ' + pattern)


def protect_file(path, expected=None):
    path = Path(path)
    check('protected input exists', path.is_file(), str(path))
    actual = sha(path)
    if expected:
        check('input matches native saved evidence', actual == expected, str(path))
    PROTECTED[str(path)] = actual


def load(path):
    path = package(path)
    if path not in CACHE:
        result = A.load_asset(path)
        check('load native asset', result is not None, path)
        CACHE[path] = result
    return CACHE[path]


def generated_class(path):
    path = package(path)
    result = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('load owned generated class', result is not None, path)
    return result


def vector(values):
    return U.Vector(*values)


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def native_transform(location=(0, 0, 0), yaw=0, scale=(1, 1, 1)):
    return U.Transform(location=vector(location), rotation=U.Rotator(yaw=yaw), scale=vector(scale))


def template_transform(row):
    result = U.Transform()
    # Actual SCS quaternion, not an Euler round-trip or a reconstructed pivot.
    result.set_editor_property('translation', vector(row['location_cm']))
    result.set_editor_property('rotation', U.Quat(*row['quaternion_xyzw']))
    result.set_editor_property('scale3d', vector(row['scale']))
    return result


def transform_record(transform):
    translation = transform.get_editor_property('translation')
    rotation = transform.get_editor_property('rotation')
    return dict(location_cm=xyz(translation), quaternion_xyzw=[float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w)],
                scale=xyz(transform.get_editor_property('scale3d')))


def transformed_bounds(bounds, transform):
    corners = [xyz(U.MathLibrary.transform_location(transform, vector(p)))
               for p in itertools.product(*zip(bounds['min'], bounds['max']))]
    low = [min(p[i] for p in corners) for i in range(3)]
    high = [max(p[i] for p in corners) for i in range(3)]
    return dict(min=low, max=high, size=[high[i] - low[i] for i in range(3)])


def place_bounds(bounds, center_xy, ground_z, yaw=0, scale=(1, 1, 1)):
    rotated = transformed_bounds(bounds, native_transform(yaw=yaw, scale=scale))
    offset = [center_xy[i] - (rotated['min'][i] + rotated['max'][i]) * .5 for i in range(2)]
    return native_transform((*offset, ground_z - rotated['min'][2]), yaw, scale)


def save_owned(obj):
    path = package(obj.get_path_name())
    check('only dedicated new asset save', path.startswith(ASSET_ROOT + '/'), path)
    A.set_metadata_tag(obj, KEY, OWNER)
    check('native dedicated asset save', A.save_loaded_asset(obj, False), path)
    R['assets'].append(dict(path=path, sha256=sha(disk(path))))


def spawn(label, kind, position=(0, 0, 0), rotation=None, group=''):
    check('bounded actor count', len(CREATED) < 400)
    actor = ACT.spawn_actor_from_class(kind, vector(position), rotation or U.Rotator(), False)
    check('native actor spawn', actor is not None, label)
    actor.set_actor_label('HC_M5VS2_Corner_' + label)
    actor.set_editor_property('tags', [U.Name(OWNER), U.Name('CornerGroup_' + group)])
    CREATED.append(actor)
    R['actors'].append(dict(label=actor.get_actor_label(), class_path=actor.get_class().get_path_name(), group=group))
    return actor


def static_mesh(label, mesh, transform, materials=None, collision=True, group=''):
    actor = spawn(label, U.StaticMeshActor, group=group)
    body = actor.get_component_by_class(U.StaticMeshComponent)
    body.set_mobility(U.ComponentMobility.STATIC)
    check('static mesh native binding', body.set_static_mesh(mesh), mesh.get_path_name())
    for index, material in enumerate(materials or []):
        if material is not None:
            body.set_material(index, material)
    body.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    actor.set_actor_transform(transform, False, True)
    observed = actor.get_actor_transform()
    # Three independent basis points catch position/rotation/scale mistakes.
    error = max(math.dist(xyz(U.MathLibrary.transform_location(transform, vector(p))),
                          xyz(U.MathLibrary.transform_location(observed, vector(p))))
                for p in ((0, 0, 0), (100, 0, 0), (0, 100, 100)))
    check('saved actor transform readback under 0.02cm', error < .02, dict(label=label, error_cm=error))
    return actor


def mesh_bounds(mesh):
    box = mesh.get_bounding_box()
    return dict(min=xyz(box.min), max=xyz(box.max))


def prop(role, label, center_xy, ground_z=0, yaw=0, scale=1, group='', collision=True):
    source = PROP_SOURCES[role]
    mesh = load(COPY_MAP[source])
    check('selected prop is native mesh', isinstance(mesh, U.StaticMesh), source)
    shape_scale = (scale, scale, scale) if isinstance(scale, (float, int)) else scale
    transform = place_bounds(mesh_bounds(mesh), center_xy, ground_z, yaw, shape_scale)
    actor = static_mesh(label, mesh, transform, collision=collision, group=group)
    bounds = transformed_bounds(mesh_bounds(mesh), transform)
    R['props'].append(dict(label=actor.get_actor_label(), role=role, source=source, mesh=mesh.get_path_name(),
                           transform=transform_record(transform), bounds_cm=bounds, collision_requested=collision))
    return actor, bounds


def box(label, center, size, material, group='', collision=True):
    return static_mesh(label, load('/Engine/BasicShapes/Cube'),
                       native_transform(center, scale=[v / 100 for v in size]), [material], collision, group)


def check_added_placement(layout, bounds, label):
    # Conservative full-AABB radius keeps added foliage/props out of the whole
    # six-metre road AND both two-metre sidewalks, not only out of the centre.
    center = [(bounds['min'][i]+bounds['max'][i])*.5 for i in (0, 1)]
    radius = math.hypot(*(bounds['max'][i]-bounds['min'][i] for i in (0, 1)))*.5
    points = road_centerline(layout['road']['points_xy'], layout['road']['samples_per_segment'])
    distances = []
    for a, b in zip(points, points[1:]):
        delta = [b[i]-a[i] for i in (0, 1)]
        t = max(0., min(1., sum((center[i]-a[i])*delta[i] for i in (0, 1))/sum(v*v for v in delta)))
        distances.append(math.dist(center, [a[i]+t*delta[i] for i in (0, 1)]))
    clearance = min(distances)-radius
    required = layout['road']['width_cm']*.5+layout['road']['sidewalk_width_cm']
    check('added detail outside complete street corridor', clearance >= required+2,
          dict(label=label, conservative_clearance_cm=clearance, required_cm=required+2))
    check('added detail bounds remain inside 50m land',
          bounds['min'][0] >= -2500 and bounds['max'][0] <= layout['land_max_x']
          and bounds['min'][1] >= -2500 and bounds['max'][1] <= 2500, dict(label=label, bounds=bounds))
    for zone in layout.get('door_clearance_zones', []):
        overlap = all(bounds['max'][i] > zone['min_xy'][i] and bounds['min'][i] < zone['max_xy'][i]
                      for i in (0, 1))
        check('added detail preserves inspected door approach', not overlap,
              dict(label=label, door=zone['name'], zone=zone, bounds=bounds))


def material_node(material, class_name, **properties):
    expression = E.create_material_expression(material, getattr(U, 'MaterialExpression' + class_name), 0, 0)
    check('create native material expression', expression is not None, class_name)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def connect(source, output, target, pin):
    reflected = [str(name) for name in E.get_material_expression_input_names(target)]
    if pin == 'Input' and pin not in reflected and len(reflected) == 1:
        R.setdefault('native_pin_aliases', []).append(dict(node=target.get_class().get_name(), requested=pin, actual=reflected[0]))
        pin = reflected[0]
    check('native material connection', E.connect_material_expressions(source, output, target, pin), pin)


def planting_mask_code(islands):
    # A bounded, slightly irregular ellipse; the ripple only contracts its
    # support, so no pixel can leave the declared full-size AABB. Blending
    # happens on the existing solid ground, without extra planes or collision.
    lines = ['float Coverage = 0.0;']
    for index, island in enumerate(islands):
        x, y = island['center_xy']; rx, ry = [v*.5 for v in island['size_cm']]
        lines += [
            'float2 Q%d = (XY-float2(%.8f,%.8f))/float2(%.8f,%.8f);' % (index, x, y, rx, ry),
            'float D%d = length(Q%d)+0.04*(0.5+0.5*sin(Q%d.x*11.0)*sin(Q%d.y*9.0));' % (index, index, index, index),
            'Coverage = max(Coverage,1.0-smoothstep(0.82,1.0,D%d));' % index]
    return '\n'.join(lines + ['return Coverage;'])


def surface_material(name, source_base_texture, source_normal_texture, period_cm,
                     normal_flatness=0., planting_islands=(), base_color_adjustment=None):
    # Original copied hand-painted textures, with a world XY metric to prevent a
    # 50m primitive from stretching one texture tile across the entire street.
    texture = load(COPY_MAP[source_base_texture])
    normal_texture = load(COPY_MAP[source_normal_texture])
    material = AT.create_asset(name, ASSET_ROOT + '/Materials', U.Material, U.MaterialFactoryNew())
    check('create new horizontal surface material', material is not None)
    position = material_node(material, 'WorldPosition')
    mask = material_node(material, 'ComponentMask', r=True, g=True, b=False, a=False)
    divide = material_node(material, 'Divide')
    period = material_node(material, 'Constant', r=period_cm)
    connect(position, '', mask, 'Input'); connect(mask, '', divide, 'A'); connect(period, '', divide, 'B')
    color = material_node(material, 'TextureSample', texture=texture)
    normal = material_node(material, 'TextureSample', texture=normal_texture, sampler_type=U.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    connect(divide, '', color, 'UVs'); connect(divide, '', normal, 'UVs')
    color_output, normal_output = color, normal
    color_pin, normal_pin = 'RGB', 'RGB'
    if base_color_adjustment:
        # Same sampled gravel detail and per-pixel luminance; this only reduces
        # the source's orange chroma. No texture edit, UV scale or light change.
        check('bounded gravel-only chroma candidate',
              source_base_texture == SOURCE_PACK + '/textures/T_ENV_TERRAIN_gravel_BC'
              and set(base_color_adjustment) == {'saturation', 'channel_tint'}
              and .5 <= base_color_adjustment['saturation'] <= 1
              and len(base_color_adjustment['channel_tint']) == 3
              and all(.85 <= v <= 1.15 for v in base_color_adjustment['channel_tint']))
        custom_input = U.CustomInput(); custom_input.set_editor_property('input_name', 'SourceColor')
        code = ('float3 W=float3(0.2126,0.7152,0.0722);\n'
                'float L=dot(SourceColor,W);\n'
                'float3 C=lerp(L.xxx,SourceColor,%.8f)*float3(%.8f,%.8f,%.8f);\n'
                'return C*(L/max(dot(C,W),0.00001));' %
                (base_color_adjustment['saturation'], *base_color_adjustment['channel_tint']))
        color_output = material_node(material, 'Custom', inputs=[custom_input], code=code,
                                     output_type=U.CustomMaterialOutputType.CMOT_FLOAT3)
        connect(color, 'RGB', color_output, 'SourceColor'); color_pin = ''
    if normal_flatness:
        flat = material_node(material, 'Constant3Vector', constant=U.LinearColor(0, 0, 1, 0))
        flatten = material_node(material, 'LinearInterpolate', const_alpha=normal_flatness)
        connect(normal, 'RGB', flatten, 'A'); connect(flat, '', flatten, 'B')
        normal_output = material_node(material, 'Normalize')
        connect(flatten, '', normal_output, 'Input'); normal_pin = ''
    if planting_islands:
        grass_color = material_node(material, 'TextureSample',
            texture=load(COPY_MAP[SOURCE_PACK + '/textures/T_ENV_TERRAIN_grass_01_BC']))
        grass_normal = material_node(material, 'TextureSample',
            texture=load(COPY_MAP[SOURCE_PACK + '/textures/T_ENV_TERRAIN_grass_01_N']),
            sampler_type=U.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        connect(divide, '', grass_color, 'UVs'); connect(divide, '', grass_normal, 'UVs')
        custom_input = U.CustomInput(); custom_input.set_editor_property('input_name', 'XY')
        coverage = material_node(material, 'Custom', inputs=[custom_input],
            code=planting_mask_code(planting_islands), output_type=U.CustomMaterialOutputType.CMOT_FLOAT1)
        connect(mask, '', coverage, 'XY')
        blended_color = material_node(material, 'LinearInterpolate')
        connect(color_output, color_pin, blended_color, 'A'); connect(grass_color, 'RGB', blended_color, 'B')
        connect(coverage, '', blended_color, 'Alpha')
        blended_normal = material_node(material, 'LinearInterpolate')
        connect(normal_output, normal_pin, blended_normal, 'A'); connect(grass_normal, 'RGB', blended_normal, 'B')
        connect(coverage, '', blended_normal, 'Alpha')
        normal_output = material_node(material, 'Normalize'); connect(blended_normal, '', normal_output, 'Input')
        color_output, color_pin, normal_pin = blended_color, '', ''
    roughness = material_node(material, 'Constant', r=.88)
    for node, output, property_name in ((color_output, color_pin, U.MaterialProperty.MP_BASE_COLOR),
                                       (normal_output, normal_pin, U.MaterialProperty.MP_NORMAL),
                                       (roughness, '', U.MaterialProperty.MP_ROUGHNESS)):
        check('native surface material output', E.connect_material_property(node, output, property_name))
    E.recompile_material(material); save_owned(material)
    R.setdefault('surface_materials', []).append(dict(path=material.get_path_name(), source_base=source_base_texture,
        source_normal=source_normal_texture, texture_period_cm=period_cm, normal_flatness=normal_flatness,
        base_color_adjustment=base_color_adjustment,
        planting_islands=list(planting_islands), planting_mask_hlsl=planting_mask_code(planting_islands) if planting_islands else None,
        ground_policy='Existing opaque solid surface only; no WPO, opacity, added ground geometry or collision changes.',
        scope='Dedicated DefaultLit horizontal-surface derivative using unchanged same-pack texture assets; shader/render quality NOT_RUN.'))
    return material


def strip_mesh(name, polygon, bottom_z, height, material):
    # UE 5.8 MeshPrimitiveFunctions.h explicitly requires CCW, without repeated end.
    area2 = sum(polygon[i][0] * polygon[(i + 1) % len(polygon)][1] - polygon[(i + 1) % len(polygon)][0] * polygon[i][1]
                for i in range(len(polygon)))
    if area2 < 0:
        polygon = list(reversed(polygon))
    check('bounded nondegenerate continuous strip', 4 <= len(polygon) <= 128 and abs(area2) > 1000)
    dynamic = U.DynamicMesh()
    U.GeometryScript_Primitives.append_simple_extrude_polygon(dynamic, U.GeometryScriptPrimitiveOptions(),
        native_transform((0, 0, bottom_z)), [U.Vector2D(*p) for p in polygon], height, 0, True, U.GeometryScriptPrimitiveOriginMode.BASE)
    options = U.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property('enable_recompute_tangents', True)
    options.set_editor_property('enable_collision', True)
    options.set_editor_property('collision_mode', U.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh, outcome = U.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic, ASSET_ROOT + '/Geometry/' + name, options)
    check('native continuous strip mesh creation', outcome == U.GeometryScriptOutcomePins.SUCCESS and mesh is not None, name)
    mesh.set_material(0, material)
    save_owned(mesh)
    static_mesh(name, mesh, native_transform(), [material], group='StreetStructure')
    return polygon


def road_centerline(points, steps):
    # Cubic Hermite through the measured layout points; endpoint tangents lie
    # along +Y so both street and sidewalks meet the exact 50m stage boundary.
    points = [tuple(p) for p in points]
    tangents = [(0, points[1][1] - points[0][1])]
    tangents += [tuple((points[i + 1][a] - points[i - 1][a]) * .5 for a in range(2)) for i in range(1, len(points) - 1)]
    tangents.append((0, points[-1][1] - points[-2][1]))
    result = []
    for i in range(len(points) - 1):
        for j in range(steps):
            t = j / steps
            weights = (2*t**3 - 3*t*t + 1, t**3 - 2*t*t + t, -2*t**3 + 3*t*t, t**3 - t*t)
            result.append(tuple(weights[0]*points[i][a] + weights[1]*tangents[i][a] + weights[2]*points[i+1][a] + weights[3]*tangents[i+1][a] for a in range(2)))
    return result + [points[-1]]


def offset_line(points, distance):
    directions = []
    for a, b in zip(points, points[1:]):
        length = math.dist(a, b)
        directions.append(((b[0]-a[0])/length, (b[1]-a[1])/length))
    result = []
    for i, point in enumerate(points):
        before = directions[max(0, i-1)]; after = directions[min(i, len(directions)-1)]
        n0, n1 = (-before[1], before[0]), (-after[1], after[0])
        length = math.hypot(n0[0]+n1[0], n0[1]+n1[1])
        bisector = ((n0[0]+n1[0])/length, (n0[1]+n1[1])/length)
        denominator = bisector[0]*n1[0] + bisector[1]*n1[1]
        assert denominator > .9, 'Street turn too sharp for bounded miter'
        result.append((point[0]+distance*bisector[0]/denominator, point[1]+distance*bisector[1]/denominator))
    # Exact flat cuts at north/south edges preserve both 2m sidewalks.
    result[0] = (points[0][0]-distance, points[0][1])
    result[-1] = (points[-1][0]-distance, points[-1][1])
    return result


def accept_npc_getup_transition(letter, spec, integration_file, expected):
    """Only a saved, backed-up native Apply may replace this exact BP hash."""
    path = package(spec['blueprint'])
    current = sha(disk(path))
    if current == expected[path]:
        return []
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCGetUpBindingApply/getup_binding_manifest.json'),
                       key=lambda p: p.stat().st_mtime, reverse=True):
        manifest = document(file)
        if (manifest.get('schema') != 'HarborCity.M5VS2.NPCGetUpBinding.v1' or manifest.get('status') != 'PASS'
                or manifest.get('phase') != 'NPCGetUpBindingApply'):
            continue
        rows = manifest.get('transitions', [])
        selected = [row for row in rows if row.get('sample') == letter and package(row.get('path', '')) == path
                    and row.get('integration_evidence_sha256') == sha(integration_file)]
        if not selected:
            continue
        check('getup manifest contains exactly two unique Q/R BP transitions', len(rows) == 2
              and {row.get('sample') for row in rows} == {'Q', 'R'}
              and len({package(row.get('path', '')) for row in rows}) == 2
              and all(row.get('class_name') == 'Blueprint' and package(row.get('path', '')).startswith(
                  NPC_ROOT + '/AvatarSample_' + row['sample'] + '/Runtime_') for row in rows)
              and len(selected) == 1)
        row = selected[0]
        check('getup exact existing-before and current-after hashes', row.get('before_sha256') == expected[path]
              and row.get('after_sha256') == current
              and re.fullmatch(r'[0-9a-f]{64}', current) is not None, path)
        evidence_file = Path(manifest['evidence']['path']).resolve()
        check('getup manifest bound to adjacent exact Apply report', evidence_file == (file.parent/'author_result.json').resolve()
              and sha(evidence_file) == manifest['evidence']['sha256'])
        evidence = document(evidence_file)
        check('getup Apply recorded only actual successful BP saves', evidence.get('status') == 'PASS'
              and evidence.get('phase') == 'NPCGetUpBindingApply' and evidence.get('applied') is True
              and evidence.get('transitions') == rows and evidence.get('dry_run') == manifest.get('dry_run')
              and evidence.get('preservation', {}).get('unexpected_changed_files') == []
              and all(evidence.get(name) == 0 for name in ('map_writes', 'animation_writes', 'physics_writes'))
              and all(c.get('status') == 'PASS' for c in evidence.get('checks', [])))
        saved = evidence.get('assets', [])
        check('getup report assets exactly match the two transition hashes', len(saved) == 2
              and {(package(a['path']), a['sha256'], a.get('class_name')) for a in saved}
              == {(package(a['path']), a['after_sha256'], 'Blueprint') for a in rows})
        dry_file = Path(manifest['dry_run']['path']).resolve()
        dry = document(dry_file)
        check('getup Apply uses exact successful unapplied DryRun', sha(dry_file) == manifest['dry_run']['sha256']
              and dry.get('status') == 'PASS' and dry.get('phase') == 'NPCGetUpBindingDryRun'
              and dry.get('applied') is False and dry.get('assets') == [] and dry.get('transitions') == [])
        dry_plans = [p for p in dry.get('plans', []) if p.get('sample') == letter]
        apply_plans = [p for p in evidence.get('plans', []) if p.get('sample') == letter]
        check('getup exact preapproved specimen plan', len(dry_plans) == len(apply_plans) == 1
              and package(dry_plans[0]['blueprint']) == path and dry_plans[0]['before_sha256'] == expected[path]
              and dry_plans[0]['integration_sha256'] == sha(integration_file)
              and {k:v for k,v in apply_plans[0].items() if k != 'backup'} == dry_plans[0])
        backup = Path(row['backup']['path']).resolve()
        backup_root = (Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / file.parent.name).resolve()
        check('getup exact private original BP backup', backup.parent == backup_root and backup.name == disk(path).name
              and row['backup']['sha256'] == expected[path] and apply_plans[0].get('backup') == row['backup'])
        protect_file(backup, expected[path])
        retarget_file = Path(row['retarget_evidence']['path']).resolve()
        retarget = document(retarget_file)
        check('getup exact preserved four-action retarget source', row['retarget_evidence'] == dry_plans[0]['retarget']
              and sha(retarget_file) == row['retarget_evidence']['sha256'] and retarget.get('status') == 'PASS'
              and retarget.get('phase') == 'GASRecoveryRetarget' + letter
              and retarget.get('preservation', {}).get('changed_files') == [])
        native = row.get('native_cdo_readback', {})
        clips = native.get('get_up_clips', [])
        check('getup exact enabled four-direction native CDO readback', native.get('use_authored_get_up') is True
              and native.get('get_up_pose_data_verified') is True and len(clips) == 4
              and {c.get('direction') for c in clips} == {'SUPINE', 'PRONE', 'LEFT', 'RIGHT'}
              and {package(c.get('animation', '')) for c in clips}
              == {package(c['path']) for c in retarget.get('target_audits', [])})
        for asset in retarget.get('assets', []):
            protect_file(disk(asset['path']), asset['sha256'])
        expected[path] = current
        return [dict(**binding(file), apply_evidence=binding(evidence_file), dry_run=binding(dry_file),
                     blueprint=path, before_sha256=row['before_sha256'], after_sha256=current,
                     backup=binding(backup), native_cdo_readback=native)]
    raise RuntimeError('NPC BP changed without an exact successful GetUp Apply transition: ' + path)


def npc_specimen(letter):
    report_file, report = latest_pass('author_*_NPCIntegration' + letter, lambda d: d.get('sample') == letter)
    spec = report['specimen']
    check('new isolated NPC integration', package(spec['blueprint']).startswith(NPC_ROOT + '/AvatarSample_' + letter + '/Runtime_'))
    # Only exact, evidenced subsequent animation/PHYS repairs may supersede
    # the original integration hashes. Never accept unrecorded current bytes.
    expected = {package(a['path']): a['sha256'] for a in report['assets']}
    locomotion_repairs = []
    animation_root = report['destination'] + '/Animation/'
    allowed_clips = {animation_root + name + '_NPC' + letter for name in
                     ('MM_Idle', 'MF_Unarmed_Walk_Fwd', 'MF_Unarmed_Jog_Fwd')}
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCInPlace' + letter + '/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if (data.get('status') != 'PASS' or data.get('sample') != letter
                or Path(data.get('integration_evidence', '')).resolve() != report_file.resolve()):
            continue
        check('in-place repair exact integration identity', data.get('destination') == report['destination']
              and data.get('specimen') == spec)
        saved_rows = data.get('assets', [])
        saved_paths = [package(a['path']) for a in saved_rows]
        check('in-place repair only unique known locomotion clips', bool(saved_paths)
              and len(saved_paths) == len(set(saved_paths)) and set(saved_paths) <= allowed_clips)
        backups = data.get('animation_backups', [])
        trajectories = data.get('locomotion_in_place', [])
        transitions = []
        for saved, path in zip(saved_rows, saved_paths):
            before = [a for a in backups if package(a['path']) == path]
            native = [a for a in trajectories if package(a['target']) == path]
            check('in-place repair has unique before/native evidence', len(before) == 1 and len(native) == 1, path)
            before, native = before[0], native[0]
            check('in-place repair begins at integration hash', path in expected
                  and before.get('sha256') == expected[path], path)
            backup = Path(before['backup']).resolve()
            backup_root = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / file.parent.name
            check('in-place repair exact backup path', backup.parent == backup_root.resolve()
                  and backup.name == disk(path).name, str(backup))
            protect_file(backup, expected[path])
            applied = native.get('apply', {})
            readback = native.get('after_native_save_or_unchanged', {})
            check('in-place native applied and saved readback', saved.get('class_name') == 'AnimSequence'
                  and applied.get('status') == 'PASS' and applied.get('applied') is True
                  and applied.get('all_unselected_tracks_exactly_preserved') is True
                  and applied.get('selected_tracks_z_scale_exactly_preserved') is True
                  and readback.get('status') == 'PASS' and readback.get('needs_repair') is False
                  and package(applied.get('target', '')) == path and package(readback.get('target', '')) == path
                  and re.fullmatch(r'[0-9a-f]{64}', saved.get('sha256', '')) is not None, path)
            transitions.append(dict(path=path, before_sha256=expected[path], after_sha256=saved['sha256'],
                                    backup=binding(backup)))
            expected[path] = saved['sha256']
        locomotion_repairs.append(dict(**binding(file), integration_evidence_sha256=sha(report_file), assets=transitions))
        break
    repairs = []
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCPhysicsRepair/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') != 'PASS':
            continue
        row = next((a for a in data.get('assets', []) if a.get('sample') == letter), None)
        if row and row.get('integration_evidence_sha256') == sha(report_file) and row.get('after_save'):
            saved = row['after_save']
            check('repair targets exact specimen physics', package(saved['path']) == package(spec['physics']))
            expected[package(saved['path'])] = saved['sha256']
            repairs.append(binding(file))
            break
    getup_repairs = accept_npc_getup_transition(letter, spec, report_file, expected)
    for path, expected_sha in expected.items():
        protect_file(disk(path), expected_sha)
    for key in ('mesh', 'profile', 'blueprint', 'anim_blueprint', 'physics'):
        protect_file(disk(spec[key]))
    R['inputs']['NPC_' + letter] = dict(**binding(report_file), specimen=spec,
                                      locomotion_repairs=locomotion_repairs, physics_repairs=repairs,
                                      getup_bindings=getup_repairs)
    return spec


def resolve_selected_materials(layout):
    global SELECTED_WATER
    selected = layout['selected_material_candidates']
    for tag, variant, phase, asset_count in (('buildings', 'SoftAnime', 'CornerStyleCompare', 7),
                                           ('water', 'WaterWorldScale', 'CornerWaterCandidate', 4)):
        entry = selected[tag]
        author_file = DOC / entry['author']['relative_path']
        author = document(author_file)
        protect_file(author_file, entry['author']['sha256'])
        check('selected material candidate native PASS with exact closure',
              author.get('status') == 'PASS' and author.get('phase') == phase
              and author.get('original_assets_changed') is False
              and author.get('character_materials_changed') is False
              and len(author.get('assets', [])) == asset_count, tag)
        namespace = author['namespace']
        check('selected candidate isolated style namespace',
              re.fullmatch(r'/Game/HarborCity/M5VS2/World/StyleReview_[0-9a-f]{12}', namespace) is not None)
        for asset in author['assets']:
            path = package(asset['path'])
            check('exact candidate closure remains inside owned namespace', path.startswith(namespace + '/'), path)
            extension = '.umap' if path in author['maps'].values() else '.uasset'
            protect_file(disk(path, extension), asset['sha256'])
        # Preserve the exact original material/function/texture dependencies.
        # Historical source-map/Config hashes belong to that old author, not to
        # today's mutable fixed Corner map or unrelated project settings.
        for path, expected in author['protected_source_sha256'].items():
            if Path(path).suffix.lower() == '.uasset':
                protect_file(path, expected)
        runtime_file = DOC / entry['runtime']['relative_path']
        runtime = document(runtime_file)
        protect_file(runtime_file, entry['runtime']['sha256'])
        upstream = runtime.get('input_plan', {}).get('sources', {}).get('corner_author', {})
        check('selected candidate binds completed six-frame actual runtime',
              runtime.get('status') == 'PASS' and runtime.get('user_stop_latched') is False
              and runtime.get('actual_verified_png_count') == 6 and len(runtime.get('captures', [])) == 6
              and all(c.get('status') == 'PASS' for c in runtime['captures'])
              and runtime.get('actual_map') == author['maps'][variant]
              and Path(upstream.get('path', '')).resolve() == author_file.resolve()
              and upstream.get('sha256') == sha(author_file), tag)
        mapping = {package(source): package(target) for source, target in author['material_mapping'].items()}
        asset_paths = {package(a['path']) for a in author['assets']}
        check('mapping references saved candidate material assets',
              all(source in COPY_MAP.values() and target in asset_paths for source, target in mapping.items()), tag)
        if tag == 'buildings':
            check('only four approved building surfaces replaced', len(mapping) == 4
                  and {p.rsplit('/', 1)[1] for p in mapping} == {'MI_wall_02', 'MI_wood_05', 'MI_rooftiles_01', 'MI_stonebrick_02'})
            BUILDING_MATERIALS.update(mapping)
        else:
            source = COPY_MAP[PROP_SOURCES['original_water_material']]
            check('exact single original water slot replacement', set(mapping) == {source})
            SELECTED_WATER = mapping[source]
        R['inputs'][tag + '_selected_author'] = binding(author_file)
        R['inputs'][tag + '_selected_runtime'] = binding(runtime_file)
        R.setdefault('selected_material_candidates', {})[tag] = dict(mapping=mapping, assets=author['assets'],
            prior_runtime_scope='Six actual captures completed; root selected this comparison. Current revised map render NOT_RUN; final user art USER_REVIEW.')
    grass_entry = selected['grass_textures']
    grass_file = DOC / grass_entry['relative_path']; grass = document(grass_file)
    protect_file(grass_file, grass_entry['sha256'])
    sources = {SOURCE_PACK + '/textures/T_ENV_TERRAIN_grass_01_' + suffix for suffix in ('BC', 'N')}
    check('exact two native copied grass textures PASS', grass.get('status') == 'PASS'
          and grass.get('phase') == 'GroundTextures' and len(grass.get('assets', [])) == 2
          and {a.get('source') for a in grass['assets']} == sources)
    check('isolated grass copy namespace',
          re.fullmatch(r'/Game/HarborCity/M5VS2/Environment/Village_[0-9a-f]{12}', grass['destination']) is not None)
    for asset in grass['assets']:
        check('grass texture path under exact copied destination', asset['path'].startswith(grass['destination'] + '/textures/'))
        protect_file(disk(asset['path']), asset['sha256'])
        COPY_MAP[asset['source']] = asset['path']
        texture = load(asset['path'])
        expected_normal = asset['source'].endswith('_N')
        check('copied grass texture native type and color space', isinstance(texture, U.Texture2D)
              and bool(texture.get_editor_property('srgb')) != expected_normal, asset['path'])
    for path, expected in grass['source_sha256'].items():
        protect_file(path, expected)
    R['inputs']['grass_texture_copy'] = binding(grass_file)
    R['selected_material_candidates']['grass_textures'] = grass['assets']


def resolve_inputs():
    layout = document(LAYOUT_FILE)
    R['art_revision'] = layout.get('art_revision', 1)
    check('exact project-owned layout identity', layout['schema_version'] == 1 and layout['owner'] == OWNER and layout['map'] == MAP)
    check('bounded 50m layout', layout['playable_bounds_xy'] == [-2500, -2500, 2500, 2500])
    check('four inspected real house choices', sorted(h['house'] for h in layout['houses']) == [2, 6, 9, 14])
    explicit = argument('M5VillageCopy')
    if explicit:
        copy_file = Path(explicit).resolve(); copied = document(copy_file)
    else:
        copy_file, copied = latest_pass('author_*Village*', lambda d: d.get('mode') == 'Copy')
    check('actual saved native Village Copy PASS', copied.get('status') == 'PASS' and copied.get('mode') == 'Copy'
          and copied.get('geometry_paths_state') == 'SAVED' and copied.get('source_unchanged') is True
          and copied.get('selection_kind') == 'STATIC_GEOMETRY_ONLY'
          and copied.get('native', {}).get('saved_packages_and_reference_remap_verified') is True)
    destination = copied['destination']
    check('isolated native copied namespace', re.fullmatch(r'/Game/HarborCity/M5VS2/Environment/Village_[0-9a-f]{12}', destination) is not None)
    for row in copied['assets']:
        check('all copy assets remain within destination', row['path'].startswith(destination + '/'))
        protect_file(disk(row['path']), row['sha256'])
        COPY_MAP[row['source']] = row['path']
    manifest_file = Path(copied['manifest']['path']).resolve()
    manifest = document(manifest_file)
    check('unchanged reviewed copy manifest', sha(manifest_file) == copied['manifest']['sha256'])
    for row in manifest['candidates']:
        PROP_SOURCES[row['role']] = row['path']
    assemblies = {int(a['source_blueprint'].rsplit('_', 1)[1]): a for a in copied['geometry_assemblies']}
    check('all four copied assemblies present', set(assemblies) == {2, 6, 9, 14})
    check('exact 55 native template mesh components', sum(len(a['components']) for a in assemblies.values()) == 55)
    for house in assemblies.values():
        for c in house['components']:
            check('geometry references only saved copy assets', c['static_mesh'] in COPY_MAP.values()
                  and all(m is None or m in COPY_MAP.values() for m in c['effective_materials']))
    R['inputs'].update(layout=binding(LAYOUT_FILE), village_copy=binding(copy_file), village_selection=binding(manifest_file))
    R['limitations'] = layout['limitations']
    resolve_selected_materials(layout)
    q, r = npc_specimen('Q'), npc_specimen('R')
    for folder in (PROJECT / 'Config', disk(HERO).parent):
        for file in sorted(folder.rglob('*')):
            if file.is_file() and file.suffix.lower() in ('.ini', '.uasset'):
                protect_file(file)
    for file in sorted((PROJECT / 'Content').rglob('*.umap')):
        if file != disk(MAP, '.umap'):
            protect_file(file)
    hero = generated_class(HERO); mode = generated_class(GAME_MODE)
    check('new GameMode retains new Hero pawn', U.get_default_object(mode).get_editor_property('default_pawn_class') == hero)
    cdo = U.get_default_object(hero)
    body = cdo.get_component_by_class(U.SkeletalMeshComponent)
    capsule = cdo.get_component_by_class(U.CapsuleComponent)
    check('actual new hero mesh and animation are bound', body is not None and body.get_editor_property('skeletal_mesh_asset') is not None
          and body.get_editor_property('anim_class') is not None and capsule is not None)
    R['hero_binding'] = dict(blueprint=HERO, generated_class=hero.get_path_name(), game_mode=mode.get_path_name(),
        mesh=body.get_editor_property('skeletal_mesh_asset').get_path_name(), animation=body.get_editor_property('anim_class').get_path_name(),
        materials=[body.get_material(i).get_path_name() if body.get_material(i) else None for i in range(body.get_num_materials())],
        material_policy='Existing actual Blueprint materials; no lookdev override, new hero duplication or style acceptance.',
        capsule_half_height_cm=float(capsule.get_unscaled_capsule_half_height()))
    for path in [R['hero_binding']['mesh'], R['hero_binding']['animation']] + R['hero_binding']['materials']:
        if path and package(path).startswith('/Game/'):
            protect_file(disk(path))
    return layout, assemblies, {'Q': q, 'R': r}, mode


def prepare_map(mode):
    check('fresh dedicated package namespace', not A.does_directory_exist(ASSET_ROOT) and not disk(ASSET_ROOT + '/unused').parent.exists())
    if A.does_asset_exist(MAP):
        world_asset = load(MAP)
        # This exact empty map was auto-saved by new_level before the first
        # material connection failed. Preserve it; never accept arbitrary bytes.
        failed_empty_sha = '34cdef423432d221366d89f21183a51b0d2db6b302bbe45fb922acafcaa3f12b'
        known_empty = sha(disk(MAP, '.umap')) == failed_empty_sha
        check('previous map belongs to this author or exact failed empty author artifact', A.get_metadata_tag(world_asset, KEY) == OWNER or known_empty)
        if known_empty:
            previous_file = DOC / 'editor_runtime/author_20260924_045252_475_980b518a_HarborCorner/author_result.json'
            previous = document(previous_file)
            check('known first author failed before actor creation', previous['status'] == 'FAIL' and previous['map'] == MAP and not previous['actors'] and not previous['assets'])
            old = dict(path=MAP, sha256=failed_empty_sha)
        else:
            previous_file, previous = latest_pass('author_*HarborCorner*', lambda d: d.get('map') == MAP)
            old = next((a for a in previous.get('assets', []) if a.get('path') == MAP), None)
        check('existing map has no unrecorded/user edits', old is not None and sha(disk(MAP, '.umap')) == old['sha256'])
        backup = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Environment/CornerBackups') / OUT.name / 'L_AnimeHarbor_Corner_P0.umap'
        check('fresh exact map backup', not backup.exists()); backup.parent.mkdir(parents=True, exist_ok=False)
        shutil.copy2(disk(MAP, '.umap'), backup)
        check('map backup byte verified', sha(backup) == old['sha256'])
        R['prior_owned_map'] = dict(evidence=binding(previous_file), backup=binding(backup))
        check('load owned corner only', LEVEL.load_level(MAP))
        for actor in ACT.get_all_level_actors():
            if actor.get_class().get_name() in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume'):
                continue
            check('only prior authored actors in corner', actor.actor_has_tag(U.Name(OWNER)), actor.get_actor_label())
            check('remove prior owned corner actor', ACT.destroy_actor(actor))
    else:
        check('new fixed map absent on disk', not disk(MAP, '.umap').exists())
        check('native empty corner world', LEVEL.new_level(MAP, False))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact current corner world', package(world.get_path_name()) == MAP)
    A.set_metadata_tag(world, KEY, OWNER)
    world.get_world_settings().set_editor_property('default_game_mode', mode)
    return world


def author_houses(layout, assemblies):
    for placement in layout['houses']:
        assembly = assemblies[placement['house']]
        transform = place_bounds(assembly['bounds_cm'], placement['center_xy'], placement['ground_z'], placement['yaw'])
        world_bounds = transformed_bounds(assembly['bounds_cm'], transform)
        check('complete house static bounds within 50m', all(-2500 <= world_bounds['min'][i] <= world_bounds['max'][i] <= 2500 for i in (0, 1)), placement['role'])
        observed_components = []
        for component in assembly['components']:
            # UE ComposeTransforms applies the exact SCS-to-Actor transform first,
            # then our whole-building placement; no external actor ever exists.
            final = U.MathLibrary.compose_transforms(template_transform(component['template_to_actor_transform']), transform)
            mesh = load(component['static_mesh'])
            source_mats = component['effective_materials']
            mats = [load(BUILDING_MATERIALS.get(package(path), path)) if path else None for path in source_mats]
            actor = static_mesh(placement['role'] + '_' + component['variable'], mesh, final, mats, group=placement['role'])
            body = actor.get_component_by_class(U.StaticMeshComponent)
            actual_mats = [body.get_material(i).get_path_name() if body.get_material(i) else None for i in range(body.get_num_materials())]
            check('actual building component slots equal selected SoftAnime binding', len(actual_mats) == len(mats)
                  and all(actual_mats[i] == m.get_path_name() for i, m in enumerate(mats) if m is not None), actor.get_actor_label())
            observed_components.append(dict(label=actor.get_actor_label(), source_variable=component['variable'],
                mesh=mesh.get_path_name(), source_materials=source_mats, materials=actual_mats,
                material_readback_source='Actual StaticMeshComponent.GetMaterial after overrides', transform=transform_record(final)))
        R['houses'].append(dict(house=placement['house'], role=placement['role'], source_blueprint_read_only=assembly['source_blueprint'],
            placement_transform=transform_record(transform), bounds_cm=world_bounds, components=observed_components,
            excluded_source_components=assembly['excluded_components'], interior='NOT_AUTHORED'))


def author_street(layout):
    stone = load(COPY_MAP[PROP_SOURCES['stone_surface']])
    road = layout['road']
    road_mat = surface_material('M_Corner_Gravel', SOURCE_PACK + '/textures/T_ENV_TERRAIN_gravel_BC',
        SOURCE_PACK + '/textures/T_ENV_TERRAIN_gravel_N', road['road_texture_period_cm'],
        base_color_adjustment=layout['gravel_color_adjustment'])
    walk_mat = surface_material('M_Corner_StoneWalk', SOURCE_PACK + '/textures/T_stonebrick_01_BC',
                                SOURCE_PACK + '/textures/T_stonebrick_01_N', road['walk_texture_period_cm'])
    islands = layout['planting_islands']
    check('exact six bounded planting islands', len(islands) == 6
          and [i['name'] for i in islands] == ['SouthTreeIsland', 'HarborRestPlanting',
              'PromenadeSouthGarden', 'PromenadeNorthGarden', 'CafeBoundaryGarden', 'GuildBoundaryGarden'])
    for island in islands:
        bounds = dict(min=[island['center_xy'][i]-island['size_cm'][i]*.5 for i in (0, 1)] + [-2],
                      max=[island['center_xy'][i]+island['size_cm'][i]*.5 for i in (0, 1)] + [0])
        check_added_placement(layout, bounds, island['name'])
    ground_mat = surface_material('M_Corner_PlantingGround', SOURCE_PACK + '/textures/T_ENV_TERRAIN_gravel_BC',
        SOURCE_PACK + '/textures/T_ENV_TERRAIN_gravel_N', road['road_texture_period_cm'], planting_islands=islands,
        base_color_adjustment=layout['gravel_color_adjustment'])
    promenade = layout['promenade_surface']
    promenade_mat = surface_material('M_Corner_PromenadeStone', SOURCE_PACK + '/textures/T_stonebrick_02_BC',
        SOURCE_PACK + '/textures/T_stonebrick_01_N', promenade['texture_period_cm'],
        normal_flatness=promenade['normal_flatness'], planting_islands=islands)
    # Land ends at the real quay wall; the water has no invisible walking floor.
    box('LandFoundation', (-350, 0, -82), (4300, 5000, 160), stone, 'StreetStructure')
    box('VillageGround', (-350, 0, -6), (4300, 5000, 8), ground_mat, 'StreetStructure')
    points = road_centerline(road['points_xy'], road['samples_per_segment'])
    half = road['width_cm'] * .5; outer = half + road['sidewalk_width_cm']
    left = offset_line(points, half); right = offset_line(points, -half)
    left_outer = offset_line(points, outer); right_outer = offset_line(points, -outer)
    road_polygon = strip_mesh('SM_Corner_Road6m', left + list(reversed(right)), -10, 10, road_mat)
    left_polygon = strip_mesh('SM_Corner_WalkWest2m', left_outer + list(reversed(left)), -10, 12, walk_mat)
    right_polygon = strip_mesh('SM_Corner_WalkEast2m', right + list(reversed(right_outer)), -10, 12, walk_mat)
    box('PromenadeStone', (1330, 0, -4), (940, 5000, 8), promenade_mat, 'Promenade')
    box('WellSquareStone', (1190, 820, 0), (1160, 1280, 4), promenade_mat, 'WellSquare')
    box('CafeTerraceStone', (-1230, -1500, -2), (470, 1190, 4), walk_mat, 'CafeTerrace')
    R['road_geometry'] = dict(centerline_cm=points, road_polygon_cm=road_polygon,
        west_walk_polygon_cm=left_polygon, east_walk_polygon_cm=right_polygon,
        road_width_cm=600, each_sidewalk_width_cm=200, curb_height_cm=2,
        source='Three continuous capped native GeometryScript meshes; no overlapping slab joints.',
        collision='Authored complex-as-simple static collision; walking/vehicle runtime NOT_RUN.')
    R['planting_ground'] = dict(islands=islands, added_ground_meshes=0, added_collision_shapes=0,
        existing_ground_top_z_cm={'VillageGround': -2, 'PromenadeStone': 0, 'WellSquareStone': 2},
        edge_blend='Bounded irregular ellipses: grass BC and tangent normal blend into each actual existing ground surface; 18% radial feather. No opacity/WPO/coplanar overlays.',
        preserved_surfaces='Road and both sidewalks retain original geometry, texture scale and collision. Only gravel chroma changes; sidewalk material is unchanged. Land and promenade dimensions/heights/collision are unchanged.',
        material_actors={'HC_M5VS2_Corner_VillageGround': ground_mat.get_path_name(),
                         'HC_M5VS2_Corner_PromenadeStone': promenade_mat.get_path_name(),
                         'HC_M5VS2_Corner_WellSquareStone': promenade_mat.get_path_name()})


def author_props(layout):
    # Cafe: two actual source furniture sets; dishes sit on measured table AABB
    # tops rather than a guessed 1m table height.
    cafe = next(g for g in layout['groups'] if g['name'] == 'CafeTerrace')
    for index, (x, y) in enumerate(cafe['tables_xy']):
        _, bounds = prop('cafe_table', 'CafeTable_' + str(index), (x, y), group='CafeTerrace')
        top = bounds['max'][2]
        for side, yaw in ((-1, 0), (1, 180)):
            prop('cafe_chair', 'CafeChair_%d_%d' % (index, side), (x, y + side*105), yaw=yaw, group='CafeTerrace')
        prop('wood_plate', 'CafePlate_' + str(index), (x-18, y-16), top+.5, group='CafeTerrace', collision=False)
        prop('cup', 'CafeCup_' + str(index), (x+27, y+19), top+.5, yaw=40, group='CafeTerrace', collision=False)
        prop('wood_bowl', 'CafeBowl_' + str(index), (x-20, y+29), top+.5, group='CafeTerrace', collision=False)
    for i, p in enumerate(((-1170, -2050), (-1160, -965))):
        prop('planter', 'CafeFlowers_' + str(i), p, group='CafeTerrace')
    market = next(g for g in layout['groups'] if g['name'] == 'ShopMarket')
    for index, stall in enumerate(market['stalls']):
        x, y = stall['center_xy']; label = 'Market_' + str(index)
        prop(stall['role'], label + '_Canopy', (x, y), yaw=stall['yaw'], group='ShopMarket')
        _, shelf = prop('market_shelf', label + '_Shelf', (x, y+55), yaw=stall['yaw'], group='ShopMarket')
        # Counter goods use the measured native shelf top and its local long
        # axis. The former fixed world offset let the bowl overhang one shelf.
        angle = math.radians(stall['yaw'])
        for display in market['display_items']:
            dx, dy = display['shelf_local_xy']
            xy = (x + dx*math.cos(angle)-dy*math.sin(angle),
                  y + 55 + dx*math.sin(angle)+dy*math.cos(angle))
            item_label = label + '_' + display['label']
            _, bounds = prop(display['role'], item_label, xy, shelf['max'][2]+.3,
                             yaw=stall['yaw']+display['yaw'], scale=display['scale'],
                             group='ShopMarket', collision=display.get('collision', True))
            check('market goods fully supported on measured shelf in XY', all(
                bounds['min'][i] >= shelf['min'][i] and bounds['max'][i] <= shelf['max'][i]
                for i in (0, 1)), item_label)
            check_added_placement(layout, bounds, item_label)
        prop('sack', label + '_Sack', (x+125, y-55), scale=.9, group='ShopMarket')
        prop('crate', label + '_Crate', (x-125, y-70), group='ShopMarket')
        prop('barrel', label + '_Barrel', (x+100, y+100), group='ShopMarket')
    well = next(g for g in layout['groups'] if g['name'] == 'WellSquare')
    prop('square_landmark_well_geometry', 'SquareWell', well['center_xy'], well['ground_z'], yaw=90, group='WellSquare')
    prop('bench', 'SquareBench', (1530, 1240), 2, yaw=180, group='WellSquare')
    prop('planter', 'SquareFlowers', (1530, 310), 2, group='WellSquare')
    prop('signpost', 'GuildWayfinding', (980, 1530), 2, yaw=180, group='WellSquare')
    prop('barrel', 'GuildDeliveryBarrel', (-440, 2040), group='MessengerGuild')
    prop('crate', 'GuildDeliveryCrate', (-570, 2120), group='MessengerGuild')
    # Same-pack stone construction pieces give the quay visible depth. Tops
    # align with promenade rather than standing as a metre-high obstruction.
    for index in range(17):
        prop('stone_quay_edge', 'QuayWall_%02d' % index, (1784, -2370+index*295), -106.5965,
             group='QuayStructure')
    for index, y in enumerate((-2200, -1875, -825, -500, -175, 150, 475, 1530, 1855, 2180)):
        prop('wood_railing', 'QuayRail_%02d' % index, (1758, y), 0, yaw=90, group='QuayStructure')
    quay = next(g for g in layout['groups'] if g['name'] == 'QuayLoading')
    prop('short_timber_bridge', 'ShortTimberPier', quay['bridge_xy'], 0, yaw=-90, group='QuayLoading')
    prop('dock_board_cluster', 'PierLandingBoards', (2130, -1450), 0, group='QuayLoading')
    prop('moored_rowboat_geometry', 'MooredRowboat', quay['boat_xy'], layout['water_z']-45, yaw=0,
         group='QuayLoading', collision=False)
    prop('boat_paddle_geometry', 'StoredPaddle', (1620, -1280), 22, yaw=0, group='QuayLoading', collision=False)
    for role, label, p in (('crate', 'CargoCrateA', (1510, -1060)), ('crate', 'CargoCrateB', (1600, -990)),
                           ('barrel', 'CargoBarrel', (1400, -1010)), ('sack', 'CargoSack', (1450, -1170))):
        prop(role, label, p, group='QuayLoading')
    for index, tree in enumerate(layout['trees']):
        _, tree_bounds = prop('tree', 'Tree_' + str(index), tree['center_xy'], yaw=tree['yaw'], scale=tree['scale'], group='Planting')
        if index >= 2:
            check_added_placement(layout, tree_bounds, 'Tree_' + str(index))
        offsets = ((-75, 55), (90, 40), (45, -65)) if index == 2 else ((-115, 80), (135, 60), (70, -95))
        for j, (dx, dy) in enumerate(offsets):
            _, bounds = prop('grass', 'Grass_%d_%d' % (index, j), (tree['center_xy'][0]+dx, tree['center_xy'][1]+dy),
                 yaw=j*73, scale=.55+j*.05 if index == 2 else .9+j*.1, group='Planting', collision=False)
            if index in (2, 6, 7):
                check_added_placement(layout, bounds, 'Grass_%d_%d' % (index, j))
        _, bounds = prop('leaf_patch', 'LowPlanting_' + str(index),
             (tree['center_xy'][0], tree['center_xy'][1]+(25 if index == 2 else 45)),
             scale=.55 if index == 2 else .65, group='Planting', collision=False)
        if index in (2, 6, 7):
            check_added_placement(layout, bounds, 'LowPlanting_' + str(index))
    for island in layout['planting_islands']:
        for index, item in enumerate(island.get('low_plants', [])):
            label = island['name'] + '_Low_' + str(index)
            _, bounds = prop(item['role'], label, item['center_xy'], item['ground_z'], item['yaw'], item['scale'],
                             'Planting', collision=False)
            check_added_placement(layout, bounds, label)
            check('new low planting within its finite island box', all(
                bounds['min'][i] >= island['center_xy'][i]-island['size_cm'][i]*.5
                and bounds['max'][i] <= island['center_xy'][i]+island['size_cm'][i]*.5 for i in (0, 1)), label)
    for index, p in enumerate(((1550, -1820), (1570, 1770), (-650, 2140))):
        prop('streetlamp_geometry', 'Streetlamp_' + str(index), p, yaw=90, group='StreetLighting')
    for wall in layout.get('edge_walls', []):
        for index, xy in enumerate(wall['centers_xy']):
            label = wall['name'] + '_' + str(index)
            _, bounds = prop('stone_quay_edge', label, xy, 0, wall['yaw'], wall['scale'], 'GardenBoundary')
            check_added_placement(layout, bounds, label)
    for cluster in layout.get('detail_clusters', []):
        item_bounds = []
        for index, item in enumerate(cluster['items']):
            label = cluster['name'] + '_' + item['role'] + '_' + str(index)
            support = item.get('support_item')
            check('stack support precedes its item', support is None or isinstance(support, int) and 0 <= support < index, label)
            ground_z = item_bounds[support]['max'][2] + .3 if support is not None else item.get('ground_z', 0)
            _, bounds = prop(item['role'], label, item['center_xy'], ground_z, item.get('yaw', 0),
                             item.get('scale', 1), cluster['name'], item.get('collision', True))
            if support is not None:
                check('stacked prop fully supported in XY', all(
                    bounds['min'][i] >= item_bounds[support]['min'][i]
                    and bounds['max'][i] <= item_bounds[support]['max'][i] for i in (0, 1)), label)
            item_bounds.append(bounds)
            check_added_placement(layout, bounds, label)
    R['composition_revision'] = dict(revision=layout.get('art_revision', 1),
        groups=layout.get('detail_clusters', []), garden_edges=layout.get('edge_walls', []),
        tree_count=len(layout['trees']), source_tree_variants=1,
        source_scope='Only already copied source meshes; no foreign Blueprint or new dependency.',
        market_displays=market['display_items'],
        revision_scope=layout.get('composition_revision_scope'),
        limitations='One source tree species, grouped at distinct scales. This remains a 50m review candidate; no distant town, interiors or new traversable land.')
    R['source_prop_placement_notes'] = [
        'Well handle is selected in the copy but not placed: no verified source assembly anchor was supplied.',
        'Rowboat is decorative/moored, collision disabled; no boat physics or boarding is claimed.',
        'Bridge/landing/paddle and furniture contact are native placement candidates; visual fit and traversal NOT_RUN.',
        'Grass/tableware have no blocking collision. Other copied mesh collision is retained, not rebuilt here.']


def author_environment(layout):
    water = load(SELECTED_WATER)
    # Read back source settings to make the first real viewport review useful;
    # this author does not guess how a source parameter's graph interprets UVs.
    water_parameters = {str(name): float(E.get_material_instance_scalar_parameter_value(water, name))
                        for name in E.get_scalar_parameter_names(water)}
    plane = load('/Engine/BasicShapes/Plane')
    plane_bounds = mesh_bounds(plane)
    sea = layout['sea_surface']
    check('bounded non-playable sea configuration', sea['collision'] is False
          and all(100000 <= value <= 1000000 for value in sea['size_cm'])
          and sea['center_xy'] == [0, 0])
    size = [plane_bounds['max'][i] - plane_bounds['min'][i] for i in range(2)]
    check('nondegenerate actual native plane dimensions', min(size) > 0, plane_bounds)
    sea_transform = native_transform((*sea['center_xy'], layout['water_z']),
        scale=(*[sea['size_cm'][i] / size[i] for i in range(2)], 1))
    sea_actor = static_mesh('SeaSurface', plane, sea_transform, [water], False, 'NonPlayableSea')
    check('sea remains without collision', sea_actor.get_component_by_class(U.StaticMeshComponent)
          .get_collision_enabled() == U.CollisionEnabled.NO_COLLISION)
    old_bounds = transformed_bounds(plane_bounds,
        native_transform((101800, 0, layout['water_z']), scale=(2000, 2000, 1)))
    current_bounds = transformed_bounds(plane_bounds, sea_actor.get_actor_transform())
    # Ground-facing rays are geometry diagnostics, not an image-based verdict.
    # Report both footprints; the explicit layout cameras supply the visual test.
    ray_checks = []
    for camera in layout['cameras']:
        location, target = camera['location_cm'], camera['target_cm']
        yaw = math.atan2(target[1]-location[1], target[0]-location[0])
        for below_horizon in (1, 3, 8):
            distance = (location[2]-layout['water_z']) / math.tan(math.radians(below_horizon))
            hit = [location[0]+distance*math.cos(yaw), location[1]+distance*math.sin(yaw)]
            covered = lambda bounds: all(bounds['min'][i] <= hit[i] <= bounds['max'][i] for i in range(2))
            ray_checks.append(dict(camera=camera['name'], degrees_below_horizon=below_horizon,
                sea_intersection_xy_cm=hit, old_plane_covers=covered(old_bounds),
                current_plane_covers=covered(current_bounds)))
    check('surrounding sea covers diagnostic camera rays', all(row['current_plane_covers'] for row in ray_checks))
    R['sea_coverage'] = dict(native_plane_bounds_cm=plane_bounds, prior_world_bounds_cm=old_bounds,
        current_world_bounds_cm=current_bounds, ray_checks=ray_checks, collision=False,
        geometry_unchanged_from_revision_3=True, original_water_asset_byte_preserved=True,
        selected_material=water.get_path_name(), visual_verification='CURRENT_REVISED_MAP_NOT_RUN')
    # The copied day texture stays byte-preserved in the library. It no longer
    # covers the atmosphere or remains at daytime when the review lowers the sun.
    atmosphere = spawn('SunDrivenAtmosphere', U.SkyAtmosphere, (0, 0, 0), group='Sky')
    atmosphere_component = atmosphere.get_component_by_class(U.SkyAtmosphereComponent)
    check('native sky atmosphere component', atmosphere_component is not None)
    for key, value in layout['atmosphere'].items():
        atmosphere_component.set_editor_property(key, U.LinearColor(*value) if key == 'sky_luminance_factor' else value)
    atmosphere_actual = atmosphere_readback(atmosphere_component)
    check('atmosphere authored values read back', all(
        abs(atmosphere_actual[key]-value) < 1.e-5 if isinstance(value, (int, float)) else
        all(abs(a-b) < 1.e-5 for a, b in zip(atmosphere_actual[key], value))
        for key, value in layout['atmosphere'].items()), atmosphere_actual)
    preset = layout['lighting_presets']['Afternoon']
    sun = spawn('AfternoonSun', U.DirectionalLight, (0, 0, 2000),
                U.Rotator(pitch=preset['sun_pitch'], yaw=preset['sun_yaw']), 'Lighting')
    light = sun.get_component_by_class(U.DirectionalLightComponent)
    light.set_mobility(U.ComponentMobility.MOVABLE)
    light.set_light_color(U.LinearColor(*preset['sun_color'], 1))
    light.set_intensity(preset['sun_intensity'])
    light.set_editor_property('atmosphere_sun_light', True)
    light.set_editor_property('light_source_angle', layout['sun_source_angle_degrees'])
    actual_source_angle = float(light.get_editor_property('light_source_angle'))
    check('native directional source angle readback', abs(actual_source_angle-layout['sun_source_angle_degrees']) < 1.e-6,
          actual_source_angle)
    sky = spawn('SkyLight', U.SkyLight, (0, 0, 2000), group='Lighting').get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE)
    sky.set_editor_property('real_time_capture', True)
    # UE documents this as an approximation of bounce for a movable SkyLight.
    # It avoids a forced black lower hemisphere in the stylized review stage.
    sky.set_editor_property('lower_hemisphere_is_black', False)
    sky.set_intensity(preset['sky_intensity'])
    post = spawn('FixedExposure', U.PostProcessVolume, group='Lighting')
    post.set_editor_property('unbound', True)
    settings = post.get_editor_property('settings')
    fields = dict(override_auto_exposure_method=True, auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True, auto_exposure_bias=preset['exposure_bias'],
        override_motion_blur_amount=True, motion_blur_amount=0.,
        override_bloom_intensity=True, bloom_intensity=preset['bloom_intensity'])
    for key, value in fields.items():
        settings.set_editor_property(key, value)
    post.set_editor_property('settings', settings)
    R['lighting'] = dict(saved_preset='Afternoon', presets=layout['lighting_presets'],
        main_light_actor=sun.get_actor_label(), sky_material=None, sun_source_angle_degrees=actual_source_angle,
        sky_policy='Native SkyAtmosphere responds to the actual tagged AtmosphereSunLight direction/color/intensity; real-time SkyLight captures it. No original day-sky sphere or volumetric cloud. Afternoon is the saved default; Dusk is explicitly staged. Rendered sky transition and material quality require native visual review.',
        atmosphere_actor=atmosphere.get_actor_label(), atmosphere_component_class=atmosphere_component.get_class().get_path_name(),
        atmosphere_actual=atmosphere_actual, sky_realtime_capture=bool(sky.get_editor_property('real_time_capture')),
        sky_lower_hemisphere_solid_color=bool(sky.get_editor_property('lower_hemisphere_is_black')),
        water_material=water.get_path_name(), scope=layout['lighting_scope'],
        water_scalar_parameters=water_parameters,
        weather_transition='NOT_AUTHORED', source_night_material_available=COPY_MAP[PROP_SOURCES['original_night_sky_material']])
    author_cloud_candidate(layout)


def verify_saved_material_bindings(saved_actors):
    by_label = {a.get_actor_label(): a for a in saved_actors}
    used_building_candidates = set()
    for house in R['houses']:
        for row in house['components']:
            body = by_label[row['label']].get_component_by_class(U.StaticMeshComponent)
            observed = [body.get_material(i).get_path_name() if body.get_material(i) else None for i in range(body.get_num_materials())]
            check('saved building slots equal actual authored material readback', observed == row['materials'], row['label'])
            row['saved_materials'] = observed
            used_building_candidates.update(package(m) for m in observed if m and package(m) in BUILDING_MATERIALS.values())
    check('all four selected building material instances are actually used', used_building_candidates == set(BUILDING_MATERIALS.values()))
    targets = dict(R['planting_ground']['material_actors'])
    targets['HC_M5VS2_Corner_SeaSurface'] = load(SELECTED_WATER).get_path_name()
    R['saved_surface_materials'] = []
    for label, expected in targets.items():
        body = by_label[label].get_component_by_class(U.StaticMeshComponent)
        observed = body.get_material(0).get_path_name() if body.get_material(0) else None
        check('saved surface slot equals selected actual material', observed == expected, label)
        R['saved_surface_materials'].append(dict(label=label, slot=0, material=observed,
            collision_enabled=str(body.get_collision_enabled())))
        if label.endswith('SeaSurface'):
            check('saved selected sea still has no collision', body.get_collision_enabled() == U.CollisionEnabled.NO_COLLISION)
    sun = by_label[R['lighting']['main_light_actor']].get_component_by_class(U.DirectionalLightComponent)
    angle = float(sun.get_editor_property('light_source_angle'))
    check('saved directional source angle equals native author readback', abs(angle-R['lighting']['sun_source_angle_degrees']) < 1.e-6)
    R['lighting']['saved_sun_source_angle_degrees'] = angle


def cloud_instance_snapshot(material):
    values = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        names = getattr(E, 'get_' + kind + '_parameter_names')(material)
        check('bounded cloud effective parameter names', len(names) <= 64)
        getter = getattr(E, 'get_material_instance_' + kind + '_parameter_value')
        values[kind] = {}
        for name in names:
            value = getter(material, name)
            if isinstance(value, U.LinearColor):
                value = [float(value.r), float(value.g), float(value.b), float(value.a)]
            elif isinstance(value, U.Object):
                value = value.get_path_name()
            values[kind][str(name)] = value
    return dict(parent=material.get_editor_property('parent').get_path_name(), effective_parameters=values,
                base_property_overrides=material.get_editor_property('base_property_overrides').export_text())


def cloud_component_readback(component):
    names = ('layer_bottom_altitude', 'layer_height', 'tracing_max_distance', 'view_sample_count_scale',
             'reflection_view_sample_count_scale_value', 'shadow_view_sample_count_scale',
             'shadow_reflection_view_sample_count_scale_value')
    result = {name: float(component.get_editor_property(name)) for name in names}
    material = component.get_editor_property('material')
    result['material_reference'] = material.get_path_name() if isinstance(material, U.Object) else str(material)
    return result


def author_cloud_candidate(layout):
    """One isolated MI; retain the full-quality Engine graph and its textures."""
    settings = layout['cloud_candidate']
    source_path = '/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst'
    check('explicit native cloud candidate source', settings['source_material'] == source_path)
    file, probe = latest_pass('author_*_CloudProbe', lambda d: d.get('cloud_source') == source_path)
    R['inputs']['cloud_probe'] = binding(file)
    engine_files = []
    for path, digest in probe['protected_source_sha256'].items():
        if Path(path).resolve().is_relative_to(Path('E:/UE_5.8/Engine/Content').resolve()):
            protect_file(path, digest)
            engine_files.append(str(Path(path).resolve()))
    # The passing probe explicitly scanned these exact files before LoadAsset.
    # EditorAssetLibrary requires AssetData; a fresh commandlet may not have
    # discovered this Engine folder even though its packages exist and are valid.
    check('bounded exact probe cloud dependencies to register', 1 <= len(engine_files) <= 64)
    registry = U.AssetRegistryHelpers.get_asset_registry()
    registry.scan_files_synchronous(engine_files, False)
    entries = registry.get_assets_by_package_name(U.Name(source_path), True)
    check('cloud native AssetData after exact file scan', len(entries) == 1
          and str(entries[0].package_name) == source_path,
          [dict(package=str(a.package_name), class_path=str(a.asset_class_path)) for a in entries])
    source = load(source_path)
    source_snapshot = cloud_instance_snapshot(source)
    check('cloud source parameters equal native PASS probe', source_snapshot == probe['instances'][source.get_path_name()])
    check('only verified bounded cloud scalar overrides', set(settings['scalar_overrides']) == {'Layout_CloudGlobalScale', 'Cloud_GlobalCoverage'}
          and 32 <= settings['scalar_overrides']['Layout_CloudGlobalScale'] <= 256
          and -.3 <= settings['scalar_overrides']['Cloud_GlobalCoverage'] <= .05)
    check('cloud layer units are explicit km', 1 <= settings['component']['layer_bottom_altitude'] <= 4
          and 1 <= settings['component']['layer_height'] <= 5)
    parent = source.get_editor_property('parent')
    check('cloud parent is actual Volume material', isinstance(parent, U.Material)
          and parent.get_editor_property('material_domain') == U.MaterialDomain.MD_VOLUME)
    target = ASSET_ROOT + '/Clouds'
    instance_path = target + '/MI_CloudCandidate'
    check('fresh isolated cloud MI', not A.does_asset_exist(instance_path))
    nodes = list(E.get_material_expressions(parent))
    expected_nodes = probe['graphs'][parent.get_path_name()]['nodes']
    check('cloud graph retains native node count', len(nodes) == len(expected_nodes))
    advanced = [n for n in nodes if n.get_class().get_name() == 'MaterialExpressionVolumetricAdvancedMaterialOutput']
    check('one actual advanced volume output', len(advanced) == 1)
    expected_advanced = next(n for n in expected_nodes if n['class_name'] == 'MaterialExpressionVolumetricAdvancedMaterialOutput')['properties']
    advanced_before = {key: advanced[0].get_editor_property(key) for key in expected_advanced}
    check('full-quality advanced volume options match source', advanced_before == expected_advanced
          and advanced_before['multi_scattering_approximation_octave_count'] == 2
          and advanced_before['ray_march_volume_shadow'] is True)
    material = A.duplicate_asset(source.get_path_name(), instance_path)
    check('native isolated cloud MI duplicate', isinstance(material, U.MaterialInstanceConstant))
    check('cloud MI initially matches exact source', cloud_instance_snapshot(material) == source_snapshot)
    for name, value in settings['scalar_overrides'].items():
        # UE5.8 scalar setter returns false even when it sets the value; read back.
        E.set_material_instance_scalar_parameter_value(material, U.Name(name), float(value))
    E.update_material_instance(material)
    expected = json.loads(json.dumps(source_snapshot))
    for name, requested in settings['scalar_overrides'].items():
        actual = float(E.get_material_instance_scalar_parameter_value(material, U.Name(name)))
        check('cloud scalar actual readback', abs(actual-requested) < 1.e-6, dict(name=name, requested=requested, actual=actual))
        expected['effective_parameters']['scalar'][name] = actual
    check('all other cloud parameters textures and switches preserved', cloud_instance_snapshot(material) == expected)
    save_owned(material)
    cloud = spawn('NativeCloudCandidate', U.VolumetricCloud, group='Sky')
    component = cloud.get_component_by_class(U.VolumetricCloudComponent)
    check('actual native volumetric cloud component', component is not None)
    component.set_material(material)
    setters = dict(layer_bottom_altitude='set_layer_bottom_altitude', layer_height='set_layer_height',
        tracing_max_distance='set_tracing_max_distance', view_sample_count_scale='set_view_sample_count_scale',
        reflection_view_sample_count_scale_value='set_reflection_view_sample_count_scale',
        shadow_view_sample_count_scale='set_shadow_view_sample_count_scale',
        shadow_reflection_view_sample_count_scale_value='set_shadow_reflection_view_sample_count_scale')
    check('exact verified cloud component fields', set(settings['component']) == set(setters))
    check('quality-first original sampling multipliers', all(settings['component'][key] == 1.0
          for key in ('view_sample_count_scale', 'reflection_view_sample_count_scale_value',
                      'shadow_view_sample_count_scale', 'shadow_reflection_view_sample_count_scale_value')))
    for key, value in settings['component'].items():
        getattr(component, setters[key])(float(value))
    actual = cloud_component_readback(component)
    check('cloud native component readback', all(abs(actual[k]-v) < 1.e-6 for k,v in settings['component'].items())
          and material.get_path_name() in actual['material_reference'], actual)
    R['cloud_candidate'] = dict(actor=cloud.get_actor_label(), source=source_path, original_master=parent.get_path_name(),
        material=material.get_path_name(), parameters=expected, component_actual=actual,
        unchanged_advanced_output=advanced_before, source_probe=binding(file),
        units='Cloud layer/height/trace distances and Layout_CloudGlobalScale are kilometres; the source graph converts layout kilometres to centimetres.',
        visual='NOT_RUN', performance='NOT_RUN', wind='Inherited source material animation; no gameplay weather controller.',
        scope='One bounded native volume-cloud candidate. Original Engine assets/textures preserved; no accepted cumulus appearance inferred.')
    R['lighting']['sky_policy'] = ('Native SkyAtmosphere and this isolated VolumetricCloud candidate share the tagged AtmosphereSunLight; '
        'real-time SkyLight captures both. The saved default is Afternoon; Dusk is explicitly staged. '
        'Cloud shape, time response and performance require viewport review; no playable weather system is claimed.')


def verify_saved_cloud_candidate(actors):
    clouds = [a for a in actors if a.get_class() == U.VolumetricCloud.static_class()]
    check('one saved native cloud candidate', len(clouds) == 1)
    actual = cloud_component_readback(clouds[0].get_component_by_class(U.VolumetricCloudComponent))
    check('saved native cloud component matches author readback', actual == R['cloud_candidate']['component_actual'], actual)
    material = A.load_asset(R['cloud_candidate']['material'])
    check('saved cloud MI retains exact candidate parameters', cloud_instance_snapshot(material) == R['cloud_candidate']['parameters'])
    R['cloud_candidate']['saved_component_actual'] = actual


def atmosphere_readback(component):
    names = ('rayleigh_scattering_scale', 'mie_scattering_scale', 'mie_anisotropy', 'multi_scattering_factor')
    result = {key: float(component.get_editor_property(key)) for key in names}
    color = component.get_editor_property('sky_luminance_factor')
    result['sky_luminance_factor'] = [float(color.r), float(color.g), float(color.b), float(color.a)]
    return result


def author_people_and_cameras(layout, specimens):
    feet = layout['player_start_feet_cm']
    spawn('PlayerStart', U.PlayerStart, (feet[0], feet[1], feet[2]+R['hero_binding']['capsule_half_height_cm']+4),
          U.Rotator(yaw=layout['player_start_yaw']), 'Gameplay')
    region = spawn('AllowedLandRegion', U.HCM3NavRegion, (-400, 0, 150), group='Gameplay')
    region.set_editor_property('stable_id', U.Name('M5VS2_Corner_AllowedLand'))
    region.set_editor_property('half_extent', U.Vector(1950, 2450, 350))
    for post in layout['npc_posts']:
        sample = post['sample']; spec = specimens[sample]; feet = post['feet_cm']
        actor = spawn('NPC_' + sample + '_' + post['role'], generated_class(spec['blueprint']),
                      (feet[0], feet[1], feet[2]+float(spec['capsule_half_height_cm'])+2),
                      U.Rotator(yaw=post['yaw']), 'NamedNPC')
        actor.set_editor_property('stationary', True)
        actor.set_editor_property('navigation_region', region)
        actor.set_editor_property('stable_id', U.Name('M5VS2_Corner_NPC_' + sample))
        profile = actor.get_editor_property('npc_profile')
        check('exact integrated NPC profile', package(profile.get_path_name()) == package(spec['profile']))
        body = actor.get_component_by_class(U.SkeletalMeshComponent)
        check('exact integrated NPC skeletal mesh', package(body.get_editor_property('skeletal_mesh_asset').get_path_name()) == package(spec['mesh']))
        groups = []
        for pose in profile.get_editor_property('face_groups'):
            groups.append(dict(group=str(pose.get_editor_property('group')), binds=[
                dict(morph=str(b.get_editor_property('morph')), weight=float(b.get_editor_property('weight')))
                for b in pose.get_editor_property('binds')]))
        R['npc_bindings'].append(dict(sample=sample, actor=actor.get_actor_label(), blueprint=package(spec['blueprint']),
            profile=profile.get_path_name(), animation=body.get_editor_property('anim_class').get_path_name(),
            physics=body.get_editor_property('physics_asset_override').get_path_name(),
            humanoid_bones={str(k):str(v) for k,v in profile.get_editor_property('humanoid_bones').items()}, face_groups=groups,
            talkable=bool(actor.get_editor_property('talkable')), stationary=True,
            behavior_scope='Inherited animation/face/dialogue bindings. No M3Experience or NavMesh is added by this art pass; full combat/navigation gate NOT_RUN.'))
    R['capture_camera_plan'] = []
    for row in layout['cameras']:
        location, target = row['location_cm'], row['target_cm']
        delta = [target[i]-location[i] for i in range(3)]
        rotation = U.Rotator(pitch=math.degrees(math.atan2(delta[2], math.hypot(delta[0], delta[1]))),
                             yaw=math.degrees(math.atan2(delta[1], delta[0])))
        camera = spawn('ReviewCamera_' + row['name'], U.CameraActor, location, rotation, 'ReviewCamera')
        camera.get_component_by_class(U.CameraComponent).set_field_of_view(row['fov'])
        R['capture_camera_plan'].append(dict(**row, actor=camera.get_actor_label(), screenshot='NOT_RUN',
                                            scope='Passive native camera bookmark; no input simulation or automatic view switching.'))


try:
    dump()
    check('explicit HarborCorner author phase', argument('M5AuthorPhase') == 'HarborCorner')
    check('exact HarborCity project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE and no pre-existing dirty maps', not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('native GeometryScript functions reflected', hasattr(U, 'GeometryScript_Primitives') and hasattr(U, 'GeometryScript_NewAssetUtils'))
    layout, assemblies, specimens, mode = resolve_inputs()
    world = prepare_map(mode)
    author_street(layout)
    author_houses(layout, assemblies)
    author_props(layout)
    author_environment(layout)
    author_people_and_cameras(layout, specimens)
    spawn('ReviewDirector', U.HCM5VS2CornerReviewDirector, group='ReviewCamera')
    check('exact two NPC posts and four complete static houses', len(R['npc_bindings']) == 2 and len(R['houses']) == 4)
    check('no automated test or foreign behavior actor', not any(
        (('Director' in a.get_class().get_name() and a.get_class() != U.HCM5VS2CornerReviewDirector.static_class()) or a.get_class().get_name() == 'HCM3Experience'
         or a.get_class().get_path_name().startswith(SOURCE_PACK)) for a in CREATED))
    expected_labels = sorted(a.get_actor_label() for a in CREATED)
    check('native save fixed new corner map', LEVEL.save_current_level())
    check('new corner map exists on disk', disk(MAP, '.umap').is_file())
    R['assets'].append(dict(path=MAP, sha256=sha(disk(MAP, '.umap'))))
    check('reload actual saved corner map', LEVEL.load_level(MAP))
    reloaded_world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('saved corner ownership and GameMode', A.get_metadata_tag(reloaded_world, KEY) == OWNER
          and reloaded_world.get_world_settings().get_editor_property('default_game_mode') == mode)
    saved_actors = [a for a in ACT.get_all_level_actors() if a.actor_has_tag(U.Name(OWNER))]
    check('every authored actor persisted once', sorted(a.get_actor_label() for a in saved_actors) == expected_labels)
    saved_atmospheres = [a for a in saved_actors if a.get_class() == U.SkyAtmosphere.static_class()]
    check('exact native atmosphere persisted with no old day sphere', len(saved_atmospheres) == 1
          and not any(a.get_actor_label().endswith('OriginalDaySkySphere') for a in saved_actors))
    R['lighting']['saved_atmosphere_actual'] = atmosphere_readback(saved_atmospheres[0].get_component_by_class(U.SkyAtmosphereComponent))
    check('saved atmosphere properties match authored readback', R['lighting']['saved_atmosphere_actual'] == R['lighting']['atmosphere_actual'])
    verify_saved_cloud_candidate(saved_actors)
    verify_saved_material_bindings(saved_actors)
    check('55 exact source geometry components persisted', sum(len(h['components']) for h in R['houses']) == 55)
    check('all copied assets, existing Hero/NPC packages, previous maps and config byte-preserved',
          all(Path(p).is_file() and sha(p) == value for p, value in PROTECTED.items()), len(PROTECTED))
    R['protected_input_file_count'] = len(PROTECTED)
    R['actor_count'] = len(saved_actors)
    R['runtime_arguments'] = [MAP, '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-language=en',
        '-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_Corner_UNIQUE']
    R['pass_scope'] = 'Native asset author/save and map save/reload/reference binding only. This is a review candidate; no rendered quality, physics, navigation, input, gameplay or art acceptance is claimed.'
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc(); U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS':
    raise RuntimeError('Corner author failed; preserve author_result.json and any partial new packages')
