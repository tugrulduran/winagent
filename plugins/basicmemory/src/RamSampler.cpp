#include "RamSampler.h"
#include <pdh.h>

#include <cstdint>

namespace basicmemory {
    RamSampler::RamSampler() {
        init();
    }

    void RamSampler::init() {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);

        // Read total RAM once at startup.
        if (GlobalMemoryStatusEx(&memInfo)) {
            totalMemory = memInfo.ullTotalPhys;
        }
    }

    uint64_t RamSampler::getAvailableMemory() {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);

        if (GlobalMemoryStatusEx(&memInfo)) {
            return memInfo.ullAvailPhys;
        }
        return 0;
    }

    uint64_t RamSampler::getTotalMemory() {
        return totalMemory;
    }
}
