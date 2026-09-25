#requires -Version 7.0
[CmdletBinding()]
param()

# Fixed source project and fixed script; no project/command override, download,
# migration, asset save, map load, plugin installation or desktop control.
$ErrorActionPreference = 'Stop'
$villageWorkspace = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if ($villageWorkspace -ine [IO.Path]::GetFullPath('D:/科研学习/codex学习')) { throw 'Wrong HarborCity workspace.' }
$villageRoot = 'E:/GameDev/Assets/HarborCity/M5_VS2/Environment/HC_VillageSource'
$villageProject = Join-Path $villageRoot 'HC_VillageSource.uproject'
$villagePack = Join-Path $villageRoot 'Content/Fantastic_Village_Pack'
$villageEditor = 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$villageScript = Join-Path $PSScriptRoot 'ue_m5_vs2_village_probe.py'
$villageReports = Join-Path $villageWorkspace 'docs/HarborCity_M5_VS2'
$villageCache = 'D:/GameDev/Cache/Unreal/HarborCity'
$villagePlugins = @(
    'E:/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/PythonScriptPlugin.uplugin',
    'E:/UE_5.8/Engine/Plugins/Editor/EditorScriptingUtilities/EditorScriptingUtilities.uplugin'
)
foreach ($villageFile in @($villageProject, $villageEditor, $villageScript) + $villagePlugins) {
    if (-not (Test-Path -LiteralPath $villageFile -PathType Leaf)) { throw "Missing local file: $villageFile" }
}
if (-not (Test-Path -LiteralPath $villagePack -PathType Container)) { throw 'Downloaded Fantastic_Village_Pack directory is absent; finish Launcher download first.' }
$villageDescriptor = Get-Content -LiteralPath $villageProject -Raw | ConvertFrom-Json
if ($villageDescriptor.EngineAssociation -ne '5.8') { throw 'Only the actual verified Village 5.8 descriptor is accepted.' }
if (@(Get-ChildItem -LiteralPath $villageRoot -File -Filter '*.uproject').Count -ne 1) { throw 'Unexpected additional project descriptor.' }
$villageVersion = Get-Content -LiteralPath 'E:/UE_5.8/Engine/Build/Build.version' -Raw | ConvertFrom-Json
if ($villageVersion.MajorVersion -ne 5 -or $villageVersion.MinorVersion -ne 8 -or $villageVersion.PatchVersion -ne 2) { throw 'Locked UE 5.8.2 required.' }
if (Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe' OR Name='HarborCity.exe'") {
    throw 'An engine/game process is running; finish it normally before this source probe.'
}
$villagePackages = @(Get-ChildItem -LiteralPath $villagePack -Recurse -File | Where-Object Extension -In @('.uasset','.umap'))
if ($villagePackages.Count -lt 1 -or $villagePackages.Count -gt 5000) { throw 'Expected 1-5000 actual downloaded packages.' }
if (@($villagePackages | Where-Object LastWriteTimeUtc -GT ([DateTime]::UtcNow.AddSeconds(-15))).Count) {
    throw 'Source packages were modified in the last 15 seconds; wait for the Launcher download to finish.'
}
$villagePluginAudit = foreach ($villagePluginFile in $villagePlugins) {
    $villagePlugin = Get-Content -LiteralPath $villagePluginFile -Raw | ConvertFrom-Json
    $villagePluginName = [IO.Path]::GetFileNameWithoutExtension($villagePluginFile)
    if ($villagePlugin.CreatedBy -ne 'Epic Games, Inc.') { throw "Unexpected plugin publisher: $villagePluginName" }
    $villageModuleManifest = Join-Path (Split-Path $villagePluginFile -Parent) 'Binaries/Win64/UnrealEditor.modules'
    if (-not (Test-Path -LiteralPath $villageModuleManifest -PathType Leaf)) { throw "Installed plugin module manifest missing: $villagePluginName" }
    $villageModules = Get-Content -LiteralPath $villageModuleManifest -Raw | ConvertFrom-Json
    $villageEngineModules = Get-Content -LiteralPath 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.modules' -Raw | ConvertFrom-Json
    if ($villageModules.BuildId -ne $villageEngineModules.BuildId) { throw "Plugin/engine build mismatch: $villagePluginName" }
    foreach ($villageModule in $villageModules.Modules.PSObject.Properties) {
        if (-not (Test-Path -LiteralPath (Join-Path (Split-Path $villageModuleManifest -Parent) $villageModule.Value) -PathType Leaf)) {
            throw "Missing installed plugin DLL: $($villageModule.Value)"
        }
    }
    [ordered]@{ name=$villagePluginName; descriptor=$villagePluginFile; sha256=(Get-FileHash -LiteralPath $villagePluginFile -Algorithm SHA256).Hash; build_id=$villageModules.BuildId }
}

function Get-VillageProtectedHashes {
    $villageFiles = @($villageProject) + @(Get-ChildItem -LiteralPath (Join-Path $villageRoot 'Config') -Recurse -File | ForEach-Object FullName)
    $villageHashes = [ordered]@{}
    foreach ($villagePath in ($villageFiles | Sort-Object)) {
        $villageHashes[[IO.Path]::GetRelativePath($villageRoot, $villagePath)] = (Get-FileHash -LiteralPath $villagePath -Algorithm SHA256).Hash
    }
    return $villageHashes
}
function Get-VillageContentHashes {
    $villageHashes = [ordered]@{}
    foreach ($villageFile in (Get-ChildItem -LiteralPath (Join-Path $villageRoot 'Content') -Recurse -File | Sort-Object FullName)) {
        $villageHashes[[IO.Path]::GetRelativePath($villageRoot, $villageFile.FullName)] = [ordered]@{
            bytes=$villageFile.Length; sha256=(Get-FileHash -LiteralPath $villageFile.FullName -Algorithm SHA256).Hash
        }
    }
    return $villageHashes
}

