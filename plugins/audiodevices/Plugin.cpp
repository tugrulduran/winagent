#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/AudioDevices.h"

// Optional Qt UI
#include <QWidget>
#include "src/AudioDevicesUi.h"

using namespace audiodevices;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "audiodevices",
    "Windows Audio Devices",
    "Lists audio devices and reports changes/default device.",
    3000
};

class AudioDevicesPlugin final : public BasePlugin {
public:
    explicit AudioDevicesPlugin(void* hostCtx, const char *configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8),
          hostApi_(static_cast<WaHostApi*>(hostCtx)) {
    }

    WaHostApi* hostApi() const { return hostApi_; }

protected:
    bool onInit(QString &err) override {
        audio_devices_.init(config());

        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        std::vector<AudioDevice> data = audio_devices_.getDevices();

        QJsonArray devices = {};
        for (auto &dev: data) {
            devices.append(QJsonObject{
                {"index", dev.index},
                {"name", QString::fromStdWString(dev.name)},
                {"deviceId", QString::fromStdWString(dev.deviceId)},
                {"default", dev.isDefault}
            });
        }

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("devices", devices);

        return snap;
    }

    QJsonObject onRequest(const QJsonObject &req) override {
        const QString cmd = req.value("cmd").toString();

        if (cmd.isEmpty()) {
            return QJsonObject{{"ok", false}, {"error", "missing_cmd"}};
        }

        if (cmd == "setDevice") {
            auto newIndex = req.value("index").toInt();
            audio_devices_.setDefaultByIndex(newIndex);

            std::vector<AudioDevice> data = audio_devices_.getDevices();

            QJsonArray devices = {};
            for (auto &dev: data) {
                devices.append(QJsonObject{
                    {"index", dev.index},
                    {"name", QString::fromStdWString(dev.name)},
                    {"deviceId", QString::fromStdWString(dev.deviceId)},
                    {"default", dev.index == newIndex}
                });
            }

            QJsonObject root;
            QJsonObject payload;
            QJsonObject modules;
            QJsonObject audiodevices;
            audiodevices.insert("ok", true);
            audiodevices.insert("devices", devices);
            modules.insert("audiodevices", audiodevices);
            payload.insert("modules", modules);
            root.insert("event", "update");
            root.insert("payload", payload);
            return root;
        }

        return QJsonObject{{"ok", false}, {"error", "unknown_cmd"}, {"cmd", cmd}};
    }

private:
    AudioDevices audio_devices_{};
    WaHostApi* hostApi_ = nullptr;
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *hostCtx, const char *cfg)  { return new AudioDevicesPlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((AudioDevicesPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((AudioDevicesPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((AudioDevicesPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((AudioDevicesPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((AudioDevicesPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (AudioDevicesPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((AudioDevicesPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((AudioDevicesPlugin *) h)->readView()
        : WaView{nullptr, 0};
}

// Optional UI export
WA_EXPORT QWidget* WA_CALL wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<AudioDevicesPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new AudioDevicesUi(p->hostApi(), parent);
}
// @formatter:on
