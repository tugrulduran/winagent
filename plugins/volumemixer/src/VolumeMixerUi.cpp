#include "VolumeMixerUi.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <QAbstractItemView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

static constexpr const char* kPluginId = "volumemixer";

QString VolumeMixerUi::stateToText(int32_t state) {
    switch (state) {
        case WA_STATE_MISSING: return "MISSING";
        case WA_STATE_RUNNING: return "RUNNING";
        case WA_STATE_PAUSED:  return "STOPPED";
        case WA_STATE_STOPPED: return "STOPPED";
        case WA_STATE_ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

QString VolumeMixerUi::configPath() {
    // Runtime config is expected at: <exe_dir>/plugins/<id>/config.json
    const QString base = QCoreApplication::applicationDirPath();
    return QDir(base).filePath(QString("plugins/%1/config.json").arg(QString::fromUtf8(kPluginId)));
}

VolumeMixerUi::VolumeMixerUi(WaHostApi* api, QWidget* parent)
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
    auto* grpSettings = new QGroupBox("VolumeMixer Settings", this);
    auto* settingsLayout = new QHBoxLayout(grpSettings);

    auto* lblInterval = new QLabel("Interval:", grpSettings);
    spinInterval_ = new QSpinBox(grpSettings);
    spinInterval_->setRange(100, 60 * 60 * 1000);
    spinInterval_->setSingleStep(100);
    spinInterval_->setSuffix(" ms");

    btnSave_ = new QPushButton("Save", grpSettings);

    settingsLayout->addWidget(lblInterval);
    settingsLayout->addWidget(spinInterval_);
    settingsLayout->addStretch(1);
    settingsLayout->addWidget(btnSave_);

    root->addWidget(grpSettings);

    // =====================
    // Ignored Apps
    // =====================
    auto* grpIgnored = new QGroupBox("Ignored Apps", this);
    auto* ignoredOuter = new QVBoxLayout(grpIgnored);

    auto* addRow = new QHBoxLayout();
    editIgnored_ = new QLineEdit(grpIgnored);
    editIgnored_->setPlaceholderText("Type an app name substring to ignore (e.g. System)");
    btnAddIgnored_ = new QPushButton("Add", grpIgnored);
    addRow->addWidget(editIgnored_);
    addRow->addWidget(btnAddIgnored_);

    listIgnored_ = new QListWidget(grpIgnored);
    listIgnored_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* removeRow = new QHBoxLayout();
    btnRemoveIgnored_ = new QPushButton("Remove Selected", grpIgnored);
    removeRow->addStretch(1);
    removeRow->addWidget(btnRemoveIgnored_);

    ignoredOuter->addLayout(addRow);
    ignoredOuter->addWidget(listIgnored_);
    ignoredOuter->addLayout(removeRow);

    root->addWidget(grpIgnored);
    root->addStretch(1);

    // =====================
    // Wire actions
    // =====================
    connect(btnAddIgnored_, &QPushButton::clicked, this, [this]() { addIgnoredFromInput(); });
    connect(editIgnored_, &QLineEdit::returnPressed, this, [this]() { addIgnoredFromInput(); });
    connect(btnRemoveIgnored_, &QPushButton::clicked, this, [this]() { removeSelectedIgnored(); });

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

    // Initial load/state
    loadFromDisk();
    refreshStatus();
}

void VolumeMixerUi::addIgnoredFromInput() {
    const QString name = editIgnored_->text().trimmed();
    if (name.isEmpty()) return;

    // Avoid duplicates (case-insensitive)
    for (int i = 0; i < listIgnored_->count(); ++i) {
        if (listIgnored_->item(i)->text().compare(name, Qt::CaseInsensitive) == 0) {
            editIgnored_->clear();
            return;
        }
    }

    listIgnored_->addItem(name);
    editIgnored_->clear();
}

void VolumeMixerUi::removeSelectedIgnored() {
    const auto items = listIgnored_->selectedItems();
    for (auto* it : items) {
        delete listIgnored_->takeItem(listIgnored_->row(it));
    }
}

void VolumeMixerUi::loadFromDisk() {
    QFile f(configPath());
    if (!f.exists()) {
        spinInterval_->setValue(1000);
        listIgnored_->clear();
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "VolumeMixer", "Failed to open config file for reading.\n" + configPath());
        spinInterval_->setValue(1000);
        listIgnored_->clear();
        return;
    }

    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "VolumeMixer", "Config JSON parse error.\n" + perr.errorString());
        spinInterval_->setValue(1000);
        listIgnored_->clear();
        return;
    }

    const QJsonObject root = doc.object();
    spinInterval_->setValue(root.value("intervalMs").toInt(1000));

    listIgnored_->clear();
    const QJsonValue v = root.value("ignoredApps");
    if (v.isArray()) {
        const QJsonArray arr = v.toArray();
        for (const auto& x : arr) {
            if (!x.isString()) continue;
            const QString s = x.toString().trimmed();
            if (!s.isEmpty()) listIgnored_->addItem(s);
        }
    }
}

bool VolumeMixerUi::saveToDisk() {
    // Ensure the config directory exists.
    {
        const QFileInfo fi(configPath());
        QDir().mkpath(fi.absolutePath());
    }

    QJsonArray ignored;
    for (int i = 0; i < listIgnored_->count(); ++i) {
        const QString s = listIgnored_->item(i)->text().trimmed();
        if (!s.isEmpty()) ignored.append(s);
    }

    QJsonObject root;
    root.insert("intervalMs", spinInterval_->value());
    root.insert("ignoredApps", ignored);

    QSaveFile sf(configPath());
    if (!sf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "VolumeMixer", "Failed to open config file for writing.\n" + configPath());
        return false;
    }

    const QJsonDocument outDoc(root);
    sf.write(outDoc.toJson(QJsonDocument::Indented));
    if (!sf.commit()) {
        QMessageBox::warning(this, "VolumeMixer", "Failed to commit config file changes.\n" + configPath());
        return false;
    }

    return true;
}

void VolumeMixerUi::refreshStatus() {
    int32_t st = WA_STATE_MISSING;
    if (api_ && api_->plugin_get_state) {
        st = api_->plugin_get_state(api_->user, kPluginId);
    }

    lblStatus_->setText("Status: " + stateToText(st));

    const bool running = (st == WA_STATE_RUNNING);
    btnStart_->setEnabled(!running);
    btnStop_->setEnabled(running);
}
