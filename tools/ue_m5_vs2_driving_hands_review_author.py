"""Private Afternoon/Night key-driven review maps. Old Live/native car guarded intact.
DrivingHandsReviewAuthor -> separate DrivingHandsReviewReload. No process/UI launch.
"""
from pathlib import Path
import ast,datetime as dt,hashlib,json,re,traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary;LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem);ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
SCHEMA='HarborCity.M5VS2.DrivingHandsReview.Author.v1';OWNER='HarborCity_M5VS2_DrivingHandsReview';PERIODS=('Afternoon','Night')
MODULE=PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll'
def arg(name):
    m=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(m)==1,name
    return m[0][0] or m[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC/'editor_runtime') and PHASE in ('DrivingHandsReviewAuthor','DrivingHandsReviewReload') and not(OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True);TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12];ROOT='/Game/HarborCity/M5VS2/DrivingHandsReview/Run_'+TOKEN
R=dict(schema=SCHEMA,owner=OWNER,phase=PHASE,status='RUNNING',checks=[],inputs={},assets=[],maps={},module_transitions=[],runtime='NOT_RUN',visual='USER_REVIEW',os_input='NOT_RUN',fresh_process_disk_readback='NOT_RUN',kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED',input_scope='ENGINE_KEYS_MOUSE_NOT_OS',camera_scope='NORMAL_PLAYER_CAMERA_NO_POSE_SETTERS',expected_screenshots=9,scope='Independent copy of current R5 Live corner; one private vehicle candidate and safety-layer Hero, door-side start, empty existing Experience for ordinary Q. No old map edits, no quest/NPC combat acceptance.',started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={};BINDINGS={}
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:dump();raise RuntimeError(name+': '+str(actual))
def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def path(obj):return obj.get_path_name() if obj is not None else None
def package(obj):return(path(obj) if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def disk(obj):
    p=package(obj);assert re.fullmatch(r'/Game/[A-Za-z0-9_/]+',p)
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.umap' if p.rsplit('/',1)[-1].startswith('L_') else '.uasset')
def kind(file):
    f=Path(file).resolve()
    for root,label in ((PROJECT/'Content','package'),(PROJECT/'Binaries/Win64','module'),(PROJECT/'Source','cpp'),(PROJECT/'Config','config'),(WORK/'tools','script'),(DOC,'evidence'),(Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups'),'backup')):
        if f.is_relative_to(root):return label
    raise RuntimeError('Source outside exact project roots: '+str(f))
def binding(file):
    f=Path(file).resolve();return dict(path=str(f),sha256=sha(f),bytes=f.stat().st_size,kind=kind(f))
def protect(file,expected=None):
    row=binding(file);check('unchanged source input SHA',expected is None or row['sha256']==expected,row['path']);PROTECTED[row['path']]=row['sha256'];BINDINGS[row['path']]=row;return row
def inherited(file,expected,origin):
    f=Path(file).resolve()
    if f==MODULE.resolve() and sha(f)!=expected:
        R['module_transitions'].append(dict(path=str(f),historical_author_sha256=expected,current_diagnostic_sha256=sha(f),source_report=str(origin),reason='New review-only class linked after historical real asset-author PASS; all assets/helper source/config remain SHA-identical. Current native Probe/Inspect/CDO/SCS must match again.'))
        protect(f)
    else:protect(f,expected)
def document(file):return json.loads(Path(file).read_text('utf-8-sig'))
def report(file,expected=None):
    f=Path(file).resolve();check('owned source report',f.is_relative_to(DOC/'editor_runtime'));protect(f,expected);d=document(f);cf=f.parent/'commandlet.json';protect(cf);c=document(cf)
    check('actual native source and process PASS',d.get('status')=='PASS' and bool(d.get('checks')) and all(x.get('status')=='PASS' for x in d['checks']) and c.get('status')=='PASS' and c.get('exit_code')==0,str(f));return d
def latest(phase,schema):
    for f in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda x:x.stat().st_mtime,reverse=True):
        d=document(f)
        if d.get('schema')==schema and d.get('phase')==phase and d.get('status')=='PASS':return f,report(f)
    raise RuntimeError('No actual '+phase+' PASS')
def assets(data):
    for row in data['assets']:protect(disk(row['path']),row['sha256'])
def load(p):
    obj=A.load_asset(package(p));check('actual native load',obj is not None,p);return obj
def cls(p):
    p=package(p);c=U.load_class(None,p+'.'+p.rsplit('/',1)[-1]+'_C');check('actual generated class',c is not None,p);return c
def readers():
    f=WORK/'tools/ue_m5_vs2_corner_playable_r4_author.py';protect(f);s=f.read_text('utf-8-sig');names=('xyz','transform','rounded','component_snapshot','actor_record','vehicle_record');nodes=[n for n in ast.parse(s).body if isinstance(n,ast.FunctionDef) and n.name in names];check('six existing readonly world/car readers',tuple(n.name for n in nodes)==names);exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),globals())
    ns=dict(U=U,check=check,path=path,package=package);f=WORK/'tools/ue_m5_vs2_hero_rev2_integration_author.py';protect(f);s=f.read_text('utf-8-sig');names=('vec','quat','transform','value','props','cdo_objects','preserved','scs_components');nodes=[n for n in ast.parse(s).body if isinstance(n,ast.FunctionDef) and n.name in names];check('eight exact readonly Hero readers',tuple(n.name for n in nodes)==names);exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),ns)
    f=WORK/'tools/ue_m5_vs2_hero_modesty_author.py';protect(f);s=f.read_text('utf-8-sig');nodes=[n for n in ast.parse(s).body if isinstance(n,ast.FunctionDef) and n.name=='baseline'];check('one existing Hero baseline reader',len(nodes)==1);exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),ns);return ns
