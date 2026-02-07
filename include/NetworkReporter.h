#ifndef NETWORKREPORTER_H
#define NETWORKREPORTER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

// Modül başlıkları
#include <functional>

#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "modules/AudioMonitor.h"
#include "modules/MediaMonitor.h"

#pragma pack(push, 1)
struct InterfacePacket {
    float inKB;
    float outKB;
};

struct AudioPacket {
    uint32_t pid;
    wchar_t name[25];
    float volume;
};

struct AudioDevicePacket {
    uint8_t index;
    wchar_t name[32];
    uint8_t isDefault;
};

struct FullMonitorPacketExtended {
    uint32_t id;
    float cpu;
    float ramUsed;
    uint8_t ramPerc;
    uint8_t netCount;
    uint8_t audioCount;
    uint8_t hasMedia;
    uint8_t deviceCount;
};
#pragma pack(pop)

class NetworkReporter {
public:
    NetworkReporter(const std::string& ip, int port, int ms,
                   CPUMonitor* c, MemoryMonitor* r, NetworkBytes* n, AudioMonitor* a, MediaMonitor* m);
    ~NetworkReporter();
    void start();
    void stop();
    // Log mesajlarını MainWindow'a göndermek için callback tanımı
    using LogCallback = std::function<void(const std::string&)>;
    void setLogCallback(LogCallback cb) { logCb = cb; }
private:
    SOCKET sock;
    SOCKET cmdSock;
    sockaddr_in serverAddr;
    std::thread worker;
    std::thread listenerThread;
    std::atomic<bool> active{false};
    std::atomic<bool> listenerActive{false};
    int intervalMs;
    uint32_t currentId = 0;

    // Pointerlarımızı tutuyoruz
    CPUMonitor* cpuPtr;
    MemoryMonitor* ramPtr;
    NetworkBytes* netPtr;
    AudioMonitor* audioPtr;
    MediaMonitor* mediaPtr;

    void run();
    void sendData();
    void listenForCommands();

    LogCallback logCb;
    void log(const std::string& msg) { if(logCb) logCb(msg); }
};

#endif