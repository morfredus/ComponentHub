#include "ui/InventoryPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QPixmap>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QShortcut>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <set>
#include <algorithm>
#include <map>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/ComponentDialog.h"
#include "ui/LabelDialog.h"
#include "ui/Language.h"
#include "platform/Clock.h"

using namespace domain;

namespace {
QString statusLabel(const std::string& s) {
    if (s == "validated")  return chui::tr("Validated");
    if (s == "validating") return chui::tr("Validating");
    if (s == "defective")  return chui::tr("Defective");
    if (s == "archived")   return chui::tr("Archived");
    return chui::tr("To test");
}

// Readable path of a location: "Workshop › Cabinet A › Drawer 3".
QString locationPath(const std::map<Id, Location>& byId, Id id) {
    QString p; int guard = 0;  // guard against a loop caused by corrupted parents
    while (id != kNoId && guard++ < 64) {
        auto it = byId.find(id); if (it == byId.end()) break;
        const QString n = QString::fromStdString(it->second.name);
        p = p.isEmpty() ? n : n + " › " + p;
        id = it->second.parentId;
    }
    return p;
}

// Cell sorted numerically (not alphabetically): "10" must come
// after "9", and "6.90 €" is compared on its value, not on its text.
class NumericItem : public QTableWidgetItem {
public:
    NumericItem(const QString& text, double key) : QTableWidgetItem(text), key_(key) {}
    bool operator<(const QTableWidgetItem& other) const override {
        if (auto* n = dynamic_cast<const NumericItem*>(&other)) return key_ < n->key_;
        return QTableWidgetItem::operator<(other);
    }
private:
    double key_;
};

// Text cell sorted case-insensitively: "abc" is ranked with
// "ABC" (Qt's raw sort places all uppercase letters before lowercase ones).
class TextItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem& other) const override {
        return text().compare(other.text(), Qt::CaseInsensitive) < 0;
    }
};
} // namespace

