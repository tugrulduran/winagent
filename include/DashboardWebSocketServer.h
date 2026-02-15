#pragma once

#include <QWebSocketServer>
#include <QWebSocket>
#include <QSet>

class LauncherMonitor;
class AudioMonitor;
class MediaMonitor;
class PluginManager;

// Module Ids
enum ModuleId : uint8_t {
    MODULE_SYSTEM = 0xA1,
    MODULE_CPU = 0xA2,
    MODULE_MEDIA = 0xA3,
    MODULE_AUDIO = 0xA4,
    MODULE_PROCESS = 0xA5,
    MODULE_LAUNCHER = 0xA6
};

// Audio Module Commands
enum AudioCommand : uint8_t {
    AUDIO_CMD_TOGGLE_MUTE = 0xB1,
    AUDIO_CMD_SET_DEVICE = 0xB2,
    AUDIO_CMD_SET_APP_VOLUME = 0xB3,
};

// Launcher Commands
enum LauncherCommand : uint8_t {
    LAUNCHER_CMD_LAUNCH = 0xD1
};

// Media Module Commands
enum MediaCommand : uint8_t {
    MEDIA_CMD_PLAY = 0xC1,
    MEDIA_CMD_PAUSE = 0xC2,
    MEDIA_CMD_STOP = 0xC3,
    MEDIA_CMD_NEXT = 0xC4,
    MEDIA_CMD_PREV = 0xC5,
    MEDIA_CMD_JUMP = 0xC6,
    MEDIA_CMD_S_P10 = 0xC7,
    MEDIA_CMD_S_M10 = 0xC8,
    MEDIA_CMD_S_P30 = 0xC9,
    MEDIA_CMD_S_M30 = 0xD0,
};

class DashboardWebSocketServer : public QWebSocketServer {
    Q_OBJECT

public:
    explicit DashboardWebSocketServer(
        PluginManager *plugins,
        QObject *parent = nullptr
    );

    ~DashboardWebSocketServer() override;

public slots:
    void start();

    void stop();

    void broadcastJson();

signals:
    void clientConnected();

    void clientDisconnected();

    void messageReceived(const QString &message);

    void started();

    void stopped();

private slots:
    void onNewConnection();

    void onSocketDisconnected();

    void onTextMessageReceived(const QString &message);

    void broadcastTick(); // 500ms callback

private:
    QSet<QWebSocket *> m_clients;
    QTimer *m_broadcastTimer = nullptr;

    PluginManager *m_plugins;

    void handleEvent(const QString &event, const QJsonObject &data);

    // ---- Event handlers (stub) ----
    void handleSetAudioDevice(const QJsonObject &data);

    void handleRunAction(const QJsonObject &data);

    void handleMediaCtrl(const QJsonObject &data);

    void handleSetVolume(const QJsonObject &data);

    // Generic module request: {"module":"cpu", "payload":{...}}
    void handleModuleRequest(const QJsonObject &data);
};
