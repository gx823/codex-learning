"""Author six isolated MToon LightLimits color-response comparisons only.

Root runs ue_m5_vs2_author_run.ps1 -Phase HeroColorResponse -ScriptPath this_file.
Requires an actual PASS MToonLightLimits author and the saved full Lookdev stage.
No runtime launch, source map save, Hero BP/material edit or shader acceptance.
Each fresh wrapper evidence directory produces a unique map, including retries.
"""
from pathlib import Path
import copy
import datetime as dt
import hashlib
import json
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
OWNER = 'HarborCity_M5_VS2_HeroColorResponse'
LIMIT_OWNER = 'HarborCity_M5_VS2_MToonLightLimits'
KEY = 'HarborCityOwnedBy'
SLOTS = {'hair', 'body', 'face', 'option', 'costume'}
PRESETS = ('Neutral', 'Dusk')
PARAMETER = 'mtoon_LightColorAttenuation'
RESPONSES = (('Attenuation_010', .1), ('Attenuation_050', .5), ('Attenuation_080', .8))
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    values = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(values) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return values[0][0] or values[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and OUT != DOC and not (OUT / 'author_result.json').exists()
assert argument('M5AuthorPhase') == 'HeroColorResponse'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
MAP = '/Game/HarborCity/M5VS2/HeroColorResponse/Review_' + TOKEN + '/L_HeroColorResponse_' + TOKEN
MATERIAL_ROOT = MAP.rsplit('/', 1)[0] + '/Materials'
R = dict(status='RUNNING', phase='HeroColorResponse', owner=OWNER, map=MAP, source_map=SOURCE_MAP,
         checks=[], assets=[], inputs={}, materials={}, shot_plan=[], runtime='NOT_RUN', visual='USER_REVIEW',
         default_map_changed=False, blueprint_materials_changed=False, screenshots_created=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         limitations=['An isolated candidate comparison; this does not select the final Hero material.',
                      'Only Neutral and Dusk from the same proven stage; no lighting, exposure, Hero BP or material-function changes.',
                      'Six images share front-camera XY/yaw/FOV and one animated eye Z locked by the existing runtime Director after warmup.',
                      'Live character animation, blinking and input are not frozen; the three material captures are sequential, not identical animation frames.',
                      'Existing Director checks native shader/render readiness and writes raw viewport PNGs only when explicitly launched; this author produces no PNG.',
                      'Source sky, skylight, fixed exposure and floor are retained. Only the requested directional-light presets and transient material variants change during review.'])
PROTECTED = {}
NEW_INSTANCES = []


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, passed, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if passed else 'FAIL', observed=observed))
    dump()
    if not passed:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def package(value):
    return str(value).split('.')[0]


def disk(value, extension='.uasset'):
    value = package(value)
    check('project asset path scope', value.startswith('/Game/HarborCity/') and '..' not in value, value)
    return PROJECT / 'Content' / Path(value.removeprefix('/Game/')).with_suffix(extension)


def protect_file(file, expected=None):
    file = Path(file).resolve()
    allowed = (PROJECT / 'Content', PROJECT / 'Plugins/VRM4U/Content', PROJECT / 'Config', Path('E:/UE_5.8/Engine/Content'))
    check('protected existing file scope', file.is_file() and any(file.is_relative_to(root.resolve()) for root in allowed), str(file))
    actual = sha(file)
    if expected:
        check('actual dependency hash matches PASS evidence', actual == expected, str(file))
    PROTECTED[str(file)] = actual


def load(path):
    obj = A.load_asset(path)
    check('native asset load', obj is not None, path)
    return obj


def class_of(path):
    cls = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('native generated class load', cls is not None, path)
    return cls


