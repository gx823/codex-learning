"""Copy only two owned local Village grass textures through native AdvancedCopy.
No source actors, Blueprints or external scripts are run. Original assets stay untouched.
"""
from pathlib import Path
import datetime as dt
import hashlib
import json
import re
import traceback
import unreal as U

W = Path('D:/科研学习/codex学习')
D = W / 'docs/HarborCity_M5_VS2'
P = W / 'HarborCity'
SOURCE = Path('E:/GameDev/Assets/HarborCity/M5_VS2/Environment/HC_VillageSource')
PREFIX = '/Game/Fantastic_Village_Pack/'
ROOTS = [PREFIX + 'textures/T_ENV_TERRAIN_grass_01_' + suffix for suffix in ('BC', 'N')]

def arg(name):
    found = re.findall(r'(?:^|\s)-' + name + r'=(?:"([^"]+)"|(\S+))', U.SystemLibrary.get_command_line())
    assert len(found) == 1
    return found[0][0] or found[0][1]

OUT = Path(arg('M5EvidenceDir')).resolve()
assert OUT.is_relative_to(D.resolve()) and arg('M5AuthorPhase') == 'GroundTextures'
assert not (OUT / 'author_result.json').exists()
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
R = dict(status='RUNNING', phase='GroundTextures', assets=[], started_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
         scope='Two owned local grass textures only; no world, actor, blueprint or original file modifications', runtime='NOT_RUN')
try:
    baseline = D / 'editor_runtime/probe_20260924_012031_241_8abe29f0_VillageReadOnly/source_content_sha256_before.json'
    inventory = json.loads(baseline.read_text(encoding='utf-8-sig'))
    before = {}
    for path in ROOTS:
        relative = 'Content/' + path[6:] + '.uasset'
        source = SOURCE / relative
        row = inventory[relative.replace('/', '\\')]
        assert sha(source) == row['sha256'].lower() and source.stat().st_size == row['bytes']
        before[str(source)] = row['sha256'].lower()
    token = hashlib.sha256((OUT.name + json.dumps(before, sort_keys=True)).encode()).hexdigest()[:12]
    destination = '/Game/HarborCity/M5VS2/Environment/Village_' + token
    R.update(destination=destination, source_sha256=before, baseline=dict(path=str(baseline), sha256=sha(baseline)))
    dry = json.loads(U.HCM5VS2VillageEditor.copy_village_selection(ROOTS, destination, False))
    R['dry_run'] = dry
    assert dry['status'] == 'PASS' and dry['package_count'] == 2
    assert set(row['source'] for row in dry['packages']) == set(ROOTS)
    assert dry['source_bytes'] < 32 * 1024 * 1024
    applied = json.loads(U.HCM5VS2VillageEditor.copy_village_selection(ROOTS, destination, True))
    R['native'] = applied
    assert applied['status'] == 'PASS' and applied['saved_packages_and_reference_remap_verified']
    for row in applied['packages']:
        path = row['destination']
        assert path.startswith(destination + '/')
        texture = U.EditorAssetLibrary.load_asset(path)
        assert isinstance(texture, U.Texture2D)
        disk = P / 'Content' / (path[6:] + '.uasset')
        R['assets'].append(dict(path=path, source=row['source'], sha256=sha(disk), bytes=disk.stat().st_size,
                                width=texture.blueprint_get_size_x(), height=texture.blueprint_get_size_y()))
    assert all(sha(path) == digest for path, digest in before.items())
    R.update(status='PASS', source_unchanged=True)
except Exception:
    R.update(status='FAIL', error=traceback.format_exc())
    U.log_error(R['error'])
finally:
    R['ended_utc'] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / 'author_result.json').write_text(json.dumps(R, ensure_ascii=False, indent=2), encoding='utf-8')
if R['status'] != 'PASS':
    raise RuntimeError('Ground texture copy failed; preserve unique output and report')
