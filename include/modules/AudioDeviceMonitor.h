#pragma once
#include <string>
#include <vector>

#include "BaseMonitor.h"

struct AudioDevice {
    int index{};
    std::wstring name;
    std::wstring deviceId;
    bool isDefault{};
};

class AudioDeviceMonitor : public BaseMonitor {
public:
    AudioDeviceMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };

    ~AudioDeviceMonitor() override { stop(); };

    void init() override;

    void update() override;

    bool setDefaultByIndex(int index);

private:
    std::vector<AudioDevice> listDevices();
    bool setDefaultById(const std::wstring& deviceId);
};