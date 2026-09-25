"""Private fixed-camera native clothing review. Does not launch UE or change source assets.
HeroModestyReviewAuthor creates private GM/map; HeroModestyReviewReload reads fresh disk.
Eight real stand/Space-jump captures are evidence for review, never all-angle acceptance.
"""
from pathlib import Path
import ast,datetime as dt,hashlib,json,math,re,traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem);ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
SCHEMA='HarborCity.M5VS2.HeroModestyReview.Author.v1';OWNER='HarborCity_M5VS2_HeroModestyReview'
SOURCE_MAP='/Game/HarborCity/M5VS2/HeroExercise/L_SelestiaExercise';SOURCE_MODE='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'
def arg(name):
    a=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(a)==1,name
    return a[0][0] or a[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC/'editor_runtime') and PHASE in ('HeroModestyReviewAuthor','HeroModestyReviewReload') and not(OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True);TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12];ROOT='/Game/HarborCity/M5VS2/HeroModestyReview/Run_'+TOKEN
R=dict(schema=SCHEMA,owner=OWNER,phase=PHASE,status='RUNNING',checks=[],assets=[],inputs={},source_bindings=[],runtime='NOT_RUN',os_input='NOT_RUN',coverage='USER_REVIEW',
       fresh_process_disk_readback='NOT_RUN',kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED',input_scope='ENGINE_SPACE_KEY_NOT_OS',camera_scope='FOUR_FIXED_DIAGNOSTIC_CAMERAS_NOT_PLAYER_MOUSE',
       scope='Private inherited neutral exercise floor/light fixture; source Hero and skirt/physics unchanged, added opaque skinned safety layer. Four fixed low cameras and actual Space jump; flight/dive and all-angle coverage NOT_RUN.',started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={};BINDINGS={}
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:dump();raise RuntimeError(name+': '+str(actual))
def sha(file):
    h=hashlib.sha256()
    with Path(file).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def path(obj):return obj.get_path_name() if obj is not None else None
def package(obj):return(path(obj) if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def disk(obj):
    p=package(obj);assert re.fullmatch(r'/Game/[A-Za-z0-9_/]+',p)
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.umap' if p.rsplit('/',1)[-1].startswith('L_') else '.uasset')
def kind(file):
    f=Path(file).resolve()
    for root,label in ((PROJECT/'Content','package'),(PROJECT/'Binaries/Win64','module'),(PROJECT/'Source','cpp'),(PROJECT/'Config','config'),(WORK/'tools','script'),(DOC,'evidence')):
        if f.is_relative_to(root):return label
    raise RuntimeError('Source outside strict project roots: '+str(f))
def binding(file):
    f=Path(file).resolve();return dict(path=str(f),sha256=sha(f),bytes=f.stat().st_size,kind=kind(f))
def protect(file,expected=None):
    row=binding(file);check('native source matches evidence',expected is None or row['sha256']==expected,row['path']);PROTECTED[row['path']]=row['sha256'];BINDINGS[row['path']]=row;return row
def document(file):return json.loads(Path(file).read_text('utf-8-sig'))
def report(file):
    f=Path(file).resolve();check('owned evidence',f.is_relative_to(DOC/'editor_runtime'));protect(f);d=document(f);cmd=f.parent/'commandlet.json';protect(cmd);c=document(cmd)
    check('actual native author/process PASS',d.get('status')=='PASS' and all(x.get('status')=='PASS' for x in d.get('checks',[])) and c.get('status')=='PASS' and c.get('exit_code')==0,str(f));return d
def latest(phase,schema):
    for f in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda f:f.stat().st_mtime,reverse=True):
        d=document(f)
        if d.get('status')=='PASS' and d.get('schema')==schema:return f,report(f)
    raise RuntimeError('No completed actual '+phase)
def load(p):
    o=A.load_asset(package(p));check('native asset loaded',o is not None,p);return o
def cls(p):
    p=package(p);c=U.load_class(None,p+'.'+p.rsplit('/',1)[-1]+'_C');check('actual generated class',c is not None,p);return c
def install_readers():
    f=WORK/'tools/ue_m5_vs2_corner_playable_r4_author.py';protect(f);text=f.read_text('utf-8-sig');tree=ast.parse(text)
    names=('xyz','transform','rounded','component_snapshot','actor_record');nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact existing readonly fixture reflection',tuple(n.name for n in nodes)==names)
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes);R['native_readers']=dict(path=str(f),functions=list(names),sha256=hashlib.sha256(code.encode()).hexdigest())
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readonly','exec'),globals())
def world():return U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
def relevant_actors():return[a for a in ACT.get_all_level_actors() if a.get_class().get_name() not in ('WorldSettings','Brush','DefaultPhysicsVolume')]
def native_candidate(d):
    hero=cls(d['blueprint']);body=U.get_default_object(hero).get_editor_property('mesh');keep=d['source_baseline']
    check('actual existing body/ABP/five materials kept',path(body.get_editor_property('skeletal_mesh_asset'))==keep['mesh'] and path(body.get_editor_property('anim_class'))==keep['animation'] and [path(body.get_material(i)) for i in range(body.get_num_materials())]==keep['materials'])
    result=json.loads(U.HCM5VS2HeroModestyEditor.inspect_safety_shorts(load(d['source_body']),load(d['shorts'])))
    check('actual closed private garment readback',result.get('status')=='PASS' and result==d['native_readback']['topology'])
    return hero,body
