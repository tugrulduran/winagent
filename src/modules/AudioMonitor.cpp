#include "modules/AudioMonitor.h"
#include <iostream>
#include <psapi.h>
#include <iomanip>
#include <shlwapi.h>

// Linker için pragmalar (Eğer CMake'e eklemediysen buradan da bağlayabilir)
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

AudioMonitor::AudioMonitor(int interval) : BaseMonitor(interval) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

AudioMonitor::~AudioMonitor() {
    CoUninitialize();
}

bool AudioMonitor::IsIgnored(const std::wstring& name) {
    // Listede tam eşleşme veya kısmi eşleşme kontrolü
    for (const auto& ignored : ignoreList) {
        if (name.find(ignored) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

std::wstring AudioMonitor::GetFriendlyName(const std::wstring &filePath) {
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
        for (unsigned int i = 0; i < (cbTranslate / sizeof(LANGANDCODEPAGE)); i++) {
            wchar_t subBlock[256];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\ProductName",
                       lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);

            wchar_t *productName = nullptr;
            UINT productNameLen = 0;
            if (VerQueryValueW(&buffer[0], subBlock, (LPVOID *) &productName, &productNameLen) && productNameLen > 0) {
                return productName;
            }
        }
    }
    return L"";
}

void AudioMonitor::update() {
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioSessionManager2 *pSessionManager = NULL;
    IAudioSessionEnumerator *pSessionEnumerator = NULL;
    IAudioEndpointVolume *pEndpointVolume = NULL;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                     (void **) &pEnumerator);
    if (!pEnumerator) return;

    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (!pDevice) {
        pEnumerator->Release();
        return;
    }

    // 1. MASTER VOLUME OKUMA
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void **) &pEndpointVolume);
    if (pEndpointVolume) {
        pEndpointVolume->GetMasterVolumeLevelScalar(&masterVolume);
        BOOL m;
        pEndpointVolume->GetMute(&m);
        masterMuted = m;
        pEndpointVolume->Release();
    }

    // 2. SESSION ENUMERATION
    pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void **) &pSessionManager);
    pSessionManager->GetSessionEnumerator(&pSessionEnumerator);

    int sessionCount = 0;
    pSessionEnumerator->GetCount(&sessionCount);
    std::vector<AppAudioData> currentApps;

    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl *pSessionControl = NULL;
        IAudioSessionControl2 *pSessionControl2 = NULL;
        ISimpleAudioVolume *pVolumeControl = NULL;

        pSessionEnumerator->GetSession(i, &pSessionControl);
        pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void **) &pSessionControl2);
        pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void **) &pVolumeControl);

        // İsimlendirme Stratejisi
        wchar_t *dName = nullptr;
        pSessionControl2->GetDisplayName(&dName);
        std::wstring finalName = (dName && wcslen(dName) > 0) ? dName : L"";
        CoTaskMemFree(dName);

        DWORD pid = 0;
        pSessionControl2->GetProcessId(&pid);
        if (pid != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcess) {
                wchar_t fullPath[MAX_PATH];
                if (GetModuleFileNameExW(hProcess, NULL, fullPath, MAX_PATH)) {
                    std::wstring friendly = GetFriendlyName(fullPath);
                    if (!friendly.empty()) finalName = friendly;
                    else if (finalName.empty() || finalName.find(L".exe") != std::wstring::npos) {
                        finalName = PathFindFileNameW(fullPath);
                    }
                }
                CloseHandle(hProcess);
            }
        }

        if (finalName.empty() || finalName[0] == L'@') finalName = L"System Sounds";

        float vol = 0;
        BOOL mute = FALSE;
        pVolumeControl->GetMasterVolume(&vol);
        pVolumeControl->GetMute(&mute);

        // --- BURAYA EKLE: PAKETLEME ---
        AudioPacket packet;
        packet.pid = (uint32_t)pid;
        packet.volume = vol;

        // Belleği sıfırla (Çöp karakterleri önlemek için)
        memset(packet.name, 0, sizeof(packet.name));

        // finalName (wstring) içeriğini packet.name (wchar_t dizisi) içine kopyala
        // 24 karakter sınırı koyuyoruz (25. karakter null kalsın diye)
        wcsncpy(packet.name, finalName.c_str(), 24);

        // Görünür sesleri ekle
        if (vol > 0.0f || finalName != L"System Sounds") {
            currentApps.push_back({(uint32_t)pid, finalName, vol, (bool) mute});
        }

        pVolumeControl->Release();
        pSessionControl2->Release();
        pSessionControl->Release();
    } {
        std::lock_guard<std::mutex> lock(dataMutex);
        apps = currentApps;
    }

    if (pSessionEnumerator) pSessionEnumerator->Release();
    if (pSessionManager) pSessionManager->Release();
    pDevice->Release();
    pEnumerator->Release();
}

void AudioMonitor::display() const {
    std::lock_guard<std::mutex> lock(dataMutex);

    std::wcout << L"[VOL] --- MASTER VOLUME --- : %" << (int) (masterVolume * 100);
    if (masterMuted) std::wcout << L" [Muted]";
    std::wcout << std::endl;

    for (const auto &app: apps) {
        std::wcout << L"[VOL] " << std::left << std::setw(25) << app.name.substr(0, 24)
                << L"(" << app.pid << L") "
                << L": %" << (int) (app.volume * 100) << std::endl;
    }
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

