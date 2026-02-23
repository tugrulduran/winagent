#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

#include <QByteArray>
#include <QString>

// A tiny UDP listener running in its own thread (Windows winsock).
// Calls the callback on the listener thread when a datagram arrives.
class UdpListener final {
public:
    using OnDatagram = std::function<void(const QByteArray& payload,
                                         const QString& senderIp,
                                         uint16_t senderPort)>;

    UdpListener() = default;
    ~UdpListener();

    UdpListener(const UdpListener&) = delete;
    UdpListener& operator=(const UdpListener&) = delete;

    bool start(uint16_t port, OnDatagram cb, QString& err);
    void stop();

    bool isRunning() const { return running_.load(); }
    uint16_t port() const { return port_.load(); }

private:
    void threadMain();

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    std::atomic<uint16_t> port_{0};

    OnDatagram cb_;
    std::thread thread_;
};
