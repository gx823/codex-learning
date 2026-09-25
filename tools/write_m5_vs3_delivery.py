"""Write the final concise review from completed local evidence."""
from pathlib import Path
import json,shutil
from PIL import Image,ImageDraw
W=Path(__file__).resolve().parents[1];D=W/'docs/HarborCity_M5_VS3';A=D/'appendix'
candidate='Candidate_20260926_052401_469_58434958'
exe='E:\\GameDev\\Builds\\HarborCity\\M5_VS3\\'+candidate+'\\Archive\\Windows\\HarborCity\\Binaries\\Win64\\HarborCity-Win64-Shipping.exe'
assert Path(exe).is_file()
v=json.loads((D/'standalone_validation.json').read_text('utf-8-sig'))
video=json.loads((D/'M5_VS3_PLAYTHROUGH.json').read_text('utf-8-sig'))
perf=json.loads((A/'PERFORMANCE_MEASUREMENTS.json').read_text('utf-8-sig'))
assert video['status']=='ENCODED' and video['audio']['signal_present']
tag='harborcity-m5-vs3-20260926-052401-469-58434958'
release='https://github.com/gx823/codex-learning/releases/tag/'+tag
sections=[
 {'title':'A · 阻断修复（PARTIAL）','conclusion':'第一章读档失败已修复：安全位置查询误把隐藏装饰组件合入玩家体积。主线四章、咖啡支线已走通，新进程与真实 F9 恢复通关状态。接送仍无法找到可行走的车门落脚点；重开另有旧 NPC、旧音乐模式断言失败。相机避让、接触反应、动画主导的非致命倒地及公会、咖啡馆、夜景已修改；倒地自然度和第一人称姿态未验收。'},
 {'title':'B · 剑与魔法（PARTIAL / USER_REVIEW）','conclusion':'空手、剑、魔法枪切换，剑连击／蓄力、四种魔法、MP／冷却已接入并运行。魔法阵改为多层原创符文、卫星小阵、卷草和独立徽记，分层显现、反向旋转、消散；四种法术颜色与图案不同。第一人称握剑、拔收剑及倒地表现仍需打磨；治疗与护盾的完整数值专项未补齐。'},
 {'title':'C · 飞行（USER_REVIEW）','conclusion':'普通飞行约二十秒、加速消耗加倍，耗尽缓降和地面恢复已在独立包验证。保留前倾、侧倾和安全衣层；三种原创立体羽毛排列已拍午后／夜晚的悬停、前飞和加速对比，等待选择。'},
 {'title':'D · 私人 BGM（USER_REVIEW）','conclusion':'两首用户音乐已本机导入、调节响度并交替循环；自然时长循环已运行验证。实际音量、衔接和压低效果仍待试听。公开录像关闭私人 BGM，只保留游戏音效；音乐源文件、转码和音乐资产不上传。'},
 {'title':'E · 下一段准备（PARTIAL）','conclusion':'已核对 Fab 已有资源，列出同作者建筑与翅膀付费候选，完成弯曲海岸港町布局草图；本轮没有购买或扩图。魔导车尚无合格替换，保留原车；新增多种成年路人面孔未完成。'}]
manual='先用 Q、左键、1—4 和 V 检查剑与魔法的手感、遮挡和倒地；用 F／Shift 飞到体力耗尽，选择翅膀方案一／二／三；试听两首 BGM 的循环与音量。接送支线当前不可完成。'
delivery={'status':'PARTIAL','candidate':candidate,'executable':exe,'sections':sections,'manual':manual,
 'review':str(D/'review/index.html'),'zip':str(D/'M5_VS3_REVIEW.zip'),'video':str(D/'M5_VS3_PLAYTHROUGH.mp4'),
 'release_tag':tag,'release_url':release,'release_body':'M5-VS3 限定范围候选（PARTIAL）。新增原创多层魔法阵、剑与魔法、飞行体力及三种翅膀排列；主线读档已修复。接送支线、倒地自然度、第一人称握剑等仍有缺口。\n\n附件为审阅 ZIP、5分03秒原生1080p实机录像及完整性清单。录像只有游戏音效，私人商业 BGM 已关闭。没有上传完整游戏、存档、Selestia／其他付费源文件或私人音乐资产。\n\n战斗、魔法、翅膀和 BGM 的用户验收 PENDING。详细范围和失败记录见审阅页。',
 'user_acceptance':{x:'PENDING' for x in ['combat','magic','wings','bgm']},'desktop_control':'STOPPED_GAME_CLOSED','test_layers':{'logic':'See per-check provenance','engine_input':'Actual standalone Actions/Keys, fixture placement identified separately','real_os':'F9 and P pause/resume observed; relative mouse NOT_RUN'}}
