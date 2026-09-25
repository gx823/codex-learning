"""Root-only native installer for the isolated revision-two performance map.

Phase CornerVTwoPerformance. Does not run a viewport, measure FPS, capture or
change the source world/character. The final selected integrated Hero is required.
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
PROJECT,DOC=WORK/'HarborCity',WORK/'docs/HarborCity_M5_VS2'
OWNER='HarborCity_M5VS2_HarborCorner_Rev2'
A=U.EditorAssetLibrary
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT=U.get_editor_subsystem(U.EditorActorSubsystem)

def argument(name):
    rows=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    if len(rows)!=1:raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]

OUT=Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
PHASE=argument('M5AuthorPhase')
ROOT='/Game/HarborCity/M5VS2/WorldRev2/Performance_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
MAP=ROOT+'/L_CornerRevTwoPerformance'
OUT.mkdir(parents=True,exist_ok=True)
R=dict(status='RUNNING',phase=PHASE,map=MAP,owner=OWNER,checks=[],assets=[],performance='NOT_RUN',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
SOURCES,PROTECTED=[],{}

def check(name,good,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if good else 'FAIL',actual=actual))
    if not good:raise RuntimeError(name+': '+str(actual))

def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def binding(file,kind,**extra):
    file=Path(file).resolve()
    return dict(path=str(file),sha256=sha(file),bytes=file.stat().st_size,kind=kind,**extra)

def document(file):
    file=Path(file).resolve()
    check('owned actual document',file.is_relative_to(DOC) and file.is_file(),str(file))
    return json.loads(file.read_text('utf-8-sig'))

def disk(package):
    check('bounded VS2 package',re.fullmatch(r'/Game/HarborCity/M5VS2/[A-Za-z0-9_/]+',package) is not None,package)
    return PROJECT/'Content'/Path(package[6:]).with_suffix('.umap' if package.rsplit('/',1)[-1].startswith('L_') else '.uasset')

def protect_assets(data):
    rows=data.get('assets',[])
    check('bounded saved package inventory',0<len(rows)<200)
    for row in rows:
        p=disk(row['path'])
        check('source package matches actual native report',p.is_file() and sha(p)==row['sha256'],row['path'])
        PROTECTED[str(p)]=row['sha256'];SOURCES.append(binding(p,'package',package=row['path']))

def completed(file,phase):
    file=Path(file).resolve();data=document(file)
    cmd=file.parent/'commandlet.json';run=document(cmd)
    check('completed native '+phase,data.get('status')=='PASS' and data.get('phase')==phase
          and run.get('status')=='PASS' and run.get('phase')==phase and run.get('exit_code')==0)
    SOURCES.extend([binding(file,'evidence'),binding(cmd,'evidence')]);protect_assets(data)
    return data

def tagged(kind,label,tag):
    actor=ACT.spawn_actor_from_class(kind,U.Vector(),U.Rotator(),False)
    check('native new performance actor',actor is not None,label)
    actor.set_actor_label(label);actor.set_editor_property('tags',[U.Name(OWNER),U.Name(tag)])
    return actor

try:
    check('specific author phase',PHASE=='CornerVTwoPerformance')
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty content/maps',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    files=sorted((DOC/'editor_runtime').glob('author_*_CornerVTwo/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
    source_file=next((p for p in files if document(p).get('status')=='PASS'),None)
    check('actual completed corner source available',source_file is not None)
    source=completed(source_file,'CornerVTwo')
    check('source corner namespace',source.get('owner')==OWNER and re.fullmatch(r'/Game/HarborCity/M5VS2/WorldRev2/Corner_[0-9a-f]{12}/L_AnimeHarbor_Corner_P0_Rev2',source['map']) is not None)
    check('source map among protected native packages',str(disk(source['map'])) in PROTECTED)
    layout_file=Path(source['inputs']['layout']['path']);layout=document(layout_file)
    check('current source layout hash',sha(layout_file)==source['inputs']['layout']['sha256'])
    SOURCES.append(binding(layout_file,'evidence'))
    geometry=source['road_geometry']
    route=[[float(v[0]),float(v[1]),170.] for v in geometry['centerline_cm']]
    length=sum(math.dist(a,b) for a,b in zip(route,route[1:]))
    check('actual 6m road route within 50m at eye height',geometry['width_cm']==600 and geometry['sidewalks_each_cm']==200
          and 5<=len(route)<=200 and 3000<=length<=4500 and all(math.isfinite(v) and abs(v)<=2500 for p in route for v in p),dict(points=len(route),length_cm=length))
    check('no duplicate native road samples',all(math.dist(a,b)>.01 for a,b in zip(route,route[1:])))
    # A reviewed, fully integrated current Hero, never silently the old baseline.
    selection_file=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json';selection=document(selection_file)
    check('explicit current integrated Hero selection',selection.get('schema_version')==1 and selection.get('status')=='SELECTED_FOR_REVIEW')
    hero=selection['blueprint'].split('.')[0]
    check('private integrated Hero package',re.fullmatch(r'/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/BP_[A-Za-z0-9_]+',hero) is not None)
    SOURCES.append(binding(selection_file,'evidence'))
    integration=None
    for ref in selection['source_reports']:
        file=Path(ref['path']);data=document(file)
        check('selected source report SHA',sha(file)==ref['sha256'])
        data=completed(file,data['phase'])
        if data['phase']=='HeroRev2IntegrationReload' and data.get('blueprint')==hero:
            check('selected Hero fresh-process bindings passed',data.get('fresh_process_disk_readback')=='PASS')
            integration=data
    check('selected final integrated Hero actual reload proof',integration is not None and str(disk(hero)) in PROTECTED)
    # The integrated BP references GAS/flight/outline/halo assets. Bind these input
    # report inventories too, so a changed dependency invalidates a later launch.
    for phase,ref in integration['inputs'].items():
        file=Path(ref['path']);check('integration dependency report SHA',sha(file)==ref['sha256'])
        completed(file,phase)
    for row in integration['effective_materials']:
        file=disk(row['path']);PROTECTED[str(file)]=sha(file);SOURCES.append(binding(file,'package',package=row['path']))
    for name in ('HCM5VS2CornerTimeDirector','HCM5VS2CornerPerformanceDirector'):
        check('native reflected class',hasattr(U,name),name)
        for suffix in ('.h','.cpp'):SOURCES.append(binding(PROJECT/'Source/HarborCity/M5VS2'/(name+suffix),'cpp'))
    SOURCES.extend([binding(__file__,'script'),binding(WORK/'tools/launch_m5_vs2_corner_performance.ps1','script'),
                    binding(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll','module')])
    check('unique destination absent',not A.does_directory_exist(ROOT) and not disk(MAP).exists())
    check('native isolated map copy',A.duplicate_asset(source['map'],MAP) is not None)
    check('load copied native map',LEVEL.load_level(MAP))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    actors=list(ACT.get_all_level_actors())
    forbidden=('HCM5VS2CornerTimeDirector','HCM5VS2CornerPerformanceDirector','HCM5VS2CornerRevTwoReviewDirector','HCM5VS2CornerReviewDirector','HCM3Recording')
    check('no inherited review/record/performance actors',not any(a.get_class().get_name() in forbidden for a in actors))
    hero_class=U.load_class(None,hero+'.'+hero.rsplit('/',1)[-1]+'_C')
    check('selected actual Hero class',hero_class is not None)
    mode_source='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'
    p=disk(mode_source);PROTECTED[str(p)]=sha(p);SOURCES.append(binding(p,'package',package=mode_source))
    mode_path=ROOT+'/BP_PerformanceGameMode';mode=A.duplicate_asset(mode_source,mode_path)
    check('new private GameMode',mode is not None)
    U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',hero_class)
    check('native GameMode compile',U.BlueprintEditorLibrary.compile_blueprint(mode))
    check('actual selected Hero binding',U.get_default_object(mode.generated_class()).get_editor_property('default_pawn_class')==hero_class)
    check('save new mode only',A.save_loaded_asset(mode,False))
    world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
    A.set_metadata_tag(world,'HarborCityOwnedBy',OWNER)
    time_actor=tagged(U.HCM5VS2CornerTimeDirector,'HC_M5VS2_Rev2_PerformanceTime','HC_VS2_Rev2_TimeDirector')
    time_actor.set_editor_property('initial_period',U.Name('Afternoon'))
    tagged(U.HCM5VS2CornerPerformanceDirector,'HC_M5VS2_Rev2_Performance','HC_VS2_Rev2_PerformanceDirector')
    check('native new map save and reload',LEVEL.save_current_level() and LEVEL.load_level(MAP))
    saved=list(ACT.get_all_level_actors())
    for name in ('TimeDirector','PerformanceDirector'):
        check('persisted unique director',sum(a.actor_has_tag(U.Name('HC_VS2_Rev2_'+name)) for a in saved)==1,name)
    check('unchanged source arrangement plus two directors',len(saved)==len(actors)+2)
    check('reloaded actual selected Hero mode',world is not None and U.get_default_object(
        U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world().get_world_settings().get_editor_property('default_game_mode')).get_editor_property('default_pawn_class')==hero_class)
    for asset in (MAP,mode_path):
        row=binding(disk(asset),'package',package=asset);SOURCES.append(row)
        R['assets'].append(dict(path=asset,sha256=row['sha256'],bytes=row['bytes']))
    check('all source packages preserved',all(Path(p).is_file() and sha(p)==h for p,h in PROTECTED.items()))
    # Deduplicate identical inventory rows without weakening their bound hashes.
    unique={}
    for row in SOURCES:
        if row['path'] in unique:check('duplicate source binding agrees',unique[row['path']]==row,row['path'])
        unique[row['path']]=row
    plan=dict(schema_version=1,plan_type='HARBOR_CORNER_REV2_PERFORMANCE',status='READY_FOR_RUNTIME_NOT_MEASURED',
        owner=OWNER,map=MAP,source_corner_map=source['map'],selected_hero=hero,expected_resolution=[1920,1080],
        source_bindings=list(unique.values()),warmup_seconds=30,measurement_seconds=65,road_camera_points_cm=route,
        route_length_cm=length,camera_fov_degrees=65,route_cycle_seconds=30,quality_choices=['High','Epic'],
        period_choices=['Afternoon','Dusk','Night'],default_period='Afternoon',
        timing='Native viewport EndDraw wall-frame intervals; same nearest-rank p99 and N/sum average as M4 profiles.',
        vram='Actual RHI adapter LUID -> DXGI per-process LOCAL CurrentUsage, nominal 1Hz sampled peak; unavailable is NOT_RUN.',
        scope='Editor binary -game, full saved 50m street and selected integrated Hero. Live world and HUD retained; camera route only, no player/OS input acceptance.',
        sampling='Separate >=30s warmup and >=65s uninterrupted measurement; no screenshot/record request; no outlier filtering.',
        interruptions='Esc permanently stops sampler/restore/autoquit. P or focus loss discards interrupted window; resume requires new complete warmup and measurement.',
        cvar_contract={'width':1920,'height':1080,'r.ScreenPercentage':100,'r.VSync':0,'t.MaxFPS':0,'r.DynamicRes.OperationMode':0},
        runtime_performance='NOT_RUN',packaged_performance='NOT_RUN',user_art_approval='USER_REVIEW')
    plan_file=OUT/'corner_v2_performance_plan.json';plan_file.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n','utf-8')
    R.update(status='PASS',performance_plan=binding(plan_file,'evidence'),source_corner_author=str(source_file),
             selected_hero=hero,actor_count=len(saved),source_packages_changed=False,
             pass_scope='Native isolated map/mode/directors saved and reloaded, plan bound to source hashes only. All FPS/VRAM/runtime results NOT_RUN.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n','utf-8')
if R['status']!='PASS':raise RuntimeError('Performance installer failed; preserve unique partial assets and report')
