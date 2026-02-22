#include "AudioDevicesUi.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include <QAbstractItemView>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include "AudioDevices.h"

static constexpr const char* kPluginId = "audiodevices";

QString AudioDevicesUi::stateToText(int32_t state) {
    switch (state) {
        case WA_STATE_MISSING: return "MISSING";
        case WA_STATE_RUNNING: return "RUNNING";
        case WA_STATE_PAUSED:  return "STOPPED";
        case WA_STATE_STOPPED: return "STOPPED";
        case WA_STATE_ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}

QString AudioDevicesUi::configPath() {
    // Runtime config is expected at: <exe_dir>/plugins/<id>/config.json
    const QString base = QCoreApplication::applicationDirPath();
    return QDir(base).filePath(QString("plugins/%1/config.json").arg(QString::fromUtf8(kPluginId)));
}

AudioDevicesUi::AudioDevicesUi(WaHostApi* api, QWidget* parent)
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
    auto* grpSettings = new QGroupBox("AudioDevices Settings", this);
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
    // Ignored Devices
    // =====================
    auto* grpIgnored = new QGroupBox("Ignored Devices", this);
    auto* ignoredOuter = new QVBoxLayout(grpIgnored);

    auto* addRow = new QHBoxLayout();
    comboIgnored_ = new QComboBox(grpIgnored);
    comboIgnored_->setEditable(false);
    comboIgnored_->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    comboIgnored_->setToolTip("Select a real audio device to ignore");
    btnAddIgnored_ = new QPushButton("Add", grpIgnored);
    addRow->addWidget(comboIgnored_);
    addRow->addWidget(btnAddIgnored_);

    listIgnored_ = new QListWidget(grpIgnored);
    listIgnored_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Plain listbox look: no custom item widgets, no special effects.
    listIgnored_->setUniformItemSizes(true);
    listIgnored_->setWordWrap(false);

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

    devicesTimer_ = new QTimer(this);
    connect(devicesTimer_, &QTimer::timeout, this, [this]() { refreshAvailableDevices(); });
    devicesTimer_->start(3000);

    // Initial load/state
    loadFromDisk();
    refreshAvailableDevices();
    refreshStatus();
}

void AudioDevicesUi::refreshAvailableDevices() {
    if (!comboIgnored_) return;

    const QString previous = comboIgnored_->currentText();

    audiodevices::AudioDevices ad;
    ad.init(QJsonObject{}); // ensure ignore list is empty
    const auto devices = ad.getDevices();

    QStringList names;
    names.reserve((int)devices.size());
    for (const auto& d : devices) {
        const QString n = QString::fromStdWString(d.name).trimmed();
        if (n.isEmpty()) continue;
        if (!names.contains(n, Qt::CaseInsensitive)) names.push_back(n);
    }
    names.sort(Qt::CaseInsensitive);

    const QSignalBlocker blocker(comboIgnored_);
    comboIgnored_->clear();
    comboIgnored_->addItems(names);

    int idx = comboIgnored_->findText(previous, Qt::MatchFixedString);
    if (idx >= 0) {
        comboIgnored_->setCurrentIndex(idx);
    } else if (comboIgnored_->count() > 0) {
        comboIgnored_->setCurrentIndex(0);
    }

    const bool hasAny = (comboIgnored_->count() > 0);
    comboIgnored_->setEnabled(hasAny);
    if (btnAddIgnored_) btnAddIgnored_->setEnabled(hasAny);
}

void AudioDevicesUi::addIgnoredItem(const QString& name) {
    if (!listIgnored_) return;

    auto* item = new QListWidgetItem(name, listIgnored_);
    item->setToolTip(name);
}

void AudioDevicesUi::addIgnoredFromInput() {
    if (!comboIgnored_ || !listIgnored_) return;
    if (comboIgnored_->currentIndex() < 0) return;
    const QString name = comboIgnored_->currentText().trimmed();
    if (name.isEmpty()) return;

    // Avoid duplicates (case-insensitive)
    for (int i = 0; i < listIgnored_->count(); ++i) {
        if (listIgnored_->item(i)->text().compare(name, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    addIgnoredItem(name);
}

void AudioDevicesUi::removeSelectedIgnored() {
    const auto items = listIgnored_->selectedItems();
    for (auto* it : items) {
        delete listIgnored_->takeItem(listIgnored_->row(it));
    }
}

void AudioDevicesUi::loadFromDisk() {
    QFile f(configPath());
    if (!f.exists()) {
        spinInterval_->setValue(3000);
        listIgnored_->clear();
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "AudioDevices", "Failed to open config file for reading.\n" + configPath());
        spinInterval_->setValue(3000);
        listIgnored_->clear();
        return;
    }

    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "AudioDevices", "Config JSON parse error.\n" + perr.errorString());
        spinInterval_->setValue(3000);
        listIgnored_->clear();
        return;
    }

    const QJsonObject root = doc.object();
    spinInterval_->setValue(root.value("intervalMs").toInt(3000));

    listIgnored_->clear();
    const QJsonValue v = root.value("ignoredDevices");
    if (v.isArray()) {
        const QJsonArray arr = v.toArray();
        for (const auto& x : arr) {
            if (!x.isString()) continue;
            const QString s = x.toString().trimmed();
            if (!s.isEmpty()) addIgnoredItem(s);
        }
    }
}

bool AudioDevicesUi::saveToDisk() {
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
    root.insert("ignoredDevices", ignored);

    QSaveFile sf(configPath());
    if (!sf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "AudioDevices", "Failed to open config file for writing.\n" + configPath());
        return false;
    }

    const QJsonDocument outDoc(root);
    sf.write(outDoc.toJson(QJsonDocument::Indented));
    if (!sf.commit()) {
        QMessageBox::warning(this, "AudioDevices", "Failed to commit config file changes.\n" + configPath());
        return false;
    }

    return true;
}

void AudioDevicesUi::refreshStatus() {
    int32_t st = WA_STATE_MISSING;
    if (api_ && api_->plugin_get_state) {
        st = api_->plugin_get_state(api_->user, kPluginId);
    }

    lblStatus_->setText("Status: " + stateToText(st));

    const bool running = (st == WA_STATE_RUNNING);
    btnStart_->setEnabled(!running);
    btnStop_->setEnabled(running);
}
