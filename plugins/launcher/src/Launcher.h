#pragma once

#include <vector>

#include <QJsonObject>
#include <QString>

// A single launcher entry, configured from plugins/launcher/config.json.
struct LauncherApp {
    int     index = 0;
    QString name;
    QString icon;
    QString path;
    int     zone = 1;
};

class Launcher {
public:
    // Parse config and build the app list.
    // Returns false only for hard failures (e.g. invalid shapes that we want to surface).
    bool init(const QJsonObject& config, QString& err);

    // Current state for dashboards / host.
    QJsonObject snapshot() const;

    // Execute an action by index.
    void runAction(int index);

private:
    std::vector<LauncherApp> apps_{};
};