InventoryPage::InventoryPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);

    auto* header = new QHBoxLayout;
    header->addWidget(uikit::pageTitle(chui::tr("Inventory")));
    header->addStretch(1);
    auto* add = uikit::button(chui::tr("Add"), icons::Glyph::Add);
    add->setToolTip(chui::tr("New record (Ctrl+N)"));
    connect(add, &QPushButton::clicked, this, &InventoryPage::addComponent);
    header->addWidget(add);
    v->addLayout(header);

    auto* bar = new QHBoxLayout;
    search_ = new QLineEdit; search_->setObjectName("search");
    search_->setPlaceholderText(chui::tr("Search (name, reference, manufacturer, notes...)"));
    search_->setClearButtonEnabled(true);
    kind_ = new QComboBox;
    kind_->addItem(chui::tr("All objects"), -1);
    kind_->addItem(chui::tr("Components"), int(ComponentKind::Component));
    kind_->addItem(chui::tr("Modules"), int(ComponentKind::Module));
    kind_->addItem(chui::tr("Tools"), int(ComponentKind::Tool));
    kind_->addItem(chui::tr("Consumables"), int(ComponentKind::Consumable));
    lowStock_ = new QCheckBox(chui::tr("Low stock"));
    location_ = new QComboBox;
    category_ = new QComboBox;
    status_ = new QComboBox;
    status_->addItem(chui::tr("All statuses"), "");
    for (const char* k : {"to_test", "validating", "validated", "defective", "archived"})
        status_->addItem(statusLabel(k), k);
    location_->setMaximumWidth(260);
    category_->setMaximumWidth(200);
    bar->addWidget(search_, 1);
    bar->addWidget(kind_);
    bar->addWidget(location_);
    bar->addWidget(category_);
    bar->addWidget(status_);
    bar->addWidget(lowStock_);
    v->addLayout(bar);

    table_ = new QTableWidget(0, 8);
    table_->setHorizontalHeaderLabels({chui::tr("Name"), chui::tr("Reference"), chui::tr("Type"), chui::tr("Category"), chui::tr("Location"), chui::tr("Qty"), chui::tr("Price"), chui::tr("Status")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(0, 220);
    // Sorting by column when clicking the header (Qty and Price sorted numerically).
    // Default order: Name column, ascending (digits then A-Z). Without this,
    // Qt starts in descending order (Z-A then digits).
    table_->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
    table_->setSortingEnabled(true);
    table_->horizontalHeader()->setSortIndicatorShown(true);
    table_->horizontalHeader()->setSectionsClickable(true);
    v->addWidget(table_, 1);

    connect(search_, &QLineEdit::textChanged, this, &InventoryPage::refresh);
    connect(kind_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InventoryPage::refresh);
    connect(lowStock_, &QCheckBox::toggled, this, &InventoryPage::refresh);
    for (auto* cb : {location_, category_, status_})
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InventoryPage::refresh);

    // Right-click menu on a row.
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &InventoryPage::showContextMenu);

    // Keyboard shortcuts: page-wide (Ctrl+N, Ctrl+F) or specific to the table.
    auto* newSc = new QShortcut(QKeySequence::New, this);
    newSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newSc, &QShortcut::activated, this, &InventoryPage::addComponent);
    auto* findSc = new QShortcut(QKeySequence::Find, this);
    findSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findSc, &QShortcut::activated, this, [this] { search_->setFocus(); search_->selectAll(); });
    auto onTable = [this](const QKeySequence& k, auto fn) {
        auto* sc = new QShortcut(k, table_);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    onTable(Qt::Key_Delete, [this] { deleteCurrent(); });
    onTable(Qt::Key_Return, [this] { editCurrent(); });
    onTable(Qt::Key_Enter,  [this] { editCurrent(); });
    onTable(Qt::Key_Plus,   [this] { adjustStock(+1); });
    onTable(Qt::Key_Minus,  [this] { adjustStock(-1); });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { editComponentRow(row); });

    // Event filter to detect mouse hover and show the tooltip with a photo
    table_->viewport()->installEventFilter(this);

    // Action shortcuts via double-click; contextual buttons at the bottom.
    auto* actions = new QHBoxLayout;
    auto* labels = uikit::button(chui::tr("Labels"), icons::Glyph::Categories, "ghost");
    auto* edit = uikit::button(chui::tr("Edit"), icons::Glyph::Edit, "ghost");
    auto* del = uikit::button(chui::tr("Delete"), icons::Glyph::Delete, "danger");
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [labels, edit, this] {
                const int selected = table_->selectionModel()->selectedRows().size();
                labels->setEnabled(selected > 0);
                edit->setEnabled(selected <= 1);
            });
    connect(labels, &QPushButton::clicked, this, [this] {
        const auto rows = table_->selectionModel()->selectedRows();
        if (rows.isEmpty()) return;
        std::vector<domain::Component> components;
        for (const auto& row : rows) {
            const auto* item = table_->item(row.row(), 0);
            if (!item) continue;
            if (const auto component = ctx_.components.findById(
                    item->data(Qt::UserRole).toString().toStdString()))
                components.push_back(*component);
        }
        if (!components.empty()) {
            LabelDialog dialog(components, this);
            dialog.exec();
        }
    });
    connect(edit, &QPushButton::clicked, this, &InventoryPage::editCurrent);
    connect(del, &QPushButton::clicked, this, &InventoryPage::deleteCurrent);
    labels->setEnabled(false);
    actions->addStretch(1); actions->addWidget(labels); actions->addWidget(edit); actions->addWidget(del);
    v->addLayout(actions);
}

void InventoryPage::showEvent(QShowEvent*) { reloadFilters(); refresh(); }

