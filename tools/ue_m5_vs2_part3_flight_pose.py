"""Fresh candidate flight poses, bound only to the current private town."""
from pathlib import Path
import json,re,hashlib,traceback,shutil
import unreal as U
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2/part3';P=W/'HarborCity'
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());O=Path(m[1] or m[2])
root='/Game/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_'+hashlib.sha256(str(O).encode()).hexdigest()[:12]
A=U.EditorAssetLibrary;B=U.BlueprintEditorLibrary;L=U.get_editor_subsystem(U.LevelEditorSubsystem)
R={'status':'RUNNING','assets':[],'unchanged':'capsule, input, movement limits, clothing and hair constraints'}
def pkg(x):return x.get_path_name().split('.')[0]
def copy(x,n):
    y=A.duplicate_asset(pkg(x),root+'/'+n);assert y;return y
def native(v):
    j=json.loads(v);assert j.get('status')=='PASS',j;return j
try:
    s=json.loads((D/'town_selection.json').read_text('utf-8-sig'));assert L.load_level(s['map'])
    world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world();settings=world.get_world_settings()
    oldmode=settings.get_editor_property('default_game_mode');oldhero=U.get_default_object(oldmode).get_editor_property('default_pawn_class')
    source=A.load_asset(pkg(oldhero));cdo=U.get_default_object(oldhero);oldbody=cdo.get_editor_property('mesh')
    oldanim=A.load_asset(pkg(oldbody.get_editor_property('anim_class')));anim=copy(oldanim,'ABP_TownFlight')
    graph=native(U.HCM5VS2FlightPoseEditor.inspect_flight_graph(anim));clips={x['name']:x['sequence'] for x in graph['nodes'] if 'sequence' in x}
    idle=A.load_asset('/Game/HarborCity/M5VS2/HeroSelestia/Animation/GAS/Retargeted/M_Relaxed_Stand_Idle_Loop_InPlace_SelestiaGAS')
    loops=[copy(idle,'FlightPoses/'+n) for n in ['Hover','Forward','Boost','Ascending','Descending']]
    R['pose_authoring']=native(U.HCM5VS2FlightPoseEditor.author_flight_loops(idle,loops,True,False,True))
    R['graph_binding']=native(U.HCM5VS2FlightPoseEditor.configure_flight_graph(anim,loops,A.load_asset(clips['VS2Flight_Clip0']),A.load_asset(clips['VS2Flight_Clip6'])))
    hero=copy(source,'BP_TownHero');h=U.get_default_object(hero.generated_class());body=h.get_editor_property('mesh')
    body.set_editor_property('anim_class',anim.generated_class());h.get_component_by_class(U.HCM4CombatComponent).set_editor_property('player_animation_blueprint',anim.generated_class());assert B.compile_blueprint(hero)
    mode=copy(A.load_asset(pkg(oldmode)),'BP_TownGameMode');U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',hero.generated_class());assert B.compile_blueprint(mode)
    for x in loops+[anim,hero,mode]:assert A.save_loaded_asset(x,False);R['assets'].append(pkg(x))
    src=P/'Content'/Path(s['map'][6:]).with_suffix('.umap');shutil.copy2(src,O/'before_flight_binding.umap')
    settings.set_editor_property('default_game_mode',mode.generated_class());assert L.save_current_level()
    R.update(status='PASS',map=s['map'],hero=pkg(hero),animation=pkg(anim),game_mode=pkg(mode),runtime='NOT_RUN',visual='USER_REVIEW')
    (D/'flight_pose_selection.json').write_text(json.dumps({k:R[k] for k in ['hero','animation','game_mode','map']},ensure_ascii=False,indent=2),'utf-8')
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:(O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
