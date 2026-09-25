"""Assemble explicit, measured VS3 delivery evidence; never infer a test PASS."""
from pathlib import Path
import json,html,shutil,hashlib,zipfile,difflib
from PIL import Image,ImageDraw
W=Path(__file__).resolve().parents[1];D=W/'docs/HarborCity_M5_VS3';R=D/'review'
v=json.loads((D/'standalone_validation.json').read_text('utf-8-sig'))
delivery=json.loads((D/'DELIVERY.json').read_text('utf-8-sig'))
assert Path(delivery['executable']).is_file()
R.mkdir(exist_ok=True);(R/'appendix').mkdir(exist_ok=True);(R/'images').mkdir(exist_ok=True)
def copy(p,d):
 d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d)
def write(p,s):p.write_text(s.replace('\r\n','\n'),encoding='utf-8',newline='\n')
css='body{background:#101724;color:#eaf0fa;font:17px/1.65 system-ui;max-width:1160px;margin:40px auto;padding:0 22px}a{color:#95d6ff}h1,h2{color:#ffe5ae}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(420px,1fr));gap:18px}img{width:100%;height:auto;border-radius:8px}figure{margin:0;background:#1a2638;padding:12px}figcaption{font-size:15px}code{overflow-wrap:anywhere}'
def page(title,body):return '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+title+'</title><style>'+css+'</style><h1>'+title+'</h1>'+body+'</html>'
shots=[];wing=[]
latest_by_mode={x['test_mode']:x['label'] for x in v['runs']}
for run in v['runs']:
 p=Path(run['results']);j=json.loads(p.read_text('utf-8-sig'))
 assert j['mode']==run['test_mode']
 copy(p,R/'appendix'/(run['label']+'_results.json'))
 if run.get('launch_path'):copy(Path(run['launch_path']),R/'appendix'/(run['label']+'_launch.json'))
 if run['label']!=latest_by_mode[run['test_mode']]:continue
 for s in sorted(p.parent.glob('*.png')):
  if s.name.startswith(('Arcane_','Wing','Sword','MagicGun','Spell','PunchImpact','FlightStamina','FlightFirstPerson','StaminaAutoLanding','DemoSaveLoad','RideCheckpoint','m5_story_complete','m5_reopened','Afternoon_Guild','Afternoon_Cafe','Night_Guild','Night_Cafe')):
   name=run['label']+'_'+s.name;copy(s,R/'images'/name)
   row=(name,s.stem)
   (wing if s.name.startswith('Wing') else shots).append(row)
def caption(t):
 for en,zh in [('Afternoon','午后'),('Night','夜晚'),('Hover','悬停'),('Forward','前飞'),('Boost','加速'),('Wing1','方案一'),('Wing2','方案二'),('Wing3','方案三'),('Arcane_FP','第一人称魔法阵'),('Arcane_TP','第三人称魔法阵'),('SwordFirstPerson','第一人称握剑：姿态仍待改进'),('SwordCombo','剑连击'),('SwordDraw','持剑'),('MagicGun','魔法枪'),('FlightStamina','飞行体力'),('StaminaAutoLanding','体力耗尽自动降落')]:t=t.replace(en,zh)
 return t.replace('_',' · ')
def gallery(rows):return '<div class="grid">'+''.join('<figure><a href="images/'+html.escape(n)+'"><img loading="lazy" src="images/'+html.escape(n)+'"></a><figcaption>'+html.escape(caption(t))+'。实机画面，待用户验收。</figcaption></figure>' for n,t in rows)+'</div>'
video_url='https://github.com/gx823/codex-learning/releases/download/'+delivery['release_tag']+'/M5_VS3_PLAYTHROUGH.mp4'
body='<p>PARTIAL · 战斗、魔法、翅膀和 BGM 的用户验收 PENDING。</p><p><a href="wings.html">翅膀候选对比</a> · <a href="STATUS.md">状态与操作入口</a> · <a href="'+video_url+'">公开版实机录像（仅音效）</a></p>'
for section in delivery['sections']:body+='<h2>'+html.escape(section['title'])+'</h2><p>'+html.escape(section['conclusion'])+'</p>'
body+=gallery(shots)+'<p>最短复测：'+html.escape(delivery['manual'])+'</p><p>新 EXE：<code>'+html.escape(delivery['executable'])+'</code></p>'
write(R/'index.html',page('海湾漫游 M5-VS3 · 审阅',body))
write(R/'wings.html',page('翅膀候选 · 原创三种排列','<p>三个方案使用同一原创立体羽毛，不是三个购买模型。请根据午后／夜晚与悬停／前飞／加速的实拍选择；尚未精修放行。</p><a href="index.html">返回审阅页</a>'+gallery(wing)))
for n in ['STATUS.md','CONTROLS.md','PERFORMANCE.md','ASSET_LICENSE_NOTICES.md','MAGIC_ART_DIRECTION.md','ARCANE_LINEWORK.svg','PURCHASE_CANDIDATES.md','NEXT_TOWN_LAYOUT.svg']:
 copy(D/n,R/n)
copy(D/'standalone_validation.json',R/'appendix'/'STANDALONE_VALIDATION.json')
copy(D/'DELIVERY.json',R/'appendix'/'DELIVERY.json')
for p in (D/'appendix').iterdir():
 if p.is_file() and p.suffix.lower() in ('.json','.png','.jpg'):copy(p,R/'appendix'/p.name)
allshots=shots+wing
if allshots:
 selected=allshots[:36];canvas=Image.new('RGB',(1000,165*((len(selected)+3)//4)),'#101724');draw=ImageDraw.Draw(canvas)
 for i,(name,label) in enumerate(selected):
  im=Image.open(R/'images'/name);im.thumbnail((250,140));x=i%4*250;y=i//4*165;canvas.paste(im,(x,y));draw.text((x+3,y+141),label[:31],fill='white')
 canvas.save(D/'standalone_contact.jpg',quality=86)
patch=[];changed=[]
with zipfile.ZipFile(W/'.private/M5_VS3/pre_vs3_source_config.zip') as before:
 for p in sorted((W/'HarborCity/Source').rglob('*')):
  if not p.is_file():continue
  rel=p.relative_to(W/'HarborCity').as_posix();old=before.read(rel) if rel in before.namelist() else b'';new=p.read_bytes()
  if old!=new:
   changed.append('HarborCity/'+rel);patch.extend(difflib.unified_diff(old.decode('utf-8-sig').splitlines(True),new.decode('utf-8-sig').splitlines(True),fromfile='before/'+rel,tofile='after/'+rel))
write(R/'appendix'/'SOURCE_CHANGES.patch',''.join(patch))
write(D/'source_changes.json',json.dumps(changed,indent=2))
archive=D/'M5_VS3_REVIEW.zip';assert not archive.exists(),'Preserve previous review'
items=[]
with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(R.rglob('*')):
  if not p.is_file():continue
  assert p.suffix.lower() in ('.html','.md','.json','.png','.jpg','.svg','.patch')
  rel=p.relative_to(R).as_posix();z.write(p,rel);items.append({'path':rel,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for item in items:assert hashlib.sha256(z.read(item['path'])).hexdigest()==item['sha256']
write(D/'REVIEW_INTEGRITY.json',json.dumps({'status':'PASS','zip':str(archive),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'files':items},indent=2))
print(json.dumps({'review':str(R/'index.html'),'zip':str(archive),'status':'ASSEMBLED_FROM_MEASURED_INPUTS'}))
