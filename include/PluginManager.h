#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "BasePlugin.h"
#include <QString>
#include <QLibrary>

// PluginManager
// -------------
// Loads external plugin DLLs from a directory and manages their lifecycle.
// Plugins are *not* C++ objects exposed to the host; the host only holds
// opaque handles and calls C ABI functions.

class PluginManager {
public:
    bool loadFromDir(const QString& dirPath, void* hostCtx);
    void stopAll();

    // Read latest JSON snapshots from all plugins as:
    QJsonObject readAll() const;

    // Route a request to a specific plugin.
    // Returns {} if plugin not found or response invalid.
    QJsonObject request(const QString& id, const QJsonObject& payload) const;

    bool has(const QString& id) const;

private:
    struct Loaded {
        QLibrary lib;
        const WaPluginInfo* (WA_CALL *get_info)() = nullptr;
        void*   (WA_CALL *create)(void*, const char*) = nullptr;
        int32_t (WA_CALL *init)(void*) = nullptr;
        int32_t (WA_CALL *start)(void*) = nullptr;
        int32_t (WA_CALL *pause)(void*) = nullptr;
        int32_t (WA_CALL *resume)(void*) = nullptr;
        int32_t (WA_CALL *stop)(void*) = nullptr;
        void    (WA_CALL *destroy)(void*) = nullptr;

        WaView  (WA_CALL *read)(void*) = nullptr;
        WaView  (WA_CALL *req)(void*, const char*) = nullptr;

        const WaPluginInfo* info = nullptr;
        void* handle = nullptr;
    };

    std::vector<std::unique_ptr<Loaded>> plugins_;
    std::unordered_map<std::string, size_t> byId_;

    static QJsonObject parseJsonObjectUtf8(const char* ptr, uint32_t len);
};
