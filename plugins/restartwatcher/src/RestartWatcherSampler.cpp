#include "RestartWatcherSampler.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winreg.h>
#endif

#include <vector>

namespace restartwatcher {

void RestartWatcherSampler::init(const QJsonObject& cfg) {
    Q_UNUSED(cfg);
}

RestartReport RestartWatcherSampler::readReport() const {
    RestartReport out;

#ifdef _WIN32
    out.windowsUpdate = hasWindowsUpdateReboot();
    out.cbsPending = hasCbsRebootPending();
    out.pendingFileRename = hasPendingFileRename();
    out.computerRename = hasComputerRenamePending();
    out.updateExeVolatile = hasUpdateExeVolatile();

    // Primary status is still a single priority-ordered code.
    if (out.windowsUpdate) {
        out.status = RestartCode::WindowsUpdate;
    } else if (out.cbsPending) {
        out.status = RestartCode::CbsPending;
    } else if (out.pendingFileRename) {
        out.status = RestartCode::PendingFileRename;
    } else if (out.computerRename) {
        out.status = RestartCode::ComputerRename;
    } else if (out.updateExeVolatile) {
        out.status = RestartCode::Other;
    } else {
        out.status = RestartCode::None;
    }

    // Level: any of these markers means "restart required" in practice.
    const bool required = out.windowsUpdate || out.cbsPending || out.pendingFileRename || out.updateExeVolatile;
    if (required) {
        out.level = RestartLevel::Required;
    } else if (out.computerRename) {
        out.level = RestartLevel::Recommended;
    } else {
        out.level = RestartLevel::None;
    }
#else
    out.status = RestartCode::None;
    out.level = RestartLevel::None;
#endif

    return out;
}

#ifdef _WIN32

static constexpr REGSAM kRead64 = KEY_READ | KEY_WOW64_64KEY;

bool RestartWatcherSampler::regKeyExists64(const wchar_t* subkey) {
    HKEY hKey = nullptr;
    const LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, kRead64, &hKey);
    if (r == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool RestartWatcherSampler::regReadString64(const wchar_t* subkey, const wchar_t* valueName, QString& out) {
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, kRead64, &hKey);
    if (r != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    r = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &size);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || size < sizeof(wchar_t)) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buf((size / sizeof(wchar_t)) + 1, 0);
    r = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buf.data()), &size);
    RegCloseKey(hKey);
    if (r != ERROR_SUCCESS) return false;

    out = QString::fromWCharArray(buf.data()).trimmed();
    return true;
}

bool RestartWatcherSampler::regValueMultiSzHasAnyEntry64(const wchar_t* subkey, const wchar_t* valueName) {
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, kRead64, &hKey);
    if (r != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    r = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &size);
    if (r != ERROR_SUCCESS || type != REG_MULTI_SZ || size < 2 * sizeof(wchar_t)) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buf((size / sizeof(wchar_t)) + 2, 0);
    r = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buf.data()), &size);
    RegCloseKey(hKey);
    if (r != ERROR_SUCCESS) return false;

    // Empty MULTI_SZ is just "\0\0".
    // If there is any non-null character before the terminal double-null, treat as non-empty.
    for (size_t i = 0; i < buf.size(); ++i) {
        if (buf[i] != L'\0') {
            return true;
        }
    }
    return false;
}

bool RestartWatcherSampler::hasWindowsUpdateReboot() {
    // Common WU reboot pending marker.
    if (regKeyExists64(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired")) {
        return true;
    }
    // Some systems use this naming.
    if (regKeyExists64(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootPending")) {
        return true;
    }
    return false;
}

bool RestartWatcherSampler::hasCbsRebootPending() {
    // Component Based Servicing pending reboot.
    if (regKeyExists64(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending")) {
        return true;
    }
    if (regKeyExists64(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootInProgress")) {
        return true;
    }
    return false;
}

bool RestartWatcherSampler::hasPendingFileRename() {
    // Pending file rename operations (requires reboot to complete).
    if (regValueMultiSzHasAnyEntry64(L"SYSTEM\\CurrentControlSet\\Control\\Session Manager", L"PendingFileRenameOperations")) {
        return true;
    }
    if (regValueMultiSzHasAnyEntry64(L"SYSTEM\\CurrentControlSet\\Control\\Session Manager", L"PendingFileRenameOperations2")) {
        return true;
    }
    return false;
}

bool RestartWatcherSampler::hasComputerRenamePending() {
    QString active;
    QString pending;
    const bool okActive = regReadString64(L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName", L"ComputerName", active);
    const bool okPending = regReadString64(L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName", L"ComputerName", pending);
    if (!okActive || !okPending) return false;

    return active.compare(pending, Qt::CaseInsensitive) != 0;
}

bool RestartWatcherSampler::hasUpdateExeVolatile() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Updates", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD val = 0;
    DWORD sz = sizeof(val);
    const LONG r = RegQueryValueExW(hKey, L"UpdateExeVolatile", nullptr, &type, reinterpret_cast<LPBYTE>(&val), &sz);
    RegCloseKey(hKey);
    if (r != ERROR_SUCCESS) return false;
    return (type == REG_DWORD && val != 0);
}

#endif // _WIN32

} // namespace restartwatcher
