@echo off
setlocal
set SW_GPU_TRANSFER=d3d11
start "SW 0.6 D3D11" /D "%~dp0" "%~dp0sw_switcher.exe"
