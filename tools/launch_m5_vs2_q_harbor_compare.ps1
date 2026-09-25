#requires -Version 7.0
param([Parameter(Mandatory=$true)][string]$AuthorReport)
$ErrorActionPreference='Stop'
$qcWork=Split-Path $PSScriptRoot -Parent
$qcDoc=Join-Path $qcWork 'docs/HarborCity_M5_VS2/editor_runtime'
$qcAuthor=Get-Content -LiteralPath $AuthorReport -Raw|ConvertFrom-Json
$qcCommand=Get-Content -LiteralPath (Join-Path (Split-Path $AuthorReport) 'commandlet.json') -Raw|ConvertFrom-Json
if($qcAuthor.status -cne 'PASS' -or $qcAuthor.phase -cne 'QHarborCompareAuthor' -or $qcCommand.exit_code -ne 0 -or $qcAuthor.map -notmatch '^/Game/HarborCity/M5VS2/WorldRev2/QSkinReview_[0-9a-f]{12}/L_QHarborCompare$'){throw 'Completed isolated Q harbor author required.'}
foreach($qcSource in $qcAuthor.sources.PSObject.Properties){if((Get-FileHash -LiteralPath $qcSource.Name).Hash -ine $qcSource.Value){throw "Changed source: $($qcSource.Name)"}}
$qcMap=Join-Path (Join-Path $qcWork 'HarborCity/Content') ($qcAuthor.map.Substring(6)+'.umap')
if((Get-FileHash -LiteralPath $qcMap).Hash -ine $qcAuthor.map_sha256){throw 'Comparison map changed.'}
foreach($qcAsset in $qcAuthor.assets){$qcPackage=($qcAsset.path -split '\.')[0];$qcExt=if($qcPackage -eq $qcAuthor.map){'.umap'}else{'.uasset'};$qcFile=Join-Path (Join-Path $qcWork 'HarborCity/Content') ($qcPackage.Substring(6)+$qcExt);if((Get-FileHash -LiteralPath $qcFile).Hash -ine $qcAsset.sha256){throw 'Comparison asset changed.'}}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity -ErrorAction SilentlyContinue){throw 'Finish existing engine/game normally first.'}
$qcRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$qcOut=Join-Path $qcDoc ($qcRun+'_q_harbor_compare_game');New-Item -ItemType Directory -Path $qcOut|Out-Null
$qcProject=Join-Path $qcWork 'HarborCity/HarborCity.uproject';$qcCache='D:/GameDev/Cache/Unreal/HarborCity'
$qcArgs=@(('"'+$qcProject+'"'),$qcAuthor.map,'-game','-unattended','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
 '-M5VS2NPCReview','-M5VS2QSkinComparison','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$qcOut+'"'),
 ('-HCM1SaveSlot=HarborCity_VS2_QSkin_Test_'+$qcRun),('-abslog="'+(Join-Path $qcOut 'game.log')+'"'),
 '-ExecCmds="sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0"',
 "-LocalDataCachePath=$qcCache/DDC","-ZenDataPath=$qcCache/Zen")
$qcDLL=Join-Path $qcWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
if($qcAuthor.candidate_root){$qcArgs+=('-M5QSkinCandidate='+$qcAuthor.candidate_root)}
$qcRecord=[ordered]@{status='STARTING';started_at=(Get-Date).ToString('o');author=$AuthorReport;author_sha256=(Get-FileHash -LiteralPath $AuthorReport).Hash;runtime_dll_sha256=(Get-FileHash -LiteralPath $qcDLL).Hash;arguments=$qcArgs;scope='NATIVE_ART_COMPARISON_NOT_GAMEPLAY_NOT_OS';visual='USER_REVIEW'}
$qcLaunch=Join-Path $qcOut 'launch.json';$qcRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $qcLaunch -Encoding utf8
$qcProcess=Start-Process -FilePath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $qcArgs -WorkingDirectory (Split-Path $qcProject) -WindowStyle Normal -PassThru
$null=$qcProcess.Handle;$qcRecord.process_id=$qcProcess.Id;Write-Output "Q harbor comparison: $qcOut"
$qcProcess.WaitForExit();$qcProcess.Refresh();$qcRecord.exit_code=$qcProcess.ExitCode;$qcRecord.ended_at=(Get-Date).ToString('o')
$qcResults=@(Get-ChildItem -LiteralPath $qcOut -Recurse -Filter 'npc_review.json' -File)
if($qcResults.Count -eq 1){
 $qcActual=Get-Content -LiteralPath $qcResults[0].FullName -Raw|ConvertFrom-Json
 $qcRecord.result=$qcResults[0].FullName;$qcRecord.stop_latched=$qcActual.user_stop_latched;$qcRecord.runtime_status=$qcActual.status
 $qcRecord.status=if($qcProcess.ExitCode -eq 0 -and $qcActual.status -ceq 'PASS' -and $qcActual.user_stop_latched -eq $false -and $qcActual.q_skin_harbor_comparison){'PASS_CAPTURE_ONLY'}else{'FAIL_OR_STOPPED'}
}else{$qcRecord.status='MISSING_RESULT'}
$qcRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $qcLaunch -Encoding utf8
Write-Output $qcRecord.status
if($qcRecord.status -cne 'PASS_CAPTURE_ONLY'){exit 1}
