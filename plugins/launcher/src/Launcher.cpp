#include <windows.h>
#include <shellapi.h>
#include <QJsonArray>
#include "Launcher.h"


namespace launcher {
    void Launcher::init(QJsonObject config) {
        const QJsonValue v = config.value("apps");
        apps.clear();
        if (!v.isArray()) {
            return;
        }

        const QJsonArray arr = v.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonValue item = arr.at(i);
            if (!item.isObject()) continue;
            const QJsonObject obj = item.toObject();
            uint8_t index = (uint8_t) i;
            std::wstring name = obj.value("name").toString().toStdWString();
            std::wstring icon = obj.value("icon").toString().toStdWString();
            std::wstring path = obj.value("path").toString().toStdWString();
            apps.push_back(App{index, name, icon, path});
        }
    }

    std::vector<App> Launcher::getApps() {
        return apps;
    }

    void Launcher::launch(uint8_t index) {
        if (index >= apps.size()) return;
        const App &app = apps.at(index);
        ShellExecuteW(NULL, L"open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}
