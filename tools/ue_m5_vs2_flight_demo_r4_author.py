"""Private flight demo map. ENGINE_INPUT_NOT_OS. No runtime or author launch here.
Deploy beside the existing author wrapper only after explicit root review.
FlightDemoR4 creates one new map; FlightDemoR4Reload is a fresh read-only process.
"""
from pathlib import Path
import ast, datetime as dt, hashlib, json, math, re, traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习').resolve(); PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary; LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem); ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
OWNER='HarborCity_M5VS2_FlightDemoR4'; SCHEMA='HarborCity.M5VS2.FlightDemoR4.Author.v1'
LIVE_SCHEMA='HarborCity.M5VS2.CornerPlayableR4.v1'
CMD=U.SystemLibrary.get_command_line()
def arg(name):
    m=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',CMD);assert len(m)==1,name
    return m[0][0] or m[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve(); PHASE=arg('M5AuthorPhase')
assert PHASE in ('FlightDemoR4','FlightDemoR4Reload') and OUT.is_relative_to(DOC/'editor_runtime')
assert not (OUT/'author_result.json').exists(); OUT.mkdir(parents=True,exist_ok=True)
TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12]; ROOT='/Game/HarborCity/M5VS2/FlightDemoR4/Run_'+TOKEN
R=dict(schema=SCHEMA,owner=OWNER,phase=PHASE,status='RUNNING',checks=[],assets=[],inputs={},runtime='NOT_RUN',os_input='NOT_RUN',
       input_scope='ENGINE_INPUT_NOT_OS',kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED',art='USER_REVIEW',fresh_process_disk_readback='NOT_RUN',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),scope='Private copy of current native PASS Live R4 Afternoon; ordinary production input path driven by explicit test director. Not packaged, not OS mouse evidence, no combat/dialogue validation.')
PROTECTED={}; SOURCES={}
def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:dump();raise RuntimeError(name+': '+str(actual))
def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def package(obj):return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def path(obj):return obj.get_path_name() if obj is not None else None
def disk(obj):
    p=package(obj);check('bounded game package',p.startswith('/Game/') and re.fullmatch(r'[A-Za-z0-9_/]+',p) is not None and '..' not in p,p)
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.umap' if p.rsplit('/',1)[-1].startswith('L_') else '.uasset')
def binding(file,kind,**extra):
    f=Path(file).resolve();return dict(path=str(f),sha256=sha(f),bytes=f.stat().st_size,kind=kind,**extra)
def protect(file,expected=None,kind='package',**extra):
    f=Path(file).resolve();roots={'package':PROJECT/'Content','module':PROJECT/'Binaries/Win64','cpp':PROJECT/'Source','script':WORK/'tools','evidence':DOC,'config':PROJECT/'Config','backup':Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')}
    check('source within strict project roots',kind in roots and f.is_relative_to(roots[kind]) and f.is_file(),str(f))
    row=binding(f,kind,**extra);check('actual source byte hash matches evidence',expected is None or row['sha256']==expected,str(f));PROTECTED[str(f)]=row['sha256'];SOURCES[str(f)]=row;return row
def document(file):
    f=Path(file).resolve();check('owned actual evidence',f.is_relative_to(DOC) and f.is_file(),str(f));return json.loads(f.read_text('utf-8-sig'))
def report(file,expected=None):
    data=document(file);protect(file,expected,'evidence');cfile=Path(file).parent/'commandlet.json';cmd=document(cfile);protect(cfile,kind='evidence')
    check('native author and real commandlet PASS',data.get('status')=='PASS' and all(x.get('status')=='PASS' for x in data.get('checks',[])) and cmd.get('status')=='PASS' and cmd.get('exit_code')==0,str(file));return data
def latest(pattern,predicate):
    for f in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda f:f.stat().st_mtime,reverse=True):
        d=document(f)
        if d.get('status')=='PASS' and predicate(d):return f,report(f)
    raise RuntimeError('No completed native prerequisite: '+pattern)
def assets(rows):
    for row in rows:protect(disk(row['path']),row['sha256'],package=package(row['path']))
def install_readers():
    # Reuse only these established native read-only reflection functions. Never
    # import/execute another author's Apply path, globals or asset mutations.
    f=WORK/'tools/ue_m5_vs2_corner_playable_r4_author.py';protect(f,kind='script');text=f.read_text('utf-8-sig');tree=ast.parse(text)
    names=('xyz','transform','rounded','component_snapshot','actor_record','flight_record','vehicle_record')
    nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact seven existing read-only reflection helpers',tuple(n.name for n in nodes)==names)
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes);R['native_readers']=dict(path=str(f),functions=list(names),sha256=hashlib.sha256(code.encode()).hexdigest())
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),globals())
def current_world():return U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
def enum_value(kind,*names):
    for name in names:
        if hasattr(kind,name):return getattr(kind,name)
    raise RuntimeError('Missing actual enum '+str(kind)+' '+str(names))
