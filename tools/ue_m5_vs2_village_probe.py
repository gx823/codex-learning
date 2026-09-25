"""Read-only native catalog of the actual Launcher-created Village source project.

Run through ue_m5_vs2_village_probe_run.ps1 after the download finishes. No world
load, actor spawn, asset save/import/migration, material edit or gameplay call.
Asset names are discovery hints, never evidence of visual quality or fitness.
"""
from pathlib import Path
import collections
import csv
import datetime as dt
import hashlib
import json
import math
import re
import traceback
import unreal as U

SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Environment/HC_VillageSource').resolve()
PROJECT = SOURCE / 'HC_VillageSource.uproject'
DOC = Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2').resolve()
PACK = '/Game/Fantastic_Village_Pack'
AH = U.AssetRegistryHelpers
AR = AH.get_asset_registry()
A = U.EditorAssetLibrary
E = U.MaterialEditingLibrary
cmd = U.SystemLibrary.get_command_line()
matches = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', cmd)
assert len(matches) == 1, 'Exactly one M5EvidenceDir required'
OUT = Path(matches[0][0] or matches[0][1]).resolve()
assert OUT.is_relative_to(DOC) and not (OUT / 'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', scope='READ_ONLY_NATIVE_VILLAGE_CATALOG', checks=[], asset_writes=0,
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), visual='NOT_RUN_NULLRHI',
         migration='NOT_RUN', map_author='NOT_RUN', runtime_collision='NOT_RUN',
         download_completion='NOT_ASSERTED_BY_CATALOG; wrapper verifies source files remain unchanged')
C = dict(schema_version=1, source_namespace=PACK, inventory=[], static_meshes=[], materials=[],
         textures=[], feature_candidates={}, directory_counts={}, limitations=[
             'Names and directories are hints, not final asset selection or art approval.',
             'Asset bounds are local-space centimetres at scale 1, not verified door/player scale.',
             'Collision counts/settings do not prove traversable doorways or runtime contacts.',
             'NullRHI does not validate shaders, water appearance, wind motion, Lumen or performance.',
             'Worlds/Blueprints/effects are registry entries only; none is instantiated or opened.',
             'Texture dimensions and compression are registry metadata; pixel quality is not reviewed.'])


def dump():
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    dump()
    if not ok:
        raise RuntimeError(name + ': ' + repr(observed))


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def tag(asset, name):
    value = AH.get_tag_value(asset, U.Name(name))
    if isinstance(value, (tuple, list)) and len(value) == 2 and isinstance(value[0], bool):
        return str(value[1]) if value[0] else None
    return None if value is None else str(value)


def tags(asset, names):
    return {name: value for name in names if (value := tag(asset, name)) is not None}


def vec(value):
    result = [round(float(value.x), 4), round(float(value.y), 4), round(float(value.z), 4)]
    if not all(math.isfinite(x) for x in result):
        raise RuntimeError('Nonfinite native geometry')
    return result


def optional(fn):
    try:
        return dict(status='OBSERVED', value=fn())
    except Exception as exc:
        return dict(status='UNAVAILABLE', reason=str(exc))


def hints(path):
    # Exact tokens avoid treating watermelons/windows as water/wind assets.
    words = set(re.findall(r'[a-z]+', re.sub(r'([a-z])([A-Z])', r'\1_\2', path).lower()))
    groups = {
        'building': {'building', 'buildings', 'house', 'houses', 'wall', 'walls', 'roof', 'rooftiles', 'beam'},
        'door_window_trim': {'door', 'doors', 'window', 'windows', 'arch', 'stairs', 'stair', 'railing'},
        'street_ground': {'road', 'roads', 'pavement', 'cobble', 'cobblestone', 'path', 'ground', 'stonebrick'},
        'shop_furniture': {'table', 'chair', 'bench', 'shelf', 'counter', 'bar', 'market', 'stall', 'awning', 'sign'},
        'small_props': {'barrel', 'crate', 'box', 'cart', 'wheel', 'basket', 'bottle', 'food', 'rope', 'lamp', 'lantern'},
        'vegetation': {'tree', 'trees', 'grass', 'plants', 'plant', 'flower', 'flowers', 'bush', 'foliage'},
        'harbor': {'boat', 'boats', 'dock', 'pier', 'jetty', 'ship', 'anchor', 'pontoon'},
        'water': {'water', 'ocean', 'sea', 'wave', 'waves', 'foam', 'ripple'},
        'wind': {'wind', 'windtrail', 'flag', 'flags'},
        'sky': {'sky', 'skybox', 'cloud', 'clouds', 'atmosphere'},
    }
    return [name for name, tokens in groups.items() if words & tokens]


DEP_OPTIONS = U.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_searchable_names=False,
    include_soft_management_references=False, include_hard_management_references=False)


