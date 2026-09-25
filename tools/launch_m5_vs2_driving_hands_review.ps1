#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Manifest,[ValidateSet('Afternoon','Night')][string]$Period='Afternoon',[string]$SourceRevision,[switch]$ValidateOnly,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$handsWorkspace=Split-Path $PSScriptRoot -Parent
$handsDocs=[IO.Path]::GetFullPath((Join-Path $handsWorkspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
$handsManifest=[IO.Path]::GetFullPath($Manifest)
if(-not $handsManifest.StartsWith($handsDocs,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($handsManifest) -cne 'driving_hands_review_manifest.json' -or $handsManifest.Contains('"') -or -not(Test-Path -LiteralPath $handsManifest -PathType Leaf)){throw 'Requires actual private native Reload manifest. Deploy staged launcher into tools first.'}
$handsData=Get-Content -LiteralPath $handsManifest -Raw|ConvertFrom-Json
$handsMap=$handsData.maps.$Period
if($handsData.schema -cne 'HarborCity.M5VS2.DrivingHandsReview.Author.v1' -or $handsData.status -cne 'READY_FOR_DRIVING_HANDS_NOT_RUNTIME_TESTED' -or $handsData.owner -cne 'HarborCity_M5VS2_DrivingHandsReview' -or $handsData.kind -cne 'EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED' -or $handsData.input_scope -cne 'ENGINE_KEYS_MOUSE_NOT_OS' -or $handsData.camera_scope -cne 'NORMAL_PLAYER_CAMERA_NO_POSE_SETTERS' -or $handsData.expected_screenshots -ne 9){throw 'Unknown bounded driving-hands-review contract.'}
if($handsData.destination -cnotmatch '^/Game/HarborCity/M5VS2/DrivingHandsReview/Run_[0-9a-f]{12}$' -or $handsMap -cne ($handsData.destination+'/L_DrivingHands_'+$Period) -or @($handsData.maps.PSObject.Properties.Name).Count -ne 2 -or $handsData.save_slot_prefix -cne 'HarborCity_VS2_DrivingHands_Test_' -or $handsData.runtime_flag -cne '-M5VS2DrivingHandsReview'){throw 'Invalid private map or input flag.'}
$handsReloadPath=Join-Path (Split-Path $handsManifest -Parent) 'author_result.json'
if([IO.Path]::GetFullPath($handsData.reload_report) -ine [IO.Path]::GetFullPath($handsReloadPath)){throw 'Reload report must accompany own manifest.'}
$handsReload=Get-Content -LiteralPath $handsReloadPath -Raw|ConvertFrom-Json
$handsCommand=Get-Content -LiteralPath (Join-Path (Split-Path $handsManifest -Parent) 'commandlet.json') -Raw|ConvertFrom-Json
if($handsReload.status -cne 'PASS' -or $handsReload.phase -cne 'DrivingHandsReviewReload' -or $handsReload.schema -cne $handsData.schema -or $handsReload.fresh_process_disk_readback -cne 'PASS' -or @($handsReload.preservation.changed_files).Count -ne 0 -or @($handsReload.checks|Where-Object status -CNE 'PASS').Count -ne 0 -or $handsCommand.status -cne 'PASS' -or $handsCommand.exit_code -ne 0){throw 'Fresh native Reload and actual commandlet have not passed.'}
if($handsReload.launch_manifest.sha256 -ine (Get-FileHash -LiteralPath $handsManifest -Algorithm SHA256).Hash -or $handsReload.launch_manifest.bytes -ne (Get-Item -LiteralPath $handsManifest).Length){throw 'Manifest differs from native Reload evidence.'}
$handsBindings=@($handsData.source_bindings)
. (Join-Path $PSScriptRoot 'm5_runtime_revision.ps1')
$handsRevision=Get-M5RuntimeRevision $SourceRevision $handsManifest @(
    $PSCommandPath,
    (Join-Path $handsWorkspace 'HarborCity/Source/HarborCity/M5VS2/HCM5VS2DrivingHandsComponent.cpp'),
    (Join-Path $handsWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'))
if($handsBindings.Count -lt 15){throw 'Incomplete source proof.'}
foreach($handsSource in $handsBindings){
    $handsFile=[IO.Path]::GetFullPath($handsSource.path)
    $handsExpected=Get-M5EffectiveBinding $handsSource $handsRevision
    $handsAllowedRoot=switch($handsSource.kind){
        'package'{Join-Path $handsWorkspace 'HarborCity/Content'}
        'module'{Join-Path $handsWorkspace 'HarborCity/Binaries/Win64'}
        'cpp'{Join-Path $handsWorkspace 'HarborCity/Source'}
        'config'{Join-Path $handsWorkspace 'HarborCity/Config'}
        'script'{$PSScriptRoot}
        'evidence'{$handsDocs}
        'backup'{'E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups'}
        default{throw 'Unknown source binding kind.'}
    }
    $handsAllowedRoot=[IO.Path]::GetFullPath($handsAllowedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $handsFile.StartsWith($handsAllowedRoot,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $handsFile -PathType Leaf) -or (Get-Item -LiteralPath $handsFile).Length -ne $handsExpected.bytes -or (Get-FileHash -LiteralPath $handsFile -Algorithm SHA256).Hash -ine $handsExpected.sha256){throw "Source changed or escaped allowed root: $handsFile"}
}
$handsProject=Join-Path $handsWorkspace 'HarborCity/HarborCity.uproject'
$handsEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$handsModule=Join-Path $handsWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$handsMapFile=Join-Path (Join-Path $handsWorkspace 'HarborCity/Content') ($handsMap.Substring(6)+'.umap')
foreach($handsFile in @($handsProject,$handsEditor,$handsModule,$handsMapFile)){if(-not(Test-Path -LiteralPath $handsFile -PathType Leaf)){throw "Missing $handsFile"}}
if(@($handsBindings|Where-Object {[IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($handsMapFile)}).Count -ne 1 -or @($handsBindings|Where-Object kind -CEQ 'module').Count -ne 1){throw 'Missing exact map/module evidence.'}
foreach($handsClass in @('HCM5VS2DrivingHandsComponent','HCM5VS2DrivingHandsReviewDirector','HCM5VS2HeroModestyComponent','HCM5VS2CornerTimeDirector')){foreach($handsExt in @('.h','.cpp')){
    $handsCpp=Join-Path $handsWorkspace ('HarborCity/Source/HarborCity/M5VS2/'+$handsClass+$handsExt)
    if(@($handsBindings|Where-Object {$_.kind -ceq 'cpp' -and [IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($handsCpp)}).Count -ne 1){throw 'Missing exact native candidate/review source proof.'}
}}
if($ValidateOnly){[ordered]@{status='PASS_STATIC_LAUNCH_GUARDS_ONLY';kind=$handsData.kind;manifest=$handsManifest;map=$handsMap;runtime='NOT_RUN';input_scope=$handsData.input_scope;camera_scope=$handsData.camera_scope;visual_acceptance='USER_REVIEW_NOT_PROVEN_BY_CAPTURE_COUNT'}|ConvertTo-Json;return}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue){throw 'Close existing engine/game normally before one bounded review.'}
$handsRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
# Production perspective preferences isolate only names containing _Test_.
# Preserve the existing accepted prefix while avoiding the user's global Views slot.
$handsSlot='HarborCity_VS2_DrivingHands_Test_'+$handsRun+'_'+$Period
$handsOut=Join-Path $handsDocs ('editor_runtime/'+$handsRun+'_driving_hands_game')
New-Item -ItemType Directory -Path $handsOut|Out-Null
$handsCache='D:/GameDev/Cache/Unreal/HarborCity'
$handsCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$handsArgs=@(('"'+$handsProject+'"'),$handsMap,'-game','-fullcrashdump','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2DrivingHandsReview','-M5VS2AutoQuit',('-HCM1SaveSlot='+$handsSlot),('-M5VS2EvidenceDir="'+$handsOut+'"'),('-abslog="'+(Join-Path $handsOut 'game.log')+'"'),('-ExecCmds="'+($handsCommands -join ',')+'"'),"-LocalDataCachePath=$handsCache/DDC","-ZenDataPath=$handsCache/Zen")
$handsRecord=[ordered]@{schema='HarborCity.M5VS2.DrivingHandsReview.Launch.v1';status='STARTING';started_at=(Get-Date).ToString('o');kind=$handsData.kind;input_scope=$handsData.input_scope;camera_scope=$handsData.camera_scope;executable=$handsEditor;map=$handsMap;period=$Period;save_slot=$handsSlot;manifest=$handsManifest;manifest_sha256=(Get-FileHash -LiteralPath $handsManifest -Algorithm SHA256).Hash;reload=$handsReloadPath;reload_sha256=(Get-FileHash -LiteralPath $handsReloadPath -Algorithm SHA256).Hash;module_sha256=(Get-FileHash -LiteralPath $handsModule -Algorithm SHA256).Hash;arguments=$handsArgs;runtime='NOT_RUN';visual_acceptance='USER_REVIEW';os_mouse='NOT_RUN';flight_dive='NOT_RUN';expected_screenshots=9;autoquit_only_without_user_stop=$true}
$handsRecordFile=Join-Path $handsOut 'launch.json'
if($SourceRevision){$handsRecord.source_revision=[ordered]@{path=[IO.Path]::GetFullPath($SourceRevision);sha256=(Get-FileHash -LiteralPath $SourceRevision).Hash;bindings=@($handsRevision.Values)}}
$handsRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $handsRecordFile -Encoding utf8
$handsEnv=@{'UE-LocalDataCachePath'="$handsCache/DDC";'UE-ZenDataPath'="$handsCache/Zen";'UE-ZenSubprocessDataPath'="$handsCache/Zen"};$handsOld=@{}
try{
    foreach($handsKey in $handsEnv.Keys){$handsOld[$handsKey]=[Environment]::GetEnvironmentVariable($handsKey,'Process');[Environment]::SetEnvironmentVariable($handsKey,$handsEnv[$handsKey],'Process')}
    # Explicitly authorized visible diagnostic game; engine owns bounded input.
    $handsProcess=Start-Process -FilePath $handsEditor -ArgumentList $handsArgs -WorkingDirectory (Split-Path $handsProject -Parent) -WindowStyle Normal -PassThru
    $null=$handsProcess.Handle;$handsRecord.process_id=$handsProcess.Id;$handsRecord.status='STARTED'
}catch{$handsRecord.status='FAIL';$handsRecord.error=$_.Exception.Message;throw}
finally{foreach($handsKey in $handsOld.Keys){[Environment]::SetEnvironmentVariable($handsKey,$handsOld[$handsKey],'Process')};$handsRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $handsRecordFile -Encoding utf8}
Write-Output "ENGINE_KEYS_MOUSE_NOT_OS; normal production camera and engine key/Mouse2D input; editor -game: $handsOut"
Write-Output 'Esc / P / focus loss permanently cancels owned input and autoquit; no retry or focus reacquire.'
if($WaitForExit){
    $handsProcess.WaitForExit();$handsProcess.Refresh();$handsRecord.exit_code=$handsProcess.ExitCode;$handsRecord.ended_at=(Get-Date).ToString('o');$handsRecord.status='EXITED'
    $handsReports=@(Get-ChildItem -LiteralPath $handsOut -Directory -Filter 'DrivingHandsReview_*'|ForEach-Object {Join-Path $_.FullName 'driving_hands_review.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    if($handsReports.Count -eq 1){
        $handsResult=Get-Content -LiteralPath $handsReports[0] -Raw|ConvertFrom-Json
        $handsRecord.runtime=$handsResult.status;$handsRecord.result_file=$handsReports[0];$handsRecord.result_sha256=(Get-FileHash -LiteralPath $handsReports[0] -Algorithm SHA256).Hash
        $handsRecord.pngs=@($handsResult.captures|ForEach-Object {if(Test-Path -LiteralPath $_.file -PathType Leaf){[ordered]@{path=$_.file;bytes=(Get-Item -LiteralPath $_.file).Length;sha256=(Get-FileHash -LiteralPath $_.file -Algorithm SHA256).Hash;label=$_.label;stage=$_.stage}}})
    }
    $handsRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $handsRecordFile -Encoding utf8
    if($null -eq $handsProcess.ExitCode){throw 'Missing actual process exit code.'}
    if($handsProcess.ExitCode -eq 0 -and ($handsRecord.runtime -cne 'PASS_CAPTURE_ONLY' -or $handsResult.stop_latched -ne $false -or @($handsRecord.pngs).Count -ne 9)){throw 'Exit0 is not proof of a completed driving-hands review; inspect actual report/log.'}
    exit $handsProcess.ExitCode
}
