"""Two isolated native shading candidates from authorized Selestia textures.
Candidate status is authoring only: both require the same-camera runtime review.
"""
from pathlib import Path
import datetime,json,re,hashlib,shutil,traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习');DOC=WORK/'docs/HarborCity_M5_VS2'
ROOT='/Game/HarborCity/M5VS2/HeroSelestia';OLD='/Game/HarborCity/M5VS1/HeroSelestia'
RAW=WORK/'assets/M5_VS1/hero/Selestia/UnityTextures_v1_02'
OWNER='HarborCity_M5_VS2_HeroMaterials'
A=U.EditorAssetLibrary;E=U.MaterialEditingLibrary;AT=U.AssetToolsHelpers.get_asset_tools()
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(args)==1
OUT=Path(args[0][0] or args[0][1]).resolve();assert OUT.is_relative_to(DOC.resolve())
R={'status':'RUNNING','checks':[],'assets':[],'variants':{},'runtime':'NOT_RUN','visual':'NOT_RUN',
   'limitations':['MToon is an approximation of original lilToon, not the original shader.',
     'SoftToon custom shader is an unlit directional comparison without environment cast-shadow reception; not accepted for final delivery.',
     'Outline pass is not authored or attached in this first comparison iteration.',
     'MatCap intentionally off because all five original material slots disable it.']}
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,observed=None):
    R['checks'].append({'name':n,'status':'PASS' if b else 'FAIL','observed':observed});dump()
    if not b:raise RuntimeError(n+': '+str(observed))
def load(p):
    o=U.load_object(None,p+'.'+p.rsplit('/',1)[1]);check('load '+p,o is not None);return o
def owned(p):
    if not A.does_asset_exist(p):return None
    o=A.load_asset(p);check('owned '+p,A.get_metadata_tag(o,'HarborCityOwnedBy')==OWNER)
    return o
def save(o):
    p=o.get_path_name().split('.')[0];check('VS2 only save',p.startswith(ROOT+'/'))
    A.set_metadata_tag(o,'HarborCityOwnedBy',OWNER);check('native save '+p,A.save_loaded_asset(o,False))
    f=WORK/'HarborCity/Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
    R['assets'].append({'path':p,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()});dump()
def make(p,cls,factory):
    o=owned(p)
    if o:return o
    folder,name=p.rsplit('/',1);o=AT.create_asset(name,folder,cls,factory);check('create '+p,o is not None);return o
def texture(filename,normal=False,mask=False):
    key=(filename,normal,mask)
    if key in textures:return textures[key]
    src=RAW/filename;check('private source texture exists',src.is_file(),filename)
    match=next(r for r in manifest['files'] if Path(r['output_path']).name==filename)
    check('private source unchanged '+filename,hashlib.sha256(src.read_bytes()).hexdigest()==match['output_sha256'])
    name='T_'+src.stem.replace(' ','_')+('_Linear' if mask else '')
    p=ROOT+'/Textures/'+name;o=owned(p)
    if o is None:
        task=U.AssetImportTask();task.filename=str(src);task.destination_path=ROOT+'/Textures';task.destination_name=name;task.automated=True;task.save=False
        AT.import_asset_tasks([task]);o=A.load_asset(p)
    check('native texture '+filename,isinstance(o,U.Texture2D))
    o.set_editor_property('srgb',not(normal or mask));o.set_editor_property('compression_settings',U.TextureCompressionSettings.TC_NORMALMAP if normal else (U.TextureCompressionSettings.TC_MASKS if mask else U.TextureCompressionSettings.TC_DEFAULT))
    o.set_editor_property('lod_bias',0);o.set_editor_property('max_texture_size',4096)
    if 'hair_alpha' in filename:
        o.set_editor_property('do_scale_mips_for_alpha_coverage',True);o.set_editor_property('alpha_coverage_thresholds',U.Vector4(0,0,0,.5))
    save(o);textures[key]=o;return o
