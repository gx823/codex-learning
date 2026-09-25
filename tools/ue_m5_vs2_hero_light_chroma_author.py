"""Project art candidate: isolate only the self-lit MToon light-color multiplier.

Native UE 5.8 author only; the root operator runs Phase HeroLightChroma.
Preserves the original Hero/LightLimits/plugin assets, material shadow function,
painted textures and all stage lights. Three strengths use one comparison map.
A native author PASS is not shader/runtime/visual acceptance.
"""
from pathlib import Path
import copy
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
HERO = '/Game/HarborCity/M5VS2/HeroSelestia'
CHARACTER = HERO + '/BP_M5VS2_Selestia'
MODE = HERO + '/BP_M5VS2_SelestiaGameMode'
SOURCE_MAP = '/Game/HarborCity/M5VS2/Lookdev/L_SelestiaLookdev'
SOURCE_OWNER = 'HarborCity_M5_VS2_SelestiaLookdev'
OWNER = 'HarborCity_M5_VS2_HeroLightChroma'
LIMIT_OWNER = 'HarborCity_M5_VS2_MToonLightLimits'
KEY = 'HarborCityOwnedBy'
SLOTS = {'hair', 'body', 'face', 'option', 'costume'}
PRESETS = ('Neutral', 'Dusk', 'NightMoonProxy')
RESPONSES = (('Chroma_000', 0.), ('Chroma_050', .5), ('Chroma_100', 1.))
PARAMETER = 'HeroLightChroma'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    values = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(values) == 1, 'Exactly one ' + name + ' required'
    return values[0][0] or values[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC / 'editor_runtime') and not (OUT / 'author_result.json').exists()
assert argument('M5AuthorPhase') == 'HeroLightChroma'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
DEST = '/Game/HarborCity/M5VS2/HeroLightChroma/Review_' + TOKEN
MAP = DEST + '/L_HeroLightChroma_' + TOKEN
FUNC = DEST + '/Materials/Core/MF_HeroLightChroma'
CODE = """const float3 W = float3(0.2126, 0.7152, 0.0722);
const float Yc = dot(C, W);
const float K = saturate(Strength);
if (K <= 0.0 || Yc <= 0.000001) return Light;
const float Grey = dot(C * Light, W) / Yc;
return lerp(Light, float3(Grey, Grey, Grey), K);"""
R = dict(status='RUNNING', phase='HeroLightChroma', owner=OWNER,
         map=MAP, target_directory=DEST, source_map=SOURCE_MAP,
         checks=[], assets=[], inputs={}, materials={}, shot_plan=[],
         runtime='NOT_RUN', shader_readiness='NOT_RUN_REQUIRES_NON_NULL_RHI', visual='USER_REVIEW',
         default_map_changed=False, original_assets_modified=False,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         semantic_scope='PROJECT_ART_CANDIDATE_NOT_ORIGINAL_UNITY_ATTENUATION',
         limitations=['Only the self-lit directional/sky multiplier changes, not all deferred scene lighting.',
                      'Original Unity lilToon MonochromeLighting=0; this is an explicit project art candidate.',
                      'Rec709 linear luminance is preserved before exposure and tonemapping; perceived post-tonemap luminance can differ.',
                      'C luminance <=1e-6 returns the original light to avoid ill-conditioned division.',
                      'No point light is authored; original Hero and neutral stage sources are used.',
                      'Live animation/blinking continues across sequential captures; these are not identical frozen poses.',
                      'Hidden Material root inputs cannot be inspected separately by Python; native duplication retains them.'])
protected, originals, copies, paths, graphs, snapshots = {}, {}, {}, {}, {}, {}
NEW_INSTANCES = []


def dump():
    text = json.dumps(R, ensure_ascii=False, indent=2)
    (OUT / 'author_result.json').write_text(text, encoding='utf-8')
    (OUT / 'hero_light_chroma_author.json').write_text(text, encoding='utf-8')


def check(name, passed, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if passed else 'FAIL', observed=observed))
    if not passed:
        dump()
        raise RuntimeError(name + ': ' + repr(observed))


def require(name, passed):
    if not passed:
        check(name, False)


def sha(file):
    return hashlib.sha256(Path(file).read_bytes()).hexdigest()


def package(obj):
    return (obj.get_path_name() if isinstance(obj, U.Object) else str(obj)).split('.')[0]


def disk(obj, extension='.uasset'):
    name = package(obj)
    for prefix, folder in (('/Game/', PROJECT / 'Content'), ('/VRM4U/', PROJECT / 'Plugins/VRM4U/Content'), ('/Engine/', Path('E:/UE_5.8/Engine/Content'))):
        if name.startswith(prefix):
            return folder / (name[len(prefix):] + extension)
    raise RuntimeError('Unrecognized package mount: ' + name)


def protect_file(file, expected=None):
    file = Path(file).resolve()
    allowed = (PROJECT / 'Content', PROJECT / 'Plugins/VRM4U/Content', Path('E:/UE_5.8/Engine/Content'))
    check('protected asset file scope', file.is_file() and file.suffix in ('.uasset', '.umap')
          and any(file.is_relative_to(p.resolve()) for p in allowed), str(file))
    h = sha(file)
    if expected is not None:
        check('actual asset hash matches PASS author', h == expected, str(file))
    protected[str(file)] = h


