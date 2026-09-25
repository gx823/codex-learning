"""Private J Chaos clothing prototype. Root alone runs native Probe/Apply/Reload.

No map, original mesh/material/PHYS, shared plugin, or engine edits. A PASS here
only establishes native authoring/readback; cloth appearance remains NOT_RUN.
"""
from pathlib import Path
import ast,copy,datetime as dt,hashlib,json,os,re,traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
STAGE=WORK/'tools/staging_npc_j_clothing';FREEZE=WORK/'tools/npc_j_clothing_source_revision_6.json'
SOURCE=DOC/'editor_runtime/author_20260925_051452_501_d1ffc0a0_NPCIdleR2JReload/author_result.json'
SOURCE_SHA='13de3b1306a7322a4e24c0e4474317cc0b9025cfb05e6276b6a9c55b2eb14503'
COMMAND_SHA='644f2116724669545b190856b541b81dcf985717e5adc8589d936a7ee492ab5b'
JROOT='/Game/HarborCity/M5VS2/NPC/AvatarSample_J';TREE=PROJECT/'Content'/JROOT.removeprefix('/Game/')
SOURCE_META=JROOT+'/Source_8b6562a56a4a/VM_AvatarSample_J_Studio2140_VrmMeta'
SOURCE_POST=JROOT+'/Source_8b6562a56a4a/ABP_Post_AvatarSample_J_Studio2140'
A,B,E=U.EditorAssetLibrary,U.BlueprintEditorLibrary,U.MaterialEditingLibrary
MATERIAL_ROOT='/VRM4U/MaterialUtil/UE5/Material/'
ANCESTORS=(('M_VrmMToonBaseOpaque','M_JClothing_BaseOpaque'),
           ('MI_VrmMToonBaseLitOpaque','MI_JClothing_BaseLitOpaque'),
           ('MI_VrmMToonOptLitOpaque','MI_JClothing_OptLitOpaque'),
           ('MI_VrmMToonOptLitOpaqueTwoSided','MI_JClothing_OptLitOpaqueTwoSided'))
GARMENT_SLOTS=(10,12,13)
PLUGIN_MATERIAL_TREE=PROJECT/'Plugins/VRM4U/Content/MaterialUtil'
def arg(name):
    values=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(values)==1,name
    return values[0][0] or values[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase');MATCH=re.fullmatch(r'NPCJClothing(Probe|Apply|Reload)',PHASE)
assert MATCH and OUT.is_relative_to(DOC/'editor_runtime') and not(OUT/'author_result.json').exists()
MODE=MATCH[1];OUT.mkdir(parents=True,exist_ok=True);TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
DEST=JROOT+'/GarmentR2/Batch_'+TOKEN
R=dict(schema='HarborCity.M5VS2.NPCJClothing.Author.v1',phase=PHASE,mode=MODE,status='RUNNING',checks=[],inputs={},assets=[],process_id=os.getpid(),
       destination=DEST,runtime='NOT_RUN',visual='USER_REVIEW',fresh_process_disk_readback='NOT_RUN',original_asset_writes=0,map_writes=0,physics_writes=0,
       limits=['Private first J prototype only; not eight-cast clothing acceptance.','Three independent clothing assets: inter-layer collisions not yet validated.',
               'Cloth environment queries cover WorldStatic simple collision; external dynamic vehicles/NPC are not claimed.',
               'CPU linear skinning diagnostic excludes actual cloth particles and cannot be used to accept this candidate.'],
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={};BEFORE=set();PLUGIN_BEFORE=set()
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed))
    if not ok:dump();raise RuntimeError(name)
