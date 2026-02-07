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
#include "modules/NetworkBytes.h"
#include <algorithm>

NetworkBytes::NetworkBytes(int interval) : BaseMonitor(interval) {}

NetworkBytes::~NetworkBytes() {
    stop();
}

void NetworkBytes::init() {
    // İlk çalıştırmada fark oluşması için değerleri bir kez oku
    update();
}

void NetworkBytes::update() {
    PMIB_IF_TABLE2 table = nullptr;

    if (GetIfTable2(&table) == NO_ERROR) {
        std::vector<InterfaceData> currentInterfaces;

        // Hız hesabı için geçen süreyi saniyeye çevir
        double seconds = intervalMs / 1000.0;

        for (ULONG i = 0; i < table->NumEntries; i++) {
            MIB_IF_ROW2& row = table->Table[i];

            // --- FİLTRELEME MANTIĞI ---
            bool isCorrectType = (row.Type == IF_TYPE_ETHERNET_CSMACD || row.Type == IF_TYPE_IEEE80211);
            bool hasMac = (row.PhysicalAddressLength > 0);

            std::wstring desc = row.Description;
            std::wstring lowDesc = desc;
            std::transform(lowDesc.begin(), lowDesc.end(), lowDesc.begin(), ::towlower);

            bool isVirtual = (lowDesc.find(L"filter") != std::wstring::npos ||
                              lowDesc.find(L"scheduler") != std::wstring::npos ||
                              lowDesc.find(L"virtual") != std::wstring::npos ||
                              lowDesc.find(L"native") != std::wstring::npos ||
                              lowDesc.find(L"npcap") != std::wstring::npos);

            if (isCorrectType && hasMac && !isVirtual) {
                uint64_t luid = row.InterfaceLuid.Value;
                double speedIn = 0, speedOut = 0;

                // Daha önce bu kartı gördüysek hız hesabı yap
                if (lastTrafficMap.count(luid)) {
                    auto& prev = lastTrafficMap[luid];
                    // (Şu anki byte - Önceki byte) / 1024 / saniye = KB/s
                    speedIn = (row.InOctets - prev.first) / 1024.0 / seconds;
                    speedOut = (row.OutOctets - prev.second) / 1024.0 / seconds;
                }

                // Değerleri bir sonraki update için sakla
                lastTrafficMap[luid] = std::make_pair((uint64_t)row.InOctets, (uint64_t)row.OutOctets);

                InterfaceData iface;
                iface.description = desc;
                iface.speedInKB = (speedIn < 0) ? 0 : speedIn;
                iface.speedOutKB = (speedOut < 0) ? 0 : speedOut;

                currentInterfaces.push_back(iface);
            }
        }

        // Thread-safe atama
        std::lock_guard<std::mutex> lock(dataMutex);
        interfaces = std::move(currentInterfaces);

        if (table) FreeMibTable(table);
    }
}

std::vector<InterfaceData> NetworkBytes::getData() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return interfaces;
}