"""New, engine-saved harbor candidate. The accepted corner and all source assets stay read-only."""
from pathlib import Path
import json, re, hashlib, math, traceback, struct, zlib
import unreal as U
W=Path('D:/科研学习/codex学习'); D=W/'docs/HarborCity_M5_VS2'; P=W/'HarborCity'
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line()); O=Path(m[1] or m[2])
token=hashlib.sha256(str(O).encode()).hexdigest()[:12]
ROOT='/Game/HarborCity/M5VS2/Part3/Town_'+token; MAP=ROOT+'/L_HarborTown'
A=U.EditorAssetLibrary; B=U.BlueprintEditorLibrary; AT=U.AssetToolsHelpers.get_asset_tools()
L=U.get_editor_subsystem(U.LevelEditorSubsystem); E=U.get_editor_subsystem(U.EditorActorSubsystem)
R={'status':'RUNNING','phase':'Part3TownAuthor','map':MAP,'root':ROOT,'checks':[],'runtime':'NOT_RUN','art':'USER_REVIEW','excluded_source_npcs':['J','T','U','W'],'assets':[]}
def check(n,b):
    R['checks'].append({'name':n,'status':'PASS' if b else 'FAIL'})
    if not b: raise RuntimeError(n)
def load(p):
    a=A.load_asset(p);check('load '+p,a is not None);return a
def save(o):
    check('save '+o.get_path_name(),A.save_loaded_asset(o,False));R['assets'].append(o.get_path_name())
def spawn(n,c,pos=(0,0,0),yaw=0):
    a=E.spawn_actor_from_class(c,U.Vector(*pos),U.Rotator(yaw=yaw),False);check('spawn '+n,a is not None)
    a.set_actor_label('HC_VS2_Town_'+n);a.tags=[U.Name('HarborCity_VS2_Part3')];return a
def put(n,mesh,pos,scale=(1,1,1),yaw=0,mat=None,collision=True):
    a=spawn(n,U.StaticMeshActor,pos,yaw);c=a.static_mesh_component;c.set_mobility(U.ComponentMobility.STATIC);c.set_static_mesh(mesh)
    if mat:c.set_material(0,mat)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');a.set_actor_scale3d(U.Vector(*scale));return a
def geometry(n,verts,faces,mat):
    source=O/(n+'.obj');lines=['# Original HarborCity authored geometry, centimeters']
    lines += ['v %.6f %.6f %.6f'%(x,-y,z) for x,y,z in verts]
    lines += ['vt %.6f %.6f'%(x/500,y/500) for x,y,z in verts]
    lines += ['f '+' '.join('%d/%d'%(i+1,i+1) for i in reversed(t)) for t in faces]
    source.write_text('\n'.join(lines)+'\n','utf-8')
    task=U.AssetImportTask();task.filename=str(source);task.destination_path=ROOT+'/Geometry';task.destination_name=n
    task.factory=U.FbxFactory();task.automated=True;task.save=False
    opts=U.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_mesh=True;opts.import_as_skeletal=False
    opts.mesh_type_to_import=U.FBXImportType.FBXIT_STATIC_MESH;opts.original_import_type=U.FBXImportType.FBXIT_STATIC_MESH
    opts.import_materials=False;opts.import_textures=False;opts.import_animations=False
    dat=opts.static_mesh_import_data;dat.combine_meshes=True;dat.auto_generate_collision=False;dat.generate_lightmap_u_vs=False
    dat.build_nanite=False;dat.import_mesh_lo_ds=False;dat.normal_import_method=U.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    dat.convert_scene=False;dat.convert_scene_unit=False;dat.import_uniform_scale=1.;task.options=opts
    AT.import_asset_tasks([task]);mesh=load(ROOT+'/Geometry/'+n)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',U.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    save(mesh);return put(n,mesh,(0,0,0),mat=mat)
def zland(x,y):
    if 3000<x<4200 and 2500<y<13000:return -260
    return max(0,min(320,(y-5000)*.064))
def road(n,a,b,width=650):
    length=math.dist(a,b);dx=-(b[1]-a[1])/length*width/2;dy=(b[0]-a[0])/length*width/2
    verts=[];faces=[];count=max(2,int(length/400))
    for i in range(count+1):
        t=i/count;x=a[0]*(1-t)+b[0]*t;y=a[1]*(1-t)+b[1]*t
        z=max(0,min(320,(y-5000)*.064))+6
        verts += [(x+dx,y+dy,z),(x-dx,y-dy,z)]
        if i: k=i*2;faces += [(k-2,k-1,k+1),(k-2,k+1,k)]
    return geometry(n,verts,faces,stone)
