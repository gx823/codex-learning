"""Author only an isolated Selestia material-comparison map in native UE Python.

Run via ue_m5_vs2_author_run.ps1, Phase Lookdev (6 shots) or LookdevFull
(90 shots). No runtime launch, default-map change, source model copy or old
asset save occurs here. Author PASS never means a rendered/visual PASS.
"""
from pathlib import Path
import datetime
import hashlib
import json
import math
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
ROOT = '/Game/HarborCity/M5VS2/Lookdev'
MAP = ROOT + '/L_SelestiaLookdev'
OLD = '/Game/HarborCity/M5VS1/HeroSelestia'
HERO = '/Game/HarborCity/M5VS2/HeroSelestia'
MODE = OLD + '/BP_M5_SelestiaGameMode'
CHARACTER = OLD + '/BP_M5_Selestia'
OWNER = 'HarborCity_M5_VS2_SelestiaLookdev'
KEY = 'HarborCityOwnedBy'
MATERIAL_OWNER = 'HarborCity_M5_VS2_HeroMaterials'
A = U.EditorAssetLibrary
E = U.MaterialEditingLibrary
AT = U.AssetToolsHelpers.get_asset_tools()
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)

cmd = U.SystemLibrary.get_command_line()
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', cmd)
if len(args) != 1:
    raise RuntimeError('Exactly one -M5EvidenceDir is required')
OUT = Path(args[0][0] or args[0][1]).resolve()
if not OUT.is_relative_to(DOC.resolve()):
    raise RuntimeError('Evidence directory must be within the authorized M5-VS2 report root')
OUT.mkdir(parents=True, exist_ok=True)
if (OUT / 'author_result.json').exists():
    raise RuntimeError('Fresh evidence directory required')
phase = re.findall(r'(?:^|\s)-M5AuthorPhase=(\S+)', cmd)
if len(phase) != 1 or phase[0] not in ('Lookdev', 'LookdevFull'):
    raise RuntimeError('Use wrapper phase Lookdev or LookdevFull')
FULL = phase[0] == 'LookdevFull'
BACKUP = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Lookdev/backups') / OUT.name
R = dict(status='RUNNING', utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         map=MAP, mode='full_90' if FULL else 'minimum_6', checks=[], assets=[], actors=[],
         runtime='NOT_RUN', visual='NOT_RUN_NULLRHI', default_map_changed=False,
         limitations=['An isolated comparison stage, not the final harbor environment.',
                     'InteriorWarmProxy tests warm light on the open stage; an actual finished indoor scene is NOT_RUN.',
                     'Camera framing uses current saved bounds/CDO eye height and needs actual image review.',
                     'Original character animation and gameplay input remain live; poses are not frozen.',
                     'SoftToon is the current authored approximation; consult its material author report.'])


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    dump()
    if not ok:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def disk(path, extension='.uasset'):
    return PROJECT / 'Content' / Path(path.split('.')[0].removeprefix('/Game/')).with_suffix(extension)


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def load(path):
    obj = A.load_asset(path)
    check('load ' + path, obj is not None)
    return obj


def class_of(path):
    obj = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('load generated class ' + path, obj is not None)
    return obj


def backup(path):
    if not path.is_file():
        return
    relative = path.relative_to(PROJECT / 'Content/HarborCity/M5VS2/Lookdev')
    target = BACKUP / relative
    check('fresh private lookdev backup', not target.exists(), str(target))
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, target)
    check('lookdev backup matches', sha(path) == sha(target))


