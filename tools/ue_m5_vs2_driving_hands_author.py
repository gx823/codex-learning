"""One private native-vehicle child with current legal Selestia arms. No map/UI launch.
DrivingHandsProbe / DrivingHandsApply / DrivingHandsReload. Old cars untouched.
"""
from pathlib import Path
import ast,datetime as dt,hashlib,json,re,traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary
SCHEMA='HarborCity.M5VS2.DrivingHands.v1';OWNER='HarborCity_M5VS2_DrivingHands'
def arg(name):
    m=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(m)==1,name
    return m[0][0] or m[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC/'editor_runtime') and PHASE in ('DrivingHandsProbe','DrivingHandsApply','DrivingHandsReload') and not(OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True);TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12];ROOT='/Game/HarborCity/M5VS2/DrivingHands/Review_'+TOKEN
R=dict(schema=SCHEMA,owner=OWNER,phase=PHASE,status='RUNNING',checks=[],inputs={},assets=[],runtime='NOT_RUN',visual='USER_REVIEW',os_input='NOT_RUN',map_writes=0,old_asset_writes=0,
       fresh_process_disk_readback='NOT_RUN',scope='One opt-in private child of unchanged native AHCM1Vehicle. Current imported Selestia arms and exact named finger-pose source. No map, camera, vehicle physics or input changes.',started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={}
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:dump();raise RuntimeError(name+': '+str(actual))
def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def path(obj):return obj.get_path_name() if obj is not None else None
def package(obj):return(path(obj) if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def disk(obj):
    p=package(obj);assert re.fullmatch(r'/Game/[A-Za-z0-9_/]+',p)
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.uasset')
def protect(file,expected=None):
    f=Path(file).resolve();check('owned source input',f.is_relative_to(WORK) and f.is_file(),str(f));h=sha(f);check('source SHA matches actual prerequisite',expected is None or h==expected,str(f));PROTECTED[str(f)]=h;return dict(path=str(f),sha256=h,bytes=f.stat().st_size)
def document(file):return json.loads(Path(file).read_text('utf-8-sig'))
def report(file,expected=None):
    f=Path(file).resolve();check('owned evidence',f.is_relative_to(DOC/'editor_runtime'));protect(f,expected);d=document(f);cf=f.parent/'commandlet.json';protect(cf);c=document(cf)
    check('actual native report/process PASS',d.get('status')=='PASS' and all(x.get('status')=='PASS' for x in d.get('checks',[])) and c.get('status')=='PASS' and c.get('exit_code')==0,str(f));return d
def latest(phase):
    for f in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda f:f.stat().st_mtime,reverse=True):
        d=document(f)
        if d.get('schema')==SCHEMA and d.get('phase')==phase and d.get('status')=='PASS':return f,report(f)
    raise RuntimeError('No actual '+phase+' PASS')
def load(p):
    o=A.load_asset(package(p));check('actual native asset load',o is not None,p);protect(disk(o));return o
def install_readers():
    f=WORK/'tools/ue_m5_vs2_corner_playable_r4_author.py';protect(f);s=f.read_text('utf-8-sig');t=ast.parse(s)
    names=('xyz','transform','rounded','component_snapshot','vehicle_record');nodes=[n for n in t.body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact existing readonly native vehicle helpers',tuple(n.name for n in nodes)==names);snippet='\n\n'.join(ast.get_source_segment(s,n) for n in nodes)
    R['vehicle_reader_source']=dict(path=str(f),functions=list(names),sha256=hashlib.sha256(snippet.encode()).hexdigest());exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),globals())
    f=WORK/'tools/ue_m5_vs2_hero_rev2_integration_author.py';protect(f);s=f.read_text('utf-8-sig');t=ast.parse(s);nodes=[n for n in t.body if isinstance(n,ast.FunctionDef) and n.name=='scs_components'];check('existing readonly SCS helper',len(nodes)==1);exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),globals())
def source():
    sf=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json';R['inputs']['selection']=protect(sf);s=document(sf);check('explicit current integrated Hero selection',s.get('status')=='SELECTED_FOR_REVIEW')
    refs=[x for x in s['source_reports'] if Path(x['path']).parent.name.endswith('_HeroRev2IntegrationReload')];check('one actual fresh integration source',len(refs)==1);d=report(refs[0]['path'],refs[0]['sha256'])
    check('actual selected Hero original readback',d.get('fresh_process_disk_readback')=='PASS' and d['blueprint']==s['blueprint'])
    for row in d['assets']:protect(disk(row['path']),row['sha256'])
    bp=load(d['blueprint']);hero=U.get_default_object(bp.generated_class());read=d['native_readback'];arms=load(read['arms']);pose=load(read['preserved']['presentation']['relaxed_finger_pose']);body=hero.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset');protect(disk(body))
    for i in range(hero.get_editor_property('mesh').get_num_materials()):
        material=hero.get_editor_property('mesh').get_material(i);check('actual selected Hero material exists',material is not None,i);protect(disk(material))
    for obj in (arms.get_editor_property('skeleton'),pose.get_editor_property('skeleton')):protect(disk(obj))
    R['source']=dict(driver_blueprint=package(bp),body=package(body),arms=package(arms),finger_pose=package(pose),finger_source_scope='Existing imported .28s finger locals only. Different skeleton pointer requires native exact named local reference basis; no whole-animation retarget assumption.')
    result=json.loads(U.HCM5VS2DrivingHandsEditor.probe_driver_source(hero,arms,pose));R['native_source_probe']=result
    check('actual native source/hand hierarchy/material/finger basis valid',result.get('status')=='PASS' and result.get('assets_mutated') is False,result)
    return hero,arms,pose
