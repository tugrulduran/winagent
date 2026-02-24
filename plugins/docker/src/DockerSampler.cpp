#include "DockerSampler.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace dockerplugin {

static QString trimLeadingSlash(const QString& name) {
    if (name.startsWith('/')) return name.mid(1);
    return name;
}

void DockerSampler::init(const QJsonObject& cfg) {
    const QString path = cfg.value("dockerPath").toString();
    if (!path.isEmpty()) dockerPath_ = path;

    const int t = cfg.value("timeoutMs").toInt(6000);
    timeoutMs_ = std::clamp(t, 1000, 30000);
}

bool DockerSampler::runDocker(const QStringList& args, QString& stdoutText, QString& stderrText, int timeoutMs) const {
    QProcess p;
    p.setProgram(dockerPath_);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::SeparateChannels);

    p.start();
    if (!p.waitForStarted(1500)) {
        stderrText = "Failed to start docker CLI. Is Docker installed and in PATH?";
        return false;
    }

    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        stderrText = "docker command timed out";
        return false;
    }

    stdoutText = QString::fromUtf8(p.readAllStandardOutput());
    stderrText = QString::fromUtf8(p.readAllStandardError());

    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (stderrText.trimmed().isEmpty()) {
            stderrText = "docker command failed";
        }
        return false;
    }

    return true;
}

double DockerSampler::parsePercent(const QString& s) {
    QString t = s.trimmed();
    if (t.endsWith('%')) t.chop(1);
    bool ok = false;
    const double v = t.toDouble(&ok);
    return ok ? v : 0.0;
}

qint64 DockerSampler::parseBytes(const QString& s) {
    QString t = s.trimmed();
    if (t.isEmpty()) return 0;
    if (t.compare("0B", Qt::CaseInsensitive) == 0) return 0;

    // Split number and unit
    QRegularExpression re(R"(^\s*([0-9]+(?:\.[0-9]+)?)\s*([A-Za-z]+)\s*$)");
    const auto m = re.match(t);
    if (!m.hasMatch()) {
        bool ok = false;
        const qint64 v = t.toLongLong(&ok);
        return ok ? v : 0;
    }

    const double num = m.captured(1).toDouble();
    QString unit = m.captured(2);
    unit = unit.toUpper();

    const auto mul = [&](double base) -> qint64 {
        return static_cast<qint64>(num * base);
    };

    if (unit == "B") return mul(1.0);

    // Docker commonly uses kB/MB/GB (decimal) and KiB/MiB/GiB (binary) depending on settings.
    if (unit == "KB" || unit == "K" || unit == "KIB") return mul(unit == "KIB" ? 1024.0 : 1000.0);
    if (unit == "MB" || unit == "M" || unit == "MIB") return mul(unit == "MIB" ? 1024.0 * 1024.0 : 1000.0 * 1000.0);
    if (unit == "GB" || unit == "G" || unit == "GIB") return mul(unit == "GIB" ? 1024.0 * 1024.0 * 1024.0 : 1000.0 * 1000.0 * 1000.0);
    if (unit == "TB" || unit == "T" || unit == "TIB") return mul(unit == "TIB" ? 1024.0 * 1024.0 * 1024.0 * 1024.0 : 1000.0 * 1000.0 * 1000.0 * 1000.0);

    return mul(1.0);
}

QString DockerSampler::detectStackName(const QJsonObject& inspectObj) {
    // Prefer compose project.
    const QJsonObject cfg = inspectObj.value("Config").toObject();
    const QJsonObject labels = cfg.value("Labels").toObject();

    const QString composeProject = labels.value("com.docker.compose.project").toString();
    if (!composeProject.isEmpty()) return composeProject;

    const QString swarmStack = labels.value("com.docker.stack.namespace").toString();
    if (!swarmStack.isEmpty()) return swarmStack;

    return "standalone";
}

static QDateTime parseDockerTime(const QString& startedAt) {
    // Example: 2026-02-22T10:20:30.123456789Z
    QString ts = startedAt.trimmed();
    if (ts.isEmpty()) return {};

    const int dot = ts.indexOf('.');
    if (dot != -1) {
        const int z = ts.indexOf('Z', dot);
        if (z != -1) {
            const int fracLen = z - dot - 1;
            if (fracLen > 3) {
                ts = ts.left(dot + 4) + ts.mid(z); // keep 3 ms digits
            }
        }
    }

    QDateTime dt = QDateTime::fromString(ts, Qt::ISODateWithMs);
    if (!dt.isValid()) dt = QDateTime::fromString(ts, Qt::ISODate);
    if (dt.isValid() && dt.timeSpec() != Qt::UTC) {
        dt = dt.toUTC();
    }
    return dt;
}

