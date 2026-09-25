"""Read-only R4 native water graph + source/render coastal-mesh diagnostic.
No package writes, actor modifications, viewport requests, UE launch or build.
Root promotes this new file then runs the existing author runner: CoastRockProbe.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve()
PROJECT=WORK/'HarborCity'
DOC=WORK/'docs/HarborCity_M5_VS2'
CMD=U.SystemLibrary.get_command_line()
def argument(name):
    m=re.search(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]*)"|(\S+))',CMD)
    return next((s for s in m.groups() if s is not None),'') if m else ''
OUT=Path(argument('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
R=dict(status='RUNNING',phase=argument('M5AuthorPhase'),checks=[],assets=[],
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),mutations='NONE',runtime='NOT_RUN')
PROTECTED={}
METHODS={};ENUMS={};CLASSES={}
A=U.EditorAssetLibrary
E=U.MaterialEditingLibrary
Q=U.GeometryScript_MeshQueries
LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
ACT=U.get_editor_subsystem(U.EditorActorSubsystem)

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed))
    if not ok:raise RuntimeError(name+': '+str(observed))
def protect(path,expected=None):
    p=Path(path).resolve();actual=sha(p)
    check('source hash matches prior evidence',expected is None or actual==expected,str(p))
    PROTECTED[str(p)]=actual
def document(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def disk(obj):
    package=obj.split('.')[0]
    check('owned exact VS2 source',package.startswith('/Game/HarborCity/M5VS2/') and '..' not in package)
    return PROJECT/'Content'/Path(package[6:]).with_suffix('.umap' if package.rsplit('/',1)[-1].startswith('L_') else '.uasset')
def vec(v):return [float(v.x),float(v.y),float(v.z)]
def color(c):return [float(getattr(c,n)) for n in ('r','g','b','a')]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def sub(a,b):return [x-y for x,y in zip(a,b)]
def norm(v):
    length=math.sqrt(dot(v,v));return [n/length for n in v] if length>1.e-12 else [0.,0.,0.]
def normalized(name):return name.replace('_','').lower()
def schema_checkpoint():
    (OUT/'schema_readback.json').write_text(json.dumps(R['schema'],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def enum_value(kind,name):return ENUMS.get((kind,normalized(name)))
def method(kind,name):return METHODS.get((kind,normalized(name)))
def is_kind(value,name):return name in CLASSES and isinstance(value,CLASSES[name])
def reflect_schema():
    # Introspect the complete pending surface before any optional material or
    # geometry property is used. Hidden enum entries stay NOT_AVAILABLE; do not
    # fabricate numeric enum values or reach through a private engine property.
    R['schema']=dict(classes={},enums={},methods={},properties={},missing=[])
    class_names=('MaterialProperty','MaterialEditingLibrary','GeometryScript_MeshQueries',
        'GeometryScript_AssetUtils','GeometryScriptCopyMeshFromAssetOptions','GeometryScriptMeshReadLOD',
        'GeometryScriptLODType','GeometryScriptOutcomePins','DynamicMesh','StaticMeshComponent',
        'MaterialExpressionConstant','MaterialExpressionCustom','MaterialExpressionScalarParameter',
        'MaterialExpressionVectorParameter','MaterialExpressionMaterialFunctionCall','MathLibrary')
    for name in class_names:
        kind=getattr(U,name,None)
        R['schema']['classes'][name]=dict(status='AVAILABLE' if kind else 'NOT_AVAILABLE',
            public_names=[n for n in dir(kind) if not n.startswith('_')] if kind else [])
        if kind:CLASSES[name]=kind
    for kind_name in ('MaterialProperty','GeometryScriptLODType','GeometryScriptOutcomePins'):
        kind=CLASSES.get(kind_name);entries={}
        if kind:
            for name in dir(kind):
                value=getattr(kind,name)
                if isinstance(value,kind):
                    entries[name]=str(value);ENUMS[(kind_name,normalized(name))]=value
        R['schema']['enums'][kind_name]=entries
    pending={'MaterialEditingLibrary':('GetMaterialPropertyInputNode','GetMaterialPropertyInputNodeOutputName',
        'GetInputsForMaterialExpression','GetMaterialInstanceScalarParameterValue','GetMaterialInstanceVectorParameterValue'),
        'GeometryScript_MeshQueries':('GetNumTriangleIDs','GetTrianglePositions','GetTriangleFaceNormal',
        'GetTriangleNormals','GetTriangleVertexColors','GetMeshInfoString','GetIsClosedMesh'),
        'GeometryScript_AssetUtils':('CopyMeshFromStaticMeshV2',),'MathLibrary':('TransformLocation',)}
    for kind_name,names in pending.items():
        kind=CLASSES.get(kind_name)
        for wanted in names:
            matches=[n for n in dir(kind) if normalized(n)==normalized(wanted)] if kind else []
            item=dict(status='AVAILABLE' if len(matches)==1 else 'NOT_AVAILABLE',actual_names=matches)
            if len(matches)==1:
                fn=getattr(kind,matches[0]);METHODS[(kind_name,normalized(wanted))]=fn;item['doc']=fn.__doc__
            else:R['schema']['missing'].append(kind_name+'.'+wanted)
            R['schema']['methods'][kind_name+'.'+wanted]=item
    R['schema']['material_outputs']={field:dict(status='AVAILABLE' if enum_value('MaterialProperty','MP_'+field) is not None else 'NOT_AVAILABLE',
        actual=str(enum_value('MaterialProperty','MP_'+field))) for field in
        ('BASE_COLOR','ROUGHNESS','NORMAL','REFRACTION','PIXEL_DEPTH_OFFSET','EMISSIVE_COLOR','OPACITY','WORLD_POSITION_OFFSET')}
    # Actual public struct property readback is also captured before mesh copies.
    for kind_name,properties in (('GeometryScriptMeshReadLOD',('lod_type','lod_index')),
                                 ('GeometryScriptCopyMeshFromAssetOptions',('apply_build_settings','use_build_scale'))):
        if kind_name in CLASSES:
            obj=CLASSES[kind_name]()
            for name in properties:property_read(obj,name,kind_name)
    schema_checkpoint()
    check('required native methods enumerated together',not R['schema']['missing'],R['schema']['missing'])

def property_read(obj,name,label=None):
    key=(label or (obj.get_path_name() if hasattr(obj,'get_path_name') else type(obj).__name__))+'.'+name
    try:
        value=obj.get_editor_property(name)
        serial=(value if value is None or isinstance(value,(bool,int,float,str)) else
                value.get_path_name() if hasattr(value,'get_path_name') else str(value))
        record=dict(status='AVAILABLE',actual=serial)
    except Exception as error:
        value=None;record=dict(status='NOT_AVAILABLE',error=str(error))
    R['schema']['properties'][key]=record
    return value,record

def graph(material,instance):
    fields=('BASE_COLOR','ROUGHNESS','NORMAL','REFRACTION','PIXEL_DEPTH_OFFSET','EMISSIVE_COLOR','OPACITY','WORLD_POSITION_OFFSET')
    property_names=('refraction_method','refraction_depth_bias','disable_depth_test','blend_mode')
    properties={name:property_read(material,name)[1] for name in property_names}
    schema_checkpoint()
    rows={};roots={}
    def visit(node):
        if not node:return None
        key=node.get_path_name()
        if key in rows:return key
        check('bounded native material graph',len(rows)<256)
        item=dict(class_path=node.get_class().get_path_name())
        rows[key]=item
        if is_kind(node,'MaterialExpressionConstant'):item['value']=property_read(node,'r')[1]
        if is_kind(node,'MaterialExpressionCustom'):item['code']=property_read(node,'code')[1]
        if is_kind(node,'MaterialExpressionScalarParameter'):
            name,read=property_read(node,'parameter_name');item['parameter']=read
            if name is not None:item['effective']=float(E.get_material_instance_scalar_parameter_value(instance,name))
        if is_kind(node,'MaterialExpressionVectorParameter'):
            name,read=property_read(node,'parameter_name');item['parameter']=read
            if name is not None:item['effective']=color(E.get_material_instance_vector_parameter_value(instance,name))
        if is_kind(node,'MaterialExpressionMaterialFunctionCall'):
            item['material_function']=property_read(node,'material_function')[1]
        item['inputs']=[visit(n) for n in E.get_inputs_for_material_expression(material,node)]
        return key
    for field in fields:
        prop=enum_value('MaterialProperty','MP_'+field)
        if prop is None:
            roots[field]=dict(status='NOT_AVAILABLE',reason='Not exposed in the actual runtime MaterialProperty enum; no numeric/private fallback.')
            continue
        roots[field]=dict(status='AVAILABLE',node=visit(E.get_material_property_input_node(material,prop)),
                         output=E.get_material_property_input_node_output_name(material,prop))
    schema_checkpoint()
    return dict(material=material.get_path_name(),instance=instance.get_path_name(),
                properties=properties,roots=roots,nodes=rows)

def mesh_read(actor,mesh,lod_name,lod_enum):
    if lod_enum is None:
        return dict(lod_type=lod_name,status='NOT_AVAILABLE',interpretation='Required LOD enum not exposed; no numeric/private fallback.')
    dynamic=U.DynamicMesh()
    lod=U.GeometryScriptMeshReadLOD(lod_type=lod_enum,lod_index=0)
    result=U.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,dynamic,
        U.GeometryScriptCopyMeshFromAssetOptions(),lod,False)
    out=dict(lod_type=lod_name,copy_outcome=str(result[-1]))
    if result[-1]!=enum_value('GeometryScriptOutcomePins','SUCCESS'):
        out.update(status='NOT_AVAILABLE',interpretation='Native copy did not provide this LOD; no normals/vertices inferred.')
        return out
    count=method('GeometryScript_MeshQueries','GetNumTriangleIDs')(dynamic)
    check('bounded native coastal triangles',0<count<=1000,dict(mesh=mesh.get_path_name(),triangles=count))
    transform=actor.get_actor_transform();triangles=[]
    for tid in range(count):
        valid,p0,p1,p2=Q.get_triangle_positions(dynamic,tid)
        if not valid:continue
        face,face_valid=Q.get_triangle_face_normal(dynamic,tid)
        normal_values=Q.get_triangle_normals(dynamic,tid)
        color_values=Q.get_triangle_vertex_colors(dynamic,tid)
        local=[vec(p) for p in (p0,p1,p2)]
        world=[vec(U.MathLibrary.transform_location(transform,p)) for p in (p0,p1,p2)]
        computed=norm(cross(sub(world[1],world[0]),sub(world[2],world[0])))
        local_computed=norm(cross(sub(local[1],local[0]),sub(local[2],local[0])))
        normal_ok=bool(normal_values[-1]);color_ok=bool(color_values[-1])
        triangles.append(dict(id=tid,local_vertices_cm=local,world_vertices_cm=world,
            native_face_normal_local=vec(face) if face_valid else None,world_cross_normal=computed,
            native_face_agrees_cross=(dot(vec(face),local_computed)>.999 if face_valid else None),
            overlay_normals=[vec(n) for n in normal_values[1:4]] if normal_ok else None,
            overlay_colors=[color(c) for c in color_values[1:4]] if color_ok else None,
            centroid_world_z=sum(p[2] for p in world)/3.,upward=computed[2]>.05))
    above=[t for t in triangles if t['centroid_world_z']>-150]
    upward=[t for t in above if t['upward']]
    out.update(status='PASS_READBACK',mesh_info=Q.get_mesh_info_string(dynamic),
               is_closed=Q.get_is_closed_mesh(dynamic),triangles=triangles,
               above_water_centroid_triangles=len(above),above_water_upward_triangles=len(upward),
               above_water_downward_triangles=sum(t['world_cross_normal'][2]<-.05 for t in above),
               native_face_cross_agreement=all(t['native_face_agrees_cross'] for t in triangles),
               interpretation='Actual native LOD positions, geometric face normals and shading overlays; not a rendered visibility proof.')
    return out

try:
    check('exact isolated read-only phase',R['phase']=='CoastRockProbe')
    check('correct project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty map',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    reflect_schema()
    source_path=DOC/'editor_runtime/author_20260925_072631_409_78941ab7_CornerVTwo/author_result.json'
    protect(source_path,'831d51f263d4aec822c1c6c6f328697d6bb9a6bd3690e6f58f2b7fcc4324d4c7')
    source=document(source_path)
    check('actual R4 successful author',source['status']=='PASS' and source['art_revision']==4)
    for row in source['assets']:protect(disk(row['path']),row['sha256'])
    probe_path=DOC/'editor_runtime/author_20260925_081317_359_fe08b9a5_CornerVTwoProbe/author_result.json'
    protect(probe_path,'2e02c945fc45434b5f5c7db9bcf3c65d29f2214e74ca1952cb5c8b10ef414bdf');probe=document(probe_path)
    check('actual R5 probe excluded disabled depth test',probe['status']=='PASS' and
          probe['r4_surface_probe']['water_master']['disable_depth_test'] is False)
    check('load R4 map read-only',LEVEL.load_level(source['map']))
    actors=list(ACT.get_all_level_actors())
    rocks=[a for a in actors if a.get_actor_label().startswith('HC_M5VS2_CornerV2_CoastRock_')]
    water_actors=[a for a in actors if a.get_actor_label()=='HC_M5VS2_CornerV2_SeaSurface']
    check('actual 15 rock actors and one water actor',len(rocks)==15 and len(water_actors)==1)
    sea=water_actors[0];component=sea.get_component_by_class(U.StaticMeshComponent)
    water=component.get_material(0);parent=water.get_editor_property('parent')
    protect(disk(water.get_path_name()));protect(disk(parent.get_path_name()))
    check('actual scene water matches R4 candidate',water.get_path_name()==source['water_revision']['candidate'])
    R['water']=dict(actor=sea.get_path_name(),location_cm=vec(sea.get_actor_location()),
                    scale=vec(sea.get_actor_scale3d()),graph=graph(parent,water))
    R['rocks']=[]
    # The blue image regions identified offline project onto rocks 06/07/08.
    # Probe exactly these three actual actors and both source/render LOD0.
    for actor in sorted(rocks,key=lambda a:a.get_actor_label()):
        if actor.get_actor_label()[-2:] not in ('06','07','08'):continue
        body=actor.get_component_by_class(U.StaticMeshComponent)
        mesh=body.get_editor_property('static_mesh')
        row=dict(actor=actor.get_path_name(),label=actor.get_actor_label(),mesh=mesh.get_path_name(),
                 location_cm=vec(actor.get_actor_location()),scale=vec(actor.get_actor_scale3d()),
                 rotation=actor.get_actor_rotation().export_text(),
                 actual_materials=[body.get_material(i).get_path_name() for i in range(body.get_num_materials())],
                 lods=[mesh_read(actor,mesh,'SOURCE_MODEL',enum_value('GeometryScriptLODType','SOURCE_MODEL')),
                       mesh_read(actor,mesh,'RENDER_DATA',enum_value('GeometryScriptLODType','RENDER_DATA'))])
        check('actual source LOD readback obtained',row['lods'][0]['status']=='PASS_READBACK')
        R['rocks'].append(row)
    check('three exact native rock subjects read',len(R['rocks'])==3)
    check('all prior package/evidence files remain byte-identical',all(sha(p)==h for p,h in PROTECTED.items()))
    R.update(status='PASS',source_map=source['map'],source_author=str(source_path),
             protected_files=PROTECTED,pass_scope='Read-only actual graph, actor bindings/transforms, source/render LOD geometry; no visual diagnosis asserted.')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    if 'schema' in R:schema_checkpoint()
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if R['status']!='PASS':raise RuntimeError('CoastRockProbe failed; evidence retained; no asset writes requested')
