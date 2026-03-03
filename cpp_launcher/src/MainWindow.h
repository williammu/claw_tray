#pragma once
#include "Common.h"
#include "Config.h"
#include "Logger.h"
#include "AnsiParser.h"
#include "ProcessManager.h"
#include "TrayIcon.h"
#include "HotkeyManager.h"
#include "StartupManager.h"
#include "I18N.h"
#include <richedit.h>

namespace Launcher {

constexpr int MIN_WINDOW_WIDTH = 880;
constexpr int MIN_WINDOW_HEIGHT = 550;
constexpr int WM_LOG_MESSAGE = WM_USER + 200;
constexpr int WM_STATE_CHANGE = WM_USER + 100;
constexpr int WM_RESTART_REQUEST = WM_USER + 101;
constexpr int WM_APPEND_TEXT = WM_USER + 202;
constexpr int WM_SET_PID_TEXT = WM_USER + 203;
constexpr int WM_CHECK_COMPLETE = WM_USER + 204;
constexpr int WM_QUIT_APP = WM_USER + 205;

struct LogMessageData {
    std::string message;
    std::string color;
    bool isStderr;
};

class MainWindow {
public:
    static MainWindow& Instance() {
        static MainWindow instance;
        return instance;
    }

    bool Create(HINSTANCE hInstance) {
        hInstance_ = hInstance;
        
        dpi_ = GetDpiForSystem();
        if (dpi_ == 0) dpi_ = 96;
        
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(101));
        wc.hIconSm = (HICON)LoadImageA(hInstance, MAKEINTRESOURCEA(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(12, 12, 12));
        wc.lpszClassName = "OpenClawLauncherClass";
        
        RegisterClassExA(&wc);
        
        hWnd_ = CreateWindowExA(
            WS_EX_COMPOSITED | WS_EX_LAYERED,
            "OpenClawLauncherClass",
            "OpenClaw Launcher",
            WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            CW_USEDEFAULT, CW_USEDEFAULT,
            Scale(900), Scale(750),
            NULL, NULL, hInstance, this
        );
        
        if (!hWnd_) return false;
        
        HICON hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(101));
        SendMessage(hWnd_, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hWnd_, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        
        SetLayeredWindowAttributes(hWnd_, RGB(0, 0, 0), 230, LWA_COLORKEY | LWA_ALPHA);
        
        MARGINS margins = { 1, 1, 1, 1 };
        DwmExtendFrameIntoClientArea(hWnd_, &margins);
        
        CreateControls();
        
        BOOL checked = Config::Instance().windowsStartup || StartupManager::Instance().IsTaskExists();
        CheckDlgButton(hWnd_, 1008, checked ? BST_CHECKED : BST_UNCHECKED);
        
        return true;
    }

