#pragma once
#include <hidapi.h>
#include <cstdint>

namespace audeze {
    class AudezeSampler {
    public:
        AudezeSampler();

        uint8_t getBattery();

    private:
        void init();

        static constexpr uint16_t VENDOR_ID = 0x3329;
        static constexpr uint16_t PRODUCT_ID_1 = 0x4B19;
        static constexpr uint16_t PRODUCT_ID_2 = 0x4B18;
        static constexpr int MSG_SIZE = 62;
        static constexpr int DELAY_MS = 60;

        std::string findDevicePath();

        std::vector<uint8_t> sendCommandAndGetReport(hid_device *handle, const std::vector<uint8_t> &command);
    };
}
