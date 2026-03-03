# OpenClaw Launcher 详细设计规格说明

## 1. 项目概述

### 1.1 项目名称
OpenClaw Launcher - OpenClaw Gateway 的 Windows 桌面启动器

### 1.2 技术栈
- **语言**: C++17
- **UI框架**: Win32 API (纯原生，无 MFC/ATL/WTL)
- **构建系统**: CMake + MSVC (Visual Studio 2022)
- **目标平台**: Windows 10/11 x64

### 1.3 核心功能
1. 启动/停止/重启 OpenClaw Gateway 进程
2. 实时显示进程输出日志（支持 ANSI 颜色）
3. 系统托盘图标和菜单
4. 全局热键显示/隐藏窗口
5. 开机自启动（通过 Windows 任务计划程序）
6. 进程健康检查和自动重启
7. 窗口最小化到托盘动画

---

## 2. 项目结构

```
cpp_launcher/
├── CMakeLists.txt          # CMake 构建配置
├── main.cpp                # 程序入口点
├── build_msvc.bat          # MSVC 构建脚本
├── res/
│   ├── resource.rc         # 资源脚本
│   ├── resource.h          # 资源 ID 定义
│   └── icon.ico            # 应用图标
├── src/
│   ├── Common.h            # 公共定义和工具函数
│   ├── Config.h            # 配置管理
│   ├── Logger.h            # 日志系统
│   ├── AnsiParser.h        # ANSI 颜色解析器
│   ├── ProcessManager.h    # 进程管理
│   ├── TrayIcon.h          # 系统托盘图标
│   ├── HotkeyManager.h     # 全局热键管理
│   ├── StartupManager.h    # 开机自启动管理
│   └── MainWindow.h        # 主窗口（核心 UI）
├── dist/                   # 输出目录
│   └── OpenClawLauncher.exe
└── installer/              # 安装程序脚本
    └── setup.iss
```

---

## 3. 核心模块设计

### 3.1 Common.h - 公共定义

```cpp
namespace Launcher {

// 状态枚举
enum class State { STOPPED, STARTING, RUNNING };

// 托盘菜单命令 ID
constexpr int ID_TRAY_SHOW = 1001;
constexpr int ID_TRAY_START_STOP = 1002;
constexpr int ID_TRAY_RESTART = 1003;
constexpr int ID_TRAY_CLEAR = 1004;
constexpr int ID_TRAY_EXIT = 1005;
constexpr int ID_TRAY_START = 1006;
constexpr int ID_TRAY_STOP = 1010;

// 托盘消息
constexpr int WM_TRAYICON = WM_USER + 1;
constexpr int WM_HOTKEY_TOGGLE = WM_USER + 2;

// 默认端口
constexpr int DEFAULT_PORT = 18789;

// 版本号
constexpr const char* VERSION = "1.0.0";

// 工具函数
std::string GetTimestamp();
bool FileExists(const std::string& path);
std::string GetAppDir();
std::string GetConfigDir();
bool IsAdmin();
void CreateDirRecursive(const std::string& path);
std::wstring Utf8ToWide(const std::string& str);
std::string WideToUtf8(const std::wstring& str);
std::vector<std::string> Split(const std::string& str, char delim);
std::string Trim(const std::string& str);

}
```

### 3.2 Config.h - 配置管理

**配置文件路径**: `%APPDATA%\.openclaw\launcher.ini`

**配置项**:
```ini
[launcher]
openclaw_path = C:\Users\xxx\.openclaw\openclaw.exe
openclaw_config = C:\Users\xxx\.openclaw\config.yaml
gateway_port = 18789
window_hotkey = ctrl+shift+o
windows_startup = false
max_start_attempts = 3
```

**实现要点**:
- 单例模式
- 自动发现 OpenClaw 可执行文件和配置文件
- 使用 Win32 API `WritePrivateProfileStringA` / `GetPrivateProfileStringA`

### 3.3 Logger.h - 日志系统

