#include "modules/CPUMonitor.h"
#include <iomanip>

CPUMonitor::CPUMonitor(int interval) : BaseMonitor(interval) {
    // 1. Statik Bilgi: Çekirdek Sayısını Al
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    coreCount = sysInfo.dwNumberOfProcessors;

    // 2. Dinamik Bilgi: PDH Sayaçlarını Hazırla
    PdhOpenQuery(NULL, NULL, &cpuQuery);
    PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
}

CPUMonitor::~CPUMonitor() {
    PdhCloseQuery(cpuQuery);
}

void CPUMonitor::update() {
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    lastValue = counterVal.doubleValue;
}

void CPUMonitor::display() const {
    std::cout << "[CPU] Cekirdek Sayisi: " << coreCount << std::endl;
    std::cout << "[CPU] Toplam Kullanim: %" << std::fixed << std::setprecision(2)
              << lastValue.load() << "   " << std::endl;
    // Sona boşluk ekledik ki ResetCursor sonrası eski karakter kalmasın
}