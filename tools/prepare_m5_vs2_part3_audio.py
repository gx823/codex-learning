"""Small original procedural game SFX; no third-party samples or background music."""
from pathlib import Path
import json, wave, hashlib
import numpy as np
ROOT=Path('D:/科研学习/codex学习/docs/HarborCity_M5_VS2/part3/audio_source')
ROOT.mkdir(parents=True,exist_ok=True)
rate=48000; rng=np.random.default_rng(250925)
def noise(n,width=1):
    x=rng.uniform(-1,1,n)
    return np.convolve(x,np.ones(width)/width,'same') if width>1 else x
def save(name,x,loop=False):
    if loop:
        n=min(2400,len(x)//4);blend=np.linspace(0,1,n);x[:n]=x[-n:]*(1-blend)+x[:n]*blend
    else:
        n=min(240,len(x)//8);x[:n]*=np.linspace(0,1,n);x[-n:]*=np.linspace(1,0,n)
    stereo=np.stack((x,x*.98),axis=1);pcm=np.int16(np.clip(stereo,-.9,.9)*32767)
    p=ROOT/(name+'.wav')
    with wave.open(str(p),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(rate);w.writeframes(pcm.tobytes())
    return {'name':name,'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'loop':loop,'duration':len(x)/rate,'rms':float(np.sqrt(np.mean(x*x)))}
rows=[];t=np.arange(rate*8)/rate
x=noise(len(t),18)*(.32+.16*np.sin(2*np.pi*t/8)) + noise(len(t),140)*.18
rows.append(save('SW_VS2_CoastalWind',x,True))
t=np.arange(int(rate*.23))/rate
rows.append(save('SW_VS2_StoneStep',(.65*noise(len(t),3)+.23*np.sin(2*np.pi*110*t))*np.exp(-t*28)))
t=np.arange(int(rate*.45))/rate
rows.append(save('SW_VS2_PistolReport',(.78*noise(len(t),2)+.21*np.sin(2*np.pi*(95*t-65*t*t)))*np.exp(-t*20)))
t=np.arange(int(rate*.3))/rate
rows.append(save('SW_VS2_ClothWhoosh',noise(len(t),12)*np.sin(np.pi*t/.3)**2*.9))
t=np.arange(rate*2)/rate
rows.append(save('SW_VS2_MotorLoop',.2*np.sin(2*np.pi*50*t)+.11*np.sin(2*np.pi*100*t)+.035*noise(len(t),8),True))
(ROOT/'SOURCE_AND_LICENSE.json').write_text(json.dumps({'source':'Original project-authored mathematical synthesis; no downloaded or extracted samples','category':'gameplay SFX and ambience, not fantasy BGM','assets':rows},ensure_ascii=False,indent=2)+'\n','utf-8')
print(json.dumps({'files':len(rows),'total_seconds':sum(r['duration'] for r in rows)}))