**功能**:
- 内存日志缓冲区（最大 1000 条）
- 文件日志输出到 `%TEMP%\openclaw_launcher.log`
- 支持颜色标记

**实现要点**:
- 单例模式
- `std::deque` 存储日志条目
- `std::mutex` 线程安全

### 3.4 AnsiParser.h - ANSI 颜色解析器

**支持的 ANSI SGR 代码**:
- `\033[30m` - `\033[37m`: 前景色 (黑、红、绿、黄、蓝、品红、青、白)
- `\033[90m` - `\033[97m`: 亮色版本
- `\033[0m` 或 `\033[39m`: 重置为默认色

**颜色映射表**:
```cpp
std::map<std::string, COLORREF> colorMap_ = {
    {"default", RGB(0, 255, 0)},      // 绿色
    {"black", RGB(30, 30, 30)},
    {"red", RGB(255, 85, 85)},
    {"green", RGB(0, 255, 68)},
    {"yellow", RGB(255, 204, 0)},
    {"blue", RGB(0, 136, 255)},
    {"magenta", RGB(255, 0, 255)},
    {"cyan", RGB(0, 255, 255)},
    {"white", RGB(255, 255, 255)},
    {"bright_black", RGB(128, 128, 128)},
    // ... 亮色版本
};
```

**解析结果结构**:
```cpp
struct TextSegment {
    std::string text;
    std::string colorName;
};
```

### 3.5 ProcessManager.h - 进程管理

**核心功能**:
1. `FindOpenclawExecutable()` - 查找 OpenClaw 可执行文件
   - 搜索路径: 当前目录、`%APPDATA%\.openclaw\`、PATH 环境变量

2. `FindConfigFile()` - 查找配置文件
   - 搜索路径: 当前目录、`%APPDATA%\.openclaw\`

3. `ReadGatewayPort(configPath)` - 从配置文件读取端口

4. `FindGatewayPids()` - 查找运行中的进程 PID
   - 使用 `CreateToolhelp32Snapshot` 枚举进程

5. `StartProcess()` - 启动进程
   - 使用 `CreateProcessW` 创建进程
   - 创建管道捕获 stdout/stderr
   - 启动后台线程读取输出

6. `StopProcess()` - 停止进程
   - 发送 Ctrl+C 信号
   - 等待进程退出
   - 必要时强制终止

7. `HealthCheck()` - HTTP 健康检查
   - 使用 WinHTTP 访问 `http://127.0.0.1:{port}/`

8. `WaitForReady(timeout)` - 等待进程就绪
   - 监控输出中是否包含 "listening on" 关键字

**输出回调**:
```cpp
std::function<void(const std::string& msg, const std::string& color, bool isStderr)> logCallback_;
```

### 3.6 TrayIcon.h - 系统托盘

**图标状态**:
- STOPPED: 深灰色圆形 (RGB 50, 50, 50)
- STARTING: 黄色圆形 (RGB 255, 204, 0)
- RUNNING: 绿色圆形 (RGB 0, 255, 68)

**托盘菜单项**:
1. 显示/隐藏窗口
2. --- 分隔线 ---
3. 启动/停止 (动态文字)
4. 重启
5. --- 分隔线 ---
6. 退出

**动态菜单更新**:
- 使用定时器 (200ms) 在菜单显示期间更新菜单项文字
- 定时器 ID: `MENU_UPDATE_TIMER_ID = 9999`

**图标创建**:
- 使用 DIB Section 创建 64x64 像素的 32 位 RGBA 图标
- 圆形填充，边缘透明

### 3.7 HotkeyManager.h - 全局热键

**功能**:
- 解析热键字符串 (如 "ctrl+shift+o")
- 注册/注销全局热键
- 需要管理员权限

**支持的控制键**:
- ctrl/control, alt, shift, win/windows

