"""Bounded native Village template probe / dependency plan / explicit selected copy.

Runs in HarborCity commandlet with the compiled HCM5VS2VillageEditor helper.
The exact external source pack is mounted transiently by native code. No worlds,
actors, construction scripts, external scripts or source assets are executed/saved.
Probe is default. Plan requires a reviewed 3-4 static house assembly manifest. Copy additionally
requires an unchanged successful Plan report. Root owns all UE execution.
"""
from pathlib import Path
import copy
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
DOC = WORK / 'docs/HarborCity_M5_VS2'
PROJECT = WORK / 'HarborCity'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Environment/HC_VillageSource').resolve()
PACK = '/Game/Fantastic_Village_Pack'
CATALOG_DIR = DOC / 'editor_runtime/probe_20260924_012031_241_8abe29f0_VillageReadOnly'
CATALOG_SHA = 'dfe544a9a7ee512cdefdf0a4ebca4ef957140adf62dfbf21a666c8b605479dde'
cmd = U.SystemLibrary.get_command_line()


def arg(name, default=None):
    found = re.findall(r'(?:^|\s)-' + re.escape(name) + r'=(?:"([^"]+)"|(\S+))', cmd)
    if not found:
        return default
    assert len(found) == 1, 'Duplicate flag: ' + name
    return found[0][0] or found[0][1]


