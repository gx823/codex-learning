# R3 prescribed independent-package checks; resumable, no functional reruns.
param([Parameter(Mandatory=$true)][string]$Candidate)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'docs/HarborCity_M5_VS3/R3'
$manifest=Get-Content -LiteralPath (Join-Path $Candidate 'package_manifest.json') -Raw|ConvertFrom-Json
if($manifest.status -ne 'PASS'){throw 'A verified new package is required'}
$exe=$manifest.actual_executable
$statePath=Join-Path $out 'FINAL_SUITE.json'
$state=if(Test-Path -LiteralPath $statePath){Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json -AsHashtable}else{@{status='RUNNING';candidate=$Candidate;executable=$exe;runs=@()}}
if($state.candidate -ne $Candidate){throw 'Existing suite belongs to another package'}
$jobs=@(
 @{name='full_once';Mode='m5_full'},
 @{name='sword_matrix';Mode='m5_vs3_r2_matrix'},
 @{name='fallback_20shots';Mode='m5_vs3_r3_b';BowFinal=$true;RecordSeconds=100},
 @{name='weapon_closeups';Mode='m5_vs3_r3_b';WeaponCloseups=$true;RecordSeconds=38},
 @{name='performance_high';Mode='perf';Quality='High'},
 @{name='performance_epic';Mode='perf';Quality='Epic'},
 @{name='final_showcase';Mode='m5_vs3_r2_showcase';R3Showcase=$true;RecordSeconds=350},
 @{name='slow_in_game';Mode='m5_vs3_r3_b';Slow=$true;RecordSeconds=120}
)
function Save-State{$state|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $statePath -Encoding utf8}
foreach($job in $jobs){
 if(@($state.runs|Where-Object name -eq $job.name).Count){continue}
 foreach($d in @('D','E')){if((Get-PSDrive -Name $d).Free -lt 30GB){$state.status='BLOCKED_DISK';Save-State;throw '30 GiB minimum reached'}}
 if(Test-Path -LiteralPath (Join-Path $out 'STOP_DESKTOP_CHAIN')){throw 'User stop sentinel: no further launches'}
 $state.current=$job.name;Save-State
 $params=@{Executable=$exe};foreach($key in $job.Keys){if($key -ne 'name'){$params[$key]=$job[$key]}}
 & (Join-Path $PSScriptRoot 'run_m5_vs3_r3_b.ps1') @params *> (Join-Path $out ('final_'+$job.name+'.log'))
 $active=Get-Content -LiteralPath (Join-Path $out 'ACTIVE_RUN.json') -Raw|ConvertFrom-Json
 $entry=@{name=$job.name;run=$active.run;status=$active.status;exit_code=$active.exit_code;recording=$active.recording}
 $state.runs+=,$entry;Save-State
 & 'D:/python/python.exe' (Join-Path $PSScriptRoot 'collect_m5_vs3_r3_run.py') $active.run
 if($active.status -ne 'EXITED' -or $active.exit_code -ne 0){$state.status='NEEDS_INSPECTION';Save-State;throw 'Process/window failure; do not repeat completed jobs'}
}
$state.status='COMPLETE';$state.current=$null;Save-State
