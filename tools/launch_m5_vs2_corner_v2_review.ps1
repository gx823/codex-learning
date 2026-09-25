#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Plan,[switch]$WaitForExit)
$ErrorActionPreference='Stop'
$reviewWorkspace=Split-Path $PSScriptRoot -Parent
$reviewDocs=[IO.Path]::GetFullPath((Join-Path $reviewWorkspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
$reviewPlan=[IO.Path]::GetFullPath($Plan)
if(-not $reviewPlan.StartsWith($reviewDocs,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($reviewPlan) -cne 'corner_v2_capture_plan.json' -or $reviewPlan.Contains('"') -or -not(Test-Path -LiteralPath $reviewPlan -PathType Leaf)){throw 'Requires an actual owned revision-two capture plan.'}
$reviewCapture=Get-Content -LiteralPath $reviewPlan -Raw|ConvertFrom-Json
$reviewExpected=if($reviewCapture.profile -ceq 'Environment'){7}elseif($reviewCapture.profile -cin @('HeroPortrait','HeroFullBody')){4}else{throw 'Unknown review profile.'}
if($reviewCapture.schema_version -ne 2 -or $reviewCapture.plan_type -cne 'HARBOR_CORNER_REV2_NATIVE_VIEWPORT' -or $reviewCapture.expected_screenshots -ne $reviewExpected -or $reviewCapture.status -cne 'READY_FOR_RUNTIME_NOT_CAPTURED' -or $reviewCapture.map -cnotmatch '^/Game/HarborCity/M5VS2/WorldRev2/Review_[0-9a-f]{12}/L_CornerRevTwoReview$'){throw 'Unrecognized bounded revision-two capture plan.'}
if(@($reviewCapture.shots).Count -ne $reviewExpected -or ($reviewCapture.expected_resolution -join ',') -ne '1920,1080'){throw 'Invalid capture count or resolution.'}
foreach($reviewSource in $reviewCapture.source_bindings){
    $reviewFile=[IO.Path]::GetFullPath($reviewSource.path)
    $reviewAllowedRoot=switch($reviewSource.kind){
        'package'{Join-Path $reviewWorkspace 'HarborCity/Content'}
        'module'{Join-Path $reviewWorkspace 'HarborCity/Binaries/Win64'}
        'cpp'{Join-Path $reviewWorkspace 'HarborCity/Source'}
        'script'{$PSScriptRoot}
        'evidence'{$reviewDocs}
        default{throw 'Unknown source kind.'}
    }
    $reviewAllowedRoot=[IO.Path]::GetFullPath($reviewAllowedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $reviewFile.StartsWith($reviewAllowedRoot,[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $reviewFile -PathType Leaf) -or (Get-Item -LiteralPath $reviewFile).Length -ne $reviewSource.bytes -or (Get-FileHash -LiteralPath $reviewFile -Algorithm SHA256).Hash -ine $reviewSource.sha256){throw "Capture source changed or outside its allowed root: $reviewFile"}
}
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue){throw 'Close existing project engine/game normally before launching review.'}
$reviewProject=Join-Path $reviewWorkspace 'HarborCity/HarborCity.uproject'
$reviewEditor='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$reviewModule=Join-Path $reviewWorkspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
foreach($reviewFile in @($reviewProject,$reviewEditor,$reviewModule)){if(-not(Test-Path -LiteralPath $reviewFile -PathType Leaf)){throw "Missing $reviewFile"}}
$reviewRun=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$reviewOut=Join-Path $reviewDocs ($reviewRun+'_corner_v2_game')
New-Item -ItemType Directory -Path $reviewOut|Out-Null
$reviewCache='D:/GameDev/Cache/Unreal/HarborCity'
$reviewCommands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$reviewArgs=@(('"'+$reviewProject+'"'),$reviewCapture.map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en','-M5VS2CornerRevTwoReview','-M5VS2AutoQuit',('-M5VS2CornerRevTwoPlan="'+$reviewPlan+'"'),('-M5VS2EvidenceDir="'+$reviewOut+'"'),('-HCM1SaveSlot=HarborCity_VS2_RevTwoReview_'+$reviewRun),('-abslog="'+(Join-Path $reviewOut 'game.log')+'"'),('-ExecCmds="'+($reviewCommands -join ',')+'"'),"-LocalDataCachePath=$reviewCache/DDC","-ZenDataPath=$reviewCache/Zen")
$reviewRecord=[ordered]@{milestone='M5_VS2';profile=$reviewCapture.profile;kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';started_at=(Get-Date).ToString('o');executable=$reviewEditor;module_sha256=(Get-FileHash -LiteralPath $reviewModule -Algorithm SHA256).Hash;plan=$reviewPlan;plan_sha256=(Get-FileHash -LiteralPath $reviewPlan -Algorithm SHA256).Hash;map=$reviewCapture.map;arguments=$reviewArgs;visual_acceptance='USER_REVIEW';os_input_used=$false;performance_benchmark=$false;requested_quality='High';preview_fps_cap=0;requested_vsync=0}
$reviewRecordPath=Join-Path $reviewOut 'launch.json'
$reviewRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $reviewRecordPath -Encoding utf8
$reviewEnv=@{'UE-LocalDataCachePath'="$reviewCache/DDC";'UE-ZenDataPath'="$reviewCache/Zen";'UE-ZenSubprocessDataPath'="$reviewCache/Zen"};$reviewOld=@{}
try{
    foreach($reviewKey in $reviewEnv.Keys){$reviewOld[$reviewKey]=[Environment]::GetEnvironmentVariable($reviewKey,'Process');[Environment]::SetEnvironmentVariable($reviewKey,$reviewEnv[$reviewKey],'Process')}
    # User authorized a real visible game-window review; no synthetic OS input.
    $reviewProcess=Start-Process -FilePath $reviewEditor -ArgumentList $reviewArgs -WorkingDirectory (Split-Path $reviewProject -Parent) -WindowStyle Normal -PassThru
    $null=$reviewProcess.Handle;$reviewRecord.process_id=$reviewProcess.Id;$reviewRecord.status='STARTED'
}catch{$reviewRecord.status='FAIL';$reviewRecord.error=$_.Exception.Message;throw}
finally{
    foreach($reviewKey in $reviewOld.Keys){[Environment]::SetEnvironmentVariable($reviewKey,$reviewOld[$reviewKey],'Process')}
    $reviewRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $reviewRecordPath -Encoding utf8
}
Write-Output "Rev2 native review evidence: $reviewOut"
if($WaitForExit){
    $reviewProcess.WaitForExit();$reviewProcess.Refresh();$reviewRecord.exit_code=$reviewProcess.ExitCode;$reviewRecord.ended_at=(Get-Date).ToString('o');$reviewRecord.status='EXITED'
    $reviewRecord|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $reviewRecordPath -Encoding utf8
    if($null -eq $reviewProcess.ExitCode){throw 'Missing actual exit code.'};exit $reviewProcess.ExitCode
}
