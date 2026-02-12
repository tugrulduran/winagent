#include "DashboardServer.h"

#include <QFile>
#include <QDir>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QMimeDatabase>
#include <QMimeType>

#include "Logger.h"

DashboardServer::DashboardServer(QObject* parent) : QTcpServer(parent) {}

void DashboardServer::start()
{
    if (isListening()) {
        return;
    }

    if (!listen(QHostAddress::AnyIPv4, 3003)) {
        Logger::error("[WEB] Listen failed!");
        return;
    }

    Logger::success("[WEB] Server started! Listening on port 3003");
    emit started();
}

void DashboardServer::stop() {
    if (!isListening())
        return;

    close();
    Logger::error("[WEB] Server stopped!");
    emit stopped();
}

void DashboardServer::incomingConnection(qintptr socketDescriptor)
{
    auto* socket = new QSslSocket(this);

    if (!socket->setSocketDescriptor(socketDescriptor)) {
        socket->deleteLater();
        return;
    }

    QFile certFile("cert.pem");
    QFile keyFile("key.pem");

    if (!certFile.open(QIODevice::ReadOnly) ||
        !keyFile.open(QIODevice::ReadOnly)) {
        Logger::error("[WEB] Cannot open cert.pem or key.pem");
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    QSslCertificate cert(&certFile, QSsl::Pem);
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);

    if (cert.isNull() || key.isNull()) {
        Logger::error("[WEB] Invalid SSL certificate or key");
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    socket->setSslConfiguration(sslConfig);

    connect(socket, &QSslSocket::encrypted,
            this, &DashboardServer::onEncrypted);

    connect(socket, &QSslSocket::sslErrors,
            this, [](const QList<QSslError>& errors) {
        for (const auto& e : errors)
                Logger::error("[WEB] SSL error");
    });

    connect(socket, &QSslSocket::disconnected,
            socket, &QObject::deleteLater);

    socket->startServerEncryption();
}

void DashboardServer::onEncrypted()
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (!socket)
        return;

    connect(socket, &QSslSocket::readyRead,
            this, &DashboardServer::onReadyRead);
}

void DashboardServer::onReadyRead()
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (!socket)
        return;

    QByteArray request = socket->readAll();
    if (request.isEmpty())
        return;

    QList<QByteArray> lines = request.split('\n');
    QList<QByteArray> parts = lines.first().split(' ');

    QString path = "/";
    if (parts.size() >= 2)
        path = QString::fromUtf8(parts[1]);

    if (path == "/")
        path = "/index.html";

    QString filePath = QDir("dashboards/default").absoluteFilePath(path.mid(1));
    QFile file(filePath);

    QByteArray response;
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(filePath);
    QString contentType = mime.isValid() ? mime.name() : "application/octet-stream";


    if (file.open(QIODevice::ReadOnly)) {
        QByteArray body = file.readAll();
        response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: " + contentType.toUtf8() + "\r\n"
            "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" +
            body;
    } else {
        QByteArray body = "404 Not Found";
        response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" +
            body;
    }

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
