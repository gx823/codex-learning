param(
    [string]$EngineRoot = 'E:\UE_5.8',
    [switch]$GenerateProjectFiles,
    [switch]$DisableUBA = $true,
    [string]$PythonPath = 'D:\python\python.exe',
    [ValidateSet('Editor','Game')][string]$Target = 'Editor',
    [ValidateSet('Development','Shipping')][string]$Configuration = 'Development',
    [string]$LogRoot = '',
    [ValidateRange(1,8)][int]$MaxParallelActions = 2,
    [string]$CacheRoot = 'E:/GameDev/Cache/Unreal/HarborCity'
)
$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $workspaceRoot 'HarborCity\HarborCity.uproject'
$dotnetPath = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$ubtPath = Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
foreach ($requiredPath in @($projectFile, $dotnetPath, $ubtPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) { throw "Missing required file: $requiredPath" }
}
$operation = if ($GenerateProjectFiles) { 'projectfiles' } elseif ($Target -eq 'Game') { 'game_build' } else { 'editor_build' }
if (-not $LogRoot) { $LogRoot = Join-Path $workspaceRoot 'docs\HarborCity_M0_E2\installed_20260912\builds' }
$resolvedLogRoot = [IO.Path]::GetFullPath($LogRoot)
$allowedDocsRoot = [IO.Path]::GetFullPath((Join-Path $workspaceRoot 'docs')) + [IO.Path]::DirectorySeparatorChar
if (-not $resolvedLogRoot.StartsWith($allowedDocsRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Build evidence must remain inside project docs' }
$runDirectory = Join-Path $resolvedLogRoot ((Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + $operation)
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$ubtLog = Join-Path $runDirectory 'ubt.log'
$buildArguments = if ($GenerateProjectFiles) {
    @('-projectfiles', "-Project=$projectFile", '-game', '-rocket')
} else {
    $targetName = if ($Target -eq 'Game') { 'HarborCity' } else { 'HarborCityEditor' }
    @($targetName, 'Win64', $Configuration, "-Project=$projectFile", '-WaitMutex', '-NoHotReloadFromIDE')
}
if ($DisableUBA) { $buildArguments += '-NoUBA' }
$resolvedCacheRoot=[IO.Path]::GetFullPath($CacheRoot).TrimEnd('\','/')
if($resolvedCacheRoot -notin @('E:\GameDev\Cache\Unreal\HarborCity','D:\GameDev\Cache\Unreal\HarborCity')){throw 'Only the approved HarborCity generated cache locations are accepted'}
$buildArguments += @('-UBADisableRemote', '-UBAHost=127.0.0.1', ('-UBARootDir='+$resolvedCacheRoot+'\UBA'), "-MaxParallelActions=$MaxParallelActions")
$buildArguments += "-Log=$ubtLog"
$record = [ordered]@{started_at=(Get-Date).ToString('o'); executable=$dotnetPath; arguments=@($ubtPath)+$buildArguments; working_directory=(Join-Path $EngineRoot 'Engine\Source'); status='RUNNING'}
$record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'command.json') -Encoding utf8
Write-Output "Build evidence: $runDirectory"
Push-Location (Join-Path $EngineRoot 'Engine\Source')
$buildExitCode = 1
$record.stage = 'preparation'
try {
    if (-not $GenerateProjectFiles) {
        if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) { throw "Missing Python required for the verified Unicode response-file repair: $PythonPath" }
        $prepareArguments = @($buildArguments | Where-Object { -not $_.StartsWith('-Log=') }) + @('-SkipBuild', "-Log=$(Join-Path $runDirectory 'prepare_ubt.log')")
        & $dotnetPath $ubtPath @prepareArguments 2>&1 | Tee-Object -FilePath (Join-Path $runDirectory 'prepare_console.log')
        $prepareExitCode = $LASTEXITCODE
        $record.prepare_arguments = @($ubtPath) + $prepareArguments
        $record.prepare_exit_code = $prepareExitCode
        if ($prepareExitCode -ne 0) { $buildExitCode = $prepareExitCode; throw "Build preparation failed with exit code $prepareExitCode; see $runDirectory" }
        $record.stage = 'response_encoding'
        & $PythonPath (Join-Path $PSScriptRoot 'normalize_response_files.py') --target $Target --configuration $Configuration --evidence-dir $runDirectory
        $record.normalization_exit_code = $LASTEXITCODE
        if ($LASTEXITCODE -ne 0) { $buildExitCode = $LASTEXITCODE; throw 'Response-file encoding repair failed' }
    }
    $record.stage = 'build'
    & $dotnetPath $ubtPath @buildArguments 2>&1 | Tee-Object -FilePath (Join-Path $runDirectory 'console.log')
    $buildExitCode = $LASTEXITCODE
} catch {
    $record.error = $_.Exception.Message
    Write-Warning $record.error
} finally {
    Pop-Location
    $record.ended_at = (Get-Date).ToString('o')
    $record.exit_code = $buildExitCode
    $record.status = if ($buildExitCode -eq 0) { 'PASS' } else { 'FAIL' }
    $record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'command.json') -Encoding utf8
}
exit $buildExitCode
