"""Native split import of the seven purchased halo pieces, preserving old combined mesh/hero."""
from pathlib import Path
import hashlib,json,re,traceback,uuid
import unreal as U
WORK=Path('D:/科研学习/codex学习');DOC=WORK/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;E=U.MaterialEditingLibrary;AT=U.AssetToolsHelpers.get_asset_tools()
arg=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(arg)==1
OUT=Path(arg[0][0] or arg[0][1]).resolve();assert OUT.is_relative_to(DOC.resolve())
ROOT='/Game/HarborCity/M5VS2/HaloRev2/Batch_'+uuid.uuid4().hex[:12]
SOURCE=WORK/'assets/M5_VS1/hero/Selestia/Source_v1_02_b9d138dabe3d/SELESTIA_halo.fbx'
R=dict(status='RUNNING',root=ROOT,checks=[],assets=[],pieces=[],runtime='NOT_RUN',visual='NOT_RUN',blueprint_binding='NOT_RUN')
def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,v=None):
    R['checks'].append(dict(name=n,status='PASS' if b else 'FAIL',observed=v));dump()
    if not b:raise RuntimeError(n+': '+str(v))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def disk(p):return WORK/'HarborCity/Content'/Path(p.removeprefix('/Game/').split('.')[0]).with_suffix('.uasset')
def vec(v):return [float(v.x),float(v.y),float(v.z)]
def save(o):
    p=o.get_path_name().split('.')[0];check('owned new namespace',p.startswith(ROOT+'/'))
    A.set_metadata_tag(o,'HarborCityOwnedBy','HarborCity_M5_VS2_HaloMotion_R2')
    check('native save '+p,A.save_loaded_asset(o,False));R['assets'].append(dict(path=p,sha256=sha(disk(p))));dump()
