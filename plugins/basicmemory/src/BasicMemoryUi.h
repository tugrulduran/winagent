#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QSpinBox;
class QPushButton;
class QTimer;

class BasicMemoryUi final : public QWidget {
public:
    explicit BasicMemoryUi(WaHostApi* api, QWidget* parent = nullptr);

private:
    static QString stateToText(int32_t state);

    QString configPath() const;

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
