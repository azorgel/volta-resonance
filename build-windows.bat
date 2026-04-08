@echo off
setlocal enabledelayedexpansion

echo.
echo   VOLTA Resonance -- Windows Build
echo.

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"

:: ── Configure ──────────────────────────────────────────────────────────────
echo [1/2] Configuring...
cmake -B "%BUILD_DIR%" -A x64 -S "%SCRIPT_DIR%"

if %errorlevel% neq 0 (
    echo.
    echo   ERROR: CMake configure failed.
    echo.
    echo   Requirements:
    echo     - Visual Studio 2019 or 2022 with "Desktop development with C++" workload
    echo       https://visualstudio.microsoft.com/downloads/
    echo     - CMake 3.22+  https://cmake.org/download/
    echo     - Git           https://git-scm.com/download/win
    echo.
    pause
    exit /b 1
)

:: ── Build ──────────────────────────────────────────────────────────────────
echo.
echo [2/2] Building Release...
cmake --build "%BUILD_DIR%" --config Release --parallel

if %errorlevel% neq 0 (
    echo.
    echo   ERROR: Build failed. Check output above for details.
    pause
    exit /b 1
)

echo.
echo   Build complete!
echo.
echo   VST3: %BUILD_DIR%\VoltaResonance_artefacts\Release\VST3\VOLTA Resonance.vst3
echo.
echo   Run package-windows.bat to create the installer .exe
echo.
pause
