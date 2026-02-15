#include "PluginManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include "Logger.h"

static QByteArray readTextFileIfExists(const QString& path) {
    QFile f(path);
    if (!f.exists()) return {};
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QJsonObject PluginManager::parseJsonObjectUtf8(const char* ptr, uint32_t len) {
    if (!ptr || len == 0) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(ptr, (int)len), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

bool PluginManager::loadFromDir(const QString& dirPath, void* hostCtx) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        Logger::info("[PLUGIN] No plugins directory: " + dirPath);
        return true;
    }

    const auto dlls = dir.entryList(QStringList() << "*.dll", QDir::Files);
    for (const auto& dllName : dlls) {
        auto p = std::make_unique<Loaded>();
        p->lib.setFileName(dir.absoluteFilePath(dllName));

        if (!p->lib.load()) {
            Logger::error("[PLUGIN] Load failed: " + dllName + " => " + p->lib.errorString());
            continue;
        }

        // Resolve required exports
        p->get_info = (decltype(p->get_info))p->lib.resolve("wa_get_info");
        p->create   = (decltype(p->create))p->lib.resolve("wa_create");
        p->init     = (decltype(p->init))p->lib.resolve("wa_init");
        p->start    = (decltype(p->start))p->lib.resolve("wa_start");
        p->stop     = (decltype(p->stop))p->lib.resolve("wa_stop");
        p->destroy  = (decltype(p->destroy))p->lib.resolve("wa_destroy");
        p->read     = (decltype(p->read))p->lib.resolve("wa_read");
        p->req      = (decltype(p->req))p->lib.resolve("wa_request");

        // Optional exports
        p->pause   = (decltype(p->pause))p->lib.resolve("wa_pause");
        p->resume  = (decltype(p->resume))p->lib.resolve("wa_resume");

        const bool missingRequired =
            !p->get_info || !p->create || !p->init || !p->start ||
            !p->stop || !p->destroy || !p->read || !p->req;
        if (missingRequired) {
            Logger::error("[PLUGIN] Missing exports: " + dllName);
            p->lib.unload();
            continue;
        }

        p->info = p->get_info();
        if (!p->info || p->info->apiVersion != WA_PLUGIN_API_VERSION || !p->info->id) {
            Logger::error("[PLUGIN] Invalid plugin info: " + dllName);
            p->lib.unload();
            continue;
        }

        // Load per-plugin config (optional): <plugins_dir>/<id>.json
        const QString cfgPath = dir.absoluteFilePath(QString("%1.json").arg(QString::fromUtf8(p->info->id)));
        const QByteArray cfgJson = readTextFileIfExists(cfgPath);

        p->handle = p->create(hostCtx, cfgJson.isEmpty() ? nullptr : cfgJson.constData());
        if (!p->handle) {
            Logger::error(QString("[PLUGIN] create() failed: %1 (%2)").arg(p->info->name).arg(p->info->id));
            p->lib.unload();
            continue;
        }

        if (p->init(p->handle) != WA_OK) {
            Logger::error(QString("[PLUGIN] init() failed: %1 (%2)").arg(p->info->name).arg(p->info->id));
            p->destroy(p->handle);
            p->lib.unload();
            continue;
        }
        if (p->start(p->handle) != WA_OK) {
            Logger::error(QString("[PLUGIN] start() failed: %1 (%2)").arg(p->info->name).arg(p->info->id));
            p->destroy(p->handle);
            p->lib.unload();
            continue;
        }

        byId_[p->info->id] = plugins_.size();
        Logger::success(QString("[PLUGIN] Loaded: %1 (%2)").arg(p->info->name).arg(p->info->id));
        plugins_.push_back(std::move(p));
    }

    return true;
}

void PluginManager::stopAll() {
    for (auto& p : plugins_) {
        if (!p || !p->handle) continue;
        (void)p->stop(p->handle);
        p->destroy(p->handle);
        p->handle = nullptr;
        p->lib.unload();
    }
    plugins_.clear();
    byId_.clear();
}

QJsonObject PluginManager::readAll() const {
    QJsonObject out;
    for (const auto& p : plugins_) {
        if (!p || !p->info || !p->handle) continue;
        WaView v = p->read(p->handle);
        QJsonObject obj = parseJsonObjectUtf8(v.ptr, v.len); // zaten Qt JSON
        if (!obj.isEmpty()) out[QString::fromUtf8(p->info->id)] = obj;
    }
    return out;
}

QJsonObject PluginManager::request(const QString& id, const QJsonObject& payload) const {
    const auto it = byId_.find(id.toStdString());
    if (it == byId_.end()) return {};

    const auto& pp = plugins_.at(it->second);
    if (!pp || !pp->handle) return {};

    const Loaded& p = *pp;
    if (!p.req) return {};  // safety

    // payload -> compact JSON utf8
    const QByteArray reqBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // call plugin endpoint
    const WaView v = p.req(p.handle, reqBytes.constData());

    // parse response
    return parseJsonObjectUtf8(v.ptr, v.len);
}

bool PluginManager::has(const QString& id) const {
    return byId_.find(id.toStdString()) != byId_.end();
}
