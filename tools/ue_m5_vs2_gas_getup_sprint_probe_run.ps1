#requires -Version 7.0
[CmdletBinding()]
param()

# Read-only five-action original-frame probe in the one completed official sample. No project/script
# override, asset save, installation, migration, cache cleanup or UI automation.
$ErrorActionPreference = 'Stop'
$gasWorkspace = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if ($gasWorkspace -ine [IO.Path]::GetFullPath('D:/科研学习/codex学习')) { throw 'Wrong HarborCity workspace.' }
$gasRoot = 'E:/GameDev/Assets/HarborCity/M5_VS2/Animation/GameAnimationSample'
$gasProject = Join-Path $gasRoot 'GameAnimationSample.uproject'
$gasEditor = 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$gasScript = Join-Path $PSScriptRoot 'ue_m5_vs2_gas_getup_sprint_probe.py'
$gasReports = Join-Path $gasWorkspace 'docs/HarborCity_M5_VS2'
$gasCache = 'D:/GameDev/Cache/Unreal/HarborCity'
$gasPluginFiles = @(
    'E:/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/PythonScriptPlugin.uplugin',
    'E:/UE_5.8/Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin'
)
foreach ($gasFile in @($gasProject, $gasEditor, $gasScript) + $gasPluginFiles) {
    if (-not (Test-Path -LiteralPath $gasFile -PathType Leaf)) { throw "Missing required local file: $gasFile" }
}
$gasDescriptor = Get-Content -LiteralPath $gasProject -Raw | ConvertFrom-Json
if ($gasDescriptor.EngineAssociation -ne '5.8') { throw 'Only the verified GAS 5.8 project is accepted.' }
if (@(Get-ChildItem -LiteralPath $gasRoot -File -Filter '*.uproject').Count -ne 1) { throw 'Unexpected additional project descriptor.' }
if (Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'") {
    throw 'Existing engine/game process; finish it normally before this read-only probe.'
}
$gasPluginAudit = foreach ($gasPluginFile in $gasPluginFiles) {
    $gasPlugin = Get-Content -LiteralPath $gasPluginFile -Raw | ConvertFrom-Json
    $gasPluginName = [IO.Path]::GetFileNameWithoutExtension($gasPluginFile)
    if ($gasPlugin.CreatedBy -ne 'Epic Games, Inc.') { throw "Unexpected plugin publisher: $gasPluginName" }
    $gasModuleManifest = Join-Path (Split-Path $gasPluginFile -Parent) 'Binaries/Win64/UnrealEditor.modules'
    if (-not (Test-Path -LiteralPath $gasModuleManifest -PathType Leaf)) { throw "Plugin has no installed editor module manifest: $gasPluginName" }
    $gasModules = Get-Content -LiteralPath $gasModuleManifest -Raw | ConvertFrom-Json
    $gasEngineModules = Get-Content -LiteralPath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.modules' -Raw | ConvertFrom-Json
    if ($gasModules.BuildId -ne $gasEngineModules.BuildId) { throw "Plugin module build does not match locked engine: $gasPluginName" }
    foreach ($gasModule in $gasModules.Modules.PSObject.Properties) {
        $gasDLL = Join-Path (Split-Path $gasModuleManifest -Parent) $gasModule.Value
        if (-not (Test-Path -LiteralPath $gasDLL -PathType Leaf)) { throw "Missing installed module: $gasDLL" }
    }
    [ordered]@{ name=$gasPluginName; descriptor=$gasPluginFile; sha256=(Get-FileHash -LiteralPath $gasPluginFile -Algorithm SHA256).Hash; build_id=$gasModules.BuildId }
}

function Get-GASProtectedHashes {
    $gasProtected = @($gasProject) + @(Get-ChildItem -LiteralPath (Join-Path $gasRoot 'Config') -Recurse -File | ForEach-Object FullName)
    $gasHashResult = [ordered]@{}
    foreach ($gasProtectedFile in ($gasProtected | Sort-Object)) {
        $gasHashResult[[IO.Path]::GetRelativePath($gasRoot, $gasProtectedFile)] = (Get-FileHash -LiteralPath $gasProtectedFile -Algorithm SHA256).Hash
    }
    return $gasHashResult
}
function Get-GASContentMetadata {
    $gasMetadata = [ordered]@{}
    foreach ($gasContentFile in (Get-ChildItem -LiteralPath (Join-Path $gasRoot 'Content') -Recurse -File | Sort-Object FullName)) {
        $gasMetadata[[IO.Path]::GetRelativePath($gasRoot, $gasContentFile.FullName)] = "$($gasContentFile.Length):$($gasContentFile.LastWriteTimeUtc.Ticks)"
    }
    return $gasMetadata
}

