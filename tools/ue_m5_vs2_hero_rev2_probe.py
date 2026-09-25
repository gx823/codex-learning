"""Read-only native preflight for source outlines, halo parts and scale. No asset writes."""
from pathlib import Path
import hashlib, json, re, traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习'); DOC=WORK/'docs/HarborCity_M5_VS2'
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
assert len(args)==1
OUT=Path(args[0][0] or args[0][1]).resolve(); assert OUT.is_relative_to(DOC.resolve())
R={'status':'RUNNING','read_only':True,'checks':[],'assets':[],'runtime':'NOT_RUN'}
def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,observed=None):
    R['checks'].append(dict(name=n,status='PASS' if b else 'FAIL',observed=observed));dump()
    if not b:raise RuntimeError(n)
def xyz(v):return [float(v.x),float(v.y),float(v.z)]
def protected(p):
    f=WORK/'HarborCity/Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
    return {'path':str(f),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
try:
    paths=['/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_Selestia',
           '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia',
           '/Game/HarborCity/M5VS1/HeroSelestia/Halo/SM_Selestia_Halo']
    R['protected_before']=[protected(p) for p in paths]
    bp=U.EditorAssetLibrary.load_asset(paths[0]);check('actual hero blueprint',bp is not None)
    cdo=U.get_default_object(bp.generated_class()); mesh=cdo.get_editor_property('mesh')
    capsule=cdo.get_editor_property('capsule_component')
    R['character']={'class':cdo.get_class().get_path_name(),'actor_scale':xyz(cdo.get_actor_scale3d()),
        'mesh':mesh.skeletal_mesh_asset.get_path_name(),'mesh_location':xyz(mesh.get_editor_property('relative_location')),
        'mesh_scale':xyz(mesh.get_editor_property('relative_scale3d')),'capsule_half_height':float(capsule.get_unscaled_capsule_half_height()),
        'capsule_radius':float(capsule.get_unscaled_capsule_radius()),
        'animation_class':str(mesh.get_editor_property('anim_class')),
        'materials':[m.get_path_name() for m in mesh.get_materials()]}
    for p in set([paths[1],R['character']['mesh'].split('.')[0]]):
        obj=U.EditorAssetLibrary.load_asset(p);check('skeletal mesh '+p,isinstance(obj,U.SkeletalMesh))
        mod=U.SkeletonModifier();check('read native skeleton '+p,mod.set_skeletal_mesh(obj))
        names=mod.get_all_bone_names()
        bones={}
        for name in names:
            s=str(name)
            if s in ('Head','Hips','LeftEye','RightEye','LeftFoot','RightFoot','LeftToes','RightToes') or any(x in s.lower() for x in ('hair_side_long','hair_front','skirt')):
                t=mod.get_bone_transform(name,True)
                bones[s]={'position_cm':xyz(t.translation),'scale':xyz(t.scale3d)}
        R['assets'].append({'path':p,'bones':bones,'bone_count':len(names),'bounds_read_api':hasattr(obj,'get_bounds')})
        if hasattr(obj,'get_bounds'):
            b=obj.get_bounds();R['assets'][-1]['bounds']={'origin':xyz(b.origin),'extent':xyz(b.box_extent)}
    halo=U.EditorAssetLibrary.load_asset(paths[2]);check('original halo static mesh',isinstance(halo,U.StaticMesh))
    R['halo_bounds']=str(halo.get_bounds())
    # Actual plugin outline inventory, used only as API/material inspection, not an assumed available asset.
    R['plugin_outline_assets']=[str(p) for p in U.EditorAssetLibrary.list_assets('/VRM4U/MaterialUtil',True,False) if 'outline' in str(p).lower()]
    R['source_outline']=[{'material':m['asset_path'],'shader':m.get('resolved_shader',{}).get('name'),
        'width':m['floats'].get('_OutlineWidth'),'fix_width':m['floats'].get('_OutlineFixWidth'),
        'width_mask':m['textures'].get('_OutlineWidthMask'),'color':m['vectors_colors'].get('_OutlineColor'),
        'outline_texture':m['textures'].get('_OutlineTex')}
        for m in json.loads((DOC/'research/SELESTIA_SOURCE_AUDIT.json').read_text(encoding='utf-8'))['materials']
        if '/lilToon/' in m['asset_path']]
    R['protected_after']=[protected(p) for p in paths]
    check('all protected asset bytes unchanged',R['protected_before']==R['protected_after'])
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:dump()
if R['status']!='PASS':raise RuntimeError(R.get('error','Hero revision probe failed'))
