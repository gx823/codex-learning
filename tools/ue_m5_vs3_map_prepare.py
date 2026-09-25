"""Copy accepted town, correct existing NPC starts and bind PRIVATE local playlist."""
from pathlib import Path
import unreal as U,json,re,traceback,hashlib
R=Path('D:/科研学习/codex学习');D=R/'docs/HarborCity_M5_VS3'
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2])
E=U.get_editor_subsystem(U.EditorActorSubsystem);L=U.get_editor_subsystem(U.LevelEditorSubsystem)
report={'status':'RUNNING'}
try:
    old=json.loads((R/'docs/HarborCity_M5_VS2/part3/town_selection.json').read_text('utf-8-sig'))
    root='/Game/HarborCity/M5VS3/Town_'+hashlib.sha256(str(O).encode()).hexdigest()[:12];mp=root+'/L_HarborTown'
    assert not U.EditorAssetLibrary.does_asset_exist(mp)
    assert U.EditorAssetLibrary.duplicate_asset(old['map'],mp)
    assert L.load_level(mp)
    W=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    actors=E.get_all_level_actors()
    scene=next(a for a in actors if isinstance(a,U.HCM2SceneSettings))
    scene.set_editor_property('scene_id',U.Name('M5_VS3_HarborTown'));scene.set_editor_property('save_slot','HarborCity_M5_VS3_HarborTown')
    assert U.HCM3EditorNavigation.build_for_authoring(W)
    repair=json.loads(U.HCM5VS3Authoring.repair_standing_starts(W));report['standing_starts']=repair
    # Always retain the author report and copied map even if one start needs a follow-up.
    assert L.save_current_level()
    (D/'town_selection.json').write_text(json.dumps({'map':mp,'root':root,'source':old['map'],'status':repair['status']},indent=2),encoding='utf-8')
    if repair['status']!='PASS':raise RuntimeError('Some NPC starts still invalid; see bounded native report')
    music=next(a for a in actors if isinstance(a,U.HCM5StoryDirector)).get_editor_property('music')
    rows=json.loads((D/'private/music.json').read_text('utf-8-sig'));waves=[]
    at=U.AssetToolsHelpers.get_asset_tools()
    for row in rows:
        task=U.AssetImportTask();task.filename=row['normalized'];task.destination_path='/Game/HarborCity/Private/Music';task.destination_name=row['asset'].split('/')[-1];task.automated=True;task.save=True;task.replace_existing=False
        if not U.EditorAssetLibrary.does_asset_exist(row['asset']):at.import_asset_tasks([task])
        wave=U.load_asset(row['asset']);assert wave
        wave.set_editor_property('looping',False);U.EditorAssetLibrary.save_loaded_asset(wave);waves.append(wave)
    music.set_editor_property('private_playlist',waves);music.set_editor_property('use_private_playlist',True);music.set_editor_property('master_volume',.4)
    for prop in ('exploration_track','combat_track','interior_track'):music.set_editor_property(prop,None)
    assert L.save_current_level();report.update(status='PASS',map=mp)
except Exception:report.update(status='FAIL',error=traceback.format_exc());U.log_error(report['error'])
finally:(O/'author_result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
