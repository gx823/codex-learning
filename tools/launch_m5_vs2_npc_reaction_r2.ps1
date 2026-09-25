#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$AuthorReport,
    [ValidateSet('Forward','Backward','Left','Right')][string]$Direction='Forward',
    [switch]$WaitForExit,
    [switch]$JClothProbe
)
$ErrorActionPreference='Stop'
$reactionWork=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if($reactionWork -ine [IO.Path]::GetFullPath('D:/科研学习/codex学习')){throw 'Wrong HarborCity workspace.'}
$reactionDocs=Join-Path $reactionWork 'docs/HarborCity_M5_VS2/editor_runtime'
$reactionReport=[IO.Path]::GetFullPath($AuthorReport)
if(-not $reactionReport.StartsWith(([IO.Path]::GetFullPath($reactionDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($reactionReport) -cne 'author_result.json'){throw 'Require this milestone native author_result.json.'}
$reactionAuthor=Get-Content -LiteralPath $reactionReport -Raw|ConvertFrom-Json
$reactionCommandFile=Join-Path (Split-Path $reactionReport -Parent) 'commandlet.json'
$reactionCommand=Get-Content -LiteralPath $reactionCommandFile -Raw|ConvertFrom-Json
$reactionScript=Join-Path $PSScriptRoot 'ue_m5_vs2_npc_reaction_r2_author.py'
if($reactionAuthor.schema -cne 'HarborCity.M5VS2.NPCReactionR2.Author.v1' -or $reactionAuthor.status -cne 'PASS' -or
    $reactionCommand.status -cne 'PASS' -or $reactionCommand.author_status -cne 'PASS' -or $reactionCommand.exit_code -ne 0 -or
    $reactionCommand.phase -cne $reactionAuthor.phase -or $reactionAuthor.owner -cne 'HarborCity_M5_VS2_NPCReactionReviewR2' -or
    $reactionAuthor.source_hash_changes.Count -ne 0 -or -not $reactionAuthor.protected_source_sha256_before -or $reactionAuthor.assets.Count -ne 2 -or
    @($reactionAuthor.checks|Where-Object {$_.status -cne 'PASS'}).Count -ne 0){throw 'Missing complete native author/protection evidence.'}
if([IO.Path]::GetFullPath($reactionCommand.script) -ine [IO.Path]::GetFullPath($reactionScript) -or
    (Get-FileHash -LiteralPath $reactionScript).Hash -ine $reactionCommand.script_sha256){throw 'Native author source changed after save.'}
$reactionPair=$reactionAuthor.review_pair
$reactionSite=$reactionAuthor.site
$reactionPrivate=$reactionAuthor.roster_version -ceq 'FINAL_IDLE_R2_PAIR'
if($reactionSite -cnotmatch '^(Open|Wall|Slope|Narrow)$' -or $reactionPair -cnotmatch '^(QR|JT|UV|WX)$'){throw 'Unknown exact site or pair.'}
if($reactionPrivate){
    if($reactionAuthor.phase -cne ('NPCReactionR2Idle'+$reactionSite+$reactionPair) -or
        $reactionAuthor.map -cnotmatch ('^/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/IdlePairs/Attempt_[0-9a-f]{12}/L_NPCReaction_'+$reactionSite+'_'+$reactionPair+'$')){
        throw 'Final-idle fixture must be a unique private pair map.'
    }
}else{
    if($reactionAuthor.roster_version -cne 'LEGACY_QR' -or $reactionPair -cne 'QR' -or
        $reactionAuthor.phase -cne ('NPCReactionR2'+$reactionSite) -or
        $reactionAuthor.map -cne ('/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/L_NPCReaction_'+$reactionSite)){
        throw 'Legacy mode is explicitly limited to original Q/R.'
    }
}
if($reactionAuthor.roster.Count -ne 2){throw 'Exactly two source-bound specimens required per bounded runtime.'}
for($reactionIndex=0;$reactionIndex -lt 2;$reactionIndex++){
    $reactionLetter=$reactionPair.Substring($reactionIndex,1);$reactionPerson=$reactionAuthor.roster[$reactionIndex]
    if($reactionPerson.sample -cne $reactionLetter -or $reactionPerson.stable_id -cne ('M5VS2_'+$reactionLetter)){throw 'Ordered actual source identity mismatch.'}
    if($reactionPrivate){
        if($reactionPerson.blueprint -cnotmatch ('^/Game/HarborCity/M5VS2/NPC/AvatarSample_'+$reactionLetter+'/IdleR2/Batch_[0-9a-f]{12}/BP_NPCIdleR2_'+$reactionLetter+'$')){throw 'Not the final private idle candidate.'}
        $reactionInput=$reactionAuthor.inputs.('NPCIdle_'+$reactionLetter)
        if(-not $reactionInput -or (Get-FileHash -LiteralPath $reactionInput.path).Hash -ine $reactionInput.sha256){throw 'Missing exact IdleReload report binding.'}
        $reactionIdle=Get-Content -LiteralPath $reactionInput.path -Raw|ConvertFrom-Json
        $reactionIdleRun=Get-Content -LiteralPath (Join-Path (Split-Path $reactionInput.path -Parent) 'commandlet.json') -Raw|ConvertFrom-Json
        if($reactionIdle.status -cne 'PASS' -or $reactionIdle.fresh_process_disk_readback -cne 'PASS' -or
            $reactionIdle.phase -cne ('NPCIdleR2'+$reactionLetter+'Reload') -or $reactionIdleRun.status -cne 'PASS' -or $reactionIdleRun.exit_code -ne 0 -or
            $reactionIdle.candidate_blueprint -cne $reactionPerson.blueprint -or $reactionIdle.preservation.changed_source_files.Count -ne 0){throw 'Final candidate native reload proof differs.'}
        foreach($reactionAsset in $reactionIdle.assets){
            if($reactionAsset.path -cnotmatch ('^/Game/HarborCity/M5VS2/NPC/AvatarSample_'+$reactionLetter+'/IdleR2/Batch_[0-9a-f]{12}/[A-Za-z0-9_]+$')){throw 'Unexpected idle package scope.'}
            $reactionFile=Join-Path $reactionWork ('HarborCity/Content/'+$reactionAsset.path.Substring(6)+'.uasset')
            if((Get-FileHash -LiteralPath $reactionFile).Hash -ine $reactionAsset.sha256){throw 'Final idle package changed after Reload.'}
        }
    }elseif($reactionPerson.blueprint -cnotmatch ('^/Game/HarborCity/M5VS2/NPC/AvatarSample_'+$reactionLetter+'/Runtime_[0-9a-f]{10}/BP_AvatarSample_'+$reactionLetter+'$')){throw 'Unexpected legacy source Blueprint.'}
}
foreach($reactionSource in $reactionAuthor.protected_source_sha256_before.PSObject.Properties){
    if((Get-FileHash -LiteralPath $reactionSource.Name).Hash -ine $reactionSource.Value){throw "Protected source changed: $($reactionSource.Name)"}
}
foreach($reactionInput in $reactionAuthor.inputs.PSObject.Properties){
    if($reactionInput.Value.path -and $reactionInput.Value.sha256 -and
        (Get-FileHash -LiteralPath $reactionInput.Value.path).Hash -ine $reactionInput.Value.sha256){throw "Bound author input changed: $($reactionInput.Name)"}
}
if($reactionAuthor.navigation_build.status -cne 'PASS' -or $reactionAuthor.navigation_saved_readback.status -cne 'PASS' -or
    $reactionAuthor.navigation_saved_readback.native_build_requested -ne $false -or $reactionAuthor.navigation_saved_readback.review_pair -cne $reactionPair){throw 'Saved pair navigation was not actually baked and reloaded.'}
$reactionMaps=@($reactionAuthor.assets|Where-Object {$_.path -ceq $reactionAuthor.map})
if($reactionMaps.Count -ne 1){throw 'One exact map required.'}
foreach($reactionAsset in $reactionAuthor.assets){
    $reactionPackage=$reactionAsset.path
    if($reactionPackage -cnotmatch '^/Game/HarborCity/M5VS2/NPC/ReactionReviewR2/[A-Za-z0-9_/]+$' -or $reactionPackage.Contains('..')){throw 'Unexpected reaction fixture package.'}
    if($reactionPrivate -and ($reactionPackage -replace '/[^/]+$','') -cne ($reactionAuthor.map -replace '/[^/]+$','')){throw 'Private pair assets must share the exact batch.'}
    $reactionExtension=if($reactionPackage -ceq $reactionAuthor.map){'.umap'}else{'.uasset'}
    $reactionFile=Join-Path $reactionWork ('HarborCity/Content/'+$reactionPackage.Substring(6)+$reactionExtension)
    if((Get-FileHash -LiteralPath $reactionFile).Hash -ine $reactionAsset.sha256){throw 'Saved fixture bytes changed.'}
}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){throw 'Close current engine/game normally before this separate rendered run.'}
$reactionProject=Join-Path $reactionWork 'HarborCity/HarborCity.uproject'
$reactionModule=Join-Path $reactionWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$reactionExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
foreach($reactionFile in @($reactionProject,$reactionModule,$reactionExe)){if(-not(Test-Path -LiteralPath $reactionFile -PathType Leaf)){throw "Missing $reactionFile"}}
$reactionRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$reactionOut=Join-Path $reactionDocs ($reactionRun+'_npcreaction_'+$reactionSite+'_'+$reactionPair+'_'+$Direction+'_game')
New-Item -ItemType Directory -Path $reactionOut|Out-Null
$reactionCache='D:/GameDev/Cache/Unreal/HarborCity'
$reactionCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$reactionArgs=@(('"'+$reactionProject+'"'),$reactionAuthor.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
    '-M5VS2NPCReactionReview',('-M5NPCFallDirection='+$Direction),'-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$reactionOut+'"'),
    ('-HCM1SaveSlot=HarborCity_M1_R2_Test_Reaction_'+$reactionRun),('-abslog="'+(Join-Path $reactionOut 'game.log')+'"'),
    ('-ExecCmds="'+($reactionCommands -join ',')+'"'),"-LocalDataCachePath=$reactionCache/DDC","-ZenDataPath=$reactionCache/Zen")
if($reactionPrivate){$reactionArgs+=('-M5VS2NPCReactionIdlePair='+$reactionPair)}
if($JClothProbe){
    if(-not $reactionPrivate -or $reactionPair -cne 'JT' -or $reactionSite -cne 'Open'){throw 'J cloth diagnostics require the exact private Open JT fixture.'}
    $reactionArgs+='-M5VS2NPCClothProbe'
}
$reactionRecord=[ordered]@{milestone='M5_VS2';phase='NPCReactionR2';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';
    executable=$reactionExe;arguments=$reactionArgs;map=$reactionAuthor.map;site=$reactionSite;review_pair=$reactionPair;roster_version=$reactionAuthor.roster_version;
    requested_impact_direction=$Direction;roster=$reactionAuthor.roster;j_cloth_probe=[bool]$JClothProbe;started_at=(Get-Date).ToString('o');
    author_report=$reactionReport;author_sha256=(Get-FileHash -LiteralPath $reactionReport).Hash;module_sha256=(Get-FileHash -LiteralPath $reactionModule).Hash;
    os_input_used=$false;actual_vehicle_contact_tested=$false;packaged_runtime='NOT_RUN';visual_acceptance='USER_REVIEW';
    coverage='Exactly two named sources in this site and requested impact direction. This is not eight-person or four-getup-direction coverage.';
    stop_policy='Esc permanently latches native director; wrapper never sends input, resumes, kills or restarts the game. P is the real pause control.';
    focus_policy=$(if($reactionPrivate){'Focus loss opens real pause menu; return then P to resume.'}else{'Legacy behavior unchanged; use P before changing focus.'});
    process_id=$null;exit_code=$null;runtime_status='NOT_RUN';stop_latched=$null}
$reactionLaunch=Join-Path $reactionOut 'launch.json'
$reactionRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $reactionLaunch -Encoding utf8
$reactionEnv=@{'UE-LocalDataCachePath'="$reactionCache/DDC";'UE-ZenDataPath'="$reactionCache/Zen";'UE-ZenSubprocessDataPath'="$reactionCache/Zen"};$reactionOld=@{}
try{
    foreach($reactionKey in $reactionEnv.Keys){$reactionOld[$reactionKey]=[Environment]::GetEnvironmentVariable($reactionKey,'Process');[Environment]::SetEnvironmentVariable($reactionKey,$reactionEnv[$reactionKey],'Process')}
    # User-authorized visible interactive diagnostic; no background helper or OS-input automation.
    $reactionProcess=Start-Process -FilePath $reactionExe -ArgumentList $reactionArgs -WorkingDirectory (Split-Path $reactionProject -Parent) -WindowStyle Normal -PassThru
    $null=$reactionProcess.Handle;$reactionRecord.process_id=$reactionProcess.Id;$reactionRecord.status='STARTED'
}catch{$reactionRecord.status='FAIL';$reactionRecord.error=$_.Exception.Message;throw}
finally{
    foreach($reactionKey in $reactionOld.Keys){[Environment]::SetEnvironmentVariable($reactionKey,$reactionOld[$reactionKey],'Process')}
    $reactionRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $reactionLaunch -Encoding utf8
}
Write-Output "Reaction review evidence: $reactionOut"
if($WaitForExit){
    $reactionProcess.WaitForExit();$reactionProcess.Refresh();$reactionRecord.exit_code=$reactionProcess.ExitCode
    $reactionResults=@(Get-ChildItem -LiteralPath $reactionOut -Directory -Filter 'NPCReactionR2_*'|ForEach-Object {Join-Path $_.FullName 'npc_reaction_review.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    if($reactionResults.Count -eq 1){
        $reactionRuntime=Get-Content -LiteralPath $reactionResults[0] -Raw|ConvertFrom-Json
        $reactionRecord.runtime_status=$reactionRuntime.status;$reactionRecord.stop_latched=$reactionRuntime.user_stop_latched;$reactionRecord.runtime_report=$reactionResults[0]
        if($reactionRuntime.map -cne $reactionAuthor.map -or $reactionRuntime.site -cne $reactionSite -or $reactionRuntime.review_pair -cne $reactionPair -or
            $reactionRuntime.roster_version -cne $reactionAuthor.roster_version -or $reactionRuntime.requested_impact_direction -cne $Direction -or
            $reactionRuntime.os_input_used -ne $false -or $reactionRuntime.actual_vehicle_contact_tested -ne $false){$reactionRecord.runtime_status='FAIL_RUNTIME_IDENTITY'}
        if($reactionRuntime.status -ceq 'PASS'){
            $reactionCaptureChecks=@()
            foreach($reactionCapture in $reactionRuntime.captures){
                $reactionPNG=[IO.Path]::GetFullPath($reactionCapture.file);$reactionValid=$false;$reactionWidth=0;$reactionHeight=0
                if($reactionPNG.StartsWith($reactionOut+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $reactionPNG -PathType Leaf)){
                    $reactionBytes=[byte[]]::new(24);$reactionStream=[IO.File]::OpenRead($reactionPNG)
                    try{$reactionRead=$reactionStream.Read($reactionBytes,0,24)}finally{$reactionStream.Dispose()}
                    if($reactionRead -eq 24 -and [BitConverter]::ToString($reactionBytes,0,8) -ceq '89-50-4E-47-0D-0A-1A-0A'){
                        $reactionWidth=([int]$reactionBytes[16]*16777216)+([int]$reactionBytes[17]*65536)+([int]$reactionBytes[18]*256)+[int]$reactionBytes[19]
                        $reactionHeight=([int]$reactionBytes[20]*16777216)+([int]$reactionBytes[21]*65536)+([int]$reactionBytes[22]*256)+[int]$reactionBytes[23]
                        $reactionValid=$reactionWidth -eq 1920 -and $reactionHeight -eq 1080 -and $reactionCapture.status -ceq 'PASS'
                    }
                }
                $reactionCaptureChecks+=@{file=$reactionPNG;valid_1920x1080=$reactionValid;width=$reactionWidth;height=$reactionHeight}
            }
            $reactionRecord.captures=$reactionCaptureChecks
            if($reactionCaptureChecks.Count -ne 10 -or @($reactionCaptureChecks|Where-Object {-not $_.valid_1920x1080}).Count){$reactionRecord.runtime_status='FAIL_CAPTURE_INTEGRITY'}
        }
    }else{$reactionRecord.runtime_status='MISSING_OR_AMBIGUOUS'}
    $reactionRecord.status=if($reactionProcess.ExitCode -eq 0 -and $reactionRecord.runtime_status -ceq 'PASS' -and $reactionRecord.stop_latched -eq $false){'PASS'}else{'FAIL_OR_STOPPED'}
    $reactionRecord.ended_at=(Get-Date).ToString('o');$reactionRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $reactionLaunch -Encoding utf8
    if($reactionRecord.status -ne 'PASS'){exit 1}
}
