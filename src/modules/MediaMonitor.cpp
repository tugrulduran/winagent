#include "modules/MediaMonitor.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// MSVC / Windows SDK WinRT Başlıkları
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
    // Ana thread için WinRT başlatma
    RoInitialize(RO_INIT_MULTITHREADED);
}

MediaMonitor::~MediaMonitor() {
    RoUninitialize();
}

void MediaMonitor::cleanTitle(std::wstring& title) {
    const std::wstring sfx[] = { L" - YouTube", L" - Spotify", L" - Google Chrome", L" - Microsoft Edge" };
    for (const auto& s : sfx) {
        size_t pos = title.find(s);
        if (pos != std::wstring::npos) title.erase(pos);
    }
}

// Yeni Saat Destekli Zaman Formatlayıcı (H:MM:SS)
std::string MediaMonitor::formatTime(uint32_t s) const {
    if (s == 0) return "00:00";
    if (s > 360000) return "--:--";

    uint32_t hrs = s / 3600;
    uint32_t mins = (s % 3600) / 60;
    uint32_t secs = s % 60;

    std::stringstream ss;
    if (hrs > 0) {
        ss << hrs << ":" << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    } else {
        ss << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    }
    return ss.str();
}

BOOL CALLBACK MediaMonitor::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    wchar_t buf[512];
    if (IsWindowVisible(hwnd) && GetWindowTextW(hwnd, buf, 512) > 0) {
        std::wstring ws(buf);
        MediaPacket* p = reinterpret_cast<MediaPacket*>(lParam);
        if (ws.find(L"YouTube") != std::wstring::npos || ws.find(L"Spotify") != std::wstring::npos) {
            if (wcslen(p->title) == 0) {
                wcsncpy(p->source, ws.find(L"YouTube") != std::wstring::npos ? L"YouTube" : L"Spotify", 16);
                wcsncpy(p->title, buf, 64);
            }
            return FALSE;
        }
    }
    return TRUE;
}

void MediaMonitor::update() {
    static thread_local bool threadInit = false;
    if (!threadInit) { RoInitialize(RO_INIT_MULTITHREADED); threadInit = true; }

    MediaPacket newData;
    memset(&newData, 0, sizeof(MediaPacket));

    ComPtr<IGlobalSystemMediaTransportControlsSessionManagerStatics> managerStatics;
    HRESULT hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager).Get(),
        __uuidof(IGlobalSystemMediaTransportControlsSessionManagerStatics),
        &managerStatics
    );

    if (SUCCEEDED(hr)) {
        ComPtr<IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager*>> op;
        if (SUCCEEDED(managerStatics->RequestAsync(&op))) {
            AsyncStatus status;
            ComPtr<IAsyncInfo> info;
            op.As(&info);
            do { info->get_Status(&status); if (status == AsyncStatus::Started) Sleep(5); } while (status == AsyncStatus::Started);

            if (status == AsyncStatus::Completed) {
                ComPtr<IGlobalSystemMediaTransportControlsSessionManager> manager;
                if (SUCCEEDED(op->GetResults(&manager)) && manager) {
                    ComPtr<IGlobalSystemMediaTransportControlsSession> session;
                    if (SUCCEEDED(manager->GetCurrentSession(&session)) && session) {

                        // Oynatma Durumu
                        ComPtr<IGlobalSystemMediaTransportControlsSessionPlaybackInfo> playback;
                        if (SUCCEEDED(session->GetPlaybackInfo(&playback)) && playback) {
                            GlobalSystemMediaTransportControlsSessionPlaybackStatus pStatus;
                            if (SUCCEEDED(playback->get_PlaybackStatus(&pStatus))) {
                                newData.isPlaying = (pStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus_Playing);
                            }
                        }

                        // Zaman Hesaplama (Interpolation - Saniyelerin akmasını sağlar)
                        ComPtr<IGlobalSystemMediaTransportControlsSessionTimelineProperties> timeline;
                        if (SUCCEEDED(session->GetTimelineProperties(&timeline)) && timeline) {
                            TimeSpan pos, end;
                            DateTime lastUpdate;
                            if (SUCCEEDED(timeline->get_Position(&pos)) &&
                                SUCCEEDED(timeline->get_EndTime(&end)) &&
                                SUCCEEDED(timeline->get_LastUpdatedTime(&lastUpdate)))
                            {
                                uint32_t totalSec = (uint32_t)(end.Duration / 10000000);
                                uint32_t posSec = (uint32_t)(pos.Duration / 10000000);

                                if (newData.isPlaying) {
                                    FILETIME nowFT;
                                    GetSystemTimeAsFileTime(&nowFT);
                                    ULARGE_INTEGER nowTicks, lastTicks;
                                    nowTicks.LowPart = nowFT.dwLowDateTime;
                                    nowTicks.HighPart = nowFT.dwHighDateTime;
                                    lastTicks.QuadPart = lastUpdate.UniversalTime;

                                    // Son güncellemeden bu yana geçen saniye farkını ekle
                                    uint64_t diffSec = (nowTicks.QuadPart - lastTicks.QuadPart) / 10000000;
                                    newData.currentTime = posSec + (uint32_t)diffSec;
                                } else {
                                    newData.currentTime = posSec;
                                }

                                if (newData.currentTime > totalSec) newData.currentTime = totalSec;
                                newData.totalTime = totalSec;
                            }
                        }
                    }
                }
            }
        }
    }

    // Başlık ve Kaynak için fallback
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&newData));
    std::wstring ts(newData.title); cleanTitle(ts); wcsncpy(newData.title, ts.c_str(), 64);

    std::lock_guard<std::mutex> lock(mediaMutex);
    currentData = newData;
}

