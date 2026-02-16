#pragma once
#include <cstdint>
#include <string>
#include <QJsonObject>

enum MixerAppType : uint8_t {
    MIXER_APP_TYPE_MASTER,
    MIXER_APP_TYPE_APP,
    MIXER_APP_TYPE_SERVICE
};

struct MixerApp {
    uint64_t pid;
    MixerAppType type;
    std::wstring name;
    float volume;
    bool muted;
};

namespace volumemixer {
    class MixerMonitor {
    public:
        ~MixerMonitor();

        std::vector<MixerApp> update();
        void setVolumeByPID(uint32_t targetPid, float newVolume);

        void init(QJsonObject config);

    private:
        bool isIgnored(const std::wstring &name);
        std::wstring getAppName(const std::wstring &filepath);

        std::unordered_set<std::wstring> ignoredApps = {};
    };
}
