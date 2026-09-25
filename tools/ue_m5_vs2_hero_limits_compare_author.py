"""Author one fresh eight-shot MToon versus LightLimits comparison map only.

Root runs ue_m5_vs2_author_run.ps1 -Phase HeroLimitsCompare -ScriptPath this_file.
Requires an actual PASS MToonLightLimits author and the saved full Lookdev stage.
No runtime launch, source map save, Hero BP/material edit or shader acceptance.
Each fresh wrapper evidence directory produces a unique map, including retries.
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
HERO = '/Game/HarborCity/M5VS2/HeroSelestia'
CHARACTER = HERO + '/BP_M5VS2_Selestia'
MODE = HERO + '/BP_M5VS2_SelestiaGameMode'
SOURCE_MAP = '/Game/HarborCity/M5VS2/Lookdev/L_SelestiaLookdev'
SOURCE_OWNER = 'HarborCity_M5_VS2_SelestiaLookdev'
OWNER = 'HarborCity_M5_VS2_HeroLimitsCompare'
LIMIT_OWNER = 'HarborCity_M5_VS2_MToonLightLimits'
KEY = 'HarborCityOwnedBy'
SLOTS = {'hair', 'body', 'face', 'option', 'costume'}
PRESETS = ('Neutral', 'Afternoon', 'Dusk', 'InteriorWarmProxy')
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
assert argument('M5AuthorPhase') == 'HeroLimitsCompare'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
MAP = '/Game/HarborCity/M5VS2/HeroLimitsCompare/Review_' + TOKEN + '/L_HeroLimitsCompare_' + TOKEN
R = dict(status='RUNNING', phase='HeroLimitsCompare', owner=OWNER, map=MAP, source_map=SOURCE_MAP,
         checks=[], assets=[], inputs={}, materials={}, shot_plan=[], runtime='NOT_RUN', visual='USER_REVIEW',
         default_map_changed=False, blueprint_materials_changed=False, screenshots_created=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         limitations=['An isolated candidate comparison; this does not select the final Hero material.',
                      'InteriorWarmProxy is warm directional light on the open comparison stage, not a completed interior.',
                      'Eight images share front-camera XY/yaw/FOV and one animated eye Z locked by the existing runtime Director after warmup.',
                      'Live character animation, blinking and input are not frozen; the two material captures are sequential, not identical animation frames.',
                      'Existing Director checks native shader/render readiness and writes raw viewport PNGs only when explicitly launched; this author produces no PNG.',
                      'Source sky, skylight, fixed exposure and floor are retained. Only the requested directional-light presets and transient material variants change during review.'])
PROTECTED = {}


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
    configured = []
    for name, mapping, owner in (('MToon', baseline, 'HarborCity_M5_VS2_HeroMaterials'), ('MToonLightLimits', candidate, LIMIT_OWNER)):
        materials = []
        for slot in slots:
            material = load(mapping[slot])
            check('exact existing material owner', A.get_metadata_tag(material, KEY) == owner, mapping[slot])
            if name == 'MToonLightLimits':
                for parameter, expected in limits['limits'].items():
                    value = E.get_material_instance_scalar_parameter_value(material, U.Name(parameter))
                    check('candidate inherited limit readback', abs(float(value) - float(expected)) < 1e-6, dict(slot=slot, parameter=parameter, value=value))
            materials.append(material)
        configured.append(native_struct(U.HCM5VS2MaterialVariant, dict(label=name, use_original_materials=False, materials=materials)))
        R['materials'][name] = [dict(slot=s, path=package(m.get_path_name()), sha256=sha(disk(m.get_path_name()))) for s, m in zip(slots, materials)]
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
        for variant_index in range(2):
            values = {key: original.get_editor_property(key) for key in ('camera_location', 'camera_rotation', 'use_player_eye_height',
                      'light_direction', 'light_color', 'light_intensity', 'ambient_color')}
            values.update(label=light + '_FaceFront', variant_index=variant_index)
            shots.append(native_struct(U.HCM5VS2LookdevShot, values))
    snapshots = [shot_readback(s) for s in shots]
    check('exactly eight same front camera configurations', len(shots) == 8 and all(
        (s['camera_location'], s['camera_rotation'], s['use_player_eye_height']) ==
        (snapshots[0]['camera_location'], snapshots[0]['camera_rotation'], snapshots[0]['use_player_eye_height']) for s in snapshots))
    light_actor = director.get_editor_property('directional_light')
    check('copied native movable light belongs to new map', light_actor is not None and light_actor.get_path_name().startswith(MAP + '.')
          and light_actor.get_component_by_class(U.DirectionalLightComponent).get_editor_property('mobility') == U.ComponentMobility.MOVABLE)
    director.set_actor_label('HC_M5VS2_HeroLimitsCompare_Director')
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
    check('saved eight shots exact readback', [shot_readback(s) for s in saved[0].get_editor_property('shots')] == snapshots)
    check('saved two five-slot variants exact readback', [variant_readback(v) for v in saved[0].get_editor_property('material_variants')] == variants)
    check('all source maps, Hero BP/assets, material dependencies and config unchanged', all(Path(p).is_file() and sha(p) == h for p, h in PROTECTED.items()))
    check('Hero default material interfaces unchanged', [body.get_material(i).get_path_name() for i in range(body.get_num_materials())] == R['hero_binding']['original_cdo_materials'])
    R['preservation'] = dict(files_checked=len(PROTECTED), changed=[])
    R['assets'] = [dict(path=MAP, sha256=sha(disk(MAP, '.umap')), bytes=disk(MAP, '.umap').stat().st_size)]
    R['checkpoint']['complete'] = True
    R['runtime_arguments'] = [MAP, '-game', '-M5VS2Lookdev', '-M5VS2EvidenceDir=<fresh absolute VS2 docs directory>', '-M5VS2AutoQuit', '-ResX=1920', '-ResY=1080']
    R['pass_scope'] = 'Native independent map author/save/reload and eight-shot/two-variant readback only; runtime and visual material choice NOT_RUN.'
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
    raise RuntimeError('Hero limits comparison author failed; preserve author_result.json and any unique partial map')
