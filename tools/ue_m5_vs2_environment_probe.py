"""Read-only UE environment/import probe. NEVER submits imports or changes a world.

Native: existing author runner + -M5EvidenceDir=... [-M5EnvironmentManifest=...].
Offline: this interpreter script --validate-manifest manifest.json.
The optional manifest names REAL downloaded files; no sample FBX names are assumed.
"""
from pathlib import Path
import argparse, hashlib, json, re, traceback

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOCS = WORK / 'docs/HarborCity_M5_VS2'
RAW = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Environment')
DEST = '/Game/HarborCity/M5VS2/Environment/Imported/'

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def validate_manifest(path):
    """Validate paths/bytes and proposed targets only; imports require a later author."""
    path=Path(path).resolve()
    if not path.is_relative_to(DOCS.resolve()) or path.stat().st_size>2*1024*1024:
        raise ValueError('Manifest must be bounded JSON under M5_VS2 docs')
    data=json.loads(path.read_text(encoding='utf-8-sig'))
    if data.get('schema_version')!=1:raise ValueError('Expected manifest schema_version 1')
    source_root=Path(data['source_root']).resolve()
    if not source_root.is_relative_to(RAW.resolve()) or not source_root.is_dir():
        raise ValueError('Source root must exist inside authorized E environment directory')
    rows=data.get('meshes',[])
    if not 1<=len(rows)<=64:raise ValueError('Probe accepts 1–64 explicitly selected FBX sources')
    result=[];destinations=set();sources=set()
    for row in rows:
        relative=Path(row['relative_path'])
        if relative.is_absolute() or '..' in relative.parts:raise ValueError('Use bounded source-relative paths')
        source=(source_root/relative).resolve()
        if not source.is_relative_to(source_root) or not source.is_file() or source.suffix.lower()!='.fbx':
            raise ValueError('Missing FBX or escaped source root: '+str(relative))
        expected=row.get('sha256','')
        if not re.fullmatch('[0-9a-fA-F]{64}',expected) or sha(source)!=expected.lower():
            raise ValueError('Actual FBX digest differs: '+str(relative))
        destination=row['destination']
        if not re.fullmatch(re.escape(DEST)+r'[A-Za-z0-9_]+/[A-Za-z0-9_/]+',destination):
            raise ValueError('Destination must use isolated VS2 Environment/Imported/<batch>/ namespace')
        if destination in destinations or source in sources:raise ValueError('Duplicate source/target')
        sources.add(source);destinations.add(destination)
        if not isinstance(row.get('role'),str) or not row['role'].strip():raise ValueError('Actual intended role required')
        result.append(dict(source=str(source),sha256=expected.lower(),bytes=source.stat().st_size,
            destination=destination,role=row['role'],unit_axis_evidence=row.get('unit_axis_evidence','UNKNOWN'),
            expected_material_slots=row.get('expected_material_slots','UNKNOWN')))
    return dict(manifest=str(path),manifest_sha256=sha(path),source_root=str(source_root),meshes=result,
        declared_source_id=data.get('source_id','UNKNOWN'),declared_license_evidence=data.get('license_evidence','UNKNOWN'),
        license_validation='NOT_PERFORMED_BY_TECHNICAL_PROBE',import_status='NOT_RUN')

