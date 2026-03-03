#include "src/Common.h"
#include "src/Config.h"
#include "src/Logger.h"
#include "src/AnsiParser.h"
#include "src/ProcessManager.h"
#include "src/TrayIcon.h"
#include "src/HotkeyManager.h"
#include "src/MainWindow.h"
#include "res/resource.h"

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

namespace Launcher {

bool KillExistingLauncherInstance() {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "OpenClawLauncher_SingleInstance");
    if (hMutex == NULL) {
        return true;
    }
    
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return true;
        }
        
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        
        DWORD currentPid = GetCurrentProcessId();
        std::vector<DWORD> pidsToKill;
        
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ProcessID != currentPid) {
                    std::wstring name = pe.szExeFile;
                    std::transform(name.begin(), name.end(), name.begin(), ::towlower);
                    
                    if (name == L"openclawlauncher.exe") {
                        pidsToKill.push_back(pe.th32ProcessID);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        
        CloseHandle(hSnapshot);
        
        for (DWORD pid : pidsToKill) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
            if (hProcess) {
                TerminateProcess(hProcess, 0);
                WaitForSingleObject(hProcess, 3000);
                CloseHandle(hProcess);
            }
        }
        
        return true;
    }
    
    return false;
}

}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    typedef DPI_AWARENESS_CONTEXT (WINAPI *SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        SetProcessDpiAwarenessContext_t fn = (SetProcessDpiAwarenessContext_t)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (fn) {
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    
    HMODULE hRichEdit = LoadLibraryA("msftedit.dll");
    
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    
    Launcher::Config::Instance().Load();
    Launcher::Logger::Instance().Init();
    
    Launcher::KillExistingLauncherInstance();
    
    if (!Launcher::MainWindow::Instance().Create(hInstance)) {
        MessageBoxA(NULL, "Failed to create main window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    Launcher::TrayIcon::Instance().Init(Launcher::MainWindow::Instance().GetHwnd());
    
    if (!Launcher::HotkeyManager::Instance().Register(Launcher::MainWindow::Instance().GetHwnd())) {
        Launcher::Logger::Instance().Log("Hotkey registration failed (admin required)");
    }
    
    Launcher::ProcessManager::Instance().Init([](const std::string& msg, const std::string& color, bool isStderr) {
        Launcher::MainWindow::Instance().LogFromThread(msg, color, isStderr);
    });
    
    Launcher::MainWindow::Instance().AutoDiscover();
    
    Launcher::MainWindow::Instance().CheckAlreadyRunning();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
