param([Parameter(Mandatory=$true)][string]$PlayerPackage,[string]$Destination='dist/SW')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$source=(Resolve-Path -LiteralPath $PlayerPackage).Path
$target=Join-Path $projectRoot (Join-Path $Destination 'player')
$exe=Join-Path $source 'BlackmagicDeckLinkUhdMxfPlayer.exe'
if((Get-FileHash -LiteralPath $exe).Hash -ne '1A19DECC5C580515E847BAFF6C7057F598EB350D324FFF944F0434658E0D7D03'){throw 'Unexpected player version. Use the 2026-09-07 RTX 5060 portable package.'}
New-Item -ItemType Directory -Path $target -Force | Out-Null
Get-ChildItem -LiteralPath $source -File | Where-Object {$_.Extension -in '.exe','.dll','.txt','.md','.cmd'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $target -Force}
Copy-Item -LiteralPath (Join-Path $source 'preview_ffmpeg') -Destination $target -Recurse -Force
Write-Host 'Exact player runtime copied to the local package. User logs and source folders are not copied.'
