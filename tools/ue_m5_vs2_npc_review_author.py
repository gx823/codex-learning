"""Author a fresh native neutral NPC review map from exact latest PASS Q/R manifests.

Root runs phase NPCReview after native integration and Director compilation.
No editor/game launch, old asset overwrite, default-map edit or visual PASS claim.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习'); PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
OWNER='HarborCity_M5_VS2_NPCReview'; KEY='HarborCityOwnedBy'; INTEGRATION_OWNER='HarborCity_M5_VS2_NPC_Integration'
MODE='/Game/HarborCity/M5VS1/HeroSelestia/BP_M5_SelestiaGameMode'
A=U.EditorAssetLibrary; E=U.MaterialEditingLibrary; AT=U.AssetToolsHelpers.get_asset_tools()
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem); ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
cmd=U.SystemLibrary.get_command_line()
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',cmd)
assert len(args)==1
OUT=Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to(DOC.resolve()) and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ROOT='/Game/HarborCity/M5VS2/NPC/Review/Run_'+hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:10]
MAP=ROOT+'/L_NPCSpecimens'
R=dict(status='RUNNING',map=MAP,checks=[],assets=[],actors=[],inputs={},runtime='NOT_RUN',visual='NOT_RUN_NULLRHI',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       limitations=['Neutral asset review stage, not the finished harbor street.',
          'Movement and physics are explicitly native component function exercises; OS input, navigation, vehicle contact, gameplay recovery/death/respawn remain NOT_RUN.',
          'Twenty native viewport PNGs still require human face/full-body art review.'])


def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,default=str),encoding='utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed)); dump()
    if not ok: raise RuntimeError(name+': '+str(observed))
def disk(path,ext='.uasset'): return PROJECT/'Content'/Path(path.split('.')[0].removeprefix('/Game/')).with_suffix(ext)
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def load(path):
    obj=A.load_asset(path); check('load '+path,obj is not None); return obj
def cls(path):
    obj=U.load_class(None,path.split('.')[0]+'.'+path.split('.')[0].rsplit('/',1)[1]+'_C')
    check('load generated class '+path,obj is not None); return obj


def manifest(letter):
    candidates=sorted((DOC/'editor_runtime').glob('author_*_NPCIntegration'+letter+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
    for path in candidates:
        data=json.loads(path.read_text(encoding='utf-8-sig'))
        if data.get('status')!='PASS' or data.get('sample')!=letter: continue
        spec=data.get('specimen',{})
        required=('blueprint','profile','anim_blueprint','physics','mesh','capsule_half_height_cm')
        check('complete successful '+letter+' specimen manifest',all(k in spec for k in required),str(path))
        for asset in data['assets']:
            check('exact saved specimen '+asset['path'],disk(asset['path']).is_file() and sha(disk(asset['path']))==asset['sha256'])
        check('independent specimen namespace',spec['blueprint'].startswith('/Game/HarborCity/M5VS2/NPC/AvatarSample_'+letter+'/Runtime_'))
        R['inputs'][letter]=dict(path=str(path),sha256=sha(path),specimen=spec)
        return spec
    raise RuntimeError('No actual PASS NPCIntegration'+letter+' manifest')


def material(name,rgb):
    obj=AT.create_asset(name,ROOT+'/Materials',U.Material,U.MaterialFactoryNew()); check('create stage material',obj is not None)
    color=E.create_material_expression(obj,U.MaterialExpressionConstant3Vector,0,0)
    color.set_editor_property('constant',U.LinearColor(*rgb,1))
    rough=E.create_material_expression(obj,U.MaterialExpressionConstant,0,100); rough.set_editor_property('r',.85)
    check('native stage base color connection',E.connect_material_property(color,'',U.MaterialProperty.MP_BASE_COLOR))
    check('native stage roughness connection',E.connect_material_property(rough,'',U.MaterialProperty.MP_ROUGHNESS))
    E.recompile_material(obj); A.set_metadata_tag(obj,KEY,OWNER); check('save stage material',A.save_loaded_asset(obj,False))
    R['assets'].append(dict(path=obj.get_path_name(),sha256=sha(disk(obj.get_path_name())))); return obj


def spawn(name,kind,pos,rot=None):
    obj=ACT.spawn_actor_from_class(kind,U.Vector(*pos),rot or U.Rotator(),False); check('spawn '+name,obj is not None)
    obj.set_actor_label('HC_M5VS2_NPCReview_'+name); obj.set_editor_property('tags',[U.Name(OWNER)])
    R['actors'].append(dict(label=obj.get_actor_label(),class_path=obj.get_class().get_path_name(),position_cm=pos)); return obj


def box(name,pos,size,mat,collision):
    obj=spawn(name,U.StaticMeshActor,pos); body=obj.get_component_by_class(U.StaticMeshComponent)
    body.set_mobility(U.ComponentMobility.STATIC); body.set_static_mesh(load('/Engine/BasicShapes/Cube'))
    body.set_material(0,mat); body.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    body.set_cast_shadow(collision); obj.set_actor_scale3d(U.Vector(*(v/100 for v in size))); return obj


try:
    phase=re.findall(r'(?:^|\s)-M5AuthorPhase=(\S+)',cmd)
    check('NPCReview author phase',phase==['NPCReview'],phase)
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT.resolve())
    check('no PIE',not U.EditorLevelLibrary.get_pie_worlds(False))
    check('no pre-existing dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('native review Director reflected',hasattr(U,'HCM5VS2NPCReviewDirector'))
    q,r=manifest('Q'),manifest('R')
    protected={str(PROJECT/'Config/DefaultEngine.ini'):sha(PROJECT/'Config/DefaultEngine.ini'),str(disk(MODE)):sha(disk(MODE))}
    for spec in (q,r):
        for name in ('blueprint','profile','anim_blueprint','physics','mesh'): protected[str(disk(spec[name]))]=sha(disk(spec[name]))
        check('owned integrated NPC Blueprint',A.get_metadata_tag(load(spec['blueprint']),KEY)==INTEGRATION_OWNER)
    check('fresh complete review namespace',not A.does_directory_exist(ROOT) and not disk(MAP,'.umap').parent.exists())
    mode=cls(MODE); check('create fresh isolated review world',LEVEL.new_level(MAP,False))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    check('exact new review world',world.get_path_name().split('.')[0]==MAP)
    A.set_metadata_tag(world,KEY,OWNER); world.get_world_settings().set_editor_property('default_game_mode',mode)
    floor=material('M_NeutralFloor',(.18,.18,.18)); back=material('M_NeutralBackdrop',(.24,.27,.31))
    box('Floor',(0,0,-10),(3600,3000,20),floor,True)
    box('Backdrop',(-900,0,300),(10,2800,600),back,False)
    box('BackdropLeft',(0,-1200,300),(3000,10,600),back,False)
    box('BackdropRight',(0,1200,300),(3000,10,600),back,False)
    spawn('PlayerStart',U.PlayerStart,(-650,0,95),U.Rotator(yaw=0))
    sun=spawn('MainLight',U.DirectionalLight,(300,0,500),U.Rotator(pitch=-28,yaw=155))
    light=sun.get_component_by_class(U.DirectionalLightComponent); light.set_mobility(U.ComponentMobility.MOVABLE)
    light.set_light_color(U.LinearColor(1,1,1,1)); light.set_intensity(3.); light.set_editor_property('atmosphere_sun_light',True)
    sky=spawn('SkyLight',U.SkyLight,(0,0,500)).get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE); sky.set_editor_property('real_time_capture',True); sky.set_intensity(.4)
    spawn('Atmosphere',U.SkyAtmosphere,(0,0,0))
    post=spawn('FixedExposure',U.PostProcessVolume,(0,0,0)); post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    values=dict(override_auto_exposure_method=True,auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True,auto_exposure_bias=0.,override_motion_blur_amount=True,motion_blur_amount=0.,
        override_bloom_intensity=True,bloom_intensity=.12)
    for name,value in values.items(): settings.set_editor_property(name,value)
    post.set_editor_property('settings',settings)
    # A semantic allowed region is supplied, but this script does not claim a baked NavMesh.
    region=spawn('AllowedRegion',U.HCM3NavRegion,(0,0,100))
    region.set_editor_property('stable_id',U.Name('M5VS2_NPCReview_Allowed'))
    region.set_editor_property('half_extent',U.Vector(1400,1000,400))
    people=[]
    for letter,spec,y in (('Q',q,-115.),('R',r,115.)):
        npc=spawn('NPC_'+letter,cls(spec['blueprint']),(0,y,float(spec['capsule_half_height_cm'])+2),U.Rotator(yaw=0))
        npc.set_editor_property('stationary',True); npc.set_editor_property('navigation_region',region)
        check('spawned NPC profile remains exact',npc.get_editor_property('npc_profile').get_path_name()==spec['profile'])
        people.append(npc)
    director=spawn('Director',U.HCM5VS2NPCReviewDirector,(0,0,-100))
    director.set_editor_property('specimens',people); director.set_editor_property('main_light',sun)
    check('native new map save',LEVEL.save_current_level())
    check('native new map on disk',disk(MAP,'.umap').is_file())
    R['assets'].append(dict(path=MAP,sha256=sha(disk(MAP,'.umap'))))
    check('reload actual saved review map',LEVEL.load_level(MAP))
    directors=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2NPCReviewDirector)]
    check('one saved native review Director',len(directors)==1)
    check('two saved actual NPC references',len(directors[0].get_editor_property('specimens'))==2)
    check('saved main light reference',directors[0].get_editor_property('main_light') is not None)
    check('source NPC assets/default map bytes preserved',all(sha(p)==h for p,h in protected.items()))
    R['runtime_arguments']=[MAP,'-game','-M5VS2NPCReview','-M5VS2EvidenceDir="D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime/NPCReview_GAME_UNIQUE"',
        '-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_NPCReview_UNIQUE','-ResX=1920','-ResY=1080','-windowed','-language=en','-M5VS2AutoQuit']
    R['expected_runtime_evidence']='Specified runtime evidence directory / NPCReview_TIMESTAMP_GUID / npc_review.json and 20 raw PNGs'
    R['autoquit']='Explicit AutoQuit exits after completion or bounded failure; viewport Esc permanently cancels automatic exit and all subsequent Director actions.'
    R['navigation']='NOT_RUN_NO_NAVMESH_BAKE; locomotion component exercise explicitly suspends NPC decision tick.'
    R['status']='PASS'; R['pass_scope']='Native isolated map/material author, save/reload and reference readback only. Runtime captures, faces, motion and physical behavior NOT_RUN.'
except Exception:
    R['status']='FAIL'; R['error']=traceback.format_exc(); U.log_error(R['error'])
finally:
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status']!='PASS': raise RuntimeError('NPC review author failed; see author_result.json')
