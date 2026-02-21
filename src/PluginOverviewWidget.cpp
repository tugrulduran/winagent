#include "PluginOverviewWidget.h"

#include <QCheckBox>
#include <QDateTime>
#include <QSet>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "PluginCardWidget.h"

namespace {
constexpr int kCardW = 220;
constexpr int kCardH = 140;
constexpr int kGridSpacing = 10;

class StatCard final : public QFrame {
public:
    StatCard(const QString& title, QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName("statCard");
        setFixedSize(kCardW, 64);
        setStyleSheet(
            "#statCard { background: #1E1E1E; border: 1px solid #333333; border-radius: 12px; }"
        );

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 8, 10, 8);
        root->setSpacing(2);

        lblTitle_ = new QLabel(title, this);
        lblTitle_->setStyleSheet("QLabel { color: #A0A0A0; font-size: 11px; }");

        lblValue_ = new QLabel("0", this);
        QFont f = lblValue_->font();
        f.setPointSize(18);
        f.setBold(true);
        lblValue_->setFont(f);
        lblValue_->setStyleSheet("QLabel { color: #F0F0F0; }");

        root->addWidget(lblTitle_);
        root->addWidget(lblValue_);
        root->addStretch(1);
    }

    QLabel* valueLabel() const { return lblValue_; }

private:
    QLabel* lblTitle_ = nullptr;
    QLabel* lblValue_ = nullptr;
};

static QString normalize(const QString& s) {
    return s.trimmed().toLower();
}

static QString formatBig(quint64 n) {
    if (n < 1000) return QString::number(n);
    if (n < 1000ull * 1000ull) {
        const double v = (double)n / 1000.0;
        return QString::number(v, 'f', v < 10.0 ? 1 : 0) + "k";
    }
    if (n < 1000ull * 1000ull * 1000ull) {
        const double v = (double)n / (1000.0 * 1000.0);
        return QString::number(v, 'f', v < 10.0 ? 1 : 0) + "M";
    }
    const double v = (double)n / (1000.0 * 1000.0 * 1000.0);
    return QString::number(v, 'f', v < 10.0 ? 1 : 0) + "B";
}
} // namespace

