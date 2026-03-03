#pragma once
#include "Common.h"
#include "Config.h"
#include "Logger.h"
#include "AnsiParser.h"
#include <tlhelp32.h>
#include <winhttp.h>
#include <regex>
#include <functional>

#pragma comment(lib, "winhttp.lib")

namespace Launcher {

class ProcessManager {
public:
    static ProcessManager& Instance() {
        static ProcessManager instance;
        return instance;
    }

    void Init(std::function<void(const std::string&, const std::string&, bool)> logCallback) {
        logCallback_ = logCallback;
    }

    std::string FindOpenclawExecutable() {
        Config& cfg = Config::Instance();
        
        if (!cfg.openclawPath.empty() && FileExists(cfg.openclawPath)) {
            return cfg.openclawPath;
        }
        
        char path[MAX_PATH];
        DWORD len = SearchPathA(NULL, "openclaw", NULL, MAX_PATH, path, NULL);
        if (len > 0 && len < MAX_PATH) {
            return std::string(path, len);
        }
        
        const char* exts[] = {".exe", ".cmd"};
        for (const char* ext : exts) {
            std::string name = std::string("openclaw") + ext;
            len = SearchPathA(NULL, name.c_str(), NULL, MAX_PATH, path, NULL);
            if (len > 0 && len < MAX_PATH) {
                return std::string(path, len);
            }
        }
        
        return "";
    }

    std::string FindConfigFile() {
        std::string home = GetEnv("USERPROFILE");
        if (home.empty()) home = GetEnv("HOMEPATH");
        
        std::vector<std::string> paths = {
            home + "\\.openclaw\\openclaw.json"
        };
        
        for (const auto& p : paths) {
            if (FileExists(p)) return p;
        }
        
        return "";
    }

    int ReadGatewayPort(const std::string& configPath) {
        if (configPath.empty()) return DEFAULT_PORT;
        
        std::ifstream file(configPath);
        if (!file.is_open()) return DEFAULT_PORT;
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        size_t pos = content.find("\"gateway\"");
        if (pos == std::string::npos) return DEFAULT_PORT;
        
        pos = content.find("\"port\"", pos);
        if (pos == std::string::npos) return DEFAULT_PORT;
        
        pos = content.find(":", pos);
        if (pos == std::string::npos) return DEFAULT_PORT;
        
        while (pos < content.size() && (content[pos] == ':' || content[pos] == ' ' || content[pos] == '\t')) pos++;
        
        std::string numStr;
        while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
            numStr += content[pos];
            pos++;
        }
        
        return numStr.empty() ? DEFAULT_PORT : std::stoi(numStr);
    }

    std::vector<DWORD> FindGatewayPids() {
        std::vector<DWORD> pids;
        
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return pids;
        
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        
        if (Process32FirstW(snapshot, &pe)) {
            do {
                std::wstring name = pe.szExeFile;
                std::transform(name.begin(), name.end(), name.begin(), ::towlower);
                
                if (name.find(L"node") != std::string::npos) {
                    std::wstring cmdLine = GetCommandLineW(pe.th32ProcessID);
                    std::transform(cmdLine.begin(), cmdLine.end(), cmdLine.begin(), ::towlower);
                    
                    if (cmdLine.find(L"openclaw") != std::string::npos && 
                        cmdLine.find(L"gateway") != std::string::npos) {
                        pids.push_back(pe.th32ProcessID);
                    }
                }
            } while (Process32NextW(snapshot, &pe));
        }
        
        CloseHandle(snapshot);
        return pids;
    }

    bool StartProcess() {
        Config& cfg = Config::Instance();
        
        std::string nodeExe = "C:\\Program Files\\nodejs\\node.exe";
        std::string scriptPath = GetEnv("APPDATA") + "\\npm\\node_modules\\openclaw\\dist\\index.js";
        
        std::string gatewayCmd = GetEnv("USERPROFILE") + "\\.openclaw\\gateway.cmd";
        if (FileExists(gatewayCmd)) {
            std::ifstream file(gatewayCmd);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();
                
                size_t pos = content.find("node.exe");
                if (pos != std::string::npos) {
                    size_t start = content.rfind("\"", pos);
                    size_t end = content.find("\"", pos);
                    if (start != std::string::npos && end != std::string::npos) {
                        nodeExe = content.substr(start + 1, end - start - 1);
                    }
                }
            }
        }
        
        std::string cmd = "\"" + nodeExe + "\" \"" + scriptPath + "\" gateway --port=" + std::to_string(cfg.gatewayPort);
        Log("Executing: " + cmd, "default", false);
        
        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {0};
        
