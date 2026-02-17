#include <qcoreevent.h>

#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/Launcher.h"

using namespace launcher;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "launcher",
    "Windows App Launcher",
    60000
};

class LauncherPlugin final : public BasePlugin {
public:
    explicit LauncherPlugin(const char *configJsonUtf8) : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8) {
    }

protected:
    bool onInit(QString &err) override {
        launcher_.init(config());

        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        std::vector<App> data = launcher_.getApps();

        QJsonArray apps = {};
        for (auto &app: data) {
            apps.append(QJsonObject{
                {"index", app.index},
                {"name", QString::fromStdWString(app.name)},
                {"icon", QString::fromStdWString(app.icon)}
            });
        }

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("apps", apps);

        return snap;
    }


    QJsonObject onRequest(const QJsonObject &req) override {
        const QString cmd = req.value("cmd").toString();

        if (cmd.isEmpty()) {
            return QJsonObject{{"ok", false}, {"error", "missing_cmd"}};
        }

        if (cmd == "launch") {
            launcher_.launch(req.value("index").toInt());
            return QJsonObject{{"ok", true}};
        }

        return QJsonObject{{"ok", false}, {"error", "unknown_cmd"}, {"cmd", cmd}};
    }

private:
    Launcher launcher_{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *, const char *cfg)  { return new LauncherPlugin(cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((LauncherPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((LauncherPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((LauncherPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((LauncherPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((LauncherPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (LauncherPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((LauncherPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((LauncherPlugin *) h)->readView()
        : WaView{nullptr, 0};
}
// @formatter:on
