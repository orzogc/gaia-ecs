@echo off
setlocal

rem make_single_header.bat
rem Forwards to make_single_header.ps1.
rem Usage:
rem   make_single_header.bat [--format|--no-format] [clang-format-executable]
rem   default: format when clang-format is available

set "SCRIPT_DIR=%~dp0"
set "SCRIPT=%SCRIPT_DIR%make_single_header.ps1"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
exit /b %ERRORLEVEL%