**支持的按键**:
- a-z, 0-9
- F1-F24
- 特殊键: space, enter, tab, insert, delete, home, end, pageup, pagedown, up, down, left, right, backspace
- 符号键: , . ; / ` [ \ ] ' - =

### 3.8 StartupManager.h - 开机自启动

**实现方式**: Windows 任务计划程序 (schtasks.exe)

**命令**:
```batch
# 创建任务
schtasks /create /tn OpenClawLauncher /tr "\"path\to\exe\"" /sc onlogon /delay 0000:30 /rl limited /f

# 删除任务
schtasks /delete /tn OpenClawLauncher /f

# 查询任务
schtasks /query /tn OpenClawLauncher
```

**注意**: 延迟 30 秒启动，避免开机时资源竞争

### 3.9 MainWindow.h - 主窗口（核心）

#### 3.9.1 窗口样式
```cpp
CreateWindowExA(
    WS_EX_COMPOSITED | WS_EX_LAYERED,  // 分层窗口，支持透明度
    "OpenClawLauncherClass",
    "OpenClaw Launcher",
    WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,  // 无标题栏，可调整大小
    ...
);
```

#### 3.9.2 窗口布局
```
+------------------------------------------+
| [图标] OpenClaw Launcher      [_] [X]   |  <- 自绘标题栏 (32px)
+------------------------------------------+
| > OpenClaw: C:\...openclaw.exe           |  <- 信息标签区
| > Config: C:\...config.yaml              |
| > Health: http://127.0.0.1:18789/        |
| > HOTKEY: ctrl+shift+o                   |
| > PID: 12345                             |
+------------------------------------------+
| [启动] [停止] [重启] [检查]  [在线]  [热键] [清空] [开机自启] | <- 按钮区
+------------------------------------------+
|                                          |
|           终端日志区域                    |  <- RichEdit 控件
|           (RichEdit)                     |
|                                          |
+------------------------------------------+
```

#### 3.9.3 控件规格

**字体**:
- 主字体: Consolas 14px
- 状态字体: Microsoft YaHei UI 20px (粗体)
- 终端字体: Consolas 9px

**颜色方案**:
- 窗口背景: RGB(12, 12, 12)
- 标题栏背景: RGB(8, 8, 8)
- 信息区背景: RGB(8, 8, 8)
- 终端背景: RGB(10, 10, 10)
- 主文字色: RGB(0, 255, 0) - 绿色
- 次要文字色: RGB(0, 170, 0)
- 边框色: RGB(0, 80, 0)
- 高亮边框: RGB(0, 120, 0)

**按钮样式** (Owner-draw):
- 正常: 背景 RGB(0, 25, 0), 边框 RGB(0, 100, 0)
- 悬停: 背景 RGB(0, 50, 0), 边框 RGB(0, 180, 0)
- 按下: 背景 RGB(0, 40, 0), 边框 RGB(0, 150, 0)
- 禁用: 背景 RGB(20, 20, 20), 边框 RGB(40, 40, 40), 文字 RGB(80, 80, 80)
- 圆角半径: 6px

#### 3.9.4 RichEdit 终端控件

**创建**:
```cpp
// 加载 RichEdit 库
HMODULE hRichEdit = LoadLibraryA("msftedit.dll");

// 创建控件
hTerminal_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
    ...);

// 设置扁平滚动条
InitializeFlatSB(hTerminal_);
FlatSB_SetScrollProp(hTerminal_, WSB_PROP_VSTYLE, FSB_ENCARTA_MODE, TRUE);

// 设置字体和颜色
CHARFORMAT2W cf;
cf.cbSize = sizeof(cf);
cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
cf.crTextColor = RGB(0, 255, 0);
cf.yHeight = 9 * 20;  // 9 磅 = 180 twips
wcscpy_s(cf.szFaceName, L"Consolas");
SendMessage(hTerminal_, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

// 设置背景色
SendMessage(hTerminal_, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));

// 启用滚动事件通知
SendMessage(hTerminal_, EM_SETEVENTMASK, 0, ENM_SCROLL);
```

**智能滚动**:
```cpp
bool autoScroll_ = true;  // 跟踪是否应该自动滚动

