"""Read-only native preflight for the already-PASS private seven-shot fixture.
No asset/config writes, no game launch. Deploy in tools and run HairSubstepProbe.
"""
from pathlib import Path
import datetime as dt,hashlib,json,re,traceback
import unreal as U
WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem);ACT=U.get_editor_subsystem(U.EditorActorSubsystem)
BASE=DOC/'editor_runtime/author_20260925_052016_850_270ec4ee_HairReviewAuthor/author_result.json'
HISTORY=DOC/'editor_runtime/20260925_052136_799_b6b922f2_hairreview_game/HairReview_20260925_052153_EB9F6D6B/hair_review.json'
# Exact template bake-source backlinks found in the original and retargeted files.
# This is not a general missing-soft-reference exemption. Native userdata readback
# below must prove the exact edge and its editor-only class on every encountered use.
EDITOR_BACKLINKS={
 '/Game/HarborCity/M5VS1/HeroSelestia/Animation/MM_Attack_01_Selestia':('e1088800482c0de23e7be575b354b1d52ff7ed2c3c210e74db8664c9d47013cc','LS_Attack_01'),
 '/Game/HarborCity/M5VS1/HeroSelestia/Animation/MM_Attack_02_Selestia':('d0c019c92c9c572f2a8f5867b9918c9b2ea434165f938ea8e196526a969df596','LS_Attack_02'),
 '/Game/HarborCity/M5VS1/HeroSelestia/Animation/MM_Attack_03_Selestia':('7c2f2b5bf899aa89c4aed3998bcc512c0e720f89aa23174325966c7d2d339748','LS_Attack_03'),
 '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01':('f796113f17c91eca7e48ef57bbadc4fc36245a86b127e75a34ca4771500639ed','LS_Attack_01'),
 '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02':('fb3c74b8e32fb5a88c6d659cae8717294af85d4eaa7f684b2a36c36d245559dc','LS_Attack_02'),
 '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03':('1c66016fc3bd386453ac7fb1229a5f0644c6bd5f98ead29f6e6a1be80632ba1f','LS_Attack_03'),
 '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack':('5d9b0fc6892519095cb6930aba5c0dd063acf647612a06e0f2f7e19c463223ee','LS_ChargedAttack'),
}
def arg(name):
    m=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(m)==1,name
    return m[0][0] or m[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve();assert OUT.is_relative_to(DOC/'editor_runtime') and arg('M5AuthorPhase')=='HairSubstepProbe' and not(OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
R=dict(schema='HarborCity.M5VS2.HairSubstepProbe.v1',phase='HairSubstepProbe',status='RUNNING',checks=[],assets_written=[],source_bindings=[],runtime='NOT_RUN',visual='USER_REVIEW',scope='Read-only preflight of original seven-shot private fixture; temporary process-wide Kawaii ini A/B, not node override.',started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
BOUND={}
def sha(f):
    h=hashlib.sha256()
    with Path(f).open('rb') as stream:
        for b in iter(lambda:stream.read(1048576),b''):h.update(b)
    return h.hexdigest()
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
def check(name,ok,actual=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',actual=actual))
    if not ok:dump();raise RuntimeError(name+': '+str(actual))
def bind(f,expected=None):
    f=Path(f).resolve();check('owned exact source',f.is_relative_to(WORK))
    actual=sha(f);check('source SHA unchanged',expected is None or actual==expected,str(f))
    BOUND[str(f)]=dict(path=str(f),sha256=actual,bytes=f.stat().st_size);return BOUND[str(f)]
def document(f):return json.loads(Path(f).read_text('utf-8-sig'))
def package(obj):return(obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def package_files(p):
    p=package(p)
    roots={'/Game/':PROJECT/'Content','/VRM4U/':PROJECT/'Plugins/VRM4U/Content','/KawaiiPhysics/':PROJECT/'Plugins/KawaiiPhysics/Content'}
    for prefix,root in roots.items():
        if p.startswith(prefix):
            candidate=root/Path(p[len(prefix):]);files=[candidate.with_suffix(e) for e in ('.uasset','.umap') if candidate.with_suffix(e).is_file()]
            return files
    raise RuntimeError('Unexpected owned package '+p)
def disk(p):
    files=package_files(p);check('one native package file',len(files)==1,p);return files[0]
def accept_editor_backlink(source,target,hard_refs):
    row=dict(source=source,target=target,dependency_kind='SOFT_PACKAGE',classification='EDITOR_SOURCE_SEQUENCE_BACKLINK',status='VERIFYING')
    R['editor_source_backlinks'].append(row)
    check('missing soft edge has exact frozen animation allowlist entry',source in EDITOR_BACKLINKS,row)
    expected_sha,leaf=EDITOR_BACKLINKS[source]
    expected_package='/Game/Characters/Mannequins/Animations/EditableAnimations/'+leaf
    expected_object=expected_package+'.'+leaf
    check('missing source sequence is only its precise soft edge',target==expected_package and target not in hard_refs,row)
    row['source_file']=bind(disk(source),expected_sha)
    animation=A.load_asset(source)
    check('backlink owner is actual native AnimSequence',animation is not None and animation.get_class().get_path_name()=='/Script/Engine.AnimSequence',source)
    data=list(animation.get_editor_property('asset_user_data'))
    links=[x for x in data if x is not None and x.get_class().get_path_name()=='/Script/LevelSequence.AnimSequenceLevelSequenceLink']
    check('one exact editor sequence link userdata',len(links)==1,dict(source=source,types=[x.get_class().get_path_name() for x in data if x is not None]))
    # StructBase.export_text is registered explicitly in PyWrapperStruct.cpp:1310.
    # It uses PPF_None; FSoftObjectPath::ExportTextItem then writes unquoted ToString().
    native_soft_path=links[0].get_editor_property('path_to_level_sequence')
    check('native backlink property is actual SoftObjectPath',isinstance(native_soft_path,U.SoftObjectPath),type(native_soft_path).__name__)
    actual_path=native_soft_path.export_text()
    row['userdata_class']=links[0].get_class().get_path_name();row['native_path_to_level_sequence']=actual_path
    row['native_export_api']='StructBase.export_text / PPF_None / FSoftObjectPath::ExportTextItem'
    check('native editor backlink property equals exact missing object',actual_path==expected_object,row)
    row['status']='EXCLUDED_EDITOR_ONLY_BACKLINK';row['runtime_animation_preserved']=True
def generated(p):
    p=package(p);c=U.load_class(None,p+'.'+p.rsplit('/',1)[-1]+'_C');check('native generated class',c is not None,p);return c
try:
    prior=document(BASE);command=document(BASE.parent/'commandlet.json');history=document(HISTORY)
    check('exact previous native author and game evidence',prior['schema']=='HarborCity.M5VS2.HairReview.Author.v1' and prior['status']=='PASS' and command['status']=='PASS' and command['exit_code']==0 and len(prior['assets'])==3 and history['status']=='PASS' and len(history['captures'])==7 and not history['user_stop_latched'])
    bind(BASE);bind(BASE.parent/'commandlet.json');bind(HISTORY)
    bind(WORK/'tools/ue_m5_vs2_hair_review_author.py',command['script_sha256'].lower())
    for asset in prior['assets']:bind(disk(asset['path']),asset['sha256'])
    source=Path(prior['source']['report']);bind(source,prior['source']['report_sha256']);integration=document(source)
    bind(source.parent/'commandlet.json');native=document(source.parent/'commandlet.json')
    check('actual historical integration reload',integration['status']=='PASS' and integration.get('fresh_process_disk_readback')=='PASS' and native['status']=='PASS' and native['exit_code']==0)
    for asset in integration['assets']:bind(disk(asset['path']),asset['sha256'])
    link_header=Path('E:/UE_5.8/Engine/Source/Runtime/LevelSequence/Public/AnimSequenceLevelSequenceLink.h')
    link_source=link_header.read_text('utf-8-sig')
    check('local native backlink class is explicitly editor-only','class UAnimSequenceLevelSequenceLink : public UAssetUserData' in link_source and 'virtual bool IsEditorOnly() const override { return true; }' in link_source and 'FSoftObjectPath PathToLevelSequence;' in link_source)
    R['editor_backlink_class_source']=dict(path=str(link_header),sha256=sha(link_header),editor_only_override=True,native_property='PathToLevelSequence',scope='Editor bake-source navigation backlink; original animation bytes and runtime tracks unchanged.')
    R['editor_source_backlinks']=[];R['dependency_edges']=[]
    ar=U.AssetRegistryHelpers.get_asset_registry()
    hard_options=U.AssetRegistryDependencyOptions(include_soft_package_references=False,include_hard_package_references=True,include_searchable_names=False,include_soft_management_references=False,include_hard_management_references=False)
    soft_options=U.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=False,include_searchable_names=False,include_soft_management_references=False,include_hard_management_references=False)
    pending=[prior['map']];seen=set();external=set()
    while pending:
        p=str(pending.pop())
        if p in seen:continue
        if not p.startswith(('/Game/','/VRM4U/','/KawaiiPhysics/')):external.add(p);continue
        seen.add(p);check('bounded asset closure',len(seen)<=1800);bind(disk(p))
        hard_refs={str(x) for x in ar.get_dependencies(p,hard_options)}
        soft_refs={str(x) for x in ar.get_dependencies(p,soft_options)}
        for target in sorted(hard_refs|soft_refs):
            kind='HARD_PACKAGE' if target in hard_refs else 'SOFT_PACKAGE'
            R['dependency_edges'].append(dict(source=p,target=target,kind=kind))
            if target.startswith(('/Game/','/VRM4U/','/KawaiiPhysics/')) and not package_files(target):
                check('no missing hard package dependencies',target not in hard_refs,dict(source=p,target=target,kind=kind))
                accept_editor_backlink(p,target,hard_refs)
            else:pending.append(target)
    R['package_closure_count']=len(seen);R['engine_or_script_references']=sorted(external)
    for folder in (PROJECT/'Config',PROJECT/'Plugins/KawaiiPhysics/Config'):
        if folder.exists():
            for f in folder.rglob('*.ini'):bind(f)
    for f in [PROJECT/'Binaries/Win64/UnrealEditor-HarborCity.dll',PROJECT/'Plugins/KawaiiPhysics/Binaries/Win64/UnrealEditor-KawaiiPhysics.dll',*[PROJECT/'Source/HarborCity/M5VS2'/('HCM5VS2HairReviewDirector.'+e) for e in ('h','cpp')]]:bind(f)
    descriptor=bind(PROJECT/'HarborCity.uproject');uproject=document(descriptor['path'])
    editor_modules=[x for x in uproject['Modules'] if x.get('Name')=='HarborCityEditor']
    check('current project editor module declaration is exact',len(editor_modules)==1 and editor_modules[0]['Type']=='UncookedOnly' and editor_modules[0]['LoadingPhase']=='Default',editor_modules)
    module_files=[PROJECT/'Binaries/Win64/UnrealEditor-HarborCityEditor.dll',PROJECT/'Source/HarborCityEditor.Target.cs',*sorted(f for f in (PROJECT/'Source/HarborCityEditor').rglob('*') if f.is_file() and f.suffix in ('.h','.cpp','.cs'))]
    check('editor module source closure present',len(module_files)>=7,[str(f) for f in module_files])
    R['current_editor_module_binding']=dict(descriptor=descriptor,files=[bind(f) for f in module_files],scope='Current native author and editor-game process modules; not a claim that this fixture uses the new FootPlacement node.')
    for f in (PROJECT/'Plugins/KawaiiPhysics/Source/KawaiiPhysics').rglob('*'):
        if f.is_file() and f.suffix in ('.h','.cpp'):bind(f)
    check('new diagnostic source merged', 'M5VS2HairSubstepAudit' in (PROJECT/'Source/HarborCity/M5VS2/HCM5VS2HairReviewDirector.cpp').read_text('utf-8-sig'))
    check('native source map load only',LEVEL.load_level(prior['map']))
    directors=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2HairReviewDirector)]
    check('original seven-shot director',len(directors)==1 and not directors[0].get_editor_property('outline_mask_comparison'))
    d=directors[0];hero=generated(prior['source']['hero']);expected=prior['source']['expected'];anim=generated(expected['animation']);mesh=A.load_asset(expected['mesh']);body=U.get_default_object(hero).get_editor_property('mesh')
    check('native private fixture and original CDO exact',d.get_editor_property('expected_character_class')==hero and d.get_editor_property('expected_animation_class')==anim and d.get_editor_property('expected_mesh')==mesh and body.get_editor_property('skeletal_mesh_asset')==mesh and body.get_editor_property('anim_class')==anim and [package(body.get_material(i)) for i in range(body.get_num_materials())]==expected['materials'])
    settings_class=U.load_class(None,'/Script/KawaiiPhysics.KawaiiPhysicsDeveloperSettings');check('actual settings class',settings_class is not None)
    settings=U.get_default_object(settings_class)
    # This plugin class currently arrives as the parent DeveloperSettings Python
    # wrapper. Its snake-case map lacks the derived properties. get_editor_property
    # falls back to native FName (PyWrapperObject.cpp:1223-1240 -> :754-756).
    # Use the exact UHT-registered property names; do not guess an alias or default.
    fixed=settings.get_editor_property('bUseFixedSubstepping')
    maximum=settings.get_editor_property('MaxSubsteps')
    check('actual native Kawaii setting value types',type(fixed) is bool and type(maximum) is int and maximum>=1,
          dict(fixed_substepping=fixed,max_substeps=maximum,python_wrapper_type=type(settings).__name__))
    R['native_settings_baseline']=dict(fixed_substepping=fixed,max_substeps=maximum,
        native_class=settings.get_class().get_path_name(),python_wrapper_type=type(settings).__name__,
        native_property_names=['bUseFixedSubstepping','MaxSubsteps'],
        readback_api='Object.get_editor_property exact native FName; no snake-case alias assumption')
    R['map']=prior['map'];R['hero']=prior['source']['hero'];R['expected']=expected;R['base_author']=bind(BASE);R['historical_runtime']=bind(HISTORY)
    R['native_readback']=dict(director=d.get_class().get_path_name(),hero=hero.get_path_name(),mesh=package(mesh),animation=package(anim),materials=[package(body.get_material(i)) for i in range(body.get_num_materials())],scale=str(body.get_editor_property('relative_scale3d')))
    R['source_bindings']=list(BOUND.values());R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,row in BOUND.items() if not Path(p).is_file() or sha(p)!=row['sha256']]
    R['preservation']=dict(changed=changed,asset_or_config_save_calls=0)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
    if R['status']=='PASS':(OUT/'hair_substep_manifest.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False)+'\n','utf-8')
if R['status']!='PASS':raise RuntimeError('HairSubstepProbe failed; keep original evidence')
