"""Fresh Hero-only MToon light-limit candidate; run by the serial native author.

No source asset, collection, mesh, actor or map setters. No texture import.
UE 5.8 MaterialEditingLibrary exposes native node connection APIs.
SetMaterialFunctionEx attempts name matching, but the actual b30b73fb run lost
ScrolledUV on a duplicated function call. Restore only proven disconnected
single-output input edges from the original snapshot, then compare every edge.
"""
from pathlib import Path
import copy
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK / 'HarborCity'
DOCS = WORK / 'docs/HarborCity_M5_VS2'
HERO = '/Game/HarborCity/M5VS2/HeroSelestia/Materials'
PLUGIN = '/VRM4U/MaterialUtil/UE5/'
ROOT = PLUGIN + 'Material/M_VrmMToonBaseOpaque'
MF_BASE = PLUGIN + 'MaterialFunction/MF_VrmMToonBase'
MF_COLOR = PLUGIN + 'MaterialFunction/MF_BaseColor'
PARENTS = [PLUGIN + 'Material/' + name for name in (
    'MI_VrmMToonBaseLitOpaque', 'MI_VrmMToonOptLitOpaque',
    'MI_VrmMToonOptLitTranslucent', 'MI_VrmMToonOptLitTranslucentTwoSided')]
SLOTS = ('hair', 'body', 'face', 'option', 'costume')
FINALS = {slot: HERO + '/MToon/MI_MToon_' + slot for slot in SLOTS}
LIMITS = {'HeroLightMin': 0.05, 'HeroLightMax': 1.0}
# Complete GetExpressionInputDescription support on the locked UE 5.8 build.
# bool = SceneTypes.h UMETA(Hidden), excluded by PyGenUtil::ShouldExportEnumEntry.
# Alias enum members and unsupported inputs (e.g. MP_CustomOutput) are not pins.
MATERIAL_INPUT_CATALOG = (
    ('MP_EmissiveColor', False), ('MP_Opacity', False), ('MP_OpacityMask', False),
    ('MP_BaseColor', False), ('MP_Metallic', False), ('MP_Specular', False),
    ('MP_Roughness', False), ('MP_Anisotropy', False), ('MP_Normal', False),
    ('MP_Tangent', False), ('MP_WorldPositionOffset', False),
    ('MP_Displacement', True), ('MP_SubsurfaceColor', False),
    ('MP_CustomData0', True), ('MP_CustomData1', True),
    ('MP_AmbientOcclusion', False), ('MP_Refraction', False),
    ('MP_MaterialAttributes', False), ('MP_PixelDepthOffset', True),
    ('MP_ShadingModel', True), ('MP_SurfaceThickness', True),
    ('MP_FrontMaterial', False),
) + tuple(('MP_CustomizedUVs' + str(i), True) for i in range(8))
OWNER = 'HarborCity_M5_VS2_MToonLightLimits'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
assert len(args) == 1, 'Exactly one -M5EvidenceDir required'
OUT = Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to((DOCS / 'editor_runtime').resolve()), 'Author evidence directory required'
OUT.mkdir(parents=True, exist_ok=True)
assert not (OUT / 'author_result.json').exists(), 'Fresh evidence directory required'
# A separate native run always gets a separate namespace, including after failure.
token = hashlib.sha256(str(OUT).encode('utf-8')).hexdigest()[:12]
DEST = HERO + '/MToonLightLimits_' + token
sources = [MF_COLOR, MF_BASE, ROOT] + PARENTS + list(FINALS.values())
paths = {p: DEST + '/' + p.rsplit('/', 1)[1] for p in sources}
R = dict(status='RUNNING', scope='NEW_HERO_MTOON_LIGHT_LIMITS_CANDIDATE_ONLY',
         target_directory=DEST, checks=[], assets=[], variants={}, runtime='NOT_RUN',
         shader_readiness='NOT_RUN_REQUIRES_NON_NULL_RHI', visual='USER_REVIEW',
         limits=LIMITS, unchanged_light_color_attenuation=0.1,
         limitations=['Not an exact lilToon shader port.',
                     'Saved native readback is not a fresh-process reload or visual acceptance.',
                     'Root material inputs hidden from Python are NOT_INSPECTED individually; '
                     'whole native duplication and source hashes do not constitute their pin readback.',
                     'Graph API reports the first matching output for a reused upstream node; '
                     'existing reused-source edges are not edited. Function replacement '
                     'is explicitly checked; a disconnected original input is restored only '
                     'when its source output and destination input are unambiguous.'])
