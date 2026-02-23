#include "TcpJsonServer.h"

#include <QString>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace androidbridge {

static bool ensureWsa(QString* err) {
#ifdef _WIN32
    static std::atomic<bool> inited{false};
    static std::atomic<bool> ok{false};
    if (inited.load()) return ok.load();

    WSADATA wsa{};
    const int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    inited.store(true);
    ok.store(r == 0);
    if (r != 0 && err) {
        *err = QString("WSAStartup failed: %1").arg(r);
    }
    return r == 0;
#else
    (void)err;
    return true;
#endif
}

TcpJsonServer::TcpJsonServer() = default;

TcpJsonServer::~TcpJsonServer() {
    stop();
}

bool TcpJsonServer::start(quint16 port, MessageCallback cb, QString* err) {
    stop();

    if (!ensureWsa(err)) return false;

    port_ = port;
    cb_ = std::move(cb);
    stop_.store(false);

#ifdef _WIN32
    listenSock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCKET) {
        if (err) *err = "TCP socket() failed";
        return false;
    }

    BOOL reuse = TRUE;
    ::setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) != 0) {
        if (err) *err = QString("TCP bind(%1) failed").arg(port_);
        ::closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        return false;
    }

    if (::listen(listenSock_, 4) != 0) {
        if (err) *err = "TCP listen() failed";
        ::closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        return false;
    }

    u_long nonblock = 1;
    ::ioctlsocket(listenSock_, FIONBIO, &nonblock);
#endif

    acceptTh_ = std::thread([this]() { acceptLoop(); });
    return true;
}

void TcpJsonServer::stop() {
    stop_.store(true);
#ifdef _WIN32
    if (listenSock_ != INVALID_SOCKET) {
        ::closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
#endif
    if (acceptTh_.joinable()) acceptTh_.join();
}

void TcpJsonServer::acceptLoop() {
#ifdef _WIN32
    while (!stop_.load()) {
        if (listenSock_ == INVALID_SOCKET) break;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenSock_, &rfds);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 250000;

        const int r = ::select(0, &rfds, nullptr, nullptr, &tv);
        if (r <= 0) continue;
        if (!FD_ISSET(listenSock_, &rfds)) continue;

        sockaddr_in peer{};
        int peerLen = sizeof(peer);
        SOCKET c = ::accept(listenSock_, (sockaddr*)&peer, &peerLen);
        if (c == INVALID_SOCKET) continue;

        char ipStr[INET_ADDRSTRLEN] = {0};
        ::inet_ntop(AF_INET, &peer.sin_addr, ipStr, (socklen_t)sizeof(ipStr));
        const quint16 peerPort = ntohs(peer.sin_port);
        const QString peerIp = QString::fromUtf8(ipStr);

        std::thread([this, c, peerIp, peerPort]() {
            clientLoop(c, peerIp, peerPort);
        }).detach();
    }
#endif
}

static void trimCR(QString& s) {
    while (s.endsWith('\r')) s.chop(1);
}

void TcpJsonServer::clientLoop(SOCKET clientSock, QString peerIp, quint16 peerPort) {
#ifdef _WIN32
    // Blocking recv is fine here; detach thread per client.
    // Still respect stop_ by using select with small timeout.
    QByteArray buf;
    buf.reserve(16 * 1024);

    char tmp[4096];
    bool sawAny = false;

    for (;;) {
        if (stop_.load()) break;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(clientSock, &rfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 250000;
        const int r = ::select(0, &rfds, nullptr, nullptr, &tv);
        if (r <= 0) continue;
        if (!FD_ISSET(clientSock, &rfds)) continue;

        const int n = ::recv(clientSock, tmp, (int)sizeof(tmp), 0);
        if (n == 0) {
            break; // closed
        }
        if (n < 0) {
            break;
        }
        sawAny = true;
        buf.append(tmp, n);

        // Process newline-delimited JSON messages.
        for (;;) {
            int idx = buf.indexOf('\n');
            if (idx < 0) break;
            QByteArray line = buf.left(idx);
            buf.remove(0, idx + 1);
            line = line.trimmed();
            if (line.isEmpty()) continue;

            if (cb_) cb_(line, peerIp, peerPort);
        }

        // If the app sends a single JSON and then closes without '\n', we'll parse on close.
    }

    // Parse remaining buffer as one JSON message (best effort).
    QByteArray tail = buf.trimmed();
    if (!tail.isEmpty() && cb_) {
        cb_(tail, peerIp, peerPort);
    }

    ::closesocket(clientSock);
#else
    (void)clientSock; (void)peerIp; (void)peerPort;
#endif
}

} // namespace androidbridge
