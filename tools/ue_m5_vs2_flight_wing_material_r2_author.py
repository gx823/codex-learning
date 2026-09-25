"""One feather-only material / safety-layer Hero A-B candidate; no map or runtime.

Root serially runs FlightWingMaterialApply then FlightWingMaterialReload.
Requires the staged optional WingMaterial property compiled into the real module.
The original additive material still drives wind ribbons and the takeoff rune.
"""
from pathlib import Path
import ast, datetime as dt, hashlib, json, math, re, traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve(); PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary; B=U.BlueprintEditorLibrary; E=U.MaterialEditingLibrary
CMD=U.SystemLibrary.get_command_line()
def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',CMD)
    assert len(rows)==1,name
    return rows[0][0] or rows[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve(); PHASE=arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC/'editor_runtime') and PHASE in ('FlightWingMaterialApply','FlightWingMaterialReload')
assert not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
SCHEMA='HarborCity.M5VS2.FlightWingMaterialR2.v1'; OWNER='HarborCity_M5VS2_FlightWingMaterialR2'
ROOT='/Game/HarborCity/M5VS2/FlightVisual/MaterialReview_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
R=dict(schema=SCHEMA,owner=OWNER,phase=PHASE,status='RUNNING',checks=[],inputs={},assets=[],map_writes=0,old_asset_writes=0,
       runtime='NOT_RUN',art='USER_REVIEW',single_camera_ab='NOT_RUN',fresh_process_disk_readback='NOT_RUN',
       scope='New feather-only material and duplicate safety-layer Hero; existing wing geometry, anchor, flap, physics, inputs, FP hide, wind/rune, body/arms/5 material slots and safety garment unchanged.',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={}; SOURCES={}
def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok: dump(); raise RuntimeError(name+': '+str(actual))

# Reuse only the already verified provenance/readback functions, never that
# script's map author. These helpers retain the precise old-DLL history exception.
READER=WORK/'tools/ue_m5_vs2_corner_playable_r5_author.py'
SOURCE=READER.read_text('utf-8-sig')
HELPERS=('sha','package','path','disk','binding','protect','document','process','report','latest','assets','load','cls',
         'resolve_base_hero','hero_readers','resolve_hero')
nodes=[n for n in ast.parse(SOURCE).body if isinstance(n,ast.FunctionDef) and n.name in HELPERS]
check('exact existing read-only provenance helper set',tuple(n.name for n in nodes)==HELPERS)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(READER)+'::wing_readers','exec'),globals())
protect(READER,kind='script')
R['provenance_readers']=dict(path=str(READER),functions=list(HELPERS),sha256=hashlib.sha256('\n\n'.join(ast.get_source_segment(SOURCE,n) for n in nodes).encode()).hexdigest())

COLORS={'RootColor':[.11,.18,.28,1.],'MidColor':[.38,.57,.72,1.],
        'TipColor':[.66,.82,.94,1.],'EdgeColor':[.68,.85,1.,1.]}
SCALARS={'GlowGain':2.3,'OpacityGain':.82}
CODE='''
// Original UV shading on the existing curved feather. No image, texture or WPO.
float u=saturate(UV.x), across=abs(UV.y*2.0-1.0);
float rootBlend=smoothstep(0.04,0.48,u);
float tipBlend=smoothstep(0.58,0.98,u);
float3 body=lerp(lerp(RootColor,MidColor,rootBlend),TipColor,tipBlend);
// Soft central ridge and diagonal barb divisions retain a readable blade.
float ridge=1.0-smoothstep(0.025,0.15,across);
float barb=0.5+0.5*cos((u*17.0+across*2.6)*6.2831853);
body*=0.88+0.12*barb;
body+=float3(0.025,0.032,0.04)*ridge;
// Narrow halo within the actual edge, never another additive feather shell.
float edge=smoothstep(0.64,0.87,across)*(1.0-smoothstep(0.90,1.0,across));
float gain=clamp(GlowGain/2.3,0.0,1.6);
float3 color=body*(0.78+0.08*gain)+EdgeColor*edge*0.36*gain;
float coverage=(1.0-smoothstep(0.92,1.0,across))*smoothstep(0.0,0.025,u);
return float4(color,saturate(OpacityGain*coverage));
'''

