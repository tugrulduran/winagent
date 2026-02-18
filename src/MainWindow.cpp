#include "MainWindow.h"

#include <iostream>
#include <QApplication>
#include <QCoreApplication>
#include <QScrollBar>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QTabWidget>
#include <QLabel>

#include "Logger.h"

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

    // Load external plugins from: <exe_dir>/plugins
    const QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    plugins_.loadFromDir(pluginDir, plugins_.hostApi());
    refreshPluginsTab();

    Logger::debug("[DEBUG] Creating servers...");
    m_DashboardServerThread = new QThread(this);
    m_DashboardWebServer = new DashboardServer();
    m_DashboardSocketServer = new DashboardWebSocketServer(
        &plugins_,
        nullptr);
    m_DashboardWebServer->moveToThread(m_DashboardServerThread);
    m_DashboardSocketServer->moveToThread(m_DashboardServerThread);
    connect(m_DashboardServerThread, &QThread::finished, m_DashboardWebServer, &DashboardServer::deleteLater);
    connect(m_DashboardServerThread, &QThread::finished, m_DashboardSocketServer, &DashboardWebSocketServer::deleteLater);

    connect(m_DashboardWebServer, &DashboardServer::started, this, [this](const QString &url) {
        m_serverRunning.store(true);
        btnToggleServer->setText("Stop Dashboard Server");
        btnToggleServer->setStyleSheet(btnServerOnStyle);
        Logger::success("[DASH] Server started");

        m_dashboardUrl = QUrl(url);
        btnOpenDashboard->setEnabled(m_dashboardUrl.isValid());
    });

    connect(m_DashboardWebServer, &DashboardServer::stopped, this, [this]() {
        m_dashboardUrl = QUrl();
        btnOpenDashboard->setEnabled(false);

        m_serverRunning.store(false);
        btnToggleServer->setText("Start Dashboard Server");
        btnToggleServer->setStyleSheet(btnServerOffStyle);
        Logger::error("[DASH] Server stopped");
    });

    m_DashboardServerThread->start();
    if (autostart) { startDashboardServer(); }
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
    if (tabPluginsInner) {
        delete tabPluginsInner;
        tabPluginsInner = nullptr;
    }

    stopDashboardServer();

    if (m_DashboardServerThread->isRunning()) {
        m_DashboardServerThread->quit();
        m_DashboardServerThread->wait();
    }

    plugins_.stopAll();
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

    btnOpenDashboard = new QPushButton("Open Dashboard", tabDashboard);
    btnOpenDashboard->setGeometry(940, 70, 250, 51);
    btnOpenDashboard->setFont(btnFont);
    btnOpenDashboard->setEnabled(false); // server yokken disabled

    btnClose = new QPushButton("Close", tabDashboard);
    btnClose->setGeometry(940, 250, 250, 51);
    btnClose->setFont(btnFont);

    txtDebug = new QPlainTextEdit(tabDashboard);
    txtDebug->setGeometry(0, 310, 1200, 260);
    txtDebug->setReadOnly(true);

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
    // Plugins TAB
    // =======================
    tabPlugins = new QWidget();
    tabWidget->addTab(tabPlugins, "Plugins");

    tabPluginsInner = new QTabWidget(tabPlugins);
    tabPluginsInner->setGeometry(0, 0, 1200, 600);

    // =======================
    // Connections
    // =======================
    connect(btnToggleServer, &QPushButton::clicked, this, &MainWindow::toggleServer);
    connect(btnClose, &QPushButton::clicked, qApp, &QApplication::quit);

    connect(btnOpenDashboard, &QPushButton::clicked, this, [this]() {
        if (m_dashboardUrl.isValid()) {
            QDesktopServices::openUrl(m_dashboardUrl);
        }
    });
}

void MainWindow::clearLogs() {
    // Clears the debug log widget.
    txtDebug->clear();
    Logger::success(">>> Logs cleared.");
}


void MainWindow::refreshPluginsTab() {
    if (!tabPluginsInner) return;

    while (tabPluginsInner->count() > 0) {
        QWidget* w = tabPluginsInner->widget(0);
        tabPluginsInner->removeTab(0);
        if (w) w->deleteLater();
    }

    const auto plugins = plugins_.list();
    if (plugins.empty()) {
        auto* lbl = new QLabel("No plugins loaded.", tabPluginsInner);
        lbl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        tabPluginsInner->addTab(lbl, "(empty)");
        return;
    }

    for (const auto& d : plugins) {
        QWidget* w = plugins_.createWidget(d.id, tabPluginsInner);
        if (!w) {
            auto* lbl = new QLabel("This plugin does not provide a UI.", tabPluginsInner);
            lbl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            w = lbl;
        }
        QString label = d.id;
        if (!label.isEmpty()) label[0] = label[0].toUpper();
        tabPluginsInner->addTab(w, label);
    }
}
