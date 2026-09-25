"""Bounded native no-key/with-key check for one purchased Selestia asset.

Run only after a successful fresh package. Extracted data/logs stay in a unique
private Selestia directory. This invokes UnrealPak, never UE/editor/game/UI.
It neither changes keys nor modifies the candidate, and proves only the tested
direct extraction boundary, not absolute resistance to reverse engineering.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import uuid

WORK = Path('D:/科研学习/codex学习')
BUILD_ROOT = Path('E:/GameDev/Builds/HarborCity/M5_VS3').resolve()
PRIVATE_ROOT = WORK / 'assets/M5_VS1/hero/Selestia/PrivateCryptoVerification'
EXE = Path('E:/UE_5.8/Engine/Binaries/Win64/UnrealPak.exe')
TARGET = 'HarborCity/Content/HarborCity/M5VS2/HeroSelestia/Animation/GASMotion/Batch_1d6685a10359/SKM_Selestia_Warp'
ALLOWED_SUFFIXES = {'.uasset', '.ubulk', '.uptnl'}


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def allowed_filename(filename):
    # Exactly the one asset, never _Arms, a wildcard, or another source model.
    name = filename.replace('\\', '/')
    relative = name[9:] if name.startswith('../../../') else name
    path = PurePosixPath(relative)
    if str(path.with_suffix('')).casefold() != TARGET.casefold() or path.suffix.lower() not in ALLOWED_SUFFIXES:
        return None
    return str(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', required=True, type=Path)
    args = parser.parse_args()
    candidate = args.candidate.resolve()
    require(candidate.parent == BUILD_ROOT and re.fullmatch(r'Candidate_\d{8}_\d{6}_\d{3}_[a-f0-9]{8}', candidate.name),
        'Candidate must be one uniquely named M5 build directory')
    manifest_file = candidate / 'package_manifest.json'
    manifest = json.loads(manifest_file.read_text(encoding='utf-8-sig'))
    require(manifest.get('status') == 'PASS' and manifest.get('milestone') == 'M5_VS3', 'A successful M5 package is required')
    require(Path(manifest['candidate_directory']).resolve() == candidate, 'Candidate manifest path mismatch')
    require(manifest.get('crypto_configuration', {}).get('status') == 'PASS_CONFIGURATION_ONLY', 'Package lacks full-asset Crypto preflight')
    archive = (candidate / 'Archive').resolve()
    game = Path(manifest['actual_executable']).resolve()
    require(game.is_relative_to(archive) and game.is_file() and sha(game).lower() == manifest['actual_executable_sha256'].lower(),
        'Actual candidate game executable differs from manifest')
    cook = Path(manifest['cook_output_directory']).resolve()
    require(cook.is_relative_to((WORK / 'docs/HarborCity_M5_VS3/package_work').resolve()), 'Crypto cache must belong to private M5 Cook work')
    crypto = (cook / 'HarborCity/Metadata/Crypto.json').resolve()
    require(crypto.is_relative_to(cook), 'Private Crypto cache resolves outside this Cook')
    require(crypto.is_file() and EXE.is_file(), 'Native UnrealPak or private Cook Crypto cache missing')
    # Read only for output redaction; never serialize or print private key values.
    cache = json.loads(crypto.read_text(encoding='utf-8-sig'))
    encoded = cache.get('EncryptionKey', {}).get('Key', '')
    import base64
    try:
        key = base64.b64decode(encoded, validate=True)
    except Exception:
        raise RuntimeError('Private Crypto cache key encoding invalid; value suppressed') from None
    require(len(key) == 32 and any(key), 'Private Crypto cache must contain a valid 32-byte key')
    patterns = [re.escape(encoded), re.escape(key.hex()), r'\s*,\s*'.join('0x' + format(x, '02x') for x in key)]

    def redact(text):
        for pattern in patterns:
            text = re.sub(pattern, '[PRIVATE_KEY_REDACTED]', text, flags=re.I)
        return text

    entries = manifest.get('pak_files', [])
    require(entries, 'No bound archive fingerprints')
    containers = []
    for entry in entries:
        path = Path(entry['FullName']).resolve()
        require(path.is_relative_to(archive) and path.is_file() and path.stat().st_size == entry['Length'] and
            sha(path).lower() == entry['SHA256'].lower(), 'Container path, length or SHA differs from candidate')
        containers.append(path)
    require(any(p.suffix == '.utoc' for p in containers), 'This bounded verifier requires IoStore; no fallback claim')
    stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%d_%H%M%S') + '_' + uuid.uuid4().hex[:8]
    run = PRIVATE_ROOT / stamp
    run.mkdir(parents=True, exist_ok=False)
    logs = run / 'logs'
    logs.mkdir()
    report_path = run / 'verification.json'
    report = dict(schema=1, status='RUNNING', candidate=str(candidate), candidate_manifest_sha256=sha(manifest_file),
        exe_sha256=manifest.get('actual_executable_sha256'), unrealpak_sha256=sha(EXE),
        crypto_cache_sha256=sha(crypto), target='/Game/'+TARGET.split('HarborCity/Content/',1)[1],
        containers=[dict(path=str(p), sha256=sha(p)) for p in containers], calls=[], targets=[],
        private_run_directory=str(run), game_loading='NOT_RUN', visual='USER_REVIEW',
        claim_scope='Only bound container flags and one asset direct extraction without supplied keys. No absolute anti-extraction or legal-compliance claim.')

    def dump():
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    def native(arguments, label):
        n = len(report['calls'])
        native_log = logs / ('native_' + str(n) + '.log')
        argv = [str(EXE)] + arguments + ['-unattended', '-UTF8Output', '-abslog=' + str(native_log)]
        result = subprocess.run(argv, cwd=run, shell=False, capture_output=True, timeout=180)
        output = redact((result.stdout + result.stderr).decode('utf-8', errors='replace'))
        (logs / (str(n) + '_' + label + '.txt')).write_text(output, encoding='utf-8')
        require(native_log.is_file(), 'Native abslog is missing; verification evidence is incomplete')
        native_text = redact(native_log.read_text(encoding='utf-8', errors='replace'))
        native_log.write_text(native_text, encoding='utf-8')
        # Arguments contain the cache path only, never key material.
        report['calls'].append(dict(label=label, arguments=argv[1:], exit_code=result.returncode,
            redacted_console_sha256=sha(logs / (str(n) + '_' + label + '.txt')),
            redacted_native_log_sha256=sha(native_log) if native_log.is_file() else None,
            validation_output_scope='Redacted console plus native abslog; Log-level extraction summary may be absent from the console.'))
        dump()
        # UE5.8 IoStoreUtilities.cpp:9097 emits the exact count/error summary at
        # Log verbosity. It is present in the native file even when the console
        # only prints Display. Exit 0 alone is insufficient: the native function
        # returns true after reporting per-chunk errors, so retain the strict
        # count/error and exact output-name/byte checks below.
        return result.returncode, output + '\n' + native_text

    try:
        # Index metadata check is separate from the actual IoStore payload test.
        for pak in (p for p in containers if p.suffix == '.pak'):
            code, text = native([str(pak), '-Info', '-CryptoKeys=' + str(crypto)], 'pak_info')
            require(code == 0 and re.search(r'\bbEncryptedIndex:\s*1\b', text), 'Pak index encryption was not verified')
        found = []
        for toc in (p for p in containers if p.suffix == '.utoc' and p.name.lower() != 'global.utoc'):
            listing = run / (toc.stem + '.csv')
            code, text = native(['IoStore', '-List=' + str(toc), '-CSV=' + str(listing), '-CryptoKeys=' + str(crypto)], 'list_with_key')
            require(code == 0 and listing.is_file(), 'Keyed container listing failed')
            with listing.open(encoding='utf-8-sig', newline='') as stream:
                rows = [{str(k).strip(): str(v).strip() for k, v in row.items()} for row in csv.DictReader(stream, skipinitialspace=True)]
            require(rows and all('Filename' in row and 'Size' in row and 'ChunkType' in row for row in rows), 'Unsupported or empty native CSV listing')
            for row in rows:
                relative = allowed_filename(row['Filename'])
                if relative:
                    found.append((toc, row, relative))
        require(found and len({p for p, _, _ in found}) == 1, 'Exact Selestia target must occur in one container')
        toc = found[0][0]
        code, text = native([str(toc), '-Info', '-CryptoKeys=' + str(crypto)], 'utoc_info')
        require(code == 0 and 'IoStore Container File:' in text and re.search(r'\bEncrypted:\s*1\b', text), 'Actual target IoStore container encryption not verified')
        names = sorted({row['Filename'] for _, row, _ in found})
        require(any(name.lower().endswith('/'+PurePosixPath(TARGET).name.lower()+'.uasset') for name in names), 'Target export bundle not listed')
        for index, name in enumerate(names):
            group = [row for _, row, _ in found if row['Filename'] == name]
            relative = allowed_filename(name)
            require(relative is not None and len(group) == 1, 'Ambiguous native chunk filename; stop rather than broadening extraction')
            row = group[0]
            size = int(row['Size'])
            require(size > 0, 'Empty target chunk')
            with_key, no_key = run / ('with_key_' + str(index)), run / ('no_key_' + str(index))
            with_key.mkdir()
            no_key.mkdir()
            base = ['IoStore', '-FindAndExtract=' + str(toc), '-PackageFilter=' + name]
            code, text = native(base + ['-OutPath=' + str(with_key), '-CryptoKeys=' + str(crypto)], 'extract_with_key')
            finished = re.findall(r'Finished extracting\s+(\d+)\s+chunks\s+\(including\s+(\d+)\s+errors\)\.', text)
            # Console and abslog may repeat the same summary; none may contradict
            # the one exact chunk/zero errors positive control.
            require(code == 0 and finished and all((int(count), int(errors)) == (1, 0) for count, errors in finished)
                and 'Fixup extract path' not in text,
                'Keyed positive control did not read the exact target cleanly')
            base_path = with_key / relative
            expected = [base_path.with_suffix('.uheader'), base_path.with_suffix('.uexp')] if row['ChunkType'] == 'ExportBundleData' else [base_path]
            actual = sorted(p for p in with_key.rglob('*') if p.is_file())
            require(set(actual) == set(expected) and all(p.stat().st_size > 0 for p in expected) and
                sum(p.stat().st_size for p in expected) == size, 'Keyed positive control output names/sizes differ from native listing')
            code, text = native(base + ['-OutPath=' + str(no_key)], 'extract_no_key')
            missing_keys = re.findall(r"Missing decryption key for IoStore container file '([^']+)'", text)
            require(code != 0 and missing_keys and all(Path(path).resolve() == toc for path in missing_keys)
                and not any(p.is_file() for p in no_key.rglob('*')),
                'No-key extraction did not fail at the verified missing-key boundary')
            report['targets'].append(dict(filename=name, chunk_type=row['ChunkType'], bytes=size,
                keyed_outputs=[dict(relative_path=str(p.relative_to(with_key)), bytes=p.stat().st_size, sha256=sha(p)) for p in expected],
                with_key='PASS_POSITIVE_CONTROL', without_key='PASS_DIRECT_EXTRACTION_REFUSED'))
            dump()
        require(all(sha(Path(e['FullName'])).lower() == e['SHA256'].lower() for e in entries), 'Candidate containers changed during verification')
        report['status'] = 'PASS_BOUNDED_DIRECT_EXTRACTION_CHECK'
    except Exception as exc:
        report['status'] = 'FAIL'
        # Never include exception objects that could contain key values/JSON lines.
        report['error_type'] = type(exc).__name__
        report['error'] = redact(str(exc)) if isinstance(exc, RuntimeError) else 'Native verification incomplete; inspect private redacted logs.'
    finally:
        dump()
    print(json.dumps(dict(status=report['status'], private_report=str(report_path), report_sha256=sha(report_path)), ensure_ascii=False))
    return 0 if report['status'] == 'PASS_BOUNDED_DIRECT_EXTRACTION_CHECK' else 1


if __name__ == '__main__':
    raise SystemExit(main())
