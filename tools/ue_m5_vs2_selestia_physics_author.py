"""Native root-scheduled author: one isolated VS2 AnimBP, no map/body/source writes."""
from pathlib import Path
import hashlib, json, re, traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
DOCS = WORK / 'docs/HarborCity_M5_VS2'
CONTENT = WORK / 'HarborCity/Content'
CONFIG = DOCS / 'research/SELESTIA_KAWAII_INITIAL_CONFIG.json'
SOURCE = '/Game/HarborCity/M5VS1/HeroSelestia/Animation/ABP_M4R2_Player_Selestia'
TARGET = '/Game/HarborCity/M5VS2/HeroSelestia/Animation/ABP_M5VS2_Selestia_Physics'
MESH = '/Game/HarborCity/M5VS1/HeroSelestia/SKM_Selestia'
OWNER = 'HarborCity_M5_VS2_SelestiaPhysics_v1'
A = U.EditorAssetLibrary
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
assert len(args) == 1, 'One -M5EvidenceDir required'
OUT = Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to(DOCS.resolve()), 'Evidence outside VS2 docs'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = OUT / 'selestia_physics_author.json'
assert not REPORT.exists(), 'Fresh evidence directory required'
R = dict(status='RUNNING', checks=[], native_execution='RUNNING', runtime='NOT_RUN', visual='NOT_RUN',
         authored_assets=[], old_assets_modified=False, look_at='FOLLOWUP_EXPRESSION_AUTHOR')

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def disk(asset): return CONTENT / (asset.removeprefix('/Game/').split('.')[0] + '.uasset')
def write():
    payload=json.dumps(R, ensure_ascii=False, indent=2)+'\n'
    REPORT.write_text(payload,encoding='utf-8')
    (OUT/'author_result.json').write_text(payload,encoding='utf-8')
def check(name, ok, value=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=value)); write()
    if not ok: raise RuntimeError(name+': '+repr(value))
def field(d,k): return next(v for n,v in d.items() if n.casefold()==k.casefold())
def subset(expected, actual):
    if isinstance(expected,dict): return all(subset(v,field(actual,k)) for k,v in expected.items())
    if isinstance(expected,list): return len(expected)==len(actual) and all(subset(a,b) for a,b in zip(expected,actual))
    if isinstance(expected,bool): return expected is actual
    if isinstance(expected,(int,float)): return abs(expected-actual)<max(1e-4,abs(expected)*1e-5)
    return str(expected).split('::')[-1]==str(actual).split('::')[-1]
def validate_readback(data, config):
    check('native reflected readback complete',data.get('status') in ('READBACK_COMPLETE','AUTHOR_CONFIGURED_NOT_RUNTIME_ACCEPTANCE'))
    nodes={x['name']:x for x in data['nodes']}
    check('eleven configured Kawaii nodes',len(nodes)==11)
    for group in config['groups']:
        node=nodes['M5VS2_Physics_'+group['name']]; props=node['node_properties']
        check('installed class '+group['name'],node['class']=='/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics')
        check('numeric/root/axis settings '+group['name'],subset(group['node_properties'],props))
        sphere=props['SphericalLimits'];capsule=props['CapsuleLimits']
        expected=group['colliders']
        check('collision count '+group['name'],len(sphere)+len(capsule)==len(expected))
        for c in expected:
            rows=capsule if c['shape']=='capsule' else sphere
            matches=[r for r in rows if field(field(r,'DrivingBone'),'BoneName')==c['driving_bone']]
            # Multiple converted helper roots can share an anchor; choose by offset.
            matches=[r for r in matches if subset(dict(zip(('X','Y','Z'),c['offset_cm'])),field(r,'OffsetLocation'))]
            check('collision anchor/offset '+c['source_root'],len(matches)==1)
            v=matches[0]
            check('collision extent '+c['source_root'],subset(c['radius_cm'],field(v,'Radius'))
                  and (c['shape']=='sphere' or subset(c['length_cm'],field(v,'Length'))))
            array_name='CapsuleLimits' if c['shape']=='capsule' else 'SphericalLimits'
            axis=next(x['axis_local'] for x in node['collision_axes_local'] if x['array']==array_name and x['index']==rows.index(v))
            check('collision native axis '+c['source_root'],subset(c['axis_local'],axis))
    return data

try:
    config=json.loads(CONFIG.read_text(encoding='utf-8')); config_sha=sha(CONFIG)
    check('source audit evidence unchanged',sha(DOCS/'research/SELESTIA_SOURCE_AUDIT.json')==config['source_audit_sha256'])
    check('target config exact',config['target_blueprint']==TARGET and config['root_count']==42)
    protected_paths=[SOURCE,MESH,'/Game/HarborCity/M5VS1/HeroSelestia/SK_Selestia']
    before={p:sha(disk(p)) for p in protected_paths}
    source=A.load_asset(SOURCE); mesh=A.load_asset(MESH)
    check('baseline assets loaded',source is not None and mesh is not None)
    R['config_sha256']=config_sha;R['protected_baseline_sha256']=before
    check('no dirty map before',not list(U.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    if A.does_asset_exist(TARGET) or disk(TARGET).exists():
        bp=A.load_asset(TARGET)
        check('existing target owned',bp is not None and A.get_metadata_tag(bp,'HarborCityOwnedBy')==OWNER)
        check('existing target same physics input',A.get_metadata_tag(bp,'PhysicsConfigSHA256')==config_sha)
        data=validate_readback(json.loads(U.HCM5VS2PhysicsEditor.read_physics_setup(bp)),config)
        R['repeat_action']='READ_ONLY_NO_SAVE'
    else:
        bp=A.duplicate_asset(SOURCE,TARGET)
        check('isolated AnimBP duplicated',bp is not None)
        result=U.HCM5VS2PhysicsEditor.apply_physics_setup(mesh,bp,CONFIG.read_text(encoding='utf-8'))
        data=json.loads(result);R['native_author']=data;write()
        check('native configure and compile',data.get('status')=='AUTHOR_CONFIGURED_NOT_RUNTIME_ACCEPTANCE',data.get('error'))
        validate_readback(data,config)
        A.set_metadata_tag(bp,'HarborCityOwnedBy',OWNER)
        A.set_metadata_tag(bp,'PhysicsConfigSHA256',config_sha)
        A.set_metadata_tag(bp,'PhysicsRuntimeAcceptance','NOT_RUN')
        check('save only new AnimBP',A.save_loaded_asset(bp,False))
        check('native asset saved',disk(TARGET).is_file())
        R['authored_assets']=[dict(path=TARGET,sha256=sha(disk(TARGET)),bytes=disk(TARGET).stat().st_size)]
        R['repeat_action']='CREATED_ONCE'
    R['native_readback']=validate_readback(json.loads(U.HCM5VS2PhysicsEditor.read_physics_setup(bp)),config)
    check('baseline mesh skeleton AnimBP disk unchanged',all(sha(disk(p))==h for p,h in before.items()))
    check('no dirty map after',not list(U.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
    R['reference_pose_overlaps']=config['reference_pose_collider_overlaps']
    R['limitations']=config['limitations']
    R['native_execution']='PASS';R['status']='PASS'
    R['pass_scope']='Only native new AnimBP configure/compile/reflected readback/save; movement and collision visual NOT_RUN.'
except Exception:
    R['status']='FAIL';R['native_execution']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    write()
if R['status']!='PASS': raise RuntimeError('Selestia physics author failed; see evidence; do not save unknown partial target')
