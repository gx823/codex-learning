"""Original layered arcane linework; anime references are observation only, no copied imagery."""
from pathlib import Path
import math,json,re,traceback,unreal as U
D=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS3')
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2])
ROOT='/Game/HarborCity/M5VS3/Combat';A=U.EditorAssetLibrary;AT=U.AssetToolsHelpers.get_asset_tools();ME=U.MaterialEditingLibrary
R={'status':'RUNNING','source':'Original polar/Bezier linework; no reference image imported','assets':[]}
TAU=math.tau
def polar(r,a):return (r*math.cos(a),r*math.sin(a))
class Diagram:
 def __init__(self):self.v=[];self.f=[];self.paths=[]
 def line(self,a,b,w=.24):
  dx=b[0]-a[0];dy=b[1]-a[1];length=math.hypot(dx,dy)
  if length<1e-6:return
  nx=-dy/length*w*.5;ny=dx/length*w*.5;k=len(self.v)
  self.v.extend([(a[0]+nx,a[1]+ny),(b[0]+nx,b[1]+ny),(b[0]-nx,b[1]-ny),(a[0]-nx,a[1]-ny)])
  self.f.extend([(k,k+1,k+2),(k,k+2,k+3)]);self.paths.append((a,b,w))
 def path(self,pts,w=.24,closed=False):
  for a,b in zip(pts,pts[1:]+([pts[0]] if closed else [])):self.line(a,b,w)
 def arc(self,r,start=0,end=TAU,w=.24,center=(0,0)):
  n=max(8,int(abs(end-start)*r*1.4));self.path([(center[0]+r*math.cos(t),center[1]+r*math.sin(t)) for t in [start+(end-start)*i/n for i in range(n+1)]],w)
 def bezier(self,p0,p1,p2,p3,w=.25):
  self.path([tuple((1-t)**3*p0[j]+3*(1-t)**2*t*p1[j]+3*(1-t)*t*t*p2[j]+t**3*p3[j] for j in [0,1]) for t in [i/32 for i in range(33)]],w)
 def rotated(self,pts,a,center=(0,0)):
  return [(center[0]+x*math.cos(a)-y*math.sin(a),center[1]+x*math.sin(a)+y*math.cos(a)) for x,y in pts]
 def glyph(self,a,r,k,small=False):
  # Project-original alphabet: variable stems, crescents, diamonds and crossbars.
  s=.62 if small else 1.;origin=polar(r,a)
  def p(x,y):return self.rotated([(x*s,y*s)],a-math.pi/2,origin)[0]
  self.path([p(-.75,-1.45),p(.15,-.55),p(-.15,.55),p(.65,1.5)],.22*s)
  if k%3==0:self.path([p(-.8,-.3),p(.1,.15),p(.9,-.45)],.20*s)
  elif k%3==1:self.bezier(p(-.7,-1),p(-1.7,.8),p(1.7,.8),p(.6,-.3),.20*s)
  else:self.path([p(0,.4),p(.9,.9),p(0,1.6),p(-.8,.9)],.20*s,True)
  if k%2:self.line(p(-.75,-1.2),p(.8,-1.2),.18*s)
 def mesh(self,name,material):
  path=O/(name+'.obj');path.write_text('\n'.join(['v 0 %.7f %.7f'%(-x,y) for x,y in self.v]+['f '+' '.join(str(i+1) for i in reversed(f)) for f in self.f]),encoding='utf-8')
  asset=ROOT+'/'+name
  if not A.does_asset_exist(asset):
   op=U.FbxImportUI();op.automated_import_should_detect_type=False;op.import_mesh=True;op.import_as_skeletal=False;op.mesh_type_to_import=U.FBXImportType.FBXIT_STATIC_MESH;op.original_import_type=U.FBXImportType.FBXIT_STATIC_MESH;op.import_materials=False;op.import_textures=False;op.import_animations=False
   op.static_mesh_import_data.combine_meshes=True;op.static_mesh_import_data.auto_generate_collision=False;op.static_mesh_import_data.convert_scene=False;op.static_mesh_import_data.convert_scene_unit=False
   t=U.AssetImportTask();t.filename=str(path);t.destination_path=ROOT;t.destination_name=name;t.automated=True;t.save=False;t.factory=U.FbxFactory();t.options=op;AT.import_asset_tasks([t])
  obj=A.load_asset(asset);assert obj;obj.set_material(0,material);assert A.save_loaded_asset(obj);R['assets'].append({'asset':asset,'triangles':len(self.f)})
