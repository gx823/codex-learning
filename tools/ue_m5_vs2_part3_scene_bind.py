"""Bind existing scene/save interfaces on the new private town, remove duplicate recorder."""
from pathlib import Path
import json,re,traceback,shutil
import unreal as U
P=Path('D:/科研学习/codex学习/HarborCity');D=P.parent/'docs/HarborCity_M5_VS2/part3'
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2]);R={'status':'RUNNING'}
try:
    s=json.loads((D/'town_selection.json').read_text('utf-8-sig'));p=P/'Content'/Path(s['map'][6:]).with_suffix('.umap');shutil.copy2(p,O/'before_scene_bind.umap')
    L=U.get_editor_subsystem(U.LevelEditorSubsystem);assert L.load_level(s['map'])
    E=U.get_editor_subsystem(U.EditorActorSubsystem);actors=E.get_all_level_actors()
    for a in actors:
        if isinstance(a,U.HCM3Recording):E.destroy_actor(a)
    assert not any(isinstance(a,U.HCM2SceneSettings) for a in actors)
    scene=E.spawn_actor_from_class(U.HCM2SceneSettings,U.Vector(0,0,-600));scene.set_actor_label('HC_VS2_Town_SceneSettings')
    vals={'scene_id':U.Name('M5_VS2_HarborTown'),'map_name':U.Name('L_HarborTown'),'save_slot':'HarborCity_M5_VS2_HarborTown','player_id':U.Name('M5_Player'),'external_time_of_day_controller':True,
          'player_start':next(a for a in actors if isinstance(a,U.PlayerStart)),'main_vehicle':next(a for a in actors if isinstance(a,U.HCM1Vehicle)),
          'interior_switch':next(a for a in actors if isinstance(a,U.HCM1LightSwitch))}
    for k,v in vals.items():scene.set_editor_property(k,v)
    assert L.save_current_level();R.update(status='PASS',map=s['map'],save_slot=vals['save_slot'],recorder='Experience spawns one when requested',time_owner='VS2 CornerTimeDirector')
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
