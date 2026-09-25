"""Author original blade/runes and eight native Selestia pose sequences. No third-party downloads."""
from pathlib import Path
import unreal as U, math, json, re, traceback
D=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS3')
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2])
A=U.EditorAssetLibrary;AT=U.AssetToolsHelpers.get_asset_tools();ME=U.MaterialEditingLibrary
ROOT='/Game/HarborCity/M5VS3/Combat';R={'status':'RUNNING','assets':[]}

class Geometry:
    def __init__(self):self.v=[];self.f=[]
    def vertex(self,p):self.v.append(p);return len(self.v)
    def quad(self,ps,slot=0):
        a,b,c,d=[self.vertex(p) for p in ps];self.f += [(a,b,c,slot),(a,c,d,slot)]
    def tube(self,sections,slot=0,n=8):
        # rings along X, measured centimetres; tapered/diamond profiles, closed ends.
        ids=[]
        for x,y,z,ry,rz in sections:ids.append([self.vertex((x,y+ry*math.cos(i*math.tau/n),z+rz*math.sin(i*math.tau/n))) for i in range(n)])
        for a,b in zip(ids,ids[1:]):
            for i in range(n):j=(i+1)%n;self.f += [(a[i],b[j],b[i],slot),(a[i],a[j],b[j],slot)]
        for ring,reverse in [(ids[0],True),(ids[-1],False)]:
            for i in range(1,n-1):self.f.append((ring[0],ring[i+1] if reverse else ring[i],ring[i] if reverse else ring[i+1],slot))
    def line(self,a,b,w,slot=0):
        dy=b[0]-a[0];dz=b[1]-a[1];length=math.hypot(dy,dz);ny=-dz/length*w/2;nz=dy/length*w/2
        self.quad([(0,a[0]+ny,a[1]+nz),(0,b[0]+ny,b[1]+nz),(0,b[0]-ny,b[1]-nz),(0,a[0]-ny,a[1]-nz)],slot)
    def ring(self,r,w,n=96):
        for i in range(n):
            a=i*math.tau/n;b=(i+1)*math.tau/n
            self.quad([(0,math.cos(t)*rr,math.sin(t)*rr) for t,rr in [(a,r-w/2),(b,r-w/2),(b,r+w/2),(a,r+w/2)]])
    def write(self,name):
        p=O/(name+'.obj');out=['# HarborCity original geometry; centimetres; no extracted models']
        out += ['v %.6f %.6f %.6f'%(x,-y,z) for x,y,z in self.v]
        for slot in sorted({f[3] for f in self.f}):
            out += ['g material_'+str(slot),'usemtl Slot'+str(slot)]
            out += ['f %d %d %d'%tuple(reversed(f[:3])) for f in self.f if f[3]==slot]
        p.write_text('\n'.join(out),encoding='utf-8');return p

def material(name,color,metal=0,glow=False):
    old=A.load_asset(ROOT+'/'+name) if A.does_asset_exist(ROOT+'/'+name) else None
    if old:return old
    mat=AT.create_asset(name,ROOT,U.Material,U.MaterialFactoryNew())
    mat.set_editor_property('two_sided',glow)
    if glow:mat.set_editor_property('shading_model',U.MaterialShadingModel.MSM_UNLIT)
    v=ME.create_material_expression(mat,U.MaterialExpressionConstant3Vector);v.constant=U.LinearColor(*color,1)
    ME.connect_material_property(v,'',U.MaterialProperty.MP_EMISSIVE_COLOR if glow else U.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(U.MaterialProperty.MP_ROUGHNESS,.32),(U.MaterialProperty.MP_METALLIC,metal)]:
        n=ME.create_material_expression(mat,U.MaterialExpressionConstant);n.r=value;ME.connect_material_property(n,'',prop)
    ME.recompile_material(mat);assert A.save_loaded_asset(mat);return mat

def mesh(name,g,mats):
    p=g.write(name);path=ROOT+'/'+name
    if A.does_asset_exist(path):return A.load_asset(path)
    op=U.FbxImportUI();op.automated_import_should_detect_type=False;op.import_mesh=True;op.import_as_skeletal=False
    op.mesh_type_to_import=U.FBXImportType.FBXIT_STATIC_MESH;op.original_import_type=U.FBXImportType.FBXIT_STATIC_MESH
    op.import_materials=False;op.import_textures=False;op.import_animations=False
    data=op.static_mesh_import_data
    for k,v in {'combine_meshes':True,'auto_generate_collision':False,'generate_lightmap_u_vs':False,'build_nanite':False,'convert_scene':False,'convert_scene_unit':False,'force_front_x_axis':False,'transform_vertex_to_absolute':True}.items():data.set_editor_property(k,v)
    data.normal_import_method=U.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    t=U.AssetImportTask();t.filename=str(p);t.destination_path=ROOT;t.destination_name=name;t.automated=True;t.save=False;t.factory=U.FbxFactory();t.options=op
    AT.import_asset_tasks([t]);obj=A.load_asset(path);assert isinstance(obj,U.StaticMesh),name
    for i,mat in enumerate(mats):obj.set_material(i,mat)
    assert A.save_loaded_asset(obj);R['assets'].append({'path':path,'vertices':len(g.v),'triangles':len(g.f),'source':'original math-authored geometry'});return obj

