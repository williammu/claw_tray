# OpenClaw Launcher - 踩坑避雷与最佳实践

## 1. UI/动画相关

### 窗口最小化动画闪烁
- **问题**：最小化到托盘时窗口闪烁
- **原因**：hide/show 操作顺序不对
- **解决**：调整操作顺序，先计算动画位置再执行透明度变化

### 半透明窗口效果
- **问题**：需要实现终端区域的半透明效果
- **解决**：使用 `WS_EX_LAYERED` + `SetLayeredWindowAttributes` 设置整体透明度

### 动画帧率优化
- **问题**：动画看起来像幻灯片
- **解决**：提高帧率到 60fps，减少步数（从 15 步减到 8 步），缩短总时间
```cpp
int steps = 8;   // 原来是 15
int delay = 16;  // 约 60fps
```

---

## 2. 图标相关

### 托盘图标黑色边角
- **问题**：托盘图标的绿色圆点周围有黑色边角
- **原因**：GDI 绘图没有正确处理 alpha 通道
- **解决**：使用 DIB sections + `BITMAPV5HEADER` 直接操作像素的 alpha 通道
```cpp
BITMAPV5HEADER bi = {0};
bi.bV5Size = sizeof(BITMAPV5HEADER);
bi.bV5BitCount = 32;
bi.bV5Compression = BI_BITFIELDS;
bi.bV5AlphaMask = 0xFF000000;
bi.bV5RedMask = 0x00FF0000;
bi.bV5GreenMask = 0x0000FF00;
bi.bV5BlueMask = 0x000000FF;

// 为每个像素设置 alpha 值
for (int i = 0; i < width * height; i++) {
    DWORD* pixel = (DWORD*)bits + i;
    BYTE alpha = (*pixel & 0xFF000000) >> 24;
    if (alpha == 0 && (*pixel & 0x00FFFFFF) != 0) {
        *pixel |= 0xFF000000;  // 为非透明像素设置完全不透明
    }
}
```

### 标题栏图标不显示
- **问题**：自定义标题栏没有显示应用图标
- **原因**：`WS_POPUP` 窗口没有系统标题栏，需要手动绘制
- **解决**：在 `WM_PAINT` 中使用 `DrawIconEx` 手动绘制图标
```cpp
HICON hTitleIcon = LoadIconA(hInstance_, MAKEINTRESOURCEA(101));
if (hTitleIcon) {
    int iconSize = (int)(Scale(24) * 0.9);
    DrawIconEx(hdc, Scale(10), Scale(5), hTitleIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
    DestroyIcon(hTitleIcon);
}
```

---

## 3. Checkbox 状态问题

### Checkbox 点击状态不变化
- **问题**：`BM_GETCHECK` 始终返回相同值
- **原因**：`BS_OWNERDRAW` 样式会阻止 Windows 自动管理 checkbox 状态
- **解决**：使用 `GWLP_USERDATA` 手动存储和管理状态
```cpp
// 在 WM_COMMAND 处理中
case IDC_CHECKBOX:
    LONG_PTR userData = GetWindowLongPtr(hWndChk, GWLP_USERDATA);
    BOOL isChecked = (userData == 1);
    SetWindowLongPtr(hWndChk, GWLP_USERDATA, isChecked ? 0 : 1);
    InvalidateRect(hWndChk, NULL, TRUE);
    break;

// 在绘制代码中
LONG_PTR userData = GetWindowLongPtr(hWndChk, GWLP_USERDATA);
BOOL isChecked = (userData == 1);
```

### Checkbox 视觉反馈不明显
- **问题**：勾选和未勾选状态看不出区别
- **解决**：调整不同状态下的背景色和边框色
```cpp
if (isChecked) {
    hBrush = CreateSolidBrush(RGB(0, 120, 215));  // 蓝色表示勾选
} else {
    hBrush = CreateSolidBrush(RGB(60, 60, 60));   // 深灰色表示未勾选
}
```

---

## 4. 进程管理问题

