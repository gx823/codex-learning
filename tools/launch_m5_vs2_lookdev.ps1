#requires -Version 7.0
[CmdletBinding()]
param([switch]$WaitForExit,[ValidateSet('Lookdev','HeroExercise','HeroGait','HeroFace','HeroLimits','HeroColorResponse','HeroLightChroma','HeroBoundReview','HeroFill','NPCReview','NPCPhysics','NPCGameplay','Corner')][string]$Mode='Lookdev',[switch]$PhysicsDisablePostProcess,[ValidateSet('HipImpulse','DistributedFront','DistributedSide')][string]$PhysicsCase='HipImpulse',[switch]$PhysicsSettledOnly,[string]$CornerPlan,[ValidateSet('Zero','Quarter','One')][string]$HeroFillCandidate='Zero',[switch]$PassiveMaterialObservation,[switch]$NoExplicitGTSubmit,[switch]$PassiveRTObservation,[switch]$SkipMaterialMapDDC,[switch]$MemoryTrace)
$ErrorActionPreference='Stop'
if(($PassiveMaterialObservation -or $NoExplicitGTSubmit -or $PassiveRTObservation -or $SkipMaterialMapDDC -or $MemoryTrace) -and $Mode -ne 'HeroFill'){throw 'Material diagnostic requires the bounded HeroFill map.'}
if(([int]$PassiveMaterialObservation.IsPresent+[int]$NoExplicitGTSubmit.IsPresent+[int]$PassiveRTObservation.IsPresent) -gt 1){throw 'Select only one bounded material diagnostic per run.'}
if($PhysicsDisablePostProcess -and $Mode -ne 'NPCPhysics'){throw 'Post-process A/B option is only valid for the bounded NPC physics review.'}
if($PhysicsSettledOnly -and $Mode -ne 'NPCPhysics'){throw 'Settled views require the bounded NPC physics review.'}
if($PhysicsCase -ne 'HipImpulse' -and $Mode -ne 'NPCPhysics'){throw 'Alternate initial impulses require the bounded NPC physics review.'}
$workspace=Split-Path $PSScriptRoot -Parent
$project=Join-Path $workspace 'HarborCity/HarborCity.uproject'
$exe='E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$map=if($Mode -in @('HeroExercise','HeroFace','HeroGait')){'/Game/HarborCity/M5VS2/HeroExercise/L_SelestiaExercise'}else{'/Game/HarborCity/M5VS2/Lookdev/L_SelestiaLookdev'}
$testFlag=if($Mode -eq 'HeroFace'){'-M5VS2HeroFaceReview'}elseif($Mode -in @('HeroExercise','HeroGait')){'-M5VS2HeroExercise'}else{'-M5VS2Lookdev'}
if($Mode -eq 'Corner'){
    if(-not $CornerPlan){throw 'Corner requires its actual saved capture plan.'}
    $CornerPlan=[IO.Path]::GetFullPath($CornerPlan)
    $cornerDocRoot=[IO.Path]::GetFullPath((Join-Path $workspace 'docs/HarborCity_M5_VS2')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(-not $CornerPlan.StartsWith($cornerDocRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($CornerPlan) -ne 'corner_capture_plan.json' -or $CornerPlan.Contains('"')){throw 'Invalid bounded corner plan path.'}
    $cornerCapture=Get-Content -LiteralPath $CornerPlan -Raw|ConvertFrom-Json
    $ownedCornerMap=$cornerCapture.map -eq '/Game/HarborCity/M5VS2/World/L_AnimeHarbor_Corner_P0' -or $cornerCapture.map -cmatch '^/Game/HarborCity/M5VS2/World/StyleReview_[0-9a-f]{12}/L_(OriginalHandPainted|SoftAnime|WaterWorldScale)$'
    if($cornerCapture.status -ne 'READY_FOR_RUNTIME_NOT_CAPTURED' -or $cornerCapture.expected_screenshots -ne 6 -or -not $ownedCornerMap){throw 'Not an actual owned corner capture plan.'}
    $cornerSources=@($cornerCapture.sources.layout,$cornerCapture.sources.corner_author,$cornerCapture.sources.village_copy,$cornerCapture.sources.plan_script)+@($cornerCapture.sources.map_and_dedicated_packages)
    foreach($source in $cornerSources){
        if(-not $source.path -or -not(Test-Path -LiteralPath $source.path -PathType Leaf) -or (Get-Item -LiteralPath $source.path).Length -ne $source.bytes -or (Get-FileHash -LiteralPath $source.path -Algorithm SHA256).Hash -ine $source.sha256){throw "Corner source no longer matches saved evidence: $($source.path)"}
    }
    $map=$cornerCapture.map; $testFlag='-M5VS2CornerReview'
}elseif($CornerPlan){throw 'Corner plan is only accepted for Corner mode.'}
if($Mode -in @('NPCReview','NPCPhysics')){
    $runtimeRoot=Join-Path $workspace 'docs/HarborCity_M5_VS2/editor_runtime'
    $reviewSources=@(Get-ChildItem -LiteralPath $runtimeRoot -Directory -Filter 'author_*_NPCReview' | Sort-Object Name -Descending)
    $chosen=$null
    foreach($source in $reviewSources){
        $resultFile=Join-Path $source.FullName 'author_result.json'
        $processFile=Join-Path $source.FullName 'commandlet.json'
        if((Test-Path -LiteralPath $resultFile) -and (Test-Path -LiteralPath $processFile)){
            $result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
            $authorProcess=Get-Content -LiteralPath $processFile -Raw | ConvertFrom-Json
            if($result.status -eq 'PASS' -and $authorProcess.status -eq 'PASS'){$chosen=$result;break}
        }
    }
    if($null -eq $chosen -or $chosen.map -notmatch '^/Game/HarborCity/M5VS2/NPC/Review/Run_[0-9a-f]{10}/L_NPCSpecimens$'){throw 'No completed native NPCReview map author record.'}
    $map=$chosen.map;$testFlag='-M5VS2NPCReview'
}
$module=Join-Path $workspace 'HarborCity/Binaries/Win64/UnrealEditor-HarborCity.dll'
if($Mode -eq 'HeroFill'){
    $fillSource=$null
    foreach($folder in (Get-ChildItem -LiteralPath (Join-Path $workspace 'docs/HarborCity_M5_VS2/editor_runtime') -Directory -Filter 'author_*_HeroFill' | Sort-Object Name -Descending)){
        $resultFile=Join-Path $folder.FullName 'author_result.json';$processFile=Join-Path $folder.FullName 'commandlet.json'
        if(-not(Test-Path -LiteralPath $resultFile) -or -not(Test-Path -LiteralPath $processFile)){continue}
        $candidate=Get-Content -LiteralPath $resultFile -Raw|ConvertFrom-Json
        $processRecord=Get-Content -LiteralPath $processFile -Raw|ConvertFrom-Json
        if($candidate.status -eq 'PASS' -and $processRecord.status -eq 'PASS'){$fillSource=$candidate;break}
    }
    if($null -eq $fillSource){throw 'No completed native HeroFill candidate author.'}
    $fill=@($fillSource.candidates|Where-Object {$_.tag -ceq $HeroFillCandidate})
    if($fill.Count -ne 1 -or $fill[0].map -cnotmatch '^/Game/HarborCity/M5VS2/HeroFill/Review_([0-9a-f]{12})/L_SelestiaFill_\1_(Zero|Quarter|One)$'){throw 'Invalid isolated HeroFill candidate map.'}
    foreach($source in $fillSource.assets){
        if($source.path -cnotmatch '^/Game/HarborCity/M5VS2/HeroFill/Review_[0-9a-f]{12}/'){throw 'Unexpected HeroFill package path.'}
        $ext=if($source.path -match '/L_SelestiaFill_'){'.umap'}else{'.uasset'}
        $file=Join-Path $workspace ('HarborCity/Content/'+$source.path.Substring(6)+$ext)
        if(-not(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $source.sha256){throw 'HeroFill saved asset changed.'}
    }
    $map=$fill[0].map;$testFlag='-M5VS2Lookdev'
}
if($Mode -in @('NPCGameplay','HeroLimits','HeroColorResponse','HeroLightChroma','HeroBoundReview')){
    $phase=if($Mode -eq 'NPCGameplay'){'NPCGameplay'}elseif($Mode -eq 'HeroColorResponse'){'HeroColorResponse'}elseif($Mode -eq 'HeroLightChroma'){'HeroLightChroma'}elseif($Mode -eq 'HeroBoundReview'){'HeroBoundReview'}else{'HeroLimitsCompare'}
    $expectedMap=if($Mode -eq 'NPCGameplay'){'^/Game/HarborCity/M5VS2/NPC/GameplayReview/L_NPCGameplay$'}elseif($Mode -eq 'HeroColorResponse'){'^/Game/HarborCity/M5VS2/HeroColorResponse/Review_[0-9a-f]+/L_HeroColorResponse_[0-9a-f]+$'}elseif($Mode -eq 'HeroLightChroma'){'^/Game/HarborCity/M5VS2/HeroLightChroma/Review_([0-9a-f]{12})/L_HeroLightChroma_\1$'}elseif($Mode -eq 'HeroBoundReview'){'^/Game/HarborCity/M5VS2/HeroBoundReview/Review_([0-9a-f]{12})/L_HeroBoundReview_\1$'}else{'^/Game/HarborCity/M5VS2/HeroLimitsCompare/Review_[0-9a-f]+/L_HeroLimitsCompare_[0-9a-f]+$'}
    $chosen=$null
    foreach($folder in (Get-ChildItem -LiteralPath (Join-Path $workspace 'docs/HarborCity_M5_VS2/editor_runtime') -Directory -Filter "author_*_$phase" | Sort-Object Name -Descending)){
        $resultFile=Join-Path $folder.FullName 'author_result.json';$processFile=Join-Path $folder.FullName 'commandlet.json'
        if(-not(Test-Path -LiteralPath $resultFile) -or -not(Test-Path -LiteralPath $processFile)){continue}
        $candidate=Get-Content -LiteralPath $resultFile -Raw|ConvertFrom-Json
        $processRecord=Get-Content -LiteralPath $processFile -Raw|ConvertFrom-Json
        if($candidate.status -eq 'PASS' -and $processRecord.status -eq 'PASS'){$chosen=$candidate;break}
    }
    if($null -eq $chosen -or $chosen.map -notmatch $expectedMap){throw "No completed $phase native map author record."}
    $map=$chosen.map
    $mapFile=Join-Path $workspace ('HarborCity/Content/'+$map.Substring(6)+'.umap')
    $mapAsset=@($chosen.assets|Where-Object {$_.path -eq $map})
    if($mapAsset.Count -ne 1 -or (Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash -ine $mapAsset[0].sha256){throw 'Runtime map differs from its actual saved author evidence.'}
    $testFlag=if($Mode -eq 'NPCGameplay'){'-M5VS2NPCGameplayReview'}else{'-M5VS2Lookdev'}
}
$existing=@(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,HarborCity,HarborCity-Win64-Development -ErrorAction SilentlyContinue)
if($existing.Count){throw 'Close existing project engine/game normally before lookdev launch.'}
foreach($file in @($project,$exe,$module,(Join-Path $workspace ('HarborCity/Content/'+$map.Substring(6)+'.umap')))){
    if(-not(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing input: $file"}
}
$run=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$dir=Join-Path $workspace ('docs/HarborCity_M5_VS2/editor_runtime/'+$run+'_'+$Mode.ToLowerInvariant()+'_game')
New-Item -ItemType Directory -Path $dir | Out-Null
$cache='D:/GameDev/Cache/Unreal/HarborCity'
$commands=@('sg.ViewDistanceQuality 2','sg.AntiAliasingQuality 2','sg.ShadowQuality 2','sg.GlobalIlluminationQuality 2','sg.ReflectionQuality 2','sg.PostProcessQuality 2','sg.TextureQuality 2','sg.EffectsQuality 2','sg.FoliageQuality 2','sg.ShadingQuality 2','r.ScreenPercentage 100','r.VSync 0','t.MaxFPS 0')
$arguments=@(('"'+$project+'"'),$map,'-game','-nosplash','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-language=en',$testFlag,'-M5VS2AutoQuit',('-M5VS2EvidenceDir="'+$dir+'"'),('-HCM1SaveSlot=HarborCity_M1_R2_Test_M5VS2_'+$Mode+'_'+$run),('-abslog="'+(Join-Path $dir 'game.log')+'"'),('-ExecCmds="'+($commands -join ',')+'"'),"-LocalDataCachePath=$cache/DDC","-ZenDataPath=$cache/Zen")
if($PassiveMaterialObservation){$arguments+='-M5VS2PassiveMaterialObservation'}
if($NoExplicitGTSubmit){$arguments+='-M5VS2NoExplicitGTSubmit'}
if($PassiveRTObservation){$arguments+='-M5VS2PassiveRTObservation'}
if($SkipMaterialMapDDC){$arguments+='-nomaterialshaderddc'}
$tracePath=$null
if($MemoryTrace){
    # This engine's early TraceLog file opener truncates the Chinese path.
    # Keep the trace on the user-authorized D drive under an ASCII project path.
    $traceDir=Join-Path 'D:/GameDev/Diagnostics/HarborCity/M5_VS2' $run
    New-Item -ItemType Directory -Path $traceDir -ErrorAction Stop | Out-Null
    $tracePath=Join-Path $traceDir 'renderer_memory.utrace'
    $arguments+='-trace=memory';$arguments+=('-tracefile="'+$tracePath+'"')
}
if($Mode -eq 'NPCPhysics'){$arguments+='-M5VS2NPCPhysicsReview';$arguments+=('-M5VS2NPCPhysicsCase='+$PhysicsCase)}
if($Mode -eq 'HeroGait'){$arguments+='-M5VS2HeroGaitReview'}
if($PhysicsDisablePostProcess){$arguments+='-M5VS2NPCPhysicsNoPostProcess'}
if($PhysicsSettledOnly){$arguments+='-M5VS2NPCPhysicsSettledOnly'}
if($Mode -eq 'Corner'){$arguments+=('-M5VS2CornerPlan="'+$CornerPlan+'"')}
$record=[ordered]@{milestone='M5_VS2';mode=$Mode;kind='EDITOR_BINARY_GAME_RUNTIME_NOT_PACKAGED';status='STARTING';started_at=(Get-Date).ToString('o');executable=$exe;module_sha256=(Get-FileHash -LiteralPath $module).Hash;map=$map;arguments=$arguments;visual_acceptance='NOT_RUN';input_scope='NATIVE_FUNCTION_CALLS_AND_SCENE_CAPTURE_NOT_MOUSE_TEST'}
$record.material_map_ddc_diagnostic=[ordered]@{
    enabled=[bool]$SkipMaterialMapDDC
    argument=if($SkipMaterialMapDDC){'-nomaterialshaderddc'}else{$null}
    scope=if($SkipMaterialMapDDC){'COLD_MATERIAL_SHADER_MAP_ASSEMBLY_NOT_FULL_DXC_COLD_COMPILE'}else{'NORMAL_DDC_POLICY'}
    behavior='Opt-in skips the first material shader-map DDC hit per key in this process; global shader-map and per-shader job DDC remain enabled. Existing DDC is neither removed nor moved.'
    readiness_deadline_seconds=120
    readiness_policy='Existing native director deadline and no-fallback capture guards remain unchanged.'
}
$record.preview_fps_cap=0
$record.memory_trace=[ordered]@{requested=$MemoryTrace.IsPresent;path=$tracePath;scope='One valid startup allocation trace after documented path failure; no DDC deletion, rendering changes or root-cause claim.'}
$record.performance_benchmark=$false
$record.load_policy='User restored full development throughput: uncapped native-resolution preview; one engine writer at a time.'
$recordPath=Join-Path $dir 'launch.json'
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8
$vars=@{'UE-LocalDataCachePath'="$cache/DDC";'UE-ZenDataPath'="$cache/Zen";'UE-ZenSubprocessDataPath'="$cache/Zen"};$prior=@{}
try{
    foreach($key in $vars.Keys){$prior[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$vars[$key],'Process')}
    # User requested real interactive game-window review; this is deliberately visible.
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path $project -Parent) -WindowStyle Normal -PassThru
    $null=$process.Handle;$record.process_id=$process.Id;$record.status='STARTED'
}catch{$record.status='FAIL';$record.error=$_.Exception.Message;throw}
finally{foreach($key in $prior.Keys){[Environment]::SetEnvironmentVariable($key,$prior[$key],'Process')};$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8}
Write-Output "Lookdev evidence: $dir"
if($WaitForExit){
    $process.WaitForExit();$process.Refresh();$record.exit_code=$process.ExitCode;$record.ended_at=(Get-Date).ToString('o');$record.status='EXITED'
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8
    if($null -eq $process.ExitCode){throw 'No exit code; cannot infer success.'};exit $process.ExitCode
}