        std::string args = "gateway --port=" + std::to_string(cfg.gatewayPort);
        std::string cmdLine = "\"" + nodeExe + "\" \"" + scriptPath + "\" " + args;
        
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(0);
        
        DWORD creationFlags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP;
        
        HANDLE hReadOut, hWriteOut;
        HANDLE hReadErr, hWriteErr;
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        
        CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
        CreatePipe(&hReadErr, &hWriteErr, &sa, 0);
        
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hWriteOut;
        si.hStdError = hWriteErr;
        si.hStdInput = NULL;
        
        BOOL success = CreateProcessA(
            NULL,
            cmdBuf.data(),
            NULL,
            NULL,
            TRUE,
            creationFlags,
            NULL,
            NULL,
            &si,
            &pi
        );
        
        CloseHandle(hWriteOut);
        CloseHandle(hWriteErr);
        
        if (!success) {
            Log("Failed to start process: " + std::to_string(GetLastError()), "red", false);
            return false;
        }
        
        processHandle_ = pi.hProcess;
        pid_ = pi.dwProcessId;
        readyDetected_ = false;
        
        Log("Node process started (PID: " + std::to_string(pid_) + ")", "green", false);
        
        std::thread(&ProcessManager::ReadOutput, this, hReadOut, "", false).detach();
        std::thread(&ProcessManager::ReadOutput, this, hReadErr, "[stderr] ", true).detach();
        
        return true;
    }

    bool StopProcess() {
        std::vector<DWORD> pids = FindGatewayPids();
        
        if (!pids.empty()) {
            Log("Found " + std::to_string(pids.size()) + " related processes: " + PidsToString(pids), "default", false);
            
            for (DWORD pid : pids) {
                std::string cmd = "taskkill /T /F /PID " + std::to_string(pid);
                Log("Executing: " + cmd, "default", false);
                
                STARTUPINFOA si = {0};
                si.cb = sizeof(si);
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                
                PROCESS_INFORMATION pi = {0};
                std::string cmdLine = cmd;
                std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
                cmdBuf.push_back(0);
                
                if (CreateProcessA(
                    NULL,
                    cmdBuf.data(),
                    NULL,
                    NULL,
                    FALSE,
                    CREATE_NO_WINDOW,
                    NULL,
                    NULL,
                    &si,
                    &pi
                )) {
                    WaitForSingleObject(pi.hProcess, 5000);
                    if (pi.hProcess) CloseHandle(pi.hProcess);
                    if (pi.hThread) CloseHandle(pi.hThread);
                }
            }
            
            Sleep(100);
            
            pids = FindGatewayPids();
            if (pids.empty()) {
                Log("OpenClaw stopped", "green", false);
            } else {
                Log("Warning: Some processes may still be running", "yellow", false);
            }
        } else {
            Log("No related processes found", "yellow", false);
        }
        
        if (processHandle_) {
            CloseHandle(processHandle_);
            processHandle_ = NULL;
        }
        pid_ = 0;
        
        return true;
    }

    bool HealthCheck(int timeoutMs = 2000) {
        Config& cfg = Config::Instance();
        
        HINTERNET hSession = WinHttpOpen(
            L"OpenClaw-Launcher/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        
        if (!hSession) {
            Logger::Instance().Log("HealthCheck: WinHttpOpen failed", "DEBUG");
            return false;
        }
        
        std::wstring server = Utf8ToWide("127.0.0.1");
        HINTERNET hConnect = WinHttpConnect(
            hSession,
            server.c_str(),
            cfg.gatewayPort,
            0
        );
        
        if (!hConnect) {
            Logger::Instance().Log("HealthCheck: WinHttpConnect failed for port " + std::to_string(cfg.gatewayPort), "DEBUG");
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            L"/",
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0
        );
        
        if (!hRequest) {
            Logger::Instance().Log("HealthCheck: WinHttpOpenRequest failed", "DEBUG");
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        BOOL result = WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        );
        
        if (!result) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        result = WinHttpReceiveResponse(hRequest, NULL);
        if (!result) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &size,
            WINHTTP_NO_HEADER_INDEX
        );
        
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        return statusCode == 200;
    }

    bool IsProcessRunning() {
        if (pid_ == 0) return false;
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid_);
        if (!hProcess) return false;
        
        DWORD exitCode;
        BOOL result = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);
        
        return result && exitCode == STILL_ACTIVE;
    }

    DWORD GetPid() const { return pid_; }
    
    bool WaitForReady(int timeoutMs = 5000) {
        auto start = std::chrono::steady_clock::now();
        while (!readyDetected_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }
            Sleep(100);
        }
        return true;
    }
    
    bool CheckReadyWithRetry(int maxRetries = 3, int retryDelayMs = 1000) {
        for (int i = 0; i < maxRetries; i++) {
            if (HealthCheck()) {
                return true;
            }
            if (i < maxRetries - 1) {
                Sleep(retryDelayMs);
            }
        }
        return false;
    }

