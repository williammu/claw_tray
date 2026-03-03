#pragma once
#include "Common.h"
#include "Config.h"
#include <mutex>
#include <deque>
#include <regex>

namespace Launcher {

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Init() {
        std::string logPath = GetLogPath();
        std::string dir = logPath.substr(0, logPath.find_last_of("\\/"));
        CreateDirRecursive(dir);
        TruncateOnStartup();
    }

    void Log(const std::string& message, const std::string& level = "INFO") {
        std::string cleanMsg = StripAnsi(message);
        std::string line = "[" + GetTimestamp() + "] [" + level + "] " + cleanMsg + "\n";
        
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            std::ofstream file(GetLogPath(), std::ios::app);
            if (file.is_open()) {
                file << line;
            }
        } catch (...) {}
    }

    std::string GetLogPath() {
        return GetAppDataPath() + "\\launcher.log";
    }

private:
    std::mutex mutex_;

    Logger() = default;

    void TruncateOnStartup() {
        std::string logPath = GetLogPath();
        if (!FileExists(logPath)) return;

        std::ifstream inFile(logPath);
        if (!inFile.is_open()) return;

        std::deque<std::string> lines;
        std::string line;
        while (std::getline(inFile, line)) {
            lines.push_back(line);
            if (lines.size() > static_cast<size_t>(Config::Instance().logMaxLines)) {
                lines.pop_front();
            }
        }
        inFile.close();

        std::ofstream outFile(logPath);
        if (!outFile.is_open()) return;
        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        outFile.close();
    }

    std::string StripAnsi(const std::string& text) {
        static std::regex ansiRegex("\033\\[[0-9;]*m");
        return std::regex_replace(text, ansiRegex, "");
    }
};

}
