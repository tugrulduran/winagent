#include "modules/CPUMonitor.h"

CPUMonitor::~CPUMonitor() {
    stop();
    if (cpuQuery) {
        PdhCloseQuery(cpuQuery);
    }
}

void CPUMonitor::init() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    dashboard_->data.cpu.setCores(sysInfo.dwNumberOfProcessors);

    PdhOpenQuery(NULL, NULL, &cpuQuery);
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
}

void CPUMonitor::update() {
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

    dashboard_->data.cpu.setLoad(counterVal.doubleValue);
}
