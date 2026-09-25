"""Read native reference pose and Kawaii schema. Never writes Unreal assets."""
from pathlib import Path
import json,re,traceback
import unreal as U
matches=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
assert len(matches)==1
out=Path(matches[0][0] or matches[0][1]).resolve()
assert out.is_relative_to(Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2').resolve())
R={'status':'RUNNING','asset_writes':0,'visual':'NOT_RUN'}
try:
    mesh=U.load_asset('/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia')
    bp=U.load_asset('/Game/HarborCity/M5VS1/HeroSelestia/Animation/ABP_M4R2_Player_Selestia')
    assert mesh and bp
    source=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2/research/SELESTIA_SOURCE_AUDIT.json').read_text(encoding='utf-8')
    result=U.HCM5VS2PhysicsEditor.inspect_physics_setup(mesh,bp,source,'Assets/SELESTIA/Prefab/SELESTIA_lilToon.prefab')
    payload=json.loads(result)
    (out/'physics_probe.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf-8')
    R['probe_file']=str(out/'physics_probe.json')
    R['probe_status']=payload.get('status','UNKNOWN')
    assert payload.get('status')=='PROBE_COMPLETE_NOT_PHYSICS_ACCEPTANCE',payload.get('error',payload.get('status'))
    assert payload.get('package_dirty_flags_unchanged') is True
    R['status']='PASS'
    R['pass_scope']='Read-only native probe completed; no physics authoring or runtime acceptance.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    (out/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('Physics probe failed; see report')
