"""Native isolated Q/R Reaction R2 maps; preserve the original gameplay map. Root runs NPCReactionR2 phase serially.

Only this new map and per-attempt neutral materials are saved. Prior source
integration, in-place and PHYS repairs are byte-bound. No gameplay result is
claimed by authoring. Failed empty-map saves are journaled before further work.
"""
from pathlib import Path
import ast
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
ROOT=NPC_ROOT+'/ReactionReviewR2'
PHASE_MATCH=re.findall(r'(?:^|\s)-M5AuthorPhase=(\S+)',U.SystemLibrary.get_command_line())
assert len(PHASE_MATCH)==1
PHASE=PHASE_MATCH[0]
PRIVATE_MATCH=re.fullmatch(r'NPCReactionR2Idle(Open|Wall|Slope|Narrow)(QR|JT|UV|WX)',PHASE)
IS_IDLE_PAIR=PRIVATE_MATCH is not None
assert IS_IDLE_PAIR or PHASE in tuple('NPCReactionR2'+v for v in ('Open','Wall','Slope','Narrow'))
SITE,PAIR=PRIVATE_MATCH.groups() if IS_IDLE_PAIR else (PHASE.removeprefix('NPCReactionR2'),'QR')
MAP=ROOT+'/L_NPCReaction_'+SITE
OWNER='HarborCity_M5_VS2_NPCReactionReviewR2'; KEY='HarborCityOwnedBy'
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
if IS_IDLE_PAIR:
    ASSET_ROOT=ROOT+'/IdlePairs/Attempt_'+hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
    MAP=ASSET_ROOT+'/L_NPCReaction_'+SITE+'_'+PAIR
