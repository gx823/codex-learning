"""Bind two already-retargeted official GAS idle gestures in the private town."""
from pathlib import Path
import json,re,traceback,shutil
import unreal as U
P=Path('D:/科研学习/codex学习/HarborCity');D=P.parent/'docs/HarborCity_M5_VS2/part3'
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2]);R={'status':'RUNNING'}
try:
    s=json.loads((D/'town_selection.json').read_text('utf-8-sig'));L=U.get_editor_subsystem(U.LevelEditorSubsystem);assert L.load_level(s['map'])
    runtime=next(a for a in U.get_editor_subsystem(U.EditorActorSubsystem).get_all_level_actors() if isinstance(a,U.HCM5VS2TownRuntime))
    clips=[U.EditorAssetLibrary.load_asset('/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/M_Relaxed_Stand_Idle_Break_v%02d_InPlace_SelestiaGAS'%i) for i in [1,2]]
    assert all(clips);shutil.copy2(P/'Content'/Path(s['map'][6:]).with_suffix('.umap'),O/'before_idle.umap')
    runtime.set_editor_property('idle_gestures',clips);assert L.save_current_level()
    R.update(status='PASS',map=s['map'],clips=[c.get_path_name() for c in clips],source='Existing authorized Game Animation Sample retargets; no new download',visual='NOT_RUN')
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