### 重启显示 "already running"
- **问题**：点击重启时显示 "already running"，然后 "stopping it first"
- **原因**：`StopProcess` 是异步的，状态检查在进程结束前执行
- **解决**：将 `StopProcess` 改为同步，使用 `WaitForSingleObject` 等待进程结束
```cpp
bool StopProcess() {
    // 先尝试优雅关闭
    // ...
    
    if (CreateProcessA(...)) {
        WaitForSingleObject(pi.hProcess, 5000);  // 最多等待 5 秒
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    Sleep(100);  // 额外等待确保进程完全退出
    
    // 验证进程已退出
    std::vector<DWORD> pids = FindGatewayPids();
    return pids.empty();
}
```

### 按钮中间状态
- **问题**：点击检查按钮时没有视觉反馈
- **解决**：添加中间状态，按钮变灰并显示 "Checking..."
```cpp
void SetCheckButtonState(CheckButtonState state) {
    checkButtonState_ = state;
    EnableWindow(hCheckButton_, state == CheckButtonState::Normal ? TRUE : FALSE);
    SetWindowText(hCheckButton_, 
        state == CheckButtonState::Checking ? L"Checking..." : L"Check Status");
    InvalidateRect(hCheckButton_, NULL, TRUE);
}
```

---

## 5. 退出相关问题

### 退出时日志不显示
- **问题**：点击退出后主窗口显示但没有日志
- **原因**：`LogToTerminal` 中有 `if (isShuttingDown_) return;` 检查
- **解决**：移除该检查，允许在关闭过程中继续记录日志
```cpp
void LogToTerminal(const std::string& message) {
    // 已移除: if (isShuttingDown_) return;
    
    std::string timestamp = GetCurrentTimestamp();
    std::string logLine = "[" + timestamp + "] " + message;
    // ... 继续记录日志
}
```

### 退出时窗口卡死
- **问题**：点击退出后主窗口无响应
- **原因**：`ExitApp` 在 UI 线程中阻塞执行
- **解决**：将退出逻辑移到异步线程，使用 `WM_QUIT_APP` 消息通知主线程退出
```cpp
#define WM_QUIT_APP (WM_USER + 100)

void ExitApp() {
    std::thread([this]() {
        // 先停止进程
        processManager_.StopProcess();
        
        // 通知主线程退出
        PostMessage(hWnd_, WM_QUIT_APP, 0, 0);
    }).detach();
}

// 在 WndProc 中
case WM_QUIT_APP:
    DestroyWindow(hWnd_);
    break;
```

### 托盘退出不杀死 OpenClaw 进程
- **问题**：从托盘退出时没有一并杀死 OpenClaw 实例
- **解决**：在退出流程中调用 `StopProcess()`
```cpp
void OnTrayExit() {
    ShowWindow();
    LogToTerminal("正在退出应用程序...");
    processManager_.StopProcess();
    ExitApp();
}
```

---

## 6. 打包发布

### 安装程序方案选择
- **选择**：Inno Setup + 便携版 ZIP
- **原因**：开源免费、功能完善、GitHub 社区广泛使用

### 版本号管理
- **问题**：需要支持动态版本号
- **解决**：Inno Setup 使用 `/DMyAppVersion=xxx` 参数传递版本号
```batch
"%INNO_PATH%" "/DMyAppVersion=%APP_VERSION%" "installer\setup.iss"
```

---

## 7. 托盘菜单动态更新

