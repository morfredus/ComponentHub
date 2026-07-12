#include "ui/ProjectsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>

#include "AppContext.h"
#include "ui/UiKit.h"

using namespace domain;

ProjectsPage::ProjectsPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    v->addWidget(uikit::pageTitle("Projets"));

    auto* split = new QHBoxLayout;
    v->addLayout(split, 1);

    // Colonne gauche : liste + actions.
    auto* left = new QVBoxLayout;
    projects_ = new QListWidget;
    projects_->setFixedWidth(260);
    left->addWidget(projects_, 1);
    auto* pAdd = uikit::button("Nouveau", icons::Glyph::Add);
    auto* pEdit = uikit::button("Modifier", icons::Glyph::Edit, "ghost");
    auto* pDel = uikit::button("Supprimer", icons::Glyph::Delete, "danger");
    auto* pbar = new QHBoxLayout; pbar->addWidget(pAdd); pbar->addWidget(pEdit); pbar->addWidget(pDel);
    left->addLayout(pbar);
    split->addLayout(left);

    // Colonne droite : détail + nomenclature.
    auto* right = new QVBoxLayout;
    detailHeader_ = new QLabel("Sélectionnez un projet");
    detailHeader_->setObjectName("pageTitle");
    detailMeta_ = uikit::hint("");
    right->addWidget(detailHeader_);
    right->addWidget(detailMeta_);

    auto* bomBar = new QHBoxLayout;
    bomBar->addWidget(new QLabel("Nomenclature (BOM)"));
    bomBar->addStretch(1);
    auto* bAdd = uikit::button("Ajouter un élément", icons::Glyph::Add, "ghost");
    auto* bDel = uikit::button("Retirer", icons::Glyph::Delete, "danger");
    bomBar->addWidget(bAdd); bomBar->addWidget(bDel);
    right->addLayout(bomBar);

    bom_ = new QTableWidget(0, 5);
    bom_->setHorizontalHeaderLabels({"Élément", "Référence", "Requis", "Dispo", "Manque"});
    bom_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bom_->setSelectionBehavior(QAbstractItemView::SelectRows);
    bom_->verticalHeader()->setVisible(false);
    bom_->horizontalHeader()->setStretchLastSection(true);
    bom_->setColumnWidth(0, 220);
    right->addWidget(bom_, 1);

    missing_ = uikit::hint("");
    right->addWidget(missing_);
    split->addLayout(right, 1);

    connect(projects_, &QListWidget::currentRowChanged, this, [this](int) { showDetail(); });
    connect(pAdd, &QPushButton::clicked, this, &ProjectsPage::addProject);
    connect(pEdit, &QPushButton::clicked, this, &ProjectsPage::editProject);
    connect(pDel, &QPushButton::clicked, this, &ProjectsPage::deleteProject);
    connect(bAdd, &QPushButton::clicked, this, &ProjectsPage::addBomLine);
    connect(bDel, &QPushButton::clicked, this, &ProjectsPage::removeBomLine);
}

void ProjectsPage::showEvent(QShowEvent*) { refreshProjects(); }

Id ProjectsPage::currentProjectId() const {
    auto* it = projects_->currentItem();
    return it ? it->data(Qt::UserRole).toInt() : kNoId;
}

void ProjectsPage::refreshProjects() {
    const Id keep = currentProjectId();
    projects_->clear();
    for (const auto& p : ctx_.projects_service.listProjects()) {
        auto* it = new QListWidgetItem(QString::fromStdString(p.name));
        it->setData(Qt::UserRole, p.id);
        projects_->addItem(it);
        if (p.id == keep) projects_->setCurrentItem(it);
    }
    if (projects_->currentRow() < 0 && projects_->count() > 0) projects_->setCurrentRow(0);
    showDetail();
}

