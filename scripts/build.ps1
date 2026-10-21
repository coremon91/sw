param([string]$QtRoot = '', [switch]$NoAja, [string]$FFmpegRoot = '', [string]$Destination = 'dist/SW')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $QtRoot) { $QtRoot = Join-Path $projectRoot '.deps/Qt/6.8.3/msvc2022_64' }
if (-not $FFmpegRoot) { $FFmpegRoot = Join-Path $projectRoot '.deps/ffmpeg' }
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
    & $cmake -S . -B build -G $generator -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot" "-DSW_WITH_AJA=$ajaOption" "-DSW_FFMPEG_ROOT=$FFmpegRoot"
    if ($LASTEXITCODE) { throw 'Configuration failed.' }
    & $cmake --build build --config Release --parallel 8
    if ($LASTEXITCODE) { throw 'Build failed.' }
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed.' }
    & $cmake --install build --config Release --prefix $Destination
    if ($LASTEXITCODE) { throw 'Packaging failed.' }
    $env:VCINSTALLDIR = Join-Path $vs.installationPath 'VC/'
    $env:VSINSTALLDIR = $vs.installationPath
    & (Join-Path $QtRoot bin/windeployqt.exe) --release --no-compiler-runtime (Join-Path $Destination 'sw_switcher.exe')
    if ($LASTEXITCODE) { throw 'Qt deployment failed.' }
    $licenses = Join-Path $Destination 'licenses'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    Copy-Item -LiteralPath THIRD_PARTY_NOTICES.md -Destination $licenses
    if (-not $NoAja) { Copy-Item -LiteralPath vendor-sdk/aja/LICENSE -Destination (Join-Path $licenses 'AJA-MIT.txt') }
    $qtLicenses = Join-Path $QtRoot 'LICENSES'
    if (Test-Path $qtLicenses) { Copy-Item -LiteralPath $qtLicenses -Destination (Join-Path $licenses 'Qt') -Recurse -Force }
    elseif (Test-Path .deps/Qt/licenses) { Copy-Item -LiteralPath .deps/Qt/licenses -Destination (Join-Path $licenses 'Qt') -Recurse -Force }
    else { throw 'Qt license files missing. Run scripts/bootstrap.ps1 or provide QtRoot/LICENSES.' }
    Copy-Item -LiteralPath docs/03-implementation.ko.md -Destination (Join-Path $Destination '사용안내.md')
    foreach ($launcher in @('START-SW.cmd','PREVIEW-NO-SDI.cmd','UHD-loopback.cmd','DeckLink-receive.cmd','KONA-receive.cmd')) { Copy-Item -LiteralPath (Join-Path 'scripts' $launcher) -Destination $Destination }
    $guides = @{'06-receiver-jitter.ko.md'='수신시험안내.md';'07-audio-validation.ko.md'='오디오시험안내.md';'08-internal-player.ko.md'='내장플레이어안내.md';'09-original-player-tab.ko.md'='플레이어탭안내.md';'10-integrated-studio.ko.md'='통합스위처안내.md'}
    foreach ($guide in $guides.GetEnumerator()) { Copy-Item -LiteralPath (Join-Path 'docs' $guide.Key) -Destination (Join-Path $Destination $guide.Value) }
    Copy-Item -LiteralPath docs/11-internal-cg.ko.md -Destination (Join-Path $Destination '내부CG사용안내.md')
    Copy-Item -LiteralPath docs/12-broadcast-layout.ko.md -Destination (Join-Path $Destination '방송CG배치안내.md')
    Copy-Item -LiteralPath docs/13-layer-studio.ko.md -Destination (Join-Path $Destination '통합CG스튜디오안내.md')
    Copy-Item -LiteralPath docs/14-logo-library-batch.ko.md -Destination (Join-Path $Destination '로고라이브러리-일괄ON안내.md')
    Copy-Item -LiteralPath docs/15-cg-controls.ko.md -Destination (Join-Path $Destination 'CG운용기능확장안내.md')
    Copy-Item -LiteralPath (Join-Path $Destination 'sw_switcher.exe') -Destination (Join-Path $Destination 'sw_switcher-studio.exe') -Force
    Copy-Item -LiteralPath scripts/START-CG-STUDIO.cmd -Destination $Destination
    New-Item -ItemType Directory -Path (Join-Path $Destination 'scripts'),(Join-Path $Destination 'integrations/nodecg') -Force | Out-Null
    Copy-Item -LiteralPath scripts/install-internal-cg.py -Destination (Join-Path $Destination 'scripts')
    Copy-Item -LiteralPath scripts/install-broadcast-layout.py -Destination (Join-Path $Destination 'scripts')
    Copy-Item -LiteralPath scripts/import-logo-library.py -Destination (Join-Path $Destination 'scripts')
    Copy-Item -LiteralPath scripts/install-weather-api.py -Destination (Join-Path $Destination 'scripts')
    $broadcastDestination = Join-Path $Destination 'integrations/nodecg/broadcast'
    New-Item -ItemType Directory -Path $broadcastDestination -Force | Out-Null
    Copy-Item -Path 'integrations/nodecg/broadcast/*' -Destination $broadcastDestination -Recurse -Force
    Get-ChildItem -LiteralPath integrations/nodecg -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $Destination 'integrations/nodecg') }
    if(Test-Path (Join-Path $FFmpegRoot 'BUILD-INFO.txt')) { Copy-Item -LiteralPath (Join-Path $FFmpegRoot 'BUILD-INFO.txt') -Destination (Join-Path $licenses 'FFmpeg-BUILD-INFO.txt') }
    Write-Host "Ready: $Destination/sw_switcher.exe (requires installed Visual C++ x64 runtime and card drivers)."
} finally { Pop-Location }
