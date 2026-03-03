#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace Launcher {

constexpr int WM_TRAYICON = WM_USER + 1;
constexpr int WM_HOTKEY_TOGGLE = WM_USER + 2;
constexpr int ID_TRAY_SHOW = 1001;
constexpr int ID_TRAY_START_STOP = 1002;
constexpr int ID_TRAY_RESTART = 1003;
constexpr int ID_TRAY_CLEAR = 1004;
constexpr int ID_TRAY_EXIT = 1005;
constexpr int ID_TRAY_START = 1006;
constexpr int ID_TRAY_STOP = 1010;
constexpr int DEFAULT_PORT = 18789;
constexpr const char* VERSION = "1.0.0";

enum class State {
    STOPPED,
    STARTING,
    RUNNING
};

inline std::string GetAppDataPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\OpenClawLauncher";
    }
    char home[MAX_PATH];
    ExpandEnvironmentStringsA("%USERPROFILE%", home, MAX_PATH);
    return std::string(home) + "\\.openclaw_launcher";
}

inline std::string GetTimestamp() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buffer);
}

inline std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

inline std::vector<std::string> Split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

inline bool FileExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

inline std::string GetEnv(const std::string& name) {
    char buffer[1024];
    DWORD len = GetEnvironmentVariableA(name.c_str(), buffer, 1024);
    if (len > 0 && len < 1024) {
        return std::string(buffer, len);
    }
    return "";
}

inline bool IsAdmin() {
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

inline void CreateDirRecursive(const std::string& path) {
    if (path.empty() || FileExists(path)) return;
    
    std::vector<std::string> parts;
    std::string current;
    for (char c : path) {
        if (c == '\\' || c == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);
    
    std::string buildPath;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) buildPath += "\\";
        buildPath += parts[i];
        if (!buildPath.empty() && buildPath.back() != ':') {
            CreateDirectoryA(buildPath.c_str(), NULL);
        }
    }
}

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring result(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

inline std::string WideToUtf8(const std::wstring& str) {
    if (str.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}

}
