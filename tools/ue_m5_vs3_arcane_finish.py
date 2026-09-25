"""Keep eye-space casting filigree legible beside the crosshair near walls."""
from pathlib import Path
import json,re,traceback,unreal as U
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line()); O=Path(m[1] or m[2])
r={'status':'RUNNING','scope':'Casting ornaments only; world/projectile collision unchanged','materials':[]}
try:
 for i in range(4):
  a=U.EditorAssetLibrary.load_asset('/Game/HarborCity/M5VS3/Combat/M_ArcaneFiligree'+str(i)); assert a
  a.set_editor_property('disable_depth_test',True)
  U.MaterialEditingLibrary.recompile_material(a); assert U.EditorAssetLibrary.save_loaded_asset(a)
  r['materials'].append(a.get_path_name())
 # Original feather surface keeps normal lighting and a restrained pearly glow.
 wing=U.EditorAssetLibrary.load_asset('/Game/HarborCity/M5VS3/Combat/M_WingSilk');assert wing
 me=U.MaterialEditingLibrary
 c=me.create_material_expression(wing,U.MaterialExpressionConstant3Vector);c.constant=U.LinearColor(.07,.10,.16,1)
 g=me.create_material_expression(wing,U.MaterialExpressionScalarParameter);g.set_editor_property('parameter_name','GlowGain');g.set_editor_property('default_value',2.3)
 mul=me.create_material_expression(wing,U.MaterialExpressionMultiply);me.connect_material_expressions(c,'',mul,'A');me.connect_material_expressions(g,'',mul,'B');me.connect_material_property(mul,'',U.MaterialProperty.MP_EMISSIVE_COLOR)
 me.recompile_material(wing);assert U.EditorAssetLibrary.save_loaded_asset(wing)
 r['status']='PASS'
except Exception:r.update(status='FAIL',error=traceback.format_exc());U.log_error(r['error'])
finally:(O/'author_result.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
