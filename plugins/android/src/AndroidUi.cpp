#include "AndroidUi.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>


static constexpr const char* kPluginId = "android";

QString AndroidUi::stateToText(int32_t state) {
    switch (state) {
        case WA_STATE_MISSING: return "MISSING";
        case WA_STATE_RUNNING: return "RUNNING";
        case WA_STATE_PAUSED:  return "STOPPED";
        case WA_STATE_STOPPED: return "STOPPED";
        case WA_STATE_ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

QString AndroidUi::configPath() {
    const QString base = QCoreApplication::applicationDirPath();
    return QDir(base).filePath(QString("plugins/%1/config.json").arg(QString::fromUtf8(kPluginId)));
}

AndroidUi::AndroidUi(WaHostApi* api, QWidget* parent)
    : QWidget(parent), api_(api) {

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // =====================
    // Status / Controls
    // =====================
    auto* grpStatus = new QGroupBox("Plugin Status", this);
    auto* statusLayout = new QHBoxLayout(grpStatus);

    lblStatus_ = new QLabel("Status: ...", grpStatus);
    lblStatus_->setMinimumWidth(260);

    btnStart_ = new QPushButton("Start", grpStatus);
    btnStop_ = new QPushButton("Stop", grpStatus);
    btnRestart_ = new QPushButton("Restart", grpStatus);

    statusLayout->addWidget(lblStatus_);
    statusLayout->addStretch(1);
    statusLayout->addWidget(btnStart_);
    statusLayout->addWidget(btnStop_);
    statusLayout->addWidget(btnRestart_);

    root->addWidget(grpStatus);

    // =====================
    // Settings
    // =====================
    auto* grpSettings = new QGroupBox("Android Bridge Settings", this);
    auto* settingsLayout = new QVBoxLayout(grpSettings);

    auto* row1 = new QHBoxLayout();
    auto* lblInterval = new QLabel("Interval:", grpSettings);
    spinInterval_ = new QSpinBox(grpSettings);
    spinInterval_->setRange(100, 60 * 60 * 1000);
    spinInterval_->setSingleStep(100);
    spinInterval_->setSuffix(" ms");
    row1->addWidget(lblInterval);
    row1->addWidget(spinInterval_);
    row1->addStretch(1);

    auto* row2 = new QHBoxLayout();
    auto* lblDisc = new QLabel("Discovery UDP port:", grpSettings);
    spinDiscoveryPort_ = new QSpinBox(grpSettings);
    spinDiscoveryPort_->setRange(1, 65535);
    spinDiscoveryPort_->setValue(45151);
    row2->addWidget(lblDisc);
    row2->addWidget(spinDiscoveryPort_);
    row2->addStretch(1);

    auto* row3 = new QHBoxLayout();
    auto* lblData = new QLabel("Data TCP port:", grpSettings);
    spinDataPort_ = new QSpinBox(grpSettings);
    spinDataPort_->setRange(1, 65535);
    spinDataPort_->setValue(45152);
    row3->addWidget(lblData);
    row3->addWidget(spinDataPort_);
    row3->addStretch(1);

    auto* row4 = new QHBoxLayout();
    auto* lblTimeout = new QLabel("Connected timeout:", grpSettings);
    spinTimeout_ = new QSpinBox(grpSettings);
    spinTimeout_->setRange(1000, 10 * 60 * 1000);
    spinTimeout_->setSingleStep(1000);
    spinTimeout_->setSuffix(" ms");
    spinTimeout_->setValue(30000);
    row4->addWidget(lblTimeout);
    row4->addWidget(spinTimeout_);
    row4->addStretch(1);

    auto* row5 = new QHBoxLayout();
    auto* lblToken = new QLabel("Token (optional):", grpSettings);
    editToken_ = new QLineEdit(grpSettings);
    editToken_->setPlaceholderText("leave empty to disable");
    row5->addWidget(lblToken);
    row5->addWidget(editToken_);

    auto* row6 = new QHBoxLayout();
    btnSave_ = new QPushButton("Save", grpSettings);
    row6->addStretch(1);
    row6->addWidget(btnSave_);

    settingsLayout->addLayout(row1);
    settingsLayout->addLayout(row2);
    settingsLayout->addLayout(row3);
    settingsLayout->addLayout(row4);
    settingsLayout->addLayout(row5);
    settingsLayout->addLayout(row6);

    root->addWidget(grpSettings);
    root->addStretch(1);

    // =====================
    // Wire actions
    // =====================
    connect(btnSave_, &QPushButton::clicked, this, [this]() {
        if (!saveToDisk()) return;

        if (api_ && api_->plugin_restart) {
            api_->plugin_restart(api_->user, kPluginId);
        }

        loadFromDisk();
        refreshStatus();
    });

    connect(btnStart_, &QPushButton::clicked, this, [this]() {
        if (api_ && api_->plugin_start) {
            api_->plugin_start(api_->user, kPluginId);
        }
        refreshStatus();
    });

    connect(btnStop_, &QPushButton::clicked, this, [this]() {
        if (api_ && api_->plugin_stop) {
            api_->plugin_stop(api_->user, kPluginId);
        }
        refreshStatus();
    });

    connect(btnRestart_, &QPushButton::clicked, this, [this]() {
        if (api_ && api_->plugin_restart) {
            api_->plugin_restart(api_->user, kPluginId);
        }
        refreshStatus();
    });

    statusTimer_ = new QTimer(this);
    connect(statusTimer_, &QTimer::timeout, this, [this]() { refreshStatus(); });
    statusTimer_->start(500);

    loadFromDisk();
    refreshStatus();
}

void AndroidUi::loadFromDisk() {
    QFile f(configPath());
    if (!f.exists()) {
        spinInterval_->setValue(1000);
        spinDiscoveryPort_->setValue(45151);
        spinDataPort_->setValue(45152);
        spinTimeout_->setValue(30000);
        editToken_->setText({});
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Android", "Failed to open config file for reading.\n" + configPath());
        return;
    }

    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Android", "Config JSON parse error.\n" + perr.errorString());
        return;
    }

    const QJsonObject root = doc.object();
    spinInterval_->setValue(root.value("intervalMs").toInt(1000));
    spinDiscoveryPort_->setValue(root.value("discoveryPort").toInt(45151));
    spinDataPort_->setValue(root.value("port").toInt(45152));
    spinTimeout_->setValue(root.value("connectedTimeoutMs").toInt(30000));
    editToken_->setText(root.value("token").toString());
}

bool AndroidUi::saveToDisk() {
    {
        const QFileInfo fi(configPath());
        QDir().mkpath(fi.absolutePath());
    }

    QJsonObject root;
    root.insert("intervalMs", spinInterval_->value());
    root.insert("discoveryPort", spinDiscoveryPort_->value());
    root.insert("port", spinDataPort_->value());
    root.insert("connectedTimeoutMs", spinTimeout_->value());
    root.insert("token", editToken_->text());

    QSaveFile sf(configPath());
    if (!sf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Android", "Failed to open config file for writing.\n" + configPath());
        return false;
    }

    const QJsonDocument outDoc(root);
    sf.write(outDoc.toJson(QJsonDocument::Indented));
    if (!sf.commit()) {
        QMessageBox::warning(this, "Android", "Failed to commit config file changes.\n" + configPath());
        return false;
    }

    return true;
}

void AndroidUi::refreshStatus() {
    int32_t st = WA_STATE_MISSING;
    if (api_ && api_->plugin_get_state) {
        st = api_->plugin_get_state(api_->user, kPluginId);
    }

    lblStatus_->setText("Status: " + stateToText(st));

    const bool running = (st == WA_STATE_RUNNING);
    btnStart_->setEnabled(!running);
    btnStop_->setEnabled(running);
}
