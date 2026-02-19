#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <QThread>
#include <memory>

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
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

    DashboardWebSocketServer *wsServer = nullptr;

public slots:
    void startDashboardServer();

    void stopDashboardServer();

signals:
    void dashboardServerStarted();

    void dashboardServerStopped();

    void dashboardServerError(const QString &msg);

private slots:
    void clearLogs();

    void toggleServer();

private:
    // Create all widgets, layouts, and signal/slot connections.
    void setupUI();
    void openDashboard();
    void refreshPluginsTab();

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
    QPushButton* btnOpenDashboard;

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
