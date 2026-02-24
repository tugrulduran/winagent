#pragma once

#include <QJsonObject>
#include <QString>

namespace restartwatcher {

// Priority-ordered status codes (single value).
//
// 0: No restart required
// 1: Windows Update requires restart
// 2: Component-Based Servicing (CBS) pending restart
// 3: Pending file rename operations
// 4: Computer rename pending
// 5: Other/unknown restart pending

enum class RestartCode : int {
    None = 0,
    WindowsUpdate = 1,
    CbsPending = 2,
    PendingFileRename = 3,
    ComputerRename = 4,
    Other = 5
};

struct RestartStatus {
    RestartCode code = RestartCode::None;
};

// Coarser level for UI.
// 0: none, 1: recommended, 2: required
enum class RestartLevel : int {
    None = 0,
    Recommended = 1,
    Required = 2
};

// Detailed restart state.
// - `status` is kept as a priority-ordered single value for backward compatibility.
// - `level` and `flags` allow the dashboard to display richer information.
struct RestartReport {
    RestartCode status = RestartCode::None;
    RestartLevel level = RestartLevel::None;

    bool windowsUpdate = false;
    bool cbsPending = false;
    bool pendingFileRename = false;
    bool computerRename = false;
    bool updateExeVolatile = false;
};

class RestartWatcherSampler final {
public:
    void init(const QJsonObject& cfg);
    RestartReport readReport() const;

private:
#ifdef _WIN32
    static bool regKeyExists64(const wchar_t* subkey);
    static bool regValueMultiSzHasAnyEntry64(const wchar_t* subkey, const wchar_t* valueName);
    static bool regReadString64(const wchar_t* subkey, const wchar_t* valueName, QString& out);

    static bool hasWindowsUpdateReboot();
    static bool hasCbsRebootPending();
    static bool hasPendingFileRename();
    static bool hasComputerRenamePending();
    static bool hasUpdateExeVolatile();
#endif
};

} // namespace restartwatcher
