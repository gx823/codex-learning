"""Read-only native inventory for the authorized third VS2 segment."""
from pathlib import Path
import json, re, traceback
import unreal as U
W=Path('D:/科研学习/codex学习'); D=W/'docs/HarborCity_M5_VS2'
cmd=U.SystemLibrary.get_command_line()
m=re.search(r'-M5EvidenceDir=(?:"([^"]+)"|(\S+))',cmd); O=Path(m[1] or m[2])
R={'status':'RUNNING','phase':'Part3Probe','maps':{}}
A=U.EditorAssetLibrary; L=U.get_editor_subsystem(U.LevelEditorSubsystem); E=U.get_editor_subsystem(U.EditorActorSubsystem)
def path(x): return x.get_path_name() if x else None
def vec(x): return [x.x,x.y,x.z]
try:
    f=D/'editor_runtime/author_20260925_144955_324_cb37bf54_PlayablePolishAuthor/author_result.json'
    old=json.loads(f.read_text('utf-8-sig')); assert old['status']=='PASS'
    R['source_report']=str(f)
    for label,p in [('corner',old['maps']['Afternoon']),('story','/Game/HarborCity/Maps/L_M5_CyberHarbor')]:
        assert L.load_level(p), p
        rows=[]
        for a in E.get_all_level_actors():
            row={'label':a.get_actor_label(),'class':path(a.get_class()),'location':vec(a.get_actor_location()),'scale':vec(a.get_actor_scale3d()),'tags':[str(t) for t in a.tags]}
            origin,extent=a.get_actor_bounds(False);row['bounds']={'origin':vec(origin),'extent':vec(extent)}
            for key in ['stable_id','display_name','role_id','stationary','dialogue_lines','ride_parking_center','ride_parking_extent','coffee_parcel_location','pickup_curb_y']:
                try: row[key]=str(a.get_editor_property(key))
                except Exception: pass
            meshes=[]
            for c in a.get_components_by_class(U.MeshComponent):
                x={'name':c.get_name(),'relative':vec(c.relative_location),'scale':vec(c.relative_scale3d),'materials':[path(c.get_material(i)) for i in range(c.get_num_materials())]}
                if isinstance(c,U.StaticMeshComponent): x['mesh']=path(c.static_mesh)
                if isinstance(c,U.SkeletalMeshComponent):
                    sm=c.get_skeletal_mesh_asset()
                    x.update(mesh=path(sm),animation=path(c.anim_class),physics=path(sm.get_editor_property('physics_asset')) if sm else None)
                meshes.append(x)
            row['meshes']=meshes;rows.append(row)
        R['maps'][label]={'path':p,'actors':rows}
    R['status']='PASS'
except Exception:
    R.update(status='FAIL',error=traceback.format_exc()); U.log_error(R['error'])
finally:
    (O/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2)+'\n','utf-8')
