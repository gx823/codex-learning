"""Build saved town navigation through UE's public synchronous build on reload."""
from pathlib import Path
import json,re,traceback
import unreal as U
D=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2/part3')
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2])
R={'status':'RUNNING','phase':'Part3NavReload'}
try:
    s=json.loads((D/'town_selection.json').read_text('utf-8-sig'));R['map']=s['map']
    L=U.get_editor_subsystem(U.LevelEditorSubsystem);assert L.load_level(s['map'])
    W=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    R['native_build']=U.HCM3EditorNavigation.build_for_authoring(W)
    if not R['native_build']:raise RuntimeError('Native build did not finish; retained saved town; no lock override')
    assert L.save_current_level()
    R['nav_data']=[a.get_class().get_name() for a in U.get_editor_subsystem(U.EditorActorSubsystem).get_all_level_actors() if isinstance(a,U.RecastNavMesh)]
    assert len(R['nav_data'])==2
    R['status']='PASS'
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
