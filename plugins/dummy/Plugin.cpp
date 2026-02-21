#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "src/DummySampler.h"
#include "src/RingLog.h"
#include "src/Utils.h"

using namespace dummy;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "dummy",
    "Dummy Plugin",
    "Example plugin that returns a dummy snapshot.",
    1000
};

class DummyPlugin final : public BasePlugin {
public:
    explicit DummyPlugin(const char* configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8)
        , log_(64) {}

protected:
    bool onInit(QString& err) override {
        // Read config with defaults
        const QJsonObject& c = config();

        nameTag_ = jsonString(c, "nameTag", "default");

        const int maxLogEntries = jsonInt(c, "maxLogEntries", 64);
        log_.setCapacity((size_t)qMax(1, maxLogEntries));

        emitLogEvery_ = jsonInt(c, "emitLogEveryNTicks", 3);
        if (emitLogEvery_ < 1) emitLogEvery_ = 1;

        const double noise = jsonDouble(c, "noise", 0.25);
        const int seedInt = jsonInt(c, "seed", 1337);
        sampler_.configure(noise, (uint64_t)(seedInt < 0 ? -seedInt : seedInt));

        if (!sampler_.init(err)) {
            log_.push("error", "sampler.init failed", QJsonObject{{"err", err}});
            return false;
        }

        // Write first log
        log_.push("info", "dummy initialized",
                  QJsonObject{
                      {"nameTag", nameTag_},
                      {"intervalMs", (int)intervalMs()},
                      {"noise", sampler_.noise()},
                      {"seed", (double)sampler_.seed()},
                  });

        inited_ = true;
        return true;
    }

    void onStop() override {
        // Nothing external, but still log it
        log_.push("info", "dummy stopped", QJsonObject{{"ticks", (int)tickCount_}});
    }

    QJsonObject onTick() override {
        tickCount_++;

        const Sample s = sampler_.sample();

        // Update running stats
        lastValue_ = s.value;
        if (tickCount_ == 1) {
            minValue_ = maxValue_ = s.value;
        } else {
            if (s.value < minValue_) minValue_ = s.value;
            if (s.value > maxValue_) maxValue_ = s.value;
        }

        // Periodic log emission
        if ((tickCount_ % (uint64_t)emitLogEvery_) == 0) {
            log_.push("debug", "tick",
                      QJsonObject{
                          {"seq", (double)s.seq},
                          {"value", s.value},
                          {"jitter", s.jitter},
                          {"phase", s.phase}
                      });
        }

        // Compose snapshot JSON
        QJsonObject snap;
        snap.insert("ok", true);
        snap.insert("id", QString::fromUtf8(INFO.id));
        snap.insert("name", QString::fromUtf8(INFO.name));
        snap.insert("nameTag", nameTag_);
        snap.insert("intervalMs", (int)intervalMs());
        snap.insert("tick", (double)tickCount_);
        snap.insert("seq", (double)s.seq);
        snap.insert("value", s.value);
        snap.insert("jitter", s.jitter);
        snap.insert("phase", s.phase);

        // basic min/max since start
        snap.insert("min", minValue_);
        snap.insert("max", maxValue_);

        // lightweight log stats (not full dump each tick)
        snap.insert("log", log_.stats());

        // some "health-ish" fields
        snap.insert("meta", QJsonObject{
            {"inited", inited_},
            {"seed", (double)sampler_.seed()},
            {"noise", sampler_.noise()}
        });

        return snap;
    }

