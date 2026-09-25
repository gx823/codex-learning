"""Explicit source/report allowlist. Does not commit, push, or upload by itself."""
from pathlib import Path
import json,hashlib,subprocess,shutil,re
W=Path(__file__).resolve().parents[1];D=W/'docs/HarborCity_M5_VS3';T=W/'.publish/harborcity-m5-vs3-20260926'
delivery=json.loads((D/'DELIVERY.json').read_text('utf-8-sig'));G=D/'github';G.mkdir(exist_ok=True)
assert T.is_dir() and (T/'.git').is_file()
def git(*args):return subprocess.check_output(['git','-c','gc.auto=0','-c','maintenance.auto=false',*args],cwd=T,text=True).strip()
assert not git('status','--porcelain'), 'Review existing worktree edits first'
head=git('rev-parse','HEAD')
files=list(json.loads((D/'source_changes.json').read_text('utf-8-sig')))
files += ['HarborCity/Config/DefaultEngine.ini','HarborCity/Config/DefaultGame.ini','HarborCity/HarborCity.uproject','docs/PROGRESS.md']
files += [p.relative_to(W).as_posix() for p in (D/'review').rglob('*') if p.is_file()]
for n in ['STATUS.md','CONTROLS.md','MAGIC_ART_DIRECTION.md','PURCHASE_CANDIDATES.md','NEXT_TOWN_LAYOUT.svg','DELIVERY.json','REVIEW_INTEGRITY.json','arcane_design_contact.png','standalone_contact.jpg']:
 if (D/n).exists():files.append((D/n).relative_to(W).as_posix())
files += [p.relative_to(W).as_posix() for p in (W/'tools').glob('*m5_vs3*') if p.is_file() and p.suffix in ('.py','.ps1')]
files += ['tools/build.ps1','tools/normalize_response_files.py']
private=(W/'HarborCity/Config/DefaultCrypto.ini').read_text('utf-8-sig')
keys=re.findall(r'(?m)^EncryptionKey=(.+)$',private);assert len(keys)==1
secret=keys[0].strip().encode(); del private,keys
records=[]
for name in sorted(set(files)):
 p=(W/name).resolve();assert p.is_relative_to(W.resolve()) and p.is_file()
 assert p.suffix.lower() in ('.cpp','.h','.cs','.ini','.uproject','.py','.ps1','.md','.html','.json','.svg','.png','.jpg','.patch')
 assert not any(x in name.lower() for x in ('/content/','/private/','.private/','defaultcrypto','defaultencryption','/package_work/','/recordings/'))
 data=p.read_bytes();assert secret not in data,'Private configuration leaked into public allowlist'
 q=T/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
 records.append({'path':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
del secret
ignore=T/'.gitignore';text=ignore.read_text('utf-8')
extra='\n# M5-VS3 private material\n.private/\nHarborCity/Content/HarborCity/Private/Music/\ndocs/HarborCity_M5_VS3/private/\n'
if '# M5-VS3 private material' not in text:ignore.write_text(text+extra,encoding='utf-8')
records.append({'path':'.gitignore','size':ignore.stat().st_size,'sha256':hashlib.sha256(ignore.read_bytes()).hexdigest()})
assets=[]
for n in ['M5_VS3_REVIEW.zip','M5_VS3_PLAYTHROUGH.mp4','REVIEW_INTEGRITY.json']:
 p=D/n;assert p.is_file()
 assets.append({'path':str(p),'name':n,'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
plan={'repository':'gx823/codex-learning','worktree':str(T),'base_commit':head,'release_tag':delivery['release_tag'],'release_name':'HarborCity M5-VS3 · 剑与魔法候选（PARTIAL）','release_body':delivery['release_body'],'files':records,'release_assets':assets}
(G/'upload_plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
subprocess.run(['git','-c','gc.auto=0','-c','maintenance.auto=false','add','-f','--',*[x['path'] for x in records]],cwd=T,check=True)
print(json.dumps({'status':'STAGED_FOR_REVIEW','files':len(records),'base_commit':head,'worktree':str(T)}))
