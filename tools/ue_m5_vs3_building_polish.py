"""Facade/interior refinement inside existing guild/cafe footprints. No town expansion."""
from pathlib import Path
import unreal as U,math,json,re,traceback
D=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS3');m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2]);R={'status':'RUNNING','changes':[]}
L=U.get_editor_subsystem(U.LevelEditorSubsystem);E=U.get_editor_subsystem(U.EditorActorSubsystem);A=U.EditorAssetLibrary;AT=U.AssetToolsHelpers.get_asset_tools()
ROOT='/Game/HarborCity/M5VS3/Buildings';V='/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc'
FIT={}
def put(name,shape,pos,scale=(1,1,1),yaw=0,mat=None,collision=True):
    for prefix,(cx,cy,sx,sy) in FIT.items():
        if name.startswith(prefix):
            pos=(cx+(pos[0]-cx)*sx,cy+(pos[1]-cy)*sy,pos[2])
            if shape.get_path_name().startswith('/Engine/BasicShapes/Cube') or shape.get_path_name().startswith(ROOT+'/'):
                scale=(scale[0]*sx,scale[1]*sy,scale[2])
            break
    label='HC_VS3_'+name
    old=next((a for a in E.get_all_level_actors() if a.get_actor_label()==label),None)
    a=old or E.spawn_actor_from_class(U.StaticMeshActor,U.Vector(*pos),U.Rotator(yaw=yaw),False)
    a.set_actor_label(label);a.set_actor_location(U.Vector(*pos),False,False);a.set_actor_rotation(U.Rotator(yaw=yaw),False);a.set_actor_scale3d(U.Vector(*scale));c=a.static_mesh_component;c.set_static_mesh(shape);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    if mat:c.set_material(0,mat)
    return a
def load(s):
    a=A.load_asset(s);assert a,s;return a
def fit(name,shape,pos,height,yaw,collision=False):
    b=shape.get_bounding_box();s=height/(b.max.z-b.min.z);return put(name,shape,(pos[0],pos[1],pos[2]-b.min.z*s),(s,s,s),yaw,collision=collision)
def roof(name,x,y,mat):
    # Closed original gable around the existing furnished room, no borrowed building source mutation.
    verts=[(-980,-790,289),(980,-790,289),(980,790,289),(-980,790,289),(-980,0,610),(980,0,610)]
    triangles=[(0,1,5),(0,5,4),(4,5,2),(4,2,3),(0,4,3),(1,2,5),(0,3,2),(0,2,1)]
    p=O/(name+'.obj');p.write_text('\n'.join(['v %f %f %f'%(a,-b,c) for a,b,c in verts]+['vt %f %f'%(a/400,b/400) for a,b,c in verts]+['f '+' '.join('%d/%d'%(i+1,i+1) for i in reversed(t)) for t in triangles]),encoding='utf-8')
    path=ROOT+'/'+name;mesh=A.load_asset(path) if A.does_asset_exist(path) else None
    if not mesh:
        op=U.FbxImportUI();op.automated_import_should_detect_type=False;op.import_mesh=True;op.import_as_skeletal=False;op.mesh_type_to_import=U.FBXImportType.FBXIT_STATIC_MESH;op.original_import_type=U.FBXImportType.FBXIT_STATIC_MESH;op.import_materials=False;op.import_textures=False;op.import_animations=False
        op.static_mesh_import_data.combine_meshes=True;op.static_mesh_import_data.auto_generate_collision=True;op.static_mesh_import_data.convert_scene=False;op.static_mesh_import_data.convert_scene_unit=False
        t=U.AssetImportTask();t.filename=str(p);t.destination_path=ROOT;t.destination_name=name;t.automated=True;t.save=False;t.factory=U.FbxFactory();t.options=op;AT.import_asset_tasks([t]);mesh=load(path);mesh.set_material(0,mat);assert A.save_loaded_asset(mesh)
    put(name,mesh,(x,y,0))
