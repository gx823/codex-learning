"""Read-only native J cloth preflight; no asset creation, saving or parameter changes.

Phase NPCJClothProbe. Root runs only after compiling the staged helper. This reads
the existing private IdleR2 J candidate; it does not pretend a new cloth fix exists.
"""
from pathlib import Path
import ast
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve(); PROJECT=WORK/'HarborCity'; DOC=WORK/'docs/HarborCity_M5_VS2'
A,B=U.EditorAssetLibrary,U.BlueprintEditorLibrary
SOURCE=DOC/'editor_runtime/author_20260925_051452_501_d1ffc0a0_NPCIdleR2JReload/author_result.json'
FROZEN=Path(__file__).with_name('cloth_probe_manifest.json')
def arg(name):
    rows=re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(rows)==1,name
    return rows[0][0] or rows[0][1]
OUT=Path(arg('M5EvidenceDir')).resolve(); PHASE=arg('M5AuthorPhase')
assert PHASE=='NPCJClothProbe' and OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
R=dict(schema='HarborCity.M5VS2.NPCJClothProbe.Author.v1',phase=PHASE,status='RUNNING',checks=[],inputs={},
       assets=[],asset_writes=0,physics_writes=0,visual='NOT_RUN',runtime='NOT_RUN',
       scope='Actual private J candidate CDO/source anchor readback only; no new cloth candidate authored.',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat())
PROTECTED={}
def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed)); dump()
    if not ok: raise RuntimeError(name)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()
def package(obj):return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def path(obj):return obj.get_path_name() if obj is not None else None
def disk(obj):
    p=package(obj); assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
def protect(p,expected=None):
    p=Path(p); actual=sha(p);check('protected file exact SHA',expected is None or actual==expected,str(p));PROTECTED[str(p)]=actual
def document(p):return json.loads(Path(p).read_text('utf-8-sig'))

try:
    manifest=document(FROZEN)
    check('frozen native preflight source report exists',SOURCE.is_file())
    protect(SOURCE,manifest['j_idle_reload_report_sha256']); prior=document(SOURCE)
    commandlet=SOURCE.with_name('commandlet.json'); protect(commandlet,manifest['j_idle_reload_commandlet_sha256'])
    completed=document(commandlet)
    check('exact J fresh-process prerequisite',prior.get('status')=='PASS' and prior.get('phase')=='NPCIdleR2JReload'
          and prior.get('fresh_process_disk_readback')=='PASS' and completed.get('status')=='PASS'
          and completed.get('exit_code')==0,completed)
    R['inputs']['idle_reload']=dict(path=str(SOURCE),sha256=sha(SOURCE))
    for row in prior['assets']:protect(disk(row['path']),row['sha256'])
    # Preserve all actual source assets and humanoid runtime assets. No helper writes them.
    for folder in ('Source_8b6562a56a4a','Runtime_0f9dd6fb5a'):
        root=PROJECT/'Content/HarborCity/M5VS2/NPC/AvatarSample_J'/folder
        for p in sorted(root.rglob('*.uasset')):protect(p)
    script=WORK/'tools/ue_m5_vs2_npc_idle_r2_author.py'; text=script.read_text('utf-8')
    names=('xyz','transform','getup_snapshot','cdo_snapshot')
    nodes=[n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name in names]
    code='\n\n'.join(ast.get_source_segment(text,n) for n in nodes)
    check('read-only original CDO snapshot functions frozen',tuple(n.name for n in nodes)==names
          and hashlib.sha256(code.encode()).hexdigest()==manifest['cdo_snapshot_function_sha256'])
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(script)+'::four_read_only_functions','exec'),globals())
    bp=A.load_asset(prior['candidate_blueprint']);check('actual final private J blueprint loads',bp is not None)
    before=cdo_snapshot(bp);check('actual J CDO exact IdleReload baseline',before==prior['candidate_cdo'])
    cdo=U.get_default_object(B.generated_class(bp));body=cdo.get_component_by_class(U.SkeletalMeshComponent)
    native=json.loads(U.HCM5VS2NPCClothDiagnostics.inspect_j_component(body));R['native_readback']=native;dump()
    check('actual original twenty human bodies preserved',native.get('original_twenty_body_names_exact') is True)
    check('actual postprocess CDO node readback',native.get('post_process',{}).get('status')=='PASS_READBACK',native.get('post_process'))
    check('actual bounded garment anchor readback',native.get('garment_surface',{}).get('status')=='PASS_REFERENCE_ANCHORS_ONLY')
    R['anchor_selection']=native['garment_surface']['points'];R['candidate_blueprint']=prior['candidate_blueprint']
    check('probe does not change actual CDO',cdo_snapshot(bp)==before)
    for p,h in PROTECTED.items():check('protected source remains byte-identical',sha(p)==h,p)
    R['source_protection']=[dict(path=p,sha256=h) for p,h in PROTECTED.items()]
    R['status']='PASS'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('NPCJClothProbe failed; inspect actual report')
U.log('HCM5VS2_NPC_J_CLOTH_PROBE_PASS read-only; runtime and cloth fix NOT_RUN')