// 检测是否在底部
bool IsScrolledToBottom() {
    SCROLLINFO si = {sizeof(si), SIF_ALL};
    GetScrollInfo(hTerminal_, SB_VERT, &si);
    return (si.nPos + (int)si.nPage >= si.nMax - 1);
}

// 滚动到底部
void ScrollToBottom() {
    SendMessage(hTerminal_, WM_VSCROLL, SB_BOTTOM, 0);
}

// 监听滚动事件
case WM_NOTIFY:
    if (((NMHDR*)lParam)->hwndFrom == hTerminal_ && 
        ((NMHDR*)lParam)->code == EN_VSCROLL) {
        autoScroll_ = IsScrolledToBottom();
    }
    break;

// 追加文本时智能滚动
void AppendText(const std::string& text) {
    bool wasAtBottom = IsScrolledToBottom();
    // ... 追加文本 ...
    if (wasAtBottom && autoScroll_) {
        ScrollToBottom();
    }
}
```

**追加带颜色文本**:
```cpp
void AppendTextWithColor(const std::string& text, COLORREF color) {
    // 获取当前文本长度
    GETTEXTLENGTHEX gtl = {GTL_DEFAULT, 1200};
    int len = (int)SendMessage(hTerminal_, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    // 设置插入点
    SendMessage(hTerminal_, EM_SETSEL, len, len);
    
    // 设置颜色
    CHARFORMAT2W cf = {sizeof(cf), CFM_COLOR};
    cf.crTextColor = color;
    SendMessage(hTerminal_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // 转换为 UTF-16 并插入
    std::wstring wtext = Utf8ToWide(text);
    SendMessageW(hTerminal_, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str());
}
```

#### 3.9.5 自定义消息

```cpp
constexpr int WM_LOG_MESSAGE = WM_USER + 200;     // 从线程记录日志
constexpr int WM_STATE_CHANGE = WM_USER + 100;    // 状态变化
constexpr int WM_RESTART_REQUEST = WM_USER + 101; // 重启请求
constexpr int WM_APPEND_TEXT = WM_USER + 202;     // 追加文本到终端
constexpr int WM_SET_PID_TEXT = WM_USER + 203;    // 设置 PID 文本
constexpr int WM_CHECK_COMPLETE = WM_USER + 204;  // 健康检查完成
constexpr int WM_QUIT_APP = WM_USER + 205;        // 退出应用
```

#### 3.9.6 最小化到托盘动画

**实现步骤**:
1. 使用 `PrintWindow` 捕获窗口截图
2. 创建分层窗口显示截图
3. 隐藏主窗口
4. 在分层窗口上执行缩放+移动动画（8帧，16ms/帧）
5. 动画结束后销毁分层窗口

**动画参数**:
- 帧数: 8
- 帧间隔: 16ms
- 缩放: 从 100% 到 10%
- 透明度: 从 255 到 50
- 移动: 从窗口中心到托盘图标中心

---

## 4. 构建配置

### 4.1 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(OpenClawLauncher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

add_executable(OpenClawLauncher WIN32
    main.cpp
    res/resource.rc
)

target_link_libraries(OpenClawLauncher PRIVATE
    comctl32
    shell32
    ole32
    oleaut32
    winhttp
    dwmapi
    comdlg32
)

target_compile_definitions(OpenClawLauncher PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)
```

### 4.2 构建脚本 (build_msvc.bat)

```batch
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

copy Release\OpenClawLauncher.exe ..\dist\
cd ..
```

---

## 5. 关键实现细节

### 5.1 DPI 缩放

```cpp
UINT dpi_ = GetDpiForSystem();

int Scale(int value) const {
    return MulDiv(value, dpi_, 96);
}
```

### 5.2 窗口圆角边框

```cpp
// 使用 GDI 绘制圆角矩形
void DrawRoundedRect(HDC hdc, RECT* rc, int radius) {
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
}

// 添加高亮效果
HPEN hHighlightPen = CreatePen(PS_SOLID, 1, RGB(0, 100, 0));
Arc(hdc, left, top, left + radius*2, top + radius*2, ...);
```

### 5.3 自绘按钮

```cpp
void DrawButton(LPDRAWITEMSTRUCT lpDIS) {
    BOOL isEnabled = IsWindowEnabled(lpDIS->hwndItem);
    BOOL isPressed = (lpDIS->itemState & ODS_SELECTED);
    
    // 根据状态选择颜色
    COLORREF bgColor = isEnabled ? (isPressed ? RGB(0, 40, 0) : RGB(0, 25, 0)) : RGB(20, 20, 20);
    COLORREF borderColor = isEnabled ? (isPressed ? RGB(0, 150, 0) : RGB(0, 100, 0)) : RGB(40, 40, 40);
    
    // 绘制圆角矩形背景
    HBRUSH hBgBrush = CreateSolidBrush(bgColor);
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, borderColor);
    // ...
}
```

### 5.4 自绘复选框

```cpp
void DrawCheckbox(LPDRAWITEMSTRUCT lpDIS) {
    LONG_PTR userData = GetWindowLongPtr(lpDIS->hwndItem, GWLP_USERDATA);
    BOOL isChecked = (userData == 1);
    
    // 绘制方框
    Rectangle(hdc, boxX, boxY, boxX + boxSize, boxY + boxSize);
    
    // 如果选中，绘制勾选标记
    if (isChecked) {
        MoveToEx(hdc, cx - offset, cy, NULL);
        LineTo(hdc, cx, cy + offset);
        LineTo(hdc, cx + offset, cy - offset);
    }
}
```

### 5.5 进程输出捕获

```cpp
// 创建管道
SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);

// 设置继承
SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

// 创建进程
STARTUPINFOEXW si = {0};
si.StartupInfo.cb = sizeof(si);
si.StartupInfo.hStdOutput = hWritePipe;
si.StartupInfo.hStdError = hWritePipe;
si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

CreateProcessW(..., EXTENDED_STARTUPINFO_PRESENT, ...);

// 启动读取线程
std::thread([this, hReadPipe]() {
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        buffer[bytesRead] = '\0';
        logCallback_(buffer, "default", false);
    }
}).detach();
```

---

## 6. 已知问题和解决方案

### 6.1 RichEdit 背景色问题
- **问题**: 启动时显示灰色背景
- **原因**: 在 RichEdit 上叠加了 STATIC 控件 (SS_BLACKRECT)
- **解决**: 移除 STATIC 框架控件，让 RichEdit 直接显示

### 6.2 托盘菜单动态更新
- **问题**: 菜单显示后文字不更新
- **解决**: 使用定时器在菜单显示期间定期调用 ModifyMenu

### 6.3 按钮ID冲突
- **问题**: 窗口按钮和托盘菜单使用相同 ID
- **解决**: 使用不同的 ID，在 WM_COMMAND 中分别处理

### 6.4 智能滚动
- **问题**: 用户查看历史日志时自动滚动干扰
- **解决**: 监听 EN_VSCROLL 事件，只在用户位于底部时自动滚动

---

## 7. 测试检查清单

- [ ] 启动时自动发现 OpenClaw 可执行文件
- [ ] 启动时自动发现配置文件
- [ ] 点击启动按钮能正确启动进程
- [ ] 点击停止按钮能正确停止进程
- [ ] 点击重启按钮能正确重启进程
- [ ] 终端能正确显示 ANSI 颜色
- [ ] 终端能正确滚动到最新日志
- [ ] 用户滚动后不会自动滚动
- [ ] 托盘图标能正确显示状态
- [ ] 托盘菜单能正确动态更新
- [ ] 双击托盘图标能显示/隐藏窗口
- [ ] 全局热键能正常工作
- [ ] 开机自启动能正常工作
- [ ] 最小化动画能正常播放
- [ ] 窗口能正确处理 DPI 缩放
- [ ] 进程意外退出能自动重启
