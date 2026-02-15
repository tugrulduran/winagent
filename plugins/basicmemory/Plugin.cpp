#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/RamSampler.h"

using namespace basicmemory;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "basicmemory",
    "Basic Ram Info",
    5000
};

class BasicMemoryPlugin final : public BasePlugin {
public:
    explicit BasicMemoryPlugin(const char *configJsonUtf8) : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8) {
    }

protected:
    bool onInit(QString &err) override {
        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        double availableMemory = sampler_.getAvailableMemory() / 1024.0 / 1024.0 / 1024.0;
        double totalMemory = sampler_.getTotalMemory() / 1024.0 / 1024.0 / 1024.0;

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("total", totalMemory);
        snap.insert("available", availableMemory);

        return snap;
    }

private:
    RamSampler sampler_{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *, const char *cfg)  { return new BasicMemoryPlugin(cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((BasicMemoryPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((BasicMemoryPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((BasicMemoryPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((BasicMemoryPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((BasicMemoryPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (BasicMemoryPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((BasicMemoryPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((BasicMemoryPlugin *) h)->readView()
        : WaView{nullptr, 0};
}
// @formatter:on
