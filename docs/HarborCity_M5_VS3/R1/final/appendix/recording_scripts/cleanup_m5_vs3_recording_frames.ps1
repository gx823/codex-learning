#requires -Version 7.0
param([Parameter(Mandatory=$true)][string]$Capture,[Parameter(Mandatory=$true)][string]$Evidence,[Parameter(Mandatory=$true)][string]$Reason)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/HarborCity_M5_VS3/recordings')).TrimEnd('\')
$path=[IO.Path]::GetFullPath($Capture)
if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $path -Leaf) -ne 'capture.json'){throw 'Capture outside authorized native recording root'}
$proof=[IO.Path]::GetFullPath($Evidence)
if(-not $proof.StartsWith(([IO.Path]::GetFullPath((Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/HarborCity_M5_VS3/R1/final'))+'\'),[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $proof -PathType Leaf)){throw 'Final evidence/report is required before temporary-frame cleanup'}
$dir=Split-Path $path -Parent
$record=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
$files=[Collections.Generic.List[string]]::new();$bytes=[long]0
foreach($frame in $record.frames){
 $file=[IO.Path]::GetFullPath((Join-Path $dir $frame.file))
 if((Split-Path $file -Parent) -ine $dir -or [IO.Path]::GetExtension($file) -notin @('.jpg','.png')){throw 'Unexpected frame path; no deletion performed'}
 if(Test-Path -LiteralPath $file -PathType Leaf){$files.Add($file);$bytes+=(Get-Item -LiteralPath $file).Length}
}
# All absolute paths were checked before any mutation. Native PowerShell only.
foreach($file in $files){Remove-Item -LiteralPath $file}
$result=[ordered]@{status='TEMPORARY_FRAMES_REMOVED';capture=$path;evidence=$proof;reason=$Reason;count=$files.Count;bytes=$bytes;timestamp=(Get-Date).ToString('o');retained='capture metadata, audio, final MP4, selected PNG/JPG and test reports; no assets/candidates/saves removed'}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'frame_cleanup.json') -Encoding utf8
$result | ConvertTo-Json
