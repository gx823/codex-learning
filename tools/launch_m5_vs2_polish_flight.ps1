#requires -Version 7.0
param([Parameter(Mandatory=$true)][string]$AuthorReport,[string]$RuntimeDllSha256='')
$ErrorActionPreference='Stop'
$pfWork=Split-Path $PSScriptRoot -Parent;$pfDocs=Join-Path $pfWork 'docs/HarborCity_M5_VS2'
$pfAuthorPath=[IO.Path]::GetFullPath($AuthorReport);$pfAuthor=Get-Content -LiteralPath $pfAuthorPath -Raw|ConvertFrom-Json
$pfCommand=Get-Content -LiteralPath (Join-Path (Split-Path $pfAuthorPath) 'commandlet.json') -Raw|ConvertFrom-Json
if($pfAuthor.status -cne 'PASS' -or $pfAuthor.phase -cne 'PlayablePolishAuthor' -or $pfCommand.status -cne 'PASS' -or $pfCommand.exit_code -ne 0 -or $pfAuthor.changed_sources.Count){throw 'New polish native author has not completed.'}
foreach($pfRow in $pfAuthor.assets){if((Get-FileHash -LiteralPath $pfRow.path).Hash -ine $pfRow.sha256){throw 'Polish asset changed after native save.'}}
$pfRuntimeHash=(Get-FileHash -LiteralPath $pfAuthor.runtime_module.path).Hash
if($pfRuntimeHash -ine $pfAuthor.runtime_module.sha256 -and ($RuntimeDllSha256 -notmatch '^[A-Fa-f0-9]{64}$' -or $RuntimeDllSha256 -ine $pfRuntimeHash)){throw 'Changed runtime requires its explicit current DLL SHA256; original asset author binding remains retained.'}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity -ErrorAction SilentlyContinue){throw 'Finish existing game/editor normally first.'}
$pfRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$pfOut=Join-Path $pfDocs ($pfRun+'_flight_demo_r5_game');New-Item -ItemType Directory -Path $pfOut|Out-Null
$pfProject=Join-Path $pfWork 'HarborCity/HarborCity.uproject';$pfCache='D:/GameDev/Cache/Unreal/HarborCity';$pfSlot='HarborCity_VS2_FlightDemo_R5_'+$pfRun
$pfMap=$pfAuthor.maps.Flight
if($pfMap -cnotmatch '^/Game/HarborCity/M5VS2/FlightDemoR5/Run_[0-9a-f]{12}/L_FlightDemoR5$'){throw 'Unexpected flight map scope.'}
$pfArgs=@(('"'+$pfProject+'"'),$pfMap,'-game','-unattended','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
 '-M5VS2FlightDemo','-M5VS2AutoQuit','-M5VS2RecordSeconds=60','-M5VS2RecordDelay=12',('-HCM1SaveSlot='+$pfSlot),('-M5VS2EvidenceDir="'+$pfOut+'"'),('-abslog="'+(Join-Path $pfOut 'game.log')+'"'),
 '-ExecCmds="sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,sg.FoliageQuality 2,sg.ShadingQuality 2,r.ScreenPercentage 100,r.VSync 0,t.MaxFPS 0"',
 "-LocalDataCachePath=$pfCache/DDC","-ZenDataPath=$pfCache/Zen")
$pfR=[ordered]@{schema='HarborCity.M5VS2.FlightDemoR5.Launch.v1';status='STARTING';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';input_scope='ENGINE_INPUT_NOT_OS';started_at=(Get-Date).ToString('o');map=$pfMap;save_slot=$pfSlot;polish_author=$pfAuthorPath;polish_author_sha256=(Get-FileHash -LiteralPath $pfAuthorPath).Hash;module_sha256=$pfAuthor.runtime_module.sha256;arguments=$pfArgs;auto_engine_input=$true;auto_camera=$false;auto_quit_only_without_user_stop=$true;os_input_result='NOT_RUN';visual='USER_REVIEW';record_seconds=60;record_delay=12;demo_result='NOT_RUN';recording_result='NOT_RUN'}
$pfLaunch=Join-Path $pfOut 'launch.json';$pfR|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $pfLaunch -Encoding utf8
$pfR.author_module_sha256=$pfAuthor.runtime_module.sha256;$pfR.module_sha256=$pfRuntimeHash
# Explicitly authorized visible game; existing flight Director owns cancellation.
$pfProcess=Start-Process -FilePath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $pfArgs -WorkingDirectory (Split-Path $pfProject) -WindowStyle Normal -PassThru
$null=$pfProcess.Handle;$pfR.process_id=$pfProcess.Id;Write-Output "Polished flight runtime: $pfOut"
$pfProcess.WaitForExit();$pfProcess.Refresh();$pfR.exit_code=$pfProcess.ExitCode;$pfR.ended_at=(Get-Date).ToString('o');$pfR.status='EXITED'
$pfResults=@(Get-ChildItem -LiteralPath $pfOut -Recurse -File -Filter 'flight_demo.json')
if($pfResults.Count -eq 1){
 $pfActual=Get-Content -LiteralPath $pfResults[0].FullName -Raw|ConvertFrom-Json
 $pfR.result_file=$pfResults[0].FullName;$pfR.result_sha256=(Get-FileHash -LiteralPath $pfResults[0].FullName).Hash
 $pfR.demo_result=$pfActual.status;$pfR.recording_result=$pfActual.actual_native_capture.status;$pfR.recording_directory=$pfActual.native_capture_directory
 $pfR.stop_latched=$pfActual.stop_latched
}
$pfR|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $pfLaunch -Encoding utf8
Write-Output ('Flight result: '+$pfR.demo_result)
if($pfProcess.ExitCode -ne 0 -or $pfR.demo_result -cne 'PASS_ENGINE_INPUT_ONLY'){exit 1}
