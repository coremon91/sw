param([string]$Python = 'python', [switch]$SkipQt)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & $Python -m pip install --target .tools/python-packages aqtinstall==3.3.0 cmake==4.4.3 ninja==1.13.2
    if ($LASTEXITCODE) { throw 'Build dependency installation failed.' }
    $env:PYTHONPATH = (Resolve-Path .tools/python-packages).Path
    if (-not $SkipQt -and -not (Test-Path .deps/Qt/6.8.3/msvc2022_64/bin/qmake.exe)) {
        & $Python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --archives qtbase qttools -O .deps/Qt
        if ($LASTEXITCODE) { throw 'Qt download failed.' }
    }
    if (-not (Test-Path vendor-sdk/aja/.git)) {
        New-Item -ItemType Directory -Path vendor-sdk -Force | Out-Null
        git clone --depth 1 --branch ntv2_18_0_0 https://github.com/aja-video/libajantv2.git vendor-sdk/aja
        if ($LASTEXITCODE) { throw 'AJA SDK download failed.' }
    }
    $revision = git -C vendor-sdk/aja rev-parse HEAD
    if ($revision -ne '4add45239a03960aa36de6fef326ee1b753e8ec9') { throw 'Unexpected AJA revision; the existing SDK was left untouched.' }
    & $Python scripts/patch_aja.py vendor-sdk/aja
    if ($LASTEXITCODE) { throw 'AJA compiler compatibility patch failed.' }
    New-Item -ItemType Directory -Path .deps/Qt/licenses -Force | Out-Null
    foreach ($licenseName in @('LGPL-3.0-only.txt','GPL-3.0-only.txt','Qt-GPL-exception-1.0.txt')) {
        Invoke-WebRequest -Uri ('https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES/' + $licenseName) -OutFile (Join-Path .deps/Qt/licenses $licenseName)
    }
    Write-Host 'Dependencies ready. Build with scripts/build.ps1.'
} finally { Pop-Location }
