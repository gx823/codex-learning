#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Manifest,[switch]$ValidateOnly,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$clothWorkspace=Split-Path $PSScriptRoot -Parent
$clothDocs=[IO.Path]::GetFullPath((Join-Path $clothWorkspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
$clothManifest=[IO.Path]::GetFullPath($Manifest)
if(-not $clothManifest.StartsWith($clothDocs,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($clothManifest) -cne 'hero_modesty_review_manifest.json' -or $clothManifest.Contains('"') -or -not(Test-Path -LiteralPath $clothManifest -PathType Leaf)){throw 'Requires actual private native Reload manifest. Deploy staged launcher into tools first.'}
$clothData=Get-Content -LiteralPath $clothManifest -Raw|ConvertFrom-Json
if($clothData.schema -cne 'HarborCity.M5VS2.HeroModestyReview.Author.v1' -or $clothData.status -cne 'READY_FOR_COVERAGE_CAPTURE_NOT_RUNTIME_TESTED' -or $clothData.owner -cne 'HarborCity_M5VS2_HeroModestyReview' -or $clothData.kind -cne 'EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED' -or $clothData.input_scope -cne 'ENGINE_SPACE_KEY_NOT_OS' -or $clothData.camera_scope -cne 'FOUR_FIXED_DIAGNOSTIC_CAMERAS_NOT_PLAYER_MOUSE' -or $clothData.expected_screenshots -ne 8){throw 'Unknown bounded clothing-review contract.'}
if($clothData.destination -cnotmatch '^/Game/HarborCity/M5VS2/HeroModestyReview/Run_[0-9a-f]{12}$' -or $clothData.map -cne ($clothData.destination+'/L_HeroModestyReview') -or $clothData.save_slot_prefix -cne 'HarborCity_VS2_Modesty_' -or $clothData.runtime_flag -cne '-M5VS2HeroModestyReview'){throw 'Invalid private map or input flag.'}
$clothReloadPath=Join-Path (Split-Path $clothManifest -Parent) 'author_result.json'
if([IO.Path]::GetFullPath($clothData.reload_report) -ine [IO.Path]::GetFullPath($clothReloadPath)){throw 'Reload report must accompany own manifest.'}
$clothReload=Get-Content -LiteralPath $clothReloadPath -Raw|ConvertFrom-Json
$clothCommand=Get-Content -LiteralPath (Join-Path (Split-Path $clothManifest -Parent) 'commandlet.json') -Raw|ConvertFrom-Json
if($clothReload.status -cne 'PASS' -or $clothReload.phase -cne 'HeroModestyReviewReload' -or $clothReload.schema -cne $clothData.schema -or $clothReload.fresh_process_disk_readback -cne 'PASS' -or @($clothReload.preservation.changed_files).Count -ne 0 -or @($clothReload.checks|Where-Object status -CNE 'PASS').Count -ne 0 -or $clothCommand.status -cne 'PASS' -or $clothCommand.exit_code -ne 0){throw 'Fresh native Reload and actual commandlet have not passed.'}
if($clothReload.launch_manifest.sha256 -ine (Get-FileHash -LiteralPath $clothManifest -Algorithm SHA256).Hash -or $clothReload.launch_manifest.bytes -ne (Get-Item -LiteralPath $clothManifest).Length){throw 'Manifest differs from native Reload evidence.'}
$clothBindings=@($clothData.source_bindings)
if($clothBindings.Count -lt 15){throw 'Incomplete source proof.'}
foreach($clothSource in $clothBindings){
    $clothFile=[IO.Path]::GetFullPath($clothSource.path)
    $clothAllowedRoot=switch($clothSource.kind){
        'package'{Join-Path $clothWorkspace 'HarborCity/Content'}
        'module'{Join-Path $clothWorkspace 'HarborCity/Binaries/Win64'}
        'cpp'{Join-Path $clothWorkspace 'HarborCity/Source'}
        'config'{Join-Path $clothWorkspace 'HarborCity/Config'}
        'script'{$PSScriptRoot}
        'evidence'{$clothDocs}
        default{throw 'Unknown source binding kind.'}
    }
    $clothAllowedRoot=[IO.Path]::GetFullPath($clothAllowedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $clothFile.StartsWith($clothAllowedRoot,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $clothFile -PathType Leaf) -or (Get-Item -LiteralPath $clothFile).Length -ne $clothSource.bytes -or (Get-FileHash -LiteralPath $clothFile -Algorithm SHA256).Hash -ine $clothSource.sha256){throw "Source changed or escaped allowed root: $clothFile"}
}
$clothProject=Join-Path $clothWorkspace 'HarborCity/HarborCity.uproject'
$clothEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$clothModule=Join-Path $clothWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$clothMapFile=Join-Path (Join-Path $clothWorkspace 'HarborCity/Content') ($clothData.map.Substring(6)+'.umap')
foreach($clothFile in @($clothProject,$clothEditor,$clothModule,$clothMapFile)){if(-not(Test-Path -LiteralPath $clothFile -PathType Leaf)){throw "Missing $clothFile"}}
if(@($clothBindings|Where-Object {[IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($clothMapFile)}).Count -ne 1 -or @($clothBindings|Where-Object kind -CEQ 'module').Count -ne 1){throw 'Missing exact map/module evidence.'}
foreach($clothClass in @('HCM5VS2HeroModestyComponent','HCM5VS2HeroModestyReviewDirector')){foreach($clothExt in @('.h','.cpp')){
    $clothCpp=Join-Path $clothWorkspace ('HarborCity/Source/HarborCity/M5VS2/'+$clothClass+$clothExt)
    if(@($clothBindings|Where-Object {$_.kind -ceq 'cpp' -and [IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($clothCpp)}).Count -ne 1){throw 'Missing exact native garment/review source proof.'}
}}
if($ValidateOnly){[ordered]@{status='PASS_STATIC_LAUNCH_GUARDS_ONLY';kind=$clothData.kind;manifest=$clothManifest;map=$clothData.map;runtime='NOT_RUN';input_scope=$clothData.input_scope;camera_scope=$clothData.camera_scope;coverage='USER_REVIEW_NOT_PROVEN_BY_CAPTURE_COUNT'}|ConvertTo-Json;return}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue){throw 'Close existing engine/game normally before one bounded review.'}
$clothRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
# Production perspective preferences isolate only names containing _Test_.
# Preserve the existing accepted prefix while avoiding the user's global Views slot.
$clothSlot='HarborCity_VS2_Modesty_Test_'+$clothRun
$clothOut=Join-Path $clothDocs ('editor_runtime/'+$clothRun+'_hero_modesty_game')
New-Item -ItemType Directory -Path $clothOut|Out-Null
$clothCache='D:/GameDev/Cache/Unreal/HarborCity'
$clothCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$clothArgs=@(('"'+$clothProject+'"'),$clothData.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2HeroModestyReview','-M5VS2AutoQuit',('-HCM1SaveSlot='+$clothSlot),('-M5VS2EvidenceDir="'+$clothOut+'"'),('-abslog="'+(Join-Path $clothOut 'game.log')+'"'),('-ExecCmds="'+($clothCommands -join ',')+'"'),"-LocalDataCachePath=$clothCache/DDC","-ZenDataPath=$clothCache/Zen")
$clothRecord=[ordered]@{schema='HarborCity.M5VS2.HeroModestyReview.Launch.v1';status='STARTING';started_at=(Get-Date).ToString('o');kind=$clothData.kind;input_scope=$clothData.input_scope;camera_scope=$clothData.camera_scope;executable=$clothEditor;map=$clothData.map;save_slot=$clothSlot;manifest=$clothManifest;manifest_sha256=(Get-FileHash -LiteralPath $clothManifest -Algorithm SHA256).Hash;reload=$clothReloadPath;reload_sha256=(Get-FileHash -LiteralPath $clothReloadPath -Algorithm SHA256).Hash;module_sha256=(Get-FileHash -LiteralPath $clothModule -Algorithm SHA256).Hash;arguments=$clothArgs;runtime='NOT_RUN';visual_coverage='USER_REVIEW';os_mouse='NOT_RUN';flight_dive='NOT_RUN';expected_screenshots=8;autoquit_only_without_user_stop=$true}
$clothRecordFile=Join-Path $clothOut 'launch.json'
$clothRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $clothRecordFile -Encoding utf8
$clothEnv=@{'UE-LocalDataCachePath'="$clothCache/DDC";'UE-ZenDataPath'="$clothCache/Zen";'UE-ZenSubprocessDataPath'="$clothCache/Zen"};$clothOld=@{}
try{
    foreach($clothKey in $clothEnv.Keys){$clothOld[$clothKey]=[Environment]::GetEnvironmentVariable($clothKey,'Process');[Environment]::SetEnvironmentVariable($clothKey,$clothEnv[$clothKey],'Process')}
    # Explicitly authorized visible diagnostic game; engine owns bounded input.
    $clothProcess=Start-Process -FilePath $clothEditor -ArgumentList $clothArgs -WorkingDirectory (Split-Path $clothProject -Parent) -WindowStyle Normal -PassThru
    $null=$clothProcess.Handle;$clothRecord.process_id=$clothProcess.Id;$clothRecord.status='STARTED'
}catch{$clothRecord.status='FAIL';$clothRecord.error=$_.Exception.Message;throw}
finally{foreach($clothKey in $clothOld.Keys){[Environment]::SetEnvironmentVariable($clothKey,$clothOld[$clothKey],'Process')};$clothRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $clothRecordFile -Encoding utf8}
Write-Output "ENGINE_SPACE_KEY_NOT_OS; four fixed low diagnostic cameras; editor -game: $clothOut"
Write-Output 'Esc / P / focus loss permanently cancels input, camera changes and autoquit; no retry or focus reacquire.'
if($WaitForExit){
    $clothProcess.WaitForExit();$clothProcess.Refresh();$clothRecord.exit_code=$clothProcess.ExitCode;$clothRecord.ended_at=(Get-Date).ToString('o');$clothRecord.status='EXITED'
    $clothReports=@(Get-ChildItem -LiteralPath $clothOut -Directory -Filter 'HeroModestyReview_*'|ForEach-Object {Join-Path $_.FullName 'modesty_review.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    if($clothReports.Count -eq 1){
        $clothResult=Get-Content -LiteralPath $clothReports[0] -Raw|ConvertFrom-Json
        $clothRecord.runtime=$clothResult.status;$clothRecord.result_file=$clothReports[0];$clothRecord.result_sha256=(Get-FileHash -LiteralPath $clothReports[0] -Algorithm SHA256).Hash
        $clothRecord.pngs=@($clothResult.captures|ForEach-Object {if(Test-Path -LiteralPath $_.file -PathType Leaf){[ordered]@{path=$_.file;bytes=(Get-Item -LiteralPath $_.file).Length;sha256=(Get-FileHash -LiteralPath $_.file -Algorithm SHA256).Hash;pose=$_.pose;view=$_.view}}})
    }
    $clothRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $clothRecordFile -Encoding utf8
    if($null -eq $clothProcess.ExitCode){throw 'Missing actual process exit code.'}
    if($clothProcess.ExitCode -eq 0 -and ($clothRecord.runtime -cne 'PASS_CAPTURE_ONLY' -or $clothResult.stop_latched -ne $false -or @($clothRecord.pngs).Count -ne 8)){throw 'Exit0 is not proof of a completed clothing review; inspect actual report/log.'}
    exit $clothProcess.ExitCode
}
