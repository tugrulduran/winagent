#include "PluginManager.h"

#include <windows.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QWidget>

#include "Logger.h"

static QByteArray readTextFileIfExists(const QString &path) {
    QFile f(path);
    if (!f.exists()) return {};
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QJsonObject PluginManager::parseJsonObjectUtf8(const char *ptr, uint32_t len) {
    if (!ptr || len == 0) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(ptr, (int) len), &err);
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

PluginManager::Loaded *PluginManager::findLoadedNoLock(const QString &id) const {
    const auto it = byId_.find(id.toStdString());
    if (it == byId_.end()) return nullptr;
    const size_t idx = it->second;
    if (idx >= plugins_.size()) return nullptr;
    return plugins_[idx].get();
}

bool PluginManager::loadFromDir(const QString &dirPath, void *hostCtx) { {
        std::lock_guard<std::mutex> g(mu_);
        pluginsDir_ = dirPath;
        hostCtx_ = hostCtx;
    }

    QDir root(dirPath);
    if (!root.exists()) {
        Logger::info("[PLUGIN] No plugins directory: " + dirPath);
        return true;
    }

    // Layout:
    // plugins/<id>/<id>.dll
    // plugins/<id>/config.json
    const auto subdirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &sub: subdirs) {
        const QString folderName = sub.fileName();
        QDir pd(sub.absoluteFilePath());

        QStringList candidates;
        candidates << pd.absoluteFilePath(folderName + ".dll");

        for (const QString &dllPath: candidates) {
            if (!QFileInfo::exists(dllPath))
                continue;

            auto p = std::make_unique<Loaded>();
            p->lib.setFileName(dllPath);

            const QString folder = QFileInfo(dllPath).absolutePath();
            SetDllDirectoryW(reinterpret_cast<LPCWSTR>(folder.utf16()));

            if (!p->lib.load()) {
                Logger::error("[PLUGIN] Load failed: " + QFileInfo(dllPath).fileName() + " => " + p->lib.errorString());
                continue;
            }

            // Required exports
            p->get_info = (FnGetInfo) p->lib.resolve("wa_get_info");
            p->create = (FnCreate) p->lib.resolve("wa_create");
            p->init = (FnInit) p->lib.resolve("wa_init");
            p->start = (FnStart) p->lib.resolve("wa_start");
            p->stop = (FnStop) p->lib.resolve("wa_stop");
            p->destroy = (FnDestroy) p->lib.resolve("wa_destroy");
            p->read = (FnRead) p->lib.resolve("wa_read");
            p->req = (FnReq) p->lib.resolve("wa_request");

            // Optional exports
            p->pause = (FnPause) p->lib.resolve("wa_pause");
            p->resume = (FnResume) p->lib.resolve("wa_resume");
            p->create_widget = (FnCreateWidget) p->lib.resolve("wa_create_widget");

            const bool missingRequired =
                    !p->get_info || !p->create || !p->init || !p->start ||
                    !p->stop || !p->destroy || !p->read || !p->req;

            if (missingRequired) {
                Logger::error("[PLUGIN] Missing exports: " + QFileInfo(dllPath).fileName());
                p->lib.unload();
                continue;
            }

            p->info = p->get_info();
            if (!p->info || p->info->apiVersion != WA_PLUGIN_API_VERSION || !p->info->id) {
                Logger::error("[PLUGIN] Invalid plugin info: " + QFileInfo(dllPath).fileName());
                p->lib.unload();
                continue;
            }

            // Config: plugins/<folderName>/config.json
            p->configPath = pd.absoluteFilePath("config.json");
            const QByteArray cfgJson = readTextFileIfExists(p->configPath);

            // Create/init/start
            p->handle = p->create(hostCtx, cfgJson.isEmpty() ? nullptr : cfgJson.constData());
            if (!p->handle) {
                Logger::error(QString("[PLUGIN] create() failed: %1 (%2)")
                    .arg(p->info->name)
                    .arg(p->info->id));
                p->lib.unload();
                continue;
            }

            if (p->init(p->handle) != WA_OK) {
                Logger::error(QString("[PLUGIN] init() failed: %1 (%2)")
                    .arg(p->info->name)
                    .arg(p->info->id));
                p->destroy(p->handle);
                p->handle = nullptr;
                p->lib.unload();
                continue;
            }

            if (p->start(p->handle) != WA_OK) {
                Logger::error(QString("[PLUGIN] start() failed: %1 (%2)")
                    .arg(p->info->name)
                    .arg(p->info->id));
                p->destroy(p->handle);
                p->handle = nullptr;
                p->lib.unload();
                continue;
            }

            p->state = State::Running;

            const auto name = p->info->name ? p->info->name : p->info->id;
            const auto id = p->info->id; {
                std::lock_guard<std::mutex> g(mu_);
                byId_[p->info->id] = plugins_.size();
                plugins_.push_back(std::move(p));
            }

            Logger::success(QString("[PLUGIN] Loaded: %1 (%2)").arg(name).arg(id));
            break;
        }
    }

    return true;
}

