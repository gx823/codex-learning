"""Bounded read-only UE 5.8 water graph / saved Corner sea probe.

Root runs ue_m5_vs2_author_run.ps1 -Phase WaterProbe -ScriptPath this_file.
No spawn, setters, graph edits, asset save, compile request, game launch or capture.
The owned saved Corner is opened only to inspect its existing SeaSurface.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
ENGINE = Path('E:/UE_5.8/Engine/Content')
WATER = '/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc/materials/MI_ENV_water'
MAP = '/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0'
OWNER = 'HarborCity_M5_VS2_HarborCorner_P0'
VILLAGE = '/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc'
PLANE = '/Engine/BasicShapes/Plane'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
AR = U.AssetRegistryHelpers.get_asset_registry()
LIMITS = dict(packages=96, graphs=32, nodes=1600, parameters=512, json_bytes=8*1024*1024)


def argument(name):
    rows = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(rows) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return rows[0][0] or rows[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and OUT != DOC and not (OUT/'author_result.json').exists()
assert argument('M5AuthorPhase') == 'WaterProbe'
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase='WaterProbe', scope='READ_ONLY_NATIVE_WATER_AND_SAVED_CORNER',
         asset_writes=0, actors_spawned=0, screenshots_created=0, runtime='NOT_RUN', visual='NOT_RUN',
         water_source=WATER, checks=[], instances={}, graphs={}, dependencies={}, textures={}, limits=LIMITS,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), limitations=[
             'Native graph and saved scene readback only; horizontal band cause and candidate appearance require rendered comparison.',
             'No scalar value is changed. Physical tile lengths will be derived from the actual graph, not parameter names.',
             'A duplicated upstream link can make the convenience API output-pin result ambiguous; per-input raw property exports are retained when accessible.',
             'If source mesh UV reflection is unavailable, this is explicitly reported; no synthetic UV values are used.',
             'Package hashes and dirty flags guard all read sources; no asset save or material compilation is requested.'])

PROTECTED, GRAPH_QUEUE, GRAPH_SEEN = {}, [], set()
NODE_COUNT = 0


def dump():
    data = json.dumps(R, ensure_ascii=False, indent=2) + '\n'
    if len(data.encode('utf-8')) > LIMITS['json_bytes']:
        raise RuntimeError('Bounded report size exceeded')
    (OUT/'author_result.json').write_text(data, encoding='utf-8')


def check(name, passed, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if passed else 'FAIL', observed=observed))
    dump()
    if not passed:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def package(path):
    return str(path).split('.')[0]


def disk(path):
    path = package(path)
    if '..' in path:
        raise RuntimeError('Invalid package path')
    if path.startswith('/Engine/'):
        return ENGINE / (path.removeprefix('/Engine/') + '.uasset')
    if path.startswith(VILLAGE + '/'):
        return PROJECT/'Content'/(path.removeprefix('/Game/') + '.uasset')
    if path == MAP:
        return PROJECT/'Content'/(path.removeprefix('/Game/') + '.umap')
    raise RuntimeError('Read exceeds fixed water/Village/Corner scope: ' + path)


def protect(path):
    file = disk(path)
    if not file.is_file():
        raise RuntimeError('Native referenced package missing on disk: ' + str(file))
    if str(file) not in PROTECTED:
        PROTECTED[str(file)] = sha(file)
    return file


def load(path):
    protect(path)
    obj = A.load_asset(path)
    if obj is None:
        raise RuntimeError('Native load failed: ' + str(path))
    return obj


def value(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, U.LinearColor):
        return [float(v.r), float(v.g), float(v.b), float(v.a)]
    if isinstance(v, U.Rotator):
        return [float(v.pitch), float(v.yaw), float(v.roll)]
    if isinstance(v, U.Vector):
        return [float(v.x), float(v.y), float(v.z)]
    if isinstance(v, U.Vector2D):
        return [float(v.x), float(v.y)]
    if isinstance(v, (list, tuple)):
        return [value(item) for item in v]
    if isinstance(v, U.Object):
        return v.get_path_name()
    return str(v)


def props(obj, names):
    return {name: value(obj.get_editor_property(name)) for name in names}


def dirty():
    return {name: sorted(p.get_path_name() for p in fn()) for name, fn in (
        ('maps', U.EditorLoadingAndSavingUtils.get_dirty_map_packages),
        ('content', U.EditorLoadingAndSavingUtils.get_dirty_content_packages))}


def enqueue(obj):
    path = obj.get_path_name()
    protect(path)
    if path not in GRAPH_SEEN:
        GRAPH_SEEN.add(path)
        GRAPH_QUEUE.append(obj)


def inspect_instance(path):
    obj, chain = load(path), []
    while isinstance(obj, U.MaterialInstanceConstant):
        identity = obj.get_path_name()
        if identity in chain or len(chain) >= 8:
            raise RuntimeError('Invalid bounded MI parent chain')
        chain.append(identity)
        protect(identity)
        bindings = {}
        for kind in ('scalar', 'vector', 'texture', 'static_switch'):
            names = getattr(E, 'get_' + kind + '_parameter_names')(obj)
            if len(names) > LIMITS['parameters']:
                raise RuntimeError('Parameter cap exceeded')
            getter = getattr(E, 'get_material_instance_' + kind + '_parameter_value')
            bindings[kind] = {str(n): value(getter(obj, n)) for n in names}
        parent = obj.get_editor_property('parent')
        R['instances'][identity] = dict(parent=value(parent), effective_parameters=bindings,
            base_property_overrides=obj.get_editor_property('base_property_overrides').export_text())
        obj = parent
    if not isinstance(obj, U.Material):
        raise RuntimeError('Expected terminal native Material')
    chain.append(obj.get_path_name())
    enqueue(obj)
    return chain


def graph(obj):
    global NODE_COUNT
    if len(R['graphs']) >= LIMITS['graphs']:
        raise RuntimeError('Graph cap exceeded')
    material = isinstance(obj, U.Material)
    if not material and not isinstance(obj, U.MaterialFunction):
        raise RuntimeError('Unsupported function interface; no partial PASS: ' + obj.get_path_name())
    nodes = list(E.get_material_expressions(obj) if material else E.get_material_function_expressions(obj))
    NODE_COUNT += len(nodes)
    if NODE_COUNT > LIMITS['nodes']:
        raise RuntimeError('Node cap exceeded')
    row = dict(class_name=obj.get_class().get_name(), nodes=[])
    R['graphs'][obj.get_path_name()] = row
    if material:
        row['properties'] = props(obj, ('material_domain', 'blend_mode', 'shading_model', 'two_sided', 'use_material_attributes',
            'translucency_lighting_mode', 'use_translucency_vertex_fog', 'compute_fog_per_pixel',
            'apply_cloud_fogging', 'screen_space_reflections', 'refraction_method'))
        row['outputs'] = {name: value(E.get_material_property_input_node(obj, getattr(U.MaterialProperty, name)))
                          for name in ('MP_BASE_COLOR', 'MP_EMISSIVE_COLOR', 'MP_OPACITY', 'MP_NORMAL', 'MP_MATERIAL_ATTRIBUTES',
                                       'MP_ROUGHNESS', 'MP_SPECULAR', 'MP_METALLIC', 'MP_REFRACTION', 'MP_WORLD_POSITION_OFFSET')}
        row['output_source_pins'] = {name: str(E.get_material_property_input_node_output_name(obj, getattr(U.MaterialProperty, name)))
                                     for name, upstream in row['outputs'].items() if upstream is not None}
    for node in nodes:
        kind = node.get_class().get_name()
        names = list(E.get_material_expression_input_names(node))
        links = list(E.get_inputs_for_material_expression(obj, node) if material else E.get_inputs_for_material_function_expression(obj, node))
        if len(names) != len(links):
            raise RuntimeError('Native graph input names/links mismatch: ' + node.get_path_name())
        entry = dict(id=node.get_name(), class_name=kind, input_name_count=len(names), input_link_count=len(links),
                     inputs=[dict(index=i, name=str(names[i]) if i < len(names) else None,
                                  upstream=link.get_name() if link else None) for i, link in enumerate(links)])
        if kind == 'MaterialExpressionMaterialFunctionCall':
            target = node.get_editor_property('material_function')
            if target is None:
                raise RuntimeError('Null function link')
            entry['function'] = value(target)
            enqueue(target)
        elif kind in ('MaterialExpressionScalarParameter', 'MaterialExpressionVectorParameter', 'MaterialExpressionStaticSwitchParameter'):
            entry['properties'] = props(node, ('parameter_name', 'default_value'))
        elif 'TextureSampleParameter' in kind or kind == 'MaterialExpressionTextureObjectParameter':
            entry['properties'] = props(node, ('parameter_name', 'texture', 'sampler_type'))
        elif kind in ('MaterialExpressionTextureSample', 'MaterialExpressionTextureObject'):
            entry['properties'] = props(node, ('texture', 'sampler_type'))
        elif kind == 'MaterialExpressionConstant':
            entry['properties'] = props(node, ('r',))
        elif kind in ('MaterialExpressionConstant3Vector', 'MaterialExpressionConstant4Vector'):
            entry['properties'] = props(node, ('constant',))
        elif kind == 'MaterialExpressionLinearInterpolate':
            entry['properties'] = props(node, ('const_a', 'const_b', 'const_alpha'))
        elif kind == 'MaterialExpressionVolumetricAdvancedMaterialOutput':
            entry['properties'] = props(node, ('const_phase_g', 'const_phase_g2', 'const_phase_blend',
                'per_sample_phase_evaluation', 'multi_scattering_approximation_octave_count',
                'const_multi_scattering_contribution', 'const_multi_scattering_occlusion',
                'const_multi_scattering_eccentricity', 'ground_contribution', 'gray_scale_material',
                'ray_march_volume_shadow', 'clamp_multi_scattering_contribution'))
        elif kind == 'MaterialExpressionCustom':
            code = str(node.get_editor_property('code'))
            entry['custom_code_sha256'] = hashlib.sha256(code.encode('utf-8')).hexdigest()
            entry['custom_code_chars'] = len(code)
        enrich_node(node, entry, links)
        row['nodes'].append(entry)


def dependency_closure(roots):
    pending, visited = list(roots), set()
    options = [U.AssetRegistryDependencyOptions(include_hard_package_references=hard,
        include_soft_package_references=not hard, include_searchable_names=False,
        include_hard_management_references=False, include_soft_management_references=False) for hard in (True, False)]
    while pending:
        path = package(pending.pop(0))
        if path in visited:
            continue
        visited.add(path)
        if len(visited) > LIMITS['packages']:
            raise RuntimeError('Dependency closure cap exceeded')
        file = protect(path)
        AR.scan_files_synchronous([str(file)], False)
        assets = AR.get_assets_by_package_name(U.Name(path), True)
        if not assets:
            raise RuntimeError('Native registry cannot resolve existing package: ' + path)
        row = dict(bytes=file.stat().st_size, sha256=PROTECTED[str(file)],
                   classes=sorted(set(str(a.asset_class_path) for a in assets)))
        for label, option in zip(('hard', 'soft'), options):
            row[label] = sorted(set(str(n) for n in AR.get_dependencies(U.Name(path), option)))
            if len(row[label]) > LIMITS['packages']:
                raise RuntimeError('Dependency fanout cap exceeded')
            pending.extend(p for p in row[label] if not p.startswith('/Script/') and p not in visited)
        R['dependencies'][path] = row


def enrich_node(node, entry, links):
    kind = entry['class_name']
    typed = {
        'MaterialExpressionTextureCoordinate': ('coordinate_index', 'u_tiling', 'v_tiling', 'un_mirror_u', 'un_mirror_v'),
        'MaterialExpressionPanner': ('speed_x', 'speed_y', 'const_coordinate', 'fractional_part'),
        'MaterialExpressionWorldPosition': ('world_position_shader_offset',),
        'MaterialExpressionDepthFade': ('opacity_default', 'fade_distance_default'),
        'MaterialExpressionSceneDepth': ('input_mode', 'const_input'),
        'MaterialExpressionComponentMask': ('r', 'g', 'b', 'a'),
        'MaterialExpressionPower': ('const_exponent',),
        'MaterialExpressionSine': ('period',),
        'MaterialExpressionCosine': ('period',),
        'MaterialExpressionFresnel': ('exponent', 'base_reflect_fraction'),
        'MaterialExpressionClamp': ('clamp_mode', 'min_default', 'max_default'),
        'MaterialExpressionConstant2Vector': ('r', 'g'),
        'MaterialExpressionFunctionInput': ('input_name', 'input_type', 'preview_value', 'use_preview_value_as_default'),
        'MaterialExpressionFunctionOutput': ('output_name',),
    }
    if kind in typed:
        entry.setdefault('properties', {}).update(props(node, typed[kind]))
    if kind in ('MaterialExpressionMultiply', 'MaterialExpressionDivide', 'MaterialExpressionAdd', 'MaterialExpressionSubtract'):
        entry.setdefault('properties', {}).update(props(node, ('const_a', 'const_b')))
    if isinstance(node, U.MaterialExpressionTextureSample):
        entry.setdefault('properties', {}).update(props(node, ('const_coordinate', 'mip_value_mode', 'sampler_source', 'automatic_view_mip_bias')))
    entry['output_names'] = [str(n) for n in E.get_material_expression_output_names(node)]
    # This convenience function searches by upstream object, so repeated use of
    # the same upstream may only return its first output. Do not conceal that.
    for edge, link in zip(entry['inputs'], links):
        if link is not None:
            edge['upstream_output_api'] = value(E.get_input_node_output_name_for_material_expression(node, link))
            edge['upstream_repeated'] = sum(x == link for x in links) > 1
    raw = {}
    # FExpressionInput is a reflected UPROPERTY, but some fields are not exposed
    # by Python. These optional exact-pin exports supplement the native links.
    native_fields = {
        'MaterialExpressionPanner': ('coordinate', 'time', 'speed'),
        'MaterialExpressionTextureSample': ('coordinates', 'texture_object', 'mip_value', 'coordinates_dx', 'coordinates_dy'),
        'MaterialExpressionTextureSampleParameter2D': ('coordinates', 'texture_object', 'mip_value', 'coordinates_dx', 'coordinates_dy'),
        'MaterialExpressionDepthFade': ('in_opacity', 'fade_distance'),
        'MaterialExpressionSceneDepth': ('input',),
        'MaterialExpressionComponentMask': ('input',),
        'MaterialExpressionPower': ('base', 'exponent'),
        'MaterialExpressionLinearInterpolate': ('a', 'b', 'alpha'),
        'MaterialExpressionMultiply': ('a', 'b'),
        'MaterialExpressionDivide': ('a', 'b'),
        'MaterialExpressionAdd': ('a', 'b'),
        'MaterialExpressionSubtract': ('a', 'b'),
    }
    for name in native_fields.get(kind, ()):
        try:
            raw[name] = dict(status='READ', export_text=node.get_editor_property(name).export_text())
        except Exception as error:
            raw[name] = dict(status='UNAVAILABLE_IN_PYTHON', reason=str(error)[:500])
    if raw:
        entry['raw_input_properties'] = raw


def inspect_textures():
    paths = set()
    for instance in R['instances'].values():
        paths.update(p for p in instance['effective_parameters']['texture'].values() if p)
    for row in R['graphs'].values():
        for node in row['nodes']:
            tex = node.get('properties', {}).get('texture')
            if tex:
                paths.add(tex)
    check('bounded water textures', len(paths) <= 24)
    for path in sorted(paths):
        tex = load(path)
        check('water graph has Texture2D inputs', isinstance(tex, U.Texture2D), path)
        R['textures'][path] = dict(size=[tex.blueprint_get_size_x(), tex.blueprint_get_size_y()],
            properties=props(tex, ('srgb', 'compression_settings', 'filter', 'address_x', 'address_y',
                                  'mip_gen_settings', 'lod_bias', 'never_stream')))


def latest_corner():
    for file in sorted((DOC/'editor_runtime').glob('author_*_HarborCorner/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS' and data.get('map') == MAP:
            matches = [a for a in data.get('assets', []) if a.get('path') == MAP]
            check('one actual saved Corner map hash', len(matches) == 1, str(file))
            check('saved Corner matches latest PASS author', sha(protect(MAP)) == matches[0]['sha256'])
            check('saved author uses exact copied water MI', package(data['lighting']['water_material']) == WATER)
            check('water scalars match saved author readback',
                  R['instances'][WATER + '.MI_ENV_water']['effective_parameters']['scalar'] == data['lighting']['water_scalar_parameters'])
            R['corner_author'] = dict(path=str(file), sha256=sha(file), saved_lighting=data['lighting'],
                sea_coverage=data['sea_coverage'], map_sha256=matches[0]['sha256'])
            return
    raise RuntimeError('No actual PASS Corner author')


def scene_sea():
    latest_corner()
    level = U.get_editor_subsystem(U.LevelEditorSubsystem)
    check('read-only load owned saved Corner', level.load_level(MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact owned current world', package(world.get_path_name()) == MAP and A.get_metadata_tag(world, 'HarborCityOwnedBy') == OWNER)
    actors = U.get_editor_subsystem(U.EditorActorSubsystem).get_all_level_actors()
    check('bounded Corner actor count', len(actors) <= 1500, len(actors))
    found = [actor for actor in actors if actor.get_actor_label() == 'HC_M5VS2_Corner_SeaSurface'
             and actor.actor_has_tag(U.Name(OWNER)) and actor.get_class() == U.StaticMeshActor.static_class()]
    check('one exact project-owned native SeaSurface', len(found) == 1)
    actor = found[0]
    component = actor.get_component_by_class(U.StaticMeshComponent)
    mesh = component.get_editor_property('static_mesh')
    check('SeaSurface still native Engine plane', mesh is not None and package(mesh.get_path_name()) == PLANE)
    protect(PLANE)
    materials = [component.get_material(i).get_path_name() if component.get_material(i) else None
                 for i in range(component.get_num_materials())]
    check('one original unchanged water slot', materials == [WATER + '.MI_ENV_water'], materials)
    check('sea collision stays disabled', component.get_collision_enabled() == U.CollisionEnabled.NO_COLLISION)
    description = mesh.get_static_mesh_description(0)
    check('read native plane mesh description', description is not None)
    plane = dict(vertices=description.get_vertex_count(), vertex_instances=description.get_vertex_instance_count(),
                 triangles=description.get_triangle_count(), uv0_status='NOT_READ')
    # Optional UV rows: no setter on the mesh or any UObject is called. The ID
    # wrapper is a transient value type; native getter bounds are checked first.
    try:
        count = plane['vertex_instances']
        if count > 64:
            raise RuntimeError('Plane UV read cap exceeded')
        rows = []
        for index in range(count):
            element = U.VertexInstanceID()
            element.import_text('(IDValue=' + str(index) + ')')
            if not description.is_vertex_instance_valid(element):
                raise RuntimeError('Plane uses sparse IDs; no guessed replacement')
            vertex = description.get_vertex_instance_vertex(element)
            rows.append(dict(id=index, local_position_cm=value(description.get_vertex_position(vertex)),
                             uv0=value(description.get_vertex_instance_uv(element, 0))))
        plane.update(uv0_status='READ_NATIVE', uv0_rows=rows)
    except Exception as error:
        plane.update(uv0_status='UNAVAILABLE_IN_PYTHON', uv0_reason=str(error)[:800])
    R['saved_sea'] = dict(actor=actor.get_path_name(), map=MAP,
        location_cm=value(actor.get_actor_location()), rotation=value(actor.get_actor_rotation()),
        scale=value(actor.get_actor_scale3d()), component_transform=component.get_world_transform().export_text(),
        mesh=mesh.get_path_name(), materials=materials, collision=value(component.get_collision_enabled()),
        local_bounds=[value(v) for v in component.get_local_bounds()], plane=plane)


try:
    check('exact HarborCity project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE', not U.EditorLevelLibrary.get_pie_worlds(False))
    R['dirty_before'] = dirty()
    check('clean commandlet packages before read', not any(R['dirty_before'].values()))
    R['engine_version'] = U.SystemLibrary.get_engine_version()
    dependency_closure([WATER])
    R['parent_chains'] = {WATER: inspect_instance(WATER)}
    while GRAPH_QUEUE:
        graph(GRAPH_QUEUE.pop(0))
    inspect_textures()
    scene_sea()
    R['dirty_after'] = dirty()
    check('read-only package dirty flags unchanged', R['dirty_after'] == R['dirty_before'], R['dirty_after'])
    changed = [file for file, digest in PROTECTED.items() if not Path(file).is_file() or sha(file) != digest]
    check('all loaded selected source bytes unchanged', not changed, changed)
    R['node_count'] = NODE_COUNT
    R['status'] = 'PASS'
    R['pass_scope'] = 'Native water graph, dependencies and saved sea readback only; no rendered diagnosis or material acceptance.'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['protected_source_sha256'] = PROTECTED
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    try:
        dump()
        (OUT/'water_probe.json').write_text((OUT/'author_result.json').read_text(encoding='utf-8'), encoding='utf-8')
    except Exception:
        failure = dict(status='FAIL', phase='WaterProbe', error=traceback.format_exc(), asset_writes=0)
        (OUT/'author_result.json').write_text(json.dumps(failure, ensure_ascii=False, indent=2), encoding='utf-8')
        R['status'] = 'FAIL'
if R['status'] != 'PASS':
    raise RuntimeError('Read-only WaterProbe failed; inspect author_result.json')

