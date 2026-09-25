"""Native private safety shorts; source body/outfit untouched. Staged, never self-launches UE.
HeroModestyProbe is read-only; Apply creates 3 new assets; Reload fresh readback.
Topology is evidence of a closed garment, never a claim of all-angle runtime safety.
"""
from pathlib import Path
import ast,datetime as dt,hashlib,json,re,traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;E=U.MaterialEditingLibrary;B=U.BlueprintEditorLibrary
SCHEMA='HarborCity.M5VS2.HeroModesty.v1';OWNER='HarborCity_M5VS2_HeroModesty'
def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(rows)==1,name
    return rows[0][0] or rows[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase');assert OUT.is_relative_to(DOC/'editor_runtime') and PHASE in ('HeroModestyProbe','HeroModestyApply','HeroModestyReload') and not(OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True);TOKEN=hashlib.sha256(str(OUT).encode()).hexdigest()[:12];ROOT='/Game/HarborCity/M5VS2/HeroModesty/Review_'+TOKEN
R=dict(schema=SCHEMA,owner=OWNER,status='RUNNING',phase=PHASE,checks=[],inputs={},assets=[],map_writes=0,old_asset_writes=0,selection_file_written=False,
       runtime='NOT_RUN',low_angle_coverage='NOT_RUN',art='USER_REVIEW',fresh_process_disk_readback='NOT_RUN',started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       scope='New opaque skinned safety-shorts layer, actual body bone follower. Existing body/panty/skirt, rig, Kawaii, animation, gameplay and camera unchanged.')
PROTECTED={}
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed))
    if not ok:dump();raise RuntimeError(name+': '+str(observed))
def sha(f):
    h=hashlib.sha256()
    with Path(f).open('rb') as stream:
        for b in iter(lambda:stream.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def path(o):return o.get_path_name() if o is not None and hasattr(o,'get_path_name') else None
def package(o):return (path(o) if hasattr(o,'get_path_name') else str(o)).split('.')[0]
def disk(o):
    p=package(o);assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p[6:]).with_suffix('.uasset')
def protect(f,expected=None):
    f=Path(f).resolve();check('protected owned input',f.is_relative_to(WORK) and f.is_file(),str(f));actual=sha(f);check('input matches source hash',expected is None or actual==expected,str(f));PROTECTED[str(f)]=actual
    return dict(path=str(f),sha256=actual,bytes=f.stat().st_size)
def load(p):
    obj=A.load_asset(package(p));check('native existing asset loads',obj is not None,p);return obj
def native(name,text):
    d=json.loads(text);R[name]=d;check('native '+name,d.get('status')=='PASS',d);return d
def report(f,expected=None):
    f=Path(f);protect(f,expected);d=json.loads(f.read_text('utf-8-sig'));cmd=f.parent/'commandlet.json';protect(cmd);c=json.loads(cmd.read_text('utf-8-sig'))
    check('actual prerequisite author/process PASS',d.get('status')=='PASS' and all(x.get('status')=='PASS' for x in d.get('checks',[])) and c.get('status')=='PASS' and c.get('exit_code')==0,str(f));return d
def latest(phase):
    for f in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda f:f.stat().st_mtime,reverse=True):
        d=json.loads(f.read_text('utf-8-sig'))
        if d.get('schema')==SCHEMA and d.get('status')=='PASS':return f,report(f)
    raise RuntimeError('No actual '+phase+' PASS')
def install_readers():
    f=WORK/'tools/ue_m5_vs2_hero_rev2_integration_author.py';protect(f);text=f.read_text('utf-8-sig');tree=ast.parse(text)
    names=('vec','quat','transform','value','props','cdo_objects','preserved','scs_components');nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names]
    check('exact existing readonly Hero reflection helpers',tuple(n.name for n in nodes)==names)
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes);R['native_readers']=dict(path=str(f),functions=list(names),sha256=hashlib.sha256(code.encode()).hexdigest())
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(f)+'::readers','exec'),globals())
def source():
    select=DOC/'research/CORNER_REV2_HERO_REVIEW_SELECTION.json';R['inputs']['selection']=protect(select);s=json.loads(select.read_text('utf-8-sig'))
    check('actual selected integrated Hero',s.get('status')=='SELECTED_FOR_REVIEW' and s['blueprint'].startswith('/Game/HarborCity/M5VS2/HeroRev2/Review_'))
    fresh=False
    for ref in s['source_reports']:
        d=report(ref['path'],ref['sha256'])
        for row in d.get('assets',[]):protect(disk(row['path']),row['sha256'])
        if d.get('phase')=='HeroRev2IntegrationReload':fresh=d.get('fresh_process_disk_readback')=='PASS' and d.get('blueprint')==s['blueprint']
    check('selected blueprint native fresh provenance',fresh);bp=load(s['blueprint']);protect(disk(bp));return bp
