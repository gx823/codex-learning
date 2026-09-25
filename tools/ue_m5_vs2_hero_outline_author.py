"""Native source-width inverted-hull outline candidate. Existing hero and paid source are read-only."""
from pathlib import Path
import hashlib,json,re,traceback,uuid
import unreal as U
WORK=Path('D:/科研学习/codex学习');DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary; E=U.MaterialEditingLibrary; AT=U.AssetToolsHelpers.get_asset_tools()
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(args)==1
OUT=Path(args[0][0] or args[0][1]).resolve();assert OUT.is_relative_to(DOC.resolve())
TOKEN=uuid.uuid4().hex[:12];ROOT='/Game/HarborCity/M5VS2/HeroRev2/Review_'+TOKEN
RAW=WORK/'assets/M5_VS1/hero/Selestia/UnityTextures_v1_02'
OLD_BP='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia'
R=dict(status='RUNNING',root=ROOT,checks=[],assets=[],runtime='NOT_RUN',visual='NOT_RUN',
       source_width_conversion='lilToon width * 0.01 metres * 100 cm/metre * actual 1.25 mesh scale',
       width_source_url='https://github.com/lilxyzw/lilToon/blob/master/Assets/lilToon/Shader/Includes/lil_common_functions.hlsl',
       limitations=['Source face outline colour texture GUID is external/unresolved; white texture fallback is explicit, source face colour and width mask retained.',
                    'UE masked inverted hull with default-lit response approximates lilToon outline lighting. Distance/readability and temporal stability require real viewport review.'])
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,v=None):
    R['checks'].append(dict(name=n,status='PASS' if b else 'FAIL',observed=v));dump()
    if not b:raise RuntimeError(n+': '+str(v))
def disk(p):return WORK/'HarborCity/Content'/Path(p.removeprefix('/Game/').split('.')[0]).with_suffix('.uasset')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(o):
    p=o.get_path_name().split('.')[0];check('unique revision namespace',p.startswith(ROOT+'/'))
    A.set_metadata_tag(o,'HarborCityOwnedBy','HarborCity_M5_VS2_HeroOutline_R2')
    check('native save '+p,A.save_loaded_asset(o,False));R['assets'].append(dict(path=p,sha256=sha(disk(p))));dump()
def make(name,cls,factory):
    p=ROOT+'/'+name;check('new asset only '+p,not A.does_asset_exist(p))
    folder,n=p.rsplit('/',1);o=AT.create_asset(n,folder,cls,factory);check('create '+p,o is not None);return o
