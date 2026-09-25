"""Curate public source/review, excluding licensed source assets, keys and game archives."""
from pathlib import Path
import json,re,shutil,hashlib,subprocess,zipfile
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2';P=D/'part3'
T=W/'.publish/harborcity-m5-vs2-town-20260925';assert (T/'.git').is_file()
delivery=json.loads((P/'DELIVERY.json').read_text('utf-8-sig'));R=Path(delivery['review_directory'])
v=json.loads((P/'standalone_validation.json').read_text('utf-8-sig'));package=json.loads(Path(v['package_record']).read_text('utf-8-sig'))
tag='harborcity-m5-vs2-town-'+Path(package['candidate_directory']).name[len('Candidate_'):].replace('_','-')
release='https://github.com/gx823/codex-learning/releases/tag/'+tag
O=P/'github';O.mkdir(exist_ok=True)
files=list(R.rglob('*'));files=[p for p in files if p.is_file()]
files+=list((W/'HarborCity/Source/HarborCity/M5VS2').glob('*.cpp'))+list((W/'HarborCity/Source/HarborCity/M5VS2').glob('*.h'))
files += [W/'HarborCity/Source/HarborCityEditor'/n for n in ['AnimGraphNode_HCM5VS2PlacementInput.h','HarborCityEditor.cpp','HCM5VS2NPCJClothingEditor.cpp','HCM5VS2NPCJClothingEditor.h']]
for stem in ['M1/HCM1PlayerController','M1/HCM1Vehicle','M1/HCM1TestRunner','M2/HCM2SceneSettings']:
 files+=[W/'HarborCity/Source/HarborCity'/(stem+x) for x in ('.h','.cpp')]
files += [W/'HarborCity/Source/HarborCity'/p for p in ['HarborCity.Build.cs','M1/HCM1HUD.cpp','M1/HCM1TestM5.cpp','M1/HCM1TestM5VS2.cpp','M3/HCM3Recording.cpp','M3/HCM3EditorNavigation.cpp','M4R1/HCM4R1VehicleImpactComponent.cpp','M4R2/HCM4R2Navigation.cpp','M4R2/HCM4R2NavigationAuthor.cpp']]
files += [p for p in (W/'tools').glob('*') if p.is_file() and p.suffix in ('.py','.ps1') and ('part3' in p.name or p.name in ['package_m5_vs2.ps1','m5_vs2_release_guards.ps1','verify_m5_vs2_selestia_encryption.py','build_m5_vs2.ps1'])]
files += [D/'M5_VS2_CONTROLS.md',D/'WORKFLOW_OVERRIDE_20260925.md',P/'DELIVERY.json',P/'STATUS.md',P/'ASSET_LICENSE_NOTICES.md']
patterns={
 'github_token':rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b',
 'openai_key':rb'\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{40,}\b',
 'private_key':rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
 'literal_secret':rb'(?i)["\x27](?:access_token|client_secret|api_key|password|encryptionkey)["\x27]\s*[:=]\s*["\x27][A-Za-z0-9+/=_-]{24,}["\x27]',
 'ini_crypto':rb'(?im)^EncryptionKey=[A-Za-z0-9+/=]{32,}' }
manifest=[];hits=[]
for p in sorted(set(files)):
 assert p.is_file() and p.is_relative_to(W) and not p.is_symlink(),p
 assert p.suffix.lower() not in ('.exe','.dll','.uasset','.umap','.vrm','.fbx','.unitypackage','.zip','.mp4','.key','.ini'),p
 b=p.read_bytes();assert len(b)<100*1024**2
 if p.suffix.lower() not in ('.png','.jpg','.jpeg'):
  hits += [dict(file=p.relative_to(W).as_posix(),pattern=n) for n,rx in patterns.items() if re.search(rx,b)]
 t=T/p.relative_to(W);t.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,t)
 manifest.append(dict(path=t.relative_to(T).as_posix(),sha256=hashlib.sha256(b).hexdigest(),size=len(b)))
(O/'content_scan.json').write_text(json.dumps({'status':'PASS' if not hits else 'BLOCKED','hits':hits,'scope':'Explicit public allowlist; common credential signatures; no raw Content, paid source, config, save, engine, full game or binary'},indent=2),'utf-8')
assert not hits,'Potential secret signature; inspect filenames only'
readme=f'''# HarborCity M5-VS2 · 港町独立候选

**PARTIAL，用户验收 PENDING。** 本轮已有新独立游戏候选；完整游戏包仅留开发机，不上传 GitHub。

- [状态与未完成项](part3/STATUS.md)
- [审阅页与原图](part3/review/index.html)（下载 ZIP 后本地打开）
- [审阅 ZIP 与有声录像]({release})
- [操作说明](M5_VS2_CONTROLS.md)
- [第二次阶段门历史资料](reviews/P0_SecondGate_20260925_154446/STATUS.md)

新包实际路径：`{delivery['exe']}`。此路径只适用于用户本机。
编辑器和独立包证据分开；引擎输入不代表真实 OS 键鼠。旧 FAIL/NOT_RUN 未删除。
只公开本轮报告、截图及相关项目源码；不含授权模型源文件、Content、第三方插件、引擎、完整游戏或存档，因此不是完整可编译工程。
'''
(T/'docs/HarborCity_M5_VS2/README.md').write_text(readme,'utf-8')
cmd=['git','-c','gc.auto=0','-c','maintenance.auto=false']
subprocess.run(cmd+['add','--','HarborCity/Source','tools','docs/HarborCity_M5_VS2'],cwd=T,check=True,stdout=subprocess.DEVNULL)
diff=subprocess.check_output(cmd+['diff','--cached','--','HarborCity/Source','tools'],cwd=T)
(R/'appendix/changes.diff').write_bytes(diff);(T/'docs/HarborCity_M5_VS2/part3/review/appendix/changes.diff').write_bytes(diff)
z=Path(delivery['zip']);assert not z.exists()
with zipfile.ZipFile(z,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
 for p in sorted(R.rglob('*')):
  if p.is_file():archive.write(p,p.relative_to(R).as_posix())
with zipfile.ZipFile(z) as archive:
 assert archive.testzip() is None;names=archive.namelist();assert 'index.html' in names
for link in re.findall(r'(?:src|href)="([^"]+)"',(R/'index.html').read_text('utf-8-sig')):
 if not link.startswith(('https:','#')):assert (R/link).is_file(),link
integrity={'status':'PASS','zip':str(z),'bytes':z.stat().st_size,'sha256':hashlib.sha256(z.read_bytes()).hexdigest(),'entries':len(names),'all_html_links_local_and_present':True,'paid_source_included':False}
Path(delivery['integrity']).write_text(json.dumps(integrity,ensure_ascii=False,indent=2),'utf-8')
shutil.copy2(Path(delivery['integrity']),T/'docs/HarborCity_M5_VS2/part3/REVIEW_INTEGRITY.json')
assets=[dict(path=str(p),name=p.name,size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in (z,Path(delivery['video']),Path(delivery['integrity']))]
plan={'repository':'gx823/codex-learning','worktree':str(T),'release_tag':tag,'release_url':release,'files':manifest,'release_assets':assets,'release_body':readme,'release_name':'HarborCity M5-VS2 · 港町独立候选（PARTIAL）'}
(O/'upload_plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),'utf-8')
subprocess.run(cmd+['add','--','docs/HarborCity_M5_VS2'],cwd=T,check=True,stdout=subprocess.DEVNULL)
print(json.dumps({'status':'PREPARED','files':len(manifest),'zip':str(z),'release':release},ensure_ascii=False))