protected, originals, copies = {}, {}, {}


def dump():
    payload = json.dumps(R, ensure_ascii=False, indent=2)
    (OUT / 'author_result.json').write_text(payload, encoding='utf-8')
    (OUT / 'mtoon_limits_author.json').write_text(payload, encoding='utf-8')


def check(name, passed, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if passed else 'FAIL', observed=observed))
    if not passed:
        dump()
        raise RuntimeError(name + ': ' + str(observed))


def require(name, passed):
    if not passed:
        check(name, False)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def disk(package):
    package = package.split('.')[0]
    for prefix, folder in (('/Game/', PROJECT / 'Content'),
                           ('/VRM4U/', PROJECT / 'Plugins/VRM4U/Content'),
                           ('/Engine/', Path('E:/UE_5.8/Engine/Content'))):
        if package.startswith(prefix):
            return folder / (package[len(prefix):] + '.uasset')
    raise RuntimeError('Unrecognized protected package mount: ' + package)


def protect(package):
    f = disk(package)
    check('source exists ' + package, f.is_file())
    protected.setdefault(str(f), sha(f))


def package(obj):
    return obj.get_path_name().split('.')[0]


def value(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, U.Object):
        return v.get_path_name()
    if isinstance(v, U.LinearColor):
        return [float(getattr(v, k)) for k in ('r', 'g', 'b', 'a')]
    return str(v)


def dirty():
    return set(p.get_path_name() for fn in (
        U.EditorLoadingAndSavingUtils.get_dirty_content_packages,
        U.EditorLoadingAndSavingUtils.get_dirty_map_packages) for p in fn())


def instance_snapshot(mi):
    params = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        names = getattr(E, 'get_' + kind + '_parameter_names')(mi)
        require('bounded material parameters', len(names) < 512)
        getter = getattr(E, 'get_material_instance_' + kind + '_parameter_value')
        params[kind] = {str(n): value(getter(mi, n)) for n in names}
    # StructBase.export_text exports every reflected override field, including
    # disabled fields; no incomplete hand-maintained list of effective overrides.
    return dict(parent=package(mi.get_editor_property('parent')), parameters=params,
                all_base_overrides=mi.get_editor_property('base_property_overrides').export_text())


def nodes(obj):
    items = (E.get_material_expressions(obj) if isinstance(obj, U.Material)
             else E.get_material_function_expressions(obj))
    require('bounded graph', len(items) < 1200)
    result = {n.get_name(): n for n in items}
    require('unique expression names', len(result) == len(items))
    return result


def links(obj, node):
    names = list(E.get_material_expression_input_names(node))
    upstreams = list(E.get_inputs_for_material_expression(obj, node) if isinstance(obj, U.Material)
                     else E.get_inputs_for_material_function_expression(obj, node))
    require('input enumeration complete ' + node.get_name(), len(names) == len(upstreams))
    return [dict(name=str(name), source=up.get_name() if up else None,
                 first_matching_output=value(E.get_input_node_output_name_for_material_expression(node, up)) if up else None)
            for name, up in zip(names, upstreams)]


def reflected_material_inputs():
    # Name normalization accommodates PythonizeName's digit/acronym underscores
    # without guessing attributes or constructing non-reflected enum integers.
    normalized = lambda name: name.replace('_', '').upper()
    reflected = {}
    for name in sorted(dir(U.MaterialProperty)):
        if name.startswith('MP_'):
            key = normalized(name)
            require('unambiguous reflected MaterialProperty name', key not in reflected)
            reflected[key] = name
    inspection, accessible = [], []
    for native, hidden in MATERIAL_INPUT_CATALOG:
        name = reflected.get(normalized(native))
        if name is None:
            # A missing public entry is an API mismatch; never silently weaken
            # the 16 public-output assertions to get a passing author.
            require('expected public material input reflected ' + native, hidden)
            inspection.append(dict(native=native, python=None, status='NOT_INSPECTED',
                                   reason='UMETA(Hidden); Python enum entry not reflected'))
        else:
            accessible.append((native, name, getattr(U.MaterialProperty, name)))
            inspection.append(dict(native=native, python=name, status='INSPECTED'))
    supported = {normalized(native) for native, _ in MATERIAL_INPUT_CATALOG}
    R['material_output_reflection'] = dict(
        supported_canonical_count=len(MATERIAL_INPUT_CATALOG),
        inspected_count=len(accessible),
        inputs=inspection,
        other_reflected_members_not_queried=[name for key, name in reflected.items() if key not in supported],
        rule='Only canonical GetExpressionInputDescription-supported entries present in the actual Python enum are queried.')
    return accessible, inspection


