#include "NetworkReporter.h"
#include <cstring>
#include <iostream>

#include "modules/AudioDeviceMonitor.h"
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
    if (!active && !listenerActive) return;

    active = false;
    listenerActive = false;

    // Dinleme thread'ini recvfrom'dan kurtarmak için soketi kapatıyoruz
    if (cmdSock != INVALID_SOCKET) {
        closesocket(cmdSock);
        cmdSock = INVALID_SOCKET;
    }

    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

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
    // KRİTİK: Bu thread üzerinde COM işlemlerini başlatıyoruz
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    cmdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(5001);

    if (bind(cmdSock, (sockaddr *) &localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        CoUninitialize();
        return;
    }

    // Konsolda dinlemenin başladığını görelim
    std::cout << "[SYSTEM] Listener Active on Port 5001..." << std::endl;

    while (listenerActive) {
        char cmdBuffer[8];
        int bytesReceived = recvfrom(cmdSock, cmdBuffer, 8, 0, NULL, NULL);

        if (!listenerActive || bytesReceived == SOCKET_ERROR) break;

        if (bytesReceived == 8) {
            uint32_t targetPid;
            float dataVal;
            memcpy(&targetPid, cmdBuffer, 4);
            memcpy(&dataVal, cmdBuffer + 4, 4);

            if (targetPid == 0xFFFFFFFF) { // Medya
                if (mediaPtr) mediaPtr->sendMediaCommand((int)dataVal);
            }
            else if (targetPid == 0xFFFFFFFE) {
                int deviceIndex = (int)dataVal;

                bool ok = AudioDeviceSwitcher::setDefaultByIndex(deviceIndex);

                if (ok) {
                    std::cout << "[SYSTEM] Audio device switched successfully." << std::endl;
                } else {
                    std::cerr << "[ERROR] Audio device switch failed." << std::endl;
                }
            }
            else { // Ses Seviyesi
                if (audioPtr) audioPtr->setVolumeByPID(targetPid, dataVal);
            }
        }
    }
    if (cmdSock != INVALID_SOCKET) closesocket(cmdSock);
    CoUninitialize();
}

void NetworkReporter::sendData() {
    // 1. Verileri Hazırla
    auto ifaces = netPtr->getInterfaces();
    auto audioApps = audioPtr->getApps();
    MediaPacket mediaData = mediaPtr->getData();
    // YENİ: Ses Aygıtlarını Listele
    auto devices = AudioDeviceSwitcher::listDevices();

    // 2. Alt Paketleri Oluştur
    std::vector<InterfacePacket> netPackets;
    for (const auto &f: ifaces) netPackets.push_back({(float) f.speedInKB, (float) f.speedOutKB});

#pragma pack(push, 1)
    struct AudioPacket {
        uint32_t pid;
        wchar_t name[25];
        float volume;
    }; // 58 Byte
#pragma pack(pop)

    std::vector<AudioPacket> audioPackets;
    for (const auto &app: audioApps) {
        AudioPacket p;
        p.pid = app.pid;
        p.volume = app.volume;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy(p.name, app.name.c_str(), 24);
        audioPackets.push_back(p);
    }

    // YENİ: Ses Aygıt Paketlerini Hazırla
    std::vector<AudioDevicePacket> devicePackets;
    for (const auto &d: devices) {
        AudioDevicePacket p;
        p.index = (uint8_t) d.index;
        p.isDefault = d.isDefault ? 1 : 0;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy_s(
            p.name,
            _countof(p.name),
            d.name.c_str(),
            _TRUNCATE
        );
        devicePackets.push_back(p);
    }

    // 3. Header'ı Doldur (18 Byte)
    FullMonitorPacketExtended header;
    header.id = ++currentId;
    header.cpu = (float) cpuPtr->getLastValue();
    header.ramUsed = (float) ramPtr->getUsedGB();
    header.ramPerc = (uint8_t) ramPtr->getPercent();
    header.netCount = (uint8_t) netPackets.size();
    header.audioCount = (uint8_t) audioPackets.size();
    header.hasMedia = (wcslen(mediaData.title) > 0) ? 1 : 0;
    header.deviceCount = (uint8_t) devicePackets.size(); // KRİTİK

    // 4. Dinamik Buffer Boyutu Hesapla
    size_t netSize = netPackets.size() * sizeof(InterfacePacket);
    size_t audioSize = audioPackets.size() * sizeof(AudioPacket);
    size_t mediaSize = header.hasMedia ? sizeof(MediaPacket) : 0;
    size_t devicesSize = devicePackets.size() * sizeof(AudioDevicePacket);

    size_t totalSize = sizeof(header) + netSize + audioSize + mediaSize + devicesSize;

    std::vector<char> buffer(totalSize);
    size_t offset = 0;

    // SIRALAMA: Header -> Net -> Audio -> Media -> Devices
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
    // YENİ: Aygıtları buffer'a ekle
    if (!devicePackets.empty()) {
        memcpy(buffer.data() + offset, devicePackets.data(), devicesSize);
        offset += devicesSize;
    }

    // 5. UDP Gönder
    sendto(sock, buffer.data(), (int) buffer.size(), 0, (sockaddr *) &serverAddr, sizeof(serverAddr));
}