R=dict(schema='HarborCity.M5VS2.NPCReactionR2.Author.v1',status='RUNNING',phase=PHASE,map=MAP,owner=OWNER,
       site=SITE,review_pair=PAIR,roster_version='FINAL_IDLE_R2_PAIR' if IS_IDLE_PAIR else 'LEGACY_QR',
       checks=[],assets=[],actors=[],inputs={},map_checkpoints=[],roster=[],
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


def package(path):return (path.get_path_name() if hasattr(path,'get_path_name') else str(path)).split('.')[0]
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

cls=generated_class

def private_pair_specimens():
    source=WORK/'tools/ue_m5_vs2_npc_cast_review_author.py';text=source.read_text('utf-8')
    wanted=('accept_npc_getup_transition','npc_specimen','cast_specimen','consume_idle_reload')
    nodes=[n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name in wanted]
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes)
    check('exact four read-only provenance/idle consumer functions',tuple(n.name for n in nodes)==wanted
          and hashlib.sha256(code.encode()).hexdigest()=='db28318a47f5a2015ccc62558b6544ad7de854e2c82e4bf1b22b861df79bac37')
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source)+'::provenance_only','exec'),globals())
    R['inputs']['idle_cast_consumer']=binding(source)
    specs={letter:(npc_specimen(letter) if letter in ('Q','R') else cast_specimen(letter)) for letter in PAIR}
    return {letter:consume_idle_reload(letter,specs[letter]) for letter in PAIR}


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
    if IS_IDLE_PAIR:check('new unique private pair map never replaces existing work',not file.exists() and not A.does_asset_exist(MAP))
    if file.exists() or A.does_asset_exist(MAP):
        check('existing map present on disk',file.is_file())
        current=sha(file);match=None
        for report_path in sorted((DOC/'editor_runtime').glob('author_*_NPCReactionR2'+SITE+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
            if report_path.resolve()==(OUT/'author_result.json').resolve():continue
            previous=json.loads(report_path.read_text(encoding='utf-8-sig'))
            if previous.get('map')!=MAP or previous.get('owner')!=OWNER:continue
            for row in previous.get('map_checkpoints',[]):
                if row.get('path')==MAP and row.get('sha256')==current:
                    match=(report_path,row);break
            if match:break
        check('existing map equals a recorded native checkpoint, no user edits',match is not None,current)
        report_path,row=match
        backup=Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/OUT.name/('L_NPCReaction_'+SITE+'.umap')
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
    check('bounded isolated actor count',len(CREATED)<40)
    actor=ACT.spawn_actor_from_class(kind,U.Vector(*pos),rotation or U.Rotator(),False)
    check('spawn '+label,actor is not None);CREATED.append(actor)
    actor.set_actor_label('HC_M5VS2_Gameplay_'+label);actor.set_editor_property('tags',[U.Name(OWNER)])
    R['actors'].append(dict(name=actor.get_name(),label=actor.get_actor_label(),class_path=actor.get_class().get_path_name(),location_cm=pos))
    return actor


def stage_material():
    if IS_IDLE_PAIR:check('fresh private pair material',not A.does_asset_exist(ASSET_ROOT+'/M_NeutralFloor'))
    else:check('fresh per-attempt material namespace',not A.does_directory_exist(ASSET_ROOT) and not disk(ASSET_ROOT+'/unused').parent.exists())
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
    # These are actual native collision obstacles, not a label on the same open floor.
    def obstacle(label,pos,scale,rotation=None):
        actor=spawn(label,U.StaticMeshActor,pos,rotation)
        comp=actor.get_component_by_class(U.StaticMeshComponent)
        comp.set_mobility(U.ComponentMobility.STATIC);comp.set_static_mesh(load('/Engine/BasicShapes/Cube'))
        comp.set_material(0,mesh.get_material(0));comp.set_collision_profile_name('BlockAll')
        actor.set_actor_scale3d(U.Vector(*scale))
    for letter,y in zip(PAIR,(-750.,750.)):
        if SITE=='Wall':obstacle('Wall_'+letter,(150,y,120),(.2,5,2.4))
        elif SITE=='Narrow':
            for sign in (-1,1):obstacle('Narrow_'+letter+'_'+str(sign),(0,y+sign*105,120),(10,.2,2.4))
        elif SITE=='Slope':obstacle('Ramp_'+letter,(0,y,60),(10,10,.4),U.Rotator(pitch=8))
    R['site_geometry']=dict(site=SITE,wall_face_distance_cm=140 if SITE=='Wall' else None,
        narrow_clear_width_cm=190 if SITE=='Narrow' else None,slope_pitch_degrees=8 if SITE=='Slope' else 0)

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
    for letter,y in zip(PAIR,(-750.,750.)):
        spec=specs[letter]
        npc=spawn('NPC_'+letter,generated_class(spec['blueprint']),(0,y,float(spec['capsule_half_height_cm'])+2+(80 if SITE=='Slope' else 0)))
        npc.set_editor_property('stable_id',U.Name('M5VS2_'+letter));npc.set_editor_property('stationary',True)
        npc.set_editor_property('talkable',True);npc.set_editor_property('navigation_region',region)
        check('exact integrated profile',npc.get_editor_property('npc_profile').get_path_name()==spec['profile'])
        body=npc.get_component_by_class(U.SkeletalMeshComponent)
        check('actual final NPC class mesh and animation',package(body.get_editor_property('skeletal_mesh_asset'))==package(spec['mesh'])
              and package(body.get_editor_property('anim_class'))==package(spec['anim_blueprint'])
              and package(body.get_editor_property('physics_asset_override'))==package(spec['physics']))
        R['roster'].append(dict(sample=letter,stable_id='M5VS2_'+letter,blueprint=package(spec['blueprint']),
            mesh=package(spec['mesh']),anim_blueprint=package(spec['anim_blueprint']),profile=package(spec['profile']),physics=package(spec['physics'])))
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
    director=spawn('Director',U.HCM5VS2NPCReactionReviewDirector,(0,0,-100))
    director.set_editor_property('specimens',people);director.set_editor_property('experience',exp)
    director.set_editor_property('vehicle',vehicle)
    director.set_editor_property('probe_site',SITE)
    if IS_IDLE_PAIR:
        director.set_editor_property('review_pair',PAIR)
        director.set_editor_property('occlusion_aware_review_camera',True)
        if SITE=='Slope':
            ramps=[next(a for a in CREATED if a.get_actor_label()=='HC_M5VS2_Gameplay_Ramp_'+letter) for letter in PAIR]
            director.set_editor_property('require_ramp_support',True)
            director.set_editor_property('ramp_support_actors',ramps)
    R['authored_placements']=dict(specimen_separation_cm=1500,minimum_vehicle_to_specimen_xy_cm=(1350**2+750**2)**.5,
        minimum_player_to_specimen_xy_cm=1250,clear_sample_radius_cm=600,floor_dimensions_cm=[3000,3000,40])


# Exact source/repair hash guard copied from the existing corner author.
def document(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def accept_npc_getup_transition(letter, spec, integration_file, expected):
    """Only a saved, backed-up native Apply may replace this exact BP hash."""
    path = package(spec['blueprint'])
    current = sha(disk(path))
    if current == expected[path]:
        return []
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCGetUpBindingApply/getup_binding_manifest.json'),
                       key=lambda p: p.stat().st_mtime, reverse=True):
        manifest = document(file)
        if (manifest.get('schema') != 'HarborCity.M5VS2.NPCGetUpBinding.v1' or manifest.get('status') != 'PASS'
                or manifest.get('phase') != 'NPCGetUpBindingApply'):
            continue
        rows = manifest.get('transitions', [])
        selected = [row for row in rows if row.get('sample') == letter and package(row.get('path', '')) == path
                    and row.get('integration_evidence_sha256') == sha(integration_file)]
        if not selected:
            continue
        check('getup manifest contains exactly two unique Q/R BP transitions', len(rows) == 2
              and {row.get('sample') for row in rows} == {'Q', 'R'}
              and len({package(row.get('path', '')) for row in rows}) == 2
              and all(row.get('class_name') == 'Blueprint' and package(row.get('path', '')).startswith(
                  NPC_ROOT + '/AvatarSample_' + row['sample'] + '/Runtime_') for row in rows)
              and len(selected) == 1)
        row = selected[0]
        check('getup exact existing-before and current-after hashes', row.get('before_sha256') == expected[path]
              and row.get('after_sha256') == current
              and re.fullmatch(r'[0-9a-f]{64}', current) is not None, path)
        evidence_file = Path(manifest['evidence']['path']).resolve()
        check('getup manifest bound to adjacent exact Apply report', evidence_file == (file.parent/'author_result.json').resolve()
              and sha(evidence_file) == manifest['evidence']['sha256'])
        evidence = document(evidence_file)
        check('getup Apply recorded only actual successful BP saves', evidence.get('status') == 'PASS'
              and evidence.get('phase') == 'NPCGetUpBindingApply' and evidence.get('applied') is True
              and evidence.get('transitions') == rows and evidence.get('dry_run') == manifest.get('dry_run')
              and evidence.get('preservation', {}).get('unexpected_changed_files') == []
              and all(evidence.get(name) == 0 for name in ('map_writes', 'animation_writes', 'physics_writes'))
              and all(c.get('status') == 'PASS' for c in evidence.get('checks', [])))
        saved = evidence.get('assets', [])
        check('getup report assets exactly match the two transition hashes', len(saved) == 2
              and {(package(a['path']), a['sha256'], a.get('class_name')) for a in saved}
              == {(package(a['path']), a['after_sha256'], 'Blueprint') for a in rows})
        dry_file = Path(manifest['dry_run']['path']).resolve()
        dry = document(dry_file)
        check('getup Apply uses exact successful unapplied DryRun', sha(dry_file) == manifest['dry_run']['sha256']
              and dry.get('status') == 'PASS' and dry.get('phase') == 'NPCGetUpBindingDryRun'
              and dry.get('applied') is False and dry.get('assets') == [] and dry.get('transitions') == [])
        dry_plans = [p for p in dry.get('plans', []) if p.get('sample') == letter]
        apply_plans = [p for p in evidence.get('plans', []) if p.get('sample') == letter]
        check('getup exact preapproved specimen plan', len(dry_plans) == len(apply_plans) == 1
              and package(dry_plans[0]['blueprint']) == path and dry_plans[0]['before_sha256'] == expected[path]
              and dry_plans[0]['integration_sha256'] == sha(integration_file)
              and {k:v for k,v in apply_plans[0].items() if k != 'backup'} == dry_plans[0])
        backup = Path(row['backup']['path']).resolve()
        backup_root = (Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / file.parent.name).resolve()
        check('getup exact private original BP backup', backup.parent == backup_root and backup.name == disk(path).name
              and row['backup']['sha256'] == expected[path] and apply_plans[0].get('backup') == row['backup'])
        protect_file(backup, expected[path])
        retarget_file = Path(row['retarget_evidence']['path']).resolve()
        retarget = document(retarget_file)
        check('getup exact preserved four-action retarget source', row['retarget_evidence'] == dry_plans[0]['retarget']
              and sha(retarget_file) == row['retarget_evidence']['sha256'] and retarget.get('status') == 'PASS'
              and retarget.get('phase') == 'GASRecoveryRetarget' + letter
              and retarget.get('preservation', {}).get('changed_files') == [])
        native = row.get('native_cdo_readback', {})
        clips = native.get('get_up_clips', [])
        check('getup exact enabled four-direction native CDO readback', native.get('use_authored_get_up') is True
              and native.get('get_up_pose_data_verified') is True and len(clips) == 4
              and {c.get('direction') for c in clips} == {'SUPINE', 'PRONE', 'LEFT', 'RIGHT'}
              and {package(c.get('animation', '')) for c in clips}
              == {package(c['path']) for c in retarget.get('target_audits', [])})
        for asset in retarget.get('assets', []):
            protect_file(disk(asset['path']), asset['sha256'])
        expected[path] = current
        return [dict(**binding(file), apply_evidence=binding(evidence_file), dry_run=binding(dry_file),
                     blueprint=path, before_sha256=row['before_sha256'], after_sha256=current,
                     backup=binding(backup), native_cdo_readback=native)]
    raise RuntimeError('NPC BP changed without an exact successful GetUp Apply transition: ' + path)


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
    getup_repairs = accept_npc_getup_transition(letter, spec, report_file, expected)
    for path, expected_sha in expected.items():
        protect_file(disk(path), expected_sha)
    for key in ('mesh', 'profile', 'blueprint', 'anim_blueprint', 'physics'):
        protect_file(disk(spec[key]))
    R['inputs']['NPC_' + letter] = dict(**binding(report_file), specimen=spec,
                                      locomotion_repairs=locomotion_repairs, physics_repairs=repairs,
                                      getup_bindings=getup_repairs)
    return spec


try:
    check('explicit NPCReactionR2 phase',argument('M5AuthorPhase')==PHASE)
    check('exact current project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty map',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('new native helpers reflected',hasattr(U,'HCM5VS2NPCReactionReviewDirector') and hasattr(U,'HCM5VS2NPCGameplayEditor'))
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([NPC_ROOT],True)
    specs=private_pair_specimens() if IS_IDLE_PAIR else {letter:npc_specimen(letter) for letter in ('Q','R')}
    for spec in specs.values():
        check('exact integrated Blueprint owner',A.get_metadata_tag(load(spec['blueprint']),KEY)==('HarborCity_M5_VS2_NPCIdleR2' if IS_IDLE_PAIR else 'HarborCity_M5_VS2_NPC_Integration'))
    for file in (PROJECT/'Config').glob('*.ini'):protect_file(file)
    for file in (PROJECT/'Content').rglob('*.umap'):
        if file.resolve()!=disk(MAP,'.umap').resolve():protect_file(file)
    for file in (PROJECT/'Content/SportsCar').glob('*.uasset'):protect_file(file)
    for name in ('ue_m5_vs2_npc_reaction_r2_author.py','ue_m5_vs2_harbor_corner_author.py'):
        R['inputs'][name]=binding(WORK/'tools'/name)
    for name in ('HCM5VS2NPCGameplayEditor.h','HCM5VS2NPCGameplayEditor.cpp','HCM5VS2NPCReactionReviewDirector.h','HCM5VS2NPCReactionReviewDirector.cpp'):
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
    directors=[a for a in actors if isinstance(a,U.HCM5VS2NPCReactionReviewDirector)]
    exps=[a for a in actors if isinstance(a,U.HCM3Experience)]
    people=[a for a in actors if isinstance(a,U.HCM5VS2NPC)]
    vehicles=[a for a in actors if isinstance(a,U.HCM1Vehicle)]
    check('exact saved actor counts',len(directors)==1 and len(exps)==1 and len(people)==2 and len(vehicles)==1)
    director=directors[0];exp=exps[0]
    check('saved exact selected pair ordered references',[str(a.get_editor_property('stable_id')) for a in director.get_editor_property('specimens')]==['M5VS2_'+v for v in PAIR])
    check('saved explicit director roster mode',str(director.get_editor_property('review_pair'))==(PAIR if IS_IDLE_PAIR else ''))
    check('saved camera visibility probe explicitly enabled only for new private pairs',
          bool(director.get_editor_property('occlusion_aware_review_camera'))==IS_IDLE_PAIR)
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
    R['runtime_arguments']=[MAP,'-game','-M5VS2NPCReactionReview','-M5NPCFallDirection=Forward','-M5VS2AutoQuit',
        '-M5VS2EvidenceDir="D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/NPCReactionR2_GAME_UNIQUE"',
        '-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_NPCReactionR2_UNIQUE','-ResX=1920','-ResY=1080','-windowed','-language=en']
    if IS_IDLE_PAIR:R['runtime_arguments'].append('-M5VS2NPCReactionIdlePair='+PAIR)
    R['status']='PASS';R['pass_scope']='Only native isolated map/material author, baked navigation and save/reload binding/path checks. Runtime and visuals NOT_RUN.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['source_hash_changes']=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['source_hash_changes']:R['status']='FAIL';dump()
if R['status']!='PASS':raise RuntimeError('NPC gameplay author failed; see author_result.json')
