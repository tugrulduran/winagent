#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <QJsonObject>

struct InterfaceData {
    std::wstring name;
    std::wstring description;
    std::wstring guid;
    double speedInKB = 0.0;
    double speedOutKB = 0.0;
};

struct TrafficSnap {
    uint64_t timestamp = 0;
    uint64_t tx = 0;
    uint64_t rx = 0;
};

namespace basicnetwork {
    class NetworkSampler {
    public:
        // Enumerates physical network interfaces (Ethernet/Wi-Fi) similarly to update(),
        // but without traffic/speed calculation and without applying allowedInterfaces.
        static std::vector<InterfaceData> enumeratePhysicalInterfaces();

        std::map<std::wstring, InterfaceData> update();

        void init(QJsonObject config);

    private:
        std::map<std::wstring, TrafficSnap> lastTrafficMap;
        std::map<std::wstring, InterfaceData> interfaces;

        std::unordered_set<std::wstring> allowedInterfaces = {};
    };
}
