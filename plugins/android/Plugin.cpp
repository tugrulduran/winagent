#include "BasePlugin.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QSysInfo>

// Optional Qt UI
#include <QWidget>
#include "src/AndroidUi.h"

#include "src/UdpDiscovery.h"
#include "src/TcpJsonServer.h"

using namespace androidbridge;

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "android",
    "Android",
    "Android phone bridge (calls + notifications).",
    1000
};

static int64_t nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}

static QJsonObject defaultPayload() {
    QJsonObject o;
    o.insert("notifications", QJsonArray{});
    o.insert("missedCalls", QJsonArray{});
    o.insert("phoneRinging", false);
    return o;
}

class AndroidPlugin final : public BasePlugin {
public:
    explicit AndroidPlugin(void *hostCtx, const char *configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8),
          hostApi_(static_cast<WaHostApi *>(hostCtx)),
          cfgUtf8_(configJsonUtf8 ? QString::fromUtf8(configJsonUtf8) : QString("{}")) {
    }

    WaHostApi *hostApi() const { return hostApi_; }

protected:
    bool onInit(QString &err) override {
        // Parse config (interval is handled by BasePlugin, but we also read our own keys)
        QJsonObject cfgObj; {
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson(cfgUtf8_.toUtf8(), &perr);
            if (perr.error == QJsonParseError::NoError && doc.isObject()) {
                cfgObj = doc.object();
            }
        }

        discoveryPort_ = (quint16) cfgObj.value("discoveryPort").toInt(45151);
        tcpPort_ = (quint16) cfgObj.value("port").toInt(45152);
        connectedTimeoutMs_ = cfgObj.value("connectedTimeoutMs").toInt(30000);
        token_ = cfgObj.value("token").toString();

        // Init default payload
        {
            QMutexLocker lock(&mu_);
            lastPayload_ = defaultPayload();
            lastEventMs_ = 0;
            lastPeer_.clear();
        }

        // Start discovery UDP listener.
        QString derr;
        if (!discovery_.start(discoveryPort_, [this](const QByteArray &data, const QString &fromIp, quint16 fromPort) {
            handleDiscoveryPacket(data, fromIp, fromPort);
        }, &derr)) {
            err = derr;
            return false;
        }

        // Start TCP server for event packets.
        QString terr;
        if (!tcp_.start(tcpPort_, [this](const QByteArray &jsonUtf8, const QString &peerIp, quint16 peerPort) {
            handleEventPacket(jsonUtf8, peerIp, peerPort);
        }, &terr)) {
            err = terr;
            discovery_.stop();
            return false;
        }

        return true;
    }

    void onStop() override {
        tcp_.stop();
        discovery_.stop();
    }

    QJsonObject onTick() override {
        QJsonObject snap;
        int64_t lastMs = 0;
        QString peer; {
            QMutexLocker lock(&mu_);
            snap = lastPayload_;
            lastMs = lastEventMs_;
            peer = lastPeer_;
        }

        const int64_t n = nowMs();
        const bool connected = (lastMs > 0) && ((n - lastMs) <= connectedTimeoutMs_);

        snap.insert("ok", true);
        snap.insert("connected", connected);
        snap.insert("lastEventMs", (qint64) lastMs);
        snap.insert("peer", peer);
        snap.insert("discoveryPort", (int) discoveryPort_);
        snap.insert("port", (int) tcpPort_);

        return snap;
    }

    QJsonObject onRequest(const QJsonObject &req) override {
        Q_UNUSED(req);
        QJsonObject r;
        r.insert("ok", true);
        r.insert("note", "android plugin currently has no commands");
        return r;
    }

private:
    QJsonObject process(const QJsonObject &in) {
        // Hook point for future aggregation/statistics.
        // For now: normalize keys and return.
        QJsonObject out = defaultPayload();

        if (in.contains("notifications") && in.value("notifications").isArray()) {
            out.insert("notifications", in.value("notifications").toArray());
        }
        if (in.contains("missedCalls") && in.value("missedCalls").isArray()) {
            out.insert("missedCalls", in.value("missedCalls").toArray());
        }
        if (in.contains("phoneRinging")) {
            bool ringing = in.value("phoneRinging").toBool(false);
            out.insert("phoneRinging", ringing);
            if (ringing) {
                if (in.contains("callFrom")) {
                    out.insert("callFrom", in.value("callFrom").toString());
                }
            }
        }

        return out;
    }

