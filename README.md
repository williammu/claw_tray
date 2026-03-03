# OpenClaw Launcher

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows%2011-blue.svg)](https://www.microsoft.com/windows)
[![C++](https://img.shields.io/badge/C++-Win32%20API-orange.svg)](https://en.wikipedia.org/wiki/Windows_API)

一个为 OpenClaw Gateway 设计的 Windows 11 桌面启动器，提供现代化的 GUI 界面、系统托盘集成、进程管理和实时日志监控。

---

## 项目简介

OpenClaw Launcher 是 OpenClaw Gateway 的 Windows 桌面启动容器，主要功能包括：

- **无窗口执行** - 彻底消除 CMD 黑窗口，所有输出重定向到 GUI 终端
- **实时彩色日志** - 完整保留 ANSI 颜色格式，支持智能滚动
- **系统托盘集成** - 三态图标显示运行状态（灰/黄/绿）
- **全局热键** - 自定义快速切换窗口显示/隐藏
- **进程监控** - PID 状态监控，异常退出自动重启
- **开机自启动** - Windows 任务计划程序实现，支持冲突检测

---

## 版本说明

本项目提供两个实现版本：

| 特性 | C++ 版本 | Python 版本 |
|------|----------|-------------|
| **状态** | **推荐使用** | 原型验证 |
| **功能完整度** | 完整 | 基本完整 |
| **包体大小** | ~500 KB | ~50 MB |
| **启动速度** | 快 | 较慢 |
| **内存占用** | 低 | 较高 |
| **最小化动画** | 支持 | 不支持 |
| **依赖** | 无（单文件） | Python 运行时 |

### 推荐选择

- **日常使用**：选择 [C++ 版本](./cpp_launcher/)
- **二次开发**：两者皆可选择
  - C++ 版本：高性能、小体积，适合生产环境
  - Python 版本：快速原型验证，适合研究和学习

> **注意**：Python 版本仅作为原型验证和研究用途，功能不如 C++ 版本完善。

---

## 快速开始

### 下载安装

1. 前往 [Releases](../../releases) 页面下载最新版本
2. 解压到任意目录
3. 双击 `OpenClawLauncher.exe` 运行

### 首次运行

启动器会自动：
1. 查找 OpenClaw 可执行文件（支持 `.exe` 和 `.cmd`）
2. 查找配置文件（`config.yaml` 或 `openclaw.json`）
3. 读取网关端口配置
4. 保存配置到 `%APPDATA%\.openclaw\launcher.ini`

### 使用说明

- **启动/停止**：点击窗口按钮或托盘菜单
- **显示/隐藏窗口**：按 `Ctrl+Shift+O` 或双击托盘图标
- **退出程序**：仅通过托盘右键菜单退出
- **配置热键**：点击"设热键"按钮自定义快捷键

---

## 项目结构

```
claw_tray/
├── cpp_launcher/           # C++ 实现（推荐）
│   ├── src/               # 源代码
│   ├── res/               # 资源文件
│   ├── installer/         # 安装程序脚本
│   ├── CMakeLists.txt     # CMake 配置
│   └── SPEC.md            # c++详细设计文档
│
├── python_proto/          # Python 原型（研究用途）
│   ├── launcher.py        # 主程序
│   ├── config.json        # 配置示例
│   └── SPEC.md            # Python专属设计文档
│
├── general_spec.md        # 通用功能规格说明
└── LICENSE                # MIT 许可证
```

---

## 构建指南

### C++ 版本

**前置要求**：
- Visual Studio 2022
- CMake 3.15+

**构建步骤**：
```batch
cd cpp_launcher
build_msvc.bat
```

输出文件位于 `cpp_launcher/dist/OpenClawLauncher.exe`

### Python 版本

**前置要求**：
- Python 3.10+
- pip

**安装依赖**：
```batch
cd python_proto
pip install -r requirements.txt
python launcher.py
```

**打包为 EXE**：
```batch
python build.py
```

---

## 功能特性

### 核心功能

- [x] 无窗口执行 OpenClaw 及其子进程
- [x] GUI 终端实时显示彩色输出（ANSI 解析）
- [x] 系统托盘三态图标（停止/启动中/运行中）
- [x] 全局热键切换窗口显示/隐藏
- [x] PID 状态监控与自动重启
- [x] 两步启动确认（输出监控 + 健康检查）
- [x] 开机自启动（任务计划程序）
- [x] 日志持久化（启动时截断保留 1 万行）
- [x] 热键运行时配置
- [x] 开机自启动冲突检测

### C++ 版本独有

- [x] 最小化到托盘动画
- [x] 自绘按钮和控件
- [x] 扁平滚动条
- [x] 更小的包体和更低的内存占用
- [x] 完整实现的复古终端样式  

---

## 配置说明（仅以c++项目为例）

配置文件位于 `%APPDATA%\.openclaw\launcher.ini`：

```ini
[launcher]
openclaw_path = C:\Users\xxx\.openclaw\openclaw.exe
openclaw_config = C:\Users\xxx\.openclaw\config.yaml
gateway_port = 18789
window_hotkey = ctrl+shift+o
windows_startup = false
max_start_attempts = 2
```

**端口优先级**：
1. OpenClaw 配置文件中的端口
2. 启动器配置文件中的 `gateway_port`
3. 默认值 18789

---

## 开源许可证

本项目采用 **MIT License** 开源许可证。

### 许可证摘要

MIT License 是最宽松的开源许可证之一，您可以：

- **商业使用** - 可用于商业项目
- **修改** - 可自由修改源代码
- **分发** - 可分发原始或修改后的代码
- **私有使用** - 可私有使用
- **再许可** - 可更改许可证

唯一要求是在代码副本中保留版权声明和许可证声明。

### 为什么选择 MIT？

我们希望：
- 尽可能多的人使用我们的代码
- 鼓励拷贝、分发和二次开发
- 降低使用门槛，促进开源社区发展

---

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

---

## 文档

- [通用功能规格说明](./general_spec.md) - 语言无关的功能设定
- [C++ 实现文档](./cpp_launcher/SPEC.md) - C++ 版本详细设计
- [Python 原型文档](./python_proto/SPEC.md) - Python 版本设计说明
- [踩坑记录](./cpp_launcher/Pitfall%20Avoidance%20%26%20Best%20Practices.md) - 开发过程中的问题和解决方案

---

## 致谢

- OpenClaw Gateway 项目
- 所有贡献者

---

## 联系方式

如有问题或建议，请提交 [Issue](../../issues)。
