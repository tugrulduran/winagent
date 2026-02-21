#include "PluginCardWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QStyle>
#include <QDateTime>
#include <QMouseEvent>

namespace {
constexpr int kCardW = 220;
constexpr int kCardH = 140;

static QString chipStyle(const QString& bg) {
    return QString(
        "QLabel { background: %1; color: #E5E5E5; font-size: 11px; border-radius: 7px; padding: 2px 6px; }"
    ).arg(bg);
}

static const char* kSubtleTextStyle = "QLabel { color: #A0A0A0; font-size: 11px; }";
static const char* kDescStyle = "QLabel { color: #D0D0D0; font-size: 11px; }";

static QString stateText(int32_t state) {
    // WaPluginState values are defined in BasePlugin.h
    switch (state) {
        case 1: return "Running";
        case 2: return "Paused";
        case 0: return "Stopped";
        case 3: return "Error";
        default: return "Missing";
    }
}

static QString stateDotColor(int32_t state) {
    switch (state) {
        case 1: return "#00C853"; // green
        case 2: return "#FFB300"; // amber
        case 0: return "#9E9E9E"; // gray
        case 3: return "#FF1744"; // red
        default: return "#616161";
    }
}

static QString agoText(qint64 nowMs, qint64 thenMs) {
    if (thenMs <= 0) return "-";
    qint64 diff = nowMs - thenMs;
    if (diff < 0) diff = 0;

    const qint64 s = diff / 1000;
    if (s < 60) return QString::number(s) + "s ago";
    const qint64 m = s / 60;
    if (m < 60) return QString::number(m) + "m ago";
    const qint64 h = m / 60;
    return QString::number(h) + "h ago";
}
} // namespace

PluginCardWidget::PluginCardWidget(QWidget* parent) : QFrame(parent) {
    setObjectName("pluginCard");
    setFixedSize(kCardW, kCardH);

    setStyleSheet(
        "#pluginCard { background: #1E1E1E; border: 1px solid #333333; border-radius: 12px; }"
        "#pluginCard:hover { border: 1px solid #555555; }"
        "QToolButton { border: none; padding: 0px; }"
        "QToolButton:hover { background: rgba(255,255,255,0.06); border-radius: 6px; }"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);

    // --- Top row: name + action buttons ---
    auto* rowTop = new QHBoxLayout();
    rowTop->setContentsMargins(0, 0, 0, 0);
    rowTop->setSpacing(6);

    lblName_ = new QLabel("Plugin", this);
    QFont nameFont = lblName_->font();
    nameFont.setPointSize(12);
    nameFont.setBold(true);
    lblName_->setFont(nameFont);
    lblName_->setStyleSheet("QLabel { color: #F0F0F0; }");

    btnUi_ = new QToolButton(this);
    btnUi_->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    btnUi_->setIconSize(QSize(16, 16));
    btnUi_->setFixedSize(24, 24);
    btnUi_->setToolTip("Open plugin UI");
    btnUi_->hide();

    btnRestart_ = new QToolButton(this);
    btnRestart_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    btnRestart_->setIconSize(QSize(16, 16));
    btnRestart_->setFixedSize(24, 24);
    btnRestart_->setToolTip("Restart");

    btnToggle_ = new QToolButton(this);
    btnToggle_->setIconSize(QSize(16, 16));
    btnToggle_->setFixedSize(24, 24);

    rowTop->addWidget(lblName_);
    rowTop->addStretch(1);
    rowTop->addWidget(btnUi_);
    rowTop->addWidget(btnRestart_);
    rowTop->addWidget(btnToggle_);
    root->addLayout(rowTop);

    // --- Status row ---
    auto* rowStatus = new QHBoxLayout();
    rowStatus->setContentsMargins(0, 0, 0, 0);
    rowStatus->setSpacing(6);

    lblStatusDot_ = new QLabel("●", this);
    QFont dotFont = lblStatusDot_->font();
    dotFont.setPointSize(12);
    lblStatusDot_->setFont(dotFont);

    lblStatusText_ = new QLabel("Stopped", this);
    lblStatusText_->setStyleSheet("QLabel { color: #CFCFCF; font-size: 11px; }");

    lblId_ = new QLabel("", this);
    lblId_->setStyleSheet(kSubtleTextStyle);
    lblId_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowStatus->addWidget(lblStatusDot_);
    rowStatus->addWidget(lblStatusText_);
    rowStatus->addStretch(1);
    rowStatus->addWidget(lblId_);
    root->addLayout(rowStatus);

    // --- Description ---
    lblDesc_ = new QLabel("", this);
    lblDesc_->setWordWrap(true);
    lblDesc_->setStyleSheet(kDescStyle);
    lblDesc_->setMaximumHeight(34);
    root->addWidget(lblDesc_);

    // --- Chips row ---
    auto* rowChips = new QHBoxLayout();
    rowChips->setContentsMargins(0, 0, 0, 0);
    rowChips->setSpacing(6);

    chipReads_ = new QLabel("R 0", this);
    chipReads_->setStyleSheet(chipStyle("#2A2A2A"));
    chipReads_->setToolTip("Plugin data reads");

    chipSent_ = new QLabel("S 0", this);
    chipSent_->setStyleSheet(chipStyle("#2A2A2A"));
    chipSent_->setToolTip("Sent data to clients");

    chipReq_ = new QLabel("Q 0", this);
    chipReq_->setStyleSheet(chipStyle("#2A2A2A"));
    chipReq_->setToolTip("Client requests");

    rowChips->addWidget(chipReads_);
    rowChips->addWidget(chipSent_);
    rowChips->addWidget(chipReq_);
    rowChips->addStretch(1);
    root->addLayout(rowChips);

    // --- Last activity ---
    lblLast_ = new QLabel("-", this);
    lblLast_->setStyleSheet(kSubtleTextStyle);
    root->addWidget(lblLast_);

    // --- Wiring ---
    connect(btnUi_, &QToolButton::clicked, this, [this] {
        if (!pluginId_.isEmpty()) emit openUiRequested(pluginId_);
    });

    connect(btnRestart_, &QToolButton::clicked, this, [this] {
        if (!pluginId_.isEmpty()) emit restartRequested(pluginId_);
    });

    connect(btnToggle_, &QToolButton::clicked, this, [this] {
        if (pluginId_.isEmpty()) return;
        const int st = property("wa_state").toInt();
        if (st == 1) emit stopRequested(pluginId_);
        else emit startRequested(pluginId_);
    });

    applyStateUi(0);
}

