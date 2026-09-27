#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
$doc=Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/HarborCity_M5_VS3/R2'
$statePath=Join-Path $doc 'postgate/FINAL_SUITE.json'
if(Test-Path -LiteralPath $statePath){throw 'Existing final suite retained. Inspect its checkpoint; do not repeat full regression.'}
$state=[ordered]@{status='RUNNING';executable=$Executable;runs=@()}
function Save-State { $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $statePath -Encoding utf8 }
function Invoke-Run([string]$Mode,[string]$Label,[int]$Record=0,[string]$Quality='High',[string]$UserDirectory='',[string]$Slot=''){
 $before=@(Get-ChildItem -LiteralPath $doc -Directory | Select-Object -ExpandProperty FullName)
 $state.current=$Label;Save-State
 & (Join-Path $PSScriptRoot 'run_m5_vs3_r2.ps1') -Mode $Mode -Executable $Executable -RecordSeconds $Record -Quality $Quality -ReuseUserDirectory $UserDirectory -ReuseTestSlot $Slot | Out-Null
 $created=@(Get-ChildItem -LiteralPath $doc -Directory | Where-Object {$_.FullName -notin $before -and $_.Name.StartsWith($Mode+'_')})
 if($created.Count -ne 1){throw "Cannot bind $Label"}
 $path=$created[0].FullName;$exit=Get-Content -LiteralPath (Join-Path $path 'exit.json') -Raw|ConvertFrom-Json
 $state.runs+=@{label=$Label;mode=$Mode;path=$path;exit_status=$exit.status;exit_code=$exit.exit_code};Save-State
 Write-Output "Completed $Label : $path ; exit=$($exit.exit_code)"
 if($exit.status -ne 'EXITED' -or $exit.exit_code -ne 0){throw "Runtime failed at $Label; preserve evidence"}
}
function Get-Slot([string]$Path){$l=Get-Content -LiteralPath (Join-Path $Path 'launch.json') -Raw|ConvertFrom-Json;return ($l.arguments|Where-Object {$_.StartsWith('-HCM1SaveSlot=')}).Substring(14)}
try {
 Save-State
 Invoke-Run 'm5_vs3_r2_showcase' 'STANDALONE_SHOWCASE' 300
 $show=$state.runs[-1].path
 Invoke-Run 'm5_vs3_r2_reopen' 'ARMORY_FRESH_PROCESS_LOAD' 0 'High' (Join-Path $show 'User') (Get-Slot $show)
 Invoke-Run 'm5_vs3_r2_armory' 'STANDALONE_WEAPON_STILLS'
 Invoke-Run 'perf' 'PERFORMANCE_HIGH' 0 'High'
 Invoke-Run 'perf' 'PERFORMANCE_EPIC' 0 'Epic'
 Invoke-Run 'm5_full' 'FULL_REGRESSION_ONCE'
 $full=$state.runs[-1].path
 Invoke-Run 'm5_reopen' 'STORY_FRESH_PROCESS_LOAD' 0 'High' (Join-Path $full 'User') (Get-Slot $full)
 $state.status='EXECUTION_COMPLETE_RESULTS_REQUIRE_REVIEW';Save-State
} catch {
 $state.status='INTERRUPTED_REVIEW_CHECKPOINT_BEFORE_RESUME';$state.error=$_.Exception.Message;Save-State;throw
}
