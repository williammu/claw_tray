#pragma once
#include "Common.h"
#include "Config.h"
#include <shellapi.h>
#include <fstream>

namespace Launcher {

class StartupManager {
public:
    static StartupManager& Instance() {
        static StartupManager instance;
        return instance;
    }

    bool IsTaskExists() {
        HANDLE hProcess = RunSchtasks("/query /tn OpenClawLauncher /fo list", true);
        if (!hProcess) return false;
        
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);
        
        return exitCode == 0;
    }

    std::vector<std::string> CheckOtherOpenClawTasks() {
        std::vector<std::string> tasks;
        
        HANDLE hProcess = RunSchtasks("/query /tn \"\\OpenClaw*\" /fo list", true);
        if (!hProcess) return tasks;
        
        DWORD exitCode = 0;
        WaitForSingleObject(hProcess, 5000);
        GetExitCodeProcess(hProcess, &exitCode);
        
        if (exitCode == 0) {
            char buffer[4096];
            DWORD bytesRead;
            HANDLE hReadPipe = GetStdHandle(STD_OUTPUT_HANDLE);
            
            FILE* pipe = _popen("schtasks /query /tn \"\\OpenClaw*\" /fo list 2>nul", "r");
            if (pipe) {
                while (fgets(buffer, sizeof(buffer), pipe)) {
                    std::string line = buffer;
                    size_t pos = line.find("TaskName:");
                    if (pos != std::string::npos) {
                        std::string taskName = line.substr(pos + 9);
                        while (!taskName.empty() && (taskName[0] == ' ' || taskName[0] == '\t')) {
                            taskName = taskName.substr(1);
                        }
                        while (!taskName.empty() && (taskName.back() == '\n' || taskName.back() == '\r')) {
                            taskName.pop_back();
                        }
                        if (taskName.find("OpenClawLauncher") == std::string::npos && !taskName.empty()) {
                            tasks.push_back(taskName);
                        }
                    }
                }
                _pclose(pipe);
            }
        }
        
        CloseHandle(hProcess);
        return tasks;
    }

    bool RemoveOtherOpenClawTasks() {
        std::vector<std::string> tasks = CheckOtherOpenClawTasks();
        for (const auto& task : tasks) {
            std::string cmd = "/delete /tn \"" + task + "\" /f";
            HANDLE hProcess = RunSchtasks(cmd, true);
            if (hProcess) {
                WaitForSingleObject(hProcess, 5000);
                CloseHandle(hProcess);
            }
        }
        return true;
    }

    bool CreateTask(const std::string& exePath) {
        RemoveOtherOpenClawTasks();
        RemoveTask();
        
        std::string cmd = "/create /tn OpenClawLauncher /tr \"\"" + exePath + "\"\" /sc onlogon /delay 0000:30 /rl limited /f";
        
        HANDLE hProcess = RunSchtasks(cmd, false);
        if (!hProcess) return false;
        
        DWORD exitCode = 0;
        WaitForSingleObject(hProcess, 10000);
        GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);
        
        return exitCode == 0;
    }

    bool RemoveTask() {
        HANDLE hProcess = RunSchtasks("/delete /tn OpenClawLauncher /f", true);
        if (!hProcess) return true;
        
        DWORD exitCode = 0;
        WaitForSingleObject(hProcess, 5000);
        GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);
        
        return exitCode == 0 || exitCode == 1;
    }

private:
    StartupManager() = default;

    HANDLE RunSchtasks(const std::string& args, bool hideWindow) {
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        
        if (hideWindow) {
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }
        
        std::string cmd = "schtasks.exe " + args;
        
        DWORD flags = CREATE_NO_WINDOW;
        
        if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi)) {
            return NULL;
        }
        
        CloseHandle(pi.hThread);
        return pi.hProcess;
    }
};

}
