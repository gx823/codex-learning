"""Keep original colored filigree readable over bright daytime paving."""
from pathlib import Path
import re,json,traceback,unreal as U
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2]);r={'status':'RUNNING'}
try:
 for i in range(4):
  a=U.EditorAssetLibrary.load_asset('/Game/HarborCity/M5VS3/Combat/M_ArcaneFiligree'+str(i));assert a
  a.set_editor_property('blend_mode',U.BlendMode.BLEND_TRANSLUCENT)
  U.MaterialEditingLibrary.recompile_material(a);assert U.EditorAssetLibrary.save_loaded_asset(a)
 r.update(status='PASS',scope='Four eye-space ornament materials only; color compositing instead of additive washout')
except Exception:r.update(status='FAIL',error=traceback.format_exc());U.log_error(r['error'])
finally:(O/'author_result.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
