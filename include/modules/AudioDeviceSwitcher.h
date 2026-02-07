#pragma once
#include <string>
#include <vector>

/*
 * AudioDeviceSwitcher
 * -------------------
 * Utility for listing audio output devices and setting the default device.
 *
 * Notes:
 * - Uses Windows Core Audio APIs.
 * - "Set default device" uses an undocumented interface (IPolicyConfig).
 * - Methods are static because this is used like a toolbox, not as a long-running monitor.
 */

struct AudioDevice {
    int index{};
    std::wstring name;
    std::wstring deviceId;
    bool isDefault{};
};

class AudioDeviceSwitcher {
public:
    static std::vector<AudioDevice> listDevices();
    static bool setDefaultById(const std::wstring& deviceId);
    static bool setDefaultByIndex(int index);
};