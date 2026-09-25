"""Bound reuse to the one exact regression-guard edit; do not infer build equality."""
from pathlib import Path
import json,sys
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS2/part3'
old=json.loads((W/'docs/HarborCity_M5_VS2/builds/20260925_194722_644_45d4aae0_package/package.json').read_text('utf-8-sig'))
new=json.loads(Path(sys.argv[1]).read_text('utf-8-sig'))
assert old['status']==new['status']=='PASS'
def inventory(p):return {r['path']:r['sha256'] for r in json.loads(Path(p).read_text('utf-8-sig'))}
def delta(a,b):return sorted(k for k in a.keys()|b.keys() if a.get(k)!=b.get(k))
source=delta(inventory(old['source_inventory']),inventory(new['source_inventory']))
content=delta(inventory(old['content_inventory']),inventory(new['content_inventory']))
assert source==['Source/HarborCity/M1/HCM1TestM5.cpp'] and not content,(source,content)
assert old['engine']==new['engine'] and old['configured_default_map']==new['configured_default_map']
report={'status':'PASS_QA_ONLY_DELTA','old_executable':old['actual_executable'],'new_executable':new['actual_executable'],'source_differences':source,'content_differences':content,'meaning':'All inventory-covered config/other C++/source assets identical. Only legacy mesh assertions, diagnostics and indoor grounded test placement changed. Cooked binaries are not asserted byte-identical. Old-candidate results retain original EXE identity; new candidate regression runs separately.'}
(D/'BUILD_CONTINUITY.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps(report,ensure_ascii=False))
