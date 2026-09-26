"""Encode native R2 PPM timestamps, with complete game audio by default, without retiming.

Derived from encode_m4_r1_playthrough.py. Capture files are read-only. Validation
failure preserves every input/output and writes a separate failed evidence JSON.
No desktop, engine launch, synthetic 60fps, interpolation, montage or audio replacement.
Explicit --silent delivers zero audio streams and retains the prior audio-failure evidence.
"""
from pathlib import Path
import argparse, array, csv, datetime, hashlib, json, math, re, struct, subprocess, sys

ROOT=Path('D:/科研学习/codex学习')
REPORT=ROOT/'docs/HarborCity_M4_R2'
BUILD=Path('E:/GameDev/Builds/HarborCity/M4_R2')
MILESTONE='M4_R2'
CAPTURE_ROOTS=[ROOT/'HarborCity/Saved/M4Recording',BUILD]
FFMPEG=ROOT/'video_build/manga_mad_project_20260827/qa_tools/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg.exe'
TIME_GRID=.001

def require(ok,message):
    if not ok: raise ValueError(message)

def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()

def vector(value):
    result=[float(re.search(axis+r'=([-+0-9.eE]+)',value).group(1))for axis in ('X','Y','Z')]
    require(all(math.isfinite(x)for x in result),'Non-finite actor capture metadata')
    return result

def inspect_pcm_wav(path):
    """Reject unfinished RIFF or PCM payload; measure signal, not subjective sound quality."""
    size=path.stat().st_size
    with path.open('rb') as f:
        header=f.read(12)
        require(len(header)==12 and header[:4]==b'RIFF' and header[8:]==b'WAVE','Expected complete RIFF/WAVE')
        declared=struct.unpack('<I',header[4:8])[0]+8
        require(declared==size,'RIFF declared size does not equal actual bytes; incomplete/trailing export')
        fmt=None;data=None;chunks=[]
        while f.tell()<size:
            block=f.read(8);require(len(block)==8,'Incomplete RIFF chunk header')
            kind,length=struct.unpack('<4sI',block);start=f.tell();end=start+length
            require(end<=size,'RIFF chunk extends beyond actual file')
            chunks.append(dict(id=kind.decode('ascii',errors='replace'),bytes=length,offset=start))
            if kind==b'fmt ':
                require(fmt is None and length>=16,'Missing/duplicate/short PCM format chunk')
                fmt=struct.unpack('<HHIIHH',f.read(16))
            elif kind==b'data':
                require(data is None and length>0,'Missing/duplicate/empty PCM data')
                data=(start,length)
            f.seek(end+(length&1))
        require(f.tell()==size and fmt and data,'Incomplete RIFF structure')
        tag,channels,rate,byte_rate,align,bits=fmt
        require(tag==1 and bits==16 and channels in (1,2),'Expected native mixer signed16 PCM mono/stereo')
        require(8000<=rate<=192000 and align==channels*2 and byte_rate==rate*align,'Inconsistent PCM format')
        require(data[1]%align==0,'Truncated PCM frame')
        frames=data[1]//align;duration=frames/rate
        require(0<duration<=370,'Audio duration outside bounded recording scope')
        f.seek(data[0]);left=data[1];squares=0;count=0;peak=0;nonzero=0;clipped=0;windows=[]
        while left:
            block=f.read(min(left,rate*align));require(block,'Truncated PCM read');left-=len(block)
            values=array.array('h');values.frombytes(block)
            if sys.byteorder!='little':values.byteswap()
            total=sum(v*v for v in values);maximum=max(abs(v)for v in values)
            rms=math.sqrt(total/len(values))/32768
            windows.append(dict(start_seconds=count/channels/rate,sample_frames=len(values)//channels,
                                rms_dbfs=20*math.log10(rms)if rms else None,peak_normalized=maximum/32768))
            squares+=total;count+=len(values);peak=max(peak,maximum)
            nonzero+=sum(v!=0 for v in values);clipped+=sum(abs(v)>=32767 for v in values)
    rms=math.sqrt(squares/count)/32768
    return dict(path=str(path),bytes=size,sha256=sha(path),riff_complete=True,chunks=chunks,
                sample_rate=rate,channels=channels,bits=bits,sample_frames=frames,duration_seconds=duration,
                rms_dbfs=20*math.log10(rms)if rms else None,peak_normalized=peak/32768,
                nonzero_samples=nonzero,clipped_samples=clipped,windows=windows,
                signal_present=nonzero>0 and rms>1e-5,
                listening_status='NOT_RUN',sound_effect_quality='USER_REVIEW',
                meaning='PCM structure and measured signal only; neither proves every effect was heard or sounds appropriate')

