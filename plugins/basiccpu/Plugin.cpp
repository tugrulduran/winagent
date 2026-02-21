#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/CpuSampler.h"

// Optional Qt UI
#include <QWidget>
#include "src/BasicCpuUi.h"

using namespace basiccpu;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "basiccpu",
    "Basic Cpu Info",
    "Reads and reports CPU cores + usage.",
    1000
};

class BasicCpuPlugin final : public BasePlugin {
public:
    explicit BasicCpuPlugin(void* hostCtx, const char *configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8),
          hostApi_(static_cast<WaHostApi*>(hostCtx)) {
    }

    WaHostApi* hostApi() const { return hostApi_; }

protected:
    bool onInit(QString &err) override {
        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        int cores = sampler_.getCpuCores();
        double load = sampler_.getCpuUsage();

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("cores", cores);
        snap.insert("load", load);

        return snap;
    }

private:
    WaHostApi* hostApi_ = nullptr;
    CpuSampler sampler_{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *hostCtx, const char *cfg)  { return new BasicCpuPlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((BasicCpuPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((BasicCpuPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((BasicCpuPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((BasicCpuPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((BasicCpuPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (BasicCpuPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((BasicCpuPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((BasicCpuPlugin *) h)->readView()
        : WaView{nullptr, 0};
}

// Optional UI export
WA_EXPORT QWidget* WA_CALL wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<BasicCpuPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new BasicCpuUi(p->hostApi(), parent);
}
// @formatter:on