void InventoryPage::reloadFilters() {
    // Rebuilds the lists (locations, categories) while keeping the current choice.
    std::map<Id, Location> byId;
    for (const auto& l : ctx_.inventory.listLocations()) byId[l.id] = l;
    std::vector<std::pair<QString, Id>> locs;
    for (const auto& [id, l] : byId) locs.emplace_back(locationPath(byId, id), id);
    std::sort(locs.begin(), locs.end(), [](const auto& a, const auto& b) {
        return a.first.compare(b.first, Qt::CaseInsensitive) < 0; });

    const QVariant keepLoc = location_->currentData();
    const QVariant keepCat = category_->currentData();
    QSignalBlocker b1(location_), b2(category_);
    location_->clear();
    location_->addItem(chui::tr("All locations"), "");
    for (const auto& [p, id] : locs) location_->addItem(p, QString::fromStdString(id));
    location_->setCurrentIndex(std::max(0, location_->findData(keepLoc)));
    category_->clear();
    category_->addItem(chui::tr("All categories"), "");
    for (const auto& c : ctx_.inventory.listCategories())
        category_->addItem(QString::fromStdString(c.name), QString::fromStdString(c.name));
    category_->setCurrentIndex(std::max(0, category_->findData(keepCat)));
}

void InventoryPage::refresh() {
    // Location index -> readable path.
    std::map<Id, Location> byId;
    for (const auto& l : ctx_.inventory.listLocations()) byId[l.id] = l;
    auto path = [&](Id id) { return locationPath(byId, id); };

    ComponentFilter filter;
    filter.query = search_->text().trimmed().toStdString();
    filter.onlyLowStock = lowStock_->isChecked();
    const int k = kind_->currentData().toInt();
    if (k >= 0) { filter.hasKind = true; filter.kind = ComponentKind(k); }
    filter.category = category_->currentData().toString().toStdString();
    filter.status = status_->currentData().toString().toStdString();

    // Location filter: the chosen location AND all its sub-locations
    // ("what is in this cabinet?"), hence the post-filtering here.
    std::set<Id> locSet;
    const Id wantLoc = location_->currentData().toString().toStdString();
    if (!wantLoc.empty()) {
        locSet.insert(wantLoc);
        for (bool grew = true; grew;) {  // fixed point: adds children until nothing changes
            grew = false;
            for (const auto& [id, l] : byId)
                if (locSet.count(l.parentId) && locSet.insert(id).second) grew = true;
        }
    }

    auto items = ctx_.inventory.listComponents(filter);
    if (!wantLoc.empty())
        items.erase(std::remove_if(items.begin(), items.end(),
            [&](const Component& c) { return !locSet.count(c.locationId); }), items.end());
    // Sorting is disabled while filling (otherwise each insertion
    // reorders the table); it is re-enabled afterwards, which applies the
    // current sort order chosen by the user.
    table_->setSortingEnabled(false);
    table_->setRowCount(0);
    for (const auto& c : items) {
        int r = table_->rowCount();
        table_->insertRow(r);
        auto* name = new TextItem(QString::fromStdString(c.name));
        name->setData(Qt::UserRole, QString::fromStdString(c.id));
        table_->setItem(r, 0, name);
        table_->setItem(r, 1, new TextItem(QString::fromStdString(c.reference)));
        // "Type" column = the Type field (reference list: Resistor, Sensor...),
        // not the kind (which stays available through the drop-down filter).
        table_->setItem(r, 2, new TextItem(QString::fromStdString(c.type)));
        table_->setItem(r, 3, new TextItem(QString::fromStdString(c.category)));
        table_->setItem(r, 4, new TextItem(path(c.locationId)));
        auto* qty = new NumericItem(QString::number(c.quantity), c.quantity);
        if (c.minStock > 0 && c.quantity <= c.minStock)
            qty->setForeground(QColor(Theme::instance().color("danger")));
        table_->setItem(r, 5, qty);
        table_->setItem(r, 6, new NumericItem(QString::number(c.price, 'f', 2) + " " + chui::currencySymbol(chui::currency()), c.price));
        table_->setItem(r, 7, new TextItem(statusLabel(c.status)));
    }
    table_->setSortingEnabled(true);
}

void InventoryPage::addComponent() {
    ComponentDialog dlg(ctx_, kNoId, this);
    if (dlg.exec() == QDialog::Accepted) refresh();
}

