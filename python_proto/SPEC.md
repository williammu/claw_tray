# OpenClaw Launcher Python 原型 - 详细设计规格说明

## 1. 项目概述

### 1.1 项目名称
OpenClaw Launcher Python Prototype - OpenClaw Gateway 的 Windows 桌面启动器（Python 原型实现）

### 1.2 技术栈
- **语言**: Python 3.10+
- **GUI框架**: customtkinter (基于 Tkinter 的现代 UI 库)
- **托盘图标**: pystray
- **全局热键**: pynput
- **进程管理**: psutil
- **图像处理**: Pillow
- **打包工具**: PyInstaller
- **目标平台**: Windows 10/11

### 1.3 版本
VERSION = "2.3"

---

## 2. 项目结构

```
python_proto/
├── launcher.py        # 主程序（所有功能单文件实现）
├── config.json        # 配置文件示例
├── requirements.txt   # Python 依赖
├── build.py           # PyInstaller 打包脚本
└── SPEC.md            # 本文档
```

---

## 3. 依赖项

### requirements.txt
```
customtkinter>=5.2.2
pystray>=0.19.4
pynput>=1.7.6
psutil>=5.9.8
Pillow>=10.2.0
pyinstaller>=6.4.0
```

---

## 4. 核心模块设计

### 4.1 AnsiParser 类 - ANSI 颜色解析器

**功能**: 解析终端输出的 ANSI 转义序列，提取颜色信息

**正则模式**:
```python
ANSI_PATTERN = re.compile(r'\033\[(?P<code>\d+(?:;\d+)*)m')
```

**ANSI 颜色映射**:
```python
ANSI_COLORS = {
    '30': 'black', '31': 'red', '32': 'green', '33': 'yellow',
    '34': 'blue', '35': 'magenta', '36': 'cyan', '37': 'white',
    '90': 'gray', '91': 'red_bright', '92': 'green_bright', 
    '93': 'yellow_bright', '94': 'blue_bright', 
    '95': 'magenta_bright', '96': 'cyan_bright', '97': 'white_bright',
}
```

**方法**:
- `reset()` - 重置当前颜色标签为 "default"
- `parse(text)` - 解析文本，返回 `(text, tag)` 的列表

### 4.2 ConfigManager 类 - 配置管理器

**配置文件路径**: `%APPDATA%\OpenClawLauncher\config.json`

**默认配置**:
```python
{
    "openclaw_path": "",
    "openclaw_config_path": "",
    "gateway_port": 18789,
    "auto_detect": True,
    "ready_pattern": "listening|ready|Gateway started",
    "health_check_url": "/",
    "health_check_timeout": 5,
    "health_check_keyword": "openclaw",
    "max_start_attempts": 2,
    "retry_delay": 3,
    "terminal_font": "Consolas",
    "terminal_font_size": 11,
    "log_max_lines": 10000,
    "auto_start": False,
    "auto_minimize": False,
    "appearance_mode": "System",
    "window_hotkey": "ctrl+shift+o",
    "windows_startup": False
}
```

**方法**:
- `load()` - 从文件加载配置
- `save()` - 保存配置到文件
- `get(key, default)` - 获取配置项
- `set(key, value)` - 设置配置项

### 4.3 IconGenerator 类 - 图标生成器

**功能**: 动态生成托盘图标（复古终端风格）

**颜色方案**:
```python
COLORS = {
    'stopped': (50, 50, 50),      # 深灰 - 离线
    'starting': (255, 204, 0),    # 琥珀色 - 启动中
    'running': (0, 255, 68),      # 荧光绿 - 在线
}
```

**方法**:
- `generate(state, size=64)` - 生成指定状态的圆角矩形图标

### 4.4 Logger 类 - 日志管理器

**日志文件路径**: `%APPDATA%\OpenClawLauncher\launcher.log`

**功能**:
- 启动时截断日志文件到 `max_lines` 行
- 记录日志时自动去除 ANSI 颜色代码
- 线程安全（使用 `threading.Lock`）

**日志格式**: `[2024-01-15 10:30:25] [INFO] 消息内容`

### 4.5 OutputMonitor 类 - 输出监控器

**功能**: 捕获进程输出并检测就绪标志

**属性**:
- `output_buffer` - 输出缓冲区（最近 100 行）
- `ready_event` - 就绪事件（线程同步）
- `ready_pattern` - 就绪模式正则表达式

**方法**:
- `set_pattern(pattern)` - 设置就绪模式
- `add_line(line)` - 添加一行输出，检查是否匹配就绪模式

### 4.6 OpenClawLauncher 类 - 主类

#### 4.6.1 状态管理

**状态枚举**:
- `STOPPED` - 已停止
- `STARTING` - 启动中
- `RUNNING` - 运行中