$villageRun = (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$villageEvidence = Join-Path $villageReports ('editor_runtime/probe_' + $villageRun + '_VillageReadOnly')
New-Item -ItemType Directory -Path $villageEvidence | Out-Null
$villageBefore = Get-VillageProtectedHashes
$villageContentBefore = Get-VillageContentHashes
$villageContentBefore | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $villageEvidence 'source_content_sha256_before.json') -Encoding utf8
$villageArgs = @(('"' + $villageProject + '"'), '-run=pythonscript', ('-script="' + $villageScript + '"'),
    ('-M5EvidenceDir="' + $villageEvidence + '"'), '-unattended', '-nop4', '-nosplash', '-NullRHI', '-language=en',
    '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities', "-LocalDataCachePath=$villageCache/DDC", "-ZenDataPath=$villageCache/Zen",
    ('-abslog="' + (Join-Path $villageEvidence 'editor.log') + '"'))
$villageRecord = [ordered]@{ milestone='M5_VS2'; phase='VILLAGE_READ_ONLY_PROBE'; project=$villageProject; script=$villageScript;
    script_sha256=(Get-FileHash -LiteralPath $villageScript -Algorithm SHA256).Hash; executable=$villageEditor; arguments=$villageArgs;
    plugins_enabled_for_process_only=$villagePluginAudit; started_at=(Get-Date).ToString('o'); status='RUNNING'; process_id=$null;
    exit_code=$null; probe_status='NOT_RUN'; visual_validation='NOT_RUN_NULLRHI'; source_config_hashes_before=$villageBefore;
    asset_writes_requested=0; content_guard='Full paths, sizes and SHA256 of all Content files, before and after';
    source_content_file_count_before=$villageContentBefore.Count; source_content_sha256_manifest='source_content_sha256_before.json';
    free_bytes_E_before=(Get-PSDrive -Name E).Free;
    expected_writes='Unique evidence and normal E DDC/Zen/Saved/Intermediate only; no asset/config save or migration.' }
$villageEnv = @{'UE-LocalDataCachePath'="$villageCache/DDC"; 'UE-ZenDataPath'="$villageCache/Zen"; 'UE-ZenSubprocessDataPath'="$villageCache/Zen"}
$villageOld = @{}
try {
    foreach ($villageKey in $villageEnv.Keys) {
        $villageOld[$villageKey] = [Environment]::GetEnvironmentVariable($villageKey, 'Process')
        [Environment]::SetEnvironmentVariable($villageKey, $villageEnv[$villageKey], 'Process')
    }
    $villageProc = Start-Process -FilePath $villageEditor -ArgumentList $villageArgs -WorkingDirectory $villageRoot -WindowStyle Hidden -PassThru
    $villageRecord.process_id = $villageProc.Id
    $villageRecord | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $villageEvidence 'commandlet.json') -Encoding utf8
    Write-Output "M5-VS2 Village read-only probe evidence: $villageEvidence"
    $villageProc.WaitForExit()
    $villageProc.Refresh()
    $villageRecord.exit_code = $villageProc.ExitCode
    $villageResultPath = Join-Path $villageEvidence 'author_result.json'
    if (Test-Path -LiteralPath $villageResultPath -PathType Leaf) {
        $villageResult = Get-Content -LiteralPath $villageResultPath -Raw | ConvertFrom-Json
        $villageRecord.probe_status = $villageResult.status
        $villageRecord.reported_source_is_exact_project = $villageResult.source_project.path -and
            [IO.Path]::GetFullPath($villageResult.source_project.path) -ieq [IO.Path]::GetFullPath($villageProject)
        $villageRecord.reported_asset_writes = $villageResult.asset_writes
    } else { $villageRecord.probe_status = 'MISSING' }
    $villageRecord.source_config_hashes_after = Get-VillageProtectedHashes
    $villageContentAfter = Get-VillageContentHashes
    $villageRecord.source_config_bytes_unchanged = ($villageBefore | ConvertTo-Json -Compress) -ceq ($villageRecord.source_config_hashes_after | ConvertTo-Json -Compress)
    $villageRecord.content_paths_sizes_sha256_unchanged = ($villageContentBefore | ConvertTo-Json -Depth 6 -Compress) -ceq ($villageContentAfter | ConvertTo-Json -Depth 6 -Compress)
    if (-not $villageRecord.content_paths_sizes_sha256_unchanged) {
        $villageContentAfter | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $villageEvidence 'source_content_sha256_after_changed.json') -Encoding utf8
    }
    $villageRecord.status = if ($villageProc.ExitCode -eq 0 -and $villageRecord.probe_status -eq 'PASS' -and
        $villageRecord.reported_source_is_exact_project -and $villageRecord.reported_asset_writes -eq 0 -and
        $villageRecord.source_config_bytes_unchanged -and $villageRecord.content_paths_sizes_sha256_unchanged) { 'PASS' } else { 'FAIL' }
} catch {
    $villageRecord.status = 'FAIL'; $villageRecord.error = $_.Exception.Message
    throw
} finally {
    foreach ($villageKey in $villageOld.Keys) { [Environment]::SetEnvironmentVariable($villageKey, $villageOld[$villageKey], 'Process') }
    $villageRecord.ended_at = (Get-Date).ToString('o')
    $villageRecord.free_bytes_E_after = (Get-PSDrive -Name E).Free
    $villageRecord | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $villageEvidence 'commandlet.json') -Encoding utf8
}
$villageRecord | ConvertTo-Json -Depth 12
if ($villageRecord.status -ne 'PASS') { exit 1 }
exit 0