def candidate_sources(ns,refs=None):
    source={}
    for key,phase,schema in (('hands','DrivingHandsReload','HarborCity.M5VS2.DrivingHands.v1'),('modesty','HeroModestyReload','HarborCity.M5VS2.HeroModesty.v1')):
        if refs:f=Path(refs[key]['path']);d=report(f,refs[key]['sha256'])
        else:f,d=latest(phase,schema)
        check('exact fresh immutable candidate prerequisite',d['schema']==schema and d['phase']==phase and d['fresh_process_disk_readback']=='PASS' and d['preservation']['changed_source_files']==[])
        for p,h in d['preservation']['source_sha256_before'].items():inherited(p,h,f)
        assets(d);source[key]=d;R['inputs'][key]=binding(f)
    h,m=source['hands'],source['modesty'];check('safety Hero is derived from original exact hands driver source',m['source_blueprint']==h['source']['driver_blueprint'])
    hero_bp=load(m['blueprint']);hero=U.get_default_object(hero_bp.generated_class());base=ns['baseline'](hero_bp);check('all Hero baseline bindings unchanged',base==m['source_baseline'] and base==ns['baseline'](load(m['source_blueprint'])))
    components=[c for c in ns['scs_components'](hero_bp) if isinstance(c,U.HCM5VS2HeroModestyComponent)];check('one exact safety-layer SCS binding',len(components)==1 and ns['props'](components[0],('safety_shorts','cloth_material'))==m['native_readback']['component'])
    topo=json.loads(U.HCM5VS2HeroModestyEditor.inspect_safety_shorts(load(m['source_body']),load(m['shorts'])));check('current module original topology readback exact',topo.get('status')=='PASS' and topo==m['native_readback']['topology'])
    probe=json.loads(U.HCM5VS2DrivingHandsEditor.probe_driver_source(hero,load(h['source']['arms']),load(h['source']['finger_pose'])));check('current module original hand source Probe exact',probe.get('status')=='PASS' and probe==h['native_source_probe'])
    car_bp=load(h['blueprint']);check('exact native vehicle parent getter',B.get_blueprint_parent_class(car_bp)==U.HCM1Vehicle.static_class());car=U.get_default_object(car_bp.generated_class());actual=vehicle_record(car);actual.pop('class_name');check('all actual vehicle CDO parameters remain original',actual==h['native_readback']['vehicle'])
    parts=[c for c in ns['scs_components'](car_bp) if isinstance(c,U.HCM5VS2DrivingHandsComponent)];check('one exact driving-hands SCS source',len(parts)==1 and ns['props'](parts[0],('arms_override','grip_finger_pose'))==h['native_readback']['bindings'])
    R['candidate']=dict(hero=m['blueprint'],vehicle=h['blueprint'],source_hero=m['source_blueprint'],hero_baseline=base,shorts=m['shorts'],cloth=m['material'],hands_probe=probe,topology=topo,vehicle_baseline=actual)
    return hero_bp.generated_class(),car_bp.generated_class()
