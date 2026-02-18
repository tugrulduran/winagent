#include "PluginManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QWidget>

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

PluginManager::PluginManager() {
    hostApi_.apiVersion = WA_HOST_API_VERSION;
    hostApi_.user = this;
    hostApi_.plugin_get_state = &PluginManager::host_get_state;
    hostApi_.plugin_start = &PluginManager::host_start;
    hostApi_.plugin_stop = &PluginManager::host_stop;
    hostApi_.plugin_restart = &PluginManager::host_restart;
}

PluginManager::Loaded* PluginManager::findLoadedNoLock(const QString& id) const {
    const auto it = byId_.find(id.toStdString());
    if (it == byId_.end()) return nullptr;
    const size_t idx = it->second;
    if (idx >= plugins_.size()) return nullptr;
    return plugins_[idx].get();
}

bool PluginManager::loadFromDir(const QString& dirPath, void* hostCtx) {
    std::lock_guard<std::mutex> g(mu_);

    pluginsDir_ = dirPath;
    hostCtx_ = hostCtx;

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
        p->get_info = (FnGetInfo)p->lib.resolve("wa_get_info");
        p->create   = (FnCreate)p->lib.resolve("wa_create");
        p->init     = (FnInit)p->lib.resolve("wa_init");
        p->start    = (FnStart)p->lib.resolve("wa_start");
        p->stop     = (FnStop)p->lib.resolve("wa_stop");
        p->destroy  = (FnDestroy)p->lib.resolve("wa_destroy");
        p->read     = (FnRead)p->lib.resolve("wa_read");
        p->req      = (FnReq)p->lib.resolve("wa_request");

        // Optional exports
        p->pause   = (FnPause)p->lib.resolve("wa_pause");
        p->resume  = (FnResume)p->lib.resolve("wa_resume");
        p->create_widget = (FnCreateWidget)p->lib.resolve("wa_create_widget");

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

        // Load per-plugin config (optional): <plugins_dir>/<id>/config.json
        // (Project convention: each plugin owns its folder.)
        const QString cfgPath = dir.absoluteFilePath(QString("%1/config.json").arg(QString::fromUtf8(p->info->id)));
        p->configPath = cfgPath;
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
            p->handle = nullptr;
            p->lib.unload();
            continue;
        }
        if (p->start(p->handle) != WA_OK) {
            Logger::error(QString("[PLUGIN] start() failed: %1 (%2)").arg(p->info->name).arg(p->info->id));
            p->destroy(p->handle);
            p->handle = nullptr;
            p->lib.unload();
            continue;
        }

        p->state = State::Running;

        byId_[p->info->id] = plugins_.size();
        Logger::success(QString("[PLUGIN] Loaded: %1 (%2)").arg(p->info->name).arg(p->info->id));
        plugins_.push_back(std::move(p));
    }

    return true;
}

void PluginManager::stopAll() {
    std::lock_guard<std::mutex> g(mu_);
    for (auto& p : plugins_) {
        if (!p || !p->handle) continue;
        (void)p->stop(p->handle);
        p->destroy(p->handle);
        p->handle = nullptr;
        p->state = State::Stopped;
        p->lib.unload();
    }
    plugins_.clear();
    byId_.clear();
    pluginsDir_.clear();
    hostCtx_ = nullptr;
}

QJsonObject PluginManager::readAll() const {
    std::lock_guard<std::mutex> g(mu_);
    QJsonObject out;
    for (const auto& p : plugins_) {
        if (!p || !p->info || !p->handle) continue;
        WaView v = p->read(p->handle);
        QJsonObject obj = parseJsonObjectUtf8(v.ptr, v.len);
        if (!obj.isEmpty()) out[QString::fromUtf8(p->info->id)] = obj;
    }
    return out;
}

QJsonObject PluginManager::request(const QString& id, const QJsonObject& payload) const {
    std::lock_guard<std::mutex> g(mu_);

    Loaded* pp = findLoadedNoLock(id);
    if (!pp || !pp->handle || !pp->req) return {};

    const QByteArray reqBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const WaView v = pp->req(pp->handle, reqBytes.constData());
    return parseJsonObjectUtf8(v.ptr, v.len);
}

bool PluginManager::has(const QString& id) const {
    std::lock_guard<std::mutex> g(mu_);
    return byId_.find(id.toStdString()) != byId_.end();
}

std::vector<PluginManager::PluginDesc> PluginManager::list() const {
    std::lock_guard<std::mutex> g(mu_);
    std::vector<PluginDesc> out;
    out.reserve(plugins_.size());
    for (const auto& p : plugins_) {
        if (!p || !p->info) continue;
        PluginDesc d;
        d.id = QString::fromUtf8(p->info->id);
        d.name = QString::fromUtf8(p->info->name ? p->info->name : p->info->id);
        d.hasUi = (p->create_widget != nullptr);
        out.push_back(d);
    }
    return out;
}

