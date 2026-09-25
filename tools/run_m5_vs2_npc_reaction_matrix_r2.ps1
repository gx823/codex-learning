#requires -Version 7.0
[CmdletBinding()]
param([switch]$Execute)
$ErrorActionPreference='Stop'
$matrixWork=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if($matrixWork -ine [IO.Path]::GetFullPath('D:/科研学习/codex学习')){throw 'Wrong HarborCity workspace.'}
$matrixRuntime=Join-Path $matrixWork 'docs/HarborCity_M5_VS2/editor_runtime'
$matrixPlan=@(
    [ordered]@{order=1;phase='NPCReactionR2IdleOpenQR';site='Open';pair='QR';direction='Forward'},
    [ordered]@{order=2;phase='NPCReactionR2IdleOpenJT';site='Open';pair='JT';direction='Backward'},
    [ordered]@{order=3;phase='NPCReactionR2IdleOpenUV';site='Open';pair='UV';direction='Left'},
    [ordered]@{order=4;phase='NPCReactionR2IdleOpenWX';site='Open';pair='WX';direction='Right'},
    [ordered]@{order=5;phase='NPCReactionR2IdleWallQR';site='Wall';pair='QR';direction='Forward'},
    [ordered]@{order=6;phase='NPCReactionR2IdleSlopeJT';site='Slope';pair='JT';direction='Backward'},
    [ordered]@{order=7;phase='NPCReactionR2IdleNarrowUV';site='Narrow';pair='UV';direction='Left'},
    [ordered]@{order=8;phase='NPCReactionR2IdleNarrowWX';site='Narrow';pair='WX';direction='Right'}
)
if(-not $Execute){
    [ordered]@{status='PLAN_ONLY_NOT_RUN';plan=$matrixPlan;run_switch='-Execute';
        scope='Eight native function-entry reaction runs; requested impulse direction does not guarantee get-up posture.';
        actual_vehicle_contact_tested=$false;weapon_trace_tested=$false;melee_hit_window_tested=$false;os_input_used=$false;
        stop_policy='No retry. Any failure or native Esc stops subsequent work; wait for natural process closure without input, kill or restart.'}|ConvertTo-Json -Depth 8
    exit 0
}
$matrixRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$matrixOut=Join-Path $matrixRuntime ($matrixRun+'_npc_reaction_matrix_r2')
New-Item -ItemType Directory -Path $matrixOut|Out-Null
$matrixFile=Join-Path $matrixOut 'matrix_result.json'
$script:matrixCurrent=$null
$matrixRecord=[ordered]@{schema='HarborCity.M5VS2.NPCReactionMatrixR2.v1';status='STARTING';started_at=(Get-Date).ToString('o');
    plan=$matrixPlan;runs=@();observations=@();source_sha256=[ordered]@{};idle_inputs=[ordered]@{};halt=$null;
    kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';visual_acceptance='USER_REVIEW';
    actual_vehicle_contact_tested=$false;weapon_trace_tested=$false;melee_hit_window_tested=$false;os_input_used=$false;save_load_tested=$false;
    limitations=@('Impulse directions are not semantic get-up poses.','Physics shape filters are not contact-manifold or penetration measurements.',
        'Eight runs do not cover every person x site x get-up pose.','Explicit fixture RestoreState is not save/load or automatic respawn.');
    completed_run_count=0;coverage=$null;ended_at=$null}
