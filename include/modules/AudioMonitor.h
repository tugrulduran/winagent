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

// Her bir uygulama için veri yapısı
struct AppAudioData {
    uint32_t pid;
    std::wstring name;
    float volume;
    bool isMuted;
};

// MainWindow'un tek seferde alacağı tüm ses tablosu
struct AudioSnapshot {
    float masterVolume = 0.0f;
    bool masterMuted = false;
    std::vector<AppAudioData> apps;
};

class AudioMonitor : public BaseMonitor {
private:
    AudioSnapshot currentSnapshot;
    mutable std::mutex dataMutex;

    std::set<std::wstring> ignoreList = {
        L"Armoury Crate Service", L"ArmouryCrate.UserSessionHelper",
        L"ASUS Optimization", L"System"
    };

    std::wstring GetFriendlyName(const std::wstring& filePath);
    bool IsIgnored(const std::wstring& name);

public:
    AudioMonitor(int interval = 1000);
    ~AudioMonitor() override;

    void init() override;
    void update() override;

    // Ham veriyi dönen getter
    AudioSnapshot getData() const;

    // Ses kontrol metodu (Qt butonları için)
    void setVolumeByPID(uint32_t targetPid, float newVolume);
};

#endif