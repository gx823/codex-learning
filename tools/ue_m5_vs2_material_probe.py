"""Read-only native material/texture inspection; never saves an Unreal asset."""
from pathlib import Path
import json
import re
import traceback
import unreal as U

DOC = Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2')
ROOT = '/Game/HarborCity/M5VS2/HeroSelestia'
A, E = U.EditorAssetLibrary, U.MaterialEditingLibrary
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
assert len(args) == 1
OUT = Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to(DOC.resolve())
OUT.mkdir(parents=True, exist_ok=True)
assert not (OUT / 'author_result.json').exists()
R = dict(status='RUNNING', purpose='READ_ONLY_MATERIAL_PROBE', asset_writes=False,
         runtime_shader_status='NOT_RUN_NULLRHI', materials=[], textures={})


def value(v):
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    if isinstance(v, U.LinearColor):
        return [v.r, v.g, v.b, v.a]
    if isinstance(v, U.Object):
        return v.get_path_name()
    return str(v)


def properties(obj, keys):
    result = {}
    for key in keys:
        try:
            result[key] = value(obj.get_editor_property(key))
        except Exception as exc:
            result[key] = {'unavailable': str(exc)}
    return result


def texture(t):
    if t is None or t.get_path_name() in R['textures']:
        return
    R['textures'][t.get_path_name()] = properties(t, [
        'srgb', 'compression_settings', 'compression_no_alpha', 'virtual_texture_streaming',
        'lod_group', 'lod_bias', 'max_texture_size', 'do_scale_mips_for_alpha_coverage',
        'alpha_coverage_thresholds', 'flip_green_channel'])


def params(mat):
    result = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        names = getattr(E, 'get_' + kind + '_parameter_names')(mat)
        getter = ('get_material_instance_' if isinstance(mat, U.MaterialInstanceConstant)
                  else 'get_material_default_') + kind + '_parameter_value'
        result[kind] = {}
        for name in names:
            v = getattr(E, getter)(mat, name)
            result[kind][str(name)] = value(v)
            if kind == 'texture':
                texture(v)
    return result


def inspect(path):
    mat = A.load_asset(path)
    if mat is None:
        raise RuntimeError('Missing material ' + path)
    row = dict(path=mat.get_path_name(), class_name=mat.get_class().get_name(), parameters=params(mat))
    if isinstance(mat, U.MaterialInstanceConstant):
        parent = mat.get_editor_property('parent')
        row['parent'] = parent.get_path_name() if parent else None
        row['base_property_overrides'] = str(mat.get_editor_property('base_property_overrides'))
    elif isinstance(mat, U.Material):
        row['properties'] = properties(mat, ['used_with_skeletal_mesh', 'used_with_morph_targets',
            'blend_mode', 'shading_model', 'two_sided', 'opacity_mask_clip_value'])
        row['expressions'] = []
        for node in E.get_material_expressions(mat):
            expr = dict(path=node.get_path_name(), class_name=node.get_class().get_name(),
                        inputs=[str(v) for v in E.get_material_expression_input_names(node)])
            if isinstance(node, U.MaterialExpressionTextureSample):
                expr['properties'] = properties(node, ['texture', 'sampler_type', 'const_coordinate'])
                texture(node.get_editor_property('texture'))
            elif isinstance(node, U.MaterialExpressionCustom):
                expr['properties'] = properties(node, ['code', 'output_type', 'inputs'])
            elif isinstance(node, U.MaterialExpressionScalarParameter):
                expr['properties'] = properties(node, ['parameter_name', 'default_value'])
            elif isinstance(node, U.MaterialExpressionVectorParameter):
                expr['properties'] = properties(node, ['parameter_name', 'default_value'])
            row['expressions'].append(expr)
        row['output_nodes'] = {}
        for name in ('MP_EMISSIVE_COLOR', 'MP_BASE_COLOR', 'MP_OPACITY', 'MP_OPACITY_MASK'):
            node = E.get_material_property_input_node(mat, getattr(U.MaterialProperty, name))
            row['output_nodes'][name] = node.get_path_name() if node else None
    R['materials'].append(row)


try:
    for variant, prefix in (('MToon', 'MI_MToon_'), ('SoftToon', 'M_SoftToon_')):
        for slot in ('hair', 'body', 'face', 'option', 'costume'):
            inspect(ROOT + '/Materials/' + variant + '/' + prefix + slot)
    for suffix in ('LitOpaque', 'LitTranslucentTwoSided'):
        inspect('/VRM4U/MaterialUtil/UE5/Material/MI_VrmMToonOpt' + suffix)
    inspect('/VRM4U/MaterialUtil/UE5/Material/M_VrmMToonBaseOpaque')
    R['status'] = 'PASS'
    R['pass_scope'] = 'Read-only native reflection only; no shader render or visual acceptance.'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
    U.log_error(R['error'])
finally:
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, default=value), encoding='utf-8')
if R['status'] != 'PASS':
    raise RuntimeError('Read-only material probe failed; see author_result.json')
