#!/usr/bin/env python3
"""
OpenClaw Launcher for Windows 11
基于 customtkinter 的现代化启动器
"""

import os
import sys
import json
import time
import shutil
import atexit
import subprocess
import threading
import re
import urllib.request
import xml.sax.saxutils
from pathlib import Path
from datetime import datetime
from collections import deque

# Windows specific imports
import ctypes
from ctypes import wintypes

# Third party imports
try:
    import customtkinter as ctk
    from PIL import Image, ImageDraw
    import psutil
    import pystray
    from pynput import keyboard
except ImportError as e:
    print(f"缺少依赖: {e}")
    print("请先运行: pip install -r requirements.txt")
    sys.exit(1)

# Version
VERSION = "2.3"
DEFAULT_PORT = 18789


class AnsiParser:
    """ANSI转义序列解析器"""
    
    ANSI_PATTERN = re.compile(r'\033\[(?P<code>\d+(?:;\d+)*)m')
    
    ANSI_COLORS = {
        '30': 'black', '31': 'red', '32': 'green', '33': 'yellow',
        '34': 'blue', '35': 'magenta', '36': 'cyan', '37': 'white',
        '90': 'gray', '91': 'red_bright', '92': 'green_bright', 
        '93': 'yellow_bright', '94': 'blue_bright', 
        '95': 'magenta_bright', '96': 'cyan_bright', '97': 'white_bright',
    }
    
    def __init__(self):
        self.reset()
    
    def reset(self):
        self.current_tag = "default"
    
    def parse(self, text):
        """解析文本，返回(text, tag)的列表"""
        result = []
        pos = 0
        
        for match in self.ANSI_PATTERN.finditer(text):
            # 插入匹配前的文本
            if match.start() > pos:
                result.append((text[pos:match.start()], self.current_tag))
            
            # 解析ANSI代码
            code = match.group('code')
            if code == '0':
                self.current_tag = "default"
            elif code in self.ANSI_COLORS:
                self.current_tag = self.ANSI_COLORS[code]
            
            pos = match.end()
        
        # 插入剩余文本
        if pos < len(text):
            result.append((text[pos:], self.current_tag))
        
        return result


def get_app_dir():
    """获取应用数据目录（Windows: %APPDATA%\OpenClawLauncher）"""
    if os.name == 'nt':  # Windows
        app_data = os.environ.get('APPDATA')
        if app_data:
            return Path(app_data) / 'OpenClawLauncher'
    # Fallback: 用户主目录
    return Path.home() / '.openclaw_launcher'


class ConfigManager:
    """配置管理器"""
    
    def __init__(self, config_path=None):
        # 默认使用 %APPDATA%\OpenClawLauncher\config.json
        if config_path is None:
            app_dir = get_app_dir()
            app_dir.mkdir(parents=True, exist_ok=True)
            self.config_path = app_dir / 'config.json'
        else:
            self.config_path = Path(config_path)
        self.config = self._load_default()
        self.load()
    
    def _load_default(self):
        return {
            "openclaw_path": "",
            "openclaw_config_path": "",
            "gateway_port": DEFAULT_PORT,
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
            "windows_startup": False  # Windows开机自启动
        }
    
    def load(self):
        """加载配置"""
        if self.config_path.exists():
            try:
                with open(self.config_path, 'r', encoding='utf-8') as f:
                    loaded = json.load(f)
                    self.config.update(loaded)
            except Exception as e:
                print(f"加载配置失败: {e}")
    
    def save(self):
        """保存配置"""
        try:
            with open(self.config_path, 'w', encoding='utf-8') as f:
                json.dump(self.config, f, indent=4, ensure_ascii=False)
        except Exception as e:
            print(f"保存配置失败: {e}")
    
    def get(self, key, default=None):
        return self.config.get(key, default)
    
    def set(self, key, value):
        self.config[key] = value


class IconGenerator:
    """图标生成器 - 复古终端风格"""
    
    COLORS = {
        'stopped': (50, 50, 50),      # 深灰 - 离线
        'starting': (255, 204, 0),    # 琥珀色 - 启动中
        'running': (0, 255, 68),      # 荧光绿 - 在线
    }
    
    @classmethod
    def generate(cls, state, size=64):
        """生成圆角矩形图标"""
        color = cls.COLORS.get(state, cls.COLORS['stopped'])
        
        # 创建图像
        img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        
        # 绘制圆角矩形
        radius = size // 4
        draw.rounded_rectangle(
            [0, 0, size-1, size-1],
            radius=radius,
            fill=color
        )
        
        return img