bool DockerSampler::scan(std::vector<StackInfo>& outStacks, QString& err) {
    outStacks.clear();
    err.clear();

    QString psOut, psErr;
    if (!runDocker({"ps", "-aq"}, psOut, psErr, timeoutMs_)) {
        err = psErr;
        stackToContainers_.clear();
        return false;
    }

    QStringList ids = psOut.split(QRegularExpression(R"([\r\n\s]+)"), Qt::SkipEmptyParts);
    if (ids.isEmpty()) {
        stackToContainers_.clear();
        return true;
    }

    // Collect inspect data in chunks to avoid command line length issues.
    QList<QJsonObject> inspectObjs;
    const int chunkSize = 80;
    for (int i = 0; i < ids.size(); i += chunkSize) {
        const QStringList chunk = ids.mid(i, chunkSize);
        QString insOut, insErr;
        QStringList args;
        args << "inspect";
        args << chunk;
        if (!runDocker(args, insOut, insErr, timeoutMs_)) {
            err = insErr;
            stackToContainers_.clear();
            return false;
        }

        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(insOut.toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isArray()) {
            err = "docker inspect returned invalid JSON";
            stackToContainers_.clear();
            return false;
        }

        const QJsonArray arr = doc.array();
        for (const QJsonValue& v : arr) {
            if (v.isObject()) inspectObjs.append(v.toObject());
        }
    }

    // Stats (running containers only)
    struct StatsInfo {
        QString idOrPrefix;
        QString name;
        double cpu = 0.0;
        qint64 ramUsed = 0;
        qint64 ramLimit = 0;
        double ramPerc = 0.0;
    };

    QHash<QString, StatsInfo> statsByPrefix;
    QHash<QString, StatsInfo> statsByName;

    {
        QString stOut, stErr;
        if (runDocker({"stats", "--no-stream", "--format", "{{json .}}"}, stOut, stErr, timeoutMs_)) {
            const QStringList lines = stOut.split(QRegularExpression(R"([\r\n]+)"), Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                QJsonParseError perr;
                const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &perr);
                if (perr.error != QJsonParseError::NoError || !doc.isObject()) continue;
                const QJsonObject o = doc.object();

                StatsInfo si;
                si.idOrPrefix = o.value("Container").toString();
                si.name = o.value("Name").toString();
                si.cpu = parsePercent(o.value("CPUPerc").toString());
                si.ramPerc = parsePercent(o.value("MemPerc").toString());

                const QString memUsage = o.value("MemUsage").toString();
                const QStringList parts = memUsage.split('/');
                if (parts.size() == 2) {
                    si.ramUsed = parseBytes(parts.at(0));
                    si.ramLimit = parseBytes(parts.at(1));
                }

                if (!si.idOrPrefix.isEmpty()) statsByPrefix.insert(si.idOrPrefix, si);
                if (!si.name.isEmpty()) statsByName.insert(si.name, si);
            }
        }
        // If stats fails (daemon paused), we just omit CPU/RAM fields.
    }

    // Group containers by stack name
    struct StackAgg {
        StackInfo s;
        int running = 0;
    };

    QHash<QString, StackAgg> stacks;
    QHash<QString, QStringList> stackToIds;

    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (const QJsonObject& obj : inspectObjs) {
        ContainerInfo c;
        c.id = obj.value("Id").toString();
        c.name = trimLeadingSlash(obj.value("Name").toString());

        const QJsonObject cfg = obj.value("Config").toObject();
        c.image = cfg.value("Image").toString();

        const QJsonObject state = obj.value("State").toObject();
        c.status = state.value("Status").toString();

        // Health status (requires HEALTHCHECK in the image)
        const QJsonObject healthObj = state.value("Health").toObject();
        c.health = healthObj.value("Status").toString();

        // Ports mapping from docker inspect (NetworkSettings.Ports)
        const QJsonObject net = obj.value("NetworkSettings").toObject();
        const QJsonObject portsObj = net.value("Ports").toObject();
        for (auto it = portsObj.begin(); it != portsObj.end(); ++it) {
            const QString containerPort = it.key(); // e.g. "80/tcp"
            const QJsonValue pv = it.value();
            if (!pv.isArray()) continue;
            const QJsonArray bindings = pv.toArray();
            for (const QJsonValue& bv : bindings) {
                if (!bv.isObject()) continue;
                const QJsonObject bo = bv.toObject();
                PortBinding pb;
                pb.containerPort = containerPort;
                pb.hostIp = bo.value("HostIp").toString();
                pb.hostPort = bo.value("HostPort").toString();
                c.ports.push_back(std::move(pb));
            }
        }

        const QString startedAt = state.value("StartedAt").toString();
        const QDateTime started = parseDockerTime(startedAt);
        if (c.status == "running" && started.isValid()) {
            c.uptimeSec = started.secsTo(now);
            if (c.uptimeSec < 0) c.uptimeSec = 0;
        } else {
            c.uptimeSec = 0;
        }

        // UI-friendly state grouping: running/exited/starting
        if (c.status == "running") {
            c.state3 = (c.health == "starting") ? "starting" : "running";
        } else if (c.status == "created" || c.status == "restarting") {
            c.state3 = "starting";
        } else {
            c.state3 = "exited";
        }

        // Stats match: by prefix, then by name.
        if (!c.id.isEmpty()) {
            // Try to find stats whose key is a prefix of the full ID.
            // Common case: stats provides 12-char short ID.
            bool matched = false;
            for (auto it = statsByPrefix.constBegin(); it != statsByPrefix.constEnd(); ++it) {
                const QString key = it.key();
                if (!key.isEmpty() && c.id.startsWith(key)) {
                    const StatsInfo& si = it.value();
                    c.cpuPerc = si.cpu;
                    c.ramUsedBytes = si.ramUsed;
                    c.ramLimitBytes = si.ramLimit;
                    c.ramPerc = si.ramPerc;
                    matched = true;
                    break;
                }
            }
            if (!matched && statsByName.contains(c.name)) {
                const StatsInfo si = statsByName.value(c.name);
                c.cpuPerc = si.cpu;
                c.ramUsedBytes = si.ramUsed;
                c.ramLimitBytes = si.ramLimit;
                c.ramPerc = si.ramPerc;
            }
        }

        const QString stackName = detectStackName(obj);
        if (!stacks.contains(stackName)) {
            StackAgg agg;
            agg.s.name = stackName;
            stacks.insert(stackName, agg);
        }

        StackAgg& agg = stacks[stackName];
        agg.s.containers.push_back(c);
        if (c.status == "running") agg.running++;

        if (!c.id.isEmpty()) stackToIds[stackName].append(c.id);
    }

    // Finalize stacks
    outStacks.reserve(stacks.size());
    stackToContainers_.clear();

    QList<QString> names = stacks.keys();
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b){ return a.toLower() < b.toLower(); });

    for (const QString& name : names) {
        StackAgg agg = stacks.value(name);

        const int total = static_cast<int>(agg.s.containers.size());
        if (total == 0) agg.s.status = "stopped";
        else if (agg.running == total) agg.s.status = "running";
        else if (agg.running == 0) agg.s.status = "stopped";
        else agg.s.status = "partial";

        // Sort containers by name
        std::sort(agg.s.containers.begin(), agg.s.containers.end(), [](const ContainerInfo& a, const ContainerInfo& b){
            return a.name.toLower() < b.name.toLower();
        });

        outStacks.push_back(std::move(agg.s));
        stackToContainers_.insert(name, stackToIds.value(name));
    }

    return true;
}

