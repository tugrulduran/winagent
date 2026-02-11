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

    static void debug(const QString& msg) {
        emit instance().logMessage(msg, "#999");
    }

    static void info(const QString& msg) {
        emit instance().logMessage(msg, "#1cb3fb");
    }

    static void warn(const QString& msg) {
        emit instance().logMessage(msg, "#ebba34");
    }

    static void success(const QString& msg) {
        emit instance().logMessage(msg, "#64d966");
    }

    static void error(const QString& msg) {
        emit instance().logMessage(msg, "#ff3838");
    }

    signals:
        void logMessage(const QString& msg, const QString& color);

private:
    Logger() = default;
};
