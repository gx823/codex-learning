# Add complete retained license texts, then regenerate the affected inventory.
param([Parameter(Mandatory=$true)][string]$PackageRecord)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'm5_vs2_release_guards.ps1')
$w=Split-Path $PSScriptRoot -Parent
$j=Get-Content -LiteralPath $PackageRecord -Raw | ConvertFrom-Json -AsHashtable
if($j.status -ne 'PASS' -or $j.milestone -ne 'M5_VS2'){throw 'Completed VS2 candidate required'}
$archive=[IO.Path]::GetFullPath($j.archive_directory)
if(-not $archive.StartsWith('E:\GameDev\Builds\HarborCity\M5_VS2\Candidate_',[StringComparison]::OrdinalIgnoreCase)){throw 'Candidate scope invalid'}
$before=Join-Path (Split-Path $PackageRecord -Parent) 'package_before_complete_notices.json'
if(Test-Path -LiteralPath $before){throw 'Already amended; inspect prior result'}
Copy-Item -LiteralPath $PackageRecord -Destination $before
$dir=Join-Path $archive 'Windows/HarborCity/ThirdPartyNotices'
if(-not(Test-Path -LiteralPath $dir -PathType Container)){throw 'Actual archive notices missing'}
$added=@()
foreach($name in @('AndroidOpenSourceProject_License.txt','M5_Original_Music_LICENSE.txt','M4R2_Audio_Provenance_original.md')){
 $src=Join-Path $w ('docs/HarborCity_M5_VS1/licenses/'+$name);$dst=Join-Path $dir $name
 if(Test-Path -LiteralPath $dst){throw 'Notice exists; no overwrite'}
 Copy-Item -LiteralPath $src -Destination $dst
 $hash=(Get-FileHash -LiteralPath $src).Hash
 if($hash -ne (Get-FileHash -LiteralPath $dst).Hash){throw 'Notice copy mismatch'}
 $added+=@{source=$src;destination=$dst;sha256=$hash}
}
foreach($entry in @(
 @('KawaiiPhysics/LICENSE','KawaiiPhysics_LICENSE.txt'),
 @('VRM4U/LICENSE','VRM4U_LICENSE.txt'),
 @('VRM4U/LICENSE_assimp','VRM4U_assimp_LICENSE.txt'),
 @('VRM4U/ThirdParty/assimp/LICENSE','assimp_LICENSE.txt')
)){
 $src=Join-Path $w ('HarborCity/Plugins/'+$entry[0]);$dst=Join-Path $dir $entry[1]
 if(Test-Path -LiteralPath $dst){throw 'Notice exists; no overwrite'}
 Copy-Item -LiteralPath $src -Destination $dst
 $hash=(Get-FileHash -LiteralPath $src).Hash
 if($hash -ne (Get-FileHash -LiteralPath $dst).Hash){throw 'Notice copy mismatch'}
 $added+=@{source=$src;destination=$dst;sha256=$hash}
}
$inventory=@(Get-HCM5VS1ArchiveInventory $archive)
ConvertTo-Json -InputObject $inventory -Depth 5 | Set-Content -LiteralPath $j.archive_inventory -Encoding utf8
Copy-Item -LiteralPath $j.archive_inventory -Destination (Join-Path (Split-Path $PackageRecord -Parent) 'archive_inventory.json')
$j.archive_inventory_sha256=(Get-FileHash -LiteralPath $j.archive_inventory).Hash
$j.archive_inventory_file_count=$inventory.Count
$j.post_package_complete_notice_additions=$added
$j.post_notice_binary_check=if((Get-FileHash -LiteralPath $j.actual_executable).Hash -eq $j.actual_executable_sha256){'PASS_UNCHANGED'}else{throw 'EXE changed'}
$j | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $PackageRecord -Encoding utf8
$manifest=Join-Path $j.candidate_directory 'package_manifest.json'
if(-not(Test-Path -LiteralPath $manifest)){throw 'Candidate manifest missing'}
Copy-Item -LiteralPath $PackageRecord -Destination $manifest
Write-Output 'Complete license texts copied and archive inventory updated; EXE unchanged.'
