#ifndef MEMORYMONITOR_H
#define MEMORYMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <mutex>

// RAM verilerini ham (raw) halde tutan struct
struct MemoryData {
    double totalGB = 0.0;
    double usedGB = 0.0;
    double freeGB = 0.0;
    int usagePercentage = 0;
};

class MemoryMonitor : public BaseMonitor {
private:
    MemoryData data;
    mutable std::mutex dataMutex; // Const metod içinde kilit kullanabilmek için mutable

public:
    MemoryMonitor(int interval = 1000);
    ~MemoryMonitor() override;

    // BaseMonitor arayüzü
    void init() override;
    void update() override;

    // Ham veriyi dönen getter
    MemoryData getData() const;
};

#endif