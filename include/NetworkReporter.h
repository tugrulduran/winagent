#ifndef NETWORKREPORTER_H
#define NETWORKREPORTER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "modules/AudioMonitor.h"
#include "modules/MediaMonitor.h"

// UDP üzerinden gönderilecek veri paketleri
#pragma pack(push, 1)
struct InterfacePacket {
    float inKB;
    float outKB;
};

// Ana Header Yapısı
struct FullMonitorPacketExtended {
    uint32_t id;
    float cpu;
    float ramUsed;
    uint8_t ramPerc;
    uint8_t netCount;
    uint8_t audioCount;
    uint8_t hasMedia;
    uint8_t deviceCount; // YENİ: Kaç tane hoparlör/kulaklık var?
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AudioDevicePacket {
    uint8_t index;
    wchar_t name[32];
    uint8_t isDefault;
};
#pragma pack(pop)

class NetworkReporter {
private:
    SOCKET sock;        // Veri gönderim soketi
    SOCKET cmdSock;     // Komut dinleme soketi (Kilitlenmeyi önlemek için member yapıldı)
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
    MediaMonitor* mediaPtr;

    void run();
    void sendData();
    void listenForCommands();

public:
    NetworkReporter(const std::string& ip, int port, int ms,
                   CPUMonitor* c, MemoryMonitor* r, NetworkBytes* n, AudioMonitor* a, MediaMonitor* m);
    ~NetworkReporter();
    void start();
    void stop();
};

#endif