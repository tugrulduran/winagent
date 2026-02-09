#ifndef AUDEZEMONITOR_H
#define AUDEZEMONITOR_H

#include "BaseMonitor.h"
#include <hidapi.h>
#include <mutex>
#include <string>
#include <vector>

struct AudezeData {
    int batteryLevel = -1;      // 0..100, -1 means unavailable
    bool isConnected = false;
    std::wstring deviceName = L"Audeze Maxwell";
};

class AudezeMonitor : public BaseMonitor {
private:
    static constexpr uint16_t VENDOR_ID = 0x3329;
    static constexpr uint16_t PRODUCT_ID_1 = 0x4B19;
    static constexpr uint16_t PRODUCT_ID_2 = 0x4B18;
    static constexpr int MSG_SIZE = 62;
    static constexpr int DELAY_MS = 60;

    AudezeData data;
    mutable std::mutex dataMutex;

    // Helper to find the specific HID path with usage page 0xFF13
    std::string findDevicePath();
    
    // Helper to send command and get report 0x07
    std::vector<uint8_t> sendCommandAndGetReport(hid_device* handle, const std::vector<uint8_t>& command);

public:
    AudezeMonitor(int interval = 30000); // Default 30s as battery doesn't change fast
    ~AudezeMonitor();

    void init() override;
    void update() override;

    AudezeData getData() const;
};

#endif