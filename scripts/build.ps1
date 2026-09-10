param([string]$QtRoot = '', [switch]$NoAja)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $QtRoot) { $QtRoot = Join-Path $projectRoot '.deps/Qt/6.8.3/msvc2022_64' }
$cmake = Join-Path $projectRoot '.tools/python-packages/cmake/data/bin/cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
if (-not $vs) { throw 'Install the Visual Studio Desktop development with C++ workload.' }
$major = [int]($vs.installationVersion.Split('.')[0])
$generator = if ($major -ge 18) { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
Push-Location $projectRoot
try {
    $ajaOption = if ($NoAja) { 'OFF' } else { 'ON' }
    & $cmake -S . -B build -G $generator -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot" "-DSW_WITH_AJA=$ajaOption"
    if ($LASTEXITCODE) { throw 'Configuration failed.' }
    & $cmake --build build --config Release --parallel 8
    if ($LASTEXITCODE) { throw 'Build failed.' }
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed.' }
    & $cmake --install build --config Release --prefix dist/SW
    if ($LASTEXITCODE) { throw 'Packaging failed.' }
    $env:VCINSTALLDIR = Join-Path $vs.installationPath 'VC/'
    $env:VSINSTALLDIR = $vs.installationPath
    & (Join-Path $QtRoot bin/windeployqt.exe) --release --no-translations --no-compiler-runtime dist/SW/sw_switcher.exe
    if ($LASTEXITCODE) { throw 'Qt deployment failed.' }
    New-Item -ItemType Directory -Path dist/SW/licenses -Force | Out-Null
    Copy-Item -LiteralPath THIRD_PARTY_NOTICES.md -Destination dist/SW/licenses/
    if (-not $NoAja) { Copy-Item -LiteralPath vendor-sdk/aja/LICENSE -Destination dist/SW/licenses/AJA-MIT.txt }
    $qtLicenses = Join-Path $QtRoot 'LICENSES'
    if (Test-Path $qtLicenses) { Copy-Item -LiteralPath $qtLicenses -Destination dist/SW/licenses/Qt -Recurse -Force }
    elseif (Test-Path .deps/Qt/licenses) { Copy-Item -LiteralPath .deps/Qt/licenses -Destination dist/SW/licenses/Qt -Recurse -Force }
    else { throw 'Qt license files missing. Run scripts/bootstrap.ps1 or provide QtRoot/LICENSES.' }
    Copy-Item -LiteralPath docs/03-implementation.ko.md -Destination dist/SW/사용안내.md
    Copy-Item -LiteralPath scripts/UHD-loopback.cmd -Destination dist/SW/
    Copy-Item -LiteralPath scripts/DeckLink-receive.cmd -Destination dist/SW/
    Write-Host 'Ready: dist/SW/sw_switcher.exe (requires installed Visual C++ x64 runtime and card drivers).'
} finally { Pop-Location }
