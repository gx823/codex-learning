#requires -Version 7.0
# One serial gate batch on retained gameplay maps. Not new-corner/package acceptance.
$ErrorActionPreference='Stop'
$gbWork=Split-Path $PSScriptRoot -Parent
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity -ErrorAction SilentlyContinue){throw 'An engine/game is running.'}
$gbToken=(Get-Date -Format 'yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$gbRoot=Join-Path $gbWork ('docs/HarborCity_M5_VS2/gate_regression/'+$gbToken)
New-Item -ItemType Directory -Path $gbRoot|Out-Null
$gbDLL=Join-Path $gbWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$gbRecord=[ordered]@{status='RUNNING';started_at=(Get-Date).ToString('o');module_sha256=(Get-FileHash -LiteralPath $gbDLL).Hash;scope='One serial preserved-map baseline batch; logic/engine inputs and explicit fixtures; NOT new harbor gameplay, OS input, or package acceptance.';runs=@()}
$gbCases=@(
 @{mode='core';ns='M1';map='L_M1_Playground';slot='HarborCity_M1_R2_Test_M4_VS2Gate_'+$gbToken},
 @{mode='r2_views';ns='M4';map='L_M2_SeafrontStreet';slot='HarborCity_M2_V1_Test_M4_R2_VS2GateViews_'+$gbToken},
 @{mode='r2_combat';ns='M4';map='L_M2_SeafrontStreet';slot='HarborCity_M2_V1_Test_M4_R2_VS2GateCombat_'+$gbToken},
 @{mode='m5_story';ns='M5';map='L_M5_CyberHarbor';slot='HarborCity_M5_VS1_Test_VS2Gate_'+$gbToken})
foreach($gbCase in $gbCases){
 $gbOut=Join-Path $gbRoot $gbCase.mode;New-Item -ItemType Directory -Path $gbOut|Out-Null
 $gbSince=(Get-Date).ToUniversalTime();$gbCache='D:/GameDev/Cache/Unreal/HarborCity'
 $gbArgs=@(('"'+(Join-Path $gbWork 'HarborCity/HarborCity.uproject')+'"'),('/Game/HarborCity/Maps/'+$gbCase.map),'-game','-unattended','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-nosplash','-M1AutoQuit',('-'+$gbCase.ns+'Test='+$gbCase.mode),('-HCM1SaveSlot='+$gbCase.slot),('-abslog="'+(Join-Path $gbOut 'game.log')+'"'),'-ExecCmds="sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0"',"-LocalDataCachePath=$gbCache/DDC","-ZenDataPath=$gbCache/Zen")
 $gbRow=[ordered]@{mode=$gbCase.mode;map=$gbCase.map;save_slot=$gbCase.slot;arguments=$gbArgs;started_utc=$gbSince.ToString('o');status='RUNNING'}
 $gbRecord.runs+=,$gbRow;$gbRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $gbRoot 'gate_baseline.json') -Encoding utf8
 $gbProc=Start-Process -FilePath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $gbArgs -WorkingDirectory (Join-Path $gbWork 'HarborCity') -WindowStyle Normal -PassThru
 $null=$gbProc.Handle;$gbRow.process_id=$gbProc.Id;Write-Output ('Started '+$gbCase.mode)
 $gbProc.WaitForExit();$gbProc.Refresh();$gbRow.exit_code=$gbProc.ExitCode;$gbRow.ended_utc=(Get-Date).ToUniversalTime().ToString('o')
 $gbSaved=Join-Path $gbWork ('HarborCity/Saved/'+$gbCase.ns+'Tests')
 $gbReports=@(Get-ChildItem -LiteralPath $gbSaved -Directory -Filter ('*_'+$gbCase.mode)|Where-Object {$_.CreationTimeUtc -ge $gbSince.AddSeconds(-2)}|ForEach-Object {Join-Path $_.FullName 'results.json'}|Where-Object {Test-Path -LiteralPath $_})
 if($gbReports.Count -eq 1){
  $gbResult=Get-Content -LiteralPath $gbReports[0] -Raw|ConvertFrom-Json
  $gbRow.result=$gbReports[0];$gbRow.runtime_status=$gbResult.status;$gbRow.failures=$gbResult.failures
  $gbRow.check_count=@($gbResult.checks).Count;$gbRow.failed_checks=@($gbResult.checks|Where-Object {$_.status -eq 'FAIL'})
  $gbRow.status=if($gbRow.exit_code -eq 0 -and $gbResult.status -ceq 'COMPLETE' -and $gbResult.failures -eq 0){'PASS'}else{'FAIL'}
  Copy-Item -LiteralPath $gbReports[0] -Destination (Join-Path $gbOut 'results.json')
  if($gbResult.status -ceq 'USER_ABORTED'){$gbRow.status='USER_ABORTED';$gbRecord.status='USER_ABORTED';break}
 }else{$gbRow.status='MISSING_RESULT'}
 Write-Output ($gbCase.mode+': '+$gbRow.status)
}
if($gbRecord.status -ne 'USER_ABORTED'){$gbRecord.status=if(@($gbRecord.runs|Where-Object {$_.status -ne 'PASS'}).Count -eq 0){'PASS_RETAINED_MAPS_ONLY'}else{'FAIL'}}
$gbRecord.ended_at=(Get-Date).ToString('o');$gbRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $gbRoot 'gate_baseline.json') -Encoding utf8
Write-Output ('Result: '+(Join-Path $gbRoot 'gate_baseline.json'))
