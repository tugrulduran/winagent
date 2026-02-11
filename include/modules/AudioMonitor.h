#ifndef AUDIOMONITOR_H
#define AUDIOMONITOR_H

#include <string>
#include <set>
#include "BaseMonitor.h"

struct AudioSnapshot {
    uint32_t pid;
    std::wstring name;
    float volume;
    bool isMuted;
};

class AudioMonitor : public BaseMonitor {
public:
    AudioMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };

    ~AudioMonitor() override;

    void init() override;

    void update() override;

    void setVolumeByPID(uint32_t targetPid, float newVolume);

private:
    std::set<std::wstring> ignoreList = {
        L"Armoury Crate Service", L"ArmouryCrate.UserSessionHelper",
        L"ASUS Optimization", L"System"
    };

    bool IsIgnored(const std::wstring &name);

    static std::wstring GetFriendlyName(const std::wstring &filePath);
};

#endif
