#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$AuthorReport,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$flightWorkspace=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if($flightWorkspace -ine [IO.Path]::GetFullPath('D:/科研学习/codex学习')){throw 'Wrong HarborCity workspace.'}
$flightDoc=[IO.Path]::GetFullPath((Join-Path $flightWorkspace 'docs/HarborCity_M5_VS2/editor_runtime'))
$flightReport=[IO.Path]::GetFullPath($AuthorReport)
if(-not $flightReport.StartsWith($flightDoc+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($flightReport) -ne 'author_result.json'){
    throw 'Require exact native author_result.json inside this milestone editor_runtime.'
}
$flightAuthor=Get-Content -LiteralPath $flightReport -Raw | ConvertFrom-Json
$flightCommand=Get-Content -LiteralPath (Join-Path (Split-Path $flightReport -Parent) 'commandlet.json') -Raw | ConvertFrom-Json
if($flightAuthor.schema -ne 'HarborCity.M5VS2.FlightReview.Author.v1' -or $flightAuthor.status -ne 'PASS' -or $flightCommand.status -ne 'PASS' -or
    $flightAuthor.map -cnotmatch '^/Game/HarborCity/M5VS2/FlightReview/Run_[0-9a-f]{12}/L_FlightReview$' -or $flightAuthor.assets.Count -ne 2 -or
    $flightAuthor.selected_hero -cnotmatch '^/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/BP_[A-Za-z0-9_]+$'){
    throw 'No completed exact native flight fixture author.'
}
$flightAuthorScript=Join-Path $PSScriptRoot 'ue_m5_vs2_flight_review_author.py'
if([IO.Path]::GetFullPath($flightCommand.script) -ine [IO.Path]::GetFullPath($flightAuthorScript) -or
    (Get-FileHash -LiteralPath $flightAuthorScript -Algorithm SHA256).Hash -ine $flightCommand.script_sha256){throw 'Author script changed after native evidence.'}
$flightMap=$flightAuthor.map
$flightMapFile=Join-Path $flightWorkspace ('HarborCity/Content/'+$flightMap.Substring(6)+'.umap')
if($flightAuthor.assets[0].path -cne $flightMap -or (Get-FileHash -LiteralPath $flightMapFile -Algorithm SHA256).Hash -ine $flightAuthor.assets[0].sha256){throw 'Fixture map differs from native saved evidence.'}
$flightExpectedMode=$flightMap.Substring(0,$flightMap.LastIndexOf('/'))+'/BP_FlightReviewGameMode'
$flightModeFile=Join-Path $flightWorkspace ('HarborCity/Content/'+$flightExpectedMode.Substring(6)+'.uasset')
if($flightAuthor.private_game_mode -cne $flightExpectedMode -or $flightAuthor.assets[1].path -cne $flightExpectedMode -or
    (Get-FileHash -LiteralPath $flightModeFile -Algorithm SHA256).Hash -ine $flightAuthor.assets[1].sha256){throw 'Private selected-Hero GameMode differs from native saved evidence.'}
foreach($flightSource in $flightAuthor.sources){
    if((Get-FileHash -LiteralPath $flightSource.path -Algorithm SHA256).Hash -ine $flightSource.sha256){throw "Current hero/GameMode no longer matches this fixture: $($flightSource.path)"}
}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'"){
    throw 'Existing engine/game process; close normally before this isolated runtime.'
}
$flightProject=Join-Path $flightWorkspace 'HarborCity/HarborCity.uproject'
$flightModule=Join-Path $flightWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
$flightExe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
foreach($flightFile in @($flightProject,$flightModule,$flightExe)){if(-not(Test-Path -LiteralPath $flightFile -PathType Leaf)){throw "Missing $flightFile"}}
$flightRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$flightDir=Join-Path $flightDoc ($flightRun+'_flightreview_game')
New-Item -ItemType Directory -Path $flightDir | Out-Null
$flightCache='D:/GameDev/Cache/Unreal/HarborCity'
$flightCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2',
    'sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$flightArgs=@(('"'+$flightProject+'"'),$flightMap,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
    '-M5VS2FlightReview','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$flightDir+'"'),('-HCM1SaveSlot=HarborCity_M1_R2_Test_Flight_'+$flightRun),
    ('-abslog="'+(Join-Path $flightDir 'game.log')+'"'),('-ExecCmds="'+($flightCommands -join ',')+'"'),"-LocalDataCachePath=$flightCache/DDC","-ZenDataPath=$flightCache/Zen")
