#pragma once

#include <QFrame>

#include <cstdint>

class QLabel;
class QToolButton;
class QMouseEvent;

// A compact "card" that visualizes one plugin's runtime status.
// - fixed size, designed for grid layouts
// - shows state + short description
// - shows counters (reads/sent/requests)
// - provides Start/Pause, Restart and (optional) Open UI actions
class PluginCardWidget final : public QFrame {
    Q_OBJECT

public:
    explicit PluginCardWidget(QWidget* parent = nullptr);

    void setStaticInfo(
        const QString& pluginId,
        const QString& displayName,
        const QString& description,
        uint32_t defaultIntervalMs,
        bool hasUi
    );

    // state: WaPluginState int (WA_STATE_*)
    void updateRuntime(
        int32_t state,
        uint64_t reads,
        uint64_t sent,
        uint64_t requests,
        qint64 lastReadMs,
        qint64 lastRequestMs
    );

    QString pluginId() const { return pluginId_; }

signals:
    void startRequested(const QString& pluginId);
    void stopRequested(const QString& pluginId);
    void restartRequested(const QString& pluginId);
    void openUiRequested(const QString& pluginId);

protected:
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    void applyStateUi(int32_t state);
    static QString formatCount(uint64_t n);

    QString pluginId_;
    bool hasUi_ = false;
    uint32_t defaultIntervalMs_ = 0;

    QLabel* lblName_ = nullptr;
    QLabel* lblId_ = nullptr;
    QLabel* lblDesc_ = nullptr;

    QLabel* lblStatusDot_ = nullptr;
    QLabel* lblStatusText_ = nullptr;

    QLabel* chipReads_ = nullptr;
    QLabel* chipSent_ = nullptr;
    QLabel* chipReq_ = nullptr;

    QLabel* lblLast_ = nullptr;

    QToolButton* btnToggle_ = nullptr;
    QToolButton* btnRestart_ = nullptr;
    QToolButton* btnUi_ = nullptr;
};