void PluginManager::stopAll() {
    std::unique_lock<std::mutex> lk(mu_);

    for (auto &p: plugins_) {
        if (!p) continue;

        // Wait until no in-flight calls (read/request/widget create)
        cv_.wait(lk, [&] { return p->inFlight.load(std::memory_order_relaxed) == 0; });

        if (!p->handle) {
            p->state = State::Stopped;
            p->lib.unload();
            continue;
        }

        void *handle = p->handle;
        auto stopFn = p->stop;
        auto destroyFn = p->destroy;

        p->handle = nullptr;
        p->state = State::Stopped;

        lk.unlock();
        (void) stopFn(handle);
        destroyFn(handle);
        lk.lock();

        p->lib.unload();
    }

    plugins_.clear();
    byId_.clear();
    pluginsDir_.clear();
    hostCtx_ = nullptr;
}

QJsonObject PluginManager::readAll() {
    QJsonObject out;

    // Iterate by index so the pointer stays stable while unlocked.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (size_t i = 0;; i++) {
        FnRead readFn = nullptr;
        void *handle = nullptr;
        QString id;
        Loaded *p = nullptr; {
            std::unique_lock<std::mutex> lk(mu_);
            if (i >= plugins_.size()) break;
            p = plugins_[i].get();
            if (!p || !p->info || !p->handle || !p->read) continue;
            if (p->state != State::Running) continue;

            p->inFlight.fetch_add(1, std::memory_order_relaxed);
            p->reads++;
            p->lastReadMs = nowMs;

            readFn = p->read;
            handle = p->handle;
            id = QString::fromUtf8(p->info->id);

            lk.unlock();

            const WaView v = readFn(handle);
            const QJsonObject obj = parseJsonObjectUtf8(v.ptr, v.len);
            if (!obj.isEmpty()) out[id] = obj;

            lk.lock();
            const int left = p->inFlight.fetch_sub(1, std::memory_order_relaxed) - 1;
            if (left == 0) cv_.notify_all();
        }
    }

    return out;
}

QJsonObject PluginManager::request(const QString &id, const QJsonObject &payload) {
    Loaded *p = nullptr;
    FnReq reqFn = nullptr;
    void *handle = nullptr;
    QByteArray reqBytes; {
        std::lock_guard<std::mutex> g(mu_);
        p = findLoadedNoLock(id);
        if (!p || !p->handle || !p->req) return {};

        p->inFlight.fetch_add(1, std::memory_order_relaxed);
        p->requests++;
        p->lastRequestMs = QDateTime::currentMSecsSinceEpoch();

        reqFn = p->req;
        handle = p->handle;
        reqBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    }

    const WaView v = reqFn(handle, reqBytes.constData());
    const QJsonObject out = parseJsonObjectUtf8(v.ptr, v.len); {
        std::lock_guard<std::mutex> g(mu_);
        // p is stable while plugins_ vector doesn't shrink during runtime
        if (p) {
            const int left = p->inFlight.fetch_sub(1, std::memory_order_relaxed) - 1;
            if (left == 0) cv_.notify_all();
        }
    }

    return out;
}

void PluginManager::markSent(const QStringList &pluginIds) {
    std::lock_guard<std::mutex> g(mu_);
    for (const QString &id: pluginIds) {
        Loaded *p = findLoadedNoLock(id);
        if (!p) continue;
        p->sent++;
    }
}

bool PluginManager::has(const QString &id) const {
    std::lock_guard<std::mutex> g(mu_);
    return byId_.find(id.toStdString()) != byId_.end();
}

