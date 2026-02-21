#pragma once

#include <array>
#include <vector>
#include <QString>
#include <QWidget>

#include "BasePlugin.h"

class QLabel;
class QSpinBox;
class QPushButton;
class QTimer;

class ZoneListWidget;

class LauncherUi final : public QWidget {
public:
    explicit LauncherUi(WaHostApi* api, QWidget* parent = nullptr);

private:
    struct UiItem {
        QString name;
        QString path;
    };

    static QString stateToText(int32_t state);
    QString configPath() const;
    QString iconsDir() const;

    void loadFromDisk();
    bool saveToDisk();

    void clearAllZones();
    void rebuildZoneFromItems(int zone, const std::vector<UiItem>& items);

    void onAddClicked(int zone);
    void onRemoveClicked();
    void onZoneSelectionChanged(int zone);

    void refreshStatus();

    WaHostApi* api_ = nullptr;

    QLabel* lblStatus_ = nullptr;
    QPushButton* btnStart_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnRestart_ = nullptr;

    QSpinBox* spinInterval_ = nullptr;

    std::array<ZoneListWidget*, 5> zoneLists_{};

    QPushButton* btnRemove_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    QTimer* statusTimer_ = nullptr;

    bool suppressSelection_ = false;
    int selectedZone_ = -1;
};
