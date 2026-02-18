#pragma once

#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QSpinBox;
class QLineEdit;
class QTableWidget;
class QPushButton;
class QTimer;

class LauncherUi final : public QWidget {
public:
    explicit LauncherUi(WaHostApi* api, QWidget* parent = nullptr);

private:
    static QString stateToText(int32_t state);

    QString configPath() const;

    void loadFromDisk();
    bool saveToDisk();

    void addRow(const QString& name, const QString& path, const QString& icon);
    void refreshStatus();

    WaHostApi* api_ = nullptr;

    // status / controls
    QLabel* lblStatus_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnRestart_ = nullptr;

    // config
    QSpinBox* spinInterval_ = nullptr;
    QTableWidget* table_ = nullptr;

    // add form
    QLineEdit* editName_ = nullptr;
    QLineEdit* editPath_ = nullptr;
    QPushButton* btnBrowse_ = nullptr;
    QPushButton* btnAdd_ = nullptr;
    QPushButton* btnRemove_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    QTimer* statusTimer_ = nullptr;
};
