"""Copy audited pinned source plugins locally; never run their scripts."""
from pathlib import Path
import json,shutil,hashlib,datetime
project=Path('D:/科研学习/codex学习/HarborCity')
root=Path('E:/GameDev/Assets/HarborCity/M5_VS2/Plugins')
record=json.loads((root/'acquisition.json').read_text(encoding='utf-8'))
rows=[]
for item in record['plugins']:
    name=item['name'];src=Path(item['extracted'])
    if name=='KawaiiPhysics':src=src/'Plugins/KawaiiPhysics'
    dest=project/'Plugins'/name
    assert not dest.exists(), 'Never overwrite an existing project plugin'
    assert (src/(name+'.uplugin')).is_file()
    files=[]
    for p in src.rglob('*'):
        rel=p.relative_to(src)
        if any(part in ('.git','.github','Binaries','Intermediate') for part in rel.parts):continue
        assert not p.is_symlink()
        if p.is_file():
            q=dest/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
            files.append({'path':rel.as_posix(),'bytes':q.stat().st_size,'sha256':hashlib.sha256(q.read_bytes()).hexdigest()})
    if name=='KawaiiPhysics':
        license_path=Path(item['extracted'])/'LICENSE'
        shutil.copy2(license_path,dest/'LICENSE')
    rows.append({'name':name,'source_commit':item['commit'],'installed_path':str(dest),'files':files,'plugin_binary_policy':'Source compile; no plugin Binaries copied. VRM4U upstream Assimp ThirdParty DLL/import library retained.'})
p=project/'HarborCity.uproject';data=json.loads(p.read_text(encoding='utf-8-sig'))
for row in rows:
    assert not any(x['Name']==row['name'] for x in data['Plugins'])
    data['Plugins'].append({'Name':row['name'],'Enabled':True})
p.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
out=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2/research/plugin_install.json')
out.write_text(json.dumps({'status':'SOURCE_INSTALLED_COMPILE_NOT_RUN','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'plugins':rows},ensure_ascii=False,indent=2),encoding='utf-8')
for r in rows:print(r['name'],len(r['files']),r['installed_path'])
