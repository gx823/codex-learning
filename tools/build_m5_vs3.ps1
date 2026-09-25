#requires -Version 7.0
[CmdletBinding()]
param([ValidateSet('Editor','Game')][string]$Target='Editor',[ValidateSet('Development','Shipping')][string]$Configuration='Development',[ValidateRange(1,8)][int]$MaxParallelActions=8)
$ErrorActionPreference='Stop'
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe' OR Name='HarborCity-Win64-Shipping.exe'"){throw 'Close the existing editor/game normally before compiling.'}
$vs2Workspace=Split-Path $PSScriptRoot -Parent
$vs2Run=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$vs2Logs=Join-Path $vs2Workspace ('docs/HarborCity_M5_VS3/builds/'+$vs2Run+'_'+$Target)
$vs2Python=Join-Path $vs2Workspace 'video_build/ltx_env/Scripts/python.exe'
& $vs2Python (Join-Path $PSScriptRoot 'm5_refresh_changed_source_times.py')
if($LASTEXITCODE -ne 0){throw 'Project source content/mtime preflight failed.'}
$vs2Pwsh=(Get-Process -Id $PID).Path
& $vs2Pwsh -NoLogo -NoProfile -File (Join-Path $PSScriptRoot 'build.ps1') -Target $Target -Configuration $Configuration -EngineRoot 'E:/UE_5.8' -PythonPath $vs2Python -LogRoot $vs2Logs -MaxParallelActions $MaxParallelActions -CacheRoot 'D:/GameDev/Cache/Unreal/HarborCity'
exit $LASTEXITCODE