def make_video_timeline(record):
    stamps=[float(x['seconds'])for x in record['frames']]
    require(len(stamps)>1 and all(math.isfinite(x)and x>=0 for x in stamps),'Invalid captured times')
    require(all(b>a for a,b in zip(stamps,stamps[1:])),'Non-monotonic captured times')
    relative=[round((x-stamps[0])*1_000_000)for x in stamps]
    require(all(b>a for a,b in zip(relative,relative[1:])),'Sub-microsecond frame collision')
    wall=float(record['wall_seconds'])
    require(math.isfinite(wall)and stamps[-1]<=wall+.001,'Capture timestamp after reported stop')
    # No following measured frame exists. Encode the final captured image for
    # one timestamp tick only; do not stretch it to the later write/drain time.
    duration=relative[-1]/1_000_000+TIME_GRID
    tail=wall-stamps[-1]
    require(0<=tail<=1.0,'More than 1 second of unobserved terminal capture; retain as incomplete')
    return dict(timestamps=stamps,relative_microseconds=relative,duration_seconds=duration,
        last_frame_to_reported_stop_seconds=tail,final_sample_tail_seconds=TIME_GRID,
        tail_scope='Only the final1ms encoding packet is appended; later drain/unobserved time is not fabricated as footage')

def make_timeline(record,audio):
    timeline=make_video_timeline(record)
    stamps=timeline['timestamps'];duration=timeline['duration_seconds']
    offset=float(record['audio_start_offset_seconds']);wall=float(record['wall_seconds'])
    require(math.isfinite(offset)and abs(offset)<=1,'Missing/implausible audio-start offset')
    audio_end=offset+audio['duration_seconds']
    require(audio_end>=stamps[-1]-.10,'Audio ends before the last captured image; incomplete submix export')
    require(audio_end<=wall+.20,'Audio duration exceeds capture wall span; clock/source mismatch')
    relative_audio_start=offset-stamps[0]
    trim=max(0,round(-relative_audio_start*audio['sample_rate']))
    delay=max(0,round(relative_audio_start*audio['sample_rate']))
    require(delay/audio['sample_rate']<=.10,'More than100ms without recorded audio at first captured image')
    duration_samples=round(duration*audio['sample_rate'])
    end_sample=min(audio['sample_frames'],trim+max(0,duration_samples-delay))
    require(end_sample>trim,'No game audio overlaps the video')
    timeline.update(
        audio_trim_start_sample=trim,audio_trim_end_sample=end_sample,audio_delay_samples=delay,
        audio_relative_start_seconds=relative_audio_start,audio_source_end_capture_seconds=audio_end,
        audio_expected_output_seconds=(end_sample-trim+delay)/audio['sample_rate'],
        synchronization_scope='Recorded game-thread audio-start offset minus first captured-frame timestamp; native mixer/presentation latency remains unmeasured')
    return timeline

def validate_ppm(path,width,height):
    with path.open('rb')as f:
        require(f.readline()==b'P6\n','Not native binary PPM')
        require(f.readline().strip()==f'{width} {height}'.encode()and f.readline()==b'255\n','PPM dimensions/header differ')
        require(path.stat().st_size==f.tell()+width*height*3,'Incomplete PPM pixels')

