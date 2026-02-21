#include "BasePlugin.h"

#include <QJsonObject>
#include <QString>

#include "src/Launcher.h"

// Optional Qt UI
#include <QWidget>
#include "src/LauncherUi.h"

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "launcher",
    "Launcher",
    "Shows configured launch targets and launches them on request.",
    1000
};

class LauncherPlugin final : public BasePlugin {
public:
    explicit LauncherPlugin(void* hostCtx, const char* cfg)
        : BasePlugin(INFO.defaultIntervalMs, cfg),
          hostApi_(static_cast<WaHostApi*>(hostCtx)) {
    }

    WaHostApi* hostApi() const { return hostApi_; }

protected:
    bool onInit(QString& err) override {
        return launcher_.init(config(), err);
    }

    void onStop() override {
    }

    QJsonObject onTick() override {
        return launcher_.snapshot();
    }

    QJsonObject onRequest(const QJsonObject& req) override {
        const QString cmd = req.value("cmd").toString();

        if (cmd == "getIcon") {
            const QString name = req.value("name").toString();
            if (!name.trimmed().isEmpty()) {
                return launcher_.getIconByName(name);
            }
            const int index = req.value("index").toInt(-1);
            return launcher_.getIconByIndex(index);
        }

        if (cmd == "runAction") {
            const int id = req.value("id").toInt(-1);
            if (id >= 0) launcher_.runAction(id);
            return QJsonObject{{"ok", true}};
        }

        if (cmd == "launch") {
            const int index = req.value("index").toInt(-1);
            if (index >= 0) launcher_.runAction(index);
            return QJsonObject{{"ok", true}};
        }

        return QJsonObject{{"ok", false}, {"error", "unknown cmd"}};
    }

private:
    WaHostApi* hostApi_ = nullptr;
    Launcher launcher_;
};

// Required exports
WA_EXPORT const WaPluginInfo* WA_CALL wa_get_info() {
    return &INFO;
}

WA_EXPORT void* WA_CALL wa_create(void* hostCtx, const char* configJsonUtf8) {
    return new LauncherPlugin(hostCtx, configJsonUtf8);
}

WA_EXPORT int32_t WA_CALL wa_init(void* handle) {
    if (!handle) return WA_ERR_BAD_ARG;
    return static_cast<LauncherPlugin*>(handle)->init();
}

WA_EXPORT int32_t WA_CALL wa_start(void* handle) {
    if (!handle) return WA_ERR_BAD_ARG;
    return static_cast<LauncherPlugin*>(handle)->start();
}

WA_EXPORT int32_t WA_CALL wa_pause(void* handle) {
    if (!handle) return WA_ERR_BAD_ARG;
    return static_cast<LauncherPlugin*>(handle)->pause();
}

WA_EXPORT int32_t WA_CALL wa_resume(void* handle) {
    if (!handle) return WA_ERR_BAD_ARG;
    return static_cast<LauncherPlugin*>(handle)->resume();
}

WA_EXPORT int32_t WA_CALL wa_stop(void* handle) {
    if (!handle) return WA_ERR_BAD_ARG;
    return static_cast<LauncherPlugin*>(handle)->stop();
}

WA_EXPORT void WA_CALL wa_destroy(void* handle) {
    delete static_cast<LauncherPlugin*>(handle);
}

WA_EXPORT WaView WA_CALL wa_read(void* handle) {
    if (!handle) return {nullptr, 0};
    return static_cast<LauncherPlugin*>(handle)->readView();
}

WA_EXPORT WaView WA_CALL wa_request(void* handle, const char* reqJsonUtf8) {
    if (!handle) return {nullptr, 0};
    return static_cast<LauncherPlugin*>(handle)->requestView(reqJsonUtf8);
}

// Optional UI export
WA_EXPORT QWidget* WA_CALL wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<LauncherPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new LauncherUi(p->hostApi(), parent);
}
