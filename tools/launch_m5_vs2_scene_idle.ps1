#requires -Version 7.0
param([Parameter(Mandatory=$true)][string]$AuthorReport)
$ErrorActionPreference='Stop'
$siWork=Split-Path $PSScriptRoot -Parent
$siAuthor=Get-Content -LiteralPath $AuthorReport -Raw|ConvertFrom-Json
$siCmd=Get-Content -LiteralPath (Join-Path (Split-Path $AuthorReport) 'commandlet.json') -Raw|ConvertFrom-Json
if($siAuthor.status -cne 'PASS' -or $siAuthor.phase -cne 'SceneIdleAuthor' -or $siCmd.exit_code -ne 0 -or $siAuthor.map -notmatch '^/Game/HarborCity/M5VS2/WorldRev2/SceneIdleReview_[0-9a-f]{12}/L_SceneIdleReview$'){throw 'Completed isolated scene-idle author required.'}
foreach($siSource in $siAuthor.sources.PSObject.Properties){if((Get-FileHash -LiteralPath $siSource.Name).Hash -ine $siSource.Value){throw 'Source changed.'}}
foreach($siAsset in $siAuthor.assets){$siExt=if($siAsset.path -ceq $siAuthor.map){'.umap'}else{'.uasset'};$siFile=Join-Path (Join-Path $siWork 'HarborCity/Content') ($siAsset.path.Substring(6)+$siExt);if((Get-FileHash -LiteralPath $siFile).Hash -ine $siAsset.sha256){throw 'Candidate asset changed.'}}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity -ErrorAction SilentlyContinue){throw 'Finish existing engine/game normally first.'}
$siRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$siOut=Join-Path $siWork ('docs/HarborCity_M5_VS2/editor_runtime/'+$siRun+'_scene_idle_game');New-Item -ItemType Directory -Path $siOut|Out-Null
$siProject=Join-Path $siWork 'HarborCity/HarborCity.uproject';$siCache='D:/GameDev/Cache/Unreal/HarborCity'
$siArgs=@(('"'+$siProject+'"'),$siAuthor.map,'-game','-unattended','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2NPCReview','-M5VS2SceneIdleReview','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$siOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_SceneIdle_Test_'+$siRun),('-abslog="'+(Join-Path $siOut 'game.log')+'"'),'-ExecCmds="sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0"',"-LocalDataCachePath=$siCache/DDC","-ZenDataPath=$siCache/Zen")
$siRecord=[ordered]@{status='STARTING';started_at=(Get-Date).ToString('o');author=$AuthorReport;author_sha256=(Get-FileHash -LiteralPath $AuthorReport).Hash;runtime_dll_sha256=(Get-FileHash -LiteralPath (Join-Path $siWork 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll')).Hash;arguments=$siArgs;scope='NATIVE_SAVED_IDLE_PLAYBACK_NOT_GAMEPLAY_NOT_OS';visual='USER_REVIEW'}
$siLaunch=Join-Path $siOut 'launch.json';$siRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $siLaunch -Encoding utf8
$siProcess=Start-Process -FilePath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $siArgs -WorkingDirectory (Split-Path $siProject) -WindowStyle Normal -PassThru
$null=$siProcess.Handle;$siRecord.process_id=$siProcess.Id;Write-Output "Scene idle review: $siOut";$siProcess.WaitForExit();$siProcess.Refresh()
$siRecord.exit_code=$siProcess.ExitCode;$siRecord.ended_at=(Get-Date).ToString('o')
$siResults=@(Get-ChildItem -LiteralPath $siOut -Recurse -Filter npc_review.json -File)
if($siResults.Count -eq 1){$siActual=Get-Content -LiteralPath $siResults[0].FullName -Raw|ConvertFrom-Json;$siRecord.result=$siResults[0].FullName;$siRecord.stop_latched=$siActual.user_stop_latched;$siRecord.status=if($siProcess.ExitCode -eq 0 -and $siActual.status -ceq 'PASS' -and $siActual.user_stop_latched -eq $false -and $siActual.scene_idle_review){'PASS_CAPTURE_ONLY'}else{'FAIL_OR_STOPPED'}}else{$siRecord.status='MISSING_RESULT'}
$siRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $siLaunch -Encoding utf8;Write-Output $siRecord.status
if($siRecord.status -cne 'PASS_CAPTURE_ONLY'){exit 1}