def graph_snapshot(obj):
    result = {'nodes': {}, 'material_outputs': {}}
    for name, n in nodes(obj).items():
        row = dict(kind=n.get_class().get_name(), inputs=links(obj, n),
                   outputs=[str(x) for x in E.get_material_expression_output_names(n)])
        if isinstance(n, U.MaterialExpressionMaterialFunctionCall):
            row['function'] = package(n.get_editor_property('material_function'))
        if isinstance(n, U.MaterialExpressionClamp):
            row['clamp'] = {k: value(n.get_editor_property(k)) for k in ('clamp_mode', 'min_default', 'max_default')}
        result['nodes'][name] = row
    if isinstance(obj, U.Material):
        accessible, inspection = reflected_material_inputs()
        result['material_output_inspection'] = inspection
        for native_name, enum_name, prop in accessible:
            n = E.get_material_property_input_node(obj, prop)
            result['material_outputs'][enum_name] = dict(node=n.get_name() if n else None,
                output=E.get_material_property_input_node_output_name(obj, prop))
    return result


def clamp_preflight(obj, sky_node):
    ns = nodes(obj)
    c = ns['MaterialExpressionClamp_1']
    check('clamp actual class ' + package(obj), isinstance(c, U.MaterialExpressionClamp))
    actual = {k: value(c.get_editor_property(k)) for k in ('clamp_mode', 'min_default', 'max_default')}
    check('original Clamp mode and defaults ' + package(obj),
          c.get_editor_property('clamp_mode') == U.ClampMode.CMODE_CLAMP
          and actual['min_default'] == 0.0
          and abs(actual['max_default'] - 1.1) < 1e-6, actual)
    check('original Clamp input and unconnected limits',
          [(x['name'], x['source']) for x in links(obj, c)] ==
          [('None', 'MaterialExpressionAdd_7'), ('Min', None), ('Max', None)])
    check('original world light addition', [x['source'] for x in links(obj, ns['MaterialExpressionAdd_7'])]
          == [sky_node, 'MaterialExpressionCustom_0'])
    check('original light multiplier', [x['source'] for x in links(obj, ns['MaterialExpressionMultiply_23'])]
          == ['MaterialExpressionClamp_1', 'MaterialExpressionCollectionParameter_1'])
    check('directional light native input', 'ResolvedView.DirectionalLightColor' in
          ns['MaterialExpressionCustom_0'].get_editor_property('code'))
    return actual


def restore_function_call_inputs(source, node_name, expected_inputs):
    """Repair lost callee inputs, without accepting any other graph delta."""
    obj = copies[source]
    ns = nodes(obj)
    call = ns[node_name]
    actual_inputs = links(obj, call)
    record = dict(source=source, candidate=paths[source], node=node_name,
                  expected_inputs=copy.deepcopy(expected_inputs),
                  after_function_setter=copy.deepcopy(actual_inputs), restored=[])
    R.setdefault('function_input_restoration', []).append(record)
    names = [row['name'] for row in expected_inputs]
    check('function input names remain unique and unchanged ' + source,
          len(names) == len(set(names)) and [row['name'] for row in actual_inputs] == names,
          dict(expected=names, actual=[row['name'] for row in actual_inputs]))
    for wanted, found in zip(expected_inputs, actual_inputs):
        if wanted == found:
            continue
        # A rewired input, unexpected new connection, or ambiguous output is a
        # failure. Only restore an original connected edge that became empty.
        check('only disconnected original function input ' + wanted['name'],
              wanted['source'] is not None and wanted['source'] in ns
              and isinstance(wanted['first_matching_output'], str)
              and found['source'] is None and found['first_matching_output'] is None,
              dict(expected=wanted, actual=found))
        upstream = ns[wanted['source']]
        outputs = [str(name) for name in E.get_material_expression_output_names(upstream)]
        check('lost function input has one exact source output ' + wanted['source'],
              outputs == [wanted['first_matching_output']], outputs)
        check('restore original function input ' + source + ':' + wanted['name'],
              E.connect_material_expressions(upstream, outputs[0], call, wanted['name']))
        record['restored'].append(dict(input=wanted['name'], upstream=wanted['source'], output=outputs[0]))
    record['after_restoration'] = links(obj, call)
    check('all original callee input edges restored ' + source,
          record['after_restoration'] == expected_inputs, record)


