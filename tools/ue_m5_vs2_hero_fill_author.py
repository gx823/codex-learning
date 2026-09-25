"""Author three isolated permanent Hero-fill candidates; never launches runtime.

Root: ue_m5_vs2_author_run.ps1 -Phase HeroFill -ScriptPath this_file.
Requires compiled HCM5VS2HeroFillEditor and actual PASS LightLimits/Lookdev inputs.
Each fresh evidence directory creates three BP + GameMode + four-shot map sets.
Original Hero, source stage, shared materials and project defaults remain unchanged.
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
OWNER = 'HarborCity_M5_VS2_HeroFill'
LIMIT_OWNER = 'HarborCity_M5_VS2_MToonLightLimits'
KEY = 'HarborCityOwnedBy'
SLOTS = {'hair', 'body', 'face', 'option', 'costume'}
PRESETS = ('Neutral', 'Dusk', 'InteriorWarmProxy', 'NightMoonProxy')
CANDIDATES = (('Zero', 0.), ('Quarter', .25), ('One', 1.))
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    matches = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(matches) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return matches[0][0] or matches[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and OUT != DOC and not (OUT / 'author_result.json').exists()
assert argument('M5AuthorPhase') == 'HeroFill'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
DEST = '/Game/HarborCity/M5VS2/HeroFill/Review_' + TOKEN
R = dict(status='RUNNING', phase='HeroFill', owner=OWNER, target_directory=DEST,
         checks=[], assets=[], inputs={}, candidates=[], runtime='NOT_RUN', visual='USER_REVIEW',
         default_map_changed=False, source_hero_changed=False, screenshots_created=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         limitations=['Three permanent capsule-attached BP lights; the Director only supplies its existing four lighting presets.',
                      'No final Hero selection or production binding; original Hero and material assets remain unchanged.',
                      'InteriorWarmProxy and NightMoonProxy are existing open-stage directional-light presets, not completed world locations.',
                      'Captures are sequential live animation frames in independent processes, not pixel-identical animation poses.',
                      'Runtime-created first-person meshes and head accessories are outside this body-channel candidate.',
                      'Final choice requires real world/night movement, shadow, first-person and performance review.'])
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
    if not value.startswith('/Game/HarborCity/') or '..' in value:
        raise RuntimeError('Outside project package scope: ' + value)
    return PROJECT / 'Content' / Path(value.removeprefix('/Game/')).with_suffix(extension)


def protect(file, expected=None):
    file = Path(file).resolve()
    roots = (PROJECT / 'Content', PROJECT / 'Plugins/VRM4U/Content', Path('E:/UE_5.8/Engine/Content'))
    check('protected native dependency only; no private Config reads',
          file.is_file() and any(file.is_relative_to(root.resolve()) for root in roots), str(file))
    digest = sha(file)
    if expected:
        check('dependency matches actual PASS evidence SHA', digest == expected, str(file))
    PROTECTED[str(file)] = digest


def load(path):
    obj = A.load_asset(path)
    check('native asset load', obj is not None, path)
    return obj


def class_of(path):
    cls = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('native generated class', cls is not None, path)
    return cls


def latest_pass(pattern, predicate):
    for file in sorted((DOC / 'editor_runtime').glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS' and predicate(data):
            check('input PASS report contains only passed checks', bool(data.get('checks'))
                  and all(c.get('status') == 'PASS' for c in data['checks']), str(file))
            return file, data
    raise RuntimeError('No matching actual PASS report: ' + pattern)


def struct(kind, values):
    obj = kind()
    for key, value in values.items():
        obj.set_editor_property(key, value)
    return obj


def shot_readback(shot):
    g = shot.get_editor_property
    vec = lambda v: [float(v.x), float(v.y), float(v.z)]
    rot = lambda v: [float(v.pitch), float(v.yaw), float(v.roll)]
    col = lambda v: [float(v.r), float(v.g), float(v.b), float(v.a)]
    return dict(label=str(g('label')), variant_index=int(g('variant_index')),
                camera_location=vec(g('camera_location')), camera_rotation=rot(g('camera_rotation')),
                use_player_eye_height=bool(g('use_player_eye_height')), light_direction=rot(g('light_direction')),
                light_color=col(g('light_color')), light_intensity=float(g('light_intensity')),
                ambient_color=col(g('ambient_color')))


def variant_readback(variant):
    return dict(label=str(variant.get_editor_property('label')),
                use_original_materials=bool(variant.get_editor_property('use_original_materials')),
                materials=[m.get_path_name() for m in variant.get_editor_property('materials')])


def save_new(obj, source):
    path = package(obj.get_path_name())
    check('only fresh candidate namespace is saved', path.startswith(DEST + '/'), path)
    A.set_metadata_tag(obj, KEY, OWNER)
    check('save new candidate native asset', A.save_loaded_asset(obj, False), path)
    R['assets'].append(dict(path=path, source=source, sha256=sha(disk(path)), bytes=disk(path).stat().st_size))


def main():
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no PIE or dirty map', not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('new bounded native helper loaded', hasattr(U, 'HCM5VS2HeroFillEditor'))
    check('entire candidate directory is fresh', not A.does_directory_exist(DEST) and not disk(DEST).with_suffix('').exists())
    f, limits = latest_pass('author_*_MToonLightLimits/author_result.json',
                            lambda d: set(d.get('variants', {}).get('MToonLightLimits', {})) == SLOTS)
    R['inputs']['light_limits_author'] = dict(path=str(f), sha256=sha(f))
    check('material source bytes retained by native author', limits['protected_source_sha256_before'] == limits['protected_source_sha256_after'])
    for path, digest in limits['protected_source_sha256_after'].items():
        protect(path, digest)
    mapping = limits['variants']['MToonLightLimits']
    recorded = {package(a['path']): a for a in limits['assets']}
    check('complete twelve-asset LightLimits closure', len(recorded) == 12 and len(limits['source_to_candidate']) == 12)
    for path, entry in recorded.items():
        check('LightLimits namespace bound', path.startswith(limits['target_directory'] + '/')
              and path.startswith(HERO + '/Materials/MToonLightLimits_'), path)
        protect(disk(path), entry['sha256'])
    f, stage = latest_pass('author_*_Lookdev*/author_result.json', lambda d: d.get('map') == SOURCE_MAP)
    R['inputs']['lookdev_author'] = dict(path=str(f), sha256=sha(f))
    stage_assets = {package(a['path']): a for a in stage['assets']}
    check('actual source map saved in PASS report', SOURCE_MAP in stage_assets)
    protect(disk(SOURCE_MAP, '.umap'), stage_assets[SOURCE_MAP]['sha256'])
    check('source stage ownership', A.get_metadata_tag(load(SOURCE_MAP), KEY) == SOURCE_OWNER)
    hero_class, mode_class = class_of(CHARACTER), class_of(MODE)
    source_hero, source_mode = load(CHARACTER), load(MODE)
    check('source GameMode spawns current Hero', U.get_default_object(mode_class).get_editor_property('default_pawn_class') == hero_class)
    body = U.get_default_object(hero_class).get_component_by_class(U.SkeletalMeshComponent)
    check('source body exists', body is not None)
    mesh = body.get_editor_property('skeletal_mesh_asset')
    check('normalized Selestia mesh preserved', mesh is not None and package(mesh.get_path_name()) == '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia')
    animation = body.get_editor_property('anim_class')
    check('actual animation class exists', animation is not None)
    slots = [str(s.get_editor_property('material_slot_name')).removeprefix('Selestia_') for s in mesh.get_editor_property('materials')]
    check('actual five distinct slots', len(slots) == 5 and set(slots) == SLOTS, slots)
    for path in (CHARACTER, MODE, mesh.get_path_name(), animation.get_path_name().removesuffix('_C')):
        protect(disk(path))
    R['original_hero_materials'] = [body.get_material(i).get_path_name() for i in range(body.get_num_materials())]
    materials, baseline = [], []
    for slot in slots:
        path = package(mapping[slot])
        check('actual saved slot candidate', path in recorded, slot)
        original = HERO + '/Materials/MToon/MI_MToon_' + slot
        check('exact source-to-candidate slot mapping', recorded[path]['source'] == original
              and limits['source_to_candidate'][original] == path, slot)
        baseline.append(original)
        mat = load(path)
        check('owned LightLimits MI', isinstance(mat, U.MaterialInstanceConstant) and A.get_metadata_tag(mat, KEY) == LIMIT_OWNER)
        for parameter, expected in dict(limits['limits'], mtoon_LightColorAttenuation=.1).items():
            actual = float(E.get_material_instance_scalar_parameter_value(mat, U.Name(parameter)))
            check('effective material scalar retained', abs(actual - float(expected)) < 1.e-6,
                  dict(slot=slot, parameter=parameter, value=actual))
        materials.append(mat)
    R['materials'] = [dict(slot=s, path=m.get_path_name(), sha256=sha(disk(m.get_path_name()))) for s, m in zip(slots, materials)]
    for source in (Path(__file__), PROJECT / 'Source/HarborCity/M5VS2/HCM5VS2HeroFillEditor.h',
                   PROJECT / 'Source/HarborCity/M5VS2/HCM5VS2HeroFillEditor.cpp'):
        R['inputs'][source.name] = dict(path=str(source.resolve()), sha256=sha(source))
    native = U.load_class(None, '/Script/HarborCity.HCM5VS2LookdevDirector')
    check('existing compiled Lookdev Director', native is not None)
    expected_shots = None
    for tag, candela in CANDIDATES:
        bp_path = DEST + '/BP_SelestiaFill_' + TOKEN + '_' + tag
        gm_path = DEST + '/BP_SelestiaFillMode_' + TOKEN + '_' + tag
        map_path = DEST + '/L_SelestiaFill_' + TOKEN + '_' + tag
        for path, ext in ((bp_path, '.uasset'), (gm_path, '.uasset'), (map_path, '.umap')):
            check('fresh candidate native destination', not A.does_asset_exist(path) and not disk(path, ext).exists(), path)
        bp = A.duplicate_asset(CHARACTER, bp_path)
        check('duplicate exact Hero BP', isinstance(bp, U.Blueprint) and package(bp.get_path_name()) == bp_path)
        native_result = json.loads(U.HCM5VS2HeroFillEditor.configure_hero_fill(bp, candela))
        item = dict(tag=tag, intensity_cd=candela, blueprint=bp_path, game_mode=gm_path, map=map_path,
                    helper=native_result, runtime='NOT_RUN', visual='USER_REVIEW')
        R['candidates'].append(item)
        check('actual native fill installation and readback', native_result.get('status') == 'PASS'
              and native_result.get('compiled_template_readback_valid') is True
              and native_result.get('blueprint') == bp_path and native_result.get('intensity_cd') == candela,
              native_result)
        new_class = class_of(bp_path)
        new_body = U.get_default_object(new_class).get_component_by_class(U.SkeletalMeshComponent)
        check('copied Hero preserves exact mesh and animation', new_body is not None
              and new_body.get_editor_property('skeletal_mesh_asset') == mesh and new_body.get_editor_property('anim_class') == animation)
        for i, mat in enumerate(materials):
            new_body.set_material(i, mat)
        check('candidate body uses five unchanged LightLimits instances',
              [new_body.get_material(i) for i in range(5)] == materials)
        save_new(bp, CHARACTER)
        gm = A.duplicate_asset(MODE, gm_path)
        check('duplicate exact source GameMode', isinstance(gm, U.Blueprint) and package(gm.get_path_name()) == gm_path)
        new_mode = class_of(gm_path)
        U.get_default_object(new_mode).set_editor_property('default_pawn_class', new_class)
        save_new(gm, MODE)
        check('new GameMode spawns isolated fill Hero', U.get_default_object(class_of(gm_path)).get_editor_property('default_pawn_class') == new_class)
        check('native new level from existing stage template', LEVEL.new_level_from_template(map_path, SOURCE_MAP))
        world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        check('exact new candidate world', package(world.get_path_name()) == map_path)
        A.set_metadata_tag(world, KEY, OWNER)
        actors = ACT.get_all_level_actors()
        allowed = {'StaticMeshActor', 'DirectionalLight', 'SkyLight', 'SkyAtmosphere', 'PostProcessVolume', 'PlayerStart', 'HCM5VS2LookdevDirector'}
        for actor in actors:
            if actor.get_class().get_name() in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume'):
                continue
            check('only existing owned comparison-stage actors', actor.actor_has_tag(U.Name(SOURCE_OWNER))
                  and actor.get_class().get_name() in allowed, actor.get_actor_label())
            actor.set_editor_property('tags', [U.Name(OWNER)])
        directors = [a for a in actors if a.get_class() == native]
        check('one native Director and one PlayerStart', len(directors) == 1 and len([a for a in actors if isinstance(a, U.PlayerStart)]) == 1)
        director = directors[0]
        source_variants = [variant_readback(v) for v in director.get_editor_property('material_variants')]
        indices = [i for i, v in enumerate(source_variants) if v['label'] == 'MToon']
        check('one exact source MToon variant', len(indices) == 1)
        index = indices[0]
        check('source slot order agrees with actual Hero', [package(p) for p in source_variants[index]['materials']] == baseline)
        shots = []
        for preset in PRESETS:
            matches = [s for s in director.get_editor_property('shots')
                       if str(s.get_editor_property('label')) == preset + '_FaceFront' and s.get_editor_property('variant_index') == index]
            check('one actual source front shot for ' + preset, len(matches) == 1)
            values = {key: matches[0].get_editor_property(key) for key in ('camera_location', 'camera_rotation', 'use_player_eye_height',
                      'light_direction', 'light_color', 'light_intensity', 'ambient_color')}
            check('source front shot retains dynamic eye-height convention', bool(values['use_player_eye_height']))
            values.update(label=preset + '_FaceFront', variant_index=0)
            shots.append(struct(U.HCM5VS2LookdevShot, values))
        snapshots = [shot_readback(s) for s in shots]
        if expected_shots is None:
            expected_shots = snapshots
        check('all candidates use identical source camera and four light presets', snapshots == expected_shots and len(shots) == 4)
        label = 'Fill_' + tag
        variant = struct(U.HCM5VS2MaterialVariant, dict(label=label, use_original_materials=False, materials=materials))
        director.set_editor_property('material_variants', [variant])
        director.set_editor_property('shots', shots)
        director.set_editor_property('use_player_eye_height', True)
        director.set_editor_property('hide_hud', True)
        world.get_world_settings().set_editor_property('default_game_mode', new_mode)
        check('save new isolated native candidate map', LEVEL.save_current_level())
        check('reload candidate map through native editor world lifecycle', LEVEL.load_level(map_path))
        world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        saved = [a for a in ACT.get_all_level_actors() if a.get_class() == native]
        check('saved candidate world, mode, ownership and Director', package(world.get_path_name()) == map_path
              and A.get_metadata_tag(world, KEY) == OWNER and world.get_world_settings().get_editor_property('default_game_mode') == new_mode and len(saved) == 1)
        check('four shots and material variant native readback', [shot_readback(s) for s in saved[0].get_editor_property('shots')] == snapshots
              and [variant_readback(v) for v in saved[0].get_editor_property('material_variants')] == [variant_readback(variant)])
        R['assets'].append(dict(path=map_path, source=SOURCE_MAP, sha256=sha(disk(map_path, '.umap')), bytes=disk(map_path, '.umap').stat().st_size))
        item['shot_plan'] = [dict(index=i, **s, expected_png='%03d_%s_%s.png' % (i, s['label'], label)) for i, s in enumerate(snapshots)]
        item['runtime_arguments'] = [map_path, '-game', '-M5VS2Lookdev', '-M5VS2EvidenceDir=<fresh absolute VS2 docs directory>',
                                     '-M5VS2AutoQuit', '-ResX=1920', '-ResY=1080']
    check('exactly nine isolated saved assets', len(R['assets']) == 9 and len({a['path'] for a in R['assets']}) == 9)
    check('every protected original retains exact bytes', all(Path(p).is_file() and sha(p) == h for p, h in PROTECTED.items()))
    check('original Hero CDO material interfaces retained', [body.get_material(i).get_path_name() for i in range(body.get_num_materials())] == R['original_hero_materials'])
    R['protected_source_sha256'] = PROTECTED
    R['pass_scope'] = 'Nine isolated native candidate assets saved; permanent fill template, original dependency hashes and four-shot map readback only. Runtime/shader/visual acceptance NOT_RUN.'
    R['status'] = 'PASS'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    R['retry_policy'] = 'Keep this failure and all partial candidate assets; use a fresh evidence directory for a new unique token.'
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('Hero fill author failed; preserve author_result.json and unique partial assets')