void ProjectsPage::showDetail() {
    const Id id = currentProjectId();
    if (id == kNoId) {
        detailHeader_->setText("Aucun projet");
        detailMeta_->setText("");
        bom_->setRowCount(0);
        missing_->setText("");
        return;
    }
    auto proj = ctx_.projects_service.getProject(id);
    if (!proj) return;
    detailHeader_->setText(QString::fromStdString(proj->name));
    QStringList meta;
    if (!proj->version.empty())  meta << "v" + QString::fromStdString(proj->version);
    if (!proj->status.empty())   meta << QString::fromStdString(proj->status);
    if (!proj->firmware.empty()) meta << "firmware: " + QString::fromStdString(proj->firmware);
    if (!proj->gitRepo.empty())  meta << QString::fromStdString(proj->gitRepo);
    QString desc = QString::fromStdString(proj->description);
    detailMeta_->setText(meta.join("  ·  ") + (desc.isEmpty() ? "" : "\n" + desc));

    const auto bom = ctx_.projects_service.bom(id);
    bom_->setRowCount(0);
    int totalMissing = 0;
    for (const auto& line : bom) {
        int r = bom_->rowCount();
        bom_->insertRow(r);
        QString label = QString::fromStdString(line.componentName);
        if (!line.inInventory) label += "  (hors inventaire)";
        auto* first = new QTableWidgetItem(label);
        first->setData(Qt::UserRole, line.id);
        bom_->setItem(r, 0, first);
        bom_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(line.reference)));
        bom_->setItem(r, 2, new QTableWidgetItem(QString::number(line.quantityRequired)));
        bom_->setItem(r, 3, new QTableWidgetItem(QString::number(line.quantityAvailable)));
        auto* miss = new QTableWidgetItem(QString::number(line.quantityMissing));
        if (line.quantityMissing > 0) miss->setForeground(QColor(Theme::instance().color("danger")));
        bom_->setItem(r, 4, miss);
        totalMissing += line.quantityMissing;
    }
    missing_->setText(totalMissing == 0
        ? "✓ Tous les composants nécessaires sont disponibles."
        : QString("⚠ %1 élément(s) manquant(s) au total.").arg(totalMissing));
}

// Boîte d'édition d'un projet (champs texte).
static bool editProjectDialog(QWidget* parent, Project& p) {
    QDialog dlg(parent);
    dlg.setWindowTitle(p.id == kNoId ? "Nouveau projet" : "Modifier le projet");
    dlg.resize(460, 420);
    auto* f = new QFormLayout(&dlg);
    auto* name = new QLineEdit(QString::fromStdString(p.name));
    auto* version = new QLineEdit(QString::fromStdString(p.version));
    auto* firmware = new QLineEdit(QString::fromStdString(p.firmware));
    auto* git = new QLineEdit(QString::fromStdString(p.gitRepo));
    auto* status = new QLineEdit(QString::fromStdString(p.status));
    auto* desc = new QPlainTextEdit(QString::fromStdString(p.description));
    auto* notes = new QPlainTextEdit(QString::fromStdString(p.notes));
    f->addRow("Nom *", name);
    f->addRow("Version", version);
    f->addRow("Firmware", firmware);
    f->addRow("Dépôt Git", git);
    f->addRow("Statut", status);
    f->addRow("Description", desc);
    f->addRow("Notes", notes);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    f->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return false;
    if (name->text().trimmed().isEmpty()) return false;
    p.name = name->text().trimmed().toStdString();
    p.version = version->text().toStdString();
    p.firmware = firmware->text().toStdString();
    p.gitRepo = git->text().toStdString();
    p.status = status->text().toStdString();
    p.description = desc->toPlainText().toStdString();
    p.notes = notes->toPlainText().toStdString();
    return true;
}

void ProjectsPage::addProject() {
    Project p;
    if (!editProjectDialog(this, p)) return;
    ctx_.projects_service.saveProject(p);
    refreshProjects();
}

void ProjectsPage::editProject() {
    const Id id = currentProjectId();
    if (id == kNoId) return;
    auto proj = ctx_.projects_service.getProject(id);
    if (!proj) return;
    Project p = *proj;
    if (!editProjectDialog(this, p)) return;
    ctx_.projects_service.saveProject(p);
    refreshProjects();
}

