#requires -Version 7.0
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$ScriptPath,
 [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z_][A-Za-z0-9_]{0,47}$')][string]$Phase,
 [ValidateSet('Probe','Plan','Copy')][string]$VillageMode='Probe',
 [string]$VillageManifest,
 [string]$VillagePlan,
 [string]$FootPlacementAuthor,
 [string]$FootPlacementFixtureInput
)
$ErrorActionPreference='Stop'
$vs2Workspace=Split-Path $PSScriptRoot -Parent
$vs2Script=[IO.Path]::GetFullPath($ScriptPath)
if([IO.Path]::GetDirectoryName($vs2Script) -ine [IO.Path]::GetFullPath($PSScriptRoot) -or [IO.Path]::GetFileName($vs2Script) -notmatch '^ue_m5_vs2_[A-Za-z0-9_]+\.py$'){throw 'Only project-owned M5-VS2 author scripts are accepted.'}
$vs2Project=Join-Path $vs2Workspace 'HarborCity/HarborCity.uproject'
$vs2Editor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach($vs2File in @($vs2Script,$vs2Project,$vs2Editor)){if(-not(Test-Path -LiteralPath $vs2File -PathType Leaf)){throw "Missing $vs2File"}}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){throw 'Existing engine/game process; finish it normally before authoring.'}
$vs2Run=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$vs2Evidence=Join-Path $vs2Workspace ('docs/HarborCity_M5_VS2/editor_runtime/author_'+$vs2Run+'_'+$Phase)
New-Item -ItemType Directory -Path $vs2Evidence|Out-Null
$vs2Cache='D:/GameDev/Cache/Unreal/HarborCity'
$vs2Args=@(('"'+$vs2Project+'"'),'-run=pythonscript',('-script="'+$vs2Script+'"'),('-M5EvidenceDir="'+$vs2Evidence+'"'),"-M5AuthorPhase=$Phase",'-unattended','-nop4','-nosplash','-NullRHI','-language=en','-EnablePlugins=GeometryScripting',"-LocalDataCachePath=$vs2Cache/DDC","-ZenDataPath=$vs2Cache/Zen",('-abslog="'+(Join-Path $vs2Evidence 'editor.log')+'"'))
if($VillageMode -ne 'Probe' -or $VillageManifest -or $VillagePlan){
 if([IO.Path]::GetFileName($vs2Script) -ne 'ue_m5_vs2_village_selection.py'){throw 'Village options require the exact native Village selection script.'}
 if($VillageMode -in @('Plan','Copy') -and -not $VillageManifest){throw 'Village Plan/Copy requires the reviewed manifest.'}
 if($VillageMode -eq 'Copy' -and -not $VillagePlan){throw 'Village Copy requires a successful native Plan report.'}
 $vs2Args+="-M5VillageMode=$VillageMode"
 $vs2DocRoot=[IO.Path]::GetFullPath((Join-Path $vs2Workspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
 foreach($vs2Item in @(@('M5VillageManifest',$VillageManifest),@('M5VillagePlan',$VillagePlan))){
  if(-not $vs2Item[1]){continue}
  $vs2Document=[IO.Path]::GetFullPath($vs2Item[1])
  if(-not $vs2Document.StartsWith($vs2DocRoot,[StringComparison]::OrdinalIgnoreCase) -or $vs2Document.Contains('"') -or -not(Test-Path -LiteralPath $vs2Document -PathType Leaf) -or [IO.Path]::GetExtension($vs2Document) -ne '.json'){throw 'Village evidence must be an existing JSON under this stage report directory.'}
  $vs2Args+=('-'+$vs2Item[0]+'="'+$vs2Document+'"')
 }
}
if($FootPlacementAuthor){
 if([IO.Path]::GetFileName($vs2Script) -cne 'ue_m5_vs2_footplacement_author.py' -or $Phase -cne 'FootPlacementABReload'){
  throw 'FootPlacementAuthor is accepted only by the exact FootPlacementABReload script and phase.'
 }
 if($FootPlacementAuthor.Contains('"') -or $FootPlacementAuthor.Contains("`r") -or $FootPlacementAuthor.Contains("`n")){
  throw 'FootPlacement author path contains command-line delimiters.'
 }
 $vs2FootAuthor=[IO.Path]::GetFullPath($FootPlacementAuthor)
 $vs2FootRoot=[IO.Path]::GetFullPath((Join-Path $vs2Workspace 'docs/HarborCity_M5_VS2/editor_runtime')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
 if(-not $vs2FootAuthor.StartsWith($vs2FootRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($vs2FootAuthor) -cne 'author_result.json' -or -not(Test-Path -LiteralPath $vs2FootAuthor -PathType Leaf)){
  throw 'FootPlacement source must be actual author_result.json within this stage editor_runtime directory.'
 }
 $vs2FootSource=Get-Content -LiteralPath $vs2FootAuthor -Raw | ConvertFrom-Json
 $vs2FootCommandPath=Join-Path ([IO.Path]::GetDirectoryName($vs2FootAuthor)) 'commandlet.json'
 if(-not(Test-Path -LiteralPath $vs2FootCommandPath -PathType Leaf)){throw 'FootPlacement author commandlet result is absent.'}
 $vs2FootCommand=Get-Content -LiteralPath $vs2FootCommandPath -Raw | ConvertFrom-Json
 if($vs2FootSource.status -cne 'PASS' -or $vs2FootSource.phase -cne 'FootPlacementABAuthor' -or $vs2FootCommand.status -cne 'PASS' -or $vs2FootCommand.phase -cne 'FootPlacementABAuthor' -or $vs2FootCommand.exit_code -ne 0){
  throw 'FootPlacement author and actual completed commandlet must both be PASS.'
 }
 $vs2Args+=('-M5FootPlacementAuthor="'+$vs2FootAuthor+'"')
}elseif($Phase -ceq 'FootPlacementABReload'){
 throw 'FootPlacementABReload requires explicit FootPlacementAuthor; latest selection is forbidden.'
}
if($FootPlacementFixtureInput){
 if([IO.Path]::GetFileName($vs2Script) -cne 'ue_m5_vs2_footplacement_fixture_author.py' -or $Phase -cnotin @('FootPlacementFixtureAuthor','FootPlacementFixtureReload')){
  throw 'FootPlacementFixtureInput requires the exact private fixture script/phase.'
 }
 if($FootPlacementFixtureInput.Contains('"') -or $FootPlacementFixtureInput.Contains("`r") -or $FootPlacementFixtureInput.Contains("`n")){
  throw 'FootPlacement fixture input contains command-line delimiters.'
 }
 $vs2FixtureInput=[IO.Path]::GetFullPath($FootPlacementFixtureInput)
 $vs2FixtureRoot=[IO.Path]::GetFullPath((Join-Path $vs2Workspace 'docs/HarborCity_M5_VS2/editor_runtime')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
 if(-not $vs2FixtureInput.StartsWith($vs2FixtureRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($vs2FixtureInput) -cne 'author_result.json' -or -not(Test-Path -LiteralPath $vs2FixtureInput -PathType Leaf)){
  throw 'Fixture input must be an actual stage-owned author_result.json.'
 }
 $vs2FixtureRequiredPhase=if($Phase -ceq 'FootPlacementFixtureAuthor'){'FootPlacementABReload'}else{'FootPlacementFixtureAuthor'}
 $vs2FixtureReport=Get-Content -LiteralPath $vs2FixtureInput -Raw|ConvertFrom-Json
 $vs2FixtureCommand=Get-Content -LiteralPath (Join-Path ([IO.Path]::GetDirectoryName($vs2FixtureInput)) 'commandlet.json') -Raw|ConvertFrom-Json
 if($vs2FixtureReport.status -cne 'PASS' -or $vs2FixtureReport.phase -cne $vs2FixtureRequiredPhase -or
    $vs2FixtureCommand.status -cne 'PASS' -or $vs2FixtureCommand.phase -cne $vs2FixtureRequiredPhase -or
    $null -eq $vs2FixtureCommand.exit_code -or $vs2FixtureCommand.exit_code -ne 0){
  throw 'Private fixture input and completed commandlet must both PASS the exact required phase.'
 }
 $vs2Args+=('-M5FootPlacementFixtureInput="'+$vs2FixtureInput+'"')
}elseif($Phase -cin @('FootPlacementFixtureAuthor','FootPlacementFixtureReload')){
 throw 'FootPlacement fixture phases require explicit FootPlacementFixtureInput; no latest search.'
}
$vs2Record=[ordered]@{milestone='M5_VS2';phase=$Phase;script=$vs2Script;script_sha256=(Get-FileHash -LiteralPath $vs2Script -Algorithm SHA256).Hash;executable=$vs2Editor;arguments=$vs2Args;started_at=(Get-Date).ToString('o');status='RUNNING';visual_validation='NOT_RUN_NULLRHI';process_id=$null;exit_code=$null}
$vs2Env=@{'UE-LocalDataCachePath'="$vs2Cache/DDC";'UE-ZenDataPath'="$vs2Cache/Zen";'UE-ZenSubprocessDataPath'="$vs2Cache/Zen"};$vs2Old=@{}
try{
 foreach($vs2Key in $vs2Env.Keys){$vs2Old[$vs2Key]=[Environment]::GetEnvironmentVariable($vs2Key,'Process');[Environment]::SetEnvironmentVariable($vs2Key,$vs2Env[$vs2Key],'Process')}
 $vs2Proc=Start-Process -FilePath $vs2Editor -ArgumentList $vs2Args -WorkingDirectory (Split-Path $vs2Project -Parent) -WindowStyle Hidden -PassThru
 $vs2Record.process_id=$vs2Proc.Id;$vs2Record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $vs2Evidence 'commandlet.json') -Encoding utf8
 Write-Output "M5-VS2 author evidence: $vs2Evidence"
 $vs2Proc.WaitForExit();$vs2Proc.Refresh();$vs2Record.exit_code=$vs2Proc.ExitCode
 $vs2Report=Join-Path $vs2Evidence 'author_result.json'
 $vs2Record.author_status=if(Test-Path -LiteralPath $vs2Report -PathType Leaf){(Get-Content -LiteralPath $vs2Report -Raw|ConvertFrom-Json).status}else{'MISSING'}
 $vs2Record.status=if($vs2Proc.ExitCode -eq 0 -and $vs2Record.author_status -eq 'PASS'){'PASS'}else{'FAIL'}
}catch{$vs2Record.status='FAIL';$vs2Record.error=$_.Exception.Message;throw}
finally{
 foreach($vs2Key in $vs2Old.Keys){[Environment]::SetEnvironmentVariable($vs2Key,$vs2Old[$vs2Key],'Process')}
 $vs2Record.ended_at=(Get-Date).ToString('o');$vs2Record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $vs2Evidence 'commandlet.json') -Encoding utf8
}
$vs2Record|ConvertTo-Json -Depth 8
if($vs2Record.status -ne 'PASS'){exit 1}
exit 0
