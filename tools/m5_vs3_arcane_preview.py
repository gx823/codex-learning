"""Render original geometric linework for inspection; does not produce Unreal assets."""
import ast,math
from pathlib import Path
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[1];doc=root/'docs/HarborCity_M5_VS3'
tree=ast.parse((root/'tools/ue_m5_vs3_arcane.py').read_text(encoding='utf-8'))
ns={'math':math,'TAU':math.tau,'D':doc,'O':doc,'ROOT':'PREVIEW_ONLY','R':{'assets':[]}}
for n in tree.body:
 if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['Diagram','polar']:
  exec(compile(ast.Module(body=[n],type_ignores=[]),'original_geometry','exec'),ns)
ns['Diagram'].mesh=lambda *args:None
ns['make_material']=lambda *args:None
body=next(n.body for n in tree.body if isinstance(n,ast.Try))
# The try block's actual geometry and SVG serialization are pure calculations.
exec(compile(ast.Module(body=body,type_ignores=[]),'original_geometry','exec'),ns)
im=Image.new('RGB',(1200,330),'#09121e');d=ImageDraw.Draw(im)
for i,(name,layers) in enumerate(ns['all_paths']):
 color=['#79eaff','#ff98c8','#9cffcb','#ffe4a2'][i]
 for layer in layers:
  for a,b,w in layer.paths:
   d.line((150+i*300+a[0]*2.3,150-a[1]*2.3,150+i*300+b[0]*2.3,150-b[1]*2.3),fill=color,width=max(1,round(w*2.3)))
 d.text((i*300+100,298),name,fill='white')
im.save(doc/'arcane_design_contact.png')
print('Original geometry preview only; Unreal runtime validation remains separate.')
