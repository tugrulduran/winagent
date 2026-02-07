#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <pdh.h>
#include <mutex>
#include <string>

// CPU Modülüne özel veri yapısı
struct CPUData {
    double load = 0.0;
    int cores = 0;
    std::wstring cpuName = L"";
};

class CPUMonitor : public BaseMonitor {
private:
    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;
    CPUData data;
    mutable std::mutex dataMutex; // getData const olduğu için mutable

public:
    CPUMonitor(int interval = 1000);
    ~CPUMonitor();

    void init() override;
    void update() override;

    // String değil, doğrudan struct döner
    CPUData getData() const;
};

#endif