def document(p):return json.loads(Path(p).read_text('utf-8-sig'))
def package(o):return(o.get_path_name() if hasattr(o,'get_path_name') else str(o)).split('.')[0]
def path(o):return o.get_path_name() if o is not None else None
def disk(o):
    p=package(o);assert p.startswith(JROOT+'/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
def source_material_disk(o):
    p=package(o)
    if p.startswith(JROOT+'/'):return disk(o)
    check('exact read-only plugin ancestor package',p in {MATERIAL_ROOT+a for a,_ in ANCESTORS},p)
    return PROJECT/'Plugins/VRM4U/Content'/Path(p.removeprefix('/VRM4U/')).with_suffix('.uasset')
def protect(p,expected=None):
    p=Path(p).resolve();check('owned protected source exists',p.is_relative_to(WORK) and p.is_file(),str(p));actual=sha(p)
    check('source SHA unchanged',expected is None or actual==expected,str(p));PROTECTED[str(p)]=actual
    return dict(path=str(p),sha256=actual,bytes=p.stat().st_size)
def load(p):
    o=A.load_asset(p);check('native exact asset load',o is not None,p);return o
def native(name,*args):
    row=json.loads(getattr(U.HCM5VS2NPCJClothingEditor,name)(*args));R.setdefault('native',{})[name]=row;dump()
    check('native '+name,row.get('status')=='PASS',row);return row
def latest(mode):
    for f in sorted((DOC/'editor_runtime').glob('author_*_NPCJClothing'+mode+'/author_result.json'),key=lambda x:x.stat().st_mtime,reverse=True):
        d=document(f)
        if d.get('schema')==R['schema'] and d.get('status')=='PASS':
            command=f.with_name('commandlet.json');c=document(command)
            check('completed prerequisite native process',c.get('status')=='PASS' and c.get('exit_code')==0 and c.get('script_sha256','').lower()==sha(__file__),str(command))
            for p,h in d['protected_sources'].items():protect(p,h)
            return f,d
    raise RuntimeError('No completed current-script NPCJClothing'+mode)
def duplicate(o,name):
    dest=DEST+'/'+name;check('fresh private asset destination',not A.does_asset_exist(dest) and not disk(dest).exists(),dest)
    result=A.duplicate_asset(package(o),dest);check('native duplicate',result is not None,dest);return result
def save(o):
    p=package(o);check('save only private batch',p.startswith(DEST+'/'),p)
    A.set_metadata_tag(o,'HarborCityOwnedBy','M5VS2_NPCJClothing');check('native save private candidate',A.save_loaded_asset(o,False) and disk(o).is_file(),p)
    R['assets']=[x for x in R['assets'] if x['path']!=p]+[dict(path=p,sha256=sha(disk(o)),bytes=disk(o).stat().st_size)]
def install_readers():
    script=WORK/'tools/ue_m5_vs2_npc_idle_r2_author.py';text=script.read_text('utf-8-sig');names=('xyz','transform','getup_snapshot','cdo_snapshot')
    nodes=[n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name in names]
    content='\n\n'.join(ast.get_source_segment(text,n) for n in nodes)
    check('frozen original read-only CDO snapshot functions',tuple(n.name for n in nodes)==names and hashlib.sha256(content.encode()).hexdigest()=='d5efc3b3e67015cf252276293bd306240dc03257ce27c263ee78538758c803e1')
    R['inputs']['cdo_readers']=protect(script);exec(compile(ast.Module(body=nodes,type_ignores=[]),str(script)+'::readonly_four','exec'),globals())

def material_plan(probe):
    rows=probe['sections'];check('exact three unsupported garment sections',
        [r['section'] for r in rows]==list(GARMENT_SLOTS) and [r['material_index'] for r in rows]==list(GARMENT_SLOTS)
        and all(r['clothing_usage_already_enabled'] is False for r in rows))
    result=[]
    for row in rows:
        chain=row['material_chain_leaf_to_root']
        expected=[row['material'].split('.')[0]]+[MATERIAL_ROOT+a for a,_ in reversed(ANCESTORS)]
        check('native exact five-level original material chain',
              [x['path'].split('.')[0] for x in chain]==expected and
              [x['class'] for x in chain]==['/Script/Engine.MaterialInstanceConstant']*4+['/Script/Engine.Material'],chain)
        for link in chain:protect(source_material_disk(link['path']))
        result.append(dict(slot=row['material_index'],source=row['material'],candidate_name='MI_JClothing_Section'+str(row['material_index'])))
    R['material_compatibility_plan']=dict(leaf_slots=list(GARMENT_SLOTS),shared_ancestors=[dict(source=MATERIAL_ROOT+a,candidate_name=b) for a,b in ANCESTORS],
        leaf_materials=result,copy_count=7,changed_root_property='bUsedWithClothing false -> true',shader_graph_edits=0,texture_or_parameter_edits=0,
        original_usage_remains_false=True,rendered_equivalence='NOT_RUN; native graph/parameter comparison is required before author PASS')
    return result

def make_materials(source_mesh,newmesh,leaves):
    copies=[];parent=None
    for original,name in ANCESTORS:
        source=load(MATERIAL_ROOT+original);copied=duplicate(source,name)
        if parent is None:
            check('exact root Material is copied',isinstance(source,U.Material) and isinstance(copied,U.Material))
            check('original root missing only requested clothing usage',source.get_editor_property('used_with_clothing') is False)
            copied.set_editor_property('used_with_clothing',True)
            errors=list(E.recompile_material(copied));check('private root material native compile has no errors',not errors,errors)
        else:
            check('exact ancestor MIC type and original parent',isinstance(copied,U.MaterialInstanceConstant) and package(source.get_editor_property('parent'))==MATERIAL_ROOT+previous)
            E.set_material_instance_parent(copied,parent);E.update_material_instance(copied)
        copies.append(copied);parent=copied;previous=original
    materials=list(newmesh.get_editor_property('materials'))
    for row in leaves:
        source=load(row['source']);copied=duplicate(source,row['candidate_name'])
        check('source garment leaf still uses exact original shared parent',package(source.get_editor_property('parent'))==MATERIAL_ROOT+ANCESTORS[-1][0])
        E.set_material_instance_parent(copied,parent);E.update_material_instance(copied)
        materials[row['slot']].set_editor_property('material_interface',copied);copies.append(copied)
    newmesh.set_editor_property('materials',materials)
    check('exact seven private material compatibility copies',len(copies)==7)
    native('copy_private_streaming_data',source_mesh,newmesh)
    return copies

def make_runtime_fixture(newbp):
    # Reuse the existing baked neutral JT arena. Only J and its references change.
    source_file=DOC/'editor_runtime/author_20260925_103039_258_223c5b14_NPCReactionR2IdleOpenJT/author_result.json'
    src=document(source_file); check('existing JT arena author passed',src['status']=='PASS')
    original=PROJECT/'Content'/Path(src['map'].removeprefix('/Game/')).with_suffix('.umap')
    original_row=next(x for x in src['assets'] if x['path']==src['map']);protect(original,original_row['sha256'])
    fixture='/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/IdlePairs/Attempt_'+TOKEN+'/L_NPCReaction_Open_JT'
    level=U.get_editor_subsystem(U.LevelEditorSubsystem);actors=U.get_editor_subsystem(U.EditorActorSubsystem)
    check('fresh J cloth fixture',not A.does_asset_exist(fixture))
    check('native duplicate baked arena',A.duplicate_asset(src['map'],fixture) is not None)
    check('load private arena',level.load_level(fixture))
    all_actors=actors.get_all_level_actors()
    director=next(x for x in all_actors if isinstance(x,U.HCM5VS2NPCReactionReviewDirector))
    people=list(director.get_editor_property('specimens'));old=people[0]
    check('original ordered JT fixture',str(old.get_editor_property('stable_id'))=='M5VS2_J' and str(people[1].get_editor_property('stable_id'))=='M5VS2_T')
    new=actors.spawn_actor_from_class(B.generated_class(newbp),old.get_actor_location(),old.get_actor_rotation())
    check('spawn private clothed J',new is not None)
    new.set_actor_transform(old.get_actor_transform(),False,True);new.set_actor_label('J_ChaosClothingReview')
    for prop in ('stable_id','stationary','talkable','navigation_region','tags'):
        new.set_editor_property(prop,old.get_editor_property(prop))
    body=new.get_component_by_class(U.SkeletalMeshComponent)
    body.set_editor_property('wait_for_parallel_cloth_task',True)
    people[0]=new;director.set_editor_property('specimens',people)
    director.get_editor_property('experience').set_editor_property('npcs',people)
    check('remove only old J from private map',actors.destroy_actor(old))
    check('save new private arena',level.save_current_level())
    file=PROJECT/'Content'/Path(fixture.removeprefix('/Game/')).with_suffix('.umap')
    R['fixture']=dict(map=fixture,path=str(file),sha256=sha(file),source_map=src['map'],only_changed_actor='J',
                      input_scope='FUNCTION_CALL_GAMEPLAY_NOT_OS_NOT_ACTUAL_VEHICLE_CONTACT',cloth_wait=True)
    R['map_writes']=1

try:
    frozen=document(FREEZE);R['inputs']['helper_freeze']=protect(FREEZE)
    for row in frozen['files']:
        f=Path(row['path']);protect(PROJECT/'Source/HarborCityEditor'/f.name,row['sha256'])
    R['current_native_module']=protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCityEditor.dll')
    protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll');protect(PROJECT/'HarborCity.uproject')
    R['inputs']['idle_reload']=protect(SOURCE,SOURCE_SHA);protect(SOURCE.with_name('commandlet.json'),COMMAND_SHA)
    source=document(SOURCE);cmd=document(SOURCE.with_name('commandlet.json'))
    check('actual completed J idle fresh reload',source['status']=='PASS' and source['phase']=='NPCIdleR2JReload' and source['fresh_process_disk_readback']=='PASS' and cmd['status']=='PASS' and cmd['exit_code']==0)
    for row in source['assets']:protect(disk(row['path']),row['sha256'])
    BEFORE={str(p.resolve()) for p in TREE.rglob('*') if p.is_file()}
    for p in sorted(BEFORE):protect(p)
    PLUGIN_BEFORE={str(p.resolve()) for p in PLUGIN_MATERIAL_TREE.rglob('*') if p.is_file()}
    check('existing read-only plugin material tree is present',len(PLUGIN_BEFORE)>100)
    for p in sorted(PLUGIN_BEFORE):protect(p)
    install_readers();spec=source['specimen'];bp=load(source['candidate_blueprint']);before=cdo_snapshot(bp)
    check('original J exact current IdleReload CDO',before==source['candidate_cdo'])
    mesh=load(spec['mesh']);physics=load(spec['physics']);meta=load(SOURCE_META);post=load(SOURCE_POST);profile=load(spec['profile'])
    R['source_blueprint']=package(bp);R['source_cdo']=before
    probe=native('probe_source',mesh,physics)
    leaves=material_plan(probe)
    if MODE!='Probe':
        if MODE=='Apply':
            R['probe_scope']='Native probe_source above is the same-process preflight; no redundant probe process.'
            check('entire new private directory is absent',not disk(DEST+'/SK_NPCJ_Clothing').parent.exists())
            newmesh=duplicate(mesh,'SK_NPCJ_Clothing');newmeta=duplicate(meta,'VM_NPCJ_Clothing');newpost=duplicate(post,'ABP_NPCJ_ClothingPost')
            newprofile=duplicate(profile,'DA_NPCJ_Clothing');newbp=duplicate(bp,'BP_NPCJ_Clothing')
            material_copies=make_materials(mesh,newmesh,leaves)
            native('validate_private_materials',mesh,newmesh)
            native('build_private_clothing',mesh,newmesh,physics,True)
            native('configure_private_spring',newmesh,meta,newmeta,post,newpost,True)
            newprofile.set_editor_property('mesh',newmesh);newprofile.set_editor_property('metadata_asset_path',package(newmeta))
            cdo=U.get_default_object(B.generated_class(newbp));cdo.modify();body=cdo.get_component_by_class(U.SkeletalMeshComponent);body.modify()
            body.set_skeletal_mesh_asset(newmesh);body.set_editor_property('collide_with_environment',True)
            body.set_editor_property('disable_cloth_simulation',False);body.set_editor_property('reset_after_teleport',True)
            for row in leaves:body.set_material(row['slot'],load(DEST+'/'+row['candidate_name']))
            cdo.set_editor_property('npc_profile',newprofile)
            for o in (*material_copies,newmeta,newpost,newmesh,newprofile,newbp):save(o)
            # The unchanged character main animation graph / getup clips remain selected.
        else:
            prior_file,prior=latest('Apply');R['inputs']['candidate_evidence']=protect(prior_file);DEST=prior['destination'];R['destination']=DEST
            check('new process really differs from Apply',prior['process_id']!=os.getpid())
            for row in prior['assets']:protect(disk(row['path']),row['sha256'])
            newmesh=load(DEST+'/SK_NPCJ_Clothing');newmeta=load(DEST+'/VM_NPCJ_Clothing');newpost=load(DEST+'/ABP_NPCJ_ClothingPost')
            newprofile=load(DEST+'/DA_NPCJ_Clothing');newbp=load(DEST+'/BP_NPCJ_Clothing');R['assets']=prior['assets']
            check('native Apply module and source match this Reload',prior['current_native_module']==R['current_native_module'])
        native('validate_private_materials',mesh,newmesh)
        native('build_private_clothing',mesh,newmesh,physics,False)
        native('configure_private_spring',newmesh,meta,newmeta,post,newpost,False)
        expected=copy.deepcopy(before);expected.update(mesh=package(newmesh),profile=package(newprofile))
        for row in leaves:expected['materials'][row['slot']]=path(load(DEST+'/'+row['candidate_name']))
        actual=cdo_snapshot(newbp);check('original animation/physics/getup/identity plus exact three compatible garment slots',actual==expected,actual)
        cdo=U.get_default_object(B.generated_class(newbp));body=cdo.get_component_by_class(U.SkeletalMeshComponent)
        state={k:bool(body.get_editor_property(k)) for k in ('collide_with_environment','disable_cloth_simulation','reset_after_teleport')}
        check('native clothing component policy',state==dict(collide_with_environment=True,disable_cloth_simulation=False,reset_after_teleport=True),state)
        check('profile private mesh and explicit metadata match',newprofile.get_editor_property('mesh')==newmesh and newprofile.get_editor_property('metadata_asset_path')==package(newmeta))
        R['candidate_blueprint']=package(newbp);R['candidate_cdo']=actual;R['clothing_component_policy']=state
        R['specimen']=dict(spec,blueprint=package(newbp),generated_class=path(B.generated_class(newbp)),mesh=package(newmesh),profile=package(newprofile),
                           postprocess=package(newpost),metadata=package(newmeta),spring='Private J removes Skirt groups 2/4/5 only; 18 original groups retained; three native Chaos clothing assets')
        if MODE=='Reload':R['fresh_process_disk_readback']='PASS'
        if MODE=='Apply':make_runtime_fixture(newbp)
    check('original J CDO unchanged in memory',cdo_snapshot(bp)==before)
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    after={str(p.resolve()) for p in TREE.rglob('*') if p.is_file()}
    plugin_after={str(p.resolve()) for p in PLUGIN_MATERIAL_TREE.rglob('*') if p.is_file()}
    allowed=disk(DEST+'/SK_NPCJ_Clothing').parent
    unexpected=[p for p in sorted(after-BEFORE) if MODE!='Apply' or not Path(p).is_relative_to(allowed)]
    plugin_added=sorted(plugin_after-PLUGIN_BEFORE);plugin_removed=sorted(PLUGIN_BEFORE-plugin_after)
    if changed or unexpected or plugin_added or plugin_removed:R['status']='FAIL'
    R['protected_sources']=PROTECTED;R['preservation']=dict(changed=changed,unexpected_added=unexpected,added_private=[p for p in sorted(after-BEFORE) if p not in unexpected],
        shared_plugin_material_tree_added=plugin_added,shared_plugin_material_tree_removed=plugin_removed)
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('NPC J clothing author failed; preserve evidence and partial private assets')
