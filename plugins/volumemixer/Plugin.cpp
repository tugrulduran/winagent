#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/MixerMonitor.h"

using namespace volumemixer;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "volumemixer",
    "Windows Volume Mixer",
    1000
};

class VolumeMixerPlugin final : public BasePlugin {
public:
    explicit VolumeMixerPlugin(const char *configJsonUtf8) : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8) {
    }

protected:
    bool onInit(QString &err) override {
        mixer.init(config());

        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        std::vector<MixerApp> data = mixer.update();

        QJsonArray apps = {};
        for (auto &app: data) {
            apps.append(QJsonObject{
                {"pid", QString::number(app.pid)},
                {"type", app.type},
                {"name", QString::fromStdWString(app.name)},
                {"volume", app.volume},
                {"muted", app.muted}
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

        if (cmd == "setAppVolume") {
            mixer.setVolumeByPID(req.value("pid").toInt(), req.value("volume").toDouble());
            return QJsonObject{{"ok", true}};
        }
        if (cmd == "setMasterVolume") {
            mixer.setMasterVolume(req.value("volume").toDouble());
            return QJsonObject{{"ok", true}};
        }
        if (cmd == "toggleAppMute") {
            mixer.toggleAppMuteByPID(req.value("pid").toInt());
            return QJsonObject{{"ok", true}};
        }
        if (cmd == "toggleMasterMute") {
            mixer.toggleMasterMute();
            return QJsonObject{{"ok", true}};
        }

        return QJsonObject{{"ok", false}, {"error", "unknown_cmd"}, {"cmd", cmd}};
    }

private:
    MixerMonitor mixer{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *, const char *cfg)  { return new VolumeMixerPlugin(cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((VolumeMixerPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((VolumeMixerPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((VolumeMixerPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((VolumeMixerPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((VolumeMixerPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (VolumeMixerPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((VolumeMixerPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((VolumeMixerPlugin *) h)->readView()
        : WaView{nullptr, 0};
}
// @formatter:on