$flightRecord=[ordered]@{milestone='M5_VS2';phase='FlightReview';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';
    executable=$flightExe;map=$flightMap;arguments=$flightArgs;started_at=(Get-Date).ToString('o');
    module_sha256=(Get-FileHash -LiteralPath $flightModule).Hash;author_report=$flightReport;author_sha256=(Get-FileHash -LiteralPath $flightReport).Hash;
    input_scope='ACTION_INJECTION_ENGINE_MOUSE_AXES_AND_REAL_METHODS_NOT_OS_INPUT';visual_acceptance='USER_REVIEW';video='NOT_RUN_BY_WRAPPER';
    packaged_runtime='NOT_RUN';process_id=$null;exit_code=$null;runtime_status='NOT_RUN';stop_latched=$null}
$flightRecordPath=Join-Path $flightDir 'launch.json'
$flightVars=@{'UE-LocalDataCachePath'="$flightCache/DDC";'UE-ZenDataPath'="$flightCache/Zen";'UE-ZenSubprocessDataPath'="$flightCache/Zen"}
$flightPrior=@{}
try{
    foreach($flightKey in $flightVars.Keys){$flightPrior[$flightKey]=[Environment]::GetEnvironmentVariable($flightKey,'Process');[Environment]::SetEnvironmentVariable($flightKey,$flightVars[$flightKey],'Process')}
    # Authorized interactive game window. Esc remains the existing stop key; P pauses.
    $flightProcess=Start-Process -FilePath $flightExe -ArgumentList $flightArgs -WorkingDirectory (Split-Path $flightProject -Parent) -WindowStyle Normal -PassThru
    $null=$flightProcess.Handle;$flightRecord.process_id=$flightProcess.Id;$flightRecord.status='STARTED'
}catch{$flightRecord.status='FAIL';$flightRecord.error=$_.Exception.Message;throw}
finally{
    foreach($flightKey in $flightPrior.Keys){[Environment]::SetEnvironmentVariable($flightKey,$flightPrior[$flightKey],'Process')}
    $flightRecord | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $flightRecordPath -Encoding utf8
}
Write-Output "Flight review evidence: $flightDir"
if($WaitForExit){
    $flightProcess.WaitForExit();$flightProcess.Refresh();$flightRecord.exit_code=$flightProcess.ExitCode
    $flightReports=@(Get-ChildItem -LiteralPath $flightDir -Directory -Filter 'FlightReview_*' | ForEach-Object {Join-Path $_.FullName 'flight_review.json'} | Where-Object {Test-Path -LiteralPath $_})
    if($flightReports.Count -eq 1){
        $flightRuntime=Get-Content -LiteralPath $flightReports[0] -Raw | ConvertFrom-Json
        $flightRecord.runtime_status=$flightRuntime.status;$flightRecord.stop_latched=$flightRuntime.stop_latched
        $flightRecord.runtime_report=$flightReports[0]
    }else{$flightRecord.runtime_status='MISSING_OR_AMBIGUOUS'}
    $flightRecord.status=if($flightProcess.ExitCode -eq 0 -and $flightRecord.runtime_status -eq 'PASS' -and $flightRecord.stop_latched -eq $false){'PASS'}else{'FAIL_OR_STOPPED'}
    $flightRecord.ended_at=(Get-Date).ToString('o')
    $flightRecord | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $flightRecordPath -Encoding utf8
    if($flightRecord.status -ne 'PASS'){exit 1}
}
