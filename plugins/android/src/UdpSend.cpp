#include "UdpSend.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <cstring>

namespace udp_send {

static bool wsaStartup(QString* err) {
#ifdef _WIN32
    WSADATA wsa{};
    const int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) {
        if (err) *err = QString("WSAStartup failed: %1").arg(r);
        return false;
    }
#endif
    (void)err;
    return true;
}

static void wsaCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool sendTo(const QString& ip, uint16_t port, const QByteArray& payload, QString* err) {
#ifdef _WIN32
    if (ip.isEmpty() || port == 0) {
        if (err) *err = "Bad destination";
        return false;
    }
    if (!wsaStartup(err)) return false;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        if (err) *err = "socket() failed";
        wsaCleanup();
        return false;
    }

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(port);

    const std::string ipStd = ip.toStdString();
    if (inet_pton(AF_INET, ipStd.c_str(), &to.sin_addr) != 1) {
        if (err) *err = "inet_pton() failed";
        closesocket(s);
        wsaCleanup();
        return false;
    }

    const int n = sendto(s,
                         payload.constData(),
                         static_cast<int>(payload.size()),
                         0,
                         reinterpret_cast<sockaddr*>(&to),
                         sizeof(to));

    closesocket(s);
    wsaCleanup();

    if (n != payload.size()) {
        if (err) *err = "sendto() failed";
        return false;
    }

    return true;
#else
    (void)ip; (void)port; (void)payload; (void)err;
    return false;
#endif
}

QString bestLocalIpForRemote(const QString& remoteIp) {
#ifdef _WIN32
    QString err;
    if (!wsaStartup(&err)) return QString();

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        wsaCleanup();
        return QString();
    }

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(9); // discard

    const std::string rip = remoteIp.toStdString();
    if (inet_pton(AF_INET, rip.c_str(), &remote.sin_addr) != 1) {
        closesocket(s);
        wsaCleanup();
        return QString();
    }

    // "Connect" a UDP socket so Windows selects the outbound interface.
    connect(s, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));

    sockaddr_in local{};
    int len = sizeof(local);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) != 0) {
        closesocket(s);
        wsaCleanup();
        return QString();
    }

    char buf[INET_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
        closesocket(s);
        wsaCleanup();
        return QString();
    }

    closesocket(s);
    wsaCleanup();

    const QString lip = QString::fromUtf8(buf);
    if (lip == "0.0.0.0" || lip.isEmpty()) return QString();
    return lip;
#else
    (void)remoteIp;
    return QString();
#endif
}

} // namespace udp_send