(D/'DELIVERY.json').write_text(json.dumps(delivery,ensure_ascii=False,indent=2),'utf-8')
text='# 海湾漫游 M5-VS3 · PARTIAL\n\n'+'\n\n'.join('**'+s['title']+'**\n\n'+s['conclusion'] for s in sections)
text+='\n\n**实测与待办**\n\n独立包专项与录像已完成。完整回归与历史失败记录保留；重开并非全通过。真实 Windows F9、P 暂停／继续已观察，真实相对鼠标 NOT_RUN。五种攻击各倒地及四种起身的完整专项录像未完成；本轮未做新的三十分钟浸泡。原倒地 FAIL 不因新功能测试通过而取消。\n\n性能见 PERFORMANCE.md；录像约五分钟，1080p、游戏音效、正常时间速度，输入与机位布置分别标注于附录。\n\n'+manual
text+='\n\n**入口**\n\n新版 EXE：\n\n`'+exe+'`\n\n审阅页：`'+str(D/'review/index.html')+'`\n\n录像：`'+str(D/'M5_VS3_PLAYTHROUGH.mp4')+'`\n\n[GitHub Release]('+release+')。付费资源只是候选，目前无需购买。\n'
(D/'STATUS.md').write_text(text,'utf-8')
reopen=next(x for x in reversed(v['runs']) if x['test_mode']=='m5_reopen')
r=json.loads(Path(reopen['results']).read_text('utf-8-sig'))
notes={'status':'PARTIAL','source':reopen['results'],'failed_checks':[c for c in r['checks'] if c['status']!='PASS'],
 'interpretation':['M5_Citizen01 lookup is absent in this town; its stand-restoration assertion is not a successful NPC restoration test.','Legacy assertion hardcodes Interior mode. Actual private playlist is active, mode Exploration, one playing channel. Keep FAIL; no claim that audio stopped.','Coffee Complete and Ride Active match the actual earlier save; both Complete cannot pass because ride boarding remains unresolved.'],
 'passed_restore_checks':[c for c in r['checks'] if c['status']=='PASS' and any(k in c['name'] for k in ['restored checkpoint','playable checkpoint','victories','disk checkpoint','view preference'])]}
(A/'FINAL_REOPEN_SCOPE.json').write_text(json.dumps(notes,ensure_ascii=False,indent=2),'utf-8')
oskeys={'status':'PASS_SCOPED_OS_KEYS','launch':str(A/'OS_FINAL_LAUNCH.json'),'actions':['F9','P','P','Alt+F4'],'observed':['Fresh chapter one viewpoint changed to chapter four completion viewpoint after real F9.','P displayed the pause menu; second P dismissed it.','Game closed normally.'],
 'screenshots':[str(D/('OS_FINAL_'+x+'.png')) for x in ['BEFORE','F9','PAUSE','RESUME']],
 'limitations':['Windows tool screenshot framing differs from native engine capture; these are input evidence, not 1080p beauty shots.','No relative OS mouse calibration. No BGM listening acceptance inferred.']}
(A/'OS_FINAL_RESULTS.json').write_text(json.dumps(oskeys,ensure_ascii=False,indent=2),'utf-8')
for n in ['OS_FINAL_F9_thumb.jpg','OS_FINAL_PAUSE_thumb.jpg']:shutil.copy2(D/n,A/n)
cap=Path(video['capture']);c=json.loads(cap.read_text('utf-8-sig'));frames=c['frames']
out=Image.new('RGB',(960,155*6),'#101724');draw=ImageDraw.Draw(out)
for i,t in enumerate([1,10,20,30,40,50,65,80,95,110,130,150,170,190,210,230,250,260,270,278,286,292,299,302]):
 f=min(frames,key=lambda f:abs(f['seconds']-t));im=Image.open(cap.parent/f['file']);im.thumbnail((240,135));x=i%4*240;y=i//4*155;out.paste(im,(x,y));draw.text((x+3,y+136),str(t)+' s',fill='white')
out.save(D/'video_contact.jpg',quality=86)
# Real unaltered video frames from the newly furnished facade/interior interval.
for t in [278,286,292]:
 f=min(frames,key=lambda f:abs(f['seconds']-t));im=Image.open(cap.parent/f['file']);im.save(A/f'NativeVideo_{t}s.png')
perf['resolution_validation']={'capture':str(cap),'scene_viewport_size':c['scene_viewport_size'],'native_output_size':c['output_size'],'note':'Performance runs used the same temporary project window configuration; 100% render scale, no recorder. Original config restored after tests.'}
(A/'PERFORMANCE_MEASUREMENTS.json').write_text(json.dumps(perf,ensure_ascii=False,indent=2),'utf-8')
progress=W/'docs/PROGRESS.md';old=progress.read_text('utf-8-sig')
prefix='2026-09-26 M5-VS3 已生成 PARTIAL 独立候选并停止桌面操作。原创复杂魔法阵、剑与魔法、飞行体力与三种翅膀排列已进入新包；主线四章、咖啡支线及真实 F9 重开恢复有证据，接送上车仍未解决。重开旧 NPC／音乐断言 FAIL、倒地与第一人称姿态缺口保留。1080p High/Epic 性能达本次目标；五分钟有声公开录像关闭私人 BGM。详见 [本轮状态](HarborCity_M5_VS3/STATUS.md)。战斗、魔法、翅膀、BGM 用户验收 PENDING；M0 PASS，旧候选、旧 FAIL/NOT_RUN 与存档保留。本轮没有扩图。\n\n'
assert not old.startswith(prefix)
progress.write_text(prefix+old,'utf-8')
print(json.dumps({'status':'WRITTEN_FROM_ACTUAL_EVIDENCE','review_pending_assembly':True,'video_duration':video['duration_seconds']}))
