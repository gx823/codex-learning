"""Root-only native author for isolated current-Hero long-hair diagnosis.

Phase HairReviewAuthor. Requires a completed HeroRev2IntegrationReload; never
falls back to old hero assets. Runtime is a separate explicit command line.
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
A = U.EditorAssetLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)
OWNER = 'HarborCity_M5VS2_HairReview'
MODE = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'


def arg(name):
    rows = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(rows) == 1, 'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT = Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
assert arg('M5AuthorPhase') == 'HairReviewAuthor'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/HairReview/Run_'+TOKEN
MAP = ROOT+'/L_HairReview'
R = dict(schema='HarborCity.M5VS2.HairReview.Author.v1', status='RUNNING', map=MAP,
         phase='HairReviewAuthor', owner=OWNER, assets=[], checks=[], actors=[], runtime='NOT_RUN',
         visual_acceptance='USER_REVIEW', scope='Isolated current-Hero diagnostic, not environment art or packaged gameplay.',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(label, okay, observed=None):
    R['checks'].append(dict(check=label, status='PASS' if okay else 'FAIL', observed=observed)); dump()
    if not okay: raise RuntimeError(label+': '+repr(observed))


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()


def package(obj): return (obj.get_path_name() if hasattr(obj, 'get_path_name') else str(obj)).split('.')[0]
def disk(obj, ext='.uasset'):
    path = package(obj)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix(ext)


def protect(file, digest=None):
    file = Path(file).resolve(); actual = sha(file)
    check('source bytes '+str(file), digest is None or actual == digest, actual)
    PROTECTED[str(file)] = actual


def load(path, kind=None):
    obj = A.load_asset(path)
    check('native load '+path, obj is not None and (kind is None or isinstance(obj, kind)))
    return obj


def generated(path):
    obj = U.load_class(None, path+'.'+path.rsplit('/', 1)[1]+'_C')
    check('generated native class', obj is not None, path)
    return obj


def save(obj):
    path = package(obj); check('only new diagnostic packages saved', path.startswith(ROOT+'/'), path)
    check('native package save', A.save_asset(path, False), path)
    R['assets'].append(dict(path=path, sha256=sha(disk(path)))); dump()


def spawn(label, kind, location=(0, 0, 0), rotation=None):
    check('bounded actor count', len(R['actors']) < 12)
    obj = ACT.spawn_actor_from_class(kind, U.Vector(*location), rotation or U.Rotator(), False)
    check('native actor '+label, obj is not None)
    obj.set_actor_label('HCHair_'+label)
    obj.set_editor_property('tags', [U.Name(OWNER), U.Name('HCHair_'+label)])
    R['actors'].append(dict(name=obj.get_name(), label=label, class_path=obj.get_class().get_path_name()))
    return obj


def box(label, position, scale, material):
    obj = spawn(label, U.StaticMeshActor, position)
    comp = obj.get_component_by_class(U.StaticMeshComponent)
    comp.set_mobility(U.ComponentMobility.STATIC)
    check('native cube binding', comp.set_static_mesh(A.load_asset('/Engine/BasicShapes/Cube')))
    comp.set_material(0, material); comp.set_collision_profile_name('BlockAll')
    obj.set_actor_scale3d(U.Vector(*scale))


try:
    check('fresh namespace', not A.does_directory_exist(ROOT) and not disk(MAP, '.umap').parent.exists(), ROOT)
    files = sorted((DOC/'editor_runtime').glob('author_*_HeroRev2IntegrationReload/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True)
    candidates = [p for p in files if json.loads(p.read_text('utf-8-sig')).get('status') == 'PASS']
    check('actual final Hero reload exists', bool(candidates))
    source = candidates[0]; prior = json.loads(source.read_text('utf-8-sig'))
    command = source.parent/'commandlet.json'; launch = json.loads(command.read_text('utf-8-sig'))
    protect(source); protect(command)
    check('completed fresh-process native Hero reload', launch.get('phase') == 'HeroRev2IntegrationReload'
          and launch.get('status') == 'PASS' and launch.get('exit_code') == 0 and prior.get('fresh_process_disk_readback') == 'PASS')
    check('exact new Hero package', re.fullmatch(r'/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/BP_HeroRev2_Integrated_[0-9a-f]{12}', prior['blueprint']) is not None)
    for row in prior['assets']: protect(disk(row['path']), row['sha256'])
    for path in [MODE, prior['expected']['mesh'], prior['expected']['animation'], *prior['expected']['materials']]: protect(disk(path))
    R['source'] = dict(report=str(source), report_sha256=sha(source), hero=prior['blueprint'], expected=prior['expected'])
    hero_class = generated(prior['blueprint'])
    mesh = load(prior['expected']['mesh'], U.SkeletalMesh)
    anim = generated(prior['expected']['animation'])
    cdo = U.get_default_object(hero_class)
    body = cdo.get_editor_property('mesh')
    check('source exact runtime CDO mesh/animation', body.get_editor_property('skeletal_mesh_asset') == mesh and body.get_editor_property('anim_class') == anim)
    check('source exact five materials', [package(body.get_material(i)) for i in range(body.get_num_materials())] == prior['expected']['materials'])
    gm = A.duplicate_asset(MODE, ROOT+'/BP_HairReviewGameMode')
    check('private diagnostic GameMode copy', gm is not None)
    U.get_default_object(gm.generated_class()).set_editor_property('default_pawn_class', hero_class)
    check('native GameMode compile', U.BlueprintEditorLibrary.compile_blueprint(gm))
    check('private GM final Hero preserved', U.get_default_object(gm.generated_class()).get_editor_property('default_pawn_class') == hero_class)
    save(gm)
    material = U.AssetToolsHelpers.get_asset_tools().create_asset('M_NeutralDiagnostic', ROOT, U.Material, U.MaterialFactoryNew())
    check('new neutral diagnostic backdrop material', material is not None)
    material.set_editor_property('shading_model', U.MaterialShadingModel.MSM_UNLIT)
    expression = U.MaterialEditingLibrary.create_material_expression(material, U.MaterialExpressionConstant3Vector, -100, 0)
    expression.set_editor_property('constant', U.LinearColor(.075, .075, .075, 1))
    check('native neutral material connection', U.MaterialEditingLibrary.connect_material_property(expression, '', U.MaterialProperty.MP_EMISSIVE_COLOR))
    U.MaterialEditingLibrary.recompile_material(material); save(material)
    check('native private diagnostic map', LEVEL.new_level(MAP, False))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact current world', package(world) == MAP)
    A.set_metadata_tag(world, 'HarborCityOwnedBy', OWNER)
    world.get_world_settings().set_editor_property('default_game_mode', gm.generated_class())
    box('Floor', (0, 0, -25), (60, 60, .5), material)
    box('Backdrop', (-150, 0, 240), (.2, 50, 8), material)
    spawn('Start', U.PlayerStart, (0, 0, 98), U.Rotator(yaw=0))
    director = spawn('Director', U.HCM5VS2HairReviewDirector)
    for name, value in dict(expected_character_class=hero_class, expected_animation_class=anim,
                            expected_mesh=mesh, source_report=str(source)).items(): director.set_editor_property(name, value)
    sun = spawn('Sun', U.DirectionalLight, (0, 0, 700), U.Rotator(pitch=-35, yaw=145)).get_component_by_class(U.DirectionalLightComponent)
    sun.set_mobility(U.ComponentMobility.MOVABLE); sun.set_intensity(3.); sun.set_editor_property('atmosphere_sun_light', True)
    sky = spawn('Sky', U.SkyLight, (0, 0, 400)).get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE); sky.set_editor_property('real_time_capture', True); sky.set_intensity(.4)
    spawn('Atmosphere', U.SkyAtmosphere)
    post = spawn('FixedExposure', U.PostProcessVolume); post.set_editor_property('unbound', True)
    settings = post.get_editor_property('settings')
    for name, value in dict(override_auto_exposure_method=True, auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
                            override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
                            override_auto_exposure_bias=True, auto_exposure_bias=0., override_motion_blur_amount=True, motion_blur_amount=0.).items(): settings.set_editor_property(name, value)
    post.set_editor_property('settings', settings)
    check('native map save', LEVEL.save_current_level())
    R['assets'].append(dict(path=MAP, sha256=sha(disk(MAP, '.umap'))))
    check('native map reload', LEVEL.load_level(MAP))
    directors = [x for x in ACT.get_all_level_actors() if isinstance(x, U.HCM5VS2HairReviewDirector)]
    check('persisted unique director and exact bindings', len(directors) == 1
          and directors[0].get_editor_property('expected_character_class') == hero_class
          and directors[0].get_editor_property('expected_animation_class') == anim
          and directors[0].get_editor_property('expected_mesh') == mesh
          and directors[0].get_editor_property('source_report') == str(source))
    check('only three new packages', len(R['assets']) == 3)
    R['runtime_arguments'] = '-M5VS2HairReview -M5VS2AutoQuit -M5VS2EvidenceDir=<unique docs run> -windowed -ResX=1920 -ResY=1080'
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc(); U.log_error(R['error'])
finally:
    changed = [file for file, digest in PROTECTED.items() if not Path(file).is_file() or sha(file) != digest]
    R['source_preservation'] = dict(files=len(PROTECTED), changed=changed)
    if changed: R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS': raise RuntimeError('HairReviewAuthor failed; retain this attempt and evidence')
