#pragma once

#include <QString>
#include <QJsonObject>
#include <QHash>

#include <vector>

namespace dockerplugin {

struct PortBinding {
    QString containerPort; // e.g. "80/tcp"
    QString hostIp;        // e.g. "0.0.0.0"
    QString hostPort;      // e.g. "8080"
};

struct ContainerInfo {
    QString id;        // container ID
    QString name;      // human name
    QString image;

    QString status;    // running/exited/paused/restarting/dead/created
    QString health;    // healthy/unhealthy/starting/"" (if no healthcheck)
    QString state3;    // running/exited/starting (UI-friendly)

    std::vector<PortBinding> ports;

    qint64 uptimeSec = 0;

    double cpuPerc = 0.0;       // percent
    qint64 ramUsedBytes = 0;
    qint64 ramLimitBytes = 0;
    double ramPerc = 0.0;       // percent
};

struct StackInfo {
    QString name;      // compose project / swarm stack / standalone
    QString status;    // running/partial/stopped
    std::vector<ContainerInfo> containers;
};

class DockerSampler final {
public:
    void init(const QJsonObject& cfg);

    // Collects container state and groups by stack.
    bool scan(std::vector<StackInfo>& outStacks, QString& err);

    // Control actions. action: start|stop|restart
    bool controlContainer(const QString& action, const QString& idOrName, QString& err);
    bool controlStack(const QString& action, const QString& stackName, QString& err);

private:
    bool runDocker(const QStringList& args, QString& stdoutText, QString& stderrText, int timeoutMs) const;

    static double parsePercent(const QString& s);
    static qint64 parseBytes(const QString& s);

    static QString detectStackName(const QJsonObject& inspectObj);

    // Last known mapping for stack operations.
    QHash<QString, QStringList> stackToContainers_;

    QString dockerPath_ = "docker";
    int timeoutMs_ = 6000;
};

} // namespace dockerplugin
