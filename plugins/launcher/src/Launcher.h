#pragma once

#include <vector>

#include <QJsonObject>
#include <QString>

struct LauncherApp {
    int     index = 0;
    QString name;
    QString path;
    QString hash;
    int     zone = 1;
    int     order = 1;
};

class Launcher {
public:
    bool init(const QJsonObject& config, QString& err);

    QJsonObject snapshot() const;

    void runAction(int index);

    QJsonObject getIconByName(const QString& name) const;
    QJsonObject getIconByIndex(int index) const;

private:
    std::vector<LauncherApp> apps_{};
};