def baseline(cdo):
    d=vehicle_record(cdo);d.pop('class_name');return d
def inspect(bp,expected):
    check('private class directly inherits unchanged native vehicle',B.get_blueprint_parent_class(bp)==U.HCM1Vehicle.static_class())
    cdo=U.get_default_object(bp.generated_class());actual=baseline(cdo);check('all original recorded vehicle geometry/physics/movement values unchanged',actual==expected)
    components=[c for c in scs_components(bp) if isinstance(c,U.HCM5VS2DrivingHandsComponent)];check('one opt-in native hands component',len(components)==1)
    component=components[0];bindings={k:path(component.get_editor_property(k)) for k in ('arms_override','grip_finger_pose')}
    check('candidate binds exact approved source',package(bindings['arms_override'])==R['source']['arms'] and package(bindings['grip_finger_pose'])==R['source']['finger_pose'],bindings)
    return dict(blueprint=package(bp),generated_class=path(bp.generated_class()),vehicle=actual,bindings=bindings)
try:
    check('correct native project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty content/maps or PIE',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    install_readers()
    if PHASE=='DrivingHandsReload':
        f,prior=latest('DrivingHandsApply');R['inputs']['apply']=protect(f);ROOT=prior['destination'];check('exact unique private vehicle destination',re.fullmatch(r'/Game/HarborCity/M5VS2/DrivingHands/Review_[0-9a-f]{12}',ROOT) is not None)
        for p,h in prior['preservation']['source_sha256_before'].items():protect(p,h)
        for row in prior['assets']:protect(disk(row['path']),row['sha256'])
        R['source']=prior['source'];bp=load(prior['blueprint']);actual=inspect(bp,prior['native_vehicle_baseline']);check('fresh native CDO/SCS matches authored evidence',actual==prior['native_readback'])
        hero=U.get_default_object(load(R['source']['driver_blueprint']).generated_class());probe=json.loads(U.HCM5VS2DrivingHandsEditor.probe_driver_source(hero,load(R['source']['arms']),load(R['source']['finger_pose'])))
        check('fresh actual source basis/material probe identical',probe==prior['native_source_probe'])
        R.update(status='PASS',fresh_process_disk_readback='PASS',destination=ROOT,blueprint=prior['blueprint'],native_readback=actual,native_source_probe=probe,native_vehicle_baseline=prior['native_vehicle_baseline'],assets=prior['assets'])
    else:
        protect(__file__);protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll')
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/('HCM5VS2DrivingHandsComponent'+ext))
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M1'/('HCM1Vehicle'+ext))
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M4R2'/('HCM4R2CockpitComponent'+ext))
        for f in (PROJECT/'Config').glob('*.ini'):protect(f)
        for subtree in ('SportsCar','VehicleTemplate','HarborCity/M4R2/Interior'):
            for f in (PROJECT/'Content'/subtree).rglob('*.uasset'):protect(f)
        hero,arms,pose=source();base=baseline(U.get_default_object(U.HCM1Vehicle.static_class()));R['native_vehicle_baseline']=base
        if PHASE=='DrivingHandsProbe':R.update(status='PASS',pass_scope='Read-only native source/basis/provenance. Vehicle seat fit, arms IK, contact and runtime remain NOT_RUN.')
        else:
            probe_file,probe=latest('DrivingHandsProbe');R['inputs']['probe']=protect(probe_file);check('separate actual Probe same native source and vehicle',probe['source']==R['source'] and probe['native_source_probe']==R['native_source_probe'] and probe['native_vehicle_baseline']==base)
            check('Probe preserved original sources',probe['preservation']['changed_source_files']==[])
            for p,h in probe['preservation']['source_sha256_before'].items():protect(p,h)
            check('unique new vehicle namespace absent',not A.does_directory_exist(ROOT))
            factory=U.BlueprintFactory();factory.set_editor_property('parent_class',U.HCM1Vehicle.static_class());bp=U.AssetToolsHelpers.get_asset_tools().create_asset('BP_VehicleDrivingHands',ROOT,U.Blueprint,factory);check('new direct native vehicle child',bp is not None)
            native=json.loads(U.HCM5VS2DrivingHandsEditor.configure_private_vehicle(bp,arms,pose));R['native_author']=native;check('native opt-in SCS configured and compiled',native.get('status')=='PASS' and native.get('saved_by_helper') is False,native)
            before=inspect(bp,base);A.set_metadata_tag(bp,'HarborCityOwnedBy',OWNER);check('save only new private vehicle',A.save_loaded_asset(bp,False));after=inspect(bp,base);check('same-process saved vehicle readback',after==before)
            R.update(status='PASS',destination=ROOT,blueprint=package(bp),native_readback=after,assets=[dict(path=package(bp),sha256=sha(disk(bp)))],pass_scope='One private vehicle BP created. Old vehicle assets unchanged. Fresh Reload, runtime contact/visibility/naturalness remain NOT_RUN.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h];R['preservation']=dict(source_sha256_before=PROTECTED,changed_source_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Private driving-hands author failed; preserve partial private asset and failure, no source rollback.')