def load(path):
    obj = A.load_asset(path)
    check('native asset exists', obj is not None, path)
    return obj


def latest_pass(pattern, predicate):
    for file in sorted((DOC / 'editor_runtime').glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS' and predicate(data):
            return file, data
    raise RuntimeError('No actual PASS dependency: ' + pattern)


MATERIAL_INPUT_CATALOG = (
    ('MP_EmissiveColor', False), ('MP_Opacity', False), ('MP_OpacityMask', False),
    ('MP_BaseColor', False), ('MP_Metallic', False), ('MP_Specular', False),
    ('MP_Roughness', False), ('MP_Anisotropy', False), ('MP_Normal', False),
    ('MP_Tangent', False), ('MP_WorldPositionOffset', False),
    ('MP_Displacement', True), ('MP_SubsurfaceColor', False),
    ('MP_CustomData0', True), ('MP_CustomData1', True),
    ('MP_AmbientOcclusion', False), ('MP_Refraction', False),
    ('MP_MaterialAttributes', False), ('MP_PixelDepthOffset', True),
    ('MP_ShadingModel', True), ('MP_SurfaceThickness', True),
    ('MP_FrontMaterial', False),
) + tuple(('MP_CustomizedUVs' + str(i), True) for i in range(8))

def value(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, U.Object):
        return v.get_path_name()
    if isinstance(v, U.LinearColor):
        return [float(getattr(v, k)) for k in ('r', 'g', 'b', 'a')]
    return str(v)

def dirty():
    return set(p.get_path_name() for fn in (
        U.EditorLoadingAndSavingUtils.get_dirty_content_packages,
        U.EditorLoadingAndSavingUtils.get_dirty_map_packages) for p in fn())

def instance_snapshot(mi):
    params = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        names = getattr(E, 'get_' + kind + '_parameter_names')(mi)
        require('bounded material parameters', len(names) < 512)
        getter = getattr(E, 'get_material_instance_' + kind + '_parameter_value')
        params[kind] = {str(n): value(getter(mi, n)) for n in names}
    # StructBase.export_text exports every reflected override field, including
    # disabled fields; no incomplete hand-maintained list of effective overrides.
    return dict(parent=package(mi.get_editor_property('parent')), parameters=params,
                all_base_overrides=mi.get_editor_property('base_property_overrides').export_text())

def nodes(obj):
    items = (E.get_material_expressions(obj) if isinstance(obj, U.Material)
             else E.get_material_function_expressions(obj))
    require('bounded graph', len(items) < 1200)
    result = {n.get_name(): n for n in items}
    require('unique expression names', len(result) == len(items))
    return result

def links(obj, node):
    names = list(E.get_material_expression_input_names(node))
    upstreams = list(E.get_inputs_for_material_expression(obj, node) if isinstance(obj, U.Material)
                     else E.get_inputs_for_material_function_expression(obj, node))
    require('input enumeration complete ' + node.get_name(), len(names) == len(upstreams))
    return [dict(name=str(name), source=up.get_name() if up else None,
                 first_matching_output=value(E.get_input_node_output_name_for_material_expression(node, up)) if up else None)
            for name, up in zip(names, upstreams)]

def reflected_material_inputs():
    # Name normalization accommodates PythonizeName's digit/acronym underscores
    # without guessing attributes or constructing non-reflected enum integers.
    normalized = lambda name: name.replace('_', '').upper()
    reflected = {}
    for name in sorted(dir(U.MaterialProperty)):
        if name.startswith('MP_'):
            key = normalized(name)
            require('unambiguous reflected MaterialProperty name', key not in reflected)
            reflected[key] = name
    inspection, accessible = [], []
    for native, hidden in MATERIAL_INPUT_CATALOG:
        name = reflected.get(normalized(native))
        if name is None:
            # A missing public entry is an API mismatch; never silently weaken
            # the 16 public-output assertions to get a passing author.
            require('expected public material input reflected ' + native, hidden)
            inspection.append(dict(native=native, python=None, status='NOT_INSPECTED',
                                   reason='UMETA(Hidden); Python enum entry not reflected'))
        else:
            accessible.append((native, name, getattr(U.MaterialProperty, name)))
            inspection.append(dict(native=native, python=name, status='INSPECTED'))
    supported = {normalized(native) for native, _ in MATERIAL_INPUT_CATALOG}
    R['material_output_reflection'] = dict(
        supported_canonical_count=len(MATERIAL_INPUT_CATALOG),
        inspected_count=len(accessible),
        inputs=inspection,
        other_reflected_members_not_queried=[name for key, name in reflected.items() if key not in supported],
        rule='Only canonical GetExpressionInputDescription-supported entries present in the actual Python enum are queried.')
    return accessible, inspection

def graph_snapshot(obj):
    result = {'nodes': {}, 'material_outputs': {}}
    for name, n in nodes(obj).items():
        row = dict(kind=n.get_class().get_name(), inputs=links(obj, n),
                   outputs=[str(x) for x in E.get_material_expression_output_names(n)])
        if isinstance(n, U.MaterialExpressionMaterialFunctionCall):
            row['function'] = package(n.get_editor_property('material_function'))
        if isinstance(n, U.MaterialExpressionClamp):
            row['clamp'] = {k: value(n.get_editor_property(k)) for k in ('clamp_mode', 'min_default', 'max_default')}
        result['nodes'][name] = row
    if isinstance(obj, U.Material):
        accessible, inspection = reflected_material_inputs()
        result['material_output_inspection'] = inspection
        for native_name, enum_name, prop in accessible:
            n = E.get_material_property_input_node(obj, prop)
            result['material_outputs'][enum_name] = dict(node=n.get_name() if n else None,
                output=E.get_material_property_input_node_output_name(obj, prop))
    return result

def restore_function_call_inputs(source, node_name, expected_inputs):
    """Repair lost callee inputs, without accepting any other graph delta."""
    obj = copies[source]
    ns = nodes(obj)
    call = ns[node_name]
    actual_inputs = links(obj, call)
    record = dict(source=source, candidate=paths[source], node=node_name,
                  expected_inputs=copy.deepcopy(expected_inputs),
                  after_function_setter=copy.deepcopy(actual_inputs), restored=[])
    R.setdefault('function_input_restoration', []).append(record)
    names = [row['name'] for row in expected_inputs]
    check('function input names remain unique and unchanged ' + source,
          len(names) == len(set(names)) and [row['name'] for row in actual_inputs] == names,
          dict(expected=names, actual=[row['name'] for row in actual_inputs]))
    for wanted, found in zip(expected_inputs, actual_inputs):
        if wanted == found:
            continue
        # A rewired input, unexpected new connection, or ambiguous output is a
        # failure. Only restore an original connected edge that became empty.
        check('only disconnected original function input ' + wanted['name'],
              wanted['source'] is not None and wanted['source'] in ns
              and isinstance(wanted['first_matching_output'], str)
              and found['source'] is None and found['first_matching_output'] is None,
              dict(expected=wanted, actual=found))
        upstream = ns[wanted['source']]
        outputs = [str(name) for name in E.get_material_expression_output_names(upstream)]
        check('lost function input has one exact source output ' + wanted['source'],
              outputs == [wanted['first_matching_output']], outputs)
        check('restore original function input ' + source + ':' + wanted['name'],
              E.connect_material_expressions(upstream, outputs[0], call, wanted['name']))
        record['restored'].append(dict(input=wanted['name'], upstream=wanted['source'], output=outputs[0]))
    record['after_restoration'] = links(obj, call)
    check('all original callee input edges restored ' + source,
          record['after_restoration'] == expected_inputs, record)

def bounded_graph_diff(expected, actual, limit=64):
    """Diagnostic only: exact leaf values, bounded output, no normalization."""
    missing = object()
    rows, total = [], 0

    def encoded(v):
        return json.dumps(v, ensure_ascii=False, sort_keys=True, separators=(',', ':'))

    def describe(v):
        if v is missing:
            return dict(present=False)
        serialized = encoded(v)
        if len(serialized) <= 2048:
            return dict(present=True, value=v, value_exact=True)
        return dict(present=True, value_exact=False, serialized_characters=len(serialized),
                    sha256=hashlib.sha256(serialized.encode('utf-8')).hexdigest(),
                    serialized_prefix=serialized[:2048])

    def visit(wanted, found, pointer):
        nonlocal total
        if wanted is not missing and found is not missing and wanted == found:
            return
        if isinstance(wanted, dict) and isinstance(found, dict):
            for key in sorted(set(wanted) | set(found)):
                part = str(key).replace('~', '~0').replace('/', '~1')
                visit(wanted.get(key, missing), found.get(key, missing), pointer + '/' + part)
            return
        if isinstance(wanted, list) and isinstance(found, list):
            for index in range(max(len(wanted), len(found))):
                visit(wanted[index] if index < len(wanted) else missing,
                      found[index] if index < len(found) else missing,
                      pointer + '/' + str(index))
            return
        total += 1
        if len(rows) < limit:
            rows.append(dict(json_pointer=pointer, expected=describe(wanted), actual=describe(found)))

    visit(expected, actual, '')
    return dict(difference_count=total, recorded_count=len(rows), truncated=total > len(rows),
                record_limit=limit, differences=rows,
                expected_sha256=hashlib.sha256(encoded(expected).encode('utf-8')).hexdigest(),
                actual_sha256=hashlib.sha256(encoded(actual).encode('utf-8')).hexdigest())

def verify_graph(source, expected):
    actual = graph_snapshot(copies[source])
    same = actual == expected
    observed = None
    if not same:
        differences = bounded_graph_diff(expected, actual)
        differences['source'] = source
        differences['candidate'] = paths[source]
        R.setdefault('graph_differences', []).append(differences)
        observed = dict(report_field='graph_differences', report_index=len(R['graph_differences']) - 1,
                        difference_count=differences['difference_count'],
                        recorded_count=differences['recorded_count'], truncated=differences['truncated'])
    check('only allowed graph delta ' + source, same, observed)
    return actual

def class_of(path):
    cls = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('native generated class load', cls is not None, path)
    return cls

def native_struct(kind, values):
    obj = kind()
    for name, value in values.items():
        obj.set_editor_property(name, value)
    return obj

def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]

