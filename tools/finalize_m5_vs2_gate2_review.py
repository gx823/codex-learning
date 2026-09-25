"""Local link/original-byte validation and one ZIP integrity pass; no browser workaround."""
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import unquote,urlparse
import argparse,datetime as dt,hashlib,json,zipfile
ap=argparse.ArgumentParser();ap.add_argument('directory');a=ap.parse_args()
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2';O=Path(a.directory).resolve()
assert O.is_relative_to(D/'reviews') and (O/'index.html').is_file()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,v):Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n','utf-8')
class Links(HTMLParser):
 def __init__(self):super().__init__();self.links=[];self.ids=set()
 def handle_starttag(self,tag,attrs):
  d=dict(attrs)
  if d.get('id'):self.ids.add(d['id'])
  for key in ('src','href'):
   if d.get(key):self.links.append((tag,d[key]))
parser=Links();parser.feed((O/'index.html').read_text('utf-8'))
local=[];external=[]
for tag,url in parser.links:
 parsed=urlparse(url)
 if parsed.scheme in ('http','https'):external.append(url);continue
 if url.startswith('#'):assert url[1:] in parser.ids;continue
 target=(O/unquote(parsed.path)).resolve();assert target.is_file(),target
 assert target.is_relative_to(D),target
 local.append(dict(tag=tag,url=url,path=str(target),bytes=target.stat().st_size))
inv=json.loads((O/'GATE2_EVIDENCE.json').read_text('utf-8'))
for row in inv['sources']:
 if row.get('review_file'):
  assert sha(O/row['review_file'])==row['sha256'],row
browser=dict(status='BLOCKED_BROWSER_URL_POLICY',render_test='NOT_RUN',reason='Browser use URL policy rejected navigation to this new local file. No alternate browser/server/CDP route attempted.',attempted_url=(O/'index.html').as_uri())
dump(O/'appendix/BROWSER_CHECK.json',browser)
links=dict(status='PASS_LOCAL_LINK_AND_SOURCE_BYTES_ONLY',local_links=local,external_links_not_refetched=sorted(set(external)),browser_visual='NOT_RUN',original_media_bytes_retained=True)
dump(O/'appendix/LOCAL_LINK_CHECK.json',links)
inv['browser_check']=browser;inv['local_link_check']=dict(status=links['status'],file='appendix/LOCAL_LINK_CHECK.json');dump(O/'GATE2_EVIDENCE.json',inv)
video=inv['video']['file']
status='''# 第二次阶段门：等待用户审阅，未全部放行

- 主角：发丝根部修复、原作描边、光环与新飞行姿态已接入；四时段近景和全身已补拍。第一人称与驾驶手部已有专项图。滑步、历史补光 Renderer 崩溃仍未解决。
- NPC：八名角色有面部与全身图，Q 暖肤色有同场对照，三种生活待机已实际播放。倒地衣物穿地及 J 的一次起身超时仍未关闭；完整拳击／枪击／真实车撞／四向起身视频未完成。
- 环境：仍为原街角，三时段与高空图可审。A 已深化；B/C 仅商店参考，未购买、未下载、UE5.8 与包体待核查。没有扩图。
- 飞行：起飞、悬停、视角切换、绕行、俯冲拉升、落地有真实引擎输入录像。已审角度有安全衣层覆盖；全部极端镜头、真实键鼠手感、正式有声视频仍待验证。
- 门前保留地图检查：视角、战斗和旧主线通过；基础玩法“刹车后倒车”的限时检查失败，原因待查。不是新港町完整玩法或独立包验收。
- 浏览器安全策略拒绝打开本地审阅页，显示检查为 NOT_RUN；原图、文件链接及审阅 ZIP 做本地校验。

审阅图与结论：`'''+str(O/'index.html')+'''`。

飞行视频：`'''+video+'''`（原生音轨静音；单独保留）。

请决定继续深化 A，或先进一步核查 B/C。环境选型不取消上述失败，也不等于授权购买。没有新的 VS2 独立包。
'''
(O/'STATUS.md').write_text(status,'utf-8')
# One entry-by-entry verification; ZipFile.read validates CRC as well as our original-byte comparison.
archive=O.with_suffix('.zip');assert not archive.exists()
members={p.relative_to(O).as_posix():dict(sha256=sha(p),bytes=p.stat().st_size) for p in O.rglob('*') if p.is_file()}
assert not any(Path(n).suffix.lower() in ('.exe','.dll','.uasset','.umap','.mp4') for n in members)
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name in sorted(members):z.write(O/name,name)
with zipfile.ZipFile(archive) as z:
 assert set(z.namelist())==set(members)
 for name,row in members.items():
  data=z.read(name);assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],name
integrity=dict(status='PASS',zip=str(archive),zip_sha256=sha(archive),bytes=archive.stat().st_size,members=members,contains_engine_or_full_project=False,contains_game_package=False,video_separate=video,verified_at=dt.datetime.now().isoformat())
ip=O.parent/(O.name+'_ZIP_INTEGRITY.json');dump(ip,integrity)
# Append a concise current record; every prior byte of PROGRESS is retained.
progress=W/'docs/PROGRESS.md';old=progress.read_bytes()
header=('2026-09-25 第二次阶段门审阅资料已生成，状态 USER_REVIEW／未全部放行。主角最新四时段与全身、Q 同场肤色、八 NPC、三种待机、R5 环境与新飞行录像已入审阅包。倒地衣物、J 起身超时、滑步、历史 Renderer 崩溃保留；门前旧地图基础倒车限时检查新增 FAIL，其余所跑视角／战斗／主线检查 PASS。浏览器显示检查因本地 URL 策略 BLOCKED，文件与 ZIP 校验 PASS。M0 PASS；无 VS2 新 EXE，不扩图，等待用户审阅。页面：'+str(O/'index.html')+'；ZIP：'+str(archive)+'。\n\n').encode('utf-8')
progress.write_bytes(header+old)
dump(D/'SECOND_GATE_DELIVERY.json',dict(status='USER_REVIEW_NOT_ALL_REQUIREMENTS_PASS',page=str(O/'index.html'),zip=str(archive),integrity=str(ip),video=video,desktop_work_stopped_after_delivery=True,no_new_vs2_exe=True,environment_expansion=False))
print(json.dumps(dict(status='PASS_FILES_AND_ZIP_ONLY',page=str(O/'index.html'),zip=str(archive),zip_bytes=archive.stat().st_size,files=len(members),browser='NOT_RUN_URL_POLICY'),ensure_ascii=True))
