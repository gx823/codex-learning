"""Combine verified wing material and opt-in hair output correction in a new hero.
No existing assets/maps edited; runtime integration and art remain pending.
"""
import datetime as dt, hashlib, json, re, traceback
from pathlib import Path
import unreal as U

W=Path('D:/科研学习/codex学习');P=W/'HarborCity';D=W/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary
def arg(n):
    rows=re.findall(r'(?:^|\s)-'+n+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(rows)==1
    return rows[0][0] or rows[0][1]
O=Path(arg('M5EvidenceDir'));assert O.is_relative_to(D/'editor_runtime') and arg('M5AuthorPhase')=='HeroPolishAuthor'
token=hashlib.sha256(str(O).encode()).hexdigest()[:12];root='/Game/HarborCity/M5VS2/HeroPolish/Batch_'+token
R=dict(status='RUNNING',phase='HeroPolishAuthor',destination=root,assets=[],runtime='NOT_RUN',art='USER_REVIEW',checks=[])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def file(package):return P/'Content'/Path(package.removeprefix('/Game/')).with_suffix('.uasset')
def package(obj):return obj.get_path_name().split('.')[0]
def dump():(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n','utf-8')
def check(n,v):
    R['checks'].append(dict(name=n,status='PASS' if v else 'FAIL'));dump()
    if not v:raise RuntimeError(n)
protected={}
try:
    wing_report=D/'editor_runtime/author_20260925_111152_236_8e5908e1_FlightWingMaterialReload/author_result.json'
    wing=json.loads(wing_report.read_text('utf-8-sig'));check('wing material native source passed',wing['status']=='PASS')
    source=A.load_asset(wing['blueprint']);check('source hero exists',source is not None)
    for row in wing['assets']:
        p=file(row['path']);check('verified wing assets unchanged',sha(p)==row['sha256']);protected[str(p)]=sha(p)
    old=U.get_default_object(source.generated_class());oldbody=old.get_editor_property('mesh')
    anim_package=package(oldbody.get_editor_property('anim_class'));anim_source=A.load_asset(anim_package)
    protected[str(file(anim_package))]=sha(file(anim_package))
    check('private hero destination unused',not A.does_asset_exist(root+'/BP_HeroPolish'))
    anim=A.duplicate_asset(anim_package,root+'/ABP_HeroPolish');check('native ABP duplicate',anim is not None)
    native=json.loads(U.HCM5VS2PhysicsEditor.anchor_private_hero_hair(anim));R['hair_readback']=native
    check('two long-hair anchors persist in compiled defaults',native.get('status')=='PASS' and native.get('anchored_groups')==2)
    bp=A.duplicate_asset(package(source),root+'/BP_HeroPolish');check('native hero duplicate',bp is not None)
    hero=U.get_default_object(bp.generated_class());body=hero.get_editor_property('mesh');combat=hero.get_component_by_class(U.HCM4CombatComponent)
    for obj in (hero,body,combat):obj.modify()
    body.set_editor_property('anim_class',anim.generated_class());combat.set_editor_property('player_animation_blueprint',anim.generated_class())
    check('compile hero with new animation',B.compile_blueprint(bp))
    hero=U.get_default_object(bp.generated_class());body=hero.get_editor_property('mesh');combat=hero.get_component_by_class(U.HCM4CombatComponent)
    check('actual body and combat animation bound',body.get_editor_property('anim_class')==anim.generated_class() and combat.get_editor_property('player_animation_blueprint')==anim.generated_class())
    check('same geometry and body proportions',body.get_skeletal_mesh_asset()==oldbody.get_skeletal_mesh_asset() and body.get_relative_transform()==oldbody.get_relative_transform())
    for obj in (anim,bp):
        check('native save new private asset',A.save_loaded_asset(obj,False));p=file(package(obj))
        R['assets'].append(dict(path=package(obj),sha256=sha(p),bytes=p.stat().st_size))
    R.update(status='PASS',blueprint=package(bp),animation=package(anim),source_blueprint=package(source),
             source_animation=anim_package,changes=['Two long-hair output anchors only; fixed step remains on','Retains verified soft wing material and safety clothing'],
             movement='Original selected locomotion unchanged; FootPlacement candidate remains unselected because sliding worsened')
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    changed=[p for p,h in protected.items() if sha(p)!=h];R['changed_sources']=changed;R['source_hashes']=protected
    if changed:R['status']='FAIL'
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS':raise RuntimeError('Hero polish author failed; preserve private partial assets')
