"""Author two isolated Corner material candidates; no runtime or screenshots.

Root: ue_m5_vs2_author_run.ps1 -Phase CornerStyleCompare -ScriptPath this_file.
Writes two saved maps + one derivative master + four MI, and one six-shot
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
OWNER = 'HarborCity_M5_VS2_CornerStyleCompare'
KEY = 'HarborCityOwnedBy'
VILLAGE = '/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc'
MASTER = VILLAGE + '/materials/master_materials/M_Master_opaque_normal'
NAMES = ('MI_wall_02', 'MI_wood_05', 'MI_rooftiles_01', 'MI_stonebrick_02')
STYLES = ('OriginalHandPainted', 'SoftAnime')
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
assert arg('M5AuthorPhase') == 'CornerStyleCompare'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/World/StyleReview_' + TOKEN
MAPS = {style: ROOT + '/L_' + style for style in STYLES}
R = dict(status='RUNNING', phase='CornerStyleCompare', owner=OWNER, source_map=SOURCE_MAP,
         map_owner=MAP_OWNER, namespace=ROOT, maps=MAPS, checks=[], assets=[], inputs={}, material_mapping={},
         original_assets_changed=False, character_materials_changed=False, runtime='NOT_RUN', visual='USER_REVIEW',
         screenshots_created=0, started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         differences=dict(OriginalHandPainted='Exact original environment materials.',
             SoftAnime='Four opaque wall/wood/roof/stone materials: centered base-color contrast 0.72 about linear 0.22; normal Flatness 0.65; specular 0.15. Roughness remains original 1.0; colors/textures and all other material inputs preserved.'),
         limitations=[
             'These are candidate material treatments, not approved anime art or a different lighting preset.',
             'The four material families change; foliage, metal detail, water, road, Hero and NPC materials remain source-identical.',
             'Same geometry, camera, sun, atmosphere, cloud settings and exposure; compare same-preset images across maps.',
             'Cloud and foliage inherit time-based animation. Separate runs do not guarantee identical animated phases.',
             'Original and SoftAnime each retain the six-shot Afternoon/Dusk schema; primary style comparison is the matching three Afternoon views.',
             'Runtime consumer/launcher must permit only these evidence-bound two map names; this script does not edit or run them.'])
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


def build_materials(probe):
    source = load(MASTER)
    observed = probe['graphs'][source.get_path_name()]
    nodes = list(E.get_material_expressions(source))
    check('exact probed opaque eight-node source master', len(nodes) == len(observed['nodes']) == 8
          and source.get_editor_property('material_domain') == U.MaterialDomain.MD_SURFACE
          and source.get_editor_property('shading_model') == U.MaterialShadingModel.MSM_DEFAULT_LIT
          and source.get_editor_property('blend_mode') == U.BlendMode.BLEND_OPAQUE)
    target = ROOT+'/Materials/M_SoftAnime'
    check('fresh derived master path', not A.does_asset_exist(target) and not disk(target).exists())
    master = A.duplicate_asset(source.get_path_name(),target)
    check('native derivative preserves Material class', isinstance(master,U.Material))
    original_color = E.get_material_property_input_node(master,U.MaterialProperty.MP_BASE_COLOR)
    original_normal = E.get_material_property_input_node(master,U.MaterialProperty.MP_NORMAL)
    check('exact original base color and FlattenNormal outputs', original_color.get_name() == 'MaterialExpressionMultiply_0'
          and original_normal.get_name() == 'MaterialExpressionMaterialFunctionCall_0')
    # C' = saturate((C - pivot) * contrast + pivot). This operates on the
    # source texture/tint graph before lighting, not a post-process color grade.
    pivot = node(master,'ScalarParameter',parameter_name=U.Name('HC_ContrastPivot'),default_value=.22)
    contrast = node(master,'ScalarParameter',parameter_name=U.Name('HC_TextureContrast'),default_value=.72)
    sub = node(master,'Subtract'); mult = node(master,'Multiply'); add = node(master,'Add'); clamp = node(master,'Saturate')
    connect(original_color,'',sub,'A');connect(pivot,'',sub,'B')
    connect(sub,'',mult,'A');connect(contrast,'',mult,'B')
    connect(mult,'',add,'A');connect(pivot,'',add,'B');connect(add,'',clamp,'Input')
    specular = node(master,'ScalarParameter',parameter_name=U.Name('HC_Specular'),default_value=.15)
    check('new centered contrast output', E.connect_material_property(clamp,'',U.MaterialProperty.MP_BASE_COLOR))
    check('new explicit lower specular response', E.connect_material_property(specular,'',U.MaterialProperty.MP_SPECULAR))
    check('normal graph identity retained', E.get_material_property_input_node(master,U.MaterialProperty.MP_NORMAL) == original_normal)
    E.recompile_material(master)
    save_new(master)
    R['new_master'] = dict(path=master.get_path_name(), source=source.get_path_name(),
        original_nodes=8, new_nodes=len(E.get_material_expressions(master)),
        formula='saturate((original_texture_times_tint - HC_ContrastPivot) * HC_TextureContrast + HC_ContrastPivot)',
        expected_constants=dict(HC_ContrastPivot=.22,HC_TextureContrast=.72,HC_Specular=.15),
        normal_semantics='Original Normal Power parameter feeds FlattenNormal.Flatness; candidate 0.65 flattens rather than amplifies.')
    for name in NAMES:
        original = load(VILLAGE+'/materials/'+name)
        before = snapshot(original)
        check('source MI equals exact native probe', before == probe['instances'][original.get_path_name()],name)
        target = ROOT+'/Materials/'+name+'_SoftAnime'
        check('fresh derivative MI',not A.does_asset_exist(target) and not disk(target).exists(),target)
        candidate = A.duplicate_asset(original.get_path_name(),target)
        check('exact MI duplication',isinstance(candidate,U.MaterialInstanceConstant) and snapshot(candidate)==before)
        E.set_material_instance_parent(candidate,master)
        E.set_material_instance_scalar_parameter_value(candidate,U.Name('Normal Power'),.65)
        E.update_material_instance(candidate)
        after = snapshot(candidate)
        expected = copy.deepcopy(before)
        expected['parent']=master.get_path_name()
        requested = dict(HC_ContrastPivot=.22,HC_TextureContrast=.72,HC_Specular=.15,**{'Normal Power':.65})
        for key,wanted in requested.items():
            actual = after['effective_parameters']['scalar'].get(key)
            check('actual style scalar value',actual is not None and abs(actual-wanted)<1.e-6,dict(material=name,parameter=key,value=actual))
            expected['effective_parameters']['scalar'][key]=actual
        check('only declared material parameters and parent differ',after==expected,name)
        save_new(candidate)
        MATERIALS[original.get_path_name()] = candidate
        R['material_mapping'][original.get_path_name()] = candidate.get_path_name()
        R.setdefault('material_readbacks',{})[name] = dict(before=before,after=after)


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
    check('all four styles affect actual scene material slots',all(n>0 for n in count.values()),count)
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
    if style=='SoftAnime':
        for actor in ACT.get_all_level_actors():
            if not actor.actor_has_tag(U.Name(MAP_OWNER)):
                continue
            body=actor.get_component_by_class(U.StaticMeshComponent)
            if not body:
                continue
            for index in range(body.get_num_materials()):
                current=body.get_material(index)
                path=value(current)
                if path in MATERIALS:
                    body.set_material(index,MATERIALS[path])
                    counts[path]+=1
        check('all intended static actor slots replaced once',counts==R['source_material_slot_counts'],counts)
    expected=copy.deepcopy(original_signature)
    if style=='SoftAnime':
        for row in expected['actors']:
            if 'materials' in row:
                row['materials']=[R['material_mapping'].get(p,p) for p in row['materials']]
    check('candidate changes only intended static material overrides',scene_signature()==expected,style)
    check('native save unique style map',LEVEL.save_current_level())
    R['assets'].append(dict(path=target,sha256=sha(disk(target))))
    # Release Python actor/world handles before the editor changes world.
    world=None
    if style=='SoftAnime':
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
        plan['review_notes'].append('Compare matching Afternoon camera names across OriginalHandPainted/SoftAnime maps. Both maps have identical source lighting; style changes are native material graph/normal/specular differences, not time-of-day changes.')
        plan['review_notes'].append('Cloud and foliage animation phases may differ between runs; judge architecture material response and retain raw frames.')
        for shot in plan['shots']:
            shot['material_style']=style
        target=Path(R['capture_plan_paths'][style]);target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():raise RuntimeError('Fresh capture-plan path required')
        target.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


try:
    check('exact HarborCity project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or prior dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    file,source=latest('author_*_HarborCorner',lambda d:d.get('map')==SOURCE_MAP and d.get('owner')==MAP_OWNER and 'cloud_candidate' in d)
    R['inputs']['source_corner_author']=bind(file)
    for asset in source['assets']:
        protect(disk(asset['path']),asset['sha256'])
    probe_file,probe=latest('author_*_CloudProbe',lambda d:d.get('cloud_source')=='/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst')
    R['inputs']['native_material_probe']=bind(probe_file)
    for path,digest in probe['protected_source_sha256'].items():
        if str(path).endswith('.umap'):continue
        protect(path,digest)
    layout_file=DOC/'research/HARBOR_CORNER_P0_LAYOUT.json'
    check('current layout equals actual Corner author',sha(layout_file)==source['inputs']['layout']['sha256'])
    R['inputs']['layout']=bind(layout_file)
    for name in ('DefaultEngine.ini','DefaultGame.ini','DefaultInput.ini'):
        if (PROJECT/'Config'/name).is_file():protect(PROJECT/'Config'/name)
    build_materials(probe)
    signature=source_scene()
    for style in STYLES:build_map(style,signature)
    check('five new material assets and exactly two new maps',len(R['assets'])==7)
    check('all protected original source bytes unchanged',all(Path(p).is_file() and sha(p)==h for p,h in PROTECTED.items()))
    R['protected_source_sha256']=PROTECTED
    R['capture_plan_paths']={style:str(OUT/style/'corner_capture_plan.json') for style in STYLES}
    R['runtime_arguments']={style:[MAPS[style],'-game','-M5VS2CornerReview',
        '-M5VS2CornerPlan="'+R['capture_plan_paths'][style]+'"','-M5VS2EvidenceDir=<fresh VS2 evidence dir>','-M5VS2AutoQuit'] for style in STYLES}
    R['pass_scope']='Native two-map and five-material author/save/reload plus source preservation only; no rendering or art approval.'
    R['status']='PASS';R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
    # Plans bind this final report hash; do not mutate the report after success.
    plans(file,source)
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    R['retry_policy']='Preserve partial packages and FAIL; fresh wrapper evidence gives a new namespace.'
    dump();U.log_error(R['error'])
if R['status']!='PASS':raise RuntimeError('CornerStyleCompare author failed; preserve evidence and partial unique assets')
