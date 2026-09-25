"""Native VRM4U single-process J/T/U/V/W/X import and inspection, confined to new M5VS2/NPC assets.

Wrapper phases: NPCImportR2J/T/U/V/W/X, optional Retry; explicit NPCImportR2WFullBody/XFullBody.
Exactly one model per process. Already-owned assets with
the same source hash are inspected again without reimporting/overwriting them.
This creates no game actor/map and does not claim animation or visual acceptance.
"""
from pathlib import Path
import datetime
import hashlib
import json
import math
import re
import struct
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
SOURCES = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/Models')
ROOT = '/Game/HarborCity/M5VS2/NPC'
OWNER = 'HarborCity_M5_VS2_NPC_OfficialSamples'
OWNER_KEY = 'HarborCityOwnedBy'
SOURCE_KEY = 'HarborCitySourceSHA256'
TERMS = 'https://vroid.pixiv.help/hc/en-us/articles/4402394424089-VRoidPreset-A-Z'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
command = U.SystemLibrary.get_command_line()
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', command)
assert len(args) == 1, 'Exactly one M5EvidenceDir required'
OUT = Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to(DOC.resolve()), 'Evidence outside authorized root'
OUT.mkdir(parents=True, exist_ok=True)
assert not (OUT / 'author_result.json').exists(), 'Fresh evidence directory required'
PROTECTED={}
SYNC_COMPILE_PREVIOUS=None
SYNC_COMPILE_VARIABLE='Editor.AsyncSkinnedAssetCompilation'
SYNC_COMPILE_FINISH='Editor.AsyncSkinnedAssetCompilationFinishAll'
R = dict(status='RUNNING', utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         checks=[], models=[], runtime='NOT_RUN', visual='NOT_RUN_NULLRHI',
         npc_binding='NOT_RUN_IMPORT_PROBE_ONLY', shader_compilation='NOT_RUN_NULLRHI',
         license_authority=dict(official_source_terms=TERMS, local_export_metadata='Editable in Studio; recorded literally, not an independent grant of rights.',
             distinction='Official A-Z source terms remain authoritative. Metadata flags do not override third-party source terms.',
             known_restriction='Official terms prohibit glorifying/promoting extreme ideology or antisocial acts; no explicit violence permission clause. Do not infer consent from editable export flags.',
             license_review='Source use is based on official A-Z terms, not user-editable metadata. Ordinary fictional combat interpretation was accepted by user on 2026-09-25; import/appearance author PASS is not combat authorization.', violence='NOT_EXPLICIT_IN_SOURCE_TERMS_USER_ACCEPTED_ORDINARY_FICTIONAL_COMBAT'),
         limitations=['No old material, skeleton, animation, Blueprint, map or default-map edit.',
             'Native imported physics is a candidate; collision, ragdoll and vehicle hit testing NOT_RUN.',
             'No direct animation reuse across incompatible skeletons.',
             'Selestia ExpressionComponent/LookGraph use Selestia-specific morph/bone names; Q/R require actual VRM group-to-morph and humanoid mappings.'])


def serialize(v):
    if isinstance(v, U.Object):
        return v.get_path_name()
    return str(v)


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, default=serialize, allow_nan=False), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    dump()
    if not ok:
        raise RuntimeError(name + ': ' + str(observed))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def source_info(path):
    size = path.stat().st_size
    check('bounded VRM source', 20 < size <= 1024 ** 3, size)
    with path.open('rb') as stream:
        magic, version, total = struct.unpack('<4sII', stream.read(12))
        check('valid exact GLB header', magic == b'glTF' and version == 2 and total == size)
        length, kind = struct.unpack('<I4s', stream.read(8))
        check('bounded embedded JSON', kind == b'JSON' and 0 < length <= min(size - 20, 64 * 1024 ** 2))
        gltf = json.loads(stream.read(length).decode('utf-8'))
    # No external file/network resources can escape the inspected embedded GLB.
    uris = [x['uri'] for group in ('buffers', 'images') for x in gltf.get(group, []) if 'uri' in x]
    check('self-contained VRM resources', all(x.startswith('data:') for x in uris), uris)
    ext = gltf.get('extensions', {})
    if 'VRM' in ext:
        v = ext['VRM']; version = 'VRM0'
        human = {x['bone']: x['node'] for x in v.get('humanoid', {}).get('humanBones', [])}
        expressions = v.get('blendShapeMaster', {}).get('blendShapeGroups', [])
        spring_count = len(v.get('secondaryAnimation', {}).get('boneGroups', []))
    elif 'VRMC_vrm' in ext:
        v = ext['VRMC_vrm']; version = 'VRM1'
        human = {k: x['node'] for k, x in v.get('humanoid', {}).get('humanBones', {}).items()}
        expressions = v.get('expressions', {})
        spring_count = len(ext.get('VRMC_springBone', {}).get('springs', []))
    else:
        raise RuntimeError('No VRM extension')
    return dict(path=str(path), bytes=size, sha256=sha(path), schema=version, raw_metadata=v.get('meta', {}),
                humanoid={k: dict(node=i, node_name=gltf['nodes'][i].get('name')) for k, i in human.items()},
                expressions=expressions, spring_group_count=spring_count,
                material_names=[m.get('name') for m in gltf.get('materials', [])],
                embedded_texture_count=len(gltf.get('textures', [])), external_resources_fetched=False)