def region(n,pos,half,kind=U.HCM3NavRegionKind.ALLOWED):
    a=spawn(n,U.HCM3NavRegion,pos);a.set_editor_property('stable_id',U.Name(n));a.set_editor_property('kind',kind)
    a.set_editor_property('half_extent',U.Vector(*half));a.get_editor_property('bounds').set_box_extent(U.Vector(*half),False);return a
def light(n,pos,intensity=550):
    a=spawn(n,U.PointLight,pos);c=a.point_light_component;c.set_mobility(U.ComponentMobility.MOVABLE)
    c.set_light_color(U.LinearColor(1,.53,.22,1),False);c.set_intensity(intensity);c.set_attenuation_radius(750);c.set_cast_shadows(True)
    a.tags = list(a.tags)+[U.Name('HC_VS2_Rev2_NightLight'),U.Name('HC_VS2_Rev2_OnIntensity='+str(intensity))]
    if n.startswith('StreetLamp'):
        pole=load('/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc/meshes/props/light/SM_PROP_streetlamp_v01_01')
        put(n+'Fixture',pole,(pos[0],pos[1],pos[2]-310))
    return a
def navigation_map(roads,landmarks):
    # Schematic baked from actual world bounds. North is +Y, right is +X.
    size=768;lo=-12500;hi=12500;pixels=bytearray(bytes((79,114,85))*size*size)
    def pix(x,y):return (int((x-lo)/(hi-lo)*(size-1)),int((hi-y)/(hi-lo)*(size-1)))
    def rect(x0,y0,x1,y1,color):
        a,b=pix(x0,y0);c,d=pix(x1,y1)
        for yy in range(max(0,min(b,d)),min(size-1,max(b,d))+1):
            l=max(0,min(a,c));r=min(size-1,max(a,c));start=(yy*size+l)*3
            if r>=l:pixels[start:start+(r-l+1)*3]=bytes(color)*(r-l+1)
    rect(11000,lo,hi,hi,(52,107,140));rect(3000,2500,4200,hi,(52,107,140))
    for a in E.get_all_level_actors():
        if isinstance(a,U.StaticMeshActor) and ('House' in a.get_actor_label() or any(n in a.get_actor_label() for n in ['GuildHall','HarborCafe','HarborBeacon'])):
            c,e=a.get_actor_bounds(False)
            if 80<e.x<3000 and 80<e.y<3000:rect(c.x-e.x,c.y-e.y,c.x+e.x,c.y+e.y,(135,96,77))
    points=set();edges=[]
    for a,b in roads:points.update((a,b));rect(min(a[0],b[0])-325,min(a[1],b[1])-325,max(a[0],b[0])+325,max(a[1],b[1])+325,(196,181,140))
    for a,b in roads:
        for c,d in roads:
            if a[0]==b[0] and c[1]==d[1] and min(c[0],d[0])<=a[0]<=max(c[0],d[0]) and min(a[1],b[1])<=c[1]<=max(a[1],b[1]):points.add((a[0],c[1]))
    points=sorted(points)
    for a,b in roads:
        on=sorted([p for p in points if min(a[0],b[0])<=p[0]<=max(a[0],b[0]) and min(a[1],b[1])<=p[1]<=max(a[1],b[1])],key=lambda p:math.dist(a,p))
        edges.extend((points.index(p),points.index(q)) for p,q in zip(on,on[1:]))
    for pos in landmarks.values():rect(pos[0]-120,pos[1]-120,pos[0]+120,pos[1]+120,(253,224,117))
    def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    raw=b''.join(b'\x00'+pixels[y*size*3:(y+1)*size*3] for y in range(size))
    png=O/'T_HarborTown.png';png.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
    t=U.AssetImportTask();t.filename=str(png);t.destination_path=ROOT+'/Navigation';t.destination_name='T_HarborTown';t.automated=True;t.save=False;AT.import_asset_tasks([t])
    texture=load(ROOT+'/Navigation/T_HarborTown');texture.set_editor_property('lod_group',U.TextureGroup.TEXTUREGROUP_UI);save(texture)
    factory=U.DataAssetFactory();factory.set_editor_property('data_asset_class',U.HCM4R2MapData)
    data=AT.create_asset('DA_HarborTown',ROOT+'/Navigation',U.HCM4R2MapData,factory)
    for name,value in [('background',texture),('world_minimum',U.Vector2D(lo,lo)),('world_maximum',U.Vector2D(hi,hi)),('road_nodes',[U.Vector(x,y,max(0,zland(x,y))+50) for x,y in points]),('road_edges',[U.IntPoint(a,b) for a,b in edges]),('landmark_ids',[U.Name(n) for n in landmarks]),('landmark_locations',[U.Vector(*p) for p in landmarks.values()]),('source_level',MAP),('bake_identity',hashlib.sha256(png.read_bytes()).hexdigest())]:data.set_editor_property(name,value)
    save(data);runtime.set_editor_property('navigation_map',data)
    R['minimap']={'kind':'native-world schematic, not screenshot','landmark_uv':{n:[(p[0]-lo)/(hi-lo),(hi-p[1])/(hi-lo)] for n,p in landmarks.items()},'road_nodes':len(points),'road_edges':len(edges)}
