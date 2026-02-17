#pragma once
#include <cstdint>
#include <string>
#include <QJsonObject>

struct App {
    uint8_t index;
    std::wstring name;
    std::wstring icon;
    std::wstring path;
};

namespace launcher {
    class Launcher {
    public:
        std::vector<App> getApps();

        void init(QJsonObject config);

        void launch(uint8_t index);

    private:
        std::vector<App> apps = {};
    };
}
