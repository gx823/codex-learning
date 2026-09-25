"""Native isolated Q/R gameplay map. Root runs NPCGameplay phase serially.

Only this new map and per-attempt neutral materials are saved. Prior source
integration, in-place and PHYS repairs are byte-bound. No gameplay result is
claimed by authoring. Failed empty-map saves are journaled before further work.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import shutil
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
NPC_ROOT='/Game/HarborCity/M5VS2/NPC'
ROOT=NPC_ROOT+'/GameplayReview'; MAP=ROOT+'/L_NPCGameplay'
OWNER='HarborCity_M5_VS2_NPCGameplayReview'; KEY='HarborCityOwnedBy'
A=U.EditorAssetLibrary; E=U.MaterialEditingLibrary
ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
CMD=U.SystemLibrary.get_command_line()


def argument(name):
    rows=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',CMD)
    assert len(rows)==1, 'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT=Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to((DOC/'editor_runtime').resolve()) and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ASSET_ROOT=ROOT+'/Attempt_'+hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
R=dict(status='RUNNING',map=MAP,owner=OWNER,checks=[],assets=[],actors=[],inputs={},map_checkpoints=[],
       runtime='NOT_RUN',visual='NOT_RUN_NULLRHI',navigation='NOT_RUN',user_art_approval='USER_REVIEW',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       scope='Isolated 30m neutral gameplay test map; no existing map/default configuration saved.',
       limitations=['Authoring is not runtime collision, recovery, death, pause or visual acceptance.',
                    'Director vehicle-impact method calls use the real gameplay reaction path but are not actual vehicle contacts.',
                    'Vehicle is the existing native HCM1Vehicle used by the approved city; no new vehicle model or physics.'])
PROTECTED={}; CREATED=[]


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')


def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed));dump()
    if not ok: raise RuntimeError(name+': '+str(observed))


def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024),b''):digest.update(data)
    return digest.hexdigest()


def package(path):return str(path).split('.')[0]
def binding(path):return dict(path=str(path),sha256=sha(path))
def disk(path,extension='.uasset'):
    path=package(path)
    assert path.startswith('/Game/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix(extension)


def latest_pass(pattern,predicate=lambda x:True):
    for file in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        value=json.loads(file.read_text(encoding='utf-8-sig'))
        if value.get('status')=='PASS' and predicate(value):return file,value
    raise RuntimeError('No actual PASS report '+pattern)


def protect_file(path,expected=None):
    path=Path(path);check('protected input exists',path.is_file(),str(path));actual=sha(path)
    if expected:check('input matches native saved evidence',actual==expected,str(path))
    PROTECTED[str(path)]=actual


def load(path):
    obj=A.load_asset(path);check('load native asset',obj is not None,path);return obj


def generated_class(path):
    path=package(path);obj=U.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C')
    check('load exact generated class',obj is not None,path);return obj


def xyz(v):return [float(v.x),float(v.y),float(v.z)]


def inventory():
    return sorted([dict(name=a.get_name(),class_path=a.get_class().get_path_name(),
                        tags=sorted(str(t) for t in a.get_editor_property('tags')))
                   for a in ACT.get_all_level_actors()
                   if a.get_class().get_name() not in ('WorldSettings','Brush','DefaultPhysicsVolume')],key=lambda x:x['name'])


def map_checkpoint(kind):
    # Called only immediately after native new_level auto-save or explicit save.
    file=disk(MAP,'.umap');check('checkpoint map actually saved',file.is_file(),kind)
    R['map_checkpoints'].append(dict(kind=kind,path=MAP,sha256=sha(file),actors=inventory()))
    dump()


def prepare_map():
    file=disk(MAP,'.umap')
    if file.exists() or A.does_asset_exist(MAP):
        check('existing map present on disk',file.is_file())
        current=sha(file);match=None
        for report_path in sorted((DOC/'editor_runtime').glob('author_*_NPCGameplay/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
            if report_path.resolve()==(OUT/'author_result.json').resolve():continue
            previous=json.loads(report_path.read_text(encoding='utf-8-sig'))
            if previous.get('map')!=MAP or previous.get('owner')!=OWNER:continue
            for row in previous.get('map_checkpoints',[]):
                if row.get('path')==MAP and row.get('sha256')==current:
                    match=(report_path,row);break
            if match:break
        check('existing map equals a recorded native checkpoint, no user edits',match is not None,current)
        report_path,row=match
        backup=Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/OUT.name/'L_NPCGameplay.umap'
        check('fresh exact map backup',not backup.exists());backup.parent.mkdir(parents=True,exist_ok=False)
        shutil.copy2(file,backup);check('map backup byte exact',sha(backup)==current)
        R['prior_owned_map']=dict(evidence=binding(report_path),checkpoint=row,backup=binding(backup));dump()
        check('load only fixed isolated map',LEVEL.load_level(MAP))
        world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        check('map owner persisted or proven first native empty auto-save',A.get_metadata_tag(world,KEY)==OWNER
              or row['kind']=='EMPTY_NATIVE_AUTO_SAVE')
        check('reloaded prior checkpoint actor inventory exact',inventory()==row['actors'])
        for actor in ACT.get_all_level_actors():
            if actor.get_class().get_name() in ('WorldSettings','Brush','DefaultPhysicsVolume'):continue
            check('destroy only recorded owned-map actor',ACT.destroy_actor(actor),actor.get_name())
    else:
        check('native create isolated map',LEVEL.new_level(MAP,False))
        current_world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
        check('new_level created exact requested map',package(current_world.get_path_name())==MAP)
        # new_level saves before metadata can be attached. Journal that exact
        # initial file now, so even a later metadata/material failure is recoverable.
        map_checkpoint('EMPTY_NATIVE_AUTO_SAVE')
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact current map',package(world.get_path_name())==MAP)
    A.set_metadata_tag(world,KEY,OWNER)
    world.get_world_settings().set_editor_property('default_game_mode',U.HCM1GameMode)
    check('native persist empty ownership checkpoint',LEVEL.save_current_level())
    map_checkpoint('OWNED_EMPTY_NATIVE_SAVE')
    return world


def spawn(label,kind,pos=(0,0,0),rotation=None):
    check('bounded isolated actor count',len(CREATED)<32)
    actor=ACT.spawn_actor_from_class(kind,U.Vector(*pos),rotation or U.Rotator(),False)
    check('spawn '+label,actor is not None);CREATED.append(actor)
    actor.set_actor_label('HC_M5VS2_Gameplay_'+label);actor.set_editor_property('tags',[U.Name(OWNER)])
    R['actors'].append(dict(name=actor.get_name(),label=actor.get_actor_label(),class_path=actor.get_class().get_path_name(),location_cm=pos))
    return actor


def stage_material():
    check('fresh per-attempt material namespace',not A.does_directory_exist(ASSET_ROOT) and not disk(ASSET_ROOT+'/unused').parent.exists())
    material=U.AssetToolsHelpers.get_asset_tools().create_asset('M_NeutralFloor',ASSET_ROOT,U.Material,U.MaterialFactoryNew())
    check('create neutral material',material is not None)
    color=E.create_material_expression(material,U.MaterialExpressionConstant3Vector,0,0)
    check('create color node',color is not None);color.set_editor_property('constant',U.LinearColor(.18,.18,.18,1))
    rough=E.create_material_expression(material,U.MaterialExpressionConstant,0,120)
    check('create roughness node',rough is not None);rough.set_editor_property('r',.85)
    check('native floor color',E.connect_material_property(color,'',U.MaterialProperty.MP_BASE_COLOR))
    check('native floor roughness',E.connect_material_property(rough,'',U.MaterialProperty.MP_ROUGHNESS))
    E.recompile_material(material);A.set_metadata_tag(material,KEY,OWNER)
    check('native save new material',A.save_loaded_asset(material,False))
    R['assets'].append(dict(path=package(material.get_path_name()),sha256=sha(disk(material.get_path_name()))))
    return material


def author_stage(specs):
    floor=spawn('Floor',U.StaticMeshActor,(0,0,-20));mesh=floor.get_component_by_class(U.StaticMeshComponent)
    mesh.set_mobility(U.ComponentMobility.STATIC);mesh.set_static_mesh(load('/Engine/BasicShapes/Cube'))
    mesh.set_material(0,stage_material());mesh.set_collision_profile_name('BlockAll')
    floor.set_actor_scale3d(U.Vector(30,30,.4))
    spawn('PlayerStart',U.PlayerStart,(1000,0,95),U.Rotator(yaw=180))
    sun=spawn('MainLight',U.DirectionalLight,(300,0,500),U.Rotator(pitch=-28,yaw=155))
    light=sun.get_component_by_class(U.DirectionalLightComponent);light.set_mobility(U.ComponentMobility.MOVABLE)
    light.set_light_color(U.LinearColor(1,1,1,1));light.set_intensity(3.)
    light.set_editor_property('atmosphere_sun_light',True)
    sky=spawn('SkyLight',U.SkyLight,(0,0,500)).get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE);sky.set_editor_property('real_time_capture',True);sky.set_intensity(.4)
    spawn('Atmosphere',U.SkyAtmosphere)
    post=spawn('FixedExposure',U.PostProcessVolume);post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    for name,value in dict(override_auto_exposure_method=True,auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
            override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
            override_auto_exposure_bias=True,auto_exposure_bias=0.,override_motion_blur_amount=True,motion_blur_amount=0.,
            override_bloom_intensity=True,bloom_intensity=.12).items():settings.set_editor_property(name,value)
    post.set_editor_property('settings',settings)
    region=spawn('AllowedRegion',U.HCM3NavRegion,(0,0,100))
    region.set_editor_property('stable_id',U.Name('M5VS2_Gameplay_Allowed'))
    region.set_editor_property('kind',U.HCM3NavRegionKind.ALLOWED);region.set_editor_property('half_extent',U.Vector(1420,1420,400))
    bounds=spawn('NavBounds',U.NavMeshBoundsVolume,(0,0,150));_,extent=bounds.get_actor_bounds(False)
    check('native navigation brush nonempty',min(xyz(extent))>0,xyz(extent))
    bounds.set_actor_scale3d(U.Vector(1490/extent.x,1490/extent.y,400/extent.z))
    _,actual=bounds.get_actor_bounds(False)
    check('exact bounded native navigation volume',max(abs(a-b) for a,b in zip(xyz(actual),(1490,1490,400)))<1,xyz(actual))
    people=[]
    for letter,y in [('Q',-750.),('R',750.)]:
        spec=specs[letter]
        npc=spawn('NPC_'+letter,generated_class(spec['blueprint']),(0,y,float(spec['capsule_half_height_cm'])+2))
        npc.set_editor_property('stable_id',U.Name('M5VS2_'+letter));npc.set_editor_property('stationary',True)
        npc.set_editor_property('talkable',True);npc.set_editor_property('navigation_region',region)
        check('exact integrated profile',npc.get_editor_property('npc_profile').get_path_name()==spec['profile'])
        people.append(npc)
    # This is the actual gameplay vehicle class used by M5 city author. The
    # migrated engine template BP is a different parent and is deliberately not used.
    vehicle=spawn('Vehicle',U.HCM1Vehicle,(-1350,0,100),U.Rotator(yaw=90))
    vehicle.set_editor_property('stable_id',U.Name('M5VS2_Gameplay_Vehicle'))
    exp=spawn('Experience',U.HCM3Experience,(0,0,-100))
    exp.set_editor_property('npcs',people);exp.set_editor_property('player_appearance_mesh',None)
    exp.set_editor_property('player_animation_class',None)
    exp.get_editor_property('coffee_parcel').set_static_mesh(None)
    exp.get_editor_property('parking_marker').set_static_mesh(None)
    director=spawn('Director',U.HCM5VS2NPCGameplayReviewDirector,(0,0,-100))
    director.set_editor_property('specimens',people);director.set_editor_property('experience',exp)
    director.set_editor_property('vehicle',vehicle)
    R['authored_placements']=dict(specimen_separation_cm=1500,minimum_vehicle_to_specimen_xy_cm=(1350**2+750**2)**.5,
        minimum_player_to_specimen_xy_cm=1250,clear_sample_radius_cm=600,floor_dimensions_cm=[3000,3000,40])


# Exact source/repair hash guard copied from the existing corner author.
def npc_specimen(letter):
    report_file, report = latest_pass('author_*_NPCIntegration' + letter, lambda d: d.get('sample') == letter)
    spec = report['specimen']
    check('new isolated NPC integration', package(spec['blueprint']).startswith(NPC_ROOT + '/AvatarSample_' + letter + '/Runtime_'))
    # Only exact, evidenced subsequent animation/PHYS repairs may supersede
    # the original integration hashes. Never accept unrecorded current bytes.
    expected = {package(a['path']): a['sha256'] for a in report['assets']}
    locomotion_repairs = []
    animation_root = report['destination'] + '/Animation/'
    allowed_clips = {animation_root + name + '_NPC' + letter for name in
                     ('MM_Idle', 'MF_Unarmed_Walk_Fwd', 'MF_Unarmed_Jog_Fwd')}
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCInPlace' + letter + '/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if (data.get('status') != 'PASS' or data.get('sample') != letter
                or Path(data.get('integration_evidence', '')).resolve() != report_file.resolve()):
            continue
        check('in-place repair exact integration identity', data.get('destination') == report['destination']
              and data.get('specimen') == spec)
        saved_rows = data.get('assets', [])
        saved_paths = [package(a['path']) for a in saved_rows]
        check('in-place repair only unique known locomotion clips', bool(saved_paths)
              and len(saved_paths) == len(set(saved_paths)) and set(saved_paths) <= allowed_clips)
        backups = data.get('animation_backups', [])
        trajectories = data.get('locomotion_in_place', [])
        transitions = []
        for saved, path in zip(saved_rows, saved_paths):
            before = [a for a in backups if package(a['path']) == path]
            native = [a for a in trajectories if package(a['target']) == path]
            check('in-place repair has unique before/native evidence', len(before) == 1 and len(native) == 1, path)
            before, native = before[0], native[0]
            check('in-place repair begins at integration hash', path in expected
                  and before.get('sha256') == expected[path], path)
            backup = Path(before['backup']).resolve()
            backup_root = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / file.parent.name
            check('in-place repair exact backup path', backup.parent == backup_root.resolve()
                  and backup.name == disk(path).name, str(backup))
            protect_file(backup, expected[path])
            applied = native.get('apply', {})
            readback = native.get('after_native_save_or_unchanged', {})
            check('in-place native applied and saved readback', saved.get('class_name') == 'AnimSequence'
                  and applied.get('status') == 'PASS' and applied.get('applied') is True
                  and applied.get('all_unselected_tracks_exactly_preserved') is True
                  and applied.get('selected_tracks_z_scale_exactly_preserved') is True
                  and readback.get('status') == 'PASS' and readback.get('needs_repair') is False
                  and package(applied.get('target', '')) == path and package(readback.get('target', '')) == path
                  and re.fullmatch(r'[0-9a-f]{64}', saved.get('sha256', '')) is not None, path)
            transitions.append(dict(path=path, before_sha256=expected[path], after_sha256=saved['sha256'],
                                    backup=binding(backup)))
            expected[path] = saved['sha256']
        locomotion_repairs.append(dict(**binding(file), integration_evidence_sha256=sha(report_file), assets=transitions))
        break
    repairs = []
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCPhysicsRepair/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') != 'PASS':
            continue
        row = next((a for a in data.get('assets', []) if a.get('sample') == letter), None)
        if row and row.get('integration_evidence_sha256') == sha(report_file) and row.get('after_save'):
            saved = row['after_save']
            check('repair targets exact specimen physics', package(saved['path']) == package(spec['physics']))
            expected[package(saved['path'])] = saved['sha256']
            repairs.append(binding(file))
            break
    for path, expected_sha in expected.items():
        protect_file(disk(path), expected_sha)
    for key in ('mesh', 'profile', 'blueprint', 'anim_blueprint', 'physics'):
        protect_file(disk(spec[key]))
    R['inputs']['NPC_' + letter] = dict(**binding(report_file), specimen=spec,
                                      locomotion_repairs=locomotion_repairs, physics_repairs=repairs)
    return spec


try:
    check('explicit NPCGameplay phase',argument('M5AuthorPhase')=='NPCGameplay')
    check('exact current project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty map',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('new native helpers reflected',hasattr(U,'HCM5VS2NPCGameplayReviewDirector') and hasattr(U,'HCM5VS2NPCGameplayEditor'))
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([NPC_ROOT],True)
    specs={letter:npc_specimen(letter) for letter in ('Q','R')}
    for spec in specs.values():
        check('exact integrated Blueprint owner',A.get_metadata_tag(load(spec['blueprint']),KEY)=='HarborCity_M5_VS2_NPC_Integration')
    for file in (PROJECT/'Config').glob('*.ini'):protect_file(file)
    for file in (PROJECT/'Content').rglob('*.umap'):
        if file.resolve()!=disk(MAP,'.umap').resolve():protect_file(file)
    for file in (PROJECT/'Content/SportsCar').glob('*.uasset'):protect_file(file)
    for name in ('ue_m5_vs2_npc_gameplay_author.py','ue_m5_vs2_harbor_corner_author.py'):
        R['inputs'][name]=binding(WORK/'tools'/name)
    for name in ('HCM5VS2NPCGameplayEditor.h','HCM5VS2NPCGameplayEditor.cpp','HCM5VS2NPCGameplayReviewDirector.h','HCM5VS2NPCGameplayReviewDirector.cpp'):
        R['inputs'][name]=binding(PROJECT/'Source/HarborCity/M5VS2'/name)
    R['protected_source_sha256_before']=dict(PROTECTED);dump()
    world=prepare_map();author_stage(specs)
    R['navigation_build']=json.loads(U.HCM5VS2NPCGameplayEditor.build_navigation(world));dump()
    check('actual synchronous navigation build and Q/R paths',R['navigation_build'].get('status')=='PASS',R['navigation_build'])
    for actor in ACT.get_all_level_actors():
        if isinstance(actor,U.RecastNavMesh):actor.set_editor_property('tags',[U.Name(OWNER)])
    check('native save only isolated gameplay map',LEVEL.save_current_level());map_checkpoint('COMPLETE_NATIVE_SAVE')
    R['assets'].append(dict(path=MAP,sha256=sha(disk(MAP,'.umap'))));dump()
    check('reload saved isolated gameplay map',LEVEL.load_level(MAP))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('saved world owner',A.get_metadata_tag(world,KEY)==OWNER)
    actors=ACT.get_all_level_actors()
    directors=[a for a in actors if isinstance(a,U.HCM5VS2NPCGameplayReviewDirector)]
    exps=[a for a in actors if isinstance(a,U.HCM3Experience)]
    people=[a for a in actors if isinstance(a,U.HCM5VS2NPC)]
    vehicles=[a for a in actors if isinstance(a,U.HCM1Vehicle)]
    check('exact saved actor counts',len(directors)==1 and len(exps)==1 and len(people)==2 and len(vehicles)==1)
    director=directors[0];exp=exps[0]
    check('saved exact Q/R ordered references',[str(a.get_editor_property('stable_id')) for a in director.get_editor_property('specimens')]==['M5VS2_Q','M5VS2_R'])
    check('saved Experience and Vehicle bindings',director.get_editor_property('experience')==exp and director.get_editor_property('vehicle')==vehicles[0])
    check('saved Experience exact NPC references',list(exp.get_editor_property('npcs'))==list(director.get_editor_property('specimens')))
    check('saved no appearance or prop override',exp.get_editor_property('player_appearance_mesh') is None
          and exp.get_editor_property('player_animation_class') is None
          and exp.get_editor_property('coffee_parcel').get_editor_property('static_mesh') is None
          and exp.get_editor_property('parking_marker').get_editor_property('static_mesh') is None)
    R['navigation_saved_readback']=json.loads(U.HCM5VS2NPCGameplayEditor.inspect_navigation(world));dump()
    check('saved baked nav and real Q/R paths, no rebuild',R['navigation_saved_readback'].get('status')=='PASS'
          and R['navigation_saved_readback'].get('native_build_requested') is False,R['navigation_saved_readback'])
    check('all original NPC/vehicle/config/map bytes unchanged',all(Path(p).is_file() and sha(p)==h for p,h in PROTECTED.items()))
    R['navigation']='PASS_NATIVE_BAKE_SAVE_RELOAD_PATHS_RUNTIME_NOT_RUN'
    R['runtime_arguments']=[MAP,'-game','-M5VS2NPCGameplayReview','-M5VS2AutoQuit',
        '-M5VS2EvidenceDir="D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/NPCGameplay_GAME_UNIQUE"',
        '-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_NPCGameplay_UNIQUE','-ResX=1920','-ResY=1080','-windowed','-language=en']
    R['status']='PASS';R['pass_scope']='Only native isolated map/material author, baked navigation and save/reload binding/path checks. Runtime and visuals NOT_RUN.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['source_hash_changes']=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('NPC gameplay author failed; see author_result.json')
