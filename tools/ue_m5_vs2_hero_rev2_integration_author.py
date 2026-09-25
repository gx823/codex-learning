"""Join actual PASS outline/GAS/halo/flight assets in a NEW Hero Blueprint.

Root alone executes HeroRev2Integration -> HeroRev2IntegrationReload. No map,
live Hero, source asset, review-selection file or gameplay tuning is written.
SCS templates are inspected separately from inherited native CDO components.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT, DOC = WORK/'HarborCity', WORK/'docs/HarborCity_M5_VS2'
BASE = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia'
HALO_REPORT = DOC/'editor_runtime/author_20260925_034631_710_7df27246_HaloParts/author_result.json'
HALO_SHA = '235ed98883696a8f287e1a9e5d1b6566deacdb77cb7dbc8ce39918fb1cf4bddf'
OWNER = 'HarborCity_M5_VS2_HeroRev2_Integration'
SLOTS = ('hair', 'body', 'face', 'option', 'costume')
A, E, B = U.EditorAssetLibrary, U.MaterialEditingLibrary, U.BlueprintEditorLibrary


def argument(name):
    rows = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(rows) != 1: raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]


OUT, PHASE = Path(argument('M5EvidenceDir')).resolve(), argument('M5AuthorPhase')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
assert PHASE in ('HeroRev2Integration', 'HeroRev2IntegrationReload')
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/HeroRev2/Review_'+TOKEN
R = dict(schema='HarborCity.M5VS2.HeroRev2Integration.v1', status='RUNNING', phase=PHASE,
         destination=ROOT, checks=[], inputs={}, assets=[], runtime='NOT_RUN', visual='USER_REVIEW',
         first_person_runtime='NOT_RUN', outline_runtime='NOT_RUN', halo_runtime='NOT_RUN',
         selection_file_written=False, map_writes=0, old_asset_writes=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), limitations=[
             'Native CDO/SCS binding and fresh-process readback do not prove final pose, outline stability, halo motion or flight animation quality.',
             'The FlightPoseReload graph adds original flight loops and banking to retained GAS ground motion; runtime naturalness and user approval remain NOT_RUN.',
             'Private FP arms copy keeps source geometry and raw hierarchy; count/bounds checks are not a pointwise vertex-buffer audit.'])
PROTECTED = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, good, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if good else 'FAIL', observed=observed)); dump()
    if not good: raise RuntimeError(name+': '+repr(observed))


def path(obj): return obj.get_path_name() if obj is not None and hasattr(obj, 'get_path_name') else None
def package(obj): return (path(obj) if hasattr(obj, 'get_path_name') else str(obj)).split('.')[0]
def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''): h.update(block)
    return h.hexdigest()


def disk(obj):
    p = package(obj)
    assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')


def protect(file, expected=None):
    file = Path(file).resolve(); actual = sha(file)
    check('protected input SHA '+str(file), expected is None or expected.lower() == actual, actual)
    PROTECTED[str(file)] = actual


def report(file, phase, expected=None):
    file = Path(file).resolve(); check('bounded source report', file.is_relative_to(DOC/'editor_runtime'))
    protect(file, expected); data = json.loads(file.read_text('utf-8-sig'))
    command = file.parent/'commandlet.json'; protect(command)
    launch = json.loads(command.read_text('utf-8-sig'))
    check('actual completed native '+phase, data.get('status') == 'PASS'
          and launch.get('status') == 'PASS' and launch.get('phase') == phase and launch.get('exit_code') == 0)
    R['inputs'][phase] = dict(path=str(file), sha256=sha(file), commandlet=str(command), commandlet_sha256=sha(command))
    check('bounded source asset inventory '+phase, 0 < len(data.get('assets', [])) < 80)
    for row in data['assets']: protect(disk(row['path']), row['sha256'])
    return data


def latest(phase):
    files = sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True)
    for file in files:
        if json.loads(file.read_text('utf-8-sig')).get('status') == 'PASS': return report(file, phase)
    raise RuntimeError('No completed native PASS '+phase+'; dependencies must run first')


def load(p, cls=None):
    obj = A.load_asset(package(p)); check('native load '+package(p), obj is not None and (cls is None or isinstance(obj, cls)))
    return obj


def vec(v): return [float(v.x), float(v.y), float(v.z)]
def quat(q): return [float(q.x), float(q.y), float(q.z), float(q.w)]
def transform(t): return dict(translation=vec(t.translation), rotation=quat(t.rotation), scale=vec(t.scale3d))


def value(v):
    if v is None or isinstance(v, (str, bool, int, float)): return v
    if isinstance(v, U.Transform): return transform(v)
    if isinstance(v, U.Vector): return vec(v)
    if isinstance(v, U.Rotator): return [float(v.pitch), float(v.yaw), float(v.roll)]
    if hasattr(v, 'get_path_name'): return v.get_path_name()
    if isinstance(v, (list, tuple, U.Array)): return [value(x) for x in v]
    # Struct __str__ includes addresses and cannot be compared across processes.
    if isinstance(v, U.StructBase): return v.export_text()
    return str(v)


def props(obj, names): return {n: value(obj.get_editor_property(n)) for n in names}


def cdo_objects(bp):
    cls = bp.generated_class(); check('real compiled Hero class', cls is not None)
    hero = U.get_default_object(cls)
    result = [hero, hero.get_editor_property('mesh'), hero.get_component_by_class(U.HCM4CombatComponent),
              hero.get_component_by_class(U.HCM4R2PresentationComponent)]
    check('owned native CDO components', all(x is not None and package(x) == package(bp) for x in result))
    return result


def preserved(hero, body, combat, presentation):
    return dict(character=props(hero, ('walk_speed', 'sprint_speed', 'base_eye_height', 'crouched_eye_height', 'use_controller_rotation_yaw')),
        body=props(body, ('relative_location', 'relative_rotation', 'relative_scale3d', 'physics_asset_override', 'lighting_channels')),
        movement=props(hero.get_component_by_class(U.CharacterMovementComponent), ('max_walk_speed', 'max_acceleration', 'braking_deceleration_walking', 'gravity_scale', 'rotation_rate', 'orient_rotation_to_movement')),
        capsule=props(hero.get_component_by_class(U.CapsuleComponent), ('capsule_radius', 'capsule_half_height')),
        combat=props(combat, ('aim_look_scale', 'ads_magnification', 'aim_transition_seconds', 'magazine_capacity', 'initial_reserve',
            'hip_spread_degrees', 'aim_spread_degrees', 'shot_range', 'body_damage', 'head_damage', 'melee_sphere_radius',
            'attack_movement_scale', 'combo_reset_seconds', 'attack_durations', 'hit_window_starts', 'hit_window_ends',
            'combo_window_starts', 'melee_damage', 'punch_animations', 'pistol_idle_animation', 'pistol_aim_animation',
            'pistol_fire_animation', 'pistol_reload_animation', 'player_hit_front_montage')),
        presentation=props(presentation, ('eye_anchor_in_mesh', 'first_person_skinning_radius_cm', 'head_accessory_bone',
            'head_accessory_local_transform', 'relaxed_hand_camera', 'relaxed_anatomical_wrists', 'relaxed_hand_swing_scale',
            'relaxed_run_hand_camera', 'relaxed_finger_pose', 'unarmed_attack_hand_offset', 'unarmed_attack_lateral_scale')),
        flight=props(hero.get_component_by_class(U.HCM5VS2FlightComponent), ('enable_flight', 'cruise_speed', 'boost_speed',
            'vertical_speed', 'flight_acceleration', 'flight_braking')))


def scs_components(bp):
    # GetObject is deprecated in favour of GetAssociatedObject but remains a pure,
    # reflected getter. GetObjectForBlueprint can create inherited overrides;
    # deliberately do not call that mutating getter on source assets.
    subsystem = U.get_engine_subsystem(U.SubobjectDataSubsystem)
    lib = U.SubobjectDataBlueprintFunctionLibrary
    result = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        check('valid native component handle', lib.is_handle_valid(handle))
        data = lib.get_data(handle); obj = lib.get_object(data)
        if isinstance(obj, U.ActorComponent): result[path(obj)] = obj
    check('bounded native component/template inventory', 0 < len(result) < 80)
    return list(result.values())


def exactly(components, cls, label):
    rows = [x for x in components if isinstance(x, cls)]
    check('exactly one '+label, len(rows) == 1, [path(x) for x in rows]); return rows[0]


def raw_ref(mesh):
    mod = U.SkeletonModifier(); check('read-only native skeleton snapshot', mod.set_skeletal_mesh(mesh))
    names = [n for n in mod.get_all_bone_names() if not str(n).startswith('VB ')]
    return [dict(name=str(n), parent=str(mod.get_parent_name(n)), local=transform(mod.get_bone_transform(n, False))) for n in names]


def shape(mesh):
    sub = U.get_editor_subsystem(U.SkeletalMeshEditorSubsystem)
    lods = sub.get_lod_count(mesh); check('bounded native mesh LOD count', 0 < lods < 12)
    bounds = mesh.get_bounds()
    return dict(lods=[dict(vertices=sub.get_num_verts(mesh, i), sections=sub.get_num_sections(mesh, i)) for i in range(lods)],
        materials=[dict(slot=str(x.get_editor_property('material_slot_name')), material=path(x.get_editor_property('material_interface'))) for x in mesh.get_editor_property('materials')],
        bounds=dict(origin=vec(bounds.origin), extent=vec(bounds.box_extent), sphere_radius=float(bounds.sphere_radius)),
        morphs=[x.get_name() for x in mesh.get_editor_property('morph_targets')])


def duplicate(obj, name):
    dest = ROOT+'/'+name
    check('new candidate package only', not A.does_asset_exist(dest) and not disk(dest).exists(), dest)
    out = A.duplicate_asset(package(obj), dest); check('native duplicate '+name, out is not None); return out


def save(obj):
    p = package(obj); check('save only new integration namespace', p.startswith(ROOT+'/'))
    A.set_metadata_tag(obj, 'HarborCityOwnedBy', OWNER)
    check('native save '+p, A.save_loaded_asset(obj, False) and disk(obj).is_file())
    R['assets'].append(dict(path=p, sha256=sha(disk(obj)), bytes=disk(obj).stat().st_size)); dump()


def native(name, raw):
    data = json.loads(raw); R[name] = data; check('native '+name, data.get('status') == 'PASS', data); return data


def source_set(prior=None):
    result = {}
    for phase in ('HeroSourceOutline', 'GASMotionReload', 'FlightPoseReload', 'FlightVisualReload'):
        ref = prior['inputs'][phase] if prior else None
        result[phase] = report(ref['path'], phase, ref['sha256']) if ref else latest(phase)
    result['HaloParts'] = report(HALO_REPORT, 'HaloParts', HALO_SHA)
    return result


def inspect(bp, expected, baseline_keep, arms_before):
    hero, body, combat, presentation = cdo_objects(bp)
    actual = dict(blueprint=package(bp), mesh=package(body.get_editor_property('skeletal_mesh_asset')),
        animation=package(body.get_editor_property('anim_class')), animation_mode=str(body.get_editor_property('animation_mode')),
        materials=[package(body.get_material(i)) for i in range(body.get_num_materials())],
        combat_animation=package(combat.get_editor_property('player_animation_blueprint')),
        arms=package(presentation.get_editor_property('first_person_arms_override')),
        old_combined_halo=value(presentation.get_editor_property('head_accessory_mesh')),
        preserved=preserved(hero, body, combat, presentation))
    for key in ('mesh', 'animation', 'materials', 'combat_animation', 'arms'):
        check('CDO binding '+key, actual[key] == expected[key], actual[key])
    check('actual animation Blueprint mode', body.get_editor_property('animation_mode') == U.AnimationMode.ANIMATION_BLUEPRINT)
    check('gameplay transforms and presentation tuning unchanged', actual['preserved'] == baseline_keep)
    check('old combined halo explicitly empty', presentation.get_editor_property('head_accessory_mesh') is None, actual['old_combined_halo'])
    check('same 1.25 body scale', max(abs(x-1.25) for x in vec(body.get_editor_property('relative_scale3d'))) < 1e-6)
    arms = load(actual['arms'], U.SkeletalMesh)
    check('FP skeleton matches actual body', arms.get_editor_property('skeleton') == body.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton'))
    check('FP raw ref hierarchy/poses unchanged', raw_ref(arms) == arms_before['raw_ref'])
    check('FP LOD shape/material inventory unchanged', shape(arms) == arms_before['shape'])
    actual['native_arms_skeleton'] = json.loads(U.HCM5VS2HeroIntegrationEditor.bind_private_arms_skeleton(
        load('/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia_Arms', U.SkeletalMesh),
        arms, body.get_editor_property('skeletal_mesh_asset'), False))
    check('native full FP skeleton and three virtual bones readback', actual['native_arms_skeleton'].get('status') == 'PASS', actual['native_arms_skeleton'])
    components = scs_components(bp)
    outline = exactly(components, U.HCM5VS2HeroOutlineComponent, 'source-outline SCS component')
    halo = exactly(components, U.HCM5VS2HaloComponent, 'source-halo SCS component')
    flight = exactly(components, U.HCM5VS2FlightVisualComponent, 'flight-visual SCS component')
    actual['scs'] = dict(outline=props(outline, ('outline_materials',)),
        halo=props(halo, ('pieces', 'source_degrees_per_second', 'glow_material', 'source_head_transform')),
        flight=props(flight, ('feather_mesh', 'ribbon_mesh', 'rune_mesh', 'light_material')))
    check('five outline slots match new body slots', [package(x) for x in outline.get_editor_property('outline_materials')] == expected['outline_materials']
          and len(body.get_editor_property('skeletal_mesh_asset').get_editor_property('materials')) == 5)
    check('seven exact halo meshes and source rates', [package(x) for x in halo.get_editor_property('pieces')] == expected['halo_meshes']
          and list(halo.get_editor_property('source_degrees_per_second')) == expected['halo_rates'])
    check('exact source halo material', package(halo.get_editor_property('glow_material')) == expected['halo_material'])
    check('source halo transform retained', transform(halo.get_editor_property('source_head_transform')) == expected['halo_transform'])
    for key, wanted in expected['flight'].items(): check('flight SCS '+key, package(flight.get_editor_property(key)) == wanted)
    actual['native_cdo_inventory'] = sorted([[x.get_name(), x.get_class().get_path_name()] for x in hero.get_components_by_class(U.ActorComponent)])
    actual['scs_inventory'] = sorted([[x.get_name(), x.get_class().get_path_name()] for x in components])
    actual['outline_attachment_contract'] = 'Runtime BeginPlay reads current Character.Mesh and uses that identical mesh with LeaderPose; actual renderer runtime NOT_RUN.'
    return actual


try:
    check('actual project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no preexisting dirty maps/content', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    for name in ('HCM5VS2HaloEditor', 'HCM5VS2FlightVisualEditor', 'HCM5VS2HeroIntegrationEditor', 'SubobjectDataSubsystem', 'SubobjectDataBlueprintFunctionLibrary', 'SkeletonModifier'):
        check('reflected API '+name, hasattr(U, name))
    prior = latest('HeroRev2Integration') if PHASE.endswith('Reload') else None
    sources = source_set(prior)
    outline, gas, halo, vfx = [sources[x] for x in ('HeroSourceOutline', 'GASMotionReload', 'HaloParts', 'FlightVisualReload')]
    flight_pose = sources['FlightPoseReload']
    check('flight pose retains exact GAS mesh/skeleton/blendspace', all(
        flight_pose['candidate'][key] == gas['candidate'][key] for key in ('mesh', 'skeleton', 'blendspace')))
    source_bp = load(outline['blueprint'], U.Blueprint)
    check('authored source outline metadata', A.get_metadata_tag(source_bp, 'HarborCityOwnedBy') == 'HarborCity_M5_VS2_HeroOutline_R2')
    base_bp = load(BASE, U.Blueprint)
    base_objects, source_objects = cdo_objects(base_bp), cdo_objects(source_bp)
    baseline_keep = preserved(*base_objects)
    check('outline candidate preserved latest native Hero tuning', preserved(*source_objects) == baseline_keep)
    materials = [base_objects[1].get_material(i) for i in range(base_objects[1].get_num_materials())]
    check('latest five material instances', len(materials) == 5 and all(isinstance(m, U.MaterialInstanceConstant) for m in materials))
    check('source outline retains actual latest five MIs', [path(source_objects[1].get_material(i)) for i in range(5)] == [path(m) for m in materials])
    R['effective_materials'] = []
    for slot, mi in zip(SLOTS, materials):
        values = {n: float(E.get_material_instance_scalar_parameter_value(mi, n)) for n in ('HeroLightMax', 'HeroLightMin', 'HeroLightChroma')}
        check('latest .75/.05/.5 lighting preserved '+slot, all(abs(values[n]-v) < 1e-6 for n, v in zip(values, (.75, .05, .5))))
        protect(disk(mi)); R['effective_materials'].append(dict(slot=slot, path=package(mi), parameters=values))
    mesh = load(gas['candidate']['mesh'], U.SkeletalMesh)
    anim = load(flight_pose['candidate']['blueprint'], U.AnimBlueprint)
    skeleton = load(gas['candidate']['skeleton'], U.Skeleton)
    check('new GAS mesh/ABP exact skeleton', mesh.get_editor_property('skeleton') == skeleton and anim.get_editor_property('target_skeleton') == skeleton)
    check('new body ordered material slots unchanged', [str(x.get_editor_property('material_slot_name')).removeprefix('Selestia_') for x in mesh.get_editor_property('materials')] == list(SLOTS))
    check('new GAS raw reference matches baseline', raw_ref(mesh) == raw_ref(base_objects[1].get_editor_property('skeletal_mesh_asset')))
    old_arms = base_objects[3].get_editor_property('first_person_arms_override')
    check('actual original FP arms override available', isinstance(old_arms, U.SkeletalMesh))
    arms_before = dict(raw_ref=raw_ref(old_arms), shape=shape(old_arms)); protect(disk(old_arms))
    check('old FP raw skeleton matches new body', arms_before['raw_ref'] == raw_ref(mesh))
    check('actual seven source halo pieces', len(halo['pieces']) == 7 and len(halo['assets']) == 8)
    pieces = [load(row['mesh'], U.StaticMesh) for row in halo['pieces']]
    rates = [float(row['source_degrees_per_second']) for row in halo['pieces']]
    head = halo['head_transform']; head_transform = U.Transform()
    head_transform.set_editor_property('translation', U.Vector(*head['relative_translation_cm']))
    head_transform.set_editor_property('rotation', U.Quat(*head['relative_rotation_xyzw']))
    head_transform.set_editor_property('scale3d', U.Vector(*head['relative_scale']))
    mod = U.SkeletonModifier(); check('native new body Head read', mod.set_skeletal_mesh(mesh))
    actual_head = transform(mod.get_bone_transform('Head', True))
    reference = dict(translation=head['reference_head_component_translation_cm'], rotation=head['reference_head_component_rotation_xyzw'], scale=head['reference_head_component_scale'])
    qerror = min(max(abs(a-b) for a,b in zip(actual_head['rotation'], reference['rotation'])), max(abs(a+b) for a,b in zip(actual_head['rotation'], reference['rotation'])))
    check('source measured Head attachment remains valid', max(abs(a-b) for a,b in zip(actual_head['translation']+actual_head['scale'], reference['translation']+reference['scale'])) < .002 and qerror < 1e-5, actual_head)
    check('actual four original VFX assets', len(vfx['assets']) == 4 and {x['kind'] for x in vfx['geometry']} == {'Feather', 'Ribbon', 'Rune'})
    geometry = {x['kind']: load(x['path'], U.StaticMesh) for x in vfx['geometry']}
    for folder in ('M5VS1/HeroSelestia', 'M5VS2/HeroSelestia', 'M5VS2/HeroRev2', 'M5VS2/HaloRev2', 'M5VS2/FlightVisual'):
        for file in (PROJECT/'Content/HarborCity'/folder).rglob('*.uasset'): PROTECTED[str(file.resolve())] = sha(file)
    for file in (PROJECT/'Config').glob('*.ini'): PROTECTED[str(file.resolve())] = sha(file)
    for file in (PROJECT/'Content/HarborCity').rglob('*.umap'): PROTECTED[str(file.resolve())] = sha(file)
    source_templates = scs_components(source_bp)
    outline_component = exactly(source_templates, U.HCM5VS2HeroOutlineComponent, 'source outline before duplicate')
    check('source has no previous halo/flight binding', not any(isinstance(x, (U.HCM5VS2HaloComponent, U.HCM5VS2FlightVisualComponent)) for x in source_templates))
    expected = dict(mesh=package(mesh), animation=package(anim), combat_animation=package(anim), materials=[package(m) for m in materials],
        outline_materials=[package(m) for m in outline_component.get_editor_property('outline_materials')],
        halo_meshes=[package(x) for x in pieces], halo_rates=rates, halo_material=package(halo['material']), halo_transform=transform(head_transform),
        flight=dict(feather_mesh=package(geometry['Feather']), ribbon_mesh=package(geometry['Ribbon']), rune_mesh=package(geometry['Rune']), light_material=package(vfx['material'])))
    if prior:
        ROOT = prior['destination']; R['destination'] = ROOT
        check('private saved integration namespace', re.fullmatch(r'/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}', ROOT) is not None and len(prior['assets']) == 2)
        for row in prior['assets']: protect(disk(row['path']), row['sha256'])
        expected['arms'] = prior['expected']['arms']
        check('same frozen input binding plan', expected == prior['expected'] and baseline_keep == prior['baseline_preserved'])
        bp = load(prior['blueprint'], U.Blueprint)
        R['native_readback'] = inspect(bp, expected, baseline_keep, arms_before)
        check('fresh-process CDO/SCS readback exact', R['native_readback'] == prior['native_readback'])
        R['assets'] = prior['assets']; R['fresh_process_disk_readback'] = 'PASS'
    else:
        check('fresh private integration root', not A.does_directory_exist(ROOT))
        arms = duplicate(old_arms, 'SKM_HeroRev2_FirstPersonArms')
        # SetSkeleton exists in C++ but is not reflected to Python in UE 5.8.
        # The project bridge validates the original raw hierarchy, binds the
        # existing GAS skeleton and rebuilds/reads all 250 reference bones.
        native('native_arms_install', U.HCM5VS2HeroIntegrationEditor.bind_private_arms_skeleton(old_arms, arms, mesh, True))
        check('private arms skeleton binding readback', arms.get_editor_property('skeleton') == skeleton)
        expected['arms'] = package(arms)
        bp = duplicate(source_bp, 'BP_HeroRev2_Integrated_'+TOKEN)
        native('halo_install', U.HCM5VS2HaloEditor.configure_halo(bp, pieces, rates, load(halo['material'], U.MaterialInterface), head_transform))
        native('flight_visual_install', U.HCM5VS2FlightVisualEditor.configure_flight_visual(bp, geometry['Feather'], geometry['Ribbon'], geometry['Rune'], load(vfx['material'], U.MaterialInterface)))
        hero, body, combat, presentation = cdo_objects(bp)
        for obj in (hero, body, combat, presentation): obj.modify()
        body.set_skeletal_mesh_asset(mesh)
        body.set_editor_property('animation_mode', U.AnimationMode.ANIMATION_BLUEPRINT)
        body.set_editor_property('anim_class', anim.generated_class())
        body.set_editor_property('override_materials', materials)
        combat.set_editor_property('player_animation_blueprint', anim.generated_class())
        presentation.set_first_person_arms_override(arms)
        check('native final Blueprint compile', B.compile_blueprint(bp))
        R['native_readback'] = inspect(bp, expected, baseline_keep, arms_before)
        save(arms); save(bp)
        check('post-save readback exact', inspect(bp, expected, baseline_keep, arms_before) == R['native_readback'])
        R['fresh_process_disk_readback'] = 'NOT_RUN'
    R.update(blueprint=package(bp), blueprint_class=path(bp.generated_class()), expected=expected,
             baseline_preserved=baseline_keep, arms_source=package(old_arms), arms_source_snapshot=arms_before)
    check('no map became dirty', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['exception'] = traceback.format_exc(); U.log_error(R['exception'])
finally:
    changed = [file for file, digest in PROTECTED.items() if not Path(file).is_file() or sha(file) != digest]
    R['source_preservation'] = dict(files=len(PROTECTED), changed_files=changed)
    if changed: R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS': raise RuntimeError('Hero integration failed; preserve partial candidate and evidence')