def make_material(kind,color):
 name='M_ArcaneFiligree'+str(kind)
 if A.does_asset_exist(ROOT+'/'+name):return A.load_asset(ROOT+'/'+name)
 mat=AT.create_asset(name,ROOT,U.Material,U.MaterialFactoryNew());mat.set_editor_property('two_sided',True);mat.set_editor_property('blend_mode',U.BlendMode.BLEND_ADDITIVE);mat.set_editor_property('shading_model',U.MaterialShadingModel.MSM_UNLIT)
 c=ME.create_material_expression(mat,U.MaterialExpressionConstant3Vector);c.constant=U.LinearColor(*color,1)
 gain=ME.create_material_expression(mat,U.MaterialExpressionScalarParameter);gain.set_editor_property('parameter_name','Glow');gain.set_editor_property('default_value',1.8)
 mul=ME.create_material_expression(mat,U.MaterialExpressionMultiply);ME.connect_material_expressions(c,'',mul,'A');ME.connect_material_expressions(gain,'',mul,'B');ME.connect_material_property(mul,'',U.MaterialProperty.MP_EMISSIVE_COLOR)
 alpha=ME.create_material_expression(mat,U.MaterialExpressionScalarParameter);alpha.set_editor_property('parameter_name','Reveal');alpha.set_editor_property('default_value',1);ME.connect_material_property(alpha,'',U.MaterialProperty.MP_OPACITY)
 ME.recompile_material(mat);assert A.save_loaded_asset(mat);return mat
