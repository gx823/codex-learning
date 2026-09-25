"""Five original pose loops plus copied candidate ABP; root executes native UE.

Phases FlightPoseCandidate -> FlightPoseReload. No Hero or world asset writes.
Depends on actual GASMotionReload / GASTransitionRetargetReload evidence.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK=Path('D:/科研学习/codex学习').resolve();DOC=WORK/'docs/HarborCity_M5_VS2';PROJECT=WORK/'HarborCity'
ROOT='/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion'
IDLE='/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS'
A=U.EditorAssetLibrary;NAMES=['Hover','Forward','Boost','Ascending','Descending']


def arg(name):
    rows=re.findall(r'(?:^|\s)-'+name+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
    if len(rows)!=1:raise RuntimeError('Exactly one '+name+' required')
    return rows[0][0] or rows[0][1]


OUT,PHASE=Path(arg('M5EvidenceDir')).resolve(),arg('M5AuthorPhase')
assert OUT.is_relative_to(DOC) and not (OUT/'author_result.json').exists()
assert PHASE in ('FlightPoseCandidate','FlightPoseReload')
OUT.mkdir(parents=True,exist_ok=True);DEST=ROOT+'/Batch_'+hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
R=dict(status='RUNNING',phase=PHASE,destination=DEST,assets=[],checks=[],inputs={},
       runtime='NOT_RUN',visual='USER_REVIEW',clothing_low_angle='NOT_RUN',hero_binding='NOT_RUN',map_writes=0,
       started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),limitations=[
           'Component-direction pose synthesis is original choreography based on actual existing Idle frame 0; not motion capture or Mixamo.',
           'Raw local lengths, hand/finger/face posture and skirt-bone local rotations are preserved; parent motion and solver response still require visual review.',
           'Existing skirt 25 degree cone and two leg capsules remain; this does not prove coverage at all camera angles.',
           'Kawaii wind limits are cm/s position drives: hair18, ribbon22, skirt horizontal2; gravity and collider definitions are unchanged.',
           'FP/combat retain existing branches. Actual game, focus, flight controls, timing and pose/cloth transitions remain untested here.'])
PROTECTED={}


def dump():(OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
def check(label,good,observed=None):
    R['checks'].append(dict(name=label,status='PASS' if good else 'FAIL',observed=observed));dump()
    if not good:raise RuntimeError(label+': '+repr(observed))
def package(obj):return (obj.get_path_name() if hasattr(obj,'get_path_name') else str(obj)).split('.')[0]
def disk(obj):
    p=package(obj);assert p.startswith('/Game/HarborCity/') and '..' not in p
    return PROJECT/'Content'/Path(p.removeprefix('/Game/')).with_suffix('.uasset')
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()
def protect(obj,expected=None):
    p=disk(obj);value=sha(p);check('source hash '+package(obj),expected is None or value==expected);PROTECTED[str(p)]=value
def load(path):
    obj=A.load_asset(path);check('load '+package(path),obj is not None);return obj
def native(label,value):
    data=json.loads(value);check(label,data.get('status')=='PASS',data.get('error'));return data
def latest(phase):
    for file in sorted((DOC/'editor_runtime').glob('author_*_'+phase+'/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True):
        data=json.loads(file.read_text('utf-8-sig'))
        if data.get('status')=='PASS' and data.get('phase')==phase:
            R['inputs'][phase]=dict(path=str(file),sha256=sha(file));return data
    raise RuntimeError('No actual PASS '+phase)
def duplicate(obj,name):
    path=DEST+'/'+name;check('fresh isolated output '+path,not A.does_asset_exist(path) and not disk(path).exists())
    result=A.duplicate_asset(package(obj),path);check('native duplicate '+path,result is not None);return result
def save(obj):
    path=package(obj);check('only isolated flight candidates saved',path.startswith(DEST+'/'))
    A.set_metadata_tag(obj,'HarborCityOwnedBy','HarborCity_M5VS2_OriginalFlightPose')
    check('native save '+path,A.save_loaded_asset(obj,False) and disk(path).is_file())
    R['assets'].append(dict(path=path,sha256=sha(disk(path)),bytes=disk(path).stat().st_size));dump()


try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    motion=latest('GASMotionReload');transition=latest('GASTransitionRetargetReload')
    check('actual isolated GAS motion set',len(motion['assets'])==7 and motion['candidate']['live_hero_unchanged'])
    check('actual completed retarget set',len(transition['assets'])==11 and len(transition['target_audits'])==10)
    for row in motion['assets']+transition['assets']:protect(row['path'],row['sha256'])
    for root in ('M5VS1/HeroSelestia','M5VS2/HeroSelestia','M5VS2/HeroRev2'):
        for file in (PROJECT/'Content/HarborCity'/root).rglob('*.uasset'):PROTECTED[str(file)]=sha(file)
    for file in (PROJECT/'Content').rglob('*.umap'):PROTECTED[str(file)]=sha(file)
    for file in (PROJECT/'Config').glob('*.ini'):PROTECTED[str(file)]=sha(file)
    source=load(IDLE);protect(source)
    source_bp=load(motion['candidate']['blueprint']);mesh=load(motion['candidate']['mesh'])
    space=load(motion['candidate']['blendspace']);original_space=load('/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/BS_M5VS2_GAS_IdleWalkRun')
    takeoff=load(transition['destination']+'/Animations/M_Relaxed_Jump_F_Start_Stand_Rfoot_Cut_InPlace_SelestiaTransition')
    landing=load(transition['destination']+'/Animations/M_Relaxed_Jump_F_Land_Stand_Light_Rfoot_Cut_InPlace_SelestiaTransition')
    if PHASE=='FlightPoseReload':
        prior=latest('FlightPoseCandidate');DEST=prior['destination'];R['destination']=DEST
        check('actual private flight candidate',re.fullmatch(ROOT+r'/Batch_[0-9a-f]{12}',DEST) is not None and len(prior['assets'])==6)
        for row in prior['assets']:protect(row['path'],row['sha256'])
        bp=load(prior['candidate']['blueprint']);loops=[load(DEST+'/FlightPoses/A_Flight_'+name) for name in NAMES]
        R['assets']=prior['assets'];R['candidate']=prior['candidate']
        R['native_poses']=native('all native pose keys reloaded',U.HCM5VS2FlightPoseEditor.author_flight_loops(source,loops,False,True))
        R['native_graph']=native('native flight graph reload',U.HCM5VS2FlightPoseEditor.inspect_flight_graph(bp))
        check('native flight graph connections persisted',R['native_graph']==prior['native_graph'])
    else:
        check('fresh private flight directory',not A.does_directory_exist(DEST) and not disk(DEST+'/placeholder').parent.exists())
        loops=[duplicate(source,'FlightPoses/A_Flight_'+name) for name in NAMES]
        R['native_plan']=native('actual source pose basis and loop plan',U.HCM5VS2FlightPoseEditor.author_flight_loops(source,loops,False,False))
        R['native_poses']=native('native five loops author and readback',U.HCM5VS2FlightPoseEditor.author_flight_loops(source,loops,True,False))
        bp=duplicate(source_bp,'ABP_Selestia_GASFlight')
        R['graph_author']=native('native flight branch and bounded wind',U.HCM5VS2FlightPoseEditor.configure_flight_graph(bp,loops,takeoff,landing))
        R['native_graph']=native('native flight graph inspection',U.HCM5VS2FlightPoseEditor.inspect_flight_graph(bp))
        R['candidate']=dict(mesh=package(mesh),skeleton=motion['candidate']['skeleton'],blueprint=package(bp),blendspace=package(space),
                            material_overrides_from_hero=motion['candidate']['material_overrides_from_hero'],live_hero_unchanged=True,
                            poses={name:package(loop) for name,loop in zip(NAMES,loops)},takeoff=package(takeoff),landing=package(landing))
    R['retained_gas_graph']=native('existing GAS samples/branches preserved',U.HCM5VS2GASMotionEditor.inspect_motion_candidate(bp,mesh,original_space,space))
    check('all original 28 GAS samples identical',R['retained_gas_graph']['samples']==motion['native_inspection']['samples'])
    check('all original 247 raw bones and 11 physics nodes retained',R['retained_gas_graph']['raw_bones_exact']==247 and R['retained_gas_graph']['legacy_kawaii']==11)
    if PHASE=='FlightPoseCandidate':
        for obj in loops+[bp]:save(obj)
    R['status']='PASS'
except Exception:R['status']='FAIL';R['exception']=traceback.format_exc()
finally:
    changed=[file for file,value in PROTECTED.items() if not Path(file).is_file() or sha(file)!=value]
    R['source_preservation']=dict(files=len(PROTECTED),changed_files=changed)
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Flight poses failed; preserve all unique candidates and logs')