def baseline(bp):
    hero,body,combat,presentation=cdo_objects(bp);components=scs_components(bp)
    keep=dict(mesh=path(body.get_editor_property('skeletal_mesh_asset')),animation=path(body.get_editor_property('anim_class')),
              materials=[path(body.get_material(i)) for i in range(body.get_num_materials())],preserved=preserved(hero,body,combat,presentation),
              combat_animation=path(combat.get_editor_property('player_animation_blueprint')),first_person_arms=path(presentation.get_editor_property('first_person_arms_override')))
    fields=((U.HCM5VS2HeroOutlineComponent,('outline_materials',)),(U.HCM5VS2HaloComponent,('pieces','source_degrees_per_second','glow_material','source_head_transform')),
            (U.HCM5VS2FlightVisualComponent,('feather_mesh','ribbon_mesh','rune_mesh','light_material')))
    for kind,names in fields:
        matched=[c for c in components if isinstance(c,kind)];check('one preserved existing '+kind.__name__,len(matched)==1);keep[kind.__name__]=props(matched[0],names)
    keep['component_class_counts']={}
    for c in components:
        if isinstance(c,U.HCM5VS2HeroModestyComponent):continue
        key=c.get_class().get_path_name();keep['component_class_counts'][key]=keep['component_class_counts'].get(key,0)+1
    return keep

def duplicate(obj,name):
    p=ROOT+'/'+name;check('unique new candidate only',not A.does_asset_exist(p) and not disk(p).exists());out=A.duplicate_asset(package(obj),p);check('native duplicate',out is not None,p);return out
def save(obj):
    p=package(obj);check('save only new private namespace',p.startswith(ROOT+'/'));A.set_metadata_tag(obj,'HarborCityOwnedBy',OWNER);check('save new native asset',A.save_loaded_asset(obj,False));R['assets'].append(dict(path=p,sha256=sha(disk(p)),bytes=disk(p).stat().st_size))
def make_material():
    m=U.AssetToolsHelpers.get_asset_tools().create_asset('M_SafetyShorts_Cloth',ROOT,U.Material,U.MaterialFactoryNew());check('private cloth material created',m is not None)
    m.set_editor_property('blend_mode',U.BlendMode.BLEND_OPAQUE);m.set_editor_property('two_sided',False);m.set_editor_property('used_with_skeletal_mesh',True)
    color=E.create_material_expression(m,U.MaterialExpressionConstant3Vector,0,0);color.set_editor_property('constant',U.LinearColor(.0301,.0196,.0397,1))
    check('opaque cloth base input',E.connect_material_property(color,'',U.MaterialProperty.MP_BASE_COLOR))
    for field,val,y in [(U.MaterialProperty.MP_ROUGHNESS,.85,160),(U.MaterialProperty.MP_SPECULAR,.08,240)]:
        node=E.create_material_expression(m,U.MaterialExpressionConstant,0,y);node.set_editor_property('r',val);check('matte scalar material connection',E.connect_material_property(node,'',field))
    E.recompile_material(m);return m

def inspect(bp,body,shorts,material,keep):
    check('all original gameplay/character/render bindings preserved',baseline(bp)==keep)
    components=[c for c in scs_components(bp) if isinstance(c,U.HCM5VS2HeroModestyComponent)];check('one private garment component',len(components)==1)
    actual=props(components[0],('safety_shorts','cloth_material'));check('real native component binding',actual==dict(safety_shorts=path(shorts),cloth_material=path(material)),actual)
    check('candidate opaque and used with skinning',material.get_editor_property('blend_mode')==U.BlendMode.BLEND_OPAQUE and bool(material.get_editor_property('used_with_skeletal_mesh')) and not material.get_editor_property('two_sided'))
    topology=native('saved_topology',U.HCM5VS2HeroModestyEditor.inspect_safety_shorts(body,shorts))
    return dict(blueprint=package(bp),source_body=path(body),shorts=path(shorts),material=path(material),component=actual,topology=topology,preserved=baseline(bp))