try:
 palettes=[(.12,.9,1.6),(1.35,.25,.7),(.27,1.3,.68),(1.55,1.06,.32)];all_paths=[]
 for kind,name in enumerate(['Bolt','Wave','Heal','Shield']):
  mat=make_material(kind,palettes[kind]);layers=[Diagram() for _ in range(3)];outer,inner,core=layers
  for r,w in [(51,.45),(50.1,.15),(43,.25),(42.1,.18)]:outer.arc(r,w=w)
  for j in range(64):
   a=j*TAU/64;outer.glyph(a,46.5,j+kind*5)
   outer.line(polar(51.5,a),polar(53.2 if j%8==0 else 52.1,a),.28 if j%8==0 else .16)
  for j in range(8):
   a=(j+.5)*TAU/8;outer.path([polar(42.1,a-.045),polar(39.9,a),polar(42.1,a+.045)],.35)
  for j in range(8):
   a=j*TAU/8;outer.path([polar(50,a-.024),polar(55.5,a),polar(50,a+.024),polar(48.5,a)],.38,True)
   for side in [-1,1]:
    p=outer.rotated([(side*.5,52),(side*4,55),(side*3,58),(0,56.8)],a-math.pi/2);outer.bezier(*p,w=.28)
  n=[8,6,6,8][kind]
  for r,w in [(37,.34),(35.9,.14),(28,.25),(26.9,.16)]:inner.arc(r,w=w)
  for j in range(n):
   a=j*TAU/n+kind*.09;center=polar(32,a);inner.arc(3.9,w=.26,center=center);inner.arc(3.35,w=.13,center=center)
   inner.glyph(a,32,j*3+kind,True)
   # Interlaced scrolls: broad curved gestures with tiny inset strokes.
   p=inner.rotated([(10,28),(27,20),(31,8),(24,3)],a);inner.bezier(*p,w=.45)
   p=inner.rotated([(10.5,27),(25,19),(28,9),(23,4)],a);inner.bezier(*p,w=.16)
  # A second fine script band keeps the design legible between the major motifs.
  for j in range(40):inner.glyph(j*TAU/40,39.25,j*7+kind,True)
  for j in range(72):
   a=j*TAU/72;inner.line(polar(23.5,a),polar(25.8 if j%6==0 else 24.5,a),.21 if j%6==0 else .13)
  core.arc(22,w=.3);core.arc(20.9,w=.14)
  if kind==0:
   for off in [0,math.pi/4]:core.path([polar(19,j*math.pi/2+off) for j in range(4)],.42,True)
   for j in range(8):
    a=j*TAU/8;core.path([polar(7,a-.25),polar(17,a),polar(7,a+.25),polar(4,a)],.30,True)
   core.arc(4,w=.48);core.arc(2,w=.18)
  elif kind==1:
   for j in range(6):
    a=j*TAU/6;core.path([polar(4,a-.4),polar(18,a-.13),polar(20,a),polar(18,a+.13),polar(4,a+.4)],.45)
    core.arc(13,a+.13,a+.68,w=.2)
   core.path([polar(8,j*TAU/6) for j in range(6)],.4,True);core.arc(2.7,w=.4)
  elif kind==2:
   for j in range(6):
    a=j*TAU/6;core.bezier(*core.rotated([(0,3),(-11,13),(-4,22),(0,19)],a),w=.42);core.bezier(*core.rotated([(0,19),(4,22),(11,13),(0,3)],a),w=.42)
    core.path(core.rotated([(0,4),(0,16)],a),.18)
    for h in [7,10,13]:
     for side in [-1,1]:core.bezier(*core.rotated([(0,h-1),(side*3,h),(side*4,h+3),(0,h+3)],a),w=.18)
   core.arc(4,w=.38);core.path([polar(2.6,j*TAU/6) for j in range(6)],.18,True)
  else:
   for off in [0,math.pi/3]:core.path([polar(19,j*TAU/3+off) for j in range(3)],.32,True)
   core.path([(-9,9),(0,13),(9,9),(8,-1),(0,-13),(-8,-1)],.55,True)
   core.path([(-6,6),(0,9),(6,6),(5,-1),(0,-8),(-5,-1)],.18,True)
   core.path([(0,6),(3,1),(0,-4),(-3,1)],.35,True)
  # Fine inner orbit and alternating knot jewels add close-up structure while
  # leaving the central spell emblem readable from gameplay distance.
  for j in range(12):
   a=j*TAU/12;core.arc(.7,w=.18,center=polar(20,a))
  for j in range(4):
   a=(j+.5)*TAU/4;core.path([polar(23.2,a),polar(21.8,a+.045),polar(20.4,a),polar(21.8,a-.045)],.24,True)
  for layer,diagram in zip(['Outer','Inner','Core'],layers):diagram.mesh('SM_Arcane'+name+'_'+layer,mat)
  all_paths.append((name,layers))
 # Compact vector proof from the very same generated linework; runtime PNGs remain separate evidence.
 svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1200 340"><rect width="1200" height="340" fill="#09121e"/>']
 for i,(name,layers) in enumerate(all_paths):
  color=['#79eaff','#ff98c8','#9cffcb','#ffe4a2'][i];cx=150+i*300
  svg.append(f'<g transform="translate({cx} 155) scale(2.3 -2.3)" fill="none" stroke="{color}">')
  for layer in layers:
   for a,b,w in layer.paths:svg.append(f'<path d="M {a[0]:.3f} {a[1]:.3f} L {b[0]:.3f} {b[1]:.3f}" stroke-width="{w:.3f}"/>')
  svg.append(f'</g><text x="{cx}" y="315" fill="white" font-size="18" text-anchor="middle">{name} / ORIGINAL VECTOR PROOF</text>')
 svg.append('</svg>');(D/'ARCANE_LINEWORK.svg').write_text(''.join(svg),encoding='utf-8')
 R['status']='PASS'
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'arcane_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8');(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
