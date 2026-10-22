@echo off
setlocal
set SW_GPU_TRANSFER=cuda
start "SW 0.4 CUDA" /D "%~dp0" "%~dp0sw_switcher.exe"
