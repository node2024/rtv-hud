@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-Windows.ps1"
set "rtv_build_exit=%ERRORLEVEL%"
echo.
pause
exit /b %rtv_build_exit%
