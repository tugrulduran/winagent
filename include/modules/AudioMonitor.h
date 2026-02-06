#ifndef AUDIOMONITOR_H
#define AUDIOMONITOR_H

#include "BaseMonitor.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <vector>
#include <string>
#include <mutex>
#include <set>

#pragma pack(push, 1) // Bellek hizalamasını kapat (Kritik!)
struct AudioPacket {
    uint32_t pid;
    wchar_t name[25]; // 25 karakter * 2 byte = 50 byte
    float volume;
};
#pragma pack(pop)

struct AppAudioData {
    uint32_t pid;
    std::wstring name;
    float volume;
    bool isMuted;
};

class AudioMonitor : public BaseMonitor {
private:
    std::vector<AppAudioData> apps;
    float masterVolume = 0.0f;
    bool masterMuted = false;
    mutable std::mutex dataMutex;

    // Engellenecek uygulama isimleri listesi
    std::set<std::wstring> ignoreList = {
        L"Armoury Crate Service",
        L"ArmouryCrate.UserSessionHelper",
        L"ASUS Optimization",
        L"System" // Windows sistem seslerini de istersen buraya ekleyebilirsin
    };

    std::wstring GetFriendlyName(const std::wstring& filePath);
    bool IsIgnored(const std::wstring& name);

public:
    AudioMonitor(int interval);
    ~AudioMonitor();
    void update() override;
    void display() const override;

    // EKSİK OLAN METOT:
    std::vector<AppAudioData> getApps() const {
        std::lock_guard<std::mutex> lock(dataMutex);
        return apps;
    }

    void setVolumeByPID(uint32_t targetPid, float newVolume);
};

#endif