#ifndef NETWORKBYTES_H
#define NETWORKBYTES_H

#include "BaseMonitor.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <map>
#include <mutex>

// Arayüz verilerini tutan yapı
struct InterfaceData {
    std::wstring description;
    double speedInKB = 0.0;
    double speedOutKB = 0.0;
};

class NetworkBytes : public BaseMonitor {
private:
    std::vector<InterfaceData> interfaces;
    // LUID bazlı trafik takibi (Hız hesabı için önceki değerleri saklar)
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> lastTrafficMap;
    mutable std::mutex dataMutex;

public:
    NetworkBytes(int interval = 1000);
    ~NetworkBytes() override;

    void init() override;
    void update() override;

    // String yerine doğrudan vektör döner
    std::vector<InterfaceData> getData() const;
};

#endif