void InventoryPage::editComponentRow(int row) {
    if (row < 0) return;
    const Id id = table_->item(row, 0)->data(Qt::UserRole).toString().toStdString();
    ComponentDialog dlg(ctx_, id, this);
    if (dlg.exec() == QDialog::Accepted) refresh();
}

void InventoryPage::editCurrent() { editComponentRow(table_->currentRow()); }

void InventoryPage::deleteCurrent() {
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;

    std::vector<Id> ids;
    QStringList names;
    for (const auto& index : rows) {
        auto* item = table_->item(index.row(), 0);
        if (!item) continue;
        ids.push_back(item->data(Qt::UserRole).toString().toStdString());
        names << item->text();
    }
    if (ids.empty()) return;

    const QString prompt = ids.size() == 1
        ? chui::tr("Delete \"%1\"?").arg(names.first())
        : QString(chui::tr("Delete %1 selected components?")).arg(ids.size());
    if (QMessageBox::question(this, chui::tr("Delete"), prompt) != QMessageBox::Yes) return;
    for (const auto& id : ids) ctx_.inventory.removeComponent(id);
    refresh();
}

Id InventoryPage::currentId() const {
    const int r = table_->currentRow();
    return r < 0 ? kNoId : table_->item(r, 0)->data(Qt::UserRole).toString().toStdString();
}

void InventoryPage::selectById(const Id& id) {
    for (int r = 0; r < table_->rowCount(); ++r)
        if (table_->item(r, 0)->data(Qt::UserRole).toString().toStdString() == id) {
            table_->selectRow(r);
            table_->setCurrentCell(r, 0);
            return;
        }
}

std::optional<Component> InventoryPage::findComponent(const Id& id) const {
    for (auto& c : ctx_.inventory.listComponents()) if (c.id == id) return c;
    return std::nullopt;
}

// Stock movement: "+ / −" one unit (delta) or absolute value (setTo >= 0).
void InventoryPage::adjustStock(int delta, int setTo) {
    const Id id = currentId();
    auto c = id.empty() ? std::nullopt : findComponent(id);
    if (!c) return;
    StockMovement m;
    m.componentId = id;
    m.timestamp = chdesktop::nowIso();
    if (setTo >= 0) { m.type = StockMovementType::Correction; m.quantity = setTo; }
    else if (delta > 0) { m.type = StockMovementType::In; m.quantity = delta; }
    else {
        if (c->quantity <= 0) return;  // never a negative stock
        m.type = StockMovementType::Out; m.quantity = -delta;
    }
    ctx_.inventory.recordMovement(m);
    refresh();
    selectById(id);
}

void InventoryPage::setQuantityCurrent() {
    auto c = findComponent(currentId());
    if (!c) return;
    bool ok = false;
    const int q = QInputDialog::getInt(this, chui::tr("Set quantity"), chui::tr("Quantity:"),
                                       c->quantity, 0, 1000000, 1, &ok);
    if (ok && q != c->quantity) adjustStock(0, q);
}

void InventoryPage::duplicateCurrent() {
    auto c = findComponent(currentId());
    if (!c) return;
    Component copy = *c;
    copy.id = kNoId;               // new identity
    copy.meta = SyncMeta();        // nouvelle enveloppe de synchronisation
    copy.name += chui::tr(" (copy)").toStdString();
    // Documents are not copied (they stay on the original record).
    const Component saved = ctx_.inventory.saveComponent(copy);
    refresh();
    selectById(saved.id);
}

void InventoryPage::moveCurrent() {
    const Id id = currentId();
    auto c = id.empty() ? std::nullopt : findComponent(id);
    if (!c) return;
    // List of locations already built for the filter (entry 0 = "all").
    QStringList names{chui::tr("(none)")};
    std::vector<Id> ids{kNoId};
    int cur = 0;
    for (int i = 1; i < location_->count(); ++i) {
        names << location_->itemText(i);
        ids.push_back(location_->itemData(i).toString().toStdString());
        if (ids.back() == c->locationId) cur = i;
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, chui::tr("Change location"),
        chui::tr("New location:"), names, cur, false, &ok);
    if (!ok) return;
    c->locationId = ids[names.indexOf(pick)];
    ctx_.inventory.saveComponent(*c);
    refresh();
    selectById(id);
}

