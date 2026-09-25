#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$AuthorReport,
    [Parameter(Mandatory=$true)][string]$ReloadReport,
    [switch]$WaitForExit,
    [switch]$PlanOnly
)
$ErrorActionPreference='Stop'
$soleWork='D:/科研学习/codex学习'
$soleDocs=[IO.Path]::GetFullPath((Join-Path $soleWork 'docs/HarborCity_M5_VS2'))
$soleProject=Join-Path $soleWork 'HarborCity/HarborCity.uproject'
$soleEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$soleScript=Join-Path $soleWork 'tools/ue_m5_vs2_gait_sole_author.py'
$soleModule=Join-Path $soleWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
function Read-SoleReport([string]$Value,[string]$Phase){
    $file=[IO.Path]::GetFullPath($Value)
    if(-not $file.StartsWith(($soleDocs+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or
       [IO.Path]::GetFileName($file) -cne 'author_result.json'){throw 'Only stage-owned native author reports are accepted.'}
    $data=Get-Content -LiteralPath $file -Raw -Encoding utf8|ConvertFrom-Json
    $command=Get-Content -LiteralPath (Join-Path (Split-Path $file -Parent) 'commandlet.json') -Raw -Encoding utf8|ConvertFrom-Json
    if($data.schema -cne 'HarborCity.M5VS2.GaitSoleCapture.Author.v1' -or $data.phase -cne $Phase -or $data.status -cne 'PASS' -or
       $command.phase -cne $Phase -or $command.status -cne 'PASS' -or $null -eq $command.exit_code -or $command.exit_code -ne 0 -or
       [IO.Path]::GetFullPath($command.script) -ine [IO.Path]::GetFullPath($soleScript) -or
       $command.script_sha256 -ine (Get-FileHash -LiteralPath $soleScript).Hash){throw 'Require completed native author/reload from the unchanged sole author.'}
    return $data
}
$soleAuthorPath=[IO.Path]::GetFullPath($AuthorReport)
$soleReloadPath=[IO.Path]::GetFullPath($ReloadReport)
$soleAuthor=Read-SoleReport $soleAuthorPath 'GaitSoleCaptureAuthor'
$soleReload=Read-SoleReport $soleReloadPath 'GaitSoleCaptureReload'
if($soleReload.author_report -ine $soleAuthorPath -or $soleReload.author_sha256 -ine (Get-FileHash -LiteralPath $soleAuthorPath).Hash -or
   $soleReload.fresh_process_disk_readback -cne 'PASS' -or $soleAuthor.map -cnotmatch '^/Game/HarborCity/M5VS2/HeroGaitR2/Run_[0-9a-f]{12}/L_HeroGaitR2$' -or
   $soleAuthor.assets.Count -ne 2 -or $soleAuthor.expected_screenshots -ne 0 -or $soleAuthor.selection.points.Count -ne 12){throw 'Exact private two-asset fixed-point fixture/readback missing.'}
$soleParent=$soleAuthor.map -replace '/L_HeroGaitR2$',''
$soleExpected=@($soleAuthor.map,($soleParent+'/M_SoleGroundReference'))|Sort-Object
$soleActual=@($soleAuthor.assets|ForEach-Object{$_.path}|Sort-Object)
if(@(Compare-Object $soleExpected $soleActual).Count -ne 0){throw 'Unexpected authored package membership.'}
foreach($soleAsset in $soleAuthor.assets){
    $soleExt=if($soleAsset.path -ceq $soleAuthor.map){'.umap'}else{'.uasset'}
    $soleFile=Join-Path $soleWork ('HarborCity/Content/'+$soleAsset.path.Substring(6)+$soleExt)
    if((Get-FileHash -LiteralPath $soleFile).Hash -ine $soleAsset.sha256){throw 'Private sole fixture changed.'}
}
foreach($soleData in @($soleAuthor,$soleReload)){
    if(@($soleData.source_preservation.changed).Count -ne 0){throw 'A native author changed protected input.'}
    foreach($soleSource in $soleData.source_preservation.sha256_before.PSObject.Properties){
        if((Get-FileHash -LiteralPath $soleSource.Name).Hash -ine $soleSource.Value){throw ('Protected source changed: '+$soleSource.Name)}
    }
    if($soleData.module_sha256 -ine (Get-FileHash -LiteralPath $soleModule).Hash){throw 'Re-author/read back sole fixture using the current native module.'}
}
if((Get-FileHash -LiteralPath $soleAuthor.selection.path).Hash -ine $soleAuthor.selection.sha256){throw 'Fixed native shoe-point selection changed.'}
$soleRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$soleOut=Join-Path $soleDocs ('editor_runtime/'+$soleRun+'_gaitsole_game')
$soleSlot='HarborCity_VS2_GaitSole_Test_'+$soleRun
$soleCache='D:/GameDev/Cache/Unreal/HarborCity'
$soleCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2',
    'sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$soleArgs=@(('"'+$soleProject+'"'),$soleAuthor.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
    '-M5VS2HeroGaitR2','-M5VS2HeroGaitUnifiedClock','-M5VS2HeroGaitSoleCapture','-M5VS2RecordSeconds=30','-M5VS2RecordDelay=3','-M5VS2AutoQuit',
    ('-M5VS2EvidenceDir="'+$soleOut+'"'),('-HCM1SaveSlot='+$soleSlot),('-abslog="'+(Join-Path $soleOut 'game.log')+'"'),
    ('-ExecCmds="'+($soleCommands -join ',')+'"'),('-LocalDataCachePath='+$soleCache+'/DDC'),('-ZenDataPath='+$soleCache+'/Zen'))
$soleRecord=[ordered]@{schema='HarborCity.M5VS2.GaitSoleCapture.Launch.v1';phase='GaitSoleCapture';status='PLAN_ONLY';
    kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';executable=$soleEditor;arguments=$soleArgs;map=$soleAuthor.map;
    author_report=$soleAuthorPath;author_sha256=(Get-FileHash -LiteralPath $soleAuthorPath).Hash;
    reload_report=$soleReloadPath;reload_sha256=(Get-FileHash -LiteralPath $soleReloadPath).Hash;
    module_sha256=(Get-FileHash -LiteralPath $soleModule).Hash;selection=$soleAuthor.selection;actual_save_slot_expected=$soleSlot;
    input_level='NATIVE_FUNCTION_CALLS_NOT_ACTION_OR_OS_INPUT';surface_scope='PRE_MORPH_CLOTH_WPO_CPU_LBS_NOT_FINAL_GPU';
    visual_acceptance='USER_REVIEW';comparison_scope='INDEPENDENT_RECORDING_FIXTURE_NOT_IDENTICAL_BASELINE_SCENE';
    process_id=$null;exit_code=$null;runtime_report=$null;runtime_status='NOT_RUN';stop_latched=$null;native_capture_directory=$null}
if($PlanOnly){$soleRecord|ConvertTo-Json -Depth 12;return}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){
    throw 'Finish the existing project engine/game normally before this isolated native recording.'
}
New-Item -ItemType Directory -Path $soleOut|Out-Null
$soleLaunchPath=Join-Path $soleOut 'launch.json'
$soleRecord.status='STARTING';$soleRecord.started_at=(Get-Date).ToString('o')
$soleRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $soleLaunchPath -Encoding utf8
$soleEnvironment=@{'UE-LocalDataCachePath'=$soleCache+'/DDC';'UE-ZenDataPath'=$soleCache+'/Zen';'UE-ZenSubprocessDataPath'=$soleCache+'/Zen'}
$solePrevious=@{}
try{
    foreach($soleKey in $soleEnvironment.Keys){$solePrevious[$soleKey]=[Environment]::GetEnvironmentVariable($soleKey,'Process');[Environment]::SetEnvironmentVariable($soleKey,$soleEnvironment[$soleKey],'Process')}
    # Visible gameplay is explicitly authorized; root alone executes this launcher.
    $soleProcess=Start-Process -FilePath $soleEditor -ArgumentList $soleArgs -WorkingDirectory (Split-Path $soleProject -Parent) -WindowStyle Normal -PassThru
    $null=$soleProcess.Handle;$soleRecord.process_id=$soleProcess.Id;$soleRecord.status='STARTED'
}catch{$soleRecord.status='FAIL';$soleRecord.error=$_.Exception.Message;throw}
finally{
    foreach($soleKey in $solePrevious.Keys){[Environment]::SetEnvironmentVariable($soleKey,$solePrevious[$soleKey],'Process')}
    $soleRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $soleLaunchPath -Encoding utf8
}
Write-Output ('Native sole recording evidence: '+$soleOut)
if($WaitForExit){
    $soleWatch=[Diagnostics.Stopwatch]::StartNew()
    while(-not $soleProcess.WaitForExit(250)){
        $soleFound=@(Get-ChildItem -LiteralPath $soleOut -Recurse -File -Filter 'hero_exercise_results.json')
        $soleLive=$null
        if($soleFound.Count -eq 1){try{$soleLive=Get-Content -LiteralPath $soleFound[0].FullName -Raw -Encoding utf8|ConvertFrom-Json}catch{}}
        if($soleLive -and $soleLive.user_stop_latched -eq $true){
            $soleRecord.status='USER_STOPPED_PROCESS_LEFT_OPEN';$soleRecord.stop_latched=$true;$soleRecord.runtime_report=$soleFound[0].FullName
            $soleRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $soleLaunchPath -Encoding utf8
            Write-Output 'User stop latched; no restart, forced close or further desktop action.'
            exit 1
        }
        if($soleWatch.Elapsed.TotalSeconds -gt 210){
            $soleRecord.status='FAIL_DEADLINE_PROCESS_LEFT_OPEN';$soleRecord.error='Bounded native capture did not exit; no forced process termination.'
            $soleRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $soleLaunchPath -Encoding utf8
            exit 1
        }
    }
    $soleProcess.Refresh();$soleRecord.exit_code=$soleProcess.ExitCode
    $soleFound=@(Get-ChildItem -LiteralPath $soleOut -Recurse -File -Filter 'hero_exercise_results.json')
    $soleOkay=$false
    if($soleFound.Count -eq 1){
        $soleRuntime=Get-Content -LiteralPath $soleFound[0].FullName -Raw -Encoding utf8|ConvertFrom-Json
        $soleRecord.runtime_report=$soleFound[0].FullName;$soleRecord.runtime_status=$soleRuntime.status;$soleRecord.stop_latched=$soleRuntime.user_stop_latched
        $soleEvidence=$soleRuntime.sole_capture
        $soleRecord.native_capture_directory=$soleEvidence.recording_directory
        $soleCapture=$soleEvidence.native_capture_report
        $soleOkay=$soleProcess.ExitCode -eq 0 -and $soleRuntime.status -ceq 'PASS' -and $soleRuntime.user_stop_latched -eq $false -and
            $soleRuntime.captures.Count -eq 0 -and $soleEvidence.cpu_surface_samples.Count -gt 100 -and $soleEvidence.cpu_surface_samples.Count -le 900 -and
            $soleEvidence.native_audio_export_pending -eq $false -and $soleEvidence.native_audio_export_finished_observed -eq $true -and
            $soleCapture.status -ceq 'CAPTURED_PENDING_REVIEW' -and $soleCapture.stop_reason -ceq 'vs2_gameplay_sequence_finished' -and
            $soleCapture.map -ceq $soleAuthor.map -and $soleCapture.actual_save_slot -ceq $soleSlot -and $soleCapture.requested_save_slot -ceq $soleSlot -and
            $soleCapture.user_stop_latched -eq $false -and $soleCapture.capture_geometry_valid -eq $true -and $soleCapture.write_failures -eq 0 -and
            $soleCapture.width -eq 1920 -and $soleCapture.height -eq 1080 -and $soleCapture.wall_seconds -ge 20 -and $soleCapture.wall_seconds -le 30
        if($soleOkay){
            $soleCaptureDir=[IO.Path]::GetFullPath($soleEvidence.recording_directory)
            if(-not $soleCaptureDir.StartsWith(([IO.Path]::GetFullPath((Join-Path $soleDocs 'recordings'))+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){
                throw 'Native capture directory escaped the owned recordings root.'
            }
            $soleDisk=Get-Content -LiteralPath (Join-Path $soleCaptureDir 'capture.json') -Raw -Encoding utf8|ConvertFrom-Json
            $soleOkay=$soleDisk.frames.Count -eq $soleCapture.frames.Count -and $soleDisk.stop_reason -ceq $soleCapture.stop_reason -and
                (Get-Item -LiteralPath (Join-Path $soleCaptureDir 'game_audio.wav')).Length -gt 44
        }
    }
    $soleRecord.status=if($soleOkay){'PASS_NATIVE_CAPTURE_PENDING_VISUAL_REVIEW'}else{'FAIL_OR_STOPPED'}
    $soleRecord.ended_at=(Get-Date).ToString('o')
    $soleRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $soleLaunchPath -Encoding utf8
    if(-not $soleOkay){exit 1}
}
