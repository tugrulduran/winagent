#include "LauncherUi.h"

#include <algorithm>
#include <vector>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

static constexpr const char *kPluginId = "launcher";

static constexpr int kRoleJsonData = Qt::UserRole + 1;

static QJsonObject getItemData(const QListWidgetItem *it) {
    if (!it) return QJsonObject();
    QByteArray jsonData = it->data(kRoleJsonData).toByteArray();
    return QJsonDocument::fromJson(jsonData).object();
}

static void setItemData(QListWidgetItem *it, const QJsonObject &obj) {
    if (!it) return;
    it->setData(kRoleJsonData, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

static QString getAppHash(const QString &path) {
    QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

static QString getIconsDir(const QString &configPath) {
    QFileInfo fi(configPath);
    return QDir(fi.absolutePath()).filePath("icons");
}

static QIcon getOrExtractIcon(const QString &path, const QString &iconsDirPath, const QString &hash) {
    QDir().mkpath(iconsDirPath);
    QString iconPath = QDir(iconsDirPath).filePath(hash + ".png");

    if (QFile::exists(iconPath)) {
        return QIcon(iconPath);
    }

    QFileIconProvider provider;
    QFileInfo fi(path);
    QIcon sysIcon = provider.icon(fi);

    if (!sysIcon.isNull()) {
        QPixmap pm = sysIcon.pixmap(256, 256);
        if (pm.isNull()) pm = sysIcon.pixmap(192, 192);
        if (pm.isNull()) pm = sysIcon.pixmap(128, 128);
        if (pm.isNull()) pm = sysIcon.pixmap(96, 96);
        if (pm.isNull()) pm = sysIcon.pixmap(82, 82);
        if (pm.isNull()) pm = sysIcon.pixmap(64, 64);
        if (pm.isNull()) pm = sysIcon.pixmap(48, 48);
        if (pm.isNull()) pm = sysIcon.pixmap(32, 32);

        if (!pm.isNull()) {
            pm.save(iconPath, "PNG");
            return QIcon(iconPath);
        }
    }
    return sysIcon;
}

static QIcon makePlusIcon(const QWidget *w, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor c = w ? w->palette().color(QPalette::Text) : QColor(220, 220, 220);
    QPen pen(c);
    pen.setWidth(std::max(3, size / 10));
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    const int m = size / 4;
    p.drawLine(size / 2, m, size / 2, size - m);
    p.drawLine(m, size / 2, size - m, size / 2);
    return QIcon(pm);
}

class ZoneListWidget final : public QListWidget {
public:
    using QListWidget::addItem;

    explicit ZoneListWidget(int zone, QWidget *parent = nullptr)
        : QListWidget(parent), zone_(zone) {
    }

    static constexpr const char *kMimeType = "application/x-winagent-launcher-item";

    int zone() const { return zone_; }

    void setAddItem(QListWidgetItem *it) { addItem_ = it; }
    QListWidgetItem *addItem() const { return addItem_; }

    void ensureAddItemLast() {
        if (!addItem_) return;
        const int r = row(addItem_);
        if (r >= 0 && r != count() - 1) {
            takeItem(r);
            addItem(addItem_);
        }
    }

    void clearDropIndicator() {
        if (dropRow_ != -1) {
            dropRow_ = -1;
            viewport()->update();
        }
    }

    int calcInsertRow(const QPoint &vpPos) const {
        const int lastInsertRow = std::max(0, count() - (addItem_ ? 1 : 0));

        QListWidgetItem *hit = itemAt(vpPos);
        if (!hit) {
            return lastInsertRow; // end (before +)
        }
        if (hit == addItem_) {
            return lastInsertRow;
        }

        const QRect r = visualItemRect(hit);
        const int baseRow = row(hit);
        const bool before = (vpPos.x() < r.center().x());
        int insertRow = before ? baseRow : (baseRow + 1);
        insertRow = std::clamp(insertRow, 0, lastInsertRow);
        return insertRow;
    }

    QListWidgetItem *findByHash(const QString &hash) const {
        if (hash.isEmpty()) return nullptr;
        for (int i = 0; i < count(); ++i) {
            auto *it = item(i);
            if (!it || it == addItem_) continue;
            const QJsonObject d = getItemData(it);
            if (d.value("isAdd").toBool()) continue;
            if (d.value("hash").toString() == hash) return it;
        }
        return nullptr;
    }

    void moveItemWithin(QListWidgetItem *it, int insertRow) {
        if (!it) return;
        if (it == addItem_) return;
        const int srcRow = row(it);
        const int lastInsertRow = std::max(0, count() - (addItem_ ? 1 : 0));
        insertRow = std::clamp(insertRow, 0, lastInsertRow);
        if (srcRow < 0) return;

        int targetRow = insertRow;
        if (targetRow > srcRow) targetRow -= 1;

        if (targetRow == srcRow) return;

        takeItem(srcRow);
        insertItem(targetRow, it);
        setCurrentItem(it);
    }

protected:
    void startDrag(Qt::DropActions supportedActions) override {
        Q_UNUSED(supportedActions);
        auto *it = currentItem();
        if (!it) return;

        const QJsonObject data = getItemData(it);
        if (it == addItem_ || data.value("isAdd").toBool()) return;

        QJsonObject payload;
        payload["sourceZone"] = zone_;
        payload["sourceRow"] = row(it);
        payload["hash"] = data.value("hash").toString();
        payload["itemData"] = data;

        auto *mime = new QMimeData();
        mime->setData(kMimeType, QJsonDocument(payload).toJson(QJsonDocument::Compact));
        mime->setText(it->text());

        auto *drag = new QDrag(this);
        drag->setMimeData(mime);

        if (!it->icon().isNull()) {
            const QPixmap pm = it->icon().pixmap(iconSize());
            if (!pm.isNull()) {
                drag->setPixmap(pm);
                drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
            }
        }

        clearDropIndicator();
        drag->exec(Qt::MoveAction);
    }

    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData() && event->mimeData()->hasFormat(kMimeType)) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
            return;
        }
        event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override {
        if (!event->mimeData() || !event->mimeData()->hasFormat(kMimeType)) {
            event->ignore();
            return;
        }

        event->setDropAction(Qt::MoveAction);
        event->accept();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint vpPos = event->position().toPoint();
#else
        const QPoint vpPos = event->pos();
#endif
        const int newDropRow = calcInsertRow(vpPos);
        if (newDropRow != dropRow_) {
            dropRow_ = newDropRow;
            viewport()->update();
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override {
        Q_UNUSED(event);
        clearDropIndicator();
    }

    void dropEvent(QDropEvent *event) override {
        if (!event->mimeData() || !event->mimeData()->hasFormat(kMimeType)) {
            event->ignore();
            clearDropIndicator();
            return;
        }

        event->setDropAction(Qt::MoveAction);

        const QByteArray bytes = event->mimeData()->data(kMimeType);
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        const QJsonObject payload = doc.object();
        const QString hash = payload.value("hash").toString();

        // Determine insertion row.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint vpPos = event->position().toPoint();
#else
        const QPoint vpPos = event->pos();
#endif
        int insertRow = calcInsertRow(vpPos);

        auto *src = dynamic_cast<ZoneListWidget *>(event->source());
        if (!src) {
            event->ignore();
            clearDropIndicator();
            return;
        }

        QListWidgetItem *dragged = nullptr;
        if (src == this) {
            dragged = findByHash(hash);
            if (!dragged) {
                event->ignore();
                clearDropIndicator();
                return;
            }
            moveItemWithin(dragged, insertRow);
        }
        else {
            dragged = src->findByHash(hash);
            if (!dragged) {
                const QJsonObject itemData = payload.value("itemData").toObject();
                if (itemData.isEmpty() || itemData.value("isAdd").toBool()) {
                    event->ignore();
                    clearDropIndicator();
                    return;
                }

                const QString name = itemData.value("name").toString();
                const QString iconPath = itemData.value("iconPath").toString();
                auto *it = new QListWidgetItem(iconPath.isEmpty() ? QIcon() : QIcon(iconPath), name);
                setItemData(it, itemData);
                it->setToolTip(itemData.value("path").toString());
                it->setSizeHint(gridSize());
                it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
                dragged = it;
            }
            else {
                const int srcRow = src->row(dragged);
                if (srcRow >= 0) {
                    src->takeItem(srcRow);
                }
            }

            const int lastInsertRow = std::max(0, count() - (addItem_ ? 1 : 0));
            insertRow = std::clamp(insertRow, 0, lastInsertRow);
            insertItem(insertRow, dragged);
            setCurrentItem(dragged);

            ensureAddItemLast();
            src->ensureAddItemLast();
        }

        if (dragged && dragged != addItem_) {
            const QJsonObject d = getItemData(dragged);
            dragged->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            const QString iconPath = d.value("iconPath").toString();
            if (!iconPath.isEmpty() && dragged->icon().isNull()) {
                dragged->setIcon(QIcon(iconPath));
            }
            dragged->setSizeHint(gridSize());
        }

        event->accept();
        clearDropIndicator();
    }

    void paintEvent(QPaintEvent *e) override {
        QListWidget::paintEvent(e);

        if (dropRow_ < 0) return;
        if (count() <= 0) return;

        const int lastInsertRow = std::max(0, count() - (addItem_ ? 1 : 0));
        const int r = std::clamp(dropRow_, 0, lastInsertRow);

        QListWidgetItem *ref = nullptr;
        if (r < count()) {
            ref = item(std::min(r, count() - 1));
        }
        if (!ref) return;

        QRect vr = visualItemRect(ref);
        if (!vr.isValid()) return;

        const QColor base = palette().color(QPalette::Highlight);
        QColor c = base;
        c.setAlpha(180);

        int x = vr.left() - 2;
        if (ref == addItem_ && r == lastInsertRow) {
            x = vr.left() - 2;
        }

        QRect bar(x, vr.top() + 6, 4, std::max(0, vr.height() - 12));

        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(bar, 2, 2);
    }

private:
    int zone_ = 1;
    QListWidgetItem *addItem_ = nullptr;
    int dropRow_ = -1;
};

QString LauncherUi::stateToText(int32_t state) {
    switch (state) {
        case WA_STATE_MISSING: return "MISSING";
        case WA_STATE_RUNNING: return "RUNNING";
        case WA_STATE_PAUSED: return "STOPPED";
        case WA_STATE_STOPPED: return "STOPPED";
        case WA_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

LauncherUi::LauncherUi(WaHostApi *api, QWidget *parent)
    : QWidget(parent), api_(api) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // =====================
    // Status / Controls
    // =====================
    auto *grpStatus = new QGroupBox("Plugin Status", this);
    auto *statusLayout = new QHBoxLayout(grpStatus);

    lblStatus_ = new QLabel("Status: ...", grpStatus);
    lblStatus_->setMinimumWidth(260);

    btnStart_ = new QPushButton("Start", grpStatus);
    btnStop_ = new QPushButton("Stop", grpStatus);
    btnRestart_ = new QPushButton("Restart", grpStatus);

    statusLayout->addWidget(lblStatus_);
    statusLayout->addStretch(1);
    statusLayout->addWidget(btnStart_);
    statusLayout->addWidget(btnStop_);
    statusLayout->addWidget(btnRestart_);

    root->addWidget(grpStatus);

    // =====================
    // Settings
    // =====================
    auto *grpSettings = new QGroupBox("Launcher Settings", this);
    auto *settingsLayout = new QHBoxLayout(grpSettings);

    auto *lblInterval = new QLabel("Interval:", grpSettings);
    spinInterval_ = new QSpinBox(grpSettings);
    spinInterval_->setRange(100, 60 * 60 * 1000);
    spinInterval_->setSingleStep(100);
    spinInterval_->setSuffix(" ms");

    settingsLayout->addWidget(lblInterval);
    settingsLayout->addWidget(spinInterval_);
    settingsLayout->addStretch(1);
    btnRemove_ = new QPushButton("Remove", this);
    btnSave_ = new QPushButton("Save", this);
    btnRemove_->setEnabled(false);
    settingsLayout->addWidget(btnRemove_);
    settingsLayout->addWidget(btnSave_);

    root->addWidget(grpSettings);

    // =====================
    // Zones (2x2)
    // =====================
    auto *grpZones = new QGroupBox("Zones", this);
    auto *zonesGrid = new QGridLayout(grpZones);

    auto buildZoneBox = [this](int zone) -> QGroupBox * {
        auto *gb = new QGroupBox(QString("Zone %1").arg(zone), this);
        auto *lay = new QVBoxLayout(gb);

        auto *list = new ZoneListWidget(zone, gb);
        list->setViewMode(QListView::IconMode);
        list->setMovement(QListView::Snap);

        list->setWrapping(false);
        list->setFlow(QListView::LeftToRight);
        list->setSpacing(10);
        list->setIconSize(QSize(48, 48));
        list->setGridSize(QSize(100, 100));
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setDragEnabled(true);
        list->setAcceptDrops(true);
        list->setDropIndicatorShown(true);
        list->setDragDropMode(QAbstractItemView::DragDrop);
        list->setDefaultDropAction(Qt::MoveAction);
        list->setDragDropOverwriteMode(false);

        list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        const int h = list->gridSize().height() + 2 * list->frameWidth()
                      + list->contentsMargins().top() + list->contentsMargins().bottom() + 10;
        list->setFixedHeight(h);

        lay->addWidget(list);
        zoneLists_[zone] = list;

        connect(list, &QListWidget::itemSelectionChanged, this, [this, zone]() { onZoneSelectionChanged(zone); });

        connect(list, &QListWidget::itemClicked, this, [this, zone](QListWidgetItem *it) {
            if (!it) return;
            if (getItemData(it).value("isAdd").toBool()) {
                onAddClicked(zone);
            }
        });

        return gb;
    };

    zonesGrid->addWidget(buildZoneBox(1), 0, 0);
    zonesGrid->addWidget(buildZoneBox(2), 0, 1);
    zonesGrid->addWidget(buildZoneBox(3), 1, 0);
    zonesGrid->addWidget(buildZoneBox(4), 1, 1);

    root->addWidget(grpZones, 0, Qt::AlignTop);
    root->addStretch(1);

    connect(btnRemove_, &QPushButton::clicked, this, [this]() { onRemoveClicked(); });

    connect(btnSave_, &QPushButton::clicked, this, [this]() {
        if (!saveToDisk()) return;
        if (api_ && api_->plugin_restart) api_->plugin_restart(api_->user, kPluginId);
        loadFromDisk();
        refreshStatus();
    });

    connect(btnStart_, &QPushButton::clicked, this, [this]() {
        if (api_ && api_->plugin_start) {
            api_->plugin_start(api_->user, kPluginId);
        }
        refreshStatus();
    });

    connect(btnStop_, &QPushButton::clicked, this, [this]() {
        if (api_ && api_->plugin_stop) {
            api_->plugin_stop(api_->user, kPluginId);
        }
        refreshStatus();
    });

    connect(btnRestart_, &QPushButton::clicked, this, [this]() {
        if (api_ && api_->plugin_restart) {
            api_->plugin_restart(api_->user, kPluginId);
        }
        refreshStatus();
    });

    statusTimer_ = new QTimer(this);
    connect(statusTimer_, &QTimer::timeout, this, [this]() { refreshStatus(); });
    statusTimer_->start(500);

    loadFromDisk();
    refreshStatus();
}

QString LauncherUi::configPath() const {
    const QString base = QCoreApplication::applicationDirPath();
    return QDir(base).filePath(QString("plugins/%1/config.json").arg(QString::fromUtf8(kPluginId)));
}

void LauncherUi::clearAllZones() {
    for (int z = 1; z <= 4; ++z) {
        if (!zoneLists_[z]) continue;
        zoneLists_[z]->clear();
        zoneLists_[z]->setAddItem(nullptr);
    }
    selectedZone_ = -1;
    btnRemove_->setEnabled(false);
}

static QListWidgetItem *makeAddItem(ZoneListWidget *list) {
    auto *add = new QListWidgetItem(makePlusIcon(list, 48), "+");

    QJsonObject obj;
    obj["isAdd"] = true;
    setItemData(add, obj);

    add->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    add->setToolTip("Add new executable");
    add->setSizeHint(list->gridSize());
    return add;
}

static QListWidgetItem *makeAppItem(ZoneListWidget *list, const QString &name, const QString &path, const QString &iconsDirPath) {
    QString hash = getAppHash(path);
    QString iconPath = QDir(iconsDirPath).filePath(hash + ".png");
    QIcon icon = getOrExtractIcon(path, iconsDirPath, hash);

    auto *it = new QListWidgetItem(icon, name);

    QJsonObject obj;
    obj["isAdd"] = false;
    obj["path"] = path;
    obj["name"] = name;
    obj["hash"] = hash;
    obj["icon"] = hash + ".png";
    obj["iconPath"] = iconPath;
    setItemData(it, obj);

    it->setToolTip(path);
    it->setSizeHint(list->gridSize());

    it->setFlags(Qt::ItemIsEnabled
                 | Qt::ItemIsSelectable
                 | Qt::ItemIsDragEnabled
                 | Qt::ItemIsDropEnabled);
    return it;
}

void LauncherUi::rebuildZoneFromItems(int zone, const std::vector<UiItem> &items) {
    auto *list = zoneLists_[zone];
    if (!list) return;

    list->clear();
    list->setAddItem(nullptr);

    const QString iconsDir = getIconsDir(configPath());

    for (const auto &it: items) {
        if (it.path.trimmed().isEmpty()) continue;
        const QString name = it.name.isEmpty() ? QFileInfo(it.path).completeBaseName() : it.name;
        list->addItem(makeAppItem(list, name, it.path, iconsDir));
    }

    auto *add = makeAddItem(list);
    list->addItem(add);
    list->setAddItem(add);
    list->ensureAddItemLast();
}

void LauncherUi::onAddClicked(int zone) {
    auto *list = zoneLists_[zone];
    if (!list) return;

    const QString startDir = QDir::homePath();
    const QString file = QFileDialog::getOpenFileName(
        this, "Select executable", startDir, "Executables (*.exe);;All Files (*.*)"
    );
    if (file.isEmpty()) return;

    const QString exePath = QDir::toNativeSeparators(file);
    const QString name = QFileInfo(file).completeBaseName();
    QString hash = getAppHash(exePath);

    for (int z = 1; z <= 4; ++z) {
        auto *checkList = zoneLists_[z];
        if (!checkList) continue;
        for (int i = 0; i < checkList->count(); ++i) {
            auto *it = checkList->item(i);
            if (!it) continue;

            QJsonObject data = getItemData(it);
            if (data.value("isAdd").toBool()) continue;

            if (data.value("hash").toString() == hash) {
                QMessageBox::information(this, "Launcher", "This executable is already added to Zone " + QString::number(z) + ".");
                return;
            }
        }
    }

    const int insertRow = std::max(0, list->count() - 1);
    auto *appItem = makeAppItem(list, name, exePath, getIconsDir(configPath()));
    list->insertItem(insertRow, appItem);
    list->ensureAddItemLast();
    list->setCurrentItem(appItem);
    onZoneSelectionChanged(zone);
}

void LauncherUi::onRemoveClicked() {
    if (selectedZone_ < 1 || selectedZone_ > 4) return;

    auto *list = zoneLists_[selectedZone_];
    if (!list) return;

    const auto selected = list->selectedItems();
    if (selected.isEmpty()) return;

    auto *it = selected.first();
    if (!it || getItemData(it).value("isAdd").toBool()) return;

    delete list->takeItem(list->row(it));
    list->ensureAddItemLast();

    selectedZone_ = -1;
    btnRemove_->setEnabled(false);
}

void LauncherUi::onZoneSelectionChanged(int zone) {
    if (suppressSelection_) return;

    auto *list = zoneLists_[zone];
    if (!list) return;

    const auto selected = list->selectedItems();
    if (selected.isEmpty()) {
        if (selectedZone_ == zone) {
            selectedZone_ = -1;
            btnRemove_->setEnabled(false);
        }
        return;
    }

    auto *it = selected.first();
    if (!it) return;

    if (getItemData(it).value("isAdd").toBool()) {
        suppressSelection_ = true;
        list->clearSelection();
        suppressSelection_ = false;
        selectedZone_ = -1;
        btnRemove_->setEnabled(false);
        return;
    }

    suppressSelection_ = true;
    for (int z = 1; z <= 4; ++z) {
        if (z == zone) continue;
        if (zoneLists_[z]) zoneLists_[z]->clearSelection();
    }
    suppressSelection_ = false;

    selectedZone_ = zone;
    btnRemove_->setEnabled(true);
}

void LauncherUi::loadFromDisk() {
    clearAllZones();

    QFile f(configPath());
    if (!f.exists()) {
        spinInterval_->setValue(1000);
        for (int z = 1; z <= 4; ++z) rebuildZoneFromItems(z, {});
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Launcher", "Failed to open config file for reading.\n" + configPath());
        spinInterval_->setValue(1000);
        for (int z = 1; z <= 4; ++z) rebuildZoneFromItems(z, {});
        return;
    }

    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Launcher", "Config JSON parse error.\n" + perr.errorString());
        spinInterval_->setValue(1000);
        for (int z = 1; z <= 4; ++z) rebuildZoneFromItems(z, {});
        return;
    }

    const QJsonObject root = doc.object();
    spinInterval_->setValue(root.value("intervalMs").toInt(1000));

    const QJsonArray apps = root.value("apps").toArray();

    struct Tmp {
        int zone;
        int order;
        QString name;
        QString path;
    };
    std::vector<Tmp> tmp;
    tmp.reserve((size_t) apps.size());

    QSet<QString> seenHashes;

    for (const QJsonValue &v: apps) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        const QString path = o.value("path").toString();
        if (path.trimmed().isEmpty()) continue;
        const QString name = o.value("name").toString(QFileInfo(path).completeBaseName());
        QString hash = o.value("hash").toString();
        if (hash.isEmpty()) hash = getAppHash(QDir::toNativeSeparators(path));
        if (seenHashes.contains(hash)) continue;
        seenHashes.insert(hash);
        int zone = o.value("zone").toInt(1);
        int order = o.value("order").toInt(1);
        if (zone < 1) zone = 1;
        if (zone > 4) zone = 4;
        if (order < 1) order = 1;
        tmp.push_back(Tmp{zone, order, name, QDir::toNativeSeparators(path)});
    }

    std::stable_sort(tmp.begin(), tmp.end(), [](const Tmp &a, const Tmp &b) {
        if (a.zone != b.zone) return a.zone < b.zone;
        if (a.order != b.order) return a.order < b.order;
        return false;
    });

    std::array<std::vector<UiItem>, 5> byZone{};
    for (const auto &t: tmp) {
        byZone[t.zone].push_back(UiItem{t.name, t.path});
    }

    for (int z = 1; z <= 4; ++z) rebuildZoneFromItems(z, byZone[z]);
}

bool LauncherUi::saveToDisk() {
    const QFileInfo fi(configPath());
    QDir().mkpath(fi.absolutePath());

    QJsonObject root;
    QFile f(configPath());
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QJsonParseError perr;
        const QJsonDocument oldDoc = QJsonDocument::fromJson(f.readAll(), &perr);
        if (perr.error == QJsonParseError::NoError && oldDoc.isObject()) {
            root = oldDoc.object();
        }
        f.close();
    }

    root.insert("intervalMs", spinInterval_->value());

    QJsonArray apps;
    int nextIndex = 0;

    QSet<QString> seenHashes;

    for (int zone = 1; zone <= 4; ++zone) {
        auto *list = zoneLists_[zone];
        if (!list) continue;

        int order = 1;
        for (int row = 0; row < list->count(); ++row) {
            auto *it = list->item(row);
            if (!it) continue;

            QJsonObject data = getItemData(it);
            if (data.value("isAdd").toBool()) continue;

            const QString hash = data.value("hash").toString();
            if (!hash.isEmpty()) {
                if (seenHashes.contains(hash)) continue;
                seenHashes.insert(hash);
            }

            const QString path = data.value("path").toString();
            if (path.trimmed().isEmpty()) continue;

            const QString name = it->text().isEmpty() ? QFileInfo(path).completeBaseName() : it->text();

            apps.append(QJsonObject{
                {"index", nextIndex++},
                {"name", name},
                {"path", path},
                {"zone", zone},
                {"order", order++},
                {"hash", hash},
                {"icon", data.value("icon").toString()}
            });
        }
    }

    root.insert("apps", apps);

    QSaveFile sf(configPath());
    if (!sf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Launcher", "Failed to open config file for writing.\n" + configPath());
        return false;
    }

    const QJsonDocument outDoc(root);
    sf.write(outDoc.toJson(QJsonDocument::Indented));
    if (!sf.commit()) {
        QMessageBox::warning(this, "Launcher", "Failed to commit config file changes.\n" + configPath());
        return false;
    }
    return true;
}

void LauncherUi::refreshStatus() {
    int32_t st = WA_STATE_MISSING;
    if (api_ && api_->plugin_get_state) {
        st = api_->plugin_get_state(api_->user, kPluginId);
    }
    lblStatus_->setText("Status: " + stateToText(st));

    const bool running = (st == WA_STATE_RUNNING);
    btnStart_->setEnabled(!running);
    btnStop_->setEnabled(running);

    bool canRemove = false;
    if (selectedZone_ >= 1 && selectedZone_ <= 4) {
        auto *list = zoneLists_[selectedZone_];
        if (list && !list->selectedItems().isEmpty()) {
            auto *it = list->selectedItems().first();
            canRemove = it && !getItemData(it).value("isAdd").toBool();
        }
    }
    btnRemove_->setEnabled(canRemove);
}
