#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ScriptPath,[Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_]+$')][string]$Phase)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$script=[IO.Path]::GetFullPath($ScriptPath)
if([IO.Path]::GetDirectoryName($script) -ine $PSScriptRoot -or [IO.Path]::GetFileName($script) -notmatch '^ue_m5_vs3_[A-Za-z0-9_]+\.py$'){throw 'Project VS3 author scripts only'}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe' OR Name='HarborCity-Win64-Shipping.exe'"){throw 'Close existing engine normally first'}
$run=Join-Path $root ('docs/HarborCity_M5_VS3/editor/'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+$Phase)
New-Item -ItemType Directory -Path $run|Out-Null
$exe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$arguments=@(('"'+$root+'/HarborCity/HarborCity.uproject"'),'-run=pythonscript',('-script="'+$script+'"'),('-M5EvidenceDir="'+$run+'"'),'-unattended','-nop4','-nosplash','-NullRHI','-language=en','-EnablePlugins=GeometryScripting','-LocalDataCachePath=D:/GameDev/Cache/Unreal/HarborCity/DDC','-ZenDataPath=D:/GameDev/Cache/Unreal/HarborCity/Zen',('-abslog="'+$run+'/editor.log"'))
$proc=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory ($root+'/HarborCity') -WindowStyle Hidden -PassThru
$record=[ordered]@{status='RUNNING';phase=$Phase;script=$script;pid=$proc.Id;arguments=$arguments;visual='NOT_RUN_NULLRHI'}
$record|ConvertTo-Json -Depth 4|Set-Content -LiteralPath ($run+'/commandlet.json') -Encoding utf8
Write-Output "VS3 author evidence: $run"
$proc.WaitForExit();$proc.Refresh();$record.exit_code=$proc.ExitCode
$report=$run+'/author_result.json'
$record.status=if($proc.ExitCode -eq 0 -and (Test-Path -LiteralPath $report) -and (Get-Content -LiteralPath $report -Raw|ConvertFrom-Json).status -eq 'PASS'){'PASS'}else{'FAIL'}
$record|ConvertTo-Json -Depth 4|Set-Content -LiteralPath ($run+'/commandlet.json') -Encoding utf8
Write-Output $record.status
if($record.status -ne 'PASS'){exit 1}
