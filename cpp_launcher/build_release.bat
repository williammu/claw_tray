@echo off
setlocal EnableDelayedExpansion

if defined VERSION (
    set APP_VERSION=%VERSION%
) else (
    set APP_VERSION=1.0.0
)
set PROJECT_DIR=%~dp0
set DIST_DIR=%PROJECT_DIR%dist
set RELEASE_DIR=%PROJECT_DIR%release
set PORTABLE_DIR=%RELEASE_DIR%\OpenClawLauncher-Portable-%APP_VERSION%

echo ========================================
echo OpenClaw Launcher Build Script
echo Version: %APP_VERSION%
echo ========================================

if not exist "%DIST_DIR%\OpenClawLauncher.exe" (
    echo ERROR: OpenClawLauncher.exe not found in dist folder
    echo Please build the project first using build_msvc.bat
    exit /b 1
)

if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"

echo.
echo [1/3] Creating portable package...
if exist "%PORTABLE_DIR%" rmdir /s /q "%PORTABLE_DIR%"
mkdir "%PORTABLE_DIR%"

copy "%DIST_DIR%\OpenClawLauncher.exe" "%PORTABLE_DIR%\" >nul
if exist "%PROJECT_DIR%README.md" copy "%PROJECT_DIR%README.md" "%PORTABLE_DIR%\" >nul
if exist "%PROJECT_DIR%..\LICENSE" copy "%PROJECT_DIR%..\LICENSE" "%PORTABLE_DIR%\" >nul

cd /d "%RELEASE_DIR%"
powershell -Command "Compress-Archive -Path 'OpenClawLauncher-Portable-%APP_VERSION%' -DestinationPath 'OpenClawLauncher-Portable-%APP_VERSION%.zip' -Force"
if exist "%PORTABLE_DIR%" rmdir /s /q "%PORTABLE_DIR%"
echo Portable ZIP created: OpenClawLauncher-Portable-%APP_VERSION%.zip

echo.
echo [2/3] Creating installer...
set INNO_PATH=
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set INNO_PATH=C:\Program Files (x86)\Inno Setup 6\ISCC.exe
) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set INNO_PATH=C:\Program Files\Inno Setup 6\ISCC.exe
)

if defined INNO_PATH (
    "%INNO_PATH%" "/DMyAppVersion=%APP_VERSION%" "%PROJECT_DIR%installer\setup.iss"
    echo Installer created successfully
) else (
    echo WARNING: Inno Setup 6 not found
    echo Please install Inno Setup 6 from https://jrsoftware.org/isinfo.php
    echo Then run: "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\setup.iss
)

echo.
echo [3/3] Build complete!
echo.
echo Release files:
dir /b "%RELEASE_DIR%\*.exe" 2>nul
dir /b "%RELEASE_DIR%\*.zip" 2>nul
echo.
echo Location: %RELEASE_DIR%
