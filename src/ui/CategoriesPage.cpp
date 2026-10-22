#include "ui/CategoriesPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Language.h"

CategoriesPage::CategoriesPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    v->addWidget(uikit::pageTitle(chui::tr("Catégories")));
    v->addWidget(uikit::hint(chui::tr("Les catégories alimentent le filtre de l'inventaire. Elles sont aussi créées automatiquement en saisissant une fiche.")));

    auto* bar = new QHBoxLayout;
    auto* add = uikit::button(chui::tr("Ajouter"), icons::Glyph::Add);
    auto* ren = uikit::button(chui::tr("Renommer"), icons::Glyph::Edit, "ghost");
    auto* del = uikit::button(chui::tr("Supprimer"), icons::Glyph::Delete, "danger");
    bar->addWidget(add); bar->addWidget(ren); bar->addWidget(del); bar->addStretch(1);
    v->addLayout(bar);

    list_ = new QListWidget;
    v->addWidget(list_, 1);

    connect(add, &QPushButton::clicked, this, &CategoriesPage::addCategory);
    connect(ren, &QPushButton::clicked, this, &CategoriesPage::renameCategory);
    connect(del, &QPushButton::clicked, this, &CategoriesPage::deleteCategory);
    connect(list_, &QListWidget::itemDoubleClicked, this, &CategoriesPage::renameCategory);
}

void CategoriesPage::showEvent(QShowEvent*) { refresh(); }

void CategoriesPage::refresh() {
    list_->clear();
    for (const auto& c : ctx_.inventory.listCategories()) {
        auto* it = new QListWidgetItem(QString::fromStdString(c.name));
        it->setData(Qt::UserRole, QString::fromStdString(c.id));
        list_->addItem(it);
    }
}

void CategoriesPage::addCategory() {
    const auto existing = ctx_.inventory.listCategories();
    auto exists = [&](const QString& v) {
        for (const auto& c : existing)
            if (QString::fromStdString(c.name).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    // Redemande tant que le nom existe déjà (popup bloquant, saisie pré-remplie).
    QString name;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, chui::tr("Nouvelle catégorie"), chui::tr("Nom :"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        if (!exists(name)) break;
        QMessageBox::warning(this, chui::tr("Nom déjà utilisé"),
            chui::tr("La catégorie « ") + name + chui::tr(" » existe déjà.\nChoisissez un autre nom ou corrigez la saisie."));
    }

    domain::Category c;
    c.name = name.toStdString();
    ctx_.inventory.saveCategory(c);
    refresh();
}

void CategoriesPage::renameCategory() {
    auto* it = list_->currentItem();
    if (!it) return;
    const domain::Id id = it->data(Qt::UserRole).toString().toStdString();
    const QString oldText = it->text();

    const auto existing = ctx_.inventory.listCategories();
    // Refuse un doublon avec une AUTRE catégorie (on s'exclut soi-même).
    auto existsOther = [&](const QString& v) {
        for (const auto& c : existing)
            if (c.id != id && QString::fromStdString(c.name).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    QString name = oldText;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, chui::tr("Renommer la catégorie"), chui::tr("Nom :"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty() || name == oldText) return;
        if (!existsOther(name)) break;
        QMessageBox::warning(this, chui::tr("Nom déjà utilisé"),
            chui::tr("La catégorie « ") + name + chui::tr(" » existe déjà.\nChoisissez un autre nom ou corrigez la saisie."));
    }

    const std::string oldName = oldText.toStdString();
    const std::string newName = name.toStdString();

    domain::Category c;
    c.id = id;
    c.name = newName;
    ctx_.inventory.saveCategory(c);

    // Les composants stockent la catégorie en texte (champ libre) : on répercute
    // le renommage pour que l'inventaire reflète le nouveau nom.
    std::vector<domain::Component> changed;
    for (auto& comp : ctx_.components.findAll())
        if (comp.category == oldName) { comp.category = newName; changed.push_back(comp); }
    if (!changed.empty()) ctx_.components.saveAll(changed);

    refresh();
    if (!changed.empty())
        QMessageBox::information(this, chui::tr("Renommé"),
            QString(chui::tr("%1 composant(s) mis à jour.")).arg((int)changed.size()));
}

void CategoriesPage::deleteCategory() {
    auto* it = list_->currentItem();
    if (!it) return;
    if (QMessageBox::question(this, chui::tr("Supprimer"), chui::tr("Supprimer la catégorie « ") + it->text() + chui::tr(" » ?"))
        != QMessageBox::Yes) return;
    ctx_.inventory.removeCategory(it->data(Qt::UserRole).toString().toStdString());
    refresh();
}
