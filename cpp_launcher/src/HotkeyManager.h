#pragma once
#include "Common.h"
#include "Config.h"

namespace Launcher {

class HotkeyManager {
public:
    static HotkeyManager& Instance() {
        static HotkeyManager instance;
        return instance;
    }

    bool Register(HWND hWnd) {
        hWnd_ = hWnd;
        
        if (!IsAdmin()) {
            return false;
        }
        
        Config& cfg = Config::Instance();
        std::string hotkey = cfg.windowHotkey;
        
        UINT modifiers = 0;
        UINT vk = 0;
        
        ParseHotkey(hotkey, modifiers, vk);
        
        if (modifiers == 0 || vk == 0) {
            return false;
        }
        
        if (!RegisterHotKey(hWnd, WM_HOTKEY_TOGGLE, modifiers, vk)) {
            return false;
        }
        
        return true;
    }

    void Unregister(HWND hWnd) {
        UnregisterHotKey(hWnd, WM_HOTKEY_TOGGLE);
    }

    bool IsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        
        SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        
        return isAdmin != FALSE;
    }

private:
    HWND hWnd_ = NULL;

    HotkeyManager() = default;

    void ParseHotkey(const std::string& hotkey, UINT& modifiers, UINT& vk) {
        modifiers = 0;
        vk = 0;
        
        std::string lower = hotkey;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        std::vector<std::string> parts = Split(lower, '+');
        
        for (const auto& part : parts) {
            std::string p = Trim(part);
            
            if (p == "ctrl" || p == "control") {
                modifiers |= MOD_CONTROL;
            } else if (p == "alt") {
                modifiers |= MOD_ALT;
            } else if (p == "shift") {
                modifiers |= MOD_SHIFT;
            } else if (p == "win" || p == "windows") {
                modifiers |= MOD_WIN;
            } else if (p == ",") {
                vk = VK_OEM_COMMA;
            } else if (p == ".") {
                vk = VK_OEM_PERIOD;
            } else if (p == ";") {
                vk = VK_OEM_1;
            } else if (p == "/") {
                vk = VK_OEM_2;
            } else if (p == "`") {
                vk = VK_OEM_3;
            } else if (p == "[") {
                vk = VK_OEM_4;
            } else if (p == "\\") {
                vk = VK_OEM_5;
            } else if (p == "]") {
                vk = VK_OEM_6;
            } else if (p == "'") {
                vk = VK_OEM_7;
            } else if (p == "-") {
                vk = VK_OEM_MINUS;
            } else if (p == "=") {
                vk = VK_OEM_PLUS;
            } else if (p.length() == 1 && p[0] >= 'a' && p[0] <= 'z') {
                vk = toupper(p[0]);
            } else if (p.length() == 1 && p[0] >= '0' && p[0] <= '9') {
                vk = p[0];
            } else if (p == "f1") vk = VK_F1;
            else if (p == "f2") vk = VK_F2;
            else if (p == "f3") vk = VK_F3;
            else if (p == "f4") vk = VK_F4;
            else if (p == "f5") vk = VK_F5;
            else if (p == "f6") vk = VK_F6;
            else if (p == "f7") vk = VK_F7;
            else if (p == "f8") vk = VK_F8;
            else if (p == "f9") vk = VK_F9;
            else if (p == "f10") vk = VK_F10;
            else if (p == "f11") vk = VK_F11;
            else if (p == "f12") vk = VK_F12;
            else if (p == "space") vk = VK_SPACE;
            else if (p == "enter") vk = VK_RETURN;
            else if (p == "tab") vk = VK_TAB;
            else if (p == "insert") vk = VK_INSERT;
            else if (p == "delete") vk = VK_DELETE;
            else if (p == "home") vk = VK_HOME;
            else if (p == "end") vk = VK_END;
            else if (p == "pageup") vk = VK_PRIOR;
            else if (p == "pagedown") vk = VK_NEXT;
            else if (p == "up") vk = VK_UP;
            else if (p == "down") vk = VK_DOWN;
            else if (p == "left") vk = VK_LEFT;
            else if (p == "right") vk = VK_RIGHT;
            else if (p == "backspace") vk = VK_BACK;
        }
    }
};

}
