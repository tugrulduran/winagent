#pragma once

#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <QString>
#include <QByteArray>
#include <QJsonObject>

// =========================
// C ABI (Host <-> Plugin)
// =========================
static constexpr uint32_t WA_PLUGIN_API_VERSION = 1;

struct WaView {
    const char* ptr;
    uint32_t    len;
};

struct WaPluginInfo {
    uint32_t    apiVersion;        // must equal WA_PLUGIN_API_VERSION
    const char* id;                // stable unique id (e.g., "cpu")
    const char* name;              // display name
    const char* desc;              // description
    uint32_t    defaultIntervalMs; // plugin default sampling interval
};

enum WaRc : int32_t {
    WA_OK = 0,
    WA_ERR = 1,
    WA_ERR_BAD_STATE = 2,
    WA_ERR_BAD_ARG = 3,
};

#if defined(_WIN32)
  #define WA_CALL __cdecl
  #if defined(WA_BUILDING_PLUGIN)
    #define WA_EXPORT extern "C" __declspec(dllexport)
  #else
    #define WA_EXPORT extern "C"
  #endif
#else
  #define WA_CALL
  #define WA_EXPORT extern "C"
#endif

// =========================
// Optional Host API (plugin -> host)
// =========================
static constexpr uint32_t WA_HOST_API_VERSION = 1;

enum WaPluginState : int32_t {
    WA_STATE_MISSING = -1,
    WA_STATE_STOPPED = 0,
    WA_STATE_RUNNING = 1,
    WA_STATE_PAUSED  = 2,
    WA_STATE_ERROR   = 3,
};

struct WaHostApi {
    uint32_t apiVersion;
    void*    user;

    int32_t (WA_CALL *plugin_get_state)(void* user, const char* pluginIdUtf8);
    int32_t (WA_CALL *plugin_start)(void* user, const char* pluginIdUtf8);
    int32_t (WA_CALL *plugin_stop)(void* user, const char* pluginIdUtf8);
    int32_t (WA_CALL *plugin_restart)(void* user, const char* pluginIdUtf8);
};

// Required exports:
WA_EXPORT const WaPluginInfo* WA_CALL wa_get_info();
WA_EXPORT void*   WA_CALL wa_create(void* hostCtx, const char* configJsonUtf8);
WA_EXPORT int32_t WA_CALL wa_init(void* handle);
WA_EXPORT int32_t WA_CALL wa_start(void* handle);
WA_EXPORT int32_t WA_CALL wa_pause(void* handle);
WA_EXPORT int32_t WA_CALL wa_resume(void* handle);
WA_EXPORT int32_t WA_CALL wa_stop(void* handle);
WA_EXPORT void    WA_CALL wa_destroy(void* handle);

// Runtime I/O:
WA_EXPORT WaView  WA_CALL wa_request(void* handle, const char* requestJsonUtf8);
WA_EXPORT WaView  WA_CALL wa_read(void* handle);

// =========================
// C++ BasePlugin
// =========================
class BasePlugin {
public:
    explicit BasePlugin(uint32_t defaultIntervalMs, const char* configJsonUtf8 = nullptr);
    virtual ~BasePlugin();

    int32_t init();
    int32_t start();
    int32_t pause();
    int32_t resume();
    int32_t stop();

    // Host-facing views (stable until next call of same function for this handle)
    WaView readView();
    WaView requestView(const char* requestJsonUtf8);

    uint32_t intervalMs() const noexcept { return intervalMs_.load(); }
    void setIntervalMs(uint32_t ms);

    // Parsed config object (from configJsonUtf8 passed to ctor)
    const QJsonObject& config() const noexcept { return config_; }

protected:
    // Implement in plugin:
    virtual bool onInit(QString& err) { Q_UNUSED(err); return true; }
    virtual void onStop() {}

    // Return JSON payload as object (BasePlugin serializes Compact UTF-8)
    virtual QJsonObject onTick() = 0;

    // Synchronous request handler (already parsed JSON object)
    virtual QJsonObject onRequest(const QJsonObject& req);

private:
    enum class State { Constructed, Inited, Running, Paused, Stopped };

    void threadMain();
    void setSnapshotObject(const QJsonObject& obj);
    static QJsonObject parseObjectUtf8(const char* jsonUtf8, QString* errOut);

    std::atomic<State> state_{State::Constructed};
    std::atomic<bool>  stopRequested_{false};
    std::atomic<uint32_t> intervalMs_{1000};

    std::thread worker_;
    std::mutex  cvMu_;
    std::condition_variable cv_;

    // JSON config
    QJsonObject config_{};

    // Snapshot / reply buffers
    std::mutex snapMu_;
    QByteArray latest_;
    QByteArray readBuf_;
    QByteArray replyBuf_;
};
