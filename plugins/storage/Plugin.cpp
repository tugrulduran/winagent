#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Optional Qt UI
#include <QWidget>
#include "src/StorageUi.h"

#include "src/StorageSampler.h"

using namespace storage;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "storage",
    "Storage Info",
    "Lists all volumes visible in This PC (fixed, removable/USB, optical, and mapped network drives) with label and free/total space. Includes fast hotplug detection.",
    1000
};

class StoragePlugin final : public BasePlugin {
public:
    explicit StoragePlugin(void* hostCtx, const char *configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8),
          hostApi_(static_cast<WaHostApi*>(hostCtx)) {
    }

    WaHostApi* hostApi() const { return hostApi_; }

protected:
    bool onInit(QString &err) override {
        Q_UNUSED(err);
        sampler_.init(config());
        return true;
    }

    void onStop() override {
        sampler_.shutdown();
    }

    QJsonObject onTick() override {
        const auto disks = sampler_.snapshot();

        QJsonArray outDisks;
        // outDisks.reserve(static_cast<int>(disks.size()));
        for (const auto& d : disks) {
            QJsonObject o;
            o.insert("drive", QString::fromStdWString(d.drive));
            o.insert("type", QString::fromStdWString(d.type));
            o.insert("label", QString::fromStdWString(d.label));
            o.insert("fileSystem", QString::fromStdWString(d.fileSystem));
            o.insert("totalBytes", static_cast<qint64>(d.totalBytes));
            o.insert("freeBytes", static_cast<qint64>(d.freeBytes));
            o.insert("ready", d.ready);
            outDisks.append(o);
        }

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("disks", outDisks);
        return snap;
    }

private:
    WaHostApi* hostApi_ = nullptr;
    StorageSampler sampler_{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()                 { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *hostCtx, const char *cfg) { return new StoragePlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                          { return h ? ((StoragePlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                         { return h ? ((StoragePlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                         { return h ? ((StoragePlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                        { return h ? ((StoragePlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                          { return h ? ((StoragePlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (StoragePlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((StoragePlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((StoragePlugin *) h)->readView()
        : WaView{nullptr, 0};
}

WA_EXPORT uint64_t WA_CALL wa_get_tick_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->tickCount() : 0;
}

WA_EXPORT uint64_t WA_CALL wa_get_read_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->readCount() : 0;
}

WA_EXPORT uint64_t WA_CALL wa_get_request_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->requestCount() : 0;
}

WA_EXPORT int64_t WA_CALL wa_get_last_tick_ms(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->lastTickMs() : 0;
}

// Optional UI export
WA_EXPORT QWidget* WA_CALL wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<StoragePlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new StorageUi(p->hostApi(), parent);
}
// @formatter:on
