import argparse,json,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
l=json.loads(a.launch.read_text('utf-8-sig'));r=json.loads(Path(l['result_file']).read_text('utf-8-sig'))
groups={}
for s in r['samples']:
    pose=s.get('evaluated_flight_pose')
    if pose is None:continue
    groups.setdefault(s['stage'],[]).append((s,pose))
out=dict(source_launch=str(a.launch),source_result=l['result_file'],runtime=r['status'],stop_latched=r['stop_latched'],stages={})
for stage,rows in groups.items():
    eligible=[p for s,p in rows if p.get('flight_pose_eligible') and not s['first_person']]
    out['stages'][stage]=dict(samples=len(rows),eligible_third_person_samples=len(eligible),
        actual_instances=sorted({p['actual_instance'] for s,p in rows}),pose_indices=sorted({p.get('pose_index') for s,p in rows}),
        median_degrees={k:statistics.median([p[k] for p in eligible]) for k in ('knee_bend_deg_L','knee_bend_deg_R','elbow_bend_deg_L','elbow_bend_deg_R')} if eligible else {})
h=out['stages'].get('Hover',{});b=h.get('median_degrees',{})
ok=r['status']=='PASS_ENGINE_INPUT_ONLY' and r['stop_latched'] is False and h.get('eligible_third_person_samples',0)>=5 and abs(b.get('knee_bend_deg_L',-999)-21.4)<3 and abs(b.get('knee_bend_deg_R',-999)-47.9)<3
out['status']='PASS_EVALUATED_HOVER_JOINTS_ONLY' if ok else 'FAIL_POSE_EXPECTATION'
out['scope']='Read actual evaluated sockets and actual anim-instance flags. This is not art, cloth-coverage, OS-input or packaged acceptance.'
a.output.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(status=out['status'],hover=h),ensure_ascii=True))
