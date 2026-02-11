#include "DashboardWebSocketServer.h"

#include <QFile>
#include <QDebug>
#include <QSslKey>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QMetaObject>
#include <QTimer>

#include "Logger.h"
#include "modules/AudioMonitor.h"
#include "modules/LauncherMonitor.h"
#include "modules/MediaMonitor.h"

DashboardWebSocketServer::DashboardWebSocketServer(
    LauncherMonitor *launcher,
    AudioMonitor *audio,
    MediaMonitor *media,
    AudioDeviceMonitor *audioDevice,
    QObject *parent)
    : QObject(parent),
      m_server(nullptr),
      m_launcher(launcher),
      m_audio(audio),
      m_audioDevice(audioDevice),
      m_media(media) {
}

DashboardWebSocketServer::~DashboardWebSocketServer() {
    stop();
}

bool DashboardWebSocketServer::start(quint16 port) {
    if (m_server)
        return true;

    m_server = new QWebSocketServer(
        QStringLiteral("Dashboard WS Server"),
        QWebSocketServer::SecureMode,
        this
    );

    QFile certFile("cert.pem");
    QFile keyFile("key.pem");

    if (!certFile.open(QIODevice::ReadOnly) ||
        !keyFile.open(QIODevice::ReadOnly)) {
        Logger::error("[WS] Cannot read SSL cert/key!");
        return false;
    }

    QSslCertificate cert(&certFile, QSsl::Pem);
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);

    QSslConfiguration sslConfig;
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    m_server->setSslConfiguration(sslConfig);

    connect(m_server, &QWebSocketServer::newConnection,
            this, &DashboardWebSocketServer::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, port)) {
        Logger::error("[WS] Listen failed!");
        return false;
    }

    Logger::success("[WS] Server started! Listening on port " + QString::number(port));

    m_broadcastTimer = new QTimer(this);
    m_broadcastTimer->setInterval(1000);

    connect(m_broadcastTimer, &QTimer::timeout,
            this, &DashboardWebSocketServer::broadcastTick);

    m_broadcastTimer->start();

    return true;
}

void DashboardWebSocketServer::stop() {
    if (!m_server)
        return;

    for (auto *socket: m_clients) {
        socket->close();
        socket->deleteLater();
    }

    m_clients.clear();

    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;

    Logger::error("[WS] Server stopped!");
}

void DashboardWebSocketServer::onNewConnection() {
    QWebSocket *socket = m_server->nextPendingConnection();

    Logger::debug("[WS] New connection from " + socket->peerAddress().toString());

    connect(socket, &QWebSocket::textMessageReceived,
            this, &DashboardWebSocketServer::onTextMessageReceived);

    connect(socket, &QWebSocket::disconnected,
            this, &DashboardWebSocketServer::onSocketDisconnected);

    m_clients.insert(socket);
    emit clientConnected();
}

void DashboardWebSocketServer::onSocketDisconnected() {
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    if (!socket)
        return;

    Logger::debug("[WS] Socket disconnected from " + socket->peerAddress().toString());

    m_clients.remove(socket);
    socket->deleteLater();

    emit clientDisconnected();
}

void DashboardWebSocketServer::onTextMessageReceived(const QString &message) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        Logger::error("[WS] Invalid JSON message: " + err.errorString());
        Logger::error("[WS] Failed message: " + message);
        return;
    }

    QJsonObject root = doc.object();

    if (!root.contains("cmd") || !root["cmd"].isString()) {
        Logger::error("[WS] Invalid JSON message: missing cmd field");
        Logger::error("[WS] Failed message: " + message);
        return;
    }

    QString cmd = root["cmd"].toString();
    QJsonObject payload = root.value("payload").toObject();

    handleEvent(cmd, payload);
}

void DashboardWebSocketServer::handleEvent(const QString &cmd, const QJsonObject &payload) {
    if (cmd == "setAudioDevice") {
        handleSetAudioDevice(payload);
    } else if (cmd == "runAction") {
        handleRunAction(payload);
    } else if (cmd == "mediaCtrl") {
        handleMediaCtrl(payload);
    } else if (cmd == "setVolume") {
        handleSetVolume(payload);
    } else {
        Logger::error("[WS] Unknown WS event: " + cmd);
    }
}

void DashboardWebSocketServer::handleSetAudioDevice(const QJsonObject &payload) {
    const uint32_t deviceIndex = payload["device"].toInt();

    m_audioDevice->setDefaultByIndex(deviceIndex);

    Logger::info("[AUDIODEVICE] Set audio device: " + QString::number(deviceIndex));
}

void DashboardWebSocketServer::handleRunAction(const QJsonObject &payload) {
    if (!m_launcher) { return; }
    const uint32_t actionId = payload["action"].toInt();
    m_launcher->executeAction(actionId);

    Logger::info("[LAUNCHER] Launcher action: " + QString::number(actionId));
}

