#include "modules/MediaMonitor.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// WinRT Başlıkları
#include <windows.media.control.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.foundation.h>

#pragma comment(lib, "runtimeobject.lib")

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Media::Control;
using namespace ABI::Windows::Foundation;

MediaMonitor::MediaMonitor(int ms) : BaseMonitor(ms) {
    memset(&currentData, 0, sizeof(MediaPacket));
}

MediaMonitor::~MediaMonitor() {
    stop();
    RoUninitialize();
}

void MediaMonitor::init() {
    RoInitialize(RO_INIT_MULTITHREADED);
}

// --- Yardımcı Metot: Başlık Temizleme ---
void MediaMonitor::cleanTitle(std::wstring &title) {
    const std::wstring sfx[] = {L" - YouTube", L" - Spotify", L" - Google Chrome", L" - Microsoft Edge"};
    for (const auto &s: sfx) {
        size_t pos = title.find(s);
        if (pos != std::wstring::npos) title.erase(pos);
    }
}

// --- Yardımcı Metot: Zaman Formatlayıcı ---
std::string MediaMonitor::formatTime(uint32_t s) const {
    if (s == 0) return "00:00";
    uint32_t hrs = s / 3600;
    uint32_t mins = (s % 3600) / 60;
    uint32_t secs = s % 60;
    std::stringstream ss;
    if (hrs > 0) ss << hrs << ":" << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    else ss << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    return ss.str();
}

// --- Yardımcı Metot: Pencere Tarama (Fallback) ---
BOOL CALLBACK MediaMonitor::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    wchar_t buf[512];
    if (IsWindowVisible(hwnd) && GetWindowTextW(hwnd, buf, 512) > 0) {
        std::wstring ws(buf);
        MediaPacket *p = reinterpret_cast<MediaPacket *>(lParam);
        if (ws.find(L"YouTube") != std::wstring::npos || ws.find(L"Spotify") != std::wstring::npos) {
            if (wcslen(p->title) == 0) {
                wcsncpy(p->source, ws.find(L"YouTube") != std::wstring::npos ? L"YouTube" : L"Spotify", 15);
                wcsncpy(p->title, buf, 63);
            }
            return FALSE;
        }
    }
    return TRUE;
}

// --- Ana İzleme Metodu ---
void MediaMonitor::update() {
    static thread_local bool threadInit = false;
    if (!threadInit) {
        RoInitialize(RO_INIT_MULTITHREADED);
        threadInit = true;
    }

    MediaPacket newData;
    memset(&newData, 0, sizeof(MediaPacket));

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
                            newData.isPlaying = (
                                pStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus_Playing);
                        }
                        ComPtr<IGlobalSystemMediaTransportControlsSessionTimelineProperties> timeline;
                        if (SUCCEEDED(session->GetTimelineProperties(&timeline)) && timeline) {
                            TimeSpan pos, end;
                            DateTime lastUpdate;
                            if (SUCCEEDED(timeline->get_Position(&pos)) && SUCCEEDED(timeline->get_EndTime(&end)) &&
                                SUCCEEDED(timeline->get_LastUpdatedTime(&lastUpdate))) {
                                uint32_t totalSec = (uint32_t) (end.Duration / 10000000);
                                uint32_t posSec = (uint32_t) (pos.Duration / 10000000);
                                if (newData.isPlaying) {
                                    FILETIME nowFT;
                                    GetSystemTimeAsFileTime(&nowFT);
                                    ULARGE_INTEGER nowTicks, lastTicks;
                                    nowTicks.LowPart = nowFT.dwLowDateTime;
                                    nowTicks.HighPart = nowFT.dwHighDateTime;
                                    lastTicks.QuadPart = lastUpdate.UniversalTime;
                                    newData.currentTime =
                                            posSec + (uint32_t) ((nowTicks.QuadPart - lastTicks.QuadPart) / 10000000);
                                } else newData.currentTime = posSec;
                                if (newData.currentTime > totalSec) newData.currentTime = totalSec;
                                newData.totalTime = totalSec;
                            }
                        }
                    }
                }
            }
        }
    }
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&newData));
    std::wstring ts(newData.title);
    cleanTitle(ts);
    wcsncpy(newData.title, ts.c_str(), 63);

    std::lock_guard<std::mutex> lock(mediaMutex);
    currentData = newData;
}

// --- SENİN MEŞHUR MEDYA KOMUT METODUN (Geri Geldi) ---
void MediaMonitor::sendMediaCommand(int commandId) {
    HRESULT hrInit = RoInitialize(RO_INIT_MULTITHREADED);
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
                        ComPtr<IAsyncOperation<bool> > actionOp;
                        if (commandId == 1) {
                            session->TryTogglePlayPauseAsync(&actionOp);
                        } else if (commandId == 2) { session->TrySkipNextAsync(&actionOp); } else if (commandId == 3) {
                            session->TrySkipPreviousAsync(&actionOp);
                        } else if (commandId == 4 || commandId == 5) {
                            ComPtr<IGlobalSystemMediaTransportControlsSessionTimelineProperties> timeline;
                            if (SUCCEEDED(session->GetTimelineProperties(&timeline))) {
                                TimeSpan currentPos;
                                if (SUCCEEDED(timeline->get_Position(&currentPos))) {
                                    long long offset = 100000000LL; // 10 saniye (Windows ticks)
                                    if (commandId == 5) offset *= -1;

                                    long long requestedPos = currentPos.Duration + offset;
                                    if (requestedPos < 0) requestedPos = 0;

                                    // TryChangePlaybackPositionAsync INT64 bekler
                                    session->TryChangePlaybackPositionAsync(requestedPos, &actionOp);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (hrInit == S_OK || hrInit == S_FALSE) RoUninitialize();
}
