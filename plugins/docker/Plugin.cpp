#include "BasePlugin.h"

#include <atomic>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/DockerSampler.h"

// Optional Qt UI
#include <QWidget>
#include "src/DockerUi.h"

using namespace dockerplugin;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "docker",
    "Docker",
    "Monitors Docker containers (grouped by compose project / swarm stack) and provides basic start/stop/restart control.",
    3000
};

class DockerPlugin final : public BasePlugin {
public:
    explicit DockerPlugin(void* hostCtx, const char* configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8),
          hostApi_(static_cast<WaHostApi*>(hostCtx)) {
    }

    WaHostApi* hostApi() const { return hostApi_; }

protected:
    bool onInit(QString& err) override {
        Q_UNUSED(err);
        sampler_.init(config());
        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        ScopedProcessing proc(processing_);
        if (!proc.acquired) return busyResp();

        std::vector<StackInfo> stacks;
        QString scanErr;
        const bool ok = sampler_.scan(stacks, scanErr);

        QJsonArray outStacks;
        for (const auto& s : stacks) {
            QJsonArray outContainers;
            for (const auto& c : s.containers) {
                QJsonObject co;
                co.insert("id", c.id);
                co.insert("name", c.name);
                co.insert("image", c.image);
                co.insert("status", c.status);
                if (!c.health.isEmpty()) co.insert("health", c.health);

                QJsonArray ports;
                for (const auto& p : c.ports) {
                    QJsonObject po;
                    po.insert("container", p.containerPort);
                    po.insert("hostIp", p.hostIp);
                    po.insert("hostPort", p.hostPort);
                    ports.append(po);
                }
                co.insert("ports", ports);

                co.insert("uptime", static_cast<qint64>(c.uptimeSec));
                co.insert("cpu", c.cpuPerc);
                co.insert("ramUsed", static_cast<qint64>(c.ramUsedBytes));
                co.insert("ramLimit", static_cast<qint64>(c.ramLimitBytes));
                co.insert("ramPerc", c.ramPerc);
                outContainers.append(co);
            }

            QJsonObject so;
            so.insert("name", s.name);
            so.insert("status", s.status);
            so.insert("containers", outContainers);
            outStacks.append(so);
        }

        QJsonObject snap;
        snap.insert("ok", ok);
        snap.insert("stacks", outStacks);
        if (!ok) snap.insert("error", scanErr);
        return snap;
    }

    QJsonObject onRequest(const QJsonObject& req) override {
        ScopedProcessing proc(processing_);
        if (!proc.acquired) return busyResp();

        const QString cmd = req.value("cmd").toString();
        if (cmd.isEmpty()) {
            return QJsonObject{{"ok", false}, {"error", "Missing cmd"}};
        }

        auto okResp = [](const QString& msg) {
            QJsonObject o;
            o.insert("ok", true);
            if (!msg.isEmpty()) o.insert("message", msg);
            return o;
        };
        auto errResp = [](const QString& msg) {
            return QJsonObject{{"ok", false}, {"error", msg}};
        };

        const QStringList parts = cmd.split('.', Qt::SkipEmptyParts);
        if (parts.size() != 2) {
            return errResp("Invalid cmd format. Expected 'stack.<action>' or 'container.<action>'");
        }

        const QString scope = parts.at(0).toLower();
        const QString action = parts.at(1).toLower();

        QString err;
        if (scope == "container") {
            QString idOrName = req.value("id").toString();
            if (idOrName.isEmpty()) idOrName = req.value("name").toString();
            if (idOrName.isEmpty()) idOrName = req.value("container").toString();

            if (!sampler_.controlContainer(action, idOrName, err)) {
                return errResp(err);
            }
            return okResp("container." + action);
        }

        if (scope == "stack") {
            QString stackName = req.value("name").toString();
            if (stackName.isEmpty()) stackName = req.value("stack").toString();

            if (!sampler_.controlStack(action, stackName, err)) {
                return errResp(err);
            }
            return okResp("stack." + action);
        }

        return errResp("Unknown cmd scope");
    }

private:
    static QJsonObject busyResp() {
        return QJsonObject{{"ok", false}, {"reason", "busy"}};
    }

    struct ScopedProcessing {
        std::atomic_bool& flag;
        bool acquired = false;
        explicit ScopedProcessing(std::atomic_bool& f) : flag(f) {
            bool expected = false;
            acquired = flag.compare_exchange_strong(expected, true);
        }
        ~ScopedProcessing() {
            if (acquired) flag.store(false);
        }
    };

    DockerSampler sampler_{};
    WaHostApi* hostApi_ = nullptr;
    std::atomic_bool processing_{false};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()                 { return &INFO; }
WA_EXPORT void * WA_CALL               wa_create(void *hostCtx, const char *cfg)  { return new DockerPlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL              wa_init(void *h)               { return h ? ((DockerPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_start(void *h)              { return h ? ((DockerPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_pause(void *h)              { return h ? ((DockerPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_resume(void *h)             { return h ? ((DockerPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_stop(void *h)               { return h ? ((DockerPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL                 wa_destroy(void *h) {
    if (!h) return;
    auto *p = (DockerPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL               wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((DockerPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL               wa_read(void *h) {
    return h
        ? ((DockerPlugin *) h)->readView()
        : WaView{nullptr, 0};
}

WA_EXPORT uint64_t WA_CALL             wa_get_tick_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->tickCount() : 0;
}

WA_EXPORT uint64_t WA_CALL             wa_get_read_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->readCount() : 0;
}

WA_EXPORT uint64_t WA_CALL             wa_get_request_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->requestCount() : 0;
}

WA_EXPORT int64_t WA_CALL              wa_get_last_tick_ms(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->lastTickMs() : 0;
}

// Optional UI export
WA_EXPORT QWidget* WA_CALL             wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<DockerPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new DockerUi(p->hostApi(), parent);
}
// @formatter:on
