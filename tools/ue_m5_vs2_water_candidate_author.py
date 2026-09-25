"""Author one isolated world-space water candidate; no runtime or screenshots.

Root: ue_m5_vs2_author_run.ps1 -Phase CornerWaterCandidate -ScriptPath this_file.
Writes one saved map + one derivative master/MI + one normal texture, and one six-shot
corner_capture_plan.json per map. Root supplies restricted runtime map support.
Original Corner, Village library, Engine assets, characters and lighting stay intact.
"""
from pathlib import Path
import copy
import datetime as dt
import hashlib
import importlib.util
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK/'HarborCity'
DOC = WORK/'docs/HarborCity_M5_VS2'
SOURCE_MAP = '/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0'
MAP_OWNER = 'HarborCity_M5_VS2_HarborCorner_P0'
OWNER = 'HarborCity_M5_VS2_CornerWaterCandidate'
KEY = 'HarborCityOwnedBy'
VILLAGE = '/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc'
MASTER = VILLAGE + '/materials/master_materials/M_Master_water'
WATER = VILLAGE + '/materials/MI_ENV_water'
NORMAL = VILLAGE + '/textures/T_FX_water_N'
SEA_LABEL = 'HC_M5VS2_Corner_SeaSurface'
NORMAL_BASE_CM = 2500.0
CAUSTIC_BASE_CM = 7000.0
STYLES = ('WaterWorldScale',)
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
LEVEL = U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT = U.get_editor_subsystem(U.EditorActorSubsystem)


def arg(name):
    matches = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(matches) != 1:
        raise RuntimeError('Exactly one ' + name + ' required')
    return matches[0][0] or matches[0][1]


OUT = Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and OUT != DOC and not (OUT/'author_result.json').exists()
assert arg('M5AuthorPhase') == 'CornerWaterCandidate'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/World/StyleReview_' + TOKEN
MAPS = {style: ROOT + '/L_' + style for style in STYLES}
R = dict(status='RUNNING', phase='CornerWaterCandidate', owner=OWNER, source_map=SOURCE_MAP,
         map_owner=MAP_OWNER, namespace=ROOT, maps=MAPS, checks=[], assets=[], inputs={}, material_mapping={},
         original_assets_changed=False, character_materials_changed=False, runtime='NOT_RUN', visual='USER_REVIEW',
         screenshots_created=0, started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         differences=dict(WaterWorldScale='Water only: world-space physical texture density, per-pixel translucent fog, exact water-height scalar, independent mip-enabled normal copy. No geometry/collision/light/style changes.'),
         limitations=[
             'Candidate only, no art or rendered band-removal acceptance.',
             'Physical wave/caustic periods are explicit authored candidates, not recovered source-demo placement or original designer intent.',
             'Multiple justified water fixes form this bounded candidate; six views do not isolate the causal contribution of each individual fix.',
             'Distance-field foam, opacity, colors, surface roughness, normal flattening and original animation constants remain unchanged.',
             'Generated mip setting is read back; GPU residency, sampling stability and appearance require actual runtime.',
             'Cloud, water and foliage animation phases may differ from earlier runs; compare the matching fixed camera/light presets.'])

PROTECTED = {}
MATERIALS = {}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


def check(name, passed, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if passed else 'FAIL', observed=observed))
    dump()
    if not passed:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def bind(path):
    path = Path(path).resolve()
    return dict(path=str(path), sha256=sha(path), bytes=path.stat().st_size)


def package(path):
    return str(path).split('.')[0]


def disk(path):
    path = package(path)
    check('bounded project asset path', path.startswith('/Game/HarborCity/') and '..' not in path, path)
    extension = '.umap' if path == SOURCE_MAP or path in MAPS.values() else '.uasset'
    return PROJECT/'Content'/(path.removeprefix('/Game/') + extension)


def protect(path, expected=None):
    path = Path(path).resolve()
    allowed = (PROJECT/'Content', Path('E:/UE_5.8/Engine/Content'), PROJECT/'Config')
    check('existing protected source scope', path.is_file() and any(path.is_relative_to(p) for p in allowed), str(path))
    digest = sha(path)
    if expected:
        check('protected source equals native evidence', digest == expected, str(path))
    PROTECTED[str(path)] = digest