def world():return U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
def actors():return[a for a in ACT.get_all_level_actors() if a.get_class().get_name() not in ('WorldSettings','Brush','DefaultPhysicsVolume')]
def snapshot():
    aa=actors();ds=[a for a in aa if isinstance(a,U.HCM5VS2DrivingHandsReviewDirector)];cars=[a for a in aa if isinstance(a,U.HCM1Vehicle)];es=[a for a in aa if isinstance(a,U.HCM3Experience)];ts=[a for a in aa if isinstance(a,U.HCM5VS2CornerTimeDirector)]
    check('one exact review/car/empty-experience/time',len(ds)==len(cars)==len(es)==len(ts)==1);e=es[0];check('no Experience appearance or NPC override',not e.get_editor_property('npcs') and e.get_editor_property('player_appearance_mesh') is None and e.get_editor_property('player_animation_class') is None)
    forbidden=[a.get_class().get_name() for a in aa if isinstance(a,U.CameraActor) or ('ReviewDirector' in a.get_class().get_name() and a!=ds[0])];check('no automatic/fixed camera fixtures',not forbidden,forbidden)
    mode=world().get_world_settings().get_editor_property('default_game_mode');row={k:path(ds[0].get_editor_property(k)) for k in ('expected_character_class','expected_vehicle_class','expected_body','expected_animation_class')};row['expected_materials']=[path(m) for m in ds[0].get_editor_property('expected_materials')];row['expected_period']=str(ds[0].get_editor_property('expected_period'))
    return rounded(dict(actors=sorted([actor_record(a) for a in aa],key=lambda x:x['label']),game_mode=path(mode),pawn=path(U.get_default_object(mode).get_editor_property('default_pawn_class')),director=row,vehicle=vehicle_record(cars[0]),period=str(ts[0].get_editor_property('initial_period')),experience=dict(npcs=[],appearance=None,animation=None,coffee_mesh=path(e.get_editor_property('coffee_parcel').get_editor_property('static_mesh')))))
def save(obj):
    p=package(obj);check('save only new private package',p.startswith(ROOT+'/'));A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER);check('native private save',A.save_loaded_asset(obj,False));R['assets'].append(dict(path=p,sha256=sha(disk(p))))
