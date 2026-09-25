"""Repair only Q/R VA Package references; no import, mesh/material or map edits.

Root runs through the existing author wrapper after compiling the native helper.
The two original VA packages are hash-bound and backed up before any mutation.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import shutil
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习')
PROJECT = WORK/'HarborCity'
DOC = WORK/'docs/HarborCity_M5_VS2'
OWNER = 'HarborCity_M5_VS2_NPC_OfficialSamples'
ROOT = '/Game/HarborCity/M5VS2/NPC'
SOURCE_HASHES = {'Q':'36c131cef1f25e9a75825dfac26f14cfdb757123f6d83331b7ebfe0c146f0f90',
                 'R':'1533ddde86353d5d611b0d57fb932eddc55bbb53a99fdf2fb51609b8f7a4498f'}
VA_HASHES = {'Q':'9cd3dc2b025dbbb13a4769b6554c3f8dc3fad72b09bd3962e64f49a6cfffb039',
             'R':'6a53b49272ece4103dbd9c68322c4b6ba4e1c202a96df1b3a6f161fab1134625'}
A = U.EditorAssetLibrary
REGISTRY = U.AssetRegistryHelpers.get_asset_registry()
args = re.findall(r'(?:^|\s)-M5EvidenceDir=(?:"([^"]+)"|(\S+))',U.SystemLibrary.get_command_line())
assert len(args)==1
OUT = Path(args[0][0] or args[0][1]).resolve()
assert OUT.is_relative_to(DOC.resolve()) and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True,exist_ok=True)
R = dict(status='RUNNING',checks=[],assets=[],started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         scope='Only the serialized Package field on the two named VA assets. No reimport or mesh/material/animation/skeleton/map edit.',
         runtime='NOT_RUN',fresh_process_reload='NOT_RUN')


def dump(): (OUT/'author_result.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf-8')


def check(name,ok,observed=None):
    R['checks'].append(dict(name=name,status='PASS' if ok else 'FAIL',observed=observed));dump()
    if not ok: raise RuntimeError(name+': '+str(observed))


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def disk(package): return PROJECT/'Content'/Path(package.removeprefix('/Game/')).with_suffix('.uasset')


def native(asset,apply):
    return {str(k):str(v) for k,v in U.VrmImporterBPFunctionLibrary.normalize_harbor_city_npc_asset_list_package(asset,apply).items()}


def dependencies(package):
    values=REGISTRY.get_dependencies(package,U.AssetRegistryDependencyOptions())
    return dict(status='UNAVAILABLE' if values is None else 'READ',packages=[] if values is None else sorted(str(p) for p in values))


try:
    check('exact project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT.resolve())
    check('no dirty maps',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    backup=Path('E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups')/OUT.name
    backup.mkdir(parents=True,exist_ok=False)
    jobs=[];protected={}
    for letter in ('Q','R'):
        folder=ROOT+'/AvatarSample_'+letter+'/Source_'+SOURCE_HASHES[letter][:12]
        package=folder+'/VA_AvatarSample_'+letter+'_Studio2140_VrmAssetList'
        file=disk(package)
        check('original VA hash '+letter,file.is_file() and sha(file)==VA_HASHES[letter])
        copy=backup/file.name;shutil.copy2(file,copy)
        check('byte-exact private VA backup '+letter,sha(copy)==VA_HASHES[letter])
        for other in file.parent.rglob('*.uasset'):
            if other!=file: protected[str(other)]=sha(other)
        REGISTRY.scan_paths_synchronous([folder],force_rescan=True)
        asset=A.load_asset(package)
        check('exact native AssetList '+letter,isinstance(asset,U.VrmAssetListObject) and asset.get_path_name()==package+'.'+package.rsplit('/',1)[1])
        check('owned official source '+letter,A.get_metadata_tag(asset,'HarborCityOwnedBy')==OWNER
              and A.get_metadata_tag(asset,'HarborCitySourceSHA256')==SOURCE_HASHES[letter])
        row=dict(sample=letter,path=package,before_sha256=sha(file),backup=str(copy),
                 dependencies_before=dependencies(package),preflight=native(asset,False))
        R['assets'].append(row);dump()
        check('native confined preflight '+letter,row['preflight'].get('status')=='PASS'
              and row['preflight'].get('actual_outer_package')==package,row['preflight'])
        jobs.append((asset,file,row))
    config=PROJECT/'Config/DefaultEngine.ini';config_hash=sha(config)
    for asset,file,row in jobs:
        row['native_apply']=native(asset,True);dump()
        check('native own-package assignment '+row['sample'],row['native_apply'].get('status')=='PASS'
              and row['native_apply'].get('package_reference_after')==row['path'],row['native_apply'])
        check('save only repaired VA '+row['sample'],A.save_loaded_asset(asset,False))
        row['after_sha256']=sha(file);row['bytes']=file.stat().st_size
        row['readback']=native(asset,False)
        check('native reference after save '+row['sample'],row['readback'].get('status')=='PASS'
              and row['readback'].get('needs_repair')=='false'
              and row['readback'].get('package_reference_after')==row['path'],row['readback'])
        REGISTRY.scan_files_synchronous([str(file)],force_rescan=True)
        row['dependencies_after']=dependencies(row['path']);dump()
        if row['dependencies_after']['status']=='READ':
            check('no serialized ImportDestination dependency '+row['sample'],
                  row['preflight']['known_import_destination'] not in row['dependencies_after']['packages'],row['dependencies_after'])
    check('all other source assets unchanged',all(sha(p)==h for p,h in protected.items()),len(protected))
    check('default-map config unchanged',sha(config)==config_hash)
    check('no maps dirtied',not U.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    R['status']='PASS'
    R['pass_scope']='Native exact-field normalization and saved VA hashes; all other source asset bytes preserved. Fresh-process/cook dependency validation remains NOT_RUN.'
except Exception:
    R['status']='FAIL';R['error']=traceback.format_exc();U.log_error(R['error'])
finally:
    R['ended_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS': raise RuntimeError('Q/R AssetList Package repair failed; see author_result.json')