    QJsonObject onRequest(const QJsonObject& req) override {
        // Expected: { "cmd": "...", ... }
        const QString cmd = req.value("cmd").toString();

        if (cmd.isEmpty()) {
            return QJsonObject{{"ok", false}, {"error", "missing_cmd"}};
        }

        // ---- Generic commands ----
        if (cmd == "ping") {
            return QJsonObject{
                {"ok", true},
                {"pong", true},
                {"ts", isoUtcNow()},
                {"tick", (double)tickCount_}
            };
        }

        if (cmd == "get_state") {
            // BasePlugin state is private; we expose what we can
            return QJsonObject{
                {"ok", true},
                {"intervalMs", (int)intervalMs()},
                {"tick", (double)tickCount_},
                {"lastValue", lastValue_},
                {"min", minValue_},
                {"max", maxValue_},
                {"log", log_.stats()},
                {"nameTag", nameTag_}
            };
        }

        // ---- Interval control (BasePlugin API) ----
        if (cmd == "set_interval") {
            const int ms = req.value("intervalMs").toInt(-1);
            if (ms <= 0) return QJsonObject{{"ok", false}, {"error", "bad_interval"}};
            setIntervalMs((uint32_t)ms);
            log_.push("info", "interval changed", QJsonObject{{"intervalMs", ms}});
            return QJsonObject{{"ok", true}, {"intervalMs", ms}};
        }

        // ---- Pause/Resume via BasePlugin’s public methods ----
        if (cmd == "pause") {
            const int32_t rc = pause();
            log_.push(rc == WA_OK ? "info" : "error",
                      "pause called",
                      QJsonObject{{"rc", rc}});
            return QJsonObject{{"ok", rc == WA_OK}, {"rc", rc}};
        }

        if (cmd == "resume") {
            const int32_t rc = resume();
            log_.push(rc == WA_OK ? "info" : "error",
                      "resume called",
                      QJsonObject{{"rc", rc}});
            return QJsonObject{{"ok", rc == WA_OK}, {"rc", rc}};
        }

        // ---- Sampler control ----
        if (cmd == "set_noise") {
            const double n = req.value("noise").toDouble(-1.0);
            if (n < 0.0) return QJsonObject{{"ok", false}, {"error", "bad_noise"}};
            sampler_.configure(n, sampler_.seed());
            log_.push("info", "noise changed", QJsonObject{{"noise", n}});
            return QJsonObject{{"ok", true}, {"noise", n}};
        }

        if (cmd == "reseed") {
            const int seedInt = req.value("seed").toInt(0);
            if (seedInt == 0) return QJsonObject{{"ok", false}, {"error", "bad_seed"}};
            const uint64_t s = (uint64_t)(seedInt < 0 ? -seedInt : seedInt);
            sampler_.reseed(s);
            log_.push("info", "sampler reseeded", QJsonObject{{"seed", (double)s}});
            return QJsonObject{{"ok", true}, {"seed", (double)s}};
        }

        // ---- Logging commands ----
        if (cmd == "log_dump") {
            return QJsonObject{
                {"ok", true},
                {"log", log_.dump()}
            };
        }

        if (cmd == "log_clear") {
            log_.clear();
            log_.push("info", "log cleared");
            return QJsonObject{{"ok", true}};
        }

        // ---- Snapshot on demand (call onTick-like without advancing thread timing) ----
        if (cmd == "snapshot_now") {
            // We do NOT call onTick() here because that advances state.
            // Instead return last known values + log dump optionally.
            const bool withLog = req.value("withLog").toBool(false);

            QJsonObject o{
                {"ok", true},
                {"ts", isoUtcNow()},
                {"tick", (double)tickCount_},
                {"lastValue", lastValue_},
                {"min", minValue_},
                {"max", maxValue_},
                {"intervalMs", (int)intervalMs()},
                {"meta", QJsonObject{
                    {"seed", (double)sampler_.seed()},
                    {"noise", sampler_.noise()},
                    {"nameTag", nameTag_}
                }}
            };
            if (withLog) o.insert("log", log_.dump());
            return o;
        }

        // ---- Unknown command ----
        log_.push("warn", "unknown cmd", QJsonObject{{"cmd", cmd}});
        return QJsonObject{{"ok", false}, {"error", "unknown_cmd"}, {"cmd", cmd}};
    }

private:
    DummySampler sampler_{};
    RingLog log_;

    QString nameTag_ = "default";

    int emitLogEvery_ = 3;

    bool inited_ = false;

    uint64_t tickCount_ = 0;
    double lastValue_ = 0.0;
    double minValue_ = 0.0;
    double maxValue_ = 0.0;
};

// ---- C ABI exports ----
WA_EXPORT const WaPluginInfo* WA_CALL wa_get_info() { return &INFO; }

WA_EXPORT void* WA_CALL wa_create(void*, const char* cfg) {
    return new DummyPlugin(cfg);
}

WA_EXPORT int32_t WA_CALL wa_init(void* h) {
    return h ? ((DummyPlugin*)h)->init() : WA_ERR_BAD_ARG;
}

WA_EXPORT int32_t WA_CALL wa_start(void* h) {
    return h ? ((DummyPlugin*)h)->start() : WA_ERR_BAD_ARG;
}

WA_EXPORT int32_t WA_CALL wa_pause(void* h) {
    return h ? ((DummyPlugin*)h)->pause() : WA_ERR_BAD_ARG;
}

WA_EXPORT int32_t WA_CALL wa_resume(void* h) {
    return h ? ((DummyPlugin*)h)->resume() : WA_ERR_BAD_ARG;
}

WA_EXPORT int32_t WA_CALL wa_stop(void* h) {
    return h ? ((DummyPlugin*)h)->stop() : WA_ERR_BAD_ARG;
}

WA_EXPORT void WA_CALL wa_destroy(void* h) {
    if (!h) return;
    auto* p = (DummyPlugin*)h;
    p->stop();
    delete p;
}

WA_EXPORT WaView WA_CALL wa_request(void* h, const char* reqJsonUtf8) {
    return h ? ((DummyPlugin*)h)->requestView(reqJsonUtf8) : WaView{nullptr, 0};
}

WA_EXPORT WaView WA_CALL wa_read(void* h) {
    return h ? ((DummyPlugin*)h)->readView() : WaView{nullptr, 0};
}