try:
    check('correct project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty content/maps/PIE',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages());ns=readers()
    if PHASE=='DrivingHandsReviewAuthor':
        protect(__file__);protect(WORK/'tools/launch_m5_vs2_driving_hands_review.ps1');protect(MODULE)
        for name in ('HCM5VS2DrivingHandsComponent','HCM5VS2DrivingHandsReviewDirector','HCM5VS2HeroModestyComponent','HCM5VS2CornerTimeDirector'):
            for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/(name+ext))
        hero,car_class=candidate_sources(ns);sf,source=latest('CornerPlayableR5Reload','HarborCity.M5VS2.CornerPlayableR5.v1');check('current R5 Live native fresh readback',source['fresh_process_disk_readback']=='PASS' and source['preservation']['changed_files']==[]);assets(source)
        mf=Path(source['launch_manifest']['path']);protect(mf,source['launch_manifest']['sha256']);manifest=document(mf)
        for row in manifest['source_bindings']:inherited(row['path'],row['sha256'],sf)
        R['inputs']['live_reload']=binding(sf);check('absent new review destination',not A.does_directory_exist(ROOT));check('load original source read-only',LEVEL.load_level(source['maps']['Afternoon']['package']))
        source_mode=world().get_world_settings().get_editor_property('default_game_mode');protect(disk(source_mode));gm=A.duplicate_asset(package(source_mode),ROOT+'/BP_DrivingHandsReviewMode');check('private GameMode',gm is not None);U.get_default_object(gm.generated_class()).set_editor_property('default_pawn_class',hero);B.compile_blueprint(gm);save(gm)
        for period in PERIODS:
            src=source['maps'][period]['package'];target=ROOT+'/L_DrivingHands_'+period;check('load source period map readonly',LEVEL.load_level(src));old=actors();cars=[a for a in old if isinstance(a,U.HCM1Vehicle)];starts=[a for a in old if isinstance(a,U.PlayerStart)];check('one original native car and start',len(cars)==len(starts)==1 and cars[0].get_class()==U.HCM1Vehicle.static_class())
            removed={cars[0].get_actor_label(),starts[0].get_actor_label()};keep=sorted([actor_record(a) for a in old if a.get_actor_label() not in removed],key=lambda x:x['label']);old_car=cars[0].get_actor_transform();car_label=cars[0].get_actor_label();start_label=starts[0].get_actor_label()
            check('create independent private map',LEVEL.new_level_from_template(target,src));old=actors();copied=[a for a in old if a.get_actor_label()==car_label];check('exact copied car replaced only',len(copied)==1 and ACT.destroy_actor(copied[0]))
            car=ACT.spawn_actor_from_class(car_class,old_car.translation,old_car.rotation.rotator(),False);check('candidate car native spawn',car is not None);car.set_actor_label('HC_DrivingHands_Candidate');car.set_editor_property('stable_id',U.Name('VS2_DrivingHands_Car'));car.set_editor_property('tags',[U.Name(OWNER)])
            start=[a for a in actors() if a.get_actor_label()==start_label][0];door=car.get_interaction_location();right=car.get_actor_right_vector();cap=U.get_default_object(hero).get_component_by_class(U.CapsuleComponent);pos=U.Vector(door.x-right.x*100,door.y-right.y*100,cap.get_scaled_capsule_half_height()+4);start.set_actor_location(pos,False,False);start.set_actor_rotation(U.MathLibrary.find_look_at_rotation(pos,U.Vector(door.x,door.y,pos.z)),False)
            e=ACT.spawn_actor_from_class(U.HCM3Experience,U.Vector(0,0,-1000),U.Rotator(),False);check('existing empty combat gate fixture',e is not None);e.set_actor_label('HC_DrivingHands_EmptyExperience');e.set_editor_property('npcs',[]);e.set_editor_property('player_appearance_mesh',None);e.set_editor_property('player_animation_class',None);e.get_editor_property('coffee_parcel').set_static_mesh(None);e.get_editor_property('parking_marker').set_static_mesh(None);e.set_actor_hidden_in_game(True)
            director=ACT.spawn_actor_from_class(U.HCM5VS2DrivingHandsReviewDirector,U.Vector(0,0,-1500),U.Rotator(),False);check('new native input review director',director is not None);director.set_actor_label('HC_DrivingHands_Review');body=U.get_default_object(hero).get_editor_property('mesh')
            for k,v in dict(expected_character_class=hero,expected_vehicle_class=car_class,expected_body=body.get_editor_property('skeletal_mesh_asset'),expected_animation_class=body.get_editor_property('anim_class'),expected_materials=[body.get_material(i) for i in range(body.get_num_materials())],expected_period=U.Name(period)).items():director.set_editor_property(k,v)
            world().get_world_settings().set_editor_property('default_game_mode',gm.generated_class());A.set_metadata_tag(world(),'HarborCityOwnedBy',OWNER)
            excluded={start_label,car.get_actor_label(),e.get_actor_label(),director.get_actor_label()};actual_keep=sorted([actor_record(a) for a in actors() if a.get_actor_label() not in excluded],key=lambda x:x['label']);check('every retained Live actor unchanged incl eight NPCs/light/flight',actual_keep==keep)
            before=snapshot();check('private bindings/period native exact',before['pawn']==path(hero) and before['period']==period and before['director']['expected_period']==period);check('only new map save',LEVEL.save_current_level());R['assets'].append(dict(path=target,sha256=sha(disk(target))));check('same process new map reload',LEVEL.load_level(target));actual=snapshot();check('full native map exact after save/reload',actual==before);R['maps'][period]=dict(package=target,native_readback=actual)
        R.update(status='PASS',destination=ROOT,source_bindings=list(BINDINGS.values()),pass_scope='Private maps and GM saved; runtime/notional full steering limit/contact/OS/packaging NOT_RUN.')
    else:
        f,prior=latest('DrivingHandsReviewAuthor',SCHEMA);ROOT=prior['destination'];check('exact new map contract',re.fullmatch(r'/Game/HarborCity/M5VS2/DrivingHandsReview/Run_[0-9a-f]{12}',ROOT) is not None and set(prior['maps'])==set(PERIODS) and len(prior['assets'])==3)
        for row in prior['source_bindings']:protect(row['path'],row['sha256'])
        assets(prior);candidate_sources(ns,prior['inputs']);check('same candidate current native readbacks',R['candidate']==prior['candidate'])
        for period,row in prior['maps'].items():
            check('exact private period map',row['package']==ROOT+'/L_DrivingHands_'+period);check('fresh new map disk load',LEVEL.load_level(row['package']));actual=snapshot();check('fresh full native CDO/map/fixture matches',actual==row['native_readback'],period);R['maps'][period]=dict(package=row['package'],native_readback=actual)
        R.update(status='PASS',fresh_process_disk_readback='PASS',destination=ROOT,assets=prior['assets'],source_author=protect(f),source_bindings=list(BINDINGS.values()),pass_scope='Fresh native read-only persistence, not runtime or art.')
        manifest={k:R[k] for k in ('schema','owner','kind','input_scope','camera_scope','expected_screenshots','destination','candidate','source_bindings','scope')};manifest.update(status='READY_FOR_DRIVING_HANDS_NOT_RUNTIME_TESTED',maps={k:v['package'] for k,v in R['maps'].items()},reload_report=str(OUT/'author_result.json'),save_slot_prefix='HarborCity_VS2_DrivingHands_Test_',runtime_flag='-M5VS2DrivingHandsReview');mf=OUT/'driving_hands_review_manifest.json';mf.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8');R['launch_manifest']=binding(mf)
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h];R['preservation']=dict(changed_files=changed,source_sha256_before=PROTECTED)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Private driving-hands review failed. Keep partial private assets and old failure history.')