### 菜单项文字不随状态变化
- **问题**：托盘菜单显示后，状态变化时菜单项文字不会自动更新（如"启动"应变为"停止"）
- **原因**：Windows 菜单是静态的，`TrackPopupMenu` 会阻塞直到菜单关闭
- **解决**：使用定时器在菜单显示期间定期更新菜单项
```cpp
constexpr int MENU_UPDATE_TIMER_ID = 9999;

void ShowContextMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_START_STOP, "启动");  // 初始文字
    
    SetTimer(hWnd, MENU_UPDATE_TIMER_ID, 200, NULL);  // 200ms 更新一次
    TrackPopupMenu(hMenu, ...);
    KillTimer(hWnd, MENU_UPDATE_TIMER_ID);
    DestroyMenu(hMenu);
}

// 在 WM_TIMER 中更新菜单
case WM_TIMER:
    if (wParam == MENU_UPDATE_TIMER_ID) {
        const char* text = (state == RUNNING) ? "停止" : "启动";
        ModifyMenu(hMenu, ID_TRAY_START_STOP, MF_STRING, ID_TRAY_START_STOP, text);
    }
    break;
```

---

## 8. RichEdit 控件问题

### 终端区域显示灰色而非深色背景
- **问题**：启动时终端区域显示灰色，调整窗口大小后正常显示深色背景
- **排查过程**：
  1. 最初以为是 `EM_SETBKGNDCOLOR` 设置问题
  2. 添加调试日志，发现 RichEdit 只创建一次，背景色设置成功
  3. 尝试设置红色背景测试，发现红色只出现在 RichEdit 下方
- **根本原因**：`hTerminalFrame_` (STATIC 控件，使用 `SS_BLACKRECT` 样式) 覆盖在 RichEdit 上面，但显示为灰色而非预期的黑色
- **解决**：移除多余的 STATIC 框架控件，让 RichEdit 直接显示，背景色由 RichEdit 自己控制
```cpp
// 错误做法：在 RichEdit 上方叠加 STATIC 控件
hTerminalFrame_ = CreateWindowExA(0, "STATIC", "",
    WS_CHILD | WS_VISIBLE | SS_BLACKRECT, ...);  // 这个控件会遮挡 RichEdit

// 正确做法：只创建 RichEdit，不叠加其他控件
hTerminal_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, ...);
```

### 启动按钮点击无响应
- **问题**：窗口上的启动按钮点击后没有任何反应
- **原因**：按钮使用的 ID (`ID_TRAY_START`) 与 WM_COMMAND 处理器中处理的 ID (`ID_TRAY_START_STOP`) 不匹配
- **解决**：确保所有按钮 ID 都在 WM_COMMAND 中正确处理
```cpp
// 窗口按钮
hStartBtn_ = CreateThemedButton(..., "[启动]", ID_TRAY_START);  // ID = 1006

// WM_COMMAND 处理
case ID_TRAY_START:       // 窗口按钮
    StartOpenClash();
    break;
case ID_TRAY_START_STOP:  // 托盘菜单（动态文字）
    if (currentState_ == State::STOPPED) StartOpenClash();
    else StopOpenClash();
    break;
```

### 字号不一致
- **问题**：RichEdit 中的文字看起来比上方标签的文字大
- **原因**：上方标签使用 `CreateFont(16, ...)` (16像素)，RichEdit 使用 `yHeight = 14 * 20` (14磅，约280 twips)
- **解决**：统一使用相同的字号单位
```cpp
// 标签字体
hFont_ = CreateFont(16, ...);  // 16 像素

// RichEdit 字体 - 需要使用 twips 单位 (1 point = 20 twips)
CHARFORMAT2W cf;
cf.yHeight = 16 * 20;  // 16 磅 = 320 twips，与标签一致
```

### 智能滚动 - 用户查看历史时不自动滚动
- **需求**：新日志自动滚动到底部，但用户滚动查看历史时不自动滚动
- **解决**：监听滚动事件，检测用户是否在底部
```cpp
bool autoScroll_ = true;  // 跟踪是否应该自动滚动

// 检测是否在底部
bool IsScrolledToBottom() {
    SCROLLINFO si = {sizeof(si), SIF_ALL};
    GetScrollInfo(hTerminal_, SB_VERT, &si);
    return (si.nPos + (int)si.nPage >= si.nMax - 1);
}

// 监听滚动事件
case WM_NOTIFY:
    if (((NMHDR*)lParam)->hwndFrom == hTerminal_ && 
        ((NMHDR*)lParam)->code == EN_VSCROLL) {
        autoScroll_ = IsScrolledToBottom();  // 更新自动滚动状态
    }
    break;

// 追加文本时智能滚动
void AppendText(const std::string& text) {
    bool wasAtBottom = IsScrolledToBottom();
    // ... 追加文本 ...
    if (wasAtBottom && autoScroll_) {
        SendMessage(hTerminal_, WM_VSCROLL, SB_BOTTOM, 0);
    }
}
```

