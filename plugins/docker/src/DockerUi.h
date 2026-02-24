#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;

class DockerUi final : public QWidget {
public:
    explicit DockerUi(WaHostApi* api, QWidget* parent = nullptr);

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
    QLineEdit* editDockerPath_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    QTimer* statusTimer_ = nullptr;
};