def new_material(name, rgb, roughness):
    path = ROOT + '/Materials/' + name
    if A.does_asset_exist(path):
        material = load(path)
        check('owned comparison material', A.get_metadata_tag(material, KEY) == OWNER, path)
        backup(disk(path))
        material.modify()
        E.delete_all_material_expressions(material)
    else:
        material = AT.create_asset(name, ROOT + '/Materials', U.Material, U.MaterialFactoryNew())
        check('create comparison material', material is not None, path)
    color = E.create_material_expression(material, U.MaterialExpressionConstant3Vector, 0, 0)
    color.set_editor_property('constant', U.LinearColor(*rgb, 1.0))
    rough = E.create_material_expression(material, U.MaterialExpressionConstant, 0, 120)
    rough.set_editor_property('r', roughness)
    check('floor color connection', E.connect_material_property(color, '', U.MaterialProperty.MP_BASE_COLOR))
    check('floor roughness connection', E.connect_material_property(rough, '', U.MaterialProperty.MP_ROUGHNESS))
    material.set_editor_property('two_sided', False)
    E.recompile_material(material)
    A.set_metadata_tag(material, KEY, OWNER)
    check('save comparison-only material', A.save_loaded_asset(material, False), path)
    R['assets'].append(dict(path=path, sha256=sha(disk(path))))
    return material


def spawn(name, cls, position, rotation=None):
    label = 'HC_M5VS2_Lookdev_' + name
    matches = [a for a in ACT.get_all_level_actors() if a.get_actor_label() == label]
    check('unique actor ' + name, len(matches) <= 1)
    if matches:
        actor = matches[0]
        check('owned actor ' + name, actor.actor_has_tag(U.Name(OWNER)))
        check('expected actor class ' + name, U.MathLibrary.class_is_child_of(actor.get_class(), cls))
        actor.modify()
        actor.set_actor_location(U.Vector(*position), False, True)
        actor.set_actor_rotation(rotation or U.Rotator(), True)
    else:
        actor = ACT.spawn_actor_from_class(cls, U.Vector(*position), rotation or U.Rotator(), False)
        check('spawn actor ' + name, actor is not None)
        actor.set_actor_label(label)
        actor.set_editor_property('tags', [U.Name(OWNER)])
    R['actors'].append(dict(label=label, class_path=actor.get_class().get_path_name(), location=xyz(actor.get_actor_location())))
    return actor


def box(name, center, size, material, collision, shadows):
    actor = spawn(name, U.StaticMeshActor, center)
    component = actor.get_component_by_class(U.StaticMeshComponent)
    component.set_mobility(U.ComponentMobility.STATIC)
    component.set_static_mesh(load('/Engine/BasicShapes/Cube'))
    component.set_material(0, material)
    component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    component.set_cast_shadow(shadows)
    actor.set_actor_scale3d(U.Vector(*(v / 100. for v in size)))
    return actor


def require_material_author():
    candidates = sorted((DOC / 'editor_runtime').glob('author_*_HeroMaterials/author_result.json'),
                        key=lambda p: p.stat().st_mtime, reverse=True)
    for path in candidates:
        data = json.loads(path.read_text('utf-8-sig'))
        if data.get('status') == 'PASS' and set(data.get('variants', {})) == {'MToon', 'SoftToon'}:
            # Never infer successful material authoring from partial files after a failed run.
            check('completed material author has ten slot bindings',
                  all(set(data['variants'][v]) == {'hair', 'body', 'face', 'option', 'costume'} for v in data['variants']))
            R['material_author'] = dict(path=str(path), sha256=sha(path))
            return data['variants']
    raise RuntimeError('Complete PASS HeroMaterials author_result.json is required before map authoring')


def reflected_struct(struct_type, values):
    # Custom reflected UStruct wrappers can reject constructor kwargs in this engine.
    result = struct_type()
    for key, value in values.items():
        result.set_editor_property(key, value)
    return result