def mtoon_enum():
    # Use reflected entries rather than guessing Python's acronym/prefix conversion.
    names = [n for n in dir(U.VRMImportMaterialType)
             if re.sub('[^a-z0-9]', '', n.lower()) in ('mtoon', 'vrmimtmtoon')]
    check('unique reflected MToon Lit enum', len(names) == 1, names)
    return getattr(U.VRMImportMaterialType, names[0])


def import_options():
    options = U.ImportOptionData()
    fields = dict(material_type=mtoon_enum(), model_scale=1.0, animation_translate_scale=1.0,
        play_rate_scale=1.0, remove_root_bone_rotation=True, remove_root_bone_position=False,
        force_original_bone_name=False, generate_humanoid_renamed_mesh=False,
        generate_ik_bone=False, skip_physics=False, skip_retargeter=True, skip_morph_target=False,
        remove_blend_shape_group_prefix=False, force_opaque=False, force_two_sided=False,
        use_ue5_material=True, generate_outline_material=True, mipmap_generate_mode=True,
        merge_material=False, merge_primitive=False, remove_degenerate_triangles=False,
        ignore_vrm_validation=False, debug_one_bone=False, debug_no_mesh=False, debug_no_material=False)
    for key, v in fields.items():
        options.set_editor_property(key, v)
        check('explicit import option ' + key, options.get_editor_property(key) == v)
    R['import_options'] = {k: str(options.get_editor_property(k)) for k in fields}
    R['import_options_hidden_native_defaults'] = {
        'Skeleton': 'nullptr in FImportOptionData native declaration; Python-protected, not assigned. No existing skeleton is supplied.'}
    R['import_api'] = 'VrmImporterBPFunctionLibrary.import_vrm_file_with_options; invokes native VRM4UImporterFactory with ProvidedImportOptions, without its import dialog.'
    return options


def configure_synchronous_skinned_import():
    """Process-local deterministic import; no config or project settings saved.

    Names are registered by local 5.8 SkinnedAssetCompiler.cpp:25 and
    AsyncCompilationHelpers.cpp:364-389. FinishAll's delegate calls the actual
    FSkinnedAssetCompilingManager::FinishAllCompilation. This addresses a
    candidate timing problem; a successful import alone is not root-cause proof.
    """
    global SYNC_COMPILE_PREVIOUS
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()
    raw=U.SystemLibrary.get_console_variable_string_value(SYNC_COMPILE_VARIABLE)
    check('actual registered SkinnedAsset compilation CVar',raw in ('0','1','2'),raw)
    SYNC_COMPILE_PREVIOUS=int(raw)
    R['skinned_compilation_control']=dict(variable=SYNC_COMPILE_VARIABLE,previous=SYNC_COMPILE_PREVIOUS,
        requested_process_value=0,finish_command=SYNC_COMPILE_FINISH,scope='THIS_NATIVE_IMPORT_PROCESS_ONLY',
        rationale='Bounded synchronous build/readback experiment for observed morph_count=0; root cause NOT_ESTABLISHED.',
        local_definition='E:/UE_5.8/Engine/Source/Runtime/Engine/Private/SkinnedAssetCompiler.cpp:25 and AsyncCompilationHelpers.cpp:364')
    U.SystemLibrary.execute_console_command(world,SYNC_COMPILE_FINISH)
    U.SystemLibrary.execute_console_command(world,SYNC_COMPILE_VARIABLE+' 0')
    check('actual synchronous skinned compilation readback',U.SystemLibrary.get_console_variable_int_value(SYNC_COMPILE_VARIABLE)==0)


