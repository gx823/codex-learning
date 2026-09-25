"""Isolated A/B for ragdoll-pose bounds changing without actor movement. No mesh/mask/rig edits."""
from pathlib import Path
import hashlib,json,re,traceback
import unreal as U
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2';P=W/'HarborCity'
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(args)==1
O=Path(args[0][0] or args[0][1]).resolve();assert O.is_relative_to(D/'editor_runtime')
source=D/'editor_runtime/author_20260925_135440_420_31e1aad0_NPCJClothingApply/author_result.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def disk(p):return P/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
R=dict(status='RUNNING',phase='NPCJClothEnvironmentAB',parent_author=str(source),parent_author_sha256=sha(source))
try:
    old=json.loads(source.read_text('utf-8-sig'));assert old['status']=='PASS'
    for row in old['assets']:assert sha(disk(row['path']))==row['sha256']
    assert sha(old['fixture']['path'])==old['fixture']['sha256']
    dest='/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/IdlePairs/Attempt_'+hashlib.sha256(str(O).encode()).hexdigest()[:12]+'/L_NPCReaction_Open_JT'
    a=U.EditorAssetLibrary;l=U.get_editor_subsystem(U.LevelEditorSubsystem);e=U.get_editor_subsystem(U.EditorActorSubsystem)
    assert not a.does_asset_exist(dest);assert a.duplicate_asset(old['fixture']['map'],dest);assert l.load_level(dest)
    js=[v for v in e.get_all_level_actors() if isinstance(v,U.HCM5VS2NPC) and '/AvatarSample_J/GarmentR2/' in v.get_class().get_path_name()];assert len(js)==1
    body=js[0].get_component_by_class(U.SkeletalMeshComponent)
    R['before']=dict(force_collision_update=bool(body.get_editor_property('force_collision_update')))
    body.set_editor_property('force_collision_update',True);assert body.get_editor_property('force_collision_update')
    assert l.save_current_level();file=P/'Content'/Path(dest.removeprefix('/Game/')).with_suffix('.umap')
    R.update(status='PASS',assets=old['assets'],protected_sources=old['protected_sources'],fixture=dict(map=dest,path=str(file),sha256=sha(file)),
        scope='Only fixture J mesh force_collision_update=true. Existing three cloth assets/masks/materials/original 20-body PHYS unchanged; authoring only, no runtime PASS.',
        runtime='NOT_RUN',visual='USER_REVIEW')
except Exception:R.update(status='FAIL',error=traceback.format_exc())
(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError(R['error'])
