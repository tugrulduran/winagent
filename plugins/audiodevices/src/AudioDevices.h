#pragma once
#include <string>
#include <QJsonObject>

struct AudioDevice {
    int index{};
    std::wstring name;
    std::wstring deviceId;
    bool isDefault{};
};

namespace audiodevices {
    class AudioDevices {
    public:
        std::vector<AudioDevice> getDevices();

        void init(QJsonObject config);

        bool setDefaultByIndex(int index);

    private:
        std::vector<AudioDevice> listDevices();

        bool isIgnored(const std::wstring &name);

        bool setDefaultById(const std::wstring &deviceId);

        std::unordered_set<std::wstring> ignoredDevices = {};
    };
}
