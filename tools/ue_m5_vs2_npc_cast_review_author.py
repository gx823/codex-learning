"""Fresh eight-source face/full-body review map; no existing source/scene saves.

Root invokes NPCCastReview (original Q), NPCCastReviewWarmQ, or the new
NPCCastReviewIdleR2 (all eight completed private relaxed-idle candidates).
All six fresh imports/integrations/getup bindings must have
actual PASS evidence first. Neutral proxy stage is not final harbor art.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve();PROJECT=WORK/'HarborCity';DOC=WORK/'docs/HarborCity_M5_VS2'
NPC_ROOT='/Game/HarborCity/M5VS2/NPC';OWNER='HarborCity_M5_VS2_NPCCastReview';KEY='HarborCityOwnedBy'
MODE='/Game/HarborCity/M5VS2/HeroSelestia/BP_M5VS2_SelestiaGameMode'
A,E=U.EditorAssetLibrary,U.MaterialEditingLibrary
ACT=U.get_editor_subsystem(U.EditorActorSubsystem);LEVEL=U.get_editor_subsystem(U.LevelEditorSubsystem)
CAST=('Q','R','J','T','U','V','W','X')

def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    assert len(rows)==1,name
    return rows[0][0] or rows[0][1]

OUT=Path(arg('M5EvidenceDir')).resolve();PHASE=arg('M5AuthorPhase')
assert PHASE in ('NPCCastReview','NPCCastReviewWarmQ','NPCCastReviewIdleR2')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
ROOT=NPC_ROOT+'/CastReview/Run_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12];MAP=ROOT+'/L_NPCCast'
R=dict(schema='HarborCity.M5VS2.NPCCastReview.Author.v1',status='RUNNING',phase=PHASE,map=MAP,
       owner=OWNER,inputs={},checks=[],assets=[],actors=[],roster=[],runtime='NOT_RUN',visual='USER_REVIEW',
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
       limitations=['Neutral proxy art stage only; no final harbor, dialogue, patrol or concurrent 12-person replacement claim.',
           'Six fresh skeletons each require their actual native import/integration/four-getup binding chain.',
           'Director records 16 raw PNGs, 3 native expression probes per person and actual automatic blink.',
           'Ragdoll/getup wall/slope/narrow, character clothing/hair physical quality and ordinary combat footage remain separate runtime tasks.',
           'Per-material render-thread fallback readiness is NOT_RUN in this minimal director; global compiler idle and material references alone are not that proof.'])
PROTECTED={}

def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed))
    if not ok:dump();raise RuntimeError(name+': '+repr(observed))
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for v in iter(lambda:f.read(1024*1024),b''):h.update(v)
    return h.hexdigest()
def package(obj):return (obj.get_path_name() if isinstance(obj,U.Object) else str(obj)).split('.')[0]
def disk(path,extension='.uasset'):
    p=package(path);assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix(extension)
def document(p):return json.loads(Path(p).read_text('utf-8-sig'))
def binding(p):return dict(path=str(p),sha256=sha(p))
def protect_file(p,expected=None):
    p=Path(p).resolve();check('protected input exists',p.is_file(),str(p));h=sha(p)
    check('exact protected source identity',expected is None or h==expected,str(p));PROTECTED[str(p)]=h
def latest_pass(pattern,predicate=lambda d:True):
    for p in sorted((DOC/'editor_runtime').glob(pattern+'/author_result.json'),key=lambda f:f.stat().st_mtime,reverse=True):
        d=document(p)
        if d.get('status')=='PASS' and predicate(d):return p,d
    raise RuntimeError('Required completed native evidence missing: '+pattern)
def load(p):
    v=A.load_asset(p);check('native exact asset load',v is not None,p);return v
def cls(p):
    p=package(p);v=U.load_class(None,p+'.'+p.rsplit('/',1)[1]+'_C');check('native generated class load',v is not None,p);return v

# BEGIN_FROZEN_QR_TRANSITION_CONSUMER
# Source: ue_m5_vs2_harbor_corner_author.py; exact already-native-used functions.
def accept_npc_getup_transition(letter, spec, integration_file, expected):
    """Only a saved, backed-up native Apply may replace this exact BP hash."""
    path = package(spec['blueprint'])
    current = sha(disk(path))
    if current == expected[path]:
        return []
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCGetUpBindingApply/getup_binding_manifest.json'),
                       key=lambda p: p.stat().st_mtime, reverse=True):
        manifest = document(file)
        if (manifest.get('schema') != 'HarborCity.M5VS2.NPCGetUpBinding.v1' or manifest.get('status') != 'PASS'
                or manifest.get('phase') != 'NPCGetUpBindingApply'):
            continue
        rows = manifest.get('transitions', [])
        selected = [row for row in rows if row.get('sample') == letter and package(row.get('path', '')) == path
                    and row.get('integration_evidence_sha256') == sha(integration_file)]
        if not selected:
            continue
        check('getup manifest contains exactly two unique Q/R BP transitions', len(rows) == 2
              and {row.get('sample') for row in rows} == {'Q', 'R'}
              and len({package(row.get('path', '')) for row in rows}) == 2
              and all(row.get('class_name') == 'Blueprint' and package(row.get('path', '')).startswith(
                  NPC_ROOT + '/AvatarSample_' + row['sample'] + '/Runtime_') for row in rows)
              and len(selected) == 1)
        row = selected[0]
        check('getup exact existing-before and current-after hashes', row.get('before_sha256') == expected[path]
              and row.get('after_sha256') == current
              and re.fullmatch(r'[0-9a-f]{64}', current) is not None, path)
        evidence_file = Path(manifest['evidence']['path']).resolve()
        check('getup manifest bound to adjacent exact Apply report', evidence_file == (file.parent/'author_result.json').resolve()
              and sha(evidence_file) == manifest['evidence']['sha256'])
        evidence = document(evidence_file)
        check('getup Apply recorded only actual successful BP saves', evidence.get('status') == 'PASS'
              and evidence.get('phase') == 'NPCGetUpBindingApply' and evidence.get('applied') is True
              and evidence.get('transitions') == rows and evidence.get('dry_run') == manifest.get('dry_run')
              and evidence.get('preservation', {}).get('unexpected_changed_files') == []
              and all(evidence.get(name) == 0 for name in ('map_writes', 'animation_writes', 'physics_writes'))
              and all(c.get('status') == 'PASS' for c in evidence.get('checks', [])))
        saved = evidence.get('assets', [])
        check('getup report assets exactly match the two transition hashes', len(saved) == 2
              and {(package(a['path']), a['sha256'], a.get('class_name')) for a in saved}
              == {(package(a['path']), a['after_sha256'], 'Blueprint') for a in rows})
        dry_file = Path(manifest['dry_run']['path']).resolve()
        dry = document(dry_file)
        check('getup Apply uses exact successful unapplied DryRun', sha(dry_file) == manifest['dry_run']['sha256']
              and dry.get('status') == 'PASS' and dry.get('phase') == 'NPCGetUpBindingDryRun'
              and dry.get('applied') is False and dry.get('assets') == [] and dry.get('transitions') == [])
        dry_plans = [p for p in dry.get('plans', []) if p.get('sample') == letter]
        apply_plans = [p for p in evidence.get('plans', []) if p.get('sample') == letter]
        check('getup exact preapproved specimen plan', len(dry_plans) == len(apply_plans) == 1
              and package(dry_plans[0]['blueprint']) == path and dry_plans[0]['before_sha256'] == expected[path]
              and dry_plans[0]['integration_sha256'] == sha(integration_file)
              and {k:v for k,v in apply_plans[0].items() if k != 'backup'} == dry_plans[0])
        backup = Path(row['backup']['path']).resolve()
        backup_root = (Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / file.parent.name).resolve()
        check('getup exact private original BP backup', backup.parent == backup_root and backup.name == disk(path).name
              and row['backup']['sha256'] == expected[path] and apply_plans[0].get('backup') == row['backup'])
        protect_file(backup, expected[path])
        retarget_file = Path(row['retarget_evidence']['path']).resolve()
        retarget = document(retarget_file)
        check('getup exact preserved four-action retarget source', row['retarget_evidence'] == dry_plans[0]['retarget']
              and sha(retarget_file) == row['retarget_evidence']['sha256'] and retarget.get('status') == 'PASS'
              and retarget.get('phase') == 'GASRecoveryRetarget' + letter
              and retarget.get('preservation', {}).get('changed_files') == [])
        native = row.get('native_cdo_readback', {})
        clips = native.get('get_up_clips', [])
        check('getup exact enabled four-direction native CDO readback', native.get('use_authored_get_up') is True
              and native.get('get_up_pose_data_verified') is True and len(clips) == 4
              and {c.get('direction') for c in clips} == {'SUPINE', 'PRONE', 'LEFT', 'RIGHT'}
              and {package(c.get('animation', '')) for c in clips}
              == {package(c['path']) for c in retarget.get('target_audits', [])})
        for asset in retarget.get('assets', []):
            protect_file(disk(asset['path']), asset['sha256'])
        expected[path] = current
        return [dict(**binding(file), apply_evidence=binding(evidence_file), dry_run=binding(dry_file),
                     blueprint=path, before_sha256=row['before_sha256'], after_sha256=current,
                     backup=binding(backup), native_cdo_readback=native)]
    raise RuntimeError('NPC BP changed without an exact successful GetUp Apply transition: ' + path)

def npc_specimen(letter):
    report_file, report = latest_pass('author_*_NPCIntegration' + letter, lambda d: d.get('sample') == letter)
    spec = report['specimen']
    check('new isolated NPC integration', package(spec['blueprint']).startswith(NPC_ROOT + '/AvatarSample_' + letter + '/Runtime_'))
    # Only exact, evidenced subsequent animation/PHYS repairs may supersede
    # the original integration hashes. Never accept unrecorded current bytes.
    expected = {package(a['path']): a['sha256'] for a in report['assets']}
    locomotion_repairs = []
    animation_root = report['destination'] + '/Animation/'
    allowed_clips = {animation_root + name + '_NPC' + letter for name in
                     ('MM_Idle', 'MF_Unarmed_Walk_Fwd', 'MF_Unarmed_Jog_Fwd')}
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCInPlace' + letter + '/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if (data.get('status') != 'PASS' or data.get('sample') != letter
                or Path(data.get('integration_evidence', '')).resolve() != report_file.resolve()):
            continue
        check('in-place repair exact integration identity', data.get('destination') == report['destination']
              and data.get('specimen') == spec)
        saved_rows = data.get('assets', [])
        saved_paths = [package(a['path']) for a in saved_rows]
        check('in-place repair only unique known locomotion clips', bool(saved_paths)
              and len(saved_paths) == len(set(saved_paths)) and set(saved_paths) <= allowed_clips)
        backups = data.get('animation_backups', [])
        trajectories = data.get('locomotion_in_place', [])
        transitions = []
        for saved, path in zip(saved_rows, saved_paths):
            before = [a for a in backups if package(a['path']) == path]
            native = [a for a in trajectories if package(a['target']) == path]
            check('in-place repair has unique before/native evidence', len(before) == 1 and len(native) == 1, path)
            before, native = before[0], native[0]
            check('in-place repair begins at integration hash', path in expected
                  and before.get('sha256') == expected[path], path)
            backup = Path(before['backup']).resolve()
            backup_root = Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups') / file.parent.name
            check('in-place repair exact backup path', backup.parent == backup_root.resolve()
                  and backup.name == disk(path).name, str(backup))
            protect_file(backup, expected[path])
            applied = native.get('apply', {})
            readback = native.get('after_native_save_or_unchanged', {})
            check('in-place native applied and saved readback', saved.get('class_name') == 'AnimSequence'
                  and applied.get('status') == 'PASS' and applied.get('applied') is True
                  and applied.get('all_unselected_tracks_exactly_preserved') is True
                  and applied.get('selected_tracks_z_scale_exactly_preserved') is True
                  and readback.get('status') == 'PASS' and readback.get('needs_repair') is False
                  and package(applied.get('target', '')) == path and package(readback.get('target', '')) == path
                  and re.fullmatch(r'[0-9a-f]{64}', saved.get('sha256', '')) is not None, path)
            transitions.append(dict(path=path, before_sha256=expected[path], after_sha256=saved['sha256'],
                                    backup=binding(backup)))
            expected[path] = saved['sha256']
        locomotion_repairs.append(dict(**binding(file), integration_evidence_sha256=sha(report_file), assets=transitions))
        break
    repairs = []
    for file in sorted((DOC / 'editor_runtime').glob('author_*_NPCPhysicsRepair/author_result.json'), key=lambda p: p.stat().st_mtime, reverse=True):
        data = json.loads(file.read_text(encoding='utf-8-sig'))
        if data.get('status') != 'PASS':
            continue
        row = next((a for a in data.get('assets', []) if a.get('sample') == letter), None)
        if row and row.get('integration_evidence_sha256') == sha(report_file) and row.get('after_save'):
            saved = row['after_save']
            check('repair targets exact specimen physics', package(saved['path']) == package(spec['physics']))
            expected[package(saved['path'])] = saved['sha256']
            repairs.append(binding(file))
            break
    getup_repairs = accept_npc_getup_transition(letter, spec, report_file, expected)
    for path, expected_sha in expected.items():
        protect_file(disk(path), expected_sha)
    for key in ('mesh', 'profile', 'blueprint', 'anim_blueprint', 'physics'):
        protect_file(disk(spec[key]))
    R['inputs']['NPC_' + letter] = dict(**binding(report_file), specimen=spec,
                                      locomotion_repairs=locomotion_repairs, physics_repairs=repairs,
                                      getup_bindings=getup_repairs)
    return spec
# END_FROZEN_QR_TRANSITION_CONSUMER

def cast_specimen(letter):
    file,data=latest_pass('author_*_NPCIntegrationR2'+letter,lambda d:d.get('sample')==letter)
    check('exact six-cast integration pass scope',data['status']=='PASS' and data.get('checks')
          and all(c['status']=='PASS' for c in data['checks']),str(file))
    spec=data['specimen'];bp=package(spec['blueprint'])
    check('independent fresh runtime namespace',re.fullmatch(re.escape(NPC_ROOT+'/AvatarSample_'+letter+'/Runtime_')+r'[0-9a-f]{10}/BP_AvatarSample_'+letter,bp) is not None)
    expected={package(a['path']):a['sha256'] for a in data['assets']}
    check('complete native runtime asset set',all(package(spec[k]) in expected for k in ('blueprint','profile','anim_blueprint','physics')))
    apply_file,applied=latest_pass('author_*_NPCGetUpR2'+letter+'Apply')
    manifest_file=apply_file.parent/'getup_binding_manifest.json';manifest=document(manifest_file)
    check('completed exact one-person getup apply',applied.get('status')=='PASS' and applied.get('applied') is True
          and len(applied.get('transitions',[]))==1 and len(applied.get('assets',[]))==1
          and applied.get('preservation',{}).get('unexpected_changed_files')==[]
          and all(applied.get(k)==0 for k in ('map_writes','animation_writes','physics_writes'))
          and applied.get('checks') and all(c['status']=='PASS' for c in applied['checks']))
    row=applied['transitions'][0]
    check('exact native getup transition manifest',manifest.get('schema')=='HarborCity.M5VS2.NPCCastR2.GetUpBinding.v1'
          and manifest.get('status')=='PASS' and manifest.get('sample')==letter
          and manifest.get('phase')=='NPCGetUpR2'+letter+'Apply'
          and manifest.get('evidence')==binding(apply_file) and manifest.get('transitions')==applied['transitions'])
    check('getup transitions only original integration BP',row['sample']==letter and package(row['path'])==bp
          and row['integration_evidence_sha256']==sha(file) and row['before_sha256']==expected[bp]
          and applied['assets'][0]['sha256']==row['after_sha256']
          and package(applied['assets'][0]['path'])==bp)
    backup=Path(row['backup']['path']).resolve()
    check('private original BP backup path',backup.parent==(Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/apply_file.parent.name).resolve()
          and backup.name==disk(bp).name and row['backup']['sha256']==expected[bp]);protect_file(backup,expected[bp])
    dry_file=Path(applied['dry_run']['path']).resolve();dry=document(dry_file)
    check('actual exact successful dry run',sha(dry_file)==applied['dry_run']['sha256']
          and dry.get('status')=='PASS' and not dry.get('applied') and dry.get('assets')==[]
          and dry.get('phase')=='NPCGetUpR2'+letter+'DryRun' and len(dry.get('plans',[]))==1
          and dry['plans'][0]['before_sha256']==expected[bp] and dry['plans'][0]['integration_sha256']==sha(file))
    rt_file=Path(row['retarget_evidence']['path']).resolve();rt=document(rt_file)
    check('exact original-frame four-action retarget evidence',sha(rt_file)==row['retarget_evidence']['sha256']
          and rt.get('status')=='PASS' and rt.get('phase')=='NPCGetUpRetargetR2'+letter
          and rt.get('preservation',{}).get('changed_files')==[] and len(rt.get('target_audits',[]))==4)
    native=row['native_cdo_readback'];clips=native.get('get_up_clips',[])
    check('four semantic directions actually enabled in saved CDO',native.get('use_authored_get_up') is True
          and native.get('get_up_pose_data_verified') is True and len(clips)==4
          and {c['direction'] for c in clips}=={'SUPINE','PRONE','LEFT','RIGHT'}
          and {package(c['animation']) for c in clips}=={package(a['path']) for a in rt['target_audits']})
    for asset in rt['assets']:protect_file(disk(asset['path']),asset['sha256'])
    expected[bp]=row['after_sha256']
    for p,h in expected.items():protect_file(disk(p),h)
    for k in ('mesh','blueprint','profile','anim_blueprint','physics'):protect_file(disk(spec[k]))
    R['inputs']['NPC_'+letter]=dict(**binding(file),specimen=spec,getup_apply=binding(apply_file),getup_manifest=binding(manifest_file),
        dry_run=binding(dry_file),retarget=binding(rt_file),native_getup_configuration=native)
    return spec

def consume_idle_reload(letter,baseline):
    file,data=latest_pass('author_*_NPCIdleR2'+letter+'Reload')
    check('completed private relaxed-idle reload schema',data.get('schema')=='HarborCity.M5VS2.NPCIdleR2.v1'
          and data.get('sample')==letter and data.get('phase')=='NPCIdleR2'+letter+'Reload'
          and data.get('fresh_process_disk_readback')=='PASS' and len(data.get('assets',[]))==4
          and data.get('preservation',{}).get('changed_source_files')==[]
          and data.get('checks') and all(c['status']=='PASS' for c in data['checks'])
          and all(data.get(k)==0 for k in ('map_writes','skeleton_writes','physics_writes')))
    original=data.get('baseline_specimen',data['source_specimen'])
    check('idle exact same already-verified integration/getup chain',original==baseline
          and data['inputs']['NPC_'+letter]==R['inputs']['NPC_'+letter])
    apply_file=Path(data['inputs']['candidate_evidence']['path']).resolve();applied=document(apply_file)
    check('idle reload tied to exact native Apply',sha(apply_file)==data['inputs']['candidate_evidence']['sha256']
          and applied.get('status')=='PASS' and applied.get('phase')=='NPCIdleR2'+letter+'Apply'
          and applied.get('schema')==data['schema'] and applied.get('preservation',{}).get('changed_source_files')==[]
          and applied['specimen']==data['specimen'] and applied['assets']==data['assets']
          and applied['candidate_cdo']==data['candidate_cdo'] and applied['source_cdo']==data['source_cdo'])
    process_evidence=[]
    for report in (file,apply_file):
        commandlet=report.parent/'commandlet.json';run=document(commandlet)
        check('idle actual complete native process exit',run.get('status')=='PASS' and run.get('author_status')=='PASS'
              and run.get('exit_code')==0 and run.get('phase')==document(report)['phase'],str(commandlet))
        process_evidence.append(binding(commandlet))
    source_bp=data['source_blueprint'];protect_file(disk(source_bp),data['source_sha256'])
    if letter=='Q':
        transition=data.get('source_transition',{});warm=data['inputs'].get('Q_skin_candidate',{})
        warm_file=Path(warm['path']).resolve();warm_data=document(warm_file)
        check('Q idle explicitly preserves selected warm-material source',sha(warm_file)==warm['sha256']
              and '_966823a4_QSkinCandidate' in warm_file.parent.name and warm_data.get('status')=='PASS'
              and transition.get('kind')=='independent_Q_warm_BP'
              and package(transition['source_blueprint'])==package(baseline['blueprint'])
              and transition['source_sha256']==sha(disk(baseline['blueprint']))
              and package(warm_data['candidate_blueprint'])==source_bp==transition['candidate_blueprint']
              and transition['candidate_sha256']==data['source_sha256']
              and transition['candidate_cdo']==data['source_cdo'])
        for asset in warm_data['assets']:protect_file(disk(asset['path']),asset['sha256'])
        R['inputs']['Q_skin_candidate']=binding(warm_file)
    else:
        check('idle copied exact original completed source BP',package(source_bp)==package(baseline['blueprint']))
    spec=data['specimen'];expected=dict(baseline)
    for k in ('blueprint','generated_class','anim_blueprint','blendspace','idle_animation'):expected[k]=spec[k]
    check('idle preserves all other specimen metadata',spec==expected)
    prefix=NPC_ROOT+'/AvatarSample_'+letter+'/IdleR2/Batch_'
    check('four exact private same-batch outputs',re.fullmatch(re.escape(prefix)+r'[0-9a-f]{12}',data['destination']) is not None
          and {package(a['path']) for a in data['assets']}=={package(spec[k]) for k in ('blueprint','anim_blueprint','blendspace','idle_animation')}
          and all(package(a['path']).startswith(data['destination']+'/') for a in data['assets']))
    for asset in data['assets']:protect_file(disk(asset['path']),asset['sha256'])
    expected_cdo=dict(data['source_cdo']);expected_cdo['anim_class']=spec['anim_blueprint']+'.'+spec['anim_blueprint'].rsplit('/',1)[1]+'_C'
    check('saved and reloaded CDO differs only in intended ABP',data['candidate_cdo']==expected_cdo)
    actual=json.loads(U.HCM5VS2NPCIdleEditor.inspect_idle_graph(load(spec['anim_blueprint'])))
    check('current actual native graph/BS/three samples equal Reload',actual.get('status')=='PASS'
          and actual['blendspace']==spec['blendspace'] and actual['nodes']==data['candidate_graph']
          and actual['samples']==data['candidate_samples'] and actual['samples'][0]['animation']==spec['idle_animation'])
    cdo=U.get_default_object(cls(spec['blueprint']));physical=cdo.get_component_by_class(U.HCM4R1PhysicalReactionComponent)
    check('current idle candidate retains four authored getup flags',physical is not None
          and physical.get_editor_property('use_authored_get_up') and physical.get_editor_property('get_up_pose_data_verified'))
    clips=[]
    for row in physical.get_editor_property('get_up_clips'):
        direction=next((k for k in ('SUPINE','PRONE','LEFT','RIGHT') if row.get_editor_property('direction')==getattr(U.HCM4R1GetUpDirection,k)),None)
        clips.append((direction,package(row.get_editor_property('animation'))))
    check('current idle candidate exact four getup clips',len(clips)==4
          and set(clips)=={(r['direction'],r['animation']) for r in data['candidate_cdo']['getup']['get_up_clips']})
    R['inputs']['NPCIdle_'+letter]=dict(**binding(file),apply_evidence=binding(apply_file),commandlets=process_evidence,
        source_blueprint=source_bp,source_sha256=data['source_sha256'],specimen=spec,visual='USER_REVIEW')
    return spec

def material(name,rgb):
    v=U.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT+'/Materials',U.Material,U.MaterialFactoryNew())
    check('create unique neutral stage material',v is not None)
    c=E.create_material_expression(v,U.MaterialExpressionConstant3Vector,0,0);c.set_editor_property('constant',U.LinearColor(*rgb,1))
    rough=E.create_material_expression(v,U.MaterialExpressionConstant,0,100);rough.set_editor_property('r',.85)
    check('connect native neutral material',E.connect_material_property(c,'',U.MaterialProperty.MP_BASE_COLOR)
          and E.connect_material_property(rough,'',U.MaterialProperty.MP_ROUGHNESS));E.recompile_material(v)
    A.set_metadata_tag(v,KEY,OWNER);check('save only new stage material',A.save_loaded_asset(v,False))
    R['assets'].append(dict(path=v.get_path_name(),sha256=sha(disk(v))));return v
def spawn(name,kind,pos,rot=None):
    check('bounded new stage actor count',len(R['actors'])<25)
    v=ACT.spawn_actor_from_class(kind,U.Vector(*pos),rot or U.Rotator(),False);check('native new stage actor',v is not None,name)
    v.set_actor_label('HC_M5VS2_CastReview_'+name);v.set_editor_property('tags',[U.Name(OWNER)])
    R['actors'].append(dict(label=v.get_actor_label(),class_path=v.get_class().get_path_name(),position_cm=pos));return v
def box(name,pos,size,mat,collision):
    a=spawn(name,U.StaticMeshActor,pos);c=a.get_component_by_class(U.StaticMeshComponent);c.set_mobility(U.ComponentMobility.STATIC)
    c.set_static_mesh(load('/Engine/BasicShapes/Cube'));c.set_material(0,mat);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    c.set_cast_shadow(collision);a.set_actor_scale3d(U.Vector(*(x/100 for x in size)));return a

try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no PIE or dirty old maps',not U.EditorLevelLibrary.get_pie_worlds(False) and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    check('native minimal cast review properties compiled',hasattr(U,'HCM5VS2NPCReviewDirector'))
    U.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([NPC_ROOT,'/Game/HarborCity/M5VS2/HeroSelestia','/Game/HarborCity/M5VS2/NPCSkinCandidates'],force_rescan=True)
    specs={k:(npc_specimen(k) if k in ('Q','R') else cast_specimen(k)) for k in CAST}
    if PHASE=='NPCCastReviewIdleR2':
        check('native bounded relaxed-idle bridge compiled',hasattr(U,'HCM5VS2NPCIdleEditor'))
        specs={k:consume_idle_reload(k,specs[k]) for k in CAST}
    if PHASE=='NPCCastReviewWarmQ':
        f,d=latest_pass('author_*_QSkinCandidate')
        check('exact independent Q candidate',d.get('schema')=='HarborCity.M5VS2.QSkinCandidate.v1'
              and d.get('preservation',{}).get('changed_source_files')==[] and d.get('preservation',{}).get('source_tree_changes')==[]
              and package(d['source_blueprint'])==package(specs['Q']['blueprint']) and len(d['assets'])==3)
        for asset in d['assets']:protect_file(disk(asset['path']),asset['sha256'])
        source_file=str(disk(specs['Q']['blueprint']).resolve())
        protect_file(source_file,d['preservation']['source_sha256_before'][source_file])
        specs['Q']=dict(specs['Q'],blueprint=d['candidate_blueprint'],generated_class=d['candidate_generated_class'])
        R['inputs']['Q_skin_candidate']=binding(f)
    protect_file(PROJECT/'Config/DefaultEngine.ini');protect_file(disk(MODE))
    check('fresh unique review map',not A.does_directory_exist(ROOT) and not disk(MAP,'.umap').parent.exists())
    check('native new isolated review map',LEVEL.new_level(MAP,False))
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world();A.set_metadata_tag(world,KEY,OWNER)
    world.get_world_settings().set_editor_property('default_game_mode',cls(MODE))
    floor=material('M_NeutralFloor',(.18,.18,.18));back=material('M_NeutralBackdrop',(.24,.27,.31))
    box('Floor',(0,0,-10),(3600,5000,20),floor,True);box('Backdrop',(-900,0,300),(10,5000,600),back,False)
    box('BackdropLeft',(0,-2400,300),(3000,10,600),back,False);box('BackdropRight',(0,2400,300),(3000,10,600),back,False)
    # Keep the unobserved player behind the portrait backdrop, not behind T/U/V.
    spawn('PlayerStart',U.PlayerStart,(-1200,0,95))
    sun=spawn('MainLight',U.DirectionalLight,(300,0,500),U.Rotator(pitch=-28,yaw=155));light=sun.get_component_by_class(U.DirectionalLightComponent)
    light.set_mobility(U.ComponentMobility.MOVABLE);light.set_light_color(U.LinearColor(1,1,1,1));light.set_intensity(3.)
    light.set_editor_property('atmosphere_sun_light',True)
    sky=spawn('SkyLight',U.SkyLight,(0,0,500)).get_component_by_class(U.SkyLightComponent)
    sky.set_mobility(U.ComponentMobility.MOVABLE);sky.set_editor_property('real_time_capture',True);sky.set_intensity(.4)
    spawn('Atmosphere',U.SkyAtmosphere,(0,0,0));post=spawn('FixedExposure',U.PostProcessVolume,(0,0,0));post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=U.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True,auto_exposure_bias=0.,override_motion_blur_amount=True,motion_blur_amount=0.,
        override_bloom_intensity=True,bloom_intensity=.12).items():settings.set_editor_property(k,v)
    post.set_editor_property('settings',settings)
    region=spawn('Region',U.HCM3NavRegion,(0,0,100));region.set_editor_property('stable_id',U.Name('M5VS2_CastReview_Allowed'))
    region.set_editor_property('half_extent',U.Vector(1400,2200,400))
    people=[]
    for i,letter in enumerate(CAST):
        spec=specs[letter];npc=spawn('NPC_'+letter,cls(spec['blueprint']),(0,(i-3.5)*500,float(spec['capsule_half_height_cm'])+2))
        npc.set_editor_property('stationary',True);npc.set_editor_property('navigation_region',region)
        check('actual fresh actor exact source profile',package(npc.get_editor_property('npc_profile'))==package(spec['profile']))
        body=npc.get_component_by_class(U.SkeletalMeshComponent)
        check('actual actor mesh/ABP/PHYS preserved',package(body.get_editor_property('skeletal_mesh_asset'))==package(spec['mesh'])
              and package(body.get_editor_property('anim_class'))==package(spec['anim_blueprint'])
              and package(body.get_editor_property('physics_asset_override'))==package(spec['physics']))
        people.append(npc);R['roster'].append(dict(sample=letter,blueprint=spec['blueprint'],profile=spec['profile'],mesh=spec['mesh'],
            materials=[body.get_material(j).get_path_name() for j in range(body.get_num_materials())],capsule_half_height_cm=spec['capsule_half_height_cm']))
    director=spawn('Director',U.HCM5VS2NPCReviewDirector,(0,0,-100));director.set_editor_property('specimens',people)
    director.set_editor_property('main_light',sun);director.set_editor_property('cast_portraits',True)
    director.set_editor_property('specimen_ids',[U.Name(k) for k in CAST])
    check('save only unique new review map',LEVEL.save_current_level());R['assets'].append(dict(path=MAP,sha256=sha(disk(MAP,'.umap'))))
    check('load actual saved cast map',LEVEL.load_level(MAP))
    directors=[a for a in ACT.get_all_level_actors() if isinstance(a,U.HCM5VS2NPCReviewDirector)]
    check('saved eight-person review binding',len(directors)==1 and directors[0].get_editor_property('cast_portraits')
          and len(directors[0].get_editor_property('specimens'))==8
          and [str(n) for n in directors[0].get_editor_property('specimen_ids')]==list(CAST))
    R['runtime_arguments']=[MAP,'-game','-M5VS2NPCReview','-M5VS2NPCCastReview','-M5VS2AutoQuit','-ResX=1920','-ResY=1080','-windowed','-language=en']
    R['required_runtime_additions']='Unique -M5VS2EvidenceDir=absolute path and unique HCM1SaveSlot; normal project wrapper also monitors Escape.'
    R['expected_runtime_evidence']='NPCReview_TIMESTAMP_GUID/npc_review.json, 16 raw face/full-body PNGs; function-expression and natural blink telemetry.'
    R['status']='PASS';R['pass_scope']='Native isolated author/save/reload and exact verified source references only. Rendered results and physical gameplay NOT_RUN.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['preservation']=dict(unexpected_changed_files=changed,source_sha256_before=PROTECTED)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Cast review author failed; preserve evidence and source packages')
