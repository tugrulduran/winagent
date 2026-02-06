#include "NetworkReporter.h"
#include <cstring>
#include <iostream>

NetworkReporter::NetworkReporter(const std::string& ip, int port, int ms,
                                 CPUMonitor* c, MemoryMonitor* r, NetworkBytes* n, AudioMonitor* a)
    : intervalMs(ms), cpuPtr(c), ramPtr(r), netPtr(n), audioPtr(a) {

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // Veri Gönderim Soketi (Ajan -> Node.js)
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
}

NetworkReporter::~NetworkReporter() {
    stop();
    closesocket(sock);
    WSACleanup();
}

void NetworkReporter::start() {
    active = true;
    // Veri gönderim döngüsü
    worker = std::thread(&NetworkReporter::run, this);

    // Komut dinleme döngüsü (Node.js -> Ajan)
    listenerActive = true;
    listenerThread = std::thread(&NetworkReporter::listenForCommands, this);
}

void NetworkReporter::stop() {
    active = false;
    listenerActive = false;

    // Soketi kapatmak recvfrom engelini kırar
    SOCKET tempSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // Dinleyiciyi uyandırmak için boş paket gönderilebilir veya soket kapatılabilir

    if (worker.joinable()) worker.join();
    if (listenerThread.joinable()) listenerThread.join();
}

void NetworkReporter::run() {
    while (active) {
        sendData();
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

// Node.js'den gelen ses komutlarını dinleyen fonksiyon
void NetworkReporter::listenForCommands() {
    SOCKET cmdSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in cmdAddr;
    cmdAddr.sin_family = AF_INET;
    cmdAddr.sin_addr.s_addr = INADDR_ANY;
    cmdAddr.sin_port = htons(5001); // Komut Portu

    if (bind(cmdSock, (sockaddr*)&cmdAddr, sizeof(cmdAddr)) == SOCKET_ERROR) {
        std::cerr << "Komut portu baglanamadi!" << std::endl;
        return;
    }

    while (listenerActive) {
        // [PID (4 byte)][Volume (4 byte float)] = 8 Byte
        char cmdBuffer[8];
        int bytesReceived = recvfrom(cmdSock, cmdBuffer, 8, 0, NULL, NULL);

        if (bytesReceived == 8) {
            uint32_t targetPid;
            float newVol;
            memcpy(&targetPid, cmdBuffer, 4);
            memcpy(&newVol, cmdBuffer + 4, 4);

            // AudioMonitor üzerinden sesi anında değiştir
            if (audioPtr) {
                audioPtr->setVolumeByPID(targetPid, newVol);
            }
        }
    }
    closesocket(cmdSock);
}

void NetworkReporter::sendData() {
    // 1. Verileri Topla
    auto ifaces = netPtr->getInterfaces();
    auto audioApps = audioPtr->getApps(); // AudioMonitor'e getApps metodu eklenmiş olmalı

    // 2. Network Paketlerini Hazırla
    std::vector<InterfacePacket> netPackets;
    for (const auto& f : ifaces) {
        netPackets.push_back({ (float)f.speedInKB, (float)f.speedOutKB });
    }

    // 3. Audio Paketlerini Hazırla (Node.js Parser ile uyumlu: 58 byte her biri)
    #pragma pack(push, 1)
    struct AudioPacket {
        uint32_t pid;
        wchar_t name[25];
        float volume;
    };
    #pragma pack(pop)

    std::vector<AudioPacket> audioPackets;
    for (const auto& app : audioApps) {
        AudioPacket p;
        p.pid = app.pid;
        p.volume = app.volume;
        memset(p.name, 0, sizeof(p.name));
        wcsncpy(p.name, app.name.c_str(), 24);
        audioPackets.push_back(p);
    }

    // 4. Header Oluştur
    FullMonitorPacket header = {
        ++currentId,
        (float)cpuPtr->getLastValue(),
        (float)ramPtr->getUsedGB(),
        (uint8_t)ramPtr->getPercent(),
        (uint8_t)netPackets.size()
    };

    // 5. Buffer Hesapla ve Birleştir
    size_t netSize = netPackets.size() * sizeof(InterfacePacket);
    size_t audioSize = audioPackets.size() * sizeof(AudioPacket);
    size_t totalSize = sizeof(FullMonitorPacket) + netSize + audioSize;

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

    // 6. Tek Seferde Gönder
    sendto(sock, buffer.data(), (int)buffer.size(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}