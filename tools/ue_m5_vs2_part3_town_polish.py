"""Bounded visual corrections after the first real town contact sheet failed review."""
from pathlib import Path
import json,math,re,traceback,shutil,hashlib
import unreal as U
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2/part3'
helper=W/'tools/ue_m5_vs2_part3_town_author.py'
exec(compile(helper.read_text('utf-8').split('\ntry:\n')[0],str(helper),'exec'),globals())
D=W/'docs/HarborCity_M5_VS2/part3'
s=json.loads((D/'town_selection.json').read_text('utf-8-sig'));ROOT=s['root']+'/Polish_'+hashlib.sha256(str(O).encode()).hexdigest()[:8];MAP=s['map']
R={'status':'RUNNING','map':MAP,'changes':[],'art':'USER_REVIEW'}
R['checks']=[];R['assets']=[]
try:
    p=W/'HarborCity/Content'/Path(MAP[6:]).with_suffix('.umap');shutil.copy2(p,O/'before_polish.umap')
    check('load selected candidate',L.load_level(MAP));actors=list(E.get_all_level_actors())
    by={a.get_actor_label().removeprefix('HC_M5VS2_CornerV2_'):a for a in actors}
    terrain=next(a for a in actors if a.get_actor_label()=='HC_VS2_Town_MainlandCoast')
    # A native lit textured meadow; no custom HLSL/render pass and no image backdrop.
    mat=AT.create_asset('M_MainlandMeadow',ROOT,U.Material,U.MaterialFactoryNew());ML=U.MaterialEditingLibrary
    tex=ML.create_material_expression(mat,U.MaterialExpressionTextureSample,0,0)
    tex.set_editor_property('texture',load('/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc/textures/T_ENV_TERRAIN_gravel_BC'))
    tint=ML.create_material_expression(mat,U.MaterialExpressionConstant3Vector,0,200);tint.set_editor_property('constant',U.LinearColor(.19,.40,.085,1))
    mul=ML.create_material_expression(mat,U.MaterialExpressionMultiply,220,0)
    ML.connect_material_expressions(tex,'RGB',mul,'A');ML.connect_material_expressions(tint,'',mul,'B');ML.connect_material_property(mul,'',U.MaterialProperty.MP_BASE_COLOR)
    rough=ML.create_material_expression(mat,U.MaterialExpressionConstant,220,200);rough.set_editor_property('r',.95);ML.connect_material_property(rough,'',U.MaterialProperty.MP_ROUGHNESS);ML.recompile_material(mat);save(mat)
    verts=[];faces=[];nx=90;ny=100
    for iy in range(ny+1):
        y=-45000+iy*900;shore=12500+1700*math.sin(y/6800)+900*math.cos(y/4100)
        for ix in range(nx+1):
            x=-60000+(shore+60000)*ix/nx
            core=abs(y)<13500 and x>-12000
            z=zland(x,y) if core else max(-70,400+280*math.sin(x/6500)*math.cos(y/7300))
            if x<-12000:z+=min(1,(-x-12000)/15000)*(1600+1000*math.sin(y/6300)**2)
            if ix==nx:z=-120
            verts.append((x,y,z-3))
            if ix and iy:
                k=iy*(nx+1)+ix;faces.extend([(k-nx-2,k-nx-1,k),(k-nx-2,k,k-1)])
    geometry('GreenMainland',verts,faces,mat);E.destroy_actor(terrain)
    # Set explicit photometric units. Interiors stay lit at all three times.
    for a in actors:
        if not isinstance(a,U.PointLight) or not a.get_actor_label().startswith('HC_VS2_Town_'):continue
        c=a.point_light_component;c.set_editor_property('intensity_units',U.LightUnits.LUMENS)
        if 'WarmInterior' in a.get_actor_label():
            a.tags=[U.Name('HarborCity_VS2_Part3')];c.set_intensity(240);c.set_attenuation_radius(650);c.set_light_color(U.LinearColor(1,.78,.53,1),False)
        else:c.set_intensity(350);a.tags=[U.Name('HarborCity_VS2_Part3'),U.Name('HC_VS2_Rev2_NightLight'),U.Name('HC_VS2_Rev2_OnIntensity=350')]
    # Fit halls to human scale, preserving their open front door and quest NPCs.
    for name,cx,cy in [('GuildHall',-4700,-4400),('HarborCafe',5800,-4400)]:
        for a in actors:
            if not a.get_actor_label().startswith('HC_VS2_Town_'+name):continue
            p=a.get_actor_location();a.set_actor_location(U.Vector(cx+(p.x-cx)*.5,cy+(p.y-cy)*.5,p.z*.9),False,True)
            if isinstance(a,U.StaticMeshActor):a.set_actor_scale3d(a.get_actor_scale3d()*U.Vector(.5,.5,.9))
        roof_mat=load('/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc/materials/MI_rooftiles_03')
        geometry(name+'CompleteRoof',[(cx-520,cy-430,310),(cx+520,cy-430,310),(cx+520,cy+430,310),(cx-520,cy+430,310),(cx-520,cy,610),(cx+520,cy,610)],[(0,1,5,4),(4,5,2,3),(0,4,3),(1,2,5),(0,3,2,1)],roof_mat)
        table=load('/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc/meshes/props/furniture/SM_PROP_table_03')
        for i in range(3):put(name+'Table'+str(i),table,(cx-240+(i%2)*470,cy-120+(i//2)*320,10),(.7,.7,.7))
    groups={n:[a for a in actors if a.get_actor_label().startswith('HC_M5VS2_CornerV2_'+n+'_')] for n in ['EastInn','EastShop','EastProvisioner','HarborShop']}
    def house(n,g,x,y,yaw):
        parts=groups[g];body=next((a for a in parts if '_body_' in a.get_actor_label()),parts[0]);anchor=body.get_actor_location();anchor.z=0;q=U.Rotator(yaw=yaw).quaternion()
        for i,a in enumerate(parts):
            c=a.static_mesh_component;p=U.Vector(x,y,max(0,zland(x,y)))+q.rotate_vector(a.get_actor_location()-anchor);scale=a.get_actor_scale3d()
            out=put(n+'_'+str(i),c.static_mesh,(p.x,p.y,p.z),(scale.x,scale.y,scale.z),yaw+a.get_actor_rotation().yaw)
            for k in range(c.get_num_materials()):out.static_mesh_component.set_material(k,c.get_material(k))
    # Fill alternating street frontages, leave actual road lanes, doors and plazas clear.
    sites=[]
    for x,yaw in [(-850,0),(850,180)]:
        for y in [-10800,-8750,-6350,4700,8100,10500]:sites.append((x,y,yaw))
    for y,yaw in [(-7150,90),(-5800,-90),(4250,90),(5750,-90),(9800,90),(11200,-90)]:
        for x in [-7050,-5650,-2900,2100,5450,6800]:
            if 3000<x<4200:continue
            sites.append((x,y,yaw))
    for i,(x,y,yaw) in enumerate(sites):house('Infill%02d'%i,list(groups)[i%4],x,y,yaw)
    trees=[a for a in actors if a.get_actor_label().startswith('HC_M5VS2_CornerV2_CoastalTree')]
    flowers=[a for a in actors if a.get_actor_label().startswith('HC_M5VS2_CornerV2_') and 'Flowers' in a.get_actor_label() and isinstance(a,U.StaticMeshActor)]
    for i in range(150):
        old=trees[i%len(trees)];x=-11300+(i%15)*1500;y=-11300+(i//15)*2500
        if abs(x)<650 or abs(y+6500)<650 or abs(y-5000)<650 or abs(y-10500)<600 or 2900<x<4500:continue
        c=old.static_mesh_component;sc=.55+(i%4)*.1;out=put('Grove%03d'%i,c.static_mesh,(x,y,max(0,zland(x,y))),(sc,sc,sc),i*137)
        for k in range(c.get_num_materials()):out.static_mesh_component.set_material(k,c.get_material(k))
    for i,(x,y,yaw) in enumerate(sites):
        old=flowers[i%len(flowers)];c=old.static_mesh_component;out=put('StreetFlowers%02d'%i,c.static_mesh,(x+360,y-310,max(0,zland(x,y))),(.6,.6,.6),yaw)
        for k in range(c.get_num_materials()):out.static_mesh_component.set_material(k,c.get_material(k))
    # Preserve authored geometry and real river water; align the bridge long axis across the canal.
    bridge=next(a for a in actors if a.get_actor_label()=='HC_VS2_Town_CanalBridge');bridge.set_actor_rotation(U.Rotator(yaw=0),True)
    runtime=next(a for a in actors if isinstance(a,U.HCM5VS2TownRuntime))
    prior=json.loads(Path(s['author_result']).read_text('utf-8-sig'));roads=[(tuple(x['a']),tuple(x['b'])) for x in prior['roads']]
    navigation_map(roads,prior['landmarks'])
    check('save actual corrections',L.save_current_level())
    R.update(status='PASS',actor_count=len(E.get_all_level_actors()),added_house_groups=len(sites),navigation='NEEDS_REBUILD_AFTER_GEOMETRY',changes=['mainland textured green and raised distant terrain','infill street fronts','native tree and flower props','explicit lumen lamps; interiors remain lit','human-scale halls with pitched Village roofs','bridge rotation corrected'])
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
