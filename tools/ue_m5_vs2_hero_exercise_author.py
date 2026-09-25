"""Author an isolated opt-in Selestia animation/face/secondary-motion exercise.

Native author only; never launches gameplay or changes the fair comparison map.
Use ue_m5_vs2_author_run.ps1 -Phase HeroExercise -ScriptPath this_file.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
ROOT = '/Game/HarborCity/M5VS2/HeroExercise'
MAP = ROOT + '/L_SelestiaExercise'
SOURCE_MAP = '/Game/HarborCity/M5VS2/Lookdev/L_SelestiaLookdev'
HERO = '/Game/HarborCity/M5VS2/HeroSelestia'
CHARACTER = HERO + '/BP_M5VS2_Selestia'
MODE = HERO + '/BP_M5VS2_SelestiaGameMode'
ANIMATION = HERO + '/Animation/ABP_M5VS2_Selestia_Physics'
OWNER = 'HarborCity_M5_VS2_HeroExercise'
SOURCE_OWNER = 'HarborCity_M5_VS2_SelestiaLookdev'
KEY = 'HarborCityOwnedBy'
A = U.EditorAssetLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
assert len(args) == 1
OUT = Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to(DOC.resolve()) and not (OUT / 'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
BACKUP = Path('E:/GameDev/Assets/HarborCity/M5_VS2/HeroExercise/backups') / OUT.name
R = dict(status='RUNNING', map=MAP, source_map=SOURCE_MAP, checks=[], assets=[],
         runtime='NOT_RUN', visual='NOT_RUN_NULLRHI', input_level='NATIVE_FUNCTION_CALLS_NOT_ACTION_OR_OS_INPUT',
         limitations=['Isolated animation exercise, not final 50m harbor art.',
                     'SoftToon is provisional for visibility, not user-selected shader.',
                     'Bone response cannot prove collision-free surfaces or natural motion.',
                     'Text mouth exercise calls the component; dialogue-gameplay integration is separate.'])


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    dump()
    if not ok:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def disk(path, ext='.uasset'):
    return PROJECT / 'Content' / Path(path.split('.')[0].removeprefix('/Game/')).with_suffix(ext)


def load(path):
    result = A.load_asset(path)
    check('load ' + path, result is not None)
    return result


def class_of(path):
    result = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('generated class ' + path, result is not None)
    return result


def struct(cls, **values):
    result = cls()
    for key, value in values.items():
        result.set_editor_property(key, value)
    return result


def main():
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT.resolve())
    check('no PIE/dirty maps', not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT, HERO, '/Game/HarborCity/M5VS2/Lookdev'], force_rescan=True)
    protected = [disk(SOURCE_MAP, '.umap'), disk(CHARACTER), disk(MODE), disk(ANIMATION), PROJECT / 'Config/DefaultEngine.ini']
    protected += list((PROJECT / 'Content/HarborCity/M5VS1/HeroSelestia').rglob('*.uasset'))
    before = {str(p): sha(p) for p in protected}
    source = load(SOURCE_MAP)
    check('owned source stage', A.get_metadata_tag(source, KEY) == SOURCE_OWNER)
    hero, mode, anim = class_of(CHARACTER), class_of(MODE), class_of(ANIMATION)
    check('new mode spawns new hero', U.get_default_object(mode).get_editor_property('default_pawn_class') == hero)
    cdo = U.get_default_object(hero)
    mesh_component = cdo.get_component_by_class(U.SkeletalMeshComponent)
    mesh = mesh_component.get_editor_property('skeletal_mesh_asset')
    check('new hero has physics/look AnimBP before BeginPlay', mesh_component.get_editor_property('anim_class') == anim)
    expression_class = U.load_class(None, '/Script/HarborCity.HCM5VS2ExpressionComponent')
    # SCS component is instantiated at spawn rather than exposed on the generated CDO.
    check('compiled expression class exists', expression_class is not None)
    native = U.load_class(None, '/Script/HarborCity.HCM5VS2HeroExerciseDirector')
    check('compiled isolated exercise director', native is not None)
    if A.does_asset_exist(MAP):
        world_asset = load(MAP)
        check('owned existing exercise map', A.get_metadata_tag(world_asset, KEY) == OWNER)
        path = disk(MAP, '.umap')
        backup = BACKUP / path.name
        check('fresh map backup', not backup.exists())
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, backup)
        check('exact map backup', sha(path) == sha(backup))
        check('load exact exercise map', LEVEL.load_level(MAP))
    else:
        check('new exercise map absent on disk', not disk(MAP, '.umap').exists())
        # Use the editor's world lifecycle: duplicating an initialized UWorld via
        # AssetTools then LoadLevel leaves a standalone world and fails GC.
        check('native stage from comparison template', LEVEL.new_level_from_template(MAP, SOURCE_MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact current world', world.get_path_name().split('.')[0] == MAP)
    A.set_metadata_tag(world, KEY, OWNER)
    world.get_world_settings().set_editor_property('default_game_mode', mode)
    for actor in ACT.get_all_level_actors():
        if actor.get_class().get_name() in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume'):
            continue
        check('only copied/owned stage actors', actor.actor_has_tag(U.Name(SOURCE_OWNER)) or actor.actor_has_tag(U.Name(OWNER)), actor.get_actor_label())
        if actor.get_class().get_name() in ('HCM5VS2LookdevDirector', 'HCM5VS2HeroExerciseDirector'):
            check('remove copied/previous director from new map only', ACT.destroy_actor(actor))
        else:
            actor.set_editor_property('tags', [U.Name(OWNER)])
    check('isolated stage has no appearance-overriding Experience',
          not any(a.get_class().get_name() == 'HCM3Experience' for a in ACT.get_all_level_actors()))
    director = ACT.spawn_actor_from_class(native, U.Vector(0, 0, -100), U.Rotator(), False)
    check('spawn exercise director', director is not None)
    director.set_actor_label('HC_M5VS2_HeroExercise_Director')
    director.set_editor_property('tags', [U.Name(OWNER)])
    director.set_editor_property('expected_character_class', hero)
    director.set_editor_property('expected_animation_class', anim)
    director.set_editor_property('expected_mesh', mesh)
    lights = [a for a in ACT.get_all_level_actors() if U.MathLibrary.class_is_child_of(a.get_class(), U.DirectionalLight.static_class())]
    check('one actual stage directional light', len(lights) == 1)
    director.set_editor_property('directional_light', lights[0])
    slots = [str(s.get_editor_property('material_slot_name')).removeprefix('Selestia_') for s in mesh.get_editor_property('materials')]
    materials = [load(HERO + '/Materials/SoftToon/M_SoftToon_' + slot) for slot in slots]
    check('only owned provisional SoftToon materials', all(A.get_metadata_tag(m, KEY) == 'HarborCity_M5_VS2_HeroMaterials' for m in materials))
    director.set_editor_property('exercise_materials', materials)
    # Choose real authored secondary chains; use bone/anchor relative transforms so
    # translating/turning the whole actor cannot masquerade as secondary motion.
    config_path = DOC / 'research/SELESTIA_KAWAII_INITIAL_CONFIG.json'
    config = json.loads(config_path.read_text('utf-8'))
    observed = []
    for group_name, anchor in [('LongHair', 'Head'), ('SideLong', 'Head'), ('Skirt', 'Hips')]:
        group = next(g for g in config['groups'] if g['name'] == group_name)
        for chain in group['chains'][:2]:
            for bone in (chain['bones'][0], chain['bones'][-2]):
                observed.append(struct(U.HCM5VS2ExerciseBone, bone=U.Name(bone), anchor=U.Name(anchor), family=group_name))
    director.set_editor_property('observed_bones', observed)
    R['physics_config'] = dict(path=str(config_path), sha256=sha(config_path), probe_count=len(observed))
    plan = []

    def phase(label, action, duration, capture=-1., face=False, emotion='Neutral'):
        plan.append(dict(label=label, action=action, duration=duration, capture_at=capture,
                         face_camera=face, emotion=U.Name(emotion)))

    phase('Warmup', 'Warmup', 10.)
    phase('Rest_Blink', 'Rest', 6., 0., True)  # Capture only an actual automatic closed blink.
    phase('Walk', 'Walk', 1.6, .8)
    phase('Walk_Turn', 'TurnWalk', 1.1)
    phase('Run_Return', 'RunHome', 2.4, .7)
    phase('Jump', 'Jump', 2., .28)
    phase('Body_Turn', 'Turn', 2.)
    phase('Recovered', 'Rest', 3., 2.)
    for emotion in ('Happy', 'Surprised', 'Angry', 'Sad', 'Shy', 'Serious'):
        phase('Emotion_' + emotion, 'Emotion', 1.6, 1., True, emotion)
    phase('Text_Mouth', 'Speech', 3., face=True)
    phase('Look_Left', 'LookLeft', 2., face=True)
    phase('Look_Right', 'LookRight', 2., 1.4, True)
    phase('Final_Rest', 'Rest', 2.)
    director.set_editor_property('phases', [struct(U.HCM5VS2ExercisePhase, **row) for row in plan])
    R['plan'] = [{**row, 'emotion': str(row['emotion'])} for row in plan]
    R['expected_screenshots'] = sum(row['capture_at'] >= 0 for row in plan)
    check('bounded twelve real captures', R['expected_screenshots'] == 12)
    check('native save exercise map', LEVEL.save_current_level())
    check('all prior assets/config byte-preserved', all(Path(p).is_file() and sha(p) == h for p, h in before.items()))
    check('saved new map exists', disk(MAP, '.umap').is_file())
    R['assets'] = [dict(path=MAP, sha256=sha(disk(MAP, '.umap')))]
    R['binding'] = dict(character=hero.get_path_name(), animation=anim.get_path_name(), mesh=mesh.get_path_name(),
                        game_mode=mode.get_path_name(), material_variant='SoftToon provisional')
    R['runtime_arguments'] = [MAP, '-game', '-M5VS2HeroExercise', '-M5VS2AutoQuit', '-M5VS2EvidenceDir=<unique absolute directory>',
                              '-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_Exercise_<unique>', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes']
    R['status'] = 'PASS'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('Hero exercise author failed; original evidence retained')
