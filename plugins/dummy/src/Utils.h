#pragma once
#include <QString>
#include <QJsonObject>
#include <QDateTime>

namespace dummy {

    // ISO8601 UTC timestamp
    inline QString isoUtcNow() {
        return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    // Safely read double from JSON (supports int/double), fallback
    inline double jsonDouble(const QJsonObject& o, const char* key, double defVal) {
        const auto v = o.value(key);
        if (v.isDouble()) return v.toDouble();
        return defVal;
    }

    inline int jsonInt(const QJsonObject& o, const char* key, int defVal) {
        const auto v = o.value(key);
        if (v.isDouble()) return v.toInt(defVal);
        return defVal;
    }

    inline QString jsonString(const QJsonObject& o, const char* key, const QString& defVal) {
        const auto v = o.value(key);
        if (v.isString()) return v.toString();
        return defVal;
    }

} // namespace dummy
