"""Native private R4 play maps; ordinary input only, never a capture director.

Run CornerPlayableR4 once, then CornerPlayableR4Reload in a NEW commandlet.
The latter is read-only and publishes the launch manifest. No UE/UI launch here.
Source R4, Hero, eight IdleR2 BPs and the native vehicle are never saved/edited.
"""
from pathlib import Path
import ast
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve(); PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary; B=U.BlueprintEditorLibrary
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem); ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
OWNER='HarborCity_M5VS2_CornerPlayableR4'; SCHEMA='HarborCity.M5VS2.CornerPlayableR4.v1'
PERIODS=('Afternoon','Dusk','Night'); LETTERS='QRJTUVWX'
IDLE_AUTHOR=WORK/'tools/ue_m5_vs2_npc_idle_r2_author.py'
CMD=U.SystemLibrary.get_command_line()
def arg(name):
    m=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',CMD)
    assert len(m)==1,name
    return m[0][0] or m[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve(); PHASE=arg('M5AuthorPhase')
assert PHASE in ('CornerPlayableR4','CornerPlayableR4Reload') and OUT.is_relative_to(DOC/'editor_runtime')
assert not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT='/Game/HarborCity/M5VS2/WorldRev2/PlayableR4_'+TOKEN
R=dict(schema=SCHEMA,status='RUNNING',phase=PHASE,owner=OWNER,checks=[],assets=[],inputs={},maps={},
       runtime='NOT_RUN',os_input='NOT_RUN',art='USER_REVIEW',fresh_process_disk_readback='NOT_RUN',
       kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED',started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       scope='50 m R4 corner, walking/flight/driving/proportion preview. NPC idle/face only; no M3Experience, combat/dialogue/quests not enabled or accepted.')
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
    p=package(obj);check('bounded game package path',p.startswith('/Game/') and re.fullmatch(r'[A-Za-z0-9_/]+',p) is not None and '..' not in p,p)
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.umap' if p.rsplit('/',1)[-1].startswith('L_') else '.uasset')
def binding(file,kind,**extras):
    file=Path(file).resolve();return dict(path=str(file),sha256=sha(file),bytes=file.stat().st_size,kind=kind,**extras)
def protect(file,expected=None,kind='package',**extras):
    file=Path(file).resolve()
    roots={'package':PROJECT/'Content','cpp':PROJECT/'Source','module':PROJECT/'Binaries/Win64','script':WORK/'tools','evidence':DOC,'config':PROJECT/'Config','backup':Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')}
    check('source within exact allowed root',kind in roots and file.is_relative_to(roots[kind]) and file.is_file(),str(file))
    row=binding(file,kind,**extras);check('current source SHA matches evidence',expected is None or row['sha256']==expected,str(file))
    PROTECTED[str(file)]=row['sha256'];SOURCES[str(file)]=row
    return row
def document(file):
    file=Path(file).resolve();check('owned evidence document',file.is_relative_to(DOC) and file.is_file(),str(file))
    return json.loads(file.read_text('utf-8-sig'))
def process(file):
    f=Path(file).parent/'commandlet.json';data=document(f)
    check('native prerequisite process really exited successfully',data.get('status')=='PASS' and data.get('exit_code')==0,str(f))
    protect(f,kind='evidence');return data
def report(file,expected=None):
    data=document(file);protect(file,expected,'evidence');process(file)
    check('source native author PASS',data.get('status')=='PASS' and all(c.get('status')=='PASS' for c in data.get('checks',[])),str(file))
    return data
def latest(pattern,predicate):
    for file in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        data=document(file)
        if data.get('status')=='PASS' and predicate(data):return file,report(file)
    raise RuntimeError('No actual completed source: '+pattern)
def assets(rows):
    for row in rows:protect(disk(row['path']),row['sha256'],package=package(row['path']))
def load(p):
    o=A.load_asset(p);check('exact native asset loads',o is not None,p);return o
def cls(p):
    p=package(p);o=U.load_class(None,p+'.'+p.rsplit('/',1)[-1]+'_C');check('exact native generated class loads',o is not None,p);return o
def xyz(v):return [float(v.x),float(v.y),float(v.z)]
def transform(t):return dict(position_cm=xyz(t.translation),quaternion_xyzw=[float(t.rotation.x),float(t.rotation.y),float(t.rotation.z),float(t.rotation.w)],scale=xyz(t.scale3d))
def rounded(value):
    if isinstance(value,float):return round(value,6)
    if isinstance(value,list):return [rounded(v) for v in value]
    if isinstance(value,dict):return {k:rounded(v) for k,v in value.items()}
    return value

def install_readers():
    # Only these two existing read-only native reflection functions, never the
    # idle author's main/Apply code. Full bytes are included in source guards.
    protect(IDLE_AUTHOR,kind='script');text=IDLE_AUTHOR.read_text('utf-8-sig');tree=ast.parse(text)
    names=('getup_snapshot','cdo_snapshot');nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('two bounded existing native NPC readers',tuple(n.name for n in nodes)==names)
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes)
    R['native_readers']=dict(path=str(IDLE_AUTHOR),functions=list(names),sha256=hashlib.sha256(code.encode()).hexdigest())
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(IDLE_AUTHOR)+'::readonly_cdo_readers','exec'),globals())