    void Show() {
        Logger::Instance().Log("Show() called", "DEBUG");
        
        RECT rcClient;
        GetClientRect(hWnd_, &rcClient);
        int width = rcClient.right - rcClient.left;
        int height = rcClient.bottom - rcClient.top;
        
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int x = (screenWidth - width) / 2;
        int y = (screenHeight - height) / 2;
        
        SetWindowPos(hWnd_, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        ShowWindow(hWnd_, SW_SHOW);
        
        SetForegroundWindow(hWnd_);
        SetFocus(hWnd_);
        BringWindowToTop(hWnd_);
        
        SetWindowPos(hWnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(hWnd_, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        
        UpdateWindow(hWnd_);
        Logger::Instance().Log("Show() completed", "DEBUG");
    }

    void Hide() {
        Logger::Instance().Log("Hide() called", "DEBUG");
        ShowWindow(hWnd_, SW_HIDE);
    }

    void Toggle() {
        Logger::Instance().Log("Toggle() called, IsWindowVisible=" + std::to_string(IsWindowVisible(hWnd_)), "DEBUG");
        if (IsWindowVisible(hWnd_)) {
            Hide();
        } else {
            Show();
        }
    }

    HWND GetHwnd() const { return hWnd_; }

    void SetState(State state) {
        currentState_ = state;
        TrayIcon::Instance().SetCurrentState(state);
        UpdateUI();
    }

    State GetState() const { return currentState_; }

    void LogToTerminal(const std::string& message, const std::string& color = "default") {
        Logger::Instance().Log(message);
        
        std::string* text = new std::string(message + "\r\n");
        PostMessage(hWnd_, WM_APPEND_TEXT, (WPARAM)text, 0);
    }

    void LogFromThread(const std::string& message, const std::string& color = "default", bool isStderr = false) {
        LogMessageData* data = new LogMessageData{message, color, isStderr};
        PostMessage(hWnd_, WM_LOG_MESSAGE, (WPARAM)data, 0);
    }

    bool IsScrolledToBottom() {
        SCROLLINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hTerminal_, SB_VERT, &si);
        return (si.nPos + (int)si.nPage >= si.nMax - 1);
    }

    void ScrollToBottom() {
        SendMessage(hTerminal_, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void AppendColoredText(const std::string& text) {
        AnsiParser::Instance().Reset();
        auto segments = AnsiParser::Instance().Parse(text);
        
        for (const auto& seg : segments) {
            AppendTextWithColor(seg.text, AnsiParser::Instance().GetColor(seg.colorName));
        }
    }

    void AppendTextWithColor(const std::string& text, COLORREF color) {
        if (text.empty()) return;
        
        GETTEXTLENGTHEX gtl;
        ZeroMemory(&gtl, sizeof(gtl));
        gtl.flags = GTL_DEFAULT;
        gtl.codepage = 1200;
        int len = (int)SendMessage(hTerminal_, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
        
        SendMessage(hTerminal_, EM_SETSEL, len, len);
        
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        SendMessage(hTerminal_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
        std::wstring wtext(wlen - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], wlen);
        
        SendMessageW(hTerminal_, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str());
    }

    void AutoDiscover() {
        LogToTerminal("");
        LogToTerminal("========================================");
        LogToTerminal("*  OpenClaw Gateway Launcher v" + std::string(VERSION) + "  *");
        LogToTerminal("========================================");
        LogToTerminal("");
        LogToTerminal("[SYSTEM] Starting auto-discovery sequence...");
        
        LogToTerminal("");
        LogToTerminal("[INIT] Scanning for OpenClaw executable...");
        
        std::string openclawPath = ProcessManager::Instance().FindOpenclawExecutable();
        if (!openclawPath.empty()) {
            LogToTerminal("  [OK] Found: " + openclawPath, "green");
            Config::Instance().openclawPath = openclawPath;
            SetWindowTextA(hInfoPath_, ("> OpenClaw: " + openclawPath).c_str());
        } else {
            LogToTerminal("  [ERR] Not found", "red");
        }
        
        LogToTerminal("");
        LogToTerminal("[INIT] Searching for config file...");
        
        std::string configPath = ProcessManager::Instance().FindConfigFile();
        if (!configPath.empty()) {
            LogToTerminal("  [OK] Found: " + configPath, "green");
            Config::Instance().openclawConfigPath = configPath;
            SetWindowTextA(hInfoConfig_, ("> Config: " + configPath).c_str());
            
            int port = ProcessManager::Instance().ReadGatewayPort(configPath);
            Config::Instance().gatewayPort = port;
            LogToTerminal("  [OK] Port: " + std::to_string(port));
        } else {
            LogToTerminal("  [WARN] Config not found, using defaults", "yellow");
        }
        
        std::string healthUrl = "http://127.0.0.1:" + std::to_string(Config::Instance().gatewayPort) + "/";
        SetWindowTextA(hInfoHealth_, ("> Health: " + healthUrl).c_str());
        
        SetWindowTextA(hInfoHotkey_, ("> HOTKEY: " + Config::Instance().windowHotkey).c_str());
        
        LogToTerminal("");
        LogToTerminal("[INIT] Saving configuration...");
        Config::Instance().Save();
        LogToTerminal("  [OK] Config saved to " + Config::Instance().GetConfigPath(), "green");
        
        LogToTerminal("");
        LogToTerminal("========================================");
        LogToTerminal("[SYSTEM] Initialization complete");
        LogToTerminal("========================================");
    }

    void CheckAlreadyRunning() {
        LogToTerminal("");
        LogToTerminal("Checking if already running...");
        
        std::thread([this]() {
            auto pids = ProcessManager::Instance().FindGatewayPids();
            if (!pids.empty()) {
                LogFromThread("Found process (PID: " + std::to_string(pids[0]) + "), checking health...");
                
                if (ProcessManager::Instance().HealthCheck()) {
                    LogFromThread("OpenClaw already running and healthy", "green");
                    PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
                } else {
                    LogFromThread("Process found but health check failed, stopping old process...");
                    ProcessManager::Instance().StopProcess();
                    Sleep(1000);
                    LogFromThread("Starting new instance...");
                    DoStartWithLog();
                }
            } else {
                LogFromThread("OpenClaw not running, auto-starting...");
                DoStartWithLog();
            }
        }).detach();
    }

    void DoStartWithLog() {
        PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STARTING, 0);
        
        LogFromThread("Starting OpenClaw...");
        
        if (!ProcessManager::Instance().StartProcess()) {
            LogFromThread("Failed to start OpenClaw");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STOPPED, 0);
            return;
        }
        
        LogFromThread("Waiting for startup (monitoring output)...");
        
        if (ProcessManager::Instance().WaitForReady(10000)) {
            LogFromThread("OpenClaw started successfully");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
            return;
        }
        
        LogFromThread("Startup timeout, checking process status...");
        
        auto pids = ProcessManager::Instance().FindGatewayPids();
        if (!pids.empty()) {
            LogFromThread("Process found (PID: " + std::to_string(pids[0]) + "), assuming success");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
        } else {
            LogFromThread("Process not found, startup failed");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STOPPED, 0);
        }
    }

    void AnimateToTray(bool isMinimize) {
        Logger::Instance().Log("=== AnimateToTray START ===", "DEBUG");
        Logger::Instance().Log("isMinimize=" + std::to_string(isMinimize), "DEBUG");
        
        Logger::Instance().Log("Step 1: Getting window rect", "DEBUG");
        RECT rcWindow;
        GetWindowRect(hWnd_, &rcWindow);
        
        int windowWidth = rcWindow.right - rcWindow.left;
        int windowHeight = rcWindow.bottom - rcWindow.top;
        int windowCenterX = rcWindow.left + windowWidth / 2;
        int windowCenterY = rcWindow.top + windowHeight / 2;
        
        Logger::Instance().Log("Window rect: left=" + std::to_string(rcWindow.left) + 
            " top=" + std::to_string(rcWindow.top) + 
            " width=" + std::to_string(windowWidth) + 
            " height=" + std::to_string(windowHeight), "DEBUG");
        
        Logger::Instance().Log("Step 2: Getting tray icon position", "DEBUG");
        NOTIFYICONIDENTIFIER nii = {0};
        nii.cbSize = sizeof(nii);
        nii.hWnd = hWnd_;
        nii.uID = 1;
        
        RECT trayRect = {0};
        HRESULT hr = Shell_NotifyIconGetRect(&nii, &trayRect);
        if (FAILED(hr)) {
            Logger::Instance().Log("Shell_NotifyIconGetRect failed with hr=" + std::to_string(hr) + ", using fallback", "DEBUG");
            trayRect.left = GetSystemMetrics(SM_CXSCREEN) - Scale(50);
            trayRect.top = GetSystemMetrics(SM_CYSCREEN) - Scale(50);
            trayRect.right = trayRect.left + Scale(32);
            trayRect.bottom = trayRect.top + Scale(32);
        } else {
            Logger::Instance().Log("Tray rect: left=" + std::to_string(trayRect.left) + 
                " top=" + std::to_string(trayRect.top), "DEBUG");
        }
        
        int trayCenterX = trayRect.left + (trayRect.right - trayRect.left) / 2;
        int trayCenterY = trayRect.top + (trayRect.bottom - trayRect.top) / 2;
        
        Logger::Instance().Log("Step 3: Creating DC and bitmap", "DEBUG");
        HDC hdcScreen = GetDC(NULL);
        Logger::Instance().Log("hdcScreen=" + std::to_string((uintptr_t)hdcScreen), "DEBUG");
        
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, windowWidth, windowHeight);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);
        
        Logger::Instance().Log("Step 4: Capturing window screenshot with PrintWindow", "DEBUG");
        BOOL pwResult = PrintWindow(hWnd_, hdcMem, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
        Logger::Instance().Log("PrintWindow result=" + std::to_string(pwResult), "DEBUG");
        
        Logger::Instance().Log("Step 5: Registering window class", "DEBUG");
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = hInstance_;
        wc.lpszClassName = "AnimateWindowClass";
        ATOM classAtom = RegisterClassExA(&wc);
        Logger::Instance().Log("RegisterClassExA result=" + std::to_string(classAtom), "DEBUG");
        
        Logger::Instance().Log("Step 6: Creating animation window (NOT visible yet)", "DEBUG");
        HWND hAnimWnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            "AnimateWindowClass",
            NULL,
            WS_POPUP,
            rcWindow.left, rcWindow.top, windowWidth, windowHeight,
            NULL, NULL, hInstance_, NULL
        );
        
        if (!hAnimWnd) {
            DWORD err = GetLastError();
            Logger::Instance().Log("FAILED to create animation window, error=" + std::to_string(err), "DEBUG");
        } else {
            Logger::Instance().Log("Animation window created successfully, hAnimWnd=" + std::to_string((uintptr_t)hAnimWnd), "DEBUG");
            Logger::Instance().Log("IsWindowVisible(hAnimWnd)=" + std::to_string(IsWindowVisible(hAnimWnd)), "DEBUG");
        }
        
        if (hAnimWnd) {
            Logger::Instance().Log("Step 8: Setting initial frame content BEFORE hiding main window", "DEBUG");
            
            BLENDFUNCTION bf = {0};
            bf.BlendOp = AC_SRC_OVER;
            bf.SourceConstantAlpha = 255;
            bf.AlphaFormat = 0;
            
            POINT ptSrc = {0, 0};
            SIZE size = {windowWidth, windowHeight};
            POINT ptDst = {rcWindow.left, rcWindow.top};
            
            BOOL ulwResult = UpdateLayeredWindow(hAnimWnd, hdcScreen, &ptDst, &size, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
            Logger::Instance().Log("UpdateLayeredWindow (initial frame) result=" + std::to_string(ulwResult), "DEBUG");
            
            Logger::Instance().Log("Step 9: ATOMIC HIDE main window + SHOW animation window", "DEBUG");
            
            SetWindowPos(
                hAnimWnd,
                hWnd_,
                rcWindow.left, rcWindow.top, windowWidth, windowHeight,
                SWP_SHOWWINDOW | SWP_NOACTIVATE
            );
            
            SetWindowPos(
                hWnd_,
                NULL,
                0, 0, 0, 0,
                SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
            );
            
            Logger::Instance().Log("IsWindowVisible(hWnd_)=" + std::to_string(IsWindowVisible(hWnd_)), "DEBUG");
            Logger::Instance().Log("IsWindowVisible(hAnimWnd)=" + std::to_string(IsWindowVisible(hAnimWnd)), "DEBUG");
        } else {
            Logger::Instance().Log("No animation window, just hiding main window", "DEBUG");
            ShowWindow(hWnd_, SW_HIDE);
        }
        
        Logger::Instance().Log("Step 10: Starting animation loop", "DEBUG");
        int steps = 8;
        int delay = 16;
        
        Logger::Instance().Log("Starting animation loop with " + std::to_string(steps) + " steps", "DEBUG");
        
        for (int i = 0; i <= steps; i++) {
            float progress = (float)i / steps;
            float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
            
            int currentWidth = (int)(windowWidth * (1.0f - eased * 0.9f));
            int currentHeight = (int)(windowHeight * (1.0f - eased * 0.9f));
            if (currentWidth < 1) currentWidth = 1;
            if (currentHeight < 1) currentHeight = 1;
            
            int currentX = windowCenterX - currentWidth / 2 + (int)((trayCenterX - windowCenterX) * eased);
            int currentY = windowCenterY - currentHeight / 2 + (int)((trayCenterY - windowCenterY) * eased);
            
            HDC hdcStep = CreateCompatibleDC(hdcScreen);
            HBITMAP hBmpStep = CreateCompatibleBitmap(hdcScreen, currentWidth, currentHeight);
            HBITMAP hOldBmpStep = (HBITMAP)SelectObject(hdcStep, hBmpStep);
            
            SetStretchBltMode(hdcStep, HALFTONE);
            StretchBlt(hdcStep, 0, 0, currentWidth, currentHeight, hdcMem, 0, 0, windowWidth, windowHeight, SRCCOPY);
            
            BLENDFUNCTION bf = {0};
            bf.BlendOp = AC_SRC_OVER;
            bf.SourceConstantAlpha = (BYTE)(255 * (1.0f - eased * 0.8f));
            bf.AlphaFormat = 0;
            
            POINT ptSrc = {0, 0};
            SIZE size = {currentWidth, currentHeight};
            POINT ptDst = {currentX, currentY};
            
            BOOL ulwResult = UpdateLayeredWindow(hAnimWnd, hdcScreen, &ptDst, &size, hdcStep, &ptSrc, 0, &bf, ULW_ALPHA);
            if (!ulwResult) {
                Logger::Instance().Log("UpdateLayeredWindow failed at step " + std::to_string(i) + ", error=" + std::to_string(GetLastError()), "DEBUG");
            }
            
            SelectObject(hdcStep, hOldBmpStep);
            DeleteObject(hBmpStep);
            DeleteDC(hdcStep);
            
            Sleep(delay);
        }
        
        Logger::Instance().Log("Animation loop completed", "DEBUG");
        
        DestroyWindow(hAnimWnd);
        
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        Logger::Instance().Log("AnimateToTray END", "DEBUG");
    }

private:
    HINSTANCE hInstance_ = NULL;
    HWND hWnd_ = NULL;
    HWND hTerminal_ = NULL;
    HWND hStatus_ = NULL;
    HWND hStartBtn_ = NULL;
    HWND hStopBtn_ = NULL;
    HWND hRestartBtn_ = NULL;
    HWND hCheckBtn_ = NULL;
    HWND hClearBtn_ = NULL;
    HWND hHotkeyBtn_ = NULL;
    HWND hStartupCheckbox_ = NULL;
    HWND hInfoPath_ = NULL;
    HWND hInfoConfig_ = NULL;
    HWND hInfoHealth_ = NULL;
    HWND hInfoHotkey_ = NULL;
    HWND hInfoPid_ = NULL;
    
    State currentState_ = State::STOPPED;
    int tryCount_ = 0;
    bool manualStop_ = false;
    bool capturingHotkey_ = false;
    bool autoScroll_ = true;
    bool closeBtnPressed_ = false;
    bool minBtnPressed_ = false;
    bool closeBtnHovered_ = false;
    bool minBtnHovered_ = false;
    std::atomic<bool> stopMonitor_{false};
    std::atomic<bool> isShuttingDown_{false};
    std::atomic<bool> isMovingOrSizing_{false};
    std::thread monitorThread_;
    std::mutex logMutex_;
    std::vector<std::string*> pendingTexts_;
    
    HFONT hFont_ = NULL;
    HFONT hBoldFont_ = NULL;
    HFONT hStatusFont_ = NULL;
    HFONT hTerminalFont_ = NULL;
    HBRUSH hBlackBrush_ = NULL;
    HBRUSH hDarkBrush_ = NULL;
    HBRUSH hFrameBrush_ = NULL;
    HBRUSH hStatusBrush_ = NULL;
    HPEN hGreenPen_ = NULL;
    HPEN hDarkGreenPen_ = NULL;
    HPEN hBorderPen_ = NULL;
    HPEN hBorderHighlightPen_ = NULL;
    UINT dpi_ = 96;
    
    int Scale(int value) const {
        return MulDiv(value, dpi_, 96);
    }

    MainWindow() {
        I18N::Instance().Initialize();
        hBlackBrush_ = CreateSolidBrush(RGB(12, 12, 12));
        hDarkBrush_ = CreateSolidBrush(RGB(10, 10, 10));
        hFrameBrush_ = CreateSolidBrush(RGB(8, 8, 8));
        hStatusBrush_ = CreateSolidBrush(RGB(12, 12, 12));
        hGreenPen_ = CreatePen(PS_SOLID, 1, RGB(0, 143, 17));
        hDarkGreenPen_ = CreatePen(PS_SOLID, 1, RGB(0, 51, 0));
        hBorderPen_ = CreatePen(PS_SOLID, 1, RGB(0, 80, 0));
        hBorderHighlightPen_ = CreatePen(PS_SOLID, 1, RGB(0, 120, 0));
    }

    ~MainWindow() {
        if (hFont_) DeleteObject(hFont_);
        if (hBoldFont_) DeleteObject(hBoldFont_);
        if (hStatusFont_) DeleteObject(hStatusFont_);
        if (hTerminalFont_) DeleteObject(hTerminalFont_);
        if (hBlackBrush_) DeleteObject(hBlackBrush_);
        if (hDarkBrush_) DeleteObject(hDarkBrush_);
        if (hFrameBrush_) DeleteObject(hFrameBrush_);
        if (hStatusBrush_) DeleteObject(hStatusBrush_);
        if (hGreenPen_) DeleteObject(hGreenPen_);
        if (hDarkGreenPen_) DeleteObject(hDarkGreenPen_);
        if (hBorderPen_) DeleteObject(hBorderPen_);
        if (hBorderHighlightPen_) DeleteObject(hBorderHighlightPen_);
        if (hGreenPen_) DeleteObject(hGreenPen_);
        if (hDarkGreenPen_) DeleteObject(hDarkGreenPen_);
    }

    void CreateControls() {
        int fontHeight = Scale(14);
        int fontHeightStatus = Scale(20);
        int fontHeightTerminal = Scale(9);
        
        hFont_ = CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        
        hBoldFont_ = CreateFont(fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        
        hStatusFont_ = CreateFont(fontHeightStatus, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Microsoft YaHei UI");
        
        hTerminalFont_ = CreateFont(fontHeightTerminal, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        
        RECT rcClient;
        GetClientRect(hWnd_, &rcClient);
        int width = rcClient.right;
        int height = rcClient.bottom;
        
        int titleBarHeight = Scale(32);
        
        int termHeight = height - titleBarHeight - Scale(195);
        if (termHeight < Scale(100)) termHeight = Scale(100);
        
        int infoY = titleBarHeight + Scale(14);
        int infoH = Scale(22);
        
        hInfoPath_ = CreateWindowExA(0, "STATIC", "> OpenClaw: [Not configured]",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), infoY, width - Scale(30), infoH, hWnd_, NULL, hInstance_, NULL);
        SendMessage(hInfoPath_, WM_SETFONT, (WPARAM)hFont_, TRUE);
        SetWindowLongPtr(hInfoPath_, GWLP_USERDATA, 1);
        
        infoY += infoH;
        hInfoConfig_ = CreateWindowExA(0, "STATIC", "> Config: [Not found]",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), infoY, width - Scale(30), infoH, hWnd_, NULL, hInstance_, NULL);
        SendMessage(hInfoConfig_, WM_SETFONT, (WPARAM)hFont_, TRUE);
        SetWindowLongPtr(hInfoConfig_, GWLP_USERDATA, 2);
        
        infoY += infoH;
        hInfoHealth_ = CreateWindowExA(0, "STATIC", "> Health: http://127.0.0.1:18789/",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), infoY, width - Scale(30), infoH, hWnd_, NULL, hInstance_, NULL);
        SendMessage(hInfoHealth_, WM_SETFONT, (WPARAM)hFont_, TRUE);
        SetWindowLongPtr(hInfoHealth_, GWLP_USERDATA, 2);
        
        infoY += infoH;
        hInfoHotkey_ = CreateWindowExA(0, "STATIC", "> HOTKEY: ctrl+shift+o",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), infoY, width - Scale(30), infoH, hWnd_, NULL, hInstance_, NULL);
        SendMessage(hInfoHotkey_, WM_SETFONT, (WPARAM)hFont_, TRUE);
        SetWindowLongPtr(hInfoHotkey_, GWLP_USERDATA, 2);
        
        infoY += infoH;
        hInfoPid_ = CreateWindowExA(0, "STATIC", "> PID: -",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            Scale(15), infoY, width - Scale(30), infoH, hWnd_, NULL, hInstance_, NULL);
        SendMessage(hInfoPid_, WM_SETFONT, (WPARAM)hFont_, TRUE);
        SetWindowLongPtr(hInfoPid_, GWLP_USERDATA, 3);
        
        int btnY = titleBarHeight + Scale(142);
        int btnW = Scale(80);
        int btnH = Scale(30);
        int btnX = Scale(15);
        
        hStartBtn_ = CreateThemedButton(btnX, btnY, btnW, btnH, TR("btn_start").c_str(), ID_TRAY_START);
        btnX += btnW + Scale(8);
        
        hStopBtn_ = CreateThemedButton(btnX, btnY, btnW, btnH, TR("btn_stop").c_str(), ID_TRAY_STOP);
        EnableWindow(hStopBtn_, FALSE);
        btnX += btnW + Scale(8);
        
        hRestartBtn_ = CreateThemedButton(btnX, btnY, btnW, btnH, TR("btn_restart").c_str(), ID_TRAY_RESTART);
        EnableWindow(hRestartBtn_, FALSE);
        btnX += btnW + Scale(8);
        
        hCheckBtn_ = CreateThemedButton(btnX, btnY, btnW, btnH, TR("btn_check").c_str(), 1007);
        
        hStatus_ = CreateWindowExA(0, "STATIC", TR("status_offline").c_str(),
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            (width - Scale(120)) / 2, btnY + Scale(4), Scale(120), Scale(24), hWnd_, NULL, hInstance_, NULL);
        SendMessage(hStatus_, WM_SETFONT, (WPARAM)hStatusFont_, TRUE);
        SetWindowLongPtr(hStatus_, GWLP_USERDATA, 4);
        
        hClearBtn_ = CreateThemedButton(width - Scale(95), btnY, btnW, btnH, TR("btn_clear").c_str(), ID_TRAY_CLEAR);
        
        hHotkeyBtn_ = CreateThemedButton(width - Scale(183), btnY, btnW, btnH, TR("btn_set_hotkey").c_str(), 1009);
        
        hStartupCheckbox_ = CreateWindowExA(0, "BUTTON", TR("btn_startup").c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW | WS_TABSTOP,
            width - Scale(270), btnY + Scale(6), Scale(80), Scale(22), hWnd_, (HMENU)1008, hInstance_, NULL);
        SendMessage(hStartupCheckbox_, WM_SETFONT, (WPARAM)hFont_, TRUE);
        SetWindowLongPtr(hStartupCheckbox_, GWLP_USERDATA, Config::Instance().windowsStartup ? 1 : 0);
        
        hTerminal_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            Scale(15), titleBarHeight + Scale(180), width - Scale(30), termHeight, hWnd_, NULL, hInstance_, NULL);
        