bool DockerSampler::controlContainer(const QString& action, const QString& idOrName, QString& err) {
    err.clear();
    const QString act = action.toLower();
    if (act != "start" && act != "stop" && act != "restart") {
        err = "Invalid action";
        return false;
    }
    if (idOrName.trimmed().isEmpty()) {
        err = "Missing container id/name";
        return false;
    }

    QString out, e;
    if (!runDocker({act, idOrName}, out, e, timeoutMs_)) {
        err = e;
        return false;
    }
    return true;
}

bool DockerSampler::controlStack(const QString& action, const QString& stackName, QString& err) {
    err.clear();
    const QString act = action.toLower();
    if (act != "start" && act != "stop" && act != "restart") {
        err = "Invalid action";
        return false;
    }
    if (stackName.trimmed().isEmpty()) {
        err = "Missing stack name";
        return false;
    }

    QStringList containers = stackToContainers_.value(stackName);
    if (containers.isEmpty()) {
        // Try a quick scan to populate.
        std::vector<StackInfo> tmp;
        QString scanErr;
        scan(tmp, scanErr);
        containers = stackToContainers_.value(stackName);
    }

    if (containers.isEmpty()) {
        err = "Stack not found or has no containers";
        return false;
    }

    const int chunkSize = 50;
    for (int i = 0; i < containers.size(); i += chunkSize) {
        const QStringList chunk = containers.mid(i, chunkSize);
        QString out, e;
        QStringList args;
        args << act;
        args << chunk;
        if (!runDocker(args, out, e, timeoutMs_)) {
            err = e;
            return false;
        }
    }
    return true;
}

} // namespace dockerplugin
