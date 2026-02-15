#include "BasePlugin.h"
#include "CpuSampler.h"

#include <cstdint>

namespace basiccpu {
    CpuSampler::CpuSampler() {
        init();
    }

    CpuSampler::~CpuSampler() {
        if (cpuQuery) {
            PdhCloseQuery(cpuQuery);
        }
    }

    void CpuSampler::init() {
        PdhOpenQuery(NULL, 0, &cpuQuery);
        PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
    }

    int CpuSampler::getCpuCores() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        return static_cast<uint8_t>(sysInfo.dwNumberOfProcessors);
    }

    double CpuSampler::getCpuUsage() {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

        return counterVal.doubleValue;
    }
}
