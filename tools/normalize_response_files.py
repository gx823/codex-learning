"""Add a UTF-8 signature to this project's generated non-ASCII MSVC response files."""
import codecs
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', choices=['Editor', 'Game'], default='Editor')
parser.add_argument('--evidence-dir', type=Path)
parser.add_argument('--configuration', choices=['Development','Shipping'], default='Development')
options = parser.parse_args()
project = root / 'HarborCity'
if not (project / 'HarborCity.uproject').is_file():
    raise RuntimeError('Official project must exist first')
generated = project / 'Intermediate/Build/Win64/x64'
allowed_roots = ((generated / 'HarborCityEditor/Development', generated / 'UnrealEditor/Development/HarborCity',
                  generated / 'UnrealEditor/Development/HarborCityEditor')
                 if options.target == 'Editor' else
                 (generated / 'HarborCity/Development', generated / 'UnrealGame/Development/HarborCity'))
# M5-VS2 source plugins create their own response directories. Apply the
# already-verified BOM repair only to the two authorized project plugins.
search_roots = [generated]
for name in ('VRM4U', 'KawaiiPhysics'):
    plugin_generated = project / 'Plugins' / name / 'Intermediate/Build/Win64/x64'
    search_roots.append(plugin_generated)
    plugin_target = 'UnrealEditor' if options.target == 'Editor' else 'UnrealGame'
    allowed_roots += (plugin_generated / plugin_target / 'Development',)
allowed_roots = tuple(Path(str(p).replace('/Development', '/'+options.configuration).replace('\\Development', '\\'+options.configuration)) for p in allowed_roots)
changes = []
for path in (p for directory in search_roots for p in directory.rglob('*.rsp')):
    if not any(path.resolve().is_relative_to(allowed.resolve()) for allowed in allowed_roots):
        continue
    if not path.resolve().is_relative_to(project.resolve()):
        raise RuntimeError('Generated response path escapes this project')
    data = path.read_bytes()
    if data.startswith((codecs.BOM_UTF8, codecs.BOM_UTF16_LE, codecs.BOM_UTF16_BE)):
        continue
    text = data.decode('utf-8')
    if any(ord(c) > 127 for c in text):
        changes.append((path, data))
stamp = datetime.now().astimezone()
ev = root / 'docs/HarborCity_M0_E2/installed_20260912/evidence'
if options.evidence_dir:
    ev = options.evidence_dir.resolve()
    if not ev.is_relative_to((root / 'docs').resolve()):
        raise RuntimeError('Response evidence must remain inside project docs')
ev.mkdir(parents=True, exist_ok=True)
record = {'recorded_at': stamp.isoformat(), 'scope': [str(p) for p in search_roots], 'operation': 'Add UTF-8 BOM; text unchanged', 'files': []}
if changes:
    backup = ev / ('response_files_before_bom_' + stamp.strftime('%Y%m%d_%H%M%S_%f') + '.zip')
    with zipfile.ZipFile(backup, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for path, data in changes:
            archive.writestr(path.relative_to(project).as_posix(), data)
    record['backup'] = str(backup)
    for path, data in changes:
        updated = codecs.BOM_UTF8 + data
        path.write_bytes(updated)
        if path.read_bytes().decode('utf-8-sig') != data.decode('utf-8'):
            raise RuntimeError('Response text verification failed')
        record['files'].append({'path': str(path.relative_to(project)), 'before_sha256': hashlib.sha256(data).hexdigest(), 'after_sha256': hashlib.sha256(updated).hexdigest()})
record['changed_count'] = len(changes)
output = ev / ('response_normalization_' + stamp.strftime('%Y%m%d_%H%M%S_%f') + '.json')
output.write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'changed_count': len(changes), 'evidence': str(output)}, ensure_ascii=True))
