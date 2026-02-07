#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <pdh.h>
#include <mutex>
#include <string>

/*
 * CPUData
 * -------
 * Plain data container returned by CPUMonitor::getData().
 */
struct CPUData {
    double load = 0.0;          // CPU usage percent (0..100)
    int cores = 0;              // logical core count
    std::wstring cpuName = L""; // reserved for future use
};

class CPUMonitor : public BaseMonitor {
private:
    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;
    CPUData data;

    // Protects 'data'. Mutable so getData() can lock in a const method.
    mutable std::mutex dataMutex;

public:
    CPUMonitor(int interval = 1000);
    ~CPUMonitor();

    void init() override;
    void update() override;

    // Returns a copy of the latest CPU snapshot (thread-safe).
    CPUData getData() const;
};

#endif