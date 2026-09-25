"""Read actual native material parameters and source mesh configuration; no asset writes."""
from pathlib import Path
import json,re,traceback
import unreal as U
cmd=U.SystemLibrary.get_command_line()
matches=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',cmd)
assert len(matches)==1
out=Path(matches[0][0] or matches[0][1]).resolve()
assert out.is_relative_to(Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2').resolve())
R={'status':'RUNNING','asset_writes':0,'visual':'NOT_RUN','materials':[],'textures':[]}
E=U.MaterialEditingLibrary;A=U.EditorAssetLibrary
def obj(p):
    o=U.load_object(None,p+'.'+p.rsplit('/',1)[1]);assert o,p;return o
def read(o,p):
    try:return str(o.get_editor_property(p))
    except Exception as e:return 'UNAVAILABLE: '+str(e)
try:
    for p in ['/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitOpaque','/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitTranslucentTwoSided','/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptUnlitOpaque','/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOptLitOutline']:
        o=obj(p)
        row={'path':p,'scalars':{},'vectors':{},'textures':{},'switches':{}}
        for kind in ('scalar','vector','texture','static_switch'):
            names=getattr(E,'get_'+kind+'_parameter_names')(o)
            dest={'scalar':'scalars','vector':'vectors','texture':'textures','static_switch':'switches'}[kind]
            for n in names:
                try:v=getattr(E,'get_material_instance_'+kind+'_parameter_value')(o,n)
                except Exception as e:v='UNAVAILABLE: '+str(e)
                row[dest][str(n)]=str(v)
        R['materials'].append(row)
    root='/Game/HarborCity/M5VS1/HeroSelestia'
    mesh=obj(root+'/SKM_Selestia');R['mesh']={'path':mesh.get_path_name(),'bounds':str(mesh.get_bounds()),'skeleton':str(mesh.get_editor_property('skeleton')),'materials':str(mesh.get_editor_property('materials'))}
    for p in A.list_assets(root+'/Textures',recursive=True,include_folder=False):
        t=A.load_asset(p)
        if isinstance(t,U.Texture2D):R['textures'].append({'path':p,**{n:read(t,n) for n in ('srgb','compression_settings','lod_bias','max_texture_size','mip_gen_settings','do_scale_mips_for_alpha_coverage','alpha_coverage_thresholds','lod_group','never_stream')}})
    R['python_api']={name:getattr(U,name).__doc__ for name in ('MaterialExpressionCustom','MaterialExpressionCollectionParameter','MaterialParameterCollection','MaterialExpressionTextureSampleParameter2D','MaterialExpressionVectorParameter','MaterialExpressionScalarParameter')}
    R['status']='PASS'
except Exception:R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:(out/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('Probe failed; see report')
