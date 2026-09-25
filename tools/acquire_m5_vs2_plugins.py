"""Fetch pinned, authorized official plugin sources into E; never execute them."""
from pathlib import Path, PurePosixPath
import datetime as dt
import hashlib,json,stat,urllib.request,zipfile,shutil

base=Path('E:/GameDev/Assets/HarborCity/M5_VS2/Plugins')
base.mkdir(parents=True,exist_ok=True)
specs=[('VRM4U','ruyo/VRM4U','680bb9d4bcc69ee2e1d78a12f2b8964980637d42'),
       ('KawaiiPhysics','pafuhana1213/KawaiiPhysics','0035f5291bc3fcf71c12724a290dba132c4ccb01')]
rows=[]
for name,repo,commit in specs:
    url=f'https://github.com/{repo}/archive/{commit}.zip'
    archive=base/f'{name}_{commit}.zip';out=base/f'src_{name}_{commit[:8]}'
    assert not out.exists(), 'Use a fresh extraction directory; never overwrite'
    reused=archive.exists()
    if not reused:
        req=urllib.request.Request(url,headers={'User-Agent':'HarborCity-authorized-asset-acquisition'})
        with urllib.request.urlopen(req,timeout=120) as response, archive.open('xb') as dest:
            count=0
            while chunk:=response.read(1024*1024):
                count+=len(chunk)
                if count>512*1024*1024:raise RuntimeError('Unexpected plugin archive over 512MiB')
                dest.write(chunk)
    count=archive.stat().st_size
    with zipfile.ZipFile(archive) as z:
        assert sum(x.file_size for x in z.infolist())<2*1024**3
        entries=[]
        for info in z.infolist():
            parts=PurePosixPath(info.filename).parts
            assert parts and all(p not in ('.','..') and ':' not in p and '\\' not in p for p in parts)
            assert not PurePosixPath(info.filename).is_absolute()
            assert not stat.S_ISLNK(info.external_attr>>16)
            assert parts[0]==f'{name}-{commit}', 'Unexpected archive root'
            target=out.joinpath(*parts[1:]).resolve()
            assert target.is_relative_to(out.resolve())
            entries.append((info,target))
        assert z.testzip() is None
        for info,target in entries:
            if info.is_dir():target.mkdir(parents=True,exist_ok=True)
            else:
                target.parent.mkdir(parents=True,exist_ok=True)
                with z.open(info) as source,target.open('xb') as dest:shutil.copyfileobj(source,dest)
    with archive.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    row=dict(name=name,repository=f'https://github.com/{repo}',commit=commit,source_zip=url,
        local_archive=str(archive),bytes=count,sha256=digest,extracted=str(out),
        status='DOWNLOADED_SOURCE_ONLY',executed=False,local_compile='NOT_RUN',reused_download=reused,
        extraction_note='Validated and removed single archive root to avoid Windows long paths; earlier partial extraction preserved',
        sha_scope='Local integrity identifier, pinned official source; not upstream signature')
    rows.append(row)
    (base/'acquisition.json').write_text(json.dumps(dict(recorded_utc=dt.datetime.now(dt.timezone.utc).isoformat(),plugins=rows,total_download_bytes=sum(r['bytes'] for r in rows)),ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(row,ensure_ascii=False),flush=True)
