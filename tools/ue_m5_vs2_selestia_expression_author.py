"""Root schedules after VS2 Hero BP + Physics ABP are natively authored.

Adds only the optional face component and native Head/Eye LookAt graph to new
VS2 assets. No map/old BP/animation/mesh edits. -M5VS2ExpressionOnly permits
the face component before Physics authoring; that mode reports look NOT_RUN.
"""
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import shutil
import traceback
import uuid
import unreal as U

WORK = Path('D:/科研学习/codex学习')
CONTENT = WORK / 'HarborCity/Content'
DOCS = WORK / 'docs/HarborCity_M5_VS2'
ROOT = '/Game/HarborCity/M5VS2/HeroSelestia'
HERO = ROOT + '/BP_M5VS2_Selestia'
ANIM = ROOT + '/Animation/ABP_M5VS2_Selestia_Physics'
OLD = '/Game/HarborCity/M5VS1/HeroSelestia'
A = U.EditorAssetLibrary
command_line = U.SystemLibrary.get_command_line()
matches = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', command_line)
if len(matches) != 1:
    raise RuntimeError('One -M5EvidenceDir is required')
OUT = Path(matches[0][0] or matches[0][1]).resolve()
if not OUT.is_relative_to(DOCS.resolve()):
    raise RuntimeError('Evidence must remain within M5_VS2 docs')
OUT.mkdir(parents=True, exist_ok=True)
REPORT = OUT / 'selestia_expression_binding.json'
WRAPPER_REPORT = OUT / 'author_result.json'
if REPORT.exists() or WRAPPER_REPORT.exists():
    raise RuntimeError('Fresh report directory required; old evidence preserved')
FACE_ONLY = bool(re.search(r'(?:^|\s)-M5VS2ExpressionOnly(?:\s|$)', command_line))
R = dict(status='RUNNING', checks=[], errors=[], runtime='NOT_RUN', visual='NOT_RUN',
         face_only=FACE_ONLY, dialogue_speaker_hook='NOT_RUN',
         old_bp_or_map_mutation=False)


