"""Bind each result to its actual standalone launch and isolated save slot."""
from pathlib import Path
import argparse,json
W=Path(__file__).resolve().parents[1];D=W/'docs/HarborCity_M5_VS3'
ap=argparse.ArgumentParser();ap.add_argument('run',type=Path);ap.add_argument('--capture',type=Path);a=ap.parse_args()
run=a.run.resolve();assert run.is_relative_to(D.resolve())
launch=json.loads((run/'launch.json').read_text('utf-8-sig'));exitinfo=json.loads((run/'exit.json').read_text('utf-8-sig'))
assert launch['kind']=='STANDALONE_PACKAGE' and exitinfo['status']=='EXITED'
mode=next(x.split('=',1)[1] for x in launch['arguments'] if x.startswith('-M5Test='));slot=next(x.split('=',1)[1] for x in launch['arguments'] if x.startswith('-HCM1SaveSlot='))
found=[]
for root in [Path.home()/'AppData/Local/HarborCity/Saved/M5Tests',Path(launch['executable']).parents[2]/'Saved/M5Tests']:
 if not root.is_dir():continue
 for p in root.glob('*_'+mode+'/results.json'):
  r=json.loads(p.read_text('utf-8-sig'))
  if r['save_slot']==slot:found.append((p,r))
assert len(found)==1,f'Expected one result for actual slot; got {len(found)}'
p,r=found[0];label=run.name
entry={'label':label,'mode':label.rsplit('_',2)[0],'test_mode':mode,'launch':launch,'launch_path':str(run/'launch.json'),'results':str(p),'result_status':r['status'],'failures':r['failures'],'exit_code':exitinfo.get('exit_code')}
if a.capture:
 c=a.capture.resolve();assert c.is_relative_to((D/'recordings').resolve())
 assert '-M5PublicCapture' in launch['arguments']
 assert c.stat().st_mtime>=p.parent.stat().st_ctime-5
 entry['capture']=str(c)
path=D/'standalone_validation.json';v=json.loads(path.read_text('utf-8-sig')) if path.exists() else {'status':'COMPLETE','meaning':'Evidence collection complete for listed runs, not all-tests PASS','runs':[]}
assert not any(x['label']==label for x in v['runs'])
v['runs'].append(entry);path.write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'label':label,'status':r['status'],'failures':r['failures'],'failed_checks':[c['name'] for c in r['checks'] if c['status']!='PASS']},ensure_ascii=True))
