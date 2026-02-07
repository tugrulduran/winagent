#pragma once
#include "BaseMonitor.h"
#include <string>
#include <windows.h>
#include <mutex>
#include <memory>

#pragma pack(push, 1)
struct MediaPacket {
    wchar_t title[64];
    wchar_t source[16];
    uint32_t currentTime;
    uint32_t totalTime;
    bool isPlaying;
};
#pragma pack(pop)

class MediaMonitor : public BaseMonitor {
private:
    MediaPacket currentData;
    mutable std::mutex mediaMutex;

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    void cleanTitle(std::wstring& title);
    std::string formatTime(uint32_t seconds) const;

public:
    MediaMonitor(int ms);
    virtual ~MediaMonitor();

    void update() override;
    void display() const override;

    // Dashboard'dan gelen komutları işler (1: Toggle, 2: Next, 3: Prev)
    void sendMediaCommand(int commandId);

    MediaPacket getData() const {
        std::lock_guard<std::mutex> lock(mediaMutex);
        return currentData;
    }
};