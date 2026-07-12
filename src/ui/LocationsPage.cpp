#include "ui/LocationsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QMap>

#include "AppContext.h"
#include "ui/UiKit.h"

using domain::Location;

LocationsPage::LocationsPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    v->addWidget(uikit::pageTitle("Emplacements"));
    v->addWidget(uikit::hint("Hiérarchie libre : Atelier › Armoire A › Tiroir A12 › Boîte ESD… "
                             "« Ajouter un enfant » crée un sous-emplacement sous la sélection."));

    auto* bar = new QHBoxLayout;
    auto* addRoot  = uikit::button("Ajouter (racine)", icons::Glyph::Add);
    auto* addChild = uikit::button("Ajouter un enfant", icons::Glyph::Add, "ghost");
    auto* ren = uikit::button("Renommer", icons::Glyph::Edit, "ghost");
    auto* del = uikit::button("Supprimer", icons::Glyph::Delete, "danger");
    bar->addWidget(addRoot); bar->addWidget(addChild); bar->addWidget(ren); bar->addWidget(del); bar->addStretch(1);
    v->addLayout(bar);

    tree_ = new QTreeWidget;
    tree_->setHeaderLabels({"Emplacement", "id"});
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

int LocationsPage::currentId() const {
    auto* it = tree_->currentItem();
    return it ? it->text(1).toInt() : domain::kNoId;
}

void LocationsPage::refresh() {
    const int keep = currentId();
    tree_->clear();

    QMap<int, QTreeWidgetItem*> items;
    std::vector<Location> locs = ctx_.inventory.listLocations();
    // Première passe : un item par emplacement.
    for (const auto& l : locs) {
        auto* it = new QTreeWidgetItem;
        it->setText(0, QString::fromStdString(l.name));
        it->setText(1, QString::number(l.id));
        items.insert(l.id, it);
    }
    // Seconde passe : rattachement parent/enfant (ordre indépendant).
    for (const auto& l : locs) {
        auto* it = items.value(l.id);
        auto* parent = items.value(l.parentId, nullptr);
        if (parent) parent->addChild(it);
        else tree_->addTopLevelItem(it);
    }
    tree_->expandAll();

    if (keep != domain::kNoId && items.contains(keep))
        tree_->setCurrentItem(items.value(keep));
}

void LocationsPage::addLocation(bool asChild) {
    const domain::Id parentId = asChild ? currentId() : domain::kNoId;

    // Unicité imposée uniquement à la RACINE (un même nom peut se répéter sous
    // des parents différents, ex. « Tiroir 1 » dans deux armoires).
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
        name = QInputDialog::getText(this, "Nouvel emplacement", "Nom :", QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        if (!existsAtRoot(name)) break;
        QMessageBox::warning(this, "Nom déjà utilisé",
            "L'emplacement racine « " + name + " » existe déjà.\nChoisissez un autre nom ou corrigez la saisie.");
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
    const domain::Id id = it->text(1).toInt();
    const QString oldName = it->text(0);

    // Récupère le parent (pour le conserver et savoir si c'est une racine).
    const auto locs = ctx_.inventory.listLocations();
    domain::Id parentId = domain::kNoId;
    for (const auto& e : locs) if (e.id == id) { parentId = e.parentId; break; }

    // Unicité imposée seulement à la racine ; on s'exclut soi-même.
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
        name = QInputDialog::getText(this, "Renommer", "Nom :", QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty() || name == oldName) return;
        if (!existsOtherRoot(name)) break;
        QMessageBox::warning(this, "Nom déjà utilisé",
            "L'emplacement racine « " + name + " » existe déjà.\nChoisissez un autre nom ou corrigez la saisie.");
    }

    Location l;
    l.id = id;
    l.name = name.toStdString();
    l.parentId = parentId;
    ctx_.inventory.saveLocation(l);
    refresh();
}

void LocationsPage::deleteLocation() {
    auto* it = tree_->currentItem();
    if (!it) return;
    const QString msg = it->childCount() > 0
        ? "« " + it->text(0) + " » contient des sous-emplacements qui deviendront orphelins. Supprimer quand même ?"
        : "Supprimer l'emplacement « " + it->text(0) + " » ?";
    if (QMessageBox::question(this, "Supprimer", msg) != QMessageBox::Yes) return;
    ctx_.inventory.removeLocation(it->text(1).toInt());
    refresh();
}