class Logger:
    """日志管理器"""
    
    def __init__(self, log_path=None, max_lines=10000):
        # 默认使用 %APPDATA%\OpenClawLauncher\launcher.log
        if log_path is None:
            app_dir = get_app_dir()
            app_dir.mkdir(parents=True, exist_ok=True)
            self.log_path = app_dir / 'launcher.log'
        else:
            self.log_path = Path(log_path)
        self.max_lines = max_lines
        self.lock = threading.Lock()
        self._truncate_on_startup()
    
    def _truncate_on_startup(self):
        """启动时截断到max_lines行"""
        if not self.log_path.exists():
            return
        
        try:
            with open(self.log_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = deque(f, maxlen=self.max_lines)
            
            with open(self.log_path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
        except Exception:
            pass
    
    def log(self, message, level="INFO"):
        """记录日志（去除ANSI代码）"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        # 去除ANSI代码
        clean_msg = re.sub(r'\033\[\d+(?:;\d+)*m', '', message)
        line = f"[{timestamp}] [{level}] {clean_msg}\n"
        
        with self.lock:
            try:
                with open(self.log_path, 'a', encoding='utf-8') as f:
                    f.write(line)
            except Exception:
                pass


class OutputMonitor:
    """输出监控器，用于捕获和检查输出"""
    
    def __init__(self, launcher):
        self.launcher = launcher
        self.output_buffer = []
        self.buffer_lock = threading.Lock()
        self.ready_event = threading.Event()
        self.ready_pattern = None
    
    def set_pattern(self, pattern):
        """设置就绪模式"""
        self.ready_pattern = re.compile(pattern, re.IGNORECASE) if pattern else None
    
    def add_line(self, line):
        """添加一行输出"""
        with self.buffer_lock:
            self.output_buffer.append(line)
            # 保持最近100行
            if len(self.output_buffer) > 100:
                self.output_buffer.pop(0)
        
        # 检查是否匹配就绪模式
        if self.ready_pattern and self.ready_pattern.search(line):
            self.ready_event.set()


class OpenClawLauncher:
    """OpenClaw启动器主类"""
    
    def __init__(self):
        self.state = "STOPPED"  # STOPPED, STARTING, RUNNING
        self.try_count = 0
        self.manual_stop = False
        self.process = None
        self.pid = None
        self.monitor_thread = None
        self.stop_monitor = threading.Event()
        self.output_monitor = OutputMonitor(self)
        
        # 组件初始化
        self.config = ConfigManager()
        self.logger = Logger(max_lines=self.config.get("log_max_lines", 10000))
        self.ansi_parser = AnsiParser()
        
        # 发现的信息
        self.openclaw_path = None
        self.openclaw_config_path = None
        self.gateway_port = DEFAULT_PORT
        
        # GUI组件
        self.root = None
        self.terminal = None
        self.tray_icon = None
        self.hotkey_listener = None
        
        # 托盘图标缓存
        self.tray_icons = {}
        
        # 启动锁，防止重复启动
        self._start_lock = threading.Lock()
        
        # 终端滚动控制
        self._terminal_at_bottom = True
        self._terminal_selecting = False
        
        # 热键配置状态
        self._listening_for_hotkey = False
        self._temp_hotkey_keys = set()
        self._temp_listener = None
    
    def run(self):
        """运行启动器"""
        # 设置customtkinter
        ctk.set_appearance_mode(self.config.get("appearance_mode", "System"))
        ctk.set_default_color_theme("blue")
        
        # 创建主窗口
        self.root = ctk.CTk()
        self.root.title("OpenClaw Launcher")
        self.root.geometry("900x700")
        self.root.minsize(800, 600)
        
        # 窗口关闭事件
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        
        # 创建UI
        self._setup_ui()
        
        # 初始化日志系统
        self._log_to_terminal("")
        self._log_to_terminal("****************************************")
        self._log_to_terminal("*  OpenClaw Gateway Launcher v" + VERSION + "  *")
        self._log_to_terminal("****************************************")
        
        # 自动发现阶段
        self._auto_discover()
        
        # 创建托盘图标
        self._setup_tray()
        
        # 注册热键
        self._setup_hotkey()
        
        # 同步自启动状态（检查并清理现有任务）
        self._sync_startup_status()
        
        # 检测是否已在运行
        self._check_already_running()
        
        # 如果未在运行，自动启动（默认行为）
        if self.state == "STOPPED":
            self._log_to_terminal("")
            self._log_to_terminal("[SYSTEM] Gateway not running, auto-starting...")
            self.start_openclaw()
        
        # 最小化到托盘
        if self.config.get("auto_minimize", False):
            self.root.withdraw()
        
        # 启动主循环
        self.root.mainloop()
    
    def _setup_ui(self):
        """设置UI - 复古终端风格（黑底绿字）"""
        # 配置复古终端颜色
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
        
        # 设置窗口背景
        self.root.configure(fg_color=self.colors["bg"])
        
        # 主框架
        self.main_frame = ctk.CTkFrame(
            self.root,
            fg_color=self.colors["bg"],
            border_color=self.colors["border"],
            border_width=2
        )
        self.main_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        # 信息栏 - 复古风格
        self.info_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="#0a0a0a",
            border_color=self.colors["border"],
            border_width=1
        )
        self.info_frame.pack(fill="x", padx=5, pady=5)
        
        # 终端风格字体
        terminal_font = ("Consolas", 11)
        
        # 四行信息
        self.info_path = ctk.CTkLabel(
            self.info_frame, 
            text="> Openclaw: [未配置]",
            font=terminal_font,
            text_color=self.colors["fg"],
            anchor="w"
        )
        self.info_path.pack(fill="x", padx=10, pady=1)
        
        self.info_config = ctk.CTkLabel(
            self.info_frame,
            text="> Config: [未找到]",
            font=terminal_font,
            text_color=self.colors["fg_dim"],
            anchor="w"
        )
        self.info_config.pack(fill="x", padx=10, pady=1)
        
        self.info_health = ctk.CTkLabel(
            self.info_frame,
            text=f"> Health: http://127.0.0.1:{DEFAULT_PORT}/",
            font=terminal_font,
            text_color=self.colors["fg_dim"],
            anchor="w"
        )
        self.info_health.pack(fill="x", padx=10, pady=1)
        
        # 显示当前热键
        current_hotkey = self.config.get("window_hotkey", "ctrl+shift+o")
        self.info_hotkey = ctk.CTkLabel(
            self.info_frame,
            text=f"> HOTKEY: {current_hotkey}",
            font=terminal_font,
            text_color=self.colors["fg_dim"],
            anchor="w"
        )
        self.info_hotkey.pack(fill="x", padx=10, pady=1)
        
        self.info_pid = ctk.CTkLabel(
            self.info_frame,
            text="> PID: -",
            font=terminal_font,
            text_color=self.colors["fg_bright"],
            anchor="w"
        )
        self.info_pid.pack(fill="x", padx=10, pady=1)
        
        # 按钮区域 - 复古风格
        self.btn_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color=self.colors["bg"],
            border_color=self.colors["border"],
            border_width=1
        )
        self.btn_frame.pack(fill="x", padx=5, pady=5)
        
        btn_font = ("Consolas", 12, "bold")
        
        self.start_btn = ctk.CTkButton(
            self.btn_frame, text="[启动]",
            command=self.start_openclaw,
            width=80,
            font=btn_font,
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["accent"],
            border_width=1
        )
        self.start_btn.pack(side="left", padx=5)
        
        self.stop_btn = ctk.CTkButton(
            self.btn_frame, text="[停止]",
            command=self.stop_openclaw,
            width=80,
            font=btn_font,
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["accent"],
            border_width=1,
            state="disabled"
        )
        self.stop_btn.pack(side="left", padx=5)
        
        self.restart_btn = ctk.CTkButton(
            self.btn_frame, text="[重启]",
            command=self.restart_openclaw,
            width=80,
            font=btn_font,
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["accent"],
            border_width=1,
            state="disabled"
        )
        self.restart_btn.pack(side="left", padx=5)
        
        self.health_btn = ctk.CTkButton(
            self.btn_frame, text="[检查]",
            command=self._manual_health_check,
            width=80,
            font=btn_font,
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["accent"],
            border_width=1
        )
        self.health_btn.pack(side="left", padx=5)
        
        # 自启动复选框
        startup_var = ctk.BooleanVar(value=self.config.get("windows_startup", False))
        self.startup_checkbox = ctk.CTkCheckBox(
            self.btn_frame,
            text="开机自启",
            variable=startup_var,
            command=self._on_startup_toggle,
            font=("Consolas", 10),
            fg_color=self.colors["accent"],
            hover_color=self.colors["fg_dim"],
            text_color=self.colors["fg"],
            border_color=self.colors["border"],
            checkbox_width=16,
            checkbox_height=16
        )
        self.startup_checkbox.pack(side="right", padx=5)
        self._startup_var = startup_var
        
        self.hotkey_config_btn = ctk.CTkButton(
            self.btn_frame, text="[设热键]",
            command=self._configure_hotkey,
            width=80,
            font=btn_font,
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["accent"],
            border_width=1
        )
        self.hotkey_config_btn.pack(side="right", padx=5)
        
        self.clear_btn = ctk.CTkButton(
            self.btn_frame, text="[清空]",
            command=self._clear_terminal,
            width=80,
            font=btn_font,
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["accent"],
            border_width=1
        )
        self.clear_btn.pack(side="right", padx=5)
        
        # 状态标签 - 复古风格
        self.status_label = ctk.CTkLabel(
            self.btn_frame,
            text="[OFFLINE]",
            font=("Consolas", 14, "bold"),
            text_color=self.colors["error"]
        )
        self.status_label.pack(side="left", padx=20)
        
        # 终端区域 - 复古 CRT 显示器风格
        self.terminal_frame = ctk.CTkFrame(
            self.main_frame,
            fg_color="#050505",  # 更深的黑色
            border_color=self.colors["accent"],
            border_width=2
        )
        self.terminal_frame.pack(fill="both", expand=True, padx=5, pady=5)
        
        # CRT 荧光效果字体
        self.terminal_font = ("Consolas", 12)
        
        self.terminal = ctk.CTkTextbox(
            self.terminal_frame,
            font=self.terminal_font,
            wrap="char",               # 字符换行，无水平滚动条
            fg_color="#0a0a0a",        # 终端黑底
            text_color=self.colors["fg"],  # 荧光绿字
            border_color=self.colors["border"],
            border_width=1,
            scrollbar_button_color=self.colors["accent"],
            scrollbar_button_hover_color=self.colors["fg"]
        )
        self.terminal.pack(fill="both", expand=True, padx=5, pady=5)
        
        # 修复：设置底层canvas背景色为黑色（避免蓝色背景）
        self.terminal._canvas.configure(bg="#0a0a0a", highlightthickness=0)
        
        # 配置Text控件 - 设置背景和选中样式
        text_widget = self.terminal._textbox
        text_widget.configure(
            bg="#0a0a0a",
            highlightthickness=0,
            selectbackground=self.colors["fg"],      # 选中背景：荧光绿
            selectforeground="#0a0a0a",               # 选中文字：深黑
            inactiveselectbackground=self.colors["fg_dim"]  # 非活动选中：暗绿
        )
        
        # 配置颜色标签
        self._setup_terminal_tags()
    
    def _setup_terminal_tags(self):
        """配置终端颜色标签 - 复古荧光绿配色"""
        text_widget = self.terminal._textbox
        
        # 复古终端配色
        colors = {
            "default": self.colors["fg"],           # 默认荧光绿
            "black": "#003300",                     # 深绿黑
            "red": "#ff3333",                       # 错误红
            "green": self.colors["fg"],             # 标准绿
            "yellow": "#ffcc00",                    # 警告黄
            "blue": "#00ccff",                      # 信息蓝
            "magenta": "#ff00ff",                   # 洋红
            "cyan": "#00ffff",                      # 青色
            "white": "#ccffcc",                     # 淡绿白
            "gray": self.colors["fg_dim"],          # 暗绿灰
            "red_bright": "#ff6666",                # 亮红
            "green_bright": self.colors["fg_bright"], # 亮绿
            "yellow_bright": "#ffee66",             # 亮黄
            "blue_bright": "#66ddff",               # 亮蓝
            "magenta_bright": "#ff66ff",            # 亮洋红
            "cyan_bright": "#66ffff",               # 亮青
            "white_bright": "#ffffff",              # 纯白
        }
        
        for tag, color in colors.items():
            # 只设置前景色，不设置背景色，以便选中样式能正确显示
            text_widget.tag_config(tag, foreground=color)
        
        # 配置选中文字高亮 - 反转颜色显示
        text_widget.tag_config("sel", background=self.colors["fg"], foreground="#0a0a0a")
        
        # 提升 sel 标签优先级，确保选中样式覆盖颜色标签
        text_widget.tag_raise("sel")
        
        # 禁用文本控件的默认选中样式（避免蓝色背景）
        text_widget.config(insertbackground=self.colors["fg"])  # 光标颜色
        
        # 绑定鼠标事件来跟踪用户是否在选择文本
        text_widget.bind("<Button-1>", self._on_terminal_click)
        text_widget.bind("<B1-Motion>", self._on_terminal_select)
        text_widget.bind("<ButtonRelease-1>", self._on_terminal_release)
        
        # 绑定滚轮事件
        text_widget.bind("<MouseWheel>", self._on_terminal_scroll)
        text_widget.bind("<Shift-MouseWheel>", self._on_terminal_scroll)
    
    def _on_terminal_click(self, event):
        """终端点击事件 - 开始选择"""
        self._terminal_selecting = True
        self._check_terminal_scroll_position()
    
    def _on_terminal_select(self, event):
        """终端选择事件 - 拖动选择中"""
        self._terminal_selecting = True
    
    def _on_terminal_release(self, event):
        """终端释放事件 - 选择结束"""
        # 延迟重置选择状态，给用户时间复制
        self.root.after(1000, lambda: setattr(self, '_terminal_selecting', False))
    
    def _on_terminal_scroll(self, event):
        """终端滚动事件"""
        self._check_terminal_scroll_position()
    
    def _check_terminal_scroll_position(self):
        """检查终端滚动位置，判断是否在底部"""
        text_widget = self.terminal._textbox
        
        # 获取当前视图范围
        first, last = text_widget.yview()
        
        # 如果 last >= 0.99 认为在底部（允许小误差）
        self._terminal_at_bottom = (last >= 0.99)
    
    def _is_terminal_at_bottom(self):
        """检查终端是否在底部"""
        if not hasattr(self, '_terminal_at_bottom'):
            return True  # 默认允许滚动
        
        # 如果用户正在选择文本，不滚动
        if hasattr(self, '_terminal_selecting') and self._terminal_selecting:
            return False
        
        return self._terminal_at_bottom
    
    def _log_to_terminal(self, message, tag="default"):
        """输出到终端（带ANSI解析，智能滚动）"""
        # 在主线程中执行
        if threading.current_thread() != threading.main_thread():
            self.root.after(0, lambda: self._log_to_terminal(message, tag))
            return
        
        # 检查当前是否在底部（插入前检查）
        should_scroll = self._is_terminal_at_bottom()
        
        # 解析ANSI
        segments = self.ansi_parser.parse(message)
        
        for text, seg_tag in segments:
            self.terminal.insert("end", text, seg_tag)
        
        self.terminal.insert("end", "\n")
        
        # 智能滚动：只有在底部或用户未选择时才滚动
        if should_scroll:
            self.terminal.see("end")
        
        # 同时记录到日志
        self.logger.log(message)
    
    def _clear_terminal(self):
        """清空终端"""
        self.terminal.delete("1.0", "end")
        self.ansi_parser.reset()
    
    def _manual_health_check(self):
        """手动触发健康检查"""
        self._log_to_terminal("")
        self._log_to_terminal("🏥 手动健康检查")
        
        # 检查进程
        pids = self._find_gateway_pids()
        if pids:
            self._log_to_terminal(f"发现进程 PID: {pids}")
        else:
            self._log_to_terminal("未找到 Openclaw 进程")
        
        # HTTP 检查
        url = f"http://127.0.0.1:{self.gateway_port}/"
        self._log_to_terminal(f"▶ HTTP GET: {url}")
        
        try:
            import urllib.request
            req = urllib.request.Request(url, method='GET')
            req.add_header('User-Agent', 'OpenClaw-Launcher')
            
            with urllib.request.urlopen(req, timeout=5) as response:
                status = response.status
                content = response.read().decode('utf-8', errors='replace')[:100]
                self._log_to_terminal(f"  Response: HTTP {status}")
                self._log_to_terminal(f"  Content: {content}...")
                
                if status == 200:
                    self._log_to_terminal("✅ Gateway 运行正常")
                    # 更新状态为 RUNNING
                    if self.state != "RUNNING":
                        self._set_state("RUNNING")
                else:
                    self._log_to_terminal(f"⚠️ HTTP 状态异常: {status}")
        except Exception as e:
            self._log_to_terminal(f"❌ 连接失败: {e}")
            self._log_to_terminal("Gateway 未运行或无法访问")
    
    def _configure_hotkey(self):
        """配置自定义热键"""
        self._log_to_terminal("")
        self._log_to_terminal("========================================")
        self._log_to_terminal("[HOTKEY CONFIGURATION]")
        self._log_to_terminal("========================================")
        self._log_to_terminal("")
        self._log_to_terminal("按下你想要的组合键...")
        self._log_to_terminal("(例如: Ctrl+Shift+O, Alt+F12 等)")
        self._log_to_terminal("按 ESC 取消")
        self._log_to_terminal("")
        
        # 停止现有热键监听
        self._unregister_hotkey()
        
        # 进入监听模式
        self._listening_for_hotkey = True
        self._temp_hotkey_keys = set()
        
        from pynput import keyboard as kb
        
        def get_key_name(key):
            """获取键的标准化名称"""
            try:
                if hasattr(key, 'name') and key.name:
                    return key.name.lower()
                elif hasattr(key, 'char') and key.char:
                    char = key.char.lower()
                    # 处理 Ctrl+Shift+字母 产生的控制字符
                    ctrl_shift_map = {
                        '\x0f': 'o', '\x01': 'a', '\x02': 'b', '\x03': 'c',
                        '\x04': 'd', '\x05': 'e', '\x06': 'f', '\x07': 'g',
                        '\x08': 'h', '\x09': 'i', '\x0a': 'j', '\x0b': 'k',
                        '\x0c': 'l', '\x0d': 'm', '\x0e': 'n', '\x10': 'p',
                        '\x11': 'q', '\x12': 'r', '\x13': 's', '\x14': 't',
                        '\x15': 'u', '\x16': 'v', '\x17': 'w', '\x18': 'x',
                        '\x19': 'y', '\x1a': 'z',
                    }
                    return ctrl_shift_map.get(char, char)
            except:
                pass
            return None
        
        def on_press(key):
            if not self._listening_for_hotkey:
                return False  # 停止监听
            
            key_name = get_key_name(key)
            
            # ESC 取消
            if key_name == 'esc':
                self._listening_for_hotkey = False
                self.root.after(0, lambda: self._log_to_terminal("[HOTKEY] Cancelled"))
                self.root.after(0, self._setup_hotkey)  # 恢复原来的热键
                return False
            
            if key_name:
                self._temp_hotkey_keys.add(key_name)
                self.root.after(0, lambda k=sorted(self._temp_hotkey_keys): 
                    self._log_to_terminal(f"[HOTKEY] Current keys: {' + '.join(k)}"))
        
        def on_release(key):
            if not self._listening_for_hotkey:
                return False
            
            key_name = get_key_name(key)
            
            # 如果释放了修饰键之外的键，认为是组合键完成
            if key_name and key_name not in ['ctrl', 'ctrl_l', 'ctrl_r', 
                                               'shift', 'shift_l', 'shift_r',
                                               'alt', 'alt_l', 'alt_r', 'menu']:
                if len(self._temp_hotkey_keys) >= 2:  # 至少2个键才是组合键
                    self._listening_for_hotkey = False
                    # 格式化热键字符串
                    hotkey_str = self._format_hotkey(self._temp_hotkey_keys)
                    self.root.after(0, lambda h=hotkey_str: self._save_hotkey(h))
                    return False
                else:
                    # 按键太少，继续等待
                    pass
            
            # 释放键时从集合中移除
            if key_name:
                self._temp_hotkey_keys.discard(key_name)
        
        # 启动临时监听
        self._temp_listener = kb.Listener(on_press=on_press, on_release=on_release)
        self._temp_listener.start()
    
    def _format_hotkey(self, keys):
        """格式化热键字符串"""
        # 定义顺序：修饰键在前，普通键在后
        modifier_order = ['ctrl', 'ctrl_l', 'ctrl_r', 
                         'alt', 'alt_l', 'alt_r', 'menu',
                         'shift', 'shift_l', 'shift_r',
                         'win', 'cmd', 'command']
        
        modifiers = []
        normal_keys = []
        
        for key in keys:
            if key in modifier_order or any(key.startswith(m) for m in ['ctrl', 'alt', 'shift']):
                # 标准化修饰键名称
                if 'ctrl' in key:
                    if 'ctrl' not in modifiers:
                        modifiers.append('ctrl')
                elif 'alt' in key or key == 'menu':
                    if 'alt' not in modifiers:
                        modifiers.append('alt')
                elif 'shift' in key:
                    if 'shift' not in modifiers:
                        modifiers.append('shift')
                elif 'win' in key or key in ['cmd', 'command']:
                    if 'win' not in modifiers:
                        modifiers.append('win')
            else:
                normal_keys.append(key)
        
        # 按标准顺序排列修饰键
        ordered_modifiers = []
        for mod in ['ctrl', 'alt', 'shift', 'win']:
            if mod in modifiers:
                ordered_modifiers.append(mod)
        
        parts = ordered_modifiers + normal_keys
        return '+'.join(parts)
    
    def _save_hotkey(self, hotkey_str):
        """保存热键到配置"""
        self._log_to_terminal("")
        self._log_to_terminal(f"[HOTKEY] New hotkey: {hotkey_str}")
        
        # 保存到配置
        self.config.set("window_hotkey", hotkey_str)
        self.config.save()
        
        # 更新UI显示
        self.info_hotkey.configure(text=f"> HOTKEY: {hotkey_str}")
        
        self._log_to_terminal(f"[HOTKEY] Saved to {self.config.config_path}")
        self._log_to_terminal("")
        
        # 重新注册热键
        self._setup_hotkey()
    
    def _on_startup_toggle(self):
        """自启动复选框切换回调 - 异步执行避免卡死UI"""
        enabled = self._startup_var.get()
        
        # 立即禁用checkbox防止重复点击
        self.startup_checkbox.configure(state="disabled")
        
        if enabled:
            # 在后台线程中执行检查和创建，避免阻塞UI
            self._log_to_terminal("[AUTO-START] Checking for conflicts...")
            
            def enable_task_thread():
                # 1. 检查冲突任务
                other_tasks = self._check_other_openclaw_tasks()
                if other_tasks:
                    # 存在冲突，回到主线程显示弹窗
                    self.root.after(0, lambda: self._on_conflict_detected(other_tasks))
                    return
                
                # 2. 创建任务
                self._log_to_terminal("[AUTO-START] Creating startup task...")
                success, msg = self._create_startup_task()
                self.root.after(0, lambda: self._on_create_task_done(success, msg))
            
            threading.Thread(target=enable_task_thread, daemon=True).start()
        else:
            # 在后台线程中删除任务
            def remove_task_thread():
                success, msg = self._remove_startup_task()
                self.root.after(0, lambda: self._on_remove_task_done(success, msg))
            
            threading.Thread(target=remove_task_thread, daemon=True).start()
    
    def _on_conflict_detected(self, other_tasks):
        """检测到冲突任务回调（主线程）"""
        self.startup_checkbox.configure(state="normal")
        self._startup_var.set(False)  # 取消勾选
        self._show_conflict_dialog(other_tasks)
    
    def _on_create_task_done(self, success, msg):
        """创建任务完成回调（主线程）"""
        self.startup_checkbox.configure(state="normal")
        
        if success:
            self.config.set("windows_startup", True)
            self.config.save()
            self._startup_var.set(True)
            self._log_to_terminal(f"[AUTO-START] ✓ {msg}")
        else:
            self._startup_var.set(False)
            self._log_to_terminal(f"[AUTO-START] ✗ {msg}")
            # 显示错误弹窗
            self._show_error_dialog("创建计划任务失败", msg)
    
    def _on_remove_task_done(self, success, msg):
        """删除任务完成回调（主线程）"""
        self.startup_checkbox.configure(state="normal")
        
        if success:
            self.config.set("windows_startup", False)
            self.config.save()
            self._startup_var.set(False)
            self._log_to_terminal(f"[AUTO-START] ✓ {msg}")
        else:
            self._startup_var.set(True)
            self._log_to_terminal(f"[AUTO-START] ✗ {msg}")
    
    def _check_other_openclaw_tasks(self):
        """检查是否有其他 OpenClaw 相关任务（排除 OpenClawLauncher）"""
        try:
            result = subprocess.run(
                ['powershell', '-Command', 
                 'Get-ScheduledTask -TaskName "*OpenClaw*" | Where-Object { $_.TaskName -ne "OpenClawLauncher" } | Select-Object -ExpandProperty TaskName'],
                capture_output=True,
                text=True,
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            if result.returncode == 0 and result.stdout.strip():
                return [line.strip() for line in result.stdout.strip().split('\n') if line.strip()]
        except Exception:
            pass
        return []
    
    def _show_conflict_dialog(self, task_names):
        """显示冲突任务提示弹窗"""
        dialog = ctk.CTkToplevel(self.root)
        dialog.title("⚠️ 检测到冲突的任务")
        dialog.geometry("450x200")
        dialog.resizable(False, False)
        dialog.transient(self.root)
        dialog.grab_set()
        
        # 设置弹窗在父窗口居中
        dialog.update_idletasks()
        x = self.root.winfo_x() + (self.root.winfo_width() - 450) // 2
        y = self.root.winfo_y() + (self.root.winfo_height() - 200) // 2
        dialog.geometry(f"+{x}+{y}")
        
        # 弹窗背景色
        dialog.configure(fg_color=self.colors["bg"])
        
        # 提示文本
        tasks_text = "\n".join([f"• {name}" for name in task_names])
        msg = f"检测到以下 OpenClaw 相关的计划任务已存在：\n\n{tasks_text}\n\n请先手动删除这些任务，再启用自启动。"
        
        label = ctk.CTkLabel(
            dialog,
            text=msg,
            font=("Consolas", 11),
            text_color=self.colors["fg"],
            justify="left",
            wraplength=400
        )
        label.pack(pady=20, padx=20)
        
        # 按钮区域
        btn_frame = ctk.CTkFrame(dialog, fg_color=self.colors["bg"])
        btn_frame.pack(pady=10)
        
        # 打开计划任务按钮
        open_btn = ctk.CTkButton(
            btn_frame,
            text="打开计划任务",
            command=lambda: [self._open_task_scheduler(), dialog.destroy()],
            font=("Consolas", 11, "bold"),
            fg_color=self.colors["accent"],
            hover_color=self.colors["fg_dim"],
            text_color=self.colors["fg"],
            width=120
        )
        open_btn.pack(side="left", padx=10)
        
        # 取消按钮
        cancel_btn = ctk.CTkButton(
            btn_frame,
            text="取消",
            command=dialog.destroy,
            font=("Consolas", 11),
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["border"],
            border_width=1,
            width=80
        )
        cancel_btn.pack(side="left", padx=10)
    
    def _open_task_scheduler(self):
        """打开计划任务管理器"""
        try:
            subprocess.Popen(
                ['taskschd.msc'],
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            self._log_to_terminal("[AUTO-START] Opened Task Scheduler")
        except Exception as e:
            self._log_to_terminal(f"[AUTO-START] Failed to open Task Scheduler: {e}")
    
    def _show_error_dialog(self, title, message):
        """显示错误弹窗"""
        dialog = ctk.CTkToplevel(self.root)
        dialog.title(f"⚠️ {title}")
        dialog.geometry("450x180")
        dialog.resizable(False, False)
        dialog.transient(self.root)
        dialog.grab_set()
        
        # 设置弹窗在父窗口居中
        dialog.update_idletasks()
        x = self.root.winfo_x() + (self.root.winfo_width() - 450) // 2
        y = self.root.winfo_y() + (self.root.winfo_height() - 180) // 2
        dialog.geometry(f"+{x}+{y}")
        
        dialog.configure(fg_color=self.colors["bg"])
        
        # 错误图标和文本
        label = ctk.CTkLabel(
            dialog,
            text=message,
            font=("Consolas", 11),
            text_color=self.colors["error"],
            justify="left",
            wraplength=400
        )
        label.pack(pady=30, padx=20)
        
        # 确定按钮
        ok_btn = ctk.CTkButton(
            dialog,
            text="确定",
            command=dialog.destroy,
            font=("Consolas", 11, "bold"),
            fg_color=self.colors["button_bg"],
            hover_color=self.colors["button_hover"],
            text_color=self.colors["fg"],
            border_color=self.colors["border"],
            border_width=1,
            width=80
        )
        ok_btn.pack(pady=10)
    
    def _check_startup_task_exists(self):
        """检查计划任务是否存在"""
        try:
            result = subprocess.run(
                ['schtasks', '/query', '/tn', 'OpenClawLauncher', '/fo', 'list'],
                capture_output=True,
                text=True,
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            return result.returncode == 0 and 'OpenClawLauncher' in result.stdout
        except Exception:
            return False
    
    def _create_startup_task(self):
        """创建开机启动计划任务（延迟30秒启动）
        
        Returns:
            (success: bool, message: str)
        """
        try:
            # 获取当前可执行路径
            exe_path = sys.executable
            script_path = os.path.abspath(__file__)
            
            self._log_to_terminal(f"[AUTO-START] EXE: {exe_path}")
            self._log_to_terminal(f"[AUTO-START] Script: {script_path}")
            self._log_to_terminal(f"[AUTO-START] Frozen: {getattr(sys, 'frozen', False)}")
            
            # 创建VBScript包装器来隐藏窗口
            vbs_content = f'''Set WshShell = CreateObject("WScript.Shell")
WshShell.Run "\"{exe_path}\" \"{script_path}\"", 0, False
Set WshShell = Nothing'''
            
            vbs_path = os.path.join(os.environ['TEMP'], 'openclaw_launcher.vbs')
            with open(vbs_path, 'w') as f:
                f.write(vbs_content)
            
            self._log_to_terminal(f"[AUTO-START] VBS created: {vbs_path}")
            
            # 删除旧任务（如果存在）
            self._remove_startup_task()
            
            # 创建任务 - 登录时延迟30秒启动
            # /tr 参数的路径需要用引号包裹（因为可能包含空格）
            cmd = [
                'schtasks', '/create',
                '/tn', 'OpenClawLauncher',
                '/tr', f'"{vbs_path}"',
                '/sc', 'onlogon',
                '/delay', '0000:30',
                '/rl', 'limited',
                '/f'
            ]
            
            self._log_to_terminal(f"[AUTO-START] Command: {' '.join(cmd)}")
            
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            
            self._log_to_terminal(f"[AUTO-START] Return code: {result.returncode}")
            if result.stdout and result.stdout.strip():
                self._log_to_terminal(f"[AUTO-START] stdout: {result.stdout}")
            if result.stderr and result.stderr.strip():
                self._log_to_terminal(f"[AUTO-START] stderr: {result.stderr}")
            
            if result.returncode == 0 or '成功' in result.stdout or 'created' in result.stdout.lower():
                return True, "Enabled - Will start on Windows boot (delayed 30s)"
            else:
                error_msg = result.stderr.strip() or result.stdout.strip() or "Unknown error"
                return False, f"Failed to create task: {error_msg}"
                
        except Exception as e:
            import traceback
            error_detail = traceback.format_exc()
            self._log_to_terminal(f"[AUTO-START] Exception: {error_detail}")
            return False, f"Exception: {str(e)}"
    
    def _generate_task_xml(self, exe_path):
        """生成任务计划XML（隐藏窗口，延迟启动）"""
        escaped_path = xml.sax.saxutils.escape(exe_path)
        
        xml_content = f'''<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Description>OpenClaw Launcher - Auto start on boot</Description>
  </RegistrationInfo>
  <Triggers>
    <LogonTrigger>
      <Enabled>true</Enabled>
      <Delay>PT30S</Delay>
    </LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author">
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>LeastPrivilege</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>true</AllowHardTerminate>
    <StartWhenAvailable>false</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <StopOnIdleEnd>true</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>false</RunOnlyIfIdle>
    <DisallowStartOnRemoteAppSession>false</DisallowStartOnRemoteAppSession>
    <UseUnifiedSchedulingEngine>true</UseUnifiedSchedulingEngine>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>{escaped_path}</Command>
      <WorkingDirectory>{xml.sax.saxutils.escape(os.path.dirname(exe_path))}</WorkingDirectory>
    </Exec>
  </Actions>
</Task>'''
        return xml_content
    
    def _remove_startup_task(self):
        """删除开机启动计划任务
        
        Returns:
            (success: bool, message: str)
        """
        try:
            result = subprocess.run(
                ['schtasks', '/delete', '/tn', 'OpenClawLauncher', '/f'],
                capture_output=True,
                text=True,
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            
            self._log_to_terminal(f"[AUTO-START] Remove return code: {result.returncode}")
            
            # 返回码0表示成功删除，1可能表示任务不存在也算成功
            if result.returncode in [0, 1] or '成功' in result.stdout or 'deleted' in result.stdout.lower():
                return True, "Disabled"
            else:
                error_msg = result.stderr.strip() or "Unknown error"
                return False, f"Failed to remove: {error_msg}"
                
        except Exception as e:
            return False, f"Exception: {str(e)}"
    
    def _sync_startup_status(self):
        """同步自启动状态（启动时调用）"""
        task_exists = self._check_startup_task_exists()
        config_enabled = self.config.get("windows_startup", False)
        
        if task_exists and not config_enabled:
            # 任务存在但配置未启用，删除任务
            self._log_to_terminal("[AUTO-START] Found existing task, removing...")
            self._remove_startup_task()
            self._startup_var.set(False)
        elif not task_exists and config_enabled:
            # 配置启用但任务不存在，创建任务
            self._log_to_terminal("[AUTO-START] Creating startup task...")
            self._create_startup_task()
            self._startup_var.set(True)
        else:
            # 同步复选框状态
            self._startup_var.set(task_exists)
            if task_exists:
                self._log_to_terminal("[AUTO-START] ✓ Startup task active")
    
    def _auto_discover(self):
        """自动发现阶段"""
        self._log_to_terminal("")
        self._log_to_terminal("[SYSTEM] Starting auto-discovery sequence...")
        
        # 步骤1: 查找可执行文件
        self._log_to_terminal("")
        self._log_to_terminal("[INIT] Scanning for Openclaw executable...")
        
        self.openclaw_path = self._find_openclaw_executable()
        
        if self.openclaw_path:
            self._log_to_terminal(f"  [OK] Found: {self.openclaw_path}")
            self.info_path.configure(text=f"> Openclaw: {self.openclaw_path}", text_color=self.colors["fg"])
            self.config.set("openclaw_path", str(self.openclaw_path))
        else:
            self._log_to_terminal("  [ERR] Not found, showing dialog...")
            self._show_file_dialog()
        
        # 步骤2: 查找配置文件
        self._log_to_terminal("")
        self._log_to_terminal("[步骤2: 查找配置文件]")
        
        self.openclaw_config_path = self._find_config_file()
        
        if self.openclaw_config_path:
            self._log_to_terminal(f"  [OK] Found: {self.openclaw_config_path}")
            self.info_config.configure(text=f"> Config: {self.openclaw_config_path}", text_color=self.colors["fg_dim"])
            self.config.set("openclaw_config_path", str(self.openclaw_config_path))
        else:
            self._log_to_terminal("  [WARN] Config not found, using defaults")
            self.info_config.configure(text="> Config: [DEFAULTS]", text_color=self.colors["warning"])
        
        # 步骤3: 读取网关端口
        self._log_to_terminal("")
        self._log_to_terminal("[INIT] Reading gateway port...")
        
        self.gateway_port = self._read_gateway_port()
        self._log_to_terminal(f"  [OK] Port: {self.gateway_port}")
        self.info_health.configure(
            text=f"> Health: http://127.0.0.1:{self.gateway_port}/",
            text_color=self.colors["fg_dim"]
        )
        self.config.set("gateway_port", self.gateway_port)
        
        # 步骤4: 保存配置
        self._log_to_terminal("")
        self._log_to_terminal("[INIT] Saving configuration...")
        self.config.save()
        self._log_to_terminal(f"  [OK] Config saved to {self.config.config_path}")
        
        self._log_to_terminal("")
        self._log_to_terminal("========================================")
        self._log_to_terminal("[SYSTEM] Initialization complete")
        self._log_to_terminal("========================================")
    
    def _find_openclaw_executable(self):
        """查找Openclaw可执行文件"""
        # 先检查配置
        configured_path = self.config.get("openclaw_path")
        if configured_path and Path(configured_path).exists():
            return Path(configured_path)
        
        # 使用shutil.which
        path = shutil.which("openclaw")
        if path:
            return Path(path)
        
        # 手动尝试扩展名
        for ext in [".exe", ".cmd"]:
            path = shutil.which(f"openclaw{ext}")
            if path:
                return Path(path)
        
        return None
    
    def _find_config_file(self):
        """查找 Openclaw 配置文件"""
        # Openclaw 使用 JSON 配置文件，位于 ~/.openclaw/openclaw.json
        possible_paths = [
            Path.home() / ".openclaw" / "openclaw.json",
            Path(os.environ.get("USERPROFILE", "")) / ".openclaw" / "openclaw.json",
        ]
        
        for path in possible_paths:
            if path.exists():
                return path
        
        return None
    
    def _read_gateway_port(self):
        """从 openclaw.json 读取网关端口"""
        if not self.openclaw_config_path:
            return self.config.get("gateway_port", DEFAULT_PORT)
        
        try:
            with open(self.openclaw_config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
            
            # 从 gateway 配置读取端口
            gateway_config = config.get("gateway", {})
            port = gateway_config.get("port")
            if port:
                return port
        except Exception as e:
            self._log_to_terminal(f"  ⚠ 读取配置文件失败: {e}")
        
        return self.config.get("gateway_port", DEFAULT_PORT)
    
    def _show_file_dialog(self):
        """显示文件选择对话框"""
        from tkinter import filedialog
        
        path = filedialog.askopenfilename(
            title="选择 Openclaw 可执行文件",
            filetypes=[("可执行文件", "*.exe *.cmd"), ("所有文件", "*.*")]
        )
        
        if path:
            self.openclaw_path = Path(path)
            self.info_path.configure(text=f"📁 Openclaw: {self.openclaw_path}")
        else:
            self._log_to_terminal("  ✗ 用户取消选择")
    
    def _check_already_running(self):
        """检查是否已在运行"""
        url = f"http://127.0.0.1:{self.gateway_port}/"
        self._log_to_terminal(f"▶ Checking if already running: HTTP GET {url}")
        
        try:
            req = urllib.request.Request(url, method='GET')
            req.add_header('User-Agent', 'OpenClaw-Launcher')
            
            with urllib.request.urlopen(req, timeout=2) as response:
                status = response.status
                self._log_to_terminal(f"  Response: HTTP {status}")
                
                if status == 200:
                    self._set_state("RUNNING")
                    self._log_to_terminal("✓ 检测到Openclaw已在运行")
                    self._start_pid_monitor()
                    return
        except Exception as e:
            self._log_to_terminal(f"  Not running: {e}")
        
        self._set_state("STOPPED")
    
    def _setup_tray(self):
        """设置系统托盘"""
        # 生成图标
        for state in ['stopped', 'starting', 'running']:
            img = IconGenerator.generate(state)
            self.tray_icons[state] = img
        
        # 创建托盘图标
        self.tray_icon = pystray.Icon(
            "openclaw_launcher",
            icon=self.tray_icons['stopped'],
            title="Openclaw - 已停止",
            menu=self._create_tray_menu()
        )
        
        # 在后台运行托盘
        threading.Thread(target=self.tray_icon.run, daemon=True).start()
    
    def _create_tray_menu(self):
        """创建托盘菜单"""
        return pystray.Menu(
            pystray.MenuItem(
                "显示/隐藏窗口",
                self._toggle_window
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "启动",
                self.start_openclaw,
                enabled=lambda item: self.state == "STOPPED"
            ),
            pystray.MenuItem(
                "停止",
                self.stop_openclaw,
                enabled=lambda item: self.state in ["RUNNING", "STARTING"]
            ),
            pystray.MenuItem(
                "重启",
                self.restart_openclaw,
                enabled=lambda item: self.state == "RUNNING"
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "清空终端",
                self._clear_terminal
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "退出",
                self._exit_app
            )
        )
    
    def _update_tray(self):
        """更新托盘图标和状态"""
        if not self.tray_icon:
            return
        
        state_map = {
            "STOPPED": ("stopped", "Openclaw - 已停止"),
            "STARTING": ("starting", f"启动中... (第{self.try_count}次尝试)" if self.try_count > 0 else "启动中..."),
            "RUNNING": ("running", "Openclaw - 运行中")
        }
        
        icon_name, title = state_map.get(self.state, ("stopped", "Openclaw"))
        
        self.tray_icon.icon = self.tray_icons.get(icon_name)
        self.tray_icon.title = title
    
    def _is_admin(self):
        """检查是否以管理员权限运行"""
        try:
            return ctypes.windll.shell32.IsUserAnAdmin()
        except Exception:
            return False
    
    def _setup_hotkey(self):
        """设置全局热键"""
        hotkey_str = self.config.get("window_hotkey", "ctrl+shift+o")
        
        # 更新UI显示当前热键
        if hasattr(self, 'info_hotkey') and self.info_hotkey:
            self.info_hotkey.configure(text=f"> HOTKEY: {hotkey_str}")
        
        self._log_to_terminal(f"[INIT] Registering hotkey: {hotkey_str}")
        
        # 检查管理员权限
        if not self._is_admin():
            self._log_to_terminal("⚠ 需要管理员权限才能注册全局热键")
            self._log_to_terminal("  请右键 -> 以管理员身份运行 launcher.py")
            return
        
        # 直接使用 pynput 的按键监听（最可靠的方式）
        try:
            self._setup_pynput_hotkey_v2(hotkey_str)
        except Exception as e:
            self._log_to_terminal(f"⚠ 热键注册失败: {e}")
            self._log_to_terminal("  热键功能已禁用")
    

    
    def _setup_pynput_hotkey_v2(self, hotkey_str):
        """使用 pynput 注册热键 - 简化的按键组合检测"""
        from pynput import keyboard as kb
        
        # 解析热键字符串 (例如: "ctrl+shift+o")
        target_keys = set(hotkey_str.lower().replace(' ', '').split('+'))
        self._log_to_terminal(f"  Target keys: {target_keys}")
        
        current_keys = set()
        
        def get_key_name(key):
            """获取键的标准化名称"""
            try:
                if hasattr(key, 'name') and key.name:
                    return key.name.lower()
                elif hasattr(key, 'char') and key.char:
                    char = key.char.lower()
                    # 处理 Ctrl+Shift+字母 产生的控制字符
                    # Ctrl+Shift+O 产生 '\x0f' (ASCII 15)
                    # Ctrl+Shift 组合键产生的字符需要映射回原始字母
                    ctrl_shift_map = {
                        '\x0f': 'o',  # Ctrl+Shift+O
                        '\x01': 'a',  # Ctrl+Shift+A
                        '\x02': 'b',  # Ctrl+Shift+B
                        '\x03': 'c',
                        '\x04': 'd',
                        '\x05': 'e',
                        '\x06': 'f',
                        '\x07': 'g',
                        '\x08': 'h',
                        '\x09': 'i',
                        '\x0a': 'j',
                        '\x0b': 'k',
                        '\x0c': 'l',
                        '\x0d': 'm',
                        '\x0e': 'n',
                        '\x10': 'p',
                        '\x11': 'q',
                        '\x12': 'r',
                        '\x13': 's',
                        '\x14': 't',
                        '\x15': 'u',
                        '\x16': 'v',
                        '\x17': 'w',
                        '\x18': 'x',
                        '\x19': 'y',
                        '\x1a': 'z',
                    }
                    if char in ctrl_shift_map:
                        return ctrl_shift_map[char]
                    return char
            except:
                pass
            return None
        
        def on_press(key):
            key_name = get_key_name(key)
            if key_name:
                current_keys.add(key_name)
            
            # 检查是否所有目标键都被按下
            all_pressed = True
            missing = []
            for target in target_keys:
                target = target.strip()
                if target == 'ctrl':
                    if not any(k in current_keys for k in ['ctrl', 'ctrl_l', 'ctrl_r', 'control', 'control_l', 'control_r']):
                        all_pressed = False
                        missing.append('ctrl')
                elif target == 'shift':
                    if not any(k in current_keys for k in ['shift', 'shift_l', 'shift_r']):
                        all_pressed = False
                        missing.append('shift')
                elif target == 'alt':
                    if not any(k in current_keys for k in ['alt', 'alt_l', 'alt_r', 'menu']):
                        all_pressed = False
                        missing.append('alt')
                elif target == 'o':
                    if 'o' not in current_keys:
                        all_pressed = False
                        missing.append('o')
                elif target not in current_keys:
                    all_pressed = False
                    missing.append(target)
            
            if all_pressed and len(current_keys) >= len(target_keys):
                self.root.after(0, self._toggle_window)
        
        def on_release(key):
            key_name = get_key_name(key)
            if key_name:
                current_keys.discard(key_name)
        
        self.hotkey_listener = kb.Listener(on_press=on_press, on_release=on_release)
        self.hotkey_listener.start()
        self._log_to_terminal(f"✓ Hotkey registered: Press {hotkey_str} to toggle window")
    
    def _unregister_hotkey(self):
        """注销热键"""
        # 停止临时监听器（配置模式）
        if self._listening_for_hotkey and self._temp_listener:
            try:
                self._listening_for_hotkey = False
                self._temp_listener.stop()
            except Exception:
                pass
        
        # 停止正常热键监听器
        if hasattr(self, 'hotkey_listener') and self.hotkey_listener:
            try:
                self.hotkey_listener.stop()
                self.hotkey_listener = None
            except Exception:
                pass
    
    def _set_state(self, state):
        """设置状态 - 复古终端风格"""
        self.state = state
        
        # 更新状态显示
        state_config = {
            "STOPPED": ("[OFFLINE]", self.colors["error"]),
            "STARTING": ("[BOOTING...]", self.colors["warning"]),
            "RUNNING": ("[ONLINE]", self.colors["fg"])
        }
        
        text, color = state_config.get(state, ("[UNKNOWN]", self.colors["fg_dim"]))
        self.status_label.configure(text=text, text_color=color)
        
        # 更新 PID 显示
        if state == "RUNNING":
            pids = self._find_gateway_pids()
            if pids:
                self.info_pid.configure(text=f"> PID: {pids}", text_color=self.colors["fg_bright"])
            else:
                self.info_pid.configure(text="> PID: scanning...", text_color=self.colors["warning"])
        elif state == "STOPPED":
            self.info_pid.configure(text="> PID: -", text_color=self.colors["fg_dim"])
        
        # 更新按钮状态
        if state == "STOPPED":
            self.start_btn.configure(state="normal")
            self.stop_btn.configure(state="disabled")
            self.restart_btn.configure(state="disabled")
        elif state == "STARTING":
            self.start_btn.configure(state="disabled")
            self.stop_btn.configure(state="normal")
            self.restart_btn.configure(state="disabled")
        elif state == "RUNNING":
            self.start_btn.configure(state="disabled")
            self.stop_btn.configure(state="normal")
            self.restart_btn.configure(state="normal")
        
        # 更新托盘
        self._update_tray()
    
    def start_openclaw(self, icon=None, item=None):
        """启动Openclaw"""
        # 使用锁防止重复启动
        if not self._start_lock.acquire(blocking=False):
            self._log_to_terminal("⚠ 启动操作正在进行中，请稍候...")
            return
        
        try:
            if self.state != "STOPPED":
                self._log_to_terminal(f"⚠ 当前状态为 {self.state}，无法启动")
                return
            
            # 再次检查是否已有进程在运行（双重检查）
            pids = self._find_gateway_pids()
            if pids:
                self._log_to_terminal(f"⚠ 检测到 Openclaw 已在运行 (PID: {pids})，跳过启动")
                self._set_state("RUNNING")
                self._start_pid_monitor()
                return
            
            # 判断是否是手动触发
            manual = self.try_count == 0
            
            # 在新线程中执行
            threading.Thread(target=self._do_start, args=(manual,), daemon=True).start()
        finally:
            # 注意：这里不释放锁，由 _do_start 在完成或失败时释放
            pass
    
    def _get_gateway_env(self):
        """从 gateway.cmd 读取环境变量"""
        env_vars = {}
        gateway_cmd_path = Path.home() / ".openclaw" / "gateway.cmd"
        
        if not gateway_cmd_path.exists():
            return env_vars
        
        try:
            with open(gateway_cmd_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 解析 set "KEY=VALUE" 格式
            import re
            pattern = r'set "([^"]+)=([^"]*)"'
            matches = re.findall(pattern, content)
            for key, value in matches:
                env_vars[key] = value
        except Exception as e:
            self._log_to_terminal(f"⚠ 读取 gateway.cmd 环境变量失败: {e}")
        
        return env_vars
    
    def _build_node_command(self):
        """构建直接启动 Node 的命令（绕过 gateway.cmd，避免终端窗口）"""
        # 读取 gateway.cmd 获取 node 路径和脚本路径
        gateway_cmd_path = Path.home() / ".openclaw" / "gateway.cmd"
        
        # 默认路径
        node_exe = r"C:\Program Files\nodejs\node.exe"
        script_path = Path.home() / "AppData" / "Roaming" / "npm" / "node_modules" / "openclaw" / "dist" / "index.js"
        
        if gateway_cmd_path.exists():
            try:
                with open(gateway_cmd_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                # 提取 node.exe 路径
                import re
                node_match = re.search(r'"([^"]+node\.exe)"', content)
                if node_match:
                    node_exe = node_match.group(1)
            except Exception:
                pass
        
        return node_exe, str(script_path)
    
    def _do_start(self, manual=True):
        """执行启动（在后台线程中）- 直接启动 Node，绕过 gateway.cmd"""
        try:
            # 手动启动时重置计数器
            if manual:
                self.try_count = 0
                self.manual_stop = False
            else:
                # 自动重启时检查次数
                self.try_count += 1
                if self.try_count > self.config.get("max_start_attempts", 2):
                    self._log_to_terminal("❌ 自动重启失败（已尝试2次），请手动启动")
                    self._set_state("STOPPED")
                    return
            
            self._set_state("STARTING")
            
            attempt_info = f" (第{self.try_count}次尝试)" if self.try_count > 0 else ""
            self._log_to_terminal("")
            self._log_to_terminal(f"🚀 正在启动Openclaw{attempt_info}")
            
            # 获取环境变量
            env_vars = self._get_gateway_env()
            env = os.environ.copy()
            env.update(env_vars)
            
            # 构建直接启动 Node 的命令（绕过 gateway.cmd）
            node_exe, script_path = self._build_node_command()
            args = f"gateway --port {self.gateway_port}"
            
            # 显示执行的命令
            cmd_display = f'"{node_exe}" "{script_path}" {args}'
            self._log_to_terminal(f"▶ Executing: {cmd_display}")
            
            # 关键：直接启动 Node，不使用 shell，彻底避免终端窗口
            creationflags = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP
            
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = 0  # SW_HIDE
            
            self.process = subprocess.Popen(
                [node_exe, script_path, "gateway", f"--port={self.gateway_port}"],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=1,
                universal_newlines=False,
                creationflags=creationflags,
                startupinfo=startupinfo,
                shell=False,  # 关键：不使用 shell
                env=env
            )
            
            self.pid = self.process.pid
            self._log_to_terminal(f"✓ Node 进程已启动 (PID: {self.pid})")
            
            # 等待并查找实际的 Openclaw node 进程
            import time
            time.sleep(1)
            pids = self._find_gateway_pids()
            if pids:
                self._log_to_terminal(f"✓ Openclaw Gateway 进程 PID: {pids}")
                self.info_pid.configure(text=f"🆔 PID: {pids}")
            else:
                self.info_pid.configure(text=f"🆔 PID: {self.pid} (启动中)")
            
            # 设置就绪模式
            ready_pattern = self.config.get("ready_pattern", "listening|ready|Gateway started")
            self.output_monitor = OutputMonitor(self)
            self.output_monitor.set_pattern(ready_pattern)
            
            # 启动输出监控线程
            threading.Thread(target=self._monitor_output, daemon=True).start()
            
            # 第一步：监控输出或健康检查
            ready = self._step_one_monitor(ready_pattern)
            
            if ready:
                # 第二步：URL健康检查
                if self._step_two_health_check():
                    self._set_state("RUNNING")
                    self._start_pid_monitor()
                    # 启动成功，释放锁
                    try:
                        self._start_lock.release()
                    except RuntimeError:
                        pass  # 锁可能已经被释放
                else:
                    self._handle_start_failure()
            else:
                self._handle_start_failure()
                
        except Exception as e:
            self._log_to_terminal(f"❌ 启动失败: {e}")
            self._set_state("STOPPED")
            # 异常时释放锁
            try:
                self._start_lock.release()
            except RuntimeError:
                pass
    
    def _monitor_output(self):
        """监控进程输出"""
        if not self.process:
            return
        
        def read_stream(stream, prefix=""):
            for line in iter(stream.readline, b''):
                try:
                    text = line.decode('utf-8', errors='replace').rstrip()
                    if text:
                        # 添加到输出监控
                        self.output_monitor.add_line(text)
                        # 显示到终端
                        self._log_to_terminal(f"{prefix}{text}")
                except Exception:
                    pass
            stream.close()
        
        # 启动stdout和stderr监控线程
        threading.Thread(target=read_stream, args=(self.process.stdout,), daemon=True).start()
        threading.Thread(target=read_stream, args=(self.process.stderr, "[stderr] "), daemon=True).start()
    
    def _step_one_monitor(self, ready_pattern):
        """第一步：监控输出或健康检查"""
        self._log_to_terminal("⏳ 第一步: 监控启动输出...")
        
        start_time = time.time()
        
        # 监控5秒输出或等待就绪模式
        while time.time() - start_time < 5:
            if self.output_monitor.ready_event.is_set():
                self._log_to_terminal("✓ 检测到就绪标志")
                return True
            time.sleep(0.1)
        
        # 5秒后，进行健康检查
        self._log_to_terminal("⏳ 5秒内未检测到就绪标志，进行健康检查...")
        
        # 尝试健康检查（重试2次，间隔1秒）
        for i in range(3):  # 第一次 + 2次重试
            if self._health_check():
                self._log_to_terminal("✓ 健康检查通过")
                return True
            if i < 2:
                self._log_to_terminal(f"  重试 {i+1}/2...")
                time.sleep(1)
        
        return False
    
    def _step_two_health_check(self):
        """第二步：URL健康检查确认"""
        url = f"http://127.0.0.1:{self.gateway_port}{self.config.get('health_check_url', '/')}"
        self._log_to_terminal(f"⏳ 进行最终健康检查")
        self._log_to_terminal(f"▶ HTTP GET: {url}")
        
        timeout = self.config.get("health_check_timeout", 5)
        keyword = self.config.get("health_check_keyword", "openclaw")
        
        try:
            req = urllib.request.Request(url, method='GET')
            req.add_header('User-Agent', 'OpenClaw-Launcher')
            
            with urllib.request.urlopen(req, timeout=timeout) as response:
                status = response.status
                content = response.read().decode('utf-8', errors='replace')
                self._log_to_terminal(f"  Response: HTTP {status}")
                
                if keyword.lower() in content.lower():
                    self._log_to_terminal(f"  Keyword '{keyword}' found in response")
                    self._log_to_terminal("✓ 最终健康检查通过")
                    return True
                else:
                    self._log_to_terminal(f"  Keyword '{keyword}' not found")
        except Exception as e:
            self._log_to_terminal(f"  Error: {e}")
            self._log_to_terminal(f"✗ 健康检查失败: {e}")
        
        return False
    
    def _health_check(self, show_cmd=True):
        """执行健康检查"""
        url = f"http://127.0.0.1:{self.gateway_port}/"
        
        if show_cmd:
            self._log_to_terminal(f"▶ HTTP GET: {url}")
        
        try:
            req = urllib.request.Request(url, method='GET')
            req.add_header('User-Agent', 'OpenClaw-Launcher')
            
            with urllib.request.urlopen(req, timeout=2) as response:
                status = response.status
                if show_cmd:
                    self._log_to_terminal(f"  Response: HTTP {status}")
                return status == 200
        except Exception as e:
            if show_cmd:
                self._log_to_terminal(f"  Error: {e}")
            return False
    
    def _handle_start_failure(self):
        """处理启动失败"""
        self._log_to_terminal("✗ 启动失败")
        
        # 终止进程
        if self.process:
            try:
                self.process.terminate()
                self.process.wait(timeout=5)
            except Exception:
                pass
        
        # 如果是自动重启且未超过次数，则重试（保持锁）
        if self.try_count > 0 and self.try_count <= self.config.get("max_start_attempts", 2):
            delay = self.config.get("retry_delay", 3)
            self._log_to_terminal(f"⏳ {delay}秒后自动重试...")
            time.sleep(delay)
            self._do_start(manual=False)
        else:
            # 不再重试，释放锁
            self._set_state("STOPPED")
            try:
                self._start_lock.release()
            except RuntimeError:
                pass
    
    def _start_pid_monitor(self):
        """启动PID监控线程"""
        self.stop_monitor.clear()
        self.monitor_thread = threading.Thread(target=self._pid_monitor_loop, daemon=True)
        self.monitor_thread.start()
    
    def _pid_monitor_loop(self):
        """PID监控循环"""
        while not self.stop_monitor.is_set() and self.state == "RUNNING":
            try:
                if self.pid and not psutil.pid_exists(self.pid):
                    # PID不存在，进程已退出
                    self.root.after(0, self._on_process_exit)
                    break
            except Exception as e:
                self._log_to_terminal(f"❌ 权限不足，无法监控进程: {e}")
                self.root.after(0, lambda: self._set_state("STOPPED"))
                break
            
            time.sleep(1)
    
    def _on_process_exit(self):
        """进程退出处理"""
        if self.manual_stop:
            # 用户手动停止，不触发自动重启
            self._log_to_terminal("✓ 进程已停止（用户手动停止）")
            self._set_state("STOPPED")
            self.manual_stop = False
        else:
            # 异常退出，触发自动重启
            self._log_to_terminal("⚠️ 检测到Openclaw进程已退出，准备自动重启...")
            self._do_start(manual=False)
    
    def _find_gateway_pids(self):
        """查找 Openclaw Gateway 相关的所有 PID"""
        pids = []
        try:
            for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
                try:
                    if proc.info['name'] and 'node' in proc.info['name'].lower():
                        cmdline = proc.info.get('cmdline', [])
                        if cmdline:
                            cmd_str = ' '.join(cmdline).lower()
                            if 'openclaw' in cmd_str and 'gateway' in cmd_str:
                                pids.append(proc.info['pid'])
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    continue
        except Exception as e:
            self._log_to_terminal(f"⚠ 查找进程时出错: {e}")
        return pids
    
    def stop_openclaw(self, icon=None, item=None):
        """停止Openclaw"""
        if self.state not in ["RUNNING", "STARTING"]:
            return
        
        self.manual_stop = True
        self.stop_monitor.set()
        
        self._log_to_terminal("⏹ 正在停止Openclaw...")
        
        # 先执行 openclaw gateway stop（取消计划任务注册）
        if self.openclaw_path:
            try:
                stop_cmd = f'"{self.openclaw_path}" gateway stop'
                self._log_to_terminal(f"▶ Executing: {stop_cmd}")
                
                result = subprocess.run(
                    stop_cmd,
                    capture_output=True,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                    timeout=10
                )
                
                # 尝试 GBK 解码（中文 Windows 系统默认）
                try:
                    stdout = result.stdout.decode('gbk', errors='replace').strip()
                except:
                    stdout = result.stdout.decode('utf-8', errors='replace').strip()
                
                if stdout:
                    self._log_to_terminal(stdout)
            except Exception as e:
                self._log_to_terminal(f"⚠ gateway stop 命令失败: {e}")
        
        # 查找并终止所有相关进程
        pids = self._find_gateway_pids()
        
        if pids:
            self._log_to_terminal(f"发现 {len(pids)} 个相关进程: {pids}")
            
            for pid in pids:
                try:
                    kill_cmd = f'taskkill /T /F /PID {pid}'
                    self._log_to_terminal(f"▶ Executing: {kill_cmd}")
                    
                    result = subprocess.run(
                        kill_cmd,
                        capture_output=True,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                        timeout=10
                    )
                    
                    # 尝试 GBK 解码（中文 Windows 系统默认），失败则使用 UTF-8
                    try:
                        stdout = result.stdout.decode('gbk', errors='replace').strip()
                    except:
                        stdout = result.stdout.decode('utf-8', errors='replace').strip()
                    
                    try:
                        stderr = result.stderr.decode('gbk', errors='replace').strip()
                    except:
                        stderr = result.stderr.decode('utf-8', errors='replace').strip()
                    
                    if stdout:
                        self._log_to_terminal(stdout)
                    if stderr:
                        self._log_to_terminal(stderr)
                        
                except Exception as e:
                    self._log_to_terminal(f"⚠ 终止 PID {pid} 时出错: {e}")
            
            self._log_to_terminal(f"✓ 已停止 Openclaw")
        else:
            self._log_to_terminal("未找到相关进程")
        
        self.process = None
        self.pid = None
        self.info_pid.configure(text="🆔 PID: -")
        self.try_count = 0
        self._set_state("STOPPED")
    
    def restart_openclaw(self, icon=None, item=None):
        """重启Openclaw"""
        if self.state != "RUNNING":
            return
        
        self._log_to_terminal("🔄 正在重启Openclaw...")
        self.try_count = 0  # 手动重启不计数
        self.manual_stop = False
        
        self.stop_openclaw()
        
        # 等待进程停止
        time.sleep(1)
        
        self._do_start(manual=True)
    
    def _toggle_window(self, icon=None, item=None):
        """切换窗口显示/隐藏"""
        if self.root.state() == 'withdrawn':
            self.root.deiconify()
            self.root.lift()
            self.root.focus_force()
        else:
            self.root.withdraw()
    
    def _on_close(self):
        """窗口关闭事件"""
        self.root.withdraw()  # 隐藏到托盘
    
    def _exit_app(self, icon=None, item=None):
        """退出程序"""
        self._log_to_terminal("[SYSTEM] Shutting down...")
        
        # 停止监控
        self.stop_monitor.set()
        
        # 停止热键监听
        self._unregister_hotkey()
        
        # 停止Openclaw
        if self.state in ["RUNNING", "STARTING"]:
            self.stop_openclaw()
        
        # 关闭托盘图标
        if self.tray_icon:
            self.tray_icon.stop()
        
        # 退出程序
        self.root.quit()
        self.root.destroy()


def is_admin():
    """检查是否以管理员权限运行"""
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except Exception:
        return False


def main():
    # 检查管理员权限（热键需要）
    if not is_admin():
        print("提示: 以管理员权限运行可获得全局热键支持")
    
    launcher = OpenClawLauncher()
    launcher.run()


if __name__ == "__main__":
    main()
