"""New isolated physical flight fixture; native author only, never gameplay acceptance.

One unique map and private GameMode per attempt. Uses the selected final Hero
without saving hero, materials, combat, physics or animation sources. Root executes serially.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity'
DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary
ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
CMD=U.SystemLibrary.get_command_line()
OWNER='HarborCity_M5VS2_FlightReview_R2'
HERO=None
MODE='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'


def arg(name):
    rows=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',CMD)
    assert len(rows)==1, 'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT=Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
assert arg('M5AuthorPhase')=='FlightReview'
OUT.mkdir(parents=True,exist_ok=True)
ROOT='/Game/HarborCity/M5VS2/FlightReview/Run_'+hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
MAP=ROOT+'/L_FlightReview'
R=dict(schema='HarborCity.M5VS2.FlightReview.Author.v1',status='RUNNING',owner=OWNER,map=MAP,
       checks=[],assets=[],actors=[],sources=[],runtime='NOT_RUN',visual_acceptance='NOT_RUN_NULLRHI',
       scope='Isolated collision fixture, not an environment art candidate or packaged game.',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={}


def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed)); dump()
    if not ok: raise RuntimeError(name+': '+str(observed))


def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for part in iter(lambda:f.read(1024*1024),b''): h.update(part)
    return h.hexdigest()


def package(p): return str(p).split('.')[0]
def disk(p,ext='.uasset'):
    p=package(p); assert p.startswith('/Game/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix(ext)


def cls(p):
    p=package(p); value=U.load_class(None,p+'.'+p.rsplit('/',1)[1]+'_C')
    check('native generated class',value is not None,p); return value


def spawn(label,kind,position=(0,0,0),rotation=None):
    check('bounded fixture actor count',len(R['actors'])<25)
    value=ACT.spawn_actor_from_class(kind,U.Vector(*position),rotation or U.Rotator(),False)
    check('native spawn',value is not None,label)
    value.set_actor_label('HCFlight_'+label); value.set_editor_property('tags',[U.Name(OWNER),U.Name('HCFlight_'+label)])
    R['actors'].append(dict(label=label,name=value.get_name(),class_path=value.get_class().get_path_name(),location_cm=position))
    return value


def box(label,position,scale,pitch=0,water=False):
    value=spawn(label,U.StaticMeshActor,position,U.Rotator(pitch=pitch))
    mesh=value.get_component_by_class(U.StaticMeshComponent)
    mesh.set_mobility(U.ComponentMobility.STATIC)
    check('native collision cube binding',mesh.set_static_mesh(A.load_asset('/Engine/BasicShapes/Cube')),label)
    mesh.set_collision_profile_name('BlockAll'); value.set_actor_scale3d(U.Vector(*scale))
    if water: value.set_editor_property('tags',list(value.get_editor_property('tags'))+[U.Name('Water')])
    R.setdefault('geometry',[]).append(dict(label=label,center_cm=position,scale_of_100cm_cube=scale,pitch=pitch,
        collision_profile=str(mesh.get_collision_profile_name()),water_tag=water))
    return value


def inventory():
    return sorted((dict(name=a.get_name(),class_path=a.get_class().get_path_name(),
        tags=sorted(str(t) for t in a.get_editor_property('tags'))) for a in ACT.get_all_level_actors()),key=lambda r:r['name'])


try:
    check('fresh isolated namespace',not A.does_directory_exist(ROOT) and not disk(MAP,'.umap').parent.exists(),ROOT)
    selection_file=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json'
    check('explicit final Hero selection exists',selection_file.is_file())
    selection=json.loads(selection_file.read_text('utf-8-sig'))
    check('shared final Hero selection schema',selection.get('schema_version')==1 and selection.get('status')=='SELECTED_FOR_REVIEW')
    HERO=package(selection['blueprint'])
    check('private selected integrated Hero',re.fullmatch(r'/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/BP_[A-Za-z0-9_]+',HERO) is not None)
    R['sources'].append(dict(path=str(selection_file),sha256=sha(selection_file)))
    integrated_reload=False;selected_asset=False
    for ref in selection['source_reports']:
        file=Path(ref['path']).resolve()
        check('owned selected native report',file.is_relative_to(DOC/'editor_runtime') and file.is_file() and sha(file)==ref['sha256'],str(file))
        report=json.loads(file.read_text('utf-8-sig'));command=file.parent/'commandlet.json'
        run=json.loads(command.read_text('utf-8-sig'))
        check('selected report and native process completed',report.get('status')=='PASS' and run.get('status')=='PASS' and run.get('exit_code')==0)
        R['sources'].extend([dict(path=str(file),sha256=sha(file)),dict(path=str(command),sha256=sha(command))])
        if report.get('phase')=='HeroRev2IntegrationReload' and report.get('blueprint')==HERO:
            integrated_reload=report.get('fresh_process_disk_readback')=='PASS'
        for row in report.get('assets',[]):
            value=package(row['path']);check('owned selected source package',value.startswith('/Game/HarborCity/M5VS2/'))
            asset=disk(value,'.umap' if value.rsplit('/',1)[-1].startswith('L_') else '.uasset')
            check('selected saved source package SHA',asset.is_file() and sha(asset)==row['sha256'],value)
            R['sources'].append(dict(path=str(asset),sha256=sha(asset)))
            selected_asset=selected_asset or value==HERO
    check('selected final Hero fresh-process and saved package proof',integrated_reload and selected_asset)
    # Immutable source bytes for current hero and all VS1/VS2 source assets.
    # Only the new namespace below may gain files. No existing package is saved.
    for relative in ('Config','Content/HarborCity/M5VS1','Content/HarborCity/M5VS2'):
        base=PROJECT/relative
        if base.exists():
            for file in sorted(base.rglob('*')):
                if file.is_file(): PROTECTED[str(file)]=sha(file)
    for path in (HERO,MODE):
        check('current binding source present',disk(path).is_file(),path)
        R['sources'].append(dict(path=str(disk(path)),sha256=sha(disk(path))))
    hero_class=cls(HERO)
    hero=U.get_default_object(hero_class); hero_mesh=hero.get_component_by_class(U.SkeletalMeshComponent)
    R['current_hero_binding']=dict(class_path=hero_class.get_path_name(),
        mesh=hero_mesh.get_editor_property('skeletal_mesh_asset').get_path_name(),
        materials=[hero_mesh.get_material(i).get_path_name() for i in range(hero_mesh.get_num_materials())],
        animation_class=hero_mesh.get_editor_property('anim_class').get_path_name())
    check('native new isolated map',LEVEL.new_level(MAP,False))
    R['empty_native_map_checkpoint']=dict(path=MAP,sha256=sha(disk(MAP,'.umap'))); dump()
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact author world',package(world.get_path_name())==MAP)
    A.set_metadata_tag(world,'HarborCityOwnedBy',OWNER)
    mode_path=ROOT+'/BP_FlightReviewGameMode';mode=A.duplicate_asset(MODE,mode_path)
    check('new private GameMode copy',mode is not None)
    U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',hero_class)
    check('native private GameMode compile',U.BlueprintEditorLibrary.compile_blueprint(mode))
    mode_class=mode.generated_class()
    check('private GameMode exact selected Hero',U.get_default_object(mode_class).get_editor_property('default_pawn_class')==hero_class)
    check('save only private flight GameMode',A.save_loaded_asset(mode,False))
    R['selected_hero']=HERO;R['private_game_mode']=mode_path
    world.get_world_settings().set_editor_property('default_game_mode',mode_class)
    box('Floor',(0,0,-30),(60,60,.6))
    box('Wall',(900,0,500),(.4,12,10))  # near face x=880; capsule center <=845
    box('Roof',(-1500,0,500),(10,10,.4))  # underside 480
    box('NarrowSouth',(0,1730,500),(30,.2,10))
    box('NarrowNorth',(0,1870,500),(30,.2,10))  # clear 120; center y<=1825
    box('Slope',(0,-1600,80),(12,8,.4),pitch=8)
    box('Water',(6000,0,-170),(30,30,.4),water=True)  # actual visibility hit, top=-150
    spawn('PlayerStart',U.PlayerStart,(0,0,98))
    bounds=spawn('Bounds',U.HCM5VS2FlightBounds)
    for name,value in dict(enable_flight=True,sea_level_z=-150.,flight_area_center=U.Vector2D(0,0),
        flight_area_half_extent=U.Vector2D(2500,2500),soft_return_distance=10000.,ceiling_height=10000.,water_clearance=50.).items():
        bounds.set_editor_property(name,value)
    vehicle=spawn('ParkedVehicle',U.HCM1Vehicle,(1800,-1600,105),U.Rotator(yaw=90))
    vehicle.set_editor_property('stable_id',U.Name('VS2_FlightReview_Car'))
    switch=spawn('InteractionSwitch',U.HCM1LightSwitch,(100,130,110))
    switch.set_editor_property('stable_id',U.Name('VS2_FlightReview_Switch'))
    switch.set_editor_property('enabled_light_intensity',0.)  # functional switch; not hero-fill lighting
    experience=spawn('Experience',U.HCM3Experience,(0,0,-100))
    experience.set_editor_property('npcs',[])
    experience.set_editor_property('player_appearance_mesh',None)
    experience.set_editor_property('player_animation_class',None)
    experience.get_editor_property('coffee_parcel').set_static_mesh(None)
    experience.get_editor_property('parking_marker').set_static_mesh(None)
    director=spawn('Director',U.HCM5VS2FlightReviewDirector)
    director.set_editor_property('vehicle',vehicle); director.set_editor_property('interaction_switch',switch)
    light=spawn('Sun',U.DirectionalLight,(0,0,800),U.Rotator(pitch=-35,yaw=145)).get_component_by_class(U.DirectionalLightComponent)
    light.set_mobility(U.ComponentMobility.MOVABLE); light.set_intensity(3.)
    light.set_editor_property('atmosphere_sun_light',True)
    sky=spawn('Sky',U.SkyLight,(0,0,500)).get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE); sky.set_editor_property('real_time_capture',True); sky.set_intensity(.4)
    spawn('Atmosphere',U.SkyAtmosphere)
    post=spawn('FixedExposure',U.PostProcessVolume); post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    for name,value in dict(override_auto_exposure_method=True,auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True,auto_exposure_bias=0.,override_motion_blur_amount=True,motion_blur_amount=0.).items():
        settings.set_editor_property(name,value)
    post.set_editor_property('settings',settings)
    before=inventory()
    check('native save new fixture',LEVEL.save_current_level())
    R['assets']=[dict(path=MAP,sha256=sha(disk(MAP,'.umap'))),dict(path=mode_path,sha256=sha(disk(mode_path)))]
    check('native reload fixture',LEVEL.load_level(MAP))
    check('persisted actor inventory exact',inventory()==before)
    w=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('persisted exact current hero GameMode',w.get_world_settings().get_editor_property('default_game_mode')==mode_class)
    directors=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2FlightReviewDirector)]
    check('persisted director references',len(directors)==1 and
        directors[0].get_editor_property('vehicle').get_editor_property('stable_id')==U.Name('VS2_FlightReview_Car') and
        directors[0].get_editor_property('interaction_switch').get_editor_property('stable_id')==U.Name('VS2_FlightReview_Switch'))
    R['status']='PASS'
except Exception:
    R['status']='FAIL'; R['error']=traceback.format_exc(); U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['source_guard']=dict(protected_files=len(PROTECTED),changes=changed)
    if changed: R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status']!='PASS': raise RuntimeError('FlightReview native author failed; preserve this evidence')
