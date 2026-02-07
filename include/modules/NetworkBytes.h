#ifndef NETWORKBYTES_H
#define NETWORKBYTES_H

#include <winsock2.h>
#include <windows.h>
#include "BaseMonitor.h"
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <map>
#include <mutex>

struct InterfaceData {
    std::wstring description;
    uint64_t bytesIn;
    uint64_t bytesOut;
    double speedInKB;
    double speedOutKB;
};

class NetworkBytes : public BaseMonitor {
private:
    std::vector<InterfaceData> interfaces;
    // LUID (64-bit ID) bazlı trafik takibi
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> lastTrafficMap;
    mutable std::mutex dataMutex;

public:
    NetworkBytes(int interval);
    void update() override;
    void display() const override;
    std::vector<InterfaceData> getInterfaces() const;
};

#endif