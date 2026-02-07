#ifndef MEDIAMONITOR_H
#define MEDIAMONITOR_H

#include "BaseMonitor.h"
#include <string>
#include <windows.h>
#include <mutex>

/*
 * MediaMonitor (Windows Global System Media Transport Controls)
 *
 * What it does:
 * - Tries to read current media info (title, source, timeline, play state).
 * - Supports media control commands (play/pause, next, previous, seek).
 *
 * Packing:
 * - NetworkReporter sends this struct as raw bytes.
 * - Packing makes the byte layout predictable.
 */
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

    // EnumWindows callback used as a fallback to guess media title from window titles.
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    // Removes common suffixes from a title (makes the UI cleaner).
    void cleanTitle(std::wstring& title);

    // Formats seconds into a string like "mm:ss" or "h:mm:ss".
    std::string formatTime(uint32_t seconds) const;

public:
    MediaMonitor(int ms = 1000);
    virtual ~MediaMonitor();

    void init() override;
    void update() override;

    // Handles media commands from UI or NetworkReporter:
    // 1: Toggle Play/Pause, 2: Next, 3: Prev, 4/5: Seek +/- 10 seconds
    void sendMediaCommand(int commandId);

    // Returns a copy of the current packet (thread-safe).
    MediaPacket getData() const {
        std::lock_guard<std::mutex> lock(mediaMutex);
        return currentData;
    }
};

#endif