def main(argv=None, *, scope_validator=None, configure_parser=None,
         formal_filename='M4_R2_PLAYTHROUGH.mp4'):
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--capture',required=True,help='Actual capture.json; read only')
    ap.add_argument('--output',default=str(REPORT/formal_filename))
    ap.add_argument('--evidence-directory',help='Fresh directory inside the milestone report tree')
    ap.add_argument('--spawn-location',nargs=3,type=float,metavar=('X','Y','Z'))
    ap.add_argument('--run-results',help='Actual completed milestone results.json; validated by the entrypoint')
    ap.add_argument('--probe',action='store_true',help='Technical editor/short probe, never the formal filename')
    ap.add_argument('--silent',action='store_true',help='Explicit video-only delivery with zero audio streams; requires prior failed audio evidence, never implicit fallback')
    ap.add_argument('--audio-failure-evidence',help='Prior failed verification.json for this exact capture, required by --silent; retained and hashed')
    ap.add_argument('--validate-only',action='store_true',help='No FFmpeg/ffprobe processes or encoding')
    if configure_parser:configure_parser(ap)
    args=ap.parse_args(argv);capture=Path(args.capture).resolve();output=Path(args.output).resolve()
    evidence=Path(args.evidence_directory).resolve()if args.evidence_directory else REPORT/'video_encoding'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    require(evidence.is_relative_to(REPORT.resolve()),'Evidence must be inside milestone reports')
    require(not evidence.exists(),'Use a fresh evidence directory; do not overwrite prior verification')
    evidence.mkdir(parents=True)
    report=dict(milestone=MILESTONE,status='RUNNING',capture=str(capture),output=str(output),
                evidence=str(evidence),encoding='NOT_RUN',visual_review='NOT_RUN',listening='NOT_RUN',
                effects_auditory_acceptance='NOT_RUN' if args.silent else 'USER_REVIEW',probe=args.probe,commands=[],
                silent_requested=args.silent,audio_encoding='NOT_RUN')
    def save():
        (evidence/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    def run(command,name,limit=600):
        report['commands'].append(dict(name=name,argv=command,timeout_seconds=limit));save()
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=limit)
        (evidence/(name+'.stdout')).write_bytes(result.stdout);(evidence/(name+'.stderr')).write_bytes(result.stderr)
        require(result.returncode==0,f'{name} failed ({result.returncode}); evidence retained')
        return result.stdout
    try:
        allowed=CAPTURE_ROOTS
        require(capture.name=='capture.json'and any(capture.is_relative_to(x.resolve())for x in allowed),'Capture outside authorized milestone scope')
        require(output.is_relative_to(REPORT.resolve())and output.suffix=='.mp4'and not output.exists(),'New milestone MP4 output required')
        data=json.loads(capture.read_text(encoding='utf-8-sig'));report['capture_sha256']=sha(capture)
        require(data['milestone']==MILESTONE and data['status']=='CAPTURED_PENDING_REVIEW'and data['write_failures']==0,'Capture incomplete or wrong milestone')
        require(data['configured_delay_seconds']==0 and data['capture_geometry_valid']is True,'Capture delayed or geometry invalid')
        require(data['framegrabber_latency']==0 and data['audio_file']=='game_audio.wav','Expected verified immediate native frames and game submix')
        if not scope_validator:require(data['stop_reason']=='r2_gameplay_sequence_finished','Capture did not finish with its actual gameplay sequence')
        w,h=data['width'],data['height'];require(type(w)is int and type(h)is int and 0<w<=4096 and 0<h<=2160 and w*9==h*16 and w%2==h%2==0,'Invalid native even16:9 size')
        require(data['requested_size']==data['output_size']==dict(width=w,height=h),'Captured dimensions differ')
        frames=data['frames'];require(1<len(frames)<=12000,'Unbounded/empty capture frame list')
        location=vector(data['initial_player_location']);velocity=vector(data['initial_player_velocity'])
        report['from_spawn']=dict(location=location,velocity=velocity,first_world_seconds=data['first_capture_world_seconds'],status='NOT_ASSESSED')
        scope_unchanged=None
        if scope_validator:scope_unchanged=scope_validator(args,data,capture,output,report)
        elif args.probe:require(output.name!='M4_R2_PLAYTHROUGH.mp4','Probe cannot use formal playthrough filename')
        else:
            require(capture.is_relative_to(BUILD.resolve())and output.name=='M4_R2_PLAYTHROUGH.mp4','Formal footage requires R2 package capture')
            require(args.spawn_location and all(math.isfinite(x)for x in args.spawn_location),'Actual authored spawn coordinates required')
            require(math.hypot(location[0]-args.spawn_location[0],location[1]-args.spawn_location[1])<=15 and abs(location[2]-args.spawn_location[2])<=30 and math.hypot(*velocity[:2])<=1,'Capture did not begin stationary at original spawn')
            report['from_spawn']['status']='PASS_POSITION_AND_CONFIGURATION_ONLY'
            require(args.run_results,'Completed natural-run results required')
            results_path=Path(args.run_results).resolve();results=json.loads(results_path.read_text(encoding='utf-8-sig'))
            require(results['mode']=='r2_playthrough'and results['test_suite']=='M4_R2'and results['status']=='COMPLETE','Wrong/incomplete run results')
            require(results.get('failures',1)==0,'Natural-run checks contain failures; retain this as a diagnostic recording')
            natural=results['m3_natural_playthrough'];require(natural['natural_route_completed']and not natural['aborted'],'Natural errands did not complete')
            require(not results.get('m3_escape_stop',{}).get('user_stop_latched',False),'Stopped run cannot be a completed recording')
            report['run_results']=dict(path=str(results_path),sha256=sha(results_path),natural_route_completed=True)
        audio=None;audio_path=capture.parent/'game_audio.wav';audio_source_sha=None
        if args.silent:
            require(args.audio_failure_evidence,'Explicit silent delivery requires the prior audio failure evidence')
            failure_path=Path(args.audio_failure_evidence).resolve()
            require(failure_path.is_relative_to(REPORT.resolve()) and failure_path.name=='verification.json','Prior audio failure must be retained R2 verification.json')
            failure=json.loads(failure_path.read_text(encoding='utf-8-sig'))
            require(failure.get('status')=='FAIL' and Path(failure['capture']).resolve()==capture
                    and failure.get('capture_sha256')==report['capture_sha256'],'Prior failure does not match this unchanged capture')
            require('Audio' in failure.get('error','') or 'audio' in failure.get('error',''),'Prior failure is not an audio validation failure')
            report['silent_delivery']=dict(intent='Explicit video-only delivery; no substituted, padded, retimed or partial audio',
                reason=failure['error'],prior_failure=dict(path=str(failure_path),sha256=sha(failure_path)),
                audio_encoding='NOT_RUN',audio_streams_expected=0,effects_auditory_acceptance='NOT_RUN')
            # Diagnose and preserve the incomplete WAV without making it an input
            # or silently weakening the default full-audio validation path.
            if audio_path.is_file():
                audio_source_sha=sha(audio_path)
                try:report['source_audio']=inspect_pcm_wav(audio_path)
                except Exception as audio_error:
                    report['source_audio']=dict(path=str(audio_path),sha256=audio_source_sha,
                        bytes=audio_path.stat().st_size,diagnostic_error=repr(audio_error),status='DIAGNOSTIC_ONLY_INVALID_AUDIO')
            else:report['source_audio']=dict(path=str(audio_path),status='MISSING_DIAGNOSTIC_ONLY')
            timeline=make_video_timeline(data)
            timeline['synchronization_scope']='NOT_RUN: explicit silent video has no audio timeline, stream or claimed sound synchronization'
        else:
            require(not args.audio_failure_evidence,'--audio-failure-evidence is only valid with explicit --silent')
            audio=inspect_pcm_wav(audio_path);report['source_audio']=audio;audio_source_sha=audio['sha256']
            require(audio['signal_present'],'Game audio is silent/inaudibly low; cannot deliver effects evidence')
            timeline=make_timeline(data,audio)
        report['timeline']={k:v for k,v in timeline.items()if k not in ('timestamps','relative_microseconds')}
        stamps=timeline['timestamps'];relative=timeline['relative_microseconds'];paths=[];states=[];sequence=['ffconcat version 1.0']
        for i,row in enumerate(frames):
            require(re.fullmatch(r'frame_\d{6}\.ppm',row['file'])is not None,'Unexpected frame filename')
            path=(capture.parent/row['file']).resolve();require(path.parent==capture.parent,'Frame escapes capture directory')
            validate_ppm(path,w,h);paths.append(path);states.append((path.stat().st_size,path.stat().st_mtime_ns))
            duration_us=relative[i+1]-relative[i]if i+1<len(frames)else 1000
            sequence.extend(["file '"+path.as_posix().replace("'","'\\''")+"'",'option framerate 1000',f'duration {duration_us/1_000_000:.6f}'])
        require(len(set(paths))==len(paths),'Repeated frame path')
        report['frames']=dict(count=len(frames),native_width=w,native_height=h,first_sha256=sha(paths[0]),last_sha256=sha(paths[-1]),
            actual_mean_capture_fps=(len(frames)-1)/(stamps[-1]-stamps[0]),requested_capture_fps=data['requested_fps'],dropped=data['dropped'],
            maximum_observed_gap_seconds=max(b-a for a,b in zip(stamps,stamps[1:])),grid_note='1ms timestamp grid is not capture FPS; no frame creation or60fps conversion')
        concat=evidence/'realtime.ffconcat';concat.write_text('\n'.join(sequence)+'\n',encoding='utf-8')
        cmd=[str(FFMPEG),'-hide_banner','-loglevel','warning','-nostdin','-n','-f','concat','-safe','0','-i',str(concat)]
        if not args.silent:cmd+=['-i',audio['path']]
        cmd+=['-map','0:v:0']
        if args.silent:cmd+=['-an']
        else:cmd+=['-map','1:a:0']
        cmd+=['-fps_mode:v','vfr','-enc_time_base:v','1:1000','-c:v','libx264','-preset','ultrafast','-crf','22','-threads','2','-pix_fmt','yuv420p']
        if not args.silent:
            audio_filter=f"atrim=start_sample={timeline['audio_trim_start_sample']}:end_sample={timeline['audio_trim_end_sample']},asetpts=PTS-STARTPTS"
            if timeline['audio_delay_samples']:audio_filter+=f",adelay={timeline['audio_delay_samples']}S:all=1"
            cmd+=['-af',audio_filter,'-c:a','aac','-b:a','192k','-ar',str(audio['sample_rate'])]
        cmd+=['-t',f"{timeline['duration_seconds']:.6f}",'-movflags','+faststart',str(output)]
        report['planned_encode_command']=cmd
        if args.validate_only:
            if scope_unchanged:scope_unchanged()
            report['status']='VIDEO_ONLY_INPUT_VALIDATED_ENCODING_NOT_RUN' if args.silent else 'INPUT_VALIDATED_ENCODING_NOT_RUN';save();print(str(evidence/'verification.json'));return
        output.parent.mkdir(parents=True,exist_ok=True);run(cmd,'encode',1200);report['encoding']='COMPLETED_PENDING_QA'
        probe=str(FFMPEG.with_name('ffprobe.exe'))
        metadata=json.loads(run([probe,'-v','error','-show_streams','-show_format','-of','json',str(output)],'ffprobe',120));report['ffprobe']=metadata
        video=[x for x in metadata['streams']if x['codec_type']=='video'];sounds=[x for x in metadata['streams']if x['codec_type']=='audio']
        require(len(video)==1 and video[0]['width']==w and video[0]['height']==h and video[0]['codec_name']=='h264','Encoded video dimensions/codec changed')
        if args.silent:require(len(sounds)==0,'Explicit silent delivery must contain zero audio streams')
        else:
            require(len(sounds)==1,'Default delivery must contain exactly one actual audio stream')
            require(sounds[0]['codec_name']=='aac'and int(sounds[0]['channels'])==audio['channels']and int(sounds[0]['sample_rate'])==audio['sample_rate'],'Missing/wrong actual AAC track')
        timing=json.loads(run([probe,'-v','error','-select_streams','v:0','-count_frames','-show_frames','-show_entries','frame=pts_time,best_effort_timestamp_time,duration_time:stream=nb_read_frames','-of','json',str(output)],'frame_probe',600))
        decoded=timing.get('frames',[]);require(len(decoded)==len(frames)and int(timing['streams'][0]['nb_read_frames'])==len(frames),'Encoded frame count differs; duplicated/dropped frames')
        pts=[float(x['pts_time'])for x in decoded];require(all(math.isfinite(x)for x in pts)and all(b>a for a,b in zip(pts,pts[1:])),'Invalid/nonmonotonic encoded PTS')
        errors=[(t-pts[0])-(stamps[i]-stamps[0])for i,t in enumerate(pts)]
        with(evidence/'frame_timing.csv').open('w',newline='',encoding='utf-8')as f:
            writer=csv.writer(f);writer.writerow(['index','captured_file','capture_seconds','encoded_pts','error_ms'])
            writer.writerows((i,frames[i]['file'],stamps[i],pts[i],errors[i]*1000)for i in range(len(frames)))
        require(max(map(abs,errors))<=.0015,'Encoded PTS changed real capture speed')
        duration=float(metadata['format']['duration']);require(abs(duration-timeline['duration_seconds'])<=.10,'Unexpected terminal extension/truncation')
        if not args.silent:
            decoded_wav=evidence/'decoded_game_audio.wav'
            run([str(FFMPEG),'-hide_banner','-loglevel','error','-nostdin','-n','-i',str(output),'-map','0:a:0','-c:a','pcm_s16le',str(decoded_wav)],'decoded_audio',300)
            rendered_audio=inspect_pcm_wav(decoded_wav);report['decoded_audio']=rendered_audio
            require(rendered_audio['signal_present'],'Encoded track is silent')
            require(abs(rendered_audio['duration_seconds']-timeline['audio_expected_output_seconds'])<=.10,'Encoded audio duration differs from aligned source')
            require(abs(float(sounds[0].get('start_time','0')))<=.025,'Audio track starts unexpectedly late')
            report['audio_encoding']='PASS_TECHNICAL_ONLY'
        require(sha(capture)==report['capture_sha256'],'Capture JSON changed during encoding')
        if audio_source_sha:require(sha(audio_path)==audio_source_sha,'Original diagnostic/source WAV changed during encoding')
        require(all((p.stat().st_size,p.stat().st_mtime_ns)==state for p,state in zip(paths,states)),'PPM changed during encoding')
        indices=sorted(set(round(i*(len(frames)-1)/5)for i in range(6)))
        select='+'.join(f'eq(n\\,{i})'for i in indices)
        run([str(FFMPEG),'-hide_banner','-loglevel','error','-nostdin','-n','-i',str(output),'-vf',f'select={select},scale=480:-1,tile=3x2','-frames:v','1',str(evidence/'contact_sheet.png')],'contact_sheet',300)
        if scope_unchanged:scope_unchanged()
        report.update(status='ENCODED_SILENT_PENDING_VISUAL_REVIEW' if args.silent else 'ENCODED_PENDING_VISUAL_AND_AUDITORY_REVIEW',encoding='PASS_TECHNICAL_ONLY',output_sha256=sha(output),output_bytes=output.stat().st_size,
            frame_pts_max_error_seconds=max(map(abs,errors)),encoded_duration_seconds=duration,encoded_final_frame_duration_seconds=decoded[-1].get('duration_time'),
            frame_count_matches=True,normal_speed_pts_matches=True,audio_track_signal_present=False if args.silent else True,audio_stream_count=len(sounds),contact_sheet=str(evidence/'contact_sheet.png'))
        save();print(str(evidence/'verification.json'))
    except Exception as e:
        report.update(status='FAIL',error=repr(e),retention='All source frames, capture JSON, audio and any partial output retained; nothing deleted')
        save();print(str(evidence/'verification.json'),file=sys.stderr);raise

if __name__=='__main__':main()