def resolve_hero():
    file=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json';select=document(file);protect(file,kind='evidence')
    check('explicit actual selected Hero',select.get('schema_version')==1 and select.get('status')=='SELECTED_FOR_REVIEW')
    bp=package(select['blueprint']);check('isolated selected Hero',bp.startswith('/Game/HarborCity/M5VS2/HeroRev2/') and '..' not in bp)
    fresh=False;proof=set()
    for ref in select['source_reports']:
        data=report(ref['path'],ref['sha256']);assets(data.get('assets',[]));proof.update(package(r['path']) for r in data.get('assets',[]))
        if data.get('phase')=='HeroRev2IntegrationReload' and package(data.get('blueprint',''))==bp:
            fresh=data.get('fresh_process_disk_readback')=='PASS'
    check('selected Hero has actual integration and fresh-process provenance',fresh and bp in proof,bp)
    R['inputs']['hero_selection']=binding(file,'evidence');R['selected_hero']=bp
    return cls(bp)

def resolve_npcs():
    specs={}
    for letter in LETTERS:
        file,data=latest('author_*_NPCIdleR2'+letter+'Reload',lambda d:d.get('sample')==letter and d.get('schema')=='HarborCity.M5VS2.NPCIdleR2.v1')
        check('strict eight-source Idle Reload contract',data['phase']=='NPCIdleR2'+letter+'Reload' and data.get('fresh_process_disk_readback')=='PASS'
              and len(data.get('assets',[]))==4 and data.get('preservation',{}).get('changed_source_files')==[]
              and all(data.get(k)==0 for k in ('map_writes','skeleton_writes','physics_writes')),letter)
        ref=data['inputs']['candidate_evidence'];prior=report(ref['path'],ref['sha256'])
        check('Reload matches original actual Apply exactly',prior.get('phase')=='NPCIdleR2'+letter+'Apply' and prior.get('schema')==data['schema']
              and all(prior.get(k)==data.get(k) for k in ('specimen','assets','candidate_cdo','source_cdo')),letter)
        assets(data['assets'])
        for source,expected in data['preservation']['source_sha256_before'].items():
            f=Path(source).resolve();kind='config' if f.is_relative_to(PROJECT/'Config') else ('backup' if f.is_relative_to(Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')) else 'package')
            protect(f,expected,kind)
        spec=data['specimen'];prefix='/Game/HarborCity/M5VS2/NPC/AvatarSample_'+letter+'/IdleR2/Batch_'
        check('own distinct native Idle Blueprint',spec['blueprint'].startswith(prefix) and spec['blueprint'].endswith('/BP_NPCIdleR2_'+letter),spec['blueprint'])
        actual=cdo_snapshot(load(spec['blueprint']))
        check('complete actual current native CDO equals Reload',actual==data['candidate_cdo'],letter)
        graph=json.loads(U.HCM5VS2NPCIdleEditor.inspect_idle_graph(load(spec['anim_blueprint'])))
        check('actual selected relaxed idle graph',graph.get('status')=='PASS' and graph.get('blendspace')==spec['blendspace']
              and graph.get('samples')==data['candidate_samples'] and graph.get('nodes')==data['candidate_graph'],letter)
        specs[letter]=dict(specimen=spec,cdo=actual,class_object=cls(spec['blueprint']))
        R['inputs']['NPC_'+letter]=binding(file,'evidence')
    check('eight actual independent source meshes',len({v['cdo']['mesh'] for v in specs.values()})==8)
    return specs

def component_snapshot(actor,ignore_root_transform=False):
    rows=[]
    for c in actor.get_components_by_class(U.SceneComponent):
        row=dict(name=c.get_name(),class_name=c.get_class().get_path_name())
        if not (ignore_root_transform and c==actor.get_editor_property('root_component')):row['relative']=transform(c.get_relative_transform())
        if isinstance(c,U.PrimitiveComponent):row['collision']=str(c.get_collision_enabled())
        if isinstance(c,U.MeshComponent):row['materials']=[path(c.get_material(i)) for i in range(c.get_num_materials())]
        if isinstance(c,U.StaticMeshComponent):row['mesh']=path(c.get_editor_property('static_mesh'))
        if isinstance(c,U.SkeletalMeshComponent):
            row.update(mesh=path(c.get_editor_property('skeletal_mesh_asset')),animation_class=path(c.get_editor_property('anim_class')),physics=path(c.get_editor_property('physics_asset_override')))
        if isinstance(c,(U.LightComponent,U.SkyLightComponent)):
            color=c.get_editor_property('light_color');row.update(intensity=float(c.get_editor_property('intensity')),color_rgba=[color.r,color.g,color.b,color.a])
        rows.append(row)
    return sorted(rows,key=lambda r:r['name'])
def actor_record(actor):
    return rounded(dict(label=actor.get_actor_label(),class_name=actor.get_class().get_path_name(),transform=transform(actor.get_actor_transform()),
        tags=sorted(str(t) for t in actor.get_editor_property('tags')),components=component_snapshot(actor)))
def flight_record(bounds):
    fields={k:float(bounds.get_editor_property(k)) for k in ('sea_level_z','soft_return_distance','ceiling_height','water_clearance')}
    for k in ('flight_area_center','flight_area_half_extent'):
        v=bounds.get_editor_property(k);fields[k]=[float(v.x),float(v.y)]
    fields['enable_flight']=bool(bounds.get_editor_property('enable_flight'))
    fields['no_takeoff_volumes']=[dict(min=xyz(b.min),max=xyz(b.max),valid=bool(b.get_editor_property('is_valid'))) for b in bounds.get_editor_property('no_takeoff_volumes')]
    return rounded(fields)
def npc_instance(actor):
    body=actor.get_component_by_class(U.SkeletalMeshComponent);cap=actor.get_component_by_class(U.CapsuleComponent)
    return rounded(dict(stable_id=str(actor.get_editor_property('stable_id')),class_name=actor.get_class().get_path_name(),
        stationary=bool(actor.get_editor_property('stationary')),region=actor.get_editor_property('navigation_region').get_actor_label(),
        body_transform=transform(body.get_relative_transform()),materials=[path(body.get_material(i)) for i in range(body.get_num_materials())],
        mesh=package(body.get_editor_property('skeletal_mesh_asset')),anim_class=path(body.get_editor_property('anim_class')),
        physics=package(body.get_editor_property('physics_asset_override')),profile=package(actor.get_editor_property('npc_profile')),
        capsule=[float(cap.get_unscaled_capsule_radius()),float(cap.get_unscaled_capsule_half_height())],
        getup=getup_snapshot(actor.get_component_by_class(U.HCM4R1PhysicalReactionComponent))))
def vehicle_record(vehicle):
    movement=vehicle.get_chaos_movement();engine=movement.get_editor_property('engine_setup')
    return rounded(dict(class_name=vehicle.get_class().get_path_name(),components=component_snapshot(vehicle,True),
        movement_class=movement.get_class().get_path_name(),mass=float(movement.get_editor_property('mass')),
        drag_coefficient=float(movement.get_editor_property('drag_coefficient')),downforce_coefficient=float(movement.get_editor_property('downforce_coefficient')),
        max_torque=float(engine.get_editor_property('max_torque')),max_rpm=float(engine.get_editor_property('max_rpm'))))
def spawn(label,kind,position=(0,0,0),yaw=0):
    actor=ACT.spawn_actor_from_class(kind,U.Vector(*position),U.Rotator(yaw=yaw),False)
    check('native private actor created',actor is not None,label)
    actor.set_actor_label('HC_VS2_PlayR4_'+label);actor.set_editor_property('tags',[U.Name(OWNER)])
    return actor

def snapshot_map(period,hero_class,specs,expected_flight,expected_vehicle):
    actors=list(ACT.get_all_level_actors());world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    forbidden=[a.get_class().get_name() for a in actors if isinstance(a,U.CameraActor) or 'ReviewDirector' in a.get_class().get_name()
               or isinstance(a,U.HCM3Experience) or a.get_class().get_name()=='HCM5Story']
    check('no camera, review controller, experience or story actor',not forbidden,forbidden)
    time=[a for a in actors if isinstance(a,U.HCM5VS2CornerTimeDirector)]
    check('one actual permanent time director',len(time)==1 and str(time[0].get_editor_property('initial_period'))==period)
    recorders=[a for a in actors if isinstance(a,U.HCM3Recording)];check('one inert opt-in native recorder',len(recorders)==1)
    vehicles=[a for a in actors if isinstance(a,U.HCM1Vehicle)]
    check('one current unchanged native vehicle',len(vehicles)==1 and vehicle_record(vehicles[0])==expected_vehicle)
    check('native stable car identity',str(vehicles[0].get_editor_property('stable_id'))=='VS2_PlayR4_Car')
    bounds=[a for a in actors if isinstance(a,U.HCM5VS2FlightBounds)]
    check('original bounds and indoor no-flight preserved',len(bounds)==1 and flight_record(bounds[0])==expected_flight)
    starts=[a for a in actors if isinstance(a,U.PlayerStart)];check('single normal player start',len(starts)==1)
    mode=world.get_world_settings().get_editor_property('default_game_mode')
    check('private GameMode exact selected Hero',package(mode).startswith(ROOT+'/BP_') and U.get_default_object(mode).get_editor_property('default_pawn_class')==hero_class)
    npc=[a for a in actors if isinstance(a,U.HCM5VS2NPC)];check('exactly eight official NPC instances',len(npc)==8)
    readbacks={}
    for letter,spec in specs.items():
        selected=[a for a in npc if a.get_class()==spec['class_object']];check('one instance per distinct source class',len(selected)==1,letter)
        row=npc_instance(selected[0]);source=rounded(spec['cdo'])
        keys=('materials','mesh','anim_class','physics','profile','capsule','getup','body_transform')
        check('instance preserves actual selected CDO rendering and reaction assets',all(row[k]==source[k] for k in keys),letter)
        check('NPC stationarity and private stable identity',row['stationary'] and row['stable_id']=='VS2_PlayR4_NPC_'+letter)
        readbacks[letter]=row
    return dict(period=period,game_mode=path(mode),hero_class=path(hero_class),actor_count=len(actors),
        actors=sorted([actor_record(a) for a in actors],key=lambda a:a['label']),npcs=readbacks,flight=flight_record(bounds[0]),vehicle=vehicle_record(vehicles[0]))

def resolve_source():
    file,source=latest('author_*_CornerVTwo',lambda d:d.get('owner')=='HarborCity_M5VS2_HarborCorner_Rev2' and d.get('art_revision')==4)
    # R4's actual layout additionally proves the bounded authored scope.
    layout_file=Path(source['inputs']['layout']['path']);layout=document(layout_file)
    protect(layout_file,source['inputs']['layout']['sha256'],'evidence')
    check('exact saved R4 layout and original boundary',layout.get('schema_version')==2 and layout.get('art_revision')==4 and layout.get('playable_bounds_xy')==[-2500,-2500,2500,2500],layout.get('playable_bounds_xy'))
    assets(source['assets']);check('actual source map covered',str(disk(source['map']).resolve()) in PROTECTED)
    R['inputs']['source_corner']=binding(file,'evidence');R['source_corner_map']=source['map'];R['layout']=binding(layout_file,'evidence')
    windmill=next(h for h in source['houses'] if h['role']=='WindmillLandmark')
    box=windmill['bounds_cm'];R['windmill']=dict(source_role=windmill['role'],bounds_cm=box,center_xy_cm=[(box['min'][i]+box['max'][i])/2 for i in (0,1)],maximum_world_z_cm=box['max'][2],scope='Actual R4 native component bounds, static obstacle evidence; not a collision-free flight route.')
    return source,layout

def populate(period,map_path,source,layout,hero_class,specs,mode,expected_vehicle):
    check('new private map destination absent',not A.does_asset_exist(map_path) and not disk(map_path).exists())
    check('native independent R4 map duplicate',A.duplicate_asset(source['map'],map_path) is not None)
    check('load private copy',LEVEL.load_level(map_path));world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    A.set_metadata_tag(world,'HarborCityOwnedBy',OWNER);world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
    inherited=list(ACT.get_all_level_actors());bounds=[a for a in inherited if isinstance(a,U.HCM5VS2FlightBounds)]
    check('source has exactly one flight volume',len(bounds)==1);flight=flight_record(bounds[0])
    check('real 50m bounds and indoor no-flight',flight['enable_flight'] and flight['flight_area_half_extent']==[2500.,2500.] and len(flight['no_takeoff_volumes'])>=1)
    # Whole source environment records, excluding only the explicitly replaced
    # original Q/R, inactive review cameras and the player's start height.
    removed=[a for a in inherited if isinstance(a,(U.HCM5VS2NPC,U.CameraActor))]
    check('source contains two NPCs and actual saved camera actors only',sum(isinstance(a,U.HCM5VS2NPC) for a in removed)==2)
    keep={a.get_actor_label():actor_record(a) for a in inherited if a not in removed and not isinstance(a,U.PlayerStart)}
    for actor in removed:check('remove actor only from independent map',ACT.destroy_actor(actor))
    starts=[a for a in inherited if isinstance(a,U.PlayerStart)];check('one source start',len(starts)==1)
    hero_cdo=U.get_default_object(hero_class);cap=hero_cdo.get_component_by_class(U.CapsuleComponent)
    f=layout['player_start_feet_cm'];starts[0].set_actor_location(U.Vector(f[0],f[1],f[2]+cap.get_scaled_capsule_half_height()+4),False,False)
    regions=[a for a in inherited if isinstance(a,U.HCM3NavRegion)];check('actual preserved allowed region',len(regions)==1)
    posts={'Q':(-650,-1550,2,0),'R':(-650,250,2,0),'J':(550,-1050,2,180),'T':(-610,-550,2,0),
           'U':(550,-450,2,180),'V':(650,750,2,180),'W':(-650,1550,2,0),'X':(550,1750,10,180)}
    for letter,(x,y,z,yaw) in posts.items():
        spec=specs[letter];h=spec['cdo']['capsule'][1]
        actor=spawn('NPC_'+letter,spec['class_object'],(x,y,z+h+2),yaw)
        actor.set_editor_property('stationary',True);actor.set_editor_property('navigation_region',regions[0])
        actor.set_editor_property('stable_id',U.Name('VS2_PlayR4_NPC_'+letter))
    car=spawn('ParkedVehicle',U.HCM1Vehicle,(70,-950,105),90);car.set_editor_property('stable_id',U.Name('VS2_PlayR4_Car'))
    center,extent=car.get_actor_bounds(False,False)
    car_box=dict(min=[center.x-extent.x,center.y-extent.y,center.z-extent.z],max=[center.x+extent.x,center.y+extent.y,center.z+extent.z])
    check('native parked-car bounds leave west passing lane',car_box['min'][0]>-200 and car_box['max'][0]<210 and car_box['min'][1]>-1400 and car_box['max'][1]<-500,car_box)
    R.setdefault('parked_vehicle_bounds_cm',{})[period]=car_box
    time=spawn('TimeDirector',U.HCM5VS2CornerTimeDirector);time.set_editor_property('initial_period',U.Name(period))
    spawn('NativeRecorder',U.HCM3Recording)
    now={a.get_actor_label():actor_record(a) for a in ACT.get_all_level_actors()}
    check('every retained environment actor unchanged',all(now.get(k)==v for k,v in keep.items()))
    before=snapshot_map(period,hero_class,specs,flight,expected_vehicle)
    check('save only independent R4 play map',LEVEL.save_current_level());check('same-process reload private map',LEVEL.load_level(map_path))
    after=snapshot_map(period,hero_class,specs,flight,expected_vehicle)
    check('native exact complete actor persistence',before==after,period)
    R['maps'][period]=dict(package=map_path,native_readback=after)
    R['assets'].append(dict(path=map_path,sha256=sha(disk(map_path))))

try:
    check('correct native project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty source maps',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    install_readers()
    if PHASE=='CornerPlayableR4':
        protect(__file__,kind='script')
        for sub in ('M1/HCM1Vehicle','M1/HCM1PlayerController','M1/HCM1Character','M3/HCM3Recording','M5VS2/HCM5VS2CornerTimeDirector','M5VS2/HCM5VS2FlightComponent'):
            for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity'/(sub+ext),kind='cpp')
        protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll',kind='module')
        source,layout=resolve_source();hero_class=resolve_hero();specs=resolve_npcs()
        source_mode='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode';protect(disk(source_mode),package=source_mode)
        check('private namespace absent',not A.does_directory_exist(ROOT));mode_path=ROOT+'/BP_PlayableR4GameMode'
        mode=A.duplicate_asset(source_mode,mode_path);check('native independent GameMode',mode is not None)
        U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',hero_class);B.compile_blueprint(mode)
        check('compiled private GameMode actual selected Hero',U.get_default_object(mode.generated_class()).get_editor_property('default_pawn_class')==hero_class)
        check('save only new private GameMode',A.save_loaded_asset(mode,False))
        R['assets'].append(dict(path=mode_path,sha256=sha(disk(mode_path))))
        # Current native car CDO is the comparison baseline, no movement tuning.
        vehicle_cdo=U.get_default_object(U.HCM1Vehicle.static_class());car_baseline=vehicle_record(vehicle_cdo)
        for sub in ('SportsCar','VehicleTemplate'):
            for file in (PROJECT/'Content'/sub).rglob('*.uasset'):protect(file)
        for component in car_baseline['components']:
            for ref in component.get('materials',[])+[component.get('mesh'),component.get('physics')]:
                if ref and ref.startswith('/Game/'):protect(disk(ref),package=package(ref))
        R['native_vehicle_cdo']=car_baseline;R['destination']=ROOT
        for period in PERIODS:populate(period,ROOT+'/L_CornerPlayableR4_'+period,source,layout,hero_class,specs,mode,car_baseline)
        R['source_bindings']=list(SOURCES.values());R['status']='PASS'
        R['pass_scope']='Native creation and same-process persistence only; separate CornerPlayableR4Reload required before launch.'
    else:
        file,prior=latest('author_*_CornerPlayableR4',lambda d:d.get('schema')==SCHEMA and d.get('owner')==OWNER)
        check('completed private source author exactly three maps',set(prior['maps'])==set(PERIODS) and len(prior['assets'])==4)
        ROOT=prior['destination'];check('precise independent destination',re.fullmatch(r'/Game/HarborCity/M5VS2/WorldRev2/PlayableR4_[0-9a-f]{12}',ROOT) is not None)
        for row in prior['source_bindings']:protect(row['path'],row['sha256'],row['kind'],**({'package':row['package']} if 'package' in row else {}))
        assets(prior['assets']);hero_class=cls(prior['selected_hero']);specs=resolve_npcs()
        check('same selected eight Idle reports as Apply',all(R['inputs']['NPC_'+l]['sha256']==prior['inputs']['NPC_'+l]['sha256'] for l in LETTERS))
        for period,row in prior['maps'].items():
            check('exact period map path',row['package']==ROOT+'/L_CornerPlayableR4_'+period)
            check('fresh process loads private saved map',LEVEL.load_level(row['package']))
            actual=snapshot_map(period,hero_class,specs,row['native_readback']['flight'],prior['native_vehicle_cdo'])
            check('fresh-process full native map matches author',actual==row['native_readback'],period);R['maps'][period]=dict(package=row['package'],native_readback=actual)
        R.update(status='PASS',fresh_process_disk_readback='PASS',assets=prior['assets'],selected_hero=prior['selected_hero'],destination=ROOT,
                 source_corner_map=prior['source_corner_map'],source_author=binding(file,'evidence'),native_vehicle_cdo=prior['native_vehicle_cdo'],windmill=prior['windmill'],parked_vehicle_bounds_cm=prior['parked_vehicle_bounds_cm'],
                 pass_scope='Fresh-process read-only load, complete actor/CDO/flight/vehicle binding validation. No gameplay, mouse, art or package PASS claim.')
        manifest=dict(schema=SCHEMA,status='READY_FOR_PLAY_NOT_RUNTIME_TESTED',owner=OWNER,kind=R['kind'],destination=ROOT,
            maps={period:row['package'] for period,row in R['maps'].items()},selected_hero=R['selected_hero'],windmill=R['windmill'],parked_vehicle_bounds_cm=R['parked_vehicle_bounds_cm'],
            source_bindings=list(SOURCES.values()),reload_report=str(OUT/'author_result.json'),expected_npcs=8,expected_vehicles=1,
            save_slot_prefix='HarborCity_VS2_PlayR4_'+ROOT.rsplit('_',1)[-1]+'_',
            camera_policy='Ordinary local input and actual PlayerCameraManager; no auto-orbit, review camera, montage route or input injection.',
            recording_policy='Saved AHCM3Recording; optional M5VS2RecordSeconds 20-120 and Delay 3-60. Native Esc/P/focus-stop contract. No input generated.',
            scope=R['scope'])
        target=OUT/'corner_playable_r4_manifest.json';target.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8')
        R['launch_manifest']=binding(target,'evidence')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(protected_files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Private playable R4 author failed; retain all failure evidence and unique partial assets.')