    void handleEventPacket(const QByteArray &jsonUtf8, const QString &peerIp, quint16 peerPort) {
        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(jsonUtf8, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            return;
        }

        QJsonObject obj = doc.object();

        if (!token_.isEmpty()) {
            const QString tok = obj.value("token").toString();
            if (tok != token_) {
                return;
            }
        }

        const QJsonObject processed = process(obj);

        QMutexLocker lock(&mu_);
        lastPayload_ = processed;
        lastEventMs_ = nowMs();
        lastPeer_ = QString("%1:%2").arg(peerIp).arg(peerPort);
    }

    void handleDiscoveryPacket(const QByteArray &data, const QString &fromIp, quint16 fromPort) {
        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            return;
        }

        const QJsonObject obj = doc.object();
        const QString t = obj.value("t").toString();
        const int v = obj.value("v").toInt();
        if (t != "discover_req" || v != 1) {
            return;
        }

        QJsonObject res;
        res.insert("t", "discover_res");
        res.insert("v", 1);

        const QString localIp = UdpDiscovery::bestLocalIpForRemote(fromIp);
        res.insert("ip", localIp);
        res.insert("port", (int) tcpPort_);
        res.insert("proto", "tcp");
        res.insert("pcName", QSysInfo::machineHostName());
        if (!token_.isEmpty()) {
            res.insert("token", token_);
        }

        const QJsonDocument outDoc(res);
        const QByteArray outBytes = outDoc.toJson(QJsonDocument::Compact);

        QString sendErr;
        UdpDiscovery::sendTo(fromIp, fromPort, outBytes, &sendErr);
    }

private:
    WaHostApi *hostApi_ = nullptr;
    QString cfgUtf8_;

    quint16 discoveryPort_ = 45151;
    quint16 tcpPort_ = 45152;
    int connectedTimeoutMs_ = 30000;
    QString token_;

    UdpDiscovery discovery_;
    TcpJsonServer tcp_;

    QMutex mu_;
    QJsonObject lastPayload_;
    int64_t lastEventMs_ = 0;
    QString lastPeer_;
};

// ---- C ABI exports ----
// @formatter:off
WA_EXPORT const WaPluginInfo * WA_CALL  wa_get_info()                 { return &INFO; }
WA_EXPORT void * WA_CALL      wa_create(void *hostCtx, const char *cfg) { return new AndroidPlugin(hostCtx, cfg); }
WA_EXPORT int32_t WA_CALL     wa_init(void *h)                        { return h ? ((AndroidPlugin *) h)->init()     : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL     wa_start(void *h)                       { return h ? ((AndroidPlugin *) h)->start()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL     wa_pause(void *h)                       { return h ? ((AndroidPlugin *) h)->pause()    : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL     wa_resume(void *h)                      { return h ? ((AndroidPlugin *) h)->resume()   : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL     wa_stop(void *h)                        { return h ? ((AndroidPlugin *) h)->stop()     : WA_ERR_BAD_ARG; }
WA_EXPORT void WA_CALL        wa_destroy(void *h) {
    if (!h) return;
    auto *p = (AndroidPlugin *) h;
    p->stop();
    delete p;
}
WA_EXPORT WaView WA_CALL      wa_request(void *h, const char *reqJsonUtf8) {
    return h
        ? ((AndroidPlugin *) h)->requestView(reqJsonUtf8)
        : WaView{nullptr, 0};
}
WA_EXPORT WaView WA_CALL      wa_read(void *h) {
    return h
        ? ((AndroidPlugin *) h)->readView()
        : WaView{nullptr, 0};
}
WA_EXPORT uint64_t WA_CALL    wa_get_tick_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->tickCount() : 0;
}

WA_EXPORT uint64_t WA_CALL    wa_get_read_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->readCount() : 0;
}

WA_EXPORT uint64_t WA_CALL    wa_get_request_count(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->requestCount() : 0;
}

WA_EXPORT int64_t WA_CALL     wa_get_last_tick_ms(void* h) {
    auto* p = static_cast<BasePlugin*>(h);
    return p ? p->lastTickMs() : 0;
}

WA_EXPORT QWidget* WA_CALL    wa_create_widget(void* pluginHandle, QWidget* parent) {
    auto* p = static_cast<AndroidPlugin*>(pluginHandle);
    if (!p) return nullptr;
    return new AndroidUi(p->hostApi(), parent);
}
// @formatter:on
