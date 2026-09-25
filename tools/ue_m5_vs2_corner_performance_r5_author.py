"""Isolated R5 performance map from a real CornerPlayableR5Reload only.

Existing native sampler and its plan contract are unchanged. No UE execution
or measurement is initiated here; root invokes this as CornerVTwoPerformanceR5.
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

WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem);ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
OWNER='HarborCity_M5VS2_HarborCorner_Rev2'
LIVE_OWNER='HarborCity_M5VS2_CornerPlayableR5';LIVE_SCHEMA='HarborCity.M5VS2.CornerPlayableR5.v1'
SCHEMA='HarborCity.M5VS2.CornerPerformanceR5.v1';PHASE='CornerVTwoPerformanceR5'
CMD=U.SystemLibrary.get_command_line()

def arg(name):
    m=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',CMD)
    assert len(m)==1,name
    return m[0][0] or m[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve()
assert arg('M5AuthorPhase')==PHASE and OUT.is_relative_to(DOC/'editor_runtime')
assert not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ROOT='/Game/HarborCity/M5VS2/WorldRev2/Performance_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
MAP=ROOT+'/L_CornerRevTwoPerformance'
R=dict(schema=SCHEMA,art_revision=5,status='RUNNING',phase=PHASE,map=MAP,owner=OWNER,checks=[],assets=[],
       performance='NOT_RUN',started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={};SOURCES={}

def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:raise RuntimeError(name+': '+str(actual))

def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def binding(file,kind,**extras):
    file=Path(file).resolve();return dict(path=str(file),sha256=sha(file),bytes=file.stat().st_size,kind=kind,**extras)

def protect(file,expected=None,kind='package',**extras):
    file=Path(file).resolve()
    roots={'package':PROJECT/'Content','module':PROJECT/'Binaries/Win64','cpp':PROJECT/'Source',
           'script':WORK/'tools','evidence':DOC,'config':PROJECT/'Config',
           'backup':Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')}
    check('exact allowed source root',kind in roots and file.is_relative_to(roots[kind]) and file.is_file(),str(file))
    row=binding(file,kind,**extras)
    check('source SHA preserved',expected is None or row['sha256'].lower()==expected.lower(),str(file))
    if str(file) in SOURCES:check('duplicate SHA agrees',SOURCES[str(file)]['sha256']==row['sha256'],str(file))
    SOURCES[str(file)]=row;PROTECTED[str(file)]=row['sha256'];return row

def document(file):
    file=Path(file).resolve();check('actual owned evidence',file.is_relative_to(DOC) and file.is_file(),str(file))
    return json.loads(file.read_text('utf-8-sig'))

def completed(file,phase):
    file=Path(file).resolve();data=document(file);run_file=file.parent/'commandlet.json';run=document(run_file)
    check('actual native and process PASS '+phase,data.get('status')=='PASS' and data.get('phase')==phase
          and all(c.get('status')=='PASS' for c in data.get('checks',[]))
          and run.get('status')=='PASS' and run.get('phase')==phase and run.get('exit_code')==0)
    protect(file,kind='evidence');protect(run_file,kind='evidence')
    protect(run['script'],run['script_sha256'],'script')
    return data

def package(value):return (value.get_path_name() if hasattr(value,'get_path_name') else str(value)).split('.')[0]
def path(value):return value.get_path_name() if value is not None else None

def disk(value):
    p=package(value)
    check('bounded content package',p.startswith('/Game/') and re.fullmatch(r'[A-Za-z0-9_/]+',p) is not None and '..' not in p,p)
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.umap' if p.rsplit('/',1)[-1].startswith('L_') else '.uasset')

def readers(live_script):
    # Reuse only immutable native serialization helpers, not the Live author main.
    wanted=('xyz','transform','rounded','component_snapshot','actor_record','flight_record','npc_instance','vehicle_record')
    text=Path(live_script).read_text('utf-8-sig');nodes=[n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name in wanted]
    check('exact eight read-only map helpers',tuple(n.name for n in nodes)==wanted)
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(live_script)+'::readonly_map','exec'),globals())
    idle=WORK/'tools/ue_m5_vs2_npc_idle_r2_author.py';protect(idle,kind='script')
    text=idle.read_text('utf-8-sig');nodes=[n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name=='getup_snapshot']
    check('one existing getup read-only helper',len(nodes)==1)
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(idle)+'::readonly_getup','exec'),globals())

def map_snapshot(expected,exclude=None):
    actors=[a for a in ACT.get_all_level_actors() if a!=exclude]
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    mode=world.get_world_settings().get_editor_property('default_game_mode')
    hero_class=U.get_default_object(mode).get_editor_property('default_pawn_class')
    npc=[a for a in actors if isinstance(a,U.HCM5VS2NPC)]
    check('eight exact live NPCs',len(npc)==8)
    npcs={}
    for letter,row in expected['npcs'].items():
        selected=[a for a in npc if str(a.get_editor_property('stable_id'))=='VS2_PlayR5_NPC_'+letter]
        check('one expected stable NPC '+letter,len(selected)==1)
        npcs[letter]=npc_instance(selected[0])
    bounds=[a for a in actors if isinstance(a,U.HCM5VS2FlightBounds)]
    vehicles=[a for a in actors if isinstance(a,U.HCM1Vehicle)]
    time=[a for a in actors if isinstance(a,U.HCM5VS2CornerTimeDirector)]
    recorders=[a for a in actors if isinstance(a,U.HCM3Recording)]
    check('one original time bounds car and inert recorder',len(bounds)==len(vehicles)==len(time)==len(recorders)==1)
    check('no inherited other review or performance actor',not any('ReviewDirector' in a.get_class().get_name()
          or isinstance(a,U.CameraActor) or isinstance(a,U.HCM5VS2CornerPerformanceDirector) for a in actors))
    return dict(period=str(time[0].get_editor_property('initial_period')),game_mode=path(mode),hero_class=path(hero_class),
        actor_count=len(actors),actors=sorted([actor_record(a) for a in actors],key=lambda a:a['label']),
        npcs=npcs,flight=flight_record(bounds[0]),vehicle=vehicle_record(vehicles[0]))

try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    candidates=sorted((DOC/'editor_runtime').glob('author_*_CornerPlayableR5Reload/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
    source_file=next((p for p in candidates if document(p).get('status')=='PASS'),None)
    check('real Live R5 Reload required without fallback',source_file is not None)
    source=completed(source_file,'CornerPlayableR5Reload')
    check('fresh R5 exact source schema',source.get('schema')==LIVE_SCHEMA and source.get('owner')==LIVE_OWNER
          and source.get('art_revision')==5 and source.get('fresh_process_disk_readback')=='PASS'
          and source.get('preservation',{}).get('changed_files')==[] and set(source.get('maps',{}))=={'Afternoon','Dusk','Night'})
    mf=source['launch_manifest'];manifest_file=Path(mf['path']);protect(manifest_file,mf['sha256'],'evidence');manifest=document(manifest_file)
    check('actual Reload manifest identity',manifest.get('schema')==LIVE_SCHEMA and manifest.get('owner')==LIVE_OWNER
          and manifest.get('status')=='READY_FOR_PLAY_NOT_RUNTIME_TESTED' and manifest.get('art_revision')==5
          and manifest.get('expected_npcs')==8 and manifest.get('expected_vehicles')==1
          and Path(manifest['reload_report']).resolve()==source_file.resolve() and manifest['hero_safety']==source['hero_safety'])
    hero=source['selected_hero'];check('actual Modesty selected without fallback',manifest['selected_hero']==hero
          and re.fullmatch(r'/Game/HarborCity/M5VS2/HeroModesty/Review_[0-9a-f]{12}/BP_HeroSafetyShorts',hero) is not None
          and source['hero_safety']['blueprint']==hero and source['hero_safety']['status']=='NATIVE_RELOAD_SELECTED_NOT_ALL_ANGLE_ART_APPROVED')
    rows=manifest['source_bindings'];check('complete native source inventory',len(rows)>=30)
    for row in rows:
        protect(row['path'],row['sha256'],row['kind'],**({'package':row['package']} if 'package' in row else {}))
        check('source byte count matches manifest',Path(row['path']).stat().st_size==row['bytes'])
    module=PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll'
    check('exact one current binary from Live Reload',sum(row['kind']=='module' and Path(row['path']).resolve()==module.resolve() for row in rows)==1)
    for row in source['assets']:protect(disk(row['path']),row['sha256'],package=row['path'])
    check('selected safety BP protected',str(disk(hero).resolve()) in PROTECTED)
    apply=source['source_author'];protect(apply['path'],apply['sha256'],'evidence');completed(apply['path'],'CornerPlayableR5')
    proof=source['source_corner_proof'];check('native R5 corner proof',proof==manifest['source_corner_proof'] and proof['art_revision']==5)
    protect(proof['author']['path'],proof['author']['sha256'],'evidence');corner=completed(proof['author']['path'],'CornerVTwo')
    check('exact source R5 corner and 50m',corner.get('art_revision')==5 and corner['map']==proof['map'])
    protect(proof['layout']['path'],proof['layout']['sha256'],'evidence');layout=document(proof['layout']['path'])
    check('exact saved 50m layout',layout.get('art_revision')==5 and layout.get('playable_bounds_xy')==[-2500,-2500,2500,2500])
    geometry=corner['road_geometry'];route=[[float(v[0]),float(v[1]),170.] for v in geometry['centerline_cm']]
    length=sum(math.dist(a,b) for a,b in zip(route,route[1:]))
    check('same native road camera route',geometry['width_cm']==600 and geometry['sidewalks_each_cm']==200
          and 5<=len(route)<=200 and 3000<=length<=4500 and all(math.isfinite(v) and abs(v)<=2500 for p in route for v in p)
          and all(math.dist(a,b)>.01 for a,b in zip(route,route[1:])))
    live_script=document(source_file.parent/'commandlet.json')['script'];readers(live_script)
    for name in ('HCM5VS2CornerTimeDirector','HCM5VS2CornerPerformanceDirector'):
        check('actual native class exists',hasattr(U,name),name)
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/(name+ext),kind='cpp')
    protect(__file__,kind='script');protect(WORK/'tools/launch_m5_vs2_corner_performance_r5.ps1',kind='script')
    expected=source['maps']['Afternoon']['native_readback'];source_map=source['maps']['Afternoon']['package']
    check('exact manifest selected live map',source_map==manifest['maps']['Afternoon']
          and source_map==source['destination']+'/L_CornerPlayableR5_Afternoon' and str(disk(source_map).resolve()) in PROTECTED)
    check('new private namespace',not A.does_directory_exist(ROOT) and not disk(MAP).exists())
    check('native copy whole Live R5 world',A.duplicate_asset(source_map,MAP) is not None and LEVEL.load_level(MAP))
    before=map_snapshot(expected);check('all copied actors and game bindings equal actual Live Reload',before==expected)
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    old_mode=world.get_world_settings().get_editor_property('default_game_mode');old_mode_path=package(old_mode)
    check('old GameMode present in protected source assets',str(disk(old_mode_path).resolve()) in PROTECTED)
    mode_path=ROOT+'/BP_PerformanceR5GameMode';mode=A.duplicate_asset(old_mode_path,mode_path)
    check('private identical GameMode duplicate',mode is not None);B.compile_blueprint(mode)
    check('private GameMode retains safety Hero',path(U.get_default_object(mode.generated_class()).get_editor_property('default_pawn_class'))==expected['hero_class'])
    check('save only new private GameMode',A.save_loaded_asset(mode,False))
    world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
    perf=ACT.spawn_actor_from_class(U.HCM5VS2CornerPerformanceDirector,U.Vector(),U.Rotator(),False)
    check('only one new native performance actor',perf is not None)
    perf.set_actor_label('HC_M5VS2_R5_Performance');perf.set_editor_property('tags',[U.Name(OWNER),U.Name('HC_VS2_Rev2_PerformanceDirector')])
    new_mode_name=path(mode.generated_class());expected_copy=dict(expected,game_mode=new_mode_name)
    current=map_snapshot(expected,perf);check('only allowed map changes before save',current==expected_copy)
    performance_before=actor_record(perf)
    check('save and reload unique performance map',LEVEL.save_current_level() and LEVEL.load_level(MAP))
    perf_after=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2CornerPerformanceDirector)]
    check('one persisted new performance actor',len(perf_after)==1)
    after=map_snapshot(expected,perf_after[0]);check('each original live actor survives exact native reload',after==expected_copy)
    check('new performance actor exact native persistence',actor_record(perf_after[0])==performance_before)
    for value in (MAP,mode_path):
        row=binding(disk(value),'package',package=value);SOURCES[row['path']]=row
        R['assets'].append(dict(path=value,sha256=row['sha256'],bytes=row['bytes']))
    plan=dict(schema_version=1,plan_type='HARBOR_CORNER_REV2_PERFORMANCE',owner=OWNER,status='READY_FOR_RUNTIME_NOT_MEASURED',
        content_schema=SCHEMA,art_revision=5,map=MAP,source_live_map=source_map,source_corner_map=proof['map'],selected_hero=hero,
        live_reload=binding(source_file,'evidence'),live_manifest=binding(manifest_file,'evidence'),hero_safety=source['hero_safety'],
        expected_npcs=8,expected_vehicles=1,retained_inert_recorders=1,live_actor_count=expected['actor_count'],
        expected_resolution=[1920,1080],source_bindings=list(SOURCES.values()),warmup_seconds=30,measurement_seconds=65,
        road_camera_points_cm=route,route_length_cm=length,camera_fov_degrees=65,route_cycle_seconds=30,
        quality_choices=['High','Epic'],period_choices=['Afternoon','Dusk','Night'],default_period='Afternoon',
        timing='Unchanged native EndDraw wall intervals; average N/sum; nearest-rank p99; no outlier filtering.',
        vram='Unchanged actual RHI adapter LUID -> DXGI per-process LOCAL CurrentUsage sampled peak; missing data is NOT_RUN.',
        recording='One unchanged inert Live recorder retained. No recording flags; native sampler fails if HasCapturedFirstFrame ever becomes true. No capture existence claim before runtime.',
        scope='Editor -game: same actual Live R5 50m world, eight NPCs, safety-layer Hero, native parked car and permanent time director. Camera route only; no OS or player-input acceptance.',
        interruptions='Unchanged Esc latch; P/focus interruption discards partial sample, requires complete warmup and measurement after resume.',
        cvar_contract={'width':1920,'height':1080,'r.ScreenPercentage':100,'r.VSync':0,'t.MaxFPS':0,'r.DynamicRes.OperationMode':0},
        runtime_performance='NOT_RUN',packaged_performance='NOT_RUN',user_art_approval='USER_REVIEW')
    plan_file=OUT/'corner_v2_performance_plan.json';encoded=json.dumps(plan,ensure_ascii=False,indent=2)+'\n'
    check('plan fits unchanged native 2MiB bound',0<len(encoded.encode('utf-8'))<=2*1024*1024)
    plan_file.write_text(encoded,'utf-8')
    R.update(status='PASS',performance_plan=binding(plan_file,'evidence'),source_live_reload=binding(source_file,'evidence'),
        source_live_manifest=binding(manifest_file,'evidence'),selected_hero=hero,actor_count=after['actor_count']+1,
        preserved_actor_count=after['actor_count'],native_readback=after,added_performance_actor=performance_before,
        pass_scope='Native source-bound duplicate and same-process saved-map readback only. No FPS, VRAM, recording or packaged result.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(protected_files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
if R['status']!='PASS':raise RuntimeError('R5 performance author failed; retain unique partial assets and evidence.')
