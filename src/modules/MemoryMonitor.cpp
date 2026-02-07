#include "modules/MemoryMonitor.h"

MemoryMonitor::MemoryMonitor(int interval) : BaseMonitor(interval) {}

MemoryMonitor::~MemoryMonitor() {
    stop();
}

void MemoryMonitor::init() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        std::lock_guard<std::mutex> lock(dataMutex);
        // Toplam bellek miktarını GB cinsinden bir kez hesaplıyoruz
        data.totalGB = static_cast<double>(memInfo.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    }
}

void MemoryMonitor::update() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo)) {
        std::lock_guard<std::mutex> lock(dataMutex);

        data.usagePercentage = static_cast<int>(memInfo.dwMemoryLoad);

        // Kullanılan miktar: Toplam - Kullanılabilir
        unsigned long long usedBytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        data.usedGB = static_cast<double>(usedBytes) / (1024.0 * 1024.0 * 1024.0);
        data.freeGB = static_cast<double>(memInfo.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
    }
}

MemoryData MemoryMonitor::getData() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return data; // Struct kopyası döner, thread-safe'dir.
}