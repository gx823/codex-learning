"""New Q skin candidate only: two child MIs and an independent duplicate BP.

Run by root's native author wrapper as QSkinCandidate. No existing package is
saved. Mean *source diffuse* luminance normalization is not final-pixel proof.
Both original and candidate must be rendered under identical corner lighting.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
A,E,B=U.EditorAssetLibrary,U.MaterialEditingLibrary,U.BlueprintEditorLibrary
OWNER='HarborCity_M5_VS2_QSkinCandidate'; KEY='HarborCityOwnedBy'
SOURCE_BP='/Game/HarborCity/M5VS2/NPC/AvatarSample_Q/Runtime_2c36ed20bf/BP_AvatarSample_Q'
SOURCE_BP_SHA='5a0945fa78758e253370e349781426934010a858f79b0cdd2ca4c4b5bc219d24'
SOURCE_ROOT='/Game/HarborCity/M5VS2/NPC/AvatarSample_Q/Source_36c131cef1f2'
AUDIT=DOC/'research/Q_SKIN_SOURCE_AUDIT_20260925.json'
AUDIT_SHA='6616a5cd8645a05a427b58f6b3ae1e5accaa9f888f38d7787b1d587520448ff0'

def arg(name):
    m=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(m)==1, name
    return m[0][0] or m[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve()
assert arg('M5AuthorPhase')=='QSkinCandidate'
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
DEST='/Game/HarborCity/M5VS2/NPCSkinCandidates/Q_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
R=dict(schema='HarborCity.M5VS2.QSkinCandidate.v1',status='RUNNING',phase='QSkinCandidate',
       checks=[],assets=[],inputs={},source_blueprint=SOURCE_BP,destination=DEST,
       scope='Two child skin MIs and an independent Q BP; original source/BP/ABP/PHYS/profile/other 16 slots unchanged.',
       runtime='NOT_RUN',visual='USER_REVIEW',started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       limitations=['Source diffuse weighted mean luminance normalization is not final rendered pixel luminance preservation.',
           'Face ShadeToony .9 to .75 is a bounded transition-softness art candidate, not a proven fix for every cast-shadow shape.',
           'Source deep brown skin identity, original normal/shadow maps and shadow receiving remain.',
           'No source texture is copied into the review package or edited. No old NPC BP is rebound.'])
PROTECTED={}; TREE_BEFORE={}

def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed))
    if not ok:
        dump(); raise RuntimeError(name+': '+repr(observed))
def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def pkg(obj):return (obj.get_path_name() if isinstance(obj,U.Object) else str(obj)).split('.')[0]
def disk(obj):
    p=pkg(obj)
    if p.startswith('/Game/'):
        return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
    assert p.startswith('/VRM4U/')
    return PROJECT/'Plugins/VRM4U/Content'/Path(p.removeprefix('/VRM4U/')).with_suffix('.uasset')
def protect(file,expected=None):
    file=Path(file).resolve();check('protected source exists',file.is_file(),str(file))
    h=sha(file);check('protected source byte identity',expected is None or h==expected,str(file));PROTECTED[str(file)]=h
def load(p):
    o=A.load_asset(p);check('native source load',o is not None,p);return o
def val(x):
    if x is None or isinstance(x,(bool,int,float,str)):return x
    if isinstance(x,U.Object):return x.get_path_name()
    if isinstance(x,U.LinearColor):return [float(x.r),float(x.g),float(x.b),float(x.a)]
    if isinstance(x,(list,tuple,U.Array)):return [val(v) for v in x]
    if isinstance(x,U.StructBase):return x.export_text()
    return str(x)
def near(a,b):
    if isinstance(a,dict):return isinstance(b,dict) and a.keys()==b.keys() and all(near(a[k],b[k]) for k in a)
    if isinstance(a,list):return isinstance(b,list) and len(a)==len(b) and all(near(x,y) for x,y in zip(a,b))
    if isinstance(a,(int,float)) and isinstance(b,(int,float)):return math.isfinite(a) and math.isfinite(b) and abs(a-b)<.00001
    return a==b
def params(mi):
    r={}
    for kind in ('scalar','vector','texture','static_switch'):
        names=list(getattr(E,'get_'+kind+'_parameter_names')(mi));check('bounded parameter inventory',len(names)<512)
        get=getattr(E,'get_material_instance_'+kind+'_parameter_value')
        r[kind]={str(n):val(get(mi,n)) for n in names}
    return r
def objects(bp):
    c=B.generated_class(bp);check('actual generated Blueprint class',c is not None)
    cdo=U.get_default_object(c);body=cdo.get_component_by_class(U.SkeletalMeshComponent)
    check('actual owned skeletal mesh component',body is not None and pkg(body)==pkg(bp),val(body))
    return cdo,body
def snapshot(cdo,body):
    reaction=cdo.get_component_by_class(U.HCM4R1PhysicalReactionComponent)
    check('existing physical reaction component',reaction is not None)
    cap=cdo.get_component_by_class(U.CapsuleComponent)
    keep={k:val(cdo.get_editor_property(k)) for k in ('npc_profile','stable_id','display_name','role_id','talkable',
        'stationary','dialogue_lines','walk_speed','panic_speed','hit_front_montage','hit_back_montage','hit_left_montage','hit_right_montage')}
    keep['body']={k:val(body.get_editor_property(k)) for k in ('skeletal_mesh_asset','anim_class','physics_asset_override',
        'relative_location','relative_rotation','relative_scale3d','animation_mode')}
    keep['capsule']={k:val(cap.get_editor_property(k)) for k in ('capsule_radius','capsule_half_height')}
    keep['physical_reaction']={k:val(reaction.get_editor_property(k)) for k in ('use_authored_get_up','get_up_pose_data_verified',
        'get_up_clips','get_up_chest_bone','get_up_chest_forward_bone_local','get_up_chest_right_bone_local',
        'get_up_authored_component_scale','minimum_down_seconds','stable_seconds','recovery_blend_seconds','emergency_recovery_seconds')}
    return dict(preserved=keep,materials=[val(body.get_material(i)) for i in range(body.get_num_materials())])
def save(obj):
    check('save only unique new candidate namespace',pkg(obj).startswith(DEST+'/'))
    A.set_metadata_tag(obj,KEY,OWNER);check('native new asset save',A.save_loaded_asset(obj,False),pkg(obj))
    R['assets'].append(dict(path=obj.get_path_name(),class_name=obj.get_class().get_name(),sha256=sha(disk(obj))))

try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE',not U.EditorLevelLibrary.get_pie_worlds(False))
    check('clean initial packages',not U.EditorLoadingAndSavingUtils.get_dirty_content_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('new unique destination',not A.does_directory_exist(DEST) and not disk(DEST+'/dummy').parent.exists())
    for root in (PROJECT/'Content/HarborCity/M5VS2/NPC',PROJECT/'Plugins/VRM4U/Content/MaterialUtil'):
        files=sorted((p.resolve() for p in root.rglob('*') if p.is_file()),key=str)
        check('existing protected tree',bool(files),str(root));TREE_BEFORE[str(root)]=[str(p) for p in files]
        for f in files:protect(f)
    protect(disk(SOURCE_BP),SOURCE_BP_SHA);protect(AUDIT,AUDIT_SHA)
    # A fresh commandlet has not necessarily indexed plugin content. Explicitly
    # scan the two existing, already byte-protected roots before EAL.LoadAsset.
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        ['/VRM4U/MaterialUtil','/Game/HarborCity/M5VS2/NPC/AvatarSample_Q'],force_rescan=True)
    data=json.loads(AUDIT.read_text('utf-8-sig'));protect(data['source_vrm'],data['source_sha256'])
    check('source identity',data['source_sha256']=='36c131cef1f25e9a75825dfac26f14cfdb757123f6d83331b7ebfe0c146f0f90')
    R['inputs']['source_audit']=dict(path=str(AUDIT),sha256=sha(AUDIT))
    # Verify the real high/default MToon parameter-to-linear path used in the analysis.
    fn=load('/VRM4U/MaterialUtil/MToonUtil/MF_LinearColorConvert')
    ns={n.get_name():n for n in E.get_material_function_expressions(fn)}
    power=ns['MaterialExpressionPower_9']
    actual_exponent=float(power.get_editor_property('const_exponent'))
    edges=dict(zip([str(n) for n in E.get_material_expression_input_names(power)],
                   [n.get_name() if n else None for n in E.get_inputs_for_material_function_expression(fn,power)]))
    check('native MToon parameter gamma branch exactly matches analysis',isinstance(power,U.MaterialExpressionPower)
          and abs(actual_exponent-data['gamma_parameter_exponent_expected'])<.000001
          and edges=={'Base':'MaterialExpressionReroute_58','Exp':None},dict(exponent=actual_exponent,edges=edges))
    src=load(SOURCE_BP);check('original integrated Q ownership',A.get_metadata_tag(src,KEY)=='HarborCity_M5_VS2_NPC_Integration')
    cdo,body=objects(src);before=snapshot(cdo,body);R['source_cdo']=before
    check('exact actual Q 18 slots',len(before['materials'])==18)
    children={}
    for row,index in zip(data['rows'],(3,7)):
        part=row['part'];name='MI_N00_000_00_'+part+'_00_SKIN__Instance_';parent=load(SOURCE_ROOT+'/'+name)
        check('exact current actual skin slot',body.get_material(index)==parent,dict(part=part,index=index))
        original=params(parent);check('source gamma mode',original['scalar'].get('bVRM10Mode')==0)
        for pname,normalization in row['normalization'].items():
            check('actual source vector equals normalization input',near(original['vector'][pname],normalization['source_parameter']),pname)
            check('diffuse branch weighted mean luminance retained',abs(normalization['source_mean_linear_y']-normalization['candidate_mean_linear_y'])<1e-12)
        expected=json.loads(json.dumps(original))
        child=U.AssetToolsHelpers.get_asset_tools().create_asset('MI_Q_'+part+'_WarmY',DEST,U.MaterialInstanceConstant,U.MaterialInstanceConstantFactoryNew())
        check('new independent child material',isinstance(child,U.MaterialInstanceConstant))
        E.set_material_instance_parent(child,parent)
        for pname,newvalue in row['parameters'].items():
            E.set_material_instance_vector_parameter_value(child,pname,U.LinearColor(*newvalue));expected['vector'][pname]=newvalue
        if part=='Face':
            check('known face toon transition baseline',abs(original['scalar']['mtoon_ShadeToony']-.9)<.000001)
            E.set_material_instance_scalar_parameter_value(child,'mtoon_ShadeToony',.75);expected['scalar']['mtoon_ShadeToony']=.75
        E.update_material_instance(child)
        raw=child.get_editor_property('base_property_overrides').export_text()
        flags=re.findall(r'(bOverride_[A-Za-z0-9_]+)=(True|False)',raw)
        check('no active base property overrides',bool(flags) and all(v=='False' for _,v in flags),raw)
        check('child exact source parent and only intended parameter differences',child.get_editor_property('parent')==parent and near(params(child),expected),part)
        save(child);children[index]=child
        R.setdefault('candidates',[]).append(dict(part=part,slot=index,source=parent.get_path_name(),candidate=child.get_path_name(),
            expected_parameters=expected,source_parameters=original,source_normalization=row['normalization']))
    bp=A.duplicate_asset(SOURCE_BP,DEST+'/BP_Q_SkinCandidate');check('duplicate only to new BP',isinstance(bp,U.Blueprint))
    check('new duplicate compiles',B.compile_blueprint(bp));ncdo,nbody=objects(bp);candidate_before=snapshot(ncdo,nbody)
    check('duplicate keeps all source CDO configuration',near(candidate_before,before))
    ncdo.modify();nbody.modify()
    for index,mi in children.items():nbody.set_material(index,mi)
    expected_materials=list(before['materials'])
    for index,mi in children.items():expected_materials[index]=mi.get_path_name()
    after=snapshot(ncdo,nbody);check('only two skin references differ',near(after['preserved'],before['preserved']) and after['materials']==expected_materials)
    check('new BP compile after override',B.compile_blueprint(bp));save(bp)
    rcdo,rbody=objects(load(bp.get_path_name()));readback=snapshot(rcdo,rbody)
    check('saved native CDO readback',near(readback,after))
    for row in R['candidates']:
        check('saved candidate exact parameter readback',near(params(load(row['candidate'])),row['expected_parameters']))
    check('original Q CDO still unchanged',near(snapshot(*objects(src)),before))
    R['candidate_cdo']=readback;R['candidate_blueprint']=bp.get_path_name()
    R['candidate_generated_class']=B.generated_class(bp).get_path_name()
    R['source_specimen_references']=dict(mesh=before['preserved']['body']['skeletal_mesh_asset'],
        profile=before['preserved']['npc_profile'],anim_class=before['preserved']['body']['anim_class'],
        physics=before['preserved']['body']['physics_asset_override'],capsule_half_height_cm=before['preserved']['capsule']['capsule_half_height'])
    R['status']='PASS'
    R['pass_scope']='Native isolated candidate author/save/CDO and parameter readback only. Original corner actors still use original Q; rendered A/B NOT_RUN.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    tree_changes=[p for p,files in TREE_BEFORE.items() if sorted((str(x.resolve()) for x in Path(p).rglob('*') if x.is_file()))!=files]
    R['preservation']=dict(changed_source_files=changed,source_tree_changes=tree_changes,
        source_sha256_before=PROTECTED,no_original_package_save=True)
    if changed or tree_changes:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Q skin candidate author failed; inspect author_result.json')