void ProjectsPage::deleteProject() {
    const Id id = currentProjectId();
    if (id == kNoId) return;
    if (QMessageBox::question(this, "Supprimer", "Supprimer ce projet et sa nomenclature ?") != QMessageBox::Yes)
        return;
    ctx_.projects_service.removeProject(id);
    refreshProjects();
}

void ProjectsPage::addBomLine() {
    const Id projectId = currentProjectId();
    if (projectId == kNoId) { QMessageBox::information(this, "Projet", "Créez ou sélectionnez d'abord un projet."); return; }

    QDialog dlg(this);
    dlg.setWindowTitle("Ajouter un élément à la nomenclature");
    dlg.resize(520, 480);
    auto* v = new QVBoxLayout(&dlg);

    auto* search = new QLineEdit;
    search->setObjectName("search");
    search->setPlaceholderText("Rechercher un composant (nom, référence, type, marque, fournisseur…)");
    search->setClearButtonEnabled(true);
    v->addWidget(new QLabel("Composant de l'inventaire :"));
    v->addWidget(search);

    auto* results = new QListWidget;
    v->addWidget(results, 1);

    // Remplissage/filtrage via la recherche globale (tous les champs).
    auto repopulate = [this, results](const QString& q) {
        results->clear();
        domain::ComponentFilter filter;
        filter.query = q.trimmed().toStdString();
        for (const auto& c : ctx_.inventory.listComponents(filter)) {
            QString label = QString::fromStdString(c.name);
            if (!c.reference.empty()) label += "  (" + QString::fromStdString(c.reference) + ")";
            QStringList meta;
            if (!c.type.empty())         meta << QString::fromStdString(c.type);
            if (!c.manufacturer.empty()) meta << QString::fromStdString(c.manufacturer);
            if (!meta.isEmpty()) label += "  —  " + meta.join(" · ");
            auto* it = new QListWidgetItem(label);
            it->setData(Qt::UserRole, c.id);
            results->addItem(it);
        }
    };
    repopulate("");
    connect(search, &QLineEdit::textChanged, &dlg, [repopulate](const QString& q){ repopulate(q); });

    auto* freeLabel = new QLineEdit;
    freeLabel->setPlaceholderText("… ou libellé d'un élément hors inventaire (pas encore acheté)");
    v->addWidget(new QLabel("Ou élément hors inventaire :"));
    v->addWidget(freeLabel);

    auto* qtyRow = new QHBoxLayout;
    qtyRow->addWidget(new QLabel("Quantité requise :"));
    auto* qty = new QSpinBox; qty->setRange(1, 1000000); qty->setValue(1);
    qtyRow->addWidget(qty); qtyRow->addStretch(1);
    v->addLayout(qtyRow);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    v->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    // Double-clic sur un résultat = validation directe.
    connect(results, &QListWidget::itemDoubleClicked, &dlg, [&dlg](QListWidgetItem*){ dlg.accept(); });
    if (dlg.exec() != QDialog::Accepted) return;

    Id componentId = kNoId;
    if (auto* sel = results->currentItem()) componentId = sel->data(Qt::UserRole).toInt();
    const QString freeText = freeLabel->text().trimmed();

    if (componentId == kNoId && freeText.isEmpty()) {
        QMessageBox::warning(this, "Élément", "Sélectionnez un composant dans la liste ou saisissez un libellé libre.");
        return;
    }
    ctx_.projects_service.addComponent(projectId, componentId, freeText.toStdString(), qty->value());
    showDetail();
}

void ProjectsPage::removeBomLine() {
    int r = bom_->currentRow();
    if (r < 0) return;
    const Id linkId = bom_->item(r, 0)->data(Qt::UserRole).toInt();
    ctx_.projects_service.removeComponent(linkId);
    showDetail();
}
