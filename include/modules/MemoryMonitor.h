#ifndef MEMORYMONITOR_H
#define MEMORYMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <mutex>

/*
 * MemoryData
 * ----------
 * Snapshot of physical memory (RAM) usage in gigabytes.
 */
struct MemoryData {
    double totalGB = 0.0;
    double usedGB = 0.0;
    double freeGB = 0.0;
    int usagePercentage = 0; // 0..100
};

class MemoryMonitor : public BaseMonitor {
private:
    MemoryData data;

    // Protects 'data'. Mutable so getData() can lock in a const method.
    mutable std::mutex dataMutex;

public:
    MemoryMonitor(int interval = 1000);
    ~MemoryMonitor() override;

    void init() override;
    void update() override;

    // Returns a copy of the latest RAM snapshot (thread-safe).
    MemoryData getData() const;
};

#endif