try:
    check('original purchased FBX exact bytes',sha(SOURCE)=='5aabdaae311e83d68b1c3c35784e0d858bb48e512ae0d2aaa9e226a9119adde1')
    audit_file=WORK/'docs/HarborCity_M5_VS1/assets/hero/SELESTIA_HALO_SOURCE_AUDIT.json'
    check('original geometry transform audit exact',sha(audit_file)=='e9184f5a474b6135b69ec9b8d8dc8a56a4289cf0fd2b7fdc52047320a00bcb68')
    audit=json.loads(audit_file.read_text(encoding='utf-8'));models=audit['sources'][0]['models'];pred=audit['static_import_attachment_prediction']
    source_audit=json.loads((DOC/'research/SELESTIA_SOURCE_AUDIT.json').read_text(encoding='utf-8'))
    motion=source_audit['halo_animations'];check('single source animation',len(motion)==1)
    motion=motion[0];check('source30sloop',motion['settings']['m_StopTime']==30 and motion['settings']['m_LoopTime']==1)
    check('floating is source physics, not translation clip',motion['position_channels']==[])
    rates={c['path']:(c['curve_summary']['last_value']['y']-c['curve_summary']['first_value']['y'])/30 for c in motion['euler_channels']}
    R['source_animation']=motion;R['head_transform']=pred
    protected={str(p):sha(p) for folder in ('Content/HarborCity/M5VS1/HeroSelestia','Content/HarborCity/M5VS2/HeroSelestia') for p in (WORK/'HarborCity'/folder).rglob('*.uasset')}
    options=U.FbxImportUI();options.automated_import_should_detect_type=False;options.import_mesh=True;options.import_as_skeletal=False
    options.mesh_type_to_import=U.FBXImportType.FBXIT_STATIC_MESH;options.original_import_type=U.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials=False;options.import_textures=False;options.import_animations=False
    d=options.static_mesh_import_data;d.combine_meshes=False;d.auto_generate_collision=False;d.generate_lightmap_u_vs=False;d.build_nanite=False
    d.remove_degenerates=False;d.import_mesh_lo_ds=False;d.normal_import_method=U.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    d.import_translation=U.Vector();d.import_rotation=U.Rotator();d.import_uniform_scale=1
    d.convert_scene=True;d.convert_scene_unit=True;d.force_front_x_axis=False;d.transform_vertex_to_absolute=True;d.bake_pivot_in_vertex=False
    task=U.AssetImportTask();task.filename=str(SOURCE);task.destination_path=ROOT;task.destination_name='SM_HaloSource'
    task.automated=True;task.replace_existing=False;task.save=False;task.factory=U.FbxFactory();task.options=options
    AT.import_asset_tasks([task]);meshes=[x for x in task.get_objects() if isinstance(x,U.StaticMesh)]
    check('seven individual source meshes',len(meshes)==7,[m.get_path_name() for m in meshes])
    material=AT.create_asset('M_OriginalHaloGlow',ROOT,U.Material,U.MaterialFactoryNew());check('new material',material is not None)
    material.set_editor_property('blend_mode',U.BlendMode.BLEND_TRANSLUCENT);material.set_editor_property('shading_model',U.MaterialShadingModel.MSM_UNLIT);material.set_editor_property('two_sided',False)
    color=E.create_material_expression(material,U.MaterialExpressionVectorParameter,0,0);color.set_editor_property('parameter_name','OriginalHDRColor');color.set_editor_property('default_value',U.LinearColor(1.0592737,.94280905,.83743626,.5882353))
    glow=E.create_material_expression(material,U.MaterialExpressionScalarParameter,0,150);glow.set_editor_property('parameter_name','FlightGlow');glow.set_editor_property('default_value',1)
    multiply=E.create_material_expression(material,U.MaterialExpressionMultiply,220,0)
    check('original color input',E.connect_material_expressions(color,'',multiply,'A'));check('flight glow input',E.connect_material_expressions(glow,'',multiply,'B'))
    check('unlit original HDR output',E.connect_material_property(multiply,'',U.MaterialProperty.MP_EMISSIVE_COLOR))
    opacity=E.create_material_expression(material,U.MaterialExpressionConstant,220,180);opacity.set_editor_property('r',.5882353)
    check('original alpha output',E.connect_material_property(opacity,'',U.MaterialProperty.MP_OPACITY));E.recompile_material(material);save(material)
    names={m['name'].split('::')[0]:m for m in models};seen=set();allmin=[1e6]*3;allmax=[-1e6]*3;tris=0
    for mesh in meshes:
        matches=[n for n in names if mesh.get_name().endswith(n)]
        # longest exact suffix disambiguates Halo_main_rhombus from Halo_main_rhombus_sub.
        check('one source node retained in name',len(matches)==1,mesh.get_name());name=matches[0];check('unique piece '+name,name not in seen);seen.add(name)
        expected=names[name]['geometry'][0]['polygons']['triangulated_count'];actual=mesh.get_num_triangles(0);check('source triangles '+name,actual==expected,[actual,expected]);tris+=actual
        bounds=mesh.get_bounding_box();bmin=vec(bounds.min);bmax=vec(bounds.max)
        allmin=[min(a,b) for a,b in zip(allmin,bmin)];allmax=[max(a,b) for a,b in zip(allmax,bmax)]
        mesh.set_material(0,material);save(mesh)
        R['pieces'].append(dict(name=name,mesh=mesh.get_path_name(),triangles=actual,bounds_min=bmin,bounds_max=bmax,source_degrees_per_second=rates.get(name,0)))
    check('source combined triangle sum unchanged',tris==pred['expected_triangles'],tris)
    check('split preserves aggregate source bounds',max(abs(a-b) for a,b in zip(allmin+allmax,pred['expected_bounds_min_cm']+pred['expected_bounds_max_cm']))<.002,[allmin,allmax])
    check('protected old hero assets unchanged',all(sha(p)==h for p,h in protected.items()),len(protected))
    R['material']=material.get_path_name();R['source_sha256']=sha(SOURCE);R['status']='PASS'
except Exception:R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:dump()
if R['status']!='PASS':raise RuntimeError(R.get('error','Halo split failed'))
