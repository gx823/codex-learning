#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$AuthorReport,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$gaitWork=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$gaitDocs=Join-Path $gaitWork 'docs/HarborCity_M5_VS2'
$gaitReport=[IO.Path]::GetFullPath($AuthorReport)
if(-not $gaitReport.StartsWith(([IO.Path]::GetFullPath($gaitDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($gaitReport) -cne 'author_result.json'){
    throw 'Require owned HeroGaitR2Author report.'
}
$gaitData=Get-Content -LiteralPath $gaitReport -Raw -Encoding utf8|ConvertFrom-Json
$gaitCommandFile=Join-Path (Split-Path $gaitReport -Parent) 'commandlet.json'
$gaitCommand=Get-Content -LiteralPath $gaitCommandFile -Raw -Encoding utf8|ConvertFrom-Json
$gaitScript=Join-Path $PSScriptRoot 'ue_m5_vs2_hero_gait_r2_author.py'
if($gaitData.schema -cne 'HarborCity.M5VS2.HeroGaitR2.Author.v1' -or $gaitData.phase -cne 'HeroGaitR2Author' -or $gaitData.status -cne 'PASS' -or
   $gaitCommand.phase -cne 'HeroGaitR2Author' -or $gaitCommand.status -cne 'PASS' -or $gaitCommand.exit_code -ne 0 -or
   $gaitData.map -cnotmatch '^/Game/HarborCity/M5VS2/HeroGaitR2/Run_[0-9a-f]{12}/L_HeroGaitR2$' -or $gaitData.assets.Count -ne 2 -or $gaitData.expected_screenshots -ne 9){
    throw 'Require completed unique two-package R2 gait fixture with nine planned captures.'
}
if([IO.Path]::GetFullPath($gaitCommand.script) -ine $gaitScript -or (Get-FileHash -LiteralPath $gaitScript).Hash -ine $gaitCommand.script_sha256){throw 'Author source changed.'}
$gaitParent=$gaitData.map -replace '/L_HeroGaitR2$',''
$gaitExpected=@($gaitData.map,($gaitParent+'/BP_HeroGaitR2GameMode'))|Sort-Object
$gaitPaths=@($gaitData.assets|ForEach-Object{$_.path}|Sort-Object)
if(@(Compare-Object $gaitExpected $gaitPaths).Count -ne 0){throw 'Unexpected private asset membership.'}
foreach($gaitAsset in $gaitData.assets){
    $gaitExtension=if($gaitAsset.path -ceq $gaitData.map){'.umap'}else{'.uasset'}
    $gaitFile=Join-Path $gaitWork ('HarborCity/Content/'+$gaitAsset.path.Substring(6)+$gaitExtension)
    if((Get-FileHash -LiteralPath $gaitFile).Hash -ine $gaitAsset.sha256){throw 'Private fixture changed.'}
}
if($gaitData.source_preservation.changed.Count -ne 0){throw 'Author changed a protected source.'}
foreach($gaitInput in $gaitData.source_preservation.sha256_before.PSObject.Properties){
    if((Get-FileHash -LiteralPath $gaitInput.Name).Hash -ine $gaitInput.Value){throw ('Protected source changed: '+$gaitInput.Name)}
}
if((Get-FileHash -LiteralPath $gaitData.source.selection).Hash -ine $gaitData.source.selection_sha256){throw 'Current Hero review selection changed.'}
$gaitModule=Join-Path $gaitWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
if((Get-FileHash -LiteralPath $gaitModule).Hash -ine $gaitData.module_sha256){throw 'Re-author fixture after changing native module.'}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){
    throw 'Finish the existing engine/game normally before starting this bounded game.'
}
$gaitProject=Join-Path $gaitWork 'HarborCity/HarborCity.uproject'
$gaitExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$gaitRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$gaitOut=Join-Path $gaitDocs ('editor_runtime/'+$gaitRun+'_herogait_r2_game')
New-Item -ItemType Directory -Path $gaitOut|Out-Null
$gaitCache='D:/GameDev/Cache/Unreal/HarborCity'
$gaitCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$gaitArgs=@(('"'+$gaitProject+'"'),$gaitData.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
    '-M5VS2HeroGaitR2','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$gaitOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_HeroGaitR2_'+$gaitRun),
    ('-abslog="'+(Join-Path $gaitOut 'game.log')+'"'),('-ExecCmds="'+($gaitCommands -join ',')+'"'),"-LocalDataCachePath=$gaitCache/DDC","-ZenDataPath=$gaitCache/Zen")
$gaitRecord=[ordered]@{milestone='M5_VS2';phase='HeroGaitR2';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';
    started_at=(Get-Date).ToString('o');executable=$gaitExe;arguments=$gaitArgs;map=$gaitData.map;author_report=$gaitReport;
    author_sha256=(Get-FileHash -LiteralPath $gaitReport).Hash;module_sha256=(Get-FileHash -LiteralPath $gaitModule).Hash;
    input_level='NATIVE_FUNCTION_CALLS_NOT_ACTION_OR_OS_INPUT';visual_acceptance='USER_REVIEW';sliding_improvement='NOT_RUN';
    process_id=$null;exit_code=$null;runtime_status='NOT_RUN';stop_latched=$null}
$gaitRecordPath=Join-Path $gaitOut 'launch.json'
$gaitRecord|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $gaitRecordPath -Encoding utf8
$gaitEnv=@{'UE-LocalDataCachePath'="$gaitCache/DDC";'UE-ZenDataPath'="$gaitCache/Zen";'UE-ZenSubprocessDataPath'="$gaitCache/Zen"}
$gaitOld=@{}
try{
    foreach($gaitKey in $gaitEnv.Keys){$gaitOld[$gaitKey]=[Environment]::GetEnvironmentVariable($gaitKey,'Process');[Environment]::SetEnvironmentVariable($gaitKey,$gaitEnv[$gaitKey],'Process')}
    # Explicitly authorized visible, bounded game diagnostic; Escape keeps the native stop latch.
    $gaitProcess=Start-Process -FilePath $gaitExe -ArgumentList $gaitArgs -WorkingDirectory (Split-Path $gaitProject -Parent) -WindowStyle Normal -PassThru
    $null=$gaitProcess.Handle;$gaitRecord.process_id=$gaitProcess.Id;$gaitRecord.status='STARTED'
}catch{$gaitRecord.status='FAIL';$gaitRecord.error=$_.Exception.Message;throw}
finally{
    foreach($gaitKey in $gaitOld.Keys){[Environment]::SetEnvironmentVariable($gaitKey,$gaitOld[$gaitKey],'Process')}
    $gaitRecord|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $gaitRecordPath -Encoding utf8
}
Write-Output "HeroGait R2 evidence: $gaitOut"
if($WaitForExit){
    $gaitProcess.WaitForExit();$gaitProcess.Refresh();$gaitRecord.exit_code=$gaitProcess.ExitCode
    $gaitResults=@(Get-ChildItem -LiteralPath $gaitOut -Recurse -File -Filter 'hero_exercise_results.json')
    if($gaitResults.Count -eq 1){
        $gaitRuntime=Get-Content -LiteralPath $gaitResults[0].FullName -Raw -Encoding utf8|ConvertFrom-Json
        $gaitRecord.runtime_status=$gaitRuntime.status;$gaitRecord.stop_latched=$gaitRuntime.user_stop_latched
        $gaitRecord.runtime_report=$gaitResults[0].FullName
        $gaitRecord.sliding_improvement='MEASURED_RESIDUALS_REQUIRE_REVIEW'
        $gaitRecord.capture_count=$gaitRuntime.captures.Count
    }else{$gaitRecord.runtime_status='MISSING_OR_AMBIGUOUS'}
    $gaitRecord.status=if($gaitProcess.ExitCode -eq 0 -and $gaitRecord.runtime_status -ceq 'PASS' -and $gaitRecord.stop_latched -eq $false -and
        $gaitRuntime.exercise_scope -ceq 'R2_INTEGRATED_GAS_WARP_FINAL_TOE_MEASUREMENT' -and $gaitRuntime.captures.Count -eq 9){'PASS'}else{'FAIL_OR_STOPPED'}
    $gaitRecord.ended_at=(Get-Date).ToString('o');$gaitRecord|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $gaitRecordPath -Encoding utf8
    if($gaitRecord.status -ne 'PASS'){exit 1}
}
