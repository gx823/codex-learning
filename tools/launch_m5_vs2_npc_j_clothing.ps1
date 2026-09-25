#requires -Version 7.0
param([Parameter(Mandatory=$true)][string]$AuthorReport,[ValidateSet('Forward','Backward','Left','Right')][string]$Direction='Forward',[string]$RuntimeDllSha256='')
$ErrorActionPreference='Stop'
$jcWork=Split-Path $PSScriptRoot -Parent
$jcDoc=Join-Path $jcWork 'docs/HarborCity_M5_VS2/editor_runtime'
$jcReport=[IO.Path]::GetFullPath($AuthorReport)
if(-not $jcReport.StartsWith(([IO.Path]::GetFullPath($jcDoc)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Expected this stage author report.'}
$jcAuthor=Get-Content -LiteralPath $jcReport -Raw|ConvertFrom-Json
$jcCommand=Get-Content -LiteralPath (Join-Path (Split-Path $jcReport) 'commandlet.json') -Raw|ConvertFrom-Json
if($jcAuthor.status -cne 'PASS' -or $jcAuthor.phase -cnotin @('NPCJClothingApply','NPCJClothEnvironmentAB') -or $jcCommand.status -cne 'PASS' -or $jcCommand.exit_code -ne 0 -or -not $jcAuthor.fixture){throw 'Native cloth author/fixture has not completed.'}
foreach($jcRow in $jcAuthor.assets){
 $jcFile=Join-Path (Join-Path $jcWork 'HarborCity/Content') ($jcRow.path.Substring(6)+'.uasset')
 if((Get-FileHash -LiteralPath $jcFile).Hash -ine $jcRow.sha256){throw "Changed new cloth asset: $jcFile"}
}
$jcRuntimeDll=Join-Path $jcWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$jcRuntimeHash=(Get-FileHash -LiteralPath $jcRuntimeDll).Hash
$jcRuntimeAuthorHash=$null
foreach($jcProp in $jcAuthor.protected_sources.PSObject.Properties){
 if([IO.Path]::GetFullPath($jcProp.Name) -ieq [IO.Path]::GetFullPath($jcRuntimeDll)){
  $jcRuntimeAuthorHash=$jcProp.Value
  if($jcRuntimeHash -ine $jcProp.Value -and ($RuntimeDllSha256 -notmatch '^[A-Fa-f0-9]{64}$' -or $jcRuntimeHash -ine $RuntimeDllSha256)){throw 'Changed runtime requires its explicit current DLL SHA256; asset author evidence remains bound to its original runtime.'}
 }elseif((Get-FileHash -LiteralPath $jcProp.Name).Hash -ine $jcProp.Value){throw "Changed author source: $($jcProp.Name)"}
}
if((Get-FileHash -LiteralPath $jcAuthor.fixture.path).Hash -ine $jcAuthor.fixture.sha256){throw 'Cloth fixture changed.'}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity -ErrorAction SilentlyContinue){throw 'Finish existing engine/game normally first.'}
$jcRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$jcOut=Join-Path $jcDoc ($jcRun+'_j_chaos_clothing_game');New-Item -ItemType Directory -Path $jcOut|Out-Null
$jcProject=Join-Path $jcWork 'HarborCity/HarborCity.uproject';$jcCache='D:/GameDev/Cache/Unreal/HarborCity'
$jcArgs=@(('"'+$jcProject+'"'),$jcAuthor.fixture.map,'-game','-unattended','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
 '-M5VS2NPCReactionReview','-M5VS2NPCReactionIdlePair=JT','-M5VS2NPCJChaosClothReview',('-M5NPCFallDirection='+$Direction),'-M5VS2AutoQuit',
 ('-M5VS2EvidenceDir="'+$jcOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_Cloth_Test_'+$jcRun),('-abslog="'+(Join-Path $jcOut 'game.log')+'"'),
 '-ExecCmds="sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0"',
 "-LocalDataCachePath=$jcCache/DDC","-ZenDataPath=$jcCache/Zen")
$jcRecord=[ordered]@{status='STARTING';started_at=(Get-Date).ToString('o');author=$jcReport;author_sha256=(Get-FileHash -LiteralPath $jcReport).Hash;runtime_dll=$jcRuntimeDll;runtime_dll_sha256=$jcRuntimeHash;author_runtime_dll_sha256=$jcRuntimeAuthorHash;arguments=$jcArgs;input_scope='FUNCTION_CALL_GAMEPLAY_NOT_OS_NOT_ACTUAL_VEHICLE_CONTACT';visual='USER_REVIEW';runtime='NOT_RUN';stop_latched=$null}
$jcLaunch=Join-Path $jcOut 'launch.json';$jcRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $jcLaunch -Encoding utf8
# Explicitly authorized visible local game; its Director owns Esc/P stop behavior.
$jcProcess=Start-Process -FilePath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $jcArgs -WorkingDirectory (Split-Path $jcProject) -WindowStyle Normal -PassThru
$null=$jcProcess.Handle;$jcRecord.process_id=$jcProcess.Id
Write-Output "J cloth runtime: $jcOut"
$jcProcess.WaitForExit();$jcProcess.Refresh();$jcRecord.exit_code=$jcProcess.ExitCode
$jcRecord.ended_at=(Get-Date).ToString('o')
$jcResults=@(Get-ChildItem -LiteralPath $jcOut -Recurse -Filter 'npc_reaction_review.json' -File)
if($jcResults.Count -eq 1){
 $jcActual=Get-Content -LiteralPath $jcResults[0].FullName -Raw|ConvertFrom-Json
 $jcRecord.runtime=$jcActual.status;$jcRecord.stop_latched=$jcActual.user_stop_latched;$jcRecord.result=$jcResults[0].FullName
 $jcCloth=@($jcActual.captures|Where-Object { $_.j_chaos_cloth_at_request }|ForEach-Object {$_.j_chaos_cloth_at_request})
 $jcRecord.cloth_readbacks=$jcCloth.Count
 $jcRecord.status=if($jcProcess.ExitCode -eq 0 -and $jcActual.status -ceq 'PASS' -and $jcActual.user_stop_latched -eq $false -and $jcCloth.Count -eq 5 -and @($jcCloth|Where-Object status -CNE 'PASS_PARTICLE_READBACK_ONLY').Count -eq 0){'PASS_CAPTURE_READBACK_ONLY'}else{'FAIL_OR_STOPPED'}
}else{$jcRecord.status='MISSING_RESULT'}
$jcRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $jcLaunch -Encoding utf8
Write-Output $jcRecord.status
if($jcRecord.status -cne 'PASS_CAPTURE_READBACK_ONLY'){exit 1}