def first_hit(value):
    if isinstance(value,U.HitResult):return value
    if isinstance(value,tuple):return next((v for v in value if isinstance(v,U.HitResult)),None)
    return None

def hit_fields(hit):
    # UE5.8 HitResult HasNativeBreak is exposed as StructBase.to_tuple(),
    # not a GameplayStatics method (see local PythonScriptPlugin binding).
    check('actual native HitResult tuple binding exists', isinstance(hit,U.HitResult) and callable(getattr(hit,'to_tuple',None)))
    fields=hit.to_tuple()
    check('actual BreakHitResult output schema',isinstance(fields,tuple) and len(fields)==18 and isinstance(fields[0],bool) and isinstance(fields[1],bool) and isinstance(fields[5],U.Vector) and isinstance(fields[7],U.Vector),dict(length=len(fields),types=[type(v).__name__ for v in fields]))
    return fields
def ground_fixture(hero_class):
    world=current_world();cap=U.get_default_object(hero_class).get_component_by_class(U.CapsuleComponent)
    radius=float(cap.get_scaled_capsule_radius());hh=float(cap.get_scaled_capsule_half_height());ignored=[]
    hit=first_hit(U.SystemLibrary.line_trace_single(world,U.Vector(-2150,350,350),U.Vector(-2150,350,-100),U.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,U.DrawDebugTrace.NONE,True))
    check('native authored-start ground trace returns actual hit',hit is not None)
    fields=hit_fields(hit);blocking,inside=fields[0],fields[1];impact=fields[5];normal=fields[7];actor=fields[9]
    check('dry flat ground near one fixed start',blocking and not inside and normal.z>=.8 and actor is not None and abs(impact.z-2)<30 and not isinstance(actor,U.Pawn) and not actor.actor_has_tag(U.Name('Water')),dict(actor=path(actor),impact_cm=xyz(impact),normal=xyz(normal)))
    stand=U.Vector(impact.x,impact.y,impact.z+hh+4)
    object_types=[getattr(U.ObjectTypeQuery,'OBJECT_TYPE_QUERY'+str(i)) for i in range(1,33) if hasattr(U.ObjectTypeQuery,'OBJECT_TYPE_QUERY'+str(i))]
    components=U.SystemLibrary.capsule_overlap_components(world,stand,radius+2,hh,object_types,None,ignored)
    if isinstance(components,tuple):components=next((v for v in components if not isinstance(v,bool)),[])
    # FlightSchemaProbe 1e1a3f66: CollisionResponse is a struct;
    # PrimitiveComponent returns the distinct CollisionResponseType enum.
    pawn=U.CollisionChannel.ECC_PAWN;block=U.CollisionResponseType.ECR_BLOCK
    blockers=[path(c) for c in (components or []) if c and c.get_collision_response_to_channel(pawn)==block]
    check('native start capsule has no blocking component',not blockers,blockers)
    # Trace uses Visibility; runtime independently sweeps actual capsule object
    # channel through the whole column before issuing even the first F input.
    roof=first_hit(U.SystemLibrary.capsule_trace_single(world,stand,U.Vector(stand.x,stand.y,stand.z+3200),radius,hh,U.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,U.DrawDebugTrace.NONE,True))
    roof_fields=hit_fields(roof) if roof else None
    check('native entire 30m ascent column clear on Visibility',roof_fields is None or not roof_fields[0],None if roof_fields is None else path(roof_fields[9]))
    bounds=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2FlightBounds)];check('one unchanged flight boundary',len(bounds)==1)
    fr=flight_record(bounds[0]);check('50m ground boundary retained',fr['flight_area_half_extent']==[2500.,2500.] and fr['enable_flight'])
    for box in fr['no_takeoff_volumes']:
        check('fixed start outside every saved no-takeoff box',not all(box['min'][i]<=xyz(stand)[i]<=box['max'][i] for i in range(3)),box)
    return rounded(dict(feet_cm=xyz(impact),capsule_center_cm=xyz(stand),radius_cm=radius,half_height_cm=hh,ground_actor_label=actor.get_actor_label(),normal=xyz(normal),authored_yaw_degrees=math.degrees(math.atan2(impact.y-1550,impact.x+1850))+90,channel_scope='Author Visibility capsule sweep plus Pawn overlap; runtime repeats actual movement capsule channel.'))