def rotation(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]

def color(value):
    return [float(value.r), float(value.g), float(value.b), float(value.a)]

def shot_readback(shot):
    get = shot.get_editor_property
    return dict(label=str(get('label')), variant_index=int(get('variant_index')),
                camera_location=xyz(get('camera_location')), camera_rotation=rotation(get('camera_rotation')),
                use_player_eye_height=bool(get('use_player_eye_height')), light_direction=rotation(get('light_direction')),
                light_color=color(get('light_color')), light_intensity=float(get('light_intensity')),
                ambient_color=color(get('ambient_color')))

def variant_readback(variant):
    return dict(label=str(variant.get_editor_property('label')),
                use_original_materials=bool(variant.get_editor_property('use_original_materials')),
                materials=[m.get_path_name() for m in variant.get_editor_property('materials')])


def math_check():
    weights = (.2126, .7152, .0722)
    dot = lambda a, b: sum(x*y for x, y in zip(a, b))
    maximum = 0.
    count = 0
    for c in ((0., 0., 0.), (1., 1., 1.), (.8, .35, .4), (.3, .06, .01), (.001, .03, .2)):
        for light in ((.05, .05, .05), (1., 1., 1.), (1., .52, .28), (.05, .1, .28), (.8, .05, .05)):
            for _, strength in RESPONSES:
                yc = dot(c, weights)
                grey = dot([x*y for x, y in zip(c, light)], weights)/yc if yc > 1.e-6 else 0.
                adjusted = light if strength == 0 or yc <= 1.e-6 else tuple((1-strength)*x+strength*grey for x in light)
                error = abs(dot([x*y for x, y in zip(c, light)], weights)-dot([x*y for x, y in zip(c, adjusted)], weights))
                maximum = max(maximum, error)
                check('formula preserves linear luminance and observed input range', error < 1.e-12
                      and all(min(light)-1.e-12 <= x <= max(light)+1.e-12 for x in adjusted))
                count += 1
    return dict(samples=count, maximum_luminance_error=maximum, weights=list(weights),
                strength_values=[v for _, v in RESPONSES], shader_validation='NOT_RUN',
                formula='Yc=dot(C,w); g=dot(C*L,w)/Yc; Lprime=lerp(L,g.xxx,k); k=0 or Yc<=1e-6 returns L')


