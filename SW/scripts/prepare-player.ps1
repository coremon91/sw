param([Parameter(Mandatory=$true)][string]$PlayerRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$headers = Join-Path $PlayerRoot 'work/FFmpeg/include'
$runtime = Join-Path $PlayerRoot 'work/FFmpegCuda/bin'
if (-not (Test-Path (Join-Path $headers 'libavcodec/avcodec.h'))) { throw 'Player FFmpeg development headers missing.' }
if (-not (Test-Path (Join-Path $runtime 'avcodec.lib'))) { throw 'Player FFmpeg MSVC import libraries missing.' }
$destination = Join-Path $projectRoot '.deps/ffmpeg'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $headers -Destination $destination -Recurse -Force
Copy-Item -LiteralPath $runtime -Destination $destination -Recurse -Force
& (Join-Path $runtime 'ffmpeg.exe') -version | Set-Content -LiteralPath (Join-Path $destination 'BUILD-INFO.txt') -Encoding utf8
Write-Host 'Existing player FFmpeg prepared for local builds. Runtime binaries are excluded from Git.'
