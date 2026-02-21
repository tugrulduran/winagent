#pragma once

#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <QString>
#include <QStringList>
#include <QLibrary>
#include <QJsonObject>

#include "BasePlugin.h"

class QWidget;

// PluginManager
// -------------
// Loads external plugin DLLs from a directory and manages their lifecycle.
// Plugins are *not* C++ objects exposed to the host; the host only holds
// opaque handles and calls C ABI functions.
class PluginManager {
public:
    struct PluginDesc {
        QString id;
        QString name;
        QString description;
        uint32_t defaultIntervalMs = 0;
        bool hasUi = false;
    };

    struct PluginUiSnapshot {
        QString id;
        QString name;
        QString description;
        uint32_t defaultIntervalMs = 0;
        bool hasUi = false;
        int32_t state = WA_STATE_MISSING;
        uint64_t reads = 0;
        uint64_t sent = 0;
        uint64_t requests = 0;
        qint64 lastReadMs = 0;
        qint64 lastRequestMs = 0;
    };

    PluginManager();

    // hostCtx is passed to wa_create(hostCtx, cfg)
    bool loadFromDir(const QString& dirPath, void* hostCtx);
    void stopAll();

    // Read latest JSON snapshots from all plugins as:
    QJsonObject readAll();

    // Route a request to a specific plugin.
    // Returns {} if plugin not found or response invalid.
    QJsonObject request(const QString& id, const QJsonObject& payload);

    bool has(const QString& id) const;

        // ---- UI (optional) ----
    std::vector<PluginDesc> list() const;
    std::vector<PluginUiSnapshot> snapshotUi() const;
    QWidget* createWidget(const QString& id, QWidget* parent) const;

    // Called by the WS server after broadcasting an update (increments per-plugin sent counters).
    void markSent(const QStringList& pluginIds);

    // ---- Lifecycle controls ----
    int32_t startPlugin(const QString& id);
    int32_t stopPlugin(const QString& id);
    int32_t restartPlugin(const QString& id);

    int32_t pluginState(const QString& id) const;

    // Convenience for plugins to use (points back to this manager)
    WaHostApi* hostApi() { return &hostApi_; }

private:
    using FnGetInfo = const WaPluginInfo* (WA_CALL*)();
    using FnCreate  = void* (WA_CALL*)(void*, const char*);
    using FnInit    = int32_t (WA_CALL*)(void*);
    using FnStart   = int32_t (WA_CALL*)(void*);
    using FnPause   = int32_t (WA_CALL*)(void*);
    using FnResume  = int32_t (WA_CALL*)(void*);
    using FnStop    = int32_t (WA_CALL*)(void*);
    using FnDestroy = void (WA_CALL*)(void*);
    using FnRead    = WaView (WA_CALL*)(void*);
    using FnReq     = WaView (WA_CALL*)(void*, const char*);
    using FnCreateWidget = QWidget* (WA_CALL*)(void* pluginHandle, QWidget* parent);

    enum class State : int32_t {
        Missing = WA_STATE_MISSING,
        Stopped = WA_STATE_STOPPED,
        Running = WA_STATE_RUNNING,
        Paused  = WA_STATE_PAUSED,
        Error   = WA_STATE_ERROR,
    };

    struct Loaded {
        QLibrary lib;

        FnGetInfo get_info = nullptr;
        FnCreate  create = nullptr;
        FnInit    init = nullptr;
        FnStart   start = nullptr;
        FnPause   pause = nullptr;
        FnResume  resume = nullptr;
        FnStop    stop = nullptr;
        FnDestroy destroy = nullptr;

        FnRead    read = nullptr;
        FnReq     req = nullptr;

        // Optional: create a Qt widget for plugin UI
        FnCreateWidget create_widget = nullptr;

        const WaPluginInfo* info = nullptr;
        void* handle = nullptr;
        QString configPath;
        State state = State::Stopped;

        // UI metadata
        QString description;

        // Runtime stats (for the Dashboard overview)
        std::atomic<int> inFlight{0};
        uint64_t reads = 0;
        uint64_t sent = 0;
        uint64_t requests = 0;
        qint64 lastReadMs = 0;
        qint64 lastRequestMs = 0;
    };

    mutable std::mutex mu_;
    mutable std::condition_variable cv_;
    std::vector<std::unique_ptr<Loaded>> plugins_;
    std::unordered_map<std::string, size_t> byId_;

    // Remember for restart
    QString pluginsDir_;
    void* hostCtx_ = nullptr;

    // Host API instance passed to plugins (stable for app lifetime)
    WaHostApi hostApi_{};

    static QJsonObject parseJsonObjectUtf8(const char* ptr, uint32_t len);

    void waitNoInflight(std::unique_lock<std::mutex>& lk, PluginManager::Loaded* p, std::condition_variable& cv);

    // Internal helpers (mu_ must be held)
    Loaded* findLoadedNoLock(const QString& id) const;
    static int32_t WA_CALL host_get_state(void* user, const char* pluginIdUtf8);
    static int32_t WA_CALL host_start(void* user, const char* pluginIdUtf8);
    static int32_t WA_CALL host_stop(void* user, const char* pluginIdUtf8);
    static int32_t WA_CALL host_restart(void* user, const char* pluginIdUtf8);
};
