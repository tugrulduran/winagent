#ifndef NETWORKMONITOR_H
#define NETWORKMONITOR_H

#include <vector>
#include <map>
#include <mutex>
#include "BaseMonitor.h"

struct InterfaceData {
    std::wstring description;
    double speedInKB = 0.0;
    double speedOutKB = 0.0;
};

class NetworkMonitor : public BaseMonitor {
public:
    NetworkMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };
    ~NetworkMonitor() override { stop(); }

    void init() override;
    void update() override;

private:
    mutable std::mutex dataMutex;
    std::vector<InterfaceData> interfaces;

    uint64_t lastInOctets = 0;
    uint64_t lastOutOctets = 0;
    uint64_t lastTimestamp = 0;

    std::map<uint64_t, std::pair<uint64_t, uint64_t>> lastTrafficMap;

    std::unordered_set<std::wstring> allowedInterfaces = {
        L"{ACE32DCF-C440-404C-8300-CEAD91002141}",  // Ethernet
        L"{05CBCB0C-6E17-4072-B148-1CCD923800FE}"   // Wifi
    };
};

#endif // NETWORKMONITOR_H
