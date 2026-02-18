#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Optional Qt UI
#include <QWidget>
#include "src/BasicNetworkUi.h"

#include "src/NetworkSampler.h"

using namespace basicnetwork;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "basicnetwork",
    "Basic Network Info",
    1000
};

class BasicNetworkPlugin final : public BasePlugin {
public:
    explicit BasicNetworkPlugin(void* hostCtx, const char *configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8),
          hostApi_(static_cast<WaHostApi*>(hostCtx)) {
    }

    WaHostApi* hostApi() const { return hostApi_; }

protected:
    bool onInit(QString &err) override {
        sampler_.init(config());

        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        std::map<std::wstring, InterfaceData> data = sampler_.update();

        QJsonArray interfaces = {};
        for (auto &[name, iface]: data) {
            interfaces.append(QJsonObject{
                {"name", QString::fromStdWString(iface.name)},
                {"description", QString::fromStdWString(iface.description)},
                {"guid", QString::fromStdWString(iface.guid)},
                {"rxSpeed", iface.speedInKB},
                {"txSpeed", iface.speedOutKB}
            });
        }

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("interfaces", interfaces);

        return snap;
    }

private:
    WaHostApi* hostApi_ = nullptr;
    NetworkSampler sampler_{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *hostCtx, const char *cfg)  { return new BasicNetworkPlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((BasicNetworkPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((BasicNetworkPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((BasicNetworkPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((BasicNetworkPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((BasicNetworkPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (BasicNetworkPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((BasicNetworkPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((BasicNetworkPlugin *) h)->readView()
        : WaView{nullptr, 0};
}

// Optional UI export
WA_EXPORT QWidget* WA_CALL wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<BasicNetworkPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new BasicNetworkUi(p->hostApi(), parent);
}
// @formatter:on
