#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <QThread>
#include <memory>
#include <QLineEdit>
#include <QSystemTrayIcon>

#include "PluginManager.h"
#include "DashboardServer.h"
#include "DashboardWebSocketServer.h"

class MemoryMonitor;
class NetworkMonitor;
class AudioMonitor;
class MediaMonitor;
class ProcessMonitor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

    DashboardWebSocketServer *wsServer = nullptr;

public slots:
    void startDashboardServer();

    void stopDashboardServer();

signals:
    void dashboardServerStarted();

    void dashboardServerStopped();

    void dashboardServerError(const QString &msg);

protected:
    void closeEvent(QCloseEvent *event) override;

    void changeEvent(QEvent *event) override;

private slots:
    void clearLogs();

    void toggleServer();

    void copyAuthKey();

    void regenerateAuthKey();

private:
    // Create all widgets, layouts, and signal/slot connections.
    void setupUI();

    void openDashboard();

    void refreshPluginsTab();

    void setupTray();

    void showFromTray();

    void hideToTray();

    void showRunningNotificationOnce();

    bool writeSecretFile(const QString &s);

    void applySecretToWsServer();

    void updateSecretUi();

    QString authSecretPath() const;

    QString generate6DigitSecret() const;

    QString loadOrCreateSecret();

    QLineEdit *txtAuthKey = nullptr;
    QPushButton *btnCopyAuthKey = nullptr;
    QPushButton *btnRegenAuthKey = nullptr;

    QString m_authKey;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_actShowHide = nullptr;
    QAction *m_actOpenDashboard = nullptr;
    QAction *m_actQuit = nullptr;

    bool m_runningNotified = false;

    QUrl m_dashboardUrl;

    // tabs
    QTabWidget *tabWidget;
    QWidget *tabDashboard;
    QWidget *tabConfig;
    QWidget *tabPlugins;

    // plugins sub-tabs
    QTabWidget *tabPluginsInner;

    // buttons
    QPushButton *btnToggleServer;
    QPushButton *btnManualTrigger;
    QPushButton *btnListAudioDevices;
    QPushButton *btnClose;
    QPushButton *btnOpenDashboard;

    // labels
    QLabel *lblCpuLoad;
    QLabel *lblMemory;
    QLabel *lblNetwork;
    QLabel *lblMedia;
    QLabel *lblAudio;

    // debug
    QPlainTextEdit *txtDebug;

    // External plugin DLLs (loaded from <exe_dir>/plugins)
    PluginManager plugins_;


    QThread *m_DashboardServerThread{nullptr};
    DashboardServer *m_DashboardWebServer{nullptr};
    DashboardWebSocketServer *m_DashboardSocketServer{nullptr};
    std::atomic_bool m_serverRunning{false};
};

#endif