def stage_record():
    rows=[]
    for a in relevant_actors():
        row=actor_record(a)
        if isinstance(a,U.PostProcessVolume):
            s=a.get_editor_property('settings');row['postprocess']={k:str(s.get_editor_property(k)) for k in ('override_auto_exposure_method','auto_exposure_method','override_auto_exposure_apply_physical_camera_exposure','auto_exposure_apply_physical_camera_exposure','override_auto_exposure_bias','auto_exposure_bias','override_motion_blur_amount','motion_blur_amount','override_bloom_intensity','bloom_intensity')}
        if isinstance(a,U.SkyLight):row['realtime_capture']=bool(a.get_component_by_class(U.SkyLightComponent).get_editor_property('real_time_capture'))
        rows.append(row)
    return sorted(rows,key=lambda r:r['label'])
def readback():
    actors=relevant_actors();directors=[a for a in actors if isinstance(a,U.HCM5VS2HeroModestyReviewDirector)];cameras=[a for a in actors if isinstance(a,U.CameraActor)]
    check('only one review director and four authored cameras',len(directors)==1 and len(cameras)==4)
    forbidden=[a.get_class().get_name() for a in actors if ('Director' in a.get_class().get_name() and a!=directors[0]) or isinstance(a,U.Pawn) or isinstance(a,U.HCM3Experience)]
    check('no other controller or spawned pawn fixtures',not forbidden,forbidden)
    d=directors[0];actual={k:path(d.get_editor_property(k)) for k in ('expected_character_class','expected_animation_class','expected_body','expected_shorts','expected_cloth')}
    actual['expected_body_materials']=[path(m) for m in d.get_editor_property('expected_body_materials')]
    actual['low_cameras']=[c.get_actor_label() for c in d.get_editor_property('low_cameras')]
    camera_rows=[]
    for c in d.get_editor_property('low_cameras'):
        cc=c.get_component_by_class(U.CameraComponent);camera_rows.append(dict(label=c.get_actor_label(),transform=transform(c.get_actor_transform()),field_of_view=float(cc.get_editor_property('field_of_view')),post_process_blend_weight=float(cc.get_editor_property('post_process_blend_weight')),constrain_aspect_ratio=bool(cc.get_editor_property('constrain_aspect_ratio'))))
    gm=world().get_world_settings().get_editor_property('default_game_mode');pawn=U.get_default_object(gm).get_editor_property('default_pawn_class')
    return rounded(dict(game_mode=path(gm),pawn_class=path(pawn),director=actual,cameras=camera_rows,actors=stage_record()))
def save(obj):
    p=package(obj);check('only new private asset saved',p.startswith(ROOT+'/'));A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER);check('native save',A.save_loaded_asset(obj,False));R['assets'].append(dict(path=p,sha256=sha(disk(p))))
