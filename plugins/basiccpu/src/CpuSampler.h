#pragma once
#include <pdh.h>

namespace basiccpu {
    class CpuSampler {
    public:
        CpuSampler();

        ~CpuSampler();

        int getCpuCores();

        double getCpuUsage();

    private:
        void init();

        PDH_HQUERY cpuQuery = nullptr;
        PDH_HCOUNTER cpuTotal = nullptr;
    };
}