try:
    sel=json.loads((D/'town_selection.json').read_text('utf-8-sig'));assert L.load_level(sel['map'])
    actors=E.get_all_level_actors();by={a.get_actor_label():a for a in actors};cube=load('/Engine/BasicShapes/Cube')
    def old(name):return by['HC_VS2_Town_'+name]
    wood=old('GuildHallCeiling').static_mesh_component.get_material(0);plaster=old('GuildHallWall0').static_mesh_component.get_material(0)
    roofmat=load(V+'/materials/MI_rooftiles_03')
    window=load(V+'/meshes/buildings/SM_BLD_window_v03_01');door=load(V+'/meshes/buildings/SM_BLD_door_v03_01')
    shelf=load(V+'/meshes/props/furniture/SM_PROP_market_shelf_01');table=load(V+'/meshes/props/furniture/SM_PROP_table_03');barrel=load(V+'/meshes/props/container/SM_PROP_barrel_01');chimney=load(V+'/meshes/buildings/SM_BLD_chimney_v01_01')
    for name,x,y in [('GuildHall',-4700,-4400),('HarborCafe',5800,-4400)]:
        center,ext=old(name+'Floor').get_actor_bounds(False);FIT[name]=(x,y,ext.x/900,ext.y/700)
        # Overlap by 10cm removes the wall/ceiling slit without moving the floor.
        for i in range(5):
            a=old(name+'Wall'+str(i));p=a.get_actor_location();a.set_actor_location(U.Vector(p.x,p.y,145.5),False,False);s=a.get_actor_scale3d();a.set_actor_scale3d(U.Vector(s.x,s.y,2.91))
        # Replace just the two front spans with collidable masonry surrounding actual apertures.
        for i in [3,4]:
            a=old(name+'Wall'+str(i));a.static_mesh_component.set_visibility(False);a.static_mesh_component.set_hidden_in_game(True);a.static_mesh_component.set_collision_enabled(U.CollisionEnabled.NO_COLLISION)
        for side in [-1,1]:
            center=side*575
            for j,(cx,sx) in enumerate([(center-222.5,205),(center+222.5,205)]):put(name+'FrontPier%d_%d'%(side,j),cube,(x+cx,y-700,145.5),(sx/100,.22,2.91),mat=plaster)
            put(name+'WindowSillWall'+str(side),cube,(x+center,y-700,47.5),(2.4,.22,.95),mat=plaster)
            put(name+'WindowTopWall'+str(side),cube,(x+center,y-700,263),(2.4,.22,.56),mat=plaster)
            fit(name+'FrontWindow'+str(side),window,(x+center,y-717,95),140,-90)
            fit(name+'DoorLeaf'+str(side),door,(x+side*222,y-770,0),255,-90+side*65)
        for j,px in enumerate([-925,-260,260,925]):put(name+'TimberPost'+str(j),cube,(x+px,y-727,145.5),(.17,.22,2.91),mat=wood)
        put(name+'EaveBeam',cube,(x,y-735,293),(19.3,.35,.25),mat=wood)
        for side in [-1,1]:
            put(name+'SideEave'+str(side),cube,(x+side*927,y,293),(.3,15.6,.25),mat=wood)
            for i,py in enumerate([-340,270]):fit(name+'SideWindow%d_%d'%(side,i),window,(x+side*920,y+py,105),140,0 if side>0 else 180)
        for a in actors:
            if name+'CompleteRoof' in a.get_actor_label() and isinstance(a,U.StaticMeshActor):
                a.static_mesh_component.set_visibility(False);a.static_mesh_component.set_hidden_in_game(True);a.static_mesh_component.set_collision_enabled(U.CollisionEnabled.NO_COLLISION)
        roof(name+'GableFitted',x,y,roofmat);fit(name+'Chimney',chimney,(x+570,y+200,470),180,0)
        # A sheltered entry: two columns and pitched canopy, path remains > 3m wide.
        for side in [-1,1]:put(name+'PorchColumn'+str(side),cube,(x+side*310,y-940,140),(.22,.22,2.8),mat=wood)
        put(name+'PorchRoof',cube,(x,y-900,285),(7,4,.25),mat=wood)
        fit(name+'ShelfA',shelf,(x-745,y+370,0),215,90,True)
        fit(name+'ShelfB',shelf,(x+745,y+370,0),215,-90,True)
        for i in range(3):fit(name+'SupplyBarrel'+str(i),barrel,(x+690,y-370+i*85,0),78,0,True)
        for i in range(2):fit(name+'SideTable'+str(i),table,(x-620+i*1240,y-210,0),80,0,True)
        # Ceiling inset and hanging timber fixture use existing material; lights are warm and bounded.
        put(name+'CeilingBorderFront',cube,(x,y-677,282),(17.7,.22,.3),mat=wood)
        put(name+'CeilingBorderBack',cube,(x,y+677,282),(17.7,.22,.3),mat=wood)
        put(name+'PendantStem',cube,(x,y,270),(.05,.05,.9),mat=wood,collision=False)
        put(name+'PendantCross',cube,(x,y,224),(2,.15,.12),mat=wood,collision=False)
        lamp=old(name+'WarmInterior');lamp.point_light_component.set_intensity(650);lamp.point_light_component.set_attenuation_radius(750)
        lamp.tags=[U.Name('HarborCity_VS3_InteriorLight')]
        R['changes'].append({'building':name,'footprint_unchanged':True,'facade':'Village windows/open door leaves, timber, gable, porch','interior':'shelves/tables/barrels/ceiling closure','visual':'NOT_RUN'})
    # Broader street pool, same lamp positions and geometry, no scene expansion.
    for a in E.get_all_level_actors():
        if isinstance(a,U.PointLight) and 'StreetLamp' in a.get_actor_label():
            a.point_light_component.set_attenuation_radius(1000)
            a.tags=[U.Name('HC_VS2_Rev2_NightLight'),U.Name('HC_VS2_Rev2_OnIntensity=900')]
    assert L.save_current_level();R.update(status='PASS',navigation='NEEDS_SEPARATE_REBUILD: newly imported mesh async lock retained within original commandlet')
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
