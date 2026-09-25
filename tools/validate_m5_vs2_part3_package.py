"""Serial, bounded validation of one actual VS2 candidate. Esc/abort stops the chain."""
from pathlib import Path
import argparse,json,subprocess,re,shutil,datetime,sys
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2/part3'
ap=argparse.ArgumentParser();ap.add_argument('package');ap.add_argument('--resume',action='store_true');ap.add_argument('--retry-initial-mesh-guard',action='store_true');ap.add_argument('--retry-indoor-ground-fixture',action='store_true');ap.add_argument('--demo-only',action='store_true');a=ap.parse_args()
package=json.loads(Path(a.package).read_text('utf-8-sig'));assert package['status']=='PASS'
exe=Path(package['actual_executable']);assert exe.is_file() and '/M5_VS2/Candidate_' in exe.as_posix()
pwsh=shutil.which('pwsh');assert pwsh
state={'status':'RUNNING','executable':str(exe),'package_record':a.package,'runs':[],'os_input':'NOT_RUN'}
out=D/'standalone_validation.json'
if out.exists():
 assert a.resume,'Explicit reviewed resume required'
 old=json.loads(out.read_text('utf-8-sig'));assert old['status']=='STOPPED' or (a.demo_only and old['status']=='READY_FOR_FOREGROUND_DEMO') or ((a.retry_initial_mesh_guard or a.retry_indoor_ground_fixture) and old['status']=='COMPLETE')
 if a.retry_initial_mesh_guard or a.retry_indoor_ground_fixture:
  blocked=next(r for r in old['runs'] if r['mode']=='m5_full')
  checks=json.loads(Path(blocked['results']).read_text('utf-8-sig'))['checks']
  failed=[x for x in checks if x['status']=='FAIL']
  if a.retry_initial_mesh_guard:assert len(failed)==1 and failed[0]['name']=='M5 actual purchased Selestia player mesh'
  else:assert any('dialogue approach grounded placement' in x['name'] and 'among16 radial samples' in x['actual'] for x in failed)
  continuity=json.loads((D/'BUILD_CONTINUITY.json').read_text('utf-8-sig'))
  assert continuity['status']=='PASS_QA_ONLY_DELTA' and continuity['new_executable']==str(exe)
  state['evidence_reuse']='Only legacy mesh assertions and indoor grounded test placement changed. Previous candidate runtime/media retained with individual launch EXE paths; retry blocked regression on new candidate.'
 archive=D/('standalone_validation_attempt_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'.json');shutil.copy2(out,archive)
 state['history']=old.get('history',[])+[str(archive)]
 # Preserve completed performance samples; retry only the failed initial-focus soak.
 state['runs']=[r for r in old['runs'] if 'results' in r and r['status'] in ('COMPLETE','COMPLETE_WITH_FAILURES') and not (r['mode']=='soak' and r['status']=='COMPLETE_WITH_FAILURES') and not (not a.demo_only and r['mode']=='m5_full' and r['status']=='COMPLETE_WITH_FAILURES') and not (a.retry_indoor_ground_fixture and r['mode']=='demo') and not (r['mode']=='perf' and r['quality']=='High' and Path(r['directory']).name=='perf_20260925_191214')]
def save():out.write_text(json.dumps(state,ensure_ascii=False,indent=2),'utf-8')
save()
try:
 for mode,quality,seconds in [('soak','High',0),('perf','High',0),('perf','Epic',0),('m5_full','High',0),('smoke','High',0),('beauty','High',0),('demo','High',340)]:
  if any(r['mode']==mode and r['quality']==quality for r in state['runs']):continue
  if mode=='demo' and a.retry_indoor_ground_fixture:
   state['status']='READY_FOR_FOREGROUND_DEMO';save();print('READY_FOR_FOREGROUND_DEMO',flush=True);sys.exit(0)
  before=set(D.glob(mode+'_*'));cmd=[pwsh,'-NoProfile','-File',str(W/'tools/run_m5_vs2_part3.ps1'),'-Mode',mode,'-Quality',quality,'-Executable',str(exe),'-RecordSeconds',str(seconds)]
  item={'mode':mode,'quality':quality,'status':'RUNNING','started':datetime.datetime.now().isoformat()};state['runs'].append(item);save();print('START '+mode+' '+quality,flush=True)
  with (D/('standalone_'+mode+'_'+quality+'.console.log')).open('wb') as f:p=subprocess.run(cmd,stdout=f,stderr=f)
  item['helper_exit_code']=p.returncode;dirs=set(D.glob(mode+'_*'))-before;dirs=[x for x in dirs if x.is_dir() and (x/'launch.json').is_file()]
  assert len(dirs)==1,'Missing or ambiguous actual launch'
  run=dirs[0];item['directory']=str(run);end=json.loads((run/'exit.json').read_text('utf-8-sig'));item['launch']=end
  log=(run/'game.log').read_text('utf-8-sig',errors='replace');finished=re.findall(r'M1_TEST_FINISHED (.+)',log)
  if end['status']!='EXITED' or not finished:raise RuntimeError('Runtime did not finish normally; no next desktop launch')
  results=Path(finished[-1].strip())/'results.json';j=json.loads(results.read_text('utf-8-sig'));item['results']=str(results);item['result_status']=j['status']
  if j['status']=='USER_ABORTED' or any(x.get('user_stop_latched') is True for x in j.values() if isinstance(x,dict)):raise RuntimeError('User stop: desktop chain stopped')
  if any(x.get('status')=='FAIL' and 'focus' in x.get('name','').lower() for x in j.get('checks',[])):raise RuntimeError('Game focus test failed; normal foreground recovery required before another launch')
  item['status']='COMPLETE_WITH_FAILURES' if any(x.get('status')=='FAIL' for x in j.get('checks',j.get('results',[]))) else 'COMPLETE'
  evidence=D/'standalone_results'/(mode+'_'+quality+'_'+run.name);evidence.mkdir(parents=True,exist_ok=False)
  shutil.copy2(results,evidence/'results.json')
  if mode=='soak':
   for image in results.parent.glob('*.png'):shutil.copy2(image,evidence/image.name)
  excerpts=[line for line in log.splitlines() if re.search(r'M1_TEST_(FINISHED|FAIL)|Fatal error|Assertion failed|LogNet:.*(Listen|socket)|UdpMessaging.*(Initializing|Transport)',line)]
  (evidence/'runtime_excerpt.log').write_text('\n'.join(excerpts)+'\n','utf-8')
  if mode=='beauty':
   shots=D/'BEAUTY_SHOTS';shots.mkdir(exist_ok=False)
   for img in results.parent.glob('*.png'):shutil.copy2(img,shots/img.name)
   item['beauty_count']=len(list(shots.glob('*.png')))
  if mode=='demo':
   captures=sorted((D/'recordings').glob('*/capture.json'),key=lambda x:x.stat().st_mtime);item['capture']=str(captures[-1]) if captures else None
  save();print('END '+mode+' '+quality+' '+item['status'],flush=True)
 state['status']='COMPLETE';save()
except Exception as e:
 state['status']='STOPPED';state['error']=str(e);save();print('STOPPED: '+str(e),flush=True);sys.exit(1)
