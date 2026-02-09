#ifndef MODULEFACTORY_H
#define MODULEFACTORY_H

#include <memory>
#include <vector>
#include "BaseMonitor.h"
#include "modules/AudezeMonitor.h"
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/MediaMonitor.h"
#include "modules/NetworkBytes.h"
#include "modules/AudioMonitor.h"
#include "modules/ProcessMonitor.h"

/*
 * ModuleFactory
 * -------------
 * Central place that decides:
 * - which monitors exist
 * - which update interval each monitor uses
 *
 * Why a factory?
 * - MainWindow does not need to know constructor details.
 * - Adding a new module becomes a small, localized change.
 */
enum class ModuleType { CPU, RAM, MEDIA, NETWORK, AUDIO, PROCESS, AUDEZE };

class ModuleFactory {
public:
    static std::unique_ptr<BaseMonitor> create_module(ModuleType type) {
        switch (type) {
            case ModuleType::CPU:
                return std::make_unique<CPUMonitor>(1000);
            case ModuleType::RAM:
                return std::make_unique<MemoryMonitor>(10000);
            case ModuleType::MEDIA:
                return std::make_unique<MediaMonitor>(1000);
            case ModuleType::NETWORK:
                return std::make_unique<NetworkBytes>(1000);
            case ModuleType::AUDIO:
                return std::make_unique<AudioMonitor>(1000);
            case ModuleType::PROCESS:
                return std::make_unique<ProcessMonitor>(5000);
            case ModuleType::AUDEZE:
                return std::make_unique<AudezeMonitor>(60000);
            default:
                return nullptr;
        }
    }

    static std::vector<std::unique_ptr<BaseMonitor>> createAll() {
        std::vector<std::unique_ptr<BaseMonitor>> monitors;

        monitors.push_back(create_module(ModuleType::CPU));
        monitors.push_back(create_module(ModuleType::RAM));
        monitors.push_back(create_module(ModuleType::MEDIA));
        monitors.push_back(create_module(ModuleType::NETWORK));
        monitors.push_back(create_module(ModuleType::AUDIO));
        monitors.push_back(create_module(ModuleType::PROCESS));
        monitors.push_back(create_module(ModuleType::AUDEZE));

        return monitors;
    }
};

#endif