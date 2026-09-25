#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath('E:/GameDev/Cache/Unreal/HarborCity').TrimEnd('\')
$destination=[IO.Path]::GetFullPath('D:/GameDev/Cache/Unreal/HarborCity').TrimEnd('\')
$reportRoot=Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/HarborCity_M5_VS2/research'
$report=Join-Path $reportRoot ('storage_relocation_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.json')
if($source -cne 'E:\GameDev\Cache\Unreal\HarborCity' -or $destination -cne 'D:\GameDev\Cache\Unreal\HarborCity'){throw 'Unexpected resolved cache paths'}
if(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor(-Cmd)?|HarborCity|ShaderCompileWorker|zenserver|zen)\.exe$'}){throw 'Engine/cache process active; no filesystem changes'}
if(Test-Path -LiteralPath $destination){throw 'Destination must be new; preserve any existing files'}
$items=@(Get-ChildItem -LiteralPath $source -Force -Recurse)
if(@($items|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count -or ((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Do not traverse reparse points'}
$files=@($items|Where-Object {-not $_.PSIsContainer})
$record=[ordered]@{status='RUNNING';source=$source;destination=$destination;started_at=(Get-Date).ToString('o');files=@();copied_bytes=0;deleted_source_files=0;policy='User authorized D-drive generated files and unused cache cleanup; source removed only after per-file SHA256 verification. No original assets, builds, saves or evidence are moved.'}
$record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $report -Encoding utf8
try{
    foreach($file in $files){
        $relative=[IO.Path]::GetRelativePath($source,$file.FullName)
        $target=[IO.Path]::GetFullPath((Join-Path $destination $relative))
        if(-not $file.FullName.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase) -or -not $target.StartsWith($destination+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Cache path escaped bounds'}
        $before=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force|Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
        if((Get-Item -LiteralPath $target).Length -ne $file.Length -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -cne $before){throw 'Copy integrity mismatch'}
        $record.files+=@{relative=$relative;bytes=$file.Length;sha256=$before}
        $record.copied_bytes+=$file.Length
    }
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -match '^(UnrealEditor(-Cmd)?|HarborCity|ShaderCompileWorker|zenserver|zen)\.exe$'}){throw 'Engine/cache started during copy; source retained'}
    $current=@(Get-ChildItem -LiteralPath $source -File -Recurse -Force)
    if($current.Count -ne $record.files.Count){throw 'Source membership changed; source retained'}
    foreach($entry in $record.files){
        $original=[IO.Path]::GetFullPath((Join-Path $source $entry.relative))
        if(-not $original.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -cne $entry.sha256){throw 'Source changed before cleanup'}
    }
    foreach($entry in $record.files){
        $original=[IO.Path]::GetFullPath((Join-Path $source $entry.relative))
        Remove-Item -LiteralPath $original -Force
        $record.deleted_source_files++
    }
    # Empty source directories remain so older launchers can regenerate their cache.
    $record.status='PASS'
}catch{$record.status='FAIL';$record.error=$_.Exception.Message;throw}
finally{$record.ended_at=(Get-Date).ToString('o');$record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $report -Encoding utf8}
[pscustomobject]@{status=$record.status;files=$record.files.Count;bytes=$record.copied_bytes;report=$report}|ConvertTo-Json -Compress
