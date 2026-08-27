$ErrorActionPreference = 'SilentlyContinue'

$gameRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$gameUrl = 'http://127.0.0.1:4173/'
$port = '4173'
$localMusic = Join-Path $gameRoot '.local-assets\styx-helix.mp3'
$gameIndex = Join-Path $gameRoot 'outputs\standalone-dist\index.html'
$servedMusic = Join-Path $gameRoot 'outputs\standalone-dist\audio\styx-helix.mp3'

if (-not (Test-Path -LiteralPath $gameIndex)) {
  $npmCommand = (Get-Command npm.cmd -ErrorAction SilentlyContinue).Source
  if ($npmCommand) {
    Start-Process -FilePath $npmCommand `
      -ArgumentList @('run', 'build:game') `
      -WorkingDirectory $gameRoot `
      -WindowStyle Hidden `
      -Wait
  }
}

if (Test-Path -LiteralPath $localMusic) {
  $servedMusicDirectory = Split-Path -Parent $servedMusic
  New-Item -ItemType Directory -Path $servedMusicDirectory -Force | Out-Null
  Copy-Item -LiteralPath $localMusic -Destination $servedMusic -Force
}

function Test-GameReady {
  try {
    $response = Invoke-WebRequest -Uri $gameUrl -UseBasicParsing -TimeoutSec 2
    return $response.StatusCode -eq 200
  }
  catch {
    return $false
  }
}

if (-not (Test-GameReady)) {
  $npmCommand = (Get-Command npm.cmd -ErrorAction SilentlyContinue).Source
  if (-not $npmCommand) {
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show('Node.js was not found. The game cannot start.', 'Starry Post') | Out-Null
    exit 1
  }

  Start-Process -FilePath $npmCommand `
    -ArgumentList @('run', 'preview:game', '--', '--port', $port) `
    -WorkingDirectory $gameRoot `
    -WindowStyle Hidden

  for ($attempt = 0; $attempt -lt 45; $attempt += 1) {
    Start-Sleep -Milliseconds 500
    if (Test-GameReady) { break }
  }
}

if (Test-GameReady) {
  Start-Process $gameUrl
}
else {
  Add-Type -AssemblyName PresentationFramework
  [System.Windows.MessageBox]::Show('The game took too long to start. Please try again.', 'Starry Post') | Out-Null
}
