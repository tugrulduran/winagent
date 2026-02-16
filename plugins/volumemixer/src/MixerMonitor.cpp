#include <shlwapi.h>
#include <algorithm>

#include <windows.h>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>

#include <QJsonArray>
#include "MixerMonitor.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

class ScopedCOM {
public:
    ScopedCOM() { initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
    ~ScopedCOM() { if (initialized) CoUninitialize(); }

private:
    bool initialized{false};
};

namespace volumemixer {
    void MixerMonitor::init(QJsonObject config) {
        const QJsonValue v = config.value("ignoredApps");
        ignoredApps.clear();
        if (!v.isArray()) {
            return;
        }

        const QJsonArray arr = v.toArray();
        for (const QJsonValue &item: arr) {
            if (!item.isString()) continue;

            const QString s = item.toString().trimmed();
            if (s.isEmpty()) continue;

            ignoredApps.insert(s.toStdWString());
        }
    }

    bool MixerMonitor::isIgnored(const std::wstring &name) {
        // Simple substring match against ignoreList.
        for (const auto &ignored: ignoredApps) {
            if (name.find(ignored) != std::wstring::npos) return true;
        }
        return false;
    }

    std::wstring MixerMonitor::getAppName(const std::wstring &filePath) {
        DWORD dummy;
        DWORD size = GetFileVersionInfoSizeW(filePath.c_str(), &dummy);
        if (size == 0) return L"";

        std::vector<BYTE> buffer(size);
        if (!GetFileVersionInfoW(filePath.c_str(), 0, size, &buffer[0])) return L"";

        struct LANGANDCODEPAGE {
            WORD wLanguage;
            WORD wCodePage;
        } *lpTranslate;
        UINT cbTranslate;

        if (VerQueryValueW(&buffer[0], L"\\VarFileInfo\\Translation", (LPVOID *) &lpTranslate, &cbTranslate)) {
            wchar_t subBlock[256];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\ProductName", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
            wchar_t *productName = nullptr;
            UINT productNameLen = 0;
            if (VerQueryValueW(&buffer[0], subBlock, (LPVOID *) &productName, &productNameLen) && productNameLen > 0)
                return productName;
        }
        return L"";
    }

    std::vector<MixerApp> MixerMonitor::update() {
        ScopedCOM com;
        std::vector<MixerApp> apps{};

        IMMDeviceEnumerator *pEnumerator = NULL;
        IMMDevice *pDevice = NULL;
        IAudioSessionManager2 *pSessionManager = NULL;
        IAudioSessionEnumerator *pSessionEnumerator = NULL;
        IAudioEndpointVolume *pEndpointVolume = NULL;

        CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **) &pEnumerator);
        if (!pEnumerator) return {};

        pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (!pDevice) {
            pEnumerator->Release();
            return {};
        }