def bounded_graph_diff(expected, actual, limit=64):
    """Diagnostic only: exact leaf values, bounded output, no normalization."""
    missing = object()
    rows, total = [], 0

    def encoded(v):
        return json.dumps(v, ensure_ascii=False, sort_keys=True, separators=(',', ':'))

    def describe(v):
        if v is missing:
            return dict(present=False)
        serialized = encoded(v)
        if len(serialized) <= 2048:
            return dict(present=True, value=v, value_exact=True)
        return dict(present=True, value_exact=False, serialized_characters=len(serialized),
                    sha256=hashlib.sha256(serialized.encode('utf-8')).hexdigest(),
                    serialized_prefix=serialized[:2048])

    def visit(wanted, found, pointer):
        nonlocal total
        if wanted is not missing and found is not missing and wanted == found:
            return
        if isinstance(wanted, dict) and isinstance(found, dict):
            for key in sorted(set(wanted) | set(found)):
                part = str(key).replace('~', '~0').replace('/', '~1')
                visit(wanted.get(key, missing), found.get(key, missing), pointer + '/' + part)
            return
        if isinstance(wanted, list) and isinstance(found, list):
            for index in range(max(len(wanted), len(found))):
                visit(wanted[index] if index < len(wanted) else missing,
                      found[index] if index < len(found) else missing,
                      pointer + '/' + str(index))
            return
        total += 1
        if len(rows) < limit:
            rows.append(dict(json_pointer=pointer, expected=describe(wanted), actual=describe(found)))

    visit(expected, actual, '')
    return dict(difference_count=total, recorded_count=len(rows), truncated=total > len(rows),
                record_limit=limit, differences=rows,
                expected_sha256=hashlib.sha256(encoded(expected).encode('utf-8')).hexdigest(),
                actual_sha256=hashlib.sha256(encoded(actual).encode('utf-8')).hexdigest())


def verify_graph(source, expected):
    actual = graph_snapshot(copies[source])
    same = actual == expected
    observed = None
    if not same:
        differences = bounded_graph_diff(expected, actual)
        differences['source'] = source
        differences['candidate'] = paths[source]
        R.setdefault('graph_differences', []).append(differences)
        observed = dict(report_field='graph_differences', report_index=len(R['graph_differences']) - 1,
                        difference_count=differences['difference_count'],
                        recorded_count=differences['recorded_count'], truncated=differences['truncated'])
    check('only allowed graph delta ' + source, same, observed)
    return actual


