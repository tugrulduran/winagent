#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/WindowsUpdateSampler.h"

// Optional Qt UI
#include <QWidget>
#include "src/WindowsUpdateUi.h"

using namespace windowsupdate;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "windowsupdate",
    "Windows Update",
    "Reports available Windows updates (pending install) using the Windows Update Agent, including basic classification.",
    60 * 60 * 1000
};

class WindowsUpdatePlugin final : public BasePlugin {
public:
    explicit WindowsUpdatePlugin(void* hostCtx, const char* configJsonUtf8)
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
        std::vector<UpdateInfo> updates;
        QString scanErr;
        const bool ok = sampler_.scan(updates, scanErr);

        QJsonArray arr;
        for (const auto& u : updates) {
            arr.append(QJsonObject{
                {"id", u.id},
                {"title", u.title},
                {"importance", u.importance},
                {"type", u.type},
            });
        }

        QJsonObject snap;
        snap.insert("ok", ok);
        snap.insert("updates", arr);
        if (!ok) {
            snap.insert("error", scanErr);
        }
        return snap;
    }

private:
    WindowsUpdateSampler sampler_{};
    WaHostApi* hostApi_ = nullptr;
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()                 { return &INFO; }
WA_EXPORT void * WA_CALL               wa_create(void *hostCtx, const char *cfg)  { return new WindowsUpdatePlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL              wa_init(void *h)               { return h ? ((WindowsUpdatePlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_start(void *h)              { return h ? ((WindowsUpdatePlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_pause(void *h)              { return h ? ((WindowsUpdatePlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_resume(void *h)             { return h ? ((WindowsUpdatePlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_stop(void *h)               { return h ? ((WindowsUpdatePlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL                 wa_destroy(void *h) {
    if (!h) return;
    auto *p = (WindowsUpdatePlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL               wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((WindowsUpdatePlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL               wa_read(void *h) {
    return h
        ? ((WindowsUpdatePlugin *) h)->readView()
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
    auto* p = static_cast<WindowsUpdatePlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new WindowsUpdateUi(p->hostApi(), parent);
}
// @formatter:on
