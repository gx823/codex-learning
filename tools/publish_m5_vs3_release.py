"""Publish this explicitly authorized review, using GCM credentials only in memory."""
import hashlib
import json
import mimetypes
import os
from pathlib import Path
import subprocess
from urllib.parse import urlsplit

import requests

ROOT = Path('D:/科研学习/codex学习')
OUT = ROOT / 'docs/HarborCity_M5_VS3/github'
PLAN = json.loads((OUT / 'upload_plan.json').read_text(encoding='utf-8'))
REPO = PLAN['repository']
API = f'https://api.github.com/repos/{REPO}'
STATE = {'repository': REPO, 'tag': PLAN['release_tag'], 'assets': [], 'status': 'IN_PROGRESS'}


def save():
    (OUT / 'publication_result.json').write_text(
        json.dumps(STATE, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def git(*args):
    result = subprocess.run(['git', '-c', 'gc.auto=0', '-c', 'maintenance.auto=false', *args],
                            cwd=PLAN['worktree'], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError('Git metadata command failed')
    return result.stdout.strip()


def run():
    commit = git('rev-parse', 'HEAD')
    STATE['commit'] = commit
    credential = subprocess.run(
        ['git', 'credential', 'fill'], cwd=ROOT,
        input=f'protocol=https\nhost=github.com\npath={REPO}.git\n\n',
        capture_output=True, text=True,
        env=dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='Never'))
    if credential.returncode:
        raise RuntimeError('Existing GitHub credential unavailable')
    fields = dict(line.split('=', 1) for line in credential.stdout.splitlines() if '=' in line)
    token = fields.get('password')
    if not token:
        raise RuntimeError('Existing GitHub credential has no token')
    session = requests.Session()
    headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'HarborCity-Authorized-Upload'}
    del credential, fields, token

    def api(method, url, allow_404=False, **kwargs):
        parsed = urlsplit(url)
        if parsed.scheme != 'https' or parsed.hostname not in ('api.github.com', 'uploads.github.com'):
            raise RuntimeError('Unexpected API host')
        extra = kwargs.pop('headers', {})
        response = session.request(method, url, headers={**headers, **extra},
                                   allow_redirects=False, timeout=(30, 900), **kwargs)
        if allow_404 and response.status_code == 404:
            return None
        if not 200 <= response.status_code < 300:
            # Never print request headers or credential provider output.
            raise RuntimeError(f'GitHub {method} {parsed.path} returned HTTP {response.status_code}')
        return response.json()

    remote = api('GET', API + '/commits/main')
    if remote['sha'] != commit:
        raise RuntimeError('Remote main does not match the prepared commit')
    release = api('GET', API + '/releases/tags/' + PLAN['release_tag'], allow_404=True)
    body = PLAN['release_body']

    if release is None:
        release = api('POST', API + '/releases', json={
            'tag_name': PLAN['release_tag'], 'target_commitish': commit,
            'name': PLAN['release_name'],
            'body': body, 'draft': True, 'prerelease': True, 'make_latest': 'false'})
    elif release.get('target_commitish') != commit:
        raise RuntimeError('Existing release is not attached to this prepared commit')
    STATE.update(release_id=release['id'], release_url=release['html_url'])
    save()
    print(json.dumps({'stage': 'release_ready', 'draft': release['draft']}), flush=True)
    existing = {a['name']: a for a in api('GET', API + f"/releases/{release['id']}/assets?per_page=100")}
    upload_url = release['upload_url'].split('{', 1)[0]

    for item in PLAN['release_assets']:
        path = Path(item['path'])
        if path.stat().st_size != item['size']:
            raise RuntimeError('Local asset size changed after preparation: ' + item['name'])
        asset = existing.get(item['name'])
        if asset is None:
            if not release['draft']:
                raise RuntimeError('Refusing to modify an incomplete published release')
            print(json.dumps({'stage': 'uploading', 'name': item['name'], 'bytes': item['size']}), flush=True)
            with path.open('rb') as stream:
                asset = api('POST', upload_url, params={'name': item['name']}, data=stream,
                            headers={'Content-Type': mimetypes.guess_type(path.name)[0] or 'application/octet-stream',
                                     'Content-Length': str(item['size'])})
        if asset['state'] != 'uploaded' or asset['size'] != item['size']:
            raise RuntimeError('Remote asset state or size mismatch: ' + item['name'])
        digest = asset.get('digest')
        if digest:
            if digest != 'sha256:' + item['sha256']:
                raise RuntimeError('Remote asset SHA256 mismatch: ' + item['name'])
            verification = 'GitHub SHA256 and size match'
        else:
            raise RuntimeError('GitHub asset digest unavailable; keep draft for explicit download verification')
        STATE['assets'].append({k: asset.get(k) for k in
                                ('id', 'name', 'size', 'digest', 'state', 'browser_download_url')})
        STATE['assets'][-1]['verification'] = verification
        save()
        print(json.dumps({'stage': 'verified', 'name': item['name']}), flush=True)

    if release['draft']:
        release = api('PATCH', API + f"/releases/{release['id']}",
                      json={'draft': False, 'prerelease': True, 'make_latest': 'false'})
    check = api('GET', API + '/releases/tags/' + PLAN['release_tag'])
    if check['draft'] or not check['prerelease']:
        raise RuntimeError('Published prerelease verification failed')
    STATE.update(status='PUBLISHED_VERIFIED', release_url=check['html_url'],
                 draft=check['draft'], prerelease=check['prerelease'])
    save()
    print(json.dumps({'status': STATE['status'], 'url': STATE['release_url'], 'commit': commit}), flush=True)


if __name__ == '__main__':
    try:
        run()
    except Exception as exc:
        # Exception class and controlled RuntimeError messages only; requests can contain URL details.
        STATE['status'] = 'INCOMPLETE'
        STATE['error'] = str(exc) if isinstance(exc, RuntimeError) else type(exc).__name__
        save()
        print(json.dumps({'status': 'INCOMPLETE', 'error': STATE['error']}), flush=True)
        raise SystemExit(1)
