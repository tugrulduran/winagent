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
#include <memory>
#include <vector>

#include "BaseMonitor.h"
#include "NetworkReporter.h"

class CPUMonitor;
class MemoryMonitor;
class NetworkBytes;
class AudioMonitor;
class MediaMonitor;
class ProcessMonitor;

/*
 * MainWindow
 * ----------
 * This is the main Qt UI window.
 *
 * Responsibilities:
 * - Build the dashboard UI (labels/buttons/log area).
 * - Start all monitor modules (CPU/RAM/Network/Audio/Media/Process).
 * - Start NetworkReporter (sends monitor data over UDP).
 * - Periodically pull data from monitors and update labels (runMonitorCycle()).
 * - Start/stop an external Node.js server process on demand.
 *
 * Important:
 * - UI strings are NOT changed by request.
 * - Only comments are being cleaned and rewritten in English.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Refresh the UI from the latest monitor data.
    void runMonitorCycle();

    // Clear the debug log widget.
    void clearLogs();

    // Start/stop the Node.js UDP server process.
    void toggleServer();

    // UI helpers for server state.
    void handleServerStarted();
    void handleServerStopped();

private:
    // Create all widgets, layouts, and signal/slot connections.
    void setupUI();

    // Best-effort cleanup: kill the process that is holding a UDP port (Windows).
    void cleanupPort(int port);

    // UI - Dashboard
    QLabel *lblCpu;
    QLabel *lblRam;
    QLabel *lblNet;
    QLabel *lblMedia;
    QLabel *lblAudioApps;

    // UI - Process (active foreground app info)
    QLabel *lblAppIcon;
    QLabel *lblAppName;

    // UI - Debug
    QPlainTextEdit *debugLog;
    QLabel *statusDot;
    QPushButton *btnServer;

    // External process (Node.js server)
    QProcess *serverProcess;
    bool isServerRunning = false;

    // Worker modules + UDP reporter
    std::vector<std::unique_ptr<BaseMonitor>> monitors;
    std::unique_ptr<NetworkReporter> reporter;

    // Triggers UI refresh periodically
    QTimer *cycleTimer;

    // Find a monitor by type (uses dynamic_cast).
    template<typename T>
    T* findMonitor() {
        for (auto& m : monitors) {
            if (auto ptr = dynamic_cast<T*>(m.get())) return ptr;
        }
        return nullptr;
    }

protected:
    // Called when the window is closing; used to stop threads/processes cleanly.
    void closeEvent(QCloseEvent *event) override;
};

#endif