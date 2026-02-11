#include "MainWindow.h"

#include <iostream>
#include <QDateTime>
#include <QApplication>
#include <QScrollBar>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

#include "Logger.h"
#include "ModuleFactory.h"
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkMonitor.h"
#include "modules/AudioMonitor.h"
#include "modules/MediaMonitor.h"
#include "modules/AudioDeviceMonitor.h"

const QString btnServerOnStyle =
        "QPushButton { padding: 8px; background: #0a0; color: white; border: 1px solid #444; } QPushButton:hover { background: #a00; }";
const QString btnServerOffStyle =
        "QPushButton { padding: 8px; background: #a00; color: white; border: 1px solid #444; } QPushButton:hover { background: #0a0; }";

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), isServerRunning(false) {
    setupUI();

    monitors = ModuleFactory::createAll();
    for (auto &m: monitors) {
        m->start();
    }

    // Create the UDP reporter. It reads snapshots from monitors and sends them out.
    // Forward reporter log messages into the UI log widget.
    // Reporter runs on background threads, so we use invokeMethod with QueuedConnection
    // to safely update the UI from the Qt main thread.

    Logger::success(">>>> WinAgent Engine & Reporter Started.");

    m_DashboardServerThread.setObjectName("DashboardServerThread");

    // Timer that refreshes dashboard labels by pulling latest data from monitors.
    cycleTimer = new QTimer(this);
    connect(cycleTimer, &QTimer::timeout, this, &MainWindow::runMonitorCycle);
    cycleTimer->start(1000);

    connect(
        &Logger::instance(),
        &Logger::logMessage,
        this,
        [this](const QString &msg, const QString &color) {
            debugLog->appendHtml(
                QString("<span style='color:%1;'>%2</span>").arg(color, msg.toHtmlEscaped())
            );
        }, Qt::QueuedConnection);
}

MainWindow::~MainWindow() {
    stopDashboardServer();
    for (auto &m: monitors) m->stop();
}

void MainWindow::startDashboardServer() {
    if (m_serverRunning.load())
        return;

    m_DashboardServerThread.start();

    QMetaObject::invokeMethod(&m_DashboardServerThread, [this]() {
        auto *server = new DashboardServer();

        if (!server->listen(QHostAddress::AnyIPv4, 3003)) {
            Logger::error("DashboardServer listen failed");
            server->deleteLater();
            return;
        }

        m_DashboardServer = server;
        m_serverRunning.store(true);

        QThread *m_wsThread = nullptr;
        wsServer = new DashboardWebSocketServer(
            findMonitor<LauncherMonitor>(),
            findMonitor<AudioMonitor>(),
            findMonitor<MediaMonitor>(),
            findMonitor<AudioDeviceMonitor>(),
            nullptr
        );
        m_wsThread = new QThread(this);

        wsServer->moveToThread(m_wsThread);
        connect(m_wsThread, &QThread::started, this, [this]() {
            wsServer->start(3004);
        });
        connect(m_wsThread, &QThread::finished, wsServer, &QObject::deleteLater);
        m_wsThread->start();
    }, Qt::QueuedConnection);
}

void MainWindow::stopDashboardServer() {
    if (!m_serverRunning.load())
        return;

    QMetaObject::invokeMethod(&m_DashboardServerThread, [this]() {
        if (m_DashboardServer) {
            m_DashboardServer->close();
            m_DashboardServer->deleteLater();
            m_DashboardServer = nullptr;
        }
        wsServer->stop();

        m_serverRunning.store(false);
    }, Qt::QueuedConnection);

    m_DashboardServerThread.quit();
    m_DashboardServerThread.wait();
    Logger::error("DashboardServer stopped!");
}