def create_chroma_function():
    check('fresh isolated function', not A.does_asset_exist(FUNC) and not disk(FUNC).exists())
    obj = U.AssetToolsHelpers.get_asset_tools().create_asset(FUNC.rsplit('/', 1)[1], FUNC.rsplit('/', 1)[0],
                                                           U.MaterialFunction, U.MaterialFunctionFactoryNew())
    check('new native function', isinstance(obj, U.MaterialFunction) and package(obj) == FUNC)
    A.set_metadata_tag(obj, KEY, OWNER)
    check('new function begins empty', len(nodes(obj)) == 0)
    inputs = {}
    for i, name in enumerate(('C', 'Light', 'Strength')):
        n = E.create_material_expression_in_function(obj, U.MaterialExpressionFunctionInput, -600, i*150)
        check('new function input', n is not None)
        n.set_editor_property('input_name', name)
        n.set_editor_property('input_type', U.FunctionInputType.FUNCTION_INPUT_SCALAR if name == 'Strength'
                              else U.FunctionInputType.FUNCTION_INPUT_VECTOR3)
        n.set_editor_property('sort_priority', i)
        n.set_editor_property('use_preview_value_as_default', False)
        inputs[name] = n
    custom = E.create_material_expression_in_function(obj, U.MaterialExpressionCustom, -250, 0)
    check('new bounded arithmetic expression', custom is not None)
    custom_inputs = []
    for name in inputs:
        entry = U.CustomInput()
        entry.set_editor_property('input_name', name)
        custom_inputs.append(entry)
    custom.set_editor_property('inputs', custom_inputs)
    custom.set_editor_property('output_type', U.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('code', CODE)
    for name, n in inputs.items():
        check('connect function input ' + name, E.connect_material_expressions(n, '', custom, name))
    output = E.create_material_expression_in_function(obj, U.MaterialExpressionFunctionOutput, 100, 0)
    check('new function output', output is not None)
    output.set_editor_property('output_name', 'LightColor')
    output.set_editor_property('sort_priority', 0)
    check('connect only new function output', E.connect_material_expressions(custom, '', output, 'None'))
    E.update_material_function(obj)
    check('exact five-node chroma function', len(nodes(obj)) == 5)
    R['chroma_function'] = dict(path=FUNC, custom_node=custom.get_name(), code=CODE,
                              code_sha256=hashlib.sha256(CODE.encode('utf-8')).hexdigest(),
                              inputs={name: node.get_name() for name, node in inputs.items()},
                              graph=graph_snapshot(obj))
    return obj


def make_stage(configured, slots, baseline, body, capsule, mode, source_report):
    native = U.load_class(None, '/Script/HarborCity.HCM5VS2LookdevDirector')
    check('existing native Director', native is not None)
    check('fresh native comparison map', not A.does_asset_exist(MAP) and not disk(MAP, '.umap').exists())
    check('source owned stage', A.get_metadata_tag(load(SOURCE_MAP), KEY) == SOURCE_OWNER)
    check('create new template map', LEVEL.new_level_from_template(MAP, SOURCE_MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('new world namespace', package(world) == MAP)
    A.set_metadata_tag(world, KEY, OWNER)
    actors = ACT.get_all_level_actors()
    allowed = {'StaticMeshActor', 'DirectionalLight', 'SkyLight', 'SkyAtmosphere', 'PostProcessVolume', 'PlayerStart', 'HCM5VS2LookdevDirector'}
    for actor in actors:
        if actor.get_class().get_name() in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume'):
            continue
        check('only known owned stage actors', actor.actor_has_tag(U.Name(SOURCE_OWNER))
              and actor.get_class().get_name() in allowed, actor.get_actor_label())
        check('no point light in source stage actor', not actor.get_components_by_class(U.PointLightComponent), actor.get_actor_label())
        actor.set_editor_property('tags', [U.Name(OWNER)])
    directors = [a for a in actors if a.get_class() == native]
    starts = [a for a in actors if isinstance(a, U.PlayerStart)]
    check('one copied Director and spawn', len(directors) == len(starts) == 1)
    director = directors[0]
    original = [variant_readback(v) for v in director.get_editor_property('material_variants')]
    ids = [i for i, v in enumerate(original) if v['label'] == 'MToon']
    check('one MToon reference variant', len(ids) == 1)
    index = ids[0]
    check('template material slot order', [package(p) for p in original[index]['materials']] == [baseline[s] for s in slots])
    location = starts[0].get_actor_location()
    check('source spawn geometry retained', abs(location.x) < .001 and abs(location.y) < .001
          and abs(starts[0].get_actor_rotation().yaw) < .001
          and abs(location.z-capsule.get_unscaled_capsule_half_height()-2.) < .1)
    world.get_world_settings().set_editor_property('default_game_mode', mode)
    source_shots = list(director.get_editor_property('shots'))
    shots = []
    for preset in PRESETS:
        matches = [s for s in source_shots if s.get_editor_property('label') == preset + '_FaceFront'
                   and s.get_editor_property('variant_index') == index]
        check('one inherited preset ' + preset, len(matches) == 1)
        reference = matches[0]
        for variant in range(3):
            fields = {k: reference.get_editor_property(k) for k in ('camera_location', 'camera_rotation', 'use_player_eye_height',
                      'light_direction', 'light_color', 'light_intensity', 'ambient_color')}
            fields.update(label=preset + '_FaceFront', variant_index=variant)
            shots.append(native_struct(U.HCM5VS2LookdevShot, fields))
    shot_values = [shot_readback(s) for s in shots]
    check('exact nine same-camera shots', len(shots) == 9 and all((s['camera_location'], s['camera_rotation'], s['use_player_eye_height'])
          == (shot_values[0]['camera_location'], shot_values[0]['camera_rotation'], True) for s in shot_values))
    light = director.get_editor_property('directional_light')
    check('light references copied stage only', light is not None and light.get_path_name().startswith(MAP + '.'))
    director.set_actor_label('HC_M5VS2_HeroLightChroma_Director')
    director.set_editor_property('material_variants', configured)
    director.set_editor_property('shots', shots)
    director.set_editor_property('use_player_eye_height', True)
    director.set_editor_property('hide_hud', True)
    variants = [variant_readback(v) for v in configured]
    R['shot_plan'] = [dict(index=i, **s, variant=variants[s['variant_index']]['label'],
                          expected_png='%03d_%s_%s.png' % (i, s['label'], variants[s['variant_index']]['label'])) for i, s in enumerate(shot_values)]
    R['stage'] = dict(source_author=source_report, point_lights_authored=0,
                      sky_exposure_geometry='Unchanged native source template', actual_directional_light=light.get_path_name())
    check('save only new complete map', LEVEL.save_current_level())
    check('reload new map', LEVEL.load_level(MAP))
    current = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    saved = [a for a in ACT.get_all_level_actors() if a.get_class() == native]
    check('saved map owner and mode', package(current) == MAP and A.get_metadata_tag(current, KEY) == OWNER
          and current.get_world_settings().get_editor_property('default_game_mode') == mode and len(saved) == 1)
    check('nine saved shots exact readback', [shot_readback(s) for s in saved[0].get_editor_property('shots')] == shot_values)
    check('three saved variants exact readback', [variant_readback(v) for v in saved[0].get_editor_property('material_variants')] == variants)
    R['assets'].append(dict(path=MAP, source=SOURCE_MAP, sha256=sha(disk(MAP, '.umap')), bytes=disk(MAP, '.umap').stat().st_size))


def main():
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    before_dirty = dirty()
    check('no PIE or dirty source packages', not U.EditorLevelLibrary.get_pie_worlds(False) and not before_dirty, sorted(before_dirty))
    R['formula_checks'] = math_check()
    R['inputs']['script'] = dict(path=str(Path(__file__)), sha256=sha(Path(__file__)))
    report_file, limits = latest_pass('author_*_MToonLightLimits/author_result.json',
        lambda d: set(d.get('variants', {}).get('MToonLightLimits', {})) == SLOTS)
    R['inputs']['light_limits_author'] = dict(path=str(report_file), sha256=sha(report_file))
    check('actual PASS dependency checks and preserved originals', all(x['status'] == 'PASS' for x in limits['checks'])
          and limits['protected_source_sha256_before'] == limits['protected_source_sha256_after'])
    for file, h in limits['protected_source_sha256_after'].items():
        protect_file(file, h)
    assets = {package(x['path']): x for x in limits['assets']}
    mapping = limits['source_to_candidate']
    check('actual twelve-asset private LightLimits closure', len(assets) == len(mapping) == 12 and set(mapping.values()) == set(assets))
    for path, asset in assets.items():
        check('source candidate namespace', path.startswith(HERO + '/Materials/MToonLightLimits_')
              and path.startswith(limits['target_directory'] + '/'), path)
        protect_file(disk(path), asset['sha256'])
    suffix = '/VRM4U/MaterialUtil/UE5/'
    root = mapping[suffix + 'Material/M_VrmMToonBaseOpaque']
    base = mapping[suffix + 'MaterialFunction/MF_VrmMToonBase']
    basecolor = mapping[suffix + 'MaterialFunction/MF_BaseColor']
    parents = [mapping[suffix + 'Material/' + name] for name in ('MI_VrmMToonBaseLitOpaque', 'MI_VrmMToonOptLitOpaque',
              'MI_VrmMToonOptLitTranslucent', 'MI_VrmMToonOptLitTranslucentTwoSided')]
    finals = {s: package(limits['variants']['MToonLightLimits'][s]) for s in SLOTS}
    core = [basecolor, base, root] + parents
    for src in core + list(finals.values()):
        originals[src] = load(src)
        check('source owner', A.get_metadata_tag(originals[src], KEY) == LIMIT_OWNER, src)
    check('actual graph classes', isinstance(originals[root], U.Material)
          and all(isinstance(originals[p], U.MaterialFunction) for p in (basecolor, base)))
    for src in parents + list(finals.values()):
        snapshots[src] = instance_snapshot(originals[src])
    for i, src in enumerate(parents):
        check('exact existing Lit inheritance', snapshots[src]['parent'] == (root if i == 0 else parents[i-1]), src)
    for slot, src in finals.items():
        s = snapshots[src]
        check('exact final slot inheritance', s['parent'] == (parents[3] if slot in ('hair', 'option') else parents[1]), slot)
        for name, value_ in (('HeroLightMin', .05), ('HeroLightMax', 1.), ('mtoon_LightColorAttenuation', .1)):
            check('native inherited light limit/attenuation', abs(s['parameters']['scalar'][name]-value_) < 1.e-6, (slot, name, s['parameters']['scalar'][name]))
        check('original lighting split retained', s['parameters']['static_switch']['bUseLight']
              and not s['parameters']['static_switch']['bUseCustomBaseColorRate']
              and PARAMETER not in s['parameters']['scalar'], slot)
    for src in (root, base, basecolor):
        graphs[src] = graph_snapshot(originals[src])
    base_nodes = nodes(originals[base])
    check('exact self-lit final multiplier class', isinstance(base_nodes['MaterialExpressionMultiply_78'], U.MaterialExpressionMultiply))
    wanted = [('A', 'MaterialExpressionReroute_17'), ('B', 'MaterialExpressionStaticSwitchParameter_5')]
    check('exact self-lit branch before exposure', [(i['name'], i['source']) for i in links(originals[base], base_nodes['MaterialExpressionMultiply_78'])] == wanted)
    check('exact original illumination switch', str(base_nodes['MaterialExpressionStaticSwitchParameter_5'].get_editor_property('parameter_name')) == 'bUseLightForce')
    for src in (base, basecolor):
        ns = nodes(originals[src])
        clamp = ns['MaterialExpressionClamp_1']
        lp = links(originals[src], clamp)
        check('native bounds retained with connected scalar parameters', [p['name'] for p in lp] == ['None', 'Min', 'Max']
              and lp[0]['source'] == 'MaterialExpressionAdd_7')
        for item, expected_name, expected_value in zip(lp[1:], ('HeroLightMin', 'HeroLightMax'), (.05, 1.)):
            n = ns[item['source']]
            check('actual bound connection and value', isinstance(n, U.MaterialExpressionScalarParameter)
                  and str(n.get_editor_property('parameter_name')) == expected_name
                  and abs(float(n.get_editor_property('default_value'))-expected_value) < 1.e-6)
        check('actual world directional input', 'ResolvedView.DirectionalLightColor' in ns['MaterialExpressionCustom_0'].get_editor_property('code'))
    evidence = DOC / 'research/SELESTIA_SOURCE_AUDIT.json'
    audit = json.loads(evidence.read_text(encoding='utf-8'))
    R['inputs']['source_material_audit'] = dict(path=str(evidence), sha256=sha(evidence))
    R['original_material_limits'] = [dict(name=m.get('name', m.get('path')), shader=m.get('resolved_shader', {}).get('name'),
        values={k:v for k,v in m.get('floats', {}).items() if k in ('_LightMinLimit', '_LightMaxLimit', '_MonochromeLighting', '_LightColorAttenuation')})
        for m in audit['materials'] if '_LightMinLimit' in m.get('floats', {})]
    source_file, source_report = latest_pass('author_*_Lookdev*/author_result.json', lambda d: d.get('map') == SOURCE_MAP)
    R['inputs']['lookdev_author'] = dict(path=str(source_file), sha256=sha(source_file))
    source_assets = {package(a['path']): a for a in source_report['assets']}
    check('source map bound by native PASS', SOURCE_MAP in source_assets)
    for path, asset in source_assets.items():
        protect_file(disk(path, '.umap' if path == SOURCE_MAP else '.uasset'), asset['sha256'])
    hero, mode = class_of(CHARACTER), class_of(MODE)
    check('original Hero GameMode', U.get_default_object(mode).get_editor_property('default_pawn_class') == hero)
    cdo = U.get_default_object(hero)
    check('no Hero point-light template', not cdo.get_components_by_class(U.PointLightComponent))
    body, capsule = cdo.get_component_by_class(U.SkeletalMeshComponent), cdo.get_component_by_class(U.CapsuleComponent)
    check('native body/capsule', body is not None and capsule is not None)
    mesh = body.get_editor_property('skeletal_mesh_asset')
    check('exact normalized source mesh', package(mesh) == '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia')
    slots = [str(s.get_editor_property('material_slot_name')).removeprefix('Selestia_') for s in mesh.get_editor_property('materials')]
    check('five distinct actual mesh slots', len(slots) == 5 and set(slots) == SLOTS)
    original_interfaces = [body.get_material(i).get_path_name() for i in range(body.get_num_materials())]
    animation = body.get_editor_property('anim_class')
    check('saved Hero animation class', animation is not None)
    for path in (CHARACTER, MODE, package(mesh), package(animation).removesuffix('_C')):
        protect_file(disk(path))
    for src in core:
        paths[src] = DEST + '/Materials/Core/' + src.rsplit('/', 1)[1]
        check('fresh copied dependency', not A.does_asset_exist(paths[src]) and not disk(paths[src]).exists())
    R['protected_source_sha256_before'] = protected.copy()
    dump()

    for src in core:
        copies[src] = A.duplicate_asset(src, paths[src])
        check('native dependency duplication', copies[src] is not None and package(copies[src]) == paths[src])
        A.set_metadata_tag(copies[src], KEY, OWNER)
        A.set_metadata_tag(copies[src], 'SourceAsset', src)
    expected_graphs = copy.deepcopy(graphs)
    for src, name, target in ((root, 'MaterialExpressionMaterialFunctionCall_2', base), (base, 'MaterialExpressionMaterialFunctionCall_5', basecolor)):
        check('exact source function link', graphs[src]['nodes'][name]['function'] == target)
        check('replace only copied function reference', nodes(copies[src])[name].set_material_function(copies[target]))
        expected_graphs[src]['nodes'][name]['function'] = paths[target]
        restore_function_call_inputs(src, name, graphs[src]['nodes'][name]['inputs'])
    chroma = create_chroma_function()
    function = copies[base]
    ns = nodes(function)
    scalar = E.create_material_expression_in_function(function, U.MaterialExpressionScalarParameter, -200, -900)
    check('new per-instance project control', scalar is not None)
    scalar.set_editor_property('parameter_name', PARAMETER)
    scalar.set_editor_property('default_value', 0.)
    call = E.create_material_expression_in_function(function, U.MaterialExpressionMaterialFunctionCall, 0, -900)
    check('new function call', call is not None and call.set_material_function(chroma))
    for up, pin in ((ns['MaterialExpressionReroute_17'], 'C'), (ns['MaterialExpressionStaticSwitchParameter_5'], 'Light'), (scalar, 'Strength')):
        check('unambiguous single source output', list(E.get_material_expression_output_names(up)) == [''])
        check('connect isolated chroma input ' + pin, E.connect_material_expressions(up, '', call, pin))
    check('only original input replacement', E.connect_material_expressions(call, 'LightColor', ns['MaterialExpressionMultiply_78'], 'B'))
    actual = graph_snapshot(function)
    expected_graphs[base]['nodes'][scalar.get_name()] = actual['nodes'][scalar.get_name()]
    expected_graphs[base]['nodes'][call.get_name()] = actual['nodes'][call.get_name()]
    expected_graphs[base]['nodes']['MaterialExpressionMultiply_78']['inputs'][1] = dict(name='B', source=call.get_name(), first_matching_output='LightColor')
    R['single_original_edge_change'] = dict(function=paths[base], node='MaterialExpressionMultiply_78', input='B',
        before='MaterialExpressionStaticSwitchParameter_5', after=call.get_name(), other_old_edges_unchanged=True)
    for src in parents:
        E.set_material_instance_parent(copies[src], copies[snapshots[src]['parent']])
    E.update_material_function(copies[basecolor])
    E.update_material_function(copies[base])
    E.recompile_material(copies[root])
    for src in parents:
        E.update_material_instance(copies[src])
    for src in (root, base, basecolor):
        verify_graph(src, expected_graphs[src])

    configured = []
    for label, strength in RESPONSES:
        materials = []
        for slot in slots:
            src = finals[slot]
            target = DEST + '/Materials/' + label + '/MI_MToon_' + slot
            check('fresh final instance', not A.does_asset_exist(target) and not disk(target).exists())
            mi = A.duplicate_asset(src, target)
            check('exact new instance duplication', isinstance(mi, U.MaterialInstanceConstant) and package(mi) == target)
            check('unmodified MI duplicate readback', instance_snapshot(mi) == snapshots[src])
            E.set_material_instance_parent(mi, copies[snapshots[src]['parent']])
            setter_result = E.set_material_instance_scalar_parameter_value(mi, U.Name(PARAMETER), strength)
            E.update_material_instance(mi)
            expected = copy.deepcopy(snapshots[src])
            expected['parent'] = paths[expected['parent']]
            observed = float(E.get_material_instance_scalar_parameter_value(mi, U.Name(PARAMETER)))
            check('native chroma scalar applied regardless of UE false setter return', abs(observed-strength) < 1.e-6,
                  dict(path=target, requested=strength, actual=observed, setter_return=setter_result))
            expected['parameters']['scalar'][PARAMETER] = observed
            check('only project parameter and private parent changed', instance_snapshot(mi) == expected)
            A.set_metadata_tag(mi, KEY, OWNER)
            A.set_metadata_tag(mi, 'SourceAsset', src)
            NEW_INSTANCES.append(dict(path=target, object=mi, expected=expected, source=src))
            materials.append(mi)
        configured.append(native_struct(U.HCM5VS2MaterialVariant, dict(label=label, use_original_materials=False, materials=materials)))
        R['materials'][label] = [dict(slot=s, path=package(m), strength=strength) for s, m in zip(slots, materials)]
    for src in parents:
        expected = copy.deepcopy(snapshots[src]); expected['parent'] = paths[expected['parent']]
        expected['parameters']['scalar'][PARAMETER] = 0.
        check('parent inherited settings retained', instance_snapshot(copies[src]) == expected, paths[src])
    check('exact asset budget before map', len(core)+1+len(NEW_INSTANCES) == 23)
    for src, obj in [(None, chroma)] + [(src, copies[src]) for src in core] + [(x['source'], x['object']) for x in NEW_INSTANCES]:
        path = package(obj)
        check('save limited to new private namespace', path.startswith(DEST + '/'))
        check('save native candidate only', A.save_loaded_asset(obj, False), path)
        R['assets'].append(dict(path=path, source=src, sha256=sha(disk(path)), bytes=disk(path).stat().st_size))
    baseline = {s: assets[finals[s]]['source'] for s in slots}
    make_stage(configured, slots, baseline, body, capsule, mode, R['inputs']['lookdev_author'])
    for x in NEW_INSTANCES:
        check('saved instances unchanged after map reload', instance_snapshot(load(x['path'])) == x['expected'])
    for src in (root, base, basecolor):
        verify_graph(src, expected_graphs[src])
        check('original graph unchanged in memory', graph_snapshot(originals[src]) == graphs[src])
    for src, expected in snapshots.items():
        check('original MI unchanged in memory', instance_snapshot(originals[src]) == expected)
    check('saved new function graph unchanged', graph_snapshot(load(FUNC)) == R['chroma_function']['graph'])
    check('saved HLSL formula exact', str(nodes(load(FUNC))[R['chroma_function']['custom_node']].get_editor_property('code')) == CODE)
    check('all protected bytes unchanged', all(Path(p).is_file() and sha(p) == h for p, h in protected.items()))
    check('original Hero CDO interfaces unchanged', [body.get_material(i).get_path_name() for i in range(body.get_num_materials())] == original_interfaces)
    check('no dirty source packages', not [p for p in dirty()-before_dirty if not p.startswith(DEST + '/')])
    check('saved output hashes remain exact', all(sha(disk(x['path'], '.umap' if x['path'] == MAP else '.uasset')) == x['sha256'] for x in R['assets']))
    R['protected_source_sha256_after'] = {p:sha(p) for p in protected}
    R['source_to_candidate_dependencies'] = paths
    R['runtime_arguments'] = [MAP, '-game', '-M5VS2Lookdev', '-M5VS2EvidenceDir=<fresh absolute VS2 docs directory>', '-M5VS2AutoQuit', '-ResX=1920', '-ResY=1080']
    R['pass_scope'] = 'Native isolated 23 material/function assets and one nine-shot map; graph/settings/hash/save readback only. No rendered acceptance.'
    R['status'] = 'PASS'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    R['partial_map_retained'] = MAP if disk(MAP, '.umap').is_file() else None
    R['retry_policy'] = 'Preserve failed namespace and report; use fresh root-wrapper evidence directory.'
    R['source_hash_changes_on_failure'] = [p for p,h in protected.items() if not Path(p).is_file() or sha(p) != h]
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('HeroLightChroma author failed; preserve actual evidence and partial private assets')