def material_snapshot(material):
    expressions=list(E.get_material_expressions(material)); rows=[]
    for node in expressions:
        row=dict(name=node.get_name(),class_name=node.get_class().get_path_name(),
                 input_names=list(E.get_material_expression_input_names(node)),
                 inputs=[x.get_name() if x else None for x in E.get_inputs_for_material_expression(material,node)])
        if isinstance(node,U.MaterialExpressionScalarParameter):
            row.update(parameter=str(node.get_editor_property('parameter_name')),default=float(node.get_editor_property('default_value')))
        if isinstance(node,U.MaterialExpressionVectorParameter):
            color=node.get_editor_property('default_value')
            row.update(parameter=str(node.get_editor_property('parameter_name')),default=[float(color.r),float(color.g),float(color.b),float(color.a)])
        if isinstance(node,U.MaterialExpressionCustom):
            row.update(code=node.get_editor_property('code'),output_type=str(node.get_editor_property('output_type')))
        rows.append(row)
    outputs={}
    for key,prop in (('emissive',U.MaterialProperty.MP_EMISSIVE_COLOR),('opacity',U.MaterialProperty.MP_OPACITY)):
        node=E.get_material_property_input_node(material,prop)
        outputs[key]=node.get_name() if node else None
    return dict(path=package(material),blend_mode=str(material.get_editor_property('blend_mode')),
                shading_model=str(material.get_editor_property('shading_model')),two_sided=bool(material.get_editor_property('two_sided')),
                outputs=outputs,nodes=sorted(rows,key=lambda row:row['name']))

def source_probe(ns,source_bp):
    components=[c for c in ns['scs_components'](source_bp) if isinstance(c,U.HCM5VS2FlightVisualComponent)]
    check('one real source flight component',len(components)==1)
    comp=components[0]
    check('new optional wing override is unused in original Hero',comp.get_editor_property('wing_material') is None)
    evidence,visual=latest('author_*_FlightVisualReload',lambda d:d.get('phase')=='FlightVisualReload' and len(d.get('assets',[]))==4)
    assets(visual['assets']);R['inputs']['original_flight_visual_reload']=binding(evidence,'evidence')
    expected_geometry={row['kind']:row['path'] for row in visual['geometry']}
    check('actual original four flight assets match successful native Reload',
          package(comp.get_editor_property('light_material'))==visual['material']
          and all(package(comp.get_editor_property(field))==expected_geometry[kind]
                  for field,kind in (('feather_mesh','Feather'),('ribbon_mesh','Ribbon'),('rune_mesh','Rune'))))
    material=comp.get_editor_property('light_material'); before=material_snapshot(material)
    check('actual source is additive unlit two-sided',material.get_editor_property('blend_mode')==U.BlendMode.BLEND_ADDITIVE
          and material.get_editor_property('shading_model')==U.MaterialShadingModel.MSM_UNLIT and material.get_editor_property('two_sided'),before)
    parameters={row['parameter']:row['default'] for row in before['nodes'] if 'parameter' in row}
    wanted={'LightColor':[.58,.83,1.,1.],'GlowGain':2.3,'OpacityGain':.8}
    check('actual source defaults explain additive emission',set(parameters)==set(wanted)
          and all(max(abs(a-b) for a,b in zip(parameters[k],v))<1e-6 if isinstance(v,list) else abs(parameters[k]-v)<1e-6 for k,v in wanted.items()),parameters)
    protect(disk(material),kind='package')
    feather=comp.get_editor_property('feather_mesh'); protect(disk(feather),kind='package')
    check('original curved feather triangle count unchanged',feather.get_num_triangles(0)==1248,feather.get_num_triangles(0))
    hero,body,_,_=ns['cdo_objects'](source_bp)
    modifier=U.SkeletonModifier(); check('native reference-pose bone reader',modifier.set_skeletal_mesh(body.get_editor_property('skeletal_mesh_asset')))
    bones={name:ns['transform'](modifier.get_bone_transform(name,True)) for name in ('Chest','Neck','Head')}
    R['source_shader']=before
    R['anchor_probe']=dict(bones_reference_mesh_space=bones,body_component_relative=ns['transform'](body.get_relative_transform()),
        source_formula='Anchor=Body.GetSocketLocation(Chest); root=Anchor+(ActorYaw*Bank).RotateVector(Base)*Size; Base=(-20,Side*(14+18*T),15+8*sin(pi*T)), T=0..1; Size=clamp(BodyScaleMax/1.25,.5,2)',
        runtime_world_bone_and_feather_positions='NOT_MEASURED_BY_COMMANDLET',scope='Real current reference bones plus unchanged actual C++ formula. Not a live posed world transform or inferred Head socket attachment.')
    return comp

