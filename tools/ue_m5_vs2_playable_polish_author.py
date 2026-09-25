"""New copies of the existing corner and flight route, using the verified hero/car.
No environment expansion or original map changes.
"""
from pathlib import Path
import datetime as dt,hashlib,json,re,traceback
import unreal as U
W=Path('D:/科研学习/codex学习');P=W/'HarborCity';D=W/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary;L=U.get_editor_subsystem(U.LevelEditorSubsystem);E=U.get_editor_subsystem(U.EditorActorSubsystem)
def arg(n):
    v=re.findall(r'(?:^|\s)-'+n+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(v)==1
    return v[0][0] or v[0][1]
O=Path(arg('M5EvidenceDir'));assert O.is_relative_to(D/'editor_runtime') and arg('M5AuthorPhase')=='PlayablePolishAuthor'
t=hashlib.sha256(str(O).encode()).hexdigest()[:12];root='/Game/HarborCity/M5VS2/WorldRev2/PlayablePolish_'+t
R=dict(status='RUNNING',phase='PlayablePolishAuthor',destination=root,checks=[],assets=[],maps={},preserved_sources={},runtime='NOT_RUN',art='USER_REVIEW')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def doc(p):return json.loads(Path(p).read_text('utf-8-sig'))
def pkg(o):return(o.get_path_name() if hasattr(o,'get_path_name') else str(o)).split('.')[0]
def disk(package,ext='.uasset'):return P/'Content'/Path(package.removeprefix('/Game/')).with_suffix(ext)
def dump():(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n','utf-8')
def check(n,ok):
    R['checks'].append(dict(name=n,status='PASS' if ok else 'FAIL'));dump()
    if not ok:raise RuntimeError(n)
def bind(p,h=None):
    p=Path(p);v=sha(p);check('source unchanged: '+p.name,h is None or h.lower()==v);R['preserved_sources'][str(p)]=v
def add(package,ext='.uasset'):
    p=disk(package,ext);R['assets'].append(dict(package=package,path=str(p),sha256=sha(p)))
try:
    hero_candidates=sorted((D/'editor_runtime').glob('author_*_FlightPosePolishAuthor/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
    check('flight pose author evidence exists',bool(hero_candidates));hero_file=hero_candidates[0]
    hero=doc(hero_file);check('hero author passed',hero['status']=='PASS');bind(hero_file)
    for a in hero['assets']:bind(disk(a['path']),a['sha256'])
    skin_file=D/'editor_runtime/author_20260925_144023_126_1d5b04f9_QHarborCompareAuthor/author_result.json'
    skin=doc(skin_file);bind(skin_file);check('warm Q skin author passed',skin['status']=='PASS' and skin['skin_revision']=='HarborWarm02')
    skin_launch=D/'editor_runtime/20260925_144135_820_a0ddc81e_q_harbor_compare_game/launch.json'
    skin_run=doc(skin_launch);bind(skin_launch);check('Q skin actual native comparison completed',skin_run['status']=='PASS_CAPTURE_ONLY' and skin_run['stop_latched'] is False)
    for a in skin['assets']:
        package=pkg(a['path']);bind(disk(package,'.umap' if package==skin['map'] else '.uasset'),a['sha256'])
    skin_materials={3:A.load_asset(skin['candidate_root']+'/MI_Q_Face_WarmY'),7:A.load_asset(skin['candidate_root']+'/MI_Q_Body_WarmY')}
    check('saved Q skin materials load',all(skin_materials.values()))
    hands_file=D/'editor_runtime/author_20260925_115146_488_d4aa006c_DrivingHandsApply/author_result.json'
    hands=doc(hands_file);check('hands car author passed',hands['status']=='PASS');bind(hands_file)
    for a in hands['assets']:bind(disk(a['path']),a['sha256'])
    live_file=D/'editor_runtime/author_20260925_102226_778_821e5904_CornerPlayableR5Reload/corner_playable_r5_manifest.json'
    live=doc(live_file);bind(live_file)
    sources=dict(live['maps']);sources['Flight']='/Game/HarborCity/M5VS2/FlightDemoR5/Run_f6154986e514/L_FlightDemoR5'
    hero_class=A.load_blueprint_class(hero['blueprint']);car_class=A.load_blueprint_class(hands['blueprint'])
    check('native new actor classes',hero_class is not None and car_class is not None)
    check('load source afternoon',L.load_level(sources['Afternoon']))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    source_mode=pkg(world.get_world_settings().get_editor_property('default_game_mode'));bind(disk(source_mode))
    mode=A.duplicate_asset(source_mode,root+'/BP_PlayablePolishGameMode');check('private GameMode duplicate',mode is not None)
    U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',hero_class)
    check('compile and save new GameMode',B.compile_blueprint(mode) and A.save_loaded_asset(mode,False));add(pkg(mode))
    check('compiled GameMode selects the new hero',U.get_default_object(mode.generated_class()).get_editor_property('default_pawn_class')==hero_class)
    for period,source in sources.items():
        bind(disk(source,'.umap'))
        target=(root+'/L_PlayablePolish_'+period) if period!='Flight' else ('/Game/HarborCity/M5VS2/FlightDemoR5/Run_'+t+'/L_FlightDemoR5')
        check('fresh target map',not A.does_asset_exist(target));check('native map copy',A.duplicate_asset(source,target) is not None)
        check('load new map',L.load_level(target));world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
        before=E.get_all_level_actors();cars=[x for x in before if isinstance(x,U.HCM1Vehicle)]
        check('one old parked car',len(cars)==1);old=cars[0]
        car=E.spawn_actor_from_class(car_class,old.get_actor_location(),old.get_actor_rotation(),False)
        check('private hands car created',car is not None);car.set_actor_transform(old.get_actor_transform(),False,True)
        car.set_actor_label(old.get_actor_label());car.set_editor_property('stable_id',old.get_editor_property('stable_id'));car.set_editor_property('tags',old.get_editor_property('tags'))
        check('replace old car only in copied map',E.destroy_actor(old))
        check('unchanged actor count and eight NPCs',len(E.get_all_level_actors())==len(before) and len([x for x in E.get_all_level_actors() if isinstance(x,U.HCM5VS2NPC)])==8)
        qs=[x for x in E.get_all_level_actors() if isinstance(x,U.HCM5VS2NPC) and '/AvatarSample_Q/' in x.get_component_by_class(U.SkeletalMeshComponent).get_skeletal_mesh_asset().get_path_name()]
        check('one actual Q in copied map',len(qs)==1)
        qbody=qs[0].get_component_by_class(U.SkeletalMeshComponent);qbody.modify()
        for index,material in skin_materials.items():qbody.set_material(index,material)
        check('save new map',L.save_current_level());add(target,'.umap');R['maps'][period]=target
    R.update(status='PASS',hero=hero['blueprint'],hero_animation=hero['animation'],vehicle=hands['blueprint'],source_maps=sources,
             q_skin_materials={str(k):pkg(v) for k,v in skin_materials.items()},
             scope='Existing 50m corner copies; selected hero with relaxed flight loops, parked driving-hands car, two warm Q skin materials. Other NPC clothing remains original and unresolved.')
    R['runtime_module']=dict(path=str(P/'Binaries/Win64/UnrealEditor-HarborCity.dll'),sha256=sha(P/'Binaries/Win64/UnrealEditor-HarborCity.dll'))
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['changed_sources']=[p for p,h in R['preserved_sources'].items() if sha(p)!=h]
    if R['changed_sources']:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Private playable polish author failed')
