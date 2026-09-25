"""Build concise review from actual candidate/runtime artifacts, with detailed JSON appendix."""
from pathlib import Path
import json,shutil,html,zipfile,hashlib,re,csv
from PIL import Image,ImageDraw
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2';P=D/'part3'
v=json.loads((P/'standalone_validation.json').read_text('utf-8-sig'));assert v['status']=='COMPLETE'
package=json.loads(Path(v['package_record']).read_text('utf-8-sig'));assert package['status']=='PASS'
video=P/'M5_VS2_TOWN_PLAYTHROUGH.mp4';assert video.is_file()
vr=json.loads(video.with_suffix('.json').read_text('utf-8-sig'));exe=v['executable']
R=P/'review';assert not R.exists();R.mkdir();A=R/'appendix';A.mkdir();S=R/'BEAUTY_SHOTS';S.mkdir()
shots=sorted((P/'BEAUTY_SHOTS').glob('*.png'));assert len(shots)>=30
for p in shots:shutil.copy2(p,S/p.name)
for p in (P/'standalone_results').rglob('*'):
 if p.is_file():t=A/'runtime'/p.relative_to(P/'standalone_results');t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,t)
summary=[];perf=[]
for row in v['runs']:
 data=json.loads(Path(row['results']).read_text('utf-8-sig'));checks=data['checks'];fails=[x for x in checks if x['status']=='FAIL']
 summary.append({'mode':row['mode'],'quality':row['quality'],'status':row['status'],'executable':row['launch']['executable'],'duration_seconds':data['duration_seconds'],'checks':len(checks),'failed':len(fails),'failures':fails,'results':row['results']})
 if row['mode']=='perf':
  raw=A/'performance'/row['quality'];raw.mkdir(parents=True)
  shutil.copy2(Path(row['directory'])/'adapter_samples.csv',raw/'adapter_samples.csv')
  shutil.copy2(Path(row['directory'])/'launch.json',raw/'launch.json')
  with (Path(row['directory'])/'game.log').open(encoding='utf-8-sig',errors='replace') as stream:
   settings=[line.strip() for line in stream if re.search(r'(sg\.(Foliage|Shading|Landscape|GlobalIllumination)Quality|r\.ScreenPercentage|r\.VSync|t\.MaxFPS)',line)]
  (raw/'settings_excerpt.log').write_text('\n'.join(settings[-45:])+'\n','utf-8')
  values=[]
  for x in checks:
   m=re.search(r'frames=(\d+) mean_fps=([\d.]+) p99_ms=([\d.]+) wall_seconds=([\d.]+)',x['actual'])
   if m:values.append(dict(label=x['name'],frames=int(m[1]),mean_fps=float(m[2]),p99_ms=float(m[3]),wall_seconds=float(m[4]),status=x['status']))
  rows=list(csv.reader((Path(row['directory'])/'adapter_samples.csv').read_text('utf-8-sig').splitlines()))[1:]
  mem=[float(x[2].strip()) for x in rows if len(x)>=4]
  perf.append(dict(quality=row['quality'],segments=values,gpu=rows[0][1].strip() if rows else None,gpu_util_average=sum(float(x[3]) for x in rows)/len(rows) if rows else None,adapter_peak_gib=max(mem)/1024 if mem else None,scope='GPU total used memory at 5s intervals, includes other applications; not exact process-local peak',raw=str(Path(row['directory'])/'adapter_samples.csv')))
appendix={'status':'PARTIAL','package':package,'runtime':summary,'performance':perf,'video':vr,'source_input_layers':{'A':'explicit placement, camera framing, runtime assertions','B':'Enhanced Input player actions; real physics within each shot','C':'NOT_RUN: OS keyboard/mouse response and subjective quality require user'},'uncompleted':['adult proportion/wardrobe approval','cloth-floor penetration and four recovery orientations','live vehicle-on-pedestrian footage','NPC dialogue footage','stray hair final determination','stride and start/stop visual approval','fully furnished interiors and authored lighthouse/temple','fantasy dialogue portraits/music/vehicle/weapon art','complete sidequest walkthrough','all-zone normal continuous-route performance'],'previous_failures_retained':True}
(A/'EVIDENCE.json').write_text(json.dumps(appendix,ensure_ascii=False,indent=2),'utf-8')
warnings=[]
for row in v['runs']:
 if row['mode'] not in ('demo','m5_full'):continue
 with (Path(row['directory'])/'game.log').open(encoding='utf-8-sig',errors='replace') as stream:
  for line in stream:
   if 'VS2_DRIVING_HANDS_CANDIDATE_INVALID' in line or 'Unable to find RecastNavMesh' in line:warnings.append(row['mode']+' '+line.strip())
