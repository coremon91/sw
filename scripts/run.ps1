param([switch]$Demo)
$projectRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $projectRoot 'dist/SW/sw_switcher.exe'
if (-not (Test-Path $exe)) { throw 'Run scripts/build.ps1 first.' }
if ($Demo) { & $exe --demo } else { & $exe }