PluginOverviewWidget::PluginOverviewWidget(PluginManager* plugins, QWidget* parent)
    : QFrame(parent), plugins_(plugins) {
    setObjectName("pluginOverview");
    setStyleSheet(
        "#pluginOverview { background: transparent; }"
        "QLineEdit { background: #1E1E1E; border: 1px solid #333333; border-radius: 10px; padding: 6px 10px; color: #EDEDED; }"
        "QLineEdit:focus { border: 1px solid #555555; }"
        "QCheckBox { color: #D0D0D0; font-size: 12px; }"
        "QScrollArea { border: none; }"
        "QScrollBar:vertical { width: 10px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.18); border-radius: 5px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(5);

    // --- Controls row ---
    auto* rowCtl = new QHBoxLayout();
    rowCtl->setContentsMargins(0, 0, 0, 0);
    rowCtl->setSpacing(8);

    auto* lblTitle = new QLabel("Plugins", this);
    QFont tf = lblTitle->font();
    tf.setPointSize(12);
    tf.setBold(true);
    lblTitle->setFont(tf);
    lblTitle->setStyleSheet("QLabel { color: #F0F0F0; }");

    txtSearch_ = new QLineEdit(this);
    txtSearch_->setPlaceholderText("Search (id / name)");
    txtSearch_->setClearButtonEnabled(true);

    chkOnlyRunning_ = new QCheckBox("Only running", this);

    lblCount_ = new QLabel("", this);
    lblCount_->setStyleSheet("QLabel { color: #A0A0A0; font-size: 11px; }");

    rowCtl->addWidget(lblTitle);
    rowCtl->addSpacing(6);
    rowCtl->addWidget(txtSearch_, 1);
    rowCtl->addWidget(chkOnlyRunning_);
    rowCtl->addWidget(lblCount_);

    root->addLayout(rowCtl);

    connect(txtSearch_, &QLineEdit::textChanged, this, &PluginOverviewWidget::onFilterChanged);
    connect(chkOnlyRunning_, &QCheckBox::toggled, this, &PluginOverviewWidget::onFilterChanged);

    // --- Summary row ---
    auto* rowSummary = new QHBoxLayout();
    rowSummary->setContentsMargins(0, 0, 0, 0);
    rowSummary->setSpacing(kGridSpacing);

    auto* cTotal = new StatCard("Total", this);
    auto* cRun = new StatCard("Running", this);
    auto* cClients = new StatCard("Clients", this);
    auto* cBroadcasts = new StatCard("Broadcasts", this);

    valTotal_ = cTotal->valueLabel();
    valRunning_ = cRun->valueLabel();
    valClients_ = cClients->valueLabel();
    valBroadcasts_ = cBroadcasts->valueLabel();

    rowSummary->addWidget(cTotal);
    rowSummary->addWidget(cRun);
    rowSummary->addWidget(cClients);
    rowSummary->addWidget(cBroadcasts);
    rowSummary->addStretch(1);

    root->addLayout(rowSummary);

    // --- Scrollable grid ---
    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setAutoFillBackground(false);
    scroll_->viewport()->setAutoFillBackground(false);
    scroll_->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    scroll_->viewport()->setStyleSheet("background: transparent;");

    gridHost_ = new QWidget(scroll_);
    gridHost_->setObjectName("pluginGridHost");

    gridHost_->setAutoFillBackground(false);
    gridHost_->setAttribute(Qt::WA_TranslucentBackground, true);
    gridHost_->setStyleSheet("background: transparent;");

    grid_ = new QGridLayout(gridHost_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(kGridSpacing);
    grid_->setVerticalSpacing(kGridSpacing);
    // Center the grid horizontally so a small number of plugins doesn't
    // look "stuck" to the left with lots of empty space.
    grid_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    emptyLabel_ = new QLabel("No plugins loaded.", gridHost_);
    emptyLabel_->setStyleSheet("QLabel { color: #A0A0A0; font-size: 12px; }");
    emptyLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    scroll_->setWidget(gridHost_);
    root->addWidget(scroll_, 1);

    lastCols_ = calcColumns();
    rebuildGrid();
}

void PluginOverviewWidget::tick(int clientsConnected, quint64 broadcastsSent) {
    lastClients_ = clientsConnected;
    lastBroadcasts_ = broadcastsSent;

    std::vector<PluginManager::PluginUiSnapshot> snap;
    if (plugins_) {
        snap = plugins_->snapshotUi();
    }

    syncCards(snap);
    updateCards(snap);

    // Summary
    const int total = (int)snap.size();
    int running = 0;
    for (const auto& s : snap) {
        if (s.state == WA_STATE_RUNNING) running++;
    }

    if (valTotal_) valTotal_->setText(QString::number(total));
    if (valRunning_) valRunning_->setText(QString::number(running));
    if (valClients_) valClients_->setText(QString::number(lastClients_));
    if (valBroadcasts_) valBroadcasts_->setText(formatBig(lastBroadcasts_));

    // Count label (showing x / total)
    int shown = 0;
    for (const auto& id : order_) {
        auto it = cards_.find(id);
        if (it == cards_.end()) continue;
        if (!it.value()->isHidden()) shown++;
    }
    lblCount_->setText(QString("%1 / %2").arg(shown).arg(total));

    // Debounced reflow (resize + filter)
    const int cols = calcColumns();
    if (cols != lastCols_) {
        lastCols_ = cols;
        rebuildGrid();
    }
}

void PluginOverviewWidget::resizeEvent(QResizeEvent* e) {
    QFrame::resizeEvent(e);
    const int cols = calcColumns();
    if (cols != lastCols_) {
        lastCols_ = cols;
        rebuildGrid();
    }
}

void PluginOverviewWidget::onFilterChanged() {
    rebuildGrid();
}

int PluginOverviewWidget::calcColumns() const {
    if (!scroll_) return 4;
    const int w = scroll_->viewport()->width();
    const int step = kCardW + kGridSpacing;
    const int cols = step > 0 ? (w + kGridSpacing) / step : 4;
    return qBound(1, cols, 6);
}

void PluginOverviewWidget::syncCards(const std::vector<PluginManager::PluginUiSnapshot>& snap) {
    // Create missing cards and update static info.
    QSet<QString> present;
    present.reserve((int)snap.size());

    order_.clear();
    order_.reserve((int)snap.size());

    for (const auto& s : snap) {
        const QString id = s.id;
        if (id.isEmpty()) continue;
        present.insert(id);
        order_.push_back(id);

        PluginCardWidget* card = cards_.value(id, nullptr);
        if (!card) {
            card = new PluginCardWidget(gridHost_);
            cards_.insert(id, card);

            connect(card, &PluginCardWidget::startRequested, this, &PluginOverviewWidget::startPluginRequested);
            connect(card, &PluginCardWidget::stopRequested, this, &PluginOverviewWidget::stopPluginRequested);
            connect(card, &PluginCardWidget::restartRequested, this, &PluginOverviewWidget::restartPluginRequested);
            connect(card, &PluginCardWidget::openUiRequested, this, &PluginOverviewWidget::openPluginUiRequested);
        }

        card->setStaticInfo(s.id, s.name, s.description, s.defaultIntervalMs, s.hasUi);
    }

    // Remove cards for plugins that no longer exist.
    // (Unlikely, but keeps UI consistent.)
    for (auto it = cards_.begin(); it != cards_.end();) {
        if (!present.contains(it.key())) {
            if (it.value()) it.value()->deleteLater();
            it = cards_.erase(it);
        } else {
            ++it;
        }
    }

    rebuildGrid();
}

void PluginOverviewWidget::updateCards(const std::vector<PluginManager::PluginUiSnapshot>& snap) {
    // Update runtime counters + state.
    for (const auto& s : snap) {
        PluginCardWidget* card = cards_.value(s.id, nullptr);
        if (!card) continue;
        card->updateRuntime(
            s.state,
            s.reads,
            s.sent,
            s.requests,
            s.lastReadMs,
            s.lastRequestMs
        );
    }
}

void PluginOverviewWidget::rebuildGrid() {
    if (!grid_) return;

    // Clear layout items
    while (QLayoutItem* it = grid_->takeAt(0)) {
        delete it;
    }

    const QString q = normalize(txtSearch_ ? txtSearch_->text() : QString());
    const bool onlyRunning = chkOnlyRunning_ ? chkOnlyRunning_->isChecked() : false;

    const int cols = calcColumns();
    int r = 0;
    int c = 0;
    int shown = 0;

    for (const QString& id : order_) {
        PluginCardWidget* card = cards_.value(id, nullptr);
        if (!card) continue;

        const QString hay = normalize(card->toolTip() + " " + id);
        const bool match = q.isEmpty() || hay.contains(q);

        const int state = card->property("wa_state").toInt();
        const bool matchRunning = (!onlyRunning) || (state == WA_STATE_RUNNING);

        const bool visible = match && matchRunning;
        card->setVisible(visible);
        if (!visible) continue;

        grid_->addWidget(card, r, c);
        shown++;

        c++;
        if (c >= cols) {
            c = 0;
            r++;
        }
    }

    if (shown == 0) {
        emptyLabel_->setText(order_.isEmpty() ? "No plugins loaded." : "No plugins match the filter." );
        emptyLabel_->setVisible(true);
        grid_->addWidget(emptyLabel_, 0, 0, 1, qMax(1, cols));
    } else {
        emptyLabel_->setVisible(false);
    }

    // keep nice top padding by stretching the last row
    gridHost_->updateGeometry();
}