def native_main(U):
    line=U.SystemLibrary.get_command_line()
    def argument(name,required=False):
        matches=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',line)
        if len(matches)>1 or (required and len(matches)!=1):raise RuntimeError('Exactly one -'+name+' required')
        return (matches[0][0] or matches[0][1]) if matches else None
    out=Path(argument('M5EvidenceDir',True)).resolve()
    if not out.is_relative_to(DOCS.resolve()):raise RuntimeError('Evidence must remain under M5_VS2 docs')
    out.mkdir(parents=True,exist_ok=True)
    report=out/'environment_probe.json'
    if report.exists() or (out/'author_result.json').exists():raise RuntimeError('Fresh evidence directory required')
    R=dict(status='RUNNING',scope='READ_ONLY_SCHEMA_AND_BOUNDED_CATALOG',checks=[],classes={},methods={},
        source_status='NOT_DOWNLOADED_OR_MANIFEST_NOT_PROVIDED',imports='NOT_RUN',asset_writes=0,
        maps_changed=0,actors_created=0,visual='NOT_RUN',style_approval='USER_REVIEW',sample_extent_m=[50,50])
    def write():
        text=json.dumps(R,ensure_ascii=False,indent=2)+'\n'
        report.write_text(text,encoding='utf-8');(out/'author_result.json').write_text(text,encoding='utf-8')
    def check(name,ok,observed=None):
        R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed));write()
        if not ok:raise RuntimeError(name+': '+repr(observed))
    def props(obj,names):
        rows={}
        for name in names:
            try:
                value=obj.get_editor_property(name)
                rows[name]=dict(available=True,value=value if isinstance(value,(int,float,bool,str)) or value is None else str(value))
            except Exception as ex:rows[name]=dict(available=False,error=str(ex))
        return rows
    def method(cls,name):
        fn=getattr(cls,name,None)
        return dict(available=fn is not None,doc=(getattr(fn,'__doc__','') or '')[:3500])
    def dirty():
        return {kind:sorted(p.get_path_name() for p in getter()) for kind,getter in (
            ('maps',U.EditorLoadingAndSavingUtils.get_dirty_map_packages),
            ('content',U.EditorLoadingAndSavingUtils.get_dirty_content_packages))}
    protected=[PROJECT/'Content/HarborCity/Maps'/n for n in (
        'L_M1_Playground.umap','L_M2_SeafrontStreet.umap','L_M5_CyberHarbor.umap','L_M5_IsekaiHarbor.umap')]
    before={str(p):sha(p) if p.exists() else None for p in protected};before_dirty=None
    try:
        check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT.resolve())
        before_dirty=dirty();R['dirty_before']=before_dirty;R['engine_version']=U.SystemLibrary.get_engine_version()
        names=('FbxImportUI','FbxFactory','AssetImportTask','StaticMeshEditorSubsystem','EditorStaticMeshLibrary',
            'EditorActorSubsystem','LevelEditorSubsystem','MaterialEditingLibrary','PostProcessSettings',
            'DirectionalLightComponent','SkyLightComponent','SkyAtmosphereComponent',
            'ExponentialHeightFogComponent','VolumetricCloudComponent')
        for name in names:
            cls=getattr(U,name,None);R['classes'][name]=dict(available=cls is not None)
        for required in ('FbxImportUI','FbxFactory','AssetImportTask','EditorActorSubsystem','LevelEditorSubsystem','MaterialEditingLibrary'):
            check('required native class '+required,R['classes'][required]['available'])
        queries={'AssetTools':['import_asset_tasks'],
            'AssetImportTask':['get_objects','is_async_import_complete'],
            'StaticMeshEditorSubsystem':['get_lod_count','get_number_verts','get_num_uv_channels','get_simple_collision_count','get_collision_complexity','get_nanite_settings'],
            'EditorActorSubsystem':['spawn_actor_from_class','get_all_level_actors'],
            'LevelEditorSubsystem':['new_level','load_level','save_current_level'],
            'MaterialEditingLibrary':['create_material_expression','connect_material_expressions','connect_material_property',
                'recompile_material','set_material_instance_scalar_parameter_value','set_material_instance_texture_parameter_value'],
            'VolumetricCloudComponent':['set_material'],
            'StaticMesh':['get_bounding_box','get_num_triangles']}
        for name,methods in queries.items():R['methods'][name]={n:method(getattr(U,name,None),n) for n in methods}
        options=U.FbxImportUI()
        values=dict(automated_import_should_detect_type=False,import_mesh=True,import_as_skeletal=False,
            mesh_type_to_import=U.FBXImportType.FBXIT_STATIC_MESH,original_import_type=U.FBXImportType.FBXIT_STATIC_MESH,
            import_materials=False,import_textures=False,import_animations=False)
        for name,value in values.items():options.set_editor_property(name,value)
        data=options.get_editor_property('static_mesh_import_data')
        geometry=dict(combine_meshes=False,auto_generate_collision=False,generate_lightmap_u_vs=False,
            build_nanite=False,import_mesh_lo_ds=True,remove_degenerates=True,import_uniform_scale=1.,
            convert_scene=True,convert_scene_unit=True,force_front_x_axis=False,
            normal_import_method=U.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
        for name,value in geometry.items():data.set_editor_property(name,value)
        R['transient_import_options']=props(options,list(values));R['transient_geometry_options']=props(data,list(geometry))
        check('import schema roundtrip',all(v['available'] for v in R['transient_import_options'].values())
              and all(v['available'] for v in R['transient_geometry_options'].values()))
        manifest=argument('M5EnvironmentManifest')
        if manifest:
            R['source_manifest']=validate_manifest(manifest);R['source_status']='MANIFEST_BYTES_VERIFIED_IMPORT_NOT_RUN'
            R['unsubmitted_tasks']=[]
            for row in R['source_manifest']['meshes']:
                task=U.AssetImportTask();task.filename=row['source'];task.destination_path,task.destination_name=row['destination'].rsplit('/',1)
                task.automated=True;task.replace_existing=False;task.replace_existing_settings=False;task.save=False
                task.factory=U.FbxFactory();task.options=options
                R['unsubmitted_tasks'].append(props(task,['filename','destination_path','destination_name','automated','replace_existing','save','factory']))
            # Intentionally NO call to import_asset_tasks, create_asset, spawn or save.
        component_properties={
            'DirectionalLightComponent':['intensity','light_color','atmosphere_sun_light','source_angle','cast_shadows'],
            'SkyLightComponent':['intensity','real_time_capture','lower_hemisphere_is_black'],
            'SkyAtmosphereComponent':['atmosphere_height','multi_scattering_factor'],
            'ExponentialHeightFogComponent':['fog_density','fog_height_falloff','enable_volumetric_fog','volumetric_fog_scattering_distribution'],
            'VolumetricCloudComponent':['layer_bottom_altitude','layer_height','material'],
            'PostProcessSettings':['override_auto_exposure_method','auto_exposure_method','auto_exposure_apply_physical_camera_exposure',
                'auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias','bloom_intensity','motion_blur_amount']}
        R['scene_schema']={}
        for name,fields in component_properties.items():
            cls=getattr(U,name,None)
            if cls:
                try:R['scene_schema'][name]=props(cls(),fields)
                except Exception as ex:R['scene_schema'][name]={'construction_error':str(ex)}
        mesh_editor=None
        if hasattr(U,'StaticMeshEditorSubsystem'):
            try:mesh_editor=U.get_editor_subsystem(U.StaticMeshEditorSubsystem)
            except Exception as ex:R['static_mesh_subsystem_error']=str(ex)
        R['static_mesh_subsystem_ready']=mesh_editor is not None
        if mesh_editor is None:mesh_editor=getattr(U,'EditorStaticMeshLibrary',None)
        paths=['/Engine/BasicShapes/Plane','/Game/HarborCity/M2/Geometry/SM_M2_ContinuousLand',
            '/Game/HarborCity/M2/Foliage/tree_small_02/SM_tree_small_02',
            '/Game/HarborCity/M2/Materials/M_M2_CoastalWater']
        R['existing_read_only_catalog']=[]
        for path in paths:
            obj=U.load_asset(path);row={'path':path,'available':obj is not None,'reuse_as_final_style':'NOT_APPROVED'}
            if obj:
                row['class']=obj.get_class().get_path_name()
                if isinstance(obj,U.StaticMesh):
                    box=obj.get_bounding_box();row['bounds_cm']={k:[float(getattr(getattr(box,k),axis)) for axis in ('x','y','z')] for k in ('min','max')}
                    row['material_slots']=[str(s.material_slot_name) for s in obj.get_editor_property('static_materials')]
                    if mesh_editor:
                        try:
                            lods=mesh_editor.get_lod_count(obj);row['lod_count']=lods
                            row['triangles']=[obj.get_num_triangles(i) for i in range(lods)]
                            row['source_uv_channels']=[mesh_editor.get_num_uv_channels(obj,i) for i in range(lods)]
                            row['simple_collision_count']=mesh_editor.get_simple_collision_count(obj)
                        except Exception as ex:row['mesh_read_error']=str(ex)
            R['existing_read_only_catalog'].append(row)
        R['dirty_after']=dirty();check('dirty package sets unchanged',R['dirty_after']==before_dirty)
        check('old and proposed maps unchanged',all((sha(p) if p.exists() else None)==before[str(p)] for p in protected))
        R['status']='PASS';R['pass_scope']='Read-only API/schema/catalog probe only. Source import, 50m authoring and art acceptance NOT_RUN.'
    except Exception:
        R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
    finally:
        R['protected_map_sha256_before']=before
        R['protected_map_sha256_after']={str(p):sha(p) if p.exists() else None for p in protected}
        write()
    if R['status']!='PASS':raise RuntimeError('Environment read-only probe failed; see evidence')

if __name__=='__main__':
    try:import unreal
    except ImportError:
        parser=argparse.ArgumentParser();parser.add_argument('--validate-manifest',required=True)
        print(json.dumps(validate_manifest(parser.parse_args().validate_manifest),ensure_ascii=False,indent=2))
    else:native_main(unreal)
