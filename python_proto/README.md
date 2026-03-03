# OpenClaw Launcher

Windows 11 专用 OpenClaw 启动器，基于 customtkinter 的现代化 GUI。

## 功能特性

- ✅ **Win11 现代风格**：圆角界面，跟随系统暗黑/亮色主题
- ✅ **无窗口执行**：启动 OpenClaw 时不弹出 CMD 黑窗口
- ✅ **ANSI 彩色输出**：终端区域原样显示 OpenClaw 的彩色输出
- ✅ **系统托盘**：三态图标（灰/黄/绿），仅托盘可退出
- ✅ **全局热键**：Ctrl+Shift+O 切换窗口显示/隐藏
- ✅ **PID 监控**：进程退出自动重启（最多2次重试）
- ✅ **自动发现**：自动查找 OpenClaw 路径、配置文件、端口
- ✅ **日志持久化**：启动时截断保留1万行

## 快速开始

### 1. 安装依赖

```bash
pip install -r requirements.txt
```

### 2. 运行启动器

```bash
python launcher.py
```

或打包为 exe：

```bash
python build.py
# 然后运行 dist/OpenClawLauncher.exe
```

## 使用方法

- **启动**：点击"启动"按钮或托盘菜单
- **停止**：点击"停止"按钮或托盘菜单
- **重启**：点击"重启"按钮或托盘菜单
- **最小化**：点击窗口最小化或关闭按钮 → 缩到托盘
- **退出**：右键托盘图标 → 退出
- **热键**：Ctrl+Shift+O 切换窗口显示/隐藏

## 配置文件

首次运行会自动创建 `config.json`：

```json
{
    "openclaw_path": "自动发现的路径",
    "gateway_port": 18789,
    "auto_start": false,
    "auto_minimize": false,
    "log_max_lines": 10000
}
```

修改配置后需重启启动器生效。

## 托盘图标状态

| 状态 | 图标颜色 | 说明 |
|------|---------|------|
| 已停止 | 🦞 灰色 | OpenClaw 未运行 |
| 启动中 | 🦞 黄色 | 正在启动（静态，不闪烁）|
| 运行中 | 🦞 绿色 | 启动成功，健康检查通过 |

## 系统要求

- Windows 11
- Python 3.8+ (如直接运行源码)

## 注意事项

1. **管理员权限**：全局热键需要管理员权限运行
2. **Win11 专用**：充分利用 Windows API，不保证其他系统兼容性
3. **自动重启**：仅 PID 监控触发的自动重启计入重试次数，手动操作不计数
