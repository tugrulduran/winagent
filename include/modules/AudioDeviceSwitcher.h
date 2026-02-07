#pragma once
#include <string>
#include <vector>

struct AudioDevice {
    int index{};
    std::wstring name;
    std::wstring deviceId;
    bool isDefault{};
};

class AudioDeviceSwitcher {
public:
    static std::vector<AudioDevice> listDevices();
    static bool setDefaultByIndex(int index);
    static bool setDefaultById(const std::wstring& deviceId);
};
