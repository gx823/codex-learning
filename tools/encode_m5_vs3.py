"""Preserve measured native JPEG/PNG frame timestamps and actual master-submix audio."""
from pathlib import Path
import argparse,json,subprocess
from PIL import Image
from encode_m4_r2_playthrough import inspect_pcm_wav,make_timeline
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS3'
ap=argparse.ArgumentParser();ap.add_argument('capture');ap.add_argument('output');a=ap.parse_args()
c=Path(a.capture).resolve();out=Path(a.output).resolve()
assert c.is_relative_to(D.resolve()) and out.is_relative_to(D.resolve()) and not out.exists()
r=json.loads(c.read_text('utf-8-sig'))
assert r['status']=='CAPTURED_PENDING_REVIEW' and r['write_failures']==0
assert r['capture_geometry_valid'] is True and (r['width'],r['height'])==(1920,1080)
assert r['requested_size']==r['output_size']==dict(width=1920,height=1080)
assert r['stop_reason']=='m5_gameplay_sequence_finished'
if out.name=='M5_VS3_PLAYTHROUGH.mp4':
 validation=json.loads((D/'standalone_validation.json').read_text('utf-8-sig'))
 assert validation['status']=='COMPLETE'
 demo=next(x for x in validation['runs'] if x['mode']=='m5_vs3_demo' and x.get('capture') and Path(x['capture']).resolve()==c)
 assert Path(demo['capture']).resolve()==c and demo['launch']['kind']=='STANDALONE_PACKAGE'
 assert '-M5PublicCapture' in demo['launch']['arguments']
 assert demo['result_status']=='COMPLETE'
wav=c.parent/r['audio_file'];audio=inspect_pcm_wav(wav)
assert audio['signal_present'],'Real audio must contain signal'
t=make_timeline(r,audio);lines=['ffconcat version 1.0'];frames=r['frames']
if out.name=='M5_VS3_PLAYTHROUGH.mp4':assert 240<=t['duration_seconds']<=360
for i,f in enumerate(frames):
 p=(c.parent/f['file']).resolve();assert p.is_relative_to(c.parent) and p.is_file() and p.suffix.lower() in ('.jpg','.png')
 if i in (0,len(frames)-1):
  with Image.open(p) as im:assert im.size==(r['width'],r['height'])
 lines+=['file '+"'"+p.as_posix().replace("'","'\\''")+"'",'option framerate 1000']
 if i+1<len(frames):lines+=['duration %.6f'%((t['relative_microseconds'][i+1]-t['relative_microseconds'][i])/1e6)]
concat=out.with_suffix('.ffconcat');concat.write_text('\n'.join(lines)+'\n','utf-8')
ff=W/'video_build/manga_mad_project_20260827/qa_tools/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg.exe'
filters='atrim=start_sample=%d:end_sample=%d,asetpts=PTS-STARTPTS,adelay=%dS:all=1'%(t['audio_trim_start_sample'],t['audio_trim_end_sample'],t['audio_delay_samples'])
cmd=[str(ff),'-hide_banner','-nostdin','-v','warning','-safe','0','-f','concat','-i',str(concat),'-i',str(wav),'-map','0:v:0','-map','1:a:0','-af',filters,'-t',str(t['duration_seconds']),'-fps_mode:v','vfr','-enc_time_base:v','1:1000','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k','-movflags','+faststart',str(out)]
log=out.with_suffix('.encode.log')
with log.open('wb') as f:subprocess.run(cmd,stderr=f,stdout=f,check=True)
probe=json.loads(subprocess.check_output([str(ff.with_name('ffprobe.exe')),'-v','error','-show_streams','-show_format','-of','json',str(out)]))
assert any(s['codec_type']=='audio' for s in probe['streams'])
result={'status':'ENCODED','video':str(out),'capture':str(c),'duration_seconds':t['duration_seconds'],'actual_capture_fps':len(frames)/t['duration_seconds'],'audio':audio,'probe':probe,'timing':'native wall timestamps, VFR; no interpolation, synthetic replacement, speed change or extra terminal freeze','visual':'USER_REVIEW','listening':'NOT_RUN'}
result['capture_metadata_note']='Recorder retains historical milestone M5_VS1 tag; actual VS3 package/map identity is established by standalone validation launch/results, not by that tag.'
if out.name=='M5_VS3_PLAYTHROUGH.mp4':result['recording_executable']=demo['launch']['executable']
out.with_suffix('.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps({k:result[k] for k in ('status','video','duration_seconds','actual_capture_fps')}))
