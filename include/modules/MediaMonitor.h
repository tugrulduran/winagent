#ifndef MEDIAMONITOR_H
#define MEDIAMONITOR_H

#include "BaseMonitor.h"
#include <string>
#include <windows.h>
#include <mutex>

// Bellek hizalamasını koruyoruz (NetworkReporter ve Paketleme için kritik)
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

    // Windows API için statik callback (Linker hatasını önler)
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    // Yardımcı iç metodlar
    void cleanTitle(std::wstring& title);
    std::string formatTime(uint32_t seconds) const;

public:
    // Constructor: BaseMonitor'den gelen interval parametresi
    MediaMonitor(int ms = 1000);
    virtual ~MediaMonitor();

    // BaseMonitor arayüzü
    void init() override;
    void update() override;

    // --- SENİN MEŞHUR MEDYA KOMUTLARIN ---
    // Dashboard'dan veya NetworkReporter'dan gelen komutları işler
    // 1: Toggle Play/Pause, 2: Next, 3: Prev
    void sendMediaCommand(int commandId);

    // Ham veriyi (struct) güvenli şekilde dönen getter
    MediaPacket getData() const {
        std::lock_guard<std::mutex> lock(mediaMutex);
        return currentData;
    }
};

#endif