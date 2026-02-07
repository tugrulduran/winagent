#include "MainWindow.h"

#include <iostream>
#include <QDateTime>
#include <QApplication>
#include <QScrollBar>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "ModuleFactory.h"
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "modules/AudioMonitor.h"
#include "modules/MediaMonitor.h"
#include "modules/AudioDeviceSwitcher.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), isServerRunning(false) {
    setupUI();

    // Create and start all monitor modules (each runs in its own worker thread).
    monitors = ModuleFactory::createAll();
    for (auto& m : monitors) {
        m->start();
    }

    // Create the UDP reporter. It reads snapshots from monitors and sends them out.
    reporter = std::make_unique<NetworkReporter>(
        "127.0.0.1", 5000, 1000,
        findMonitor<CPUMonitor>(),
        findMonitor<MemoryMonitor>(),
        findMonitor<NetworkBytes>(),
        findMonitor<AudioMonitor>(),
        findMonitor<MediaMonitor>()
    );

    // Forward reporter log messages into the UI log widget.
    // Reporter runs on background threads, so we use invokeMethod with QueuedConnection
    // to safely update the UI from the Qt main thread.
    reporter->setLogCallback([this](const std::string& msg) {
        QMetaObject::invokeMethod(this, [this, msg](){
            debugLog->appendHtml(QString("<span style='color: #ffff00;'>%1</span>").arg(QString::fromStdString(msg)));
        }, Qt::QueuedConnection);
    });

    reporter->start();

    debugLog->appendPlainText(">>> WinAgent Engine & Reporter Başlatıldı.");

    // Timer that refreshes dashboard labels by pulling latest data from monitors.
    cycleTimer = new QTimer(this);
    connect(cycleTimer, &QTimer::timeout, this, &MainWindow::runMonitorCycle);
    cycleTimer->start(2000);
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
    QGroupBox *dashboardGroup = new QGroupBox("Sistem Durumu");
    dashboardGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #55aaff; border: 1px solid #333; margin-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    QVBoxLayout *dashLayout = new QVBoxLayout(dashboardGroup);

    QString labelStyle = "font-size: 14pt; font-weight: bold; color: #ffffff; padding: 5px;";
    lblCpu = new QLabel("CPU: %0.0"); lblCpu->setStyleSheet(labelStyle);
    lblRam = new QLabel("RAM: 0.0 GB"); lblRam->setStyleSheet(labelStyle);
    lblNet = new QLabel("Ağ: Hazır"); lblNet->setStyleSheet("font-size: 11pt; color: #aaa;");
    lblMedia = new QLabel("Medya: Boşta"); lblMedia->setStyleSheet("font-size: 11pt; color: #55aaff;");
    lblAudioApps = new QLabel("Ses Kanalları: -"); lblAudioApps->setStyleSheet("font-size: 10pt; color: #888;");

    // Foreground app display (icon + app name)
    lblAppIcon = new QLabel();
    lblAppIcon->setFixedSize(64, 64);
    lblAppIcon->setAlignment(Qt::AlignCenter);

    lblAppName = new QLabel("Aktif Uygulama: -");
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
    leftLayout->addWidget(new QLabel("Sistem Logları:"), 0);
    leftLayout->addWidget(debugLog, 1);

    mainLayout->addLayout(leftLayout, 3);

    // Right side: control buttons + server status
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    QString btnStyle = "QPushButton { padding: 8px; background: #222; color: white; border: 1px solid #444; } QPushButton:hover { background: #333; }";

    QHBoxLayout *serverStatusLayout = new QHBoxLayout();
    statusDot = new QLabel();
    statusDot->setFixedSize(12, 12);
    statusDot->setStyleSheet("background-color: red; border-radius: 6px;");
    btnServer = new QPushButton("UDP Server Başlat");
    btnServer->setStyleSheet(btnStyle);
    serverStatusLayout->addWidget(statusDot);
    serverStatusLayout->addWidget(btnServer);
    buttonLayout->addLayout(serverStatusLayout);

    QPushButton *btnRefresh = new QPushButton("Manuel Tetikle");
    QPushButton *btnDevices = new QPushButton("Ses Cihazlarını Listele");
    QPushButton *btnClear = new QPushButton("Logları Temizle");
    QPushButton *btnExit = new QPushButton("Kapat");

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
    connect(serverProcess, &QProcess::started, this, &MainWindow::handleServerStarted);
    connect(serverProcess, &QProcess::finished, this, &MainWindow::handleServerStopped);

    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearLogs);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::runMonitorCycle);
    connect(btnExit, &QPushButton::clicked, qApp, &QApplication::quit);

    // Lists audio output devices in the debug log.
    connect(btnDevices, &QPushButton::clicked, this, [this](){
        auto devices = AudioDeviceSwitcher::listDevices();
        debugLog->appendHtml("<br><b style='color: #55aaff;'>--- AKTİF SES CİHAZLARI ---</b>");
        for(const auto& d : devices) {
            debugLog->appendPlainText(QString("%1 [%2] %3").arg(d.isDefault ? ">>" : "  ").arg(d.index).arg(QString::fromWCharArray(d.name.c_str())));
        }
    });
}

