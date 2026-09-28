# Shared by the R4 runner, coordinator and OS-input helper. Never clears a stop.
function Get-HCR4RunOutcome {
 param([string]$Run)
 $files=@(Get-ChildItem -LiteralPath (Join-Path $Run 'User/Saved/M5Tests') -Filter results.json -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending)
 if(-not $files.Count){return @{status='NO_RESULTS';failures=$null;stopped=$false}}
 try {$r=Get-Content -LiteralPath $files[0].FullName -Raw | ConvertFrom-Json} catch {return @{status='INCOMPLETE_WRITE';failures=$null;stopped=$false}}
 return @{status=$r.status;failures=$r.failures;stopped=($r.status -eq 'USER_ABORTED' -or $r.m3_escape_stop.user_stop_latched -eq $true);path=$files[0].FullName}
}
function Set-HCR4DesktopStop {
 param([string]$Reason,[string]$Run='')
 $stopFile=Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/HarborCity_M5_VS3/R4/STOP_DESKTOP_CHAIN'
 if(-not (Test-Path -LiteralPath $stopFile)){
  @{reason=$Reason;run=$Run;time=(Get-Date -Format o);resume='Explicit user resume required; archive this evidence before any new desktop operation.'}|ConvertTo-Json|Set-Content -LiteralPath $stopFile -Encoding utf8
 }
}
function Test-HCR4DesktopStopped {
 param([string]$Run='',[switch]$ReadResults)
 $stopFile=Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/HarborCity_M5_VS3/R4/STOP_DESKTOP_CHAIN'
 if(Test-Path -LiteralPath $stopFile){return $true}
 # Include the press-since-last-query bit so a short press between polls also latches.
 if(('HCR3WindowGuard' -as [type]) -and ([HCR3WindowGuard]::GetAsyncKeyState(27) -band 0x8001)){
  Set-HCR4DesktopStop -Reason 'OS Escape pressed' -Run $Run;return $true
 }
 if($ReadResults -and $Run -and (Get-HCR4RunOutcome -Run $Run).stopped){
  Set-HCR4DesktopStop -Reason 'Game viewport latched USER_ABORTED' -Run $Run;return $true
 }
 return $false
}