try:
    dump()
    R['engine_version'] = U.SystemLibrary.get_engine_version()
    R['script_sha256'] = sha(Path(__file__))
    R['material_output_api_source_sha256'] = {str(p): sha(p) for p in (
        Path('E:/UE_5.8/Engine/Source/Runtime/Engine/Public/SceneTypes.h'),
        Path('E:/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/Material.cpp'),
        Path('E:/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyGenUtil.cpp'))}
    before_dirty = dirty()
    R['dirty_before'] = sorted(before_dirty)
    # Protect the already native-enumerated recursive source dependency closure,
    # hashing its CURRENT bytes; never require stale hashes from an older author.
    graph_evidence = DOCS / 'editor_runtime/author_20260923_225027_761_6461f724_MToonGraphProbe/mtoon_graph_probe.json'
    evidence = json.loads(graph_evidence.read_text(encoding='utf-8'))
    check('recursive graph probe was successful', evidence['status'] == 'PASS' and not evidence['errors'])
    R['source_graph_probe_sha256'] = sha(graph_evidence)
    for filename in evidence['loaded_asset_sha256']:
        f = Path(filename).resolve()
        allowed = (PROJECT / 'Content', PROJECT / 'Plugins/VRM4U/Content', Path('E:/UE_5.8/Engine/Content'))
        check('protected graph file scope', f.suffix == '.uasset' and any(f.is_relative_to(p.resolve()) for p in allowed))
        protected[str(f)] = sha(f)
    for src in sources:
        protect(src)
        check('fresh candidate destination ' + paths[src], not A.does_asset_exist(paths[src]) and not disk(paths[src]).exists())
        originals[src] = U.load_asset(src)
        check('source loaded ' + src, originals[src] is not None)
    for slot, src in FINALS.items():
        check('source Hero owner ' + slot,
              A.get_metadata_tag(originals[src], 'HarborCityOwnedBy') == 'HarborCity_M5_VS2_HeroMaterials')
    for src in (MF_COLOR, MF_BASE):
        check('source function type', isinstance(originals[src], U.MaterialFunction))
    check('source root material type', isinstance(originals[ROOT], U.Material))
    snapshots = {src: instance_snapshot(originals[src]) for src in PARENTS + list(FINALS.values())}
    for snapshot in snapshots.values():
        for texture in snapshot['parameters']['texture'].values():
            if texture:
                protect(texture)
    for i, src in enumerate(PARENTS):
        check('exact Lit inheritance ' + src, snapshots[src]['parent'] == (ROOT if i == 0 else PARENTS[i - 1]))
    for slot, src in FINALS.items():
        expected_parent = PARENTS[3] if slot in ('hair', 'option') else PARENTS[1]
        check('exact Hero parent ' + slot, snapshots[src]['parent'] == expected_parent)
        p = snapshots[src]['parameters']['scalar']
        check('attenuation remains current .1 ' + slot, abs(p['mtoon_LightColorAttenuation'] - .1) < 1e-6)
        check('new names absent ' + slot, all(n not in p for n in LIMITS))
    R['source_instances'] = snapshots
    R['clamp_preflight'] = {
        MF_BASE: clamp_preflight(originals[MF_BASE], 'MaterialExpressionSkyLightEnvMapSample_0'),
        MF_COLOR: clamp_preflight(originals[MF_COLOR], 'MaterialExpressionStaticSwitchParameter_50')}
    graphs = {src: graph_snapshot(originals[src]) for src in (ROOT, MF_BASE, MF_COLOR)}
    redirects = [(ROOT, 'MaterialExpressionMaterialFunctionCall_2', MF_BASE),
                 (MF_BASE, 'MaterialExpressionMaterialFunctionCall_5', MF_COLOR)]
    for src, node, target in redirects:
        check('exact function reference ' + src, graphs[src]['nodes'][node]['function'] == target)
        check('native function setter callable', callable(getattr(nodes(originals[src])[node], 'set_material_function', None)))
    R['protected_source_sha256_before'] = protected.copy()
    dump()

    for src in sources:
        dst = paths[src]
        obj = A.duplicate_asset(src, dst)
        check('native duplicate ' + src, obj is not None and package(obj) == dst)
        copies[src] = obj
        A.set_metadata_tag(obj, 'HarborCityOwnedBy', OWNER)
        A.set_metadata_tag(obj, 'SourceAsset', src)
        A.set_metadata_tag(obj, 'RuntimeAcceptance', 'NOT_RUN')
    expected_graphs = copy.deepcopy(graphs)
    for src, node_name, target in redirects:
        call = nodes(copies[src])[node_name]
        check('native function reference replacement', call.set_material_function(copies[target]))
        expected_graphs[src]['nodes'][node_name]['function'] = paths[target]
        restore_function_call_inputs(src, node_name, graphs[src]['nodes'][node_name]['inputs'])
    for src in (MF_COLOR, MF_BASE):
        obj = copies[src]
        c = nodes(obj)['MaterialExpressionClamp_1']
        for idx, (name, number) in enumerate(LIMITS.items()):
            n = E.create_material_expression_in_function(obj, U.MaterialExpressionScalarParameter, -1400, 500 + idx * 160)
            check('create limit scalar ' + name, n is not None)
            n.set_editor_property('parameter_name', name)
            n.set_editor_property('default_value', number)
            pin = 'Min' if name.endswith('Min') else 'Max'
            check('connect only Clamp ' + pin, E.connect_material_expressions(n, '', c, pin))
            after = graph_snapshot(obj)
            expected_graphs[src]['nodes'][n.get_name()] = after['nodes'][n.get_name()]
            # This is the only permitted old-node input delta.
            index = 1 if pin == 'Min' else 2
            expected_graphs[src]['nodes']['MaterialExpressionClamp_1']['inputs'][index] = dict(
                name=pin, source=n.get_name(), first_matching_output='')
    for src in (ROOT, MF_BASE, MF_COLOR):
        verify_graph(src, expected_graphs[src])
    for src in PARENTS + list(FINALS.values()):
        E.set_material_instance_parent(copies[src], copies[snapshots[src]['parent']])
    # No global parameter collections or original material interfaces are edited.
    E.update_material_function(copies[MF_COLOR])
    E.update_material_function(copies[MF_BASE])
    E.recompile_material(copies[ROOT])
    for src in PARENTS + list(FINALS.values()):
        E.update_material_instance(copies[src])

    def verify_all():
        actual_instances = {}
        for src in PARENTS + list(FINALS.values()):
            actual = instance_snapshot(copies[src])
            expected = copy.deepcopy(snapshots[src])
            expected['parent'] = paths[expected['parent']]
            expected['parameters']['scalar'].update(LIMITS)
            # UE scalar defaults are float32 (.05 cannot compare equal to Python
            # float64). Preserve all other values exactly; check limits separately.
            for name, number in LIMITS.items():
                observed = actual['parameters']['scalar'].get(name)
                check('limit readback ' + name, observed is not None and abs(observed - number) < 1e-6, observed)
                expected['parameters']['scalar'][name] = observed
            check('all inherited parameters and base overrides preserved ' + src, actual == expected)
            actual_instances[paths[src]] = actual
        for src in (ROOT, MF_BASE, MF_COLOR):
            verify_graph(src, expected_graphs[src])
            check('original graph unchanged in memory ' + src, graph_snapshot(originals[src]) == graphs[src])
        for src in PARENTS + list(FINALS.values()):
            check('original instance unchanged in memory ' + src, instance_snapshot(originals[src]) == snapshots[src])
        check('all source asset bytes unchanged', all(Path(p).is_file() and sha(Path(p)) == h for p, h in protected.items()))
        unexpected = dirty() - before_dirty
        unexpected = sorted(p for p in unexpected if not p.startswith(DEST + '/'))
        check('no new dirty packages outside candidate', not unexpected, unexpected)
        return actual_instances

    R['candidate_instances_before_save'] = verify_all()
    for src in sources:
        obj = copies[src]
        check('save scope', package(obj) == paths[src] and package(obj).startswith(DEST + '/'))
        check('native save ' + paths[src], A.save_loaded_asset(obj, False))
        R['assets'].append(dict(path=paths[src], source=src, sha256=sha(disk(paths[src]))))
        dump()
    R['candidate_instances_saved_readback'] = verify_all()
    R['protected_source_sha256_after'] = {p: sha(Path(p)) for p in protected}
    R['variants']['MToonLightLimits'] = {slot: paths[src] for slot, src in FINALS.items()}
    R['source_to_candidate'] = paths
    R['dirty_after'] = sorted(dirty())
    R['status'] = 'PASS'
    R['pass_scope'] = 'Twelve isolated native assets saved; all expression edges, reflected supported root inputs and inherited settings read back; hidden root inputs individually NOT_INSPECTED; source bytes unchanged. Runtime NOT_RUN.'
    dump()
except Exception as exc:
    R['status'] = 'FAIL'
    R['error'] = str(exc)
    R['traceback'] = traceback.format_exc()
    R['partial_candidates_retained'] = [package(o) for o in copies.values()]
    R['source_hash_changes_on_failure'] = [p for p, h in protected.items() if not Path(p).is_file() or sha(Path(p)) != h]
    dump()
    raise
