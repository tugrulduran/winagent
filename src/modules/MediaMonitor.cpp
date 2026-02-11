#include <iostream>
#include <iomanip>
#include <sstream>
#include <windows.media.control.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.foundation.h>
#include "modules/MediaMonitor.h"

#pragma comment(lib, "runtimeobject.lib")

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media::Control;
using namespace ABI::Windows::Foundation;

MediaMonitor::~MediaMonitor() {
    stop();
    RoUninitialize();
}

void MediaMonitor::init() {
    // WinRT initialization for this thread.
    RoInitialize(RO_INIT_MULTITHREADED);
}

void MediaMonitor::cleanTitle(std::wstring &title) {
    // Remove common suffixes that browsers/media apps add to window titles.
    const std::wstring sfx[] = {L" - YouTube", L" - Spotify", L" - Google Chrome", L" - Microsoft Edge"};
    for (const auto &s: sfx) {
        size_t pos = title.find(s);
        if (pos != std::wstring::npos) title.erase(pos);
    }
}

std::string MediaMonitor::formatTime(uint32_t s) const {
    // Helper for formatting seconds into a readable string.
    if (s == 0) return "00:00";
    uint32_t hrs = s / 3600;
    uint32_t mins = (s % 3600) / 60;
    uint32_t secs = s % 60;
    std::stringstream ss;
    if (hrs > 0) ss << hrs << ":" << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    else ss << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    return ss.str();
}

