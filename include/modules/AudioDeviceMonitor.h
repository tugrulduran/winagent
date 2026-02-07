#pragma once

#include <windows.h>
#include <mmdeviceapi.h>

#include <string>
#include <vector>
#include <mutex>

// =======================================================
//  AUDIO DEVICE INFO
// =======================================================

struct AudioDeviceInfo {
    int index = -1;
    std::wstring name;      // SABİT DİZİ YERİNE GÜVENLİ
    std::wstring deviceId;
    bool isDefault = false;
};

// =======================================================
//  AUDIO DEVICE MONITOR
// =======================================================

class AudioDeviceMonitor {
private:
    static std::mutex deviceMutex;

public:
    static std::vector<AudioDeviceInfo> getDevices();
    static void setDefaultDevice(int index);
};