try:
    source='/Game/HarborCity/M5VS2/WorldRev2/PlayablePolish_ecda03921645/L_PlayablePolish_Afternoon'
    source_disk=P/'Content'/Path(source[6:]).with_suffix('.umap');before=hashlib.sha256(source_disk.read_bytes()).hexdigest()
    check('unique target',not A.does_asset_exist(MAP));check('duplicate approved corner',A.duplicate_asset(source,MAP) is not None)
    check('load private town',L.load_level(MAP));world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    actors=list(E.get_all_level_actors());by={a.get_actor_label().removeprefix('HC_M5VS2_CornerV2_'):a for a in actors}
    # Quarantine the four rejected young appearances before any runtime authoring or test.
    templates={}
    for a in actors:
        if isinstance(a,U.HCM5VS2NPC):
            body=a.get_component_by_class(U.SkeletalMeshComponent);sample=re.search(r'/AvatarSample_([A-Z])/',body.get_skeletal_mesh_asset().get_path_name())[1]
            if sample in 'QRVX':templates[sample]={'class':a.get_class(),'materials':[body.get_material(i) for i in range(body.get_num_materials())]}
            check('remove old NPC from candidate only',E.destroy_actor(a))
    check('four retained adult base appearances',set(templates)==set('QRVX'))
    # Runtime actor contains ordinary PointLightComponent and ordinary audio only.
    runtime=spawn('Runtime',U.HCM5VS2TownRuntime)
    meta=json.loads((D/'part3/audio_source/SOURCE_AND_LICENSE.json').read_text('utf-8'))
    audio={}
    for row in meta['assets']:
        t=U.AssetImportTask();t.filename=row['path'];t.destination_path=ROOT+'/Audio';t.destination_name=row['name'];t.automated=True;t.save=False
        AT.import_asset_tasks([t]);sound=load(ROOT+'/Audio/'+row['name']);sound.set_editor_property('looping',row['loop']);save(sound);audio[row['name']]=sound
    for prop,name in [('harbor_ambience','CoastalWind'),('footstep','StoneStep'),('pistol_shot','PistolReport'),('melee_whoosh','ClothWhoosh'),('motor_loop','MotorLoop')]:runtime.set_editor_property(prop,audio['SW_VS2_'+name])
    stone=by['ContinuousCurvedStoneRoad6m'].static_mesh_component.get_material(0)
    grass=by['ClosedLandSurface'].static_mesh_component.get_material(0)
    cube=load('/Engine/BasicShapes/Cube');wood=load('/Game/HarborCity/M2/Materials/M_PH_oak_wood_planks')
    plaster=load('/Game/HarborCity/M2/Materials/M_PH_white_plaster_02')
    # 0.75/0.85/1.0 are recorded against actual door bounds; 0.75 is the candidate.
    groups={}
    for a in actors:
        label=a.get_actor_label().removeprefix('HC_M5VS2_CornerV2_')
        if '_SM_BLD_' in label or ('_SM_PROP_' in label and '_SM_' in label):groups.setdefault(label.split('_SM_')[0],[]).append(a)
    R['proportion_options']=[{'scale':s,'door_height_cm':[round(by[n].get_actor_bounds(False)[1].z*2*s,2) for n in by if '_door_' in n]} for s in [.75,.85,1.0]]
    R['selected_building_scale']=.75;R['hero_scale']='existing selected 1.25 body scale retained pending runtime proportion review'
    for name,parts in groups.items():
        body=next((a for a in parts if '_body_' in a.get_actor_label()),parts[0]);anchor=body.get_actor_location();anchor.z=0
        for a in parts:
            a.set_actor_location(anchor+(a.get_actor_location()-anchor)*.75,False,True);a.set_actor_scale3d(a.get_actor_scale3d()*.75)
    def house(n,source_name,pos,yaw=0):
        parts=groups[source_name];body=next((a for a in parts if '_body_' in a.get_actor_label()),parts[0]);anchor=body.get_actor_location();anchor.z=0
        q=U.Rotator(yaw=yaw).quaternion()
        for i,a in enumerate(parts):
            c=a.static_mesh_component;delta=a.get_actor_location()-anchor;p=U.Vector(*pos)+q.rotate_vector(delta)
            out=put(n+'_'+str(i),c.static_mesh,(p.x,p.y,p.z),tuple([a.get_actor_scale3d().x]*3),yaw+a.get_actor_rotation().yaw)
            for slot in range(c.get_num_materials()):out.static_mesh_component.set_material(slot,c.get_material(slot))
    # Broad mainland with irregular east coast; the town occupies only its eastern edge.
    verts=[];faces=[];nx=80;ny=100
    for iy in range(ny+1):
        y=-45000+iy*900;shore=12500+1700*math.sin(y/6800)+900*math.cos(y/4100)
        for ix in range(nx+1):
            x=-60000+(shore+60000)*ix/nx
            z=zland(x,y) if abs(y)<14000 and x>-12000 else max(-90,180+140*math.sin(x/6500)*math.cos(y/7300))
            if ix==nx:z=-120
            verts.append((x,y,z-3))
            if ix and iy:
                k=iy*(nx+1)+ix;faces += [(k-nx-2,k-nx-1,k),(k-nx-2,k,k-1)]
    geometry('MainlandCoast',verts,faces,grass)
    roads=[((0,-11000),(0,11000)),((-8000,-6500),(9000,-6500)),((-8000,5000),(9000,5000)),((-8000,-6500),(-8000,10500)),((8000,-6500),(8000,10500)),((-8000,10500),(8000,10500))]
    for i,(a,b) in enumerate(roads):road('StoneRoad'+str(i),a,b)
    R['roads']=[{'a':a,'b':b} for a,b in roads]
    sites=[]
    for x in [-1500,1600]:
        for y in [-9700,-7600,-5000,3600,7000,9200]:
            sites.append((x,y,0 if x<0 else 180))
    for y in [-7800,6300]:
        for x in [-6400,-4400,4900,6600]:sites.append((x,y,90 if y<0 else -90))
    for i,(x,y,yaw) in enumerate(sites):house('DistrictHouse%02d'%i,['EastInn','EastShop','EastProvisioner','HarborShop'][i%4],(x,y,max(0,zland(x,y))),yaw)
    house('HillWindmill','WindmillLandmark',(-6400,9200,320),0)
    # Copy authored life props and trees along the new streets, preserving native materials.
    decor=[by[n] for n in ['MarketCanopy','MarketCounter','MarketCrate','MarketBarrel','MarketSack','CafeChairA','CafeSupplyCrate']]
    trees=[a for a in actors if a.get_actor_label().startswith('HC_M5VS2_CornerV2_') and 'TREE' in a.get_actor_label() and 'Island' not in a.get_actor_label()]
    for i,(x,y,yaw) in enumerate(sites):
        for k,old in enumerate(decor[2:5]):
            c=old.static_mesh_component;p=(x+440+k*85,y-460,max(0,zland(x,y)))
            put('StreetSupply%d_%d'%(i,k),c.static_mesh,p,(.8,.8,.8),i*43,c.get_material(0))
        light('StreetLamp%d'%i,(x*.65,y-570,max(0,zland(x,y))+310))
    if trees:
        for i in range(50):
            old=trees[i%len(trees)];x=-10000+(i%10)*2000;y=-11000+(i//10)*5400
            if abs(x)<800:continue
            c=old.static_mesh_component;put('TownTree%02d'%i,c.static_mesh,(x,y,max(0,zland(x,y))),(1,1,1),i*137,c.get_material(0))
    # Two walk-in rooms with actual collision and open entrance, furnished from Village props.
    def room(n,x,y):
        z=max(0,zland(x,y));put(n+'Floor',cube,(x,y,z-8),(18,14,.16),mat=wood)
        for i,(px,py,sx,sy) in enumerate([(x-900,y,.2,14),(x+900,y,.2,14),(x,y+700,18,.2),(x-575,y-700,6.5,.2),(x+575,y-700,6.5,.2)]):
            put(n+'Wall'+str(i),cube,(px,py,z+155),(sx,sy,3.1),mat=plaster)
        put(n+'Lintel',cube,(x,y-700,z+285),(5,.24,.5),mat=wood)
        put(n+'Ceiling',cube,(x,y,z+330),(18,14,.18),mat=wood)
        for i in range(4):
            old=by['CafeChairA'];c=old.static_mesh_component;put(n+'Chair'+str(i),c.static_mesh,(x-500+(i%2)*1000,y-150+(i//2)*450,z),(1,1,1),90*(i%2),c.get_material(0))
        for i in range(3):
            c=by['MarketCounter'].static_mesh_component;put(n+'Counter'+str(i),c.static_mesh,(x-500+i*500,y+420,z),(1,1,1),0,c.get_material(0))
        light(n+'WarmInterior',(x,y,z+280),1000)
        return U.Box(min=U.Vector(x-900,y-700,z-10),max=U.Vector(x+900,y+700,z+370))
    indoor=[room('GuildHall',-4700,-4400),room('HarborCafe',5800,-4400)]
    # Borrow a complete tall Village silhouette for the lighthouse; art replacement remains recorded.
    house('HarborBeacon','WindmillLandmark',(10200,-5800,0),180)
    c=by['MarketCounter'].static_mesh_component
    put('HarborLanding',cube,(9800,-6500,-30),(20,12,.6),mat=stone)
    bridge=load('/Game/HarborCity/M5VS2/Environment/Village_015f59a690cc/meshes/props/construction/SM_PROP_bridge_wood_01')
    put('CanalBridge',bridge,(3600,5000,65),(2.5,2.5,2.5),90)
    # Gameplay IDs/conditions are unchanged; all gameplay actors use the new locations.
    exp=spawn('Experience',U.HCM3Experience,(0,0,-500))
    exp.set_editor_property('player_appearance_mesh',None);exp.set_editor_property('player_animation_class',None)
    exp.set_editor_property('ride_parking_center',U.Vector(-7500,9500,320));exp.set_editor_property('ride_parking_extent',U.Vector(180,280,260))
    exp.set_editor_property('coffee_parcel_location',U.Vector(5600,-4100,135));exp.set_editor_property('pickup_curb_y',-6500)
    zones={n:region('VS2_'+n,(x,y,max(0,zland(x,y))+130),(850,700,450)) for n,x,y in [('Guild',-4700,-4400),('Harbor',8400,-5400),('Temple',-4800,7800),('Cafe',5800,-4400),('Square',-500,2500),('Ride',-6000,-5900),('Dropoff',-6900,9800)]}
    exp.set_editor_property('dropoff_region',zones['Dropoff']);exp.set_editor_property('passenger_dropoff',U.Transform(location=U.Vector(-6900,9800,420)))
    specs=[('M5_Contact','弦音 · 信使公会','R','Guild',(-4650,-4450),True),('M5_Harbor','汐原 · 港务官','V','Harbor',(8400,-5400),True),
      ('M5_InterceptorA','蒙面盗贼 · 绛','R','Temple',(-4800,7900),False),('M5_InterceptorB','蒙面盗贼 · 灰','V','Temple',(-4400,7700),False),
      ('M3_LinXia','林夏 · 咖啡馆店主','Q','Cafe',(5600,-4300),True),('M3_ChenBo','陈伯 · 守望人','R','Square',(-500,2500),True),('M3_ANing','阿宁 · 旅行商人','V','Ride',(-6000,-5900),True)]
    for i in range(12):
        x,y,_=sites[i];name=['商人','水手','工匠','卫兵'][i%4];letter=['R','V','R','V'][i%4]
        specs.append(('VS2_Citizen%02d'%i,name+' '+str(i+1),letter,'Square',(x+550,y-250),True))
    npcs=[]
    for stable,name,letter,zone,xy,talk in specs:
        tpl=templates[letter];cdo=U.get_default_object(tpl['class']);hh=cdo.get_component_by_class(U.CapsuleComponent).get_unscaled_capsule_half_height()
        x,y=xy;a=spawn(stable,tpl['class'],(x,y,max(0,zland(x,y))+hh+3),90)
        a.set_editor_property('stable_id',U.Name(stable));a.set_editor_property('role_id',U.Name(stable));a.set_editor_property('display_name',name)
        a.set_editor_property('stationary',True);a.set_editor_property('talkable',talk);a.set_editor_property('navigation_region',zones[zone] if not stable.startswith('VS2_') else region(stable+'_Region',(x,y,hh+100),(350,350,500)))
        a.set_editor_property('dialogue_lines',['欢迎来到海湾港町。沿着石板路，可以到信使公会和灯塔。','风车在坡上，咖啡馆就在码头回来的路边。'])
        mesh=a.get_component_by_class(U.SkeletalMeshComponent)
        for i,mat in enumerate(tpl['materials']):mesh.set_material(i,mat)
        npcs.append(a)
    exp.set_editor_property('npcs',npcs)
    story=spawn('Story',U.HCM5StoryDirector,(0,0,-500))
    for prop,v in [('contact_location',(-4650,-4450,90)),('harbor_location',(8400,-5400,90)),('ambush_location',(-4800,7900,180)),('tower_location',(10200,-6500,100)),('interior_center',(-4700,-4400,180)),('interior_half_extent',(900,700,500))]:story.set_editor_property(prop,U.Vector(*v))
    # The three earlier synthesized BGM tracks are retained as historical assets, not selected here.
    for prop in ['exploration_track','combat_track','interior_track']:story.get_editor_property('music').set_editor_property(prop,None)
    for stable,pos in [('M5_RelayCache',(-4800,8700,365)),('M5_TowerUplink',(10200,-6500,100))]:
        a=spawn(stable,U.HCM5StoryTerminal,pos);a.set_editor_property('stable_id',U.Name(stable))
    bounds=next(a for a in actors if isinstance(a,U.HCM5VS2FlightBounds));bounds.set_editor_property('flight_area_half_extent',U.Vector2D(11500,11500));bounds.set_editor_property('no_takeoff_volumes',indoor)
    for a in actors:
        if isinstance(a,U.HCM3NavRegion):E.destroy_actor(a)
    for i,(a,b) in enumerate(roads):
        center=((a[0]+b[0])/2,(a[1]+b[1])/2,180);half=(abs(a[0]-b[0])/2+320,abs(a[1]-b[1])/2+320,600)
        region('M3_Nav_Road_VS2_%d'%i,center,half,U.HCM3NavRegionKind.FORBIDDEN)
    npcnav=spawn('NPCNavBounds',U.NavMeshBoundsVolume,(0,0,700));_,npc_extent=npcnav.get_actor_bounds(False)
    npcnav.set_actor_scale3d(U.Vector(12000/npc_extent.x,12000/npc_extent.y,1800/npc_extent.z))
    nav=spawn('PlayerNavBounds',U.NavMeshBoundsVolume,(0,0,700));_,extent=nav.get_actor_bounds(False);check('native nav brush nonempty',min(extent.x,extent.y,extent.z)>0)
    nav.set_actor_scale3d(U.Vector(12000/extent.x,12000/extent.y,1800/extent.z));nav.tags = list(nav.tags)+[U.Name('HarborCity_M4_R2_Navigation')]
    check('player navigation configuration',U.HCM4R2NavigationAuthor.configure_player_navigation(world,nav))
    # Loading a just-replaced navigation system inside this large import pass
    # retained UE's async asset lock. Build on a fresh reload, never clear it.
    R['navigation_build']='PENDING_FRESH_RELOAD'
    spawn('FunctionalRunner',U.HCM1TestRunner,(0,0,-500))
    switch=spawn('HarborLampSwitch',U.HCM1LightSwitch,(-600,2250,90));switch.set_editor_property('stable_id',U.Name('VS2_PlazaLamp'))
    # The playable world has no review/capture renderer directors.
    R['npcs']=[{'id':s[0],'adult_base':s[2],'role':s[1]} for s in specs]
    R['interiors']=['GuildHall','HarborCafe'];R['scope']='A harbor integration candidate; incomplete decorative/animation polish remains PARTIAL'
    R['landmarks']={'Guild':[-4650,-4450,90],'Harbor':[8400,-5400,90],'Temple':[-4800,7900,180],'Lighthouse':[10200,-6500,100],'Cafe':[5600,-4300,90],'Windmill':[-6400,9200,320]}
    navigation_map(roads,R['landmarks'])
    check('save new playable town',L.save_current_level());check('accepted corner unchanged',hashlib.sha256(source_disk.read_bytes()).hexdigest()==before)
    R.update(status='PASS',actor_count=len(E.get_all_level_actors()),source_map=source,source_sha256=before)
except Exception:
    R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:
    (O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n','utf-8')