(A/'runtime_warnings.log').write_text('\n'.join(warnings)+'\n','utf-8')
for name in ['BUILD_CONTINUITY.json','VIDEO_REVIEW.json','NETWORK_FINDINGS.json','AUDIO_FINDINGS.json','RUNTIME_OPEN_ISSUES.json','ENCRYPTION_CHECK.json','HIGH_PRELIMINARY_SCOPE.json','HARDWARE.json','town_selection.json','flight_pose_selection.json']:
 shutil.copy2(P/name,A/name)
for path in v.get('history',[]):shutil.copy2(path,A/Path(path).name)
soak=next(x for x in summary if x['mode']=='soak')
soak_ok=soak['duration_seconds']>=1800 and not any('30 minute' in x['name'] for x in soak['failures'])
soak_text='标准点光源完成 30 分钟运行，未退出崩溃：规避成功，根因未知。' if soak_ok else '补光浸泡未达到验收条件，详见附录；历史根因仍未知。'
status=f'''# M5-VS2 港町独立候选 · PARTIAL

- **A 阻断项：PARTIAL。** J/T/U/W 已从街上移除，使用 Q/R/V 候选成年底模；成年比例和服装尚待确认，衣物穿地、四种起身和真实车撞录像未完成。倒车旧失败来自测试继续加油撞墙后反弹，编辑器输入层持续倒行检查已通过，车辆参数未改。HUD 标题与 H 面板完成。两种 GAS 待机已接入；发丝、滑步仍待视觉验收。{soak_text}
- **B 港町：PARTIAL。** A 方向已扩到约 230 米核心，保留原街角，加入陆地、街区、河桥、山坡、风车、公会与咖啡馆入口及内部、19 名 NPC 和新导航底图。主体建筑缩放 0.75，主角维持 1.25。精装室内、神殿、灯塔、地面过渡和密度仍不足。
- **C 飞行：PARTIAL。** 前飞/加速前倾与屈膝加强，安全衣层及原规则保留；自然度与高速碰撞需要用户继续复测。
- **D 世界观：未完成。** 新奇幻 BGM、立绘对话、复古魔导车与魔导手枪尚未制作；旧合成 BGM 已从此地图播放绑定中移除，录像使用真实游戏声效。

新 EXE：`{exe}`

补光、性能和截图来自本轮前一独立候选 `194722_644_45d4aae0`；最终候选仅修正旧网格检查、室内测试定位和诊断输出。配置、资产及其他源码的差异核对见附录 BUILD_CONTINUITY.json；回归和正式录像使用最终候选。

**回归仍有阻断：** 公会对话已执行，随后第一章后的存档读取检查失败；四章主线、完整支线与存读档尚未通过。主角读数为统一 1.25 倍、绑定尺度为 1，旧骨骼数量检查因增加三个 IK 骨骼而失败，比例美术验收仍待进行。驾驶席手部姿态与 CrowdManager 导航另有运行警告，见 RUNTIME_OPEN_ISSUES.json。

网络：UDP/TCP Messaging 已停用；Development 构建的 Unreal Trace TCP 1985 仍在监听，防火墙相关要求尚未全部完成。未修改系统防火墙或引擎源码，详见附录 NETWORK_FINDINGS.json。

录像：`{video}`。保持实际时间与游戏音轨，已看到驾驶、飞行及高空第一人称画面；区间间有明确定位取景，不能当作无中断全程路线或 OS 键鼠验收。拳击未命中，战斗近景有遮挡；NPC 对话、真实车撞与四种起身的专项录像仍缺。

请亲手检查：视角/驾驶/飞行手感、主角发丝与脚步、成年外观与倒地；完成四章主线、送咖啡与载客、存读档。主角/NPC/环境/飞行/BGM 用户验收均为 PENDING。

截图：`BEAUTY_SHOTS/`；逐项实际结果、旧失败和采样限制见 `appendix/EVIDENCE.json`。
'''
(R/'STATUS.md').write_text(status,'utf-8');(P/'STATUS.md').write_text(status,'utf-8')
perf_md='# 港町性能抽测\n\n1920×1080、100% 渲染比例、VSync 关闭、无帧率上限，无录像/截图。独立包，分段真实输入路线；不是全区连续路线验收。\n\n|画质/区间|平均 FPS|p99 ms|\n|---|---:|---:|\n'
for q in perf:
 for s in q['segments']:perf_md+=f"|{q['quality']} / {s['label']}|{s['mean_fps']:.2f}|{s['p99_ms']:.2f}|\n"
for q in perf:
 perf_md+=f"\n{q['quality']} 整卡显存采样峰值：{q['adapter_peak_gib']:.2f} GiB（含其他程序，5 秒采样）。\n" if q['adapter_peak_gib'] is not None else '\n显存 NOT_RUN。\n'
