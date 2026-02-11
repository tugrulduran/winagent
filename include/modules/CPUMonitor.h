#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include <pdh.h>
#include "BaseMonitor.h"

class CPUMonitor : public BaseMonitor {
public:
    CPUMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };

    ~CPUMonitor() override;

    void init() override;

    void update() override;

private:
    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuTotal = nullptr;
};

#endif
