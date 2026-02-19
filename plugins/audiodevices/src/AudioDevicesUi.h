#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTimer;

// Simple configuration UI for audiodevices plugin.
// Discovered by the host via the optional export: wa_create_widget().
class AudioDevicesUi final : public QWidget {
public:
    explicit AudioDevicesUi(WaHostApi* api, QWidget* parent = nullptr);

private:
    static QString stateToText(int32_t state);
    static QString configPath();

    void loadFromDisk();
    bool saveToDisk();
    void refreshStatus();

    void addIgnoredFromInput();
    void removeSelectedIgnored();

    WaHostApi* api_ = nullptr;

    // Status
    QLabel* lblStatus_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnRestart_ = nullptr;

    // Settings
    QSpinBox* spinInterval_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    // ignoredDevices
    QLineEdit* editIgnored_ = nullptr;
    QPushButton* btnAddIgnored_ = nullptr;
    QPushButton* btnRemoveIgnored_ = nullptr;
    QListWidget* listIgnored_ = nullptr;

    QTimer* statusTimer_ = nullptr;
};
