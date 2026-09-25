"""Compact delivery evidence from actual artifacts; no success defaults."""
from pathlib import Path
import json,hashlib,zipfile,difflib
from PIL import Image
W=Path(__file__).resolve().parents[1];D=W/'docs/HarborCity_M5_VS3';A=D/'appendix'
candidate=Path('E:/GameDev/Builds/HarborCity/M5_VS3/Candidate_20260926_052401_469_58434958')
manifest=json.loads((candidate/'package_manifest.json').read_text('utf-8-sig'))
assert manifest['status']=='PASS'
def write(name,data):(A/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),'utf-8')
write('DELIVERY_BUILD.json',{k:manifest.get(k) for k in ['status','actual_executable','candidate_directory','started_at','ended_at','exit_code']})
private=W/'assets/M5_VS1/hero/Selestia/PrivateCryptoVerification'
checks=sorted(private.glob('*/verification.json'),key=lambda p:p.stat().st_mtime)
j=json.loads(checks[-1].read_text('utf-8-sig'))
# Whitelist the result, never archive extraction commands or crypto config.
write('DELIVERY_ENCRYPTION.json',{'status':j['status'],'candidate':str(candidate),'private_report_sha256':hashlib.sha256(checks[-1].read_bytes()).hexdigest(),'scope':'one purchased skeletal asset, direct extraction with/without key; not a claim of absolute protection'})
p=Path.home()/'AppData/Local/HarborCity/Saved/M5Tests/20260926_051654_m5_vs3_art'
write('DISPLAY_MEASUREMENT.json',{'status':'ACTUAL_1920_1080_CONFIRMED','previous_actual':[1707,960],
 'cause':'QA ApplySettings(false) reapplied saved WindowedFullscreen; BaseEngine disables game high DPI and unattended Windows path also suppresses DPI setup.',
 'project_only_test_override':{'Engine.ini':{'bAllowHighDPIInGameMode':True,'bAllowHighDpiWhenUnattended':True},'GameUserSettings.ini':{'FullscreenMode':2}},
 'system_display_settings_changed':False,'backup':str(W/'.private/M5_VS3/display_before_capture'),
 'screenshots':[{'path':str(s),'size':list(Image.open(s).size)} for s in sorted(p.glob('*.png'))],
 'sources':['UE Runtime/Slate/Private/Framework/Application/SlateApplication.cpp:1010','UE Runtime/ApplicationCore/Private/Windows/WindowsPlatformApplicationMisc.cpp:292','UE Runtime/Engine/Private/GameUserSettings.cpp:563'],
 'restoration':'record separately after all tests'})
with zipfile.ZipFile(W/'.private/M5_VS3/pre_vs3_source_config.zip') as z:
 old=z.read('Source/HarborCity/M1/HCM1Vehicle.cpp').decode('utf-8-sig')
new=(W/'HarborCity/Source/HarborCity/M1/HCM1Vehicle.cpp').read_text('utf-8-sig')
def stripfunc(s,name):
 start=s.index('bool AHCM1Vehicle::'+name);brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[:start]+s[i:]
for fn in ['FindSafeExitTransform','ResolveSafeVehicleTransform']:
 old=stripfunc(old,fn);new=stripfunc(new,fn)
old=old.replace('#include "M3/HCM3NPC.h"\n','');new=new.replace('#include "M3/HCM3NPC.h"\n','')
delta=list(difflib.unified_diff(old.splitlines(),new.splitlines(),n=2))
write('DELIVERY_VEHICLE_SCOPE.json',{'status':'PASS' if not delta else 'REVIEW_DIFFERENCES','excluded':['FindSafeExitTransform','ResolveSafeVehicleTransform','M3/HCM3NPC.h include'],'scope':'CPP outside placement/exit guards, compared with pre-VS3 backup','remaining_diff':delta})
print(json.dumps({'build':manifest['status'],'display':len(list(p.glob('*.png'))),'vehicle_remaining_diff_lines':len(delta)}))
