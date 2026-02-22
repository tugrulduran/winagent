#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600 // Vista+

#include "StorageSampler.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <dbt.h>
#include <winioctl.h>
#include <initguid.h>
#include <devguid.h>

#include <chrono>
#include <algorithm>
#include <string>
#include <vector>

namespace storage {

static std::wstring driveHandlePath(wchar_t letter) {
    std::wstring p = L"\\\\.\\";
    p.push_back(letter);
    p.push_back(L':');
    return p;
}

static bool getPhysicalDeviceNumber(wchar_t letter, DWORD& outDeviceNumber) {
    outDeviceNumber = 0;
    const std::wstring vol = driveHandlePath(letter);

    HANDLE h = CreateFileW(vol.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    STORAGE_DEVICE_NUMBER sdn{};
    DWORD bytes = 0;
    const BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                   nullptr, 0,
                                   &sdn, sizeof(sdn),
                                   &bytes, nullptr);
    CloseHandle(h);

    if (!ok) return false;
    outDeviceNumber = sdn.DeviceNumber;
    return true;
}

struct PhysInfo {
    STORAGE_BUS_TYPE bus = BusTypeUnknown;
    bool busKnown = false;
    bool seekPenaltyKnown = false;
    bool incursSeekPenalty = false;
};

static PhysInfo queryPhysicalInfo(DWORD physDeviceNumber) {
    PhysInfo info;

    std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(physDeviceNumber);
    HANDLE h = CreateFileW(path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return info;
    }

    // Bus type
    {
        STORAGE_PROPERTY_QUERY q{};
        q.PropertyId = StorageDeviceProperty;
        q.QueryType = PropertyStandardQuery;

        BYTE buffer[1024];
        DWORD bytes = 0;
        if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                            &q, sizeof(q),
                            buffer, sizeof(buffer),
                            &bytes, nullptr)) {
            if (bytes >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
                auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
                info.bus = desc->BusType;
                info.busKnown = true;
            }
        }
    }

    // Seek penalty (SSD vs HDD heuristic)
    {
        STORAGE_PROPERTY_QUERY q{};
        q.PropertyId = StorageDeviceSeekPenaltyProperty;
        q.QueryType = PropertyStandardQuery;

        DEVICE_SEEK_PENALTY_DESCRIPTOR d{};
        DWORD bytes = 0;
        if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                            &q, sizeof(q),
                            &d, sizeof(d),
                            &bytes, nullptr)) {
            if (bytes >= sizeof(d)) {
                info.seekPenaltyKnown = true;
                info.incursSeekPenalty = d.IncursSeekPenalty ? true : false;
            }
        }
    }

    CloseHandle(h);
    return info;
}

static std::wstring driveTypeToString(UINT driveType, wchar_t letter) {
    if (driveType == DRIVE_REMOTE) return L"Network";
    if (driveType == DRIVE_CDROM)  return L"CDROM";
    if (driveType == DRIVE_RAMDISK) return L"RAM";
    if (driveType == DRIVE_NO_ROOT_DIR) return L"Missing";
    if (driveType == DRIVE_UNKNOWN) return L"Unknown";

    // For FIXED/REMOVABLE, try to identify USB and SSD/HDD.
    DWORD devNum = 0;
    if (!getPhysicalDeviceNumber(letter, devNum)) {
        return (driveType == DRIVE_REMOVABLE) ? L"USB" : L"Fixed";
    }

    const PhysInfo phys = queryPhysicalInfo(devNum);

    // Prefer bus classification
    if (phys.busKnown) {
        if (phys.bus == BusTypeUsb) return L"USB";
        if (phys.bus == BusTypeVirtual) return L"Virtual";
    }

    if (driveType == DRIVE_REMOVABLE) {
        return L"USB";
    }

    if (phys.seekPenaltyKnown) {
        return phys.incursSeekPenalty ? L"HDD" : L"SSD";
    }

    return L"Fixed";
}

struct ScopedErrorMode {
    UINT old = 0;
    explicit ScopedErrorMode(UINT mode) { old = SetErrorMode(mode); }
    ~ScopedErrorMode() { SetErrorMode(old); }
};

