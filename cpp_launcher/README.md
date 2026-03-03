# OpenClaw Launcher (C++ Native Version)

A lightweight Windows 11 native launcher for OpenClaw Gateway.

## Features

- **Native Windows Application** - Built with pure Win32 API, no external dependencies
- **Small Size** - Approximately 1-3 MB executable
- **System Tray Integration** - Minimizes to tray, shows status with colored icons
- **Global Hotkey** - Press `Ctrl+Shift+O` to toggle window visibility
- **Process Management** - Start/Stop/Restart OpenClaw Gateway
- **Health Monitoring** - Automatic process monitoring with auto-restart
- **Auto-Discovery** - Automatically finds OpenClaw executable and config

## Build Requirements

- Windows 11
- One of the following compilers:
  - Microsoft Visual Studio 2019/2022 (MSVC)
  - MinGW-w64 (GCC)
  - LLVM/Clang

## Build Instructions

### Option 1: Using MSVC (Recommended)

1. Open "Developer Command Prompt for Visual Studio" from Start Menu
2. Navigate to this directory
3. Run:
   ```batch
   build.bat
   ```

### Option 2: Using CMake

```batch
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Option 3: Using MinGW-w64

```batch
g++ -std=c++17 -O2 -mwindows main.cpp -o OpenClawLauncher.exe -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -lcomctl32 -lwinhttp -ladvapi32
```

## Project Structure

```
cpp_launcher/
├── main.cpp           # Entry point
├── CMakeLists.txt     # CMake configuration
├── build.bat          # MSVC build script
├── README.md          # This file
└── src/
    ├── Common.h       # Common definitions and utilities
    ├── Config.h       # Configuration management
    ├── Logger.h       # Logging system
    ├── AnsiParser.h   # ANSI escape sequence parser
    ├── ProcessManager.h # Process management
    ├── TrayIcon.h     # System tray icon
    ├── HotkeyManager.h # Global hotkey registration
    └── MainWindow.h   # Main GUI window
```

## Configuration

Configuration is stored in `%APPDATA%\OpenClawLauncher\config.json`:

```json
{
    "openclaw_path": "",
    "openclaw_config_path": "",
    "gateway_port": 18789,
    "window_hotkey": "ctrl+shift+o",
    "max_start_attempts": 2,
    "retry_delay": 3
}
```

## Usage

1. Run `OpenClawLauncher.exe`
2. The launcher will automatically find OpenClaw and start it
3. Use the system tray icon to control the launcher
4. Press `Ctrl+Shift+O` to show/hide the window (requires admin rights)

## License

MIT License
