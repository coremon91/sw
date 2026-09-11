@echo off
cd /d "%~dp0"
echo Connect AJA SDI OUT 1 to a DeckLink SDI input with a 12G-SDI cable.
echo This test uses the first AJA card and the first four DeckLink endpoints.
echo Close other video applications before starting.
pause
sw_loopback.exe --uhd --seconds 12
echo.
echo Test finished. All test streams have been stopped.
pause