void DashboardWebSocketServer::handleMediaCtrl(const QJsonObject &payload) {
    const uint16_t subCommand = payload["cmd"].toInteger();
    const uint32_t value = payload["value"].toInt();

    switch (subCommand) {
        case MEDIA_CMD_PLAY:
        case MEDIA_CMD_PAUSE:
            m_media->playpause();
            break;
        case MEDIA_CMD_NEXT:
            m_media->next();
            break;
        case MEDIA_CMD_PREV:
            m_media->prev();
            break;
        case MEDIA_CMD_JUMP:
            m_media->jump(value);
            break;
    }

    Logger::info("[MEDIA] Media command: " + QString::number(subCommand) + (subCommand == MEDIA_CMD_JUMP ? QString(" %1").arg(value) : QString()));
}

void DashboardWebSocketServer::handleSetVolume(const QJsonObject &payload) {
    const uint32_t pid = payload["pid"].toInteger();
    const double volume = payload["volume"].toDouble();

    m_audio->setVolumeByPID(pid, volume);

    Logger::info("[AUDIO] Set volume for pid " + QString::number(pid) + ": " + QString::number(volume));
}

void DashboardWebSocketServer::broadcastTick() {
    broadcastJson();
}

void DashboardWebSocketServer::broadcastJson() {
    if (m_clients.isEmpty())
        return;

    //@formatter:off
    QJsonObject root;       // root
    QJsonObject payload;    // payload root
    QJsonObject system;     // ├─ system
    QJsonObject cpu;        // │  ├─ cpu        🧠
    QJsonObject ram;        // │  ├─ ram        💾
    QJsonObject processes;  // │  └─ processes  ⚙️
    QJsonObject network;    // ├─ network       🌐
    QJsonObject audio;      // ├─ audio         🔊
    QJsonObject apps;       // │  ├─ apps       📦
    QJsonObject devices;    // │  ├─ devices    🎛️
    QJsonObject audeze;     // │  └─ audeze     🎧
    QJsonObject media;      // ├─ media         🎵
    QJsonObject launcher;   // └─ launcher      🚀
    //@formatter:on

    auto &data = static_cast<DashboardData &>(Dashboard::instance().data);

    cpu["load"] = static_cast<double>(data.cpu.getLoad());
    ram["total"] = static_cast<qint64>(data.memory.getTotal());
    ram["used"] = static_cast<qint64>(data.memory.getUsed());
    network["rx"] = static_cast<qint64>(data.network.getRxBytes());
    network["tx"] = static_cast<qint64>(data.network.getTxBytes());
    QJsonObject activeProcess;
    activeProcess["pid"] = static_cast<qint64>(data.process.getActiveProcess().id);
    activeProcess["name"] = QString::fromStdString(data.process.getActiveProcess().name);
    processes["active"] = activeProcess;

    system["cpu"] = cpu;
    system["ram"] = ram;
    system["network"] = network;
    system["processes"] = processes;

    auto audioApps = data.audio.apps.snapshot();
    QJsonArray appsArray;
    for (const auto &app: audioApps) {
        QJsonObject obj;
        obj["pid"] = static_cast<qint64>(app.pid);
        obj["volume"] = app.volume;
        obj["muted"] = app.muted;
        obj["name"] = QString::fromWCharArray(app.name.c_str());
        appsArray.append(obj);
    }

    auto audioDevices = data.audio.devices.snapshot();
    QJsonArray devicesArray;
    for (const auto &device: audioDevices) {
        QJsonObject obj;
        obj["index"] = static_cast<qint64>(device.index);
        obj["isDefault"] = device.isDefault;
        obj["name"] = QString::fromWCharArray(device.name.c_str());
        obj["deviceId"] = QString::fromWCharArray(device.deviceId.c_str());
        devicesArray.append(obj);
    }

    media["source"] = static_cast<qint64>(data.media.source);
    if (data.media.source != MEDIA_SOURCE_NO_MEDIA) {
        media["title"] = QString::fromWCharArray(data.media.title.c_str());
        media["duration"] = static_cast<qint64>(data.media.duration);
        media["currentTime"] = static_cast<qint64>(data.media.currentTime);
        media["isPlaying"] = static_cast<bool>(data.media.isPlaying);
    }

    audeze["battery"] = static_cast<qint64>(data.audeze.getBattery());

    audio["apps"] = appsArray;
    audio["devices"] = devicesArray;
    audio["audeze"] = audeze;

    QJsonArray launchers;
    for (const auto &a: data.launcher.getActions()) {
        QJsonObject obj;
        obj["id"] = static_cast<qint64>(a.id);
        obj["name"] = QString::fromWCharArray(a.name.c_str());
        launchers.append(obj);
    }
    launcher["actions"] = launchers;

    payload["system"] = system;
    payload["audio"] = audio;
    payload["media"] = media;
    payload["launcher"] = launcher;

    root["event"] = "update";
    root["payload"] = payload;

    QJsonDocument doc(root);
    const QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    for (QWebSocket *socket: m_clients) {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(json);
        }
    }
}