def snapshot_map():
    actors=list(ACT.get_all_level_actors());directors=[a for a in actors if isinstance(a,U.HCM5VS2FlightDemoDirector)]
    starts=[a for a in actors if isinstance(a,U.PlayerStart)];check('one start and one opt-in demo director',len(starts)==1 and len(directors)==1)
    forbidden=[a.get_class().get_path_name() for a in actors if isinstance(a,U.CameraActor) or 'ReviewDirector' in a.get_class().get_name() or isinstance(a,U.HCM3Experience) or a.get_class().get_name()=='HCM5Story']
    check('no camera track or experience changes',not forbidden,forbidden)
    check('eight native NPCs and current vehicle retained',sum(isinstance(a,U.HCM5VS2NPC) for a in actors)==8 and sum(isinstance(a,U.HCM1Vehicle) for a in actors)==1)
    check('one passive recorder and one unchanged time director',sum(isinstance(a,U.HCM3Recording) for a in actors)==1 and sum(isinstance(a,U.HCM5VS2CornerTimeDirector) for a in actors)==1)
    time=next(a for a in actors if isinstance(a,U.HCM5VS2CornerTimeDirector));check('actual Afternoon inherited',str(time.get_editor_property('initial_period'))=='Afternoon')
    d=directors[0];props=rounded(dict(expected_start_feet=xyz(d.get_editor_property('expected_start_feet')),orbit_center=xyz(d.get_editor_property('orbit_center')),orbit_radius=float(d.get_editor_property('orbit_radius')),height_above_start=float(d.get_editor_property('height_above_start'))))
    check('bounded explicit director route properties',props['orbit_center']==[-1850.,1550.,0.] and props['orbit_radius']==1200. and props['height_above_start']==3000.)
    mode=current_world().get_world_settings().get_editor_property('default_game_mode');hero=U.get_default_object(mode).get_editor_property('default_pawn_class')
    check('selected Hero class unchanged',package(hero)==R['selected_hero'],path(hero))
    return dict(game_mode=path(mode),hero_class=path(hero),actor_count=len(actors),actors=sorted([actor_record(a) for a in actors],key=lambda a:a['label']),director=props,
                flight=flight_record(next(a for a in actors if isinstance(a,U.HCM5VS2FlightBounds))),vehicle=vehicle_record(next(a for a in actors if isinstance(a,U.HCM1Vehicle))))
def resolve_live():
    f,data=latest('author_*_CornerPlayableR4Reload',lambda d:d.get('schema')==LIVE_SCHEMA and d.get('fresh_process_disk_readback')=='PASS')
    check('source fresh Reload preserved originals',data.get('preservation',{}).get('changed_files')==[])
    ref=data['launch_manifest'];manifest=document(ref['path']);protect(ref['path'],ref['sha256'],'evidence')
    check('source is exact native Live R4 manifest',manifest.get('schema')==LIVE_SCHEMA and manifest.get('status')=='READY_FOR_PLAY_NOT_RUNTIME_TESTED' and manifest.get('expected_npcs')==8 and manifest.get('expected_vehicles')==1)
    check('source manifest belongs to own actual Reload',Path(manifest['reload_report']).resolve()==f.resolve())
    for row in manifest['source_bindings']:protect(row['path'],row['sha256'],row['kind'],**({'package':row['package']} if 'package' in row else {}))
    assets(data['assets']);source_map=manifest['maps']['Afternoon'];check('source exact Afternoon map',source_map==manifest['destination']+'/L_CornerPlayableR4_Afternoon')
    R['inputs']['live_reload']=binding(f,'evidence');R['inputs']['live_manifest']=binding(ref['path'],'evidence');R['source_map']=source_map;R['selected_hero']=manifest['selected_hero'];R['windmill']=manifest['windmill']
    return source_map,data['maps']['Afternoon']['native_readback']
def replace_map_namespace(value,source,dest):
    if isinstance(value,str):return value.replace(source+'.',dest+'.')
    if isinstance(value,list):return [replace_map_namespace(v,source,dest) for v in value]
    if isinstance(value,dict):return {k:replace_map_namespace(v,source,dest) for k,v in value.items()}
    return value