OUT = Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(DOC) and not (OUT / 'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
MODE = arg('M5VillageMode', 'Probe')
R = dict(status='RUNNING', mode=MODE, scope='VILLAGE_NATIVE_TEMPLATES_AND_SELECTED_CLOSURE',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat(), checks=[],
         source_assets_saved=0, worlds_created=0, actors_spawned=0,
         construction_scripts_executed=False, visual='NOT_RUN_NULLRHI', runtime_collision='NOT_RUN')


def write(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def dump():
    write(OUT / 'author_result.json', R)


def check(name, ok, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if ok else 'FAIL', observed=observed))
    dump()
    if not ok:
        raise RuntimeError(name + ': ' + str(observed))


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def document(flag):
    value = arg(flag)
    check('required ' + flag, value is not None)
    path = Path(value).resolve()
    check('document stays in VS2 evidence', path.is_relative_to(DOC) and path.is_file(), str(path))
    return path, json.loads(path.read_text(encoding='utf-8-sig'))


def source_snapshot():
    source_pack = SOURCE / 'Content/Fantastic_Village_Pack'
    files = sorted(p for p in source_pack.rglob('*') if p.is_file())
    assert 1 <= len(files) <= 5000, 'Source pack file count outside bound'
    files += [SOURCE / 'HC_VillageSource.uproject']
    files += sorted(p for p in (SOURCE / 'Config').rglob('*') if p.is_file())
    values = {p.relative_to(SOURCE).as_posix(): dict(bytes=p.stat().st_size, sha256=sha(p)) for p in files}
    digest = hashlib.sha256(json.dumps(values, sort_keys=True).encode()).hexdigest()
    return values, digest


def geometry(house):
    """Exact native SM templates only; no component from a child Actor is read or executed."""
    components = house['components']
    by_name = {c['variable']: c for c in components}
    assert len(by_name) == len(components), 'Duplicate source component variable'
    rows = []
    for c in components:
        if c['class'] != '/Script/Engine.StaticMeshComponent':
            continue
        assert c.get('static_mesh', 'None') != 'None', 'Null mesh in selected house'
        ancestor = c
        visited = set()
        while ancestor['parent_variable']:
            name = ancestor['parent_variable']
            assert name not in visited and name in by_name, 'Broken/cyclic template parent chain'
            visited.add(name); ancestor = by_name[name]
            assert ancestor['class'] in ('/Script/Engine.SceneComponent', '/Script/Engine.StaticMeshComponent'), 'Static geometry inherits an excluded component'
        rows.append({k: copy.deepcopy(c[k]) for k in ('variable', 'static_mesh', 'template_to_actor_transform', 'effective_materials', 'mesh_local_bounds_cm', 'template_actor_bounds_cm')})
    assert rows, 'No static geometry'
    return dict(source_blueprint=house['path'], bounds_cm=house['template_assembly_bounds_cm'],
                components=rows, excluded_components=[dict(variable=c['variable'], class_name=c['class']) for c in components if c['class'] != '/Script/Engine.StaticMeshComponent'])


# Every candidate below exists in the actual PASS catalog. Visual suitability and
# door traversal remain untested. Complete houses are selected only after SCS readback.
CANDIDATES = [
    ('door', 'meshes/buildings/SM_BLD_door_v04_01'),
    ('window', 'meshes/buildings/SM_BLD_window_v06_01'),
    ('window', 'meshes/buildings/SM_BLD_window_v09_01'),
    ('wood_railing', 'meshes/props/construction/SM_PROP_fence_v02_01'),
    ('stone_quay_edge', 'meshes/props/construction/SM_PROP_wall_stone_small_01'),
    ('short_timber_bridge', 'meshes/props/construction/SM_PROP_bridge_wood_01'),
    ('dock_board_cluster', 'meshes/props/construction/SM_PROP_planks_02'),
    ('market_canopy_a', 'meshes/props/construction/SM_PROP_market_v01_01'),
    ('market_canopy_b', 'meshes/props/construction/SM_PROP_market_v03_01'),
    ('market_shelf', 'meshes/props/furniture/SM_PROP_market_shelf_01'),
    ('barrel', 'meshes/props/container/SM_PROP_barrel_01'),
    ('crate', 'meshes/props/container/SM_PROP_crate_01'),
    ('sack', 'meshes/props/food/SM_PROP_sack_01'),
    ('bench', 'meshes/props/furniture/SM_PROP_bench_01'),
    ('signpost', 'meshes/props/deco/SM_PROP_signpost_01'),
    ('streetlamp_geometry', 'meshes/props/light/SM_PROP_streetlamp_v01_01'),
    ('window_planter', 'meshes/props/natural/SM_PROP_PLANT_pot_v1_01'),
    ('planter', 'meshes/props/natural/SM_PROP_PLANT_pot_v2_01'),
    ('tree', 'meshes/environment/SM_ENV_TREE_village_LOD0'),
    ('grass', 'meshes/environment/SM_ENV_PLANT_grass_village'),
    ('leaf_patch', 'meshes/environment/SM_ENV_PLANT_leaf_village'),
    ('original_water_material', 'materials/MI_ENV_water'),
    ('original_day_sky_material', 'materials/MI_skybox_day'),
    ('original_night_sky_material', 'materials/MI_skybox_night'),
    ('stone_surface', 'materials/MI_stonebrick_01'),
    ('moored_rowboat_geometry', 'meshes/props/vehicles/SM_PROP_rowboat'),
    ('boat_paddle_geometry', 'meshes/props/vehicles/SM_PROP_rowboat_paddle_01'),
    ('square_landmark_well_geometry', 'meshes/props/deco/SM_PROP_well'),
    ('well_handle_geometry', 'meshes/props/deco/SM_PROP_well_handle'),
    ('cafe_table', 'meshes/props/furniture/SM_PROP_table_03'),
    ('cafe_chair', 'meshes/props/furniture/SM_PROP_chair_03'),
    ('cup', 'meshes/props/deco/SM_PROP_cup'),
    ('wood_plate', 'meshes/props/deco/SM_PROP_plate_wood'),
    ('wood_bowl', 'meshes/props/food/SM_PROP_bowl_wood_01'),
]

before = None
try:
    dump()
    check('exact HarborCity host', Path(U.Paths.project_dir()).resolve() == PROJECT)
    check('supported explicit mode', MODE in ('Probe', 'Plan', 'Copy'), MODE)
    catalog_file = CATALOG_DIR / 'village_catalog.json'
    catalog = json.loads(catalog_file.read_text(encoding='utf-8-sig'))
    check('actual source catalog PASS and unchanged', catalog.get('probe_status') == 'PASS' and sha(catalog_file) == CATALOG_SHA)
    inventory = {x['path']: x for x in catalog['inventory']}
    meshes = {x['path']: x for x in catalog['static_meshes']}
    before, source_digest = source_snapshot()
    write(OUT / 'source_sha256_before.json', before)
    R['source_snapshot_sha256'] = source_digest
    R['catalog'] = dict(path=str(catalog_file), sha256=CATALOG_SHA)
    helper = getattr(U, 'HCM5VS2VillageEditor', None)
    check('compiled native helper available', helper is not None)
    if MODE == 'Probe':
        native = json.loads(helper.probe_village_houses())
        write(OUT / 'village_house_templates.json', native)
        R['native_report'] = str(OUT / 'village_house_templates.json')
        check('native SCS graph probe completed', native.get('status') == 'PASS', native.get('error'))
        rows = []
        for role, relative in CANDIDATES:
            path = PACK + '/' + relative
            check('candidate exists in actual registry catalog', path in inventory, path)
            row = dict(role=role, path=path, class_name=inventory[path]['class_name'])
            if path in meshes:
                row['bounds_cm'] = meshes[path]['bounds_cm']
                row['collision'] = meshes[path]['collision']
            rows.append(row)
        house_probe = OUT / 'village_house_templates.json'
        draft = dict(schema_version=2, selection_kind='STATIC_GEOMETRY_ONLY', status='NEEDS_HOUSE_SELECTION', revision=1,
                     house_probe=str(house_probe), house_probe_sha256=sha(house_probe),
                     source_snapshot_sha256=source_digest, houses=[], geometry_assemblies=[], candidates=rows,
                     roots=[x['path'] for x in rows],
                     note='Choose 3-4 inspected source houses, store geometry(house) records, and add only their SM packages plus necessary material overrides to roots. No Blueprint roots. Set status READY_FOR_PLAN. No art approval is inferred.')
        write(OUT / 'village_selection_draft.json', draft)
        R['selection_draft'] = str(OUT / 'village_selection_draft.json')
        R['house_summary'] = [{k: h.get(k) for k in ('path', 'safe_geometry_blueprint', 'static_mesh_component_count', 'template_assembly_bounds_cm')} for h in native['houses']]
    else:
        manifest_path, manifest = document('M5VillageManifest')
        check('reviewed static geometry selection manifest', manifest.get('schema_version') == 2 and manifest.get('selection_kind') == 'STATIC_GEOMETRY_ONLY' and manifest.get('status') == 'READY_FOR_PLAN')
        check('source unchanged since template probe', manifest.get('source_snapshot_sha256') == source_digest)
        probe_file = Path(manifest['house_probe']).resolve()
        check('same native template report', probe_file.is_relative_to(DOC) and probe_file.is_file() and sha(probe_file) == manifest['house_probe_sha256'])
        probe = json.loads(probe_file.read_text(encoding='utf-8-sig'))
        check('native template report PASS', probe.get('status') == 'PASS')
        inspected_houses = {h['path']: h for h in probe['houses'] if h.get('inert_graph_allowlist_pass')}
        roots = manifest['roots']; houses = manifest['houses']
        check('bounded unique explicit roots', 1 <= len(roots) <= 64 and len(set(roots)) == len(roots) and all(x in inventory for x in roots))
        check('3-4 inspected static house assemblies', 3 <= len(houses) <= 4 and len(set(houses)) == len(houses) and set(houses) <= set(inspected_houses))
        check('roots contain only mesh or material assets', all(inventory[x]['class_name'] in ('StaticMesh', 'Material', 'MaterialInstanceConstant') for x in roots))
        assemblies = [geometry(inspected_houses[path]) for path in houses]
        check('assembly transforms exactly match native probe', manifest.get('geometry_assemblies') == assemblies)
        required_meshes = {c['static_mesh'].split('.')[0] for a in assemblies for c in a['components']}
        check('every assembled mesh is selected', required_meshes <= set(roots))
        material_roots = {x for x in roots if inventory[x]['class_name'] in ('Material', 'MaterialInstanceConstant')}
        material_dependencies = {s['material'].split('.')[0] for x in required_meshes for s in meshes[x]['material_slots']}
        effective_materials = {m.split('.')[0] for a in assemblies for c in a['components'] for m in c['effective_materials'] if m != 'None'}
        check('all effective material overrides included', effective_materials <= material_dependencies | material_roots)
        digest = hashlib.sha256((sha(manifest_path) + source_digest).encode()).hexdigest()[:12]
        destination = '/Game/HarborCity/M5VS2/Environment/Village_' + digest
        R.update(manifest=dict(path=str(manifest_path), sha256=sha(manifest_path)), destination=destination, roots=roots,
                 selection_kind='STATIC_GEOMETRY_ONLY', source_geometry_assemblies=assemblies,
                 excluded_source_behavior='No external Blueprints, ChildActors, particles or lights copied or executed. New scene author owns lighting.')
        if MODE == 'Copy':
            plan_path, plan = document('M5VillagePlan')
            check('exact successful dry-run plan', plan.get('status') == 'PASS' and plan.get('mode') == 'Plan' and plan.get('manifest') == R['manifest'] and plan.get('destination') == destination and plan.get('source_snapshot_sha256') == source_digest)
            R['plan'] = dict(path=str(plan_path), sha256=sha(plan_path))
        native = json.loads(helper.copy_village_selection(roots, destination, MODE == 'Copy'))
        write(OUT / 'village_dependency_plan.json', native)
        R['native'] = native
        check('native selection ' + MODE, native.get('status') == 'PASS', native.get('error'))
        copied_packages = {entry['source']: entry['destination'] for entry in native['packages']}
        check('every template mesh and effective material is in the copied closure', required_meshes | effective_materials <= set(copied_packages))
        R['geometry_assemblies'] = copy.deepcopy(assemblies)
        for assembly in R['geometry_assemblies']:
            for component in assembly['components']:
                component['static_mesh'] = copied_packages[component['static_mesh'].split('.')[0]]
                component['effective_materials'] = [copied_packages[m.split('.')[0]] if m != 'None' else None for m in component['effective_materials']]
        R['geometry_paths_state'] = 'SAVED' if MODE == 'Copy' else 'PLANNED_NOT_WRITTEN'
        if MODE == 'Copy':
            R['assets'] = []
            for entry in native['packages']:
                path = entry['destination']
                disk = PROJECT / 'Content' / (path.removeprefix('/Game/') + '.uasset')
                check('saved new native package', disk.is_file(), path)
                R['assets'].append(dict(path=path, sha256=sha(disk), bytes=disk.stat().st_size, source=entry['source']))
    after, _ = source_snapshot()
    check('source config and every source pack file unchanged', after == before)
    R['status'] = 'PASS'
except Exception as exc:
    R['status'] = 'FAIL'; R['error'] = str(exc); R['traceback'] = traceback.format_exc()
finally:
    if before is not None:
        try:
            after, _ = source_snapshot()
            R['source_unchanged'] = before == after
            if before != after:
                R['status'] = 'FAIL'; R['source_guard_failure'] = 'Source files changed; preserve all evidence and stop'
        except Exception as exc:
            R['status'] = 'FAIL'; R['source_guard_failure'] = str(exc)
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat(); dump()
if R['status'] != 'PASS':
    raise RuntimeError(R.get('error', R.get('source_guard_failure', 'Village selection failed')))