void MainWindow::cleanupPort(int port) {
    // Uses PowerShell to find a process owning a UDP endpoint and stops it.
    // This avoids parsing netstat output (which can vary across systems/locales).
    QString command = "powershell";
    QStringList args;
    args << "-NoProfile" << "-Command"
         << QString("$p = Get-NetUDPEndpoint -LocalPort %1 -ErrorAction SilentlyContinue; "
                    "if($p) { Stop-Process -Id $p.OwningProcess -Force -ErrorAction SilentlyContinue }").arg(port);

    QProcess::startDetached(command, args);

    debugLog->appendHtml(QString("<i style='color: orange;'>[SYSTEM] Port %1 temizleniyor...</i>").arg(port));
}

void MainWindow::toggleServer() {
    // Starts/stops the external Node.js server and keeps UI state in sync.
    if (!isServerRunning) {
        // Ask Windows to free the UDP port first (best-effort).
        cleanupPort(5000);

        // Give the OS a short moment to actually kill the owning process.
        QTimer::singleShot(2500, this, [this]() {
            // Build the script path relative to the application directory.
            QString scriptPath = QCoreApplication::applicationDirPath() + "/../../nodedash/agentserver.js";

            // Set working directory so node can resolve relative paths correctly.
            QDir scriptDir(QFileInfo(scriptPath).absolutePath());
            serverProcess->setWorkingDirectory(scriptDir.absolutePath());

            // Start Node. The script is referenced by filename because working directory is set.
            serverProcess->start("node.exe", QStringList() << "agentserver.js");

            debugLog->appendHtml("<i style='color: gray;'>[DEBUG] Sunucu başlatıldı.</i>");
        });
    } else {
        // Stop the process, then free the port again just in case.
        serverProcess->kill();
        serverProcess->waitForFinished(1000);
        cleanupPort(5000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Clean shutdown:
    // - stop server process
    // - free UDP port
    // - stop reporter and all monitor threads

    debugLog->appendPlainText(">>> Uygulama kapatılıyor, temizlik yapılıyor...");

    if (serverProcess && serverProcess->state() != QProcess::NotRunning) {
        serverProcess->kill();
        serverProcess->waitForFinished(1000);
    }

    cleanupPort(5000);

    if (reporter) reporter->stop();
    for (auto& m : monitors) m->stop();

    event->accept();
}

void MainWindow::handleServerStarted() {
    // Update UI when Node.js server process starts.
    isServerRunning = true;
    btnServer->setText("UDP Server Durdur");
    statusDot->setStyleSheet("background-color: #00ff00; border-radius: 6px;");
    debugLog->appendHtml("<b style='color: #00ff00;'>[SERVER] Node.js UDP Server Aktif.</b>");
}

void MainWindow::handleServerStopped() {
    // Update UI when Node.js server process stops.
    isServerRunning = false;
    btnServer->setText("UDP Server Başlat");
    statusDot->setStyleSheet("background-color: red; border-radius: 6px;");
    debugLog->appendHtml("<b style='color: red;'>[SERVER] UDP Server Durduruldu.</b>");
}

void MainWindow::runMonitorCycle() {
    // Pull the latest data from each monitor and update labels.
    // Each monitor returns a copy (thread-safe), so the UI never holds monitor locks.

    for (const auto& monitor : monitors) {
        if (auto m = dynamic_cast<CPUMonitor*>(monitor.get())) {
            lblCpu->setText(QString("CPU Kullanımı: %1%").arg(m->getData().load, 0, 'f', 1));
        }
        else if (auto m = dynamic_cast<MemoryMonitor*>(monitor.get())) {
            auto d = m->getData();
            lblRam->setText(QString("Bellek: %1 GB / %2 GB (%3%)").arg(d.usedGB, 0, 'f', 2).arg(d.totalGB, 0, 'f', 2).arg(d.usagePercentage));
        }
        else if (auto m = dynamic_cast<NetworkBytes*>(monitor.get())) {
            // Build a short per-interface summary for the label.
            QString netS = "Ağ Trafiği:\n";
            for (const auto& iface : m->getData()) {
                if (iface.speedInKB > 0.1) {
                    netS += QString(" • %1: ↓%2 KB/s\n")
                                .arg(QString::fromWCharArray(iface.description.c_str()).left(20))
                                .arg(iface.speedInKB, 0, 'f', 1);
                }
            }
            lblNet->setText(netS);
        }
        else if (auto m = dynamic_cast<MediaMonitor*>(monitor.get())) {
            auto media = m->getData();
            lblMedia->setText(wcslen(media.title) > 0 ? QString("Medya: %1").arg(QString::fromWCharArray(media.title)) : "Medya: Boşta");
        }
        else if (auto m = dynamic_cast<AudioMonitor*>(monitor.get())) {
            // Simple "active channel" count: how many apps currently have volume > 0.
            int active = 0;
            for (auto &app : m->getData().apps) if (app.volume > 0) active++;
            lblAudioApps->setText(QString("Aktif Ses Kanalı: %1").arg(active));
        }
        else if (auto m = dynamic_cast<ProcessMonitor*>(monitor.get())) {
            // Show the foreground application and select an icon based on filename.
            std::string appName = m->getData().activeProcessName;
            lblAppName->setText(QString("Aktif: %1").arg(QString::fromStdString(appName)));

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
    debugLog->appendPlainText(">>> Loglar temizlendi.");
}

MainWindow::~MainWindow() {
    // Destructor is a last safety net to stop background work.
    if (serverProcess) { serverProcess->kill(); serverProcess->waitForFinished(); }
    if (reporter) reporter->stop();
    for (auto& m : monitors) m->stop();
}