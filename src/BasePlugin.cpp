#include "BasePlugin.h"

#include <QJsonDocument>
#include <QJsonParseError>

BasePlugin::BasePlugin(uint32_t defaultIntervalMs, const char* configJsonUtf8) {
    intervalMs_.store(defaultIntervalMs);

    QString err;
    config_ = parseObjectUtf8(configJsonUtf8, &err);
    if (!config_.isEmpty()) {
        const auto v = config_.value("intervalMs");
        if (v.isDouble()) {
            const int ms = v.toInt();
            if (ms > 0) intervalMs_.store((uint32_t)ms);
        }
    }

    // Default snapshot
    setSnapshotObject(QJsonObject{
        {"ok", false},
        {"error", "not_started"}
    });
}

BasePlugin::~BasePlugin() {
    stop();
}

int32_t BasePlugin::init() {
    State expected = State::Constructed;
    if (!state_.compare_exchange_strong(expected, State::Constructed)) {
        // already inited or running
    }

    QString err;
    if (!onInit(err)) {
        setSnapshotObject(QJsonObject{{"ok", false}, {"error", err.isEmpty() ? "init_failed" : err}});
        return WA_ERR;
    }

    state_.store(State::Inited);
    return WA_OK;
}

int32_t BasePlugin::start() {
    auto st = state_.load();
    if (st != State::Inited && st != State::Paused) {
        if (st == State::Running) return WA_OK;
        return WA_ERR_BAD_STATE;
    }

    if (!worker_.joinable()) {
        stopRequested_.store(false);
        worker_ = std::thread(&BasePlugin::threadMain, this);
    }

    state_.store(State::Running);
    cv_.notify_all();
    return WA_OK;
}

int32_t BasePlugin::pause() {
    auto st = state_.load();
    if (st == State::Running) {
        state_.store(State::Paused);
        cv_.notify_all();
        return WA_OK;
    }
    return (st == State::Paused) ? WA_OK : WA_ERR_BAD_STATE;
}

int32_t BasePlugin::resume() {
    auto st = state_.load();
    if (st == State::Paused) {
        state_.store(State::Running);
        cv_.notify_all();
        return WA_OK;
    }
    return (st == State::Running) ? WA_OK : WA_ERR_BAD_STATE;
}

int32_t BasePlugin::stop() {
    if (state_.load() == State::Stopped) return WA_OK;

    stopRequested_.store(true);
    cv_.notify_all();

    if (worker_.joinable()) worker_.join();

    state_.store(State::Stopped);
    onStop();
    return WA_OK;
}

void BasePlugin::setIntervalMs(uint32_t ms) {
    if (ms == 0) return;
    intervalMs_.store(ms);
    cv_.notify_all();
}

WaView BasePlugin::readView() {
    std::lock_guard<std::mutex> g(snapMu_);
    readBuf_ = latest_; // stable buffer for caller
    if (readBuf_.isEmpty()) readBuf_ = "{}";
    return { readBuf_.constData(), (uint32_t)readBuf_.size() };
}

WaView BasePlugin::requestView(const char* requestJsonUtf8) {
    QString err;
    QJsonObject req = parseObjectUtf8(requestJsonUtf8, &err);
    QJsonObject resp;

    if (!err.isEmpty()) {
        resp = QJsonObject{{"ok", false}, {"error", "bad_json"}, {"details", err}};
    } else {
        resp = onRequest(req);
    }

    QJsonDocument doc(resp);
    {
        std::lock_guard<std::mutex> g(snapMu_);
        replyBuf_ = doc.toJson(QJsonDocument::Compact);
        if (replyBuf_.isEmpty()) replyBuf_ = "{}";
        return { replyBuf_.constData(), (uint32_t)replyBuf_.size() };
    }
}

QJsonObject BasePlugin::onRequest(const QJsonObject& req) {
    Q_UNUSED(req);
    return QJsonObject{{"ok", true}};
}

void BasePlugin::threadMain() {
    while (!stopRequested_.load()) {
        // Wait until Running (or stop)
        {
            std::unique_lock<std::mutex> lk(cvMu_);
            cv_.wait(lk, [&]{
                return stopRequested_.load() || state_.load() == State::Running;
            });
        }
        if (stopRequested_.load()) break;

        // Tick
        QJsonObject obj = onTick();
        setSnapshotObject(obj);

        // Sleep interval or wake on pause/stop/interval change
        const auto ms = intervalMs_.load();
        std::unique_lock<std::mutex> lk(cvMu_);
        cv_.wait_for(lk, std::chrono::milliseconds(ms), [&]{
            return stopRequested_.load() || state_.load() != State::Running;
        });
        // if paused -> loop will wait for Running again
    }
}

void BasePlugin::setSnapshotObject(const QJsonObject& obj) {
    QJsonDocument doc(obj);
    std::lock_guard<std::mutex> g(snapMu_);
    latest_ = doc.toJson(QJsonDocument::Compact);
    if (latest_.isEmpty()) latest_ = "{}";
}

QJsonObject BasePlugin::parseObjectUtf8(const char* jsonUtf8, QString* errOut) {
    if (errOut) errOut->clear();
    if (!jsonUtf8 || !*jsonUtf8) return QJsonObject{}; // empty = {}

    QJsonParseError pe{};
    const QByteArray bytes(jsonUtf8);
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (errOut) *errOut = pe.errorString();
        return QJsonObject{};
    }
    if (!doc.isObject()) {
        if (errOut) *errOut = "json_root_is_not_object";
        return QJsonObject{};
    }
    return doc.object();
}