**状态相关属性**:
```python
self.state = "STOPPED"
self.try_count = 0          # 自动重启尝试次数
self.manual_stop = False    # 是否为手动停止
self.process = None         # subprocess.Popen 对象
self.pid = None             # 进程 PID
```

#### 4.6.2 颜色方案（复古终端风格）

```python
self.colors = {
    "bg": "#0c0c0c",           # 深黑背景
    "fg": "#00ff00",           # 荧光绿文字
    "fg_dim": "#00aa00",       # 暗绿
    "fg_bright": "#39ff14",    # 亮绿
    "accent": "#008f11",       # 终端绿
    "border": "#003300",       # 深绿边框
    "button_bg": "#1a1a1a",    # 按钮背景
    "button_hover": "#2d2d2d", # 按钮悬停
    "error": "#ff3333",        # 错误红
    "warning": "#ffaa00",      # 警告黄
}
```

#### 4.6.3 UI 组件

**信息栏**:
- `info_path` - OpenClaw 路径
- `info_config` - 配置文件路径
- `info_health` - 健康检查地址
- `info_hotkey` - 当前热键
- `info_pid` - 进程 PID

**按钮**:
- `start_btn` - 启动按钮
- `stop_btn` - 停止按钮
- `restart_btn` - 重启按钮
- `health_btn` - 健康检查按钮
- `hotkey_config_btn` - 热键配置按钮
- `clear_btn` - 清空终端按钮
- `startup_checkbox` - 开机自启动复选框

**终端**:
- `terminal` - CTkTextbox 控件，支持 ANSI 颜色

#### 4.6.4 智能滚动

**实现**:
```python
def _is_terminal_at_bottom(self):
    """检查终端是否在底部"""
    if self._terminal_selecting:  # 用户正在选择文本
        return False
    return self._terminal_at_bottom

def _check_terminal_scroll_position(self):
    """检查滚动位置"""
    first, last = text_widget.yview()
    self._terminal_at_bottom = (last >= 0.99)
```

**事件绑定**:
- `<Button-1>` - 点击开始选择
- `<B1-Motion>` - 拖动选择
- `<ButtonRelease-1>` - 释放结束选择
- `<MouseWheel>` - 滚轮滚动

---

## 5. 核心功能实现

### 5.1 自动发现流程

```
启动器启动
    ↓
[步骤1: 查找可执行文件]
    ├── 检查配置中的路径
    ├── 使用 shutil.which("openclaw")
    ├── 尝试扩展名 .exe, .cmd
    └── 未找到 → 弹出文件选择对话框
    ↓
[步骤2: 查找配置文件]
    ├── ~/.openclaw/openclaw.json
    └── 未找到 → 使用默认配置
    ↓
[步骤3: 读取网关端口]
    ├── 从 openclaw.json 读取 gateway.port
    └── 失败 → 使用默认端口 18789
    ↓
[步骤4: 保存配置]
    └── 自动保存到 config.json
```

### 5.2 启动流程

```python
def _do_start(self, manual=True):
    # 1. 手动启动时重置计数器
    if manual:
        self.try_count = 0
        self.manual_stop = False
    else:
        self.try_count += 1
        if self.try_count > max_start_attempts:
            return  # 放弃
    
    # 2. 设置状态为 STARTING
    self._set_state("STARTING")
    
    # 3. 构建命令（直接启动 Node，绕过 gateway.cmd）
    node_exe, script_path = self._build_node_command()
    
    # 4. 创建进程（无窗口）
    self.process = subprocess.Popen(
        [node_exe, script_path, "gateway", f"--port={port}"],
        creationflags=subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP,
        ...
    )
    
    # 5. 第一步：监控输出或健康检查
    ready = self._step_one_monitor(ready_pattern)
    
    # 6. 第二步：URL 健康检查
    if ready and self._step_two_health_check():
        self._set_state("RUNNING")
        self._start_pid_monitor()
```

### 5.3 无窗口执行

**关键代码**:
```python
creationflags = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP

startupinfo = subprocess.STARTUPINFO()
startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startupinfo.wShowWindow = 0  # SW_HIDE

self.process = subprocess.Popen(
    ...,
    creationflags=creationflags,
    startupinfo=startupinfo,
    shell=False,  # 关键：不使用 shell
)
```

### 5.4 PID 监控

```python
def _pid_monitor_loop(self):
    while not self.stop_monitor.is_set() and self.state == "RUNNING":
        if self.pid and not psutil.pid_exists(self.pid):
            self.root.after(0, self._on_process_exit)
            break
        time.sleep(1)

def _on_process_exit(self):
    if self.manual_stop:
        self._set_state("STOPPED")
    else:
        # 异常退出，触发自动重启
        self._do_start(manual=False)
```

