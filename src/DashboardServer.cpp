#include "DashboardServer.h"

#include <QFile>
#include <QDir>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QMimeDatabase>
#include <QMimeType>
#include <QNetworkInterface>
#include <QHostAddress>

#include "Logger.h"

DashboardServer::DashboardServer(QObject* parent) : QTcpServer(parent) {}

static bool looksVirtual(const QNetworkInterface& iface)
{
    const QString n = (iface.humanReadableName() + " " + iface.name()).toLower();

    // yaygın sanal/tünel adaptör isimleri
    static const char* bad[] = {
        "virtualbox", "vmware", "hyper-v", "vethernet", "tunnel",
        "tap", "tun", "vpn", "wireguard", "openvpn",
        "docker", "wsl", "loopback", "npcap", "hamachi"
    };

    for (auto s : bad)
        if (n.contains(s)) return true;

    return false;
}

static int scoreIface(const QNetworkInterface& iface)
{
    int score = 0;

    // up + running ise zaten buraya geliyoruz; yine de ufak artı
    score += 10;

    // Point-to-point genelde VPN/tunnel -> düş
    if (iface.flags().testFlag(QNetworkInterface::IsPointToPoint))
        score -= 200;

    // İsimden sanal şüphesi
    if (looksVirtual(iface))
        score -= 500;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    // Qt6/Qt5.11+ : type() kullanılabiliyorsa bonus ver
    switch (iface.type()) {
        case QNetworkInterface::Wifi:     score += 300; break;
        case QNetworkInterface::Ethernet: score += 250; break;
        case QNetworkInterface::Loopback: score -= 1000; break;
        default: break;
    }
#endif

    const QString n = (iface.humanReadableName() + " " + iface.name()).toLower();
    if (n.contains("wi-fi") || n.contains("wifi") || n.contains("wlan")) score += 120;
    if (n.contains("ethernet") || n.contains("eth")) score += 80;

    return score;
}

static QString bestLocalIPv4()
{
    QString bestIp = "127.0.0.1";
    int bestScore = -1e9;

    for (const auto& iface : QNetworkInterface::allInterfaces())
    {
        const auto flags = iface.flags();
        const bool ok =
            flags.testFlag(QNetworkInterface::IsUp) &&
            flags.testFlag(QNetworkInterface::IsRunning) &&
            !flags.testFlag(QNetworkInterface::IsLoopBack);

        if (!ok) continue;

        const int ifaceScore = scoreIface(iface);
        if (ifaceScore < -400) continue; // sanal/tunnel ise direkt ele

        for (const auto& entry : iface.addressEntries())
        {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol) continue;
            if (ip.isNull() || ip.isLoopback()) continue;
            if (ip.isInSubnet(QHostAddress("169.254.0.0"), 16)) continue; // link-local

            // IP'yi de puanla: RFC1918 private adresleri hafifçe öne al
            int ipScore = ifaceScore;
            if (ip.isInSubnet(QHostAddress("10.0.0.0"), 8))       ipScore += 30;
            else if (ip.isInSubnet(QHostAddress("172.16.0.0"), 12)) ipScore += 30;
            else if (ip.isInSubnet(QHostAddress("192.168.0.0"), 16)) ipScore += 30;

            if (ipScore > bestScore) {
                bestScore = ipScore;
                bestIp = ip.toString();
            }
        }
    }

    return bestIp;
}

void DashboardServer::start()
{
    if (isListening()) {
        return;
    }

    if (!listen(QHostAddress::AnyIPv4, 3003)) {
        Logger::error("[WEB] Listen failed!");
        return;
    }

    const QString ip = bestLocalIPv4();
    const int port = 3003;
    const QString url = QStringLiteral("https://%1:%2/").arg(ip).arg(port);

    const QString qmsg = QStringLiteral("[WEB] Server started! Go to %1 from your device").arg(url);
    Logger::success(qmsg.toUtf8().constData());

    emit started(url);
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

    QFile certFile("certs/cert.pem");
    QFile keyFile("certs/key.pem");

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