$gasRun = (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$gasEvidence = Join-Path $gasReports ('editor_runtime/probe_' + $gasRun + '_GASGetUpSprintReadOnly')
New-Item -ItemType Directory -Path $gasEvidence | Out-Null
$gasBefore = Get-GASProtectedHashes
$gasContentBefore = Get-GASContentMetadata
$gasArgs = @(('"' + $gasProject + '"'), '-run=pythonscript', ('-script="' + $gasScript + '"'),
    ('-M5EvidenceDir="' + $gasEvidence + '"'), '-unattended', '-nop4', '-nosplash', '-NullRHI', '-language=en',
    '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities', "-LocalDataCachePath=$gasCache/DDC", "-ZenDataPath=$gasCache/Zen",
    ('-abslog="' + (Join-Path $gasEvidence 'editor.log') + '"'))
$gasRecord = [ordered]@{ milestone='M5_VS2'; phase='GAS_GETUP_SPRINT_READ_ONLY_PROBE'; project=$gasProject; script=$gasScript;
    script_sha256=(Get-FileHash -LiteralPath $gasScript -Algorithm SHA256).Hash; executable=$gasEditor; arguments=$gasArgs;
    plugins_enabled_for_process_only=$gasPluginAudit; selection='Four exact standing getups and Relaxed Sprint; fixed by script'; started_at=(Get-Date).ToString('o');
    status='RUNNING'; process_id=$null; exit_code=$null; probe_status='NOT_RUN'; visual_validation='NOT_RUN_NULLRHI';
    source_config_hashes_before=$gasBefore; asset_writes_requested=0; free_bytes_E_before=(Get-PSDrive -Name E).Free;
    expected_writes='Unique evidence logs and normal E DDC/Zen/Saved/Intermediate; no source asset/config saves or migration.' }
$gasEnv = @{'UE-LocalDataCachePath'="$gasCache/DDC"; 'UE-ZenDataPath'="$gasCache/Zen"; 'UE-ZenSubprocessDataPath'="$gasCache/Zen"}
$gasOld = @{}
try {
    foreach ($gasKey in $gasEnv.Keys) {
        $gasOld[$gasKey] = [Environment]::GetEnvironmentVariable($gasKey, 'Process')
        [Environment]::SetEnvironmentVariable($gasKey, $gasEnv[$gasKey], 'Process')
    }
    $gasProc = Start-Process -FilePath $gasEditor -ArgumentList $gasArgs -WorkingDirectory $gasRoot -WindowStyle Hidden -PassThru
    $gasRecord.process_id = $gasProc.Id
    $gasRecord | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $gasEvidence 'commandlet.json') -Encoding utf8
    Write-Output "M5-VS2 GAS getup/sprint read-only probe evidence: $gasEvidence"
    $gasProc.WaitForExit()
    $gasProc.Refresh()
    $gasRecord.exit_code = $gasProc.ExitCode
    $gasResultPath = Join-Path $gasEvidence 'author_result.json'
    if (Test-Path -LiteralPath $gasResultPath -PathType Leaf) {
        $gasResult = Get-Content -LiteralPath $gasResultPath -Raw | ConvertFrom-Json
        $gasRecord.probe_status = $gasResult.status
        $gasRecord.reported_source_is_exact_project = $gasResult.source_project.path -and
            [IO.Path]::GetFullPath($gasResult.source_project.path) -ieq [IO.Path]::GetFullPath($gasProject)
        $gasRecord.reported_asset_writes = $gasResult.asset_writes
    } else { $gasRecord.probe_status = 'MISSING' }
    $gasRecord.source_config_hashes_after = Get-GASProtectedHashes
    $gasContentAfter = Get-GASContentMetadata
    $gasRecord.source_config_bytes_unchanged = ($gasBefore | ConvertTo-Json -Compress) -ceq ($gasRecord.source_config_hashes_after | ConvertTo-Json -Compress)
    $gasRecord.content_paths_sizes_mtimes_unchanged = ($gasContentBefore | ConvertTo-Json -Compress) -ceq ($gasContentAfter | ConvertTo-Json -Compress)
    $gasRecord.content_metadata_guard_is_not_full_sha256 = $true
    $gasRecord.status = if ($gasProc.ExitCode -eq 0 -and $gasRecord.probe_status -eq 'PASS' -and
        $gasRecord.reported_source_is_exact_project -and $gasRecord.reported_asset_writes -eq 0 -and
        $gasRecord.source_config_bytes_unchanged -and $gasRecord.content_paths_sizes_mtimes_unchanged) { 'PASS' } else { 'FAIL' }
} catch {
    $gasRecord.status = 'FAIL'
    $gasRecord.error = $_.Exception.Message
    throw
} finally {
    foreach ($gasKey in $gasOld.Keys) { [Environment]::SetEnvironmentVariable($gasKey, $gasOld[$gasKey], 'Process') }
    $gasRecord.ended_at = (Get-Date).ToString('o')
    $gasRecord.free_bytes_E_after = (Get-PSDrive -Name E).Free
    $gasRecord | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $gasEvidence 'commandlet.json') -Encoding utf8
}
$gasRecord | ConvertTo-Json -Depth 12
if ($gasRecord.status -ne 'PASS') { exit 1 }
exit 0