def latest_pass(pattern, predicate):
    for file in sorted((DOC / 'editor_runtime').glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS' and predicate(data):
            return file, data
    raise RuntimeError('No actual PASS report for ' + pattern + '; FAIL/RUNNING candidates cannot be used')


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


def instance_snapshot(material):
    parameters = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        names = getattr(E, 'get_' + kind + '_parameter_names')(material)
        check('bounded effective instance parameter list', len(names) < 512)
        getter = getattr(E, 'get_material_instance_' + kind + '_parameter_value')
        values = {}
        for name in names:
            value = getter(material, name)
            if isinstance(value, U.LinearColor):
                value = color(value)
            elif isinstance(value, U.Object):
                value = value.get_path_name()
            values[str(name)] = value
        parameters[kind] = values
    return dict(parent=package(material.get_editor_property('parent').get_path_name()), parameters=parameters,
                all_base_overrides=material.get_editor_property('base_property_overrides').export_text())


def inspect_response_graph(limits):
    """Read actual selected function edges/defaults before any new asset is made."""
    result = dict(parameter=PARAMETER, values=[v for _, v in RESPONSES], function_readbacks=[],
        scope='Native graph readback, not a rendered prediction or a claim that a larger value is better.',
        interpretation='This parameter is not a pure RGB saturation control. The observed path changes the normal fed into a custom lighting expression; the base function also changes a base-color lighting ratio through OneMinus. Test color, brightness and shading together.')
    for name in ('MF_VrmMToonBase', 'MF_BaseColor'):
        source = '/VRM4U/MaterialUtil/UE5/MaterialFunction/' + name
        path = limits['source_to_candidate'][source]
        function = load(path)
        check('exact inherited candidate material function', isinstance(function, U.MaterialFunction), path)
        expressions = list(E.get_material_function_expressions(function))
        check('bounded existing function graph', 1 <= len(expressions) < 1200)
        nodes = {n.get_name(): n for n in expressions}
        check('unique existing expression identities', len(nodes) == len(expressions))

        def node(identity, kind):
            value = nodes.get(identity)
            check('expected linked expression identity', value is not None and value.get_class().get_name() == kind, identity)
            return value

        def inputs(value, expected):
            names = [str(v) for v in E.get_material_expression_input_names(value)]
            upstream = list(E.get_inputs_for_material_function_expression(function, value))
            check('complete native expression input enumeration', len(names) == len(upstream))
            actual = {n: u.get_name() if u else None for n, u in zip(names, upstream)}
            check('exact source-bound response graph connection', actual == expected,
                  dict(function=path, node=value.get_name(), actual=actual, expected=expected))
            return actual

        scalar = node('MaterialExpressionScalarParameter_5', 'MaterialExpressionScalarParameter')
        check('actual attenuation parameter identity', str(scalar.get_editor_property('parameter_name')) == PARAMETER)
        response = node('MaterialExpressionLinearInterpolate_9', 'MaterialExpressionLinearInterpolate')
        response_edges = inputs(response, dict(A=None, B=None, Alpha=scalar.get_name()))
        a, b = float(response.get_editor_property('const_a')), float(response.get_editor_property('const_b'))
        check('actual response constants support bounded increasing interpolation', 0 <= a < b <= 1, dict(function=path, const_a=a, const_b=b))
        switch = node('MaterialExpressionStaticSwitchParameter_3', 'MaterialExpressionStaticSwitchParameter')
        check('actual branch is the lit switch', str(switch.get_editor_property('parameter_name')) == 'bUseLight')
        switch_edges = inputs(switch, {'True': response.get_name(), 'False': 'MaterialExpressionConstant_3'})
        normal = node('MaterialExpressionLinearInterpolate_8', 'MaterialExpressionLinearInterpolate')
        normal_edges = inputs(normal, dict(A='MaterialExpressionPixelNormalWS_1', B='MaterialExpressionConstant3Vector_0', Alpha=switch.get_name()))
        up = color(node('MaterialExpressionConstant3Vector_0', 'MaterialExpressionConstant3Vector').get_editor_property('constant'))
        check('normal interpolation endpoint is world up', up[:3] == [0., 0., 1.], up)
        lighting = node('MaterialExpressionCustom_2', 'MaterialExpressionCustom')
        custom_edges = inputs(lighting, dict(Normal=normal.get_name()))
        code = str(lighting.get_editor_property('code'))
        check('bounded actual custom lighting body', 0 < len(code) < 8192)
        row = dict(path=path, sha256=sha(disk(path)), parameter_node=scalar.get_name(),
            response_node=response.get_name(), response_inputs=response_edges, const_a=a, const_b=b,
            effective_interpolation=[dict(attenuation=v, alpha=a+(b-a)*v) for _, v in RESPONSES],
            switch_inputs=switch_edges, normal_inputs=normal_edges, normal_target=up,
            custom_inputs=custom_edges, custom_code=code, custom_code_sha256=hashlib.sha256(code.encode('utf-8')).hexdigest())
        if name == 'MF_VrmMToonBase':
            inverse = node('MaterialExpressionOneMinus_4', 'MaterialExpressionOneMinus')
            row['one_minus_inputs'] = inputs(inverse, {'None': switch.get_name()})
            ratio = node('MaterialExpressionLinearInterpolate_5', 'MaterialExpressionLinearInterpolate')
            row['base_color_ratio_inputs'] = inputs(ratio, dict(A=None, B=None, Alpha=inverse.get_name()))
            row['base_color_ratio_constants'] = {key: float(ratio.get_editor_property(key)) for key in ('const_a', 'const_b')}
            branch = node('MaterialExpressionStaticSwitchParameter_18', 'MaterialExpressionStaticSwitchParameter')
            check('base color custom-rate switch identity', str(branch.get_editor_property('parameter_name')) == 'bUseCustomBaseColorRate')
            row['custom_base_color_rate_inputs'] = inputs(branch, {'True': 'MaterialExpressionScalarParameter_19', 'False': ratio.get_name()})
        result['function_readbacks'].append(row)
    return result


def main():
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE or dirty map', not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('fresh per-run review map', not A.does_asset_exist(MAP) and not disk(MAP, '.umap').exists())
    file, limits = latest_pass('author_*_MToonLightLimits/author_result.json',
                              lambda d: set(d.get('variants', {}).get('MToonLightLimits', {})) == SLOTS)
    R['inputs']['light_limits_author'] = dict(path=str(file), sha256=sha(file))
    check('PASS material report has no failed checks', all(c.get('status') == 'PASS' for c in limits['checks']))
    check('material author retained all source bytes', limits['protected_source_sha256_before'] == limits['protected_source_sha256_after'])
    for path, digest in limits['protected_source_sha256_after'].items():
        protect_file(path, digest)
    candidate = limits['variants']['MToonLightLimits']
    assets = {package(a['path']): a for a in limits['assets']}
    check('complete isolated twelve-asset candidate', len(assets) == 12 and len(limits['source_to_candidate']) == 12)
    for path, asset in assets.items():
        check('candidate stays in report namespace', path.startswith(limits['target_directory'] + '/')
              and path.startswith(HERO + '/Materials/MToonLightLimits_'), path)
        protect_file(disk(path), asset['sha256'])
    baseline = {}
    for slot in SLOTS:
        path = package(candidate[slot])
        check('candidate final slot present in actual saved assets', path in assets, slot)
        baseline[slot] = assets[path]['source']
        check('exact existing MToon baseline slot', baseline[slot] == HERO + '/Materials/MToon/MI_MToon_' + slot)
        check('source-to-candidate mapping agrees', limits['source_to_candidate'][baseline[slot]] == path)
    source_file, source_report = latest_pass('author_*_Lookdev*/author_result.json', lambda d: d.get('map') == SOURCE_MAP)
    R['inputs']['lookdev_author'] = dict(path=str(source_file), sha256=sha(source_file))
    source_assets = {package(a['path']): a for a in source_report['assets']}
    check('native source map recorded in PASS report', SOURCE_MAP in source_assets)
    for path, asset in source_assets.items():
        protect_file(disk(path, '.umap' if path == SOURCE_MAP else '.uasset'), asset['sha256'])
    for config in (PROJECT / 'Config').glob('*.ini'):
        protect_file(config)
    R['inputs']['script'] = dict(path=str(Path(__file__).resolve()), sha256=sha(Path(__file__)))
    R['review_basis'] = dict(
        observed_run='20260924_121441_560_9fe41560_herolimits_game/Lookdev_20260924_121458_D3F18004',
        original_pngs_viewed=8,
        observed_visual_issue='The actual LightLimits Neutral face retained pink hair and pale skin; Dusk and InteriorWarmProxy remained strongly orange. LightLimits alone did not resolve color response.',
        experiment_scope='Three attenuation values under identical inherited Neutral/Dusk presets; no preferred value or visual PASS before native captures.')
    native = U.load_class(None, '/Script/HarborCity.HCM5VS2LookdevDirector')
    check('existing compiled native Lookdev Director', native is not None)
    source = load(SOURCE_MAP)
    check('owned native source stage', A.get_metadata_tag(source, KEY) == SOURCE_OWNER)
    hero, mode = class_of(CHARACTER), class_of(MODE)
    check('new GameMode spawns exact new Hero', U.get_default_object(mode).get_editor_property('default_pawn_class') == hero)
    cdo = U.get_default_object(hero)
    body = cdo.get_component_by_class(U.SkeletalMeshComponent)
    capsule = cdo.get_component_by_class(U.CapsuleComponent)
    check('actual Hero body and capsule', body is not None and capsule is not None)
    mesh = body.get_editor_property('skeletal_mesh_asset')
    check('existing normalized Selestia mesh', mesh is not None and package(mesh.get_path_name()) == '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia')
    slots = [str(s.get_editor_property('material_slot_name')).removeprefix('Selestia_') for s in mesh.get_editor_property('materials')]
    check('actual five distinct material slots', len(slots) == 5 and set(slots) == SLOTS, slots)
    animation = body.get_editor_property('anim_class')
    check('Hero actual saved AnimBP exists', animation is not None)
    R['hero_binding'] = dict(blueprint=CHARACTER, game_mode=MODE, mesh=mesh.get_path_name(), animation=animation.get_path_name(),
                            slot_order=slots, original_cdo_materials=[body.get_material(i).get_path_name() for i in range(body.get_num_materials())])
    for path in (CHARACTER, MODE, mesh.get_path_name(), animation.get_path_name().removesuffix('_C')):
        protect_file(disk(path))
    R['parameter_semantics'] = inspect_response_graph(limits)
    source_materials = {}
    source_snapshots = {}
    for slot in slots:
        material = load(candidate[slot])
        check('exact existing LightLimits material owner', isinstance(material, U.MaterialInstanceConstant)
              and A.get_metadata_tag(material, KEY) == LIMIT_OWNER, candidate[slot])
        snapshot = instance_snapshot(material)
        check('source attenuation is measured .1', abs(snapshot['parameters']['scalar'][PARAMETER]-.1) < 1.e-6, slot)
        for parameter, expected in limits['limits'].items():
            check('all original LightLimits values retained', abs(snapshot['parameters']['scalar'][parameter]-float(expected)) < 1.e-6,
                  dict(slot=slot, parameter=parameter, value=snapshot['parameters']['scalar'][parameter]))
        check('actual lit switch enabled', snapshot['parameters']['static_switch'].get('bUseLight') is True, slot)
        check('actual base-color ratio path not bypassed', snapshot['parameters']['static_switch'].get('bUseCustomBaseColorRate') is False, slot)
        source_materials[slot] = material
        source_snapshots[slot] = snapshot
    R['source_instance_readback'] = source_snapshots
    configured = []
    for name, attenuation in RESPONSES:
        materials = []
        for slot in slots:
            source = source_materials[slot]
            if attenuation == .1:
                material = source
            else:
                target = MATERIAL_ROOT + '/' + name + '/MI_' + slot
                check('fresh isolated MI destination', not A.does_asset_exist(target) and not disk(target).exists(), target)
                material = A.duplicate_asset(source.get_path_name(), target)
                check('native duplicate exact new MI', isinstance(material, U.MaterialInstanceConstant)
                      and package(material.get_path_name()) == target)
                check('MI duplicate preserves all effective parameters and overrides', instance_snapshot(material) == source_snapshots[slot], slot)
                # UE 5.8 MaterialEditingLibrary.cpp:1485-1493 initializes bResult
                # false and never updates it even after applying the parameter.
                # Verify the actual value below; retain the return as diagnostics.
                setter_result = E.set_material_instance_scalar_parameter_value(material, U.Name(PARAMETER), attenuation)
                R.setdefault('scalar_setter_diagnostics', []).append(dict(
                    path=target, parameter=PARAMETER, requested=attenuation,
                    native_return=setter_result, acceptance='effective scalar and complete instance readback below'))
                E.update_material_instance(material)
                expected = copy.deepcopy(source_snapshots[slot])
                expected['parameters']['scalar'][PARAMETER] = float(E.get_material_instance_scalar_parameter_value(material, U.Name(PARAMETER)))
                check('requested scalar readback', abs(expected['parameters']['scalar'][PARAMETER]-attenuation) < 1.e-6, target)
                check('only one effective parameter changed; parent, static switches, textures and base overrides preserved',
                      instance_snapshot(material) == expected, target)
                A.set_metadata_tag(material, KEY, OWNER)
                check('save only new color-response MI', A.save_loaded_asset(material, False), target)
                check('saved new MI exact native readback', instance_snapshot(A.load_asset(target)) == expected, target)
                NEW_INSTANCES.append(dict(path=target, expected=expected))
                R['assets'].append(dict(path=target, source=package(source.get_path_name()), attenuation=attenuation,
                                        sha256=sha(disk(target)), bytes=disk(target).stat().st_size))
            materials.append(material)
        configured.append(native_struct(U.HCM5VS2MaterialVariant, dict(label=name, use_original_materials=False, materials=materials)))
        R['materials'][name] = [dict(slot=s, path=package(m.get_path_name()), attenuation=attenuation,
                                     sha256=sha(disk(m.get_path_name()))) for s, m in zip(slots, materials)]
    check('exactly ten new MI; five existing .1 references reused', len(NEW_INSTANCES) == 10 and len(configured) == 3)
    # Native editor world lifecycle, not AssetTools duplicate_asset(UWorld).
    check('create fresh map from native template', LEVEL.new_level_from_template(MAP, SOURCE_MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact isolated current world', package(world.get_path_name()) == MAP)
    A.set_metadata_tag(world, KEY, OWNER)
    actors = ACT.get_all_level_actors()
    native_classes = {'StaticMeshActor', 'DirectionalLight', 'SkyLight', 'SkyAtmosphere', 'PostProcessVolume', 'PlayerStart', 'HCM5VS2LookdevDirector'}
    for actor in actors:
        if actor.get_class().get_name() in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume'):
            continue
        check('only known owned source stage actors', actor.actor_has_tag(U.Name(SOURCE_OWNER)) and actor.get_class().get_name() in native_classes, actor.get_actor_label())
        actor.set_editor_property('tags', [U.Name(OWNER)])
    directors = [a for a in actors if a.get_class() == native]
    starts = [a for a in actors if isinstance(a, U.PlayerStart)]
    check('one existing copied Director and PlayerStart', len(directors) == 1 and len(starts) == 1)
    director = directors[0]
    original_variants = [variant_readback(v) for v in director.get_editor_property('material_variants')]
    baseline_indices = [i for i, v in enumerate(original_variants) if v['label'] == 'MToon']
    check('template has one matching native MToon variant', len(baseline_indices) == 1)
    index = baseline_indices[0]
    check('template baseline material order matches actual Hero slots',
          [package(p) for p in original_variants[index]['materials']] == [baseline[s] for s in slots])
    start_location = starts[0].get_actor_location()
    check('native spawn agrees with existing front camera convention', abs(start_location.x) < .001 and abs(start_location.y) < .001
          and abs(starts[0].get_actor_rotation().yaw) < .001
          and abs(start_location.z - capsule.get_unscaled_capsule_half_height() - 2.) < .1)
    world.get_world_settings().set_editor_property('default_game_mode', mode)
    check('save owned new-map checkpoint', LEVEL.save_current_level())
    R['checkpoint'] = dict(path=MAP, sha256=sha(disk(MAP, '.umap')), complete=False)
    source_shots = list(director.get_editor_property('shots'))
    shots = []
    for light in PRESETS:
        matches = [s for s in source_shots if s.get_editor_property('label') == light + '_FaceFront'
                   and s.get_editor_property('variant_index') == index]
        check('one actual template front shot for ' + light, len(matches) == 1)
        original = matches[0]
        check('template face uses locked eye height and level front camera', original.get_editor_property('use_player_eye_height')
              and abs(original.get_editor_property('camera_rotation').pitch) < .001 and abs(original.get_editor_property('camera_rotation').roll) < .001)
        for variant_index in range(3):
            values = {key: original.get_editor_property(key) for key in ('camera_location', 'camera_rotation', 'use_player_eye_height',
                      'light_direction', 'light_color', 'light_intensity', 'ambient_color')}
            values.update(label=light + '_FaceFront', variant_index=variant_index)
            shots.append(native_struct(U.HCM5VS2LookdevShot, values))
    snapshots = [shot_readback(s) for s in shots]
    check('exactly six same front camera configurations', len(shots) == 6 and all(
        (s['camera_location'], s['camera_rotation'], s['use_player_eye_height']) ==
        (snapshots[0]['camera_location'], snapshots[0]['camera_rotation'], snapshots[0]['use_player_eye_height']) for s in snapshots))
    light_actor = director.get_editor_property('directional_light')
    check('copied native movable light belongs to new map', light_actor is not None and light_actor.get_path_name().startswith(MAP + '.')
          and light_actor.get_component_by_class(U.DirectionalLightComponent).get_editor_property('mobility') == U.ComponentMobility.MOVABLE)
    director.set_actor_label('HC_M5VS2_HeroColorResponse_Director')
    director.set_editor_property('material_variants', configured)
    director.set_editor_property('shots', shots)
    director.set_editor_property('use_player_eye_height', True)
    director.set_editor_property('hide_hud', True)
    variants = [variant_readback(v) for v in configured]
    R['shot_plan'] = [dict(index=i, **s, variant=variants[s['variant_index']]['label'], horizontal_fov=40.,
                          expected_png='%03d_%s_%s.png' % (i, s['label'], variants[s['variant_index']]['label'])) for i, s in enumerate(snapshots)]
    R['stage'] = dict(source_author=R['inputs']['lookdev_author'], actual_main_light=light_actor.get_path_name(),
                      original_native_actor_count=len(actors), sky_exposure_geometry='Inherited unchanged from exact source map hash')
    check('save isolated completed comparison map', LEVEL.save_current_level())
    check('reload new native comparison map', LEVEL.load_level(MAP))
    current = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    saved = [a for a in ACT.get_all_level_actors() if a.get_class() == native]
    check('saved owned map, mode and unique native Director', package(current.get_path_name()) == MAP
          and A.get_metadata_tag(current, KEY) == OWNER and current.get_world_settings().get_editor_property('default_game_mode') == mode and len(saved) == 1)
    check('saved six shots exact readback', [shot_readback(s) for s in saved[0].get_editor_property('shots')] == snapshots)
    check('saved three five-slot variants exact readback', [variant_readback(v) for v in saved[0].get_editor_property('material_variants')] == variants)
    check('all source maps, Hero BP/assets, material dependencies and config unchanged', all(Path(p).is_file() and sha(p) == h for p, h in PROTECTED.items()))
    check('Hero default material interfaces unchanged', [body.get_material(i).get_path_name() for i in range(body.get_num_materials())] == R['hero_binding']['original_cdo_materials'])
    R['preservation'] = dict(files_checked=len(PROTECTED), changed=[])
    for item in NEW_INSTANCES:
        check('new MI preserved through map save/reload', instance_snapshot(A.load_asset(item['path'])) == item['expected'], item['path'])
    R['assets'].append(dict(path=MAP, sha256=sha(disk(MAP, '.umap')), bytes=disk(MAP, '.umap').stat().st_size))
    R['checkpoint']['complete'] = True
    R['runtime_arguments'] = [MAP, '-game', '-M5VS2Lookdev', '-M5VS2EvidenceDir=<fresh absolute VS2 docs directory>', '-M5VS2AutoQuit', '-ResX=1920', '-ResY=1080']
    R['pass_scope'] = 'Native independent map author/save/reload and six-shot/three-variant readback only; runtime and visual material choice NOT_RUN.'
    R['status'] = 'PASS'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    R['partial_map_retained'] = MAP if disk(MAP, '.umap').is_file() else None
    R['retry_policy'] = 'Preserve this failure/checkpoint; rerun wrapper with fresh evidence directory to obtain a new unique map.'
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('Hero color-response author failed; preserve author_result.json and any unique partial map')
