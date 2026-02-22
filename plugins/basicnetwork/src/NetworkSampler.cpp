#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600 // Vista+ support

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mutex>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <vector>
#include <QJsonArray>
#include "NetworkSampler.h"

namespace basicnetwork {

    static bool isProbablyVirtualOrFiltered(const std::wstring& description) {
        std::wstring descLower = description;
        std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::towlower);

        // Keep in sync with the heuristics used in update().
        return (
            descLower.find(L"virtual") != std::wstring::npos ||
            descLower.find(L"npcap") != std::wstring::npos ||
            descLower.find(L"filter") != std::wstring::npos ||
            descLower.find(L"bluetooth device") != std::wstring::npos ||
            descLower.find(L"qos packet scheduler") != std::wstring::npos ||
            descLower.find(L"ndis network device") != std::wstring::npos
        );
    }

    std::vector<InterfaceData> NetworkSampler::enumeratePhysicalInterfaces() {
        std::vector<InterfaceData> out;

        PMIB_IF_TABLE2 table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR) {
            return out;
        }

        out.reserve(table->NumEntries);
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            MIB_IF_ROW2& row = table->Table[i];

            const bool isPhysical = (row.Type == IF_TYPE_ETHERNET_CSMACD || row.Type == IF_TYPE_IEEE80211);
            if (!isPhysical || row.PhysicalAddressLength == 0) continue;

            const std::wstring desc = row.Description;
            if (isProbablyVirtualOrFiltered(desc)) continue;

            wchar_t buf[39];
            StringFromGUID2(row.InterfaceGuid, buf, 39);
            const std::wstring guidStr = buf;

            InterfaceData d;
            d.name = row.Alias;
            d.description = desc;
            d.guid = guidStr;
            d.speedInKB = 0.0;
            d.speedOutKB = 0.0;
            out.push_back(std::move(d));
        }

        if (table) FreeMibTable(table);

        // Stable sorting by Alias (case-insensitive)
        std::sort(out.begin(), out.end(), [](const InterfaceData& a, const InterfaceData& b) {
            std::wstring an = a.name;
            std::wstring bn = b.name;
            std::transform(an.begin(), an.end(), an.begin(), ::towlower);
            std::transform(bn.begin(), bn.end(), bn.begin(), ::towlower);
            return an < bn;
        });

        return out;
    }
    void NetworkSampler::init(QJsonObject config) {
        const QJsonValue v = config.value("allowedInterfaces");
        allowedInterfaces.clear();
        if (!v.isArray()) {
            return;
        }

        const QJsonArray arr = v.toArray();
        for (const QJsonValue &item: arr) {
            if (!item.isString()) continue;

            const QString s = item.toString().trimmed();
            if (s.isEmpty()) continue;

            allowedInterfaces.insert(s.toStdWString());
        }
    }

    std::map<std::wstring, InterfaceData> NetworkSampler::update() {
        PMIB_IF_TABLE2 table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR) return {};

        for (ULONG i = 0; i < table->NumEntries; ++i) {
            MIB_IF_ROW2 &row = table->Table[i];

            std::wstring desc = row.Description;
            wchar_t buf[39];
            StringFromGUID2(row.InterfaceGuid, buf, 39);
            std::wstring guidStr = buf;

            if (
                !allowedInterfaces.contains(guidStr) &&
                !allowedInterfaces.contains(row.Alias) &&
                !allowedInterfaces.empty()
            ) { continue; }

            const bool isPhysical = (row.Type == IF_TYPE_ETHERNET_CSMACD || row.Type == IF_TYPE_IEEE80211);
            if (!isPhysical || row.PhysicalAddressLength == 0) continue;

            std::transform(desc.begin(), desc.end(), desc.begin(), ::towlower);

            if (desc.find(L"virtual") != std::wstring::npos ||
                desc.find(L"npcap") != std::wstring::npos ||
                desc.find(L"filter") != std::wstring::npos ||
                desc.find(L"bluetooth device") != std::wstring::npos ||
                desc.find(L"qos packet scheduler") != std::wstring::npos ||
                desc.find(L"ndis network device") != std::wstring::npos
            )
                continue;

            uint64_t timestamp = static_cast<uint64_t>(
                duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );
            uint64_t tx = row.OutOctets;
            uint64_t rx = row.InOctets;

            lastTrafficMap.try_emplace(guidStr, TrafficSnap{0, 0, 0});

            auto lt = lastTrafficMap.find(guidStr);
            auto &prev = lt->second;

            uint64_t dtx = (tx >= prev.tx) ? (tx - prev.tx) : 0;
            uint64_t drx = (rx >= prev.rx) ? (rx - prev.rx) : 0;
            uint64_t dt = timestamp - prev.timestamp;
            lastTrafficMap[guidStr] = {timestamp, tx, rx};

            if (dt == 0) {
                continue;
            }

            interfaces.insert_or_assign(guidStr, InterfaceData{
                                            row.Alias,
                                            row.Description,
                                            guidStr,
                                            (drx / 1024.0f) / (dt / 1000.0f),
                                            (dtx / 1024.0f) / (dt / 1000.0f)
                                        });
        }

        if (table) FreeMibTable(table);

        return interfaces;
    }
}
