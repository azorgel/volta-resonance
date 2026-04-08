@echo off
setlocal enabledelayedexpansion

echo.
echo   VOLTA Resonance -- Windows Package Builder
echo.

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "DIST_DIR=%SCRIPT_DIR%dist"
set "VST3_SRC=%BUILD_DIR%\VoltaResonance_artefacts\Release\VST3\VOLTA Resonance.vst3"

:: ── Check build artifacts ─────────────────────────────────────────────────
if not exist "%VST3_SRC%" (
    echo   ERROR: Build artifacts not found. Run build-windows.bat first.
    pause
    exit /b 1
)

:: ── Find NSIS ─────────────────────────────────────────────────────────────
set "NSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
if not exist "%NSIS%" (
    echo   ERROR: NSIS not found.
    echo.
    echo   Download NSIS 3.x from: https://nsis.sourceforge.io/Download
    echo   Install it, then run this script again.
    pause
    exit /b 1
)

:: ── Build installer ───────────────────────────────────────────────────────
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo   Building installer...
"%NSIS%" /DBUILD_DIR="%BUILD_DIR%" /DDIST_DIR="%DIST_DIR%" "%SCRIPT_DIR%installer\volta-resonance.nsi"

if %errorlevel% neq 0 (
    echo.
    echo   ERROR: Installer creation failed.
    pause
    exit /b 1
)

echo.
echo   Done!  Send this file to your friend:
echo   %DIST_DIR%\VOLTA Resonance Installer.exe
echo.
echo   Their install steps:
echo   1. Double-click 'VOLTA Resonance Installer.exe'
echo   2. Click through the wizard (needs admin/UAC prompt)
echo   3. Open Ableton Live
echo   4. Preferences ^> Plug-Ins ^> Rescan Plug-Ins
echo   5. Add a MIDI track ^> Instruments ^> VST3 ^> Volta Labs ^> VOLTA Resonance
echo.
pause
