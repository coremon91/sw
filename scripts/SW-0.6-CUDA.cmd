@echo off
setlocal
set SW_GPU_TRANSFER=cuda
start "SW 0.6 CUDA" /D "%~dp0" "%~dp0sw_switcher.exe"
