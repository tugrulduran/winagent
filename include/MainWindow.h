#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox> // <--- EKLENDİ
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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void runMonitorCycle();
    void clearLogs();
    void toggleServer();
    void handleServerStarted();
    void handleServerStopped();

private:
    void setupUI();
    void cleanupPort(int port); // Portu işgal eden süreci bulup öldürür

    // UI - Dashboard
    QLabel *lblCpu;
    QLabel *lblRam;
    QLabel *lblNet;
    QLabel *lblMedia;
    QLabel *lblAudioApps;

    // UI - Process
    QLabel *lblAppIcon; // İkonu göstereceğimiz yer
    QLabel *lblAppName; // Uygulama ismini yazacağımız yer

    // UI - Debug
    QPlainTextEdit *debugLog; // logArea yerine artık bu kullanılıyor
    QLabel *statusDot;
    QPushButton *btnServer;

    QProcess *serverProcess;
    bool isServerRunning = false;

    std::vector<std::unique_ptr<BaseMonitor>> monitors;
    std::unique_ptr<NetworkReporter> reporter;
    QTimer *cycleTimer;

    template<typename T>
    T* findMonitor() {
        for (auto& m : monitors) {
            if (auto ptr = dynamic_cast<T*>(m.get())) return ptr;
        }
        return nullptr;
    }
protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif