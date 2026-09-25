"""Native isolated Rev2 review-map installer and source-bound seven-shot plan.

Root runs via ue_m5_vs2_author_run.ps1 -Phase CornerVTwoReview.
No editor/game launch, viewport capture or old-map write here. Each new map has
its own GameMode binding the selected integrated Hero; source character stays intact.
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
PROJECT=WORK/'HarborCity'
DOC=WORK/'docs/HarborCity_M5_VS2'
OWNER='HarborCity_M5VS2_HarborCorner_Rev2'
CMD=U.SystemLibrary.get_command_line()
A=U.EditorAssetLibrary
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT=U.get_editor_subsystem(U.EditorActorSubsystem)

def argument(name):
    match=re.search(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]*)"|(\S+))',CMD)
    return next((x for x in match.groups() if x is not None),'') if match else ''

OUT=Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ROOT='/Game/HarborCity/M5VS2/WorldRev2/Review_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
MAP=ROOT+'/L_CornerRevTwoReview'
PHASE=argument('M5AuthorPhase')
PROFILE={'CornerVTwoReview':'Environment','CornerVTwoHeroPortrait':'HeroPortrait','CornerVTwoHeroFullBody':'HeroFullBody'}.get(PHASE)
R=dict(status='RUNNING',phase=PHASE,profile=PROFILE,map=MAP,owner=OWNER,checks=[],assets=[],
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),runtime='NOT_RUN',visual='NOT_RUN_NULLRHI')

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def binding(p,kind,**extra):
    p=Path(p).resolve()
    return dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size,kind=kind,**extra)
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:raise RuntimeError(name+': '+str(actual))
def document(p):
    p=Path(p).resolve();check('owned evidence document',p.is_relative_to(DOC) and p.is_file(),str(p))
    return json.loads(p.read_text('utf-8-sig'))
def disk(package):
    check('owned VS2 package',package.startswith('/Game/HarborCity/M5VS2/') and '..' not in package and re.fullmatch(r'[A-Za-z0-9_/]+',package) is not None,package)
    return PROJECT/'Content'/Path(package[6:]).with_suffix('.umap' if package.rsplit('/',1)[-1].startswith('L_') else '.uasset')
def camera_rotation(row):
    d=[row['target_cm'][i]-row['location_cm'][i] for i in range(3)]
    check('nondegenerate actual saved camera',all(math.isfinite(x) for x in d) and math.sqrt(sum(x*x for x in d))>1)
    return [math.degrees(math.atan2(d[2],math.hypot(d[0],d[1]))),math.degrees(math.atan2(d[1],d[0])),0.]
def tagged(kind,label,tag):
    actor=ACT.spawn_actor_from_class(kind,U.Vector(),U.Rotator(),False)
    check('native new review actor',actor is not None,label)
    actor.set_actor_label(label);actor.set_editor_property('tags',[U.Name(OWNER),U.Name(tag)])
    return actor

try:
    check('bounded review author phase',PROFILE is not None)
    check('correct project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE/dirty maps',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    explicit=argument('M5CornerVTwoAuthor')
    candidates=[Path(explicit)] if explicit else sorted((DOC/'editor_runtime').glob('author_*_CornerVTwo/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
    chosen=None
    for path in candidates:
        data=document(path)
        if data.get('status')=='PASS' and data.get('phase')=='CornerVTwo' and data.get('owner')==OWNER:
            chosen=(path,data);break
    check('completed actual native CornerVTwo author available',chosen is not None)
    source_file,source=chosen
    process_file=source_file.parent/'commandlet.json';process=document(process_file)
    check('source native process completed',process.get('status')=='PASS' and process.get('exit_code')==0)
    source_map=source['map'];check('source new map namespace',re.fullmatch(r'/Game/HarborCity/M5VS2/WorldRev2/Corner_[0-9a-f]{12}/L_AnimeHarbor_Corner_P0_Rev2',source_map) is not None)
    layout_file=Path(source['inputs']['layout']['path']);layout=document(layout_file)
    check('actual layout unchanged',sha(layout_file)==source['inputs']['layout']['sha256'] and layout.get('schema_version')==2)
    sources=[binding(source_file,'evidence'),binding(process_file,'evidence'),binding(layout_file,'evidence'),binding(__file__,'script')]
    protected={}
    for row in source['assets']:
        path=disk(row['path']);check('native source package unchanged',path.is_file() and sha(path)==row['sha256'],row['path'])
        protected[str(path)]=row['sha256'];sources.append(binding(path,'package',package=row['path']))
    check('source map present in package proof',str(disk(source_map)) in protected)
    copy_file=Path(source['inputs']['village_copy']['path']);check('village source record unchanged',sha(copy_file)==source['inputs']['village_copy']['sha256'])
    sources.append(binding(copy_file,'evidence'))
    code_root=PROJECT/'Source/HarborCity/M5VS2'
    for name in ('HCM5VS2CornerTimeDirector','HCM5VS2CornerRevTwoReviewDirector'):
        for suffix in ('.h','.cpp'):sources.append(binding(code_root/(name+suffix),'cpp'))
    sources.append(binding(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll','module'))
    selection=None
    if PROFILE in ('Environment','HeroPortrait','HeroFullBody'):
        selection_file=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json'
        selection=document(selection_file)
        check('explicit selected Hero candidate',selection.get('schema_version')==1 and selection.get('status')=='SELECTED_FOR_REVIEW')
        hero_bp=selection['blueprint'].split('.')[0]
        check('selected isolated Hero candidate package',hero_bp.startswith('/Game/HarborCity/M5VS2/') and not hero_bp.startswith('/Game/HarborCity/M5VS2/HeroSelestia/') and '..' not in hero_bp)
        sources.append(binding(selection_file,'evidence'));candidate_assets={}
        integrated_reload=False
        for ref in selection['source_reports']:
            report_file=Path(ref['path']);report=document(report_file)
            check('selected native candidate report unchanged',sha(report_file)==ref['sha256'] and report.get('status')=='PASS')
            sources.append(binding(report_file,'evidence'))
            command_file=report_file.parent/'commandlet.json';command=document(command_file)
            check('selected native author process completed',command.get('status')=='PASS' and command.get('exit_code')==0)
            sources.append(binding(command_file,'evidence'))
            if report.get('phase')=='HeroRev2IntegrationReload' and report.get('blueprint')==hero_bp:
                integrated_reload=report.get('fresh_process_disk_readback')=='PASS'
            for row in report.get('assets',[]):
                package=row['path'].split('.')[0]
                if package.startswith('/Game/HarborCity/M5VS2/'):
                    file=disk(package);check('selected candidate saved package matches source',file.is_file() and sha(file)==row['sha256'],package)
                    candidate_assets[package]=binding(file,'package',package=package);protected[str(file)]=row['sha256']
        check('selected Hero Blueprint among actually authored native assets',hero_bp in candidate_assets)
        check('selected integrated Hero fresh-process proof',integrated_reload)
        sources.extend(candidate_assets.values())
    for name in ('HCM5VS2CornerTimeDirector','HCM5VS2CornerRevTwoReviewDirector'):
        check('review class reflected',hasattr(U,name),name)
    check('unique review namespace absent',not A.does_directory_exist(ROOT) and not disk(MAP).exists())
    copied=A.duplicate_asset(source_map,MAP);check('native independent world copy',copied is not None)
    check('load copied review map',LEVEL.load_level(MAP))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    A.set_metadata_tag(world,'HarborCityOwnedBy',OWNER)
    new_assets=[]
    hero_plan=None
    if selection:
        hero_class=U.load_class(None,hero_bp+'.'+hero_bp.rsplit('/',1)[-1]+'_C')
        check('selected actual Hero class loads',hero_class is not None)
        source_mode='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'
        protected[str(disk(source_mode))]=sha(disk(source_mode));sources.append(binding(disk(source_mode),'package',package=source_mode))
        mode_path=ROOT+'/BP_ReviewGameMode';mode=A.duplicate_asset(source_mode,mode_path)
        check('new isolated candidate GameMode',mode is not None)
        U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',hero_class)
        U.BlueprintEditorLibrary.compile_blueprint(mode)
        check('candidate GameMode actual pawn binding',U.get_default_object(mode.generated_class()).get_editor_property('default_pawn_class')==hero_class)
        check('save only new candidate GameMode',A.save_loaded_asset(mode,False));new_assets.append(mode_path)
        world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
        if PROFILE!='Environment':
            inn=next(h for h in source['houses'] if h['role']==layout['interior']['house_role'])
            body=next(c for c in inn['components'] if c['label'].endswith('_SM_BLD_body_v02_01'))['transform']
            transform=U.Transform();transform.set_editor_property('translation',U.Vector(*body['location_cm']))
            transform.set_editor_property('rotation',U.Quat(*body['quaternion_xyzw']));transform.set_editor_property('scale3d',U.Vector(*body['scale']))
            inside=U.MathLibrary.transform_location(transform,U.Vector(*layout['interior']['review_hero_feet_local_cm']))
            inside_feet=[inside.x,inside.y,inside.z]
            hero_plan=dict(blueprint_class=hero_class.get_path_name(),left_eye_bone='LeftEye',right_eye_bone='RightEye',
                selected_blueprint=hero_bp,interior_feet_cm=inside_feet,interior_yaw=-180,outdoor_feet_cm=[1000,0,2],outdoor_yaw=-155,
                material_policy='Exact selected saved candidate only; no per-shot material, extra light or animation override.')
    actors=list(ACT.get_all_level_actors())
    check('no inherited review/time directors',not any(a.get_class().get_name() in ('HCM5VS2CornerTimeDirector','HCM5VS2CornerRevTwoReviewDirector','HCM5VS2CornerReviewDirector') for a in actors))
    time_actor=tagged(U.HCM5VS2CornerTimeDirector,'HC_M5VS2_Rev2_TimeDirector','HC_VS2_Rev2_TimeDirector')
    time_actor.set_editor_property('initial_period',U.Name('Afternoon'))
    tagged(U.HCM5VS2CornerRevTwoReviewDirector,'HC_M5VS2_Rev2_ReviewDirector','HC_VS2_Rev2_ReviewDirector')
    check('save unique review map',LEVEL.save_current_level())
    check('reload saved review map',LEVEL.load_level(MAP))
    saved=list(ACT.get_all_level_actors())
    current_world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('persisted selected integrated Hero GameMode',U.get_default_object(
        current_world.get_world_settings().get_editor_property('default_game_mode')).get_editor_property('default_pawn_class')==hero_class)
    for name in ('TimeDirector','ReviewDirector'):
        check('single persisted new review binding',sum(a.actor_has_tag(U.Name('HC_VS2_Rev2_'+name)) for a in saved)==1,name)
    check('two actors added to unchanged source arrangement',len(saved)==len(actors)+2,dict(before=len(actors),after=len(saved)))
    check('all source native packages preserved',all(sha(p)==h for p,h in protected.items()))
    package_binding=binding(disk(MAP),'package',package=MAP);sources.append(package_binding)
    R['assets']=[dict(path=MAP,sha256=package_binding['sha256'])]
    for asset in new_assets:
        saved_binding=binding(disk(asset),'package',package=asset);sources.append(saved_binding)
        R['assets'].append(dict(path=asset,sha256=saved_binding['sha256']))
    cameras={c['name']:c for c in source['capture_camera_plan']}
    shots=[]
    for period,name in [('Afternoon','SouthStreet'),('Afternoon','Promenade'),('Dusk','SouthStreet'),('Dusk','Promenade'),('Night','SouthStreet'),('Night','Promenade'),('Afternoon','Air50m')]:
        c=cameras[name];i=len(shots);label='%02d_%s_%s'%(i,period,name)
        shots.append(dict(index=i,label=label,expected_png=label+'.png',capture_status='NOT_RUN',
            camera_name=name,camera_location_cm=c['location_cm'],camera_rotation_pitch_yaw_roll=camera_rotation(c),
            camera_fov_degrees=c['fov'],light_preset=period,minimum_settle_seconds=10 if i%2==0 else 2,
            minimum_settle_rendered_frames=30))
    if hero_plan:
        shots=[]
        for i,scene in enumerate(('Afternoon','Dusk','Night','Interior')):
            label='%02d_%s_%s'%(i,scene,PROFILE);inside=i==3
            shots.append(dict(index=i,label=label,expected_png=label+'.png',capture_status='NOT_RUN',camera_name=PROFILE,
                camera_fov_degrees=40 if PROFILE=='HeroPortrait' else 65,light_preset='Afternoon' if inside else scene,
                hero_feet_cm=hero_plan['interior_feet_cm' if inside else 'outdoor_feet_cm'],hero_yaw=hero_plan['interior_yaw' if inside else 'outdoor_yaw'],
                minimum_settle_seconds=10,minimum_settle_rendered_frames=30))
    plan=dict(schema_version=2,plan_type='HARBOR_CORNER_REV2_NATIVE_VIEWPORT',profile=PROFILE,
        status='READY_FOR_RUNTIME_NOT_CAPTURED',map=MAP,owner=OWNER,expected_screenshots=len(shots),expected_resolution=[1920,1080],
        source_bindings=sources,shots=shots,source_corner_map=source_map,selected_hero=hero_bp,
        camera_policy='Unmodified authored cameras, same street camera reused at all three periods. Air50m is a native viewport camera, not a flight-input acceptance test.',
        lighting_policy='Permanent HCM5VS2CornerTimeDirector.SetTimeOfDay public API; record actual full readback. No review-only lighting, exposure or materials.',
        runtime_contract=dict(opt_in='M5VS2CornerRevTwoReview',plan_argument='M5VS2CornerRevTwoPlan',maximum_duration_seconds=300 if hero_plan else 180,
            startup_deadline_seconds=10,observe_esc_without_consuming=True,esc_latches_and_cancels_all_capture_restore_autoquit=True,
            preserve_p_pause=True,screenshot='Native FScreenshotRequest / processed delegate / EndDraw camera and lights / raw 1920x1080 PNG',
            character_material_animation_overrides=False,character_position_staging=bool(hero_plan),os_input=False,art_acceptance='USER_REVIEW'))
    if hero_plan:
        plan['hero']=hero_plan
        plan['camera_policy']='Actual animated eyes sampled after readiness; one locked outside camera reused for afternoon/dusk/night, one indoor camera. Full-body framing fits the actual body/imported extent and visible moving halo with a 12 percent margin at the recorded viewport aspect and FOV. No guessed CDO eye height or gameplay camera change.'
    plan_path=OUT/'corner_v2_capture_plan.json';plan_path.write_text(json.dumps(plan,ensure_ascii=False,indent=2)+'\n','utf-8')
    R.update(status='PASS',source_corner_author=binding(source_file,'evidence'),source_corner_map=source_map,
        capture_plan=binding(plan_path,'evidence'),expected_screenshots=len(shots),actor_count=len(saved),
        protected_source_package_count=len(protected),source_packages_changed=False,
        pass_scope='Native copied map, two saved director actors and source-bound capture plan only. Render/inputs/art NOT_RUN.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n','utf-8')
if R['status']!='PASS':raise RuntimeError('CornerVTwoReview author failed; retained actual failure and any unique partial map')
