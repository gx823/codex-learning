$ErrorActionPreference = 'SilentlyContinue'

$gameRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$gameUrl = 'http://127.0.0.1:4173/'
$port = '4173'
$localMusic = Join-Path $gameRoot '.local-assets\styx-helix.mp3'
$servedMusic = Join-Path $gameRoot 'dist\client\audio\styx-helix.mp3'

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
    -ArgumentList @('run', 'start', '--', '--port', $port) `
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
