"""Read exact current VS2 transition metadata; no asset writes, compile or gameplay.

Root runner: ue_m5_vs2_author_run.ps1 -Phase GASTransitionGraphProbe
Requires the compiled HCM5VS2GASTransitionEditor helper; never falls back to guessed values.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
BP = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics'
A = U.EditorAssetLibrary


def arg(name):
    matches = re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    if len(matches) != 1:
        raise RuntimeError('Exactly one -'+name+' required')
    return matches[0][0] or matches[0][1]


OUT = Path(arg('M5EvidenceDir')).resolve()
PHASE = arg('M5AuthorPhase')
assert PHASE == 'GASTransitionGraphProbe'
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
R = dict(status='RUNNING', phase=PHASE, checks=[], inputs={}, asset_writes=0,
         config_writes=0, compiled=False, runtime='NOT_RUN', visual='NOT_RUN',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED = {}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda: f.read(1024*1024), b''):
            h.update(b)
    return h.hexdigest()


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, condition, observed=None):
    R['checks'].append(dict(name=name, status='PASS' if condition else 'FAIL', observed=observed))
    dump()
    if not condition:
        raise RuntimeError(name+': '+repr(observed))


def input_file(name, path):
    path = Path(path).resolve()
    check('input exists '+name, path.is_file(), str(path))
    R['inputs'][name] = dict(path=str(path), sha256=sha(path))


def rule_result_pins(graph):
    return [p for n in graph['nodes'] if n['class_name'] == 'AnimGraphNode_TransitionResult'
            for p in n['pins'] if p['name'] == 'bCanEnterTransition']


try:
    check('exact loaded project', Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve() == PROJECT)
    for folder in ('M5VS1/HeroSelestia/Animation', 'M5VS2/HeroSelestia/Animation'):
        base = PROJECT/'Content/HarborCity'/folder
        for path in sorted(base.rglob('*.uasset')):
            PROTECTED[str(path.resolve())] = sha(path)
    check('existing animation inputs protected', len(PROTECTED) >= 10, len(PROTECTED))
    input_file('blueprint', PROJECT/'Content'/Path(BP.removeprefix('/Game/')).with_suffix('.uasset'))
    input_file('native_helper_cpp', PROJECT/'Source/HarborCity/M5VS2/HCM5VS2GASTransitionEditor.cpp')
    input_file('native_helper_header', PROJECT/'Source/HarborCity/M5VS2/HCM5VS2GASTransitionEditor.h')
    input_file('probe_script', WORK/'tools/ue_m5_vs2_gas_transition_graph_probe.py')
    for name in ('AnimStateTransitionNode.h', 'AnimStateAliasNode.h', 'AnimStateNode.h'):
        input_file('ue_schema_'+name, Path('E:/UE_5.8/Engine/Source/Editor/AnimGraph/Public')/name)
    blueprint = A.load_asset(BP)
    check('actual existing AnimBlueprint', blueprint is not None and isinstance(blueprint, U.AnimBlueprint))
    check('native read-only helper compiled', hasattr(U, 'HCM5VS2GASTransitionEditor'))
    native = json.loads(U.HCM5VS2GASTransitionEditor.read_transition_graph(blueprint))
    R['native_readback'] = native
    check('native metadata readback', native.get('status') == 'READBACK_COMPLETE', native.get('error'))
    check('native no compile or save', native['read_only'] and not native['compiled'] and not native['saved'])
    check('package dirty state unchanged', native['package_dirty_state_unchanged'])
    machines = {m['name']: m for m in native['state_machines']}
    check('exact two installed state machines', set(machines) == {'Locomotion', 'Main States'})
    check('actual locomotion states', {s['state_name'] for s in machines['Locomotion']['states']} == {'Idle', 'Walk / Run'})
    check('actual main states', {s['state_name'] for s in machines['Main States']['states']} == {'Locomotion', 'Jump', 'Fall Loop', 'Land'})
    R['transition_summary'] = []
    for name, machine in machines.items():
        by_path = {g['path']: g for g in machine['graphs']}
        for alias in machine['aliases']:
            check('alias members resolve '+alias['path'], alias['invalid_explicit_member_count'] == 0)
        for transition in machine['transitions']:
            check('actual rule graph present '+transition['path'], transition['rule_graph'] in by_path)
            pins = rule_result_pins(by_path[transition['rule_graph']])
            check('one actual transition result pin '+transition['path'], len(pins) == 1, len(pins))
            summary = {key: transition[key] for key in ('path', 'previous_node', 'next_node', 'priority_order',
                'crossfade_seconds', 'automatic_rule', 'automatic_rule_trigger_time_seconds', 'disabled', 'logic_type')}
            summary['machine'] = name
            summary['rule_result_pin'] = pins[0]
            R['transition_summary'].append(summary)
    R['limitations'] = ['No asset editing, graph compilation, gameplay execution or OS input.',
        'Automatic rules and aliases are actual authored metadata, not inferred from empty result-pin links.',
        'Blueprint static settings do not prove runtime transition timing, visual naturalness or input preservation.']
    R['status'] = 'PASS'
except Exception:
    R['status'] = 'FAIL'
    R['error'] = traceback.format_exc()
finally:
    changed = [p for p, digest in PROTECTED.items() if not Path(p).is_file() or sha(p) != digest]
    R['preservation'] = dict(protected_file_count=len(PROTECTED), changed_files=changed)
    if changed:
        R['status'] = 'FAIL'
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    dump()
    U.log('M5VS2 GASTransitionGraphProbe '+R['status']+' '+str(OUT))
if R['status'] != 'PASS':
    raise RuntimeError(R.get('error', 'Read-only preservation failed'))
