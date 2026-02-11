#include <thread>
#include <chrono>
#include <algorithm>
#include "modules/AudezeMonitor.h"

void AudezeMonitor::init() {
    hid_init();
}

std::string AudezeMonitor::findDevicePath() {
    struct hid_device_info* devs = hid_enumerate(VENDOR_ID, 0x0);
    struct hid_device_info* cur_dev = devs;
    std::string path = "";

    while (cur_dev) {
        if ((cur_dev->product_id == PRODUCT_ID_1 || cur_dev->product_id == PRODUCT_ID_2) && 
            cur_dev->usage_page == 0xFF13) {
            path = cur_dev->path;
            break;
        }
        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs);
    return path;
}

std::vector<uint8_t> AudezeMonitor::sendCommandAndGetReport(hid_device* handle, const std::vector<uint8_t>& command) {
    std::vector<uint8_t> buf(MSG_SIZE + 1, 0);
    // HIDAPI expects report ID at index 0 for write
    std::copy(command.begin(), command.end(), buf.begin());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
    
    if (hid_write(handle, buf.data(), MSG_SIZE) <= 0) {
        return {};
    }

    // Prepare buffer for reading Report ID 0x07
    std::vector<uint8_t> readBuf(MSG_SIZE + 1, 0);
    readBuf[0] = 0x07; 
    
    int res = hid_get_input_report(handle, readBuf.data(), MSG_SIZE + 1);
    if (res <= 0) {
        return {};
    }
    return readBuf;
}

void AudezeMonitor::update() {
    std::string path = findDevicePath();
    
    if (path.empty()) {
        dashboard_->data.audeze.setBattery(0xFF);
        return;
    }

    hid_device* handle = hid_open_path(path.c_str());
    if (!handle) return;

    std::vector<std::vector<uint8_t>> uniqueRequests = {
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x20},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x25},
        {0x06, 0x07, 0x80, 0x05, 0x5A, 0x03, 0x00, 0x07, 0x1C},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x28},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x83, 0x2C, 0x01},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x83, 0x2C, 0x07},
        {0x06, 0x07, 0x00, 0x05, 0x5A, 0x03, 0x00, 0x07, 0x1C},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x2D},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x2C},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x83, 0x2C, 0x0B},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x2F},
        {0x06, 0x07, 0x80, 0x05, 0x5A, 0x03, 0x00, 0xD6, 0x0C}
    };

    std::vector<std::vector<uint8_t>> statusRequests = {
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x22},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x83, 0x2C, 0x0B},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x01, 0x09, 0x2C},
        {0x06, 0x08, 0x80, 0x05, 0x5A, 0x04, 0x00, 0x83, 0x2C, 0x07}
    };

    std::vector<std::vector<uint8_t>> allResponses;

    for (const auto& cmd : uniqueRequests) {
        auto res = sendCommandAndGetReport(handle, cmd);
        if (!res.empty()) allResponses.push_back(res);
    }

    for (const auto& cmd : statusRequests) {
        auto res = sendCommandAndGetReport(handle, cmd);
        if (!res.empty()) allResponses.push_back(res);
    }

    int battery = -1;
    bool found = false;

    for (const auto& resp : allResponses) {
        if (resp.size() < 5) continue;
        for (size_t i = 0; i <= resp.size() - 5; ++i) {
            if (resp[i] == 0xD6 && resp[i+1] == 0x0C && resp[i+2] == 0x00 && resp[i+3] == 0x00) {
                battery = resp[i+4];
                found = true;
                break;
            }
        }
        if (found) break;
    }

    hid_close(handle);

    dashboard_->data.audeze.setBattery(battery);
}