function Save-Matrix { $matrixRecord|ConvertTo-Json -Depth 35|Set-Content -LiteralPath $matrixFile -Encoding utf8 }
function Read-SharedText([string]$Path,[int]$TailBytes=0){
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $reader=$null
    try{
        if($TailBytes -gt 0 -and $stream.Length -gt $TailBytes){$null=$stream.Seek(-$TailBytes,[IO.SeekOrigin]::End)}
        $reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true)
        $snapshot=$reader.ReadToEnd()
    }finally{
        if($reader){$reader.Dispose()}else{$stream.Dispose()}
    }
    return $snapshot # The handle is closed before JSON parsing or other slow work.
}
function Read-Json([string]$Path){(Read-SharedText $Path)|ConvertFrom-Json}
function Require-Matrix([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
function Assert-NoEngine {
    Require-Matrix (-not(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'")) 'An engine/game is still active; no concurrent work or forced closure allowed.'
}
function Assert-Frozen {
    foreach($item in $matrixRecord.source_sha256.GetEnumerator()){
        Require-Matrix ((Get-FileHash -LiteralPath $item.Key).Hash -ieq $item.Value) ('Frozen source/module changed: '+$item.Key)
    }
    foreach($item in $matrixRecord.idle_inputs.GetEnumerator()){
        Require-Matrix ((Get-FileHash -LiteralPath $item.Value.path).Hash -ieq $item.Value.sha256) ('IdleReload report changed: '+$item.Key)
        foreach($asset in $item.Value.assets){
            $path=Join-Path $matrixWork ('HarborCity/Content/'+$asset.path.Substring(6)+'.uasset')
            Require-Matrix ((Get-FileHash -LiteralPath $path).Hash -ieq $asset.sha256) ('Final idle asset changed: '+$asset.path)
        }
    }
}
function Quote-Arg([string]$Value){
    if($Value.Contains('"') -or $Value.Contains("`r") -or $Value.Contains("`n")){throw 'Unexpected argument characters.'}
    '"'+$Value+'"'
}
function Get-AnimationPackage([string]$Path){
    $match=[regex]::Match($Path,'^(/Game/[A-Za-z0-9_/]+)(?:\.([A-Za-z0-9_]+))?$')
    Require-Matrix ($match.Success) ('Unexpected actual animation path: '+$Path)
    $package=$match.Groups[1].Value
    Require-Matrix (-not $match.Groups[2].Success -or $match.Groups[2].Value -ceq ($package.Split('/')[-1])) 'Animation object name differs from its exact package basename.'
    return $package
}
function Read-MatrixNativeStop([string]$Evidence,$Step){
    if(-not $Evidence -or $Step.native_stop_or_fail){return}
    $files=@(Get-ChildItem -LiteralPath $Evidence -Directory -Filter 'NPCReactionR2_*'|ForEach-Object {Join-Path $_.FullName 'npc_reaction_review.json'}|Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    foreach($path in $files){
        # Read one published snapshot; a transient read failure only delays observation.
        $native=$null;try{$native=Read-Json $path}catch{continue}
        if($native.user_stop_latched -eq $true -or $native.status -ceq 'FAIL'){
            $Step.native_stop_or_fail=[ordered]@{path=$path;status=$native.status;user_stop_latched=$native.user_stop_latched;observed_at=(Get-Date).ToString('o')}
            $matrixRecord.halt=$Step.native_stop_or_fail
            $matrixRecord.status=if($native.user_stop_latched){'STOPPED_WAITING_FOR_NATURAL_CLOSE'}else{'FAIL_WAITING_FOR_NATURAL_CLOSE'}
            Save-Matrix
            Write-Host 'Native stop/failure observed. No further stages will run; waiting without input or forced process closure.'
            return
        }
    }
    # A native Esc/final-publication failure must remain observable even when the
    # JSON could not be published. PASS log lines are never an acceptance source.
    $log=Join-Path $Evidence 'game.log'
    if(Test-Path -LiteralPath $log -PathType Leaf){
        $tail=$null;try{$tail=Read-SharedText $log 65536}catch{return}
        $escaped=$tail -cmatch 'LogTemp: Warning: M5VS2_NPC_REACTION_R2_USER_ABORTED frame='
        $failed=$tail -cmatch 'LogTemp: Error: M5VS2_NPC_REACTION_R2_FINAL_EVIDENCE_WRITE_FAILED '
        if($escaped -or $failed){
            $Step.native_stop_or_fail=[ordered]@{path=$log;status=$(if($escaped){'USER_ABORTED'}else{'FAIL_FINAL_EVIDENCE'});
                user_stop_latched=$escaped;observed_at=(Get-Date).ToString('o');source='explicit native stop/error marker; no PASS inference'}
            $matrixRecord.halt=$Step.native_stop_or_fail
            $matrixRecord.status=if($escaped){'STOPPED_WAITING_FOR_NATURAL_CLOSE'}else{'FAIL_WAITING_FOR_NATURAL_CLOSE'}
            Save-Matrix
            Write-Host 'Explicit native stop/final-write error observed in log. No further stages; waiting without input or forced process closure.'
        }
    }
}
function Invoke-MatrixChild([string]$Script,[string[]]$Arguments,[string]$Pattern,[string]$Stage,[switch]$WatchRuntime){
    Assert-NoEngine;Assert-Frozen
    $before=@{};foreach($dir in Get-ChildItem -LiteralPath $matrixRuntime -Directory -Filter $Pattern){$before[$dir.FullName]=$true}
    $stdout=Join-Path $matrixOut ($Stage+'_stdout.log');$stderr=Join-Path $matrixOut ($Stage+'_stderr.log')
    $childArgs=@('-NoProfile','-NonInteractive','-File',(Quote-Arg $Script))+$Arguments
    $child=Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $childArgs -WorkingDirectory $matrixWork -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null=$child.Handle
    $step=[ordered]@{stage=$Stage;script=$Script;arguments=$Arguments;process_id=$child.Id;started_at=(Get-Date).ToString('o');
        stdout=$stdout;stderr=$stderr;exit_code=$null;evidence_directory=$null;native_stop_or_fail=$null;ended_at=$null}
    $script:matrixCurrent.steps+=,$step;Save-Matrix
    $evidence=$null
    while(-not $child.WaitForExit(1000)){
        if(-not $evidence){
            $new=@(Get-ChildItem -LiteralPath $matrixRuntime -Directory -Filter $Pattern|Where-Object {-not $before.ContainsKey($_.FullName)})
            if($new.Count -eq 1){$evidence=$new[0].FullName;$step.evidence_directory=$evidence;Save-Matrix}
        }
        if($WatchRuntime){Read-MatrixNativeStop $evidence $step}
    }
    $child.WaitForExit();$child.Refresh();$step.exit_code=$child.ExitCode;$step.ended_at=(Get-Date).ToString('o')
    $new=@(Get-ChildItem -LiteralPath $matrixRuntime -Directory -Filter $Pattern|Where-Object {-not $before.ContainsKey($_.FullName)})
    if($new.Count -eq 1){$evidence=$new[0].FullName;$step.evidence_directory=$evidence}
    if($WatchRuntime){Read-MatrixNativeStop $evidence $step};Save-Matrix
    Require-Matrix ($new.Count -eq 1) ('Missing or ambiguous new evidence for '+$Stage)
    Require-Matrix (-not $step.native_stop_or_fail) ('Native stop/failure: '+$Stage)
    Require-Matrix ($step.exit_code -eq 0) ('Child wrapper failed: '+$Stage+'; see '+$stderr)
    Assert-NoEngine;Assert-Frozen
    return $evidence
}
function Add-RecoveryObservations($Author,$Runtime,[string]$RuntimePath){
    for($i=0;$i -lt 2;$i++){
        $letter=$matrixCurrent.pair.Substring($i,1);$stable='M5VS2_'+$letter
        foreach($name in @('ordinary_auto_recovery','authored_getup_completed')){
            $checks=@($Runtime.checks|Where-Object {$_.check -ceq $name -and $_.specimen_index -eq $i})
            Require-Matrix ($checks.Count -eq 1 -and $checks[0].status -ceq 'PASS') ('Missing native completed-recovery check: '+$stable+'/'+$name)
        }
        $shots=@($Runtime.captures|Where-Object {$_.label -ceq 'AutomaticRecovery' -and $_.state_at_request.stable_id -ceq $stable})
        Require-Matrix ($shots.Count -eq 1 -and $shots[0].status -ceq 'PASS') ('Missing actual recovery capture: '+$stable)
        $state=$shots[0].state_at_request;$getup=$state.getup
        Require-Matrix ($getup.authored_getup_completions -ge 1 -and $getup.getup_montage_position_s -ge 4.5) ('Incomplete real get-up: '+$stable)
        $idle=Read-Json $matrixRecord.idle_inputs[$letter].path
        $actualPackage=Get-AnimationPackage $getup.getup_clip
        $clipMatches=@($idle.candidate_cdo.getup.get_up_clips|Where-Object {(Get-AnimationPackage $_.animation) -ceq $actualPackage})
        Require-Matrix ($clipMatches.Count -eq 1 -and $clipMatches[0].direction -cin @('SUPINE','PRONE','LEFT','RIGHT')) ('Actual clip not uniquely mapped to final CDO: '+$stable)
        $matrixRecord.observations+=,[ordered]@{run=$matrixCurrent.order;stable_id=$stable;sample=$letter;site=$matrixCurrent.site;
            requested_impact_direction=$matrixCurrent.direction;actual_getup_direction=$clipMatches[0].direction;actual_getup_clip=$getup.getup_clip;actual_getup_package=$actualPackage;
            montage_position_s=$getup.getup_montage_position_s;authored_getup_completions=$getup.authored_getup_completions;
            idle_reload=$matrixRecord.idle_inputs[$letter].path;runtime_report=$RuntimePath;capture=$shots[0].file;visual_acceptance='USER_REVIEW'}
    }
}
Save-Matrix
try{
    Assert-NoEngine
    $authorScript=Join-Path $PSScriptRoot 'ue_m5_vs2_npc_reaction_r2_author.py'
    $authorRunner=Join-Path $PSScriptRoot 'ue_m5_vs2_author_run.ps1'
    $launcher=Join-Path $PSScriptRoot 'launch_m5_vs2_npc_reaction_r2.ps1'
    $expected=@{
        $authorScript='719d4220a4dca291639352aea15fbf5d8025f0c7e6512090206ca67169bac8af';
        $authorRunner='2e3ed98adb7baef88e79ed5009104aeaaaae95bf1bdb8937e02a822d06dbe585';
        $launcher='908171c879214ebe03a5a1012ed3bb85e1ebc585304ba449a7cae5e8d2e5692c'
    }
    foreach($item in $expected.GetEnumerator()){
        Require-Matrix ((Get-FileHash -LiteralPath $item.Key).Hash -ieq $item.Value) ('Matrix was reviewed against a different wrapper/author: '+$item.Key)
    }
    $sources=@($PSCommandPath,$authorScript,$authorRunner,$launcher,
        (Join-Path $PSScriptRoot 'ue_m5_vs2_npc_cast_review_author.py'),
        (Join-Path $PSScriptRoot 'ue_m5_vs2_npc_idle_r2_author.py'),
        (Join-Path $matrixWork 'HarborCity/Source/HarborCity/M5VS2/HCM5VS2NPCReactionReviewDirector.h'),
        (Join-Path $matrixWork 'HarborCity/Source/HarborCity/M5VS2/HCM5VS2NPCReactionReviewDirector.cpp'),
        (Join-Path $matrixWork 'HarborCity/Source/HarborCity/M5VS2/HCM5VS2NPCGameplayEditor.cpp'),
        (Join-Path $matrixWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'))
    foreach($path in $sources){$matrixRecord.source_sha256[$path]=(Get-FileHash -LiteralPath $path).Hash}
    foreach($letter in @('Q','R','J','T','U','V','W','X')){
        $candidates=@(Get-ChildItem -LiteralPath $matrixRuntime -Directory -Filter ('author_*_NPCIdleR2'+$letter+'Reload')|Sort-Object Name -Descending)
        $picked=$null
        foreach($dir in $candidates){
            $path=Join-Path $dir.FullName 'author_result.json';$commandPath=Join-Path $dir.FullName 'commandlet.json'
            if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or -not(Test-Path -LiteralPath $commandPath -PathType Leaf)){continue}
            $idle=Read-Json $path;$command=Read-Json $commandPath
            if($idle.status -ceq 'PASS' -and $idle.fresh_process_disk_readback -ceq 'PASS' -and $command.status -ceq 'PASS' -and $command.exit_code -eq 0){
                $picked=[ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash;commandlet=$commandPath;
                    blueprint=$idle.candidate_blueprint;assets=@($idle.assets)};break
            }
        }
        Require-Matrix ($null -ne $picked) ('No complete native IdleReload for '+$letter)
        Require-Matrix ($idle.phase -ceq ('NPCIdleR2'+$letter+'Reload') -and $idle.sample -ceq $letter -and $idle.assets.Count -eq 4) ('Incorrect final candidate identity: '+$letter)
        Require-Matrix ($idle.candidate_blueprint -cmatch ('^/Game/HarborCity/M5VS2/NPC/AvatarSample_'+$letter+'/IdleR2/Batch_[0-9a-f]{12}/BP_NPCIdleR2_'+$letter+'$')) 'Not a private final idle Blueprint.'
        $matrixRecord.idle_inputs[$letter]=$picked
    }
    Assert-Frozen;$matrixRecord.status='RUNNING';Save-Matrix
    foreach($plan in $matrixPlan){
        $script:matrixCurrent=[ordered]@{order=$plan.order;phase=$plan.phase;site=$plan.site;pair=$plan.pair;direction=$plan.direction;status='RUNNING';steps=@();author_report=$null;runtime_report=$null}
        $matrixRecord.runs+=,$matrixCurrent;Save-Matrix
        $authorDir=Invoke-MatrixChild $authorRunner @('-ScriptPath',(Quote-Arg $authorScript),'-Phase',$plan.phase) ('author_*_'+$plan.phase) ('{0:D2}_author' -f $plan.order)
        $authorPath=Join-Path $authorDir 'author_result.json';$author=Read-Json $authorPath;$command=Read-Json (Join-Path $authorDir 'commandlet.json')
        Require-Matrix ($author.status -ceq 'PASS' -and $command.status -ceq 'PASS' -and $command.exit_code -eq 0 -and
            $author.phase -ceq $plan.phase -and $author.roster_version -ceq 'FINAL_IDLE_R2_PAIR' -and $author.site -ceq $plan.site -and $author.review_pair -ceq $plan.pair) 'Exact new private-pair author failed.'
        foreach($character in $plan.pair.ToCharArray()){
            $letter=[string]$character;$idleInput=$author.inputs.('NPCIdle_'+$letter);$pinned=$matrixRecord.idle_inputs[$letter]
            Require-Matrix ([IO.Path]::GetFullPath($idleInput.path) -ieq [IO.Path]::GetFullPath($pinned.path) -and $idleInput.sha256 -ieq $pinned.sha256) ('Author selected a different IdleReload: '+$letter)
        }
        $matrixCurrent.author_report=$authorPath;Save-Matrix
        $gameDir=Invoke-MatrixChild $launcher @('-AuthorReport',(Quote-Arg $authorPath),'-Direction',$plan.direction,'-WaitForExit') ('*_npcreaction_'+$plan.site+'_'+$plan.pair+'_'+$plan.direction+'_game') ('{0:D2}_runtime' -f $plan.order) -WatchRuntime
        $launch=Read-Json (Join-Path $gameDir 'launch.json')
        Require-Matrix ($launch.status -ceq 'PASS' -and $launch.runtime_status -ceq 'PASS' -and $launch.exit_code -eq 0 -and $launch.stop_latched -eq $false -and
            $launch.author_report -ieq $authorPath -and $launch.author_sha256 -ieq (Get-FileHash -LiteralPath $authorPath).Hash) 'Exact runtime/exit/author evidence failed.'
        $runtimePath=[IO.Path]::GetFullPath($launch.runtime_report)
        Require-Matrix ($runtimePath.StartsWith($gameDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Runtime report outside its new directory.'
        $runtime=Read-Json $runtimePath
        Require-Matrix ($runtime.status -ceq 'PASS' -and $runtime.user_stop_latched -eq $false -and $runtime.map -ceq $author.map -and
            $runtime.site -ceq $plan.site -and $runtime.review_pair -ceq $plan.pair -and $runtime.requested_impact_direction -ceq $plan.direction -and
            $runtime.roster_version -ceq 'FINAL_IDLE_R2_PAIR' -and $runtime.captures.Count -eq 10 -and
            $runtime.actual_vehicle_contact_tested -eq $false -and $runtime.os_input_used -eq $false) 'Native result identity/coverage differs.'
        Add-RecoveryObservations $author $runtime $runtimePath
        $matrixCurrent.runtime_report=$runtimePath;$matrixCurrent.status='PASS';$matrixRecord.completed_run_count++;Save-Matrix
    }
    $people=@($matrixRecord.observations.sample|Sort-Object -Unique)
    $directions=@($matrixRecord.observations.actual_getup_direction|Sort-Object -Unique)
    $missing=@('SUPINE','PRONE','LEFT','RIGHT')|Where-Object {$_ -cnotin $directions}
    $matrixRecord.coverage=[ordered]@{completed_runs=$matrixRecord.completed_run_count;people=$people;people_count=$people.Count;
        sites=@($matrixRecord.observations.site|Sort-Object -Unique);actual_getup_directions=$directions;missing_getup_directions=@($missing);
        four_direction_status=$(if(@($missing).Count -eq 0){'PASS_NATIVE_COMPLETED_CLIPS'}else{'NOT_RUN_MISSING_POSES'});
        obstacle_contact_manifolds='NOT_RUN';penetration_depth_measurement='NOT_RUN';
        complete_person_site_pose_cross_product='NOT_RUN';all_art_acceptance='USER_REVIEW'}
    Require-Matrix ($people.Count -eq 8 -and $matrixRecord.completed_run_count -eq 8) 'Incomplete planned people/run coverage.'
    $matrixRecord.status=if(@($missing).Count -eq 0){'PASS_NATIVE_MATRIX'}else{'PARTIAL_COVERAGE'}
}catch{
    if($null -ne $matrixCurrent -and $matrixCurrent.status -ceq 'RUNNING'){$matrixCurrent.status='FAIL_OR_STOPPED'}
    $matrixRecord.status=if($matrixRecord.halt -and $matrixRecord.halt.user_stop_latched){'STOPPED'}else{'FAIL'}
    $matrixRecord.error=$_.Exception.Message
}finally{
    $matrixRecord.ended_at=(Get-Date).ToString('o');Save-Matrix
}
Write-Output "Reaction matrix evidence: $matrixFile"
Write-Output ($matrixRecord|ConvertTo-Json -Depth 8)
if($matrixRecord.status -ceq 'PASS_NATIVE_MATRIX'){exit 0}
if($matrixRecord.status -ceq 'PARTIAL_COVERAGE'){exit 2}
exit 1
