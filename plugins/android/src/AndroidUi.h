#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QPushButton;
class QSpinBox;
class QLineEdit;
class QTimer;

// Simple configuration UI for android plugin.
// Discovered by the host via the optional export: wa_create_widget().
class AndroidUi final : public QWidget {
    Q_OBJECT
public:
    explicit AndroidUi(WaHostApi* api, QWidget* parent = nullptr);

private:
    static QString stateToText(int32_t state);
    static QString configPath();

    void loadFromDisk();
    bool saveToDisk();
    void refreshStatus();

    WaHostApi* api_ = nullptr;

    QLabel* lblStatus_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnRestart_ = nullptr;

    QSpinBox* spinInterval_ = nullptr;
    QSpinBox* spinDiscoveryPort_ = nullptr;
    QSpinBox* spinDataPort_ = nullptr;
    QSpinBox* spinTimeout_ = nullptr;
    QLineEdit* editToken_ = nullptr;

    QPushButton* btnSave_ = nullptr;
    QTimer* statusTimer_ = nullptr;
};
