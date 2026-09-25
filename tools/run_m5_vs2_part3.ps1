#requires -Version 7.0
[CmdletBinding()]
param(
 [ValidateSet('smoke','soak','beauty','motion','demo','perf','reverse','reverse_legacy','m5_review','m5_full')][string]$Mode='smoke',
 [string]$Executable='',
 [ValidateSet('High','Epic')][string]$Quality='High',
 [int]$RecordSeconds=0
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$doc=Join-Path $root 'docs/HarborCity_M5_VS2/part3'
if(Test-Path -LiteralPath (Join-Path $doc 'STOP_DESKTOP_CHAIN')){throw 'Desktop test chain paused; inspect focus/stop condition before a new launch.'}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){throw 'Existing engine/game must finish normally first.'}
$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
$run=Join-Path $doc ($Mode+'_'+$stamp)
New-Item -ItemType Directory -Path $run | Out-Null
$project=Join-Path $root 'HarborCity/HarborCity.uproject'
$editor= -not $Executable
if($editor){$Executable='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'}
elseif(-not ([IO.Path]::GetFullPath($Executable).StartsWith('E:\GameDev\Builds\HarborCity\M5_VS2\Candidate_',[StringComparison]::OrdinalIgnoreCase))){throw 'Only VS2 candidate executables are allowed.'}
$argsList=[Collections.Generic.List[string]]::new()
if($editor){$argsList.Add('"'+$project+'"')}
if($Mode.StartsWith('reverse')){
 $map='/Game/HarborCity/Maps/L_M1_Playground';$test=if($Mode -eq 'reverse_legacy'){'reverse_diagnostic_legacy'}else{'reverse_diagnostic'}
 $slot='HarborCity_M1_R2_Test_M4_VS2_'+$Mode+'_'+$stamp
 $argsList.Add('-M1Test='+$test);$argsList.Add('-M5VS2ReverseTrace')
}else{
 $selection=Get-Content (Join-Path $doc 'town_selection.json') -Raw | ConvertFrom-Json
 $map=$selection.map
 if($map -notmatch '^/Game/HarborCity/M5VS2/Part3/Town_[a-f0-9]+/L_HarborTown$'){throw 'Invalid saved town selection.'}
 $slot='HarborCity_M5_VS1_Test_VS2_'+$Mode+'_'+$stamp
 $test=if($Mode -in @('m5_review','m5_full')){$Mode}else{'m5_vs2_'+$Mode};$argsList.Add('-M5Test='+$test)
}
$argsList.Add($map)
if($editor){$argsList.Add('-game')}
foreach($a in @('-unattended','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-nosplash','-M1AutoQuit',('-HCM1SaveSlot='+$slot),('-abslog="'+(Join-Path $run 'game.log')+'"'),'-LocalDataCachePath=D:/GameDev/Cache/Unreal/HarborCity/DDC','-ZenDataPath=D:/GameDev/Cache/Unreal/HarborCity/Zen')){$argsList.Add($a)}
$q=if($Quality -eq 'Epic'){3}else{2}
$exec='sg.ViewDistanceQuality '+$q+',sg.AntiAliasingQuality '+$q+',sg.ShadowQuality '+$q+',sg.GlobalIlluminationQuality '+$q+',sg.ReflectionQuality '+$q+',sg.PostProcessQuality '+$q+',sg.TextureQuality '+$q+',sg.EffectsQuality '+$q+',sg.FoliageQuality '+$q+',sg.ShadingQuality '+$q+',sg.LandscapeQuality '+$q+',r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0'
if($Mode -eq 'perf'){$exec+=',r.ScreenPercentage,r.VSync,t.MaxFPS,sg.FoliageQuality,sg.ShadingQuality,sg.LandscapeQuality'}
$argsList.Add('-ExecCmds="'+$exec+'"')
if($RecordSeconds -gt 0){$argsList.Add('-HCM4RecordSeconds='+$RecordSeconds);$argsList.Add('-HCM3RecordDelay=6')}
$started=Get-Date
$proc=Start-Process -FilePath $Executable -ArgumentList $argsList.ToArray() -WorkingDirectory (Split-Path $Executable -Parent) -PassThru
$rec=[ordered]@{status='RUNNING';pid=$proc.Id;executable=$Executable;kind=if($editor){'EDITOR_GAME'}else{'STANDALONE_PACKAGE'};input_layer='ENGINE_ACTION_NOT_OS';arguments=$argsList.ToArray();started=$started.ToString('o');map=$map}
$rec | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'launch.json') -Encoding utf8
$limit=if($Mode -eq 'soak'){2400}else{600}
if($Mode -eq 'perf'){
 $samples=[Collections.Generic.List[string]]::new()
 $samples.Add('timestamp, gpu_name, memory_used_MiB, gpu_util_percent')
 while(-not $proc.WaitForExit(5000) -and ((Get-Date)-$started).TotalSeconds -lt $limit){
  $s=& nvidia-smi --query-gpu=timestamp,name,memory.used,utilization.gpu --format=csv,noheader,nounits
  foreach($line in $s){$samples.Add($line)}
 }
 $samples | Set-Content (Join-Path $run 'adapter_samples.csv') -Encoding utf8
}
if($proc.WaitForExit($limit*1000)){$rec.status='EXITED';$rec.exit_code=$proc.ExitCode}else{$rec.status='TIMEOUT_PROCESS_LEFT_RUNNING'}
$rec.ended=(Get-Date).ToString('o');$rec | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'exit.json') -Encoding utf8
$rec | ConvertTo-Json -Depth 5