void MainWindow::setupUI() {
    // Build the entire UI (widgets + layouts + signal/slot connections).
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    setWindowTitle("WinAgent Dashboard v2.1");
    resize(1100, 750);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Left side: dashboard + debug log
    QVBoxLayout *leftLayout = new QVBoxLayout();

    // Dashboard group (system status summary)
    QGroupBox *dashboardGroup = new QGroupBox("System Status");
    dashboardGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; color: #55aaff; border: 1px solid #333; margin-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    QVBoxLayout *dashLayout = new QVBoxLayout(dashboardGroup);

    QString labelStyle = "font-size: 14pt; font-weight: bold; color: #ffffff; padding: 5px;";
    lblCpu = new QLabel("CPU: %0.0");
    lblCpu->setStyleSheet(labelStyle);
    lblRam = new QLabel("RAM: 0.0 GB");
    lblRam->setStyleSheet(labelStyle);
    lblNet = new QLabel("Network: Ready");
    lblNet->setStyleSheet("font-size: 11pt; color: #aaa;");
    lblMedia = new QLabel("Media: Idle");
    lblMedia->setStyleSheet("font-size: 11pt; color: #55aaff;");
    lblAudioApps = new QLabel("Audio Channels: -");
    lblAudioApps->setStyleSheet("font-size: 10pt; color: #888;");

    // Foreground app display (icon + app name)
    lblAppIcon = new QLabel();
    lblAppIcon->setFixedSize(64, 64);
    lblAppIcon->setAlignment(Qt::AlignCenter);

    lblAppName = new QLabel("Active Application: -");
    lblAppName->setStyleSheet("font-size: 10pt; color: #00ff00; font-weight: bold;");

    dashLayout->addWidget(lblAppIcon);
    dashLayout->addWidget(lblAppName);

    dashLayout->addWidget(lblCpu);
    dashLayout->addWidget(lblRam);
    dashLayout->addWidget(lblNet);
    dashLayout->addWidget(lblMedia);
    dashLayout->addWidget(lblAudioApps);
    dashLayout->addStretch();

    // Debug log area (read-only text console)
    debugLog = new QPlainTextEdit();
    debugLog->setReadOnly(true);
    debugLog->setMaximumHeight(250);
    debugLog->setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: 'Consolas'; font-size: 9pt;");

    leftLayout->addWidget(dashboardGroup, 2);
    leftLayout->addWidget(new QLabel("System Logs:"), 0);
    leftLayout->addWidget(debugLog, 1);

    mainLayout->addLayout(leftLayout, 3);

    // Right side: control buttons + server status
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    QString btnStyle =
            "QPushButton { padding: 8px; background: #222; color: white; border: 1px solid #444; } QPushButton:hover { background: #333; }";

    QHBoxLayout *serverStatusLayout = new QHBoxLayout();
    btnServer = new QPushButton("Start Dashboard Server");
    btnServer->setStyleSheet(btnServerOffStyle);
    serverStatusLayout->addWidget(btnServer);
    buttonLayout->addLayout(serverStatusLayout);

    QPushButton *btnRefresh = new QPushButton("Manual Trigger");
    QPushButton *btnDevices = new QPushButton("List Audio Devices");
    QPushButton *btnClear = new QPushButton("Clear Logs");
    QPushButton *btnExit = new QPushButton("Close");

    btnRefresh->setStyleSheet(btnStyle);
    btnDevices->setStyleSheet(btnStyle);
    btnClear->setStyleSheet(btnStyle);
    btnExit->setStyleSheet("padding: 8px; background: #441111; color: white; border: 1px solid #662222;");

    buttonLayout->addWidget(btnRefresh);
    buttonLayout->addWidget(btnDevices);
    buttonLayout->addWidget(btnClear);
    buttonLayout->addStretch();
    buttonLayout->addWidget(btnExit);

    mainLayout->addLayout(buttonLayout, 1);

    // External server process (Node.js) + UI wiring
    serverProcess = new QProcess(this);
    connect(btnServer, &QPushButton::clicked, this, &MainWindow::toggleServer);

    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearLogs);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::runMonitorCycle);
    connect(btnExit, &QPushButton::clicked, qApp, &QApplication::quit);

    // Lists audio output devices in the debug log.
    connect(btnDevices, &QPushButton::clicked, this, [this]() {
        auto devices = dashboard_.data.audio.devices.snapshot();
        debugLog->appendHtml("<br><b style='color: #55aaff;'>--- ACTIVE AUDIO DEVICES ---</b>");
        for (const auto &d: devices) {
            debugLog->appendPlainText(
                QString("%1 [%2] %3").arg(d.isDefault ? ">>" : "  ").arg(d.index).arg(
                    QString::fromWCharArray(d.name.c_str())));
        }
    });
}

void MainWindow::closeEvent(QCloseEvent *event) {
    for (auto &m: monitors) m->stop();

    event->accept();
}

