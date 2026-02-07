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

/*
 * Audio monitor module (Windows Core Audio)
 *
 * What it does:
 * - Reads master (system) volume and mute state.
 * - Reads per-application audio sessions (PID, display name, volume, mute).
 *
 * Thread model:
 * - update() runs in the BaseMonitor worker thread.
 * - getData() returns a copy protected by a mutex.
 *
 * Note:
 * - COM must be initialized in the thread that uses Core Audio APIs.
 */

// One row in the per-application volume table.
struct AppAudioData {
    uint32_t pid;
    std::wstring name;
    float volume;     // 0.0 .. 1.0
    bool isMuted;
};

// Full snapshot returned to the UI / NetworkReporter.
struct AudioSnapshot {
    float masterVolume = 0.0f;  // 0.0 .. 1.0
    bool masterMuted = false;
    std::vector<AppAudioData> apps;
};

class AudioMonitor : public BaseMonitor {
private:
    AudioSnapshot currentSnapshot;
    mutable std::mutex dataMutex;

    // Process names to skip. This keeps the UI list cleaner.
    std::set<std::wstring> ignoreList = {
        L"Armoury Crate Service", L"ArmouryCrate.UserSessionHelper",
        L"ASUS Optimization", L"System"
    };

    // Reads the ProductName from the EXE's version info (if available).
    std::wstring GetFriendlyName(const std::wstring& filePath);

    // Returns true if this app should be skipped.
    bool IsIgnored(const std::wstring& name);

public:
    AudioMonitor(int interval = 1000);
    ~AudioMonitor() override;

    void init() override;
    void update() override;

    // Returns a copy of the latest snapshot (thread-safe).
    AudioSnapshot getData() const;

    // Sets per-app session volume by PID (0.0 .. 1.0).
    void setVolumeByPID(uint32_t targetPid, float newVolume);
};

#endif