def latest(pattern, predicate):
    for path in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'), key=lambda p:p.stat().st_mtime, reverse=True):
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        if data.get('status') == 'PASS' and predicate(data):
            return path, data
    raise RuntimeError('No native PASS input: '+pattern)


def load(path):
    obj = A.load_asset(path)
    check('native asset load', obj is not None, path)
    return obj


def value(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, U.LinearColor):
        return [float(v.r), float(v.g), float(v.b), float(v.a)]
    if isinstance(v, U.Color):
        return [int(v.r), int(v.g), int(v.b), int(v.a)]
    if isinstance(v, U.Vector2D):
        return [float(v.x), float(v.y)]
    if isinstance(v, U.Object):
        return v.get_path_name()
    return str(v)


def props(obj, names):
    return {name:value(obj.get_editor_property(name)) for name in names}


def snapshot(material):
    parameters = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        names = getattr(E, 'get_'+kind+'_parameter_names')(material)
        check('bounded material parameter list', len(names) <= 64)
        getter = getattr(E, 'get_material_instance_'+kind+'_parameter_value')
        parameters[kind] = {str(n):value(getter(material,n)) for n in names}
    return dict(parent=material.get_editor_property('parent').get_path_name(), effective_parameters=parameters,
                base_property_overrides=material.get_editor_property('base_property_overrides').export_text())


def save_new(obj):
    path = package(obj.get_path_name())
    check('save only dedicated new style asset', path.startswith(ROOT+'/'), path)
    A.set_metadata_tag(obj, KEY, OWNER)
    check('native style asset save', A.save_loaded_asset(obj, False), path)
    R['assets'].append(dict(path=path, sha256=sha(disk(path))))


def node(material, kind, **properties):
    obj = E.create_material_expression(material, getattr(U,'MaterialExpression'+kind), 0, 0)
    check('native expression created', obj is not None, kind)
    for name, v in properties.items():
        obj.set_editor_property(name,v)
    return obj


def connect(source, output, target, pin):
    names = [str(n) for n in E.get_material_expression_input_names(target)]
    if pin == 'Input' and pin not in names and len(names) == 1:
        pin = names[0]
    check('native material edge', E.connect_material_expressions(source,output,target,pin), dict(node=target.get_name(),pin=pin))


# Material construction helpers appear below before the final execution block.

def scene_signature():
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    rows=[]
    for actor in ACT.get_all_level_actors():
        if not actor.actor_has_tag(U.Name(MAP_OWNER)):
            continue
        transform=actor.get_actor_transform()
        row=dict(label=actor.get_actor_label(),class_path=actor.get_class().get_path_name(),transform=transform.export_text())
        body=actor.get_component_by_class(U.StaticMeshComponent)
        if body:
            row['mesh']=value(body.get_editor_property('static_mesh'))
            row['materials']=[value(body.get_material(i)) for i in range(body.get_num_materials())]
            row['collision']=str(body.get_collision_enabled())
        for kind,fields in (
            (U.DirectionalLightComponent,('intensity','light_color','atmosphere_sun_light','atmosphere_sun_light_index')),
            (U.SkyLightComponent,('intensity','real_time_capture','lower_hemisphere_is_black','source_type')),
            (U.SkyAtmosphereComponent,('rayleigh_scattering_scale','mie_scattering_scale','mie_anisotropy','multi_scattering_factor','sky_luminance_factor')),
            (U.VolumetricCloudComponent,('material','layer_bottom_altitude','layer_height','tracing_max_distance','view_sample_count_scale','reflection_view_sample_count_scale_value'))):
            component=actor.get_component_by_class(kind)
            if component:row['lighting']=props(component,fields)
        if isinstance(actor,U.PostProcessVolume):
            row['post_process']=actor.get_editor_property('settings').export_text()
        rows.append(row)
    check('bounded complete owned Corner actor set', 100 <= len(rows) <= 400 and len({r['label'] for r in rows})==len(rows))
    return dict(game_mode=value(world.get_world_settings().get_editor_property('default_game_mode')),actors=sorted(rows,key=lambda x:x['label']))