def dependencies(package):
    paths = sorted(set(str(x) for x in AR.get_dependencies(package, DEP_OPTIONS)))
    if len(paths) > 512:
        raise RuntimeError('Unexpected dependency fanout >512: ' + package)
    return paths


def collision(mesh, registry_tags):
    row = dict(registry_tags=registry_tags, runtime='NOT_RUN')
    # In this engine, body_setup is an EditAnywhere reflected property. Calling
    # the mesh's native geometry getters first synchronizes normal mesh reads.
    # Do not use EditorStaticMeshLibrary: its commandlet guard returns -1 here.
    def read_body():
        body = mesh.get_editor_property('body_setup')
        if body is None:
            return dict(body_setup=None, primitive_counts={})
        agg = body.get_editor_property('agg_geom')
        fields = ('sphere_elems', 'box_elems', 'sphyl_elems', 'convex_elems', 'tapered_capsule_elems',
                  'level_set_elems', 'skinned_level_set_elems', 'ml_level_set_elems', 'skinned_triangle_mesh_elems')
        counts = {field: len(agg.get_editor_property(field)) for field in fields}
        return dict(body_setup=body.get_path_name(), primitive_counts=counts,
                    primitive_total=sum(counts.values()), complexity=str(body.get_editor_property('collision_trace_flag')))
    row['native_body_setup'] = optional(read_body)
    # Missing optional access is explicit UNKNOWN, never converted into zero.
    row['catalog_state'] = 'OBSERVED' if registry_tags or row['native_body_setup']['status'] == 'OBSERVED' else 'UNKNOWN'
    return row


def inspect_mesh(asset):
    package = str(asset.package_name)
    mesh = AH.get_asset(asset)
    if not isinstance(mesh, U.StaticMesh):
        raise RuntimeError('Registry/load class mismatch: ' + package)
    box = mesh.get_bounding_box()
    lo, hi = vec(box.min), vec(box.max)
    slots = []
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        mat = slot.material_interface
        slots.append(dict(index=index, name=str(slot.material_slot_name), material=mat.get_path_name() if mat else None))
    lod_count = int(mesh.get_num_lods())
    if lod_count < 0 or lod_count > 16:
        raise RuntimeError('Invalid bounded LOD count: ' + package)
    row = dict(path=package, name=str(asset.asset_name), name_hints_only=hints(package),
               bounds_cm=dict(min=lo, max=hi, size=[round(hi[i]-lo[i], 4) for i in range(3)]),
               material_slots=slots, lods=[dict(index=i, vertices=int(mesh.get_num_vertices(i)),
                   triangles=int(mesh.get_num_triangles(i)), uv_channels=int(mesh.get_num_tex_coords(i))) for i in range(lod_count)],
               collision=collision(mesh, tags(asset, ('CollisionPrims', 'SectionsWithCollision', 'DefaultCollision', 'CollisionComplexity'))),
               dependencies=dependencies(package))
    row['nanite_enabled'] = optional(lambda: bool(mesh.get_editor_property('nanite_settings').get_editor_property('enabled')))
    C['static_meshes'].append(row)


def inspect_material(asset):
    package = str(asset.package_name)
    material = AH.get_asset(asset)
    if not isinstance(material, U.MaterialInterface):
        raise RuntimeError('Registry/load material mismatch: ' + package)
    row = dict(path=package, name=str(asset.asset_name), class_name=material.get_class().get_name(),
               name_hints_only=hints(package), dependencies=dependencies(package))
    if isinstance(material, U.MaterialInstanceConstant):
        parent = material.get_editor_property('parent')
        row['parent'] = parent.get_path_name() if parent else None
        # Native parameter APIs include inherited names. Reading does not edit MI values.
        texture_names = list(E.get_texture_parameter_names(material))
        row['texture_parameters'] = {str(n): (v.get_path_name() if (v := E.get_material_instance_texture_parameter_value(material, n)) else None)
                                     for n in texture_names[:64]}
    if isinstance(material, U.Material):
        row['blend_mode'] = str(material.get_editor_property('blend_mode'))
        row['two_sided'] = bool(material.get_editor_property('two_sided'))
    # Detail just the actual water/wind/sky candidates, keeping the main catalog small.
    if set(row['name_hints_only']) & {'water', 'wind', 'sky'}:
        row['parameter_names'] = dict(
            scalar=[str(n) for n in E.get_scalar_parameter_names(material)],
            vector=[str(n) for n in E.get_vector_parameter_names(material)],
            texture=[str(n) for n in E.get_texture_parameter_names(material)],
            static_switch=[str(n) for n in E.get_static_switch_parameter_names(material)])
    C['materials'].append(row)


