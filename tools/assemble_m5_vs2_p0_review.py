"""Assemble a source-bound, offline P0 art review. Ordinary Python; never runs UE.

Requires a completed native Portraits corner run with all six actual 1080p PNGs.
Writes only a new direct child of docs/HarborCity_M5_VS2/reviews. It never edits,
decodes/re-encodes, crops, grades or composites an image. Failed output is retained.

Usage:
  python assemble_m5_vs2_p0_review.py --portrait-run <actual-result-directory> \
      --bound-run <actual-HeroBoundReview-result-directory> \
      --corner-run <actual-revision6-Base-result-directory> \
      --output D:/科研学习/codex学习/docs/HarborCity_M5_VS2/reviews/P0_<unique>
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import html
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import zipfile
import zlib

WORK = Path('D:/科研学习/codex学习').resolve()
DOC = WORK / 'docs/HarborCity_M5_VS2'
RUNTIME = DOC / 'editor_runtime'
REVIEWS = DOC / 'reviews'
RUNS = {
    'bound': ('20260925_002805_532_da9cffa9_heroboundreview_game/'
              'Lookdev_20260925_002824_D5F95CF7/lookdev_results.json'),
    'corner': ('20260925_005000_964_390c19fe_corner_game/'
               'CornerReview_20260925_005019_AA202C12/corner_review.json'),
    'original': ('20260924_145349_655_0cb4b18a_corner_game/'
                 'CornerReview_20260924_145406_050E5B3A/corner_review.json'),
    'soft': ('20260924_145613_613_4af8f536_corner_game/'
             'CornerReview_20260924_145632_DEA45AF4/corner_review.json'),
}
REPORTS = (
    ('HERO_FIDELITY_REPORT.md', '主角精美度与动作'),
    ('NPC_CAST_REPORT.md', '两名 NPC 样板'),
    ('ISEKAI_ENVIRONMENT_REPORT.md', '异世界港町环境'),
    ('LIGHTING_AND_POST_REPORT.md', '光照与后期'),
)
CORNER_LABELS = tuple(f'{i:02d}_{preset}_{camera}'
    for i, (preset, camera) in enumerate((p, c)
        for p in ('Afternoon', 'Dusk') for c in ('SouthStreet', 'Promenade', 'GuildSquare')))
PORTRAIT_LABELS = tuple(f'{i:02d}_{preset}_{person}'
    for i, (preset, person) in enumerate((p, s)
        for p in ('Afternoon', 'Dusk') for s in ('HeroPortrait', 'QPortrait', 'RPortrait')))


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise RuntimeError(reason)


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def binding(path: Path) -> dict:
    path = path.resolve(strict=True)
    require(path.is_file(), 'Expected file: ' + str(path))
    return {'source_path': str(path), 'sha256': sha(path), 'bytes': path.stat().st_size}


def load_json(path: Path) -> tuple[dict, dict]:
    path = path.resolve(strict=True)
    require(path.is_relative_to(RUNTIME.resolve()), 'Run evidence is outside editor_runtime')
    require(path.stat().st_size <= 32 * 1024 * 1024, 'Run report is unexpectedly large')
    record = binding(path)
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    require(isinstance(data, dict) and data.get('status') == 'PASS', 'Native run must be PASS: ' + str(path))
    require(data.get('user_stop_latched') is False, 'Stopped/unverified native run: ' + str(path))
    require(data.get('visual_acceptance') == 'USER_REVIEW', 'Unexpected native evidence scope')
    return data, record


def png_integrity(path: Path) -> dict:
    """Check the original PNG signature, dimensions, all chunk CRCs and terminal IEND."""
    require(path.is_file() and 45 <= path.stat().st_size <= 64 * 1024 * 1024,
            'Missing or unexpectedly sized PNG: ' + str(path))
    chunks, width, height = 0, None, None
    with path.open('rb') as stream:
        require(stream.read(8) == b'\x89PNG\r\n\x1a\n', 'Invalid PNG signature: ' + str(path))
        while True:
            header = stream.read(8)
            require(len(header) == 8, 'Truncated PNG chunk header')
            length, kind = struct.unpack('>I4s', header)
            require(length <= 64 * 1024 * 1024, 'Oversized PNG chunk')
            payload, checksum = stream.read(length), stream.read(4)
            require(len(payload) == length and len(checksum) == 4, 'Truncated PNG chunk')
            crc = zlib.crc32(payload, zlib.crc32(kind)) & 0xffffffff
            require(crc == struct.unpack('>I', checksum)[0], 'PNG chunk CRC mismatch: ' + str(path))
            if chunks == 0:
                require(kind == b'IHDR' and length == 13, 'PNG must begin with IHDR')
                width, height = struct.unpack('>II', payload[:8])
                require((width, height) == (1920, 1080), 'Only original 1920x1080 PNGs accepted')
            chunks += 1
            require(chunks < 100000, 'Unbounded PNG chunk count')
            if kind == b'IEND':
                require(length == 0 and not stream.read(1), 'Invalid PNG terminal chunk/trailing data')
                break
    return {'width': width, 'height': height, 'png_chunk_crc': 'PASS', 'png_chunks': chunks}


def image_row(report_path: Path, row: dict, basename: str) -> dict:
    require(row.get('status') == 'PASS', 'Capture has not passed native PNG delivery')
    require(row.get('width') == 1920 and row.get('height') == 1080, 'Native capture dimensions differ')
    file = Path(row.get('file', '')).resolve(strict=True)
    require(file == (report_path.parent / basename).resolve(strict=True), 'Capture path/name mismatch')
    record = binding(file)
    require(row.get('file_bytes') == record['bytes'], 'Capture byte count changed: ' + str(file))
    record.update(png_integrity(file))
    record.update(native_label=row.get('label'), native_capture_status='PASS',
                  native_report=str(report_path), native_report_sha256=sha(report_path))
    return record


def corner_run(path: Path, portraits: bool = False, style: str | None = None) -> tuple[dict, list[dict]]:
    path = path.resolve(strict=True)
    data, record = load_json(path)
    labels = PORTRAIT_LABELS if portraits else CORNER_LABELS
    captures = data.get('captures')
    require(isinstance(captures, list) and len(captures) == 6
            and data.get('actual_verified_png_count') == 6, 'Exactly six native PNGs required')
    if portraits:
        require(data.get('profile') == 'Portraits'
                and data.get('input_plan', {}).get('profile') == 'Portraits', 'This is not the new Portraits run')
        require(len(data.get('portrait_eye_locks', [])) == 3, 'Missing actual three-person eye locks')
    if style:
        require(data.get('input_plan', {}).get('material_style') == style, 'Actual style identity mismatch')
    images = []
    for i, row in enumerate(captures):
        require(isinstance(row, dict) and row.get('index') == i and row.get('label') == labels[i],
                'Capture ordering/identity mismatch')
        if portraits:
            require(row.get('request_view_matches_locked_camera') is True
                    and row.get('final_view_matches_locked_camera') is True,
                    'Portrait actual view did not match the locked camera')
        images.append(image_row(path, row, labels[i] + '.png'))
    record.update(status='PASS', native_capture_count=6, visual_acceptance='USER_REVIEW',
                  map=data.get('actual_map'), profile=data.get('profile', 'Base'),
                  material_style=data.get('input_plan', {}).get('material_style'),
                  all_six_pngs_verified=[{'source_path': i['source_path'], 'sha256': i['sha256'],
                      'bytes': i['bytes']} for i in images])
    return record, images


def portrait_report(directory: Path) -> Path:
    directory = directory.resolve(strict=True)
    require(directory.is_dir() and directory.is_relative_to(RUNTIME.resolve()),
            '--portrait-run must be a real result directory inside editor_runtime')
    direct = directory / 'corner_review.json'
    found = [direct] if direct.is_file() else list(directory.glob('CornerReview_*/corner_review.json'))
    require(len(found) == 1, 'Expected exactly one actual corner_review.json in --portrait-run')
    return found[0].resolve(strict=True)


def bound_report(directory: Path | None) -> Path:
    if directory is None:
        return (RUNTIME / RUNS['bound']).resolve(strict=True)
    directory = directory.resolve(strict=True)
    require(directory.is_dir() and directory.is_relative_to(RUNTIME.resolve()),
            '--bound-run must be a real HeroBoundReview result directory inside editor_runtime')
    direct = directory / 'lookdev_results.json'
    found = [direct] if direct.is_file() else list(directory.glob('Lookdev_*/lookdev_results.json'))
    require(len(found) == 1, 'Expected exactly one actual lookdev_results.json in --bound-run')
    return found[0].resolve(strict=True)


def corner_report(directory: Path | None) -> Path:
    if directory is None:
        return (RUNTIME / RUNS['corner']).resolve(strict=True)
    directory = directory.resolve(strict=True)
    require(directory.is_dir() and directory.is_relative_to(RUNTIME.resolve()),
            '--corner-run must be a real Base result directory inside editor_runtime')
    direct = directory / 'corner_review.json'
    found = [direct] if direct.is_file() else list(directory.glob('CornerReview_*/corner_review.json'))
    require(len(found) == 1, 'Expected exactly one actual corner_review.json in --corner-run')
    return found[0].resolve(strict=True)


def preflight(portrait_directory: Path, bound_directory: Path | None = None,
              corner_directory: Path | None = None) -> tuple[list[dict], list[dict], list[dict]]:
    run_records = []
    bound_path = bound_report(bound_directory)
    bound, bound_record = load_json(bound_path)
    shots = bound.get('shots')
    require(isinstance(shots, list) and len(shots) == 8, 'Bound run must contain eight actual shots')
    bound_images = []
    for index in range(8):
        variant = ('LegacyVS1_UsageCompatible', 'Current_Bound')[index % 2]
        label = ('Neutral_FaceFront', 'Neutral_HalfFront', 'Afternoon_FaceFront', 'Dusk_FaceFront')[index // 2]
        row = shots[index]
        require(isinstance(row, dict) and row.get('shot_index') == index and row.get('label') == label
                and row.get('variant') == variant and row.get('native_png_exists') is True,
                'Bound eight-shot identity/order changed')
        require(row.get('material_readiness', {}).get('status') == 'READY', 'Bound material readback not ready')
        bound_images.append(image_row(bound_path, row, f'{index:03d}_{label}_{variant}.png'))
    selected_bound = bound_images[:2]
    bound_record.update(status='PASS', visual_acceptance='USER_REVIEW', native_capture_count=8,
                        selected_native_shot_indices=[0, 1],
                        all_eight_pngs_verified=[{'source_path': i['source_path'], 'sha256': i['sha256'],
                            'bytes': i['bytes']} for i in bound_images])
    run_records.append(bound_record)
    portrait_record, portraits = corner_run(portrait_report(portrait_directory), portraits=True)
    run_records.append(portrait_record)
    corner_path = corner_report(corner_directory)
    corner_data, _ = load_json(corner_path)
    reference_base, _ = load_json(RUNTIME / RUNS['corner'])
    plan, reference_plan = corner_data.get('input_plan', {}), reference_base.get('input_plan', {})
    require(corner_data.get('profile', 'Base') == plan.get('profile', 'Base') == 'Base'
            and not plan.get('material_style'), '--corner-run must be Base, not Portraits or a style override')
    require(corner_data.get('actual_map') == plan.get('map') == '/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0'
            and plan.get('owner') == 'HarborCity_M5_VS2_HarborCorner_P0'
            and plan.get('plan_type') == 'HARBOR_CORNER_NATIVE_VIEWPORT', 'Base source map/owner/type mismatch')
    # A new capture may use the updated Hero binding; its street layout and
    # corner author must still be the exact frozen revision-6 source pair.
    for key in ('layout', 'corner_author'):
        actual_source = plan.get('sources', {}).get(key, {})
        frozen_source = reference_plan.get('sources', {}).get(key, {})
        require(actual_source.get('sha256') == frozen_source.get('sha256')
                and bool(re.fullmatch(r'[0-9a-fA-F]{64}', str(actual_source.get('sha256', ''))))
                and Path(actual_source.get('path', '')).resolve() == Path(frozen_source.get('path', '')).resolve(),
                'Base revision-6 source whitelist mismatch: ' + key)
    corner_record, street = corner_run(corner_path)
    corner_record['frozen_revision6_source_sha256'] = {
        key: plan['sources'][key]['sha256'] for key in ('layout', 'corner_author')}
    run_name = corner_path.parent.parent.name
    run_match = re.search(r'_([0-9a-f]{8})_corner_game$', run_name)
    corner_run_id = run_match.group(1) if run_match else run_name
    run_records.append(corner_record)
    original_record, original = corner_run(RUNTIME / RUNS['original'], style='OriginalHandPainted')
    soft_record, soft = corner_run(RUNTIME / RUNS['soft'], style='SoftAnime')
    run_records.extend((original_record, soft_record))

    # Exact requested set: two Bound faces, four new portraits and six rev6 streets.
    selections = [
        (selected_bound[0], 'main', 'hero_old', '主角 · 旧版脸', '中性光 / LegacyVS1_UsageCompatible'),
        (selected_bound[1], 'main', 'hero_new', '主角 · 当前脸', '同一中性光 / Current_Bound'),
        (portraits[0], 'main', 'hero_afternoon', '主角 · 港町午后', '新 Portraits 原生机位'),
        (portraits[3], 'main', 'hero_dusk', '主角 · 港町黄昏', '同机位 / 黄昏光照比较'),
        (portraits[1], 'main', 'npc_q', 'NPC Q · 午后', '官方 AvatarSample_Q / 当前样板'),
        (portraits[2], 'main', 'npc_r', 'NPC R · 午后', '官方 AvatarSample_R / 当前样板'),
    ]
    for camera, title, a, b in (('south', '南街', 0, 3), ('promenade', '滨海步道', 1, 4), ('guild', '公会广场', 2, 5)):
        selections.extend(((street[a], 'main', camera + '_afternoon', title + ' · 午后', '街角 revision 6 / ' + corner_run_id),
                           (street[b], 'main', camera + '_dusk', title + ' · 黄昏', '同机位 / 街角 revision 6')))
    for preset, title, index in (('afternoon', '午后', 2), ('dusk', '黄昏', 5)):
        selections.extend(((original[index], 'style', 'original_' + preset, '原手绘 · ' + title, '0cb4b18a / 早期公会广场对照'),
                           (soft[index], 'style', 'soft_' + preset, '柔和动漫 · ' + title, '4af8f536 / 同期公会广场对照')))
    images, group_counts = [], {'main': 0, 'style': 0}
    for source, group, key, title, caption in selections:
        sequence = group_counts[group]
        group_counts[group] += 1
        images.append(dict(source, group=group, key=key, title=title, caption=caption,
                           relative_path=f'images/{group}/{sequence:02d}_{key}.png'))
    require(group_counts == {'main': 12, 'style': 4}, 'Exact selected image counts differ')
    require(len({r['source_path'] for r in images}) == 16, 'A selected native image was duplicated')
    reports = []
    for filename, title in REPORTS:
        file = DOC / filename
        require(file.is_file() and 0 < file.stat().st_size <= 256 * 1024, 'Required short report is missing/oversized: ' + filename)
        record = binding(file)
        record.update(relative_path='reports/' + filename, title=title)
        reports.append(record)
    return images, reports, run_records


def html_page(images: list[dict], reports: list[dict], unique: str) -> str:
    by_key = {row['key']: row for row in images}

    def figure(key: str) -> str:
        row = by_key[key]
        file, title = html.escape(row['relative_path'], quote=True), html.escape(row['title'])
        return (f'<figure><a href="{file}" target="_blank" rel="noopener" title="打开未经修改的 1920×1080 原图">'
                f'<img src="{file}" width="1920" height="1080" loading="lazy" alt="{title}"></a>'
                f'<figcaption><strong>{title}</strong><span>{html.escape(row["caption"])}</span>'
                f'<a class="original" href="{file}" target="_blank" rel="noopener">打开原图 ↗</a></figcaption></figure>')

    def section(anchor: str, title: str, subtitle: str, pairs: list[tuple[str, str]]) -> str:
        return (f'<section id="{anchor}"><h2>{html.escape(title)}</h2><p class="note">{html.escape(subtitle)}</p>'
                + ''.join('<div class="pair">' + figure(a) + figure(b) + '</div>' for a, b in pairs) + '</section>')

    body = section('hero', '01 / 主角的脸', '左为旧版对照，右为当前绑定；只比较实机造型与材质，不代表动作已通过。', [('hero_old', 'hero_new')])
    body += section('portraits', '02 / 角色在港町中的光照', '午后与黄昏为原生分时光照比较；不是已完成的动态昼夜系统。',
                    [('hero_afternoon', 'hero_dusk'), ('npc_q', 'npc_r')])
    body += section('street', '03 / 第一处港町街角', 'revision 6 的三个原生机位。每行左午后、右黄昏。',
                    [(c + '_afternoon', c + '_dusk') for c in ('south', 'promenade', 'guild')])
    body += section('style', '04 / 环境材质方向对照', '这四张来自较早的同批公会广场试验；用于材质方向对比，不能当作 revision 6 的最新街景。',
                    [('original_afternoon', 'soft_afternoon'), ('original_dusk', 'soft_dusk')])
    documents = []
    for report in reports:
        text = Path(report['source_path']).read_text(encoding='utf-8-sig')
        documents.append(f'<details><summary>{html.escape(report["title"])}</summary><p><a href="{html.escape(report["relative_path"], quote=True)}">原 Markdown 报告</a></p><pre>{html.escape(text)}</pre></details>')
    return '''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src 'self' file:; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'">
<title>海湾漫游 · P0 原生图审</title><style>
:root{color-scheme:dark;--bg:#11191e;--panel:#1b282f;--ink:#eef3f3;--muted:#b9c6c9;--accent:#b3ddd3;--line:#35464d}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.65 "Microsoft YaHei","Segoe UI",sans-serif}
header,main,footer{max-width:1500px;margin:auto;padding:32px clamp(18px,3vw,48px)}header{padding-top:46px;padding-bottom:12px}
.eyebrow{color:var(--accent);letter-spacing:.13em;font-size:13px}h1{font-size:clamp(28px,4vw,44px);line-height:1.25;margin:14px 0}h2{font-size:25px;margin:0 0 5px}
p{margin:10px 0}.note,.subtle{color:var(--muted)}.gate{padding:16px 20px;border:1px solid #827956;border-radius:10px;background:#302d22;margin:22px 0}
.gate strong{color:#e8d8a4}nav{display:flex;flex-wrap:wrap;gap:10px;margin-top:24px}a{color:var(--accent);text-underline-offset:3px}nav a{padding:7px 13px;border:1px solid var(--line);border-radius:20px;text-decoration:none;font-size:14px}
section{scroll-margin-top:18px;margin-bottom:48px}.pair{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:18px;margin-top:18px}
figure{margin:0;background:var(--panel);border:1px solid var(--line);border-radius:10px;overflow:hidden}figure>a{display:block}img{display:block;width:100%;height:auto;object-fit:contain;background:#090d10}
figcaption{padding:13px 16px}figcaption strong,figcaption span{display:block}figcaption span{font-size:13px;color:var(--muted);margin:4px 0}.original{font-size:13px}
details{border-top:1px solid var(--line);padding:15px 0}summary{cursor:pointer;font-size:18px}pre{white-space:pre-wrap;overflow-wrap:anywhere;font:14px/1.7 "Microsoft YaHei",sans-serif;background:var(--panel);padding:20px;border-radius:8px}
footer{border-top:1px solid var(--line);font-size:13px;color:var(--muted)}a:focus-visible,summary:focus-visible{outline:3px solid #d8bc86;outline-offset:5px}
@media(max-width:650px){.pair{grid-template-columns:1fr}header,main,footer{padding:22px 16px}h2{font-size:22px}}
</style></head><body><header><div class="eyebrow">HARBORCITY / 海湾漫游</div><h1>P0 样板 · 原生图审</h1>
<p class="subtle">12 张主图 · 4 张材质对照 · 1920×1080 原图 · 点击图片放大查看</p>
<div class="gate"><strong>阶段门：USER_REVIEW，等待用户美术认可。</strong><br>这是可离线查看的图片与报告审阅包，不含 EXE，也不是 M5 最终交付。NPC 倒地自然度仍 FAIL / OPEN；静态图不替代动作、玩法或性能验证。</div>
<nav aria-label="图审目录"><a href="#hero">主角旧新</a><a href="#portraits">角色肖像</a><a href="#street">港町日夜</a><a href="#style">材质对照</a><a href="#reports">证据与边界</a></nav></header><main>
''' + body + '<section id="reports"><h2>05 / 证据与边界</h2><p class="note">原图文件字节未改变：没有裁剪、拼图或调色。报告内的历史路径保留为来源记录；本页与所选 16 张原图无需联网。</p>' + ''.join(documents) + '''
<p><a href="SOURCE_MANIFEST.json">SHA256 来源与文件清单</a></p></section></main><footer>
审阅批次：''' + html.escape(unique) + '''。网页只按屏幕宽度等比显示原图，不改变源文件。ZIP 的 CRC 与逐项 SHA256 校验结果在包外 ZIP_VERIFICATION.json 中。
</footer></body></html>'''


def dump_new(path: Path, value: dict) -> None:
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portrait-run', required=True, type=Path)
    parser.add_argument('--bound-run', type=Path, help='New actual HeroBoundReview outer or Lookdev result directory; omitted: historical da9cffa9 run')
    parser.add_argument('--corner-run', type=Path, help='New actual frozen-revision6 Base outer or CornerReview result directory; omitted: historical 390c19fe run')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    require(output.parent == REVIEWS.resolve() and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{5,100}', output.name) is not None,
            '--output must be a new, uniquely named direct child of the VS2 reviews directory (ASCII letters/digits/_/-)')
    require(not output.exists(), 'Refuse to overwrite or reuse any output directory')
    images, reports, runs = preflight(args.portrait_run, args.bound_run, args.corner_run)
    # All prerequisites, including all six Portraits images, pass before creating output.
    REVIEWS.mkdir(parents=True, exist_ok=True)
    output.mkdir(exist_ok=False)
    files = images + reports
    for row in files:
        source, target = Path(row['source_path']), output / row['relative_path']
        target.parent.mkdir(parents=True, exist_ok=True)
        with source.open('rb') as src, target.open('xb') as dst:
            shutil.copyfileobj(src, dst, 1024 * 1024)
        require(sha(target) == row['sha256'] and target.stat().st_size == row['bytes'], 'Byte-identical copy failed')
        row['byte_identical_copy'] = True
    page = html_page(images, reports, output.name)
    with (output / 'index.html').open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(page)
    manifest = {
        'schema': 'HarborCity.M5VS2.P0SampleReview.v1', 'assembly_status': 'PASS',
        'art_gate': 'USER_REVIEW', 'npc_knockdown_art': 'FAIL / OPEN', 'final_delivery': False,
        'executable_included': False, 'main_image_count': 12, 'style_image_count': 4,
        'image_processing': 'NONE: byte-identical native PNG copies; CSS display sizing only',
        'paid_source_assets_included': False, 'generated_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
        'generator': binding(Path(__file__)), 'native_runs': runs, 'images': images, 'reports': reports,
        'html': {'relative_path': 'index.html', 'sha256': sha(output / 'index.html'),
                 'bytes': (output / 'index.html').stat().st_size},
        'scope': 'Static P0 art review only. Not an executable, packaged runtime test, action acceptance or final M5 delivery.',
    }
    dump_new(output / 'SOURCE_MANIFEST.json', manifest)
    names = sorted([r['relative_path'] for r in files] + ['index.html', 'SOURCE_MANIFEST.json'])
    require(len(names) == len(set(names)) == 22, 'Only sixteen PNGs, four reports, HTML and source manifest are packaged')
    expected = {name: {'sha256': sha(output / name), 'bytes': (output / name).stat().st_size} for name in names}
    archive = output / ('P0_SAMPLE_REVIEW_' + output.name + '.zip')
    # Explicit whitelist: never recurse into project/assets, or accidentally include the ZIP itself.
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_STORED, allowZip64=True) as package:
        for name in names:
            package.write(output / name, arcname=name)
    verified = []
    with zipfile.ZipFile(archive, 'r') as package:
        require(package.namelist() == names, 'ZIP entry set/order differs from exact whitelist')
        require(package.testzip() is None, 'ZIP CRC test failed')
        for info in package.infolist():
            digest, crc, size = hashlib.sha256(), 0, 0
            with package.open(info, 'r') as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    digest.update(chunk)
                    crc = zlib.crc32(chunk, crc)
                    size += len(chunk)
            require(digest.hexdigest() == expected[info.filename]['sha256']
                    and size == expected[info.filename]['bytes'] == info.file_size
                    and (crc & 0xffffffff) == info.CRC, 'ZIP entry hash/CRC mismatch: ' + info.filename)
            verified.append(dict(path=info.filename, sha256=digest.hexdigest(), bytes=size,
                                 crc32=f'{info.CRC:08x}', status='PASS'))
    # Prevent a race from silently mixing source/report versions during assembly.
    for row in files + runs:
        require(sha(Path(row['source_path'])) == row['sha256'], 'Source changed during assembly')
    for run in runs:
        for row in run.get('all_six_pngs_verified', []) + run.get('all_eight_pngs_verified', []):
            require(sha(Path(row['source_path'])) == row['sha256'], 'An input capture changed during assembly')
    dump_new(output / 'ZIP_VERIFICATION.json', {
        'status': 'PASS', 'scope': 'Actual ZIP reread: full entry-set, CRC and every entry SHA256',
        'zip': binding(archive), 'entries': verified, 'entry_count': len(verified),
        'all_source_hashes_unchanged': True, 'art_gate': 'USER_REVIEW',
        'self_note': 'This verification report is outside the ZIP to avoid a self-referential archive hash.',
    })
    print(json.dumps({'status': 'PASS', 'html': str(output / 'index.html'), 'zip': str(archive),
                      'verification': str(output / 'ZIP_VERIFICATION.json'), 'main_images': 12,
                      'style_images': 4, 'zip_entries': 22, 'art_gate': 'USER_REVIEW'}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, KeyError, TypeError) as exc:
        print('P0 review assembly FAIL; preserve any unique partial output: ' + str(exc), file=sys.stderr)
        raise SystemExit(1)