def source_scene():
    check('read exact saved source Corner',LEVEL.load_level(SOURCE_MAP))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact source world ownership',package(world.get_path_name())==SOURCE_MAP and A.get_metadata_tag(world,KEY)==MAP_OWNER)
    signature=scene_signature()
    count={path:sum(row.get('materials',[]).count(path) for row in signature['actors']) for path in MATERIALS}
    check('exactly one water slot on the existing sea',count=={WATER+'.MI_ENV_water':1},count)
    sea=[row for row in signature['actors'] if row['label']==SEA_LABEL]
    check('one original sea with exact probed transform',len(sea)==1 and sea[0]['transform']==R['inputs']['sea_actor_transform'])
    R['source_material_slot_counts']=count
    return signature


def build_map(style,original_signature):
    target=MAPS[style]
    check('fresh unique comparison map',not A.does_asset_exist(target) and not disk(target).exists(),target)
    check('native world template lifecycle',LEVEL.new_level_from_template(target,SOURCE_MAP))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact new style world',package(world.get_path_name())==target)
    A.set_metadata_tag(world,KEY,MAP_OWNER)
    A.set_metadata_tag(world,'HarborCityStyleReview',OWNER)
    A.set_metadata_tag(world,'HarborCityStyle',style)
    check('untouched clone equals original full scene signature',scene_signature()==original_signature)
    counts={path:0 for path in MATERIALS}
    if style=='WaterWorldScale':
        for actor in ACT.get_all_level_actors():
            if not actor.actor_has_tag(U.Name(MAP_OWNER)):
                continue
            body=actor.get_component_by_class(U.StaticMeshComponent)
            if not body:
                continue
            if any(value(body.get_material(i)) in MATERIALS for i in range(body.get_num_materials())):
                check('only the actual SeaSurface uses candidate water',actor.get_actor_label()==SEA_LABEL)
            for index in range(body.get_num_materials()):
                current=body.get_material(index)
                path=value(current)
                if path in MATERIALS:
                    body.set_material(index,MATERIALS[path])
                    counts[path]+=1
        check('all intended static actor slots replaced once',counts==R['source_material_slot_counts'],counts)
    expected=copy.deepcopy(original_signature)
    if style=='WaterWorldScale':
        for row in expected['actors']:
            if 'materials' in row:
                row['materials']=[R['material_mapping'].get(p,p) for p in row['materials']]
    check('candidate changes only intended static material overrides',scene_signature()==expected,style)
    check('native save unique style map',LEVEL.save_current_level())
    R['assets'].append(dict(path=target,sha256=sha(disk(target))))
    # Release Python actor/world handles before the editor changes world.
    world=None
    if style=='WaterWorldScale':
        actor=None;body=None;current=None
    check('reload saved style map',LEVEL.load_level(target))
    check('reloaded scene matches exact expected signature',scene_signature()==expected,style)
    R.setdefault('style_map_readbacks',{})[style]=dict(map=target,actor_count=len(expected['actors']),
        signature_sha256=hashlib.sha256(json.dumps(expected,sort_keys=True).encode('utf-8')).hexdigest(),
        material_slot_replacements=counts,lighting_and_geometry_equal_source=True)


