#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <pdh.h>

class CPUMonitor : public BaseMonitor {
private:
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuTotal;
    std::atomic<double> lastValue{0.0};
    int coreCount = 0; // Bir kez okunacak

public:
    CPUMonitor(int interval);

    ~CPUMonitor();

    void update() override;

    void display() const override;

    double getLastValue() const { return lastValue.load(); }
};

#endif
