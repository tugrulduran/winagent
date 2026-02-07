#ifndef PROCESSMONITOR_H
#define PROCESSMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <string>
#include <mutex>

/*
 * ProcessMonitor
 * --------------
 * Tracks the currently focused (foreground) application.
 *
 * How it works:
 * - Uses GetForegroundWindow() to get the active window.
 * - Uses GetWindowThreadProcessId() to get the owning process ID.
 * - Uses QueryFullProcessImageNameA() to read the EXE path.
 * - Stores only the EXE filename (example: "chrome.exe").
 *
 * Thread-safety:
 * - update() writes under a mutex.
 * - getData() returns a copy under the same mutex.
 */

struct ProcessData {
    std::string activeProcessName = "unknown";
};

class ProcessMonitor : public BaseMonitor {
private:
    ProcessData data;
    mutable std::mutex dataMutex;

public:
    ProcessMonitor(int interval = 1000) : BaseMonitor(interval) {}
    
    void init() override {} // No special initialization is needed.

    void update() override {
        char buffer[MAX_PATH] = "";
        std::string currentApp = "unknown";

        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            DWORD processID;
            GetWindowThreadProcessId(hwnd, &processID);
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
            
            if (hProcess) {
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameA(hProcess, 0, buffer, &size)) {
                    std::string fullPath(buffer);
                    size_t lastSlash = fullPath.find_last_of("\\");
                    currentApp = fullPath.substr(lastSlash + 1);
                }
                CloseHandle(hProcess);
            }
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        data.activeProcessName = currentApp;
    }

    ProcessData getData() const {
        std::lock_guard<std::mutex> lock(dataMutex);
        return data;
    }
};

#endif