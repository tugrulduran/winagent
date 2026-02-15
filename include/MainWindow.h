#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QProcess>
#include <QThread>
#include <memory>
#include <vector>

#include "Dashboard.h"
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
    // Refresh the UI from the latest monitor data.
    void runMonitorCycle();

    // Clear the debug log widget.
    void clearLogs();

    // Start/stop the Node.js UDP server process.
    void toggleServer();

private:
    // Create all widgets, layouts, and signal/slot connections.
    void setupUI();
    void openDashboard();

    Dashboard &dashboard_ = Dashboard::instance();
    QUrl m_dashboardUrl;

    // tabs
    QTabWidget *tabWidget;
    QWidget *tabDashboard;
    QWidget *tabConfig;

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

    QTimer *m_reloadDataTimer{nullptr};
};

#endif
