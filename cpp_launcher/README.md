# OpenClaw Launcher (C++ 原生版本)

一个轻量级的 OpenClaw Gateway Windows 11 原生启动器。

## 功能特性

- **原生 Windows 应用** - 使用纯 Win32 API 构建，无外部依赖
- **体积小巧** - 可执行文件仅约 500 KB
- **系统托盘集成** - 最小化到托盘，彩色图标显示运行状态
- **全局热键** - 按 `Ctrl+Shift+O` 切换窗口显示/隐藏
- **进程管理** - 启动/停止/重启 OpenClaw Gateway
- **健康监控** - 自动进程监控，异常退出自动重启
- **自动发现** - 自动查找 OpenClaw 可执行文件和配置文件
- **彩色日志** - 完整支持 ANSI 颜色解析，实时显示彩色输出
- **智能滚动** - 用户查看历史日志时不自动滚动

## 构建要求

- Windows 11
- 以下编译器之一：
  - Microsoft Visual Studio 2019/2022 (MSVC)
  - MinGW-w64 (GCC)
  - LLVM/Clang

## 构建说明

### 方式一：使用 MSVC（推荐）

1. 从开始菜单打开 "Developer Command Prompt for Visual Studio"
2. 进入本目录
3. 运行：
   ```batch
   build_msvc.bat
   ```

### 方式二：使用 CMake

```batch
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 方式三：使用 MinGW-w64

```batch
g++ -std=c++17 -O2 -mwindows main.cpp -o OpenClawLauncher.exe -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -lcomctl32 -lwinhttp -ladvapi32 -ldwmapi -lcomdlg32
```

## 项目结构

```
cpp_launcher/
├── main.cpp              # 程序入口
├── CMakeLists.txt        # CMake 配置
├── build_msvc.bat        # MSVC 构建脚本
├── build_release.bat     # 发布版构建脚本
├── README.md             # 本文件
├── SPEC.md               # 详细设计文档
├── installer/            # 安装程序
│   └── setup.iss         # Inno Setup 脚本
├── res/                  # 资源文件
│   ├── icon.ico          # 应用图标
│   ├── resource.h        # 资源头文件
│   └── resource.rc       # 资源脚本
└── src/                  # 源代码
    ├── Common.h          # 公共定义和工具函数
    ├── Config.h          # 配置管理
    ├── Logger.h          # 日志系统
    ├── AnsiParser.h      # ANSI 转义序列解析器
    ├── ProcessManager.h  # 进程管理
    ├── TrayIcon.h        # 系统托盘图标
    ├── HotkeyManager.h   # 全局热键管理
    ├── StartupManager.h  # 开机自启动管理
    └── MainWindow.h      # 主窗口 GUI
```

## 配置说明

配置文件存储在 `%APPDATA%\.openclaw\launcher.ini`：

```ini
[launcher]
openclaw_path = C:\Users\xxx\.openclaw\openclaw.exe
openclaw_config = C:\Users\xxx\.openclaw\config.yaml
gateway_port = 18789
window_hotkey = ctrl+shift+o
windows_startup = false
max_start_attempts = 2
```

### 端口优先级

1. OpenClaw 配置文件中的端口（最高优先级）
2. 启动器配置文件中的 `gateway_port`
3. 默认值 18789

## 使用方法

1. 运行 `OpenClawLauncher.exe`
2. 启动器会自动查找 OpenClaw 并启动
3. 使用系统托盘图标控制启动器
4. 按 `Ctrl+Shift+O` 显示/隐藏窗口（需要管理员权限）
5. 点击"设热键"按钮可自定义热键

## 状态说明

| 托盘图标颜色 | 状态 | 说明 |
|-------------|------|------|
| 灰色 | 已停止 | OpenClaw 未运行 |
| 黄色 | 启动中 | 正在启动 OpenClaw |
| 绿色 | 运行中 | OpenClaw 正常运行 |

## 开机自启动

启动器支持通过 Windows 任务计划程序实现开机自启动：

1. 勾选窗口中的"开机自启"复选框
2. 会自动创建计划任务（延迟 30 秒启动）
3. 如有冲突任务，会弹窗提示

## 许可证

MIT License - 可自由使用、修改和分发
