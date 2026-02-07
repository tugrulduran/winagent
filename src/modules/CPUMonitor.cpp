#include "modules/CPUMonitor.h"

CPUMonitor::CPUMonitor(int interval) : BaseMonitor(interval) {}

CPUMonitor::~CPUMonitor() {
    stop();
    if (cpuQuery) {
        PdhCloseQuery(cpuQuery);
    }
}

void CPUMonitor::init() {
    // Read logical CPU core count once at startup.
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    data.cores = sysInfo.dwNumberOfProcessors;

    // Setup a PDH query that reads total CPU time.
    PdhOpenQuery(NULL, NULL, &cpuQuery);

    // Use English counter name to avoid locale issues on Windows.
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);

    // First collection "primes" the counter.
    PdhCollectQueryData(cpuQuery);
}

void CPUMonitor::update() {
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

    // Publish latest value safely.
    std::lock_guard<std::mutex> lock(dataMutex);
    data.load = counterVal.doubleValue;
}

CPUData CPUMonitor::getData() const {
    // Return a copy so callers do not need to hold the mutex.
    std::lock_guard<std::mutex> lock(dataMutex);
    return data;
}