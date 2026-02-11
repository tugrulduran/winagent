#include <windows.h>
#include <string>
#include "modules/ProcessMonitor.h"

void ProcessMonitor::update() {
    char buffer[MAX_PATH] = "";
    DWORD processID = 0;
    std::string currentApp = "unknown";

    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
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

    dashboard_->data.process.setActiveProcess(processID, currentApp);
}
