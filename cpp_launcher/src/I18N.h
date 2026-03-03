#pragma once

#include <string>
#include <map>
#include <windows.h>
#include "Config.h"

namespace Launcher {

enum class Language {
    ENGLISH,
    CHINESE
};

class I18N {
public:
    static I18N& Instance() {
        static I18N instance;
        return instance;
    }

    void Initialize() {
        std::string savedLang = Config::Instance().language;
        if (savedLang == "zh" || savedLang == "chinese") {
            currentLang_ = Language::CHINESE;
        } else if (savedLang == "en" || savedLang == "english") {
            currentLang_ = Language::ENGLISH;
        } else {
            LANGID langId = GetUserDefaultUILanguage();
            if (PRIMARYLANGID(langId) == LANG_CHINESE) {
                currentLang_ = Language::CHINESE;
            } else {
                currentLang_ = Language::ENGLISH;
            }
        }
    }

    void SetLanguage(Language lang) {
        currentLang_ = lang;
        Config::Instance().language = (lang == Language::CHINESE) ? "zh" : "en";
        Config::Instance().Save();
    }

    Language GetLanguage() const {
        return currentLang_;
    }

    bool IsChinese() const {
        return currentLang_ == Language::CHINESE;
    }

    void ToggleLanguage() {
        if (currentLang_ == Language::CHINESE) {
            SetLanguage(Language::ENGLISH);
        } else {
            SetLanguage(Language::CHINESE);
        }
    }

    std::string Get(const std::string& key) {
        auto& dict = (currentLang_ == Language::CHINESE) ? zh_ : en_;
        auto it = dict.find(key);
        if (it != dict.end()) {
            return it->second;
        }
        return key;
    }

    std::wstring GetW(const std::string& key) {
        return Utf8ToWide(Get(key));
    }

private:
    I18N() {
        InitEnglish();
        InitChinese();
    }

    void InitEnglish() {
        en_ = {
            {"app_title", "OpenClaw Launcher"},
            {"menu_show", "Show Window"},
            {"menu_hide", "Hide Window"},
            {"menu_start", "Start"},
            {"menu_stop", "Stop"},
            {"menu_restart", "Restart"},
            {"menu_settings", "Settings"},
            {"menu_about", "About"},
            {"menu_clear", "Clear Terminal"},
            {"menu_language", "Language"},
            {"menu_exit", "Exit"},
            {"btn_start", "[Start]"},
            {"btn_stop", "[Stop]"},
            {"btn_restart", "[Restart]"},
            {"btn_check", "[Check]"},
            {"btn_set_hotkey", "[Hotkey]"},
            {"btn_clear", "[Clear]"},
            {"btn_startup", "Auto-start on boot"},
            {"status_online", "[Online]"},
            {"status_offline", "[Offline]"},
            {"status_starting", "[Starting...]"},
            {"info_openclaw", "> OpenClaw: "},
            {"info_config", "> Config: "},
            {"info_health", "> Health: "},
            {"info_hotkey", "> HOTKEY: "},
            {"info_pid", "> PID: "},
            {"tooltip_stopped", "OpenClaw - Stopped"},
            {"tooltip_starting", "Starting..."},
            {"tooltip_running", "OpenClaw - Running"},
            {"msg_not_found", "OpenClaw executable not found"},
            {"msg_select_file", "Please select OpenClaw executable"},
            {"msg_started", "OpenClaw started successfully"},
            {"msg_stopped", "OpenClaw stopped"},
            {"msg_restart_success", "OpenClaw restarted successfully"},
            {"msg_start_failed", "Failed to start OpenClaw"},
            {"msg_health_ok", "Health check passed"},
            {"msg_health_failed", "Health check failed"},
            {"msg_hotkey_set", "Hotkey set to: "},
            {"msg_hotkey_hint", "Press your desired key combination..."},
            {"msg_hotkey_cancel", "Hotkey setting cancelled"},
            {"msg_startup_enabled", "Auto-start enabled"},
            {"msg_startup_disabled", "Auto-start disabled"},
            {"msg_startup_conflict", "Conflicting scheduled tasks found:"},
            {"msg_open_tasks", "Open Task Scheduler"},
            {"msg_auto_restart_failed", "Auto-restart failed (attempted 2 times)"},
        };
    }

    void InitChinese() {
        zh_ = {
            {"app_title", "OpenClaw Launcher"},
            {"menu_show", "显示窗口"},
            {"menu_hide", "隐藏窗口"},
            {"menu_start", "启动"},
            {"menu_stop", "停止"},
            {"menu_restart", "重启"},
            {"menu_clear", "清空终端"},
            {"menu_language", "语言"},
            {"menu_exit", "退出"},
            {"btn_start", "[启动]"},
            {"btn_stop", "[停止]"},
            {"btn_restart", "[重启]"},
            {"btn_check", "[检查]"},
            {"btn_set_hotkey", "[设热键]"},
            {"btn_clear", "[清空]"},
            {"btn_startup", "开机启动"},
            {"status_online", "[在线]"},
            {"status_offline", "[离线]"},
            {"status_starting", "[启动中...]"},
            {"info_openclaw", "> OpenClaw: "},
            {"info_config", "> 配置文件: "},
            {"info_health", "> 健康检查: "},
            {"info_hotkey", "> 热键: "},
            {"info_pid", "> PID: "},
            {"tooltip_stopped", "OpenClaw - 已停止"},
            {"tooltip_starting", "启动中..."},
            {"tooltip_running", "OpenClaw - 运行中"},
            {"msg_not_found", "未找到 OpenClaw 可执行文件"},
            {"msg_select_file", "请选择 OpenClaw 可执行文件"},
            {"msg_started", "OpenClaw 启动成功"},
            {"msg_stopped", "OpenClaw 已停止"},
            {"msg_restart_success", "OpenClaw 重启成功"},
            {"msg_start_failed", "OpenClaw 启动失败"},
            {"msg_health_ok", "健康检查通过"},
            {"msg_health_failed", "健康检查失败"},
            {"msg_hotkey_set", "热键已设置为: "},
            {"msg_hotkey_hint", "按下你想要的组合键..."},
            {"msg_hotkey_cancel", "热键设置已取消"},
            {"msg_startup_enabled", "已启用开机自启动"},
            {"msg_startup_disabled", "已禁用开机自启动"},
            {"msg_startup_conflict", "检测到冲突的计划任务:"},
            {"msg_open_tasks", "打开任务计划程序"},
            {"msg_auto_restart_failed", "自动重启失败（已尝试2次）"},
        };
    }

    std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring wstr(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        return wstr;
    }

    Language currentLang_ = Language::ENGLISH;
    std::map<std::string, std::string> en_;
    std::map<std::string, std::string> zh_;
};

#define TR(key) Launcher::I18N::Instance().Get(key)
#define TRW(key) Launcher::I18N::Instance().GetW(key)

}
