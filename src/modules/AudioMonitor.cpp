#include "modules/AudioMonitor.h"
#include <psapi.h>
#include <shlwapi.h>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

AudioMonitor::AudioMonitor(int interval) : BaseMonitor(interval) {}

AudioMonitor::~AudioMonitor() {
    stop();
    CoUninitialize();
}

void AudioMonitor::init() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

bool AudioMonitor::IsIgnored(const std::wstring& name) {
    for (const auto& ignored : ignoreList) {
        if (name.find(ignored) != std::wstring::npos) return true;
    }
    return false;
}

std::wstring AudioMonitor::GetFriendlyName(const std::wstring &filePath) {
    DWORD dummy;
    DWORD size = GetFileVersionInfoSizeW(filePath.c_str(), &dummy);
    if (size == 0) return L"";

    std::vector<BYTE> buffer(size);
    if (!GetFileVersionInfoW(filePath.c_str(), 0, size, &buffer[0])) return L"";

    struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; } *lpTranslate;
    UINT cbTranslate;

    if (VerQueryValueW(&buffer[0], L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate)) {
        wchar_t subBlock[256];
        swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\ProductName", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
        wchar_t *productName = nullptr;
        UINT productNameLen = 0;
        if (VerQueryValueW(&buffer[0], subBlock, (LPVOID*)&productName, &productNameLen) && productNameLen > 0)
            return productName;
    }
    return L"";
}

void AudioMonitor::update() {
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioSessionManager2 *pSessionManager = NULL;
    IAudioSessionEnumerator *pSessionEnumerator = NULL;
    IAudioEndpointVolume *pEndpointVolume = NULL;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (!pEnumerator) return;

    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (!pDevice) { pEnumerator->Release(); return; }

    AudioSnapshot newSnapshot;

    // Master Volume
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
    if (pEndpointVolume) {
        pEndpointVolume->GetMasterVolumeLevelScalar(&newSnapshot.masterVolume);
        BOOL m; pEndpointVolume->GetMute(&m);
        newSnapshot.masterMuted = m;
        pEndpointVolume->Release();
    }

    // App Sessions
    pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
    if (pSessionManager) {
        pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
        int sessionCount = 0;
        pSessionEnumerator->GetCount(&sessionCount);

        for (int i = 0; i < sessionCount; i++) {
            IAudioSessionControl *pSessionCtrl = NULL;
            IAudioSessionControl2 *pSessionCtrl2 = NULL;
            ISimpleAudioVolume *pVolCtrl = NULL;

            pSessionEnumerator->GetSession(i, &pSessionCtrl);
            pSessionCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionCtrl2);
            pSessionCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolCtrl);

            DWORD pid = 0;
            pSessionCtrl2->GetProcessId(&pid);
            std::wstring finalName = L"System Sounds";

            if (pid != 0) {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProc) {
                    wchar_t fullPath[MAX_PATH];
                    if (GetModuleFileNameExW(hProc, NULL, fullPath, MAX_PATH)) {
                        std::wstring friendly = GetFriendlyName(fullPath);
                        finalName = (!friendly.empty()) ? friendly : PathFindFileNameW(fullPath);
                    }
                    CloseHandle(hProc);
                }
            }

            if (!IsIgnored(finalName)) {
                float vol = 0; BOOL mute = FALSE;
                pVolCtrl->GetMasterVolume(&vol);
                pVolCtrl->GetMute(&mute);
                newSnapshot.apps.push_back({(uint32_t)pid, finalName, vol, (bool)mute});
            }

            pVolCtrl->Release(); pSessionCtrl2->Release(); pSessionCtrl->Release();
        }
        pSessionEnumerator->Release();
        pSessionManager->Release();
    }

    std::lock_guard<std::mutex> lock(dataMutex);
    currentSnapshot = std::move(newSnapshot);

    pDevice->Release();
    pEnumerator->Release();
}

AudioSnapshot AudioMonitor::getData() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return currentSnapshot;
}

void AudioMonitor::setVolumeByPID(uint32_t targetPid, float newVolume) {
    if (newVolume < 0.0f) newVolume = 0.0f;
    if (newVolume > 1.0f) newVolume = 1.0f;

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioSessionManager2* pSessionManager = NULL;
    IAudioSessionEnumerator* pSessionEnumerator = NULL;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
    pSessionManager->GetSessionEnumerator(&pSessionEnumerator);

    int count = 0;
    pSessionEnumerator->GetCount(&count);

    for (int i = 0; i < count; i++) {
        IAudioSessionControl* pSessionControl = NULL;
        IAudioSessionControl2* pSessionControl2 = NULL;

        pSessionEnumerator->GetSession(i, &pSessionControl);
        pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);

        DWORD currentPid = 0;
        pSessionControl2->GetProcessId(&currentPid);

        if (currentPid == targetPid) { // Sayısal ID Karşılaştırması
            ISimpleAudioVolume* pVolumeControl = NULL;
            pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolumeControl);
            if (pVolumeControl) {
                pVolumeControl->SetMasterVolume(newVolume, NULL);
                pVolumeControl->Release();
            }
            // Hedefi bulduk, döngüden çıkabiliriz
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
