"""Native author: eight source-bound VS1 versus currently bound VS2 images.

Run Phase HeroBoundReview through the existing serial author wrapper. Writes one
unique map plus two usage-compatible legacy copies, reusing the saved Lookdev
stage, lights and fixed camera shots. No original material/BP write or render.
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
HERO = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia'
MODE = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'
OLD = '/Game/HarborCity/M5VS1/HeroSelestia'
SOURCE_MAP = '/Game/HarborCity/M5VS2/Lookdev/L_SelestiaLookdev'
SOURCE_OWNER = 'HarborCity_M5_VS2_SelestiaLookdev'
OWNER = 'HarborCity_M5_VS2_HeroBoundReview'
KEY = 'HarborCityOwnedBy'
SLOTS = ('hair', 'body', 'face', 'option', 'costume')
PAIR_LABELS = ('Neutral_FaceFront', 'Neutral_HalfFront', 'Afternoon_FaceFront', 'Dusk_FaceFront')
SOURCE_REPORT = DOC / 'editor_runtime/author_20260924_012122_159_278c81f7_LookdevFull/author_result.json'
SOURCE_REPORT_SHA = 'e652012ccaf79927f6dcfa36ea8bed95d6c91344c74732afd9ad5f1880c52750'
USAGE_FAILURE = DOC / 'editor_runtime/20260925_001413_263_ad51afb7_heroboundreview_game/Lookdev_20260925_001429_933655D2/lookdev_results.json'
USAGE_FAILURE_SHA = '7895931cbb0dc9ad32696cb40d2f4bf11db0f1c541fdcec89663a7561e442f1b'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)


def argument(name):
    hits = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(hits) == 1, 'Exactly one ' + name + ' required'
    return hits[0][0] or hits[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC / 'editor_runtime') and not (OUT / 'author_result.json').exists()
assert argument('M5AuthorPhase') == 'HeroBoundReview'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
DEST = '/Game/HarborCity/M5VS2/HeroBoundReview/Review_' + TOKEN
MAP = DEST + '/L_HeroBoundReview_' + TOKEN
R = dict(schema='HarborCity.M5VS2.HeroBoundReviewAuthor.v1', status='RUNNING', phase='HeroBoundReview',
         owner=OWNER, map=MAP, target_directory=DEST, source_map=SOURCE_MAP,
         checks=[], assets=[], inputs={}, materials={}, shot_plan=[], runtime='NOT_RUN',
         visual='USER_REVIEW', default_map_changed=False, blueprint_materials_changed=False,
         screenshots_created=0, started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         limitations=[
             'Eight captures on an isolated unchanged Lookdev stage; this is not the actual harbor environment.',
             'Neutral/Afternoon/Dusk are the existing template directional-light proxies with unchanged sky, exposure and geometry.',
             'No completed interior/night scene, new fill lights, lighting calibration or final art acceptance is claimed.',
             'Four pairs use the same current mesh, animation and pose system. The old five VS1 materials are a material-only baseline, not a replay of the entire old build.',
             'Current_Bound uses actual startup body interfaces without creating or changing their material parameters; Corner actual_materials remains the real environment binding evidence.',
             'LegacyVS1_UsageCompatible retains three original VS1 materials and uses isolated face/option copies with only MorphTargets usage enabled. It is the old appearance on usage-compatible copies, not five unchanged original files.',
             'The two copies preserve the existing material graph, texture references and appearance properties; usage flags add the required deformation shader permutations. Rendered visual equivalence remains a runtime review item.',
             'The legacy group uses transient instances because the existing Director supports one actual-startup variant; author verifies none of its three written vector parameter names exist in these materials.',
             'Face Z is locked once to actual animated eye height by the existing Director; half-body uses the unchanged template fixed camera.',
             'Live animation/blinking continues; sequential pairs have identical camera/light settings but are not identical frozen animation frames.',
             'Native author/save/reload only; root must launch the new map, inspect raw screenshots and retain user review.'])
PROTECTED, TREES = {}, {}
NEW_MATERIALS = []


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    if not ok:
        dump()
        raise RuntimeError(name + ': ' + repr(observed))


def sha(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def bind(file):
    file = Path(file).resolve()
    return dict(path=str(file), sha256=sha(file))


def package(obj):
    return (obj.get_path_name() if isinstance(obj, U.Object) else str(obj)).split('.')[0]


def disk(obj, extension='.uasset'):
    path = package(obj)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return (PROJECT / 'Content' / (path.removeprefix('/Game/') + extension)).resolve()


def protect(file, expected=None):
    file = Path(file).resolve()
    check('protected input exists in project or stage evidence', file.is_file()
          and (file.is_relative_to(PROJECT) or file.is_relative_to(DOC)), str(file))
    actual = sha(file)
    check('protected exact source hash', expected is None or expected == actual, str(file))
    PROTECTED[str(file)] = actual


def tree(folder):
    folder = Path(folder).resolve()
    names = sorted(str(p.resolve()) for p in folder.rglob('*') if p.is_file())
    check('protected source tree exists', folder.is_dir() and bool(names), str(folder))
    TREES[str(folder)] = names
    for name in names:
        protect(name)


def load(path):
    obj = A.load_asset(path)
    check('native asset loaded', obj is not None, path)
    return obj


def generated(path):
    cls = U.load_class(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    check('native generated class loaded', cls is not None, path)
    return cls


def dirty():
    return sorted({p.get_path_name() for fn in (U.EditorLoadingAndSavingUtils.get_dirty_content_packages,
                                               U.EditorLoadingAndSavingUtils.get_dirty_map_packages) for p in fn()})


def native_struct(kind, values):
    obj = kind()
    for key, value in values.items():
        obj.set_editor_property(key, value)
    return obj


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def rot(v):
    return [float(v.pitch), float(v.yaw), float(v.roll)]


def color(v):
    return [float(getattr(v, k)) for k in ('r', 'g', 'b', 'a')]


def stable(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, U.Object):
        return v.get_path_name()
    if isinstance(v, U.StructBase):
        return v.export_text()
    return str(v)


def shot_row(shot):
    get = shot.get_editor_property
    return dict(label=str(get('label')), variant_index=int(get('variant_index')),
                camera_location=vec(get('camera_location')), camera_rotation=rot(get('camera_rotation')),
                use_player_eye_height=bool(get('use_player_eye_height')), light_direction=rot(get('light_direction')),
                light_color=color(get('light_color')), light_intensity=float(get('light_intensity')),
                ambient_color=color(get('ambient_color')))


def variant_row(variant):
    get = variant.get_editor_property
    return dict(label=str(get('label')), use_original_materials=bool(get('use_original_materials')),
                materials=[m.get_path_name() for m in get('materials')])


def stage_row(actors, director_class):
    rows = []
    for actor in actors:
        kind = actor.get_class().get_name()
        if kind in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume') or actor.get_class() == director_class:
            continue
        row = dict(label=actor.get_actor_label(), class_name=kind,
                   transform=actor.get_actor_transform().export_text())
        if isinstance(actor, U.DirectionalLight):
            comp = actor.get_component_by_class(U.DirectionalLightComponent)
            row['light'] = {p: stable(comp.get_editor_property(p)) for p in ('intensity', 'light_color', 'mobility')}
        elif isinstance(actor, U.SkyLight):
            comp = actor.get_component_by_class(U.SkyLightComponent)
            row['sky'] = {p: stable(comp.get_editor_property(p)) for p in ('intensity', 'mobility', 'real_time_capture', 'source_type', 'cubemap')}
        elif isinstance(actor, U.PostProcessVolume):
            row['postprocess_settings'] = actor.get_editor_property('settings').export_text()
        elif isinstance(actor, U.StaticMeshActor):
            comp = actor.get_component_by_class(U.StaticMeshComponent)
            row['mesh'] = package(comp.get_editor_property('static_mesh'))
            row['materials'] = [comp.get_material(i).get_path_name() for i in range(comp.get_num_materials())]
        rows.append(row)
    return sorted(rows, key=lambda r: (r['label'], r['class_name']))


def legacy_appearance(material):
    """Read the exact simple face/option graphs authored by VS1, never edit them."""
    rows = []
    for node in E.get_material_expressions(material):
        kind = node.get_class().get_name()
        fields = {'MaterialExpressionConstant': ('r',),
                  'MaterialExpressionTextureSample': ('texture', 'sampler_type'),
                  'MaterialExpressionAdd': ('const_a', 'const_b'),
                  'MaterialExpressionMultiply': ('const_a', 'const_b')}
        check('existing VS1 face/option expression type fully recognized', kind in fields, kind)
        names = list(E.get_material_expression_input_names(node))
        inputs = list(E.get_inputs_for_material_expression(material, node))
        check('complete legacy graph input enumeration', len(names) == len(inputs), node.get_name())
        rows.append(dict(name=node.get_name(), class_name=kind,
            properties={p: stable(node.get_editor_property(p)) for p in fields[kind]},
            inputs=[dict(name=str(n), source=x.get_name() if x else None,
                         output=stable(E.get_input_node_output_name_for_material_expression(node, x)) if x else None)
                    for n, x in zip(names, inputs)],
            outputs=[str(x) for x in E.get_material_expression_output_names(node)]))
    roots = {}
    for name in ('MP_BASE_COLOR', 'MP_METALLIC', 'MP_SPECULAR', 'MP_ROUGHNESS', 'MP_NORMAL',
                 'MP_EMISSIVE_COLOR', 'MP_OPACITY', 'MP_OPACITY_MASK', 'MP_WORLD_POSITION_OFFSET',
                 'MP_AMBIENT_OCCLUSION', 'MP_REFRACTION', 'MP_MATERIAL_ATTRIBUTES'):
        prop = getattr(U.MaterialProperty, name)
        node = E.get_material_property_input_node(material, prop)
        roots[name] = dict(node=node.get_name() if node else None,
                          output=E.get_material_property_input_node_output_name(material, prop) if node else None)
    return dict(expressions=sorted(rows, key=lambda r: r['name']), public_root_inputs=roots,
                appearance_properties={p: stable(material.get_editor_property(p)) for p in
                    ('blend_mode', 'shading_model', 'two_sided', 'opacity_mask_clip_value', 'translucency_lighting_mode')},
                vector_parameter_names=sorted(str(n) for n in E.get_vector_parameter_names(material)),
                scalar_parameter_names=sorted(str(n) for n in E.get_scalar_parameter_names(material)),
                texture_parameter_names=sorted(str(n) for n in E.get_texture_parameter_names(material)))


def usage_compatible_legacy(originals):
    protect(USAGE_FAILURE, USAGE_FAILURE_SHA)
    evidence = json.loads(USAGE_FAILURE.read_text('utf-8-sig'))
    readiness = evidence['active_material_readiness']
    check('exact prior usage failure with zero accepted images', evidence['status'] == 'FAIL'
          and evidence['user_stop_latched'] is False and evidence['planned_shots'] == 8
          and len(evidence['shots']) == 8 and all(s['status'] == 'NOT_RUN' for s in evidence['shots'])
          and not list(USAGE_FAILURE.parent.glob('*.png')) and readiness['status'] == 'FAIL'
          and readiness['shot_index'] == 0 and readiness['definite_compile_or_usage_failure'] is True)
    rows = readiness['slots']
    missing = [r for r in rows if r['game_thread_reason'] == 'MISSING_REQUIRED_USAGE']
    check('only exact legacy face/option morph usage was missing', len(rows) == 5
          and [r['slot'] for r in missing] == [2, 3]
          and all(not r['compile_errors_bounded'] for r in rows)
          and all(r['used_with_skeletal_mesh'] is True for r in rows)
          and all(r['morph_usage_required_by_active_sections'] is True and r['used_with_morph_targets'] is False for r in missing)
          and all(r['ready'] is True for r in rows if r['slot'] not in (2, 3)))
    result = list(originals)
    R['legacy_usage_compatibility'] = dict(source_failure=bind(USAGE_FAILURE), changed_usage='MorphTargets false -> true on face/option copies only',
        shader_graph_edit_calls=0, appearance_property_write_calls=0, original_asset_writes=0,
        copy_mechanism='Native EditorAssetLibrary.duplicate_asset; no graph reconstruction', copies=[],
        visual_equivalence='NOT_RUN: graph/appearance readback equivalence is checked; actual render follows existing strict readiness guard')
    for index in (2, 3):
        source = originals[index]
        row = rows[index]
        check('observed failing source is exact baseline base material', isinstance(source, U.Material)
              and row['slot'] == index and row['base_material'] == source.get_path_name()
              and source.get_editor_property('used_with_skeletal_mesh') is True
              and source.get_editor_property('used_with_morph_targets') is False)
        appearance = legacy_appearance(source)
        target = DEST + '/LegacyVS1Usage/M_LegacyVS1_' + SLOTS[index]
        check('fresh compatibility copy package only', not A.does_asset_exist(target) and not disk(target).exists())
        copied = A.duplicate_asset(package(source), target)
        check('native legacy base material duplication', isinstance(copied, U.Material))
        check('native copy preserves full observed appearance before usage edit', legacy_appearance(copied) == appearance)
        A.set_metadata_tag(copied, KEY, OWNER)
        copied.set_editor_property('used_with_morph_targets', True)
        E.recompile_material(copied)
        check('only required morph usage enabled; skeletal flag retained', copied.get_editor_property('used_with_morph_targets') is True
              and copied.get_editor_property('used_with_skeletal_mesh') is True)
        check('usage edit leaves observed graph and appearance exactly unchanged', legacy_appearance(copied) == appearance)
        check('native save isolated compatible material only', A.save_loaded_asset(copied, False))
        result[index] = copied
        NEW_MATERIALS.append(dict(path=target, source=source.get_path_name(), source_sha256=sha(disk(source)),
                                  sha256=sha(disk(target)), bytes=disk(target).stat().st_size,
                                  usage_before=dict(skeletal_mesh=True, morph_targets=False),
                                  usage_after=dict(skeletal_mesh=True, morph_targets=True), appearance_readback=appearance))
        R['legacy_usage_compatibility']['copies'].append(dict(slot=index, slot_name=SLOTS[index], **NEW_MATERIALS[-1]))
    check('exactly two compatibility copies, other baseline interfaces retained', len(NEW_MATERIALS) == 2
          and all(result[i] == originals[i] for i in (0, 1, 4)))
    return result


def binding_proof():
    # Only the exact strict recovery chain is currently authorized. Its original
    # Apply stays FAIL; this source never relabels or edits that old record.
    candidates = sorted((DOC / 'editor_runtime').glob('author_*_HeroChromaBindingRecoveryVerify/hero_chroma_binding_manifest.json'),
                        key=lambda p: p.stat().st_mtime, reverse=True)
    for file in candidates:
        manifest = json.loads(file.read_text('utf-8-sig'))
        if manifest.get('status') == 'PASS' and manifest.get('fresh_process_disk_readback') == 'PASS':
            break
    else:
        raise RuntimeError('Fresh-process HeroChromaBindingRecoveryVerify PASS required before bound review authoring')
    report_file = Path(manifest['evidence']['path']).resolve()
    check('recovery report is exact adjacent evidence', report_file == file.parent / 'author_result.json')
    protect(file)
    protect(report_file, manifest['evidence']['sha256'])
    report = json.loads(report_file.read_text('utf-8-sig'))
    command_file = file.parent / 'commandlet.json'
    protect(command_file)
    command = json.loads(command_file.read_text('utf-8-sig'))
    check('actual read-only recovery completed normally', command['status'] == 'PASS' and command['exit_code'] == 0
          and report['status'] == 'PASS' and report['phase'] == 'HeroChromaBindingRecoveryVerify'
          and manifest['phase'] == report['phase'] and report['fresh_process_disk_readback'] == 'PASS'
          and report['transitions'] == manifest['transitions'] and len(report['transitions']) == 1
          and report['preservation']['unexpected_changed_files'] == [] and report['preservation']['tree_membership_changes'] == [])
    recovery = manifest['recovery']
    check('original failed Apply status retained exactly', recovery == report['recovery']
          and recovery['outcome'] == 'RECOVERED_AFTER_ORDER_ONLY_GUARD_FAILURE'
          and recovery['original_apply_status'] == 'FAIL' and recovery['asset_writes'] == 0
          and recovery['original_native_checks_passed'] == 725 and recovery['original_protected_files_verified'] == 279)
    old = recovery['original_apply_evidence']
    protect(old['path'], old['sha256'])
    transition = manifest['transitions'][0]
    check('exact current Hero transition and five slots', transition['path'] == HERO
          and [x['slot_name'] for x in transition['selected_materials']] == list(SLOTS))
    protect(disk(HERO), transition['after_sha256'])
    for material in transition['selected_materials']:
        check('actual selected Chroma050 namespace', material['path'].startswith('/Game/HarborCity/M5VS2/HeroLightChroma/Review_c65c8f5c593f/Materials/Chroma_050/'))
        protect(disk(material['path']), material['sha256'])
    R['inputs']['binding_manifest'] = bind(file)
    R['inputs']['binding_recovery_report'] = bind(report_file)
    R['inputs']['original_apply_status'] = 'FAIL_PRESERVED_RECOVERED_BY_SEPARATE_READ_ONLY_VERIFY'
    return transition


def main():
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    check('no initial dirty packages or PIE', not dirty() and not U.EditorLevelLibrary.get_pie_worlds(False))
    check('fresh unique review map', not A.does_asset_exist(MAP) and not disk(MAP, '.umap').exists())
    transition = binding_proof()
    protect(SOURCE_REPORT, SOURCE_REPORT_SHA)
    source_report = json.loads(SOURCE_REPORT.read_text('utf-8-sig'))
    check('exact successful full source stage author', source_report['status'] == 'PASS' and source_report['map'] == SOURCE_MAP)
    for item in source_report['assets']:
        protect(disk(item['path'], '.umap' if item['path'] == SOURCE_MAP else '.uasset'), item['sha256'])
    for folder in ('Content/HarborCity/M5VS1', 'Content/HarborCity/M5VS2/HeroSelestia',
                   'Content/HarborCity/M5VS2/HeroLightChroma', 'Config'):
        tree(PROJECT / folder)
    R['inputs']['lookdev_author'] = bind(SOURCE_REPORT)
    R['inputs']['script'] = bind(Path(__file__))
    hero, mode = generated(HERO), generated(MODE)
    check('exact VS2 GameMode pawn', U.get_default_object(mode).get_editor_property('default_pawn_class') == hero)
    cdo = U.get_default_object(hero)
    body = cdo.get_component_by_class(U.SkeletalMeshComponent)
    check('current Hero body and no candidate pointlights', body is not None and not cdo.get_components_by_class(U.PointLightComponent))
    mesh = body.get_editor_property('skeletal_mesh_asset')
    slots = [str(s.get_editor_property('material_slot_name')).removeprefix('Selestia_') for s in mesh.get_editor_property('materials')]
    check('actual ordered Selestia five slots', slots == list(SLOTS) and body.get_num_materials() == 5)
    current = [body.get_material(i) for i in range(5)]
    current_paths = [m.get_path_name() for m in current]
    check('actual BP effective current materials equal recovered serialized result', current_paths == transition['after_native_cdo']['materials'])
    baseline_originals = [load(OLD + '/Materials/M_Selestia_' + slot) for slot in slots]
    baseline = usage_compatible_legacy(baseline_originals)
    ignored_names = {'SunDirection', 'LightColor', 'AmbientColor'}
    for material in baseline:
        check('old baseline has no comparison-director writable vector parameter',
              not (set(str(n) for n in E.get_vector_parameter_names(material)) & ignored_names), material.get_path_name())
    for label, mats in (('LegacyVS1_UsageCompatible', baseline), ('Current_Bound', current)):
        R['materials'][label] = [dict(slot=i, slot_name=s, path=package(m), sha256=sha(disk(m))) for i, (s, m) in enumerate(zip(slots, mats))]
    configured = [native_struct(U.HCM5VS2MaterialVariant, dict(label='LegacyVS1_UsageCompatible', use_original_materials=False, materials=baseline)),
                  native_struct(U.HCM5VS2MaterialVariant, dict(label='Current_Bound', use_original_materials=True, materials=current))]
    R['hero_binding'] = dict(blueprint=HERO, game_mode=MODE, mesh=mesh.get_path_name(),
                            animation=body.get_editor_property('anim_class').get_path_name(),
                            actual_cdo_materials=current_paths, saved_blueprint_sha256=sha(disk(HERO)))
    source = load(SOURCE_MAP)
    check('original stage owned', A.get_metadata_tag(source, KEY) == SOURCE_OWNER)
    native = U.load_class(None, '/Script/HarborCity.HCM5VS2LookdevDirector')
    check('existing native Lookdev Director available', native is not None)
    check('native independent map from unchanged source template', LEVEL.new_level_from_template(MAP, SOURCE_MAP))
    world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact fresh map', package(world) == MAP)
    A.set_metadata_tag(world, KEY, OWNER)
    actors = ACT.get_all_level_actors()
    allowed = {'StaticMeshActor', 'DirectionalLight', 'SkyLight', 'SkyAtmosphere', 'PostProcessVolume', 'PlayerStart', 'HCM5VS2LookdevDirector'}
    for actor in actors:
        kind = actor.get_class().get_name()
        if kind in ('WorldSettings', 'Brush', 'DefaultPhysicsVolume'):
            continue
        check('only inherited owned stage actors, no new point/spot fill light', kind in allowed and actor.actor_has_tag(U.Name(SOURCE_OWNER)), actor.get_actor_label())
        actor.set_editor_property('tags', [U.Name(OWNER)])
    directors = [a for a in actors if a.get_class() == native]
    starts = [a for a in actors if isinstance(a, U.PlayerStart)]
    check('one inherited Director and PlayerStart', len(directors) == len(starts) == 1)
    stage_before = stage_row(actors, native)
    director = directors[0]
    variants_before = [variant_row(v) for v in director.get_editor_property('material_variants')]
    original_indices = [i for i, v in enumerate(variants_before) if v['label'] == 'VS1_DefaultLit']
    check('one native old VS1 template group', len(original_indices) == 1)
    source_index = original_indices[0]
    check('source template old group uses startup originals', variants_before[source_index]['use_original_materials'])
    source_shots = list(director.get_editor_property('shots'))
    shots = []
    for label in PAIR_LABELS:
        candidates = [s for s in source_shots if str(s.get_editor_property('label')) == label
                      and int(s.get_editor_property('variant_index')) == source_index]
        check('one exact inherited shot ' + label, len(candidates) == 1)
        original = candidates[0]
        for variant_index in (0, 1):
            values = {key: original.get_editor_property(key) for key in ('camera_location', 'camera_rotation', 'use_player_eye_height',
                      'light_direction', 'light_color', 'light_intensity', 'ambient_color')}
            values.update(label=label, variant_index=variant_index)
            shots.append(native_struct(U.HCM5VS2LookdevShot, values))
    rows = [shot_row(s) for s in shots]
    check('eight exact same-camera same-light pairs', len(rows) == 8 and all(
        {k: v for k, v in rows[i].items() if k != 'variant_index'} ==
        {k: v for k, v in rows[i+1].items() if k != 'variant_index'} for i in range(0, 8, 2)))
    light = director.get_editor_property('directional_light')
    check('inherited movable light is in new map', light is not None and light.get_path_name().startswith(MAP + '.')
          and light.get_component_by_class(U.DirectionalLightComponent).get_editor_property('mobility') == U.ComponentMobility.MOVABLE)
    world.get_world_settings().set_editor_property('default_game_mode', mode)
    director.set_actor_label('HC_M5VS2_HeroBoundReview_Director')
    director.set_editor_property('material_variants', configured)
    director.set_editor_property('shots', shots)
    director.set_editor_property('use_player_eye_height', True)
    director.set_editor_property('hide_hud', True)
    variant_rows = [variant_row(v) for v in configured]
    R['shot_plan'] = [dict(index=i, **s, variant=variant_rows[s['variant_index']]['label'], horizontal_fov=40.,
        expected_png='%03d_%s_%s.png' % (i, s['label'], variant_rows[s['variant_index']]['label'])) for i, s in enumerate(rows)]
    R['stage'] = dict(inherited_actor_readback=stage_before, light_proxy_scope='Original saved template; actual harbor lighting NOT_RUN',
                      changed_stage_lights=0, added_actor_count=0, inherited_main_light=light.get_path_name())
    check('only new map dirty before save', set(dirty()).issubset({MAP}), dirty())
    check('native save new comparison map only', LEVEL.save_current_level())
    check('native reload new comparison map', LEVEL.load_level(MAP))
    saved_world = U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    saved_actors = ACT.get_all_level_actors()
    saved = [a for a in saved_actors if a.get_class() == native]
    check('saved map identity and unchanged stage light/geometry/exposure', package(saved_world) == MAP
          and A.get_metadata_tag(saved_world, KEY) == OWNER and stage_row(saved_actors, native) == stage_before
          and saved_world.get_world_settings().get_editor_property('default_game_mode') == mode and len(saved) == 1)
    check('saved eight shots read back exactly', [shot_row(s) for s in saved[0].get_editor_property('shots')] == rows)
    check('saved two material group references and semantics exact', [variant_row(v) for v in saved[0].get_editor_property('material_variants')] == variant_rows)
    for item in NEW_MATERIALS:
        material = load(item['path'])
        check('saved compatibility flags and unchanged appearance read back', A.get_metadata_tag(material, KEY) == OWNER
              and material.get_editor_property('used_with_skeletal_mesh') is True
              and material.get_editor_property('used_with_morph_targets') is True
              and legacy_appearance(material) == item['appearance_readback'] and sha(disk(item['path'])) == item['sha256'])
    check('source Hero CDO interfaces unchanged', [body.get_material(i).get_path_name() for i in range(5)] == current_paths)
    check('no dirty package remains', not dirty(), dirty())
    R['assets'] = [dict(path=x['path'], sha256=x['sha256'], bytes=x['bytes']) for x in NEW_MATERIALS]
    R['assets'].append(dict(path=MAP, sha256=sha(disk(MAP, '.umap')), bytes=disk(MAP, '.umap').stat().st_size))
    R['runtime_arguments'] = [MAP, '-game', '-M5VS2Lookdev', '-M5VS2EvidenceDir=<fresh absolute VS2 docs directory>', '-M5VS2AutoQuit', '-ResX=1920', '-ResY=1080']
    R['pass_scope'] = 'Native save/reload of one isolated map and two legacy usage-compatible material copies, eight inherited shots and explicit legacy/current groups. Original files byte-preserved; render/visual equivalence NOT_RUN.'
    R['status'] = 'PASS'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    R['partial_map_retained'] = MAP if disk(MAP, '.umap').is_file() else None
    R['retry_policy'] = 'Keep evidence and unique partial map; use a fresh wrapper run for any corrected attempt.'
    U.log_error(R['error'])
finally:
    changed = [p for p, h in PROTECTED.items() if not Path(p).is_file() or sha(p) != h]
    trees_changed = []
    for folder, before in TREES.items():
        after = sorted(str(p.resolve()) for p in Path(folder).rglob('*') if p.is_file())
        if before != after:
            trees_changed.append(dict(folder=folder, added=sorted(set(after)-set(before)), removed=sorted(set(before)-set(after))))
    R['preservation'] = dict(files_checked=len(PROTECTED), unexpected_changed_files=changed, tree_membership_changes=trees_changed)
    R['protected_source_sha256_before'] = PROTECTED
    if changed or trees_changed:
        R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('Hero bound review author failed; preserve its evidence and unique map; never reuse old screenshots as this result.')
