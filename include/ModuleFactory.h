#include <memory>
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
// #include "modules/DiskMonitor.h"

enum class ModuleType { CPU, RAM, DISK };

std::unique_ptr<BaseMonitor> create_module(ModuleType type, int interval) {
    switch (type) {
        case ModuleType::CPU:
            return std::make_unique<CPUMonitor>(interval);
        case ModuleType::RAM:
            return std::make_unique<MemoryMonitor>(interval);
        default:
            return nullptr;
    }
}