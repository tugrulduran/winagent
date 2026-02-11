#include <string>
#include <windows.h>
#include <shellapi.h>
#include "modules/LauncherMonitor.h"
#include "modules/ProcessMonitor.h"

void LauncherMonitor::init() {
    for (const auto &a:actions) {
        dashboard_->data.launcher.add(a.id, a.name, a.execPath);
    }
}

void LauncherMonitor::executeAction(uint32_t id) {
    for (const auto &a: dashboard_->data.launcher.getActions()) {
        if (a.id == id) {
            ShellExecuteA(NULL, "open", a.execPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        }
    }
}