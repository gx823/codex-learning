"""Bounded read-only UE 5.8 cloud / saved Corner lighting / Village MI probe.

Root runs ue_m5_vs2_author_run.ps1 -Phase CloudProbe -ScriptPath this_file.
No spawn, setters, graph edits, asset save, compile request, game launch or capture.
The owned saved Corner is opened only to inspect its native lighting components.
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
CLOUD = '/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst'
MAP = '/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0'
OWNER = 'HarborCity_M5_VS2_HarborCorner_P0'
VILLAGE = '/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc'
STYLE_NAMES = ('MI_wall_02', 'MI_wood_05', 'MI_rooftiles_01', 'MI_stonebrick_02')
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
AR = U.AssetRegistryHelpers.get_asset_registry()
LIMITS = dict(packages=160, graphs=48, nodes=3000, parameters=512, json_bytes=8*1024*1024)


def argument(name):
    rows = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(rows) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return rows[0][0] or rows[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and OUT != DOC and not (OUT/'author_result.json').exists()
assert argument('M5AuthorPhase') == 'CloudProbe'
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase='CloudProbe', scope='READ_ONLY_NATIVE_CLOUD_AND_SAVED_CORNER',
         asset_writes=0, actors_spawned=0, screenshots_created=0, runtime='NOT_RUN', visual='NOT_RUN',
         cloud_source=CLOUD, checks=[], instances={}, graphs={}, dependencies={}, limits=LIMITS,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), limitations=[
             'Parameters and graph edges are native reads; cloud shape, light response, shader readiness and performance require real rendering.',
             'Saved Corner lighting is editor-world data, not a simulated Dusk capture or weather system.',
             'Hard and soft package references are recorded separately; searchable names and management references are not load dependencies.',
             'Village opaque material parameters are inspected for later style candidates; no material or style is applied.',
             'Expression links retain names and upstream nodes; a reused upstream node can have ambiguous output-pin identity.'])
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
    raise RuntimeError('Read exceeds fixed cloud/Village/Corner scope: ' + path)


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
        row['properties'] = props(obj, ('material_domain', 'blend_mode', 'shading_model', 'two_sided', 'use_material_attributes'))
        row['outputs'] = {name: value(E.get_material_property_input_node(obj, getattr(U.MaterialProperty, name)))
                          for name in ('MP_BASE_COLOR', 'MP_EMISSIVE_COLOR', 'MP_OPACITY', 'MP_NORMAL', 'MP_MATERIAL_ATTRIBUTES')}
    for node in nodes:
        kind = node.get_class().get_name()
        names = list(E.get_material_expression_input_names(node))
        links = list(E.get_inputs_for_material_expression(obj, node) if material else E.get_inputs_for_material_function_expression(obj, node))
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


def latest_corner():
    for file in sorted((DOC/'editor_runtime').glob('author_*_HarborCorner/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS' and data.get('map') == MAP:
            matches = [a for a in data.get('assets', []) if a.get('path') == MAP]
            check('one actual saved Corner map hash', len(matches) == 1, str(file))
            check('saved Corner matches latest PASS author', sha(protect(MAP)) == matches[0]['sha256'])
            R['corner_author'] = dict(path=str(file), sha256=sha(file), saved_lighting=data.get('lighting'))
            return
    raise RuntimeError('No actual PASS Corner author')


def scene_lighting():
    latest_corner()
    level = U.get_editor_subsystem(U.LevelEditorSubsystem)
    check('read-only load owned saved Corner', level.load_level(MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact owned current world', package(world.get_path_name()) == MAP and A.get_metadata_tag(world, 'HarborCityOwnedBy') == OWNER)
    actors = U.get_editor_subsystem(U.EditorActorSubsystem).get_all_level_actors()
    if len(actors) > 1500:
        raise RuntimeError('Bounded Corner actor count exceeded')
    def one(kind):
        found = [a for a in actors if a.get_class() == kind.static_class() and a.actor_has_tag(U.Name(OWNER))]
        check('one tagged native ' + kind.__name__, len(found) == 1)
        return found[0]
    sun, sky, atmosphere, post = [one(kind) for kind in (U.DirectionalLight, U.SkyLight, U.SkyAtmosphere, U.PostProcessVolume)]
    sun_comp = sun.get_component_by_class(U.DirectionalLightComponent)
    sky_comp = sky.get_component_by_class(U.SkyLightComponent)
    atmosphere_comp = atmosphere.get_component_by_class(U.SkyAtmosphereComponent)
    R['saved_scene'] = dict(map=MAP, actor_count=len(actors),
        sun=dict(actor=sun.get_path_name(), rotation=value(sun.get_actor_rotation()), light_color_linear=value(sun_comp.get_light_color()),
            properties=props(sun_comp, ('intensity', 'mobility', 'atmosphere_sun_light', 'atmosphere_sun_light_index',
                'cast_cloud_shadows', 'cloud_shadow_strength', 'cloud_shadow_on_surface_strength',
                'cloud_shadow_on_atmosphere_strength', 'cloud_shadow_extent', 'cloud_scattered_luminance_scale'))),
        sky=dict(actor=sky.get_path_name(), properties=props(sky_comp, ('intensity', 'mobility', 'source_type',
            'real_time_capture', 'lower_hemisphere_is_black', 'cloud_ambient_occlusion', 'cloud_ambient_occlusion_strength'))),
        atmosphere=dict(actor=atmosphere.get_path_name(), properties=props(atmosphere_comp, ('rayleigh_scattering_scale',
            'mie_scattering_scale', 'mie_anisotropy', 'multi_scattering_factor', 'sky_luminance_factor'))),
        post=dict(actor=post.get_path_name(), settings=props(post.get_editor_property('settings'),
            ('auto_exposure_method', 'auto_exposure_bias', 'auto_exposure_apply_physical_camera_exposure', 'bloom_intensity'))),
        existing_cloud_actors=[a.get_path_name() for a in actors if a.get_class() == U.VolumetricCloud.static_class()])
    check('actual sun drives atmosphere', bool(sun_comp.get_editor_property('atmosphere_sun_light')))
    check('actual SkyLight captures real-time scene', bool(sky_comp.get_editor_property('real_time_capture')))


try:
    check('exact HarborCity project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE', not U.EditorLevelLibrary.get_pie_worlds(False))
    R['dirty_before'] = dirty()
    check('clean commandlet packages before read', not any(R['dirty_before'].values()))
    R['engine_version'] = U.SystemLibrary.get_engine_version()
    roots = [CLOUD] + [VILLAGE + '/materials/' + name for name in STYLE_NAMES]
    dependency_closure(roots)
    R['parent_chains'] = {path: inspect_instance(path) for path in roots}
    while GRAPH_QUEUE:
        graph(GRAPH_QUEUE.pop(0))
    terminal = load(R['parent_chains'][CLOUD][-1])
    check('actual cloud material domain is Volume', terminal.get_editor_property('material_domain') == U.MaterialDomain.MD_VOLUME)
    default = U.get_default_object(U.VolumetricCloudComponent.static_class())
    R['cloud_component_actual_CDO'] = props(default, ('layer_bottom_altitude', 'layer_height',
        'tracing_start_max_distance', 'tracing_start_distance_from_camera', 'tracing_max_distance',
        'view_sample_count_scale', 'reflection_view_sample_count_scale_value', 'shadow_view_sample_count_scale',
        'shadow_reflection_view_sample_count_scale_value', 'shadow_tracing_distance',
        'sky_light_cloud_bottom_occlusion', 'use_per_sample_atmospheric_light_transmittance', 'visible_in_real_time_sky_captures'))
    R['units'] = dict(layer_bottom_altitude='km above ground', layer_height='km above layer bottom',
        tracing_start_max_distance='km', tracing_start_distance_from_camera='km', tracing_max_distance='km within cloud layer',
        shadow_tracing_distance='km', atmosphere_scattering_scale='absolute extinction/scattering coefficient in 1/km, not relative multiplier')
    scene_lighting()
    R['dirty_after'] = dirty()
    check('read-only package dirty flags unchanged', R['dirty_after'] == R['dirty_before'], R['dirty_after'])
    changed = [file for file, digest in PROTECTED.items() if not Path(file).is_file() or sha(file) != digest]
    check('all loaded selected source bytes unchanged', not changed, changed)
    R['node_count'] = NODE_COUNT
    R['status'] = 'PASS'
    R['pass_scope'] = 'Native parameter/graph/dependency/saved-lighting readback only; no rendered cloud acceptance.'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['protected_source_sha256'] = PROTECTED
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    try:
        dump()
        (OUT/'cloud_probe.json').write_text((OUT/'author_result.json').read_text(encoding='utf-8'), encoding='utf-8')
    except Exception:
        failure = dict(status='FAIL', phase='CloudProbe', error=traceback.format_exc(), asset_writes=0)
        (OUT/'author_result.json').write_text(json.dumps(failure, ensure_ascii=False, indent=2), encoding='utf-8')
        R['status'] = 'FAIL'
if R['status'] != 'PASS':
    raise RuntimeError('Read-only CloudProbe failed; inspect author_result.json')
