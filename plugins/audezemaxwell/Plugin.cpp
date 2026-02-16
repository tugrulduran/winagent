#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/AudezeSampler.h"

using namespace audeze;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "audezemaxwell",
    "Audeze Maxwell Info",
    10000
};

class AudezePlugin final : public BasePlugin {
public:
    explicit AudezePlugin(const char *configJsonUtf8) : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8) {
    }

protected:
    bool onInit(QString &err) override {
        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        uint8_t battery = sampler_.getBattery();

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("battery", battery);

        return snap;
    }

private:
    AudezeSampler sampler_{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *, const char *cfg)  { return new AudezePlugin(cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((AudezePlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((AudezePlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((AudezePlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((AudezePlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((AudezePlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (AudezePlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((AudezePlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((AudezePlugin *) h)->readView()
        : WaView{nullptr, 0};
}
// @formatter:on
