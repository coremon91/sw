@echo off
cd /d "%~dp0"
if exist "%~dp0sw_switcher-studio-uhd5994.exe" (
  start "" "%~dp0sw_switcher-studio-uhd5994.exe"
) else (
  start "" "%~dp0sw_switcher.exe"
)
