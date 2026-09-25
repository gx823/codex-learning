#requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateRange(1,8)][int]$MaxParallelActions = 8,
    [ValidateRange(1,200)][int]$MinimumFreeSpaceGB = 10,
    [ValidateRange(1,400)][int]$MinimumWorkFreeSpaceGB = 20,
    [string[]]$NoticeFiles = @()
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'm5_vs2_release_guards.ps1')
$workspaceRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $workspaceRoot 'HarborCity\HarborCity.uproject'
$engineRoot = 'E:\UE_5.8'
$buildRoot = 'E:\GameDev\Builds\HarborCity\M5_VS2'
$cacheRoot = 'D:\GameDev\Cache\Unreal\HarborCity'
$evidenceRoot = Join-Path $workspaceRoot 'docs\HarborCity_M5_VS2\builds'
$buildScript = Join-Path $PSScriptRoot 'build_m5_vs2.ps1'
$dotnetPath = Join-Path $engineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$uatPath = Join-Path $engineRoot 'Engine\Binaries\DotNET\AutomationTool\AutomationTool.dll'
$editorPath = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
# Reuse the invoking, already-verified PowerShell runtime. Do not select a different
# Windows PowerShell installation or alter execution policy for the child build.
$powershellPath = (Get-Process -Id $PID).Path
$map = '/Game/HarborCity/M5VS2/Part3/Town_ea45a319b3dd/L_HarborTown'
$runId = (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$packageStartedUtc = [DateTime]::UtcNow
$runDirectory = Join-Path $evidenceRoot ($runId + '_package')
$candidateDirectory = Join-Path $buildRoot ('Candidate_' + $runId)
$recordPath = Join-Path $runDirectory 'package.json'
# Keep large fresh Cook/stage intermediates on D:, final deliverable on E:.
$workDirectory = Join-Path $workspaceRoot ('docs/HarborCity_M5_VS2/package_work/' + $runId)
if (-not $NoticeFiles.Count) { $NoticeFiles = @((Join-Path $workspaceRoot 'docs/HarborCity_M5_VS2/part3/ASSET_LICENSE_NOTICES.md')) }

function Invoke-M5CookedMapAudit {
    param([Parameter(Mandatory=$true)][string]$CandidateDirectory, [Parameter(Mandatory=$true)][string]$CookedProject)
$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Join-Path $workspaceRoot 'HarborCity'
$projectFile = Join-Path $projectRoot 'HarborCity.uproject'
$contentRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'Content')).TrimEnd('\','/') + '\'
$buildRoot = [IO.Path]::GetFullPath('E:\GameDev\Builds\HarborCity\M5_VS2').TrimEnd('\')
$candidatePath = [IO.Path]::GetFullPath($CandidateDirectory).TrimEnd('\','/')
if ((Split-Path $candidatePath -Parent) -ine $buildRoot -or
    (Split-Path $candidatePath -Leaf) -notlike 'Candidate_*') {
    throw 'CandidateDirectory must be a Candidate_* directly inside the authorized M5_VS1 build root.'
}
$editorPath = 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$cacheRoot = 'D:\GameDev\Cache\Unreal\HarborCity'
$cookedProject = [IO.Path]::GetFullPath($CookedProject)
$allowedCookRoot = [IO.Path]::GetFullPath((Join-Path $workspaceRoot 'docs/HarborCity_M5_VS2/package_work')).TrimEnd('\','/') + '\'
if (-not $cookedProject.StartsWith($allowedCookRoot,[StringComparison]::OrdinalIgnoreCase) -or
    $cookedProject -notmatch '[\\/]Cooked[\\/]Windows[\\/]HarborCity$') { throw 'Cooked audit input escapes this milestone work root.' }
$registryPath = Join-Path $cookedProject 'AssetRegistry.bin'
$referencedSetPath = Join-Path $cookedProject 'Metadata\ReferencedSet.txt'
$expectedPackage = '/Game/HarborCity/M5VS2/Part3/Town_ea45a319b3dd/L_HarborTown'
$expectedWorld = $expectedPackage + '.L_HarborTown'
$runId = (Get-Date -Format 'yyyyMMdd_HHmmss_fff') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$runDirectory = Join-Path $workspaceRoot ('docs\HarborCity_M5_VS2\commandlets\' + $runId + '_cooked_maps')
$dumpDirectory = Join-Path $runDirectory 'registry_dump'
$reportPath = Join-Path $runDirectory 'map_audit.json'
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$record = [ordered]@{
    started_at=(Get-Date).ToString('o');status='RUNNING';candidate_directory=$candidatePath;
    report_path=$reportPath;registry_path=$registryPath;referenced_set_path=$referencedSetPath;
    expected_game_world=$expectedWorld;commandlet_exit_code=$null;game_worlds=@();
    source_maps_in_referenced_set=@();runtime_verification='NOT_RUN'
}
try {
    foreach ($requiredPath in @($projectFile,$editorPath,$registryPath,$referencedSetPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) { throw "Missing required file; wait for Cook finalisation: $requiredPath" }
    }
    # Never inspect ue.projectstore: it may contain local Zen credentials.
    foreach ($inputPath in @($registryPath,$referencedSetPath)) {
        $checkPath = $inputPath
        while ($checkPath) {
            if ((Get-Item -LiteralPath $checkPath -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Cooked input path contains a junction or symbolic link: $checkPath"
            }
            $checkPath = Split-Path $checkPath -Parent
        }
    }
    foreach ($editor in @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'")) {
        if (-not $editor.CommandLine) { throw "Cannot identify editor PID $($editor.ProcessId); inspect it before this audit." }
        if ($editor.CommandLine.IndexOf($projectFile,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $editor.CommandLine -match '(?i)(^|[\\/"\s])HarborCity\.uproject(["\s]|$)') {
            throw "HarborCity editor/cooker PID $($editor.ProcessId) is still running; finish it normally before this audit."
        }
    }
    $record.registry_sha256 = (Get-FileHash -LiteralPath $registryPath -Algorithm SHA256).Hash
    $record.referenced_set_sha256 = (Get-FileHash -LiteralPath $referencedSetPath -Algorithm SHA256).Hash
    $commandletArguments = @($projectFile,'-run=DumpAssetRegistry',"-Path=$registryPath",
        "-OutDir=$dumpDirectory",'-Class','-LinesPerPage=100000','-unattended','-NullRHI',
        '-nosplash','-nosound','-stdout','-FullStdOutLogOutput','-language=en',
        "-LocalDataCachePath=$cacheRoot\DDC", "-ZenDataPath=$cacheRoot\Zen",
        ('-abslog=' + (Join-Path $runDirectory 'commandlet.log')))
    $record.executable = $editorPath
    $record.arguments = $commandletArguments
    $record | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding utf8
    Push-Location $projectRoot
    try {
        # Out-Host keeps engine output out of this helper's returned result object.
        & $editorPath @commandletArguments 2>&1 |
            Tee-Object -FilePath (Join-Path $runDirectory 'console.log') | Out-Host
        $record.commandlet_exit_code = $LASTEXITCODE
    } finally { Pop-Location }
    if ($record.commandlet_exit_code -ne 0) { throw "DumpAssetRegistry failed with exit code $($record.commandlet_exit_code)." }
    if ((Get-FileHash -LiteralPath $registryPath -Algorithm SHA256).Hash -ne $record.registry_sha256 -or
        (Get-FileHash -LiteralPath $referencedSetPath -Algorithm SHA256).Hash -ne $record.referenced_set_sha256) {
        throw 'Cooked inputs changed during the audit; do not accept an inconsistent snapshot.'
    }

    # AssetRegistryState.cpp PrintAssetDataMap emits one-tab class headings followed
    # by two-tab object paths. Preserve state across Page_*.txt boundaries.
    $pages = @(Get-ChildItem -LiteralPath $dumpDirectory -File -Filter 'Page_*.txt' | Sort-Object Name)
    if ($pages.Count -eq 0) { throw 'DumpAssetRegistry returned no pages.' }
    $record.dump_pages = @($pages | Select-Object -ExpandProperty FullName)
    $inClassSection = $false
    $inWorldGroup = $false
    $sectionCount = 0
    $closedSectionCount = 0
    $worldGroupCount = 0
    $declaredWorldCount = -1
    $worlds = [Collections.Generic.List[string]]::new()
    $registryPackages = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($page in $pages) {
        foreach ($line in [IO.File]::ReadAllLines($page.FullName)) {
            if ($line -ceq '--- Begin CachedAssetsByClass ---') {
                $inClassSection=$true; $inWorldGroup=$false; $sectionCount++; continue
            }
            if ($line -match '^--- End CachedAssetsByClass : \d+ entries ---$') {
                $inClassSection=$false; $inWorldGroup=$false; $closedSectionCount++; continue
            }
            if (-not $inClassSection) { continue }
            if ($line -match '^\t\t(?<asset>/Game/[^\s]+)$') {
                [void]$registryPackages.Add($Matches['asset'].Split('.')[0])
            }
            if ($line -match '^\t(?<class>/[^\s]+) : (?<count>\d+) item\(s\)$') {
                $inWorldGroup = $Matches['class'] -ceq '/Script/Engine.World'
                if ($inWorldGroup) { $worldGroupCount++; $declaredWorldCount=[int]$Matches['count'] }
                continue
            }
            if ($inWorldGroup -and $line -match '^\t\t(?<object>/[^\s]+)$') { $worlds.Add($Matches['object']) }
        }
    }
    $record.world_class_object_count = $declaredWorldCount
    $record.all_worlds = @($worlds.ToArray())
    if ($sectionCount -ne 1 -or $closedSectionCount -ne 1 -or $worldGroupCount -ne 1 -or
        $worlds.Count -ne $declaredWorldCount) {
        throw 'Class dump is incomplete or does not match the verified AssetRegistry dump format.'
    }
    $gameWorlds = @($worlds.ToArray() | Where-Object { $_.StartsWith('/Game/',[StringComparison]::OrdinalIgnoreCase) })
    $record.game_worlds = $gameWorlds
    if ($gameWorlds.Count -ne 1 -or $gameWorlds[0] -ine $expectedWorld) {
        throw 'Runtime asset registry does not contain exactly the required M5 /Game World.'
    }

    # ReferencedSet is the current Cook's lower-case committed package list used by
    # Zen/staging. Cross-check all actual project source maps, including retained M0 maps.
    $referenceLines = [IO.File]::ReadAllLines($referencedSetPath)
    if ($referenceLines.Count -eq 0 -or $referenceLines[0] -cne '# Version 1') { throw 'Unsupported ReferencedSet format.' }
    $referencedPackages = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in $referenceLines) {
        if ($line.StartsWith('/')) { [void]$referencedPackages.Add($line.Trim()) }
    }
    $sourceMaps = @(Get-ChildItem -LiteralPath $contentRoot -Recurse -File -Filter '*.umap' | ForEach-Object {
        $sourcePath = [IO.Path]::GetFullPath($_.FullName)
        if (-not $sourcePath.StartsWith($contentRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Source map escapes project Content.' }
        $relative = $sourcePath.Substring($contentRoot.Length).Replace('\','/')
        '/Game/' + $relative.Substring(0,$relative.Length - '.umap'.Length)
    } | Sort-Object)
    $record.project_source_maps = $sourceMaps
    $matchedSourceMaps = @($sourceMaps | Where-Object { $referencedPackages.Contains($_) })
    $record.source_maps_in_referenced_set = $matchedSourceMaps
    $record.referenced_package_count = $referencedPackages.Count
    if ($matchedSourceMaps.Count -ne 1 -or $matchedSourceMaps[0] -ine $expectedPackage) {
        throw 'ReferencedSet cross-check did not contain exactly the M5 source map.'
    }
    $m4AssetRoot = Join-Path $contentRoot 'HarborCity/M4'
    if (-not (Test-Path -LiteralPath $m4AssetRoot -PathType Container)) { throw 'M4 authored assets are missing.' }
    $m4Packages = @(Get-ChildItem -LiteralPath $m4AssetRoot -Recurse -File -Filter '*.uasset' | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($contentRoot, $_.FullName).Replace('\','/')
        '/Game/' + $relative.Substring(0,$relative.Length - '.uasset'.Length)
    } | Sort-Object)
    $m4Missing = @($m4Packages | Where-Object { -not $referencedPackages.Contains($_) })
    $record.m4_source_asset_packages = $m4Packages
    $record.m4_missing_from_cook = $m4Missing
    if ($m4Packages.Count -lt 10 -or $m4Missing.Count -ne 0) {
        throw ('M4 asset Cook coverage incomplete: ' + ($m4Missing -join ', '))
    }
    $record.m4_asset_cook_coverage = 'PASS'
    $m5Coverage = Assert-HCM5VS1CookCoverage -ContentRoot $contentRoot -ReferencedPackages $referencedPackages -RegistryPackages $registryPackages
    $record.m5_asset_cook_coverage = $m5Coverage.status
    $record.m5_required_packages = $m5Coverage.required_packages
    $record.m5_audited_packages = $m5Coverage.audited_packages
    $record.m5_missing_from_cook = $m5Coverage.missing_from_cook
    $record.m5_missing_from_runtime_registry = $m5Coverage.missing_from_runtime_registry
    $record.m5_excluded_asset_guard = $m5Coverage.excluded_asset_guard
    $record.m5_forbidden_in_cook = $m5Coverage.forbidden_in_cook
    $record.m5_forbidden_in_runtime_registry = $m5Coverage.forbidden_in_runtime_registry
    $record.status = 'PASS'
} catch {
    $record.status = 'FAIL'
    $record.error = $_.Exception.Message
} finally {
    $record.ended_at = (Get-Date).ToString('o')
    $record | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding utf8
}
if ($record.status -ne 'PASS') { throw "Cooked map audit failed: $($record.error) Evidence: $reportPath" }
[pscustomobject]$record


}


function Get-M5DefaultMapConfiguration {
    $defaultEngine = Join-Path (Split-Path $projectFile -Parent) 'Config\DefaultEngine.ini'
    $defaultGame = Join-Path (Split-Path $projectFile -Parent) 'Config\DefaultGame.ini'
    $defaultMapMatches = @([regex]::Matches([IO.File]::ReadAllText($defaultEngine), '(?m)^\s*GameDefaultMap\s*=\s*([^\r\n]+)'))
    if ($defaultMapMatches.Count -ne 1) { throw 'Expected exactly one explicit project GameDefaultMap.' }
    $defaultMap = $defaultMapMatches[0].Groups[1].Value.Trim()
    if ($defaultMap -cne ($map + '.L_HarborTown') -and $defaultMap -cne $map) {
        throw 'Project GameDefaultMap must be the M5 CyberHarbor map before M5 packaging.'
    }
    $cookMapMatches = @([regex]::Matches([IO.File]::ReadAllText($defaultGame), '(?m)^\s*\+MapsToCook\s*=\s*\(FilePath="([^"]+)"\)\s*$'))
    $configuredCookMaps = @($cookMapMatches | ForEach-Object { $_.Groups[1].Value })
    if ($configuredCookMaps.Count -ne 1 -or $configuredCookMaps[0] -cne $map) {
        throw 'Project MapsToCook must contain exactly the M5 CyberHarbor map; retained M1 assets must not become the default Cook map.'
    }
    $cookDirectoryMatches = @([regex]::Matches([IO.File]::ReadAllText($defaultGame), '(?m)^\s*\+DirectoriesToAlwaysCook\s*=\s*\(Path="([^"]+)"\)\s*$'))
    $configuredCookDirectories = @($cookDirectoryMatches | ForEach-Object { $_.Groups[1].Value })
    $null = Assert-HCM5VS1HeroCookDirectories -Directories $configuredCookDirectories
    $neverCookMatches = @([regex]::Matches([IO.File]::ReadAllText($defaultGame), '(?m)^\s*\+DirectoriesToNeverCook\s*=\s*\(Path="([^"]+)"\)\s*$'))
    $configuredNeverCook = @($neverCookMatches | ForEach-Object { $_.Groups[1].Value })
    $null = Assert-HCM5VS1NeverCookDirectories -Directories $configuredNeverCook
    [pscustomobject]@{default_map=$defaultMap;maps_to_cook=$configuredCookMaps;directories_to_always_cook=$configuredCookDirectories;
        directories_to_never_cook=$configuredNeverCook;status='PASS'}
}

function Get-ProjectContentInventory {
    $contentProjectRoot = Split-Path $projectFile -Parent
    @(Get-ChildItem -LiteralPath (Join-Path $contentProjectRoot 'Content') -Recurse -File | Sort-Object FullName | ForEach-Object {
        Assert-HCM5VS1NoReparsePath $_.FullName
        [ordered]@{path=[IO.Path]::GetRelativePath($contentProjectRoot,$_.FullName).Replace('\','/');
            bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}

function Assert-NoProjectEditor {
    $editors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'")
    foreach ($editor in $editors) {
        if (-not $editor.CommandLine) { throw "Cannot identify editor PID $($editor.ProcessId); inspect it before packaging." }
        if ($editor.CommandLine.IndexOf($projectFile,[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $editor.CommandLine -match '(?i)(^|[\\/"\s])HarborCity\.uproject(["\s]|$)') {
            throw "HarborCity editor PID $($editor.ProcessId) is running. Save and close it normally before packaging."
        }
    }
}

function Assert-NoReparseParents([string]$Path) {
    $itemPath = [IO.Path]::GetFullPath($Path)
    while ($itemPath) {
        if (Test-Path -LiteralPath $itemPath) {
            $item = Get-Item -Force -LiteralPath $itemPath
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Output path contains a junction or symbolic link: $itemPath" }
        }
        $parentPath = Split-Path $itemPath -Parent
        if ($parentPath -eq $itemPath) { break }
        $itemPath = $parentPath
    }
}

function Get-ProjectSourceInventory {
    $sourceProjectRoot = Split-Path $projectFile -Parent
    $inputFiles = @(Get-ChildItem -LiteralPath (Join-Path $sourceProjectRoot 'Source'),(Join-Path $sourceProjectRoot 'Config') -Recurse -File)
    $inputFiles += Get-Item -LiteralPath $projectFile
    @($inputFiles | Sort-Object FullName | ForEach-Object {
        Assert-HCM5VS1NoReparsePath $_.FullName
        [ordered]@{path=[IO.Path]::GetRelativePath($sourceProjectRoot,$_.FullName).Replace('\','/');
            bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
}

New-Item -ItemType Directory -Path $runDirectory | Out-Null
$record = [ordered]@{
    schema_version=6; milestone='M5_VS2'; started_at=(Get-Date).ToString('o'); status='RUNNING'; project=$projectFile;
    engine=$engineRoot; target='HarborCity Win64 Development'; maps=@($map);
    candidate_directory=$candidateDirectory; actual_executable=$null; cache_root=$cacheRoot;
    max_parallel_actions=$MaxParallelActions; cook_process_count=1; shader_workers=0;
    stages=@(); packaged_runtime='NOT_RUN'; reuse_cook_requested=$false; full_cook_required=$true
}
$exitCode = 1
$oldUatLog = [Environment]::GetEnvironmentVariable('uebp_LogFolder','Process')
$oldUatFinalLog = [Environment]::GetEnvironmentVariable('uebp_FinalLogFolder','Process')
$nativeCacheEnvironment = [ordered]@{
    'UE-ZenDataPath'=(Join-Path $cacheRoot 'Zen');
    'UE-ZenSubprocessDataPath'=(Join-Path $cacheRoot 'Zen');
    'UE-LocalDataCachePath'=(Join-Path $cacheRoot 'DDC')
}
$previousCacheEnvironment = @{}
foreach ($environmentName in $nativeCacheEnvironment.Keys) {
    $previousCacheEnvironment[$environmentName] = [Environment]::GetEnvironmentVariable($environmentName,'Process')
}
try {
    foreach ($requiredPath in @($projectFile,$buildScript,$dotnetPath,$uatPath,$editorPath,$powershellPath,
        (Join-Path $workspaceRoot 'HarborCity\Content\HarborCity\M5VS2\Part3\Town_ea45a319b3dd\L_HarborTown.umap'))) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) { throw "Missing required file: $requiredPath" }
    }
    Assert-HCM5VS1NoEngineProcess
    # Fail before compile rather than letting UBT silently turn encryption off.
    # This report contains hash/flags/length only, never the private key.
    $record.crypto_configuration = Assert-HCM5VS1CryptoConfiguration -ProjectRoot (Split-Path $projectFile -Parent)
    $record.asset_extraction_protection = 'NOT_RUN_REQUIRES_BOUND_NATIVE_VERIFICATION'
    $record.private_review_exclusions = @('HarborCity/Config/DefaultCrypto.ini','HarborCity/Config/DefaultEncryption.ini',
        '**/Crypto.json','**/CryptoKeys*.json','HarborCity/Intermediate/**','**/response_files_before_bom_*.zip',
        'docs/HarborCity_M5_VS2/package_work/**','assets/M5_VS1/hero/Selestia/PrivateCryptoVerification/**')
    $mapConfiguration = Get-M5DefaultMapConfiguration
    $record.configured_default_map = $mapConfiguration.default_map
    $record.configured_maps_to_cook = $mapConfiguration.maps_to_cook
    $record.configured_cook_directories = $mapConfiguration.directories_to_always_cook
    $record.configured_never_cook_directories = $mapConfiguration.directories_to_never_cook
    $record.default_map_guard = $mapConfiguration.status
    $record.release_scripts = @('package_m5_vs2.ps1','build_m5_vs2.ps1','launch_game_m5_vs1.ps1','launch_editor_game_m5_vs1.ps1','m5_vs2_release_guards.ps1','build.ps1','normalize_response_files.py' | ForEach-Object {
        $path = Join-Path $PSScriptRoot $_
        [ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    })
    # Fail before any compile if a required new runtime asset or notice is absent.
    foreach ($package in @(Get-HCM5VS1RequiredPackages)) {
        $assetPath = Join-Path $workspaceRoot ('HarborCity/Content/' + $package.Substring(6) + '.uasset')
        if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) { throw "Missing required cooked source asset: $package" }
    }
    $noticeNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $noticeInputs = @(foreach ($notice in $NoticeFiles) {
        $notice = [IO.Path]::GetFullPath($notice)
        Assert-HCM5VS1NoReparsePath $notice
        $file = Get-Item -LiteralPath $notice
        if ($file.PSIsContainer -or $file.Length -lt 1 -or -not $noticeNames.Add($file.Name)) { throw 'Notice files must be nonempty files with unique names.' }
        if ($file.Extension -notin @('.md','.txt','.pdf') -or $file.Name -match '(?i)crypto|encryption|definitions|response_files') {
            throw 'Only public Markdown/text/PDF license notices may be copied into the archive.'
        }
        if ($file.Extension -in @('.md','.txt')) {
            $noticeText = [IO.File]::ReadAllText($file.FullName)
            if ((Protect-HCM5VS1CryptoLogLine $noticeText) -cne $noticeText) { throw 'A notice contains private key material; value suppressed.' }
        }
        [pscustomobject]@{source=$file.FullName;name=$file.Name;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
    })
    $record.license_notice_inputs = $noticeInputs
    Assert-NoReparseParents $workDirectory
    if (Test-Path -LiteralPath $workDirectory) { throw 'Fresh Cook work directory already exists.' }
    $workDrive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($workDirectory))
    $record.work_free_bytes_before = $workDrive.AvailableFreeSpace
    $record.minimum_work_free_bytes = [long]$MinimumWorkFreeSpaceGB * 1GB
    if ($workDrive.AvailableFreeSpace -lt $record.minimum_work_free_bytes) { throw "Insufficient D: work space; $MinimumWorkFreeSpaceGB GiB required." }
    $record.work_directory = $workDirectory
    Assert-NoReparseParents $buildRoot
    $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($buildRoot))
    $record.free_bytes_before = $drive.AvailableFreeSpace
    $record.minimum_free_bytes = [long]$MinimumFreeSpaceGB * 1GB
    $record.existing_candidates = if (Test-Path -LiteralPath $buildRoot) {
        @(Get-ChildItem -LiteralPath $buildRoot -Directory | Select-Object -ExpandProperty Name)
    } else { @() }
    if ($drive.AvailableFreeSpace -lt $record.minimum_free_bytes) { throw "Insufficient free space on E:; at least $MinimumFreeSpaceGB GiB is required." }
    New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
    if (Test-Path -LiteralPath $candidateDirectory) { throw "Candidate already exists: $candidateDirectory" }
    New-Item -ItemType Directory -Path $candidateDirectory | Out-Null
    # Bind this unique package to its actual inputs without copying private Config contents.
    $sourceInputs = @(Get-ProjectSourceInventory)
    $sourceInputsJson = ConvertTo-Json -InputObject $sourceInputs -Depth 5 -Compress
    $sourceInventoryPath = Join-Path $candidateDirectory 'source_inventory.json'
    ConvertTo-Json -InputObject $sourceInputs -Depth 5 | Set-Content -LiteralPath $sourceInventoryPath -Encoding utf8
    Copy-Item -LiteralPath $sourceInventoryPath -Destination (Join-Path $runDirectory 'source_inventory.json')
    $record.source_inventory = $sourceInventoryPath
    $record.source_inventory_sha256 = (Get-FileHash -LiteralPath $sourceInventoryPath -Algorithm SHA256).Hash
    # Bind every Content file separately from Source/Config/uproject.
    $contentInputs = @(Get-ProjectContentInventory)
    $contentInputsJson = ConvertTo-Json -InputObject $contentInputs -Depth 5 -Compress
    $contentInventoryPath = Join-Path $candidateDirectory 'content_inventory.json'
    ConvertTo-Json -InputObject $contentInputs -Depth 5 | Set-Content -LiteralPath $contentInventoryPath -Encoding utf8
    Copy-Item -LiteralPath $contentInventoryPath -Destination (Join-Path $runDirectory 'content_inventory.json')
    $record.content_inventory = $contentInventoryPath
    $record.content_inventory_sha256 = (Get-FileHash -LiteralPath $contentInventoryPath -Algorithm SHA256).Hash
    New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
    # UnrealPak's StartZenServerForStage does not receive AdditionalCookerOptions.
    # Process environment reaches every UAT child; restore the caller's values below.
    foreach ($environmentName in $nativeCacheEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($environmentName,$nativeCacheEnvironment[$environmentName],'Process')
    }
    $record.native_cache_environment = $nativeCacheEnvironment

    # Build both targets from this frozen input inventory. A stale editor module
    # must not cook new story/property data even when the Game target built cleanly.
    foreach ($buildTarget in @('Editor','Game')) {
    $buildArguments = @('-NoLogo','-NoProfile','-File',$buildScript,'-Target',$buildTarget,'-MaxParallelActions',"$MaxParallelActions")
    $buildStage = [ordered]@{name=($buildTarget+'Build');status='RUNNING';started_at=(Get-Date).ToString('o');
        executable=$powershellPath;arguments=$buildArguments;exit_code=$null}
    $record.stages += $buildStage
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8
    & $buildStage.executable @buildArguments 2>&1 | ForEach-Object { Protect-HCM5VS1CryptoLogLine $_ } |
        Tee-Object -FilePath (Join-Path $runDirectory ($buildTarget.ToLower()+'_build_console.log'))
    $buildExitCode = $LASTEXITCODE
    $record.text_log_privacy = Protect-HCM5VS1RecentBuildLogs $evidenceRoot $packageStartedUtc
    $buildStage.exit_code = $buildExitCode
    $buildStage.ended_at = (Get-Date).ToString('o')
    $buildStage.status = if ($buildExitCode -eq 0) { 'PASS' } else { 'FAIL' }
    if ($buildExitCode -ne 0) { $exitCode=$buildExitCode; throw "$buildTarget build failed: exit $buildExitCode" }
    }
    Assert-HCM5VS1NoEngineProcess
    $uatLogDirectory = Join-Path $runDirectory 'uat_logs'
    [Environment]::SetEnvironmentVariable('uebp_LogFolder',$uatLogDirectory,'Process')
    [Environment]::SetEnvironmentVariable('uebp_FinalLogFolder',$uatLogDirectory,'Process')
    # CookByTheBook uses an explicit output directory verbatim; UAT staging only
    # avoids appending the cook platform when the directory already ends in Windows.
    $cookDirectory = Join-Path $workDirectory 'Cooked\Windows'
    $stageDirectory = Join-Path $workDirectory 'Staged'
    $archiveDirectory = Join-Path $candidateDirectory 'Archive'
    # Verified locally: ProjectParams.cs / CookCommand.Automation.cs / CookOnTheFlyServer.cpp.
    # NoShaderWorker is a real ShaderCompiler.cpp option: compile in the single cooker process.
    $cookerOptions = "-CookProcessCount=1 -noshaderworker -LocalDataCachePath=$cacheRoot\DDC -ZenDataPath=$cacheRoot\Zen -language=en"
    # CookCommandlet.cpp accepts repeated -CookDir=<absolute directory> switches.
    # Explicit approved roots cover string/sibling loads. NeverCook still excludes
    # offline Retarget authoring assets inside the otherwise formal hero directory.
    $extraCookRoots = @(Get-HCM5VS1CookDirectories | ForEach-Object { $_.Substring(6) })
    $null = Assert-HCM5VS1HeroCookDirectories -Directories @($extraCookRoots | ForEach-Object { '/Game/' + $_ })
    foreach ($relativeRoot in $extraCookRoots) {
        $root = Join-Path $workspaceRoot ('HarborCity/Content/' + $relativeRoot)
        if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Missing explicit Cook directory: $root" }
        $cookerOptions += ' -CookDir="' + $root + '"'
    }
    $record.explicit_cook_directories = $extraCookRoots
    $record.staging_directory = $stageDirectory
    # M5-VS1 always performs a fresh full Cook in this invocation's unique candidate.
    $uatArguments = @('-NoCompile','-NoP4','-UTF8Output','BuildCookRun',"-project=$projectFile",
        '-target=HarborCity','-platform=Win64','-clientconfig=Development','-skipbuild','-nocompileeditor',
        '-cook','-stage','-pak','-package','-archive',"-map=$map",'-skipeditorcontent',
        "-unrealexe=$editorPath", "-CookOutputDir=$cookDirectory", "-stagingdirectory=$stageDirectory",
        "-archivedirectory=$archiveDirectory",'-unattended')
    $uatArguments += "-AdditionalCookerOptions=$cookerOptions"
    $uatStageName = 'CookStagePakPackageArchive'
    $record.cook_source_candidate = $candidateDirectory
    $record.cook_output_directory = $cookDirectory
    $uatStage = [ordered]@{name=$uatStageName;status='RUNNING';started_at=(Get-Date).ToString('o');
        executable=$dotnetPath;arguments=@($uatPath)+$uatArguments;exit_code=$null;
        note='UAT internal stages share one process exit code; console and UAT logs preserve each stage boundary.'}
    $record.stages += $uatStage
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8
    Push-Location (Split-Path $uatPath -Parent)
    try {
        & $dotnetPath $uatPath @uatArguments 2>&1 | ForEach-Object { Protect-HCM5VS1CryptoLogLine $_ } |
            Tee-Object -FilePath (Join-Path $runDirectory 'uat_console.log')
        $uatExitCode = $LASTEXITCODE
    } finally { Pop-Location }
    $uatStage.exit_code = $uatExitCode
    $record.text_log_privacy = Protect-HCM5VS1RecentBuildLogs $evidenceRoot $packageStartedUtc
    $uatStage.ended_at = (Get-Date).ToString('o')
    $uatStage.status = if ($uatExitCode -eq 0) { 'PASS' } else { 'FAIL' }
    if ($uatExitCode -ne 0) { $exitCode=$uatExitCode; throw "UAT failed: exit $uatExitCode; candidate and logs are retained." }
    if (Select-String -LiteralPath (Join-Path $runDirectory 'uat_console.log') -Pattern 'Encryption will be disabled' -Quiet) {
        throw 'UAT disabled requested asset encryption; this candidate cannot pass.'
    }

    $mapAudit = Invoke-M5CookedMapAudit -CandidateDirectory $candidateDirectory -CookedProject (Join-Path $cookDirectory 'HarborCity')
    $record.cooked_project_maps = @($mapAudit.source_maps_in_referenced_set)
    $record.cooked_game_worlds = @($mapAudit.game_worlds)
    $record.cooked_map_audit = $mapAudit.report_path
    $record.m5_asset_cook_coverage = $mapAudit.m5_asset_cook_coverage
    $record.m5_excluded_asset_guard = $mapAudit.m5_excluded_asset_guard
    $record.m5_required_packages = $mapAudit.m5_required_packages
    $record.m5_audited_packages = $mapAudit.m5_audited_packages
    $record.stages += [ordered]@{
        name='CookedMapAudit';status=$mapAudit.status;executable=$mapAudit.executable;
        exit_code=$mapAudit.commandlet_exit_code;reused_existing_audit=$false;
        report_path=$mapAudit.report_path
    }

    # Select the real game binary, not the small bootstrap launcher at the archive root.
    $executables = @(Get-ChildItem -LiteralPath $archiveDirectory -Recurse -File -Filter 'HarborCity.exe' |
        Where-Object { $_.FullName -match '[\\/]HarborCity[\\/]Binaries[\\/]Win64[\\/]HarborCity\.exe$' })
    if ($executables.Count -ne 1) { throw "Expected one archived Win64 game executable, found $($executables.Count)." }
    $gameExecutable = $executables[0].FullName
    $gameDirectory = Split-Path (Split-Path (Split-Path $gameExecutable -Parent) -Parent) -Parent
    $paks = @(Get-ChildItem -LiteralPath (Join-Path $gameDirectory 'Content\Paks') -File -Filter '*.pak')
    if ($paks.Count -eq 0) { throw 'Archive contains no game pak; executable presence alone is not a package.' }
    $record.actual_executable = $gameExecutable
    $record.actual_executable_sha256 = (Get-FileHash -LiteralPath $gameExecutable -Algorithm SHA256).Hash
    $record.archive_directory = $archiveDirectory
    $record.pak_files = @(Get-HCM5VS1ArchiveFingerprints -PakDirectory (Join-Path $gameDirectory 'Content/Paks'))
    $record.archive_fingerprint_guard = 'PASS'
    # Copy actual local license/asset notices verbatim before inventorying the package.
    $noticeDirectory = Join-Path $gameDirectory 'ThirdPartyNotices'
    New-Item -ItemType Directory -Path $noticeDirectory | Out-Null
    $record.license_notices = @(foreach ($notice in $noticeInputs) {
        if ((Get-FileHash -LiteralPath $notice.source -Algorithm SHA256).Hash -ine $notice.sha256) { throw 'License notice changed during package.' }
        $destination = Join-Path $noticeDirectory $notice.name
        Copy-Item -LiteralPath $notice.source -Destination $destination
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ine $notice.sha256) { throw 'Packaged notice copy differs.' }
        [pscustomobject]@{source=$notice.source;archive_relative_path=[IO.Path]::GetRelativePath($archiveDirectory,$destination).Replace('\','/');sha256=$notice.sha256;bytes=$notice.bytes}
    })
    $record.license_notice_guard = 'PASS'
    $record.private_archive_guard = Assert-HCM5VS1NoPrivateArchiveFiles $archiveDirectory
    $archiveInventory = @(Get-HCM5VS1ArchiveInventory $archiveDirectory)
    $record.archive_inventory = Join-Path $candidateDirectory 'archive_inventory.json'
    ConvertTo-Json -InputObject $archiveInventory -Depth 5 | Set-Content -LiteralPath $record.archive_inventory -Encoding utf8
    Copy-Item -LiteralPath $record.archive_inventory -Destination (Join-Path $runDirectory 'archive_inventory.json')
    $record.archive_inventory_sha256 = (Get-FileHash -LiteralPath $record.archive_inventory -Algorithm SHA256).Hash
    $record.archive_inventory_file_count = $archiveInventory.Count
    $record.full_archive_inventory_guard = 'PASS'
    $record.work_free_bytes_after = $workDrive.AvailableFreeSpace
    $record.free_bytes_after = $drive.AvailableFreeSpace
    $record.stages += [ordered]@{name='VerifyArchiveFiles';status='PASS';
        ended_at=(Get-Date).ToString('o');note='File/hash verification only; actual game launch remains NOT_RUN.'}
    $sourceAfterJson = ConvertTo-Json -InputObject @(Get-ProjectSourceInventory) -Depth 5 -Compress
    if ($sourceAfterJson -cne $sourceInputsJson) { throw 'Project Source/Config changed during this build; candidate inputs are not stable.' }
    $record.source_inputs_stable_during_package = $true
    $contentAfterJson = ConvertTo-Json -InputObject @(Get-ProjectContentInventory) -Depth 5 -Compress
    if ($contentAfterJson -cne $contentInputsJson) { throw 'Project Content changed during packaging; candidate art inputs are not stable.' }
    $record.content_inputs_stable_during_package = $true
    foreach ($script in $record.release_scripts) {
        if ((Get-FileHash -LiteralPath $script.path -Algorithm SHA256).Hash -ine $script.sha256) { throw 'Release tooling changed during packaging.' }
    }
    $record.release_scripts_stable_during_package = $true
    $record.status = 'PASS'
    $exitCode = 0
} catch {
    $record.error = Protect-HCM5VS1CryptoLogLine $_.Exception.Message
    $record.status = 'FAIL'
    foreach ($unfinishedStage in $record.stages) {
        if ($unfinishedStage.status -eq 'RUNNING') {
            $unfinishedStage.status = 'FAIL'
            $unfinishedStage.ended_at = (Get-Date).ToString('o')
            $unfinishedStage.error = $record.error
        }
    }
    Write-Warning $record.error
} finally {
    [Environment]::SetEnvironmentVariable('uebp_LogFolder',$oldUatLog,'Process')
    [Environment]::SetEnvironmentVariable('uebp_FinalLogFolder',$oldUatFinalLog,'Process')
    foreach ($environmentName in $nativeCacheEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($environmentName,$previousCacheEnvironment[$environmentName],'Process')
    }
    $record.ended_at = (Get-Date).ToString('o')
    $record.exit_code = $exitCode
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding utf8
    if (Test-Path -LiteralPath $candidateDirectory -PathType Container) {
        $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $candidateDirectory 'package_manifest.json') -Encoding utf8
    }
}
Write-Output "Package evidence: $recordPath"
if ($record.actual_executable) { Write-Output "Actual game executable: $($record.actual_executable)" }
exit $exitCode


