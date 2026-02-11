#ifndef MEDIAMONITOR_H
#define MEDIAMONITOR_H

#include <functional>
#include <string>
#include <windows.h>
#include <windows.media.control.h>
#include "BaseMonitor.h"

class MediaMonitor : public BaseMonitor {
public:
    MediaMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    };

    ~MediaMonitor() override;

    void init() override;

    void update() override;

    // Handles media commands from UI or NetworkReporter:
    // 1: Toggle Play/Pause, 2: Next, 3: Prev, 4/5: Seek +/- 10 seconds
    void playpause();

    void stop();

    void next();

    void prev();

    void jump(uint16_t time);

private:
    void cleanTitle(std::wstring &title);

    std::string formatTime(uint32_t seconds) const;

    void executeMediaAction(
        std::function<HRESULT(ABI::Windows::Media::Control::IGlobalSystemMediaTransportControlsSession *)> action);
};

#endif
