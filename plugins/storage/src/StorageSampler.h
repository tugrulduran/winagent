#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <QJsonObject>

#ifdef _WIN32
#include <windows.h>
#endif

struct StorageDisk {
    std::wstring drive;      // e.g. "C:"
    std::wstring type;       // e.g. "SSD", "HDD", "USB", "Network", "CDROM"
    std::wstring label;      // Volume label
    std::wstring fileSystem; // e.g. "NTFS", "exFAT"

    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    bool ready = false;      // false if queries failed (e.g. disconnected network drive)
};

namespace storage {

class StorageSampler {
public:
    void init(const QJsonObject& cfg);
    void shutdown();

    // Returns the last known snapshot quickly. A background watcher keeps this updated.
    std::vector<StorageDisk> snapshot();

    // Synchronous enumeration (used internally by the watcher).
    static std::vector<StorageDisk> enumerateDisks();

private:
    void ensureStarted();
    void refreshNow();

#ifdef _WIN32
    void watcherThreadMain();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HANDLE refreshEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    HWND hwnd_ = nullptr;
    HDEVNOTIFY devNotify_ = nullptr;
    std::thread watcher_;
#endif

    std::mutex mtx_;
    std::vector<StorageDisk> last_;
    bool started_ = false;
};

} // namespace storage
