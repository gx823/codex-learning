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
if($flightAuthor.schema -ne 'HarborCity.M5VS2.FlightCollisionR2.Author.v1' -or $flightAuthor.status -ne 'PASS' -or $flightCommand.status -ne 'PASS' -or
    $null -eq $flightCommand.exit_code -or $flightCommand.exit_code -ne 0 -or
    $flightAuthor.map -cnotmatch '^/Game/HarborCity/M5VS2/FlightCollisionR2/Run_[0-9a-f]{12}/L_FlightCollisionR2$' -or $flightAuthor.assets.Count -ne 2 -or
    $flightAuthor.selected_hero -cnotmatch '^/Game/HarborCity/M5VS2/HeroRev2/Review_[0-9a-f]{12}/BP_[A-Za-z0-9_]+$'){
    throw 'No completed exact native flight fixture author.'
}
$flightAuthorScript=Join-Path $PSScriptRoot 'ue_m5_vs2_flight_collision_r2_author.py'
if([IO.Path]::GetFullPath($flightCommand.script) -ine [IO.Path]::GetFullPath($flightAuthorScript) -or
    (Get-FileHash -LiteralPath $flightAuthorScript -Algorithm SHA256).Hash -ine $flightCommand.script_sha256){throw 'Author script changed after native evidence.'}
if(@($flightAuthor.checks).Count -lt 1 -or @($flightAuthor.checks|Where-Object {$_.status -cne 'PASS'}).Count -ne 0 -or
    $null -eq $flightAuthor.source_guard.changes -or @($flightAuthor.source_guard.changes).Count -ne 0){throw 'Author source-preservation or checks incomplete.'}
$flightMap=$flightAuthor.map
$flightMapFile=Join-Path $flightWorkspace ('HarborCity/Content/'+$flightMap.Substring(6)+'.umap')
if($flightAuthor.assets[0].path -cne $flightMap -or (Get-FileHash -LiteralPath $flightMapFile -Algorithm SHA256).Hash -ine $flightAuthor.assets[0].sha256){throw 'Fixture map differs from native saved evidence.'}
$flightExpectedMode=$flightMap.Substring(0,$flightMap.LastIndexOf('/'))+'/BP_FlightCollisionR2GameMode'
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
$flightDir=Join-Path $flightDoc ($flightRun+'_flightcollision_r2_game')
New-Item -ItemType Directory -Path $flightDir | Out-Null
$flightCache='D:/GameDev/Cache/Unreal/HarborCity'
$flightCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2',
    'sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$flightArgs=@(('"'+$flightProject+'"'),$flightMap,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',
    '-M5VS2FlightCollisionR2','-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$flightDir+'"'),('-HCM1SaveSlot=HarborCity_M1_R2_Test_Flight_'+$flightRun),
    ('-abslog="'+(Join-Path $flightDir 'game.log')+'"'),('-ExecCmds="'+($flightCommands -join ',')+'"'),"-LocalDataCachePath=$flightCache/DDC","-ZenDataPath=$flightCache/Zen")