        InitializeFlatSB(hTerminal_);
        FlatSB_SetScrollProp(hTerminal_, WSB_PROP_VSTYLE, FSB_ENCARTA_MODE, TRUE);
        
        SendMessage(hTerminal_, WM_SETFONT, (WPARAM)hTerminalFont_, TRUE);
        
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = RGB(0, 255, 0);
        cf.yHeight = fontHeightTerminal * 15;
        wcscpy_s(cf.szFaceName, LF_FACESIZE, L"Consolas");
        SendMessage(hTerminal_, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
        
        SendMessage(hTerminal_, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));
        SendMessage(hTerminal_, EM_SETEVENTMASK, 0, ENM_SCROLL);
        
        SendMessageW(hTerminal_, EM_REPLACESEL, FALSE, (LPARAM)L" ");
        SendMessage(hTerminal_, EM_SETSEL, 0, 1);
        SendMessageW(hTerminal_, EM_REPLACESEL, FALSE, (LPARAM)L"");
        
        ShowWindow(hTerminal_, SW_SHOW);
        SetWindowPos(hTerminal_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        InvalidateRect(hTerminal_, NULL, TRUE);
        UpdateWindow(hTerminal_);
        RedrawWindow(hTerminal_, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        
        ProcessManager::Instance().Init([this](const std::string& msg, const std::string& color, bool isStderr) {
            LogFromThread(msg, color);
        });
    }

    HWND CreateThemedButton(int x, int y, int w, int h, const char* text, int id) {
        HWND hWndBtn = CreateWindowExA(0, "BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            x, y, w, h, hWnd_, (HMENU)(UINT_PTR)id, hInstance_, NULL);
        SendMessage(hWndBtn, WM_SETFONT, (WPARAM)hBoldFont_, TRUE);
        SetWindowLongPtr(hWndBtn, GWLP_USERDATA, 10);
        return hWndBtn;
    }
    
    void DrawRoundedRect(HDC hdc, RECT* rc, int radius) {
        HGDIOBJ hOldPen = SelectObject(hdc, hBorderPen_);
        HGDIOBJ hOldBrush = SelectObject(hdc, hFrameBrush_);
        
        RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
        
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
    }
    
    void DrawButton(LPDRAWITEMSTRUCT lpDIS) {
        HWND hWndBtn = lpDIS->hwndItem;
        RECT rc = lpDIS->rcItem;
        HDC hdc = lpDIS->hDC;
        
        char text[64] = {0};
        GetWindowTextA(hWndBtn, text, sizeof(text));
        
        BOOL isEnabled = IsWindowEnabled(hWndBtn);
        BOOL isPressed = (lpDIS->itemState & ODS_SELECTED);
        BOOL isFocused = (lpDIS->itemState & ODS_FOCUS);
        
        COLORREF bgColor = isEnabled ? (isPressed ? RGB(0, 40, 0) : RGB(0, 25, 0)) : RGB(20, 20, 20);
        COLORREF borderColor = isEnabled ? (isPressed ? RGB(0, 150, 0) : RGB(0, 100, 0)) : RGB(40, 40, 40);
        COLORREF textColor = isEnabled ? RGB(0, 255, 0) : RGB(80, 80, 80);
        COLORREF highlightColor = isEnabled ? RGB(0, 180, 0) : RGB(60, 60, 60);
        
        HPEN hBorderPen = CreatePen(PS_SOLID, Scale(1), borderColor);
        HPEN hHighlightPen = CreatePen(PS_SOLID, Scale(1), highlightColor);
        HBRUSH hBgBrush = CreateSolidBrush(bgColor);
        
        int radius = Scale(6);
        int inset = Scale(1);
        
        HGDIOBJ hOldPen = SelectObject(hdc, hBorderPen);
        HGDIOBJ hOldBrush = SelectObject(hdc, hBgBrush);
        
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
        
        if (isEnabled && !isPressed) {
            SelectObject(hdc, hHighlightPen);
            Arc(hdc, rc.left + inset, rc.top + inset, 
                rc.left + radius * 2 - inset, rc.top + radius * 2 - inset,
                rc.left + radius, rc.top + inset, rc.left + inset, rc.top + radius);
            Arc(hdc, rc.right - radius * 2 + inset, rc.top + inset,
                rc.right - inset, rc.top + radius * 2 - inset,
                rc.right - inset, rc.top + radius, rc.right - radius + inset, rc.top + inset);
        }
        
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        
        DeleteObject(hBorderPen);
        DeleteObject(hHighlightPen);
        DeleteObject(hBgBrush);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        
        HGDIOBJ hOldFont = SelectObject(hdc, hBoldFont_);
        
        SIZE textSize;
        GetTextExtentPoint32A(hdc, text, (int)strlen(text), &textSize);
        int textX = rc.left + (rc.right - rc.left - textSize.cx) / 2;
        int textY = rc.top + (rc.bottom - rc.top - textSize.cy) / 2;
        
        if (isPressed) {
            textX += Scale(1);
            textY += Scale(1);
        }
        
        TextOutA(hdc, textX, textY, text, (int)strlen(text));
        
        SelectObject(hdc, hOldFont);
    }
    
    void DrawCheckbox(LPDRAWITEMSTRUCT lpDIS) {
        HWND hWndChk = lpDIS->hwndItem;
        RECT rc = lpDIS->rcItem;
        HDC hdc = lpDIS->hDC;
        
        char text[64] = {0};
        GetWindowTextA(hWndChk, text, sizeof(text));
        
        LONG_PTR userData = GetWindowLongPtr(hWndChk, GWLP_USERDATA);
        BOOL isChecked = (userData == 1);
        BOOL isEnabled = IsWindowEnabled(hWndChk);
        
        Logger::Instance().Log("DrawCheckbox: text='" + std::string(text) + 
            "' itemState=" + std::to_string(lpDIS->itemState) + 
            " GWLP_USERDATA=" + std::to_string(userData) +
            " isChecked=" + std::to_string(isChecked) + 
            " isEnabled=" + std::to_string(isEnabled), "DEBUG");
        
        {
            RECT bgRect = rc;
            HBRUSH hBgBrush = CreateSolidBrush(RGB(10, 10, 10));
            FillRect(hdc, &bgRect, hBgBrush);
            DeleteObject(hBgBrush);
        }
        
        int boxSize = Scale(16);
        int boxX = rc.left;
        int boxY = rc.top + (rc.bottom - rc.top - boxSize) / 2;
        
        COLORREF borderColor = isEnabled ? (isChecked ? RGB(0, 255, 0) : RGB(0, 150, 0)) : RGB(60, 60, 60);
        COLORREF fillColor = isChecked ? RGB(0, 60, 0) : RGB(15, 15, 15);
        COLORREF textColor = isEnabled ? (isChecked ? RGB(0, 255, 0) : RGB(0, 200, 0)) : RGB(100, 100, 100);
        COLORREF checkColor = RGB(0, 255, 0);
        
        Logger::Instance().Log("DrawCheckbox colors: borderColor=" + 
            std::to_string(GetRValue(borderColor)) + "," + std::to_string(GetGValue(borderColor)) + "," + std::to_string(GetBValue(borderColor)) +
            " fillColor=" + std::to_string(GetRValue(fillColor)) + "," + std::to_string(GetGValue(fillColor)) + "," + std::to_string(GetBValue(fillColor)), "DEBUG");
        
        HPEN hBorderPen = CreatePen(PS_SOLID, Scale(2), borderColor);
        HBRUSH hFillBrush = CreateSolidBrush(fillColor);
        
        HGDIOBJ hOldPen = SelectObject(hdc, hBorderPen);
        HGDIOBJ hOldBrush = SelectObject(hdc, hFillBrush);
        
        Rectangle(hdc, boxX, boxY, boxX + boxSize, boxY + boxSize);
        
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hBorderPen);
        DeleteObject(hFillBrush);
        
        if (isChecked) {
            Logger::Instance().Log("DrawCheckbox: Drawing check mark at boxX=" + std::to_string(boxX) + 
                " boxY=" + std::to_string(boxY) + " boxSize=" + std::to_string(boxSize), "DEBUG");
            
            HPEN hCheckPen = CreatePen(PS_SOLID, Scale(3), checkColor);
            hOldPen = SelectObject(hdc, hCheckPen);
            
            int cx = boxX + boxSize / 2;
            int cy = boxY + boxSize / 2;
            int offset = boxSize / 4;
            
            MoveToEx(hdc, cx - offset, cy, NULL);
            LineTo(hdc, cx - Scale(1), cy + offset);
            LineTo(hdc, cx + offset + Scale(2), cy - offset);
            
            SelectObject(hdc, hOldPen);
            DeleteObject(hCheckPen);
        } else {
            Logger::Instance().Log("DrawCheckbox: NOT checked, no check mark drawn", "DEBUG");
        }
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        
        HGDIOBJ hOldFont = SelectObject(hdc, hFont_);
        TextOutA(hdc, boxX + boxSize + Scale(5), boxY, text, (int)strlen(text));
        SelectObject(hdc, hOldFont);
    }

    void UpdateUI() {
        switch (currentState_) {
            case State::STOPPED:
                SetWindowTextA(hStatus_, TR("status_offline").c_str());
                EnableWindow(hStartBtn_, TRUE);
                EnableWindow(hStopBtn_, FALSE);
                EnableWindow(hRestartBtn_, FALSE);
                SetWindowTextA(hInfoPid_, "> PID: -");
                break;
            case State::STARTING:
                SetWindowTextA(hStatus_, TR("status_starting").c_str());
                EnableWindow(hStartBtn_, FALSE);
                EnableWindow(hStopBtn_, TRUE);
                EnableWindow(hRestartBtn_, FALSE);
                break;
            case State::RUNNING:
                SetWindowTextA(hStatus_, TR("status_online").c_str());
                EnableWindow(hStartBtn_, FALSE);
                EnableWindow(hStopBtn_, TRUE);
                EnableWindow(hRestartBtn_, TRUE);
                std::thread([this]() {
                    auto pids = ProcessManager::Instance().FindGatewayPids();
                    if (!pids.empty()) {
                        std::string pidStr = "> PID: ";
                        for (size_t i = 0; i < pids.size(); i++) {
                            if (i > 0) pidStr += ", ";
                            pidStr += std::to_string(pids[i]);
                        }
                        PostMessage(hWnd_, WM_SET_PID_TEXT, (WPARAM)new std::string(pidStr), 0);
                    }
                }).detach();
                break;
        }
    }

    void RefreshAllText() {
        SetWindowTextA(hWnd_, TR("app_title").c_str());
        SetWindowTextA(hStartBtn_, TR("btn_start").c_str());
        SetWindowTextA(hStopBtn_, TR("btn_stop").c_str());
        SetWindowTextA(hRestartBtn_, TR("btn_restart").c_str());
        SetWindowTextA(hCheckBtn_, TR("btn_check").c_str());
        SetWindowTextA(hHotkeyBtn_, TR("btn_set_hotkey").c_str());
        SetWindowTextA(hClearBtn_, TR("btn_clear").c_str());
        SetWindowTextA(hStartupCheckbox_, TR("btn_startup").c_str());
        UpdateUI();
        TrayIcon::Instance().SetState(currentState_);
    }

    void ClearTerminal() {
        SetWindowTextW(hTerminal_, L"");
        SendMessage(hTerminal_, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));
        InvalidateRect(hTerminal_, NULL, TRUE);
    }

