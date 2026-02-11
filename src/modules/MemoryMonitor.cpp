#include <windows.h>
#include "modules/MemoryMonitor.h"

void MemoryMonitor::init() {
    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo)) {
        dashboard_->data.memory.setTotal(memInfo.ullTotalPhys);
    }
}

void MemoryMonitor::update() {
    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo)) {
        const auto usedBytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        dashboard_->data.memory.setUsed(usedBytes);
    }
}