std::vector<PluginManager::PluginDesc> PluginManager::list() const {
    std::lock_guard<std::mutex> g(mu_);
    std::vector<PluginDesc> out;
    out.reserve(plugins_.size());
    for (const auto &p: plugins_) {
        if (!p || !p->info) continue;
        PluginDesc d;
        d.id = QString::fromUtf8(p->info->id);
        d.name = QString::fromUtf8(p->info->name ? p->info->name : p->info->id);
        d.description = p->info->desc;
        d.defaultIntervalMs = p->info->defaultIntervalMs;
        d.hasUi = (p->create_widget != nullptr);
        out.push_back(d);
    }
    return out;
}

std::vector<PluginManager::PluginUiSnapshot> PluginManager::snapshotUi() const {
    std::lock_guard<std::mutex> g(mu_);
    std::vector<PluginUiSnapshot> out;
    out.reserve(plugins_.size());

    for (const auto &p: plugins_) {
        if (!p || !p->info) continue;
        PluginUiSnapshot s;
        s.id = QString::fromUtf8(p->info->id);
        s.name = QString::fromUtf8(p->info->name ? p->info->name : p->info->id);
        s.description = p->info->desc;
        s.defaultIntervalMs = p->info->defaultIntervalMs;
        s.hasUi = (p->create_widget != nullptr);
        s.state = (int32_t) p->state;
        s.reads = p->reads;
        s.sent = p->sent;
        s.requests = p->requests;
        s.lastReadMs = p->lastReadMs;
        s.lastRequestMs = p->lastRequestMs;
        out.push_back(s);
    }

    return out;
}

QWidget *PluginManager::createWidget(const QString &id, QWidget *parent) const {
    FnCreateWidget fn = nullptr;
    void *handle = nullptr;
    Loaded *p = nullptr; {
        std::lock_guard<std::mutex> g(mu_);
        p = findLoadedNoLock(id);
        if (!p || !p->handle || !p->create_widget) return nullptr;
        fn = p->create_widget;
        handle = p->handle;
        p->inFlight.fetch_add(1, std::memory_order_relaxed);
    }

    QWidget *w = fn(handle, parent); {
        std::lock_guard<std::mutex> g(mu_);
        if (p) {
            const int left = p->inFlight.fetch_sub(1, std::memory_order_relaxed) - 1;
            if (left == 0) cv_.notify_all();
        }
    }

    return w;
}

void PluginManager::waitNoInflight(std::unique_lock<std::mutex> &lk, PluginManager::Loaded *p, std::condition_variable &cv) {
    if (!p) return;
    cv.wait(lk, [&] { return p->inFlight.load(std::memory_order_relaxed) == 0; });
}


int32_t PluginManager::startPlugin(const QString &id) {
    std::unique_lock<std::mutex> lk(mu_);
    Loaded *p = findLoadedNoLock(id);
    if (!p) return WA_ERR_BAD_ARG;

    if (p->state == State::Running) return WA_OK;

    waitNoInflight(lk, p, cv_);

    // Resume if supported and currently paused
    if (p->state == State::Paused && p->resume && p->handle) {
        void *handle = p->handle;
        auto resumeFn = p->resume;
        lk.unlock();
        const int32_t rc = resumeFn(handle);
        lk.lock();
        if (rc == WA_OK) p->state = State::Running;
        return rc;
    }

    // Otherwise: full recreate
    void *oldHandle = p->handle;
    auto stopFn = p->stop;
    auto destroyFn = p->destroy;

    p->handle = nullptr;
    p->state = State::Stopped;

    const QByteArray cfgJson = readTextFileIfExists(p->configPath);
    void *hostCtx = hostCtx_;
    auto createFn = p->create;
    auto initFn = p->init;
    auto startFn = p->start;

    lk.unlock();
    if (oldHandle) {
        (void) stopFn(oldHandle);
        destroyFn(oldHandle);
    }

    void *newHandle = createFn(hostCtx, cfgJson.isEmpty() ? nullptr : cfgJson.constData());
    if (!newHandle) {
        lk.lock();
        p->state = State::Error;
        return WA_ERR;
    }

    if (initFn(newHandle) != WA_OK) {
        destroyFn(newHandle);
        lk.lock();
        p->state = State::Error;
        return WA_ERR;
    }

    if (startFn(newHandle) != WA_OK) {
        destroyFn(newHandle);
        lk.lock();
        p->state = State::Error;
        return WA_ERR;
    }

    lk.lock();
    p->handle = newHandle;
    p->state = State::Running;
    return WA_OK;
}

