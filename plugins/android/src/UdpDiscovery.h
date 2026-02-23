#pragma once

#include <QByteArray>
#include <QString>

#include <atomic>
#include <functional>
#include <thread>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

namespace androidbridge {

// Lightweight UDP listener for discovery requests.
class UdpDiscovery {
public:
    using PacketCallback = std::function<void(const QByteArray& data, const QString& fromIp, quint16 fromPort)>;

    UdpDiscovery();
    ~UdpDiscovery();

    bool start(quint16 port, PacketCallback cb, QString* err = nullptr);
    void stop();

    static bool sendTo(const QString& ip, quint16 port, const QByteArray& payload, QString* err = nullptr);

    // Best-effort: determine the local IP that would be used to reach `remoteIp`.
    static QString bestLocalIpForRemote(const QString& remoteIp);

private:
    void threadMain();

    quint16 port_ = 0;
    PacketCallback cb_;

    std::atomic<bool> stop_{false};
    std::thread th_;

#ifdef _WIN32
    SOCKET sock_ = INVALID_SOCKET;
#endif
};

} // namespace androidbridge
