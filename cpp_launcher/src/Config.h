#pragma once
#include "Common.h"

namespace Launcher {

class Config {
public:
    std::string openclawPath;
    std::string openclawConfigPath;
    int gatewayPort = DEFAULT_PORT;
    bool autoDetect = true;
    std::string readyPattern = "listening|ready|Gateway started";
    std::string healthCheckUrl = "/";
    int healthCheckTimeout = 5;
    std::string healthCheckKeyword = "openclaw";
    int maxStartAttempts = 2;
    int retryDelay = 3;
    std::string terminalFont = "Consolas";
    int terminalFontSize = 11;
    int logMaxLines = 10000;
    bool autoStart = false;
    bool autoMinimize = false;
    std::string appearanceMode = "System";
    std::string windowHotkey = "ctrl+shift+o";
    bool windowsStartup = false;
    std::string language = "";

    static Config& Instance() {
        static Config instance;
        return instance;
    }

    bool Load() {
        std::string configPath = GetConfigPath();
        if (!FileExists(configPath)) {
            Save();
            return true;
        }

        std::ifstream file(configPath);
        if (!file.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        ParseJson(content);
        return true;
    }

    bool Save() {
        std::string configPath = GetConfigPath();
        std::string dir = configPath.substr(0, configPath.find_last_of("\\/"));
        CreateDirRecursive(dir);

        std::ofstream file(configPath);
        if (!file.is_open()) return false;

        file << "{\n";
        file << "    \"openclaw_path\": \"" << EscapeJson(openclawPath) << "\",\n";
        file << "    \"openclaw_config_path\": \"" << EscapeJson(openclawConfigPath) << "\",\n";
        file << "    \"gateway_port\": " << gatewayPort << ",\n";
        file << "    \"auto_detect\": " << (autoDetect ? "true" : "false") << ",\n";
        file << "    \"ready_pattern\": \"" << EscapeJson(readyPattern) << "\",\n";
        file << "    \"health_check_url\": \"" << EscapeJson(healthCheckUrl) << "\",\n";
        file << "    \"health_check_timeout\": " << healthCheckTimeout << ",\n";
        file << "    \"health_check_keyword\": \"" << EscapeJson(healthCheckKeyword) << "\",\n";
        file << "    \"max_start_attempts\": " << maxStartAttempts << ",\n";
        file << "    \"retry_delay\": " << retryDelay << ",\n";
        file << "    \"terminal_font\": \"" << EscapeJson(terminalFont) << "\",\n";
        file << "    \"terminal_font_size\": " << terminalFontSize << ",\n";
        file << "    \"log_max_lines\": " << logMaxLines << ",\n";
        file << "    \"auto_start\": " << (autoStart ? "true" : "false") << ",\n";
        file << "    \"auto_minimize\": " << (autoMinimize ? "true" : "false") << ",\n";
        file << "    \"appearance_mode\": \"" << EscapeJson(appearanceMode) << "\",\n";
        file << "    \"window_hotkey\": \"" << EscapeJson(windowHotkey) << "\",\n";
        file << "    \"windows_startup\": " << (windowsStartup ? "true" : "false") << ",\n";
        file << "    \"language\": \"" << EscapeJson(language) << "\"\n";
        file << "}\n";

        file.close();
        return true;
    }

    std::string GetConfigPath() {
        return GetAppDataPath() + "\\config.json";
    }

private:
    Config() = default;

    std::string EscapeJson(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }

    std::string ExtractString(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";
        
        size_t start = json.find("\"", pos);
        if (start == std::string::npos) return "";
        start++;
        
        size_t end = start;
        while (end < json.size()) {
            if (json[end] == '\\' && end + 1 < json.size()) {
                end += 2;
                continue;
            }
            if (json[end] == '"') break;
            end++;
        }
        
        std::string result = json.substr(start, end - start);
        std::string unescaped;
        for (size_t i = 0; i < result.size(); i++) {
            if (result[i] == '\\' && i + 1 < result.size()) {
                switch (result[i + 1]) {
                    case '"': unescaped += '"'; i++; break;
                    case '\\': unescaped += '\\'; i++; break;
                    case 'n': unescaped += '\n'; i++; break;
                    case 'r': unescaped += '\r'; i++; break;
                    case 't': unescaped += '\t'; i++; break;
                    default: unescaped += result[i]; break;
                }
            } else {
                unescaped += result[i];
            }
        }
        return unescaped;
    }

    int ExtractInt(const std::string& json, const std::string& key, int defaultVal = 0) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return defaultVal;
        
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        std::string numStr;
        while (pos < json.size() && (json[pos] >= '0' && json[pos] <= '9' || json[pos] == '-')) {
            numStr += json[pos];
            pos++;
        }
        
        return numStr.empty() ? defaultVal : std::stoi(numStr);
    }

    bool ExtractBool(const std::string& json, const std::string& key, bool defaultVal = false) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return defaultVal;
        
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        if (json.substr(pos, 4) == "true") return true;
        if (json.substr(pos, 5) == "false") return false;
        return defaultVal;
    }

    void ParseJson(const std::string& json) {
        openclawPath = ExtractString(json, "openclaw_path");
        openclawConfigPath = ExtractString(json, "openclaw_config_path");
        gatewayPort = ExtractInt(json, "gateway_port", DEFAULT_PORT);
        autoDetect = ExtractBool(json, "auto_detect", true);
        readyPattern = ExtractString(json, "ready_pattern");
        if (readyPattern.empty()) readyPattern = "listening|ready|Gateway started";
        healthCheckUrl = ExtractString(json, "health_check_url");
        if (healthCheckUrl.empty()) healthCheckUrl = "/";
        healthCheckTimeout = ExtractInt(json, "health_check_timeout", 5);
        healthCheckKeyword = ExtractString(json, "health_check_keyword");
        if (healthCheckKeyword.empty()) healthCheckKeyword = "openclaw";
        maxStartAttempts = ExtractInt(json, "max_start_attempts", 2);
        retryDelay = ExtractInt(json, "retry_delay", 3);
        terminalFont = ExtractString(json, "terminal_font");
        if (terminalFont.empty()) terminalFont = "Consolas";
        terminalFontSize = ExtractInt(json, "terminal_font_size", 11);
        logMaxLines = ExtractInt(json, "log_max_lines", 10000);
        autoStart = ExtractBool(json, "auto_start", false);
        autoMinimize = ExtractBool(json, "auto_minimize", false);
        appearanceMode = ExtractString(json, "appearance_mode");
        if (appearanceMode.empty()) appearanceMode = "System";
        windowHotkey = ExtractString(json, "window_hotkey");
        if (windowHotkey.empty()) windowHotkey = "ctrl+shift+o";
        windowsStartup = ExtractBool(json, "windows_startup", false);
        language = ExtractString(json, "language");
    }
};

}
