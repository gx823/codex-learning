#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$AuthorReport,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$hairWork=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$hairDocs=Join-Path $hairWork 'docs/HarborCity_M5_VS2'
$hairReport=[IO.Path]::GetFullPath($AuthorReport)
if(-not $hairReport.StartsWith(([IO.Path]::GetFullPath($hairDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($hairReport) -cne 'author_result.json'){throw 'Require an owned native HairReview author report.'}
$hairData=Get-Content -LiteralPath $hairReport -Raw|ConvertFrom-Json
$hairCommandFile=Join-Path (Split-Path $hairReport -Parent) 'commandlet.json'
$hairCommand=Get-Content -LiteralPath $hairCommandFile -Raw|ConvertFrom-Json
$hairOutline=$hairData.schema -ceq 'HarborCity.M5VS2.HairOutlineReview.Author.v1'
if($hairOutline){
    $hairScript=Join-Path $PSScriptRoot 'ue_m5_vs2_hair_outline_review_author.py'
    if($hairData.phase -cne 'HairOutlineReviewAuthor' -or $hairCommand.phase -cne 'HairOutlineReviewAuthor' -or $hairData.assets.Count -ne 7 -or $hairData.outline_comparison.variants.Count -ne 3){throw 'Require exact private seven-asset outline author.'}
    $hairExpectedNames=@('M_HairOutlineOriginal','M_HairOutlineOff','M_HairOutlineSourceAlpha','BP_HairOutlinePrivateHero','BP_HairReviewGameMode','M_NeutralDiagnostic','L_HairReview')
    $hairNames=@($hairData.assets|ForEach-Object{($_.path -split '/')[-1]}|Sort-Object)
    if(@(Compare-Object ($hairExpectedNames|Sort-Object) $hairNames).Count -ne 0){throw 'Unexpected outline diagnostic asset membership.'}
    if($hairData.private_hero -cne ($hairData.map -replace '/L_HairReview$','/BP_HairOutlinePrivateHero')){throw 'Private Hero is outside its exact fixture.'}
    for($hairIndex=0;$hairIndex -lt 3;$hairIndex++){
        $hairVariantPath=$hairData.map -replace '/L_HairReview$',('/'+$hairExpectedNames[$hairIndex])
        if($hairData.outline_comparison.variants[$hairIndex].material -cne $hairVariantPath){throw 'Outline variants out of order or outside exact fixture.'}
    }
}else{
    if($hairData.schema -cne 'HarborCity.M5VS2.HairReview.Author.v1' -or $hairData.assets.Count -ne 3){throw 'No completed bounded HairReview author.'}
    $hairScript=Join-Path $PSScriptRoot 'ue_m5_vs2_hair_review_author.py'
}
if($hairData.status -cne 'PASS' -or $hairCommand.status -cne 'PASS' -or $hairCommand.exit_code -ne 0 -or $hairData.map -cnotmatch '^/Game/HarborCity/M5VS2/HairReview/Run_[0-9a-f]{12}/L_HairReview$'){throw 'No completed bounded HairReview author.'}
if([IO.Path]::GetFullPath($hairCommand.script) -ine $hairScript -or (Get-FileHash -LiteralPath $hairScript).Hash -ine $hairCommand.script_sha256){throw 'Author source changed.'}
foreach($hairAsset in $hairData.assets){
    $hairPackage=$hairAsset.path
    if($hairPackage -cnotmatch '^/Game/HarborCity/M5VS2/HairReview/Run_[0-9a-f]{12}/[A-Za-z0-9_]+$'){throw 'Unexpected diagnostic package.'}
    if($hairOutline -and ($hairPackage -replace '/[^/]+$','') -cne ($hairData.map -replace '/[^/]+$','')){throw 'Outline package not in exact authored fixture.'}
    $hairExtension=if($hairPackage -ceq $hairData.map){'.umap'}else{'.uasset'}
    $hairFile=Join-Path $hairWork ('HarborCity/Content/'+$hairPackage.Substring(6)+$hairExtension)
    if((Get-FileHash -LiteralPath $hairFile).Hash -ine $hairAsset.sha256){throw 'Diagnostic asset changed.'}
}
if($hairOutline){
    if($hairData.source_preservation.changed.Count -ne 0 -or -not $hairData.source_preservation.sha256_before){throw 'Missing exact source preservation ledger.'}
    foreach($hairSource in $hairData.source_preservation.sha256_before.PSObject.Properties){
        if((Get-FileHash -LiteralPath $hairSource.Name).Hash -ine $hairSource.Value){throw 'Protected outline diagnostic source changed.'}
    }
}
if((Get-FileHash -LiteralPath $hairData.source.report).Hash -ine $hairData.source.report_sha256){throw 'Hero integration source changed.'}
$hairHero=Get-Content -LiteralPath $hairData.source.report -Raw|ConvertFrom-Json
foreach($hairAsset in $hairHero.assets){
    if($hairAsset.path -cnotmatch '^/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/[A-Za-z0-9_]+$'){throw 'Unexpected Hero source package.'}
    $hairFile=Join-Path $hairWork ('HarborCity/Content/'+$hairAsset.path.Substring(6)+'.uasset')
    if((Get-FileHash -LiteralPath $hairFile).Hash -ine $hairAsset.sha256){throw 'Hero asset changed.'}
}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){throw 'Finish the existing engine/game normally first.'}
$hairProject=Join-Path $hairWork 'HarborCity/HarborCity.uproject'
$hairExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$hairModule=Join-Path $hairWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$hairRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$hairOut=Join-Path $hairDocs ('editor_runtime/'+$hairRun+'_hairreview_game')
New-Item -ItemType Directory -Path $hairOut|Out-Null
$hairCache='D:/GameDev/Cache/Unreal/HarborCity'
$hairCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$hairArgs=@(('"'+$hairProject+'"'),$hairData.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2HairReview','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$hairOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_HairReview_'+$hairRun),('-abslog="'+(Join-Path $hairOut 'game.log')+'"'),('-ExecCmds="'+($hairCommands -join ',')+'"'),"-LocalDataCachePath=$hairCache/DDC","-ZenDataPath=$hairCache/Zen")
if($hairOutline){$hairArgs+='-M5VS2HairOutlineReview'}
$hairRecord=[ordered]@{milestone='M5_VS2';phase='HairReview';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';started_at=(Get-Date).ToString('o');executable=$hairExe;arguments=$hairArgs;map=$hairData.map;author_report=$hairReport;author_sha256=(Get-FileHash -LiteralPath $hairReport).Hash;module_sha256=(Get-FileHash -LiteralPath $hairModule).Hash;os_input_used=$false;visual_acceptance='USER_REVIEW';process_id=$null;exit_code=$null;runtime_status='NOT_RUN';stop_latched=$null}
$hairRecord.outline_mask_comparison=$hairOutline
$hairRecordPath=Join-Path $hairOut 'launch.json'
$hairRecord|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $hairRecordPath -Encoding utf8
$hairEnv=@{'UE-LocalDataCachePath'="$hairCache/DDC";'UE-ZenDataPath'="$hairCache/Zen";'UE-ZenSubprocessDataPath'="$hairCache/Zen"};$hairOld=@{}
try{
    foreach($hairKey in $hairEnv.Keys){$hairOld[$hairKey]=[Environment]::GetEnvironmentVariable($hairKey,'Process');[Environment]::SetEnvironmentVariable($hairKey,$hairEnv[$hairKey],'Process')}
    # The user authorized this visible, bounded game-window diagnostic. Esc retains its stop latch.
    $hairProcess=Start-Process -FilePath $hairExe -ArgumentList $hairArgs -WorkingDirectory (Split-Path $hairProject -Parent) -WindowStyle Normal -PassThru
    $null=$hairProcess.Handle;$hairRecord.process_id=$hairProcess.Id;$hairRecord.status='STARTED'
}catch{$hairRecord.status='FAIL';$hairRecord.error=$_.Exception.Message;throw}
finally{
    foreach($hairKey in $hairOld.Keys){[Environment]::SetEnvironmentVariable($hairKey,$hairOld[$hairKey],'Process')}
    $hairRecord|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $hairRecordPath -Encoding utf8
}
Write-Output "Hair diagnostic evidence: $hairOut"
if($WaitForExit){
    $hairProcess.WaitForExit();$hairProcess.Refresh();$hairRecord.exit_code=$hairProcess.ExitCode
    $hairResults=@(Get-ChildItem -LiteralPath $hairOut -Recurse -File -Filter 'hair_review.json')
    if($hairResults.Count -eq 1){
        $hairRuntime=Get-Content -LiteralPath $hairResults[0].FullName -Raw|ConvertFrom-Json;$hairRecord.runtime_status=$hairRuntime.status;$hairRecord.stop_latched=$hairRuntime.user_stop_latched;$hairRecord.runtime_report=$hairResults[0].FullName
        if($hairOutline -and ($hairRuntime.outline_mask_comparison -ne $true -or $hairRuntime.captures.Count -ne 6)){$hairRecord.runtime_status='FAIL_WRONG_OUTLINE_PROFILE'}
        if($hairOutline){foreach($hairCapture in $hairRuntime.captures){if($hairCapture.outline_comparison.status -cne 'PASS' -or $hairCapture.material_readiness.status -cne 'READY'){$hairRecord.runtime_status='FAIL_OUTLINE_INVARIANT_OR_READINESS'}}}
    }
    else{$hairRecord.runtime_status='MISSING_OR_AMBIGUOUS'}
    $hairRecord.status=if($hairProcess.ExitCode -eq 0 -and $hairRecord.runtime_status -ceq 'PASS' -and $hairRecord.stop_latched -eq $false){'PASS'}else{'FAIL_OR_STOPPED'}
    $hairRecord.ended_at=(Get-Date).ToString('o');$hairRecord|ConvertTo-Json -Depth 7|Set-Content -LiteralPath $hairRecordPath -Encoding utf8
    if($hairRecord.status -ne 'PASS'){exit 1}
}