def main():
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT.resolve())
    check('no PIE', not U.EditorLevelLibrary.get_pie_worlds(False))
    check('no dirty pre-existing map', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    variants = require_material_author()
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([OLD, HERO, ROOT], force_rescan=True)
    # Read-only preservation; no private paid source or old asset bytes enter reports/backups.
    protected_files = list((PROJECT / 'Content/HarborCity/M5VS1/HeroSelestia').rglob('*.uasset'))
    protected_files += [PROJECT / 'Config/DefaultEngine.ini', PROJECT / 'Content/HarborCity/Maps/L_M5_CyberHarbor.umap']
    protected = {str(p): sha(p) for p in protected_files if p.is_file()}
    check('legacy character and default config preservation set', len(protected) >= 10)
    mode_class = class_of(MODE)
    character_class = class_of(CHARACTER)
    mode_cdo = U.get_default_object(mode_class)
    check('existing Selestia GameMode spawns existing Selestia class',
          mode_cdo.get_editor_property('default_pawn_class') == character_class)
    character_cdo = U.get_default_object(character_class)
    body = character_cdo.get_component_by_class(U.SkeletalMeshComponent)
    capsule = character_cdo.get_component_by_class(U.CapsuleComponent)
    check('existing character body/capsule', body is not None and capsule is not None)
    mesh = body.get_editor_property('skeletal_mesh_asset')
    check('actual Selestia mesh', mesh is not None and mesh.get_path_name().split('.')[0] == OLD + '/SKM_Selestia')
    half_capsule = float(capsule.get_unscaled_capsule_half_height())
    eye_relative = float(character_cdo.get_editor_property('base_eye_height'))
    mesh_offset = body.get_editor_property('relative_location')
    mesh_scale = body.get_editor_property('relative_scale3d')
    mesh_rotation = body.get_editor_property('relative_rotation')
    check('character mesh stays upright and faces actor +X',
          abs(mesh_rotation.pitch) < .001 and abs(mesh_rotation.roll) < .001 and abs(mesh_rotation.yaw + 90.) < .001)
    bounds = mesh.get_bounds()
    origin = bounds.get_editor_property('origin')
    extent = bounds.get_editor_property('box_extent')
    player_z = half_capsule + 2.0
    body_center_z = player_z + mesh_offset.z + origin.z * mesh_scale.z
    body_height = 2.0 * extent.z * mesh_scale.z
    eye_z = player_z + eye_relative
    check('finite useful Selestia framing dimensions', all(math.isfinite(v) for v in (player_z, body_center_z, body_height, eye_z))
          and 100. < body_height < 250. and 100. < eye_z < 220.)
    # The mesh's actual material-slot order is the binding authority.
    slot_rows = mesh.get_editor_property('materials')
    slots = [str(row.get_editor_property('material_slot_name')).removeprefix('Selestia_') for row in slot_rows]
    check('five actual Selestia slots', len(slots) == 5 and set(slots) == {'hair', 'body', 'face', 'option', 'costume'}, slots)
    configured = [reflected_struct(U.HCM5VS2MaterialVariant, dict(label='VS1_DefaultLit', use_original_materials=True, materials=[]))]
    for variant in ('MToon', 'SoftToon'):
        mats = []
        for slot in slots:
            path = variants[variant][slot].split('.')[0]
            check('isolated candidate material path', path.startswith(HERO + '/Materials/'))
            mat = load(path)
            check('candidate material owner', A.get_metadata_tag(mat, KEY) == MATERIAL_OWNER, path)
            mats.append(mat)
        configured.append(reflected_struct(U.HCM5VS2MaterialVariant,
                                          dict(label=variant, use_original_materials=False, materials=mats)))
    native_director = U.load_class(None, '/Script/HarborCity.HCM5VS2LookdevDirector')
    check('compiled lookdev director exists', native_director is not None)
    R['character_readback'] = dict(game_mode=mode_class.get_path_name(), character_class=character_class.get_path_name(),
        mesh=mesh.get_path_name(), capsule_half_height=half_capsule, base_eye_height=eye_relative,
        player_start_z=player_z, predicted_initial_eye_z=eye_z, mesh_offset=xyz(mesh_offset),
        mesh_scale=xyz(mesh_scale), bounds_origin=xyz(origin), bounds_extent=xyz(extent),
        predicted_body_center_z=body_center_z, predicted_body_height=body_height, material_slot_order=slots,
        runtime_eye_and_actor_position='Director reads and records actual runtime values; these saved-CDO predictions are not runtime evidence.')
    if A.does_asset_exist(MAP):
        world_asset = load(MAP)
        check('owned existing lookdev map', A.get_metadata_tag(world_asset, KEY) == OWNER)
        backup(disk(MAP, '.umap'))
        check('load isolated lookdev map', LEVEL.load_level(MAP))
    else:
        check('create isolated lookdev map', LEVEL.new_level(MAP, False))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('authoring exact isolated world', world.get_path_name().split('.')[0] == MAP, world.get_path_name())
    A.set_metadata_tag(world, KEY, OWNER)
    # Persist ownership first, so an interrupted first run can safely resume.
    check('save owned empty/current lookdev map', LEVEL.save_current_level())
    foreign = [a.get_actor_label() for a in ACT.get_all_level_actors()
               if a.get_class().get_name() not in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume')
               and not a.actor_has_tag(U.Name(OWNER))]
    check('no unrelated actor would be overwritten', not foreign, foreign)
    world.get_world_settings().set_editor_property('default_game_mode', mode_class)
    floor = new_material('M_LookdevNeutralFloor', (.18, .18, .18), .8)
    backdrop = new_material('M_LookdevNeutralBackdrop', (.24, .27, .31), .85)
    box('Floor', (0, 0, -10), (3000, 3000, 20), floor, True, True)
    # Backdrops are visible geometry, not lights; they do not block the comparison key.
    box('BackdropWest', (-1100, 0, 300), (10, 2800, 600), backdrop, False, False)
    box('BackdropEast', (1100, 0, 300), (10, 2800, 600), backdrop, False, False)
    box('BackdropNorth', (0, 1100, 300), (2800, 10, 600), backdrop, False, False)
    box('BackdropSouth', (0, -1100, 300), (2800, 10, 600), backdrop, False, False)
    spawn('PlayerStart', U.PlayerStart, (0, 0, player_z), U.Rotator(yaw=0.))
    sun = spawn('MainLight', U.DirectionalLight, (300, 0, 500), U.Rotator(pitch=-28., yaw=155.))
    light = sun.get_component_by_class(U.DirectionalLightComponent)
    light.set_mobility(U.ComponentMobility.MOVABLE)
    light.set_light_color(U.LinearColor(1, 1, 1, 1))
    light.set_intensity(3.)
    light.set_editor_property('atmosphere_sun_light', True)
    sky = spawn('SkyLight', U.SkyLight, (0, 0, 500))
    sky_component = sky.get_component_by_class(U.SkyLightComponent)
    sky_component.set_mobility(U.ComponentMobility.MOVABLE)
    sky_component.set_editor_property('real_time_capture', True)
    sky_component.set_intensity(.4)
    spawn('Atmosphere', U.SkyAtmosphere, (0, 0, 0))
    post = spawn('FixedExposure', U.PostProcessVolume, (0, 0, 0))
    post.set_editor_property('unbound', True)
    settings = post.get_editor_property('settings')
    exposure = dict(override_auto_exposure_method=True, auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
                    override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
                    override_auto_exposure_bias=True, auto_exposure_bias=0.,
                    override_motion_blur_amount=True, motion_blur_amount=0.,
                    override_bloom_intensity=True, bloom_intensity=.12)
    for key, value in exposure.items():
        settings.set_editor_property(key, value)
    post.set_editor_property('settings', settings)
    # Filmic/Lumen/TSR settings remain the project settings; this is not a screenshot-only render path.
    R['exposure'] = {key: str(post.get_editor_property('settings').get_editor_property(key)) for key in exposure}
    director = spawn('Director', native_director, (0, 0, -100))
    director.set_editor_property('directional_light', sun)
    director.set_editor_property('material_variants', configured)
    director.set_editor_property('use_player_eye_height', True)
    director.set_editor_property('hide_hud', True)
    vertical_tan = math.tan(math.radians(40. / 2.)) / (1920. / 1080.)
    full_distance = body_height / (.88 * 2. * vertical_tan)
    half_height = body_height * .58
    half_distance = half_height / (.9 * 2. * vertical_tan)
    half_center = body_center_z + body_height * .5 - half_height * .5
    face_distance = 52. / (2. * vertical_tan)
    views = [('FaceFront', face_distance, 0., eye_z, True),
             ('FullFront', full_distance, 0., body_center_z, False)]
    if FULL:
        views = [('FaceFront', face_distance, 0., eye_z, True),
                 ('HalfFront', half_distance, 0., half_center, False),
                 ('FullFront', full_distance, 0., body_center_z, False),
                 ('FullThreeQuarter', full_distance, 30., body_center_z, False),
                 ('FullSide', full_distance, 90., body_center_z, False),
                 ('FullBack', full_distance, 180., body_center_z, False)]
    lights = [('Neutral', (-28., 155.), (1., 1., 1.), 3., (.30, .32, .38))]
    if FULL:
        lights += [('Afternoon', (-48., 135.), (1., .91, .78), 3.5, (.28, .32, .40)),
                   ('Dusk', (-12., 155.), (1., .52, .28), 2., (.30, .32, .43)),
                   ('NightMoonProxy', (-38., 135.), (.48, .64, 1.), .28, (.045, .065, .12)),
                   ('InteriorWarmProxy', (-32., 155.), (1., .68, .40), 2.2, (.26, .24, .27))]
    shots, plan = [], []
    for light_name, direction, light_color, intensity, ambient in lights:
        for view_name, distance, angle, height, use_eye in views:
            radians = math.radians(angle)
            position = [distance * math.cos(radians), distance * math.sin(radians), height]
            camera_yaw = (angle + 180.) % 360.
            for variant_index, variant_name in enumerate(('VS1_DefaultLit', 'MToon', 'SoftToon')):
                label = light_name + '_' + view_name
                shot = reflected_struct(U.HCM5VS2LookdevShot,
                    dict(label=label, variant_index=variant_index, camera_location=U.Vector(*position),
                         camera_rotation=U.Rotator(pitch=0., yaw=camera_yaw, roll=0.), use_player_eye_height=use_eye,
                         light_direction=U.Rotator(pitch=direction[0], yaw=direction[1]),
                         light_color=U.LinearColor(*light_color, 1.), light_intensity=intensity,
                         ambient_color=U.LinearColor(*ambient, 1.)))
                shots.append(shot)
                plan.append(dict(label=label, variant=variant_name, variant_index=variant_index,
                                 camera_location=position, camera_yaw=camera_yaw, camera_pitch=0.,
                                 use_player_eye_height=use_eye, fov=40., light_rotation=direction,
                                 light_color=light_color, intensity=intensity, ambient=ambient))
    director.set_editor_property('shots', shots)
    check('native director plan count', len(director.get_editor_property('shots')) == (90 if FULL else 6))
    check('native director has three variants', len(director.get_editor_property('material_variants')) == 3)
    check('only one PlayerStart', len([a for a in ACT.get_all_level_actors() if isinstance(a, U.PlayerStart)]) == 1)
    check('only one light director', len([a for a in ACT.get_all_level_actors() if a.get_class() == native_director]) == 1)
    R['shot_plan'] = plan
    check('save isolated final map', LEVEL.save_current_level())
    check('saved map exists', disk(MAP, '.umap').is_file())
    R['assets'].append(dict(path=MAP, sha256=sha(disk(MAP, '.umap'))))
    check('reload saved isolated map', LEVEL.load_level(MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('saved GameMode remains old Selestia', world.get_world_settings().get_editor_property('default_game_mode') == mode_class)
    saved_director = next(a for a in ACT.get_all_level_actors() if a.get_class() == native_director)
    check('saved plans retain exact count', len(saved_director.get_editor_property('shots')) == len(shots))
    check('saved main light reference', saved_director.get_editor_property('directional_light') is not None)
    check('legacy paid character assets and default map/config unchanged', all(Path(p).is_file() and sha(p) == h for p, h in protected.items()))
    R['preservation'] = dict(files_checked=len(protected), modified_files=[])
    R['status'] = 'PASS'
    R['pass_scope'] = 'Native author/save/reload and readback only; first runtime capture and visual review NOT_RUN.'
    R['runtime_arguments'] = [MAP, '-game', '-M5VS2Lookdev', '-M5VS2EvidenceDir="D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/lookdev_capture"',
                              '-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_Lookdev_UNIQUE', '-ResX=1920', '-ResY=1080', '-windowed', '-language=en']
    R['optional_runtime_argument'] = '-M5VS2AutoQuit (only completes automatically when no Esc stop occurred)'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    R['ended_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('M5-VS2 lookdev author failed; see author_result.json')