### 滚动条样式优化
- **问题**：默认滚动条有白色槽，与深色主题不协调
- **解决**：使用 Flat Scroll Bar API 设置扁平滚动条
```cpp
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

InitializeFlatSB(hTerminal_);
FlatSB_SetScrollProp(hTerminal_, WSB_PROP_VSTYLE, FSB_ENCARTA_MODE, TRUE);
```

---

## 关键经验总结

| 问题类型 | 核心教训 |
|---------|---------|
| Win32 自定义控件 | `BS_OWNERDRAW` 需要完全手动管理状态 |
| 透明度/Alpha | GDI 不自动处理 alpha，需要用 DIB sections |
| 异步操作 | UI 操作不能阻塞主线程，用消息机制解耦 |
| 进程管理 | `CreateProcess` 后需要 `WaitForSingleObject` 确保同步 |
| 状态检查 | 进程退出后需要额外 `Sleep` 确保资源释放 |
| 控件重叠 | 多个控件重叠时，检查 Z-order 和创建顺序；STATIC 控件的 `SS_BLACKRECT` 可能不按预期显示 |
| 按钮ID管理 | 窗口按钮和托盘菜单使用不同的 ID，需要在 WM_COMMAND 中分别处理 |
| 菜单动态更新 | `TrackPopupMenu` 会阻塞，需要用定时器实现动态更新 |
| RichEdit 字号 | 使用 twips 单位 (1 point = 20 twips)，与其他控件的像素单位不同 |
| 调试方法 | 写日志文件比 OutputDebugString 更容易查看和追踪问题 |

---

## 最佳实践检查清单

### 修改代码前
- [ ] 理解现有代码模式和约定
- [ ] 检查是否已存在类似功能
- [ ] 考虑线程安全影响

### 使用 Win32 API 时
- [ ] 使用 `GWLP_USERDATA` 存储自定义控件状态
- [ ] 为 owner-drawn 控件处理 `WM_PAINT`
- [ ] 使用 DIB sections 正确处理 alpha 通道
- [ ] 始终清理 GDI 对象（画刷、画笔等）

### 使用 RichEdit 控件时
- [ ] 加载 `msftedit.dll` 库 (`LoadLibraryA("msftedit.dll")`)
- [ ] 使用 `MSFTEDIT_CLASS` 而非旧的 `RICHEDIT_CLASS`
- [ ] 字号使用 twips 单位 (`yHeight = points * 20`)
- [ ] 设置 `EM_SETEVENTMASK` 接收滚动等事件通知
- [ ] 避免在 RichEdit 上叠加其他控件

### 处理菜单时
- [ ] 托盘菜单使用 `SetForegroundWindow` 确保正确关闭
- [ ] 动态菜单使用定时器更新，菜单关闭后记得 `KillTimer`
- [ ] 确保菜单项 ID 与 WM_COMMAND 处理匹配

### 处理进程时
- [ ] 使用 `WaitForSingleObject` 实现同步进程操作
- [ ] 进程终止后添加小延迟等待清理完成
- [ ] 操作后验证进程状态

### 使用线程时
- [ ] 永远不要阻塞 UI 线程
- [ ] 使用 `PostMessage` 进行跨线程 UI 更新
- [ ] 分离应该独立运行的线程

### 打包发布时
- [ ] 在干净的机器上测试安装程序
- [ ] 包含所有必要的依赖项
- [ ] 同时提供安装版和便携版
- [ ] 测试卸载流程
