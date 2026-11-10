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
    v->addWidget(uikit::pageTitle(chui::tr("Categories")));
    v->addWidget(uikit::hint(chui::tr("Categories feed the inventory filter. They are also created automatically when entering a component.")));

    auto* bar = new QHBoxLayout;
    auto* add = uikit::button(chui::tr("Add"), icons::Glyph::Add);
    auto* ren = uikit::button(chui::tr("Rename"), icons::Glyph::Edit, "ghost");
    auto* del = uikit::button(chui::tr("Delete"), icons::Glyph::Delete, "danger");
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

    // Asks again while the name already exists (blocking popup, pre-filled input).
    QString name;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, chui::tr("New category"), chui::tr("Name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        if (!exists(name)) break;
        QMessageBox::warning(this, chui::tr("Name already used"),
            chui::tr("The category \"%1\" already exists.\nChoose another name or correct your entry.").arg(name));
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
    // Rejects a duplicate of ANOTHER category (the category itself is excluded).
    auto existsOther = [&](const QString& v) {
        for (const auto& c : existing)
            if (c.id != id && QString::fromStdString(c.name).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    QString name = oldText;
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, chui::tr("Rename category"), chui::tr("Name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty() || name == oldText) return;
        if (!existsOther(name)) break;
        QMessageBox::warning(this, chui::tr("Name already used"),
            chui::tr("The category \"%1\" already exists.\nChoose another name or correct your entry.").arg(name));
    }

    const std::string oldName = oldText.toStdString();
    const std::string newName = name.toStdString();

    domain::Category c;
    c.id = id;
    c.name = newName;
    ctx_.inventory.saveCategory(c);

    // Components store the category as text (free field): the rename is
    // propagated so that the inventory reflects the new name.
    std::vector<domain::Component> changed;
    for (auto& comp : ctx_.components.findAll())
        if (comp.category == oldName) { comp.category = newName; changed.push_back(comp); }
    if (!changed.empty()) ctx_.components.saveAll(changed);

    refresh();
    if (!changed.empty())
        QMessageBox::information(this, chui::tr("Renamed"),
            QString(chui::tr("%1 component(s) updated.")).arg((int)changed.size()));
}

void CategoriesPage::deleteCategory() {
    auto* it = list_->currentItem();
    if (!it) return;
    if (QMessageBox::question(this, chui::tr("Delete"), chui::tr("Delete the category \"%1\"?").arg(it->text()))
        != QMessageBox::Yes) return;
    ctx_.inventory.removeCategory(it->data(Qt::UserRole).toString().toStdString());
    refresh();
}
