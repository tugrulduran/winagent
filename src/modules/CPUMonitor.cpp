#include "modules/CPUMonitor.h"

CPUMonitor::CPUMonitor(int interval) : BaseMonitor(interval) {}

CPUMonitor::~CPUMonitor() {
    stop();
    if (cpuQuery) {
        PdhCloseQuery(cpuQuery);
    }
}

void CPUMonitor::init() {
    // 1. Çekirdek Sayısı
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    data.cores = sysInfo.dwNumberOfProcessors;

    // 2. PDH Sorgusu Hazırlığı
    PdhOpenQuery(NULL, NULL, &cpuQuery);
    // Not: PdhAddEnglishCounter kullanmak dil bağımsızlığı için kritiktir
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
}

void CPUMonitor::update() {
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

    // Thread-safe yazma
    std::lock_guard<std::mutex> lock(dataMutex);
    data.load = counterVal.doubleValue;
}

CPUData CPUMonitor::getData() const {
    // Thread-safe okuma
    std::lock_guard<std::mutex> lock(dataMutex);
    return data;
}