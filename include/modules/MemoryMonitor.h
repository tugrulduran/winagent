#ifndef MEMORYMONITOR_H
#define MEMORYMONITOR_H

#include "BaseMonitor.h"

class MemoryMonitor : public BaseMonitor {
public:
    MemoryMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };

    ~MemoryMonitor() override { stop(); }

    void init() override;

    void update() override;
};

#endif
