# R4 prescribed independent-package checks; resumable, no functional reruns.
param([Parameter(Mandatory=$true)][string]$Candidate,[string[]]$RetryJob=@(),[string[]]$OnlyJob=@(),[switch]$SupersedeCandidate)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'hc_r4_stop_guard.ps1')
$out=Join-Path $root 'docs/HarborCity_M5_VS3/R4'
$manifest=Get-Content -LiteralPath (Join-Path $Candidate 'package_manifest.json') -Raw|ConvertFrom-Json
if($manifest.status -ne 'PASS'){throw 'A verified new package is required'}
$exe=$manifest.actual_executable
$statePath=Join-Path $out 'FINAL_SUITE.json'
$state=if(Test-Path -LiteralPath $statePath){Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json -AsHashtable}else{@{status='RUNNING';candidate=$Candidate;executable=$exe;runs=@()}}
if($state.candidate -ne $Candidate){
 if(-not $SupersedeCandidate){throw 'Existing suite belongs to another package; inspected transition required'}
 if(@($state.runs|Where-Object name -eq 'full_once').Count){throw 'Full regression already performed; review before changing final candidate'}
 foreach($entry in $state.runs){if(-not $entry.ContainsKey('candidate')){$entry.candidate=$state.candidate;$entry.executable=$state.executable}}
 $state.candidate_history=@($state.candidate_history)+@(@{candidate=$state.candidate;executable=$state.executable;transition=(Get-Date -Format o);reason='FP shield presentation correction; old test evidence retained with original executable, never relabelled as new-package runs.'})
 $state.candidate=$Candidate;$state.executable=$exe
}
$jobs=@(
 @{name='ads_20shots';Mode='m5_vs3_r3_b';BowFinal=$true},
 @{name='hipfire_10shots';Mode='m5_vs3_r3_b';BowFinal=$true;R4Case='hipfire'},
 @{name='fp_pairs';Mode='m5_vs3_r3_b';R4Case='pairs'},
 @{name='shield_check';Mode='m5_vs3_r3_b';R4Case='shield'},
 @{name='gun_colour';Mode='m5_vs3_r3_b';R4Case='colour'},
 @{name='gun_colour_baseline';Mode='m5_vs3_r3_b';R4Case='colourbefore'},
 @{name='hair';Mode='m5_vs3_r3_b';HairDiagnostic=$true;RecordSeconds=50},
 @{name='coverage';Mode='m5_vs3_r2_showcase';CoverageOnly=$true},
 @{name='weapon_closeups';Mode='m5_vs3_r3_b';WeaponCloseups=$true},
 @{name='sword_matrix';Mode='m5_vs3_r2_matrix'},
 @{name='performance_high';Mode='perf';Quality='High'},
 @{name='performance_epic';Mode='perf';Quality='Epic'},
 @{name='final_showcase';Mode='m5_vs3_r2_showcase';R3Showcase=$true;RecordSeconds=300},
 @{name='slow_in_game';Mode='m5_vs3_r3_b';Slow=$true;RecordSeconds=160},
 @{name='os_input';Mode='m5_vs3_r3_b';R4Case='os'},
 @{name='full_once';Mode='m5_full'}
)
function Save-State{$state|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $statePath -Encoding utf8}
foreach($job in $jobs){
 if($OnlyJob.Count -and $job.name -notin $OnlyJob){continue}
 $previous=@($state.runs|Where-Object name -eq $job.name)
 if($previous.Count -and $job.name -notin $RetryJob){
  $priorOutcome=Get-HCR4RunOutcome -Run $previous[-1].run
  if($priorOutcome.status -eq 'COMPLETE'){continue}
  throw ('Incomplete previous job requires an explicit RetryJob after inspection: '+$job.name)
 }
 foreach($d in @('D','E')){if((Get-PSDrive -Name $d).Free -lt 30GB){$state.status='BLOCKED_DISK';Save-State;throw '30 GiB minimum reached'}}
 if(Test-Path -LiteralPath (Join-Path $out 'STOP_DESKTOP_CHAIN')){throw 'User stop sentinel: no further launches'}
 $hostCheck=& (Join-Path $PSScriptRoot 'hc_r4_host_status.ps1') | ConvertFrom-Json
 if($hostCheck.foreground_process -eq 'LockApp' -or $hostCheck.foreground_pid -eq 0){$state.status='BLOCKED_LOCKED';$state.current=$job.name;Save-State;throw 'Windows locked or input desktop unavailable; no game launch or input until user unlocks'}
 if(($job.Mode -eq 'perf' -or $job.ContainsKey('RecordSeconds')) -and $hostCheck.ac_line -ne 1){$state.status='BLOCKED_POWER';$state.current=$job.name;Save-State;throw 'Known AC power required for comparable performance and final capture'}
 $state.status='RUNNING';$state.current=$job.name;Save-State
 Write-Output ('R4 JOB START '+$job.name)
 $params=@{Executable=$exe};foreach($key in $job.Keys){if($key -ne 'name'){$params[$key]=$job[$key]}}
 & (Join-Path $PSScriptRoot 'run_m5_vs3_r4.ps1') @params *> (Join-Path $out ('final_'+$job.name+'.log'))
 $active=Get-Content -LiteralPath (Join-Path $out 'ACTIVE_RUN.json') -Raw|ConvertFrom-Json
 $entry=@{name=$job.name;run=$active.run;status=$active.status;exit_code=$active.exit_code;recording=$active.recording;candidate=$Candidate;executable=$exe}
 $state.runs+=,$entry;Save-State
 & 'D:/python/python.exe' -X utf8 (Join-Path $PSScriptRoot 'collect_m5_vs3_r3_run.py') $active.run | Out-Null
 $outcome=Get-HCR4RunOutcome -Run $active.run
 $entry.test_status=$outcome.status;$entry.failures=$outcome.failures;Save-State
 Write-Output ('R4 JOB RESULT '+$job.name+' '+$outcome.status+' failures='+$outcome.failures)
 if($outcome.stopped -or $active.status -eq 'USER_ABORTED'){
  Set-HCR4DesktopStop -Reason 'Coordinator received USER_ABORTED' -Run $active.run
  $state.status='USER_ABORTED';Save-State;throw 'User stopped: no further launches or desktop operations'
 }
 if($active.status -ne 'EXITED' -or $active.exit_code -ne 0){$state.status='NEEDS_INSPECTION';Save-State;throw 'Process/window failure; do not repeat completed jobs'}
 if($outcome.status -ne 'COMPLETE'){$state.status='NEEDS_INSPECTION';Save-State;throw 'Missing or incomplete test results'}
}
$allFinished=$true
foreach($expected in $jobs){
 $done=@($state.runs|Where-Object name -eq $expected.name)
 if(-not $done.Count -or (Get-HCR4RunOutcome -Run $done[-1].run).status -ne 'COMPLETE'){$allFinished=$false;break}
}
$state.status=if($allFinished){'COMPLETE'}else{'PARTIAL_CHECKS'};$state.current=$null;Save-State
