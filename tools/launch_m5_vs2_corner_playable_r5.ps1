#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Manifest,
    [ValidateSet('Afternoon','Dusk','Night')][string]$Period='Afternoon',
    [ValidateRange(0,120)][int]$RecordSeconds=0,
    [ValidateRange(3,60)][int]$RecordDelay=8,
    [switch]$ValidateOnly,
    [switch]$WaitForExit
)
$ErrorActionPreference='Stop'
$playWorkspace=Split-Path $PSScriptRoot -Parent
$playDocs=[IO.Path]::GetFullPath((Join-Path $playWorkspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
$playManifest=[IO.Path]::GetFullPath($Manifest)
if(-not $playManifest.StartsWith($playDocs,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($playManifest) -cne 'corner_playable_r5_manifest.json' -or $playManifest.Contains('"') -or -not(Test-Path -LiteralPath $playManifest -PathType Leaf)){throw 'Requires the actual private R5 native Reload manifest.'}
if($RecordSeconds -gt 0 -and $RecordSeconds -lt 20){throw 'Native VS2 recorder accepts 20-120 seconds, or use 0 to disable.'}
$playData=Get-Content -LiteralPath $playManifest -Raw|ConvertFrom-Json
if($playData.schema -cne 'HarborCity.M5VS2.CornerPlayableR5.v1' -or $playData.status -cne 'READY_FOR_PLAY_NOT_RUNTIME_TESTED' -or $playData.owner -cne 'HarborCity_M5VS2_CornerPlayableR5' -or $playData.kind -cne 'EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED' -or $playData.expected_npcs -ne 8 -or $playData.expected_vehicles -ne 1){throw 'Unrecognized private playable manifest.'}
if($playData.destination -cnotmatch '^/Game/HarborCity/M5VS2/WorldRev2/PlayableR5_[0-9a-f]{12}$'){throw 'Invalid private namespace.'}
$playMap=$playData.maps.$Period
if($playMap -cne ($playData.destination+'/L_CornerPlayableR5_'+$Period)){throw 'Wrong saved period map.'}
$playReloadPath=Join-Path (Split-Path $playManifest -Parent) 'author_result.json'
if([IO.Path]::GetFullPath($playData.reload_report) -ine [IO.Path]::GetFullPath($playReloadPath)){throw 'Reload report must accompany its own manifest.'}
$playReload=Get-Content -LiteralPath $playReloadPath -Raw|ConvertFrom-Json
$playCommandPath=Join-Path (Split-Path $playManifest -Parent) 'commandlet.json'
$playCommand=Get-Content -LiteralPath $playCommandPath -Raw|ConvertFrom-Json
if($playReload.status -cne 'PASS' -or $playReload.phase -cne 'CornerPlayableR5Reload' -or $playReload.schema -cne $playData.schema -or $playReload.fresh_process_disk_readback -cne 'PASS' -or @($playReload.preservation.changed_files).Count -ne 0 -or @($playReload.checks | Where-Object status -CNE 'PASS').Count -ne 0 -or $playCommand.status -cne 'PASS' -or $playCommand.exit_code -ne 0){throw 'Fresh native Reload/process has not passed.'}
if($playReload.launch_manifest.sha256 -ine (Get-FileHash -LiteralPath $playManifest -Algorithm SHA256).Hash -or $playReload.launch_manifest.bytes -ne (Get-Item -LiteralPath $playManifest).Length){throw 'Manifest differs from actual Reload evidence.'}
if($playData.art_revision -ne 5 -or $playData.source_corner_proof.art_revision -ne 5 -or $playData.hero_safety.status -cne 'NATIVE_RELOAD_SELECTED_NOT_ALL_ANGLE_ART_APPROVED' -or $playData.hero_safety.blueprint -cne $playData.selected_hero -or $playData.selected_hero -cnotmatch '^/Game/HarborCity/M5VS2/HeroModesty/Review_[0-9a-f]{12}/BP_HeroSafetyShorts$'){throw 'R5 source and actual safety-layer Reload proof are mandatory.'}
$playBindings=@($playData.source_bindings)
if($playBindings.Count -lt 30){throw 'Missing native source bindings.'}
foreach($playSource in $playBindings){
    $playFile=[IO.Path]::GetFullPath($playSource.path)
    $playAllowedRoot=switch($playSource.kind){
        'package'{Join-Path $playWorkspace 'HarborCity/Content'}
        'module'{Join-Path $playWorkspace 'HarborCity/Binaries/Win64'}
        'cpp'{Join-Path $playWorkspace 'HarborCity/Source'}
        'config'{Join-Path $playWorkspace 'HarborCity/Config'}
        'backup'{'E:/GameDev/Assets/HarborCity/M5_VS2/NPC/NativeBackups'}
        'script'{$PSScriptRoot}
        'evidence'{$playDocs}
        default{throw 'Unknown source binding kind.'}
    }
    $playAllowedRoot=[IO.Path]::GetFullPath($playAllowedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $playFile.StartsWith($playAllowedRoot,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $playFile -PathType Leaf) -or (Get-Item -LiteralPath $playFile).Length -ne $playSource.bytes -or (Get-FileHash -LiteralPath $playFile -Algorithm SHA256).Hash -ine $playSource.sha256){throw "Private play source changed or escaped allowed root: $playFile"}
}
$playProject=Join-Path $playWorkspace 'HarborCity/HarborCity.uproject'
$playEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$playModule=Join-Path $playWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
foreach($playFile in @($playProject,$playEditor,$playModule)){if(-not(Test-Path -LiteralPath $playFile -PathType Leaf)){throw "Missing $playFile"}}
$playMapFile=Join-Path (Join-Path $playWorkspace 'HarborCity/Content') ($playMap.Substring(6)+'.umap')
if(@($playBindings|Where-Object { [IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($playMapFile) }).Count -ne 1 -or @($playBindings|Where-Object kind -CEQ 'module').Count -ne 1){throw 'Missing exact selected map or module byte proof.'}
$playToken=$playData.destination.Substring($playData.destination.LastIndexOf('_')+1)
# Stable per candidate AND period: exit/reopen preserves this private F5/F9 slot.
$playSlot='HarborCity_VS2_PlayR5_'+$playToken+'_Test_'+$Period
if($playData.save_slot_prefix -cne ('HarborCity_VS2_PlayR5_'+$playToken+'_')){throw 'Invalid isolated save namespace.'}
if($ValidateOnly){
    [ordered]@{status='PASS_STATIC_LAUNCH_GUARDS_ONLY';kind=$playData.kind;manifest=$playManifest;map=$playMap;save_slot=$playSlot;record_seconds=$RecordSeconds;runtime='NOT_RUN';input_injection=$false}|ConvertTo-Json
    return
}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue){throw 'Close existing project engine/game normally before starting this play session.'}
$playRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$playOut=Join-Path $playDocs ($playRun+'_corner_playable_r5_game')
New-Item -ItemType Directory -Path $playOut|Out-Null
$playCache='D:/GameDev/Cache/Unreal/HarborCity'
$playCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$playArgs=@(('"'+$playProject+'"'),$playMap,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',('-HCM1SaveSlot='+$playSlot),('-M5VS2EvidenceDir="'+$playOut+'"'),('-abslog="'+(Join-Path $playOut 'game.log')+'"'),('-ExecCmds="'+($playCommands -join ',')+'"'),"-LocalDataCachePath=$playCache/DDC","-ZenDataPath=$playCache/Zen")
if($RecordSeconds -gt 0){$playArgs+=@(('-M5VS2RecordSeconds='+$RecordSeconds),('-M5VS2RecordDelay='+$RecordDelay))}
$playRecord=[ordered]@{schema='HarborCity.M5VS2.CornerPlayableR5.Launch.v1';status='STARTING';kind=$playData.kind;started_at=(Get-Date).ToString('o');executable=$playEditor;map=$playMap;period=$Period;save_slot=$playSlot;manifest=$playManifest;manifest_sha256=(Get-FileHash -LiteralPath $playManifest -Algorithm SHA256).Hash;reload=$playReloadPath;reload_sha256=(Get-FileHash -LiteralPath $playReloadPath -Algorithm SHA256).Hash;module_sha256=(Get-FileHash -LiteralPath $playModule -Algorithm SHA256).Hash;arguments=$playArgs;scope=$playData.scope;auto_input=$false;auto_camera=$false;auto_quit=$false;os_input_result='NOT_RUN';visual='USER_REVIEW';record_seconds=$RecordSeconds;record_delay=$RecordDelay;recording_directory_root=(Join-Path $playDocs 'recordings');recording_result='NOT_RUN';requested_quality='High';requested_fps_cap=0;requested_vsync=0}
$playRecordPath=Join-Path $playOut 'launch.json'
$playRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $playRecordPath -Encoding utf8
$playEnv=@{'UE-LocalDataCachePath'="$playCache/DDC";'UE-ZenDataPath'="$playCache/Zen";'UE-ZenSubprocessDataPath'="$playCache/Zen"};$playOld=@{}
try{
    foreach($playKey in $playEnv.Keys){$playOld[$playKey]=[Environment]::GetEnvironmentVariable($playKey,'Process');[Environment]::SetEnvironmentVariable($playKey,$playEnv[$playKey],'Process')}
    # Explicitly authorized ordinary visible play window; no automated input.
    $playProcess=Start-Process -FilePath $playEditor -ArgumentList $playArgs -WorkingDirectory (Split-Path $playProject -Parent) -WindowStyle Normal -PassThru
    $null=$playProcess.Handle;$playRecord.process_id=$playProcess.Id;$playRecord.status='STARTED'
}catch{$playRecord.status='FAIL';$playRecord.error=$_.Exception.Message;throw}
finally{
    foreach($playKey in $playOld.Keys){[Environment]::SetEnvironmentVariable($playKey,$playOld[$playKey],'Process')}
    $playRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $playRecordPath -Encoding utf8
}
Write-Output "Ordinary editor -game play session (not packaged): $playOut"
Write-Output "Private reusable save: $playSlot"
if($WaitForExit){
    $playProcess.WaitForExit();$playProcess.Refresh();$playRecord.exit_code=$playProcess.ExitCode;$playRecord.ended_at=(Get-Date).ToString('o');$playRecord.status='EXITED'
    $playRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $playRecordPath -Encoding utf8
    if($null -eq $playProcess.ExitCode){throw 'Missing actual process exit code.'};exit $playProcess.ExitCode
}
