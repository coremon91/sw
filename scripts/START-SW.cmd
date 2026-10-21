@echo off
cd /d "%~dp0"
if exist "%~dp0sw_switcher-studio.exe" (
  start "" "%~dp0sw_switcher-studio.exe"
) else (
  start "" "%~dp0sw_switcher.exe"
)
