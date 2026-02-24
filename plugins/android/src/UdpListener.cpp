#include "UdpListener.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <chrono>

UdpListener::~UdpListener() {
    stop();
}

bool UdpListener::start(uint16_t port, OnDatagram cb, QString& err) {
    if (running_.load()) {
        err = "UDP listener already running";
        return false;
    }
    if (!cb) {
        err = "UDP listener callback is empty";
        return false;
    }
    if (port == 0) {
        err = "UDP port must be > 0";
        return false;
    }

    cb_ = std::move(cb);
    port_.store(port);
    stop_.store(false);
    running_.store(true);

    try {
        thread_ = std::thread([this]() { this->threadMain(); });
    } catch (...) {
        running_.store(false);
        err = "Failed to create UDP listener thread";
        return false;
    }

    return true;
}

void UdpListener::stop() {
    if (!running_.load()) return;

    stop_.store(true);

#ifdef _WIN32
    // Closing the socket from another thread is messy; we rely on recv timeout.
    // The thread uses SO_RCVTIMEO so it will wake up quickly.
#endif

    if (thread_.joinable()) {
        thread_.join();
    }

    running_.store(false);
}

void UdpListener::threadMain() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        running_.store(false);
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        running_.store(false);
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_.load());
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        running_.store(false);
        return;
    }

    // Wake periodically so stop() can join promptly.
    DWORD timeoutMs = 300;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    while (!stop_.load()) {
        char buf[64 * 1024];
        sockaddr_in from{};
        int fromLen = sizeof(from);
        const int n = recvfrom(sock, buf, static_cast<int>(sizeof(buf)), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) {
            // timeout or error
            continue;
        }

        char ipStr[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));
        const uint16_t fromPort = ntohs(from.sin_port);

        if (cb_) {
            cb_(QByteArray(buf, n), QString::fromUtf8(ipStr), fromPort);
        }
    }

    closesocket(sock);
    WSACleanup();
#else
    // Non-Windows: no-op.
    running_.store(false);
#endif
}
