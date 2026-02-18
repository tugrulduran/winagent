#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTimer;

// Simple configuration UI for basicnetwork plugin.
// Discovered by the host via the optional export: wa_create_widget().
class BasicNetworkUi final : public QWidget {
public:
    explicit BasicNetworkUi(WaHostApi* api, QWidget* parent = nullptr);

private:
    static QString stateToText(int32_t state);
    static QString configPath();

    void loadFromDisk();
    bool saveToDisk();
    void refreshStatus();

    void addInterfaceFromInput();
    void removeSelectedInterfaces();

    WaHostApi* api_ = nullptr;

    // Status
    QLabel* lblStatus_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnRestart_ = nullptr;

    // Settings
    QSpinBox* spinInterval_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    // Allowed interfaces
    QLineEdit* editIface_ = nullptr;
    QPushButton* btnAddIface_ = nullptr;
    QPushButton* btnRemoveIface_ = nullptr;
    QListWidget* listIfaces_ = nullptr;

    QTimer* statusTimer_ = nullptr;
};