def finish_skinned_compilation():
    U.SystemLibrary.execute_console_command(U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world(),SYNC_COMPILE_FINISH)


def package(obj):
    return obj.get_path_name().split('.')[0]


def disk(pkg):
    return PROJECT / 'Content' / Path(pkg.removeprefix('/Game/')).with_suffix('.uasset')


def material_info(mat):
    row = dict(path=mat.get_path_name(), class_name=mat.get_class().get_name(), parameters={})
    if isinstance(mat, U.MaterialInstanceConstant):
        parent = mat.get_editor_property('parent')
        row['parent'] = parent.get_path_name() if parent else None
        for kind in ('scalar', 'vector', 'texture', 'static_switch'):
            result = {}
            for name in getattr(E, 'get_' + kind + '_parameter_names')(mat):
                value = getattr(E, 'get_material_instance_' + kind + '_parameter_value')(mat, name)
                if kind == 'vector': value = [value.r, value.g, value.b, value.a]
                elif kind == 'texture': value = value.get_path_name() if value else None
                result[str(name)] = value
            row['parameters'][kind] = result
    return row


def mesh_info(mesh):
    skeleton = mesh.get_editor_property('skeleton')
    check('native independent skeletal mesh and skeleton', isinstance(mesh, U.SkeletalMesh) and isinstance(skeleton, U.Skeleton))
    bounds = mesh.get_bounds()
    origin, extent = vec(bounds.origin), vec(bounds.box_extent)
    check('finite positive imported dimensions', all(math.isfinite(x) for x in origin + extent) and all(x > 0 for x in extent))
    modifier = U.SkeletonModifier()
    check('read-only mesh reference modifier', modifier.set_skeletal_mesh(mesh))
    bones = []
    for name in modifier.get_all_bone_names():
        local = modifier.get_bone_transform(name, False)
        component = modifier.get_bone_transform(name, True)
        bones.append(dict(name=str(name), parent=str(modifier.get_parent_name(name)),
                          local_translation_cm=vec(local.translation), component_translation_cm=vec(component.translation)))
    # No commit_skeleton_to_skeletal_mesh call: the modifier is inspection only.
    morphs = [m.get_name() for m in mesh.get_editor_property('morph_targets')]
    editor = U.get_editor_subsystem(U.SkeletalMeshEditorSubsystem)
    slots = [dict(index=i, name=str(s.material_slot_name), material=s.material_interface.get_path_name() if s.material_interface else None)
             for i, s in enumerate(mesh.get_editor_property('materials'))]
    physics = mesh.get_editor_property('physics_asset')
    return dict(path=mesh.get_path_name(), skeleton=skeleton.get_path_name(), physics_asset=physics.get_path_name() if physics else None,
        bounds_origin_cm=origin, bounds_extent_cm=extent, reference_height_cm=2 * extent[2],
        reference_bones=bones, morph_target_names=morphs, material_slots=slots,
        lods=[dict(lod=i, vertices=editor.get_num_verts(mesh, i), sections=editor.get_num_sections(mesh, i))
              for i in range(editor.get_lod_count(mesh))])


def meta_info(meta):
    groups = []
    for group in meta.get_editor_property('blend_shape_group'):
        binds = []
        for bind in group.get_editor_property('blend_shape'):
            binds.append({k: bind.get_editor_property(k) for k in ('morph_target_name', 'node_name', 'mesh_name', 'mesh_id', 'shape_index', 'weight')})
        groups.append(dict(name=group.get_editor_property('name'), binds=binds,
            is_binary=group.get_editor_property('is_binary'),
            override_blink=group.get_editor_property('override_blink'),
            override_look_at=group.get_editor_property('override_look_at'),
            override_mouth=group.get_editor_property('override_mouth')))
    spring1 = meta.get_editor_property('vrm1_spring_bone_meta')
    return dict(path=meta.get_path_name(),
        humanoid={str(k): str(v) for k, v in meta.get_editor_property('humanoid_bone_table').items()},
        expression_groups=groups, vrm0_spring_groups=len(meta.get_editor_property('vrm_spring_meta')),
        vrm0_collider_groups=len(meta.get_editor_property('vrm_collider_meta')),
        vrm1_spring_groups=len(spring1.get_editor_property('springs')))


