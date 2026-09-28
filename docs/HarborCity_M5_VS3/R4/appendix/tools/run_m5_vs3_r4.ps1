#requires -Version 7.0
[CmdletBinding()]
param(
 [ValidateSet('m5_vs3_r3_b','m5_vs3_r3_baseline','m5_full','perf','m5_vs3_r2_armory','m5_vs3_r2_matrix','m5_vs3_r2_showcase','m5_vs3_r2_reopen','m5_vs3_r1_falls')][string]$Mode='m5_vs3_r3_b',
 [string]$Executable='',
 [string]$R4Case='',
 [ValidatePattern('^(20|30|60|0)(,(20|30|60|0))*$')][string]$MatrixCaps='20,30,60,0',
 [ValidateSet('High','Epic')][string]$Quality='High',
 [int]$RecordSeconds=0,
 [switch]$Followup,
 [switch]$SprintOnly,
 [switch]$WeaponCloseups,
 [switch]$CoverageOnly,
 [switch]$HairDiagnostic,
 [switch]$BowDiagnostic,
 [switch]$BowFinal,
 [switch]$BowSave,
 [switch]$R3Showcase,
 [switch]$Slow,
 [string]$SeedSaveFile='',
 [string]$ReuseTestSlot='',
 [string]$ReuseUserDirectory=''
)
$ErrorActionPreference='Stop'
if($RecordSeconds -gt 0){foreach($drive in @('D','E')){$free=(Get-PSDrive -Name $drive).Free;if($free -lt 30GB){throw ('Recording paused: '+$drive+': has only '+[math]::Round($free/1GB,2)+' GiB; 30 GiB required.')};Write-Output ('Recording free space '+$drive+': '+[math]::Round($free/1GB,2)+' GiB')}}
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'hc_r3_window_guard.ps1')
. (Join-Path $PSScriptRoot 'hc_r4_stop_guard.ps1')
$baseDoc=Join-Path $root 'docs/HarborCity_M5_VS3'
$doc=Join-Path $baseDoc 'R4'
$hostCheck=& (Join-Path $PSScriptRoot 'hc_r4_host_status.ps1') | ConvertFrom-Json
if($hostCheck.foreground_process -eq 'LockApp' -or $hostCheck.foreground_pid -eq 0){throw 'BLOCKED_LOCKED: no launch or desktop input behind Windows lockscreen/unavailable desktop'}
if(Test-Path -LiteralPath (Join-Path $doc 'PAUSE_FOR_REBUILD')){throw 'Remaining jobs paused between runs: inspect recorded visual failure before rebuilding.'}
if(Test-Path -LiteralPath (Join-Path $doc 'final/MEDIA_WORKFLOW_UPGRADE_PENDING')){throw 'Current step completed; media workflow upgrade must finish before another launch.'}
if(Test-Path -LiteralPath (Join-Path $doc 'STOP_DESKTOP_CHAIN')){throw 'Desktop test chain paused; inspect focus/stop condition before a new launch.'}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe' OR Name='HarborCity-Win64-Shipping.exe'"){throw 'Existing engine/game must finish normally first.'}
$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
$run=Join-Path $doc ($Mode+'_'+$stamp)
New-Item -ItemType Directory -Path $run | Out-Null
$project=Join-Path $root 'HarborCity/HarborCity.uproject'
$editor= -not $Executable
if($editor){$Executable='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'}
elseif(-not ([IO.Path]::GetFullPath($Executable).StartsWith('E:\GameDev\Builds\HarborCity\M5_VS3\Candidate_',[StringComparison]::OrdinalIgnoreCase))){throw 'Only VS2 candidate executables are allowed.'}
$argsList=[Collections.Generic.List[string]]::new()
$isolatedUser=Join-Path $run 'User'
if($ReuseUserDirectory){
 $resolvedUser=[IO.Path]::GetFullPath($ReuseUserDirectory)
 if(-not $resolvedUser.StartsWith(([IO.Path]::GetFullPath($doc)+'\'),[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $resolvedUser -Leaf) -ne 'User' -or -not (Test-Path -LiteralPath $resolvedUser)){throw 'Reuse must be an existing isolated R2 User directory'}
 $isolatedUser=$resolvedUser
}
foreach($platform in $(if($ReuseUserDirectory){@()}else{@('Windows','WindowsEditor')})){
 $cfg=Join-Path $isolatedUser ('Saved/Config/'+$platform)
 New-Item -ItemType Directory -Path $cfg -Force|Out-Null
 @'
[/Script/Engine.UserInterfaceSettings]
bAllowHighDPIInGameMode=True
bAllowHighDpiWhenUnattended=True
[DevOptions.Shaders]
NumUnusedShaderCompilingThreads=23
NumUnusedShaderCompilingThreadsDuringGame=23
ShaderCompilerCoreCountThreshold=128
PercentageUnusedShaderCompilingThreads=100
'@ | Set-Content -LiteralPath (Join-Path $cfg 'Engine.ini') -Encoding utf8
 @'
[/Script/Engine.GameUserSettings]
ResolutionSizeX=1920
ResolutionSizeY=1080
LastUserConfirmedResolutionSizeX=1920
LastUserConfirmedResolutionSizeY=1080
FullscreenMode=2
LastConfirmedFullscreenMode=2
PreferredFullscreenMode=2
Version=5
bUseVSync=False
FrameRateLimit=0.000000
'@ | Set-Content -LiteralPath (Join-Path $cfg 'GameUserSettings.ini') -Encoding utf8
}
$argsList.Add('-UserDir="'+$isolatedUser+'"')
$argsList.Add('-M5PublicCapture')
$argsList.Add('-R3WindowGuard')
if($R4Case -in @('pairs','shield')){$argsList.Add('-R4RealTimeSteps')}
if($R4Case -eq 'hipfire'){$argsList.Add('-R4HipFire')}
if($R4Case){if($R4Case -notmatch '^[A-Za-z0-9_]+$'){throw 'Invalid R4 case'};$argsList.Add('-R4Case='+$R4Case)}
if($editor){$argsList.Insert(0,'"'+$project+'"')}
if($Mode.StartsWith('reverse')){
 $map='/Game/HarborCity/Maps/L_M1_Playground';$test=if($Mode -eq 'reverse_legacy'){'reverse_diagnostic_legacy'}else{'reverse_diagnostic'}
 $slot='HarborCity_M1_R2_Test_M4_VS2_'+$Mode+'_'+$stamp
 $argsList.Add('-M1Test='+$test);$argsList.Add('-M5VS2ReverseTrace')
}else{
 $selection=Get-Content (Join-Path $baseDoc 'town_selection.json') -Raw | ConvertFrom-Json
 $map=$selection.map
 if($map -notmatch '^/Game/HarborCity/(M5VS2/Part3|M5VS3)/Town_[a-f0-9]+/L_HarborTown$'){throw 'Invalid saved town selection.'}
 $slot='HarborCity_M5_VS1_Test_VS3_'+$Mode+'_'+$stamp
 $test=if($Mode.StartsWith('m5_')){$Mode}else{'m5_vs2_'+$Mode};$argsList.Add('-M5Test='+$test)
}
if($ReuseTestSlot){if($ReuseTestSlot -notmatch '^HarborCity_M5_VS1_Test_VS3_[A-Za-z0-9_]+$' -or $ReuseTestSlot.Length -gt 100){throw 'Invalid isolated slot'};$slot=$ReuseTestSlot}
if($SeedSaveFile){
 $seed=[IO.Path]::GetFullPath($SeedSaveFile)
 if(-not $seed.StartsWith(([IO.Path]::GetFullPath($doc)+'\'),[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($seed) -cne ($slot+'.sav') -or -not (Test-Path -LiteralPath $seed -PathType Leaf) -or $ReuseUserDirectory){throw 'Seed must be the exact prior isolated R3 test slot, copied into a fresh User directory'}
 $saveDirectory=Join-Path $isolatedUser 'Saved/SaveGames';New-Item -ItemType Directory -Path $saveDirectory -Force|Out-Null
 Copy-Item -LiteralPath $seed -Destination (Join-Path $saveDirectory ($slot+'.sav'))
}
if($Mode -eq 'm5_save_probe'){$argsList.Add('-M5VS3SaveDiagnostics')}
$argsList.Add($map)
if($editor){$argsList.Add('-game')}
foreach($a in @('-unattended','-windowed','-ResX=1920','-ResY=1080','-language=en','-nosplash','-M1AutoQuit',('-HCM1SaveSlot='+$slot),('-abslog="'+(Join-Path $run 'game.log')+'"'),'-LocalDataCachePath=D:/GameDev/Cache/Unreal/HarborCity/DDC','-ZenDataPath=D:/GameDev/Cache/Unreal/HarborCity/Zen')){$argsList.Add($a)}
$q=if($Quality -eq 'Epic'){3}else{2}
$argsList.Add('-R2MatrixCaps='+$MatrixCaps);$argsList.Add('-HCM5VS3QA');$argsList.Add('-R1Trace');$argsList.Add('-M5Quality='+$q)
if($RecordSeconds -gt 0){$argsList.Add('-M5PublicCapture')}
$exec='sg.ViewDistanceQuality '+$q+',sg.AntiAliasingQuality '+$q+',sg.ShadowQuality '+$q+',sg.GlobalIlluminationQuality '+$q+',sg.ReflectionQuality '+$q+',sg.PostProcessQuality '+$q+',sg.TextureQuality '+$q+',sg.EffectsQuality '+$q+',sg.FoliageQuality '+$q+',sg.ShadingQuality '+$q+',sg.LandscapeQuality '+$q+',r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0'
if($Mode -eq 'perf'){$exec+=',r.ScreenPercentage,r.VSync,t.MaxFPS,sg.FoliageQuality,sg.ShadingQuality,sg.LandscapeQuality'}
$argsList.Add('-ExecCmds="'+$exec+'"')
if($RecordSeconds -gt 0){$argsList.Add('-R1GateCapture');$argsList.Add('-HCM4RecordSeconds='+$RecordSeconds);$argsList.Add('-HCM3RecordDelay=6')}
if($Mode -eq 'm5_vs3_r3_baseline'){$argsList.Add('-R3Baseline')}
if($Followup){$argsList.Add('-R3Followup')}
if($HairDiagnostic){$argsList.Add('-R3HairDiagnostic')}
if($BowDiagnostic -or $BowFinal){$argsList.Add('-R3BowDiagnostic')}
if($BowFinal){$argsList.Add('-R3BowFinal')}
if($R3Showcase){$argsList.Add('-R3Showcase');$argsList.Add('-R4Showcase')}
if($BowSave){$argsList.Add('-R3BowSave')}
if($SprintOnly){$argsList.Add('-R3SprintOnly')}
if($WeaponCloseups){$argsList.Add('-R3WeaponCloseups')}
if($CoverageOnly){$argsList.Add('-R3CoverageOnly')}
if($Slow){$argsList.Add('-R3Slow')}
$started=Get-Date
if(Test-HCR4DesktopStopped){throw 'USER_ABORTED: no launch after Escape'}
$proc=Start-Process -FilePath $Executable -ArgumentList $argsList.ToArray() -WorkingDirectory (Split-Path $Executable -Parent) -PassThru
$rec=[ordered]@{status='RUNNING';pid=$proc.Id;executable=$Executable;kind=if($editor){'EDITOR_GAME'}else{'STANDALONE_PACKAGE'};input_layer='ENGINE_ACTION_NOT_OS';arguments=$argsList.ToArray();started=$started.ToString('o');map=$map}
$rec | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'launch.json') -Encoding utf8
$activePath=Join-Path $doc 'ACTIVE_RUN.json'
[ordered]@{status='RUNNING';pid=$proc.Id;run=$run;mode=$Mode;started=$started.ToString('o');resume='Check this PID and exit.json before starting anything; never repeat a completed run because the chat disconnected.'} | ConvertTo-Json | Set-Content -LiteralPath $activePath -Encoding utf8
$limit=if($Mode -eq 'soak'){2400}elseif($Mode -in @('m5_vs3_r2_matrix','m5_vs3_r2_showcase','m5_full')){1800}else{600}
$memory=[Collections.Generic.List[object]]::new()
$networkSampled=$false
$lastForeground=$null;$initialFocusSet=$false
$everForeground=$false;$lostForegroundSince=$null
$adapter=[Collections.Generic.List[string]]::new();$adapter.Add('timestamp, gpu_name, memory_used_MiB, gpu_util_percent')
while(-not $proc.WaitForExit(250) -and ((Get-Date)-$started).TotalSeconds -lt $limit){
 if(Test-HCR4DesktopStopped -Run $run -ReadResults){$rec.status='USER_ABORTED';break}
 if(-not (Test-HCR3Window -KeepForeground -GameProcessId $proc.Id -EvidencePath (Join-Path $run 'window_guard.jsonl'))){$rec.status='BLOCKED_WINDOW';break}
 $gameWindows=@([HCR3WindowGuard]::Windows($proc.Id)|Where-Object {[HCR3WindowGuard]::WindowClass($_) -eq 'UnrealWindow' -and [HCR3WindowGuard]::IsWindowVisible($_)})
 if($gameWindows.Count -and -not $initialFocusSet){
  if(Test-HCR4DesktopStopped -Run $run -ReadResults){$rec.status='USER_ABORTED';break}
  [void][HCR3WindowGuard]::SetForegroundWindow($gameWindows[0]);$initialFocusSet=$true
 }
 $hasFocus=[HCR3WindowGuard]::GetForegroundWindow() -in $gameWindows
 if($hasFocus -ne $lastForeground){@{seconds=((Get-Date)-$started).TotalSeconds;game_foreground=$hasFocus}|ConvertTo-Json -Compress|Add-Content -LiteralPath (Join-Path $run 'focus_events.jsonl');$lastForeground=$hasFocus}
 if($hasFocus){$everForeground=$true;$lostForegroundSince=$null}
 elseif($everForeground -and $gameWindows.Count -gt 0 -and $R4Case -ne 'os'){
  if($null -eq $lostForegroundSince){$lostForegroundSince=Get-Date}
  if(((Get-Date)-$lostForegroundSince).TotalSeconds -ge 2){
   if(Test-HCR4DesktopStopped -Run $run -ReadResults){$rec.status='USER_ABORTED';break}
   $rec.status='BLOCKED_FOCUS';$rec.focus_stop='Actual game window lost foreground for two seconds; no repeated focus stealing or further test jobs.'
   [void]$proc.CloseMainWindow();[void]$proc.WaitForExit(5000);break
  }
 }
 $proc.Refresh()
 $memory.Add([ordered]@{seconds=((Get-Date)-$started).TotalSeconds;private_bytes=$proc.PrivateMemorySize64;working_set_bytes=$proc.WorkingSet64;process_peak_working_set_bytes=$proc.PeakWorkingSet64})
 if(-not $networkSampled -and ((Get-Date)-$started).TotalSeconds -ge 10){
  $networkSampled=$true
  try {
   $listeners=@(Get-NetTCPConnection -State Listen -ErrorAction Stop | Where-Object OwningProcess -eq $proc.Id | Select-Object LocalAddress,LocalPort)
   [ordered]@{status='SAMPLED';pid=$proc.Id;tcp_listeners=$listeners;scope='One runtime TCP-listener sample, not proof that Windows showed no firewall dialog.'}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $run 'network_sample.json') -Encoding utf8
  } catch {[ordered]@{status='NOT_RUN';reason='Runtime TCP listener query unavailable'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $run 'network_sample.json') -Encoding utf8}
 }
 if($Mode -eq 'perf'){$sample=& nvidia-smi --query-gpu=timestamp,name,memory.used,utilization.gpu --format=csv,noheader,nounits;foreach($line in $sample){$adapter.Add($line)}}
}
$memory | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $run 'memory_samples.json') -Encoding utf8
if($Mode -eq 'perf'){$adapter | Set-Content (Join-Path $run 'adapter_samples.csv') -Encoding utf8}
if(Test-HCR4DesktopStopped -Run $run -ReadResults){$rec.status='USER_ABORTED'}
if($rec.status -in @('BLOCKED_WINDOW','BLOCKED_FOCUS','USER_ABORTED')){$rec.exit_code=$null}elseif($proc.HasExited){$rec.status='EXITED';$rec.exit_code=$proc.ExitCode}else{$rec.status='TIMEOUT_PROCESS_LEFT_RUNNING'}
$rec.ended=(Get-Date).ToString('o');$rec | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $run 'exit.json') -Encoding utf8
[ordered]@{status=$rec.status;pid=$proc.Id;run=$run;mode=$Mode;exit_code=$rec.exit_code;ended=$rec.ended;recording=if($RecordSeconds -gt 0){'ENCODING_PENDING'}else{'NOT_REQUESTED'}} | ConvertTo-Json | Set-Content -LiteralPath $activePath -Encoding utf8
$rec | ConvertTo-Json -Depth 5

if($RecordSeconds -gt 0 -and $proc.HasExited -and $rec.status -ne 'USER_ABORTED'){
 if($proc.ExitCode -ne 0 -and -not @(Get-ChildItem -LiteralPath (Join-Path $baseDoc 'recordings') -Filter capture.json -Recurse | Where-Object LastWriteTime -ge $started).Count){
  $active=Get-Content -LiteralPath $activePath -Raw | ConvertFrom-Json;$active.recording='NOT_STARTED_PROCESS_FAILED';$active|ConvertTo-Json|Set-Content -LiteralPath $activePath -Encoding utf8
  throw ('Engine exited '+$proc.ExitCode+' before a completed capture. Inspect the bounded error lines in '+(Join-Path $run 'game.log'))
 }
 try {
  & (Join-Path $PSScriptRoot 'complete_m5_vs3_capture.ps1') -Started $started -Executable $Executable
  $active=Get-Content -LiteralPath $activePath -Raw | ConvertFrom-Json;$active.recording='ENCODED_AND_DECODE_VERIFIED';$active|ConvertTo-Json|Set-Content -LiteralPath $activePath -Encoding utf8
 } catch {
  $active=Get-Content -LiteralPath $activePath -Raw | ConvertFrom-Json;$active.recording='FAILED_PRESERVE_SOURCE_FRAMES';$active|ConvertTo-Json|Set-Content -LiteralPath $activePath -Encoding utf8
  throw
 }
}
