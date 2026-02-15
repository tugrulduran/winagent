#pragma once
#include <cstdint>

namespace basicmemory {
    class RamSampler {
    public:
        RamSampler();

        uint64_t getAvailableMemory();
        uint64_t getTotalMemory();

    private:
        void init();

        uint64_t totalMemory;
        uint64_t availableMemory;
    };
}
