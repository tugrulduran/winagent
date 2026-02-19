#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QSpinBox;
class QPushButton;
class QTimer;

// Simple configuration UI for media plugin.
// Discovered by the host via the optional export: wa_create_widget().
class MediaUi final : public QWidget {
public:
    explicit MediaUi(WaHostApi* api, QWidget* parent = nullptr);

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
    QPushButton* btnSave_ = nullptr;

    QTimer* statusTimer_ = nullptr;
};
