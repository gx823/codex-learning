"""Root-only HeroGaitR2Author: new fixture, selected integrated Hero, no asset tuning.

Copies the actual old exercise stage to preserve its floor/light geometry. The
new runtime flag observes GASMotion/FlightPose final toes; old map stays intact.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT,DOC=WORK/'HarborCity',WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
SELECT=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json'
SOURCE_MAP='/Game/HarborCity/M5VS2/HeroExercise/L_SelestiaExercise'
SOURCE_OWNER='HarborCity_M5_VS2_HeroExercise'
SOURCE_MODE='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'
ORIGINAL_SPACE='/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun'
OWNER='HarborCity_M5VS2_HeroGaitR2'


def argument(name):
    rows=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(rows)==1,'Exactly one '+name+' required'
    return rows[0][0] or rows[0][1]


OUT=Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
assert argument('M5AuthorPhase')=='HeroGaitR2Author'
OUT.mkdir(parents=True,exist_ok=True)
TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
ROOT='/Game/HarborCity/M5VS2/HeroGaitR2/Run_'+TOKEN
MAP=ROOT+'/L_HeroGaitR2'
R=dict(schema='HarborCity.M5VS2.HeroGaitR2.Author.v1',phase='HeroGaitR2Author',status='RUNNING',
       map=MAP,source_map=SOURCE_MAP,owner=OWNER,checks=[],assets=[],inputs={},runtime='NOT_RUN',
       visual_acceptance='USER_REVIEW',started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       scope='Same old exercise stage geometry; current integrated Hero and actual final gait. Not OS input or packaged gameplay.')
PROTECTED={}


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')


def check(name,okay,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if okay else 'FAIL',observed=observed));dump()
    if not okay:raise RuntimeError(name+': '+repr(observed))


def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()


def document(file):return json.loads(Path(file).read_text(encoding='utf-8-sig'))
def package(obj):return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def disk(obj,ext='.uasset'):
    p=package(obj);assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix(ext)


def protect(file,digest=None):
    file=Path(file).resolve();actual=sha(file)
    check('protected input '+str(file),digest is None or actual==digest,actual)
    PROTECTED[str(file)]=actual


def report(ref,phase):
    p=Path(ref['path']).resolve();check('source report in owned evidence',p.is_relative_to(DOC/'editor_runtime'))
    protect(p,ref['sha256']);data=document(p);cmd=p.parent/'commandlet.json';protect(cmd);launch=document(cmd)
    check('completed native '+phase,data.get('status')=='PASS' and data.get('phase')==phase
          and launch.get('phase')==phase and launch.get('status')=='PASS' and launch.get('exit_code')==0)
    for row in data['assets']:protect(disk(row['path']),row['sha256'])
    R['inputs'][phase]=dict(path=str(p),sha256=sha(p),commandlet=str(cmd),commandlet_sha256=sha(cmd))
    return data


def load(path,kind=None):
    obj=A.load_asset(package(path));check('native load '+package(path),obj is not None and (kind is None or isinstance(obj,kind)))
    return obj


def generated(path):
    p=package(path);obj=U.load_class(None,p+'.'+p.rsplit('/',1)[1]+'_C')
    check('compiled class '+p,obj is not None);return obj


def save(obj):
    p=package(obj);check('new fixture asset only',p.startswith(ROOT+'/'))
    A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER)
    check('native save '+p,A.save_loaded_asset(obj,False))
    R['assets'].append(dict(path=p,sha256=sha(disk(p))))


try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty map',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('fresh fixture namespace',not A.does_directory_exist(ROOT) and not disk(MAP,'.umap').parent.exists())
    protect(SELECT);selection=document(SELECT)
    check('explicit review selection',selection['status']=='SELECTED_FOR_REVIEW'
          and re.fullmatch(r'/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/BP_HeroRev2_Integrated_[0-9a-f]{12}',selection['blueprint']) is not None)
    dependencies={}
    for phase in ('HeroRev2IntegrationReload','GASMotionReload','FlightPoseReload'):
        refs=[ref for ref in selection['source_reports'] if Path(ref['path']).parent.name.endswith('_'+phase)]
        check('unique selected dependency '+phase,len(refs)==1)
        dependencies[phase]=report(refs[0],phase)
    hero=dependencies['HeroRev2IntegrationReload'];motion=dependencies['GASMotionReload'];flight=dependencies['FlightPoseReload']
    check('selected Hero is actual reloaded integration',hero['blueprint']==selection['blueprint']
          and hero.get('fresh_process_disk_readback')=='PASS')
    check('same retained GAS mesh and new flight ABP',hero['expected']['mesh']==motion['candidate']['mesh']==flight['candidate']['mesh']
          and hero['expected']['animation']==flight['candidate']['blueprint']
          and motion['candidate']['blendspace']==flight['candidate']['blendspace'])
    for p in (SOURCE_MAP,):protect(disk(p,'.umap'))
    for p in (SOURCE_MODE,ORIGINAL_SPACE,hero['blueprint'],hero['expected']['mesh'],hero['expected']['animation'],*hero['expected']['materials']):protect(disk(p))
    for file in (PROJECT/'Config').glob('*.ini'):protect(file)
    for name in ('HCM5VS2HeroExerciseDirector.h','HCM5VS2HeroExerciseDirector.cpp','HCM5VS2LookAnimInstance.h','HCM5VS2LookAnimInstance.cpp'):
        protect(PROJECT/'Source/HarborCity/M5VS2'/name)
    module=PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll';protect(module)
    source=load(SOURCE_MAP)
    check('actual owned old exercise source',A.get_metadata_tag(source,'HarborCityOwnedBy')==SOURCE_OWNER)
    hero_class=generated(hero['blueprint']);anim_class=generated(hero['expected']['animation'])
    mesh=load(hero['expected']['mesh'],U.SkeletalMesh);space=load(motion['candidate']['blendspace'],U.BlendSpace)
    cdo=U.get_default_object(hero_class);body=cdo.get_editor_property('mesh')
    materials=[load(p) for p in hero['expected']['materials']]
    check('exact current integrated mesh animation and materials',body.get_editor_property('skeletal_mesh_asset')==mesh
          and body.get_editor_property('anim_class')==anim_class
          and [body.get_material(i) for i in range(body.get_num_materials())]==materials)
    check('unchanged movement speeds',cdo.get_editor_property('walk_speed')==400 and cdo.get_editor_property('sprint_speed')==650)
    flight_native=json.loads(U.HCM5VS2FlightPoseEditor.inspect_flight_graph(load(hero['expected']['animation'],U.AnimBlueprint)))
    R['native_retained_flight_graph']=flight_native
    check('native current graph confirms both GAS and flight flags',flight_native.get('status')=='PASS')
    native=json.loads(U.HCM5VS2GASMotionEditor.inspect_motion_candidate(
        load(hero['expected']['animation'],U.AnimBlueprint),mesh,load(ORIGINAL_SPACE,U.BlendSpace),space))
    R['native_retained_gas_graph']=native
    check('actual retained graph and 28 samples readback',native.get('status')=='PASS' and len(native.get('samples',[]))==28)
    samples={row['index']:row for row in native['samples']}
    parent=package(space).rsplit('/',1)[0]
    for index,name,speed in ((0,'Run_WarpLoop',400),(9,'Sprint_WarpLoop',650),(27,'Walk_WarpLoop',None)):
        row=samples[index]
        check('exact real target clip '+name,row['animation']==parent+'/Loops/'+name and row['x']==0
              and (speed is None or row['y']==speed),row)
    gm=A.duplicate_asset(SOURCE_MODE,ROOT+'/BP_HeroGaitR2GameMode')
    check('private game mode',gm is not None)
    U.get_default_object(gm.generated_class()).set_editor_property('default_pawn_class',hero_class)
    check('private game mode compile',U.BlueprintEditorLibrary.compile_blueprint(gm));save(gm)
    check('new map from original exercise fixture',LEVEL.new_level_from_template(MAP,SOURCE_MAP))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact private fixture world',package(world)==MAP)
    A.set_metadata_tag(world,'HarborCityOwnedBy',OWNER)
    world.get_world_settings().set_editor_property('default_game_mode',gm.generated_class())
    directors=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2HeroExerciseDirector)]
    check('one copied original exercise director',len(directors)==1)
    prior=directors[0];bones=list(prior.get_editor_property('observed_bones'));phases=list(prior.get_editor_property('phases'))
    sun=prior.get_editor_property('directional_light')
    check('same bounded stage probes and light',1<=len(bones)<=16 and 1<=len(phases)<=24 and sun is not None)
    check('remove only copied old director',ACT.destroy_actor(prior))
    for actor in ACT.get_all_level_actors():
        if actor.get_class().get_name() in ('WorldSettings','Brush','DefaultPhysicsVolume'):continue
        check('copied fixture actors are old owned stage',actor.actor_has_tag(U.Name(SOURCE_OWNER)),actor.get_actor_label())
        actor.set_editor_property('tags',[U.Name(OWNER)])
    director=ACT.spawn_actor_from_class(U.HCM5VS2HeroExerciseDirector,U.Vector(0,0,-100),U.Rotator(),False)
    check('new private gait director',director is not None)
    director.set_actor_label('HC_HeroGaitR2_Director');director.set_editor_property('tags',[U.Name(OWNER)])
    values=dict(expected_character_class=hero_class,expected_animation_class=anim_class,expected_mesh=mesh,
                expected_gait_blend_space=space,gait_source_report=R['inputs']['HeroRev2IntegrationReload']['path'],
                directional_light=sun,exercise_materials=materials,observed_bones=bones,phases=phases)
    for key,value in values.items():director.set_editor_property(key,value)
    check('native private map save',LEVEL.save_current_level())
    R['assets'].append(dict(path=MAP,sha256=sha(disk(MAP,'.umap'))))
    check('native private map reload',LEVEL.load_level(MAP))
    reloaded=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2HeroExerciseDirector)]
    check('unique persisted R2 director',len(reloaded)==1)
    for key in ('expected_character_class','expected_animation_class','expected_mesh','expected_gait_blend_space','gait_source_report'):
        check('persisted '+key,reloaded[0].get_editor_property(key)==values[key])
    check('only two new packages',len(R['assets'])==2)
    R['source']=dict(selection=str(SELECT),selection_sha256=sha(SELECT),report=R['inputs']['HeroRev2IntegrationReload']['path'],
                     report_sha256=R['inputs']['HeroRev2IntegrationReload']['sha256'],hero=hero['blueprint'],expected=hero['expected'])
    R['binding']=dict(character=hero['blueprint'],animation=hero['expected']['animation'],mesh=hero['expected']['mesh'],
                      blendspace=package(space),loop_targets={name:samples[index]['animation'] for index,name in ((0,'run'),(9,'sprint'),(27,'walk'))})
    R['runtime_source_sha256']={str(p):sha(p) for p in [PROJECT/'Source/HarborCity/M5VS2/HCM5VS2HeroExerciseDirector.cpp',PROJECT/'Source/HarborCity/M5VS2/HCM5VS2HeroExerciseDirector.h']}
    R['module_sha256']=sha(module);R['expected_screenshots']=9
    R['runtime_flag']='-M5VS2HeroGaitR2'
    R['measurement_scope']='Same contact>=.8, speed400/650 +/-2, effective target weight>=.95, interval<=75ms and yaw delta<1. R2 uses actual new cached clips and physical body-relative direction; extra0.38 analog input observes low-speed loop.'
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['source_preservation']=dict(files=len(PROTECTED),changed=changed,sha256_before=PROTECTED)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('HeroGaitR2Author failed; retained evidence and previous map unchanged')
