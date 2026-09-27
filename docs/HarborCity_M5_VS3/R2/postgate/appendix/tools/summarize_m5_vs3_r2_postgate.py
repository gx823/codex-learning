"""Bounded release evidence; never infer a passed test from process exit alone."""
from pathlib import Path
import json, re, shutil, zipfile, difflib, hashlib, html
W=Path('D:/科研学习/codex学习');P=W/'HarborCity';D=W/'docs/HarborCity_M5_VS3/R2';O=D/'postgate';A=O/'appendix'
A.mkdir(exist_ok=True);R=O/'review';R.mkdir(exist_ok=True)
def read(p):return json.loads(p.read_text('utf-8-sig'))
suite=read(O/'FINAL_SUITE.json')
summary={'status':'PARTIAL','suite_status':suite['status'],'executable':suite['executable'],'runs':[],'real_OS_mouse':'NOT_RUN_TOOL_BACKEND','user_acceptance':'PENDING'}
for run in suite['runs']:
 path=Path(run['path']);launch=read(path/'launch.json');user=Path(next(x.split('=',1)[1].strip(chr(34)) for x in launch['arguments'] if x.startswith('-UserDir=')));files=[p for p in user.glob('**/results.json') if p.parent.name.endswith('_'+run['mode'])];files.sort(key=lambda p:p.stat().st_mtime,reverse=True);row={**run}
 if files:
  raw=read(files[0]);checks=raw.get('checks',[]);row.update(test_status=raw.get('status'),failures=raw.get('failures'),checks=len(checks),failed_checks=[x for x in checks if x.get('status')!='PASS'])
  (A/(run['label']+'_CHECKS.json')).write_text(json.dumps({'source':str(files[0]),'kind':'STANDALONE_PACKAGE','input':'ENGINE_ACTION_AND_LOGIC_NOT_OS','checks':checks},ensure_ascii=False,indent=2),'utf-8')
 if (path/'adapter_samples.csv').exists():
  vals=[]
  for line in (path/'adapter_samples.csv').read_text('utf-8-sig').splitlines()[1:]:
   try:vals.append(float(line.split(',')[-2]))
   except ValueError:pass
  row['adapter_peak_MiB']=max(vals) if vals else None
  shutil.copy2(path/'adapter_samples.csv',A/(run['label']+'_GPU.csv'))
 for name in ['launch.json','exit.json','network_sample.json']:
  if (path/name).exists():shutil.copy2(path/name,A/(run['label']+'_'+name))
 log=(path/'game.log').read_text('utf-8',errors='replace') if (path/'game.log').exists() else ''
 lines=[l for l in log.splitlines() if re.search(r'R2_(WAVE|SWORD|VFX|FIXTURE)|\bFAIL\b|Fatal error',l)]
 (A/(run['label']+'_LOG_EXCERPT.txt')).write_text('\n'.join(lines[:120]+(['[...bounded excerpt...]'] if len(lines)>200 else [])+lines[120:200] if len(lines)<=200 else lines[:120]+['[...bounded excerpt...]']+lines[-80:]),'utf-8')
 row['logged_project_fx_spawns']={k:len(re.findall(re.escape(k),log)) for k in sorted(set(re.findall(r'R2_VFX_SPAWN system=[^\s]+\.(NS_\w+)',log)))}
 summary['runs'].append(row)
(A/'FINAL_RESULTS.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),'utf-8')
for name in ['INTERRUPTION_AUDIT.json','FX_SCOPE.json','DOWNLOADS.json']:
 if (O/name).exists():shutil.copy2(O/name,A/name)
# Keep all source changes reviewable, excluding secret crypto configuration.
parts=[];changed=[]
with zipfile.ZipFile(W/'.private/M5_VS3_R2/postgate_baseline/source_config.zip') as z:
 old={n:z.read(n) for n in z.namelist()}
 current={p.relative_to(P).as_posix():p for root in ['Source','Config'] for p in (P/root).rglob('*') if p.is_file() and p.suffix in ['.h','.cpp','.cs','.ini'] and 'Crypto' not in p.name}
 for name,p in current.items():
  new=p.read_bytes();before=old.get(name,b'')
  if new==before:continue
  changed.append(name);parts.extend(difflib.unified_diff(before.decode('utf-8-sig',errors='replace').replace('\r\n','\n').splitlines(True),new.decode('utf-8-sig',errors='replace').replace('\r\n','\n').splitlines(True),fromfile='a/'+name,tofile='b/'+name))
(A/'SOURCE_CHANGES.patch').write_text(''.join(parts),'utf-8');(A/'CHANGED_FILES.json').write_text(json.dumps(changed,ensure_ascii=False,indent=2),'utf-8')
# Actual packaged stills only. Rendered preview icons are images, never raw meshes/textures.
stills=next((Path(x['path']) for x in suite['runs'] if x['label']=='STANDALONE_WEAPON_STILLS'),None);cards=[]
images=R/'images';images.mkdir(exist_ok=True)
if stills:
 for p in sorted(stills.glob('**/*.png')):
  if p.name.startswith(('Armory_','SteamPaint_','FP_','TP_','Icon_')):
   shutil.copy2(p,images/p.name)
   if not p.name.startswith('Icon_'):cards.append((p.name,p.stem.replace('_',' ')))
body=''.join(f'<figure><img loading="lazy" src="images/{html.escape(n)}"><figcaption>{html.escape(t)}。新独立包截图，造型与手感待用户确认。</figcaption></figure>' for n,t in cards)
rows=''.join(f'<tr><td>{html.escape(x["label"])}</td><td>{html.escape(str(x.get("test_status","NOT_RUN")))}</td><td>{x.get("failures","未知")}</td></tr>' for x in summary['runs'])
(R/'index.html').write_text('''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>HarborCity R2 门后候选</title><style>body{max-width:1100px;margin:40px auto;padding:0 24px;background:#111b29;color:#e6ecf4;font:18px/1.7 system-ui}img,video{width:100%;border-radius:12px}figure{margin:32px 0}figcaption{color:#bbcbdf}a{color:#83c9ff}td,th{padding:10px;text-align:left}code{overflow-wrap:anywhere}</style><h1>武器库与战斗修复 · PARTIAL</h1><p>四把武器、八套蒸汽铳涂装已接入。剑击判定和重击剑气有引擎输入证据；画面与操作仍待试玩确认。</p><p>已购 31 个特效与 27 个音效尚未接入，星弓尚未上线。浏览器／桌面工具故障阻断下载与动作取得。当前原创 Niagara 临时效果不算购买包完成。</p><p>魔导枪晶核造型、原作者剑贴图、倒地质量和完整慢放对照仍有缺口。具体失败保留在报告。</p><p><a href="../STATUS.md">结论和剩余问题</a> · <a href="../ARMORY.md">武器一览</a> · <a href="../CONTROLS.md">操作</a></p><video controls preload="metadata" src="../M5_VS3_R2_POSTGATE.mp4"></video>'''+body+'<details><summary>验证附录</summary><table><tr><th>项目</th><th>状态</th><th>失败项</th></tr>'+rows+'</table><p>逻辑／引擎输入和真实操作系统键鼠分开；真实鼠标 NOT_RUN。完整数据见 appendix/FINAL_RESULTS.json。</p></details></html>','utf-8')
print(json.dumps({'executable':suite['executable'],'runs':[(x['label'],x.get('failures')) for x in summary['runs']],'changed_source_files':len(changed),'stills':len(cards)},ensure_ascii=False))
