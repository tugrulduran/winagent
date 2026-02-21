#include "Launcher.h"

#include <algorithm>

#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <QBuffer>
#include <QByteArray>
#include <QFileIconProvider>
#include <QIcon>
#include <QImage>
#include <QPixmap>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <QDesktopServices>
#include <QUrl>
#endif

bool Launcher::init(const QJsonObject &config, QString &err) {
    err.clear();
    apps_.clear();

    const QJsonValue vApps = config.value("apps");
    if (!vApps.isUndefined() && !vApps.isArray()) {
        err = "config.apps is not an array";
        return false;
    }

    const QJsonArray arr = vApps.toArray();
    apps_.reserve((size_t) arr.size());

    for (int i = 0; i < arr.size(); ++i) {
        const QJsonValue item = arr.at(i);
        if (!item.isObject()) continue;
        const QJsonObject o = item.toObject();

        LauncherApp a;
        a.index = o.value("index").isDouble() ? o.value("index").toInt(i) : i;
        a.name = o.value("name").toString();
        a.path = o.value("path").toString();
        a.hash = o.value("hash").toString();
        a.zone = o.value("zone").toInt(1);
        a.order = o.value("order").toInt(1);

        if (a.name.trimmed().isEmpty() || a.path.trimmed().isEmpty()) continue;
        if (a.zone < 1) a.zone = 1;
        if (a.zone > 4) a.zone = 4;
        if (a.order < 1) a.order = 1;

        apps_.push_back(std::move(a));
    }

    std::stable_sort(apps_.begin(), apps_.end(), [](const LauncherApp &a, const LauncherApp &b) {
        if (a.zone != b.zone) return a.zone < b.zone;
        if (a.order != b.order) return a.order < b.order;
        return a.index < b.index;
    });

    return true;
}

static QJsonObject iconError(const QString &msg) {
    return QJsonObject{
        {"ok", false},
        {"error", msg}
    };
}

static bool encodePng64(const QImage &img, QByteArray &outPng) {
    outPng.clear();
    if (img.isNull()) return false;
    QBuffer buf(&outPng);
    buf.open(QIODevice::WriteOnly);
    return img.save(&buf, "PNG");
}

QJsonObject Launcher::getIconByName(const QString &name) const {
    const QString needle = name.trimmed();
    if (needle.isEmpty()) return iconError("missing name");

    const LauncherApp *hit = nullptr;
    for (const auto &a: apps_) {
        if (a.name == needle) {
            hit = &a;
            break;
        }
    }
    if (!hit) {
        for (const auto &a: apps_) {
            if (a.name.compare(needle, Qt::CaseInsensitive) == 0) {
                hit = &a;
                break;
            }
        }
    }
    if (!hit) return iconError("unknown app name");

    QString exePath = hit->path.trimmed();
    if (exePath.isEmpty()) return iconError("missing path");
    const QFileInfo fi(exePath);
    if (!fi.isAbsolute()) {
        const QString found = QStandardPaths::findExecutable(exePath);
        if (!found.isEmpty()) exePath = found;
    }

    const QFileInfo efi(exePath);
    if (!efi.exists()) return iconError("executable not found");

    QFileIconProvider provider;
    const QIcon ic = provider.icon(efi);
    if (ic.isNull()) return iconError("no icon available");

    QPixmap pm = ic.pixmap(64, 64);
    if (pm.isNull()) pm = ic.pixmap(48, 48);
    if (pm.isNull()) pm = ic.pixmap(32, 32);
    if (pm.isNull()) return iconError("failed to render icon");

    QImage img = pm.toImage();
    QByteArray png;
    if (!encodePng64(img, png)) return iconError("failed to encode png");

    return QJsonObject{
        {"event", "launcher_icon_update"},
        {"ok", true},
        {"name", hit->name},
        {"index", hit->index},
        {"mime", "image/png"},
        {"b64", QString::fromLatin1(png.toBase64())}
    };
}

QJsonObject Launcher::getIconByIndex(int index) const {
    if (index < 0) return iconError("bad index");
    for (const auto &a: apps_) {
        if (a.index == index) return getIconByName(a.name);
    }
    return iconError("unknown index");
}

QJsonObject Launcher::snapshot() const {
    QJsonArray apps;
    for (const auto &a: apps_) {
        apps.append(QJsonObject{
            {"index", a.index},
            {"name", a.name},
            {"hash", a.hash},
            {"zone", a.zone},
            {"order", a.order},
        });
    }

    return QJsonObject{
        {"ok", true},
        {"apps", apps},
    };
}

void Launcher::runAction(int index) {
    if (index < 0) return;
    const LauncherApp *hit = nullptr;
    for (const auto &a: apps_) {
        if (a.index == index) {
            hit = &a;
            break;
        }
    }
    if (!hit) return;

    const std::wstring path = hit->path.toStdWString();
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