def write():
    payload = json.dumps(R, ensure_ascii=False, indent=2) + '\n'
    REPORT.write_text(payload, encoding='utf-8')
    WRAPPER_REPORT.write_text(payload, encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    write()
    if not ok:
        raise RuntimeError(name + ': ' + repr(observed))


def disk(asset_path):
    if not asset_path.startswith('/Game/'):
        raise RuntimeError('Game asset expected')
    return CONTENT / Path(asset_path.removeprefix('/Game/').split('.')[0]).with_suffix('.uasset')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def generated(bp):
    value = U.BlueprintEditorLibrary.generated_class(bp)
    check('generated class: ' + bp.get_name(), value is not None)
    return value


def owned_component(cdo, cls):
    value = cdo.get_component_by_class(cls)
    check('new character CDO component: ' + cls.static_class().get_name(),
          value is not None and value.get_path_name().split('.')[0] == HERO,
          value.get_path_name() if value else None)
    return value


try:
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT, OLD], force_rescan=True)
    paths = [HERO] + ([] if FACE_ONLY else [ANIM])
    objects = {}
    for path in paths:
        check('native asset persisted before extension: ' + path, disk(path).is_file())
        obj = A.load_asset(path)
        check('asset loaded: ' + path, obj is not None)
        owner = A.get_metadata_tag(obj, 'HarborCityOwnedBy')
        check('VS2 owned author asset: ' + path, owner.startswith('HarborCity_M5_VS2'), owner)
        objects[path] = obj
    protected_paths = [OLD + '/BP_M5_Selestia', OLD + '/SKM_Selestia', OLD + '/SK_Selestia',
                       OLD + '/Animation/ABP_M4R2_Player_Selestia']
    protected = {p: sha(disk(p)) for p in protected_paths}
    bp = objects[HERO]
    cdo = U.get_default_object(generated(bp))
    check('compatible native character', U.MathLibrary.class_is_child_of(
        cdo.get_class(), U.HCM1Character.static_class()))
    body = owned_component(cdo, U.SkeletalMeshComponent)
    mesh = body.get_skeletal_mesh_asset()
    check('actual target mesh', mesh is not None)
    mesh_path = mesh.get_path_name().split('.')[0]
    protected[mesh_path] = sha(disk(mesh_path))
    skeleton = mesh.get_editor_property('skeleton')
    check('actual target skeleton', skeleton is not None)
    skeleton_path = skeleton.get_path_name().split('.')[0]
    protected[skeleton_path] = sha(disk(skeleton_path))
    saved_transform = [body.get_editor_property(p) for p in
                       ('relative_location', 'relative_rotation', 'relative_scale3d')]
    backup_dir = WORK / 'assets/M5_VS2/hero/Selestia/PrivateNativeBackups' / (
        dt.datetime.now(dt.timezone.utc).strftime('%Y%m%d_%H%M%S_%f') + '_expression_' + uuid.uuid4().hex[:8])
    backup_dir.mkdir(parents=True, exist_ok=False)
    backups = []
    for path in paths:
        original = disk(path)
        backup = backup_dir / (original.stem + '.before.uasset')
        shutil.copy2(original, backup)
        check('private backup bytes: ' + path, sha(original) == sha(backup))
        backups.append(dict(asset=path, private_path=str(backup), sha256=sha(backup)))
    R['private_backups_do_not_zip'] = backups
    result = json.loads(U.HCM5VS2ExpressionEditor.install_hero_expression_component(bp))
    R['native_expression'] = result
    check('native SCS optional expression install', result.get('status') == 'PASS', result.get('error'))
    if not FACE_ONLY:
        abp = objects[ANIM]
        result = json.loads(U.HCM5VS2ExpressionEditor.configure_look_graph(abp, mesh))
        R['native_look'] = result
        check('native head/eyes before Kawaii', result.get('status') == 'PASS', result.get('error'))
        check('save new look/physics ABP', A.save_loaded_asset(abp, False))
        anim_class = generated(abp)
        cdo = U.get_default_object(generated(bp))
        body = owned_component(cdo, U.SkeletalMeshComponent)
        combat = owned_component(cdo, U.HCM4CombatComponent)
        body.modify(); combat.modify()
        body.set_editor_property('anim_class', anim_class)
        combat.set_editor_property('player_animation_blueprint', anim_class)
        check('new character recompile after AnimBP binding', U.BlueprintEditorLibrary.compile_blueprint(bp))
        cdo = U.get_default_object(generated(bp))
        body = owned_component(cdo, U.SkeletalMeshComponent)
        combat = owned_component(cdo, U.HCM4CombatComponent)
        check('new graph used from BeginPlay', body.get_editor_property('anim_class') == anim_class
              and combat.get_editor_property('player_animation_blueprint') == anim_class)
        R['map_experience_followup'] = 'Root must bind NEW map Experience.PlayerAnimationClass to ' + ANIM + '_C; this author never changes a map.'
    else:
        R['native_look'] = dict(status='NOT_RUN', reason='Explicit expression-only author mode')
    current = U.get_default_object(generated(bp))
    body = owned_component(current, U.SkeletalMeshComponent)
    check('mesh preserved', body.get_skeletal_mesh_asset() == mesh)
    check('proportion/location/rotation preserved', all(body.get_editor_property(p) == v for p, v in zip(
        ('relative_location', 'relative_rotation', 'relative_scale3d'), saved_transform)))
    check('save only new character Blueprint', A.save_loaded_asset(bp, False))
    check('protected baseline / mesh / skeleton bytes unchanged', all(sha(disk(p)) == h for p, h in protected.items()))
    check('no maps dirtied', not list(U.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    R['assets'] = [dict(path=p, disk=str(disk(p)), sha256=sha(disk(p))) for p in paths]
    R['runtime_required'] = ['FaceReady 34 morphs', 'random blink closed hold', 'six emotions and actual mesh morph readback',
                             'visible-text mouth then stop', 'accepted damage/attack expression',
                             'Head/LeftEye/RightEye target change and return', 'FP/combat/driving look alpha zero',
                             'hair root follows head before secondary physics']
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['errors'].append(traceback.format_exc())
finally:
    write()
    U.log('M5VS2_EXPRESSION_AUTHOR ' + R['status'] + ' ' + str(REPORT))
