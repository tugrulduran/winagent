#pragma once
#include <functional>
#include <string>
#include <windows.h>
#include <windows.media.control.h>

enum MediaSource : int8_t {
    MEDIA_SOURCE_ERROR = -1,
    MEDIA_SOURCE_NO_MEDIA = 0,
    MEDIA_SOURCE_GENERIC,
    MEDIA_SOURCE_YOUTUBE,
    MEDIA_SOURCE_TWITCH,
    MEDIA_SOURCE_KICK,
    MEDIA_SOURCE_MEDIA_PLAYER,
    MEDIA_SOURCE_YOUTUBE_MUSIC,
    MEDIA_SOURCE_SPOTIFY
};

struct MediaData {
    std::wstring title;
    int8_t source;
    uint16_t duration;
    uint16_t currentTime;
    bool isPlaying;
};

namespace media {
    class MediaMonitor {
    public:
        MediaData getMedia();

        // Handles media commands from UI:
        void playpause();

        void next();

        void prev();

        void jump(uint16_t time);

    private:
        void cleanTitle(std::wstring &title);

        std::string formatTime(uint32_t seconds) const;

        void executeMediaAction(
            std::function<HRESULT(ABI::Windows::Media::Control::IGlobalSystemMediaTransportControlsSession *)> action);
    };
}
