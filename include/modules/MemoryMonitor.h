#ifndef MEMORYMONITOR_H
#define MEMORYMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>

class MemoryMonitor : public BaseMonitor {
private:
    std::atomic<int> usagePercentage{0};
    std::atomic<double> usedGB{0.0};
    double totalGB = 0.0;

public:
    MemoryMonitor(int interval);

    void update() override;

    void display() const override;

    double getUsedGB() const { return usedGB.load(); }
    uint8_t getPercent() const { return static_cast<uint8_t>(usagePercentage.load()); }
};

#endif
