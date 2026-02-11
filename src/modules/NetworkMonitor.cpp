#include <QDebug>
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600 // Vista+ support

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <vector>
#include <mutex>
#include <algorithm>
#include <windows.media.control.h>
#include "modules/NetworkMonitor.h"

void NetworkMonitor::init() {
    update();
}

void NetworkMonitor::update() {
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR) return;

    std::vector<InterfaceData> currentInterfaces;
    const double seconds = intervalMs / 1000.0;
    uint64_t totalIn = 0, totalOut = 0;

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        MIB_IF_ROW2 &row = table->Table[i];

        std::wstring desc = row.Description;
        wchar_t buf[39];
        StringFromGUID2(row.InterfaceGuid, buf, 39);
        std::wstring guidStr = buf;

        // qDebug() << "ALL INTERFACES: " << desc << "GUID: " << guidStr << " IN: " << row.InOctets << " OUT: " << row.OutOctets;
        if (!allowedInterfaces.contains(guidStr)) { continue; }
        // qDebug() << "WATCHING: " << desc << "GUID: " << guidStr << " IN: " << row.InOctets << " OUT: " << row.OutOctets;

        const bool isPhysical = (row.Type == IF_TYPE_ETHERNET_CSMACD || row.Type == IF_TYPE_IEEE80211);
        if (!isPhysical || row.PhysicalAddressLength == 0) continue;

        std::transform(desc.begin(), desc.end(), desc.begin(), ::towlower);

        if (desc.find(L"virtual") != std::wstring::npos ||
            desc.find(L"npcap") != std::wstring::npos ||
            desc.find(L"filter") != std::wstring::npos)
            continue;

        totalIn += row.InOctets;
        totalOut += row.OutOctets;
    }
    // qDebug() << "--------------";

    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    uint64_t timeDiff = ts - lastTimestamp;
    uint64_t inDiff = totalIn - lastInOctets;
    uint64_t outDiff = totalOut - lastOutOctets;

    lastInOctets = totalIn;
    lastOutOctets = totalOut;
    lastTimestamp = ts;

    uint64_t inSpeed = (inDiff * 1000.0f) / timeDiff;
    uint64_t outSpeed = (outDiff * 1000.0f) / timeDiff;


    dashboard_->data.network.setRxBytes(inSpeed);
    dashboard_->data.network.setTxBytes(outSpeed);

    if (table) FreeMibTable(table);
}
