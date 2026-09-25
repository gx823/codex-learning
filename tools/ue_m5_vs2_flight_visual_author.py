"""Create four isolated original flight VFX assets, never bind a Hero or map.

Root runs FlightVisualAssets -> FlightVisualReload using the existing author
launcher. Small OBJ source geometry is generated locally in the evidence folder.
These are original curved meshes, not downloaded shapes or placeholder boxes.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve();DOC=WORK/'docs/HarborCity_M5_VS2';PROJECT=WORK/'HarborCity'
A,E,AT=U.EditorAssetLibrary,U.MaterialEditingLibrary,U.AssetToolsHelpers.get_asset_tools()


def argument(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    if len(rows)!=1:raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]


OUT=Path(argument('M5EvidenceDir')).resolve();PHASE=argument('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and PHASE in ('FlightVisualAssets','FlightVisualReload')
assert not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ROOT='/Game/HarborCity/M5VS2/FlightVisual/Batch_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
R=dict(status='RUNNING',phase=PHASE,destination=ROOT,assets=[],checks=[],geometry=[],
       runtime='NOT_RUN',visual='USER_REVIEW',hero_binding='NOT_RUN',map_writes=0,
       provenance='Original parametric curved light feather, wind ribbon, ring/star glyph authored for HarborCity; no third-party source files.',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={}


def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
def check(name,good,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if good else 'FAIL',observed=observed));dump()
    if not good:raise RuntimeError(name+': '+repr(observed))
def package(obj):return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def disk(obj):
    path=package(obj);assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')
def save(obj):
    path=package(obj);check('fresh own asset '+path,path.startswith(ROOT+'/'))
    A.set_metadata_tag(obj,'HarborCityOwnedBy','HarborCity_M5_VS2_OriginalFlightVFX')
    check('native save '+path,A.save_loaded_asset(obj,False) and disk(path).is_file())
    R['assets'].append(dict(path=path,sha256=sha(disk(path)),bytes=disk(path).stat().st_size));dump()


def make_geometry(kind):
    """Return vertices (x,y,z,u,v) and nondegenerate triangles; all units cm."""
    vertices=[];faces=[]
    def vertex(x,y,z,u=.5,v=.5):
        vertices.append((x,y,z,u,v));return len(vertices)-1
    def face(a,b,c):
        p,q,r=[vertices[i][:3] for i in (a,b,c)]
        d=[q[i]-p[i] for i in range(3)];e=[r[i]-p[i] for i in range(3)]
        cross=(d[1]*e[2]-d[2]*e[1],d[2]*e[0]-d[0]*e[2],d[0]*e[1]-d[1]*e[0])
        if sum(x*x for x in cross)>1e-10:faces.append((a,b,c))
    def quad(a,b,c,d):face(a,b,c);face(a,c,d)
    if kind=='Feather':
        # Closed bowed blade with tapering thickness, shallow barb edge notches,
        # rounded cross-section and a pointed tip. No box primitive is involved.
        nx,ny=40,8
        for side in (1,-1):
            for i in range(nx+1):
                t=i/nx;profile=math.sin(math.pi*t)**.7
                width=7.8*profile*(1-.085*math.sin(t*math.pi*20)**2)
                for j in range(ny+1):
                    s=j/ny*2-1
                    x=t*100;y=s*width
                    z=9*math.sin(math.pi*t)+side*.65*(1-t)**.8*(1-s*s)*profile
                    vertex(x,y,z,t,(s+1)/2)
        grid=(nx+1)*(ny+1)
        for side in range(2):
            for i in range(nx):
                for j in range(ny):
                    a=side*grid+i*(ny+1)+j;b=a+ny+1
                    if side==0:quad(a,b,b+1,a+1)
                    else:quad(a,a+1,b+1,b)
        for j in (0,ny):
            for i in range(nx):
                a=i*(ny+1)+j;b=a+ny+1;quad(a,a+grid,b+grid,b)
    elif kind=='Ribbon':
        for i in range(33):
            t=i/32;w=.85*math.sin(math.pi*t)
            for j in range(3):vertex(t*100,(j-1)*w,2*math.sin(t*math.pi),t,j/2)
        for i in range(32):
            for j in range(2):a=i*3+j;quad(a,a+3,a+4,a+1)
    elif kind=='Rune':
        def ring(radius,width,count=144):
            for i in range(count):
                a=i/count*2*math.pi;b=(i+1)/count*2*math.pi
                ids=[vertex(math.cos(q)*r,math.sin(q)*r,0) for q,r in ((a,radius-width/2),(b,radius-width/2),(b,radius+width/2),(a,radius+width/2))]
                quad(*ids)
        def line(a,b,width):
            d=(b[0]-a[0],b[1]-a[1]);n=math.hypot(*d);p=(-d[1]/n*width/2,d[0]/n*width/2)
            ids=[vertex(x,y,0.12) for x,y in ((a[0]+p[0],a[1]+p[1]),(b[0]+p[0],b[1]+p[1]),(b[0]-p[0],b[1]-p[1]),(a[0]-p[0],a[1]-p[1]))]
            quad(*ids)
        ring(99,1.6);ring(88,0.7);ring(31,0.7)
        points=[(math.cos(i*math.pi/3)*72,math.sin(i*math.pi/3)*72) for i in range(6)]
        for i in range(6):line(points[i],points[(i+2)%6],.8)
        for i in range(18):
            angle=i*2*math.pi/18
            line((math.cos(angle)*91,math.sin(angle)*91),(math.cos(angle)*96,math.sin(angle)*96),1.1)
    else:raise ValueError(kind)
    assert vertices and faces and all(math.isfinite(n) for row in vertices for n in row)
    return vertices,faces


def write_obj(kind):
    vertices,faces=make_geometry(kind);path=OUT/('OriginalLight'+kind+'.obj')
    lines=['# Original HarborCity flight effect. Units centimetres.','o OriginalLight'+kind]
    lines += ['v %.9f %.9f %.9f'%v[:3] for v in vertices]
    lines += ['vt %.9f %.9f'%v[3:] for v in vertices]
    lines += ['f '+' '.join(str(i+1)+'/'+str(i+1) for i in face) for face in faces]
    path.write_text('\n'.join(lines)+'\n',encoding='ascii')
    return path,dict(kind=kind,vertices=len(vertices),triangles=len(faces),source=str(path),sha256=sha(path),
                     source_min=[min(v[i] for v in vertices) for i in range(3)],source_max=[max(v[i] for v in vertices) for i in range(3)])


def material():
    m=AT.create_asset('M_OriginalFlightLight',ROOT,U.Material,U.MaterialFactoryNew());check('new native VFX material',m is not None)
    for name,value in dict(blend_mode=U.BlendMode.BLEND_ADDITIVE,shading_model=U.MaterialShadingModel.MSM_UNLIT,two_sided=True).items():m.set_editor_property(name,value)
    def node(cls):return E.create_material_expression(m,cls)
    def connect(a,b,pin,out=''):check('material native pin '+pin,E.connect_material_expressions(a,out,b,pin))
    def mul(a,b):
        result=node(U.MaterialExpressionMultiply);connect(a,result,'A');connect(b,result,'B');return result
    def const(value):
        result=node(U.MaterialExpressionConstant);result.set_editor_property('r', value);return result
    color=node(U.MaterialExpressionVectorParameter);color.set_editor_property('parameter_name', 'LightColor');color.set_editor_property('default_value', U.LinearColor(0.58, 0.83, 1.0, 1.0))
    gain=node(U.MaterialExpressionScalarParameter);gain.set_editor_property('parameter_name', 'GlowGain');gain.set_editor_property('default_value', 2.3)
    opacity=node(U.MaterialExpressionScalarParameter);opacity.set_editor_property('parameter_name', 'OpacityGain');opacity.set_editor_property('default_value', 0.8)
    uv=node(U.MaterialExpressionTextureCoordinate);mask=node(U.MaterialExpressionComponentMask)
    mask.set_editor_property('r', False);mask.set_editor_property('g', True);mask.set_editor_property('b', False);mask.set_editor_property('a', False);connect(uv,mask,'')
    centered=node(U.MaterialExpressionSubtract);connect(mul(mask,const(2)),centered,'A');connect(const(1),centered,'B')
    absolute=node(U.MaterialExpressionAbs);connect(centered,absolute,'')
    one_minus=node(U.MaterialExpressionOneMinus);connect(absolute,one_minus,'')
    soft=node(U.MaterialExpressionPower);connect(one_minus,soft,'Base');connect(const(.65),soft,'Exp')
    check('emissive native material output',E.connect_material_property(mul(color,gain),'',U.MaterialProperty.MP_EMISSIVE_COLOR))
    check('soft curved-edge opacity output',E.connect_material_property(mul(soft,opacity),'',U.MaterialProperty.MP_OPACITY))
    E.recompile_material(m);return m


def audit_mesh(mesh,row):
    box=mesh.get_bounding_box();actual_min=[box.min.x,box.min.y,box.min.z];actual_max=[box.max.x,box.max.y,box.max.z]
    check('native exact triangles '+row['kind'],mesh.get_num_triangles(0)==row['triangles'],[mesh.get_num_triangles(0),row['triangles']])
    # Raw OBJ import may change handedness but all generated Y ranges are symmetric.
    error=max(abs(a-b) for a,b in zip(actual_min+actual_max,row['source_min']+row['source_max']))
    check('native centimeter bounds '+row['kind'],error<.002,dict(min=actual_min,max=actual_max,max_error_cm=error))
    return dict(**row,path=package(mesh),native_bounds_min=actual_min,native_bounds_max=actual_max)


try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    for folder in ('M5VS1/HeroSelestia','M5VS2/HeroSelestia','M5VS2/HeroRev2'):
        for file in (PROJECT/'Content/HarborCity'/folder).rglob('*.uasset'):PROTECTED[str(file)]=sha(file)
    if PHASE=='FlightVisualReload':
        reports=sorted((DOC/'editor_runtime').glob('author_*_FlightVisualAssets/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True)
        prior=next(json.loads(p.read_text('utf-8-sig')) for p in reports if json.loads(p.read_text('utf-8-sig')).get('status')=='PASS')
        ROOT=prior['destination'];R['destination']=ROOT
        check('actual private four outputs',re.fullmatch(r'/Game/HarborCity/M5VS2/FlightVisual/Batch_[0-9a-f]{12}',ROOT) is not None and len(prior['assets'])==4)
        for row in prior['assets']:
            check('saved asset SHA '+row['path'],sha(disk(row['path']))==row['sha256']);PROTECTED[str(disk(row['path']))]=row['sha256']
        R['assets']=prior['assets'];R['material']=prior['material']
        for row in prior['geometry']:
            mesh=A.load_asset(row['path']);check('native mesh reload '+row['kind'],isinstance(mesh,U.StaticMesh))
            audit=audit_mesh(mesh,{k:v for k,v in row.items() if k not in ('path','native_bounds_min','native_bounds_max')});R['geometry'].append(audit)
    else:
        check('fresh private VFX namespace',not A.does_directory_exist(ROOT))
        glow=material();save(glow);R['material']=package(glow)
        for kind in ('Feather','Ribbon','Rune'):
            path,row=write_obj(kind);options=U.FbxImportUI()
            options.set_editor_property('automated_import_should_detect_type', False);options.set_editor_property('import_mesh', True);options.set_editor_property('import_as_skeletal', False)
            options.set_editor_property('mesh_type_to_import', U.FBXImportType.FBXIT_STATIC_MESH);options.set_editor_property('original_import_type', U.FBXImportType.FBXIT_STATIC_MESH)
            options.set_editor_property('import_materials', False);options.set_editor_property('import_textures', False);options.set_editor_property('import_animations', False)
            d=options.get_editor_property('static_mesh_import_data');d.set_editor_property('combine_meshes', True);d.set_editor_property('auto_generate_collision', False);d.set_editor_property('generate_lightmap_u_vs', False);d.set_editor_property('build_nanite', False)
            d.set_editor_property('remove_degenerates', True);d.set_editor_property('import_mesh_lo_ds', False);d.set_editor_property('normal_import_method', U.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
            d.set_editor_property('import_translation', U.Vector());d.set_editor_property('import_rotation', U.Rotator());d.set_editor_property('import_uniform_scale', 1)
            d.set_editor_property('convert_scene', False);d.set_editor_property('convert_scene_unit', False);d.set_editor_property('force_front_x_axis', False);d.set_editor_property('transform_vertex_to_absolute', True);d.set_editor_property('bake_pivot_in_vertex', False)
            task=U.AssetImportTask();task.set_editor_property('filename', str(path));task.set_editor_property('destination_path', ROOT);task.set_editor_property('destination_name', 'SM_OriginalLight' + kind)
            task.set_editor_property('automated', True);task.set_editor_property('replace_existing', False);task.set_editor_property('save', False);task.set_editor_property('factory', U.FbxFactory());task.set_editor_property('options', options)
            AT.import_asset_tasks([task]);meshes=[x for x in task.get_objects() if isinstance(x,U.StaticMesh)]
            check('one imported original mesh '+kind,len(meshes)==1)
            mesh=meshes[0];R['geometry'].append(audit_mesh(mesh,row));mesh.set_material(0,glow);save(mesh)
    R['status']='PASS'
except Exception:R['status']='FAIL';R['exception']=traceback.format_exc()
finally:
    changed=[file for file,value in PROTECTED.items() if not Path(file).is_file() or sha(file)!=value]
    R['source_preservation']=dict(files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Flight VFX author failed; preserve outputs and evidence')
