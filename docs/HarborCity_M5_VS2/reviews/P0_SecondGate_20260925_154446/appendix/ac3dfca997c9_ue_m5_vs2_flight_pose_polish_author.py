"""New asymmetric suspended flight poses; retain selected hero/hair/wings/modesty/gameplay."""
from pathlib import Path
import hashlib,json,re,traceback,copy
import unreal as U
W=Path('D:/科研学习/codex学习');P=W/'HarborCity';D=W/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary
args=re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(args)==1
O=Path(args[0][0] or args[0][1]).resolve();assert O.is_relative_to(D/'editor_runtime')
root='/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_'+hashlib.sha256(str(O).encode()).hexdigest()[:12]
R=dict(status='RUNNING',phase='FlightPosePolishAuthor',assets=[],source_hashes={},checks=[],runtime='NOT_RUN',art='USER_REVIEW')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pkg(o):return (o.get_path_name() if isinstance(o,U.Object) else o).split('.')[0]
def disk(o):return P/'Content'/Path(pkg(o).removeprefix('/Game/')).with_suffix('.uasset')
def keep(o):p=disk(o);R['source_hashes'][str(p)]=sha(p)
def check(n,v):
    R['checks'].append(dict(name=n,status='PASS' if v else 'FAIL'))
    if not v:raise RuntimeError(n)
def native(n,s):
    d=json.loads(s);check(n,d.get('status')=='PASS');return d
def dupe(o,name):
    target=root+'/'+name;check('fresh asset '+name,not A.does_asset_exist(target));x=A.duplicate_asset(pkg(o),target);check('duplicate '+name,x is not None);return x
try:
    s=D/'editor_runtime/author_20260925_124709_707_bdbd7477_HeroPolishAuthor/author_result.json';data=json.loads(s.read_text('utf-8-sig'));check('selected hero passed authoring',data['status']=='PASS')
    R['source_hashes'][str(s)]=sha(s)
    for x in data['assets']:check('hero inputs unchanged',sha(disk(x['path']))==x['sha256']);keep(x['path'])
    source=A.load_asset(data['blueprint']);animsource=A.load_asset(data['animation']);old=U.get_default_object(source.generated_class());oldbody=old.get_editor_property('mesh')
    idle=A.load_asset('/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS');keep(idle)
    anim=dupe(animsource,'ABP_FlightPosePolish');before=native('existing graph cloned',U.HCM5VS2FlightPoseEditor.inspect_flight_graph(anim))
    clips={x['name']:x['sequence'] for x in before['nodes'] if 'sequence' in x}
    takeoff=A.load_asset(clips['VS2Flight_Clip0']);landing=A.load_asset(clips['VS2Flight_Clip6']);keep(takeoff);keep(landing)
    names=['Hover','Forward','Boost','Ascending','Descending'];loops=[dupe(idle,'FlightPoses/A_Flight_'+name) for name in names]
    R['native_poses']=native('new original poses authored with bone-length readback',U.HCM5VS2FlightPoseEditor.author_flight_loops(idle,loops,True,False))
    R['graph_edit']=native('replace five flight loops only',U.HCM5VS2FlightPoseEditor.configure_flight_graph(anim,loops,takeoff,landing))
    after=native('existing graph inspected after rebinding',U.HCM5VS2FlightPoseEditor.inspect_flight_graph(anim));expected=copy.deepcopy(before)
    for row in expected['nodes']:
        for i in range(5):
            if row['name']=='VS2Flight_Clip'+str(i+1):row['sequence']=pkg(loops[i])
    check('all flight graph links, transitions, bank and wind nodes unchanged',after==expected);R['native_graph']=after
    bp=dupe(source,'BP_FlightPosePolish');hero=U.get_default_object(bp.generated_class());body=hero.get_editor_property('mesh');combat=hero.get_component_by_class(U.HCM4CombatComponent)
    hero.modify();body.modify();combat.modify();body.set_editor_property('anim_class',anim.generated_class());combat.set_editor_property('player_animation_blueprint',anim.generated_class())
    check('compile new hero',B.compile_blueprint(bp));hero=U.get_default_object(bp.generated_class());body=hero.get_editor_property('mesh')
    check('same geometry and proportions',body.get_skeletal_mesh_asset()==oldbody.get_skeletal_mesh_asset() and body.get_relative_transform()==oldbody.get_relative_transform())
    check('new hero uses authored graph',body.get_editor_property('anim_class')==anim.generated_class())
    for obj in loops+[anim,bp]:
        check('save new asset',A.save_loaded_asset(obj,False));R['assets'].append(dict(path=pkg(obj),sha256=sha(disk(obj))))
    R.update(status='PASS',blueprint=pkg(bp),animation=pkg(anim),source_blueprint=data['blueprint'],source_animation=data['animation'],
        scope='Five original flight loops only: relaxed elbows, asymmetric knees, pointed feet. Takeoff/landing, walking, combat, banking, input, hair anchors, safety clothing and wings retained from selected HeroPolish.')
except Exception:R.update(status='FAIL',error=traceback.format_exc())
R['changed_sources']=[p for p,h in R['source_hashes'].items() if sha(p)!=h]
if R['changed_sources']:R['status']='FAIL'
(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')
if R['status']!='PASS':raise RuntimeError(R.get('error','Source changed'))