std::vector<StorageDisk> StorageSampler::enumerateDisks() {
    std::vector<StorageDisk> out;

    wchar_t drivesBuf[4096];
    const DWORD n = GetLogicalDriveStringsW(static_cast<DWORD>(sizeof(drivesBuf)/sizeof(drivesBuf[0])), drivesBuf);
    if (n == 0 || n > (sizeof(drivesBuf)/sizeof(drivesBuf[0]))) {
        return out;
    }

    // drivesBuf is a MULTI_SZ list: "C:\\0D:\\0...0\0"
    for (const wchar_t* p = drivesBuf; *p; ) {
        std::wstring root = p; // e.g. "C:\\"
        p += root.size() + 1;

        if (root.size() < 2) continue;
        const wchar_t letter = root[0];

        StorageDisk d;
        d.drive = std::wstring(1, letter) + L":";

        const UINT dt = GetDriveTypeW(root.c_str());
        d.type = driveTypeToString(dt, letter);

        // Volume label + filesystem
        wchar_t volName[MAX_PATH] = {0};
        wchar_t fsName[MAX_PATH] = {0};
        DWORD serial = 0;
        DWORD maxCompLen = 0;
        DWORD fsFlags = 0;

        const ScopedErrorMode em(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

        const BOOL volOk = GetVolumeInformationW(
            root.c_str(),
            volName, MAX_PATH,
            &serial,
            &maxCompLen,
            &fsFlags,
            fsName, MAX_PATH
        );

        if (volOk) {
            d.label = volName;
            d.fileSystem = fsName;
        }

        // Space
        ULARGE_INTEGER freeAvail{};
        ULARGE_INTEGER total{};
        ULARGE_INTEGER freeTotal{};
        const BOOL spaceOk = GetDiskFreeSpaceExW(root.c_str(), &freeAvail, &total, &freeTotal);
        if (spaceOk) {
            d.totalBytes = static_cast<uint64_t>(total.QuadPart);
            d.freeBytes = static_cast<uint64_t>(freeTotal.QuadPart);
        }

        d.ready = (volOk && spaceOk);
        out.push_back(std::move(d));
    }

    // Stable sort by drive letter
    std::sort(out.begin(), out.end(), [](const StorageDisk& a, const StorageDisk& b) {
        return a.drive < b.drive;
    });

    return out;
}

void StorageSampler::init(const QJsonObject& cfg) {
    Q_UNUSED(cfg);
    ensureStarted();
    // Make sure we have an initial snapshot immediately.
    refreshNow();
}

std::vector<StorageDisk> StorageSampler::snapshot() {
    ensureStarted();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!last_.empty()) return last_;
    }

    // Fallback: synchronous refresh if cache is empty.
    refreshNow();

    std::lock_guard<std::mutex> lk(mtx_);
    return last_;
}

void StorageSampler::refreshNow() {
    const auto disks = enumerateDisks();
    std::lock_guard<std::mutex> lk(mtx_);
    last_ = disks;
}

void StorageSampler::ensureStarted() {
    if (started_) return;

    refreshEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr); // auto-reset
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);     // manual-reset

    watcher_ = std::thread([this]() { watcherThreadMain(); });
    started_ = true;
}

void StorageSampler::shutdown() {
    if (!started_) return;

    if (stopEvent_) {
        SetEvent(stopEvent_);
    }

    // Wake waiter (MsgWaitForMultipleObjects) so shutdown is fast.
    if (refreshEvent_) {
        SetEvent(refreshEvent_);
    }


    if (watcher_.joinable()) {
        watcher_.join();
    }

    if (refreshEvent_) { CloseHandle(refreshEvent_); refreshEvent_ = nullptr; }
    if (stopEvent_) { CloseHandle(stopEvent_); stopEvent_ = nullptr; }

    started_ = false;
}

LRESULT CALLBACK StorageSampler::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    auto* self = reinterpret_cast<StorageSampler*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_DEVICECHANGE: {
            if (!self || !self->refreshEvent_) break;
            if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE || wParam == DBT_DEVNODES_CHANGED) {
                SetEvent(self->refreshEvent_);
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void StorageSampler::watcherThreadMain() {
    HINSTANCE hinst = GetModuleHandleW(nullptr);

    const wchar_t* kClassName = L"WinAgentStorageWatcherWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = StorageSampler::WndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kClassName;

    RegisterClassW(&wc); // if already registered, it's fine

    hwnd_ = CreateWindowExW(0, kClassName, L"", 0,
                            0, 0, 0, 0,
                            HWND_MESSAGE,
                            nullptr,
                            hinst,
                            this);

    if (hwnd_) {
        DEV_BROADCAST_DEVICEINTERFACE_W filter{};
        filter.dbcc_size = sizeof(filter);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        filter.dbcc_classguid = GUID_DEVINTERFACE_VOLUME;

        devNotify_ = RegisterDeviceNotificationW(hwnd_, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    }

    // First refresh.
    refreshNow();

    HANDLE handles[2] = { refreshEvent_, stopEvent_ };

    while (true) {
        DWORD r = MsgWaitForMultipleObjects(2, handles, FALSE, 60000, QS_ALLINPUT);

        if (r == WAIT_OBJECT_0) {
            // refreshEvent_
            refreshNow();
            continue;
        }
        if (r == WAIT_OBJECT_0 + 1) {
            break; // stop
        }
        if (r == WAIT_OBJECT_0 + 2) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            continue;
        }
        if (r == WAIT_TIMEOUT) {
            refreshNow();
            continue;
        }

        break;
    }

    if (devNotify_) {
        UnregisterDeviceNotification(devNotify_);
        devNotify_ = nullptr;
    }

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

} // namespace storage

#else

#include "StorageSampler.h"

namespace storage {

std::vector<StorageDisk> StorageSampler::enumerateDisks() { return {}; }
void StorageSampler::init(const QJsonObject& cfg) { Q_UNUSED(cfg); }
void StorageSampler::shutdown() {}
std::vector<StorageDisk> StorageSampler::snapshot() { return {}; }

} // namespace storage

#endif
