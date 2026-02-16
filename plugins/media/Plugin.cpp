#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "src/MediaMonitor.h"

using namespace media;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "media",
    "Media Controller",
    1000
};

class MediaPlugin final : public BasePlugin {
public:
    explicit MediaPlugin(const char *configJsonUtf8) : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8) {
    }

protected:
    bool onInit(QString &err) override {
        return true;
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        MediaData data = media.getMedia();

        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("isPlaying", data.isPlaying);
        if (data.source > 0) {
            snap.insert("source", data.source);
            snap.insert("title", QString::fromStdWString(data.title));
            snap.insert("duration", data.duration);
            snap.insert("currentTime", data.currentTime);
        }

        return snap;
    }


    QJsonObject onRequest(const QJsonObject &req) override {
        const QString cmd = req.value("cmd").toString();

        if (cmd.isEmpty()) {
            return QJsonObject{{"ok", false}, {"error", "missing_cmd"}};
        }

        if (cmd == "playpause") {
            media.playpause();
            return QJsonObject{{"ok", true}};
        }
        if (cmd == "prev") {
            media.prev();
            return QJsonObject{{"ok", true}};
        }
        if (cmd == "next") {
            media.next();
            return QJsonObject{{"ok", true}};
        }
        if (cmd == "jump") {
            media.jump(req.value("time").toInt());
            return QJsonObject{{"ok", true}};
        }

        return QJsonObject{{"ok", false}, {"error", "unknown_cmd"}, {"cmd", cmd}};
    }

private:
    MediaMonitor media{};
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()           { return &INFO; }
WA_EXPORT void * WA_CALL    wa_create(void *, const char *cfg)  { return new MediaPlugin(cfg); }
WA_EXPORT int32_t WA_CALL   wa_init(void *h)                    { return h ? ((MediaPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_start(void *h)                   { return h ? ((MediaPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_pause(void *h)                   { return h ? ((MediaPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_resume(void *h)                  { return h ? ((MediaPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL   wa_stop(void *h)                    { return h ? ((MediaPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL      wa_destroy(void *h) {
    if (!h) return;
    auto *p = (MediaPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL    wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((MediaPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL    wa_read(void *h) {
    return h
        ? ((MediaPlugin *) h)->readView()
        : WaView{nullptr, 0};
}
// @formatter:on
