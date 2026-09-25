"""Four-image native material fixture. Root runs commandlet; never launches UE.

Copies the real R5 Afternoon world/GM. The already reloaded safety-layer Wing
candidate is referenced, never saved. New diagnostic module hashes are explicit;
historical successful DLL hashes remain historical, all other sources exact.
"""
from pathlib import Path
import ast, datetime as dt, hashlib, json, re, traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary;E=U.MaterialEditingLibrary
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem);ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
CMD=U.SystemLibrary.get_command_line()
def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',CMD)
    assert len(rows)==1,name
    return rows[0][0] or rows[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert PHASE=='WingMaterialReviewAuthor' and OUT.is_relative_to(DOC/'editor_runtime')
assert not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ROOT='/Game/HarborCity/M5VS2/FlightVisual/Review_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
MAP=ROOT+'/L_WingMaterialReview';SCHEMA='HarborCity.M5VS2.WingMaterialReview.Author.v1'
R=dict(schema=SCHEMA,phase=PHASE,status='RUNNING',map=MAP,checks=[],assets=[],inputs={},
       old_asset_writes=0,runtime='NOT_RUN',art='USER_REVIEW',os_input_used=False,
       scope='FUNCTION_CALL_VISUAL_FIXTURE; four original 1920x1080 material A/B PNGs, not input or full-flight acceptance.',
       historical_modules=[],started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={};SOURCES={}
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:raise RuntimeError(name+': '+str(actual))

def import_functions(filename,names):
    text=Path(filename).read_text('utf-8-sig')
    nodes=[n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact read-only helper membership '+Path(filename).name,tuple(n.name for n in nodes)==names)
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(filename)+'::fixture_readers','exec'),globals())

PERF=WORK/'tools/ue_m5_vs2_corner_performance_r5_author.py'
import_functions(PERF,('sha','binding','protect','document','completed','package','path','disk','readers','map_snapshot'))
LIVE=WORK/'tools/ue_m5_vs2_corner_playable_r5_author.py'
import_functions(LIVE,('hero_readers',))

def latest_complete(pattern,phase):
    files=sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
    for file in files:
        d=document(file)
        if d.get('status')=='PASS' and d.get('phase')==phase:return file,completed(file,phase)
    raise RuntimeError('No actual completed prerequisite '+phase)

MODULES={str((PROJECT/'Binaries/Win64'/('UnrealEditor-'+name+'.dll')).resolve()) for name in ('HarborCity','HarborCityEditor')}
def history_bindings(rows,label):
    for row in rows:
        f=Path(row['path']).resolve()
        if row['kind']=='module':
            check('only exact historical project DLL exception',str(f) in MODULES,str(f))
            R['historical_modules'].append(dict(source=label,**row,scope='HISTORICAL_AUTHOR_DLL; current diagnostic modules bound separately'))
        else:
            protect(f,row['sha256'],row['kind'])
            check('source byte size unchanged',f.stat().st_size==row['bytes'],str(f))

def director_snapshot(actor):
    return dict(reference_hero_class=path(actor.get_editor_property('reference_hero_class')),
        candidate_hero_class=path(actor.get_editor_property('candidate_hero_class')),
        reference_material=path(actor.get_editor_property('reference_material')),
        candidate_material=path(actor.get_editor_property('candidate_material')),
        label=actor.get_actor_label(),class_name=actor.get_class().get_path_name())

try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    protect(__file__,kind='script');protect(PERF,kind='script');protect(LIVE,kind='script')
    project_file=PROJECT/'HarborCity.uproject';PROTECTED[str(project_file.resolve())]=sha(project_file)
    R['project_definition']=binding(project_file,'project')
    check('actual runtime plus editor module definition',[m['Name'] for m in json.loads(project_file.read_text('utf-8-sig'))['Modules']]==['HarborCity','HarborCityEditor'])
    for f in sorted(MODULES):protect(f,kind='module')
    for stem in ('HCM5VS2WingMaterialReviewDirector','HCM5VS2FlightVisualComponent'):
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/(stem+ext),kind='cpp')
    check('new native fixture class compiled',hasattr(U,'HCM5VS2WingMaterialReviewDirector'))

    wing_file,wing=latest_complete('author_*_FlightWingMaterialReload','FlightWingMaterialReload')
    check('real Wing material fresh Reload exact schema',wing.get('schema')=='HarborCity.M5VS2.FlightWingMaterialR2.v1'
          and wing.get('fresh_process_disk_readback')=='PASS' and len(wing['assets'])==2
          and wing.get('preservation',{}).get('changed_source_files')==[])
    if wing['project_definition']['sha256'] != sha(project_file):
        # The asset is unchanged; the verified graph-module load correction does
        # not warrant duplicating/re-authoring the same material and Hero again.
        original_project=DOC/'source_snapshots/batch_20260925_113042/HarborCity.uproject'
        protect(original_project,wing['project_definition']['sha256'],'evidence')
        old_definition=document(original_project)
        expected=json.loads(json.dumps(old_definition))
        graph=[m for m in expected['Modules'] if m['Name']=='HarborCityEditor']
        check('one historical editor graph module',len(graph)==1 and graph[0]['Type']=='Editor' and graph[0]['LoadingPhase']=='Default')
        graph[0]['Type']='UncookedOnly'
        check('only reviewed graph-module host correction',expected==json.loads(project_file.read_text('utf-8-sig')))
        R['historical_project_definition']=wing['project_definition']
    prior_ref=wing['inputs']['apply'];protect(prior_ref['path'],prior_ref['sha256'],'evidence')
    prior=completed(prior_ref['path'],'FlightWingMaterialApply')
    check('actual Apply and Reload agree',all(wing[k]==prior[k] for k in ('blueprint','source_blueprint','material','assets','native_readback','source_shader')))
    history_bindings(list(wing['source_bindings'].values()),'FlightWingMaterialReload')
    for row in wing['assets']:protect(disk(row['path']),row['sha256'],package=row['path'])
    reference=A.load_asset(wing['source_blueprint']);candidate=A.load_asset(wing['blueprint']);mat=A.load_asset(wing['material'])
    check('exact two existing native Hero classes and material',reference is not None and candidate is not None and mat is not None
          and re.fullmatch(r'/Game/HarborCity/M5VS2/FlightVisual/MaterialReview_[0-9a-f]{12}/BP_HeroWingSoftLayers',wing['blueprint']) is not None)
    ns=hero_readers();keep=wing['native_readback']['source_baseline']
    check('both Hero CDO baselines match native material evidence',ns['baseline'](reference)==keep and ns['baseline'](candidate)==keep)
    comps=ns['scs_components'](candidate);flight=[c for c in comps if isinstance(c,U.HCM5VS2FlightVisualComponent)]
    safety=[c for c in comps if isinstance(c,U.HCM5VS2HeroModestyComponent)]
    check('one real candidate wing and safety component',len(flight)==len(safety)==1)
    original=flight[0].get_editor_property('light_material')
    check('exact runtime wing-only override and safety garment',flight[0].get_editor_property('wing_material')==mat
          and package(original)==wing['native_readback']['same_original_light_material']
          and ns['props'](safety[0],('safety_shorts','cloth_material'))==wing['native_readback']['safety_component'])
    original_components=[c for c in ns['scs_components'](reference) if isinstance(c,U.HCM5VS2FlightVisualComponent)]
    check('reference source has no wing override and same original light',len(original_components)==1
          and original_components[0].get_editor_property('wing_material') is None
          and original_components[0].get_editor_property('light_material')==original)
    wing_script=document(wing_file.parent/'commandlet.json')['script'];import_functions(wing_script,('material_snapshot',))
    check('actual saved material graph readback unchanged',material_snapshot(mat)==wing['native_readback']['material']
          and material_snapshot(original)==wing['source_shader'])
    R['inputs']['wing_reload']=binding(wing_file,'evidence')

    live_file,live=latest_complete('author_*_CornerPlayableR5Reload','CornerPlayableR5Reload')
    check('actual R5 world fresh Reload',live.get('schema')=='HarborCity.M5VS2.CornerPlayableR5.v1' and live.get('art_revision')==5
          and live.get('fresh_process_disk_readback')=='PASS' and live.get('preservation',{}).get('changed_files')==[]
          and live['selected_hero']==wing['source_blueprint'])
    mf=live['launch_manifest'];protect(mf['path'],mf['sha256'],'evidence');manifest=document(mf['path'])
    check('same native live map manifest',manifest['art_revision']==5 and manifest['expected_npcs']==8
          and manifest['selected_hero']==live['selected_hero'] and Path(manifest['reload_report']).resolve()==live_file.resolve())
    history_bindings(manifest['source_bindings'],'CornerPlayableR5Reload')
    for row in live['assets']:protect(disk(row['path']),row['sha256'],package=row['path'])
    readers(document(live_file.parent/'commandlet.json')['script'])
    expected=live['maps']['Afternoon']['native_readback'];source_map=live['maps']['Afternoon']['package']
    check('exact existing Afternoon map',manifest['maps']['Afternoon']==source_map and expected['period']=='Afternoon')
    check('fresh fixture namespace',not A.does_directory_exist(ROOT) and not disk(MAP).exists())
    check('copy complete real R5 map',A.duplicate_asset(source_map,MAP) is not None and LEVEL.load_level(MAP))
    check('all live actors preserved before fixture changes',map_snapshot(expected)==expected)
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    old_mode=world.get_world_settings().get_editor_property('default_game_mode')
    mode_path=ROOT+'/BP_WingMaterialReviewGameMode';mode=A.duplicate_asset(package(old_mode),mode_path)
    check('new private GM only',mode is not None)
    U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',candidate.generated_class());B.compile_blueprint(mode)
    check('private candidate Hero binding persisted',U.get_default_object(mode.generated_class()).get_editor_property('default_pawn_class')==candidate.generated_class()
          and A.save_loaded_asset(mode,False))
    world.get_world_settings().set_editor_property('default_game_mode',mode.generated_class())
    actor=ACT.spawn_actor_from_class(U.HCM5VS2WingMaterialReviewDirector,U.Vector(),U.Rotator(),False)
    check('one actual native fixture director',actor is not None)
    actor.set_actor_label('HC_VS2_WingMaterialReview')
    for prop,value in (('reference_hero_class',reference.generated_class()),('candidate_hero_class',candidate.generated_class()),
                       ('reference_material',original),('candidate_material',mat)):actor.set_editor_property(prop,value)
    before=director_snapshot(actor);expected_copy=dict(expected,game_mode=path(mode.generated_class()),hero_class=path(candidate.generated_class()))
    check('every original live actor unchanged before save',map_snapshot(expected,actor)==expected_copy)
    check('native unique fixture map saved and reloaded',LEVEL.save_current_level() and LEVEL.load_level(MAP))
    actors=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2WingMaterialReviewDirector)]
    check('one saved fixture director',len(actors)==1)
    after=director_snapshot(actors[0]);check('all four native reference fields persist exactly',after==before)
    readback=map_snapshot(expected,actors[0]);check('every original live actor persists exactly',readback==expected_copy)
    for value in (MAP,mode_path):R['assets'].append(dict(path=value,sha256=sha(disk(value)),bytes=disk(value).stat().st_size))
    R.update(status='PASS',source_live_reload=binding(live_file,'evidence'),source_live_map=source_map,
        source_blueprint=wing['source_blueprint'],candidate_blueprint=wing['blueprint'],material_a=package(original),material_b=wing['material'],
        preserved_live_actor_count=expected['actor_count'],native_map_readback=readback,director_readback=after,
        expected_png_count=4,expected_resolution=[1920,1080],pass_scope='Native author and saved-map readback only; runtime/visual NOT_RUN.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(source_sha256_before=PROTECTED,changed_source_files=changed)
    R['source_bindings']=list(SOURCES.values())
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
if R['status']!='PASS':raise RuntimeError('Wing A/B fixture author failed; preserve evidence and partial unique assets.')
