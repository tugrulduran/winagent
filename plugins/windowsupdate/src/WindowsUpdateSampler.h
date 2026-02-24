#pragma once

#include <QString>
#include <QJsonObject>

#include <vector>

namespace windowsupdate {

struct UpdateInfo {
    QString id;
    QString title;
    // "critical" | "important" | "optional" | "other"
    QString importance;

    // "software" | "driver" | "other"
    QString type;
};

class WindowsUpdateSampler final {
public:
    void init(const QJsonObject& cfg);

    // Scans available (not installed, not hidden) updates.
    // Returns true on success; false on failure (err filled).
    bool scan(std::vector<UpdateInfo>& outUpdates, QString& err);

private:
    static QString bstrToQString(const wchar_t* bstr);
};

} // namespace windowsupdate
