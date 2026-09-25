"""Three private core assets for isolated native FootPlacement A/B.

No source asset writes, no actor movement or runtime launch. Fixture/first-person
arms integration is a subsequent explicit stage, not implied by asset PASS.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

WORK = Path('D:/科研学习/codex学习').resolve()
PROJECT = WORK / 'HarborCity'
DOC = WORK / 'docs/HarborCity_M5_VS2'
BASE = DOC / 'editor_runtime/author_20260925_101813_171_d15e10ec_GaitSoleCaptureAuthor/author_result.json'
BASE_RELOAD = DOC / 'editor_runtime/author_20260925_101905_282_e91e3747_GaitSoleCaptureReload/author_result.json'
A = U.EditorAssetLibrary


def argument(name):
    matches = re.findall(r'(?:^|\s)-'+re.escape(name)+r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(matches) == 1, 'One exact '+name+' required'
    return matches[0][0] or matches[0][1]


OUT = Path(argument('M5EvidenceDir')).resolve()
PHASE = argument('M5AuthorPhase')
assert PHASE in ('FootPlacementABAuthor', 'FootPlacementABReload')
assert OUT.is_relative_to(DOC/'editor_runtime') and not (OUT/'author_result.json').exists()
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = hashlib.sha256(str(OUT).encode()).hexdigest()[:12]
DEST = '/Game/HarborCity/M5VS2/FootPlacementAB/Batch_'+TOKEN
PROTECTED = {}
R = dict(schema='HarborCity.M5VS2.FootPlacementAB.Author.v1', phase=PHASE, status='RUNNING',
         checks=[], assets=[], runtime='NOT_RUN', visible_sliding='USER_REVIEW', fixture='NOT_AUTHORED_BY_THIS_STAGE',
         scope='Private skeleton/mesh/ABP only. Existing 28-sample BlendSpace, clips, 247 raw bones and selected Hero untouched.',
         started_utc=dt.datetime.now(dt.timezone.utc).isoformat())


def dump():
    (OUT/'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')


def check(name, value, observed=None):
    R['checks'].append(dict(name=name,status='PASS' if value else 'FAIL',observed=observed)); dump()
    if not value: raise RuntimeError(name+': '+repr(observed))


def sha(path):
    with Path(path).open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()


def doc(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def package(value):
    return (value.get_path_name() if hasattr(value,'get_path_name') else str(value)).split('.')[0]


def disk(path):
    path = package(path)
    assert path.startswith('/Game/HarborCity/') and '..' not in path
    return PROJECT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')


def protect(path, expected=None):
    path = Path(path).resolve(); value = sha(path)
    check('source hash '+str(path), expected is None or value == expected.lower())
    PROTECTED[str(path)] = value


def completed(path, phase):
    path = Path(path).resolve(); check('owned report',path.is_relative_to(DOC/'editor_runtime'))
    protect(path); protect(path.parent/'commandlet.json')
    data, command = doc(path), doc(path.parent/'commandlet.json')
    check('completed '+phase, data.get('status')=='PASS' and data.get('phase')==phase
          and command.get('phase')==phase and command.get('status')=='PASS' and command.get('exit_code')==0)
    return data


def load(path, kind):
    result = A.load_asset(package(path)); check('load '+package(path),isinstance(result,kind)); return result


def native(candidate, apply):
    return json.loads(U.HCM5VS2FootPlacementEditor.configure_candidate(
        load(R['source']['animation'],U.AnimBlueprint),load(R['source']['mesh'],U.SkeletalMesh),
        load(candidate['animation'],U.AnimBlueprint),load(candidate['skeleton'],U.Skeleton),
        load(candidate['mesh'],U.SkeletalMesh),apply))


try:
    check('actual project',Path(U.Paths.convert_relative_path_to_full(U.Paths.project_dir())).resolve()==PROJECT)
    descriptor_path = PROJECT/'HarborCity.uproject'
    protect(descriptor_path)
    descriptor_modules = [m for m in doc(descriptor_path)['Modules'] if m['Name'] == 'HarborCityEditor']
    check('custom graph module loads in uncooked game only', len(descriptor_modules) == 1
          and descriptor_modules[0]['Type'] == 'UncookedOnly' and descriptor_modules[0]['LoadingPhase'] == 'Default')
    R['project_descriptor'] = dict(path=str(descriptor_path), sha256=sha(descriptor_path),
                                   graph_module_type='UncookedOnly', graph_module_loading_phase='Default')

    check('no PIE or dirty packages',not U.EditorLevelLibrary.get_pie_worlds(False)
          and not U.EditorLoadingAndSavingUtils.get_dirty_map_packages()
          and not U.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    check('separate editor helper actually loaded',hasattr(U,'HCM5VS2FootPlacementEditor'))
    base = completed(BASE,'GaitSoleCaptureAuthor'); reload = completed(BASE_RELOAD,'GaitSoleCaptureReload')
    check('exact prior native source/reload',reload['author_sha256']==sha(BASE)
          and reload['fresh_process_disk_readback']=='PASS' and reload['assets']==base['assets'])
    R['source'] = base['binding']
    R['source_author'] = dict(path=str(BASE),sha256=sha(BASE))
    R['source_reload'] = dict(path=str(BASE_RELOAD),sha256=sha(BASE_RELOAD))
    for path,digest in base['source_preservation']['sha256_before'].items():
        if Path(path).suffix.lower() in ('.uasset','.umap'):
            protect(path,digest)
    for row in base['assets']:
        file = disk(row['path']).with_suffix('.umap' if row['path']==base['map'] else '.uasset')
        protect(file,row['sha256'])
    source_bp = load(R['source']['animation'],U.AnimBlueprint)
    source_mesh = load(R['source']['mesh'],U.SkeletalMesh)
    source_skeleton = source_mesh.get_editor_property('skeleton')
    protect(disk(source_skeleton))
    space = load(R['source']['blendspace'],U.BlendSpace)
    samples = list(space.get_editor_property('sample_data'))
    check('actual unchanged 28 samples',len(samples)==28)
    sample_manifest=[]
    for s in samples:
        animation = s.get_editor_property('animation')
        check('actual referenced clip',isinstance(animation,U.AnimSequence))
        protect(disk(animation))
        value = s.get_editor_property('sample_value')
        sample_manifest.append(dict(animation=package(animation),sha256=sha(disk(animation)),
                                    rate_scale=s.get_editor_property('rate_scale'),value=[value.x,value.y,value.z]))
    R['samples_unchanged'] = sample_manifest
    R['modules'] = [dict(path=str(PROJECT/'Binaries/Win64'/name),sha256=sha(PROJECT/'Binaries/Win64'/name))
                    for name in ('UnrealEditor-HarborCity.dll','UnrealEditor-HarborCityEditor.dll')]
    if PHASE == 'FootPlacementABReload':
        # Exact author selected by argument; never silently choose another latest candidate.
        prior_path = Path(argument('M5FootPlacementAuthor')).resolve()
        prior = completed(prior_path,'FootPlacementABAuthor')
        check('same project descriptor as actual author', prior.get('project_descriptor') == R['project_descriptor'])
        check('same frozen script',sha(Path(__file__))==doc(prior_path.parent/'commandlet.json')['script_sha256'].lower())
        check('same two compiled modules',R['modules']==prior['modules'])
        check('exact retained source inputs',prior['source']==R['source'] and prior['samples_unchanged']==sample_manifest)
        candidate = prior['candidate']; candidate_root = candidate['animation'].rsplit('/',1)[0]
        check('strict private candidate batch',re.fullmatch(r'/Game/HarborCity/M5VS2/FootPlacementAB/Batch_[0-9a-f]{12}',candidate_root) is not None)
        expected = {candidate_root+'/SK_Placement',candidate_root+'/SKM_Placement',candidate_root+'/ABP_Selestia_GASMotion_Placement'}
        check('exact three core packages',len(prior['assets'])==3 and {x['path'] for x in prior['assets']}==expected)
        for row in prior['assets']: protect(disk(row['path']),row['sha256'])
        readback = native(candidate,False)
        check('fresh process native readback',readback.get('status')=='PASS',readback)
        check('saved native configuration exact',readback==prior['native'])
        R.update(candidate=candidate,native=readback,assets=prior['assets'],author_report=str(prior_path),
                 author_sha256=sha(prior_path),fresh_process_disk_readback='PASS')
    else:
        check('unique private destination',not A.does_directory_exist(DEST) and not disk(DEST+'/unused').parent.exists())
        candidate = dict(animation=DEST+'/ABP_Selestia_GASMotion_Placement',skeleton=DEST+'/SK_Placement',mesh=DEST+'/SKM_Placement')
        for source,key,kind in ((source_skeleton,'skeleton',U.Skeleton),(source_mesh,'mesh',U.SkeletalMesh),(source_bp,'animation',U.AnimBlueprint)):
            obj = A.duplicate_asset(package(source),candidate[key])
            check('fresh private duplicate '+key,isinstance(obj,kind))
        R['candidate'] = candidate
        configured = native(candidate,True)
        check('actual native configure',configured.get('status')=='PASS',configured)
        R['native'] = configured
        for key in ('skeleton','mesh','animation'):
            obj=A.load_asset(candidate[key]);A.set_metadata_tag(obj,'HarborCityOwnedBy','HarborCity_M5VS2_FootPlacementAB')
            check('native save '+key,A.save_loaded_asset(obj,False))
            R['assets'].append(dict(path=candidate[key],sha256=sha(disk(obj))))
        check('same-process readback',native(candidate,False)==configured)
    R['status']='PASS'
except Exception as exc:
    R.update(status='FAIL',error=repr(exc),traceback=traceback.format_exc());U.log_error(R['traceback'])
finally:
    changed=[p for p,h in PROTECTED.items() if not Path(p).is_file() or sha(p)!=h]
    R['source_preservation']=dict(sha256_before=PROTECTED,changed=changed)
    if changed:R.update(status='FAIL',error='Protected source changed')
    R['finished_utc']=dt.datetime.now(dt.timezone.utc).isoformat();dump()
if R['status']!='PASS': raise RuntimeError('FootPlacement core author failed; preserve failed assets and original evidence')
