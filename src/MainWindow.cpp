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
        "QPushButton { background: #0a0; color: white; } QPushButton:hover { background: #a00; }";
const QString btnServerOffStyle =
        "QPushButton { background: #a00; color: white; } QPushButton:hover { background: #0a0; }";

const bool autostart = true;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();

    connect(&Logger::instance(), &Logger::logMessage, this, [this](const QString &msg, const QString &color, const bool bold) {
        txtDebug->appendHtml(QString("<span style='color:%1; font-weight:%3;'>%2</span>").arg(color, msg.toHtmlEscaped(), QString::fromStdString(bold ? "bold" : "normal")));
    }, Qt::QueuedConnection);

    Logger::debug("[DEBUG] Creating monitors...");
    monitors = ModuleFactory::createAll();
    for (auto &m: monitors) { m->start(); }

    Logger::debug("[DEBUG] Creating servers...");
    m_DashboardServerThread = new QThread(this);
    m_DashboardWebServer = new DashboardServer();
    m_DashboardSocketServer = new DashboardWebSocketServer(
        findMonitor<LauncherMonitor>(),
        findMonitor<AudioMonitor>(),
        findMonitor<MediaMonitor>(),
        findMonitor<AudioDeviceMonitor>(),
        nullptr);
    m_DashboardWebServer->moveToThread(m_DashboardServerThread);
    m_DashboardSocketServer->moveToThread(m_DashboardServerThread);
    connect(m_DashboardServerThread, &QThread::finished, m_DashboardWebServer, &DashboardServer::deleteLater);
    connect(m_DashboardServerThread, &QThread::finished, m_DashboardSocketServer, &DashboardWebSocketServer::deleteLater);

    connect(m_DashboardWebServer, &DashboardServer::started, this, [this]() {
        m_serverRunning.store(true);
        btnToggleServer->setText("Stop Dashboard Server");
        btnToggleServer->setStyleSheet(btnServerOnStyle);
        Logger::success("[DASH] Server started");
    });

    connect(m_DashboardWebServer, &DashboardServer::stopped, this, [this]() {
        m_serverRunning.store(false);
        btnToggleServer->setText("Start Dashboard Server");
        btnToggleServer->setStyleSheet(btnServerOffStyle);
        Logger::error("[DASH] Server stopped");
    });

    m_DashboardServerThread->start();
    if (autostart) { startDashboardServer(); }

    m_reloadDataTimer = new QTimer(this);
    connect(m_reloadDataTimer, &QTimer::timeout, this, &MainWindow::runMonitorCycle);
    m_reloadDataTimer->start(1000);
}

void MainWindow::startDashboardServer() {
    QMetaObject::invokeMethod(m_DashboardWebServer, "start", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_DashboardSocketServer, "start", Qt::QueuedConnection);
}

void MainWindow::stopDashboardServer() {
    QMetaObject::invokeMethod(m_DashboardWebServer, "stop", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_DashboardSocketServer, "stop", Qt::QueuedConnection);
}

MainWindow::~MainWindow() {
    stopDashboardServer();

    if (m_DashboardServerThread->isRunning()) {
        m_DashboardServerThread->quit();
        m_DashboardServerThread->wait();
    }

    for (auto &m: monitors) { m->stop(); }
}

void MainWindow::toggleServer() {
    if (!m_serverRunning.load()) { startDashboardServer(); }
    else { stopDashboardServer(); }
}

void MainWindow::setupUI() {
    setWindowFlags(
        Qt::Window
        | Qt::CustomizeWindowHint
        | Qt::WindowMinimizeButtonHint
        | Qt::WindowCloseButtonHint
    );

    setWindowTitle("WinAgent Dashboard Server");
    resize(1200, 600);
    setFixedSize(1200, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    tabWidget = new QTabWidget(centralWidget);
    tabWidget->setGeometry(0, 0, 1200, 600);

    // =======================
    // Dashboard TAB
    // =======================
    tabDashboard = new QWidget();
    tabWidget->addTab(tabDashboard, "Dashboard");

    QFont bigFont("Prime", 28);
    QFont btnFont("Prime", 12);

    btnToggleServer = new QPushButton("Start Dashboard Server", tabDashboard);
    btnToggleServer->setGeometry(940, 10, 250, 51);
    btnToggleServer->setFont(btnFont);

    btnManualTrigger = new QPushButton("Manual Trigger", tabDashboard);
    btnManualTrigger->setGeometry(940, 70, 250, 51);
    btnManualTrigger->setFont(btnFont);

    btnListAudioDevices = new QPushButton("List Audio Devices", tabDashboard);
    btnListAudioDevices->setGeometry(940, 130, 250, 51);
    btnListAudioDevices->setFont(btnFont);

    btnClose = new QPushButton("Close", tabDashboard);
    btnClose->setGeometry(940, 250, 250, 51);
    btnClose->setFont(btnFont);

    txtDebug = new QPlainTextEdit(tabDashboard);
    txtDebug->setGeometry(0, 310, 1200, 260);
    txtDebug->setReadOnly(true);

    auto makeTitle = [&](const QString &text, int y) {
        QLabel *l = new QLabel(text, tabDashboard);
        l->setGeometry(10, y, 200, 50);
        l->setFont(bigFont);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };

    makeTitle("CPU Load:", 10);
    makeTitle("Memory:", 70);
    makeTitle("Network:", 130);
    makeTitle("Media:", 190);
    makeTitle("Audio:", 250);

    auto makeValue = [&](int y) {
        QLabel *l = new QLabel("-", tabDashboard);
        l->setGeometry(230, y, 600, 50);
        l->setFont(bigFont);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return l;
    };

    lblCpuLoad = makeValue(10);
    lblMemory = makeValue(70);
    lblNetwork = makeValue(130);
    lblMedia = makeValue(190);
    lblAudio = makeValue(250);

    // =======================
    // Config TAB
    // =======================
    tabConfig = new QWidget();
    tabWidget->addTab(tabConfig, "Configuration");

    QLabel *l = new QLabel("Not implemented yet", tabConfig);
    l->setGeometry(0, 0, 1200, 600);
    l->setFont(btnFont);
    l->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);


    // =======================
    // Connections
    // =======================
    connect(btnToggleServer, &QPushButton::clicked, this, &MainWindow::toggleServer);
    connect(btnManualTrigger, &QPushButton::clicked, this, &MainWindow::runMonitorCycle);
    connect(btnClose, &QPushButton::clicked, qApp, &QApplication::quit);

    connect(btnListAudioDevices, &QPushButton::clicked, this, [this]() {
        auto devices = dashboard_.data.audio.devices.snapshot();
        Logger::info("|||||||||| ACTIVE AUDIO DEVICES ||||||||||", true);
        for (const auto &d: devices) {
            Logger::info(QString("%1 [%2] %3")
                         .arg(d.isDefault ? " ✓ " : "")
                         .arg(d.index)
                         .arg(QString::fromWCharArray(d.name.c_str())
                         ), d.isDefault);
        }
    });
}

