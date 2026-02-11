#ifndef PROCESSMONITOR_H
#define PROCESSMONITOR_H

#include "BaseMonitor.h"

class ProcessMonitor : public BaseMonitor {
public:
    ProcessMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };

    ~ProcessMonitor() override { stop(); }

    void init() override {
    };

    void update() override;
};

#endif