perf_md+='\n硬件：'+str(perf[0]['gpu'])+'。高 GPU 利用率仅支持 GPU 负载较重的判断，不能代替 GPU/CPU 专项剖析。未关闭 Lumen 或删除 NPC/植被。样本满足阈值也不等于全港町性能放行，仍需覆盖夜间灯群、两处室内与所有区域的连续驾驶路线。\n'
(R/'PERFORMANCE.md').write_text(perf_md,'utf-8')
body=['<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>海湾漫游 · 港町候选</title><style>body{background:#16222b;color:#f1eee5;font:17px/1.65 system-ui;max-width:1400px;margin:auto;padding:32px}h1{font-size:36px}section{margin:36px 0}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:18px}img{width:100%;display:block}figure{margin:0;background:#243640;padding:10px}figcaption{font-size:14px}a{color:#9fdddd}</style><h1>海湾漫游 · 港町候选</h1><p>PARTIAL · 可玩候选，画面尚未全部达标。</p><p>已扩建街区与陆地，接入新飞行姿态、待机及游戏声效。衣物倒地、滑步、服装、精装室内与奇幻 BGM 仍待完成。</p><p>'+html.escape(soak_text)+'</p><p><a href="STATUS.md">结论与剩余问题</a> · <a href="PERFORMANCE.md">性能抽测</a> · <a href="appendix/EVIDENCE.json">技术附录</a></p>']
captions={'Corner':'保留原街角，继续补足立面生活细节。','Guild':'公会建筑与入口；内部仍待精装。','GuildInside':'公会内部，家具与陈设尚显简陋。','Cafe':'港口咖啡馆入口。','CafeInside':'咖啡馆内部，尚未达到精装要求。','Market':'集市街与连续立面；空地仍偏多。','Bridge':'运河与石桥。','Windmill':'坡地、民居与风车。','Temple':'旧神殿所在区域，地标仍需完善。','Aerial':'高空观察陆地与港町；岸线和布局仍待美术验收。'}
for period,title in [('Afternoon','午后'),('Dusk','黄昏'),('Night','夜晚')]:
 body+=['<section><h2>'+title+'</h2><div class="grid">']
 for p in shots:
  if p.stem.startswith(period+'_'):
   name=p.stem[len(period)+1:];body+=['<figure><a href="BEAUTY_SHOTS/'+p.name+'"><img loading="lazy" src="BEAUTY_SHOTS/'+p.name+'"></a><figcaption>'+captions.get(name,name)+'</figcaption></figure>']
 body+=['</div></section>']
body+=['<p>原图来自本次独立包；录像单独下载。源码和测量记录见附录，用户验收保持 PENDING。</p></html>'];(R/'index.html').write_text('\n'.join(body),'utf-8')
contact=Image.new('RGB',(1280,((len(shots)+3)//4)*200));dr=ImageDraw.Draw(contact)
for i,p in enumerate(shots):
 im=Image.open(p);assert im.size==(1920,1080);im.thumbnail((320,180));x=i%4*320;y=i//4*200;contact.paste(im,(x,y));dr.text((x+3,y+181),p.stem,fill='white')
contact.save(P/'standalone_contact.jpg')
controls=(D/'M5_VS2_CONTROLS.md').read_text('utf-8-sig');(P/'CONTROLS_BEFORE.md').write_text(controls,'utf-8')
controls=controls.replace('（第二次阶段门开发中）','（港町独立候选）').replace('当前适用于独立保存的 VS2 样板地图；尚无新的 VS2 发布包。旧版本 EXE 不包含本轮全部修改。','当前适用于本次新港町独立候选。旧版本 EXE 不包含本轮全部修改。').replace('当前只按约 50×50 m 样板及外延 100 m 的软边界测试，不代表全港町已完成。','当前按约 230×230 m 港町核心及外延软边界运行；全路线仍待验收。')
controls+='\nT：切午后/黄昏/夜晚。H：展开/收起左上操作说明，默认收起。\n\n本轮独立包：`'+exe+'`。所有用户手感与美术验收 PENDING。\n'
(D/'M5_VS2_CONTROLS.md').write_text(controls,'utf-8');shutil.copy2(D/'M5_VS2_CONTROLS.md',R/'CONTROLS.md')
progress=W/'docs/PROGRESS.md';old=progress.read_text('utf-8-sig');progress.write_text('2026-09-25 M5-VS2 第三段：PARTIAL 独立候选已生成；A/B/C 部分完成、D 待办；实际新 EXE：'+exe+'。'+soak_text+' 实际测试与美术不足见 HarborCity_M5_VS2/part3/STATUS.md。旧 FAIL/NOT_RUN 保留；M0 PASS；主角/NPC/环境/飞行/BGM 用户验收 PENDING。\n\n'+old,'utf-8')
delivery={'status':'PARTIAL_USER_REVIEW','exe':exe,'page':str(R/'index.html'),'review_directory':str(R),'zip':str(P/'M5_VS2_TOWN_REVIEW.zip'),'video':str(video),'integrity':str(P/'REVIEW_INTEGRITY.json')}
(P/'DELIVERY.json').write_text(json.dumps(delivery,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps(delivery,ensure_ascii=False))