void MediaMonitor::update() {
    // Primary approach:
    // Use Windows Global System Media Transport Controls to get playback state and timeline.
    //
    // This code uses WinRT async operations and waits for completion in a simple loop.
    static thread_local bool threadInit = false;
    if (!threadInit) {
        RoInitialize(RO_INIT_MULTITHREADED);
        threadInit = true;
    }

    uint8_t media_source = MEDIA_SOURCE_NO_MEDIA;
    std::wstring media_title = L"";
    uint64_t media_duration = 0;
    uint64_t media_currentTime = 0;
    bool media_isPlaying = false;

    ComPtr<IGlobalSystemMediaTransportControlsSessionManagerStatics> managerStatics;
    if (SUCCEEDED(
        RoGetActivationFactory(HStringReference(
            RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager).Get(), __uuidof(
            IGlobalSystemMediaTransportControlsSessionManagerStatics), &managerStatics))) {
        ComPtr<IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager *> > op;
        if (SUCCEEDED(managerStatics->RequestAsync(&op))) {
            AsyncStatus status;
            ComPtr<IAsyncInfo> info;
            op.As(&info);
            do {
                info->get_Status(&status);
                if (status == AsyncStatus::Started) Sleep(5);
            } while (status == AsyncStatus::Started);

            if (status == AsyncStatus::Completed) {
                ComPtr<IGlobalSystemMediaTransportControlsSessionManager> manager;
                if (SUCCEEDED(op->GetResults(&manager)) && manager) {
                    ComPtr<IGlobalSystemMediaTransportControlsSession> session;
                    if (SUCCEEDED(manager->GetCurrentSession(&session)) && session) {
                        ComPtr<IGlobalSystemMediaTransportControlsSessionPlaybackInfo> playback;
                        if (SUCCEEDED(session->GetPlaybackInfo(&playback)) && playback) {
                            GlobalSystemMediaTransportControlsSessionPlaybackStatus pStatus;
                            playback->get_PlaybackStatus(&pStatus);
                            media_isPlaying =
                                    pStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus_Playing;
                        }
                        ComPtr<IGlobalSystemMediaTransportControlsSessionTimelineProperties> timeline;
                        if (SUCCEEDED(session->GetTimelineProperties(&timeline)) && timeline) {
                            TimeSpan pos, end;
                            DateTime lastUpdate;
                            if (SUCCEEDED(timeline->get_Position(&pos)) && SUCCEEDED(timeline->get_EndTime(&end)) &&
                                SUCCEEDED(timeline->get_LastUpdatedTime(&lastUpdate))) {
                                uint32_t totalSec = (uint32_t) (end.Duration / 10000000);
                                uint32_t posSec = (uint32_t) (pos.Duration / 10000000);
                                if (media_isPlaying) {
                                    FILETIME nowFT;
                                    GetSystemTimeAsFileTime(&nowFT);
                                    ULARGE_INTEGER nowTicks, lastTicks;
                                    nowTicks.LowPart = nowFT.dwLowDateTime;
                                    nowTicks.HighPart = nowFT.dwHighDateTime;
                                    lastTicks.QuadPart = lastUpdate.UniversalTime;
                                    media_currentTime =
                                            posSec + (uint32_t) ((nowTicks.QuadPart - lastTicks.QuadPart) / 10000000);
                                } else media_currentTime = posSec;
                                if (media_currentTime > totalSec) media_currentTime = totalSec;
                                media_duration = totalSec;
                            }
                        }

                        // NEW: Get Media Properties (Title, Artist, etc.) directly from WinRT
                        ComPtr<IAsyncOperation<GlobalSystemMediaTransportControlsSessionMediaProperties *> > propOp;
                        if (SUCCEEDED(session->TryGetMediaPropertiesAsync(&propOp)) && propOp) {
                            AsyncStatus propStatus;
                            ComPtr<IAsyncInfo> propInfo;
                            propOp.As(&propInfo);
                            do {
                                propInfo->get_Status(&propStatus);
                                if (propStatus == AsyncStatus::Started) Sleep(5);
                            } while (propStatus == AsyncStatus::Started);

                            if (propStatus == AsyncStatus::Completed) {
                                ComPtr<IGlobalSystemMediaTransportControlsSessionMediaProperties> props;
                                if (SUCCEEDED(propOp->GetResults(&props)) && props) {
                                    HString title, appId;
                                    props->get_Title(title.GetAddressOf());
                                    session->get_SourceAppUserModelId(appId.GetAddressOf());

                                    if (title.GetRawBuffer(nullptr)) {
                                        std::wstring fullTitle(title.GetRawBuffer(nullptr));
                                        // Special handling for Kick streams
                                        size_t kickPos = fullTitle.find(L" Stream - Watch Live on Kick");
                                        if (kickPos != std::wstring::npos) {
                                            media_source = MEDIA_SOURCE_KICK;
                                            media_title = fullTitle.substr(0, min(64, kickPos));
                                        } else {
                                            media_source = MEDIA_SOURCE_GENERIC;
                                            media_title = fullTitle.substr(0, 64);
                                        }
                                    }
                                    if (media_source == MEDIA_SOURCE_GENERIC && appId.GetRawBuffer(nullptr)) {
                                        std::wstring app(appId.GetRawBuffer(nullptr));
                                        if (app.find(L"Spotify") != std::wstring::npos)
                                            media_source = MEDIA_SOURCE_SPOTIFY;
                                        else if (app.find(L"Chrome") != std::wstring::npos)
                                            media_source = MEDIA_SOURCE_GENERIC;
                                        else if (app.find(L"Edge") != std::wstring::npos)
                                            media_source = MEDIA_SOURCE_GENERIC;
                                        else if (app.find(L"Video.UI") != std::wstring::npos)
                                            media_source = MEDIA_SOURCE_MEDIA_PLAYER;
                                        else if (app.find(L"Music.UI") != std::wstring::npos)
                                            media_source = MEDIA_SOURCE_MEDIA_PLAYER;
                                        else media_source = MEDIA_SOURCE_GENERIC;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Clean up the title for nicer display.
    std::wstring ts(media_title);
    cleanTitle(ts);
    media_title = ts;

    dashboard_->data.media.set(
        media_title,
        media_source,
        media_duration,
        media_currentTime,
        media_isPlaying
    );
}

void MediaMonitor::executeMediaAction(std::function<HRESULT(IGlobalSystemMediaTransportControlsSession *)> action) {
    HRESULT hrInit = RoInitialize(RO_INIT_MULTITHREADED);

    ComPtr<IGlobalSystemMediaTransportControlsSessionManagerStatics> managerStatics;
    if (SUCCEEDED(RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager).Get(),
        IID_PPV_ARGS(&managerStatics)))) {
        ComPtr<IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager *> > op;
        if (SUCCEEDED(managerStatics->RequestAsync(&op))) {
            // Asenkron bekleme (Basit senkron döngü)
            AsyncStatus status;
            ComPtr<IAsyncInfo> info;
            op.As(&info);
            while (SUCCEEDED(info->get_Status(&status)) && status == AsyncStatus::Started) {
                Sleep(5);
            }

            if (status == AsyncStatus::Completed) {
                ComPtr<IGlobalSystemMediaTransportControlsSessionManager> manager;
                if (SUCCEEDED(op->GetResults(&manager)) && manager) {
                    ComPtr<IGlobalSystemMediaTransportControlsSession> session;
                    if (SUCCEEDED(manager->GetCurrentSession(&session)) && session) {
                        // İŞTE BURADA ASIL KOMUT ÇALIŞIYOR:
                        action(session.Get());
                    }
                }
            }
        }
    }

    if (SUCCEEDED(hrInit)) RoUninitialize();
}

void MediaMonitor::playpause() {
    executeMediaAction([](auto *session) {
        ComPtr<IAsyncOperation<bool> > actionOp;
        return session->TryTogglePlayPauseAsync(&actionOp);
    });
}

void MediaMonitor::stop() {
    executeMediaAction([](auto *session) {
        ComPtr<IAsyncOperation<bool> > actionOp;
        return session->TryStopAsync(&actionOp);
    });
}

void MediaMonitor::next() {
    executeMediaAction([](auto *session) {
        ComPtr<IAsyncOperation<bool> > actionOp;
        return session->TrySkipNextAsync(&actionOp);
    });
}

void MediaMonitor::prev() {
    executeMediaAction([](auto *session) {
        ComPtr<IAsyncOperation<bool> > actionOp;
        return session->TrySkipPreviousAsync(&actionOp);
    });
}

void MediaMonitor::jump(uint16_t time) {
    long long ticks = static_cast<long long>(time * 10000000LL);

    executeMediaAction([ticks](auto *session) {
        ComPtr<IAsyncOperation<bool> > actionOp;
        return session->TryChangePlaybackPositionAsync(ticks, &actionOp);
    });
}
