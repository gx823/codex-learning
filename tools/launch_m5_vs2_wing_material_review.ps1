#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$AuthorReport,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$wingWork=[IO.Path]::GetFullPath('D:/科研学习/codex学习')
$wingDocs=Join-Path $wingWork 'docs/HarborCity_M5_VS2'
$wingProject=Join-Path $wingWork 'HarborCity/HarborCity.uproject'
$wingScript=Join-Path $wingWork 'tools/ue_m5_vs2_wing_material_review_author.py'
function Require-Wing([bool]$Condition,[string]$Detail){if(-not $Condition){throw $Detail}}
function Read-WingSharedJSON([string]$File){
    $stream=[IO.FileStream]::new($File,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $reader=$null
    try{$reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true);return ($reader.ReadToEnd()|ConvertFrom-Json)}
    finally{if($reader){$reader.Dispose()}else{$stream.Dispose()}}
}
function Assert-WingHash([string]$File,[string]$Expected){
    $full=[IO.Path]::GetFullPath($File)
    $allowed=$full.StartsWith(($wingWork+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or $full.StartsWith('E:\GameDev\Assets\HarborCity\M5_VS2\NPC\NativeBackups\',[StringComparison]::OrdinalIgnoreCase)
    Require-Wing ($allowed -and (Test-Path -LiteralPath $full -PathType Leaf)) 'Protected source is outside owned scope or absent.'
    Require-Wing ((Get-FileHash -LiteralPath $full).Hash -ieq $Expected) ('Protected source changed: '+$full)
}
$wingReport=[IO.Path]::GetFullPath($AuthorReport)
Require-Wing ($wingReport.StartsWith(([IO.Path]::GetFullPath($wingDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($wingReport) -ceq 'author_result.json') 'Require owned native author evidence.'
$wingData=Get-Content -LiteralPath $wingReport -Raw|ConvertFrom-Json
$wingCommandFile=Join-Path (Split-Path $wingReport -Parent) 'commandlet.json'
$wingCommand=Get-Content -LiteralPath $wingCommandFile -Raw|ConvertFrom-Json
Require-Wing ($wingData.schema -ceq 'HarborCity.M5VS2.WingMaterialReview.Author.v1' -and $wingData.phase -ceq 'WingMaterialReviewAuthor' -and $wingData.status -ceq 'PASS' -and $wingCommand.phase -ceq 'WingMaterialReviewAuthor' -and $wingCommand.status -ceq 'PASS' -and $wingCommand.exit_code -eq 0) 'Completed native fixture author and exit 0 required.'
Require-Wing (@($wingData.checks|Where-Object status -cne 'PASS').Count -eq 0 -and $wingData.expected_png_count -eq 4 -and $wingData.old_asset_writes -eq 0) 'Author guards or four-image contract failed.'
Require-Wing ([IO.Path]::GetFullPath($wingCommand.script) -ieq $wingScript) 'Exact deployed fixture author required.'
Assert-WingHash $wingScript $wingCommand.script_sha256
Require-Wing ($wingData.map -cmatch '^/Game/HarborCity/M5VS2/FlightVisual/Review_[0-9a-f]{12}/L_WingMaterialReview$') 'Exact owned fixture map required.'
$wingNamespace=$wingData.map -replace '/L_WingMaterialReview$',''
$wingExpectedAssets=@($wingData.map,($wingNamespace+'/BP_WingMaterialReviewGameMode'))|Sort-Object
Require-Wing ($wingData.assets.Count -eq 2 -and @(Compare-Object $wingExpectedAssets @($wingData.assets.path|Sort-Object)).Count -eq 0) 'Only the exact independent world and GM may be authored.'
foreach($wingAsset in $wingData.assets){
    $wingExtension=if($wingAsset.path -ceq $wingData.map){'.umap'}else{'.uasset'}
    Assert-WingHash (Join-Path $wingWork ('HarborCity/Content/'+$wingAsset.path.Substring(6)+$wingExtension)) $wingAsset.sha256
}
Require-Wing ($wingData.preservation.changed_source_files.Count -eq 0 -and @($wingData.preservation.source_sha256_before.PSObject.Properties).Count -gt 20) 'Missing unchanged native source ledger.'
foreach($wingSource in $wingData.preservation.source_sha256_before.PSObject.Properties){Assert-WingHash $wingSource.Name $wingSource.Value}
Require-Wing ($wingData.project_definition.sha256 -and $wingData.candidate_blueprint -cmatch '^/Game/HarborCity/M5VS2/FlightVisual/MaterialReview_[0-9a-f]{12}/BP_HeroWingSoftLayers$' -and $wingData.source_blueprint -cmatch '^/Game/HarborCity/M5VS2/HeroModesty/Review_[0-9a-f]{12}/BP_HeroSafetyShorts$') 'Actual safety-layer material A/B lineage missing.'
Assert-WingHash $wingProject $wingData.project_definition.sha256
foreach($wingModuleName in @('HarborCity','HarborCityEditor')){
    $wingModule=[IO.Path]::GetFullPath((Join-Path $wingWork ('HarborCity/Binaries/Win64/UnrealEditor-'+$wingModuleName+'.dll')))
    $wingBinding=@($wingData.source_bindings|Where-Object{ $_.kind -ceq 'module' -and [IO.Path]::GetFullPath($_.path) -ieq $wingModule })
    Require-Wing ($wingBinding.Count -eq 1) 'Current compiled module binding missing or ambiguous.'
    Assert-WingHash $wingModule $wingBinding[0].sha256
}
Require-Wing (-not (Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'")) 'Finish existing engine/game normally first.'
$wingExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$wingRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$wingOut=Join-Path $wingDocs ('editor_runtime/'+$wingRun+'_wing_material_review_game')
New-Item -ItemType Directory -Path $wingOut|Out-Null
$wingCache='D:/GameDev/Cache/Unreal/HarborCity'
$wingCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$wingArgs=@(('"'+$wingProject+'"'),$wingData.map,'-game','-fullcrashdump','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2WingMaterialReview','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$wingOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_WingMaterialReview_'+$wingRun),('-abslog="'+(Join-Path $wingOut 'game.log')+'"'),('-ExecCmds="'+($wingCommands -join ',')+'"'),"-LocalDataCachePath=$wingCache/DDC","-ZenDataPath=$wingCache/Zen")
$wingRecord=[ordered]@{milestone='M5_VS2';phase='WingMaterialReview';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';scope='FUNCTION_CALL_VISUAL_FIXTURE';status='STARTING';started_at=(Get-Date).ToString('o');executable=$wingExe;arguments=$wingArgs;map=$wingData.map;author_report=$wingReport;author_sha256=(Get-FileHash -LiteralPath $wingReport).Hash;launcher_sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;os_input_used=$false;art='USER_REVIEW';process_id=$null;exit_code=$null;runtime_status='NOT_RUN';stop_latched=$null}
$wingRecordPath=Join-Path $wingOut 'launch.json'
$wingRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $wingRecordPath -Encoding utf8
$wingEnv=@{'UE-LocalDataCachePath'="$wingCache/DDC";'UE-ZenDataPath'="$wingCache/Zen";'UE-ZenSubprocessDataPath'="$wingCache/Zen"};$wingOld=@{}
try{
    foreach($wingKey in $wingEnv.Keys){$wingOld[$wingKey]=[Environment]::GetEnvironmentVariable($wingKey,'Process');[Environment]::SetEnvironmentVariable($wingKey,$wingEnv[$wingKey],'Process')}
    # User authorized this visible, bounded game window; Esc/P/focus stop stays in the native fixture.
    $wingProcess=Start-Process -FilePath $wingExe -ArgumentList $wingArgs -WorkingDirectory (Split-Path $wingProject -Parent) -WindowStyle Normal -PassThru
    $null=$wingProcess.Handle;$wingRecord.process_id=$wingProcess.Id;$wingRecord.status='STARTED'
}catch{$wingRecord.status='FAIL';$wingRecord.error=$_.Exception.Message;throw}
finally{
    foreach($wingKey in $wingOld.Keys){[Environment]::SetEnvironmentVariable($wingKey,$wingOld[$wingKey],'Process')}
    $wingRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $wingRecordPath -Encoding utf8
}
Write-Output "Wing A/B evidence: $wingOut"
if($WaitForExit){
    $wingWatch=[Diagnostics.Stopwatch]::StartNew()
    while(-not $wingProcess.WaitForExit(250)){
        $wingLiveFiles=@(Get-ChildItem -LiteralPath $wingOut -Recurse -File -Filter 'wing_material_review.json')
        $wingLive=$null
        if($wingLiveFiles.Count -eq 1){try{$wingLive=Read-WingSharedJSON $wingLiveFiles[0].FullName}catch{}}
        if($wingLive -and $wingLive.user_stop_latched -eq $true){
            $wingRecord.status='USER_STOPPED_PROCESS_LEFT_OPEN';$wingRecord.stop_latched=$true;$wingRecord.runtime_report=$wingLiveFiles[0].FullName
            $wingRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $wingRecordPath -Encoding utf8
            Write-Output 'Observed native stop. No restart, forced close or further desktop action.'
            exit 1
        }
        if($wingWatch.Elapsed.TotalSeconds -gt 210){
            $wingRecord.status='FAIL_DEADLINE_PROCESS_LEFT_OPEN';$wingRecord.error='Bounded fixture did not exit; no forced termination.'
            $wingRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $wingRecordPath -Encoding utf8
            exit 1
        }
    }
    $wingProcess.Refresh();$wingRecord.exit_code=$wingProcess.ExitCode
    try{
        $wingResults=@(Get-ChildItem -LiteralPath $wingOut -Recurse -File -Filter 'wing_material_review.json')
        Require-Wing ($wingResults.Count -eq 1) 'Missing or ambiguous native result.'
        $wingResultFile=$wingResults[0].FullName;$wingRuntime=Get-Content -LiteralPath $wingResultFile -Raw|ConvertFrom-Json
        $wingRecord.runtime_report=$wingResultFile;$wingRecord.runtime_sha256=(Get-FileHash -LiteralPath $wingResultFile).Hash
        $wingRecord.runtime_status=$wingRuntime.status;$wingRecord.stop_latched=$wingRuntime.user_stop_latched
        Require-Wing ($wingProcess.ExitCode -eq 0 -and $wingRuntime.status -ceq 'PASS' -and $wingRuntime.user_stop_latched -eq $false) 'Native fixture failed or stopped; no retry.'
        Require-Wing ($wingRuntime.schema -ceq 'HarborCity.M5VS2.WingMaterialReview.Runtime.v1' -and $wingRuntime.map -ceq $wingData.map -and $wingRuntime.captures.Count -eq 4 -and $wingRuntime.os_input_used -eq $false) 'Wrong native fixture scope or image count.'
        Require-Wing ($wingRuntime.source_blueprint_class -ceq $wingData.director_readback.reference_hero_class -and $wingRuntime.candidate_blueprint_class -ceq $wingData.director_readback.candidate_hero_class -and $wingRuntime.material_a -ceq $wingData.director_readback.reference_material -and $wingRuntime.material_b -ceq $wingData.director_readback.candidate_material) 'Runtime references do not match the saved native author.'
        $wingPNGProof=@();$wingImageRoot=[IO.Path]::GetFullPath((Split-Path $wingResultFile -Parent))
        $wingNames=@('00_DefaultGain_A_Additive.png','01_DefaultGain_B_SoftLayers.png','02_BoostGain_A_Additive.png','03_BoostGain_B_SoftLayers.png')
        for($wingIndex=0;$wingIndex -lt 4;$wingIndex++){
            $wingCapture=$wingRuntime.captures[$wingIndex];$wingPNG=[IO.Path]::GetFullPath($wingCapture.file)
            Require-Wing ($wingPNG -ieq (Join-Path $wingImageRoot $wingNames[$wingIndex])) 'Screenshot path/index does not match the owned native request.'
            Require-Wing ($wingCapture.status -ceq 'PASS' -and $wingCapture.shot_index -eq $wingIndex -and $wingCapture.same_pose_camera_material -eq $true -and $wingCapture.same_pose_after_save -eq $true -and $wingCapture.feather_count -eq 20 -and $wingCapture.actual_feathers.Count -eq 20 -and $wingCapture.width -eq 1920 -and $wingCapture.height -eq 1080) 'Actual pose/camera/mesh image guard failed.'
            Require-Wing ($wingCapture.shader_readiness.status -ceq 'READY' -and $wingCapture.shader_readiness.game_thread_ready -eq $true -and $wingCapture.shader_readiness.render_thread_ready_without_fallback -eq $true -and $wingCapture.draws_after_material_ready -ge 2) 'Actual material readiness/rendered-frame guard failed.'
            $wingFile=Get-Item -LiteralPath $wingPNG;Require-Wing ($wingFile.Length -eq $wingCapture.file_bytes -and $wingFile.Length -gt 33) 'Original screenshot byte size mismatch.'
            $wingStream=[IO.File]::OpenRead($wingPNG)
            try{$wingHeader=[byte[]]::new(24);Require-Wing ($wingStream.Read($wingHeader,0,24) -eq 24) 'Truncated PNG.'}finally{$wingStream.Dispose()}
            Require-Wing ([BitConverter]::ToString($wingHeader[0..7]) -ceq '89-50-4E-47-0D-0A-1A-0A' -and [BitConverter]::ToString($wingHeader[12..23]) -ceq '49-48-44-52-00-00-07-80-00-00-04-38') 'Original PNG IHDR is not 1920x1080.'
            $wingPNGProof+=@{file=$wingPNG;sha256=(Get-FileHash -LiteralPath $wingPNG).Hash;bytes=$wingFile.Length}
        }
        Require-Wing (@(Get-ChildItem -LiteralPath $wingImageRoot -File -Filter '*.png').Count -eq 4) 'Unexpected extra PNGs in fixture.'
        $wingRecord.original_pngs=$wingPNGProof;$wingRecord.status='PASS'
    }catch{$wingRecord.status='FAIL_OR_STOPPED';$wingRecord.error=$_.Exception.Message}
    finally{$wingRecord.ended_at=(Get-Date).ToString('o');$wingRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $wingRecordPath -Encoding utf8}
    if($wingRecord.status -cne 'PASS'){exit 1}
}