def create_material():
    mat=U.AssetToolsHelpers.get_asset_tools().create_asset('M_WingSoftLayers',ROOT,U.Material,U.MaterialFactoryNew())
    check('fresh native wing-only material',mat is not None)
    mat.set_editor_property('blend_mode',U.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',U.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    def make(kind,**values):
        node=E.create_material_expression(mat,kind)
        check('native material expression exists',node is not None)
        for name,value in values.items(): node.set_editor_property(name,value)
        return node
    def connect(source,target,pin):
        result=E.connect_material_expressions(source,'',target,pin)
        check('material pin actual connected '+pin,result and source in E.get_inputs_for_material_expression(mat,target))
    inputs=[]; values={}
    for name,color in COLORS.items(): values[name]=make(U.MaterialExpressionVectorParameter,parameter_name=name,default_value=U.LinearColor(*color))
    for name,value in SCALARS.items(): values[name]=make(U.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)
    values['UV']=make(U.MaterialExpressionTextureCoordinate)
    for name in values:
        item=U.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom=make(U.MaterialExpressionCustom,code=CODE,output_type=U.CustomMaterialOutputType.CMOT_FLOAT4,inputs=inputs)
    for name,node in values.items(): connect(node,custom,name)
    # UE 5.8 MaterialGraphNode shortens Input to NAME_None; empty pin selects 0.
    color=make(U.MaterialExpressionComponentMask,r=True,g=True,b=True,a=False);connect(custom,color,'')
    alpha=make(U.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True);connect(custom,alpha,'')
    for node,prop in ((color,U.MaterialProperty.MP_EMISSIVE_COLOR),(alpha,U.MaterialProperty.MP_OPACITY)):
        E.connect_material_property(node,'',prop)
        check('native material output readback',E.get_material_property_input_node(mat,prop)==node)
    E.recompile_material(mat)
    return mat

def inspect(ns,bp,material,keep,safety):
    check('complete original body arms materials motion input baseline preserved',ns['baseline'](bp)==keep)
    components=ns['scs_components'](bp)
    wings=[c for c in components if isinstance(c,U.HCM5VS2FlightVisualComponent)]
    modesty=[c for c in components if isinstance(c,U.HCM5VS2HeroModestyComponent)]
    check('exact one wing component and exact same original light material',len(wings)==1 and wings[0].get_editor_property('wing_material')==material
          and package(wings[0].get_editor_property('light_material'))==package(keep['HCM5VS2FlightVisualComponent']['light_material']))
    check('exact safety garment layer preserved',len(modesty)==1 and ns['props'](modesty[0],('safety_shorts','cloth_material'))==safety)
    snap=material_snapshot(material)
    check('only translucent unlit feather override',material.get_editor_property('blend_mode')==U.BlendMode.BLEND_TRANSLUCENT
          and material.get_editor_property('shading_model')==U.MaterialShadingModel.MSM_UNLIT and material.get_editor_property('two_sided'))
    params={row['parameter']:row['default'] for row in snap['nodes'] if 'parameter' in row}
    expected=dict(COLORS,**SCALARS)
    check('all gradient parameter defaults read back',set(params)==set(expected)
          and all(max(abs(a-b) for a,b in zip(params[k],v))<1e-6 if isinstance(v,list) else abs(params[k]-v)<1e-6 for k,v in expected.items()),params)
    custom=[row for row in snap['nodes'] if 'code' in row]
    check('exact original shader and both native outputs',len(custom)==1 and custom[0]['code']==CODE and all(snap['outputs'].values()))
    return dict(blueprint=package(bp),source_baseline=keep,safety_component=safety,wing_material=package(material),material=snap,
                same_original_light_material=package(wings[0].get_editor_property('light_material')))

def save(obj):
    check('save new own package only',package(obj).startswith(ROOT+'/'))
    A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER)
    check('native save succeeded',A.save_loaded_asset(obj,False))
    R['assets'].append(dict(path=package(obj),sha256=sha(disk(obj)),bytes=disk(obj).stat().st_size))

