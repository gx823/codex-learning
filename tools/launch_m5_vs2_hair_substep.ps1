#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Manifest,
 [Parameter(Mandatory=$true)][ValidateSet('On','Off')][string]$FixedSubstepping,
 [ValidateSet(30,120)][int]$FrameCap=120,[string]$ControlLaunch,
 [string]$SourceRevision,[switch]$RootAnchorCandidate,[switch]$ValidateOnly,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$hsWork='D:/科研学习/codex学习';$hsDocs=Join-Path $hsWork 'docs/HarborCity_M5_VS2'
$hsManifest=[IO.Path]::GetFullPath($Manifest)
if(-not $hsManifest.StartsWith(([IO.Path]::GetFullPath($hsDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($hsManifest) -cne 'hair_substep_manifest.json'){throw 'Require exact owned native probe manifest.'}
$hsData=Get-Content -LiteralPath $hsManifest -Raw|ConvertFrom-Json
$hsFolder=Split-Path $hsManifest -Parent;$hsAuthor=Join-Path $hsFolder 'author_result.json';$hsCommand=Get-Content -LiteralPath (Join-Path $hsFolder 'commandlet.json') -Raw|ConvertFrom-Json
if($hsData.schema -cne 'HarborCity.M5VS2.HairSubstepProbe.v1' -or $hsData.phase -cne 'HairSubstepProbe' -or $hsData.status -cne 'PASS' -or $hsCommand.phase -cne 'HairSubstepProbe' -or $hsCommand.status -cne 'PASS' -or $hsCommand.exit_code -ne 0 -or $hsData.assets_written.Count -ne 0 -or $hsData.preservation.changed.Count -ne 0){throw 'Native read-only probe not completed.'}
if((Get-FileHash -LiteralPath $hsManifest).Hash -ine (Get-FileHash -LiteralPath $hsAuthor).Hash){throw 'Manifest differs from actual native result.'}
$hsProbe=Join-Path $hsWork 'tools/ue_m5_vs2_hair_substep_probe.py'
if([IO.Path]::GetFullPath($hsCommand.script) -ine $hsProbe -or (Get-FileHash -LiteralPath $hsProbe).Hash -ine $hsCommand.script_sha256){throw 'Probe script changed.'}
if($hsData.map -cnotmatch '^/Game/HarborCity/M5VS2/HairReview/Run_[0-9a-f]{12}/L_HairReview$' -or $hsData.source_bindings.Count -lt 30){throw 'Private fixture/source closure missing.'}
. (Join-Path $PSScriptRoot 'm5_runtime_revision.ps1')
$hsAllowed=@('Source/HarborCity/M5VS2/HCM5VS2HairReviewDirector.h','Source/HarborCity/M5VS2/HCM5VS2HairReviewDirector.cpp',
 'Plugins/KawaiiPhysics/Source/KawaiiPhysics/Public/AnimNode_KawaiiPhysics.h','Plugins/KawaiiPhysics/Source/KawaiiPhysics/Private/AnimNode_KawaiiPhysicsSimulation.cpp',
 'Binaries/Win64/UnrealEditor-HarborCity.dll','Binaries/Win64/UnrealEditor-HarborCityEditor.dll',
 'Plugins/KawaiiPhysics/Binaries/Win64/UnrealEditor-KawaiiPhysics.dll') | ForEach-Object {Join-Path $hsWork ('HarborCity/'+$_)}
$hsRevision=Get-M5RuntimeRevision $SourceRevision $hsManifest $hsAllowed
if($RootAnchorCandidate -and (-not $SourceRevision -or $FixedSubstepping -cne 'On')){throw 'Anchor candidate needs the explicit new code revision and fixed stepping On.'}
foreach($hsSource in $hsData.source_bindings){
 $hsFile=[IO.Path]::GetFullPath($hsSource.path)
 $hsExpected=Get-M5EffectiveBinding $hsSource $hsRevision
 if(-not $hsFile.StartsWith(([IO.Path]::GetFullPath($hsWork)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $hsFile).Length -ne $hsExpected.bytes -or (Get-FileHash -LiteralPath $hsFile).Hash -ine $hsExpected.sha256){throw "Changed protected source: $hsFile"}
}
if($FixedSubstepping -eq 'Off'){
 if(-not $ControlLaunch){throw 'Off requires a completed same-fixture On launch; never auto-start a pair.'}
 $hsControlFile=[IO.Path]::GetFullPath($ControlLaunch)
 if(-not $hsControlFile.StartsWith(([IO.Path]::GetFullPath($hsDocs)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($hsControlFile) -cne 'launch.json'){throw 'Require owned A launch record.'}
 $hsControl=Get-Content -LiteralPath $hsControlFile -Raw|ConvertFrom-Json
 if($hsControl.schema -cne 'HarborCity.M5VS2.HairSubstepLaunch.v1' -or $hsControl.status -cne 'PASS_CAPTURE_ONLY' -or $hsControl.fixed_substepping -cne 'On' -or $hsControl.frame_cap -ne $FrameCap -or $hsControl.stop_latched -ne $false -or $hsControl.exit_code -ne 0 -or $hsControl.manifest_sha256 -ine (Get-FileHash -LiteralPath $hsManifest).Hash){throw 'A is not a completed matching condition; stop is never bypassed.'}
}elseif($ControlLaunch){throw 'On is the independent control and takes no previous launch.'}
if($ValidateOnly){[ordered]@{status='PASS_STATIC_GUARDS_ONLY';runtime='NOT_RUN';map=$hsData.map;fixed_substepping=$FixedSubstepping;frame_cap=$FrameCap;scope='ALL_KAWAII_NODES_THIS_PROCESS';manifest=$hsManifest}|ConvertTo-Json;exit 0}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){throw 'Existing engine/game: finish normally first.'}
$hsExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe';$hsProject=Join-Path $hsWork 'HarborCity/HarborCity.uproject'
$hsRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$hsOut=Join-Path $hsDocs ('editor_runtime/'+$hsRun+'_hair_substep_'+$FixedSubstepping)
New-Item -ItemType Directory -Path $hsOut|Out-Null
$hsCache='D:/GameDev/Cache/Unreal/HarborCity';$hsBool=if($FixedSubstepping -ceq 'On'){'True'}else{'False'};$hsExpected=if($FixedSubstepping -ceq 'On'){1}else{0}
$hsCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0',"t.MaxFPS $FrameCap")
$hsArgs=@(('"'+$hsProject+'"'),$hsData.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2HairReview','-M5VS2HairSubstepAudit','-M5VS2AutoQuit',"-M5VS2FixedSubstepExpected=$hsExpected",('-ini:Engine:[/Script/KawaiiPhysics.KawaiiPhysicsDeveloperSettings]:bUseFixedSubstepping='+$hsBool),('-M5VS2EvidenceDir="'+$hsOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_HairSubstep_Test_'+$hsRun),('-abslog="'+(Join-Path $hsOut 'game.log')+'"'),('-ExecCmds="'+($hsCommands -join ',')+'"'),"-LocalDataCachePath=$hsCache/DDC","-ZenDataPath=$hsCache/Zen")
$hsRecord=[ordered]@{schema='HarborCity.M5VS2.HairSubstepLaunch.v1';status='STARTING';runtime_layer='EDITOR_GAME_NOT_PACKAGE';input_scope='NATIVE_FUNCTIONS_NOT_OS';scope='Temporary process-wide ini; all Kawaii nodes, not per-node; no asset/config mutation';fixed_substepping=$FixedSubstepping;frame_cap=$FrameCap;fps_acceptance='REQUEST_ONLY_ACTUAL_DT_IN_REPORT';manifest=$hsManifest;manifest_sha256=(Get-FileHash -LiteralPath $hsManifest).Hash;control_launch=$ControlLaunch;arguments=$hsArgs;map=$hsData.map;started_at=(Get-Date).ToString('o');process_id=$null;exit_code=$null;stop_latched=$null;runtime_report=$null;visual='USER_REVIEW';runtime='NOT_RUN'}
$hsRecordPath=Join-Path $hsOut 'launch.json';$hsRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $hsRecordPath -Encoding utf8
if($RootAnchorCandidate){$hsArgs+='-M5VS2HairRootAnchorCandidate';$hsRecord.arguments=$hsArgs;$hsRecord.root_anchor_candidate=$true}
if($SourceRevision){$hsRecord.source_revision=[ordered]@{path=[IO.Path]::GetFullPath($SourceRevision);sha256=(Get-FileHash -LiteralPath $SourceRevision).Hash;bindings=@($hsRevision.Values)}}
$hsEnv=@{'UE-LocalDataCachePath'="$hsCache/DDC";'UE-ZenDataPath'="$hsCache/Zen";'UE-ZenSubprocessDataPath'="$hsCache/Zen"};$hsOld=@{}
try{
 foreach($hsKey in $hsEnv.Keys){$hsOld[$hsKey]=[Environment]::GetEnvironmentVariable($hsKey,'Process');[Environment]::SetEnvironmentVariable($hsKey,$hsEnv[$hsKey],'Process')}
 # The explicitly authorized bounded diagnostic is a visible game window.
 $hsProcess=Start-Process -FilePath $hsExe -ArgumentList $hsArgs -WorkingDirectory (Split-Path $hsProject -Parent) -WindowStyle Normal -PassThru
 $null=$hsProcess.Handle;$hsRecord.process_id=$hsProcess.Id;$hsRecord.status='STARTED'
}catch{$hsRecord.status='FAIL';$hsRecord.error=$_.Exception.Message;throw}
finally{foreach($hsKey in $hsOld.Keys){[Environment]::SetEnvironmentVariable($hsKey,$hsOld[$hsKey],'Process')};$hsRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $hsRecordPath -Encoding utf8}
Write-Output "Hair substep evidence: $hsOut"
if($WaitForExit){
 $hsProcess.WaitForExit();$hsProcess.Refresh();$hsRecord.exit_code=$hsProcess.ExitCode
 $hsResults=@(Get-ChildItem -LiteralPath $hsOut -Recurse -File -Filter 'hair_review.json')
 $hsGood=$false
 if($hsResults.Count -eq 1){
  $hsActual=Get-Content -LiteralPath $hsResults[0].FullName -Raw|ConvertFrom-Json;$hsRecord.runtime_report=$hsResults[0].FullName;$hsRecord.stop_latched=$hsActual.user_stop_latched;$hsRecord.runtime=$hsActual.status
  $hsGood=$hsActual.status -ceq 'PASS' -and $hsActual.captures.Count -eq 7 -and $hsActual.user_stop_latched -eq $false -and $hsActual.substep_audit.settings_readback_valid -eq $true -and $hsActual.substep_audit.actual_fixed_substepping -eq ($FixedSubstepping -ceq 'On') -and $hsActual.substep_audit.frames.Count -gt 100 -and $hsActual.substep_audit.frames.Count -le 4500
  foreach($hsFrame in $hsActual.substep_audit.frames){if($hsFrame.status -cne 'PASS_READBACK_ONLY' -or $hsFrame.actual_fixed_substepping -ne ($FixedSubstepping -ceq 'On')){$hsGood=$false}}
  foreach($hsCapture in $hsActual.captures){if($hsCapture.status -cne 'PASS' -or -not(Test-Path -LiteralPath $hsCapture.file -PathType Leaf)){$hsGood=$false}}
 }
 $hsChanged=@();foreach($hsSource in $hsData.source_bindings){$hsExpected=Get-M5EffectiveBinding $hsSource $hsRevision;if((Get-FileHash -LiteralPath $hsSource.path).Hash -ine $hsExpected.sha256){$hsChanged+=$hsSource.path}}
 $hsRecord.changed_protected_sources=$hsChanged;$hsRecord.ended_at=(Get-Date).ToString('o')
 $hsRecord.status=if($hsProcess.ExitCode -eq 0 -and $hsGood -and $hsChanged.Count -eq 0){'PASS_CAPTURE_ONLY'}else{'FAIL_OR_STOPPED'}
 $hsRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $hsRecordPath -Encoding utf8
 if($hsRecord.status -cne 'PASS_CAPTURE_ONLY'){exit 1}
}
