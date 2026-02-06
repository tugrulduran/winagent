#ifndef NETWORKREPORTER_H
#define NETWORKREPORTER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <vector>
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "modules/AudioMonitor.h"

#pragma pack(push, 1)
struct InterfacePacket {
    float inKB;
    float outKB;
};

struct FullMonitorPacket {
    uint32_t packetId;
    float cpuUsage;
    float ramUsageGB;
    uint8_t ramPercent;
    uint8_t interfaceCount;
};

class NetworkReporter {
private:
    SOCKET sock;
    sockaddr_in serverAddr;
    std::thread worker;
    std::thread listenerThread;
    std::atomic<bool> active{false};
    std::atomic<bool> listenerActive{false};
    int intervalMs;
    uint32_t currentId = 0;

    CPUMonitor* cpuPtr;
    MemoryMonitor* ramPtr;
    NetworkBytes* netPtr;
    AudioMonitor* audioPtr;

    void run();
    void sendData();
    void listenForCommands();

public:
    NetworkReporter(const std::string& ip, int port, int ms,
                   CPUMonitor* c, MemoryMonitor* r, NetworkBytes* n, AudioMonitor* a);
    ~NetworkReporter();
    void start();
    void stop();
};

#endif