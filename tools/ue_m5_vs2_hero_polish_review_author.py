"""Refresh the established four-light portrait/full-body fixtures to the selected saved hero."""
from pathlib import Path
import copy,hashlib,json,re,traceback
import unreal as U
W=Path('D:/科研学习/codex学习');P=W/'HarborCity';D=W/'docs/HarborCity_M5_VS2'
A=U.EditorAssetLibrary;L=U.get_editor_subsystem(U.LevelEditorSubsystem)
def arg(n):
 v=re.findall(r'(?:^|\s)-'+n+r'=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line());assert len(v)==1;return v[0][0] or v[0][1]
O=Path(arg('M5EvidenceDir'));assert O.is_relative_to(D/'editor_runtime') and arg('M5AuthorPhase')=='HeroPolishReviewAuthor'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def doc(p):return json.loads(Path(p).read_text('utf-8-sig'))
def pkg(x):return (x.get_path_name() if hasattr(x,'get_path_name') else str(x)).split('.')[0]
def disk(x,ext='.uasset'):return P/'Content'/Path(pkg(x).removeprefix('/Game/')).with_suffix(ext)
def bind(p,kind,**more):p=Path(p);return dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size,kind=kind,**more)
R=dict(status='RUNNING',phase='HeroPolishReviewAuthor',assets=[],plans={},checks=[],protected={})
def check(n,ok):
 R['checks'].append(dict(name=n,status='PASS' if ok else 'FAIL'))
 if not ok:raise RuntimeError(n)
try:
 hf=next(p for p in sorted((D/'editor_runtime').glob('author_*_FlightPosePolishAuthor/author_result.json'),key=lambda p:p.stat().st_mtime,reverse=True) if doc(p)['status']=='PASS')
 hero=doc(hf);bp=hero['blueprint'];kind=A.load_blueprint_class(bp);check('saved selected hero',kind is not None)
 hero_sources=[bind(hf,'evidence')]
 for row in hero['assets']:
  f=disk(row['path']);check('actual selected hero source unchanged',sha(f)==row['sha256']);R['protected'][str(f)]=sha(f);hero_sources.append(bind(f,'package',package=pkg(row['path'])))
 pairs=[('Portrait','author_20260925_074146_675_00a52268_CornerVTwoHeroPortrait'),('FullBody','author_20260925_074202_375_2c48b7b0_CornerVTwoHeroFullBody')]
 for name,oldfolder in pairs:
  oldfile=D/'editor_runtime'/oldfolder/'corner_v2_capture_plan.json';plan=copy.deepcopy(doc(oldfile));src=plan['map']
  token=hashlib.sha256((str(O)+name).encode()).hexdigest()[:12];root='/Game/HarborCity/M5VS2/WorldRev2/Review_'+token;dest=root+'/L_CornerRevTwoReview'
  sources=[]
  for row in plan['source_bindings']:
   f=Path(row['path'])
   if row['kind']=='package':check('original fixture asset unchanged',sha(f)==row['sha256']);R['protected'][str(f)]=sha(f)
   # Original report records old code versions; this new plan binds today's actual code/module.
   sources.append(bind(f,row['kind'],**({'package':row['package']} if 'package' in row else {})))
  sources.extend(hero_sources+[bind(oldfile,'evidence'),bind(__file__,'script')])
  check('unique review map',not A.does_asset_exist(dest));check('native fixture copy',A.duplicate_asset(src,dest) is not None);check('load new fixture',L.load_level(dest))
  world=U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world();settings=world.get_world_settings();oldmode=pkg(settings.get_editor_property('default_game_mode'))
  R['protected'][str(disk(oldmode))]=sha(disk(oldmode));mode=A.duplicate_asset(oldmode,root+'/BP_ReviewGameMode');check('private mode copy',mode is not None)
  U.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class',kind)
  check('compile actual hero binding',U.BlueprintEditorLibrary.compile_blueprint(mode));check('save private mode',A.save_loaded_asset(mode,False))
  settings.set_editor_property('default_game_mode',mode.generated_class());check('save new fixture',L.save_current_level())
  for asset,ext in [(dest,'.umap'),(pkg(mode),'.uasset')]:
   f=disk(asset,ext);row=bind(f,'package',package=asset);sources.append(row);R['assets'].append(dict(path=asset,sha256=row['sha256']))
  plan['map']=dest;plan['selected_hero']=bp;plan['hero']['selected_blueprint']=bp;plan['hero']['blueprint_class']=kind.get_path_name()
  plan['source_bindings']=list({x['path']:x for x in sources}.values())
  out=O/name;out.mkdir();file=out/'corner_v2_capture_plan.json';file.write_text(json.dumps(plan,ensure_ascii=False,indent=2),'utf-8');R['plans'][name]=str(file)
 R.update(status='PASS',blueprint=bp,scope='Existing four-time rendered review fixtures, selected saved hair/cloth/halo/flight-pose hero. No material/time/camera policy changes. Runtime NOT_RUN.')
except Exception:R.update(status='FAIL',error=traceback.format_exc());U.log_error(R['error'])
finally:
 R['changed_sources']=[p for p,h in R['protected'].items() if sha(p)!=h]
 if R['changed_sources']:R['status']='FAIL'
 (O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),'utf-8')
if R['status']!='PASS':raise RuntimeError('Polished hero review author failed')