def import_model(letter, options, explicit_retry=False, full_body=False):
    check('FullBody export is exact W/X opt-in', not full_body or (letter in ('W','X') and not explicit_retry))
    source = SOURCES / ('AvatarSample_' + letter) / ('AvatarSample_' + letter + '_Studio2140' + ('_FullBody' if full_body else '') + '.vrm')
    check('source file exists ' + letter, source.is_file(), str(source))
    info = source_info(source)
    destination = ROOT + '/AvatarSample_' + letter + '/Source_' + info['sha256'][:12]
    existing = list(A.list_assets(destination, True, False))
    existing_disk = list((PROJECT / 'Content' / destination.removeprefix('/Game/')).rglob('*.uasset'))
    existing_objects = [A.load_asset(p) for p in existing]
    fully_owned = bool(existing_objects) and all(o and A.get_metadata_tag(o, OWNER_KEY) == OWNER
        and A.get_metadata_tag(o, SOURCE_KEY) == info['sha256'] for o in existing_objects)
    if explicit_retry or ((existing or existing_disk) and not fully_owned):
        # A native crash may have saved only part of the import before ownership tags.
        # Preserve it, including any user files; use a fresh bounded sibling namespace.
        suffix = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:10]
        previous = destination
        destination += '_Retry_' + suffix
        R.setdefault('preserved_partial_imports', []).append(dict(sample=letter,
            preserved_namespace=previous, retry_namespace=destination,
            reason='Explicit new-process retry; previous import and missing-exit evidence preserved' if explicit_retry else 'Partial unowned output preserved',
            registry_assets=len(existing), disk_assets=len(existing_disk)))
        existing = list(A.list_assets(destination, True, False))
        existing_disk = list((PROJECT / 'Content' / destination.removeprefix('/Game/')).rglob('*.uasset'))
        check('retry namespace is fresh', not existing and not existing_disk, destination)
    model = dict(sample=letter, source=info, destination=destination, assets_saved=[], native={},
                 source_variant='EXPLICIT_FULLBODY_EXPORT' if full_body else 'ORIGINAL_EXPORT')
    R['models'].append(model); dump()
    if existing:
        objects = [A.load_asset(p) for p in existing]
        check('every existing asset is owned with same source', all(o and A.get_metadata_tag(o, OWNER_KEY) == OWNER
              and A.get_metadata_tag(o, SOURCE_KEY) == info['sha256'] for o in objects))
        lists = [o for o in objects if isinstance(o, U.VrmAssetListObject)]
        check('one existing native asset list', len(lists) == 1)
        imported = lists[0]
        model['operation'] = 'READ_ONLY_REINSPECTION'
    else:
        check('fresh isolated import folder on disk', not existing_disk, [str(p) for p in existing_disk])
        imported = U.VrmImporterBPFunctionLibrary.import_vrm_file_with_options(
            str(source), destination + '/ImportDestination', options)
        check('native VRM asset list returned', isinstance(imported, U.VrmAssetListObject))
        objects = [A.load_asset(p) for p in A.list_assets(destination, True, False)]
        model['operation'] = 'NATIVE_VRM4U_IMPORT'
    check('returned asset list inside isolated namespace', package(imported).startswith(destination + '/'), imported.get_path_name())
    normalization=dict(U.VrmImporterBPFunctionLibrary.normalize_harbor_city_npc_asset_list_package(imported,False))
    model['asset_list_package_readback']=normalization
    check('native AssetList package reference already normalized without resave',normalization.get('status')=='PASS' and
        normalization.get('needs_repair')=='false' and normalization.get('package_reference_after')==normalization.get('actual_outer_package'),normalization)
    # Include direct outputs in addition to registered assets; no imported dependency is saved outside destination.
    references = [imported]
    for key in ('skeletal_mesh', 'vrm_meta_object', 'vrm_license_object', 'vrm1_license_object', 'humanoid_rig', 'pose_body', 'pose_face'):
        obj = imported.get_editor_property(key)
        if obj: references.append(obj)
    for key in ('textures', 'materials', 'outline_materials'):
        references.extend(imported.get_editor_property(key))
    mesh = imported.get_editor_property('skeletal_mesh')
    check('output skeletal mesh', isinstance(mesh, U.SkeletalMesh))
    model['morph_compile_readback']=dict(immediately_after_import=len(mesh.get_editor_property('morph_targets')))
    finish_skinned_compilation()
    model['morph_compile_readback']['after_native_finish_all']=len(mesh.get_editor_property('morph_targets'))
    dump()
    references += [mesh.get_editor_property('skeleton'), mesh.get_editor_property('physics_asset')]
    objects = {o.get_path_name(): o for o in objects + references if o and package(o).startswith(destination + '/')}
    check('native output set is nonempty', len(objects) >= 5, len(objects))
    check('output mesh and skeleton are isolated', all(package(o).startswith(destination + '/') for o in (mesh, mesh.get_editor_property('skeleton'))))
    model['native']['asset_list'] = imported.get_path_name()
    model['native']['mesh'] = mesh_info(mesh)
    model['morph_compile_readback']['after_native_mesh_skeleton_inspection']=len(mesh.get_editor_property('morph_targets'))
    meta = imported.get_editor_property('vrm_meta_object')
    check('native VRM humanoid/expression metadata exists', meta is not None)
    model['native']['meta'] = meta_info(meta)
    native_bones = {x['name'] for x in model['native']['mesh']['reference_bones']}
    required = {'hips', 'spine', 'head', 'leftUpperArm', 'leftLowerArm', 'leftHand', 'rightUpperArm', 'rightLowerArm',
                'rightHand', 'leftUpperLeg', 'leftLowerLeg', 'leftFoot', 'rightUpperLeg', 'rightLowerLeg', 'rightFoot'}
    mapped = model['native']['meta']['humanoid']
    check('required humanoid mappings resolve to imported bones', required.issubset(mapped) and all(mapped[k] in native_bones for k in required))
    check('native facial morphs and expression groups exist ' + letter,
        bool(model['native']['mesh']['morph_target_names']) and bool(model['native']['meta']['expression_groups']),
        dict(morph_count=len(model['native']['mesh']['morph_target_names']),
             group_count=len(model['native']['meta']['expression_groups'])))
    model['native']['materials'] = [material_info(m) for m in imported.get_editor_property('materials')]
    model['native']['textures'] = [dict(path=t.get_path_name(), srgb=t.get_editor_property('srgb'),
        compression=str(t.get_editor_property('compression_settings')), virtual_texture_streaming=t.get_editor_property('virtual_texture_streaming'))
        for t in imported.get_editor_property('textures')]
    model['native']['translucent_flags'] = list(imported.get_editor_property('material_flag_translucent'))
    model['native']['two_sided_flags'] = list(imported.get_editor_property('material_flag_two_sided'))
    if model['operation'] == 'NATIVE_VRM4U_IMPORT':
        # Save only newly-created isolated assets after all critical inspection succeeds.
        for obj in objects.values():
            check('save confinement', package(obj).startswith(destination + '/'))
            A.set_metadata_tag(obj, OWNER_KEY, OWNER)
            A.set_metadata_tag(obj, SOURCE_KEY, info['sha256'])
            A.set_metadata_tag(obj, 'HarborCityOfficialSourceTerms', TERMS)
            check('native save ' + obj.get_name(), A.save_loaded_asset(obj, False))
            path = disk(package(obj))
            check('native asset exists on disk', path.is_file(), str(path))
            model['assets_saved'].append(dict(path=obj.get_path_name(), class_name=obj.get_class().get_name(), bytes=path.stat().st_size, sha256=sha(path)))
    model['reload_readback'] = A.load_asset(imported.get_path_name()).get_path_name()
    check('source VRM unchanged ' + letter, sha(source) == info['sha256'])
    model['status'] = 'PASS'
    dump()


