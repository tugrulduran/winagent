#include "NetworkReporter.h"
#include <cstring>
#include <iostream>
#include "modules/AudioDeviceSwitcher.h"

NetworkReporter::NetworkReporter(const std::string &ip, int port, int ms,
                                 CPUMonitor *c, MemoryMonitor *r, NetworkBytes *n, AudioMonitor *a, MediaMonitor *m, AudezeMonitor *audeze)
    : intervalMs(ms), cpuPtr(c), ramPtr(r), netPtr(n), audioPtr(a), mediaPtr(m), audezePtr(audeze),
      sock(INVALID_SOCKET), cmdSock(INVALID_SOCKET) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // UDP socket used for sending monitor packets to the server.
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
    // Stop threads first (they may be blocking on sockets).
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
    // MTA arka plan threadleri için daha stabildir
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    cmdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Soket zaman aşımı ekle (Böylece listenerActive kontrolü için recvfrom periyodik olarak uyanır)
    DWORD timeout = 1000; // 1 saniye
    setsockopt(cmdSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(5001);

    if (bind(cmdSock, (sockaddr *) &localAddr, sizeof(localAddr)) != SOCKET_ERROR) {
        while (listenerActive) {
            char cmdBuffer[16];
            int bytesReceived = recvfrom(cmdSock, cmdBuffer, sizeof(cmdBuffer), 0, NULL, NULL);

            if (!listenerActive) break;

            if (bytesReceived == 16) {
                auto* data = reinterpret_cast<const ControlPacket*>(cmdBuffer);
                char hexLog[128];
                snprintf(hexLog, sizeof(hexLog),
                    "[DATA] 0x%02X 0x%02X 0x%04X 0x%08X 0x%08X 0x%08X",
                    data->moduleId,
                    data->commandId,
                    data->subCommand,
                    data->data1,
                    data->data2,
                    data->data3);

                log(std::string(hexLog)); // Mevcut log metoduna gönder

                if (data->moduleId == MODULE_MEDIA && mediaPtr) {
                    log("[MEDIA] Received command: " + std::to_string(data->commandId) + " Data: " + std::to_string(data->data1));

                    switch(data->commandId) {
                        case MEDIA_CMD_PLAY:
                        case MEDIA_CMD_PAUSE:
                            mediaPtr->playpause();
                            break;
                        case MEDIA_CMD_NEXT:
                            mediaPtr->next();
                            break;
                        case MEDIA_CMD_PREV:
                            mediaPtr->prev();
                            break;
                        case MEDIA_CMD_JUMP:
                            mediaPtr->jump(data->data1);
                            break;
                    }
                }
                else if (data->moduleId == MODULE_AUDIO && audioPtr) {
                    log("[AUDIO] Received command: " + std::to_string(data->commandId) + " Data: " + std::to_string(data->data1));

                    switch (data->commandId) {
                        case AUDIO_CMD_TOGGLE_MUTE:
                            log("[AUDIO] Toggling mute !NOT IMPLEMENTED");
                            break;
                        case AUDIO_CMD_SET_DEVICE:
                            log("[AUDIO] Setting device to " + std::to_string(data->data1));
                            AudioDeviceSwitcher::setDefaultByIndex(data->data1);
                            break;
                        case AUDIO_CMD_SET_APP_VOLUME:
                            log("[AUDIO] Setting app (PID:" + std::to_string(data->data1) + ") volume to " + std::to_string(data->data2));
                            audioPtr->setVolumeByPID(data->data1, data->data2 / 100.0f);
                            break;
                    }
                }
            } else if (bytesReceived == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSAETIMEDOUT) { /* log error */ }
            }
        }
    }

    closesocket(cmdSock);
    CoUninitialize();
}

void NetworkReporter::sendData() {
    // Read the latest snapshot from each monitor (each getData() returns a safe copy).
    CPUData cpuData = cpuPtr->getData();
    MemoryData ramData = ramPtr->getData();
    AudezeData audezeData = audezePtr->getData();
    auto netIfaces = netPtr->getData();
    AudioSnapshot audioSnap = audioPtr->getData();
    MediaPacket mediaData = mediaPtr->getData();
    auto audioDevices = AudioDeviceSwitcher::listDevices();

    // Build variable-length sub-packets.
    std::vector<InterfacePacket> netPackets;
    for (const auto &f: netIfaces)
        netPackets.push_back({(float) f.speedInKB, (float) f.speedOutKB});

    std::vector<AudioPacket> audioPackets;
    for (const auto &app: audioSnap.apps) {
        AudioPacket p;
        p.pid = app.pid;
        p.volume = app.volume;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy(p.name, app.name.c_str(), 24);
        audioPackets.push_back(p);
    }

    std::vector<AudioDevicePacket> devicePackets;
    for (const auto &d: audioDevices) {
        AudioDevicePacket p;
        p.index = (uint8_t) d.index;
        p.isDefault = d.isDefault ? 1 : 0;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy_s(p.name, _countof(p.name), d.name.c_str(), _TRUNCATE);
        devicePackets.push_back(p);
    }

    // Fill the header (counts tell the receiver how to parse the payload).
    FullMonitorPacketExtended header;
    header.id = ++currentId;
    header.cpu = (float) cpuData.load;
    header.ramUsed = (float) ramData.usedGB;
    header.ramPerc = (uint8_t) ramData.usagePercentage;
    header.netCount = (uint8_t) netPackets.size();
    header.audioCount = (uint8_t) audioPackets.size();
    header.hasMedia = (wcslen(mediaData.title) > 0) ? 1 : 0;
    header.deviceCount = (uint8_t) devicePackets.size();
    header.audezeBattery = (uint8_t) audezeData.batteryLevel;

    // Allocate one contiguous buffer: [header][net...][audio...][media?][devices...]
    size_t netSize = netPackets.size() * sizeof(InterfacePacket);
    size_t audioSize = audioPackets.size() * sizeof(AudioPacket);
    size_t mediaSize = header.hasMedia ? sizeof(MediaPacket) : 0;
    size_t devicesSize = devicePackets.size() * sizeof(AudioDevicePacket);

    size_t totalSize = sizeof(header) + netSize + audioSize + mediaSize + devicesSize;
    std::vector<char> buffer(totalSize);
    size_t offset = 0;

    memcpy(buffer.data() + offset, &header, sizeof(header));
    offset += sizeof(header);
    if (!netPackets.empty()) {
        memcpy(buffer.data() + offset, netPackets.data(), netSize);
        offset += netSize;
    }
    if (!audioPackets.empty()) {
        memcpy(buffer.data() + offset, audioPackets.data(), audioSize);
        offset += audioSize;
    }
    if (header.hasMedia) {
        memcpy(buffer.data() + offset, &mediaData, sizeof(MediaPacket));
        offset += mediaSize;
    }
    if (!devicePackets.empty()) {
        memcpy(buffer.data() + offset, devicePackets.data(), devicesSize);
        offset += devicesSize;
    }

    // Send packet to server (UDP).
    sendto(sock, buffer.data(), (int) buffer.size(), 0, (sockaddr *) &serverAddr, sizeof(serverAddr));
}
