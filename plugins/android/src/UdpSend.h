#pragma once

#include <cstdint>
#include <QString>
#include <QByteArray>

// Small UDP helper for unicast sends + best-effort local IP discovery.
namespace udp_send {

// Sends a UDP datagram to (ip:port). Returns true on success.
bool sendTo(const QString& ip, uint16_t port, const QByteArray& payload, QString* err = nullptr);

// Best-effort: find the local IPv4 address that would be used to reach remoteIp.
// Returns empty string if not available.
QString bestLocalIpForRemote(const QString& remoteIp);

} // namespace udp_send
