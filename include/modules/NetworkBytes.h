#ifndef NETWORKBYTES_H
#define NETWORKBYTES_H

#include "BaseMonitor.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <map>
#include <mutex>

/*
 * InterfaceData
 * -------------
 * Speed values are computed as a delta between two byte counters:
 * - speedInKB  = incoming KB per second
 * - speedOutKB = outgoing KB per second
 */
struct InterfaceData {
    std::wstring description;
    double speedInKB = 0.0;
    double speedOutKB = 0.0;
};

class NetworkBytes : public BaseMonitor {
private:
    std::vector<InterfaceData> interfaces;

    // Key: interface LUID, Value: last seen (InOctets, OutOctets).
    // Used to compute speed from byte counters.
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> lastTrafficMap;

    mutable std::mutex dataMutex;

public:
    NetworkBytes(int interval = 1000);
    ~NetworkBytes() override;

    void init() override;
    void update() override;

    // Returns a copy of the latest interface list (thread-safe).
    std::vector<InterfaceData> getData() const;
};

#endif