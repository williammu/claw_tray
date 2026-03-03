@echo off
setlocal enabledelayedexpansion

set OUT=dist\OpenClawLauncher.exe
set SRC=main.cpp
set RC=res\resource.rc
set RES=res\resource.res
set CXXFLAGS=/EHsc /W3 /O2 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /source-charset:utf-8 /execution-charset:gbk
set LDFLAGS=/SUBSYSTEM:WINDOWS
set LIBS=user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib comctl32.lib winhttp.lib advapi32.lib

echo ========================================
echo OpenClaw Launcher Build Script
echo ========================================
echo.

REM Kill existing process
echo [INFO] Stopping existing process...
taskkill /F /IM OpenClawLauncher.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM Create dist directory
if not exist "dist" mkdir dist

REM Find and setup MSVC environment
set "VCVARS="
for %%p in (
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do (
    if exist %%p set "VCVARS=%%~p"
)

if "%VCVARS%"=="" (
    echo [ERROR] MSVC not found. Please install Visual Studio Build Tools.
    echo Download from: https://visualstudio.microsoft.com/visual-cpp-build-tools/
    exit /b 1
)

echo [INFO] Found MSVC at: %VCVARS%
echo.

call "%VCVARS%"

echo [INFO] Compiling resources...
echo.

rc /fo %RES% %RC%
if %ERRORLEVEL% neq 0 (
    echo [WARN] Resource compilation failed, continuing without resources
)

echo [INFO] Compiling %SRC%...
echo.

cl %CXXFLAGS% %SRC% /Fe%OUT% /link %LDFLAGS% %LIBS% %RES%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ========================================
    echo [ERROR] Build failed with error code: %ERRORLEVEL%
    echo ========================================
    exit /b %ERRORLEVEL%
)

if not exist "%OUT%" (
    echo.
    echo ========================================
    echo [ERROR] Output file not created: %OUT%
    echo ========================================
    exit /b 1
)

echo.
echo ========================================
echo [SUCCESS] Build completed successfully
echo ========================================
echo Output: %OUT%
for %%f in (%OUT%) do echo Size: %%~zf bytes
echo.

REM Clean up intermediate files
if exist main.obj del main.obj
if exist %RES% del %RES%

echo [INFO] Launching application...
start "" "%OUT%"

exit /b 0
