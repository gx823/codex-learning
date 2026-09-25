"""Start the user-authorized VS2 visual sample gate; retain all prior work."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, shutil, subprocess

root=Path(__file__).resolve().parents[1]; project=root/'HarborCity'
doc=root/'docs/HarborCity_M5_VS2';doc.mkdir(exist_ok=True)
assert not (doc/'baseline_inventory.json').exists(), 'Do not replace an existing VS2 baseline'
stamp=datetime.now().strftime('%Y%m%d_%H%M%S')
private=root/'assets/M5_VS2/PrivateBaseline'/stamp
private.mkdir(parents=True,exist_ok=False)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
rows=[]
for folder in ['Source','Config','Content']:
    for p in sorted((project/folder).rglob('*')):
        if not p.is_file():continue
        rel=p.relative_to(project)
        rows.append(dict(path=rel.as_posix(),bytes=p.stat().st_size,sha256=sha(p)))
        if folder!='Content':
            dest=private/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
shutil.copy2(project/'HarborCity.uproject',private/'HarborCity.uproject')
shutil.copy2(root/'docs/PROGRESS.md',private/'PROGRESS.before.md')
candidate=Path('E:/GameDev/Builds/HarborCity/M5_VS1/Candidate_20260923_143846_570_d0819454')
exe=candidate/'Archive/Windows/HarborCity/Binaries/Win64/HarborCity.exe'
assert sha(exe)=='4448dbdc274230f60a03e04df35766fe24d90f10c60be2a03868a260665ae7d4'
record=dict(milestone='M5_VS2',recorded_utc=datetime.now(timezone.utc).isoformat(),
    private_code_config_backup=str(private),previous_candidate=str(candidate),
    previous_exe_sha256=sha(exe),files=rows,
    policy='All old maps, saves, candidates and failures retained. New assets M5VS2. Paid original source remains existing private Selestia folder. No full-project copy.',
    gate='Hero comparison + two finished adult anime NPCs + 50x50m final-quality corner, 8-12 native images. Await user explicit approval before expansion.')
(doc/'baseline_inventory.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
shutil.copy2(Path('C:/Users/HP/.codex/attachments/1dbc9fd6-8f5d-4be3-9c06-9f4af8b506fb/已粘贴的文本.txt'),doc/'REQUEST.md')
(doc/'git_status_before.txt').write_bytes(subprocess.run(['git','status','--short'],cwd=project,capture_output=True).stdout)
status='''# M5-VS2 异世界动漫港町

IN_PROGRESS：先制作主角同机位对照、两名有脸且有表情的成年动漫NPC、约50×50米最终品质样板街角。8—12张原生实机图与2—3种环境做法比较完成后，按用户阶段门停下等待明确“可以”，再扩展250米核心区域。P0未达标不铺P1，不以断言计数代替画面。

用户对VS1 d0819454试玩结论：主角观感FAIL（生硬）；赛博环境方向REJECTED；NPC外观FAIL。VS1独立包四章回归、存档重开、30分钟探索、正式录像保持NOT_RUN。MioV2/V3 REJECTED、V4内部FAIL历史不变。M0 PASS。

旧地图、旧候选、旧存档和旧报告保留。步行1.5、驾驶1.125、转向2倍、ADS1.6/补偿0.625、四种视角、任务稳定ID、战斗与NPC规则保持。

当前素材/实现：研究和源文件核对中；新插件编译、主角新材质/物理/表情、新NPC、新地图和截图均NOT_RUN。用户主角、NPC、环境、光影、BGM验收PENDING。新包/正式录像/ZIP尚未生成。

新增下载预算40GiB，全部E盘；初查E盘剩余约16.96GiB，下载与构建前需继续核对实际空间。不会删除旧包腾空间。外部登录、协议、安全提示和付费交用户本人操作。
'''
(doc/'M5_VS2_STATUS.md').write_text(status,encoding='utf-8')
progress=root/'docs/PROGRESS.md';old=progress.read_text(encoding='utf-8-sig');head,rest=old.split('\n',1)
block=f'''\n\n<!-- HARBORCITY_M5_VS2_BEGIN -->
## M5-VS2 异世界动漫港町：样板阶段进行中

{datetime.now().isoformat(timespec='seconds')} 用户新请求：VS1 d0819454主角观感FAIL（“生硬”）、赛博环境方向REJECTED、NPC外观FAIL。VS1未完成的独立包四章回归、存档重开、30分钟探索、正式录像保持NOT_RUN，不倒填。MioV2/V3 REJECTED、V4内部FAIL不变。

先制作主角对照组、约50×50米最终品质街角和两名新动漫NPC，完成8—12张实机原图及环境风格比较后停下，等待用户明确“可以”再大规模铺场景。当前研究/许可/基线备份已启动，新美术运行NOT_RUN，用户各项验收PENDING。新图与新资源使用M5VS2范围，旧图、旧包、存档不删；冻结玩法参数保留。详见 [VS2状态](HarborCity_M5_VS2/M5_VS2_STATUS.md)。
<!-- HARBORCITY_M5_VS2_END -->
'''
progress.write_text(head+block+rest,encoding='utf-8')
print(json.dumps(dict(status='BASELINE_RECORDED',files=len(rows),backup=str(private)),ensure_ascii=False))
