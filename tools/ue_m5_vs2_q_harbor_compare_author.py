"""One isolated afternoon harbor map; source/candidate Q skin only, no source edits."""
import hashlib,json,re,traceback
from pathlib import Path
import unreal as U
W=Path('D:/科研学习/codex学习');P=W/'HarborCity';D=W/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;L=U.get_editor_subsystem(U.LevelEditorSubsystem);E=U.get_editor_subsystem(U.EditorActorSubsystem);M=U.MaterialEditingLibrary
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(args)==1
O=Path(args[0][0] or args[0][1]).resolve();assert O.is_relative_to(D/'editor_runtime')
ROOT='/Game/HarborCity/M5VS2/WorldRev2/QSkinReview_'+hashlib.sha256(str(O).encode()).hexdigest()[:12]
R=dict(status='RUNNING',phase='QHarborCompareAuthor',map=ROOT+'/L_QHarborCompare',checks=[],sources={},assets=[])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def disk(p,ext='.uasset'):return P/'Content'/Path(p.split('.')[0].removeprefix('/Game/')).with_suffix(ext)
def keep(p):R['sources'][str(p)]=sha(p)
def check(n,v):
    R['checks'].append(dict(name=n,status='PASS' if v else 'FAIL'))
    if not v:raise RuntimeError(n)
try:
    src=D/'editor_runtime/author_20260925_132335_738_fbf698ae_PlayablePolishAuthor/author_result.json'
    hero=json.loads(src.read_text('utf-8-sig'));check('actual completed street source',hero['status']=='PASS');keep(src)
    source=hero['maps']['Afternoon'];keep(disk(source,'.umap'))
    skinfile=D/'editor_runtime/author_20260925_042816_270_966823a4_QSkinCandidate/author_result.json'
    skin=json.loads(skinfile.read_text('utf-8-sig'));check('actual saved skin candidate',skin['status']=='PASS');keep(skinfile)
    for row in skin['assets']:
        f=disk(row['path']);check('unchanged candidate asset',sha(f)==row['sha256']);keep(f)
    # Previous actual harbor comparison was too grey. This keeps source textures,
    # all shadow receiving and source parents, changes only two skin colors/toon edge.
    R['candidate_root']='/Game/HarborCity/M5VS2/NPCSkinCandidates/Q_'+ROOT.rsplit('_',1)[1]
    R['skin_revision']='HarborWarm02';R['skin_parameters']={}
    for row in skin['candidates']:
        check('new skin material namespace',not A.does_asset_exist(R['candidate_root']+'/MI_Q_'+row['part']+'_WarmY'))
        material=U.AssetToolsHelpers.get_asset_tools().create_asset('MI_Q_'+row['part']+'_WarmY',R['candidate_root'],U.MaterialInstanceConstant,U.MaterialInstanceConstantFactoryNew())
        M.set_material_instance_parent(material,A.load_asset(row['source']))
        colors={'mtoon_Color':[1.20,1.03,.89,1.0],'mtoon_ShadeColor':[1.10,.80,.74,1.0]}
        for name,color in colors.items():M.set_material_instance_vector_parameter_value(material,name,U.LinearColor(*color))
        M.set_material_instance_scalar_parameter_value(material,'mtoon_ShadeToony',.75 if row['part']=='Face' else .85)
        M.update_material_instance(material);check('save private skin revision',A.save_loaded_asset(material,False))
        R['skin_parameters'][row['part']]=colors
        R['assets'].append(dict(path=material.get_path_name(),sha256=sha(disk(material.get_path_name()))))
    check('fresh map path',not A.does_asset_exist(R['map']))
    check('duplicate harbor map',A.duplicate_asset(source,R['map']) is not None)
    check('load isolated map',L.load_level(R['map']))
    actors=E.get_all_level_actors()
    qs=[a for a in actors if isinstance(a,U.HCM5VS2NPC) and '/AvatarSample_Q/' in a.get_component_by_class(U.SkeletalMeshComponent).get_editor_property('skeletal_mesh_asset').get_path_name()]
    suns=[a for a in actors if isinstance(a,U.DirectionalLight) and ('HC_VS2_Rev2_Sun' in [str(t) for t in a.get_editor_property('tags')] or a.get_actor_label()=='HC_VS2_Rev2_Sun')]
    R['discovered_subjects']=dict(q=[a.get_path_name() for a in qs],sun=[a.get_path_name() for a in suns])
    check('exactly one Q and one sun',len(qs)==1 and len(suns)==1)
    q=qs[0];q.set_editor_property('stationary',True)
    body=q.get_component_by_class(U.SkeletalMeshComponent)
    for row in skin['candidates']:
        body.set_material(row['slot'],A.load_asset(row['source']));keep(disk(row['source']))
    R['subject']=dict(actor=q.get_path_name(),blueprint=q.get_class().get_path_name(),transform=q.get_actor_transform().export_text(),
        mesh=body.get_editor_property('skeletal_mesh_asset').get_path_name(),materials=[body.get_material(i).get_path_name() for i in range(body.get_num_materials())])
    check('no existing NPC review controller',not any(isinstance(a,U.HCM5VS2NPCReviewDirector) for a in actors))
    director=E.spawn_actor_from_class(U.HCM5VS2NPCReviewDirector,U.Vector(0,0,-100))
    check('spawn native isolated review',director is not None)
    director.set_actor_label('HC_Q_Harbor_SkinComparison');director.set_editor_property('specimens',[q]);director.set_editor_property('main_light',suns[0])
    check('save isolated native map',L.save_current_level());R['map_sha256']=sha(disk(R['map'],'.umap'));R['assets'].append(dict(path=R['map'],sha256=R['map_sha256']))
    check('source bytes retained',all(sha(p)==h for p,h in R['sources'].items()))
    R.update(status='PASS',runtime='NOT_RUN',visual='USER_REVIEW',scope='Same afternoon harbor/Q/pose/camera/sun; native runtime switches only face slot 3 and body slot 7.')
except Exception:
    R.update(status='FAIL',error=traceback.format_exc())
finally:
    (O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError(R.get('error'))