int32_t PluginManager::stopPlugin(const QString &id) {
    std::unique_lock<std::mutex> lk(mu_);
    Loaded *p = findLoadedNoLock(id);
    if (!p) return WA_ERR_BAD_ARG;

    if (!p->handle) {
        p->state = State::Stopped;
        return WA_OK;
    }

    if (p->state != State::Running) {
        // Already not running
        return WA_OK;
    }

    waitNoInflight(lk, p, cv_);

    // Prefer pause (keeps plugin handle alive)
    if (p->pause) {
        void *handle = p->handle;
        auto pauseFn = p->pause;

        // Block reads while pausing
        p->state = State::Paused;

        lk.unlock();
        const int32_t rc = pauseFn(handle);
        lk.lock();
        if (rc != WA_OK) {
            p->state = State::Running;
        }
        return rc;
    }

    // No pause: stop and mark stopped (handle kept, but won't be read/broadcast)
    void *handle = p->handle;
    auto stopFn = p->stop;

    p->state = State::Stopped;

    lk.unlock();
    const int32_t rc = stopFn(handle);
    lk.lock();
    if (rc != WA_OK) {
        p->state = State::Running;
    }
    return rc;
}

int32_t PluginManager::restartPlugin(const QString &id) {
    std::unique_lock<std::mutex> lk(mu_);
    Loaded *p = findLoadedNoLock(id);
    if (!p) return WA_ERR_BAD_ARG;

    waitNoInflight(lk, p, cv_);

    void *oldHandle = p->handle;
    auto stopFn = p->stop;
    auto destroyFn = p->destroy;

    p->handle = nullptr;
    p->state = State::Stopped;

    const QByteArray cfgJson = readTextFileIfExists(p->configPath);
    void *hostCtx = hostCtx_;
    auto createFn = p->create;
    auto initFn = p->init;
    auto startFn = p->start;

    lk.unlock();

    if (oldHandle) {
        (void) stopFn(oldHandle);
        destroyFn(oldHandle);
    }

    void *newHandle = createFn(hostCtx, cfgJson.isEmpty() ? nullptr : cfgJson.constData());
    if (!newHandle) {
        lk.lock();
        p->state = State::Error;
        return WA_ERR;
    }

    if (initFn(newHandle) != WA_OK) {
        destroyFn(newHandle);
        lk.lock();
        p->state = State::Error;
        return WA_ERR;
    }

    if (startFn(newHandle) != WA_OK) {
        destroyFn(newHandle);
        lk.lock();
        p->state = State::Error;
        return WA_ERR;
    }

    lk.lock();
    p->handle = newHandle;
    p->state = State::Running;
    return WA_OK;
}

int32_t PluginManager::pluginState(const QString &id) const {
    std::lock_guard<std::mutex> g(mu_);
    Loaded *p = findLoadedNoLock(id);
    if (!p) return (int32_t) State::Missing;
    return (int32_t) p->state;
}

// ---- Host API static wrappers ----
int32_t WA_CALL PluginManager::host_get_state(void *user, const char *pluginIdUtf8) {
    auto *pm = static_cast<PluginManager *>(user);
    if (!pm || !pluginIdUtf8) return WA_STATE_MISSING;
    return pm->pluginState(QString::fromUtf8(pluginIdUtf8));
}

int32_t WA_CALL PluginManager::host_start(void *user, const char *pluginIdUtf8) {
    auto *pm = static_cast<PluginManager *>(user);
    if (!pm || !pluginIdUtf8) return WA_ERR_BAD_ARG;
    return pm->startPlugin(QString::fromUtf8(pluginIdUtf8));
}

int32_t WA_CALL PluginManager::host_stop(void *user, const char *pluginIdUtf8) {
    auto *pm = static_cast<PluginManager *>(user);
    if (!pm || !pluginIdUtf8) return WA_ERR_BAD_ARG;
    return pm->stopPlugin(QString::fromUtf8(pluginIdUtf8));
}

int32_t WA_CALL PluginManager::host_restart(void *user, const char *pluginIdUtf8) {
    auto *pm = static_cast<PluginManager *>(user);
    if (!pm || !pluginIdUtf8) return WA_ERR_BAD_ARG;
    return pm->restartPlugin(QString::fromUtf8(pluginIdUtf8));
}
