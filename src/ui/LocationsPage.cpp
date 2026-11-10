#include "ui/LocationsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QMap>
#include <QTimer>
#include <QDropEvent>
#include <QTreeWidgetItemIterator>
#include <functional>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Language.h"

using domain::Location;

namespace {
// Tree that never applies the move itself: drag and drop is
// delegated to the page (which writes to the database then rebuilds the tree). This keeps Qt
// and the database from diverging, and ignores order (siblings are not ordered).
class DropTree : public QTreeWidget {
public:
    // (moved item, new parent or nullptr = root)
    std::function<void(QTreeWidgetItem*, QTreeWidgetItem*)> onMove;
protected:
    void dropEvent(QDropEvent* e) override {
        QTreeWidgetItem* src = currentItem();
        QTreeWidgetItem* target = itemAt(e->position().toPoint());
        e->ignore();  // never a native move
        if (!src || !onMove) return;
        QTreeWidgetItem* newParent = nullptr;
        if (target) {
            // On the item = child; between two items = sibling of the target.
            newParent = (dropIndicatorPosition() == OnItem) ? target : target->parent();
        }
        onMove(src, newParent);
    }
};
}  // namespace

LocationsPage::LocationsPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    v->addWidget(uikit::pageTitle(chui::tr("Locations")));
    v->addWidget(uikit::hint(chui::tr("Free hierarchy: Workshop › Cabinet A › Drawer A12 › ESD box... \"Add child\" creates a sub-location under the selection.")));
    v->addWidget(uikit::hint(chui::tr("Drag and drop: drop a location onto another to make it a child, or onto empty space to make it a root.")));

    auto* bar = new QHBoxLayout;
    auto* addRoot  = uikit::button(chui::tr("Add (root)"), icons::Glyph::Add);
    auto* addChild = uikit::button(chui::tr("Add child"), icons::Glyph::Add, "ghost");
    auto* ren = uikit::button(chui::tr("Rename"), icons::Glyph::Edit, "ghost");
    auto* del = uikit::button(chui::tr("Delete"), icons::Glyph::Delete, "danger");
    bar->addWidget(addRoot); bar->addWidget(addChild); bar->addWidget(ren); bar->addWidget(del); bar->addStretch(1);
    v->addLayout(bar);

    auto* dt = new DropTree;
    tree_ = dt;
    tree_->setDragDropMode(QAbstractItemView::InternalMove);
    tree_->setDropIndicatorShown(true);
    dt->onMove = [this](QTreeWidgetItem* s, QTreeWidgetItem* p) { moveLocation(s, p); };
    tree_->setHeaderLabels({chui::tr("Location"), "id"});
    tree_->setColumnHidden(1, true);
    tree_->header()->setStretchLastSection(true);
    v->addWidget(tree_, 1);

    connect(addRoot,  &QPushButton::clicked, this, [this] { addLocation(false); });
    connect(addChild, &QPushButton::clicked, this, [this] { addLocation(true); });
    connect(ren, &QPushButton::clicked, this, &LocationsPage::renameLocation);
    connect(del, &QPushButton::clicked, this, &LocationsPage::deleteLocation);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &LocationsPage::renameLocation);
}

void LocationsPage::showEvent(QShowEvent*) { refresh(); }

domain::Id LocationsPage::currentId() const {
    auto* it = tree_->currentItem();
    return it ? it->text(1).toStdString() : domain::kNoId;
}

void LocationsPage::refresh() {
    const domain::Id keep = currentId();
    tree_->clear();

    // Identifiers are UUIDs (strings) since v0.2: QString key.
    QMap<QString, QTreeWidgetItem*> items;
    std::vector<Location> locs = ctx_.inventory.listLocations();
    // First pass: one item per location.
    for (const auto& l : locs) {
        auto* it = new QTreeWidgetItem;
        it->setText(0, QString::fromStdString(l.name));
        it->setText(1, QString::fromStdString(l.id));
        items.insert(QString::fromStdString(l.id), it);
    }
    // Second pass: parent/child attachment (order independent).
    for (const auto& l : locs) {
        auto* it = items.value(QString::fromStdString(l.id));
        auto* parent = items.value(QString::fromStdString(l.parentId), nullptr);
        if (parent) parent->addChild(it);
        else tree_->addTopLevelItem(it);
    }
    tree_->expandAll();

    if (!keep.empty() && items.contains(QString::fromStdString(keep)))
        tree_->setCurrentItem(items.value(QString::fromStdString(keep)));
}