try:
    silver=material('M_SwordSilver',(.6,.72,.85),.8);gold=material('M_SwordGold',(.8,.5,.13),.72);leather=material('M_SwordGrip',(.05,.06,.15),0);gem=material('M_SwordGem',(.15,.85,1.2),.1,True)
    colors=[(.2,1.0,2.0),(1.8,.38,.6),(.2,1.5,.72),(.9,.48,1.9)]
    magic=[material('M_Magic'+str(i),c,glow=True) for i,c in enumerate(colors)]
    g=Geometry();g.tube([(13,0,0,4.5,1.3),(74,0,0,3.5,1.05),(93,0,0,2,.75),(103,0,0,.02,.02)],0,4)
    g.tube([(-11,0,0,1.8,1.8),(0,0,0,1.5,1.5),(11,0,0,1.8,1.8)],2,12)
    g.tube([(-15,0,0,.6,.6),(-13,0,0,3.2,3.2),(-10,0,0,1.9,1.9)],1,8)
    for sign in [-1,1]:g.tube([(8,sign*4,0,4,1.5),(13,sign*9,0,4,1.2),(19,sign*14,0,.1,.1)],1,4)
    g.tube([(10,0,0,2,2),(14,0,0,3,2.5),(18,0,0,.1,.1)],3,6)
    mesh('SM_Sword',g,[silver,gold,leather,gem])
    for i,name in enumerate(['SM_RuneBolt','SM_RuneWave','SM_RuneHeal','SM_RuneShield']):
        g=Geometry();g.ring(29,1);g.ring(26,.35);g.ring(8,.6);n=[5,8,4,6][i]
        pts=[(math.cos(j*math.tau/n)*21,math.sin(j*math.tau/n)*21) for j in range(n)]
        for j in range(n):g.line(pts[j],pts[(j+(2 if i==0 else 1))%n],.6)
        for j in range(16+i*4):
            t=j*math.tau/(16+i*4);g.line((math.cos(t)*27,math.sin(t)*27),(math.cos(t)*30,math.sin(t)*30),.7)
        if i==2:
            g.line((-17,0),(17,0),2);g.line((0,-17),(0,17),2)
        mesh(name,g,[magic[i]])
    g=Geometry();g.tube([(-10,0,0,.02,.02),(0,0,0,4,4),(14,0,0,.02,.02)],0,8);mesh('SM_MagicBolt',g,[magic[0]])
    silk=material('M_WingSilk',(.74,.81,.94),.05)
    g=Geometry();sections=[]
    for i in range(41):
        t=i/40;w=max(.025,(math.sin(t*math.pi)**.7)*5.2)*(1-.32*t)
        sections.append((100*t,w*.13,4.5*math.sin(t*math.pi),w,max(.03,.72*math.sin(t*math.pi))))
    g.tube(sections,0,10);mesh('SM_SilkFeather',g,[silk])
    # Magic gun proportions retain the existing 17.5 cm muzzle reference.
    g=Geometry();g.tube([(-8,0,4,2.5,2.5),(12,0,4,3,3),(17.5,0,4.45,1.8,1.8)],0,8)
    g.tube([(-9,0,-5,2,3),(-4,0,-3,2,4),(0,0,2,2.2,3.4)],2,6)
    for y in [-3,3]:g.tube([(-5,y,4,1,1),(5,y,4,1,1),(13,y,4,.2,.2)],1,6)
    g.tube([(3,0,7,1,1),(7,0,7,2,2),(12,0,7,.1,.1)],3,6);mesh('SM_MagicGun',g,[silver,gold,leather,gem])
    src=A.load_asset('/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS');assert src
    names=['A_Sword1','A_Sword2','A_Sword3','A_SwordHeavy','A_SwordCharge','A_SwordDraw','A_SwordSheath','A_Cast']
    targets=[A.load_asset(ROOT+'/'+n) if A.does_asset_exist(ROOT+'/'+n) else AT.duplicate_asset(n,ROOT,src) for n in names]
    R['animations']=json.loads(U.HCM5VS3Authoring.author_combat_poses(src,targets));assert R['animations']['status']=='PASS'
    for a in targets:assert A.save_loaded_asset(a)
    R['status']='PASS'
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