def color(values):return U.LinearColor(*[float(values.get(k,1)) for k in ('r','g','b','a')])
def node(m,cls,x=0,y=0):return E.create_material_expression(m,cls,x,y)
def scalar(m,name,value):
    n=node(m,U.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def vector(m,name,value):
    n=node(m,U.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',color(value));return n
def link(a,b,pin,out=''):
    check('link '+pin,E.connect_material_expressions(a,out,b,pin))
def prop(n,p,out=''):check('material output '+str(p),E.connect_material_property(n,out,p))
def sample(m,t,normal=False,mask=False):
    n=node(m,U.MaterialExpressionTextureSample);n.texture=t
    n.sampler_type=U.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else (U.MaterialSamplerType.SAMPLERTYPE_MASKS if mask else U.MaterialSamplerType.SAMPLERTYPE_COLOR)
    return n
def setparam(mi,kind,n,value):
    names=[str(x) for x in getattr(E,'get_'+kind+'_parameter_names')(mi)]
    check('actual MToon '+kind+' parameter '+n,n in names)
    # UE5.8 SetMaterialInstanceTextureParameterValue always returns its initial
    # false despite applying the value (local source lines 1507-1515). Read back.
    getattr(E,'set_material_instance_'+kind+'_parameter_value')(mi,n,value)
    actual=getattr(E,'get_material_instance_'+kind+'_parameter_value')(mi,n)
    valid=(actual==value) if kind=='texture' else (abs(actual-value)<1e-5 if kind=='scalar' else max(abs(getattr(actual,k)-getattr(value,k)) for k in ('r','g','b','a'))<1e-5)
    check('readback '+n,valid,str(actual))
def switch(mi,n,value):
    check('actual static switch '+n,n in [str(x) for x in E.get_static_switch_parameter_names(mi)])
    U.VrmBPFunctionLibrary.vrm_change_material_static_switch(mi,n,value)
    check('static switch readback '+n,E.get_material_instance_static_switch_parameter_value(mi,n)==value)
def mtoon(slot,source):
    name='MI_MToon_'+slot;mi=make(ROOT+'/Materials/MToon/'+name,U.MaterialInstanceConstant,U.MaterialInstanceConstantFactoryNew())
    suffix='LitTranslucentTwoSided' if slot in ('hair','option') else 'LitOpaque'
    E.set_material_instance_parent(mi,load('/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOpt'+suffix))
    f=source['floats'];c=source['vectors_colors'];t=source['textures']
    tex=texture(Path(t['_MainTex']['reference']).name)
    for k in ('gltf_tex_diffuse','mtoon_tex_Shade'):setparam(mi,'texture',k,tex)
    setparam(mi,'vector','mtoon_Color',color(c['_Color']));setparam(mi,'vector','mtoon_ShadeColor',color(c['_ShadowColor']))
    # lilToon border is defined in half-Lambert space. MToon shift/toony
    # mapping is only an initial approximation; preserve both source values.
    shift=max(-1,min(1,2*f['_ShadowBorder']-1));toony=max(0,min(1,1-f['_ShadowBlur']))
    for k,v in {'mtoon_ShadeShift':shift,'mtoon_ShadeToony':toony,'mtoon_ReceiveShadowRate':1,'mtoon_LightColorAttenuation':.1,'mtoon_OutlineWidth':0}.items():setparam(mi,'scalar',k,v)
    switch(mi,'bUseMatCap',False);switch(mi,'bUseMatCap2',False)
    switch(mi,'bUseRimLight',bool(f.get('_UseRim',0)))
    # The original FBX is already authored for this game's projection/tonemap.
    # Do not inherit VRM demonstration projection or inverse-film compensation.
    for n in ('bUseFilmTonemapInverse','bDivAdaptation','bUseFOVFix','bUseFOVScale'):switch(mi,n,False)
    switch(mi,'bUseNormalmap',bool(t.get('_BumpMap',{}).get('reference')))
    switch(mi,'bUseEmissiveMap',bool(f['_UseEmission']))
    setparam(mi,'vector','mtoon_RimColor',color(c['_RimColor']))
    setparam(mi,'scalar','mtoon_RimFresnelPower',f['_RimFresnelPower'])
    setparam(mi,'scalar','mtoon_RimLightingMix',1.)
    if t.get('_BumpMap',{}).get('reference'):
        setparam(mi,'texture','mtoon_tex_Normal',texture(Path(t['_BumpMap']['reference']).name,normal=True))
        setparam(mi,'scalar','mtoon_NormalScale',f['_BumpScale'])
    if f['_UseEmission']:
        setparam(mi,'texture','mtoon_tex_Emissive',texture(Path(t['_EmissionMap']['reference']).name))
        setparam(mi,'vector','mtoon_EmissionColor',color(c['_EmissionColor']))
    else:setparam(mi,'vector','mtoon_EmissionColor',U.LinearColor(0,0,0,1))
    if slot=='hair':
        # The author's hair uses lilToon transparency with ZWrite=1 and queue2450.
        # UE Translucent does not reproduce that depth behavior. The exact source
        # hair alpha spans154..255, so this .5 masked candidate removes no source
        # texels; fractional transparency is approximated and needs visual review.
        check('hair alpha candidate source depth/cull',f['_ZWrite']==1 and f['_Cull']==0
              and source['render_queue']==2450 and Path(t['_MainTex']['reference']).name=='Selestia_hair_alpha.png')
        check('hair alpha uses main texture',not E.get_material_instance_static_switch_parameter_value(mi,'bUseAlphaTexture'))
        clip=float(f['_PreCutoff']);check('source hair prepass cutoff',abs(clip-.5)<1e-6)
        overrides=mi.get_editor_property('base_property_overrides')
        alpha_fields={'override_blend_mode':True,'blend_mode':U.BlendMode.BLEND_MASKED,
                      'override_two_sided':True,'two_sided':True,
                      'override_opacity_mask_clip_value':True,'opacity_mask_clip_value':clip}
        for key,value in alpha_fields.items():overrides.set_editor_property(key,value)
        mi.set_editor_property('base_property_overrides',overrides)
        E.update_material_instance(mi)
        actual=mi.get_editor_property('base_property_overrides')
        for key,value in alpha_fields.items():
            observed=actual.get_editor_property(key)
            check('hair masked override readback '+key,abs(observed-value)<1e-6 if isinstance(value,float) else observed==value,str(observed))
        R['hair_alpha_candidate']={'source_material_sha256':source['raw_sha256'],
            'source_shader':source.get('resolved_shader',{}).get('name'),
            'source_render_queue':source['render_queue'],'source_zwrite':f['_ZWrite'],
            'source_cutoff':f['_Cutoff'],'source_prepass_cutoff':clip,
            'candidate_blend':'BLEND_MASKED','candidate_two_sided':True,'candidate_clip':clip,
            'parent_and_color_parameters':'UNCHANGED','option':'UNCHANGED_TRANSLUCENT',
            'scope':'Native override readback only; shader readiness and same-camera appearance require runtime review.'}
    else:E.update_material_instance(mi)
    save(mi);return mi
def soft(slot,source):
    name='M_SoftToon_'+slot;m=make(ROOT+'/Materials/SoftToon/'+name,U.Material,U.MaterialFactoryNew())
    E.delete_all_material_expressions(m);m.set_editor_property('shading_model',U.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('used_with_skeletal_mesh',True);m.set_editor_property('used_with_morph_targets',True);m.set_editor_property('two_sided',True)
    m.set_editor_property('blend_mode',U.BlendMode.BLEND_TRANSLUCENT if slot=='option' else (U.BlendMode.BLEND_MASKED if slot=='hair' else U.BlendMode.BLEND_OPAQUE))
    f=source['floats'];c=source['vectors_colors'];t=source['textures']
    base=sample(m,texture(Path(t['_MainTex']['reference']).name))
    custom=node(m,U.MaterialExpressionCustom)
    inputs=['Base','Normal','View','Sun','Key','Ambient','Shadow1','Shadow2','Border','Blur','Border2','Blur2','RimColor','RimPower','RimStrength']
    custom_inputs=[]
    for name in inputs:
        entry=U.CustomInput();entry.set_editor_property('input_name',name);custom_inputs.append(entry)
    custom.set_editor_property('inputs',custom_inputs);custom.set_editor_property('output_type',U.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('code','''float ndl=dot(normalize(Normal),normalize(Sun))*0.5+0.5;
float s1=smoothstep(Border-max(Blur,0.001)*0.5,Border+max(Blur,0.001)*0.5,ndl);
float s2=smoothstep(Border2-max(Blur2,0.001)*0.5,Border2+max(Blur2,0.001)*0.5,ndl);
float3 shade=lerp(Shadow2,Shadow1,s2);shade=lerp(shade,float3(1,1,1),s1);
float rim=pow(1-saturate(dot(normalize(Normal),normalize(View))),RimPower)*RimStrength;
return Base*shade*(Ambient+Key)+Base*RimColor*rim;''')
    link(base,custom,'Base','RGB');link(node(m,U.MaterialExpressionPixelNormalWS),custom,'Normal');link(node(m,U.MaterialExpressionCameraVectorWS),custom,'View')
    values={'Sun':('SunDirection',dict(r=.5,g=.4,b=.76,a=1)),'Key':('LightColor',dict(r=.8,g=.8,b=.8,a=1)),'Ambient':('AmbientColor',dict(r=.3,g=.32,b=.38,a=1)),
        'Shadow1':('ShadowColor',c['_ShadowColor']),'Shadow2':('Shadow2Color',c['_Shadow2ndColor']),'RimColor':('RimColor',c['_RimColor'])}
    for pin,(param,value) in values.items():link(vector(m,param,value),custom,pin)
    for pin,param,value in [('Border','ShadowBorder',f['_ShadowBorder']),('Blur','ShadowBlur',f['_ShadowBlur']),('Border2','Shadow2Border',f['_Shadow2ndBorder']),('Blur2','Shadow2Blur',f['_Shadow2ndBlur']),('RimPower','RimPower',f['_RimFresnelPower']),('RimStrength','RimStrength',.12 if f.get('_UseRim') else 0)]:link(scalar(m,param,value),custom,pin)
    result=custom
    if f['_UseEmission']:
        em=sample(m,texture(Path(t['_EmissionMap']['reference']).name));mul=node(m,U.MaterialExpressionMultiply);link(em,mul,'A','RGB');link(vector(m,'EmissionColor',c['_EmissionColor']),mul,'B');add=node(m,U.MaterialExpressionAdd);link(custom,add,'A');link(mul,add,'B');result=add
    prop(result,U.MaterialProperty.MP_EMISSIVE_COLOR)
    if slot in ('hair','option'):prop(base,U.MaterialProperty.MP_OPACITY if slot=='option' else U.MaterialProperty.MP_OPACITY_MASK,'A')
    E.recompile_material(m);save(m);return m
try:
    audit=json.loads((DOC/'research/SELESTIA_SOURCE_AUDIT.json').read_text(encoding='utf-8'))
    manifest=json.loads((RAW/'TEXTURE_EXTRACTION_MANIFEST.json').read_text(encoding='utf-8'));textures={}
    sources={Path(m['asset_path']).stem.removeprefix('Selestia_'):m for m in audit['materials'] if '/lilToon/' in m['asset_path']}
    check('all original lilToon slots',set(sources)=={'hair','body','face','option','costume'})
    for mode,fn in [('MToon',mtoon),('SoftToon',soft)]:
        R['variants'][mode]={slot:fn(slot,sources[slot]).get_path_name() for slot in ('hair','body','face','option','costume')}
    R['status']='PASS';R['pass_scope']='Native material authoring and parameter readback only; rendered shader compilation and appearance not yet validated.'
except Exception:R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:dump()
if R['status']!='PASS':raise RuntimeError('M5-VS2 material authoring failed')