$flightRecord=[ordered]@{milestone='M5_VS2';phase='FlightCollisionR2';kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';
    executable=$flightExe;map=$flightMap;arguments=$flightArgs;started_at=(Get-Date).ToString('o');
    module_sha256=(Get-FileHash -LiteralPath $flightModule).Hash;author_report=$flightReport;author_sha256=(Get-FileHash -LiteralPath $flightReport).Hash;
    input_scope='PRODUCTION_ENHANCED_ACTIONS_NOT_OS_INPUT_FIXTURE_TELEPORTS_AND_INITIAL_LOOK_ARE_SETUP_ONLY';visual_acceptance='NOT_RUN_COLLISION_DIAGNOSTIC';video='NOT_RUN_BY_WRAPPER';
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
    try{
        $flightReports=@(Get-ChildItem -LiteralPath $flightDir -Directory -Filter 'FlightCollisionR2_*' | ForEach-Object {Join-Path $_.FullName 'flight_collision_r2.json'} | Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
        if($flightReports.Count -ne 1){throw 'Missing or ambiguous native collision report.'}
        $flightRuntime=Get-Content -LiteralPath $flightReports[0] -Raw|ConvertFrom-Json
        $flightRecord.runtime_report=$flightReports[0];$flightRecord.runtime_report_sha256=(Get-FileHash -LiteralPath $flightReports[0]).Hash
        $flightRecord.runtime_status=$flightRuntime.status;$flightRecord.stop_latched=$flightRuntime.stop_latched
        if($flightRuntime.schema -cne 'HarborCity.M5VS2.FlightCollisionR2.v1' -or $flightRuntime.map -cne $flightMap -or
            $flightRuntime.os_input -cne 'NOT_RUN' -or $flightRuntime.packaged_runtime -cne 'NOT_RUN' -or
            $flightRuntime.visual_acceptance -cne 'NOT_RUN_COLLISION_DIAGNOSTIC' -or $flightRuntime.active_limit_seconds -ne 90){throw 'Unexpected native mode/map/acceptance scope.'}
        if($flightRuntime.status -ceq 'PASS' -and $flightRuntime.stop_latched -eq $false){
            if($flightRuntime.active_seconds -le 0 -or $flightRuntime.active_seconds -gt 90 -or
                @($flightRuntime.checks|Where-Object {$_.status -cne 'PASS'}).Count -ne 0){throw 'Native completion or bounded checks invalid.'}
            $flightRequired=@('actual_F_takeoff','high_speed_roof_upward_primary_sweep','high_speed_dry_platform_dive_primary_sweep',
                'narrow_straight_actual_high_speed_transit','narrow_oblique_wall_at_measured_boost_speed',
                'boost_dive_brakes_above_water_without_collision','actual_F_water_landing_refused')
            foreach($flightCheck in $flightRequired){
                $flightMatches=@($flightRuntime.checks|Where-Object {$_.name -ceq $flightCheck -and $_.status -ceq 'PASS'})
                if($flightMatches.Count -ne 1){throw "Missing/duplicate actual result $flightCheck"}
            }
            $flightFrames=@($flightRuntime.measured_trajectory_frames)
            if($flightFrames.Count -lt 20 -or $flightFrames.Count -ne $flightRuntime.measured_frame_count -or $flightFrames.Count -gt 24000 -or
                @($flightFrames|Where-Object {$_.actual_capsule_blocking_overlap -ne $false -or $_.frame_delta_seconds -le 0}).Count -ne 0){throw 'Bounded actual trajectory/overlap records incomplete.'}
            $flightClass=$flightAuthor.selected_hero+'.'+($flightAuthor.selected_hero -split '/')[-1]+'_C'
            if(@($flightFrames|Where-Object {$_.hero_class -cne $flightClass}).Count -ne 0){throw 'Measured Hero differs from selected native source.'}
            foreach($flightPhase in @('RoofBoost','HighDryPlatformDive','StraightNarrowPassage','HighSpeedObliqueWall','WaterBoostDive')){
                if(@($flightFrames|Where-Object {$_.phase -ceq $flightPhase}).Count -lt 2){throw "Missing measured phase $flightPhase"}
            }
            $flightTargets=@(@('RoofBoost','high_roof'),@('HighDryPlatformDive','high_platform'),@('HighSpeedObliqueWall','bend_north'))
            foreach($flightTarget in $flightTargets){
                $flightExpectedActor=$flightMap+'.'+($flightMap -split '/')[-1]+':PersistentLevel.'+$flightAuthor.director_bindings.($flightTarget[1])
                $flightHits=@($flightRuntime.actual_capsule_hits|Where-Object {$_.phase -ceq $flightTarget[0] -and $_.assigned_target_and_component -eq $true})
                if($flightHits.Count -lt 1){throw 'Missing actual designated collision.'}
                $flightHit=$flightHits[0]
                if($flightHit.hit_actor -cne $flightExpectedActor -or -not $flightHit.hit_component.StartsWith($flightExpectedActor+'.',[StringComparison]::Ordinal) -or
                    $flightHit.primary_flight_sweep -ne $true -or $flightHit.start_penetrating -ne $false -or
                    $flightHit.previous_frame_actual_speed_cm_s -lt 2300 -or $flightHit.sweep_speed_from_trace_cm_s -lt 2300 -or
                    $flightHit.sweep_speed_from_trace_cm_s -gt 2600){throw 'Assigned hit lacks actual high-speed primary-sweep evidence.'}
            }
            $flightRecord.measured_frame_count=$flightFrames.Count
            $flightRecord.measured_mean_fps=$flightRuntime.measured_mean_fps
            $flightRecord.scope='Five native CMC collision scenarios; not OS input, full map, NPC/story rules or art acceptance.'
        }
    }catch{$flightRecord.validation_error=$_.Exception.Message;$flightRecord.runtime_status='FAIL_EVIDENCE_VALIDATION'}
    finally{
        $flightRecord.status=if($null -ne $flightRecord.exit_code -and $flightRecord.exit_code -eq 0 -and $flightRecord.runtime_status -ceq 'PASS' -and $flightRecord.stop_latched -eq $false){'PASS'}else{'FAIL_OR_STOPPED'}
        $flightRecord.ended_at=(Get-Date).ToString('o')
        $flightRecord|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $flightRecordPath -Encoding utf8
    }
    if($flightRecord.status -cne 'PASS'){exit 1}
}