try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty assets/maps or PIE',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    install_readers()
    if PHASE=='HeroModestyReviewAuthor':
        protect(__file__);protect(WORK/'tools/launch_m5_vs2_hero_modesty_review.ps1');protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll')
        for name in ('HCM5VS2HeroModestyComponent','HCM5VS2HeroModestyReviewDirector'):
            for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/(name+ext))
        f,source=latest('HeroModestyReload','HarborCity.M5VS2.HeroModesty.v1');R['inputs']['modesty_reload']=binding(f)
        check('fresh native garment Reload has actual provenance',source.get('fresh_process_disk_readback')=='PASS' and source.get('preservation',{}).get('changed_source_files')==[])
        for p,h in source['preservation']['source_sha256_before'].items():protect(p,h)
        for row in source['assets']:protect(disk(row['path']),row['sha256'])
        hero,body=native_candidate(source);R['candidate']=dict(blueprint=source['blueprint'],source_body=source['source_body'],shorts=source['shorts'],material=source['material'])
        protect(disk(SOURCE_MAP));protect(disk(SOURCE_MODE))
        for f in (PROJECT/'Config').glob('*.ini'):protect(f)
        check('new review namespace',not A.does_directory_exist(ROOT) and not disk(ROOT+'/L_HeroModestyReview').exists())
        template=load(SOURCE_MAP);check('actual existing owned neutral exercise template',A.get_metadata_tag(template,'HarborCityOwnedBy')=='HarborCity_M5_VS2_HeroExercise')
        check('load original template read only',LEVEL.load_level(SOURCE_MAP))
        old=[a for a in relevant_actors() if isinstance(a,U.HCM5VS2HeroExerciseDirector)];check('one original exercise actor',len(old)==1)
        inherited=[r for r in stage_record() if r['label']!=old[0].get_actor_label()]
        for a in relevant_actors():
            for c in a.get_components_by_class(U.MeshComponent):
                if isinstance(c,U.StaticMeshComponent):
                    m=c.get_editor_property('static_mesh')
                    if m and package(m).startswith('/Game/'):protect(disk(m))
                for i in range(c.get_num_materials()):
                    m=c.get_material(i)
                    if m and package(m).startswith('/Game/'):protect(disk(m))
        gm=A.duplicate_asset(SOURCE_MODE,ROOT+'/BP_HeroModestyReviewMode');check('new private game mode',gm is not None)
        U.get_default_object(gm.generated_class()).set_editor_property('default_pawn_class',hero);U.BlueprintEditorLibrary.compile_blueprint(gm);save(gm)
        target=ROOT+'/L_HeroModestyReview';check('native private map from source lifecycle',LEVEL.new_level_from_template(target,SOURCE_MAP))
        old=[a for a in relevant_actors() if isinstance(a,U.HCM5VS2HeroExerciseDirector)];check('one copied exercise director',len(old)==1);check('remove only copied director',ACT.destroy_actor(old[0]))
        check('source light/floor/backdrop/postprocess preserved',stage_record()==inherited)
        world().get_world_settings().set_editor_property('default_game_mode',gm.generated_class());A.set_metadata_tag(world(),'HarborCityOwnedBy',OWNER)
        starts=[a for a in relevant_actors() if isinstance(a,U.PlayerStart)];check('single preserved PlayerStart',len(starts)==1);start=starts[0];origin=start.get_actor_location()
        move=U.get_default_object(hero).get_editor_property('character_movement');jz=float(move.get_editor_property('jump_z_velocity'));gravity=980*float(move.get_editor_property('gravity_scale'));apex=jz*jz/(2*gravity)
        check('bounded unchanged jump supports low camera fixture',250<=jz<=500 and 500<=gravity<=2000 and 15<apex<160,dict(jump_z_velocity=jz,gravity_magnitude_assumption=gravity,ballistic_apex_estimate=apex))
        # The inherited floor top is z=0. Runtime's real grounded origin is authoritative.
        scale=body.get_relative_transform().scale3d;check('retained uniform1.25 body scale',max(abs(float(v)-1.25) for v in (scale.x,scale.y,scale.z))<.0001)
        aim_z=(56+81)*.5*float(scale.z)+apex*.5;radius=300.;camera_z=30.;fov=55.;cameras=[]
        forward=start.get_actor_forward_vector();right=start.get_actor_right_vector()
        for label,direction in (('Front',forward),('Rear',U.Vector(-forward.x,-forward.y,0)),('Left',U.Vector(-right.x,-right.y,0)),('Right',right)):
            p=U.Vector(origin.x+direction.x*radius,origin.y+direction.y*radius,camera_z);aim=U.Vector(origin.x,origin.y,aim_z);rot=U.MathLibrary.find_look_at_rotation(p,aim)
            c=ACT.spawn_actor_from_class(U.CameraActor,p,rot,False);check('native fixed camera',c is not None,label);c.set_actor_label('HC_Modesty_Low_'+label);c.set_editor_property('tags',[U.Name(OWNER)])
            cc=c.get_component_by_class(U.CameraComponent);cc.set_editor_property('field_of_view',fov);cc.set_editor_property('constrain_aspect_ratio',False);cc.set_editor_property('post_process_blend_weight',0.);cameras.append(c)
        director=ACT.spawn_actor_from_class(U.HCM5VS2HeroModestyReviewDirector,U.Vector(0,0,-150),U.Rotator(),False);check('new native review director',director is not None);director.set_actor_label('HC_Modesty_Review');director.set_editor_property('tags',[U.Name(OWNER)])
        for k,v in dict(expected_character_class=hero,expected_animation_class=body.get_editor_property('anim_class'),expected_body=body.get_editor_property('skeletal_mesh_asset'),expected_shorts=load(source['shorts']),expected_cloth=load(source['material']),expected_body_materials=[body.get_material(i) for i in range(body.get_num_materials())],low_cameras=cameras).items():director.set_editor_property(k,v)
        keep=[r for r in stage_record() if r['label'] not in {a.get_actor_label() for a in [director,*cameras]}];check('all source fixture actors unchanged',keep==inherited)
        before=readback();check('correct private game mode pawn',before['pawn_class']==path(hero))
        check('save only new review map',LEVEL.save_current_level());R['assets'].append(dict(path=target,sha256=sha(disk(target))))
        check('same process reload private map',LEVEL.load_level(target));actual=readback();check('native saved readback stable',before==actual)
        R.update(status='PASS',destination=ROOT,map=target,native_readback=actual,source_bindings=list(BINDINGS.values()),source_map=SOURCE_MAP,expected_screenshots=8,
                 camera_design=dict(radius_cm=radius,height_above_floor_cm=camera_z,aim_z_cm=aim_z,fov_degrees=fov,view_order=['Front','Rear','Left','Right'],standing_and_jump=True,jump_estimate_only=apex),
                 lighting_scope='Inherited neutral exercise proxy, not actual port-town environment; no new fill/exposure/material substitutions.',pass_scope='Native private map/GM created and saved. Fresh Reload and real clothing capture NOT_RUN.')
    else:
        f,prior=latest('HeroModestyReviewAuthor',SCHEMA);ROOT=prior['destination'];check('exact saved destination',re.fullmatch(r'/Game/HarborCity/M5VS2/HeroModestyReview/Run_[0-9a-f]{12}',ROOT) is not None and prior['map']==ROOT+'/L_HeroModestyReview' and len(prior['assets'])==2)
        for row in prior['source_bindings']:protect(row['path'],row['sha256'])
        for row in prior['assets']:protect(disk(row['path']),row['sha256'])
        check('fresh native private map load',LEVEL.load_level(prior['map']));actual=readback();check('fresh complete map/GM/cameras/binding readback',actual==prior['native_readback'])
        for k in ('candidate','destination','map','assets','source_map','expected_screenshots','camera_design','lighting_scope'):R[k]=prior[k]
        R.update(status='PASS',fresh_process_disk_readback='PASS',native_readback=actual,source_author=binding(f),source_bindings=list(BINDINGS.values()),pass_scope='Fresh native read-only persistence. Eight PNGs, clothing coverage and flight/dive remain NOT_RUN.')
        manifest={k:R[k] for k in ('schema','owner','kind','input_scope','camera_scope','destination','map','candidate','source_bindings','expected_screenshots','scope')}
        manifest.update(status='READY_FOR_COVERAGE_CAPTURE_NOT_RUNTIME_TESTED',reload_report=str(OUT/'author_result.json'),save_slot_prefix='HarborCity_VS2_Modesty_',runtime_flag='-M5VS2HeroModestyReview')
        mf=OUT/'hero_modesty_review_manifest.json';mf.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8');R['launch_manifest']=binding(mf)
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h];R['preservation']=dict(changed_files=changed,source_sha256_before=PROTECTED)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Private modesty review author failed. Keep partial assets and original failure; no old asset rollback.')