    void StartOpenClash() {
        if (currentState_ != State::STOPPED) return;
        
        tryCount_ = 0;
        manualStop_ = false;
        
        std::thread([this]() {
            DoStart(true);
        }).detach();
    }

    void DoStart(bool manual) {
        if (manual) {
            tryCount_ = 0;
        } else {
            tryCount_++;
            if (tryCount_ > Config::Instance().maxStartAttempts) {
                LogFromThread("Auto-restart failed (max attempts reached), please start manually");
                PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STOPPED, 0);
                return;
            }
        }
        
        auto pids = ProcessManager::Instance().FindGatewayPids();
        if (!pids.empty()) {
            LogFromThread("OpenClaw already running (PID: " + std::to_string(pids[0]) + "), use restart instead");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
            return;
        }
        
        PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STARTING, 0);
        
        std::string attemptInfo = tryCount_ > 0 ? " (attempt " + std::to_string(tryCount_) + ")" : "";
        LogFromThread("Starting OpenClaw" + attemptInfo + "...");
        
        if (!ProcessManager::Instance().StartProcess()) {
            LogFromThread("Failed to start OpenClaw");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STOPPED, 0);
            return;
        }
        
        LogFromThread("Waiting for startup (monitoring output)...");
        
        if (ProcessManager::Instance().WaitForReady(10000)) {
            LogFromThread("OpenClaw started successfully");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
            return;
        }
        
        LogFromThread("Startup timeout, checking process status...");
        
        pids = ProcessManager::Instance().FindGatewayPids();
        if (!pids.empty()) {
            LogFromThread("Process found (PID: " + std::to_string(pids[0]) + "), assuming success");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
        } else {
            LogFromThread("Process not found, startup failed");
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STOPPED, 0);
        }
    }

    void StopOpenClash() {
        manualStop_ = true;
        stopMonitor_ = true;
        
        LogToTerminal("Stopping OpenClaw...");
        
        std::thread([this]() {
            ProcessManager::Instance().StopProcess();
            tryCount_ = 0;
            PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::STOPPED, 0);
        }).detach();
    }

    void RestartOpenClash() {
        if (currentState_ != State::RUNNING) return;
        
        tryCount_ = 0;
        manualStop_ = false;
        stopMonitor_ = true;
        
        LogToTerminal("Restarting OpenClaw...");
        
        std::thread([this]() {
            ProcessManager::Instance().StopProcess();
            Sleep(500);
            
            auto pids = ProcessManager::Instance().FindGatewayPids();
            if (!pids.empty()) {
                LogFromThread("Failed to stop existing process, aborting restart");
                PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
                return;
            }
            
            DoStart(true);
        }).detach();
    }

    void StartPidMonitor() {
        stopMonitor_ = false;
        std::thread([this]() {
            while (!stopMonitor_ && currentState_ == State::RUNNING) {
                Sleep(1000);
                
                auto pids = ProcessManager::Instance().FindGatewayPids();
                if (pids.empty() && !manualStop_) {
                    LogFromThread("Process exited unexpectedly, attempting restart...");
                    PostMessage(hWnd_, WM_RESTART_REQUEST, 0, 0);
                    break;
                }
            }
        }).detach();
    }

    void ManualHealthCheck() {
        EnableWindow(hCheckBtn_, FALSE);
        SetWindowTextA(hCheckBtn_, "检查中...");
        
        LogToTerminal("Manual health check");
        
        std::thread([this]() {
            auto pids = ProcessManager::Instance().FindGatewayPids();
            if (!pids.empty()) {
                LogFromThread("Found process PID: " + std::to_string(pids[0]));
            } else {
                LogFromThread("No OpenClaw process found");
            }
            
            std::string url = "http://127.0.0.1:" + std::to_string(Config::Instance().gatewayPort) + "/";
            LogFromThread("HTTP GET: " + url);
            
            if (ProcessManager::Instance().HealthCheck()) {
                LogFromThread("Response: HTTP 200", "green");
                LogFromThread("Gateway is running normally", "green");
                if (currentState_ != State::RUNNING) {
                    PostMessage(hWnd_, WM_STATE_CHANGE, (WPARAM)State::RUNNING, 0);
                }
            } else {
                LogFromThread("Connection failed", "red");
                LogFromThread("Gateway not running or inaccessible", "red");
            }
            
            PostMessage(hWnd_, WM_CHECK_COMPLETE, 0, 0);
        }).detach();
    }

    void ConfigureHotkey() {
        if (capturingHotkey_) {
            capturingHotkey_ = false;
            SetWindowTextA(hHotkeyBtn_, TR("btn_set_hotkey").c_str());
            ReleaseCapture();
            LogToTerminal("Hotkey capture cancelled");
            return;
        }
        
        capturingHotkey_ = true;
        SetWindowTextA(hHotkeyBtn_, "[按下...]");
        LogToTerminal("Press any key combination (ESC to cancel)...");
        SetFocus(hWnd_);
        SetCapture(hWnd_);
    }

    void OnStartupCheckboxToggle() {
        LONG_PTR userData = GetWindowLongPtr(hStartupCheckbox_, GWLP_USERDATA);
        BOOL checked = (userData == 1);
        
        Logger::Instance().Log("OnStartupCheckboxToggle: GWLP_USERDATA=" + std::to_string(userData) + " checked=" + std::to_string(checked), "DEBUG");
        
        checked = !checked;
        
        SetWindowLongPtr(hStartupCheckbox_, GWLP_USERDATA, checked ? 1 : 0);
        
        Config& cfg = Config::Instance();
        
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        
        if (checked) {
            if (StartupManager::Instance().CreateTask(exePath)) {
                cfg.windowsStartup = true;
                cfg.Save();
                LogToTerminal("[AUTO-START] Enabled - Will start on Windows boot (delayed 30s)", "green");
            } else {
                SetWindowLongPtr(hStartupCheckbox_, GWLP_USERDATA, 0);
                LogToTerminal("[AUTO-START] Failed to create startup task", "red");
            }
        } else {
            if (StartupManager::Instance().RemoveTask()) {
                cfg.windowsStartup = false;
                cfg.Save();
                LogToTerminal("[AUTO-START] Disabled", "green");
            } else {
                SetWindowLongPtr(hStartupCheckbox_, GWLP_USERDATA, 1);
                LogToTerminal("[AUTO-START] Failed to remove startup task", "red");
            }
        }
        
        InvalidateRect(hStartupCheckbox_, NULL, TRUE);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = nullptr;
        
        if (message == WM_NCCREATE) {
            pThis = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        } else {
            pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }
        
        if (pThis) {
            return pThis->HandleMessage(hWnd, message, wParam, lParam);
        }
        
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_NCCALCSIZE: {
                if (wParam) {
                    return 0;
                }
                break;
            }
            
            case WM_NCHITTEST: {
                LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
                
                POINT pt;
                pt.x = GET_X_LPARAM(lParam);
                pt.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hWnd, &pt);
                
                RECT rcClient;
                GetClientRect(hWnd_, &rcClient);
                
                int titleBarHeight = Scale(32);
                int borderWidth = Scale(8);
                int btnSize = Scale(24);
                int btnY = Scale(4);
                int closeBtnX = rcClient.right - Scale(32);
                int minBtnX = closeBtnX - Scale(30);
                
                if (pt.y >= btnY && pt.y <= btnY + btnSize) {
                    if (pt.x >= closeBtnX && pt.x <= closeBtnX + btnSize) {
                        Logger::Instance().Log("WM_NCHITTEST: returning HTCLIENT for close button", "DEBUG");
                        return HTCLIENT;
                    }
                    if (pt.x >= minBtnX && pt.x <= minBtnX + btnSize) {
                        Logger::Instance().Log("WM_NCHITTEST: returning HTCLIENT for minimize button", "DEBUG");
                        return HTCLIENT;
                    }
                }
                
                if (pt.y < titleBarHeight) {
                    if (pt.x < borderWidth) return HTTOPLEFT;
                    if (pt.x > rcClient.right - borderWidth) return HTTOPRIGHT;
                    return HTCAPTION;
                }
                
                if (pt.x < borderWidth) {
                    if (pt.y < borderWidth) return HTTOPLEFT;
                    if (pt.y > rcClient.bottom - borderWidth) return HTBOTTOMLEFT;
                    return HTLEFT;
                }
                
                if (pt.x > rcClient.right - borderWidth) {
                    if (pt.y < borderWidth) return HTTOPRIGHT;
                    if (pt.y > rcClient.bottom - borderWidth) return HTBOTTOMRIGHT;
                    return HTRIGHT;
                }
                
                if (pt.y > rcClient.bottom - borderWidth) return HTBOTTOM;
                
                return hit;
            }
            
            case WM_ENTERSIZEMOVE:
                isMovingOrSizing_ = true;
                break;
            
            case WM_EXITSIZEMOVE:
                isMovingOrSizing_ = false;
                for (auto* text : pendingTexts_) {
                    AppendColoredText(*text);
                }
                if (autoScroll_) {
                    ScrollToBottom();
                }
                for (auto* text : pendingTexts_) {
                    delete text;
                }
                pendingTexts_.clear();
                break;
            
            case WM_APPEND_TEXT: {
                std::string* text = reinterpret_cast<std::string*>(wParam);
                if (text) {
                    if (isMovingOrSizing_) {
                        pendingTexts_.push_back(text);
                    } else {
                        bool wasAtBottom = IsScrolledToBottom();
                        AppendColoredText(*text);
                        if (wasAtBottom && autoScroll_) {
                            ScrollToBottom();
                        }
                        delete text;
                    }
                }
                return 0;
            }
            
            case WM_SET_PID_TEXT: {
                std::string* text = reinterpret_cast<std::string*>(wParam);
                if (text) {
                    SetWindowTextA(hInfoPid_, text->c_str());
                    delete text;
                }
                return 0;
            }
            
            case WM_LOG_MESSAGE: {
                LogMessageData* data = reinterpret_cast<LogMessageData*>(wParam);
                if (data) {
                    LogToTerminal(data->message, data->color);
                    delete data;
                }
                return 0;
            }
            
            case WM_STATE_CHANGE:
                SetState(static_cast<State>(wParam));
                if (currentState_ == State::RUNNING) {
                    StartPidMonitor();
                }
                return 0;
            
            case WM_RESTART_REQUEST:
                if (!manualStop_) {
                    DoStart(false);
                }
                return 0;
            
            case WM_CHECK_COMPLETE:
                EnableWindow(hCheckBtn_, TRUE);
                SetWindowTextA(hCheckBtn_, "[检查]");
                return 0;
            
            case WM_QUIT_APP:
                TrayIcon::Instance().Destroy();
                DestroyWindow(hWnd_);
                return 0;
            
            case WM_GETMINMAXINFO: {
                MINMAXINFO* pMMI = reinterpret_cast<MINMAXINFO*>(lParam);
                
                RECT rc = {0, 0, Scale(MIN_WINDOW_WIDTH), Scale(MIN_WINDOW_HEIGHT)};
                AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
                
                pMMI->ptMinTrackSize.x = rc.right - rc.left;
                pMMI->ptMinTrackSize.y = rc.bottom - rc.top;
                return 0;
            }
            
            case WM_NOTIFY: {
                NMHDR* pnmhdr = (NMHDR*)lParam;
                if (pnmhdr->hwndFrom == hTerminal_ && pnmhdr->code == EN_VSCROLL) {
                    autoScroll_ = IsScrolledToBottom();
                }
                break;
            }
            
            case WM_DRAWITEM: {
                LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
                if (lpDIS->hwndItem == hStartupCheckbox_) {
                    DrawCheckbox(lpDIS);
                    return TRUE;
                } else if (lpDIS->CtlType == ODT_BUTTON) {
                    DrawButton(lpDIS);
                    return TRUE;
                }
                break;
            }
            
            case WM_ERASEBKGND: {
                HDC hdc = (HDC)wParam;
                RECT rcClient;
                GetClientRect(hWnd_, &rcClient);
                
                static int eraseCounter = 0;
                if (eraseCounter++ % 30 == 0) {
                    Logger::Instance().Log("WM_ERASEBKGND: closeBtnHovered_=" + std::to_string(closeBtnHovered_) + 
                        " closeBtnPressed_=" + std::to_string(closeBtnPressed_), "DEBUG");
                }
                
                FillRect(hdc, &rcClient, hBlackBrush_);
                
                int titleBarHeight = Scale(32);
                
                RECT titleBar;
                titleBar.left = 0;
                titleBar.top = 0;
                titleBar.right = rcClient.right;
                titleBar.bottom = titleBarHeight;
                
                HBRUSH hTitleBrush = CreateSolidBrush(RGB(8, 8, 8));
                FillRect(hdc, &titleBar, hTitleBrush);
                DeleteObject(hTitleBrush);
                
                HPEN hTitlePen = CreatePen(PS_SOLID, Scale(1), RGB(0, 80, 0));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hTitlePen);
                MoveToEx(hdc, 0, titleBarHeight - 1, NULL);
                LineTo(hdc, rcClient.right, titleBarHeight - 1);
                SelectObject(hdc, hOldPen);
                DeleteObject(hTitlePen);
                
                HICON hTitleIcon = LoadIconA(hInstance_, MAKEINTRESOURCEA(101));
                if (hTitleIcon) {
                    int iconSize = (int)(Scale(24) * 0.9);
                    DrawIconEx(hdc, Scale(10), Scale(5), hTitleIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
                    DestroyIcon(hTitleIcon);
                }
                
                HFONT hTitleFont = CreateFont(Scale(16), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
                HGDIOBJ hOldFont = SelectObject(hdc, hTitleFont);
                
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(0, 255, 0));
                
                std::string title = "OpenClaw Launcher";
                TextOutA(hdc, Scale(40), Scale(7), title.c_str(), (int)title.length());
                
                SelectObject(hdc, hOldFont);
                DeleteObject(hTitleFont);
                
                int btnSize = Scale(24);
                int btnY = Scale(4);
                int closeBtnX = rcClient.right - Scale(32);
                
                COLORREF closeBgColor, closeBorderColor, closeXColor;
                if (closeBtnPressed_) {
                    closeBgColor = RGB(0, 80, 0);
                    closeBorderColor = RGB(0, 255, 0);
                    closeXColor = RGB(0, 255, 0);
                } else if (closeBtnHovered_) {
                    closeBgColor = RGB(0, 50, 0);
                    closeBorderColor = RGB(0, 180, 0);
                    closeXColor = RGB(0, 230, 0);
                } else {
                    closeBgColor = RGB(20, 20, 20);
                    closeBorderColor = RGB(0, 100, 0);
                    closeXColor = RGB(0, 200, 0);
                }
                
                HPEN hClosePen = CreatePen(PS_SOLID, Scale(1), closeBorderColor);
                HBRUSH hCloseBrush = CreateSolidBrush(closeBgColor);
                hOldPen = (HPEN)SelectObject(hdc, hClosePen);
                HGDIOBJ hOldBrush = SelectObject(hdc, hCloseBrush);
                
                RoundRect(hdc, closeBtnX, btnY, closeBtnX + btnSize, btnY + btnSize, Scale(4), Scale(4));
                
                SelectObject(hdc, hOldBrush);
                SelectObject(hdc, hOldPen);
                DeleteObject(hClosePen);
                DeleteObject(hCloseBrush);
                
                HPEN hXPen = CreatePen(PS_SOLID, Scale(2), closeXColor);
                hOldPen = (HPEN)SelectObject(hdc, hXPen);
                int xInset = Scale(8);
                MoveToEx(hdc, closeBtnX + xInset, btnY + xInset, NULL);
                LineTo(hdc, closeBtnX + btnSize - xInset, btnY + btnSize - xInset);
                MoveToEx(hdc, closeBtnX + btnSize - xInset, btnY + xInset, NULL);
                LineTo(hdc, closeBtnX + xInset, btnY + btnSize - xInset);
                SelectObject(hdc, hOldPen);
                DeleteObject(hXPen);
                
                int minBtnX = closeBtnX - Scale(30);
                
                COLORREF minBgColor, minBorderColor, minLineColor;
                if (minBtnPressed_) {
                    minBgColor = RGB(0, 80, 0);
                    minBorderColor = RGB(0, 255, 0);
                    minLineColor = RGB(0, 255, 0);
                } else if (minBtnHovered_) {
                    minBgColor = RGB(0, 50, 0);
                    minBorderColor = RGB(0, 180, 0);
                    minLineColor = RGB(0, 230, 0);
                } else {
                    minBgColor = RGB(20, 20, 20);
                    minBorderColor = RGB(0, 100, 0);
                    minLineColor = RGB(0, 200, 0);
                }
                
                HPEN hMinPen = CreatePen(PS_SOLID, Scale(1), minBorderColor);
                HBRUSH hMinBrush = CreateSolidBrush(minBgColor);
                hOldPen = (HPEN)SelectObject(hdc, hMinPen);
                hOldBrush = SelectObject(hdc, hMinBrush);
                
                RoundRect(hdc, minBtnX, btnY, minBtnX + btnSize, btnY + btnSize, Scale(4), Scale(4));
                
                SelectObject(hdc, hOldBrush);
                SelectObject(hdc, hOldPen);
                DeleteObject(hMinPen);
                DeleteObject(hMinBrush);
                
                HPEN hLinePen = CreatePen(PS_SOLID, Scale(2), minLineColor);
                hOldPen = (HPEN)SelectObject(hdc, hLinePen);
                int lineY = btnY + btnSize / 2 + Scale(2);
                MoveToEx(hdc, minBtnX + Scale(6), lineY, NULL);
                LineTo(hdc, minBtnX + btnSize - Scale(6), lineY);
                SelectObject(hdc, hOldPen);
                DeleteObject(hLinePen);
                
                RECT infoFrame;
                infoFrame.left = Scale(10);
                infoFrame.top = titleBarHeight + Scale(10);
                infoFrame.right = rcClient.right - Scale(10);
                infoFrame.bottom = titleBarHeight + Scale(134);
                
                HPEN hBorderPen = CreatePen(PS_SOLID, Scale(1), RGB(0, 80, 0));
                HBRUSH hFrameBrush = CreateSolidBrush(RGB(8, 8, 8));
                hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
                hOldBrush = (HBRUSH)SelectObject(hdc, hFrameBrush);
                int radius = Scale(8);
                RoundRect(hdc, infoFrame.left, infoFrame.top, infoFrame.right, infoFrame.bottom, radius, radius);
                
                HPEN hHighlightPen = CreatePen(PS_SOLID, Scale(1), RGB(0, 100, 0));
                SelectObject(hdc, hHighlightPen);
                Arc(hdc, infoFrame.left + Scale(1), infoFrame.top + Scale(1),
                    infoFrame.left + radius * 2, infoFrame.top + radius * 2,
                    infoFrame.left + radius, infoFrame.top + Scale(1),
                    infoFrame.left + Scale(1), infoFrame.top + radius);
                Arc(hdc, infoFrame.right - radius * 2, infoFrame.top + Scale(1),
                    infoFrame.right - Scale(1), infoFrame.top + radius * 2,
                    infoFrame.right - Scale(1), infoFrame.top + radius,
                    infoFrame.right - radius, infoFrame.top + Scale(1));
                SelectObject(hdc, hOldPen);
                DeleteObject(hHighlightPen);
                DeleteObject(hBorderPen);
                DeleteObject(hFrameBrush);
                
                RECT logFrame;
                logFrame.left = Scale(10);
                logFrame.top = titleBarHeight + Scale(175);
                logFrame.right = rcClient.right - Scale(10);
                logFrame.bottom = rcClient.bottom - Scale(10);
                
                int logRadius = Scale(8);
                
                HBRUSH hLogBgBrush = CreateSolidBrush(RGB(10, 10, 10));
                HPEN hLogBorderPen = CreatePen(PS_SOLID, Scale(1), RGB(0, 80, 0));
                SelectObject(hdc, hLogBgBrush);
                SelectObject(hdc, hLogBorderPen);
                RoundRect(hdc, logFrame.left, logFrame.top, logFrame.right, logFrame.bottom, logRadius, logRadius);
                SelectObject(hdc, hOldBrush);
                SelectObject(hdc, hOldPen);
                DeleteObject(hLogBgBrush);
                DeleteObject(hLogBorderPen);
                
                HPEN hLogHighlightPen = CreatePen(PS_SOLID, Scale(1), RGB(0, 80, 0));
                SelectObject(hdc, hLogHighlightPen);
                Arc(hdc, logFrame.left + Scale(1), logFrame.top + Scale(1),
                    logFrame.left + logRadius * 2, logFrame.top + logRadius * 2,
                    logFrame.left + logRadius, logFrame.top + Scale(1),
                    logFrame.left + Scale(1), logFrame.top + logRadius);
                Arc(hdc, logFrame.right - logRadius * 2, logFrame.top + Scale(1),
                    logFrame.right - Scale(1), logFrame.top + logRadius * 2,
                    logFrame.right - Scale(1), logFrame.top + logRadius,
                    logFrame.right - logRadius, logFrame.top + Scale(1));
                Arc(hdc, logFrame.left + Scale(1), logFrame.bottom - logRadius * 2,
                    logFrame.left + logRadius * 2, logFrame.bottom - Scale(1),
                    logFrame.left + Scale(1), logFrame.bottom - logRadius,
                    logFrame.left + logRadius, logFrame.bottom - Scale(1));
                Arc(hdc, logFrame.right - logRadius * 2, logFrame.bottom - logRadius * 2,
                    logFrame.right - Scale(1), logFrame.bottom - Scale(1),
                    logFrame.right - logRadius, logFrame.bottom - Scale(1),
                    logFrame.right - Scale(1), logFrame.bottom - logRadius);
                SelectObject(hdc, hOldPen);
                DeleteObject(hLogHighlightPen);
                
                return TRUE;
            }
            
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLORDLG: {
                HDC hdc = (HDC)wParam;
                LONG_PTR userData = GetWindowLongPtr((HWND)lParam, GWLP_USERDATA);
                
                if (userData == 1) {
                    SetTextColor(hdc, RGB(0, 255, 0));
                    SetBkColor(hdc, RGB(8, 8, 8));
                    return (LRESULT)hFrameBrush_;
                } else if (userData == 2) {
                    SetTextColor(hdc, RGB(0, 170, 0));
                    SetBkColor(hdc, RGB(8, 8, 8));
                    return (LRESULT)hFrameBrush_;
                } else if (userData == 3) {
                    SetTextColor(hdc, RGB(57, 255, 20));
                    SetBkColor(hdc, RGB(8, 8, 8));
                    return (LRESULT)hFrameBrush_;
                } else if (userData == 4) {
                    if (currentState_ == State::RUNNING) {
                        SetTextColor(hdc, RGB(0, 255, 0));
                    } else if (currentState_ == State::STARTING) {
                        SetTextColor(hdc, RGB(255, 204, 0));
                    } else {
                        SetTextColor(hdc, RGB(255, 51, 51));
                    }
                    SetBkColor(hdc, RGB(12, 12, 12));
                    return (LRESULT)hStatusBrush_;
                } else {
                    SetTextColor(hdc, RGB(0, 255, 0));
                    SetBkColor(hdc, RGB(8, 8, 8));
                    return (LRESULT)hFrameBrush_;
                }
            }
            
            case WM_CTLCOLORBTN: {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, RGB(0, 255, 0));
                SetBkMode(hdc, TRANSPARENT);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            
            case WM_CTLCOLOREDIT: {
                HDC hdc = (HDC)wParam;
                SetTextColor(hdc, RGB(0, 255, 0));
                SetBkColor(hdc, RGB(10, 10, 10));
                return (LRESULT)hDarkBrush_;
            }
            
            case WM_TRAYICON:
                Logger::Instance().Log("WM_TRAYICON: lParam=" + std::to_string(LOWORD(lParam)), "DEBUG");
                if (LOWORD(lParam) == WM_RBUTTONUP) {
                    TrayIcon::Instance().ShowContextMenu(hWnd);
                } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                    Logger::Instance().Log("Tray icon double-clicked, calling Toggle()", "DEBUG");
                    Toggle();
                }
                break;
            
            case WM_TIMER:
                if (wParam == MENU_UPDATE_TIMER_ID) {
                    TrayIcon::Instance().UpdateMenuFromTimer();
                }
                break;
            
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case ID_TRAY_SHOW:
                        Toggle();
                        break;
                    case ID_TRAY_START_STOP:
                        if (currentState_ == State::STOPPED) {
                            StartOpenClash();
                        } else {
                            StopOpenClash();
                        }
                        break;
                    case ID_TRAY_START:
                        StartOpenClash();
                        break;
                    case ID_TRAY_RESTART:
                        RestartOpenClash();
                        break;
                    case ID_TRAY_STOP:
                        StopOpenClash();
                        break;
                    case ID_TRAY_CLEAR:
                        ClearTerminal();
                        break;
                    case ID_TRAY_EXIT:
                        ExitApp();
                        break;
                    case ID_TRAY_LANG_ZH:
                        I18N::Instance().SetLanguage(Language::CHINESE);
                        RefreshAllText();
                        break;
                    case ID_TRAY_LANG_EN:
                        I18N::Instance().SetLanguage(Language::ENGLISH);
                        RefreshAllText();
                        break;
                    case 1007:
                        ManualHealthCheck();
                        break;
                    case 1009:
                        ConfigureHotkey();
                        break;
                    case 1008:
                        OnStartupCheckboxToggle();
                        break;
                }
                break;
            
            case WM_HOTKEY:
                if (wParam == WM_HOTKEY_TOGGLE) {
                    Toggle();
                }
                break;
            
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                if (capturingHotkey_) {
                    if (wParam == VK_ESCAPE) {
                        capturingHotkey_ = false;
                        SetWindowTextA(hHotkeyBtn_, TR("btn_set_hotkey").c_str());
                        ReleaseCapture();
                        LogToTerminal("Hotkey capture cancelled");
                        return 0;
                    }
                    
                    if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
                        return 0;
                    }
                    
                    std::string hotkey;
                    
                    bool ctrl = GetKeyState(VK_CONTROL) & 0x8000;
                    bool alt = GetKeyState(VK_MENU) & 0x8000;
                    bool shift = GetKeyState(VK_SHIFT) & 0x8000;
                    
                    if (ctrl) hotkey += "ctrl+";
                    if (alt) hotkey += "alt+";
                    if (shift) hotkey += "shift+";
                    
                    char keyName[64] = {0};
                    UINT vk = (UINT)wParam;
                    int scancode = (lParam >> 16) & 0xFF;
                    bool isExtended = (lParam >> 24) & 0x1;
                    
                    switch (vk) {
                        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
                        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
                        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                        case 'Y': case 'Z':
                            keyName[0] = (char)tolower(vk);
                            keyName[1] = '\0';
                            break;
                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                            keyName[0] = (char)vk;
                            keyName[1] = '\0';
                            break;
                        case VK_NUMPAD0: strcpy_s(keyName, "numpad0"); break;
                        case VK_NUMPAD1: strcpy_s(keyName, "numpad1"); break;
                        case VK_NUMPAD2: strcpy_s(keyName, "numpad2"); break;
                        case VK_NUMPAD3: strcpy_s(keyName, "numpad3"); break;
                        case VK_NUMPAD4: strcpy_s(keyName, "numpad4"); break;
                        case VK_NUMPAD5: strcpy_s(keyName, "numpad5"); break;
                        case VK_NUMPAD6: strcpy_s(keyName, "numpad6"); break;
                        case VK_NUMPAD7: strcpy_s(keyName, "numpad7"); break;
                        case VK_NUMPAD8: strcpy_s(keyName, "numpad8"); break;
                        case VK_NUMPAD9: strcpy_s(keyName, "numpad9"); break;
                        case VK_MULTIPLY: strcpy_s(keyName, "multiply"); break;
                        case VK_ADD: strcpy_s(keyName, "add"); break;
                        case VK_SUBTRACT: strcpy_s(keyName, "subtract"); break;
                        case VK_DECIMAL: strcpy_s(keyName, "decimal"); break;
                        case VK_DIVIDE: strcpy_s(keyName, "divide"); break;
                        case VK_F1: strcpy_s(keyName, "f1"); break;
                        case VK_F2: strcpy_s(keyName, "f2"); break;
                        case VK_F3: strcpy_s(keyName, "f3"); break;
                        case VK_F4: strcpy_s(keyName, "f4"); break;
                        case VK_F5: strcpy_s(keyName, "f5"); break;
                        case VK_F6: strcpy_s(keyName, "f6"); break;
                        case VK_F7: strcpy_s(keyName, "f7"); break;
                        case VK_F8: strcpy_s(keyName, "f8"); break;
                        case VK_F9: strcpy_s(keyName, "f9"); break;
                        case VK_F10: strcpy_s(keyName, "f10"); break;
                        case VK_F11: strcpy_s(keyName, "f11"); break;
                        case VK_F12: strcpy_s(keyName, "f12"); break;
                        case VK_F13: strcpy_s(keyName, "f13"); break;
                        case VK_F14: strcpy_s(keyName, "f14"); break;
                        case VK_F15: strcpy_s(keyName, "f15"); break;
                        case VK_F16: strcpy_s(keyName, "f16"); break;
                        case VK_F17: strcpy_s(keyName, "f17"); break;
                        case VK_F18: strcpy_s(keyName, "f18"); break;
                        case VK_F19: strcpy_s(keyName, "f19"); break;
                        case VK_F20: strcpy_s(keyName, "f20"); break;
                        case VK_F21: strcpy_s(keyName, "f21"); break;
                        case VK_F22: strcpy_s(keyName, "f22"); break;
                        case VK_F23: strcpy_s(keyName, "f23"); break;
                        case VK_F24: strcpy_s(keyName, "f24"); break;
                        case VK_SPACE: strcpy_s(keyName, "space"); break;
                        case VK_RETURN: strcpy_s(keyName, "enter"); break;
                        case VK_TAB: strcpy_s(keyName, "tab"); break;
                        case VK_INSERT: strcpy_s(keyName, "insert"); break;
                        case VK_DELETE: strcpy_s(keyName, "delete"); break;
                        case VK_HOME: strcpy_s(keyName, "home"); break;
                        case VK_END: strcpy_s(keyName, "end"); break;
                        case VK_PRIOR: strcpy_s(keyName, "pageup"); break;
                        case VK_NEXT: strcpy_s(keyName, "pagedown"); break;
                        case VK_UP: strcpy_s(keyName, "up"); break;
                        case VK_DOWN: strcpy_s(keyName, "down"); break;
                        case VK_LEFT: strcpy_s(keyName, "left"); break;
                        case VK_RIGHT: strcpy_s(keyName, "right"); break;
                        case VK_PRINT: strcpy_s(keyName, "print"); break;
                        case VK_SNAPSHOT: strcpy_s(keyName, "printscreen"); break;
                        case VK_SCROLL: strcpy_s(keyName, "scrolllock"); break;
                        case VK_PAUSE: strcpy_s(keyName, "pause"); break;
                        case VK_NUMLOCK: strcpy_s(keyName, "numlock"); break;
                        case VK_CAPITAL: strcpy_s(keyName, "capslock"); break;
                        case VK_BACK: strcpy_s(keyName, "backspace"); break;
                        case VK_APPS: strcpy_s(keyName, "menu"); break;
                        case VK_LWIN: case VK_RWIN: strcpy_s(keyName, "win"); break;
                        case VK_OEM_1: strcpy_s(keyName, ";"); break;
                        case VK_OEM_2: strcpy_s(keyName, "/"); break;
                        case VK_OEM_3: strcpy_s(keyName, "`"); break;
                        case VK_OEM_4: strcpy_s(keyName, "["); break;
                        case VK_OEM_5: strcpy_s(keyName, "\\"); break;
                        case VK_OEM_6: strcpy_s(keyName, "]"); break;
                        case VK_OEM_7: strcpy_s(keyName, "'"); break;
                        case VK_OEM_PLUS: strcpy_s(keyName, "="); break;
                        case VK_OEM_MINUS: strcpy_s(keyName, "-"); break;
                        case VK_OEM_COMMA: strcpy_s(keyName, ","); break;
                        case VK_OEM_PERIOD: strcpy_s(keyName, "."); break;
                        case VK_OEM_102: strcpy_s(keyName, "oem_102"); break;
                        case VK_BROWSER_BACK: strcpy_s(keyName, "browser_back"); break;
                        case VK_BROWSER_FORWARD: strcpy_s(keyName, "browser_forward"); break;
                        case VK_BROWSER_REFRESH: strcpy_s(keyName, "browser_refresh"); break;
                        case VK_BROWSER_STOP: strcpy_s(keyName, "browser_stop"); break;
                        case VK_BROWSER_SEARCH: strcpy_s(keyName, "browser_search"); break;
                        case VK_BROWSER_FAVORITES: strcpy_s(keyName, "browser_favorites"); break;
                        case VK_BROWSER_HOME: strcpy_s(keyName, "browser_home"); break;
                        case VK_VOLUME_MUTE: strcpy_s(keyName, "volume_mute"); break;
                        case VK_VOLUME_DOWN: strcpy_s(keyName, "volume_down"); break;
                        case VK_VOLUME_UP: strcpy_s(keyName, "volume_up"); break;
                        case VK_MEDIA_NEXT_TRACK: strcpy_s(keyName, "media_next"); break;
                        case VK_MEDIA_PREV_TRACK: strcpy_s(keyName, "media_prev"); break;
                        case VK_MEDIA_STOP: strcpy_s(keyName, "media_stop"); break;
                        case VK_MEDIA_PLAY_PAUSE: strcpy_s(keyName, "media_play_pause"); break;
                        case VK_LAUNCH_MAIL: strcpy_s(keyName, "launch_mail"); break;
                        case VK_LAUNCH_MEDIA_SELECT: strcpy_s(keyName, "launch_media"); break;
                        case VK_LAUNCH_APP1: strcpy_s(keyName, "launch_app1"); break;
                        case VK_LAUNCH_APP2: strcpy_s(keyName, "launch_app2"); break;
                        default:
                            if (scancode > 0) {
                                LPARAM lParamForName = (scancode << 16);
                                if (isExtended) {
                                    lParamForName |= (1 << 24);
                                }
                                GetKeyNameTextA(lParamForName, keyName, sizeof(keyName));
                                for (char* p = keyName; *p; p++) {
                                    if (*p == ' ') *p = '_';
                                    else *p = tolower(*p);
                                }
                            }
                            break;
                    }
                    
                    capturingHotkey_ = false;
                    SetWindowTextA(hHotkeyBtn_, TR("btn_set_hotkey").c_str());
                    ReleaseCapture();
                    
                    if (keyName[0]) {
                            hotkey += keyName;
                            
                            Config::Instance().windowHotkey = hotkey;
                            Config::Instance().Save();
                            
                            HotkeyManager::Instance().Unregister(hWnd_);
                            bool registered = HotkeyManager::Instance().Register(hWnd_);
                            
                            SetWindowTextA(hInfoHotkey_, ("> HOTKEY: " + hotkey).c_str());
                            if (registered) {
                                LogToTerminal("Hotkey updated: " + hotkey, "green");
                            } else {
                                LogToTerminal("Hotkey saved but registration failed (admin required): " + hotkey, "yellow");
                            }
                        } else {
                            LogToTerminal("Unknown key, please try again");
                        }
                        return 0;
                }
                break;
            }
            
            case WM_SIZE: {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                
                int titleBarHeight = Scale(32);
                
                int termHeight = height - titleBarHeight - Scale(195);
                if (termHeight < Scale(100)) termHeight = Scale(100);
                
                SetWindowPos(hTerminal_, NULL, Scale(15), titleBarHeight + Scale(180), width - Scale(30), termHeight, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SendMessage(hTerminal_, EM_SETBKGNDCOLOR, 0, RGB(10, 10, 10));
                
                int btnY = titleBarHeight + Scale(142);
                int btnW = Scale(80);
                int btnH = Scale(30);
                
                SetWindowPos(hStatus_, NULL, (width - Scale(120)) / 2, btnY + Scale(4), Scale(120), Scale(24), SWP_NOZORDER | SWP_NOSENDCHANGING);
                
                SetWindowPos(hClearBtn_, NULL, width - Scale(95), btnY, btnW, btnH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SetWindowPos(hHotkeyBtn_, NULL, width - Scale(183), btnY, btnW, btnH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SetWindowPos(hStartupCheckbox_, NULL, width - Scale(270), btnY + Scale(6), Scale(80), Scale(22), SWP_NOZORDER | SWP_NOSENDCHANGING);
                
                int infoX = width - Scale(30);
                int infoH = Scale(22);
                int infoY = titleBarHeight + Scale(14);
                SetWindowPos(hInfoPath_, NULL, Scale(15), infoY, infoX, infoH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SetWindowPos(hInfoConfig_, NULL, Scale(15), infoY + infoH, infoX, infoH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SetWindowPos(hInfoHealth_, NULL, Scale(15), infoY + infoH * 2, infoX, infoH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SetWindowPos(hInfoHotkey_, NULL, Scale(15), infoY + infoH * 3, infoX, infoH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                SetWindowPos(hInfoPid_, NULL, Scale(15), infoY + infoH * 4, infoX, infoH, SWP_NOZORDER | SWP_NOSENDCHANGING);
                
                InvalidateRect(hWnd_, NULL, TRUE);
                InvalidateRect(hTerminal_, NULL, TRUE);
                break;
            }
            
            case WM_LBUTTONDOWN: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                RECT rcClient;
                GetClientRect(hWnd_, &rcClient);
                
                int titleBarHeight = Scale(32);
                int btnSize = Scale(24);
                int btnY = Scale(4);
                int closeBtnX = rcClient.right - Scale(32);
                int minBtnX = closeBtnX - Scale(30);
                
                Logger::Instance().Log("WM_LBUTTONDOWN: x=" + std::to_string(x) + " y=" + std::to_string(y) + 
                    " closeBtnX=" + std::to_string(closeBtnX) + " minBtnX=" + std::to_string(minBtnX) + 
                    " btnY=" + std::to_string(btnY) + " btnSize=" + std::to_string(btnSize), "DEBUG");
                
                RECT titleRect = {0, 0, rcClient.right, titleBarHeight};
                
                if (y >= btnY && y <= btnY + btnSize) {
                    if (x >= closeBtnX && x <= closeBtnX + btnSize) {
                        Logger::Instance().Log("Close button pressed", "DEBUG");
                        closeBtnPressed_ = true;
                        RedrawWindow(hWnd_, &titleRect, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
                        SetCapture(hWnd_);
                        return 0;
                    }
                    
                    if (x >= minBtnX && x <= minBtnX + btnSize) {
                        Logger::Instance().Log("Minimize button pressed", "DEBUG");
                        minBtnPressed_ = true;
                        RedrawWindow(hWnd_, &titleRect, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
                        SetCapture(hWnd_);
                        return 0;
                    }
                }
                break;
            }
            
            case WM_LBUTTONUP: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                ReleaseCapture();
                
                RECT rcClient;
                GetClientRect(hWnd_, &rcClient);
                
                int btnSize = Scale(24);
                int btnY = Scale(4);
                int closeBtnX = rcClient.right - Scale(32);
                int minBtnX = closeBtnX - Scale(30);
                
                bool wasClosePressed = closeBtnPressed_;
                bool wasMinPressed = minBtnPressed_;
                closeBtnPressed_ = false;
                minBtnPressed_ = false;
                
                RECT titleRect = {0, 0, rcClient.right, Scale(32)};
                
                if (wasClosePressed) {
                    if (x >= closeBtnX && x <= closeBtnX + btnSize && y >= btnY && y <= btnY + btnSize) {
                        AnimateToTray(false);
                        return 0;
                    }
                    InvalidateRect(hWnd_, &titleRect, FALSE);
                    UpdateWindow(hWnd_);
                }
                
                if (wasMinPressed) {
                    if (x >= minBtnX && x <= minBtnX + btnSize && y >= btnY && y <= btnY + btnSize) {
                        AnimateToTray(true);
                        return 0;
                    }
                    InvalidateRect(hWnd_, &titleRect, FALSE);
                    UpdateWindow(hWnd_);
                }
                
                break;
            }
            
            case WM_MOUSEMOVE: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                
                RECT rcClient;
                GetClientRect(hWnd_, &rcClient);
                
                int btnSize = Scale(24);
                int btnY = Scale(4);
                int closeBtnX = rcClient.right - Scale(32);
                int minBtnX = closeBtnX - Scale(30);
                
                bool needRedraw = false;
                
                bool closeHovered = (x >= closeBtnX && x <= closeBtnX + btnSize && y >= btnY && y <= btnY + btnSize);
                bool minHovered = (x >= minBtnX && x <= minBtnX + btnSize && y >= btnY && y <= btnY + btnSize);
                
                {
                    static int logCounter = 0;
                    if (logCounter++ % 10 == 0) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "MOUSEMOVE x=%d y=%d closeBtnX=%d closeHovered=%d closeBtnHovered_=%d", 
                                 x, y, closeBtnX, closeHovered, closeBtnHovered_);
                        Logger::Instance().Log(buf, "DEBUG");
                    }
                }
                
                if (closeHovered != closeBtnHovered_) {
                    closeBtnHovered_ = closeHovered;
                    needRedraw = true;
                    Logger::Instance().Log("Close hover changed to: " + std::to_string(closeHovered) + ", needRedraw=true", "DEBUG");
                }
                
                if (minHovered != minBtnHovered_) {
                    minBtnHovered_ = minHovered;
                    needRedraw = true;
                    Logger::Instance().Log("Min hover changed to: " + std::to_string(minHovered) + ", needRedraw=true", "DEBUG");
                }
                
                if (closeBtnPressed_ || minBtnPressed_) {
                    if (closeBtnPressed_) {
                        bool inBtn = closeHovered;
                        if (!inBtn) {
                            closeBtnPressed_ = false;
                            needRedraw = true;
                        }
                    }
                    
                    if (minBtnPressed_) {
                        bool inBtn = minHovered;
                        if (!inBtn) {
                            minBtnPressed_ = false;
                            needRedraw = true;
                        }
                    }
                }
                
                if (needRedraw) {
                    RECT titleRect = {0, 0, rcClient.right, Scale(32)};
                    Logger::Instance().Log("Calling RedrawWindow with RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW", "DEBUG");
                    RedrawWindow(hWnd_, &titleRect, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
                }
                
                if (closeHovered || minHovered) {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hWnd_;
                    tme.dwHoverTime = HOVER_DEFAULT;
                    TrackMouseEvent(&tme);
                }
                
                break;
            }
            
            case WM_MOUSELEAVE: {
                Logger::Instance().Log("WM_MOUSELEAVE received", "DEBUG");
                
                bool needRedraw = false;
                
                if (closeBtnHovered_) {
                    closeBtnHovered_ = false;
                    needRedraw = true;
                }
                
                if (minBtnHovered_) {
                    minBtnHovered_ = false;
                    needRedraw = true;
                }
                
                if (needRedraw) {
                    InvalidateRect(hWnd_, NULL, FALSE);
                }
                
                break;
            }
            
            case WM_CLOSE:
                Hide();
                return 0;
            
            case WM_DESTROY:
                TrayIcon::Instance().Destroy();
                PostQuitMessage(0);
                break;
            
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }

    void ExitApp() {
        if (isShuttingDown_) return;
        
        isShuttingDown_ = true;
        
        bool wasVisible = IsWindowVisible(hWnd_);
        
        if (wasVisible) {
            SetForegroundWindow(hWnd_);
            SetFocus(hWnd_);
            BringWindowToTop(hWnd_);
        } else {
            Show();
        }
        
        LogToTerminal("[SYSTEM] Shutting down...");
        
        stopMonitor_ = true;
        
        HotkeyManager::Instance().Unregister(hWnd_);
        
        std::thread([this]() {
            if (currentState_ == State::RUNNING || currentState_ == State::STARTING) {
                LogFromThread("Stopping OpenClaw instance...");
                
                ProcessManager::Instance().StopProcess();
                Sleep(500);
                
                LogFromThread("OpenClaw instance stopped", "green");
            }
            
            LogFromThread("Exiting launcher...", "yellow");
            Sleep(200);
            
            PostMessage(hWnd_, WM_QUIT_APP, 0, 0);
        }).detach();
    }
};

}
