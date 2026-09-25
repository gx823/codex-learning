#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Plan,
    [Parameter(Mandatory=$true)][ValidateSet('High','Epic')][string]$Quality,
    [ValidateSet('Afternoon','Dusk','Night')][string]$Period='Afternoon',
    [switch]$ValidateOnly,
    [switch]$WaitForExit
)
$ErrorActionPreference='Stop'
$perfWorkspace='D:/科研学习/codex学习'
$perfDocs=[IO.Path]::GetFullPath((Join-Path $perfWorkspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
$perfPlan=[IO.Path]::GetFullPath($Plan)
if(-not $perfPlan.StartsWith($perfDocs,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($perfPlan) -cne 'corner_v2_performance_plan.json' -or $perfPlan.Contains('"') -or -not(Test-Path -LiteralPath $perfPlan -PathType Leaf)){throw 'Requires an actual owned revision-two performance plan.'}
$perfData=Get-Content -LiteralPath $perfPlan -Raw|ConvertFrom-Json
if($perfData.schema_version -ne 1 -or $perfData.plan_type -cne 'HARBOR_CORNER_REV2_PERFORMANCE' -or $perfData.status -cne 'READY_FOR_RUNTIME_NOT_MEASURED' -or $perfData.map -cnotmatch '^/Game/HarborCity/M5VS2/WorldRev2/Performance_[0-9a-f]{12}/L_CornerRevTwoPerformance$'){throw 'Unrecognized bounded native performance plan.'}
if(($perfData.expected_resolution -join ',') -ne '1920,1080' -or $perfData.warmup_seconds -ne 30 -or $perfData.measurement_seconds -ne 65 -or @($perfData.road_camera_points_cm).Count -lt 5 -or @($perfData.source_bindings).Count -lt 10){throw 'Unexpected warmup, sample duration, resolution or source inventory.'}
if($perfData.content_schema -cne 'HarborCity.M5VS2.CornerPerformanceR5.v1' -or $perfData.art_revision -ne 5 -or $perfData.expected_npcs -ne 8 -or $perfData.expected_vehicles -ne 1 -or $perfData.retained_inert_recorders -ne 1 -or $perfData.selected_hero -cnotmatch '^/Game/HarborCity/M5VS2/HeroModesty/Review_[0-9a-f]{12}/BP_HeroSafetyShorts$' -or $perfData.hero_safety.blueprint -cne $perfData.selected_hero){throw 'Requires actual Live R5 content and the selected safety layer.'}
$perfAuthorPath=Join-Path (Split-Path $perfPlan -Parent) 'author_result.json'
$perfCommandPath=Join-Path (Split-Path $perfPlan -Parent) 'commandlet.json'
$perfAuthor=Get-Content -LiteralPath $perfAuthorPath -Raw|ConvertFrom-Json
$perfCommand=Get-Content -LiteralPath $perfCommandPath -Raw|ConvertFrom-Json
if($perfAuthor.status -cne 'PASS' -or $perfAuthor.phase -cne 'CornerVTwoPerformanceR5' -or $perfAuthor.schema -cne $perfData.content_schema -or $perfAuthor.map -cne $perfData.map -or $perfAuthor.selected_hero -cne $perfData.selected_hero -or @($perfAuthor.checks|Where-Object status -CNE 'PASS').Count -ne 0 -or @($perfAuthor.preservation.changed_files).Count -ne 0 -or $perfCommand.phase -cne 'CornerVTwoPerformanceR5' -or $perfCommand.status -cne 'PASS' -or $perfCommand.exit_code -ne 0){throw 'Native R5 performance author/process has not passed.'}
if([IO.Path]::GetFullPath($perfAuthor.performance_plan.path) -ine $perfPlan -or $perfAuthor.performance_plan.sha256 -ine (Get-FileHash -LiteralPath $perfPlan -Algorithm SHA256).Hash -or $perfAuthor.performance_plan.bytes -ne (Get-Item -LiteralPath $perfPlan).Length){throw 'Plan differs from actual native author report.'}
$perfAuthorScript=[IO.Path]::GetFullPath($perfCommand.script)
if(-not $perfAuthorScript.StartsWith(([IO.Path]::GetFullPath((Join-Path $perfWorkspace 'tools')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $perfAuthorScript -Algorithm SHA256).Hash -ine $perfCommand.script_sha256){throw 'Author script differs from the actual commandlet.'}
$perfLivePath=[IO.Path]::GetFullPath($perfData.live_reload.path)
$perfLiveManifestPath=[IO.Path]::GetFullPath($perfData.live_manifest.path)
foreach($perfProof in @($perfData.live_reload,$perfData.live_manifest)){
    $perfProofPath=[IO.Path]::GetFullPath($perfProof.path)
    if(-not $perfProofPath.StartsWith($perfDocs,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $perfProofPath -PathType Leaf) -or (Get-FileHash -LiteralPath $perfProofPath -Algorithm SHA256).Hash -ine $perfProof.sha256){throw 'Actual Live proof missing or changed.'}
}
$perfLive=Get-Content -LiteralPath $perfLivePath -Raw|ConvertFrom-Json
$perfLiveManifest=Get-Content -LiteralPath $perfLiveManifestPath -Raw|ConvertFrom-Json
$perfLiveCommandPath=Join-Path (Split-Path $perfLivePath -Parent) 'commandlet.json'
$perfLiveCommand=Get-Content -LiteralPath $perfLiveCommandPath -Raw|ConvertFrom-Json
if($perfLive.status -cne 'PASS' -or $perfLive.phase -cne 'CornerPlayableR5Reload' -or $perfLive.schema -cne 'HarborCity.M5VS2.CornerPlayableR5.v1' -or $perfLive.art_revision -ne 5 -or $perfLive.fresh_process_disk_readback -cne 'PASS' -or @($perfLive.checks|Where-Object status -CNE 'PASS').Count -ne 0 -or @($perfLive.preservation.changed_files).Count -ne 0 -or $perfLiveCommand.phase -cne 'CornerPlayableR5Reload' -or $perfLiveCommand.status -cne 'PASS' -or $perfLiveCommand.exit_code -ne 0 -or $perfLive.selected_hero -cne $perfData.selected_hero){throw 'Complete actual Live R5 Reload is required.'}
if($perfLiveManifest.schema -cne $perfLive.schema -or $perfLiveManifest.status -cne 'READY_FOR_PLAY_NOT_RUNTIME_TESTED' -or $perfLiveManifest.expected_npcs -ne 8 -or $perfLiveManifest.expected_vehicles -ne 1 -or $perfLiveManifest.art_revision -ne 5 -or [IO.Path]::GetFullPath($perfLiveManifest.reload_report) -ine $perfLivePath -or $perfLive.launch_manifest.sha256 -ine $perfData.live_manifest.sha256 -or $perfLive.maps.Afternoon.package -cne $perfData.source_live_map -or $perfLiveManifest.maps.Afternoon -cne $perfData.source_live_map){throw 'Live map and manifest do not match performance provenance.'}
$perfKinds=@{}
foreach($perfSource in $perfData.source_bindings){
    $perfFile=[IO.Path]::GetFullPath($perfSource.path)
    $perfAllowedRoot=switch($perfSource.kind){
        'package'{Join-Path $perfWorkspace 'HarborCity/Content'}
        'module'{Join-Path $perfWorkspace 'HarborCity/Binaries/Win64'}
        'cpp'{Join-Path $perfWorkspace 'HarborCity/Source'}
        'script'{Join-Path $perfWorkspace 'tools'}
        'config'{Join-Path $perfWorkspace 'HarborCity/Config'}
        'backup'{'E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups'}
        'evidence'{$perfDocs}
        default{throw 'Unknown source kind.'}
    }
    $perfAllowedRoot=[IO.Path]::GetFullPath($perfAllowedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $perfFile.StartsWith($perfAllowedRoot,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $perfFile -PathType Leaf) -or (Get-Item -LiteralPath $perfFile).Length -ne $perfSource.bytes -or (Get-FileHash -LiteralPath $perfFile -Algorithm SHA256).Hash -ine $perfSource.sha256){throw "Performance source changed or outside allowed root: $perfFile"}
    $perfKinds[$perfSource.kind]=$true
}
foreach($perfKind in @('package','module','cpp','script','evidence')){if(-not $perfKinds.ContainsKey($perfKind)){throw "Missing source binding kind $perfKind"}}
$perfContent=[IO.Path]::GetFullPath((Join-Path $perfWorkspace 'HarborCity/Content'))
$perfRequiredPackages=@($perfData.map,$perfData.source_live_map,$perfData.selected_hero)
foreach($perfPackage in $perfRequiredPackages){
    $perfSuffix=if($perfPackage.Split('/')[-1].StartsWith('L_')){'.umap'}else{'.uasset'}
    $perfPackageFile=[IO.Path]::GetFullPath((Join-Path $perfContent ($perfPackage.Substring(6)+$perfSuffix)))
    if(@($perfData.source_bindings|Where-Object { $_.kind -ceq 'package' -and [IO.Path]::GetFullPath($_.path) -ieq $perfPackageFile }).Count -ne 1){throw 'Missing exact source or target package binding.'}
}
$perfModuleRequired=[IO.Path]::GetFullPath((Join-Path $perfWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'))
if(@($perfData.source_bindings|Where-Object kind -CEQ 'module').Count -ne 1 -or @($perfData.source_bindings|Where-Object { $_.kind -ceq 'module' -and [IO.Path]::GetFullPath($_.path) -ieq $perfModuleRequired }).Count -ne 1){throw 'Exactly one current native DLL is required.'}
if($ValidateOnly){[ordered]@{status='PASS_STATIC_LAUNCH_GUARDS_ONLY';map=$perfData.map;content_schema=$perfData.content_schema;source_live_map=$perfData.source_live_map;selected_hero=$perfData.selected_hero;npcs=8;runtime='NOT_RUN';recording_requested=$false}|ConvertTo-Json;return}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue){throw 'Close the existing engine/game normally before native performance sampling.'}
$perfProject=Join-Path $perfWorkspace 'HarborCity/HarborCity.uproject'
$perfEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$perfModule=Join-Path $perfWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
foreach($perfFile in @($perfProject,$perfEditor,$perfModule)){if(-not(Test-Path -LiteralPath $perfFile -PathType Leaf)){throw "Missing $perfFile"}}
$perfRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$perfOut=Join-Path $perfDocs ('editor_runtime/'+$perfRun+'_corner_performance_r5_'+$Quality+'_'+$Period)
New-Item -ItemType Directory -Path $perfOut|Out-Null
$perfCache='D:/GameDev/Cache/Unreal/HarborCity'
$perfLevel=if($Quality -ceq 'High'){2}else{3}
$perfCommands=@('ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading')|ForEach-Object{"sg.$($_)Quality $perfLevel"}
$perfCommands+=@('sg.ResolutionQuality 100','r.ScreenPercentage 100','r.SecondaryScreenPercentage.GameViewport 100','r.DynamicRes.OperationMode 0','r.VSync 0','t.MaxFPS 0')
$perfArgs=@(('"'+$perfProject+'"'),$perfData.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-d3d12','-language=en','-M5VS2CornerPerformance','-M5VS2AutoQuit',('-M5VS2PerfQuality='+$Quality),('-M5VS2PerfPeriod='+$Period),('-M5VS2CornerPerformancePlan="'+$perfPlan+'"'),('-M5VS2EvidenceDir="'+$perfOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_Performance_Test_'+$perfRun),('-abslog="'+(Join-Path $perfOut 'game.log')+'"'),('-ExecCmds="'+($perfCommands -join ',')+'"'),"-LocalDataCachePath=$perfCache/DDC","-ZenDataPath=$perfCache/Zen")
$perfRecord=[ordered]@{content_schema=$perfData.content_schema;art_revision=5;source_live_map=$perfData.source_live_map;live_reload=$perfLivePath;live_reload_sha256=$perfData.live_reload.sha256;expected_npcs=8;expected_vehicles=1;retained_inert_recorders=1;native_recording_observed='NOT_RUN';capture_artifact_check='NOT_RUN';milestone='M5_VS2';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';started_at=(Get-Date).ToString('o');executable=$perfEditor;module_sha256=(Get-FileHash -LiteralPath $perfModule -Algorithm SHA256).Hash;plan=$perfPlan;plan_sha256=(Get-FileHash -LiteralPath $perfPlan -Algorithm SHA256).Hash;source_hash_validation='PASS';map=$perfData.map;selected_hero=$perfData.selected_hero;arguments=$perfArgs;os_input_used=$false;screenshot_or_video_requested=$false;requested_quality=$Quality;requested_period=$Period;requested_width=1920;requested_height=1080;requested_render_percent=100;requested_vsync=0;requested_fps_cap=0;warmup_seconds=30;measurement_seconds=65;runtime_performance='NOT_RUN'}
$perfRecordingRoot=Join-Path $perfDocs 'recordings'
$perfCapturesBefore=@(if(Test-Path -LiteralPath $perfRecordingRoot){Get-ChildItem -LiteralPath $perfRecordingRoot -File -Filter 'capture.json' -Recurse|ForEach-Object FullName})
if(@($perfArgs|Where-Object {$_ -match '(?i)-M5VS2Record|-(?:M3|M4).*Record'}).Count -ne 0){throw 'R5 performance must not enable recording.'}
$perfRecordPath=Join-Path $perfOut 'launch.json'
$perfRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $perfRecordPath -Encoding utf8
$perfEnv=@{'UE-LocalDataCachePath'="$perfCache/DDC";'UE-ZenDataPath'="$perfCache/Zen";'UE-ZenSubprocessDataPath'="$perfCache/Zen"};$perfOld=@{}
try{
    foreach($perfKey in $perfEnv.Keys){$perfOld[$perfKey]=[Environment]::GetEnvironmentVariable($perfKey,'Process');[Environment]::SetEnvironmentVariable($perfKey,$perfEnv[$perfKey],'Process')}
    # Authorized visible game window; no screenshot, recording or synthetic OS input.
    $perfProcess=Start-Process -FilePath $perfEditor -ArgumentList $perfArgs -WorkingDirectory (Split-Path $perfProject -Parent) -WindowStyle Normal -PassThru
    $null=$perfProcess.Handle;$perfRecord.process_id=$perfProcess.Id;$perfRecord.status='STARTED'
}catch{$perfRecord.status='FAIL';$perfRecord.error=$_.Exception.Message;throw}
finally{
    foreach($perfKey in $perfOld.Keys){[Environment]::SetEnvironmentVariable($perfKey,$perfOld[$perfKey],'Process')}
    $perfRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $perfRecordPath -Encoding utf8
}
Write-Output "Live R5 performance evidence: $perfOut"
if($WaitForExit){
    $perfProcess.WaitForExit();$perfProcess.Refresh();$perfRecord.exit_code=$perfProcess.ExitCode;$perfRecord.ended_at=(Get-Date).ToString('o');$perfRecord.status='EXITED'
    $perfResults=@(Get-ChildItem -LiteralPath $perfOut -Directory|ForEach-Object {Join-Path $_.FullName 'corner_performance.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    if($perfResults.Count -eq 1){$perfResult=Get-Content -LiteralPath $perfResults[0] -Raw|ConvertFrom-Json;$perfRecord.runtime_report=$perfResults[0];$perfRecord.runtime_performance=$perfResult.status}
    $perfCapturesAfter=@(if(Test-Path -LiteralPath $perfRecordingRoot){Get-ChildItem -LiteralPath $perfRecordingRoot -File -Filter 'capture.json' -Recurse|ForEach-Object FullName})
    $perfNewCaptures=@($perfCapturesAfter|Where-Object {$_ -notin $perfCapturesBefore})
    $perfRecord.new_capture_artifacts=$perfNewCaptures
    $perfRecord.capture_artifact_check=if($perfNewCaptures.Count -eq 0){'NO_NEW_CAPTURE_JSON'}else{'FAIL_NEW_CAPTURE_ARTIFACT'}
    if($perfResults.Count -eq 1){
        $perfRecord.native_recording_observed=if($perfResult.status -ceq 'PASS'){'NO_FIRST_FRAME_DETECTED_BY_NATIVE_1HZ_GUARD'}else{'INCONCLUSIVE_SEE_NATIVE_DETAIL'}
        if($perfResult.map -cne $perfData.map -or $perfResult.input_plan.content_schema -cne $perfData.content_schema -or $perfResult.input_plan.selected_hero -cne $perfData.selected_hero -or $perfResult.user_stop_latched -or $perfResult.screenshots_or_recording_requested){$perfRecord.runtime_performance='FAIL'}
    }
    if($perfNewCaptures.Count -ne 0){$perfRecord.runtime_performance='FAIL'}
    $perfRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $perfRecordPath -Encoding utf8
    if($null -eq $perfProcess.ExitCode){throw 'Missing actual process exit code.'}
    if($perfProcess.ExitCode -ne 0){exit $perfProcess.ExitCode}
    if($perfRecord.runtime_performance -cne 'PASS'){exit 2}
    exit 0
}
