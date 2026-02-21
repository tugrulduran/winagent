#pragma once

#include <QFrame>
#include <QHash>
#include <QVector>

#include <cstdint>

#include "PluginManager.h"

class QLineEdit;
class QCheckBox;
class QScrollArea;
class QGridLayout;
class QLabel;
class QWidget;
class PluginCardWidget;

// A scrollable, card-based overview of loaded plugins.
// Designed to live in the Dashboard tab's empty top-left area.
class PluginOverviewWidget final : public QFrame {
    Q_OBJECT

public:
    explicit PluginOverviewWidget(PluginManager* plugins, QWidget* parent = nullptr);

    // Update cards + summary counters.
    // Safe to call periodically (e.g., via a QTimer).
    void tick(int clientsConnected, quint64 broadcastsSent);

signals:
    void startPluginRequested(const QString& pluginId);
    void stopPluginRequested(const QString& pluginId);
    void restartPluginRequested(const QString& pluginId);
    void openPluginUiRequested(const QString& pluginId);

protected:
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onFilterChanged();

private:
    void syncCards(const std::vector<PluginManager::PluginUiSnapshot>& snap);
    void updateCards(const std::vector<PluginManager::PluginUiSnapshot>& snap);
    void rebuildGrid();
    int calcColumns() const;

    PluginManager* plugins_ = nullptr;

    QLineEdit* txtSearch_ = nullptr;
    QCheckBox* chkOnlyRunning_ = nullptr;
    QLabel* lblCount_ = nullptr;

    // Summary values
    QLabel* valTotal_ = nullptr;
    QLabel* valRunning_ = nullptr;
    QLabel* valClients_ = nullptr;
    QLabel* valBroadcasts_ = nullptr;

    QScrollArea* scroll_ = nullptr;
    QWidget* gridHost_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QLabel* emptyLabel_ = nullptr;

    QHash<QString, PluginCardWidget*> cards_;
    QVector<QString> order_;

    int lastCols_ = 0;
    int lastClients_ = 0;
    quint64 lastBroadcasts_ = 0;

    bool pendingReflow_ = false;
};
