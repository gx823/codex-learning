"""Bind original animation-first nonlethal falls only to the VS3 map's existing adult NPCs."""
from pathlib import Path
import unreal as U,json,re,traceback
D=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS3');m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2]);R={'status':'RUNNING','npcs':[]}
A=U.EditorAssetLibrary;AT=U.AssetToolsHelpers.get_asset_tools();L=U.get_editor_subsystem(U.LevelEditorSubsystem);E=U.get_editor_subsystem(U.EditorActorSubsystem)
try:
    assert L.load_level(json.loads((D/'town_selection.json').read_text('utf-8-sig'))['map'])
    authored=set()
    for npc in E.get_all_level_actors():
        if not isinstance(npc,U.HCM3NPC):continue
        c=npc.get_physical_reaction();clips=c.get_editor_property('get_up_clips');assert len(clips)==4
        assert c.get_editor_property('get_up_pose_data_verified')
        targets=[]
        for clip in clips:
            ref=clip.get_editor_property('animation');source=ref if isinstance(ref,U.AnimSequence) else U.load_asset(str(ref))
            assert source,'Get-up source not resolved'
            root='/Game/HarborCity/M5VS3/NPC/'+source.get_skeleton().get_name();name='A_Fall_'+source.get_name()
            target=A.load_asset(root+'/'+name) if A.does_asset_exist(root+'/'+name) else AT.duplicate_asset(name,root,source)
            if target.get_path_name() not in authored:
                assert U.HCM5VS3Authoring.author_fall_pose(source,target);assert A.save_loaded_asset(target);authored.add(target.get_path_name())
            targets.append(target)
        c.set_editor_property('animated_fall_clips',targets);c.set_editor_property('animation_primary_knockdown',True)
        R['npcs'].append({'id':str(npc.get_editor_property('stable_id')),'fall_assets':[x.get_path_name() for x in targets],'visual_status':'NOT_RUN'})
    assert L.save_current_level();R['status']='PASS'
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
