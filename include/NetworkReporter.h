#ifndef NETWORKREPORTER_H
#define NETWORKREPORTER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <functional>

#include "modules/AudezeMonitor.h"
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "modules/AudioMonitor.h"
#include "modules/MediaMonitor.h"

/*
 * NetworkReporter
 * --------------
 * Sends monitor data to a UDP server as a compact binary packet.
 *
 * Two background threads:
 * 1) worker thread: periodically sends current monitor snapshot to server (port 5000)
 * 2) listener thread: listens for incoming UDP commands (port 5001)
 *
 * Design rule:
 * - Monitors own their data and provide it via getData() copies.
 * - Reporter only reads data and never blocks monitor threads for long.
 */

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
    uint8_t audezeBattery;
};
#pragma pack(pop)

// Module Ids
enum ModuleId : uint8_t {
    MODULE_SYSTEM = 0xA1,
    MODULE_CPU = 0xA2,
    MODULE_MEDIA = 0xA3,
    MODULE_AUDIO = 0xA4,
    MODULE_PROCESS = 0xA5
};

// Audio Module Commands
enum AudioCommand : uint8_t {
    AUDIO_CMD_TOGGLE_MUTE = 0xB1,
    AUDIO_CMD_SET_DEVICE = 0xB2,
    AUDIO_CMD_SET_APP_VOLUME = 0xB3,
};

// Media Module Commands
enum MediaCommand : uint8_t {
    MEDIA_CMD_PLAY = 0xC1,
    MEDIA_CMD_PAUSE = 0xC2,
    MEDIA_CMD_STOP = 0xC3,
    MEDIA_CMD_NEXT = 0xC4,
    MEDIA_CMD_PREV = 0xC5,
    MEDIA_CMD_JUMP = 0xC6,
    MEDIA_CMD_S_P10 = 0xC7,
    MEDIA_CMD_S_M10 = 0xC8,
    MEDIA_CMD_S_P30 = 0xC9,
    MEDIA_CMD_S_M30 = 0xD0,
};

// Standard Packet Structure (16 Byte)
#pragma pack(push, 1)
struct ControlPacket {
    uint8_t moduleId;
    uint8_t commandId;
    uint16_t subCommand;
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
};
#pragma pack(pop)

class NetworkReporter {
public:
    NetworkReporter(const std::string &ip, int port, int ms,
                    CPUMonitor *c, MemoryMonitor *r, NetworkBytes *n, AudioMonitor *a, MediaMonitor *m, AudezeMonitor *audeze);

    ~NetworkReporter();

    void start();

    void stop();

    // UI logging hook. MainWindow can attach a callback to display logs.
    using LogCallback = std::function<void(const std::string &)>;
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

    // Non-owning pointers (monitors are owned elsewhere).
    CPUMonitor *cpuPtr;
    MemoryMonitor *ramPtr;
    NetworkBytes *netPtr;
    AudioMonitor *audioPtr;
    MediaMonitor *mediaPtr;
    AudezeMonitor *audezePtr;

    void run();

    void sendData();

    void listenForCommands();

    LogCallback logCb;
    void log(const std::string &msg) { if (logCb) logCb(msg); }
};

#endif