void MainWindow::runMonitorCycle() {
    // Pull the latest data from each monitor and update labels.
    // Each monitor returns a copy (thread-safe), so the UI never holds monitor locks.

    for (const auto &monitor: monitors) {
        if (auto m = dynamic_cast<CPUMonitor *>(monitor.get())) {
            auto cpuload = dashboard_.data.cpu.getLoad();
            auto cpucores = dashboard_.data.cpu.getCores();
            lblCpuLoad->setText(QString("%1% (%2 cores)")
                .arg(cpuload, 0, 'f', 1)
                .arg(cpucores));
        }
        else if (auto m = dynamic_cast<MemoryMonitor *>(monitor.get())) {
            auto totalMem = dashboard_.data.memory.getTotal() / 1024.0 / 1024.0 / 1024.0;
            auto usedMem = dashboard_.data.memory.getUsed() / 1024.0 / 1024.0 / 1024.0;
            auto percent = usedMem / totalMem;
            lblMemory->setText(QString("%1 / %2 GB (%3%)")
                .arg(usedMem, 0, 'f', 0)
                .arg(totalMem, 0, 'f', 0)
                .arg(percent * 100, 0, 'f', 0)
            );
        }
        else if (auto m = dynamic_cast<NetworkMonitor *>(monitor.get())) {
            // Build a short per-interface summary for the label.
            auto rxSpeed = dashboard_.data.network.getRxBytes() / 1024.0 / 1024.0;
            auto txSpeed = dashboard_.data.network.getTxBytes() / 1024.0 / 1024.0;
            lblNetwork->setText(QString("DL: %1, UL: %2 MB/sec")
                .arg(rxSpeed, 0, 'f', 2)
                .arg(txSpeed, 0, 'f', 2)
            );
        }
        else if (auto m = dynamic_cast<MediaMonitor *>(monitor.get())) {
            if (dashboard_.data.media.source != MEDIA_SOURCE_NO_MEDIA) {
                lblMedia->setText(QString("%1").arg(QString::fromStdWString(dashboard_.data.media.title)));
            }
            else {
                lblMedia->setText("Idle");
            }
        }
        /*
        else if (auto m = dynamic_cast<AudioMonitor *>(monitor.get())) {
            // Simple "active channel" count: how many apps currently have volume > 0.
            int active = 0;
            // for (auto &app : m->getData().apps) if (app.volume > 0) active++;
            lblAudioApps->setText(QString("Active Audio Channels: %1").arg(active));
        }
        else if (auto m = dynamic_cast<AudezeMonitor *>(monitor.get())) {
            // Simple "active channel" count: how many apps currently have volume > 0.
            int battery = dashboard_.data.audeze.getBattery();
            // for (auto &app : m->getData().apps) if (app.volume > 0) active++;
            lblAudioApps->setText(QString("Headset battery: %1").arg(battery));
        }
        else if (auto m = dynamic_cast<ProcessMonitor *>(monitor.get())) {
            // Show the foreground application and select an icon based on filename.
            std::string appName = ""; //m->getData().activeProcessName;
            lblAppName->setText(QString("Active: %1").arg(QString::fromStdString(appName)));

            // Icon selection rules (simple mapping).
            QString iconPath = ":/icons/default.png";

            if (appName == "xplane.exe") {
                iconPath = ":/icons/airplane.png";
            }
            else if (appName == "clion64.exe") {
                iconPath = ":/icons/coding.png";
            }
            else if (appName == "chrome.exe") {
                iconPath = ":/icons/web.png";
            }

            // Load and display the icon.
            QPixmap pix(iconPath);
            if (!pix.isNull()) {
                lblAppIcon->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    */
    }
}

void MainWindow::clearLogs() {
    // Clears the debug log widget.
    txtDebug->clear();
    Logger::success(">>> Logs cleared.");
}
