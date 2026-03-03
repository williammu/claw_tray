#pragma once
#include "Common.h"
#include "I18N.h"

namespace Launcher {

class TrayIcon {
public:

    static TrayIcon& Instance() {
        static TrayIcon instance;
        return instance;
    }

    bool Init(HWND hWnd) {
        hWnd_ = hWnd;
        
        hIcons_[0] = CreateIcon(50, 50, 50);
        hIcons_[1] = CreateIcon(255, 204, 0);
        hIcons_[2] = CreateIcon(0, 255, 68);
        
        ZeroMemory(&nid_, sizeof(nid_));
        nid_.cbSize = sizeof(NOTIFYICONDATA);
        nid_.hWnd = hWnd;
        nid_.uID = 1;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = WM_TRAYICON;
        nid_.hIcon = hIcons_[0];
        strcpy_s(nid_.szTip, TR("tooltip_stopped").c_str());
        
        Shell_NotifyIcon(NIM_ADD, &nid_);
        
        return true;
    }

    void SetState(State state) {
        int index = 0;
        const char* tip = TR("tooltip_stopped").c_str();
        
        switch (state) {
            case State::STOPPED:
                index = 0;
                tip = TR("tooltip_stopped").c_str();
                break;
            case State::STARTING:
                index = 1;
                tip = TR("tooltip_starting").c_str();
                break;
            case State::RUNNING:
                index = 2;
                tip = TR("tooltip_running").c_str();
                break;
        }
        
        nid_.hIcon = hIcons_[index];
        strcpy_s(nid_.szTip, tip);
        Shell_NotifyIcon(NIM_MODIFY, &nid_);
    }

    void ShowContextMenu(HWND hWnd) {
        POINT pt;
        GetCursorPos(&pt);
        
        menuTextShow_ = TR("menu_show");
        menuTextStart_ = TR("menu_start");
        menuTextStop_ = TR("menu_stop");
        menuTextRestart_ = TR("menu_restart");
        menuTextLanguage_ = TR("menu_language");
        menuTextExit_ = TR("menu_exit");
        
        hMenu_ = CreatePopupMenu();
        
        AppendMenu(hMenu_, MF_STRING, ID_TRAY_SHOW, menuTextShow_.c_str());
        AppendMenu(hMenu_, MF_SEPARATOR, 0, NULL);
        
        const char* startStopText = (currentState_ == State::RUNNING || currentState_ == State::STARTING) ? menuTextStop_.c_str() : menuTextStart_.c_str();
        AppendMenu(hMenu_, MF_STRING, ID_TRAY_START_STOP, startStopText);
        
        AppendMenu(hMenu_, MF_STRING, ID_TRAY_RESTART, menuTextRestart_.c_str());
        AppendMenu(hMenu_, MF_SEPARATOR, 0, NULL);
        
        HMENU hLangMenu = CreatePopupMenu();
        AppendMenu(hLangMenu, MF_STRING | (I18N::Instance().IsChinese() ? MF_CHECKED : 0), ID_TRAY_LANG_ZH, "中文");
        AppendMenu(hLangMenu, MF_STRING | (!I18N::Instance().IsChinese() ? MF_CHECKED : 0), ID_TRAY_LANG_EN, "English");
        AppendMenu(hMenu_, MF_POPUP, (UINT_PTR)hLangMenu, menuTextLanguage_.c_str());
        
        AppendMenu(hMenu_, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu_, MF_STRING, ID_TRAY_EXIT, menuTextExit_.c_str());
        
        UpdateMenuState();
        
        SetForegroundWindow(hWnd);
        SetTimer(hWnd, MENU_UPDATE_TIMER_ID, 200, NULL);
        TrackPopupMenu(hMenu_, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
        KillTimer(hWnd, MENU_UPDATE_TIMER_ID);
        DestroyMenu(hLangMenu);
        DestroyMenu(hMenu_);
        hMenu_ = NULL;
    }

    void UpdateMenuFromTimer() {
        if (hMenu_) {
            UpdateMenuState();
        }
    }

    void SetCurrentState(State state) {
        currentState_ = state;
        SetState(state);
    }

    void Destroy() {
        Shell_NotifyIcon(NIM_DELETE, &nid_);
        for (int i = 0; i < 3; i++) {
            if (hIcons_[i]) DestroyIcon(hIcons_[i]);
        }
    }

private:
    HWND hWnd_ = NULL;
    NOTIFYICONDATA nid_;
    HICON hIcons_[3] = {NULL, NULL, NULL};
    State currentState_ = State::STOPPED;
    HMENU hMenu_ = NULL;
    std::string menuTextShow_;
    std::string menuTextStart_;
    std::string menuTextStop_;
    std::string menuTextRestart_;
    std::string menuTextLanguage_;
    std::string menuTextExit_;
    std::string tooltipText_;

    TrayIcon() = default;

    void UpdateMenuState() {
        if (!hMenu_) return;
        
        menuTextStart_ = TR("menu_start");
        menuTextStop_ = TR("menu_stop");
        const char* startStopText = (currentState_ == State::RUNNING || currentState_ == State::STARTING) ? menuTextStop_.c_str() : menuTextStart_.c_str();
        ModifyMenu(hMenu_, ID_TRAY_START_STOP, MF_STRING, ID_TRAY_START_STOP, startStopText);
        
        if (currentState_ == State::STOPPED) {
            EnableMenuItem(hMenu_, ID_TRAY_START_STOP, MF_ENABLED);
            EnableMenuItem(hMenu_, ID_TRAY_RESTART, MF_GRAYED);
        } else if (currentState_ == State::STARTING) {
            EnableMenuItem(hMenu_, ID_TRAY_START_STOP, MF_ENABLED);
            EnableMenuItem(hMenu_, ID_TRAY_RESTART, MF_GRAYED);
        } else {
            EnableMenuItem(hMenu_, ID_TRAY_START_STOP, MF_ENABLED);
            EnableMenuItem(hMenu_, ID_TRAY_RESTART, MF_ENABLED);
        }
    }

    HICON CreateIcon(int r, int g, int b) {
        int size = 64;
        
        HDC hdcScreen = GetDC(NULL);
        
        BITMAPV5HEADER bi = {0};
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = size;
        bi.bV5Height = size;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;
        
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        void* pvBits = NULL;
        HBITMAP hBitmap = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        
        DWORD* pPixels = (DWORD*)pvBits;
        
        int margin = 4;
        int center = size / 2;
        int radius = (size - margin * 2) / 2;
        
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int dx = x - center;
                int dy = y - center;
                int dist = dx * dx + dy * dy;
                int r2 = radius * radius;
                
                int idx = y * size + x;
                
                if (dist <= r2) {
                    pPixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else {
                    pPixels[idx] = 0x00000000;
                }
            }
        }
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        ICONINFO ii = {0};
        ii.fIcon = TRUE;
        ii.hbmMask = hBitmap;
        ii.hbmColor = hBitmap;
        
        HICON hIcon = CreateIconIndirect(&ii);
        
        DeleteObject(hBitmap);
        
        return hIcon;
    }
};

}