void LocationsPage::addLocation(bool asChild) {
    const domain::Id parentId = asChild ? currentId() : domain::kNoId;

    // Uniqueness is enforced only at the ROOT (the same name may repeat under
    // different parents, e.g. "Drawer 1" in two cabinets).
    auto existsAtRoot = [&](const QString& v) {
        if (asChild) return false;
        for (const auto& e : ctx_.inventory.listLocations())
            if (e.parentId == domain::kNoId &&
                QString::fromStdString(e.name).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    QString name;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, chui::tr("New location"), chui::tr("Name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        if (!existsAtRoot(name)) break;
        QMessageBox::warning(this, chui::tr("Name already used"),
            chui::tr("The root location \"%1\" already exists.\nChoose another name or correct your entry.").arg(name));
    }

    Location l;
    l.name = name.toStdString();
    l.parentId = parentId;
    ctx_.inventory.saveLocation(l);
    refresh();
}

void LocationsPage::renameLocation() {
    auto* it = tree_->currentItem();
    if (!it) return;
    const domain::Id id = it->text(1).toStdString();
    const QString oldName = it->text(0);

    // Gets the parent (to keep it and to know whether this is a root).
    const auto locs = ctx_.inventory.listLocations();
    domain::Id parentId = domain::kNoId;
    for (const auto& e : locs) if (e.id == id) { parentId = e.parentId; break; }

    // Uniqueness enforced only at the root; the location itself is excluded.
    auto existsOtherRoot = [&](const QString& v) {
        if (parentId != domain::kNoId) return false;
        for (const auto& e : locs)
            if (e.id != id && e.parentId == domain::kNoId &&
                QString::fromStdString(e.name).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    QString name = oldName;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, chui::tr("Rename"), chui::tr("Name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty() || name == oldName) return;
        if (!existsOtherRoot(name)) break;
        QMessageBox::warning(this, chui::tr("Name already used"),
            chui::tr("The root location \"%1\" already exists.\nChoose another name or correct your entry.").arg(name));
    }

    Location l;
    l.id = id;
    l.name = name.toStdString();
    l.parentId = parentId;
    ctx_.inventory.saveLocation(l);
    refresh();
}

void LocationsPage::moveLocation(QTreeWidgetItem* src, QTreeWidgetItem* newParent) {
    if (src->parent() == newParent) return;  // nothing to change
    // Forbidden: dropping onto itself or onto one of its descendants (cycle).
    for (auto* a = newParent; a; a = a->parent())
        if (a == src) return;

    const domain::Id id = src->text(1).toStdString();
    const domain::Id parentId = newParent ? newParent->text(1).toStdString() : domain::kNoId;
    const QString name = src->text(0);

    // Same rule as at creation: unique name only at the root.
    if (!newParent) {
        for (const auto& e : ctx_.inventory.listLocations())
            if (e.id != id && e.parentId == domain::kNoId &&
                QString::fromStdString(e.name).compare(name, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, chui::tr("Name already used"),
                    chui::tr("The root location \"%1\" already exists.\nChoose another name or correct your entry.").arg(name));
                return;
            }
    }

    Location l;
    l.id = id;
    l.name = name.toStdString();
    l.parentId = parentId;
    ctx_.inventory.saveLocation(l);
    // Deferred rebuild: we are still inside the tree's dropEvent.
    QTimer::singleShot(0, this, [this, id] {
        refresh();
        for (QTreeWidgetItemIterator it(tree_); *it; ++it)
            if ((*it)->text(1).toStdString() == id) { tree_->setCurrentItem(*it); break; }
    });
}

void LocationsPage::deleteLocation() {
    auto* it = tree_->currentItem();
    if (!it) return;
    const QString msg = it->childCount() > 0
        ? chui::tr("\"%1\" contains sub-locations that will become orphaned. Delete anyway?").arg(it->text(0))
        : chui::tr("Delete the location \"%1\"?").arg(it->text(0));
    if (QMessageBox::question(this, chui::tr("Delete"), msg) != QMessageBox::Yes) return;
    ctx_.inventory.removeLocation(it->text(1).toStdString());
    refresh();
}
