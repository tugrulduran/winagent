#include "LauncherUi.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

static constexpr const char* kPluginId = "launcher";
static constexpr int COL_NAME = 0;
static constexpr int COL_PATH = 1;
static constexpr int ROLE_ICON = Qt::UserRole + 1;

QString LauncherUi::stateToText(int32_t state) {
    switch (state) {
        case WA_STATE_MISSING: return "MISSING";
        case WA_STATE_RUNNING: return "RUNNING";
        case WA_STATE_PAUSED:  return "STOPPED";
        case WA_STATE_STOPPED: return "STOPPED";
        case WA_STATE_ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

LauncherUi::LauncherUi(WaHostApi* api, QWidget* parent)
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
    auto* grpSettings = new QGroupBox("Launcher Settings", this);
    auto* settingsLayout = new QHBoxLayout(grpSettings);

    auto* lblInterval = new QLabel("Interval:", grpSettings);
    spinInterval_ = new QSpinBox(grpSettings);
    spinInterval_->setRange(100, 60 * 60 * 1000);
    spinInterval_->setSingleStep(100);
    spinInterval_->setSuffix(" ms");

    settingsLayout->addWidget(lblInterval);
    settingsLayout->addWidget(spinInterval_);
    settingsLayout->addStretch(1);

    root->addWidget(grpSettings);

    // =====================
    // App list
    // =====================
    auto* grpApps = new QGroupBox("Applications", this);
    auto* appsLayout = new QVBoxLayout(grpApps);

    table_ = new QTableWidget(grpApps);
    table_->setColumnCount(2);
    table_->setHorizontalHeaderLabels({"Name", "Path"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table_->setAlternatingRowColors(true);

    appsLayout->addWidget(table_);

    // add form
    auto* formRow = new QHBoxLayout();
    editName_ = new QLineEdit(grpApps);
    editName_->setPlaceholderText("Name");

    editPath_ = new QLineEdit(grpApps);
    editPath_->setPlaceholderText("Path");

    btnBrowse_ = new QPushButton("Browse...", grpApps);
    btnAdd_ = new QPushButton("Add", grpApps);
    btnRemove_ = new QPushButton("Remove", grpApps);
    btnSave_ = new QPushButton("Save", grpApps);

    formRow->addWidget(editName_, 1);
    formRow->addWidget(editPath_, 3);
    formRow->addWidget(btnBrowse_);
    formRow->addWidget(btnAdd_);
    formRow->addWidget(btnRemove_);
    formRow->addWidget(btnSave_);

    appsLayout->addLayout(formRow);

    root->addWidget(grpApps, 1);

    // =====================
    // Wire actions
    // =====================
    connect(btnBrowse_, &QPushButton::clicked, this, [this]() {
        const QString startDir = editPath_->text().isEmpty() ? QDir::homePath() : QFileInfo(editPath_->text()).absolutePath();
        const QString file = QFileDialog::getOpenFileName(this, "Select executable", startDir,
                                                         "Executables (*.exe);;All Files (*.*)");
        if (file.isEmpty()) return;
        editPath_->setText(QDir::toNativeSeparators(file));
        if (editName_->text().trimmed().isEmpty()) {
            editName_->setText(QFileInfo(file).completeBaseName());
        }
    });

    connect(btnAdd_, &QPushButton::clicked, this, [this]() {
        const QString name = editName_->text().trimmed();
        const QString path = editPath_->text().trimmed();
        if (name.isEmpty() || path.isEmpty()) {
            QMessageBox::warning(this, "Launcher", "Please provide both name and path.");
            return;
        }
        addRow(name, path, "fa-solid fa-question");
        editName_->clear();
        editPath_->clear();
    });

    connect(btnRemove_, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentRow();
        if (row >= 0) {
            table_->removeRow(row);
        }
    });

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

    // Load initial config/state
    loadFromDisk();
    refreshStatus();
}

QString LauncherUi::configPath() const {
    // Runtime config is expected at: <exe_dir>/plugins/<id>/config.json
    const QString base = QCoreApplication::applicationDirPath();
    return QDir(base).filePath(QString("plugins/%1/config.json").arg(QString::fromUtf8(kPluginId)));
}

void LauncherUi::addRow(const QString& name, const QString& path, const QString& icon) {
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto* itName = new QTableWidgetItem(name);
    auto* itPath = new QTableWidgetItem(path);
    itName->setData(ROLE_ICON, icon);
    itPath->setData(ROLE_ICON, icon);

    table_->setItem(row, COL_NAME, itName);
    table_->setItem(row, COL_PATH, itPath);
}

void LauncherUi::loadFromDisk() {
    table_->setRowCount(0);

    QFile f(configPath());
    if (!f.exists()) {
        spinInterval_->setValue(1000);
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Launcher", "Failed to open config file for reading.\n" + configPath());
        spinInterval_->setValue(1000);
        return;
    }

    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Launcher", "Config JSON parse error.\n" + perr.errorString());
        spinInterval_->setValue(1000);
        return;
    }

    const QJsonObject root = doc.object();
    spinInterval_->setValue(root.value("intervalMs").toInt(1000));

    const QJsonArray apps = root.value("apps").toArray();
    for (const QJsonValue& v : apps) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        const QString name = o.value("name").toString();
        const QString path = o.value("path").toString();
        const QString icon = o.value("icon").toString("fa-solid fa-question");
        if (name.isEmpty() || path.isEmpty()) continue;
        addRow(name, path, icon);
    }
}

