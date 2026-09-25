#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$AuthorReport,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$castWork=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if($castWork -ine [IO.Path]::GetFullPath('D:/科研学习/codex学习')){throw 'Wrong HarborCity workspace.'}
$castDocs=Join-Path $castWork 'docs/HarborCity_M5_VS2/editor_runtime'
function Get-CastPackage([string]$Value){return ($Value -split '\.')[0]}
function Assert-CastHash([string]$Path,[string]$Expected){
    if($Expected -cnotmatch '^[0-9a-fA-F]{64}$' -or -not(Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Expected){throw "Missing or changed bound file: $Path"}
}
function Assert-CastCommandlet([string]$Report,[string]$Phase){
    $castCmdPath=Join-Path (Split-Path $Report -Parent) 'commandlet.json'
    $castCmd=Get-Content -LiteralPath $castCmdPath -Raw|ConvertFrom-Json
    if($castCmd.status -cne 'PASS' -or $castCmd.author_status -cne 'PASS' -or
        $null -eq $castCmd.exit_code -or $castCmd.exit_code -ne 0 -or $castCmd.phase -cne $Phase){throw "Native process did not complete successfully: $castCmdPath"}
    return $castCmd
}
$castReport=[IO.Path]::GetFullPath($AuthorReport)
if(-not $castReport.StartsWith(([IO.Path]::GetFullPath($castDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($castReport) -cne 'author_result.json'){throw 'Require this milestone native author_result.json.'}
$castAuthor=Get-Content -LiteralPath $castReport -Raw|ConvertFrom-Json
$castCommand=Assert-CastCommandlet $castReport 'NPCCastReviewIdleR2'
$castScript=Join-Path $PSScriptRoot 'ue_m5_vs2_npc_cast_review_author.py'
if($castAuthor.schema -cne 'HarborCity.M5VS2.NPCCastReview.Author.v1' -or $castAuthor.status -cne 'PASS' -or
    $castAuthor.phase -cne 'NPCCastReviewIdleR2' -or $castAuthor.owner -cne 'HarborCity_M5_VS2_NPCCastReview' -or
    $castAuthor.map -cnotmatch '^/Game/HarborCity/M5VS2/NPC/CastReview/Run_[0-9a-f]{12}/L_NPCCast$' -or
    @($castAuthor.assets).Count -ne 3 -or @($castAuthor.roster).Count -ne 8 -or @($castAuthor.checks).Count -lt 1 -or
    @($castAuthor.checks|Where-Object {$_.status -cne 'PASS'}).Count -ne 0 -or
    $null -eq $castAuthor.preservation.unexpected_changed_files -or @($castAuthor.preservation.unexpected_changed_files).Count -ne 0 -or
    -not $castAuthor.preservation.source_sha256_before){throw 'Missing complete final-eight native author/protection evidence.'}
if([IO.Path]::GetFullPath($castCommand.script) -ine [IO.Path]::GetFullPath($castScript)){throw 'Unexpected native author script.'}
Assert-CastHash $castScript $castCommand.script_sha256
$castBatch=$castAuthor.map -replace '/L_NPCCast$',''
$castExpectedPackages=@($castAuthor.map,($castBatch+'/Materials/M_NeutralFloor'),($castBatch+'/Materials/M_NeutralBackdrop'))
$castPackages=@($castAuthor.assets|ForEach-Object {Get-CastPackage $_.path})
if(@(Compare-Object ($castExpectedPackages|Sort-Object) ($castPackages|Sort-Object) -CaseSensitive).Count -ne 0 -or
    @($castPackages|Sort-Object -Unique).Count -ne 3){throw 'Exactly one private map and its two neutral materials required.'}
foreach($castAsset in $castAuthor.assets){
    $castPackage=Get-CastPackage $castAsset.path
    $castExt=if($castPackage -ceq $castAuthor.map){'.umap'}else{'.uasset'}
    Assert-CastHash (Join-Path $castWork ('HarborCity/Content/'+$castPackage.Substring(6)+$castExt)) $castAsset.sha256
}
$castLetters=@('Q','R','J','T','U','V','W','X')
$castExpectedRoster=@()
for($castIndex=0;$castIndex -lt 8;$castIndex++){
    $castLetter=$castLetters[$castIndex];$castPerson=$castAuthor.roster[$castIndex]
    $castBP=Get-CastPackage $castPerson.blueprint
    if($castPerson.sample -cne $castLetter -or
        $castBP -cnotmatch ('^/Game/HarborCity/M5VS2/NPC/AvatarSample_'+$castLetter+'/IdleR2/Batch_[0-9a-f]{12}/BP_NPCIdleR2_'+$castLetter+'$')){throw 'Ordered cast must use all eight final private IdleR2 candidates.'}
    $castInput=$castAuthor.inputs.('NPCIdle_'+$castLetter)
    if(-not $castInput){throw "Missing final IdleReload input for $castLetter"}
    Assert-CastHash $castInput.path $castInput.sha256
    $castIdle=Get-Content -LiteralPath $castInput.path -Raw|ConvertFrom-Json
    $null=Assert-CastCommandlet $castInput.path ('NPCIdleR2'+$castLetter+'Reload')
    if($castIdle.schema -cne 'HarborCity.M5VS2.NPCIdleR2.v1' -or $castIdle.status -cne 'PASS' -or $castIdle.sample -cne $castLetter -or
        $castIdle.phase -cne ('NPCIdleR2'+$castLetter+'Reload') -or $castIdle.fresh_process_disk_readback -cne 'PASS' -or
        $castIdle.candidate_blueprint -cne $castBP -or @($castIdle.assets).Count -ne 4 -or
        @($castIdle.preservation.changed_source_files).Count -ne 0 -or
        (Get-CastPackage $castIdle.specimen.profile) -cne (Get-CastPackage $castPerson.profile) -or
        (Get-CastPackage $castIdle.specimen.mesh) -cne (Get-CastPackage $castPerson.mesh)){throw "Final native candidate differs: $castLetter"}
    Assert-CastHash $castInput.apply_evidence.path $castInput.apply_evidence.sha256
    $castApply=Get-Content -LiteralPath $castInput.apply_evidence.path -Raw|ConvertFrom-Json
    $null=Assert-CastCommandlet $castInput.apply_evidence.path ('NPCIdleR2'+$castLetter+'Apply')
    if($castApply.status -cne 'PASS' -or $castApply.candidate_blueprint -cne $castBP){throw 'Wrong actual Idle Apply.'}
    foreach($castNativeRun in $castInput.commandlets){Assert-CastHash $castNativeRun.path $castNativeRun.sha256}
    foreach($castAsset in $castIdle.assets){
        $castPackage=Get-CastPackage $castAsset.path
        if($castPackage -cnotmatch ('^/Game/HarborCity/M5VS2/NPC/AvatarSample_'+$castLetter+'/IdleR2/Batch_[0-9a-f]{12}/[A-Za-z0-9_]+$') -or
            ($castPackage -replace '/[^/]+$','') -cne ($castBP -replace '/[^/]+$','')){throw 'Unexpected final Idle package.'}
        Assert-CastHash (Join-Path $castWork ('HarborCity/Content/'+$castPackage.Substring(6)+'.uasset')) $castAsset.sha256
    }
    $castExpectedRoster+=@{sample=$castLetter;blueprint=$castBP;profile=(Get-CastPackage $castPerson.profile);mesh=(Get-CastPackage $castPerson.mesh);physics=(Get-CastPackage $castIdle.specimen.physics)}
}
foreach($castSource in $castAuthor.preservation.source_sha256_before.PSObject.Properties){Assert-CastHash $castSource.Name $castSource.Value}
foreach($castInput in $castAuthor.inputs.PSObject.Properties){
    if($castInput.Value.path -and $castInput.Value.sha256){Assert-CastHash $castInput.Value.path $castInput.Value.sha256}
}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){throw 'Close the existing engine/game normally before this separate rendered run.'}
$castProject=Join-Path $castWork 'HarborCity/HarborCity.uproject'
$castModule=Join-Path $castWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$castExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
foreach($castFile in @($castProject,$castModule,$castExe)){if(-not(Test-Path -LiteralPath $castFile -PathType Leaf)){throw "Missing $castFile"}}
$castRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$castOut=Join-Path $castDocs ($castRun+'_npccast_idle_r2_game')
New-Item -ItemType Directory -Path $castOut|Out-Null
$castCache='D:/GameDev/Cache/Unreal/HarborCity'
$castCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$castArgs=@(('"'+$castProject+'"'),$castAuthor.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
    '-M5VS2NPCReview','-M5VS2NPCCastReview','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$castOut+'"'),
    ('-HCM1SaveSlot=HarborCity_M1_R2_Test_NPCCast_'+$castRun),('-abslog="'+(Join-Path $castOut 'game.log')+'"'),
    ('-ExecCmds="'+($castCommands -join ',')+'"'),"-LocalDataCachePath=$castCache/DDC","-ZenDataPath=$castCache/Zen")
$castRecord=[ordered]@{milestone='M5_VS2';phase='NPCCastReviewIdleR2';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';
    executable=$castExe;arguments=$castArgs;map=$castAuthor.map;roster=$castExpectedRoster;started_at=(Get-Date).ToString('o');
    author_report=$castReport;author_sha256=(Get-FileHash -LiteralPath $castReport).Hash;author_script_sha256=$castCommand.script_sha256;
    module_sha256=(Get-FileHash -LiteralPath $castModule).Hash;launcher_sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;
    os_input_used=$false;packaged_runtime='NOT_RUN';visual_acceptance='USER_REVIEW';expected_pngs=16;
    coverage='Eight final private IdleR2 sources, neutral proxy face/full-body views; three SetEmotion function probes and automatic blink. Not harbor, dialogue, navigation, combat, physical recovery or OS input acceptance.';
    stop_policy='Native viewport Esc latches permanently and cancels auto-exit; wrapper never sends input, kills, resumes or restarts. P is the real pause control.';
    focus_policy='Existing Director has no explicit focus-loss auto-pause; use P before changing focus. Original 240-second wall-clock bound remains.';
    material_readiness='Per-material render-thread fallback NOT_RUN; existing global shader-idle and reference checks only.';
    process_id=$null;exit_code=$null;runtime_status='NOT_RUN';stop_latched=$null}
$castLaunch=Join-Path $castOut 'launch.json'
$castRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $castLaunch -Encoding utf8
$castEnv=@{'UE-LocalDataCachePath'="$castCache/DDC";'UE-ZenDataPath'="$castCache/Zen";'UE-ZenSubprocessDataPath'="$castCache/Zen"};$castOld=@{}
try{
    foreach($castKey in $castEnv.Keys){$castOld[$castKey]=[Environment]::GetEnvironmentVariable($castKey,'Process');[Environment]::SetEnvironmentVariable($castKey,$castEnv[$castKey],'Process')}
    # User-authorized visible interactive diagnostic; no OS input automation.
    $castProcess=Start-Process -FilePath $castExe -ArgumentList $castArgs -WorkingDirectory (Split-Path $castProject -Parent) -WindowStyle Normal -PassThru
    $null=$castProcess.Handle;$castRecord.process_id=$castProcess.Id;$castRecord.status='STARTED'
}catch{$castRecord.status='FAIL';$castRecord.error=$_.Exception.Message;throw}
finally{
    foreach($castKey in $castOld.Keys){[Environment]::SetEnvironmentVariable($castKey,$castOld[$castKey],'Process')}
    $castRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $castLaunch -Encoding utf8
}
Write-Output "NPC cast evidence: $castOut"
if($WaitForExit){
    $castProcess.WaitForExit();$castProcess.Refresh();$castRecord.exit_code=$castProcess.ExitCode
    try{
        $castResults=@(Get-ChildItem -LiteralPath $castOut -Directory -Filter 'NPCReview_*'|ForEach-Object {Join-Path $_.FullName 'npc_review.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
        if($castResults.Count -ne 1){throw 'Missing or ambiguous native cast runtime report.'}
        $castRuntime=Get-Content -LiteralPath $castResults[0] -Raw|ConvertFrom-Json
        $castRecord.runtime_status=$castRuntime.status;$castRecord.stop_latched=$castRuntime.user_stop_latched
        $castRecord.runtime_report=$castResults[0];$castRecord.runtime_report_sha256=(Get-FileHash -LiteralPath $castResults[0]).Hash
        # Director serializes GetMapName(), not its full package. Each actor path below verifies the unique full package.
        if($castRuntime.map -cne 'L_NPCCast' -or $castRuntime.cast_portraits -ne $true -or
            $castRuntime.physics_only_progression -ne $false -or $castRuntime.os_input_used -ne $false -or
            $castRuntime.visual_acceptance -cne 'USER_REVIEW'){throw 'Unexpected actual runtime mode or acceptance scope.'}
        if($castRuntime.status -ceq 'PASS' -and $castRuntime.user_stop_latched -eq $false){
            if(@($castRuntime.captures).Count -ne 16){throw 'Exactly sixteen completed native PNGs required.'}
            $castCaptureChecks=@();$castNames=@();$castDir=Split-Path $castResults[0] -Parent
            for($castIndex=0;$castIndex -lt 16;$castIndex++){
                $castCapture=$castRuntime.captures[$castIndex];$castSubject=$castLetters[[int][Math]::Floor($castIndex/2)]
                $castFrame=if($castIndex%2 -eq 0){'FACE'}else{'FULL_BODY'}
                $castLabel=$castSubject+$(if($castIndex%2 -eq 0){'_Face_Neutral'}else{'_FullBody'})
                $castPNG=[IO.Path]::GetFullPath($castCapture.file);$castName=('{0:D2}_{1}.png' -f $castIndex,$castLabel)
                if((Split-Path $castPNG -Parent) -ine $castDir -or [IO.Path]::GetFileName($castPNG) -cne $castName -or
                    $castCapture.status -cne 'PASS' -or $castCapture.subject -cne $castSubject -or $castCapture.framing -cne $castFrame -or
                    $castCapture.label -cne $castLabel -or $castCapture.width -ne 1920 -or $castCapture.height -ne 1080){throw 'Capture sequence/path/framing mismatch.'}
                $castBytes=[byte[]]::new(24);$castStream=[IO.File]::OpenRead($castPNG)
                try{$castRead=$castStream.Read($castBytes,0,24);$castLength=$castStream.Length}finally{$castStream.Dispose()}
                if($castRead -ne 24 -or $castLength -lt 33 -or [BitConverter]::ToString($castBytes,0,8) -cne '89-50-4E-47-0D-0A-1A-0A' -or
                    [Text.Encoding]::ASCII.GetString($castBytes,12,4) -cne 'IHDR'){throw 'Invalid PNG header.'}
                $castWidth=([int]$castBytes[16]*16777216)+([int]$castBytes[17]*65536)+([int]$castBytes[18]*256)+[int]$castBytes[19]
                $castHeight=([int]$castBytes[20]*16777216)+([int]$castBytes[21]*65536)+([int]$castBytes[22]*256)+[int]$castBytes[23]
                if($castWidth -ne 1920 -or $castHeight -ne 1080 -or $castLength -ne $castCapture.file_bytes){throw 'Actual PNG dimensions or recorded size differ.'}
                if(@($castCapture.specimens).Count -ne 8){throw 'Actual eight-subject capture telemetry missing.'}
                $castIdentities=@()
                for($castPersonIndex=0;$castPersonIndex -lt 8;$castPersonIndex++){
                    $castExpected=$castExpectedRoster[$castPersonIndex];$castSeen=$castCapture.specimens[$castPersonIndex]
                    $castClass=$castExpected.blueprint+'.'+($castExpected.blueprint -split '/')[-1]+'_C'
                    if(-not $castSeen.actor.StartsWith($castAuthor.map+'.',[StringComparison]::Ordinal) -or $castSeen.blueprint_class -cne $castClass -or
                        (Get-CastPackage $castSeen.profile) -cne $castExpected.profile -or (Get-CastPackage $castSeen.mesh) -cne $castExpected.mesh -or
                        (Get-CastPackage $castSeen.physics_asset) -cne $castExpected.physics -or $castSeen.source_sha256 -cnotmatch '^[0-9a-fA-F]{64}$' -or
                        $castSeen.visual_profile_ready -ne $true -or $castSeen.face_ready -ne $true){throw 'Rendered actor is not its exact final source-bound candidate.'}
                    $castIdentities+=$castSeen.source_sha256
                }
                if(@($castIdentities|Sort-Object -Unique).Count -ne 8){throw 'Actual cast source identities are not independent.'}
                $castNames+=$castName;$castCaptureChecks+=@{file=$castPNG;sha256=(Get-FileHash -LiteralPath $castPNG).Hash;bytes=$castLength;width=$castWidth;height=$castHeight;subject=$castSubject;framing=$castFrame;status='PASS'}
            }
            if(@(Get-ChildItem -LiteralPath $castDir -File -Filter '*.png').Count -ne 16){throw 'Unexpected additional or missing PNGs in native cast directory.'}
            $castRecord.captures=$castCaptureChecks
            if(@($castRuntime.three_native_expression_readbacks_per_person).Count -ne 24 -or @($castRuntime.automatic_blink_observations).Count -ne 8){throw 'Bounded expression/blink coverage missing.'}
            foreach($castLetter in $castLetters){
                $castExpressions=@($castRuntime.three_native_expression_readbacks_per_person|Where-Object {$_.subject -ceq $castLetter})
                if($castExpressions.Count -ne 3 -or @(Compare-Object @('Angry','Happy','Sad') @($castExpressions.emotion|Sort-Object) -CaseSensitive).Count -ne 0 -or
                    @($castExpressions|Where-Object {$_.actual_bound_group_responded -ne $true}).Count -ne 0){throw 'Actual expression function readbacks incomplete.'}
                $castBlink=@($castRuntime.automatic_blink_observations|Where-Object {$_.subject -ceq $castLetter})
                if($castBlink.Count -ne 1 -or $castBlink[0].peak_automatic_blink_weight -lt .95){throw 'Actual automatic blink observation incomplete.'}
            }
        }
    }catch{$castRecord.validation_error=$_.Exception.Message;$castRecord.runtime_status='FAIL_EVIDENCE_VALIDATION'}
    finally{
        $castRecord.status=if($null -ne $castRecord.exit_code -and $castRecord.exit_code -eq 0 -and $castRecord.runtime_status -ceq 'PASS' -and $castRecord.stop_latched -eq $false){'PASS'}else{'FAIL_OR_STOPPED'}
        $castRecord.ended_at=(Get-Date).ToString('o');$castRecord|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $castLaunch -Encoding utf8
    }
    if($castRecord.status -cne 'PASS'){exit 1}
}
