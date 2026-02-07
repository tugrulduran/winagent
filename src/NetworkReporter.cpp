#include "NetworkReporter.h"
#include <cstring>
#include <iostream>
#include "modules/AudioDeviceSwitcher.h"

NetworkReporter::NetworkReporter(const std::string &ip, int port, int ms,
                                 CPUMonitor *c, MemoryMonitor *r, NetworkBytes *n, AudioMonitor *a, MediaMonitor *m)
    : intervalMs(ms), cpuPtr(c), ramPtr(r), netPtr(n), audioPtr(a), mediaPtr(m),
      sock(INVALID_SOCKET), cmdSock(INVALID_SOCKET) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
}

NetworkReporter::~NetworkReporter() {
    stop();
}

void NetworkReporter::start() {
    active = true;
    worker = std::thread(&NetworkReporter::run, this);
    listenerActive = true;
    listenerThread = std::thread(&NetworkReporter::listenForCommands, this);
}

void NetworkReporter::stop() {
    active = false;
    listenerActive = false;
    if (cmdSock != INVALID_SOCKET) closesocket(cmdSock);
    if (sock != INVALID_SOCKET) closesocket(sock);
    if (worker.joinable()) worker.join();
    if (listenerThread.joinable()) listenerThread.join();
    WSACleanup();
}

void NetworkReporter::run() {
    while (active) {
        sendData();
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

void NetworkReporter::listenForCommands() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    cmdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(5001);

    if (bind(cmdSock, (sockaddr *)&localAddr, sizeof(localAddr)) != SOCKET_ERROR) {
        while (listenerActive) {
            char cmdBuffer[8];
            int bytesReceived = recvfrom(cmdSock, cmdBuffer, 8, 0, NULL, NULL);
            if (!listenerActive || bytesReceived == SOCKET_ERROR) break;

            if (bytesReceived == 8) {
                uint32_t targetPid;
                float dataVal;
                memcpy(&targetPid, cmdBuffer, 4);
                memcpy(&dataVal, cmdBuffer + 4, 4);

                if (targetPid == 0xFFFFFFFF) {
                    std::string cmdName;
                    int cmdId = (int)dataVal;
                    if (cmdId == 1) cmdName = "PLAY/PAUSE";
                    else if (cmdId == 2) cmdName = "NEXT";
                    else if (cmdId == 3) cmdName = "PREV";

                    log("[REPORTER] Medya Komutu Alındı: " + cmdName);
                    if (mediaPtr) mediaPtr->sendMediaCommand(cmdId);
                }
                else if (targetPid == 0xFFFFFFFE) {
                    log("[REPORTER] Ses Aygıtı Değiştirme: Index " + std::to_string((int)dataVal));
                    AudioDeviceSwitcher::setDefaultByIndex((int)dataVal);
                }
                else {
                    log("[REPORTER] Ses Ayarı - PID: " + std::to_string(targetPid) + " Seviye: %" + std::to_string((int)(dataVal * 100)));
                    if (audioPtr) audioPtr->setVolumeByPID(targetPid, dataVal);
                }
            } else {
                log("[REPORTER] Geçersiz paket boyutu: " + std::to_string(bytesReceived) + " byte");
            }
        }
    }
    CoUninitialize();
}

void NetworkReporter::sendData() {
    // 1. Yeni getData() disiplini ile ham verileri topla
    CPUData cpuData = cpuPtr->getData();
    MemoryData ramData = ramPtr->getData();
    auto netIfaces = netPtr->getData();
    AudioSnapshot audioSnap = audioPtr->getData();
    MediaPacket mediaData = mediaPtr->getData();
    auto audioDevices = AudioDeviceSwitcher::listDevices();

    // 2. Alt Paketleri Hazırla
    std::vector<InterfacePacket> netPackets;
    for (const auto &f : netIfaces)
        netPackets.push_back({(float)f.speedInKB, (float)f.speedOutKB});

    std::vector<AudioPacket> audioPackets;
    for (const auto &app : audioSnap.apps) {
        AudioPacket p;
        p.pid = app.pid;
        p.volume = app.volume;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy(p.name, app.name.c_str(), 24);
        audioPackets.push_back(p);
    }

    std::vector<AudioDevicePacket> devicePackets;
    for (const auto &d : audioDevices) {
        AudioDevicePacket p;
        p.index = (uint8_t)d.index;
        p.isDefault = d.isDefault ? 1 : 0;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy_s(p.name, _countof(p.name), d.name.c_str(), _TRUNCATE);
        devicePackets.push_back(p);
    }

    // 3. Header'ı Doldur
    FullMonitorPacketExtended header;
    header.id = ++currentId;
    header.cpu = (float)cpuData.load;
    header.ramUsed = (float)ramData.usedGB;
    header.ramPerc = (uint8_t)ramData.usagePercentage;
    header.netCount = (uint8_t)netPackets.size();
    header.audioCount = (uint8_t)audioPackets.size();
    header.hasMedia = (wcslen(mediaData.title) > 0) ? 1 : 0;
    header.deviceCount = (uint8_t)devicePackets.size();

    // 4. Dinamik Buffer Oluştur ve Kopyala
    size_t netSize = netPackets.size() * sizeof(InterfacePacket);
    size_t audioSize = audioPackets.size() * sizeof(AudioPacket);
    size_t mediaSize = header.hasMedia ? sizeof(MediaPacket) : 0;
    size_t devicesSize = devicePackets.size() * sizeof(AudioDevicePacket);

    size_t totalSize = sizeof(header) + netSize + audioSize + mediaSize + devicesSize;
    std::vector<char> buffer(totalSize);
    size_t offset = 0;

    memcpy(buffer.data() + offset, &header, sizeof(header)); offset += sizeof(header);
    if (!netPackets.empty()) { memcpy(buffer.data() + offset, netPackets.data(), netSize); offset += netSize; }
    if (!audioPackets.empty()) { memcpy(buffer.data() + offset, audioPackets.data(), audioSize); offset += audioSize; }
    if (header.hasMedia) { memcpy(buffer.data() + offset, &mediaData, sizeof(MediaPacket)); offset += mediaSize; }
    if (!devicePackets.empty()) { memcpy(buffer.data() + offset, devicePackets.data(), devicesSize); offset += devicesSize; }

    // 5. Gönder
    sendto(sock, buffer.data(), (int)buffer.size(), 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
}