void MainWindow::toggleServer() {
    if (!m_serverRunning.load()) {
        startDashboardServer();
        btnServer->setText("Stop Dashboard Server");
        btnServer->setStyleSheet(btnServerOnStyle);
        debugLog->appendHtml(QString("<span style='color: #00ff00;'>[DASH] Server started</span>"));
    } else {
        stopDashboardServer();
        btnServer->setText("Start Dashboard Server");
        btnServer->setStyleSheet(btnServerOffStyle);
        debugLog->appendHtml(QString("<span style='color: #ff0000;'>[DASH] Server stopped</span>"));
    }
}

void MainWindow::runMonitorCycle() {
    // Pull the latest data from each monitor and update labels.
    // Each monitor returns a copy (thread-safe), so the UI never holds monitor locks.

    for (const auto &monitor: monitors) {
        if (auto m = dynamic_cast<CPUMonitor *>(monitor.get())) {
            auto cpuload = dashboard_.data.cpu.getLoad();
            lblCpu->setText(QString("CPU Usage: %1%").arg(cpuload, 0, 'f', 1));
        } else if (auto m = dynamic_cast<MemoryMonitor *>(monitor.get())) {
            auto totalMem = dashboard_.data.memory.getTotal() / 1024.0 / 1024.0 / 1024.0;
            auto usedMem = dashboard_.data.memory.getUsed() / 1024.0 / 1024.0 / 1024.0;
            auto percent = usedMem / totalMem;
            lblRam->setText(
                QString("Memory: %1 GB / %2 GB (%3%)").arg(usedMem, 0, 'f', 1).arg(totalMem, 0, 'f', 1).arg(percent));
        } else if (auto m = dynamic_cast<NetworkMonitor *>(monitor.get())) {
            // Build a short per-interface summary for the label.
            QString netS = "Network Traffic:\n";
            auto rxSpeed = dashboard_.data.network.getRxBytes() / 1024.0 / 1024.0;
            auto txSpeed = dashboard_.data.network.getTxBytes() / 1024.0 / 1024.0;
            lblNet->setText(
                QString("Network: DL: %1 MB/sec, UL: %2 MB/sec").arg(rxSpeed, 0, 'f', 2).arg(txSpeed, 0, 'f', 2));
        } else if (auto m = dynamic_cast<MediaMonitor *>(monitor.get())) {
            if (dashboard_.data.media.source != MEDIA_SOURCE_NO_MEDIA) {
                lblMedia->setText(QString("Media: %1").arg(QString::fromStdWString(dashboard_.data.media.title)));
            } else {
                lblMedia->setText("Media: Idle");
            }
        } else if (auto m = dynamic_cast<AudioMonitor *>(monitor.get())) {
            // Simple "active channel" count: how many apps currently have volume > 0.
            int active = 0;
            // for (auto &app : m->getData().apps) if (app.volume > 0) active++;
            lblAudioApps->setText(QString("Active Audio Channels: %1").arg(active));
        } else if (auto m = dynamic_cast<AudezeMonitor *>(monitor.get())) {
            // Simple "active channel" count: how many apps currently have volume > 0.
            int battery = dashboard_.data.audeze.getBattery();
            // for (auto &app : m->getData().apps) if (app.volume > 0) active++;
            lblAudioApps->setText(QString("Headset battery: %1").arg(battery));
        } else if (auto m = dynamic_cast<ProcessMonitor *>(monitor.get())) {
            // Show the foreground application and select an icon based on filename.
            std::string appName = ""; //m->getData().activeProcessName;
            lblAppName->setText(QString("Active: %1").arg(QString::fromStdString(appName)));

            // Icon selection rules (simple mapping).
            QString iconPath = ":/icons/default.png";

            if (appName == "xplane.exe") {
                iconPath = ":/icons/airplane.png";
            } else if (appName == "clion64.exe") {
                iconPath = ":/icons/coding.png";
            } else if (appName == "chrome.exe") {
                iconPath = ":/icons/web.png";
            }

            // Load and display the icon.
            QPixmap pix(iconPath);
            if (!pix.isNull()) {
                lblAppIcon->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    }

    // Keep the log view scrolled to the bottom.
    debugLog->verticalScrollBar()->setValue(debugLog->verticalScrollBar()->maximum());
}

void MainWindow::clearLogs() {
    // Clears the debug log widget.
    debugLog->clear();
    Logger::success(">>> Logs cleared.");
}
