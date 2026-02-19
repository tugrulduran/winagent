#include "Launcher.h"

#include <QJsonArray>

#if defined(_WIN32)
  #include <windows.h>
  #include <shellapi.h>
#else
  #include <QDesktopServices>
  #include <QUrl>
#endif

bool Launcher::init(const QJsonObject& config, QString& err) {
    err.clear();
    apps_.clear();

    const QJsonValue vApps = config.value("apps");
    if (!vApps.isUndefined() && !vApps.isArray()) {
        err = "config.apps is not an array";
        return false;
    }

    const QJsonArray arr = vApps.toArray();
    apps_.reserve((size_t)arr.size());

    for (int i = 0; i < arr.size(); ++i) {
        const QJsonValue item = arr.at(i);
        if (!item.isObject()) continue;
        const QJsonObject o = item.toObject();

        LauncherApp a;
        a.index = o.value("index").isDouble() ? o.value("index").toInt(i) : i;
        a.name  = o.value("name").toString();
        a.icon  = o.value("icon").toString();
        a.path  = o.value("path").toString();
        a.zone  = o.value("zone").toInt(1);

        if (a.name.trimmed().isEmpty() || a.path.trimmed().isEmpty()) continue;
        if (a.icon.trimmed().isEmpty()) a.icon = "fa-solid fa-question";

        apps_.push_back(std::move(a));
    }

    return true;
}

QJsonObject Launcher::snapshot() const {
    QJsonArray apps;
    for (const auto& a : apps_) {
        apps.append(QJsonObject{
            {"index", a.index},
            {"name", a.name},
            {"icon", a.icon},
            {"path", a.path},
            {"zone", a.zone},
        });
    }

    return QJsonObject{
        {"ok", true},
        {"apps", apps},
    };
}

void Launcher::runAction(int index) {
    if (index < 0 || index >= (int)apps_.size()) return;
    const auto& a = apps_.at((size_t)index);

#if defined(_WIN32)
    const std::wstring path = a.path.toStdWString();
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(a.path));
#endif
}