def plans(source_file,source_author):
    # Reuse only this already-reviewed project-owned pure-Python plan builder.
    file=WORK/'tools/ue_m5_vs2_corner_review_author.py'
    spec=importlib.util.spec_from_file_location('hc_owned_corner_plan',str(file))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    layout=module.read_document(DOC/'research/HARBOR_CORNER_P0_LAYOUT.json')
    base=module.build_plan(source_file,source_author,layout)
    for style in STYLES:
        plan=copy.deepcopy(base)
        plan['map']=MAPS[style]
        plan['material_style']=style
        plan['material_style_scope']=R['differences'][style]
        plan['material_style_author']=bind(OUT/'author_result.json')
        plan['sources']['source_corner_author']=plan['sources']['corner_author']
        plan['sources']['corner_author']=bind(OUT/'author_result.json')
        plan['sources']['plan_script']=bind(Path(__file__))
        plan['sources']['original_plan_builder']=bind(file)
        packages=[p for p in plan['sources']['map_and_dedicated_packages'] if p['package']!=SOURCE_MAP]
        for asset in R['assets']:
            path=asset['path']
            if path in MAPS.values() and path!=MAPS[style]:continue
            # Avoid report-writing validation helpers after plans bind its final hash.
            extension='.umap' if path in MAPS.values() else '.uasset'
            package_file=PROJECT/'Content'/(path.removeprefix('/Game/')+extension)
            packages.append(dict(package=path,**bind(package_file)))
        if len(packages)>32:raise RuntimeError('Runtime package proof limit exceeded')
        plan['sources']['map_and_dedicated_packages']=packages
        plan['review_notes'].append('Compare matching camera and light preset against the original source Corner. Only the SeaSurface material differs; the scene is not the SoftAnime architecture treatment.')
        plan['review_notes'].append('Cloud, water and foliage phases can differ between runs; retain raw frames and inspect water bands, shoreline depth, detail scale and temporal stability.')
        for shot in plan['shots']:
            shot['material_style']=style
        target=Path(R['capture_plan_paths'][style]);target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():raise RuntimeError('Fresh capture-plan path required')
        target.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def graph_signature(material, reference):
    """Read exactly the native fields/pins used by the successful water probe."""
    by_name = {obj.get_name(): obj for obj in E.get_material_expressions(material)}
    rows = {}
    for ref in reference['nodes']:
        obj = by_name.get(ref['id'])
        check('probed node identity and class preserved', obj is not None and obj.get_class().get_name() == ref['class_name'], ref['id'])
        names = list(E.get_material_expression_input_names(obj))
        links = list(E.get_inputs_for_material_expression(material, obj))
        check('complete native input list', len(names) == len(links) == ref['input_name_count'], ref['id'])
        row = dict(class_name=obj.get_class().get_name(), properties=props(obj, tuple(ref.get('properties', {}))),
                   inputs=[dict(name=str(name), upstream=link.get_name() if link else None,
                                output=str(E.get_input_node_output_name_for_material_expression(obj, link)) if link else None)
                           for name, link in zip(names, links)])
        if 'function' in ref:
            row['function'] = value(obj.get_editor_property('material_function'))
        rows[ref['id']] = row
    outputs = {}
    for name in reference['outputs']:
        prop = getattr(U.MaterialProperty, name)
        upstream = E.get_material_property_input_node(material, prop)
        outputs[name] = dict(node=upstream.get_name() if upstream else None,
                             output=str(E.get_material_property_input_node_output_name(material, prop)) if upstream else None)
    return dict(properties=props(material, tuple(reference['properties'])), nodes=rows, outputs=outputs)


def expected_probe_signature(reference):
    rows = {}
    for ref in reference['nodes']:
        row = dict(class_name=ref['class_name'], properties=ref.get('properties', {}),
                   inputs=[dict(name=edge['name'], upstream=edge['upstream'],
                                output=edge.get('upstream_output_api') if edge['upstream'] else None)
                           for edge in ref['inputs']])
        if 'function' in ref:
            row['function'] = ref['function']
        rows[ref['id']] = row
    return dict(properties=reference['properties'], nodes=rows,
                outputs={name:dict(node=path.rsplit(':', 1)[-1] if path else None,
                                   output=reference['output_source_pins'].get(name) if path else None)
                         for name, path in reference['outputs'].items()})


def graph_check(name, actual, expected):
    # Equality remains exact. Bounded field differences diagnose a rejection
    # without replacing a mismatched native value with the expected probe value.
    differences = []
    def visit(a, b, path):
        if len(differences) >= 64 or a == b:
            return
        if isinstance(a, dict) and isinstance(b, dict):
            for key in sorted(set(a) | set(b)):
                if len(differences) >= 64:
                    break
                if key not in a or key not in b:
                    differences.append(dict(path=path+'.'+key,actual=a.get(key),expected=b.get(key),missing_key=True))
                else:
                    visit(a[key], b[key], path+'.'+key)
        elif isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
            for i, (left, right) in enumerate(zip(a, b)):
                visit(left, right, path+'['+str(i)+']')
        else:
            differences.append(dict(path=path,actual=a,expected=b,actual_type=type(a).__name__,expected_type=type(b).__name__))
    visit(actual, expected, '$')
    check(name, actual == expected, dict(differences=differences,diagnostic_limit=64))