        // Read master volume (system output volume).
        pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void **) &pEndpointVolume);
        if (pEndpointVolume) {
            MixerApp master{
                0,
                MIXER_APP_TYPE_MASTER,
                L"MASTER VOLUME",
            };
            pEndpointVolume->GetMasterVolumeLevelScalar(&master.volume);
            BOOL m;
            pEndpointVolume->GetMute(&m);
            master.muted = m;
            pEndpointVolume->Release();
            apps.push_back(master);
        }

        // Enumerate per-application audio sessions.
        pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void **) &pSessionManager);
        if (pSessionManager) {
            pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
            int sessionCount = 0;
            pSessionEnumerator->GetCount(&sessionCount);

            for (int i = 0; i < sessionCount; i++) {
                IAudioSessionControl *pSessionCtrl = NULL;
                IAudioSessionControl2 *pSessionCtrl2 = NULL;
                ISimpleAudioVolume *pVolCtrl = NULL;

                pSessionEnumerator->GetSession(i, &pSessionCtrl);
                pSessionCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void **) &pSessionCtrl2);
                pSessionCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void **) &pVolCtrl);

                DWORD pid = 0;
                pSessionCtrl2->GetProcessId(&pid);
                std::wstring finalName = L"System Sounds";

                // If PID is known, try to resolve EXE path and a friendly ProductName.
                if (pid != 0) {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                    if (hProc) {
                        wchar_t fullPath[MAX_PATH];
                        if (GetModuleFileNameExW(hProc, NULL, fullPath, MAX_PATH)) {
                            std::wstring friendly = getAppName(fullPath);
                            finalName = (!friendly.empty()) ? friendly : PathFindFileNameW(fullPath);
                        }
                        CloseHandle(hProc);
                    }
                }

                // Store session volume/mute unless this app is in the ignore list.
                if (!isIgnored(finalName)) {
                    float volume = 0;
                    BOOL muted = FALSE;
                    pVolCtrl->GetMasterVolume(&volume);
                    pVolCtrl->GetMute(&muted);
                    apps.push_back(MixerApp{
                        (uint32_t) pid,
                        MIXER_APP_TYPE_APP,
                        finalName,
                        volume,
                        (bool) muted
                    });
                }

                pVolCtrl->Release();
                pSessionCtrl2->Release();
                pSessionCtrl->Release();
            }
            pSessionEnumerator->Release();
            pSessionManager->Release();
        }


        pDevice->Release();
        pEnumerator->Release();

        return apps;
    }

    void MixerMonitor::setVolumeByPID(uint32_t targetPid, float newVolume) {
        ScopedCOM com;

        // Clamp volume into the expected range.
        if (newVolume < 0.0f) newVolume = 0.0f;
        if (newVolume > 1.0f) newVolume = 1.0f;

        // Find the audio session whose process id matches targetPid, then set its volume.
        IMMDeviceEnumerator *pEnumerator = NULL;
        IMMDevice *pDevice = NULL;
        IAudioSessionManager2 *pSessionManager = NULL;
        IAudioSessionEnumerator *pSessionEnumerator = NULL;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), (void **) &pEnumerator);
        if (FAILED(hr) || !pEnumerator) return;

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr) || !pDevice) {
            pEnumerator->Release();
            return;
        }

        pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void **) &pSessionManager);
        pSessionManager->GetSessionEnumerator(&pSessionEnumerator);

        int count = 0;
        pSessionEnumerator->GetCount(&count);

        for (int i = 0; i < count; i++) {
            IAudioSessionControl *pSessionControl = NULL;
            IAudioSessionControl2 *pSessionControl2 = NULL;

            pSessionEnumerator->GetSession(i, &pSessionControl);
            pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void **) &pSessionControl2);

            DWORD currentPid = 0;
            pSessionControl2->GetProcessId(&currentPid);

            // Match by numeric PID.
            if (currentPid == targetPid) {
                ISimpleAudioVolume *pVolumeControl = NULL;
                pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void **) &pVolumeControl);
                if (pVolumeControl) {
                    pVolumeControl->SetMasterVolume(newVolume, NULL);
                    pVolumeControl->Release();
                }

                // Target found; stop scanning sessions.
                pSessionControl2->Release();
                pSessionControl->Release();
                break;
            }

            pSessionControl2->Release();
            pSessionControl->Release();
        }

        if (pSessionEnumerator) pSessionEnumerator->Release();
        if (pSessionManager) pSessionManager->Release();

        if (pDevice) pDevice->Release();
        if (pEnumerator) pEnumerator->Release();
    }

    void MixerMonitor::setMasterVolume(float newVolume) {
        ScopedCOM com;

        // Clamp volume into the expected range.
        if (newVolume < 0.0f) newVolume = 0.0f;
        if (newVolume > 1.0f) newVolume = 1.0f;

        // Find the audio session whose process id matches targetPid, then set its volume.
        IMMDeviceEnumerator *pEnumerator = NULL;
        IMMDevice *pDevice = NULL;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), (void **) &pEnumerator);
        if (FAILED(hr) || !pEnumerator) return;

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr) || !pDevice) {
            pEnumerator->Release();
            return;
        }

        IAudioEndpointVolume *pEndpointVolume = NULL;
        pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void **) &pEndpointVolume);
        if (pEndpointVolume) {
            pEndpointVolume->SetMasterVolumeLevelScalar(newVolume, NULL);
            pEndpointVolume->Release();
        }

        if (pDevice) pDevice->Release();
        if (pEnumerator) pEnumerator->Release();
    }
}