try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty content or PIE',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    protect(__file__,kind='script')
    for ext in ('.h','.cpp'): protect(PROJECT/'Source/HarborCity/M5VS2'/('HCM5VS2FlightVisualComponent'+ext),kind='cpp')
    project_file=PROJECT/'HarborCity.uproject'
    definition=json.loads(project_file.read_text('utf-8-sig'))
    module_names=[item['Name'] for item in definition['Modules']]
    check('actual project runtime and editor modules',module_names==['HarborCity','HarborCityEditor'],module_names)
    # Exact project file is outside the shared helper's config/ roots.
    PROTECTED[str(project_file.resolve())]=sha(project_file)
    R['project_definition']=dict(path=str(project_file),sha256=sha(project_file))
    R['current_modules']=[protect(PROJECT/'Binaries/Win64'/('UnrealEditor-'+name+'.dll'),kind='module') for name in module_names]
    if PHASE=='FlightWingMaterialReload':
        f,prior=latest('author_*_FlightWingMaterialApply',lambda d:d.get('schema')==SCHEMA)
        check('same frozen author and compiled sources before fresh Reload',all(sha(p)==h for p,h in prior['preservation']['source_sha256_before'].items()))
        for name,row in prior['source_bindings'].items():protect(name,row['sha256'],row['kind'])
        R['inputs']['apply']=binding(f,'evidence');assets(prior['assets'])
        ROOT=prior['destination'];check('exact unique candidate namespace',re.fullmatch(r'/Game/HarborCity/M5VS2/FlightVisual/MaterialReview_[0-9a-f]{12}',ROOT) is not None)
        ns=hero_readers(); bp=load(prior['blueprint']);mat=load(prior['material'])
        actual=inspect(ns,bp,mat,prior['native_readback']['source_baseline'],prior['native_readback']['safety_component'])
        check('fresh-process readback exactly matches Apply',actual==prior['native_readback'])
        R.update(status='PASS',destination=ROOT,blueprint=prior['blueprint'],source_blueprint=prior['source_blueprint'],material=prior['material'],
                 assets=prior['assets'],native_readback=actual,source_shader=prior['source_shader'],anchor_probe=prior['anchor_probe'],
                 fresh_process_disk_readback='PASS',ab_candidate=prior['ab_candidate'])
    else:
        resolve_hero();source_bp=load(R['selected_hero']);ns=hero_readers();source_probe(ns,source_bp)
        keep=ns['baseline'](source_bp);safety=R['hero_safety']['component']
        check('new namespace not previously authored',not A.does_directory_exist(ROOT))
        mat=create_material();bp=A.duplicate_asset(package(source_bp),ROOT+'/BP_HeroWingSoftLayers')
        check('new native safety-layer Hero copy',bp is not None)
        components=[c for c in ns['scs_components'](bp) if isinstance(c,U.HCM5VS2FlightVisualComponent)]
        check('one duplicated editable wing component',len(components)==1)
        components[0].set_editor_property('wing_material',mat);B.compile_blueprint(bp)
        before=inspect(ns,bp,mat,keep,safety)
        save(mat);save(bp)
        after=inspect(ns,bp,mat,keep,safety);check('same-process saved native readback',after==before)
        R.update(status='PASS',destination=ROOT,source_blueprint=package(source_bp),blueprint=package(bp),material=package(mat),native_readback=after,
                 ab_candidate=dict(A=package(source_bp),B=package(bp),only_difference='WingMaterial override; original LightMaterial/feather mesh and all other character bindings unchanged',
                    suggested_single_camera='Same R5 afternoon, same rear-hover camera and fly state; one A image and one B image after shader readiness. Visual-only comparison, never input test.',
                    actual_images='NOT_RUN'))
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(source_sha256_before=PROTECTED,changed_source_files=changed)
    R['source_bindings']=SOURCES
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Wing material candidate failed; retain unique partial assets and all prior evidence.')
