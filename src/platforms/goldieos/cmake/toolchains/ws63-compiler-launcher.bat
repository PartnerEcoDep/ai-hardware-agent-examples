@echo off
setlocal enabledelayedexpansion

REM WS63 compiler launcher - ensures toolchain bin directory is in PATH
REM so that cc1.exe can find libssp-0.dll and other runtime DLLs.

REM Extract compiler directory from first argument
set "COMPILER=%~1"
for %%I in ("%COMPILER%") do set "COMPILER_DIR=%%~dpI"

REM Add compiler directory to PATH if not already present
echo %PATH% | findstr /I /C:"%COMPILER_DIR%" >nul 2>&1
if errorlevel 1 (
    set "PATH=%COMPILER_DIR%;%PATH%"
)

REM Forward all arguments to the actual compiler
%*

