@echo off
setlocal enabledelayedexpansion

set OUT=OpenClawLauncher.exe
set SRC=main.cpp
set CXXFLAGS=/EHsc /W3 /O2 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS
set LDFLAGS=/SUBSYSTEM:WINDOWS
set LIBS=user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib comctl32.lib winhttp.lib advapi32.lib

echo ========================================
echo OpenClaw Launcher Build Script
echo ========================================
echo.

where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [MSVC] Found cl.exe, using MSVC compiler...
    echo.
    
    cl %CXXFLAGS% %SRC% /Fe%OUT% /link %LDFLAGS% %LIBS%
    
    if %ERRORLEVEL% equ 0 (
        echo.
        echo [SUCCESS] Build completed: %OUT%
        dir %OUT% 2>nul
    ) else (
        echo.
        echo [ERROR] Build failed
        exit /b 1
    )
) else (
    echo [INFO] MSVC cl.exe not found in PATH
    echo.
    echo To build this project, you need one of the following:
    echo.
    echo 1. Microsoft Visual Studio (MSVC):
    echo    - Run "Developer Command Prompt for VS" from Start Menu
    echo    - Navigate to this directory
    echo    - Run build.bat
    echo.
    echo 2. CMake (recommended):
    echo    - mkdir build
    echo    - cd build
    echo    - cmake ..
    echo    - cmake --build . --config Release
    echo.
    echo 3. MinGW-w64 (g++):
    echo    - g++ -std=c++17 -O2 -mwindows %SRC% -o %OUT% -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -lcomctl32 -lwinhttp -ladvapi32
    echo.
    exit /b 1
)
