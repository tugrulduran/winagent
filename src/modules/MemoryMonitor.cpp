#include "modules/MemoryMonitor.h"
#include <iomanip>

MemoryMonitor::MemoryMonitor(int interval) : BaseMonitor(interval) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        totalGB = static_cast<double>(memInfo.ullTotalPhys) / (1024 * 1024 * 1024);
    }
}

void MemoryMonitor::update() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        usagePercentage = memInfo.dwMemoryLoad;
        usedGB = (static_cast<double>(memInfo.ullTotalPhys - memInfo.ullAvailPhys)) / (1024 * 1024 * 1024);
    }
}

void MemoryMonitor::display() const {
    std::cout << "[ RAM ] Kullanım: %" << usagePercentage.load()
              << " (" << std::fixed << std::setprecision(2) << usedGB.load() << " GB / "
              << totalGB << " GB)" << std::endl;
}