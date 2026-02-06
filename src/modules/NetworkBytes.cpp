#ifndef UNICODE
#define UNICODE
#endif

// Windows sürümünü en tepeye zorla
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600 // Vista+ desteği

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <algorithm> // find için
#include "modules/NetworkBytes.h"

NetworkBytes::NetworkBytes(int interval) : BaseMonitor(interval) {
    update();
}

void NetworkBytes::update() {
    PMIB_IF_TABLE2 table = nullptr;

    if (GetIfTable2(&table) == NO_ERROR) {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::vector<InterfaceData> currentInterfaces;

        for (ULONG i = 0; i < table->NumEntries; i++) {
            MIB_IF_ROW2& row = table->Table[i];

            // --- GARANTİ FİLTRELEME ---
            // ConnectorPresent hata verdiği için en temel fiziksel özellikleri kullanıyoruz.

            // 1. Tip: Ethernet veya Wi-Fi
            bool isCorrectType = (row.Type == IF_TYPE_ETHERNET_CSMACD || row.Type == IF_TYPE_IEEE80211);

            // 2. Fiziksel Adres: Gerçek kartların mutlaka bir MAC adresi olur.
            bool hasMac = (row.PhysicalAddressLength > 0);

            // 3. Yazılımsal Tanım Kontrolü (WFP, QoS vb. temizlemek için en etkili yol)
            std::wstring desc = row.Description;
            auto toLower = [](std::wstring s) {
                std::transform(s.begin(), s.end(), s.begin(), ::towlower);
                return s;
            };
            std::wstring lowDesc = toLower(desc);

            bool isVirtual = (lowDesc.find(L"filter") != std::wstring::npos ||
                              lowDesc.find(L"scheduler") != std::wstring::npos ||
                              lowDesc.find(L"virtual") != std::wstring::npos ||
                              lowDesc.find(L"native") != std::wstring::npos ||
                              lowDesc.find(L"npcap") != std::wstring::npos);

            // Eğer tip doğruysa, MAC varsa ve açıklama "kirli" değilse alıyoruz.
            if (isCorrectType && hasMac && !isVirtual) {
                uint64_t luid = row.InterfaceLuid.Value;
                double speedIn = 0, speedOut = 0;

                if (lastTrafficMap.count(luid)) {
                    auto& prev = lastTrafficMap[luid];
                    speedIn = (row.InOctets - prev.first) / 1024.0 / (intervalMs / 1000.0);
                    speedOut = (row.OutOctets - prev.second) / 1024.0 / (intervalMs / 1000.0);
                }

                lastTrafficMap[luid] = std::make_pair((uint64_t)row.InOctets, (uint64_t)row.OutOctets);

                InterfaceData data;
                data.description = desc;
                data.speedInKB = (speedIn < 0) ? 0 : speedIn;
                data.speedOutKB = (speedOut < 0) ? 0 : speedOut;

                currentInterfaces.push_back(data);
            }
        }
        interfaces = currentInterfaces;
        if (table) FreeMibTable(table);
    }
}

void NetworkBytes::display() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    for (const auto& iface : interfaces) {
        std::wcout << L"[NET] " << iface.description
                   << L" -> In: " << iface.speedInKB << L" KB/s"
                   << L" | Out: " << iface.speedOutKB << L" KB/s   " << std::endl;
    }
}

std::vector<InterfaceData> NetworkBytes::getInterfaces() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return interfaces;
}