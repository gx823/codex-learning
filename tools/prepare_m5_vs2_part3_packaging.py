"""Create a VS2-scoped copy of the validated fresh-Cook/encryption packager."""
from pathlib import Path
import json,re
W=Path('D:/科研学习/codex学习');T=W/'tools';D=W/'docs/HarborCity_M5_VS2/part3';P=W/'HarborCity'
s=json.loads((D/'town_selection.json').read_text('utf-8-sig'));world=s['map'];root=s['root']
assert re.fullmatch(r'/Game/HarborCity/M5VS2/Part3/Town_[a-f0-9]+/L_HarborTown',world)
source=(T/'package_m5_vs1.ps1').read_text('utf-8-sig')
for a,b in [('m5_vs1_release_guards.ps1','m5_vs2_release_guards.ps1'),('build_m5_vs1.ps1','build_m5_vs2.ps1'),('package_m5_vs1.ps1','package_m5_vs2.ps1'),('HarborCity_M5_VS1','HarborCity_M5_VS2'),("milestone='M5_VS1'","milestone='M5_VS2'"),('Builds\\HarborCity\\M5_VS1','Builds\\HarborCity\\M5_VS2'),('E:\\GameDev\\Cache\\Unreal\\HarborCity','D:\\GameDev\\Cache\\Unreal\\HarborCity'),('/Game/HarborCity/Maps/L_M5_CyberHarbor',world),('.L_M5_CyberHarbor','.L_HarborTown'),('HarborCity\\Content\\HarborCity\\Maps\\L_M5_CyberHarbor.umap','HarborCity\\Content\\'+world[6:].replace('/','\\')+'.umap')]:source=source.replace(a,b)
source=source.replace('[ValidateRange(1,2)][int]$MaxParallelActions = 2','[ValidateRange(1,8)][int]$MaxParallelActions = 8')
source=source.replace('docs/HarborCity_M5_VS2/ASSET_LICENSE_NOTICES.md','docs/HarborCity_M5_VS2/part3/ASSET_LICENSE_NOTICES.md')
(T/'package_m5_vs2.ps1').write_text(source,'utf-8-sig')
# Reuse unchanged crypto/private-file/container helpers. Override only the exact
# VS2 asset set: map references carry environment and adult NPC dependencies.
guard=". (Join-Path $PSScriptRoot 'm5_vs1_release_guards.ps1')\n"
old=(T/'m5_vs1_release_guards.ps1').read_text('utf-8-sig')
getdirs=re.search(r'function Get-HCM5VS1CookDirectories \{(.*?)\n\}',old,re.S)[1]
dirs=re.findall(r"'(/Game/[^']+)'",getdirs)
dirs.append(root)
req=re.findall(r"'(/Game/[^']+)'",re.search(r'function Get-HCM5VS1RequiredPackages \{(.*?)\n\}',old,re.S)[1])
req.extend('/Game/'+p.relative_to(P/'Content').as_posix()[:-7] for p in (P/'Content'/root[6:]).rglob('*.uasset'))
guard+='function Get-HCM5VS1CookDirectories {\n @('+','.join("'"+x+"'" for x in dirs)+')\n}\n'
guard+='function Get-HCM5VS1RequiredPackages {\n @('+','.join("'"+x+"'" for x in sorted(set(req)))+')\n}\n'
(T/'m5_vs2_release_guards.ps1').write_text(guard,'utf-8-sig')
# Snapshot before this one scoped update. Never emit private config values.
for name in ['DefaultEngine.ini','DefaultGame.ini']:
 p=P/'Config'/name;backup=D/'before'/name
 if not backup.exists():backup.write_bytes(p.read_bytes())
 text=p.read_text('utf-8-sig')
 if name=='DefaultEngine.ini':
  text=re.sub(r'(?m)^GameDefaultMap=.*$',lambda m:'GameDefaultMap='+world+'.L_HarborTown',text)
  for section in ['UdpMessaging','TcpMessaging']:
   header='[/Script/'+section+'.'+section+'Settings]'
   if header not in text:text+='\n'+header+'\nEnableTransport=False\n'
   else:
    pattern=r'('+re.escape(header)+r'[^\[]*)'
    def disable(m):
     block=m[1]
     return re.sub(r'(?m)^EnableTransport=.*$','EnableTransport=False',block) if re.search(r'(?m)^EnableTransport=',block) else block+'EnableTransport=False\n'
    text=re.sub(pattern,disable,text)
 else:
  text=re.sub(r'(?m)^\+MapsToCook=.*$',lambda m:'+MapsToCook=(FilePath="'+world+'")',text)
  text=re.sub(r'(?m)^\+DirectoriesToAlwaysCook=.*\n?','',text)
  section='[/Script/UnrealEd.ProjectPackagingSettings]'
  assert section in text
  text=text.replace(section,section+'\n'+'\n'.join('+DirectoriesToAlwaysCook=(Path="'+x+'")' for x in dirs),1)
 p.write_text(text,'utf-8')
(D/'packaging_selection.json').write_text(json.dumps({'map':world,'cook_directories':dirs,'required_packages':req,'encryption':'unchanged validated full-asset configuration','network':'UDP/TCP messaging transports disabled in project settings; runtime socket verification pending'},indent=2),'utf-8')
print('Prepared VS2-only packager and exact default-map/cook selection; private values suppressed.')