QWidget* PluginManager::createWidget(const QString& id, QWidget* parent) const {
    FnCreateWidget fn = nullptr;
    void* handle = nullptr;
    {
        std::lock_guard<std::mutex> g(mu_);
        Loaded* p = findLoadedNoLock(id);
        if (!p || !p->handle || !p->create_widget) return nullptr;
        fn = p->create_widget;
        handle = p->handle;
    }
    return fn(handle, parent);
}

int32_t PluginManager::recreatePluginNoLock(Loaded* p) {
    if (!p || !p->create || !p->init || !p->start) return WA_ERR;

    const QByteArray cfgJson = readTextFileIfExists(p->configPath);
    p->handle = p->create(hostCtx_, cfgJson.isEmpty() ? nullptr : cfgJson.constData());
    if (!p->handle) {
        p->state = State::Error;
        return WA_ERR;
    }

    if (p->init(p->handle) != WA_OK) {
        p->destroy(p->handle);
        p->handle = nullptr;
        p->state = State::Error;
        return WA_ERR;
    }
    if (p->start(p->handle) != WA_OK) {
        p->destroy(p->handle);
        p->handle = nullptr;
        p->state = State::Error;
        return WA_ERR;
    }

    p->state = State::Running;
    return WA_OK;
}

int32_t PluginManager::startPlugin(const QString& id) {
    std::lock_guard<std::mutex> g(mu_);
    Loaded* p = findLoadedNoLock(id);
    if (!p) return WA_ERR_BAD_ARG;

    if (p->state == State::Running) return WA_OK;

    if (p->state == State::Paused) {
        if (p->resume) {
            const int32_t rc = p->resume(p->handle);
            if (rc == WA_OK) p->state = State::Running;
            return rc;
        }
        // no resume => full recreate
        if (p->handle) {
            (void)p->stop(p->handle);
            p->destroy(p->handle);
            p->handle = nullptr;
        }
        return recreatePluginNoLock(p);
    }

    // Stopped/Error => recreate
    if (p->handle) {
        (void)p->stop(p->handle);
        p->destroy(p->handle);
        p->handle = nullptr;
    }
    return recreatePluginNoLock(p);
}

int32_t PluginManager::stopPlugin(const QString& id) {
    std::lock_guard<std::mutex> g(mu_);
    Loaded* p = findLoadedNoLock(id);
    if (!p) return WA_ERR_BAD_ARG;

    if (!p->handle) {
        p->state = State::Stopped;
        return WA_OK;
    }

    if (p->pause) {
        const int32_t rc = p->pause(p->handle);
        if (rc == WA_OK) p->state = State::Paused;
        return rc;
    }

    const int32_t rc = p->stop(p->handle);
    if (rc == WA_OK) p->state = State::Stopped;
    return rc;
}

int32_t PluginManager::restartPlugin(const QString& id) {
    std::lock_guard<std::mutex> g(mu_);
    Loaded* p = findLoadedNoLock(id);
    if (!p) return WA_ERR_BAD_ARG;

    if (p->handle) {
        (void)p->stop(p->handle);
        p->destroy(p->handle);
        p->handle = nullptr;
    }

    return recreatePluginNoLock(p);
}

int32_t PluginManager::pluginState(const QString& id) const {
    std::lock_guard<std::mutex> g(mu_);
    Loaded* p = findLoadedNoLock(id);
    if (!p) return (int32_t)State::Missing;
    return (int32_t)p->state;
}

// ---- Host API static wrappers ----
int32_t WA_CALL PluginManager::host_get_state(void* user, const char* pluginIdUtf8) {
    auto* pm = static_cast<PluginManager*>(user);
    if (!pm || !pluginIdUtf8) return WA_STATE_MISSING;
    return pm->pluginState(QString::fromUtf8(pluginIdUtf8));
}

int32_t WA_CALL PluginManager::host_start(void* user, const char* pluginIdUtf8) {
    auto* pm = static_cast<PluginManager*>(user);
    if (!pm || !pluginIdUtf8) return WA_ERR_BAD_ARG;
    return pm->startPlugin(QString::fromUtf8(pluginIdUtf8));
}

int32_t WA_CALL PluginManager::host_stop(void* user, const char* pluginIdUtf8) {
    auto* pm = static_cast<PluginManager*>(user);
    if (!pm || !pluginIdUtf8) return WA_ERR_BAD_ARG;
    return pm->stopPlugin(QString::fromUtf8(pluginIdUtf8));
}

int32_t WA_CALL PluginManager::host_restart(void* user, const char* pluginIdUtf8) {
    auto* pm = static_cast<PluginManager*>(user);
    if (!pm || !pluginIdUtf8) return WA_ERR_BAD_ARG;
    return pm->restartPlugin(QString::fromUtf8(pluginIdUtf8));
}