private:
    HANDLE processHandle_ = NULL;
    DWORD pid_ = 0;
    std::function<void(const std::string&, const std::string&, bool)> logCallback_;
    std::atomic<bool> stopOutput_{false};
    std::atomic<bool> readyDetected_{false};

    ProcessManager() = default;

    void Log(const std::string& msg, const std::string& color, bool isStderr) {
        if (logCallback_) {
            logCallback_(msg, color, isStderr);
        }
    }

    void ReadOutput(HANDLE hPipe, const std::string& prefix, bool isStderr) {
        char buffer[4096];
        DWORD bytesRead;
        
        while (true) {
            if (!ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) || bytesRead == 0) {
                break;
            }
            
            buffer[bytesRead] = 0;
            std::string text(buffer);
            
            std::istringstream stream(text);
            std::string line;
            while (std::getline(stream, line)) {
                if (!prefix.empty() && line.find(prefix) == 0) {
                    line = line.substr(prefix.length());
                }
                
                std::string color = "default";
                if (line.find("\033[31m") != std::string::npos || line.find("\033[91m") != std::string::npos) {
                    color = "red";
                } else if (line.find("\033[32m") != std::string::npos || line.find("\033[92m") != std::string::npos) {
                    color = "green";
                } else if (line.find("\033[33m") != std::string::npos || line.find("\033[93m") != std::string::npos) {
                    color = "yellow";
                } else if (line.find("\033[34m") != std::string::npos || line.find("\033[94m") != std::string::npos) {
                    color = "blue";
                } else if (line.find("\033[35m") != std::string::npos || line.find("\033[95m") != std::string::npos) {
                    color = "magenta";
                } else if (line.find("\033[36m") != std::string::npos || line.find("\033[96m") != std::string::npos) {
                    color = "cyan";
                }
                
                std::string fullLine = prefix + line;
                Log(fullLine, color, isStderr);
                
                std::string lowerLine = line;
                std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                if (lowerLine.find("listening") != std::string::npos || 
                    lowerLine.find("gateway started") != std::string::npos ||
                    lowerLine.find("ready") != std::string::npos) {
                    readyDetected_ = true;
                }
            }
        }
        
        CloseHandle(hPipe);
    }

    std::wstring GetCommandLineW(DWORD pid) {
        std::wstring result;
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) return result;
        
        typedef LONG(NTAPI* pNtQueryInformationProcess)(
            HANDLE, DWORD, PVOID, ULONG, PULONG);
        
        HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
        if (hNtDll) {
            pNtQueryInformationProcess NtQueryInformationProcess =
                (pNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");
            
            if (NtQueryInformationProcess) {
                struct PROCESS_BASIC_INFORMATION {
                    PVOID Reserved1;
                    PVOID PebBaseAddress;
                    PVOID Reserved2[2];
                    ULONG_PTR UniqueProcessId;
                    PVOID Reserved3;
                } pbi;
                
                ULONG returnLength;
                LONG status = NtQueryInformationProcess(
                    hProcess, 0, &pbi, sizeof(pbi), &returnLength);
                
                if (status >= 0 && pbi.PebBaseAddress) {
                    struct PEB {
                        BYTE Reserved1[2];
                        BYTE BeingDebugged;
                        BYTE Reserved2[1];
                        PVOID Reserved3[2];
                        struct {
                            ULONG Length;
                            ULONG MaximumLength;
                            PVOID Buffer;
                        } CommandLine;
                    } peb;
                    
                    if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(peb), NULL)) {
                        if (peb.CommandLine.Buffer && peb.CommandLine.Length > 0) {
                            result.resize(peb.CommandLine.Length / 2);
                            if (ReadProcessMemory(hProcess, peb.CommandLine.Buffer,
                                &result[0], peb.CommandLine.Length, NULL)) {
                            }
                        }
                    }
                }
            }
        }
        
        CloseHandle(hProcess);
        return result;
    }

    std::string PidsToString(const std::vector<DWORD>& pids) {
        std::string result;
        for (size_t i = 0; i < pids.size(); i++) {
            if (i > 0) result += ", ";
            result += std::to_string(pids[i]);
        }
        return result;
    }
};

}
