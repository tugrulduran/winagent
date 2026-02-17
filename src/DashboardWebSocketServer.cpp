#include "DashboardWebSocketServer.h"

#include <QFile>
#include <QSslKey>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QMetaObject>
#include <QTimer>

#include "Logger.h"
#include "PluginManager.h"

DashboardWebSocketServer::DashboardWebSocketServer(
    PluginManager *plugins,
    QObject *parent) : QWebSocketServer(QStringLiteral("Dashboard WS Server"), QWebSocketServer::SecureMode, parent),
                       m_plugins(plugins),
                       m_broadcastTimer(new QTimer(this)) {
    m_broadcastTimer->setTimerType(Qt::CoarseTimer);
    m_broadcastTimer->setInterval(1000);
    connect(m_broadcastTimer, &QTimer::timeout, this, &DashboardWebSocketServer::broadcastTick);

    connect(this, &QWebSocketServer::newConnection, this, &DashboardWebSocketServer::onNewConnection);
}

DashboardWebSocketServer::~DashboardWebSocketServer() { stop(); }

void DashboardWebSocketServer::start() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
        return;
    }

    if (isListening()) return;

    QFile certFile("certs/cert.pem");
    QFile keyFile("certs/key.pem");

    if (!certFile.open(QIODevice::ReadOnly) ||
        !keyFile.open(QIODevice::ReadOnly)) {
        Logger::error("[WS] Cannot read SSL cert/key!");
        return;
    }

    QSslCertificate cert(&certFile, QSsl::Pem);
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);

    QSslConfiguration sslConfig;
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    setSslConfiguration(sslConfig);

    if (!listen(QHostAddress::Any, 3004)) {
        Logger::error("[WS] Listen failed!");
        return;
    }

    Logger::success("[WS] Server started! Listening on port 3004");
    emit started();

    m_broadcastTimer->start();

    Logger::success("[WS] Broadcast started with interval 1000ms.");
}

void DashboardWebSocketServer::stop() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "stop", Qt::QueuedConnection);
        return;
    }

    if (m_broadcastTimer) m_broadcastTimer->stop();
    if (isListening()) close();

    for (auto *client: std::as_const(m_clients)) {
        if (!client) continue;
        client->close();
        client->deleteLater();
    }
    m_clients.clear();

    Logger::error("[WS] Server stopped!");
    emit stopped();
}

void DashboardWebSocketServer::onNewConnection() {
    QWebSocket *socket = nextPendingConnection();

    Logger::debug("[WS] New connection from " + socket->peerAddress().toString());

    connect(socket, &QWebSocket::textMessageReceived, this, &DashboardWebSocketServer::onTextMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &DashboardWebSocketServer::onSocketDisconnected);

    m_clients.insert(socket);
    emit clientConnected();
}

void DashboardWebSocketServer::onSocketDisconnected() {
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    if (!socket) return;

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

    handleModuleRequest(root);
}

void DashboardWebSocketServer::broadcastTick() {
    broadcastJson();
}

void DashboardWebSocketServer::broadcastJson() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "broadcastJson", Qt::QueuedConnection);
        return;
    }

    if (m_clients.isEmpty())
        return;

    //@formatter:off
    QJsonObject root;       // Json root
    QJsonObject payload;    // Payload root
    QJsonObject timestamp;  // ├─ Current time  ⏱️
    QJsonObject modules;    // └─ modules       🧩 (data from plugins)
    //@formatter:on

    payload["timestamp"] = QDateTime::currentSecsSinceEpoch();
    if (m_plugins) {
        modules = m_plugins->readAll();
        payload["modules"] = modules;
    }

    root["event"] = "update";
    root["payload"] = payload;

    QJsonDocument doc(root);
    const QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    const auto clients = m_clients;
    for (QWebSocket *socket: clients) {
        if (!socket) continue;
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(json);
        }
    }
}

void DashboardWebSocketServer::sendResponse(const QJsonObject &data) {
    if (QThread::currentThread() != thread()) { return; }
    if (m_clients.isEmpty()) { return; }

    QJsonDocument doc(data);
    const QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    const auto clients = m_clients; // snapshot
    for (QWebSocket *socket: clients) {
        if (!socket) continue;
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->sendTextMessage(json);
        }
    }
}

void DashboardWebSocketServer::handleModuleRequest(const QJsonObject &data) {
    if (!m_plugins) return;
    const QString module = data.value("module").toString();
    const QJsonObject payload = data.value("payload").toObject();
    if (module.isEmpty()) return;

    const QJsonObject res = m_plugins->request(module, payload);
    // Optional: broadcast immediately after a module command.
    if (!res.isEmpty()) {
        Logger::info("[PLUGIN] " + module + " request ok");
        sendResponse(res);
        QTimer::singleShot(100, this, [&] { broadcastJson(); });
    }
}