bool LauncherUi::saveToDisk() {
    // Ensure the config directory exists.
    {
        const QFileInfo fi(configPath());
        QDir().mkpath(fi.absolutePath());
    }

    // Start with existing JSON, so we don't nuke unrelated keys.
    QJsonObject root;

    QFile f(configPath());
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QJsonParseError perr;
        const QJsonDocument oldDoc = QJsonDocument::fromJson(f.readAll(), &perr);
        if (perr.error == QJsonParseError::NoError && oldDoc.isObject()) {
            root = oldDoc.object();
        }
        f.close();
    }

    root.insert("intervalMs", spinInterval_->value());

    QJsonArray apps;
    for (int row = 0; row < table_->rowCount(); ++row) {
        const auto* itName = table_->item(row, COL_NAME);
        const auto* itPath = table_->item(row, COL_PATH);
        if (!itName || !itPath) continue;

        const QString name = itName->text().trimmed();
        const QString path = itPath->text().trimmed();
        if (name.isEmpty() || path.isEmpty()) continue;

        const QString icon = itName->data(ROLE_ICON).toString();
        apps.append(QJsonObject{{"name", name}, {"path", path}, {"icon", icon.isEmpty() ? "fa-solid fa-question" : icon}});
    }
    root.insert("apps", apps);

    // Ensure directory exists
    QDir().mkpath(QFileInfo(configPath()).absolutePath());

    QSaveFile sf(configPath());
    if (!sf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Launcher", "Failed to open config file for writing.\n" + configPath());
        return false;
    }

    const QJsonDocument outDoc(root);
    sf.write(outDoc.toJson(QJsonDocument::Indented));
    if (!sf.commit()) {
        QMessageBox::warning(this, "Launcher", "Failed to commit config file changes.\n" + configPath());
        return false;
    }

    return true;
}

void LauncherUi::refreshStatus() {
    int32_t st = WA_STATE_MISSING;
    if (api_ && api_->plugin_get_state) {
        st = api_->plugin_get_state(api_->user, kPluginId);
    }

    lblStatus_->setText("Status: " + stateToText(st));

    const bool running = (st == WA_STATE_RUNNING);
    btnStart_->setEnabled(!running);
    btnStop_->setEnabled(running);
}