try:
    check('correct local native project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty maps',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    install_readers()
    if PHASE=='FlightDemoR4':
        protect(__file__,kind='script');protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll',kind='module')
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/('HCM5VS2FlightDemoDirector'+ext),kind='cpp')
        source,source_snapshot=resolve_live();check('exact native new demo class registered',hasattr(U,'HCM5VS2FlightDemoDirector'))
        target=ROOT+'/L_FlightDemoR4';check('unique private namespace absent',not A.does_directory_exist(ROOT) and not disk(target).exists())
        check('duplicate only private Live map',A.duplicate_asset(source,target) is not None);check('load new independent map',LEVEL.load_level(target))
        inherited=list(ACT.get_all_level_actors());starts=[a for a in inherited if isinstance(a,U.PlayerStart)];check('one inherited normal player start',len(starts)==1)
        source_records={r['label']:replace_map_namespace(r,source,target) for r in source_snapshot['actors'] if r['class_name']!='/Script/Engine.PlayerStart'}
        keep={a.get_actor_label():actor_record(a) for a in inherited if a!=starts[0]}
        check('new map retains exact source environment and all NPC/car components',keep==source_records)
        mode=current_world().get_world_settings().get_editor_property('default_game_mode');hero=U.get_default_object(mode).get_editor_property('default_pawn_class')
        check('same source private GameMode and selected Hero',path(mode)==source_snapshot['game_mode'] and package(hero)==R['selected_hero'])
        fixture=ground_fixture(hero);R['ground_fixture']=fixture
        starts[0].set_actor_location(U.Vector(*fixture['capsule_center_cm']),False,False);starts[0].set_actor_rotation(U.Rotator(yaw=fixture['authored_yaw_degrees']),False)
        d=ACT.spawn_actor_from_class(U.HCM5VS2FlightDemoDirector,U.Vector(),U.Rotator(),False);check('native opt-in director created',d is not None)
        d.set_actor_label('HC_VS2_FlightDemoR4_InputOnly');d.set_editor_property('tags',[U.Name(OWNER)])
        d.set_editor_property('expected_start_feet',U.Vector(*fixture['feet_cm']));d.set_editor_property('orbit_center',U.Vector(-1850,1550,0));d.set_editor_property('orbit_radius',1200.);d.set_editor_property('height_above_start',3000.)
        A.set_metadata_tag(current_world(),'HarborCityOwnedBy',OWNER)
        after={a.get_actor_label():actor_record(a) for a in ACT.get_all_level_actors() if a!=starts[0] and a!=d};check('only new-map start and new director changed',keep==after)
        before=snapshot_map();check('one new actor only',before['actor_count']==source_snapshot['actor_count']+1)
        check('save only new private demo map',LEVEL.save_current_level());check('same-process native reload',LEVEL.load_level(target));actual=snapshot_map();check('exact persistent native actor/component/route readback',actual==before)
        R.update(status='PASS',destination=ROOT,map=target,native_readback=actual,source_bindings=list(SOURCES.values()),pass_scope='New private map persisted; runtime and fresh-process Reload still NOT_RUN.')
        R['assets']=[dict(path=target,sha256=sha(disk(target)))]
    else:
        f,prior=latest('author_*_FlightDemoR4',lambda d:d.get('schema')==SCHEMA and d.get('owner')==OWNER)
        check('single bounded authored destination',re.fullmatch(r'/Game/HarborCity/M5VS2/FlightDemoR4/Run_[0-9a-f]{12}',prior['destination']) is not None and prior['map']==prior['destination']+'/L_FlightDemoR4' and len(prior['assets'])==1)
        for row in prior['source_bindings']:protect(row['path'],row['sha256'],row['kind'],**({'package':row['package']} if 'package' in row else {}))
        assets(prior['assets']);R['selected_hero']=prior['selected_hero'];check('read saved private map in fresh commandlet',LEVEL.load_level(prior['map']))
        actual=snapshot_map();check('fresh-process exact whole-map and director readback',actual==prior['native_readback'])
        R.update(status='PASS',fresh_process_disk_readback='PASS',destination=prior['destination'],map=prior['map'],selected_hero=prior['selected_hero'],source_map=prior['source_map'],
                 native_readback=actual,ground_fixture=prior['ground_fixture'],windmill=prior['windmill'],assets=prior['assets'],source_author=binding(f,'evidence'),pass_scope='Fresh-process native read-only persistence only; real input and capture are NOT_RUN.')
        manifest=dict(schema=SCHEMA,status='READY_FOR_ENGINE_INPUT_DEMO_NOT_RUNTIME_TESTED',owner=OWNER,kind=R['kind'],input_scope=R['input_scope'],destination=R['destination'],map=R['map'],selected_hero=R['selected_hero'],
                      source_bindings=list(SOURCES.values()),reload_report=str(OUT/'author_result.json'),expected_npcs=8,expected_vehicles=1,save_slot_prefix='HarborCity_VS2_FlightDemo_',
                      record_seconds=60,record_delay=12,sequence_wall_deadline_seconds=55,ground_fixture=R['ground_fixture'],windmill=R['windmill'],scope=R['scope'])
        target=OUT/'flight_demo_r4_manifest.json';target.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8');R['launch_manifest']=binding(target,'evidence')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h];R['preservation']=dict(protected_files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Flight demo author failed; preserve unique partial assets and original failure evidence.')
