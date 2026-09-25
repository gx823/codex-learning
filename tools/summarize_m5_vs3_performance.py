"""Report measured phases without averaging percentiles or inventing GPU timings."""
from pathlib import Path
import csv,json,re,subprocess
W=Path(__file__).resolve().parents[1];D=W/'docs/HarborCity_M5_VS3'
v=json.loads((D/'standalone_validation.json').read_text('utf-8-sig'))
runs=[x for x in v['runs'] if x['test_mode']=='m5_vs2_perf']
qualities={}
for run in runs:
 args=run['launch']['arguments'];quality='Epic' if '-M5Quality=3' in args else 'High'
 results=json.loads(Path(run['results']).read_text('utf-8-sig'));phases=[]
 for c in results['checks']:
  text=c.get('actual','')
  if 'mean_fps=' not in text:continue
  values={k:float(x) for k,x in re.findall(r'(frames|mean_fps|p99_ms|wall_seconds|npcs|damaged)=([0-9.]+)',text)}
  phases.append(dict(name=c['name'],status=c['status'],**values))
 csvpath=Path(run['launch_path']).parent/'adapter_samples.csv';gpu=[]
 if csvpath.exists():
  for row in list(csv.reader(csvpath.read_text('utf-8-sig').splitlines()))[1:]:
   if len(row)>=4:
    try:gpu.append({'time':row[0].strip(),'gpu':row[1].strip(),'used_mib':float(row[2]),'utilization_percent':float(row[3])})
    except ValueError:pass
 seconds=sum(p.get('wall_seconds',0) for p in phases)
 stress=next((p for p in phases if 'six NPC' in p['name']),{})
 qualities[quality]={'run':run['label'],'results':run['results'],'launch':run['launch'],
  'phases':phases,'weighted_mean_fps':sum(p.get('frames',0) for p in phases)/seconds if seconds else None,
  'worst_phase_p99_ms':max((p['p99_ms'] for p in phases),default=None),
  'whole_adapter_peak_gib':max((x['used_mib'] for x in gpu),default=0)/1024 if gpu else None,
  'adapter_samples':gpu,'six_npc_melee_status':'NOT_RUN',
  'stress_scope':'Ordinary AI response near six placed adult NPCs; damaged count measured. Simultaneous melee by six NPCs was not separately verified.',
  'stress_measured':stress}
assert set(qualities)=={'High','Epic'},'Both actual runs are required'
hardware=subprocess.check_output(['nvidia-smi','--query-gpu=name,memory.total,driver_version','--format=csv,noheader'],text=True).strip()
data={'status':'MEASURED','resolution':'1920x1080 requested by QA launch; screen percentage 100, VSync and cap disabled by QA code',
 'hardware':hardware,'quality_measurements':qualities,'global_p99_status':'NOT_COMPUTED_RAW_PHASE_SAMPLES_NOT_RETAINED',
 'sampling_notes':['No recording during measured runs.','Frame times are actual wall time, not a configured FPS cap.','Adapter VRAM includes desktop and other applications.','No GPU/CPU stage timings retained; utilization alone cannot establish an exact bottleneck.']}
(D/'appendix/PERFORMANCE_MEASUREMENTS.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),'utf-8')
def f(x):return 'NOT_RUN' if x is None else f'{x:.2f}'
lines=['# M5-VS3 性能实测','',hardware,'','1920×1080，High / Epic；关闭帧率上限，与录像分开采样。','',
 '| 画质 | 采样段合并平均 FPS | 最差分段 p99（ms） | 整卡显存峰值（GiB） |', '|---|---:|---:|---:|']
for q in ['High','Epic']:
 x=qualities[q];lines.append(f"| {q} | {f(x['weighted_mean_fps'])} | {f(x['worst_phase_p99_ms'])} | {f(x['whole_adapter_peak_gib'])} |")
lines+=['','p99 是最差分段值，不是全程合并百分位；显存含桌面进程。分段条件、帧数和失败项见附录 JSON。','',
 '六名 NPC 附近的多魔法负载已采样，实际受伤人数见附录；六人同时近战未独立确认，仍为 NOT_RUN。',
 '未保存 GPU / CPU 分项耗时，不能仅凭总帧率断定瓶颈。未为达标关闭 Lumen 或删减 NPC、植被。']
(D/'PERFORMANCE.md').write_text('\n'.join(lines)+'\n','utf-8')
print(json.dumps({q:{k:x[k] for k in ['weighted_mean_fps','worst_phase_p99_ms','whole_adapter_peak_gib']} for q,x in qualities.items()}))
