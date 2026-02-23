#pragma once

#include <QByteArray>
#include <QString>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

namespace androidbridge {

class TcpJsonServer {
public:
    using MessageCallback = std::function<void(const QByteArray& jsonUtf8, const QString& peerIp, quint16 peerPort)>;

    TcpJsonServer();
    ~TcpJsonServer();

    bool start(quint16 port, MessageCallback cb, QString* err = nullptr);
    void stop();

private:
    void acceptLoop();
    void clientLoop(SOCKET clientSock, QString peerIp, quint16 peerPort);

    quint16 port_ = 0;
    MessageCallback cb_;

    std::atomic<bool> stop_{false};
    std::thread acceptTh_;

#ifdef _WIN32
    SOCKET listenSock_ = INVALID_SOCKET;
#endif
};

} // namespace androidbridge
