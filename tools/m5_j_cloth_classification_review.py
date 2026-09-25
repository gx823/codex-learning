import json,sys
from pathlib import Path
from PIL import Image,ImageDraw
launch=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
l=json.loads(launch.read_text('utf-8-sig'));r=json.loads(Path(l['result']).read_text('utf-8-sig'))
rows=[];imgs=[]
for c in r['captures']:
    if not c.get('j_chaos_cloth_at_request'):continue
    d=c['j_chaos_cloth_at_request']
    fields=['asset_index','actual_max_distance_map_matches_particle_count','fixed_particles_below_ground','moving_particles_below_ground','fixed_minimum_gap_cm','moving_minimum_gap_cm']
    rows.append(dict(phase=c['label'],static_components=d['nearby_static_collision_components'],collide_with_environment=d['component_collide_with_environment'],force_collision_update=d.get('force_collision_update'),simulation=d['simulation_runtime'],cloth=[{k:a.get(k) for k in fields} for a in d['cloth']]))
    f=Path(c.get('file') or c.get('png'));im=Image.open(f).convert('RGB');im.thumbnail((384,216));imgs.append((c['label'],im,str(f)))
canvas=Image.new('RGB',(384*3,246*2),(28,29,32));draw=ImageDraw.Draw(canvas)
for i,(label,im,f) in enumerate(imgs):
    x=i%3*384;y=i//3*246;canvas.paste(im,(x,y));draw.text((x+7,y+220),label,fill='white')
canvas.save(out/'contact.jpg',quality=86)
data=dict(status='FAIL_VISUAL_OPEN',source_launch=str(launch),source_result=l['result'],original_images=[f for _,_,f in imgs],captures=rows,
    conclusion='Fixed and dynamic particles both penetrate the floor; zero-MaxDistance pins cannot be displaced by cloth collision. Nearby WorldStatic availability is not solver-ingestion proof.')
(out/'classification.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
print(json.dumps(dict(contact=str(out/'contact.jpg'),report=str(out/'classification.json'),captures=len(rows))))