void PluginCardWidget::setStaticInfo(
    const QString& pluginId,
    const QString& displayName,
    const QString& description,
    uint32_t defaultIntervalMs,
    bool hasUi
) {
    pluginId_ = pluginId;
    hasUi_ = hasUi;
    defaultIntervalMs_ = defaultIntervalMs;

    lblName_->setText(displayName);

    QString right = pluginId;
    if (defaultIntervalMs_ > 0) {
        right += QString(" • %1ms").arg(defaultIntervalMs_);
    }
    lblId_->setText(right);

    lblDesc_->setText(description.isEmpty() ? QString("No description") : description);
    setToolTip(QString("%1 (%2)\n%3").arg(displayName, pluginId, lblDesc_->text()));

    btnUi_->setVisible(hasUi_);
}

void PluginCardWidget::updateRuntime(
    int32_t state,
    uint64_t reads,
    uint64_t sent,
    uint64_t requests,
    qint64 lastReadMs,
    qint64 lastRequestMs
) {
    applyStateUi(state);

    chipReads_->setText("R " + formatCount(reads));
    chipSent_->setText("S " + formatCount(sent));
    chipReq_->setText("Q " + formatCount(requests));

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString lastUpdate = agoText(nowMs, lastReadMs);
    const QString lastReq = agoText(nowMs, lastRequestMs);

    if (lastReadMs > 0 && lastRequestMs > 0) {
        lblLast_->setText(QString("Last: upd %1 • req %2").arg(lastUpdate, lastReq));
    } else if (lastReadMs > 0) {
        lblLast_->setText(QString("Last update: %1").arg(lastUpdate));
    } else if (lastRequestMs > 0) {
        lblLast_->setText(QString("Last request: %1").arg(lastReq));
    } else {
        lblLast_->setText("-");
    }
}

void PluginCardWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e) e->accept();
    if (hasUi_ && !pluginId_.isEmpty()) emit openUiRequested(pluginId_);
}

void PluginCardWidget::applyStateUi(int32_t state) {
    setProperty("wa_state", state);

    lblStatusDot_->setStyleSheet(QString("QLabel { color: %1; }").arg(stateDotColor(state)));
    lblStatusText_->setText(stateText(state));

    if (state == 1) {
        btnToggle_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        btnToggle_->setToolTip("Pause");
    } else {
        btnToggle_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        btnToggle_->setToolTip("Start");
    }
}

QString PluginCardWidget::formatCount(uint64_t n) {
    if (n < 1000) return QString::number(static_cast<qulonglong>(n));
    if (n < 1000ull * 1000ull) {
        const double v = static_cast<double>(n) / 1000.0;
        return QString::number(v, 'f', v < 10.0 ? 1 : 0) + "k";
    }
    if (n < 1000ull * 1000ull * 1000ull) {
        const double v = static_cast<double>(n) / (1000.0 * 1000.0);
        return QString::number(v, 'f', v < 10.0 ? 1 : 0) + "M";
    }
    const double v = static_cast<double>(n) / (1000.0 * 1000.0 * 1000.0);
    return QString::number(v, 'f', v < 10.0 ? 1 : 0) + "B";
}
