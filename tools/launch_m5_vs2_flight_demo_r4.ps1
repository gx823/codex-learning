#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Manifest,[switch]$ValidateOnly,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$demoWorkspace=Split-Path $PSScriptRoot -Parent
$demoDocs=[IO.Path]::GetFullPath((Join-Path $demoWorkspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
$demoManifest=[IO.Path]::GetFullPath($Manifest)
if(-not $demoManifest.StartsWith($demoDocs,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($demoManifest) -cne 'flight_demo_r4_manifest.json' -or $demoManifest.Contains('"') -or -not(Test-Path -LiteralPath $demoManifest -PathType Leaf)){throw 'Requires actual private FlightDemoR4 native Reload manifest; deploy this staged launcher into tools first.'}
$demoData=Get-Content -LiteralPath $demoManifest -Raw|ConvertFrom-Json
if($demoData.schema -cne 'HarborCity.M5VS2.FlightDemoR4.Author.v1' -or $demoData.status -cne 'READY_FOR_ENGINE_INPUT_DEMO_NOT_RUNTIME_TESTED' -or $demoData.owner -cne 'HarborCity_M5VS2_FlightDemoR4' -or $demoData.kind -cne 'EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED' -or $demoData.input_scope -cne 'ENGINE_INPUT_NOT_OS' -or $demoData.expected_npcs -ne 8 -or $demoData.expected_vehicles -ne 1){throw 'Unrecognized bounded flight-demo manifest.'}
if($demoData.destination -cnotmatch '^/Game/HarborCity/M5VS2/FlightDemoR4/Run_[0-9a-f]{12}$' -or $demoData.map -cne ($demoData.destination+'/L_FlightDemoR4')){throw 'Invalid private map namespace.'}
if($demoData.record_seconds -ne 60 -or $demoData.record_delay -ne 12 -or $demoData.sequence_wall_deadline_seconds -ne 55 -or $demoData.save_slot_prefix -cne 'HarborCity_VS2_FlightDemo_'){throw 'Unexpected capture or private-save contract.'}
$demoMap=$demoData.map
$demoReloadPath=Join-Path (Split-Path $demoManifest -Parent) 'author_result.json'
if([IO.Path]::GetFullPath($demoData.reload_report) -ine [IO.Path]::GetFullPath($demoReloadPath)){throw 'Reload report must accompany own manifest.'}
$demoReload=Get-Content -LiteralPath $demoReloadPath -Raw|ConvertFrom-Json
$demoCommand=Get-Content -LiteralPath (Join-Path (Split-Path $demoManifest -Parent) 'commandlet.json') -Raw|ConvertFrom-Json
if($demoReload.status -cne 'PASS' -or $demoReload.phase -cne 'FlightDemoR4Reload' -or $demoReload.schema -cne $demoData.schema -or $demoReload.fresh_process_disk_readback -cne 'PASS' -or @($demoReload.preservation.changed_files).Count -ne 0 -or @($demoReload.checks|Where-Object status -CNE 'PASS').Count -ne 0 -or $demoCommand.status -cne 'PASS' -or $demoCommand.exit_code -ne 0){throw 'Fresh native Reload/process has not passed.'}
if($demoReload.launch_manifest.sha256 -ine (Get-FileHash -LiteralPath $demoManifest -Algorithm SHA256).Hash -or $demoReload.launch_manifest.bytes -ne (Get-Item -LiteralPath $demoManifest).Length){throw 'Manifest differs from own actual Reload evidence.'}
$demoBindings=@($demoData.source_bindings)
if($demoBindings.Count -lt 30){throw 'Missing native source bindings.'}
foreach($demoSource in $demoBindings){
    $demoFile=[IO.Path]::GetFullPath($demoSource.path)
    $demoAllowedRoot=switch($demoSource.kind){
        'package'{Join-Path $demoWorkspace 'HarborCity/Content'}
        'module'{Join-Path $demoWorkspace 'HarborCity/Binaries/Win64'}
        'cpp'{Join-Path $demoWorkspace 'HarborCity/Source'}
        'config'{Join-Path $demoWorkspace 'HarborCity/Config'}
        'backup'{'E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups'}
        'script'{$PSScriptRoot}
        'evidence'{$demoDocs}
        default{throw 'Unknown source binding kind.'}
    }
    $demoAllowedRoot=[IO.Path]::GetFullPath($demoAllowedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $demoFile.StartsWith($demoAllowedRoot,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $demoFile -PathType Leaf) -or (Get-Item -LiteralPath $demoFile).Length -ne $demoSource.bytes -or (Get-FileHash -LiteralPath $demoFile -Algorithm SHA256).Hash -ine $demoSource.sha256){throw "Demo source changed/escaped allowed root: $demoFile"}
}
$demoProject=Join-Path $demoWorkspace 'HarborCity/HarborCity.uproject'
$demoEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$demoModule=Join-Path $demoWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
foreach($demoFile in @($demoProject,$demoEditor,$demoModule)){if(-not(Test-Path -LiteralPath $demoFile -PathType Leaf)){throw "Missing $demoFile"}}
$demoMapFile=Join-Path (Join-Path $demoWorkspace 'HarborCity/Content') ($demoMap.Substring(6)+'.umap')
if(@($demoBindings|Where-Object {[IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($demoMapFile)}).Count -ne 1 -or @($demoBindings|Where-Object kind -CEQ 'module').Count -ne 1){throw 'Missing exact map or module byte proof.'}
foreach($demoExtension in @('.h','.cpp')){
    $demoSourceFile=Join-Path $demoWorkspace ('HarborCity/Source/HarborCity/M5VS2/HCM5VS2FlightDemoDirector'+$demoExtension)
    if(@($demoBindings|Where-Object {$_.kind -ceq 'cpp' -and [IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($demoSourceFile)}).Count -ne 1){throw 'Missing native input-demo source binding.'}
}
if($ValidateOnly){[ordered]@{status='PASS_STATIC_LAUNCH_GUARDS_ONLY';input_scope='ENGINE_INPUT_NOT_OS';kind=$demoData.kind;manifest=$demoManifest;map=$demoMap;runtime='NOT_RUN';record_seconds=60;record_delay=12}|ConvertTo-Json;return}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue){throw 'Close existing project engine/game normally before running one bounded demo.'}
$demoRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
# Existing perspective persistence isolates only slots containing _Test_.
$demoSlot='HarborCity_VS2_FlightDemo_Test_'+$demoRun
$demoOut=Join-Path $demoDocs ($demoRun+'_flight_demo_r4_game')
New-Item -ItemType Directory -Path $demoOut|Out-Null
$demoCache='D:/GameDev/Cache/Unreal/HarborCity'
$demoCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$demoArgs=@(('"'+$demoProject+'"'),$demoMap,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2FlightDemo','-M5VS2AutoQuit','-M5VS2RecordSeconds=60','-M5VS2RecordDelay=12',('-HCM1SaveSlot='+$demoSlot),('-M5VS2EvidenceDir="'+$demoOut+'"'),('-abslog="'+(Join-Path $demoOut 'game.log')+'"'),('-ExecCmds="'+($demoCommands -join ',')+'"'),"-LocalDataCachePath=$demoCache/DDC","-ZenDataPath=$demoCache/Zen")
$demoRecord=[ordered]@{schema='HarborCity.M5VS2.FlightDemoR4.Launch.v1';status='STARTING';kind=$demoData.kind;input_scope='ENGINE_INPUT_NOT_OS';started_at=(Get-Date).ToString('o');executable=$demoEditor;map=$demoMap;save_slot=$demoSlot;manifest=$demoManifest;manifest_sha256=(Get-FileHash -LiteralPath $demoManifest -Algorithm SHA256).Hash;reload=$demoReloadPath;reload_sha256=(Get-FileHash -LiteralPath $demoReloadPath -Algorithm SHA256).Hash;module_sha256=(Get-FileHash -LiteralPath $demoModule -Algorithm SHA256).Hash;arguments=$demoArgs;scope=$demoData.scope;auto_engine_input=$true;auto_camera=$false;auto_quit_only_without_user_stop=$true;os_input_result='NOT_RUN';visual='USER_REVIEW';record_seconds=60;record_delay=12;recording_directory_root=(Join-Path $demoDocs 'recordings');recording_result='NOT_RUN';demo_result='NOT_RUN';requested_quality='High';requested_fps_cap=0;requested_vsync=0}
$demoRecordPath=Join-Path $demoOut 'launch.json'
$demoRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $demoRecordPath -Encoding utf8
$demoEnv=@{'UE-LocalDataCachePath'="$demoCache/DDC";'UE-ZenDataPath'="$demoCache/Zen";'UE-ZenSubprocessDataPath'="$demoCache/Zen"};$demoOld=@{}
try{
    foreach($demoKey in $demoEnv.Keys){$demoOld[$demoKey]=[Environment]::GetEnvironmentVariable($demoKey,'Process');[Environment]::SetEnvironmentVariable($demoKey,$demoEnv[$demoKey],'Process')}
    # Authorized visible game window. All scripted input is in the native opt-in director.
    $demoProcess=Start-Process -FilePath $demoEditor -ArgumentList $demoArgs -WorkingDirectory (Split-Path $demoProject -Parent) -WindowStyle Normal -PassThru
    $null=$demoProcess.Handle;$demoRecord.process_id=$demoProcess.Id;$demoRecord.status='STARTED'
}catch{$demoRecord.status='FAIL';$demoRecord.error=$_.Exception.Message;throw}
finally{
    foreach($demoKey in $demoOld.Keys){[Environment]::SetEnvironmentVariable($demoKey,$demoOld[$demoKey],'Process')}
    $demoRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $demoRecordPath -Encoding utf8
}
Write-Output "ENGINE_INPUT_NOT_OS; editor -game, not packaged: $demoOut"
Write-Output 'Esc / P / application focus loss permanently cancels this run; never resumes or reacquires focus.'
if($WaitForExit){
    $demoProcess.WaitForExit();$demoProcess.Refresh();$demoRecord.exit_code=$demoProcess.ExitCode;$demoRecord.ended_at=(Get-Date).ToString('o');$demoRecord.status='EXITED'
    $demoResults=@(Get-ChildItem -LiteralPath $demoOut -Directory -Filter 'FlightDemo_*'|ForEach-Object {Join-Path $_.FullName 'flight_demo.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    if($demoResults.Count -eq 1){
        $demoResult=Get-Content -LiteralPath $demoResults[0] -Raw|ConvertFrom-Json
        $demoRecord.demo_result=$demoResult.status;$demoRecord.result_file=$demoResults[0];$demoRecord.result_sha256=(Get-FileHash -LiteralPath $demoResults[0] -Algorithm SHA256).Hash
        $demoRecord.recording_result=$demoResult.actual_native_capture.status;$demoRecord.recording_directory=$demoResult.native_capture_directory
    }
    $demoRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $demoRecordPath -Encoding utf8
    if($null -eq $demoProcess.ExitCode){throw 'Missing actual process exit code.'}
    if($demoProcess.ExitCode -eq 0 -and $demoRecord.demo_result -cne 'PASS_ENGINE_INPUT_ONLY'){throw 'Process exit 0 is not evidence of completed demo; inspect actual result/log.'}
    exit $demoProcess.ExitCode
}