def main():
    source = Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()
    check('exact authorized Village source project', source == SOURCE and PROJECT.is_file(), str(source))
    check('one fixed source descriptor', list(SOURCE.glob('*.uproject')) == [PROJECT])
    version = U.SystemLibrary.get_engine_version()
    check('locked native UE 5.8.2', version.startswith('5.8.2'), version)
    check('actual downloaded pack directory', (SOURCE / 'Content/Fantastic_Village_Pack').is_dir())
    R['source_project'] = dict(path=str(PROJECT), sha256=sha(PROJECT), engine=version)
    R['dirty_packages_before'] = [p.get_path_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    AR.scan_paths_synchronous([PACK], force_rescan=True)
    assets = sorted(AR.get_assets_by_path(PACK, recursive=True, include_only_on_disk_assets=True), key=lambda a: str(a.package_name))
    check('bounded actual pack registry', 1 <= len(assets) <= 5000, len(assets))
    meshes, materials = [], []
    dirs = collections.defaultdict(collections.Counter)
    classes = collections.Counter()
    for asset in assets:
        package, kind = str(asset.package_name), str(asset.asset_class_path.asset_name)
        if not package.startswith(PACK + '/'):
            raise RuntimeError('Registry escaped pack namespace')
        row = dict(path=package, name=str(asset.asset_name), class_name=kind)
        C['inventory'].append(row)
        dirs[str(asset.package_path)][kind] += 1
        classes[kind] += 1
        for feature in hints(package):
            C['feature_candidates'].setdefault(feature, []).append(dict(path=package, class_name=kind))
        if kind == 'StaticMesh':
            meshes.append(asset)
        elif kind in ('Material', 'MaterialInstanceConstant'):
            materials.append(asset)
        elif kind.startswith('Texture'):
            C['textures'].append(dict(**row, registry_tags=tags(asset, ('Dimensions', 'ImportedSize', 'MaxSize', 'Format', 'sRGB', 'CompressionSettings', 'LODGroup', 'HasAlphaChannel'))))
    check('bounded native mesh catalog', 1 <= len(meshes) <= 768, len(meshes))
    check('bounded native material catalog', 1 <= len(materials) <= 256, len(materials))
    C['directory_counts'] = {path: dict(sorted(count.items())) for path, count in sorted(dirs.items())}
    for index, asset in enumerate(meshes):
        inspect_mesh(asset)
        if index % 50 == 0:
            U.log('M5 Village read-only meshes: %d/%d' % (index+1, len(meshes)))
    for asset in materials:
        inspect_material(asset)
    R['class_counts'] = dict(sorted(classes.items()))
    R['counts'] = dict(registry=len(assets), static_meshes=len(C['static_meshes']), materials=len(C['materials']), textures=len(C['textures']))
    R['feature_name_hint_counts'] = {k: len(v) for k, v in sorted(C['feature_candidates'].items())}
    R['collision_unknown_meshes'] = [m['path'] for m in C['static_meshes'] if m['collision']['catalog_state'] == 'UNKNOWN']
    R['dirty_packages_after'] = [p.get_path_name() for p in U.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    R['read_only_note'] = 'No mutating asset/world API called. Loading can leave transient dirty packages; none are saved.'
    check('source descriptor unchanged', sha(PROJECT) == R['source_project']['sha256'])
    R['status'] = 'PASS'
    R['pass_scope'] = 'Native source registry/mesh/material catalog only. No import, migration, render, collision gameplay or art acceptance.'


try:
    main()
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    C['probe_status'] = R['status']
    C['source_project'] = R.get('source_project')
    catalog = OUT / 'village_catalog.json'
    catalog.write_text(json.dumps(C, ensure_ascii=False, indent=2), encoding='utf-8')
    mesh_csv = OUT / 'village_meshes.csv'
    with mesh_csv.open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['path', 'size_x_cm', 'size_y_cm', 'size_z_cm', 'lod0_triangles', 'lod0_vertices', 'lod0_uv_channels', 'collision_registry', 'material_slots', 'name_hints_only'])
        for mesh in C['static_meshes']:
            lod0 = mesh['lods'][0] if mesh['lods'] else {}
            writer.writerow([mesh['path'], *mesh['bounds_cm']['size'], lod0.get('triangles'), lod0.get('vertices'), lod0.get('uv_channels'),
                             json.dumps(mesh['collision'], ensure_ascii=False), json.dumps(mesh['material_slots'], ensure_ascii=False), ','.join(mesh['name_hints_only'])])
    R['outputs'] = dict(catalog=str(catalog), catalog_sha256=sha(catalog), meshes_csv=str(mesh_csv))
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
if R['status'] != 'PASS':
    raise RuntimeError('Village read-only probe failed; see preserved author_result.json')