void MediaMonitor::display() const {
    MediaPacket d = getData();
    if (wcslen(d.title) > 0) {
        char t8[256], s8[64];
        WideCharToMultiByte(CP_UTF8, 0, d.title, -1, t8, 256, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, d.source, -1, s8, 64, NULL, NULL);

        std::cout << "[MEDIA] " << std::left << std::setw(8) << s8 << ": "
                  << std::setw(40) << t8
                  << " [" << formatTime(d.currentTime) << " / " << formatTime(d.totalTime) << "]"
                  << (d.isPlaying ? " (PLAYING)" : " (PAUSED)") << std::endl;
    }
}

// ... önceki kısımlar aynı (update, display vb.) ...

void MediaMonitor::sendMediaCommand(int commandId) {
    // Statik başlatma (Thread güvenliği için)
    HRESULT hrInit = RoInitialize(RO_INIT_MULTITHREADED);

    ComPtr<IGlobalSystemMediaTransportControlsSessionManagerStatics> managerStatics;
    if (SUCCEEDED(RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager).Get(),
        __uuidof(IGlobalSystemMediaTransportControlsSessionManagerStatics),
        &managerStatics)))
    {
        ComPtr<IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager*>> op;
        if (SUCCEEDED(managerStatics->RequestAsync(&op))) {

            // --- KRİTİK: Manager'ın gelmesini bekle ---
            AsyncStatus status;
            ComPtr<IAsyncInfo> info;
            op.As(&info);

            // İstek tamamlanana kadar kısa döngüde bekle (polling)
            do {
                info->get_Status(&status);
                if (status == AsyncStatus::Started) Sleep(5);
            } while (status == AsyncStatus::Started);

            if (status == AsyncStatus::Completed) {
                ComPtr<IGlobalSystemMediaTransportControlsSessionManager> manager;
                if (SUCCEEDED(op->GetResults(&manager)) && manager) {
                    ComPtr<IGlobalSystemMediaTransportControlsSession> session;
                    if (SUCCEEDED(manager->GetCurrentSession(&session)) && session) {

                        ComPtr<IAsyncOperation<bool>> actionOp;

                        // Komut 1: Play/Pause Toggle
                        if (commandId == 1) {
                            session->TryTogglePlayPauseAsync(&actionOp);
                        }
                        // Komut 2: Next
                        else if (commandId == 2) {
                            session->TrySkipNextAsync(&actionOp);
                        }
                        // Komut 3: Previous
                        else if (commandId == 3) {
                            session->TrySkipPreviousAsync(&actionOp);
                        }
                        // Komut 4-5: İleri / Geri Sarma (10sn)
                        else if (commandId == 4 || commandId == 5) {
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

                        // Not: actionOp'un sonucunu beklemeye gerek yok,
                        // "fire and forget" (tetikle ve geç) mantığı komutlar için yeterlidir.
                    }
                }
            }
        }
    }

    // Eğer RoInitialize biz yaptıysak uninitialize edelim
    if (hrInit == S_OK || hrInit == S_FALSE) RoUninitialize();
}
