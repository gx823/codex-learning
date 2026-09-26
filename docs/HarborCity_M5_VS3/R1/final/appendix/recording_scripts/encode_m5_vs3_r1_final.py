"""Preserve measured native JPEG/PNG frame timestamps and actual master-submix audio."""
from pathlib import Path
import argparse,json,subprocess,shutil
from PIL import Image
from encode_m4_r2_playthrough import inspect_pcm_wav,make_timeline
W=Path('D:/科研学习/codex学习');D=W/'docs/HarborCity_M5_VS3'
ap=argparse.ArgumentParser();ap.add_argument('capture');ap.add_argument('output');ap.add_argument('--trim-leading',type=int,default=0);ap.add_argument('--verify-existing',action='store_true');a=ap.parse_args()
c=Path(a.capture).resolve();out=Path(a.output).resolve()
assert c.is_relative_to(D.resolve()) and out.is_relative_to(D.resolve()) and (not out.exists() or a.verify_existing)
r=json.loads(c.read_text('utf-8-sig'))
source_frames=list(r.get('frames',[]))
assert r['status']=='CAPTURED_PENDING_REVIEW' and r['write_failures']==0
assert r['capture_geometry_valid'] is True and (r['width'],r['height'])==(1920,1080)
assert r['requested_size']==r['output_size']==dict(width=1920,height=1080)
assert r['stop_reason'] in ('m5_gameplay_sequence_finished','duration_reached','vs2_gameplay_sequence_finished','test_complete','r2_gameplay_sequence_finished')
if out.name=='M5_VS3_PLAYTHROUGH.mp4':
 validation=json.loads((D/'standalone_validation.json').read_text('utf-8-sig'))
 assert validation['status']=='COMPLETE'
 demo=next(x for x in validation['runs'] if x['mode']=='m5_vs3_demo' and x.get('capture') and Path(x['capture']).resolve()==c)
 assert Path(demo['capture']).resolve()==c and demo['launch']['kind']=='STANDALONE_PACKAGE'
 assert '-M5PublicCapture' in demo['launch']['arguments']
 assert demo['result_status']=='COMPLETE'
wav=c.parent/r['audio_file'];audio=inspect_pcm_wav(wav)
# Silence is a test failure, not a reason to retain gigabytes of frames forever.
# Preserve the actual recorded track, never substitute sound or claim audible SFX.
leading_trim={'count':a.trim_leading,'reason':'explicitly inspected initial frames only; keyframes and timing metadata retained'}
assert 0 <= a.trim_leading <= 2
if a.trim_leading:r['frames']=r['frames'][a.trim_leading:]
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
cmd=[str(ff),'-hide_banner','-nostdin','-v','warning','-safe','0','-f','concat','-i',str(concat),'-i',str(wav),'-map','0:v:0','-map','1:a:0','-af',filters,'-t',str(t['duration_seconds']),'-fps_mode:v','vfr','-enc_time_base:v','1:1000','-c:v','libx264','-threads','2','-preset','fast','-crf','21','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k','-movflags','+faststart',str(out)]
log=out.with_suffix('.encode.log')
if not a.verify_existing:
 with log.open('wb') as f:subprocess.run(cmd,stderr=f,stdout=f,check=True)
probe=json.loads(subprocess.check_output([str(ff.with_name('ffprobe.exe')),'-v','error','-show_streams','-show_format','-of','json',str(out)]))
assert any(s['codec_type']=='audio' for s in probe['streams'])
assert 10<=t['duration_seconds']<=360
result={'status':'ENCODED','audio_signal_status':'PASS' if audio['signal_present'] else 'FAIL_SILENT_NATIVE_CAPTURE','video':str(out),'capture':str(c),'duration_seconds':t['duration_seconds'],'actual_capture_fps':len(frames)/t['duration_seconds'],'audio':audio,'probe':probe,'timing':'native wall timestamps, VFR; no interpolation, synthetic replacement, speed change or extra terminal freeze','visual':'USER_REVIEW','listening':'NOT_RUN'}
result['leading_trim']=leading_trim
result['capture_metadata_note']='Native viewport frame timestamps and submix sound. Consult attached launch/results for executable binding. Setup is A fixture; gameplay is B input; this video is not OS mouse proof.'
if out.name=='M5_VS3_PLAYTHROUGH.mp4':result['recording_executable']=demo['launch']['executable']
out.with_name(out.stem+'.qa.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
# Decode every compressed packet before deleting source frames. A readable header
# alone does not prove that the produced video/audio can actually be played.
decode=subprocess.run([str(ff),'-nostdin','-v','error','-xerror','-threads','2','-i',str(out),'-map','0:v:0','-map','0:a:0','-fps_mode','passthrough','-enc_time_base:v','1:1000','-f','null','-'],capture_output=True)
if decode.returncode or decode.stderr:raise RuntimeError('MP4 decode verification failed; temporary frames retained')
paths=[(c.parent/f['file']).resolve() for f in source_frames]
# Resolve and validate every file before any deletion. PPM cleanup requires a
# separate explicit user confirmation and is intentionally never performed here.
assert c.parent.parent.resolve()==(D/'recordings').resolve()
assert all(p.parent==c.parent and p.suffix.lower() in ('.jpg','.png') and p.is_file() for p in paths)
keydir=c.parent/'keyframes';keydir.mkdir(exist_ok=True)
for i in sorted({round(k*(len(paths)-1)/7) for k in range(8)}):shutil.copy2(paths[i],keydir/paths[i].name)
total=sum(p.stat().st_size for p in paths)
for p in paths:p.unlink()
concat.unlink()
assert wav.resolve().parent==c.parent and wav.suffix.lower()=='.wav'
wav.unlink() # recorded submix intermediate, never a source music asset
result.update(decode_validation='PASS_FULL_VIDEO_AND_AUDIO_DECODE',temporary_frames_removed=len(paths),temporary_bytes_removed=total,retained_keyframes=8,output_bytes=out.stat().st_size)
out.with_name(out.stem+'.qa.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
(c.parent/'frame_cleanup.json').write_text(json.dumps(dict(status='TEMPORARY_JPEG_PNG_REMOVED_AFTER_VERIFIED_MP4',video=str(out),frames=len(paths),bytes=total,ppm_deleted=0,audio_intermediate_removed=True,retained='capture metadata, 8 keyframes, MP4 and QA'),ensure_ascii=False,indent=2),'utf-8')
print(json.dumps({k:result[k] for k in ('status','video','duration_seconds','actual_capture_fps')}))