// First "datasheet" record of the link type (an imported file is not opened here).
static const Document* findDatasheetLink(const std::vector<Document>& docs) {
    for (const auto& d : docs)
        if (!d.isFile() && !d.url.empty() && d.category == "datasheet") return &d;
    return nullptr;
}

void InventoryPage::openDatasheet() {
    const auto docs = ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, currentId());
    if (const Document* d = findDatasheetLink(docs))
        QDesktopServices::openUrl(QUrl::fromUserInput(QString::fromStdString(d->url)));
}

void InventoryPage::showContextMenu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    if (row < 0) return;
    table_->selectRow(row);
    table_->setCurrentCell(row, 0);

    const auto docs = ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, currentId());
    const bool hasSheet = findDatasheetLink(docs) != nullptr;

    QMenu m(this);
    m.addAction(chui::tr("Edit"), this, [this] { editCurrent(); });
    m.addAction(chui::tr("Duplicate"), this, [this] { duplicateCurrent(); });
    m.addSeparator();
    m.addAction(chui::tr("Add 1 (+)"), this, [this] { adjustStock(+1); });
    m.addAction(chui::tr("Remove 1 (−)"), this, [this] { adjustStock(-1); });
    m.addAction(chui::tr("Set quantity..."), this, [this] { setQuantityCurrent(); });
    m.addAction(chui::tr("Change location..."), this, [this] { moveCurrent(); });
    m.addAction(chui::tr("Open datasheet"), this, [this] { openDatasheet(); })->setEnabled(hasSheet);
    m.addSeparator();
    m.addAction(chui::tr("Delete"), this, [this] { deleteCurrent(); });
    m.exec(table_->viewport()->mapToGlobal(pos));
}

bool InventoryPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == table_->viewport() && event->type() == QEvent::ToolTip) {
        const QHelpEvent* helpEvent = static_cast<const QHelpEvent*>(event);
        const QModelIndex index = table_->indexAt(helpEvent->pos());
        if (index.isValid()) {
            const QString tooltip = buildPhotoTooltip(index.row());
            if (!tooltip.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos(), tooltip, this);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QString InventoryPage::buildPhotoTooltip(int row) {
    if (row < 0) return QString();
    const Id id = table_->item(row, 0)->data(Qt::UserRole).toString().toStdString();
    if (id.empty()) return QString();

    // Get the documents of the component (photos)
    const auto documents = ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, id);

    // Look for the first photo (image file)
    for (const auto& doc : documents) {
        if (!doc.isFile()) continue;

        // Check whether it is an image, by extension
        const QString fileName = QString::fromStdString(doc.fileName);
        const QString ext = fileName.section('.', -1).toLower();
        if (ext != "jpg" && ext != "jpeg" && ext != "png" && ext != "gif" && ext != "bmp" && ext != "webp")
            continue;

        // Check whether the content is available
        if (!ctx_.attachments.has(doc.blobHash)) continue;

        // Load the image from the store
        std::string bytes;
        if (!ctx_.attachments.get(doc.blobHash, bytes)) continue;

        QPixmap pixmap;
        if (!pixmap.loadFromData(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<int>(bytes.size())))
            continue;

        // Resize the image for the tooltip (max 400x400)
        const int maxSize = 400;
        if (pixmap.width() > maxSize || pixmap.height() > maxSize) {
            pixmap = pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // Save the image to a temporary file to show it in the tooltip
        const QString tmpDir = QDir::tempPath() + "/componenthub-tooltips";
        QDir().mkpath(tmpDir);
        const QString tmpPath = tmpDir + "/" + QString::fromStdString(doc.blobHash) + ".png";
        if (pixmap.save(tmpPath)) {
            // Show the tooltip with the image
            const QString html = QString("<img src=\"%1\" width=\"%2\" height=\"%3\">")
                .arg(tmpPath)
                .arg(pixmap.width())
                .arg(pixmap.height());
            return html;
        }
    }

    return QString();
}