try:
    phase = re.findall(r'(?:^|\s)-M5AuthorPhase=(\S+)', command)
    phase_match = re.fullmatch(r'NPCImportR2([JTUVWX])(Retry|FullBody)?', phase[0]) if len(phase) == 1 else None
    check('supported import phase', phase_match is not None, phase)
    check('FullBody phase only for W/X',phase_match.group(2)!='FullBody' or phase_match.group(1) in ('W','X'),phase)
    R['phase']=phase[0]
    license_file=DOC/'research/NPC_CAST_R2_LICENSE_LEDGER.json'
    check('exact user decision ledger',sha(license_file)=='30aab61772bb44c0de26c40016d6e1a2b525e714404ee435b96c5a8f17cb55ca')
    license_data=json.loads(license_file.read_text(encoding='utf-8'))
    check('ordinary fictional combat interpretation accepted separately',license_data['user_decision']['status']=='ACCEPTED')
    R['license_ledger']=dict(path=str(license_file),sha256=sha(license_file),user_decision=license_data['user_decision'],source_violence_clause=license_data['fictional_violence'])
    check('exact project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT.resolve())
    check('no PIE', not U.EditorLevelLibrary.get_pie_worlds(False))
    check('no dirty existing map', not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT], force_rescan=True)
    # Existing independent models and plugin material templates are read-only.
    # A separate process and preserved-byte guard expose cross-model contamination.
    for base in (PROJECT/'Content/HarborCity/M5VS2/NPC', PROJECT/'Plugins/VRM4U/Content'):
        for file in base.rglob('*.uasset'):
            PROTECTED[str(file.resolve())]=sha(file)
    for file in (PROJECT/'Config').glob('*.ini'):PROTECTED[str(file.resolve())]=sha(file)
    R['protected_sha256_before']=dict(PROTECTED)
    R['importer_template']=dict(path=str(WORK/'tools/ue_m5_vs2_npc_import_probe.py'),sha256='3e4de85d8d0b7e403bfd3bb117af869a7b3cd15b381ceba369584c5e8486a6a6')
    check('preserved source importer exact reviewed version',sha(WORK/'tools/ue_m5_vs2_npc_import_probe.py')==R['importer_template']['sha256'])
    configure_synchronous_skinned_import()
    config = PROJECT / 'Config/DefaultEngine.ini'
    before_config = sha(config)
    for letter in (phase_match.group(1),):
        import_model(letter, import_options(), explicit_retry=phase_match.group(2)=='Retry',full_body=phase_match.group(2)=='FullBody')
        model=R['models'][-1]
        imported_morphs=set(model['native']['mesh']['morph_target_names'])
        groups=model['native']['meta']['expression_groups']
        for group in groups:
            if group['name']=='Neutral':continue
            check('each model expression uses its own actual morphs '+str(group['name']),
                all(b['morph_target_name'] in imported_morphs for b in group['binds']))
        check('minimum blink and three expression groups present',
            any('blink' in str(g['name']).lower() and g['binds'] for g in groups) and len([g for g in groups if g['name'] in ('Angry','Fun','Joy','Sorrow','Surprised') and g['binds']])>=3)
        prefix=model['destination']+'/'
        check('mesh and skeleton are in this models independent namespace',all(model['native']['mesh'][k].split('.')[0].startswith(prefix) for k in ('path','skeleton')))
        R['source_flags_unmodified']=True
        R['combat_runtime']='NOT_RUN_IMPORT_ONLY_USER_ACCEPTANCE_RECORDED'

    check('default map/project config unchanged', sha(config) == before_config)
    R['npc_integration_next'] = dict(native_base='/Script/HarborCity.HCM3NPC',
        existing_locomotion='/Game/HarborCity/M4/Animation/ABP_M4_Player',
        required_before_binding=['Retarget an independent locomotion/reaction set to the actual imported skeleton; do not bind incompatible old AnimBP.',
            'Map VRM expression groups to the native morphTargetName/weight pairs in this report; do not call group names as morph names.',
            'Map humanoid head/leftEye/rightEye from metadata before any look-at graph.',
            'AHCM3NPC BeginPlay ApplyNPCVisuals expects old six named outfit slots; new Q/R BP needs an opt-in native bypass before use with an M3Experience.',
            'Validate scale, capsule alignment, facial appearance, spring motion and physics in real rendering before street-corner acceptance.'])
    R['status'] = 'PASS'
    R['pass_scope'] = 'Native isolated VRM import/save plus model/material/bone/expression readback only. Gameplay, animation retarget, shader readiness and visual acceptance NOT_RUN.'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    if SYNC_COMPILE_PREVIOUS is not None:
        try:
            finish_skinned_compilation()
            U.SystemLibrary.execute_console_command(U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world(),
                SYNC_COMPILE_VARIABLE+' '+str(SYNC_COMPILE_PREVIOUS))
            restored=U.SystemLibrary.get_console_variable_int_value(SYNC_COMPILE_VARIABLE)
            R['skinned_compilation_control']['restored_process_value']=restored
            if restored!=SYNC_COMPILE_PREVIOUS:R['status']='FAIL'
        except Exception:
            R['status']='FAIL'; R['skinned_compilation_restore_error']=traceback.format_exc()
    R['preservation']=dict(files_checked=len(PROTECTED),changed_files=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h])
    if R['preservation']['changed_files']:R['status']='FAIL'
    R['ended_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('NPC VRM import probe failed; see author_result.json')