### 5.5 开机自启动

**实现方式**: Windows 任务计划程序 + VBScript 包装器

**创建任务**:
```python
def _create_startup_task(self):
    # 创建 VBScript 包装器（隐藏窗口）
    vbs_content = f'''Set WshShell = CreateObject("WScript.Shell")
WshShell.Run "\"{exe_path}\" \"{script_path}\"", 0, False
Set WshShell = Nothing'''
    
    # 创建计划任务（延迟 30 秒启动）
    cmd = [
        'schtasks', '/create',
        '/tn', 'OpenClawLauncher',
        '/tr', f'"{vbs_path}"',
        '/sc', 'onlogon',
        '/delay', '0000:30',
        '/rl', 'limited',
        '/f'
    ]
```

**删除任务**:
```python
def _remove_startup_task(self):
    subprocess.run(['schtasks', '/delete', '/tn', 'OpenClawLauncher', '/f'])
```

### 5.6 全局热键

**实现**: 使用 pynput 监听键盘事件

```python
def _setup_pynput_hotkey_v2(self, hotkey_str):
    target_keys = set(hotkey_str.lower().split('+'))
    current_keys = set()
    
    def on_press(key):
        current_keys.add(get_key_name(key))
        if all_pressed:
            self.root.after(0, self._toggle_window)
    
    def on_release(key):
        current_keys.discard(get_key_name(key))
    
    self.hotkey_listener = kb.Listener(on_press=on_press, on_release=on_release)
    self.hotkey_listener.start()
```

**热键配置**:
- 点击"设热键"按钮进入配置模式
- 按下组合键后自动保存
- 按 ESC 取消

### 5.7 托盘图标

**创建托盘**:
```python
def _setup_tray(self):
    for state in ['stopped', 'starting', 'running']:
        img = IconGenerator.generate(state)
        self.tray_icons[state] = img
    
    self.tray_icon = pystray.Icon(
        "openclaw_launcher",
        icon=self.tray_icons['stopped'],
        menu=self._create_tray_menu()
    )
    
    threading.Thread(target=self.tray_icon.run, daemon=True).start()
```

**托盘菜单**:
- 显示/隐藏窗口
- --- 分隔线 ---
- 启动/停止（动态启用/禁用）
- 重启
- --- 分隔线 ---
- 清空终端
- --- 分隔线 ---
- 退出

---

## 6. 打包配置

### build.py

```python
cmd = [
    sys.executable, "-m", "PyInstaller",
    "--onefile",
    "--noconsole",
    "--name", "OpenClawLauncher",
    "--paths", str(anaconda_lib),
]

# 添加 DLL 文件
for dll in dlls:
    cmd.extend(["--add-binary", f"{dll_path};."])

# 添加依赖
cmd.extend([
    "--collect-all", "PIL",
    "--collect-all", "customtkinter",
    "--hidden-import", "PIL._imaging",
    "--hidden-import", "PIL._imagingft",
    "launcher.py"
])
```

---

## 7. 与 C++ 实现的差异

| 功能 | Python 原型 | C++ 实现 |
|------|------------|----------|
| GUI 框架 | customtkinter | Win32 API |
| 托盘图标 | pystray（独立线程） | Shell_NotifyIcon（主线程） |
| 全局热键 | pynput（键盘监听） | RegisterHotKey |
| 进程管理 | psutil | CreateToolhelp32Snapshot |
| 最小化动画 | ❌ 无 | ✅ 有 |
| 配置文件 | JSON | INI |
| 单文件 | 是（~2200 行） | 否（多文件模块化） |
| 打包大小 | ~50MB（含 Python 运行时） | ~500KB |

---

## 8. 已知限制

1. **打包体积大**: PyInstaller 打包后约 50MB
2. **启动速度**: 比 C++ 实现慢
3. **无最小化动画**: 窗口直接隐藏/显示
4. **热键权限**: 需要管理员权限
5. **DPI 缩放**: 依赖 customtkinter 自动处理

---

## 9. 测试检查清单

- [ ] 启动时自动发现 OpenClaw 可执行文件
- [ ] 启动时自动发现配置文件
- [ ] 点击启动按钮能正确启动进程
- [ ] 点击停止按钮能正确停止进程
- [ ] 点击重启按钮能正确重启进程
- [ ] 终端能正确显示 ANSI 颜色
- [ ] 终端能正确滚动到最新日志
- [ ] 用户滚动后不会自动滚动
- [ ] 托盘图标能正确显示状态
- [ ] 双击托盘图标能显示/隐藏窗口
- [ ] 全局热键能正常工作
- [ ] 开机自启动能正常工作
- [ ] 进程意外退出能自动重启
- [ ] 日志能正确持久化
- [ ] PyInstaller 打包能正常工作