try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty content/maps',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    check('real registered helper',hasattr(U,'HCM5VS2HeroModestyEditor') and hasattr(U,'HCM5VS2HeroModestyComponent'));install_readers()
    if PHASE=='HeroModestyReload':
        f,prior=latest('HeroModestyApply');R['inputs']['apply']=protect(f);ROOT=prior['destination'];check('private destination exact',re.fullmatch(r'/Game/HarborCity/M5VS2/HeroModesty/Review_[0-9a-f]{12}',ROOT) is not None)
        for name,h in prior['preservation']['source_sha256_before'].items():protect(name,h)
        for row in prior['assets']:protect(disk(row['path']),row['sha256'])
        bp=load(prior['blueprint']);body=load(prior['source_body']);shorts=load(prior['shorts']);material=load(prior['material'])
        readback=inspect(bp,body,shorts,material,prior['source_baseline']);check('fresh-process CDO/SCS/closed geometry readback exactly matches',readback==prior['native_readback'])
        R.update(status='PASS',fresh_process_disk_readback='PASS',destination=ROOT,blueprint=prior['blueprint'],source_blueprint=prior['source_blueprint'],source_body=prior['source_body'],shorts=prior['shorts'],material=prior['material'],native_readback=readback,assets=prior['assets'],source_baseline=prior['source_baseline'])
    else:
        protect(__file__)
        for ext in ('.h','.cpp'):protect(PROJECT/'Source/HarborCity/M5VS2'/('HCM5VS2HeroModestyComponent'+ext))
        protect(PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll')
        bp=source();keep=baseline(bp);hero=U.get_default_object(bp.generated_class());body=hero.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset');protect(disk(body))
        R.update(source_blueprint=package(bp),source_body=package(body),source_baseline=keep)
        probe=native('body_probe',U.HCM5VS2HeroModestyEditor.probe_body(body));check('read-only probe did not mutate',probe.get('asset_mutated') is False)
        if PHASE=='HeroModestyProbe':R.update(status='PASS',pass_scope='Read-only native source topology/clipping feasibility; no garment saved, runtime NOT_RUN.')
        else:
            _,previous=latest('HeroModestyProbe');check('actual separate source probe matches',previous['source_body']==R['source_body'] and previous['source_baseline']==keep and previous['body_probe']==probe)
            check('private destination absent',not A.does_directory_exist(ROOT));material=make_material();shorts=duplicate(body,'SKM_SafetyShorts');candidate=duplicate(bp,'BP_HeroSafetyShorts')
            result=native('native_build',U.HCM5VS2HeroModestyEditor.build_safety_shorts(body,shorts,material));check('helper did not self-save',result.get('saved_by_helper') is False)
            native('native_binding',U.HCM5VS2HeroModestyEditor.configure_modesty(candidate,shorts,material));B.compile_blueprint(candidate)
            before=inspect(candidate,body,shorts,material,keep)
            for obj in (material,shorts,candidate):save(obj)
            after=inspect(candidate,body,shorts,material,keep);check('same-process saved readback',before==after)
            R.update(status='PASS',destination=ROOT,blueprint=package(candidate),shorts=package(shorts),material=package(material),native_readback=after,
                     chosen_cloth=dict(base_linear_rgb=[.0301,.0196,.0397],approx_srgb_rgb=[.19,.15,.22],roughness=.85,specular=.08,source='Project-authored matte dark-purple garment; not claimed original artist material'),
                     pass_scope='Private native closed geometry and component creation; fresh Reload, low angles, jump/flight/body-intersection/art still NOT_RUN.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h];R['preservation']=dict(source_sha256_before=PROTECTED,changed_source_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Hero modesty candidate failed; no rollback of prior evidence, retain unique partial assets.')
