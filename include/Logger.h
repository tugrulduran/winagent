#pragma once
#include <QObject>
#include <QString>

class Logger : public QObject {
    Q_OBJECT
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    static void debug(const QString& msg, const bool bold = false) {
        emit instance().logMessage(msg, "#999", bold);
    }

    static void info(const QString& msg, const bool bold = false) {
        emit instance().logMessage(msg, "#1cb3fb", bold);
    }

    static void warn(const QString& msg, const bool bold = false) {
        emit instance().logMessage(msg, "#ebba34", bold);
    }

    static void success(const QString& msg, const bool bold = false) {
        emit instance().logMessage(msg, "#64d966", bold);
    }

    static void error(const QString& msg, const bool bold = false) {
        emit instance().logMessage(msg, "#ff3838", bold);
    }

    signals:
        void logMessage(const QString& msg, const QString& color, const bool bold = false);

private:
    Logger() = default;
};
