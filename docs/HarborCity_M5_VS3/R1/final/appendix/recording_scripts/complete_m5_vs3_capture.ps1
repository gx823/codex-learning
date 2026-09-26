#requires -Version 7.0
param([Parameter(Mandatory=$true)][datetime]$Started,[Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
$workspace=Split-Path $PSScriptRoot -Parent
$root=Join-Path $workspace 'docs/HarborCity_M5_VS3/recordings'
$python='C:/Users/HP/.codex/runtimes/scientific-skills/Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)){throw 'Existing media Python unavailable; no automatic install'}
$captures=@(Get-ChildItem -LiteralPath $root -Filter capture.json -Recurse | Where-Object {$_.LastWriteTime -ge $Started -and $_.Directory.Parent.FullName -eq [IO.Path]::GetFullPath($root)})
if($captures.Count -ne 1){throw "Expected exactly one completed recording; found $($captures.Count). Preserve frames for diagnosis."}
$capture=$captures[0]
$output=Join-Path $capture.Directory.FullName 'capture.mp4'
if(Test-Path -LiteralPath $output){throw 'Existing MP4 retained; refusing overwrite'}
$env:PYTHONIOENCODING='utf-8'
& $python (Join-Path $PSScriptRoot 'encode_m5_vs3_r1_final.py') $capture.FullName $output
if($LASTEXITCODE -ne 0){throw 'Encoding/decode verification failed; frames retained, no further recording should start.'}
$qa=Get-Content -LiteralPath (Join-Path $capture.Directory.FullName 'capture.qa.json') -Raw | ConvertFrom-Json
if($qa.decode_validation -ne 'PASS_FULL_VIDEO_AND_AUDIO_DECODE'){throw 'Missing playback decode verification'}
if($qa.audio_signal_status -eq 'FAIL_SILENT_NATIVE_CAPTURE'){Write-Warning 'Native audio was silent: recording audio FAIL preserved; MP4 decodes correctly and temporary frames were cleaned.'}
[ordered]@{executable=$Executable;capture=$capture.FullName;video=$output;started=$Started.ToString('o');temporary_frames_removed=$qa.temporary_frames_removed} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $capture.Directory.FullName 'run_binding.json') -Encoding utf8
