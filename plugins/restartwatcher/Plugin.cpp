#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/RestartWatcherSampler.h"

// Optional Qt UI
#include <QWidget>
#include "src/RestartWatcherUi.h"

using namespace restartwatcher;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "restartwatcher",
    "restartwatcher",
    "Tracks whether Windows currently requires a restart (updates/servicing/pending operations).",
    60 * 1000
};

class RestartWatcherPlugin final : public BasePlugin {
public:
    explicit RestartWatcherPlugin(void* hostCtx, const char* configJsonUtf8)
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
        const RestartReport rep = sampler_.readReport();

        QJsonObject snap;
        snap.insert("ok", true);

        // Backward compatible (single, priority-ordered): 0..5
        snap.insert("status", static_cast<int>(rep.status));

        // Rich fields for dashboard logic:
        // level: 0 none, 1 recommended, 2 required
        snap.insert("level", static_cast<int>(rep.level));
        snap.insert("required", rep.level == RestartLevel::Required);
        snap.insert("recommended", rep.level == RestartLevel::Recommended);

        QJsonObject flags;
        flags.insert("windowsUpdate", rep.windowsUpdate);
        flags.insert("cbsPending", rep.cbsPending);
        flags.insert("pendingFileRename", rep.pendingFileRename);
        flags.insert("computerRename", rep.computerRename);
        flags.insert("updateExeVolatile", rep.updateExeVolatile);
        snap.insert("flags", flags);

        QJsonArray reasons;
        // Keep stable string IDs for UI mapping.
        if (rep.windowsUpdate)       reasons.append("windows_update");
        if (rep.cbsPending)          reasons.append("cbs_pending");
        if (rep.pendingFileRename)   reasons.append("pending_file_rename");
        if (rep.computerRename)      reasons.append("computer_rename");
        if (rep.updateExeVolatile)   reasons.append("update_exe_volatile");
        snap.insert("reasons", reasons);

        return snap;
    }

private:
    RestartWatcherSampler sampler_{};
    WaHostApi* hostApi_ = nullptr;
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()                 { return &INFO; }
WA_EXPORT void * WA_CALL               wa_create(void *hostCtx, const char *cfg)  { return new RestartWatcherPlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL              wa_init(void *h)               { return h ? ((RestartWatcherPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_start(void *h)              { return h ? ((RestartWatcherPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_pause(void *h)              { return h ? ((RestartWatcherPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_resume(void *h)             { return h ? ((RestartWatcherPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL              wa_stop(void *h)               { return h ? ((RestartWatcherPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL                 wa_destroy(void *h) {
    if (!h) return;
    auto *p = (RestartWatcherPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL               wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((RestartWatcherPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL               wa_read(void *h) {
    return h
        ? ((RestartWatcherPlugin *) h)->readView()
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
    auto* p = static_cast<RestartWatcherPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new RestartWatcherUi(p->hostApi(), parent);
}
// @formatter:on