def node(m,cls):return E.create_material_expression(m,cls,0,0)
def scalar(m,name,value):
    n=node(m,U.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def constant(m,v):
    n=node(m,U.MaterialExpressionConstant);n.set_editor_property('r',v);return n
def link(a,b,pin,out=''):check('material expression link',E.connect_material_expressions(a,out,b,pin),pin)
def prop(a,p,out=''):check('material output',E.connect_material_property(a,out,p),str(p))
def mul(m,a,b):
    n=node(m,U.MaterialExpressionMultiply);link(a,n,'A');link(b,n,'B');return n
textures={}
def texture(filename,mask=False):
    key=(filename,mask)
    if key in textures:return textures[key]
    src=RAW/filename;check('private texture exists',src.is_file(),filename)
    manifest=json.loads((RAW/'TEXTURE_EXTRACTION_MANIFEST.json').read_text(encoding='utf-8'))
    row=next(x for x in manifest['files'] if Path(x['output_path']).name==filename)
    check('source texture SHA '+filename,sha(src)==row['output_sha256'])
    name='T_'+src.stem.replace(' ','_')+('_SourceWidth' if mask else '')
    task=U.AssetImportTask();task.filename=str(src);task.destination_path=ROOT+'/Textures';task.destination_name=name;task.automated=True;task.save=False
    AT.import_asset_tasks([task]);t=A.load_asset(ROOT+'/Textures/'+name);check('native texture',isinstance(t,U.Texture2D))
    # Source Unity width masks explicitly use sRGBTexture=1; preserve that transfer.
    # A generic linear-mask import would widen fractional mask values.
    t.set_editor_property('srgb',True);t.set_editor_property('compression_settings',U.TextureCompressionSettings.TC_DEFAULT)
    save(t);textures[key]=t;return t
def sample(m,t,mask=False,vertex=False):
    s=node(m,U.MaterialExpressionTextureSample);s.set_editor_property('texture',t);s.set_editor_property('sampler_type',U.MaterialSamplerType.SAMPLERTYPE_COLOR)
    if vertex:
        s.set_editor_property('mip_value_mode',U.TextureMipValueMode.TMVM_MIP_LEVEL)
        link(constant(m,0),s,'Level')
    return s
try:
    check('native new outline helper compiled',hasattr(U,'HCM5VS2HeroOutlineEditor'))
    original=A.load_asset(OLD_BP);check('source hero compiled',original is not None)
    protected={str(disk(OLD_BP)):sha(disk(OLD_BP))}
    body=U.get_default_object(original.generated_class()).get_editor_property('mesh')
    scale=body.get_editor_property('relative_scale3d');check('actual uniform mesh scale 1.25',max(abs(getattr(scale,k)-1.25) for k in ('x','y','z'))<1e-5)
    materials=json.loads((DOC/'research/SELESTIA_SOURCE_AUDIT.json').read_text(encoding='utf-8'))['materials']
    sources={Path(x['asset_path']).stem.removeprefix('Selestia_'):x for x in materials if '/Materials/lilToon/' in x['asset_path']}
    mats=[];R['source_slots']=[]
    for slot in ('hair','body','face','option','costume'):
        source=sources[slot];enabled=source['resolved_shader']['name'].endswith('Outline')
        m=make('Materials/M_SourceOutline_'+slot,U.Material,U.MaterialFactoryNew())
        m.set_editor_property('blend_mode',U.BlendMode.BLEND_MASKED);m.set_editor_property('two_sided',True)
        m.set_editor_property('shading_model',U.MaterialShadingModel.MSM_DEFAULT_LIT)
        for key in ('used_with_skeletal_mesh','used_with_morph_targets'):m.set_editor_property(key,True)
        # Back faces only, with normal-depth testing still active. The translucent cape has no source outline.
        sign=node(m,U.MaterialExpressionTwoSidedSign);back=node(m,U.MaterialExpressionOneMinus);link(sign,back,'')
        prop(mul(m,back,constant(m,.5 if enabled else 0)),U.MaterialProperty.MP_OPACITY_MASK)
        color=source['vectors_colors']['_OutlineColor'];c=node(m,U.MaterialExpressionVectorParameter);c.set_editor_property('parameter_name','SourceOutlineColor');c.set_editor_property('default_value',U.LinearColor(color['r'],color['g'],color['b'],color['a']))
        tint=c;tex=source['textures']['_OutlineTex'].get('reference')
        if tex and tex not in ('BUILTIN_OR_EXTERNAL',):
            tint=mul(m,c,sample(m,texture(Path(tex).name)))
        prop(tint,U.MaterialProperty.MP_BASE_COLOR)
        prop(constant(m,1),U.MaterialProperty.MP_ROUGHNESS);prop(constant(m,0),U.MaterialProperty.MP_SPECULAR)
        width=float(source['floats']['_OutlineWidth']) if enabled else 0
        w=scalar(m,'SourceOutlineWidthCm',width*float(scale.x))
        mask=source['textures']['_OutlineWidthMask'].get('reference')
        if mask:
            sm=sample(m,texture(Path(mask).name,True),True,True)
            red=node(m,U.MaterialExpressionComponentMask)
            for channel in ('r','g','b','a'):red.set_editor_property(channel,channel=='r')
            link(sm,red,'')
            w=mul(m,w,red)
        prop(mul(m,node(m,U.MaterialExpressionVertexNormalWS),w),U.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        E.recompile_material(m);save(m);mats.append(m)
        R['source_slots'].append(dict(slot=slot,enabled=enabled,width_liltoon=width,width_world_cm=width*float(scale.x),width_mask=mask,color=color,outline_texture=tex,material=m.get_path_name()))
    bp=AT.duplicate_asset('BP_HeroSourceOutline_'+TOKEN,ROOT,original);check('new hero copy',bp is not None)
    result=json.loads(U.HCM5VS2HeroOutlineEditor.configure_outline(bp,mats));R['native_install']=result;check('native outline configuration',result.get('status')=='PASS')
    save(bp);R['blueprint']=bp.get_path_name();R['blueprint_class']=bp.generated_class().get_path_name()
    check('existing hero bytes unchanged',all(sha(p)==h for p,h in protected.items()),protected)
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:dump()
if R['status']!='PASS':raise RuntimeError(R.get('error','Outline author failed'))
