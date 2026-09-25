"""Build a portable, truthful second-gate review. Original images; video stays external."""
from pathlib import Path
from urllib.parse import quote
import argparse,datetime as dt,difflib,hashlib,html,json,os,re,shutil,zipfile
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2'
ap=argparse.ArgumentParser();ap.add_argument('--hero-author',required=True);ap.add_argument('--baseline',required=True);args=ap.parse_args()
stamp=dt.datetime.now().strftime('%Y%m%d_%H%M%S');OUT=D/'reviews'/('P0_SecondGate_'+stamp);OUT.mkdir(parents=True,exist_ok=False)
(OUT/'media').mkdir();(OUT/'appendix').mkdir()
def doc(p):return json.loads(Path(p).read_text('utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
sources={}; media={};results={}
def evidence(p,copy_file=True):
 p=Path(p).resolve();assert p.is_file();key=str(p)
 if key not in sources:
  row=dict(original=key,sha256=sha(p),bytes=p.stat().st_size)
  if copy_file:
   dst=OUT/'appendix'/(row['sha256'][:12]+'_'+p.name);shutil.copyfile(p,dst);row['review_file']=dst.relative_to(OUT).as_posix()
  sources[key]=row
 return sources[key]
def native_result(rel,key,result_name=None):
 launch=D/rel/'launch.json';run=doc(launch);evidence(launch)
 assert run.get('exit_code')==0 and not run.get('stop_latched',False),launch
 if run.get('result'):p=Path(run['result'])
 else:
  paths=list(launch.parent.glob('*/'+result_name));assert len(paths)==1,paths;p=paths[0]
 r=doc(p);evidence(p);assert r['status'].startswith('PASS') and not r.get('user_stop_latched',r.get('stop_latched',False)),p
 results[key]=dict(result=str(p),launch=str(launch),status=r['status'],scope='EDITOR_GAME_NOT_PACKAGE',visual='USER_REVIEW')
 return r
def E(s):return html.escape(str(s))
def image(p):
 p=Path(p).resolve();key=str(p)
 if key not in media:
  assert p.is_relative_to(D) and p.is_file(),p
  row=evidence(p,False);dest=OUT/'media'/(row['sha256'][:10]+'_'+p.name);shutil.copyfile(p,dest)
  row['review_file']=dest.relative_to(OUT).as_posix();media[key]=row['review_file']
 return quote(media[key])
def figure(p,title,caption):
 src=image(p);return '<figure><a href="'+src+'"><img loading="lazy" src="'+src+'" alt="'+E(title)+'"></a><figcaption><b>'+E(title)+'</b> '+E(caption)+'</figcaption></figure>'
def figures(captures,title,caption):return ''.join(figure(c['file'],title(c),caption(c)) for c in captures)

hero_author=doc(args.hero_author);assert hero_author['status']=='PASS';evidence(args.hero_author)
hero=[]
for profile in ('Portrait','FullBody'):
 plan=hero_author['plans'][profile];evidence(plan)
 launches=[]
 for p in D.glob('*_corner_v2_game/launch.json'):
  x=doc(p)
  if Path(x.get('plan','')).resolve()==Path(plan).resolve():launches.append(p)
 assert len(launches)==1,launches
 hero.append(native_result(str(launches[0].parent.relative_to(D)),profile,'corner_v2_review.json'))
n=native_result('editor_runtime/20260925_080151_029_4c87eb31_npccast_idle_r2_game','NPC','npc_review.json')
e=native_result('20260925_090835_598_39cbd5a0_corner_v2_game','Environment','corner_v2_review.json')
q=native_result('editor_runtime/20260925_144135_820_a0ddc81e_q_harbor_compare_game','QSkin','npc_review.json')
idle=native_result('editor_runtime/20260925_152606_206_997d29a7_scene_idle_game','SceneIdle','npc_review.json')
flight=native_result('20260925_145119_800_9128ee67_flight_demo_r5_game','Flight','flight_demo.json')
video_file=D/'video_encoding/20260925_145345_740249_d54629dc/verification.json';v=doc(video_file);evidence(video_file)
video=Path(v['output']);evidence(video,False);assert sha(video)==v['output_sha256']
perf=doc(D/'PERFORMANCE_BASELINE_R5.json');evidence(D/'PERFORMANCE_BASELINE_R5.json')
base=doc(args.baseline);assert base['status']!='RUNNING';evidence(args.baseline)
for run in base['runs']:
 if run.get('result'):evidence(run['result'])
for rel in ['M5_VS2_CONTROLS.md','PURCHASE_CANDIDATES.md','WORKFLOW_OVERRIDE_20260925.md',
 'research/RESUME_VISUAL_FIXES_20260925.json','research/INTEGRATION_20260925_AFTER_RESUME.json',
 'research/FLIGHT_e578baa7_POSE_READBACK.json','research/J_GARMENT_SOURCE_GEOMETRY.json',
 'research/SCENE_IDLE_997d29a7_READBACK.json',
 'research/J_CLOTH_12cc066f_CLASSIFIED/classification.json','research/J_CLOTH_f8d97c35_FORCE_UPDATE/classification.json',
 'research/HERO_FILL_DRAWCOMMAND_LIFETIME_REVIEW_20260925.md','research/GAS_CLOCK_SOLE_356579ca_REVIEW.md',
 'research/FOOTPLACEMENT_SOLE_b44ef459_ANALYSIS.json']:
 evidence(D/rel)
diffs=[]
for old,new in [
 ('source_snapshots/getup_local_yaw_20260925/HCM4R1PhysicalReactionGetUp.cpp','HarborCity/Source/HarborCity/M4R1/HCM4R1PhysicalReactionGetUp.cpp'),
 ('source_snapshots/resume_20260925_121107/HCM5VS2DrivingHandsComponent.cpp','HarborCity/Source/HarborCity/M5VS2/HCM5VS2DrivingHandsComponent.cpp'),
 ('source_snapshots/flight_graph_node_deduction_20260925_042823/HCM5VS2FlightPoseEditor.cpp','HarborCity/Source/HarborCity/M5VS2/HCM5VS2FlightPoseEditor.cpp')]:
 before=D/old;after=W/new;assert before.is_file() and after.is_file()
 text=''.join(difflib.unified_diff(before.read_text('utf-8-sig').splitlines(True),after.read_text('utf-8-sig').splitlines(True),fromfile=str(before),tofile=str(after)))
 out=OUT/'appendix'/(after.name+'.diff');out.write_text(text,'utf-8')
 diffs.append(dict(before=str(before),before_sha256=sha(before),after=str(after),after_sha256=sha(after),diff=out.relative_to(OUT).as_posix(),scope='Compared with named preserved snapshot; not a complete project-history patch'))
for source in ['tools/ue_m5_vs2_scene_idle_author.py','tools/ue_m5_vs2_hero_polish_review_author.py','tools/run_m5_vs2_gate_baseline.ps1','tools/ue_m5_vs2_flight_pose_polish_author.py',
 'HarborCity/Source/HarborCity/M5VS2/HCM5VS2NPCIdleEditor.cpp','HarborCity/Source/HarborCity/M5VS2/HCM5VS2NPCIdleEditor.h',
 'HarborCity/Source/HarborCity/M5VS2/HCM5VS2NPCClothDiagnostics.cpp']:
 evidence(W/source)

gaps=[
 dict(area='NPC 倒地衣物',status='FAIL',conclusion='J 衣物穿地未修好；原生布料和环境更新候选都未通过画面检查，未替换正式街角角色。',next='按真实衣物轮廓重做固定区域与碰撞体贴合，必要时回到 DCC 修衣裙；不继续堆碰撞参数。'),
 dict(area='NPC 起身',status='FAIL',conclusion='四种起身方向已有运行样本，窄处与坡面已有专项；J 的一次颤动超时仍未关闭。拳击、枪击、实际车撞加四种起身的完整录像尚未完成。',next='保留失败样本，区分身体稳定性和衣裙问题后再录完整受击演示。'),
 dict(area='跑步滑步',status='FAIL',conclusion='当前步幅／朝向修正仍未达到肉眼无滑步；Foot Placement 候选反而变差，未采用。',next='重新核对触地相位与动画速度匹配，保留当前可用步态。'),
 dict(area='补光 Renderer 崩溃',status='FAIL',conclusion='历史两次崩溃根因仍未完全确定。当前补光保留；后续运行未重现不能关闭问题。',next='已有内存生命周期线索，下一次自然复现采完整崩溃证据，避免无依据刷新渲染资源。'),
 dict(area='飞行外观与录音',status='USER_REVIEW',conclusion='新姿态、风动和羽翼已运行；已审角度有安全衣层覆盖。录像是引擎输入，原生音轨静音；真实键鼠手感、所有极端角度及正式带声视频仍待验证。',next='用户审姿态与羽翼方向；后续补键鼠手感及最终有声视频。'),
 dict(area='新港町玩法与独立包',status='NOT_RUN',conclusion='本门仍为街角样板。保留地图基线回归与新港町全流程、独立包验收分开；尚无 VS2 新 EXE。',next='环境方向选定且关键问题修妥后，按原阶段流程推进。')]
for run in base['runs']:
 if run['status']!='PASS':gaps.append(dict(area='门前保留地图检查 · '+run['mode'],status=run['status'],conclusion='本轮检查未全部通过：'+('、'.join(c['name'] for c in run.get('failed_checks',[])) or '结果缺失或运行未完成')+'。原因尚未确定，旧通过记录不被擦除。',next='保留原运行证据，做针对性诊断；不修改冻结车辆参数来凑通过。'))

parts=['<header><p class="eyebrow">HARBORCITY · 海湾漫游</p><h1>港町样板 · 第二次审阅</h1><p class="lead">主角、八名路人、街角与飞行的新画面已集中在这里。<strong>本次未达到全部放行条件</strong>：倒地衣物、滑步和历史补光崩溃仍未解决。</p><nav><a href="#hero">主角</a><a href="#npc">路人</a><a href="#world">街角</a><a href="#choices">环境方向</a><a href="#flight">飞行</a><a href="#performance">性能</a><a href="#issues">剩余问题</a></nav></header>',
 '<section id="hero"><h2>主角 · 最新修订</h2><p>采用最新发丝根部修复、原作描边和光环的已保存角色；午后、黄昏、夜晚与室内均为实际游戏截图。时段融入与整体美感请直接看图。</p><div class="grid">']
periods=['午后近景','黄昏近景','夜晚近景','室内近景']
caps=['脸、眼睛与发色保留，观察细发丝与披肩的关系。','受暖色环境光影响；不是沿用午后截图。','脸部可读性与夜景融合仍需一起审。','室内暖灯实际运行；此图不代表历史崩溃根因已关闭。']
for c,title,caption in zip(hero[0]['captures'],periods,caps):parts.append(figure(c['file'],title,caption))
parts+=['</div>',figure(hero[1]['captures'][0]['file'],'主角全身','原始全身图用于看描边、比例与发丝。跑步滑步仍单独列为未解决。'),'<h3>第一人称手部与驾驶席</h3><div class="grid">']
hands=doc(D/'research/RESUME_VISUAL_FIXES_20260925.json')['tests']
for key,index,title in [('hands_day',1,'白天转向握盘'),('hands_night',1,'夜间转向握盘'),('hands_day',7,'步行空手'),('hands_day',8,'步行持枪')]:
 # Earlier isolated hand tests use the same saved materials; flight loop changes do not alter these poses.
 if key not in hands:key='hands' if key=='hands_day' else key
 parts.append(figure(hands[key]['images'][index],title,'同材质手部专项原图；转向范围与换手已测，手指包裹感仍可继续改善。'))
parts+=['</div></section>',
 '<section id="npc"><h2>路人 · 八种独立造型</h2><p>下面是八名不同来源角色的面部与全身。站立展示通过不等于倒地自然度通过；Q 的日光肤色更新在下方单独对照。</p>']
for letter in 'QRJTUVWX':
 parts.append('<h3>角色 '+letter+'</h3><div class="grid">')
 for c in n['captures']:
  if c['subject']==letter:parts.append(figure(c['file'],letter+(' · 面部' if c['framing']=='FACE' else ' · 全身'),'造型原图；Q 此处是较早中性展示，现行暖肤色以下方为准。' if letter=='Q' else '已导入的真实角色；倒地及衣物问题不由站立图豁免。'))
 parts.append('</div>')
parts+=['<h3>Q · 同一港町日光下的肤色</h3><div class="grid">']
for i,c in enumerate(q['captures']):parts.append(figure(c['file'],'修正前' if i==0 else '暖肤色候选','同一角色、姿态、镜头和日光；只修改脸与身体皮肤材质，保留深肤色。'))
parts+=['</div><h3>生活待机 · 售货、望海、交谈</h3><div class="grid">']
for c,title in zip(idle['captures'][::2],['售货手势','望海遮光手势','交谈手势']):parts.append(figure(c['file'],title,'在现有放松待机上添加的小幅原创动作，已保存并实际播放；目前为独立街角副本。'))
cloth=doc(D/'research/J_CLOTH_f8d97c35_FORCE_UPDATE/classification.json')
parts+=['</div><h3>倒地问题 · 未通过的原图</h3><div class="grid">',figure(cloth['original_images'][0],'J · 活体倒地','候选布料仍有衣物穿地，不能作为修复完成。'),figure(cloth['original_images'][-1],'J · 死亡倒地','保持失败画面与数据；该候选未替换正式街角角色。'),'</div><p class="notice">倒地衣物仍不合格。本次没有把直接伤害函数调用包装成拳击、枪击或真实车辆碰撞视频。</p></section>',
 '<section id="world"><h2>A · Fantastic Village 深化</h2><p>仍只做原来的街角，没有扩成全图。已补连续街面、收岸、远景、分时照明与高空视角；建筑方向仍偏西式童话，地面与岸台过渡还有人工感。</p><div class="grid">']
for c in e['captures']:
 title='高空俯瞰' if 'Air50m' in c['label'] else ('黄昏' if 'Dusk' in c['label'] else '夜晚' if 'Night' in c['label'] else '午后')
 parts.append(figure(c['file'],title,'真实街角原图；观察屋顶、岸线、云形与角色所在世界的风格是否一致。'))
parts+=['</div></section><section id="choices"><h2>B / C · 更偏日式动漫的候选</h2><p><strong>以下为商店宣传图，不是本游戏截图。</strong>没有购买或下载；两套都带现代元素，需要筛选改造成奇幻港町。</p><div class="grid">']
store=D/'research/store_review_20260925_0552'
choices=[('B','Japan Village','bbbf727f-b4c7-4f9f-b4ea-4e2b849904f6','B_JapanVillage_media1_browser.png','页面价 ¥7,911–¥9,177；支持 UE 4.27。'),('C','Japan Countryside (Anime Environment)','107824af-f138-4caf-b805-1b9bc6b4eabf','C_JapanCountryside_media2_browser.png','页面价 ¥12,659–¥36,400；支持 UE 5.3–5.4。')]
for code,title,listing,img,caption in choices:
 parts.append(figure(store/img,code+' · '+title,caption+'商店宣传图，不是本游戏截图。')+'<p><a href="https://www.fab.com/listings/'+listing+'">'+code+' 官方商品页</a></p>')
parts+=['</div><p>两者页面显示 Standard License；币种、下载大小与解压大小未确认，UE 5.8 尚未验证。价目是本轮页面观察，购买前需再次确认。你可以选继续深化 A，或先核查 B / C；选择方向不等于授权购买。</p></section>',
 '<section id="flight"><h2>飞行 · 最新真实路线</h2><p>已完成起飞、悬停、绕行、加速俯冲、拉升与落地的引擎输入演示，含第一人称。膝肘放松与左右不对称悬停已生效；羽翼造型仍待你审美确认。</p>',
 '<video controls preload="metadata" src="'+E(quote(os.path.relpath(video,OUT).replace('\\','/'),safe='/.:'))+'"></video><p>真实游戏画面，未用镜头动画代替鼠标测试。原生音轨为静音；视频单独保存，ZIP 内不含视频。它不是独立包或真实 OS 键鼠验收。</p><p class="path">'+E(str(video))+'</p></section>',
 '<section id="performance"><h2>街角性能基线</h2><table><thead><tr><th>画质</th><th>平均 FPS</th><th>p99 帧时间</th><th>显存采样峰值</th></tr></thead><tbody>']
for row in perf['runs']:parts.append('<tr><td>'+E(row['quality'])+'</td><td>'+format(row['average_fps'],'.1f')+'</td><td>'+format(row['p99_frame_ms'],'.2f')+' ms</td><td>'+format(row['process_local_peak_sampled_gib'],'.2f')+' GiB</td></tr>')
parts+=['</tbody></table><p>1920×1080、100% 渲染比例、无帧率上限与 VSync，独立于截图录像采样。i7-14650HX／RTX 4060 Laptop；这是此前八人街角样板的 Editor-game 数据，不是最新版完整港町或独立包承诺。</p></section><section id="issues"><h2>仍需解决</h2><ul>']
for gap in gaps:parts.append('<li><strong>'+E(gap['area'])+'</strong>：'+E(gap['conclusion'])+'</li>')
parts+=['</ul><p>下一步环境方向由你选；关键失败不会因选型而取消，也没有许可购买、扩图或发布新包的自动动作。</p></section>',
 '<details><summary>测试、问题与来源附录</summary><p>正文仅展示画面和结论。逐项数据、构建来源、原图路径与文件校验见 <a href="GATE2_EVIDENCE.json">JSON 附录</a>；操作说明与其余原始报告收在 appendix 中。</p></details><footer>第二次阶段门 · USER_REVIEW／未全部放行。所有画面均保留原图；点击可查看原尺寸。</footer>']
css='*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0 auto;max-width:1180px;padding:40px 24px;background:#f5f3ee;color:#24313b;font:17px/1.75 system-ui,"Microsoft YaHei",sans-serif}h1{font-size:clamp(30px,4vw,46px);line-height:1.2}h2{font-size:29px}h3{font-size:21px}header{padding:15px 0 30px;border-bottom:2px solid #cadbd8}.eyebrow{font-size:13px;letter-spacing:3px;color:#3e7778}.lead{max-width:850px}nav{display:flex;flex-wrap:wrap;gap:12px 22px}a{color:#216b73}section{padding-top:40px}p{max-width:1020px}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:22px}figure{margin:10px 0 22px;background:white;border:1px solid #d6dedb;border-radius:8px;overflow:hidden}img{display:block;width:100%;height:auto}figcaption{padding:12px 16px;font-size:15px}figcaption b{display:block}video{width:100%;background:#13252c}.notice{padding:14px;background:#f0dfcf}.path{word-break:break-all;font-size:13px}table{width:100%;border-collapse:collapse;background:white}td,th{text-align:left;padding:12px;border:1px solid #d6dedb}li{margin:14px 0}details{padding:18px;background:#e2ece9;margin-top:45px}footer{padding:28px 0;color:#657975;font-size:14px}@media(max-width:760px){body{padding:22px 14px}.grid{grid-template-columns:1fr}}'
(OUT/'index.html').write_text('<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>海湾漫游 · 第二次样板审阅</title><style>'+css+'</style><body>'+''.join(parts)+'</body></html>','utf-8')
inventory=dict(status='USER_REVIEW_NOT_ALL_REQUIREMENTS_PASS',review=str(OUT/'index.html'),built_at=dt.datetime.now().isoformat(),results=results,remaining=gaps,retained_map_gate_baseline=base,video=dict(file=str(video),evidence=v,zip_included=False),sources=list(sources.values()),related_differences=diffs,browser_check='NOT_RUN',zip_verification='See adjacent ZIP_INTEGRITY.json created after archiving')
(OUT/'GATE2_EVIDENCE.json').write_text(json.dumps(inventory,ensure_ascii=False,indent=2),'utf-8')
(OUT/'README.md').write_text('# 第二次阶段门审阅\n\n打开 index.html 查看原图与结论。本次未全部放行；倒地衣物、滑步、历史补光崩溃仍未解决。没有 VS2 新 EXE。\n\n飞行视频单独保留：'+str(video)+'\n\n从 ZIP 解压后，视频相对链接可能失效，请用上述原路径打开。原图与报告已包含在 ZIP 中。\n','utf-8')
print(str(OUT))
