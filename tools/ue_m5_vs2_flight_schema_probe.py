from pathlib import Path
import re,json,traceback
import unreal as U
cmd=U.SystemLibrary.get_command_line()
match=re.search(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',cmd)
assert match
out=Path(match.group(1) or match.group(2)).resolve()
assert out.is_relative_to(Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2/editor_runtime'))
r=dict(status='RUNNING',asset_mutated=False,schemas={})
try:
    names=[n for n in dir(U) if 'CollisionResponse' in n]+['CollisionChannel','ObjectTypeQuery','HitResult']
    for name in names:
        obj=getattr(U,name)
        r['schemas'][name]=dict(doc=obj.__doc__,members=[n for n in dir(obj) if not n.startswith('_')])
    r['system_methods']={n:getattr(U.SystemLibrary,n).__doc__ for n in ('line_trace_single','capsule_overlap_components','capsule_trace_single')}
    r['component_methods']={n:getattr(U.PrimitiveComponent,n).__doc__ for n in ('get_collision_response_to_channel','get_collision_enabled')}
    r['status']='PASS'
except Exception:
    r.update(status='FAIL',error=traceback.format_exc())
(out/'author_result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),'utf-8')
if r['status']!='PASS':raise RuntimeError(r['error'])
