"""Private same-pose hair-outline on/off/source-alpha diagnostic author.

Phase HairOutlineReviewAuthor. Requires a completed HeroRev2IntegrationReload; never
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
OWNER = 'HarborCity_M5VS2_HairOutlineReview'
MODE = '/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'


def arg(name):
    rows = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(rows) == 1, 'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT = Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
assert arg('M5AuthorPhase') == 'HairOutlineReviewAuthor'
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT = '/Game/HarborCity/M5VS2/HairReview/Run_'+TOKEN
MAP = ROOT+'/L_HairReview'
R = dict(schema='HarborCity.M5VS2.HairOutlineReview.Author.v1', status='RUNNING', map=MAP,
         phase='HairOutlineReviewAuthor', owner=OWNER, assets=[], checks=[], actors=[], runtime='NOT_RUN',
         visual_acceptance='USER_REVIEW', scope='Same evaluated frozen pose: main hair/cape unchanged; three private hair-outline graph variants, front/rear. Not gameplay input or art acceptance.',
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


def author_outline_variants(body, prior):
    E = U.MaterialEditingLibrary
    main = body.get_material(0)
    source = load(prior['expected']['outline_materials'][0], U.Material)
    for expression in E.get_material_expressions(source):
        if isinstance(expression,U.MaterialExpressionTextureSample):
            referenced=expression.get_editor_property('texture')
            if referenced is not None and package(referenced).startswith('/Game/HarborCity/'): protect(disk(referenced))
    raw = WORK/'assets/M5_VS1/hero/Selestia/UnityTextures_v1_02/Selestia_hair_alpha.png'
    protect(raw, '615ab574c65504e17f0e416a63d9fe22391c587c52bc35e1cd824945355b92e1')
    audit_path = DOC/'research/SELESTIA_SOURCE_AUDIT.json'; protect(audit_path)
    audit = json.loads(audit_path.read_text('utf-8'))
    authored = next(row for row in audit['materials'] if row['asset_path'].endswith('/lilToon/Selestia_hair.mat'))
    tex_info = authored['textures']['_MainTex']
    check('exact original main alpha texture UV identity', tex_info['reference'].endswith('/Selestia_hair_alpha.png')
          and tex_info['scale'] == {'x': 1, 'y': 1} and tex_info['offset'] == {'x': 0, 'y': 0}
          and authored['vectors_colors']['_Color']['a'] == 1 and authored['floats']['_AlphaMaskMode'] == 0, tex_info)
    texture = E.get_material_instance_texture_parameter_value(main, 'gltf_tex_diffuse')
    check('actual inherited main texture is the source RGBA hair texture', texture is not None
          and package(texture) == '/Game/HarborCity/M5VS2/HeroSelestia/Textures/T_Selestia_hair_alpha',
          package(texture) if texture else None)
    protect(disk(texture))
    alpha_switch = E.get_material_instance_static_switch_parameter_value(main, 'bUseAlphaTexture')
    main_color = E.get_material_instance_vector_parameter_value(main, 'mtoon_Color')
    check('actual main shader selects main texture alpha with unit color alpha', not alpha_switch and abs(main_color.a-1)<1e-6)
    uv = {key:E.get_material_instance_scalar_parameter_value(main,key) for key in
          ('mtoon_UvAnimScrollY','mtoon_UvAnimScrollX','mtoon_UvAnimRotation','UVOffset_U','UVOffset_V')}
    check('actual main UV has no scrolling rotation or offset',all(abs(value)<1e-7 for value in uv.values()),uv)
    check('main texture is not mixed with diffuse2',not E.get_material_instance_static_switch_parameter_value(main,'bUseDiffuse2'))
    ancestry = []; current = main
    while isinstance(current, U.MaterialInstanceConstant):
        check('bounded acyclic actual material ancestry', len(ancestry)<12 and package(current) not in [x['material'] for x in ancestry])
        if package(current).startswith('/Game/HarborCity/'): protect(disk(current))
        overrides = current.get_editor_property('base_property_overrides')
        ancestry.append(dict(material=package(current), parent=package(current.get_editor_property('parent')),
                             overrides={k:str(overrides.get_editor_property(k)) for k in
                                        ('override_blend_mode','blend_mode','override_opacity_mask_clip_value','opacity_mask_clip_value','override_two_sided','two_sided')}))
        current = current.get_editor_property('parent')
    check('actual main material root exists', isinstance(current, U.Material))
    # Follow this current parent's native functions, not only a historical plugin graph dump.
    pending=[current]; visited=set(); alpha_graphs=[]
    while pending:
        graph=pending.pop(); path=graph.get_path_name()
        if path in visited: continue
        visited.add(path); check('bounded native function inspection',len(visited)<=96)
        if package(graph).startswith('/Game/HarborCity/'): protect(disk(graph))
        nodes=list(E.get_material_expressions(graph) if isinstance(graph,U.Material) else E.get_material_function_expressions(graph))
        for n in nodes:
            if isinstance(n,U.MaterialExpressionTextureSample):
                referenced=n.get_editor_property('texture')
                if referenced is not None and package(referenced).startswith('/Game/HarborCity/'): protect(disk(referenced))
            if isinstance(n,U.MaterialExpressionMaterialFunctionCall):
                function=n.get_editor_property('material_function')
                if isinstance(function,U.MaterialFunction): pending.append(function)
            if isinstance(n,U.MaterialExpressionStaticSwitchParameter) and str(n.get_editor_property('parameter_name'))=='bUseAlphaTexture':
                check('alpha switch is in inspectable native function',isinstance(graph,U.MaterialFunction))
                links=list(E.get_inputs_for_material_function_expression(graph,n)); mask=links[1] if len(links)>1 else None
                check('actual false branch is a component mask',isinstance(mask,U.MaterialExpressionComponentMask))
                channels={key:bool(mask.get_editor_property(key)) for key in ('r','g','b','a')}
                check('actual selected channel is Alpha only',channels==dict(r=False,g=False,b=False,a=True),channels)
                inputs=list(E.get_inputs_for_material_function_expression(graph,mask)); selector=inputs[0] if inputs else None
                check('selected alpha comes from actual diffuse selector',isinstance(selector,U.MaterialExpressionStaticSwitchParameter)
                      and str(selector.get_editor_property('parameter_name'))=='bUseDiffuse2')
                inputs=list(E.get_inputs_for_material_function_expression(graph,selector)); sample=inputs[1] if len(inputs)>1 else None
                check('actual diffuse false branch reads main texture',isinstance(sample,U.MaterialExpressionTextureSampleParameter2D)
                      and str(sample.get_editor_property('parameter_name'))=='gltf_tex_diffuse')
                alpha_graphs.append(dict(graph=path,switch=n.get_name(),false_node=mask.get_name(),channels=channels,
                                         diffuse_selector=selector.get_name(),diffuse_texture_node=sample.get_name()))
    check('one actual native main-alpha function branch',len(alpha_graphs)==1,alpha_graphs)
    fields = ('blend_mode','two_sided','opacity_mask_clip_value','disable_depth_test','allow_translucent_custom_depth_writes',
              'used_with_skeletal_mesh','used_with_morph_targets','shading_model')
    source_properties = {key:str(source.get_editor_property(key)) for key in fields}
    check('original outline remains masked skeletal/morph material', source.get_editor_property('blend_mode') == U.BlendMode.BLEND_MASKED
          and source.get_editor_property('used_with_skeletal_mesh') and source.get_editor_property('used_with_morph_targets'))
    clip = float(source.get_editor_property('opacity_mask_clip_value'))
    # Local Pillow read on the exact SHA above: RGBA4096; this is source-pixel evidence, not a claim about compressed GPU mips.
    R['alpha_source'] = dict(raw_path=str(raw), raw_sha256=sha(raw), raw_rgba_extrema=[[144,255],[60,255],[96,255],[154,255]],
        raw_resolution=[4096,4096], source_pixel_measurement='Local Pillow original-file channel extrema, 2026-09-25; exact SHA guarded; not GPU/mip readback.',
        sampled_channel='TextureSample A (linear alpha; Color sampler sRGB transfer affects RGB, not alpha)',
        original_alpha_mask_mode=authored['floats']['_AlphaMaskMode'], original_color_alpha=authored['vectors_colors']['_Color']['a'],
        original_source_cutoff=authored['floats']['_Cutoff'], original_prepass_cutoff=authored['floats']['_PreCutoff'],
        actual_texture=package(texture), actual_bUseAlphaTexture=alpha_switch, actual_main_color_alpha=main_color.a,
        actual_texture_properties={key:str(texture.get_editor_property(key)) for key in ('srgb','compression_settings','do_scale_mips_for_alpha_coverage','alpha_coverage_thresholds')},
        main_material_ancestry=ancestry, main_root=package(current),actual_main_uv_scalars=uv,actual_native_alpha_graph=alpha_graphs,
        main_alpha_graph_source='Current parent native function traversal: gltf_tex_diffuse selected by bUseDiffuse2=false -> component A selected by bUseAlphaTexture=false; exact node readback above.',
        main_graph_evidence=str(DOC/'editor_runtime/author_20260923_225027_761_6461f724_MToonGraphProbe/mtoon_graph_probe.json'),
        unchanged_outline_clip=clip, predicted_source_texels_below_clip=0 if 154/255>clip else 'UNDETERMINED',
        interpretation='All source alpha texels exceed .5; no forced higher threshold. No visible alpha-mask difference is a valid result, not a reason to delete strands.')
    R['outline_comparison'] = dict(source=package(source), source_properties=source_properties,
        source_width_world_cm=E.get_material_default_scalar_parameter_value(source,'SourceOutlineWidthCm'),
        width_units='Existing source outline parameter already converts to world centimetres at body scale1.25; WPO and width-mask graph preserved verbatim.',
        variants=[], pose='One settled actual body pose frozen by diagnostic; six PNGs front/rear, not mouse input.')
    materials = []
    for label in ('Original','Off','SourceAlpha'):
        material = A.duplicate_asset(package(source), ROOT+'/M_HairOutline'+label)
        check('private outline native graph copy '+label, isinstance(material,U.Material))
        original_mask = E.get_material_property_input_node(material,U.MaterialProperty.MP_OPACITY_MASK)
        original_out = E.get_material_property_input_node_output_name(material,U.MaterialProperty.MP_OPACITY_MASK)
        wpo = E.get_material_property_input_node(material,U.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        check('copied source backface mask and width graph '+label, original_mask is not None and wpo is not None
              and original_mask.get_name() == E.get_material_property_input_node(source,U.MaterialProperty.MP_OPACITY_MASK).get_name()
              and wpo.get_name() == E.get_material_property_input_node(source,U.MaterialProperty.MP_WORLD_POSITION_OFFSET).get_name())
        before_nodes = list(E.get_material_expressions(material))
        if label == 'Off':
            output = E.create_material_expression(material,U.MaterialExpressionConstant,0,0); output.set_editor_property('r',0.)
            check('off affects only private hair outline opacity',E.connect_material_property(output,'',U.MaterialProperty.MP_OPACITY_MASK))
        elif label == 'SourceAlpha':
            sample = E.create_material_expression(material,U.MaterialExpressionTextureSample,0,0)
            sample.set_editor_property('texture',texture); sample.set_editor_property('sampler_type',U.MaterialSamplerType.SAMPLERTYPE_COLOR)
            check('real alpha output pin exists','A' in [str(x) for x in E.get_material_expression_output_names(sample)])
            output = E.create_material_expression(material,U.MaterialExpressionMultiply,0,0)
            check('preserve original backface mask',E.connect_material_expressions(original_mask,original_out,output,'A'))
            check('only actual original texture alpha enters mask',E.connect_material_expressions(sample,'A',output,'B'))
            check('alpha mask native output',E.connect_material_property(output,'',U.MaterialProperty.MP_OPACITY_MASK))
            check('native multiply reads exact intended two nodes',list(E.get_inputs_for_material_expression(material,output)) == [original_mask,sample])
        else: output = original_mask
        check('all appearance/render properties unchanged '+label,{key:str(material.get_editor_property(key)) for key in fields} == source_properties)
        check('exact source width unchanged '+label,abs(E.get_material_default_scalar_parameter_value(material,'SourceOutlineWidthCm')-
              R['outline_comparison']['source_width_world_cm'])<1e-9)
        check('native WPO graph preserved '+label,E.get_material_property_input_node(material,U.MaterialProperty.MP_WORLD_POSITION_OFFSET) == wpo)
        check('only bounded mask nodes added '+label,len(E.get_material_expressions(material))-len(before_nodes) == {'Original':0,'Off':1,'SourceAlpha':2}[label])
        E.recompile_material(material); save(material)
        check('native persisted mask output '+label,E.get_material_property_input_node(A.load_asset(package(material)),U.MaterialProperty.MP_OPACITY_MASK).get_name() == output.get_name())
        R['outline_comparison']['variants'].append(dict(label=label,material=package(material),opacity_node=output.get_name(),
            unchanged_clip=clip,wpo_node=wpo.get_name(),properties={key:str(material.get_editor_property(key)) for key in fields}))
        materials.append(material)
    return materials


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
    for path in [MODE, prior['expected']['mesh'], prior['expected']['animation'], *prior['expected']['materials'], *prior['expected']['outline_materials']]: protect(disk(path))
    R['source'] = dict(report=str(source), report_sha256=sha(source), hero=prior['blueprint'], expected=prior['expected'])
    hero_class = generated(prior['blueprint'])
    mesh = load(prior['expected']['mesh'], U.SkeletalMesh)
    anim = generated(prior['expected']['animation'])
    cdo = U.get_default_object(hero_class)
    body = cdo.get_editor_property('mesh')
    check('source exact runtime CDO mesh/animation', body.get_editor_property('skeletal_mesh_asset') == mesh and body.get_editor_property('anim_class') == anim)
    check('source exact five materials', [package(body.get_material(i)) for i in range(body.get_num_materials())] == prior['expected']['materials'])
    outline_variants = author_outline_variants(body, prior)
    source_class = hero_class
    private_hero = A.duplicate_asset(prior['blueprint'], ROOT+'/BP_HairOutlinePrivateHero')
    check('private exact Hero BP copy', private_hero is not None)
    check('native private Hero compile', U.BlueprintEditorLibrary.compile_blueprint(private_hero))
    hero_class = private_hero.generated_class()
    private_body = U.get_default_object(hero_class).get_editor_property('mesh')
    check('private Hero exact mesh animation and all five main slots', private_body.get_editor_property('skeletal_mesh_asset') == mesh and private_body.get_editor_property('anim_class') == anim and [package(private_body.get_material(i)) for i in range(private_body.get_num_materials())] == prior['expected']['materials'])
    save(private_hero); R['private_hero'] = package(private_hero)
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
    # Unlike the original walking sequence this pose never moves forward: keep the wall behind the rear camera.
    box('Backdrop', (-650, 0, 240), (.2, 50, 8), material)
    R['fixture_note'] = 'Original neutral fixture lighting/exposure; backdrop x=-650 so the stationary rear camera x=-320 is not behind a wall. Same backdrop throughout all six shots.'
    spawn('Start', U.PlayerStart, (0, 0, 98), U.Rotator(yaw=0))
    director = spawn('Director', U.HCM5VS2HairReviewDirector)
    for name, value in dict(expected_character_class=hero_class, expected_animation_class=anim,
                            expected_mesh=mesh, source_report=str(source), outline_mask_comparison=True,
                            outline_comparison_materials=outline_variants).items(): director.set_editor_property(name, value)
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
          and directors[0].get_editor_property('source_report') == str(source)
          and directors[0].get_editor_property('outline_mask_comparison')
          and list(directors[0].get_editor_property('outline_comparison_materials')) == outline_variants)
    check('only seven private diagnostic packages', len(R['assets']) == 7)
    R['runtime_arguments'] = '-M5VS2HairReview -M5VS2HairOutlineReview -M5VS2AutoQuit -M5VS2EvidenceDir=<unique docs run> -windowed -ResX=1920 -ResY=1080'
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'; R['error'] = traceback.format_exc(); U.log_error(R['error'])
finally:
    changed = [file for file, digest in PROTECTED.items() if not Path(file).is_file() or sha(file) != digest]
    R['source_preservation'] = dict(files=len(PROTECTED), changed=changed,sha256_before=PROTECTED)
    if changed: R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS': raise RuntimeError('HairOutlineReviewAuthor failed; retain this attempt and evidence')