def texture_snapshot(texture, reference):
    return dict(size=[texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
                properties=props(texture, tuple(reference['properties'])))


def fresh_duplicate(source, target, kind):
    check('unique new water asset path', target.startswith(ROOT+'/') and not A.does_asset_exist(target)
          and not disk(target).exists(), target)
    result = A.duplicate_asset(source.get_path_name(), target)
    check('native duplicate exact asset class', isinstance(result, kind), target)
    return result


def build_water(probe):
    source = load(MASTER)
    original = load(WATER)
    reference = probe['graphs'][source.get_path_name()]
    check('exact probed water master', len(E.get_material_expressions(source)) == len(reference['nodes']) == 107
          and source.get_editor_property('blend_mode') == U.BlendMode.BLEND_TRANSLUCENT
          and not source.get_editor_property('compute_fog_per_pixel')
          and source.get_editor_property('use_translucency_vertex_fog'))
    before_graph = graph_signature(source, reference)
    graph_check('all native original water graph nodes/fields/pins equal probe', before_graph, expected_probe_signature(reference))
    before_mi = snapshot(original)
    check('original water instance equals native probe', before_mi == probe['instances'][original.get_path_name()])
    scalar = before_mi['effective_parameters']['scalar']
    check('actual source tiling values match the reviewed physical calculation',
          [scalar[n] for n in ('Tile Normal_1','Tile Normal_2','Caustic Tile_1','Caustic Tile_2')] == [5.0,2.0,7.0,10.0])
    plane = probe['saved_sea']['plane']
    check('exact giant two-triangle source sea and native UV coverage', plane['vertices'] == 4 and plane['triangles'] == 2
          and plane['uv0_status'] == 'READ_NATIVE' and probe['saved_sea']['scale'] == [10000.,10000.,1.]
          and sorted(set(tuple(row['uv0']) for row in plane['uv0_rows'])) == [(0.,0.),(0.,1.),(1.,0.),(1.,1.)])
    water_z = float(probe['saved_sea']['location_cm'][2])
    check('current verified sea height', water_z == -150. and probe['saved_sea']['rotation'] == [0.,0.,0.])

    original_normal = load(NORMAL)
    normal_reference = probe['textures'][original_normal.get_path_name()]
    normal_before = texture_snapshot(original_normal, normal_reference)
    check('exact original no-mip normal input', normal_before == normal_reference
          and original_normal.get_editor_property('mip_gen_settings') == U.TextureMipGenSettings.TMGS_NO_MIPMAPS
          and not original_normal.get_editor_property('srgb'))
    normal = fresh_duplicate(original_normal, ROOT+'/Textures/T_Water_Normal_Mips', U.Texture2D)
    check('normal duplicate preserves source size/settings', texture_snapshot(normal, normal_reference) == normal_before)
    normal.set_editor_property('mip_gen_settings', U.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    normal_after = texture_snapshot(normal, normal_reference)
    normal_expected = copy.deepcopy(normal_before)
    normal_expected['properties']['mip_gen_settings'] = str(U.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    check('normal derivative changes only mip generation setting', normal_after == normal_expected)
    save_new(normal)
    R['normal_texture'] = dict(source=original_normal.get_path_name(), candidate=normal.get_path_name(),
        before=normal_before, after=normal_after, source_pixels_operation='Native duplicate; no source pixels painted, replaced or externally processed.',
        gpu_mip_count='NOT_RUN: generator setting is verified, GPU residency is not queried by this author.')

    master = fresh_duplicate(source, ROOT+'/Materials/M_WaterWorldScale', U.Material)
    graph_check('water master duplicate is graph-identical before edits', graph_signature(master, reference), before_graph)
    by_name = {obj.get_name(): obj for obj in E.get_material_expressions(master)}
    world = by_name['MaterialExpressionWorldPosition_0']
    check('world XY pin exists and source has no vertex displacement',
          'XY' in [str(n) for n in E.get_material_expression_output_names(world)]
          and E.get_material_property_input_node(master,U.MaterialProperty.MP_WORLD_POSITION_OFFSET) is None)
    normal_base = node(master,'ScalarParameter',parameter_name=U.Name('HC_NormalBasePeriodCm'),default_value=NORMAL_BASE_CM)
    caustic_base = node(master,'ScalarParameter',parameter_name=U.Name('HC_CausticBasePeriodCm'),default_value=CAUSTIC_BASE_CM)
    normal_uv = node(master,'Divide')
    caustic_uv = node(master,'Divide')
    connect(world,'XY',normal_uv,'A');connect(normal_base,'',normal_uv,'B')
    connect(world,'XY',caustic_uv,'A');connect(caustic_base,'',caustic_uv,'B')
    edits = [('MaterialExpressionPanner_0', 'Coordinate', normal_uv),
             ('MaterialExpressionPanner_3', 'Coordinate', normal_uv),
             ('MaterialExpressionMultiply_5', 'A', caustic_uv),
             ('MaterialExpressionMultiply_8', 'A', caustic_uv)]
    expected_graph = copy.deepcopy(before_graph)
    for name, pin, replacement in edits:
        connect(replacement,'',by_name[name],pin)
        matches = [edge for edge in expected_graph['nodes'][name]['inputs'] if edge['name'] == pin]
        check('one reviewed existing coordinate edge',len(matches)==1,dict(node=name,pin=pin))
        matches[0].update(upstream=replacement.get_name(),output='')
    master.set_editor_property('compute_fog_per_pixel', True)
    expected_graph['properties']['compute_fog_per_pixel'] = True
    check('exactly four new water coordinate nodes',len(E.get_material_expressions(master)) == 111)
    graph_check('only declared original graph differences',graph_signature(master, reference),expected_graph)
    E.recompile_material(master)
    save_new(master)
    R['master_graph_changes'] = dict(source=source.get_path_name(),candidate=master.get_path_name(),
        original_nodes=107,candidate_nodes=111,coordinate_nodes=[n.get_name() for n in (normal_base,caustic_base,normal_uv,caustic_uv)],
        original_graph_after=graph_signature(master, reference),only_material_property_change=dict(compute_fog_per_pixel=True),
        original_outputs_preserved=True,source_material_functions_changed=False,
        world_coordinate_note='WorldPosition.XY in cm is absolute, and source/candidate WPO is unconnected. No actor scale enters texture density.')

    candidate = fresh_duplicate(original,ROOT+'/Materials/MI_WaterWorldScale',U.MaterialInstanceConstant)
    check('water MI native duplicate preserves effective parameters',snapshot(candidate)==before_mi)
    E.set_material_instance_parent(candidate,master)
    scalar_return=E.set_material_instance_scalar_parameter_value(candidate,U.Name('Water Height'),water_z)
    texture_return=E.set_material_instance_texture_parameter_value(candidate,U.Name('Normal Texture'),normal)
    E.update_material_instance(candidate)
    after_mi=snapshot(candidate)
    expected_mi=copy.deepcopy(before_mi)
    expected_mi['parent']=master.get_path_name()
    expected_mi['effective_parameters']['scalar'].update({'Water Height':water_z,'HC_NormalBasePeriodCm':NORMAL_BASE_CM,
                                                        'HC_CausticBasePeriodCm':CAUSTIC_BASE_CM})
    expected_mi['effective_parameters']['texture']['Normal Texture']=normal.get_path_name()
    check('native MI changes only declared height/coordinate parameters/normal texture and parent',after_mi==expected_mi)
    save_new(candidate)
    MATERIALS[original.get_path_name()]=candidate
    R['material_mapping'][original.get_path_name()]=candidate.get_path_name()
    R['material_readbacks']=dict(before=before_mi,after=after_mi,scalar_setter_return=scalar_return,texture_setter_return=texture_return)
    # Panner adds time*speed. Normal tiling follows the panner and cancels from
    # physical speed; caustic tiling precedes the panner, so divide by its tile.
    caustic_speed_1=props(by_name['MaterialExpressionConstant2Vector_1'],('r','g'))
    caustic_speed_2=props(by_name['MaterialExpressionConstant2Vector_0'],('r','g'))
    R['physical_scale']=dict(units='cm and cm/s',origin_xy_cm=[0,0],
        basis='Explicit art candidate; original source demo water Actor scale was not read. Original runtime Plane UV coverage is measured.',
        old_periods_cm=dict(normal_1=1000000/scalar['Tile Normal_1'],normal_2=1000000/scalar['Tile Normal_2'],
                           caustic_1=1000000/scalar['Caustic Tile_1'],caustic_2=1000000/scalar['Caustic Tile_2']),
        candidate_periods_cm=dict(normal_1=NORMAL_BASE_CM/scalar['Tile Normal_1'],normal_2=NORMAL_BASE_CM/scalar['Tile Normal_2'],
                                 caustic_1=CAUSTIC_BASE_CM/scalar['Caustic Tile_1'],caustic_2=CAUSTIC_BASE_CM/scalar['Caustic Tile_2']),
        candidate_pattern_translation_velocity_cm_s=dict(
            normal_1=[-NORMAL_BASE_CM*scalar['Speed_1_X'],-NORMAL_BASE_CM*scalar['Speed_1_Y']],
            normal_2=[-NORMAL_BASE_CM*scalar['Speed_2_X'],-NORMAL_BASE_CM*scalar['Speed_2_Y']],
            caustic_1=[-CAUSTIC_BASE_CM/scalar['Caustic Tile_1']*caustic_speed_1[n] for n in ('r','g')],
            caustic_2=[-CAUSTIC_BASE_CM/scalar['Caustic Tile_2']*caustic_speed_2[n] for n in ('r','g')]),
        water_height_before_cm=scalar['Water Height'],water_height_after_cm=water_z,
        water_height_graph='The source reconstructs opaque scene world Z and subtracts Water Height. Candidate supplies the actual fixed water plane world Z.',
        foam_scale_uv='Scale UV=7 belongs to distance-field foam angle mapping, not surface normal or caustic tiling; unchanged.')


try:
    check('exact HarborCity project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or prior dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    file,source=latest('author_*_HarborCorner',lambda d:d.get('map')==SOURCE_MAP and d.get('owner')==MAP_OWNER and 'cloud_candidate' in d)
    R['inputs']['source_corner_author']=bind(file)
    for asset in source['assets']:protect(disk(asset['path']),asset['sha256'])
    probe_file,probe=latest('author_*_WaterProbe',lambda d:d.get('phase')=='WaterProbe' and d.get('water_source')==WATER)
    R['inputs']['native_material_probe']=bind(probe_file)
    check('water probe binds this exact source Corner',probe['corner_author']['path']==str(file)
          and probe['corner_author']['sha256']==sha(file))
    R['inputs']['sea_actor_transform']=probe['saved_sea']['component_transform']
    for path,digest in probe['protected_source_sha256'].items():protect(path,digest)
    layout_file=DOC/'research/HARBOR_CORNER_P0_LAYOUT.json'
    check('current layout equals actual Corner author',sha(layout_file)==source['inputs']['layout']['sha256'])
    R['inputs']['layout']=bind(layout_file)
    for name in ('DefaultEngine.ini','DefaultGame.ini','DefaultInput.ini'):
        if (PROJECT/'Config'/name).is_file():protect(PROJECT/'Config'/name)
    build_water(probe)
    signature=source_scene()
    build_map('WaterWorldScale',signature)
    check('exactly one normal texture/master/MI/new map',len(R['assets'])==4)
    check('protected sources remain clean',not any(package(obj.get_path_name())==SOURCE_MAP or package(obj.get_path_name()).startswith(VILLAGE+'/')
          for obj in list(U.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(U.EditorLoadingAndSavingUtils.get_dirty_content_packages())))
    check('all protected original source bytes unchanged',all(Path(p).is_file() and sha(p)==h for p,h in PROTECTED.items()))
    R['protected_source_sha256']=PROTECTED
    R['capture_plan_paths']={'WaterWorldScale':str(OUT/'WaterWorldScale/corner_capture_plan.json')}
    R['runtime_arguments']={'WaterWorldScale':[MAPS['WaterWorldScale'],'-game','-M5VS2CornerReview',
        '-M5VS2CornerPlan="'+R['capture_plan_paths']['WaterWorldScale']+'"','-M5VS2EvidenceDir=<fresh VS2 evidence dir>','-M5VS2AutoQuit']}
    R['pass_scope']='Native independent water material/texture/map author, exact source preservation and save/reload only. Render, mip residency and band removal NOT_RUN.'
    R['status']='PASS';R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
    plans(file,source)
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    R['retry_policy']='Preserve partial packages and FAIL; fresh wrapper evidence gives a new namespace.'
    dump();U.log_error(R['error'])
if R['status']!='PASS':raise RuntimeError('CornerWaterCandidate author failed; preserve evidence and partial unique assets')
