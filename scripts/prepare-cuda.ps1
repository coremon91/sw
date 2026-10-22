param([string]$Python = 'python')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & $Python -m pip download nvidia-cuda-runtime-cu12==12.9.79 nvidia-cuda-nvcc-cu12==12.9.86 --no-deps --only-binary=:all: --platform win_amd64 --python-version 312 -d .deps/cuda-download
    if($LASTEXITCODE){throw 'CUDA SDK download failed'}
    $extract=@'
from pathlib import Path
from zipfile import ZipFile
import shutil
root=Path('.deps')
for name in ('nvidia_cuda_runtime_cu12-12.9.79-py3-none-win_amd64.whl','nvidia_cuda_nvcc_cu12-12.9.86-py3-none-win_amd64.whl'):
    with ZipFile(root/'cuda-download'/name) as archive:
        archive.extractall(root/'cuda')
shutil.copytree(root/'cuda/nvidia/cuda_nvcc/include/crt',root/'cuda/nvidia/cuda_runtime/include/crt',dirs_exist_ok=True)
'@
    & $Python -c $extract
    if($LASTEXITCODE){throw 'CUDA SDK preparation failed'}
} finally {Pop-Location}
