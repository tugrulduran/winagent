#ifndef AUDEZEMONITOR_H
#define AUDEZEMONITOR_H

#include <hidapi.h>
#include <string>
#include <vector>
#include "BaseMonitor.h"

class AudezeMonitor : public BaseMonitor {
public:
    AudezeMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };
    ~AudezeMonitor() override { stop(); hid_exit(); }

    void init() override;

    void update() override;

private:
    static constexpr uint16_t VENDOR_ID = 0x3329;
    static constexpr uint16_t PRODUCT_ID_1 = 0x4B19;
    static constexpr uint16_t PRODUCT_ID_2 = 0x4B18;
    static constexpr int MSG_SIZE = 62;
    static constexpr int DELAY_MS = 60;

    std::string findDevicePath();

    std::vector<uint8_t> sendCommandAndGetReport(hid_device *handle, const std::vector<uint8_t> &command);
};

#endif
