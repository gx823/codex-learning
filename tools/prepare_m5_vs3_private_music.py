"""Local user music only; never put audio payloads in public evidence directories."""
from pathlib import Path
import subprocess, json, re, hashlib
ROOT=Path('D:/科研学习/codex学习')
PRIVATE=Path('E:/GameDev/Assets/HarborCity/PrivateMusic')
FF=ROOT/'video_build/manga_mad_project_20260827/qa_tools/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg.exe'
OUT=ROOT/'docs/HarborCity_M5_VS3/private'
OUT.mkdir(parents=True,exist_ok=True)
rows=[]
for name,stem in [('梶浦由记 - light your sword.ogg','SW_Private_LightYourSword'),('清水嶺 - Thymon.ogg','SW_Private_Thymon')]:
    source=PRIVATE/name
    r=subprocess.run([str(FF),'-hide_banner','-nostdin','-i',str(source),'-af','loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json','-f','null','-'],capture_output=True)
    log=r.stderr.decode('utf-8',errors='replace')
    (OUT/(stem+'_measure.log')).write_text(log,encoding='utf-8')
    if r.returncode:raise RuntimeError('Loudness measurement failed')
    m=json.loads(re.findall(r'\{[^{}]*"input_i"[^{}]*\}',log)[-1])
    dst=PRIVATE/(stem+'.wav')
    if dst.exists():raise RuntimeError('Refusing to overwrite private normalized music')
    filt=('loudnorm=I=-16:TP=-1.5:LRA=11:linear=true:print_format=json'
          f':measured_I={m["input_i"]}:measured_TP={m["input_tp"]}:measured_LRA={m["input_lra"]}'
          f':measured_thresh={m["input_thresh"]}:offset={m["target_offset"]}')
    p=subprocess.run([str(FF),'-hide_banner','-nostdin','-i',str(source),'-af',filt,'-ar','48000','-ac','2','-c:a','pcm_s16le',str(dst)],capture_output=True)
    log=p.stderr.decode('utf-8',errors='replace');(OUT/(stem+'_normalize.log')).write_text(log,encoding='utf-8')
    if p.returncode:raise RuntimeError('Normalization failed')
    normalized=json.loads(re.findall(r'\{[^{}]*"input_i"[^{}]*\}',log)[-1])
    rows.append(dict(source=str(source),normalized=str(dst),asset='/Game/HarborCity/Private/Music/'+stem,input_lufs=float(m['input_i']),output_lufs=float(normalized['output_i']),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),license='用户私人音乐，仅本机试玩，不得分发',status='NORMALIZED'))
(OUT/'music.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'track':r['asset'].split('/')[-1],'input_lufs':r['input_lufs'],'output_lufs':r['output_lufs']} for r in rows]))
