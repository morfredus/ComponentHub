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
#include <map>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/ComponentDialog.h"

using namespace domain;

namespace {
QString statusLabel(const std::string& s) {
    if (s == "validated")  return "Validé";
    if (s == "validating") return "En validation";
    if (s == "defective")  return "Défectueux";
    if (s == "archived")   return "Archivé";
    return "À tester";
}

// Cellule triée numériquement (et non alphabétiquement) : « 10 » doit venir
// après « 9 », et « 6.90 € » se compare sur sa valeur, pas sur son texte.
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
} // namespace

InventoryPage::InventoryPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);

    auto* header = new QHBoxLayout;
    header->addWidget(uikit::pageTitle("Inventaire"));
    header->addStretch(1);
    auto* add = uikit::button("Ajouter", icons::Glyph::Add);
    connect(add, &QPushButton::clicked, this, &InventoryPage::addComponent);
    header->addWidget(add);
    v->addLayout(header);

    auto* bar = new QHBoxLayout;
    search_ = new QLineEdit; search_->setObjectName("search");
    search_->setPlaceholderText("Rechercher (nom, référence, fabricant, notes…)");
    search_->setClearButtonEnabled(true);
    kind_ = new QComboBox;
    kind_->addItem("Tous les objets", -1);
    kind_->addItem("Composants",   int(ComponentKind::Component));
    kind_->addItem("Modules",      int(ComponentKind::Module));
    kind_->addItem("Outils",       int(ComponentKind::Tool));
    kind_->addItem("Consommables", int(ComponentKind::Consumable));
    lowStock_ = new QCheckBox("Stock faible");
    bar->addWidget(search_, 1);
    bar->addWidget(kind_);
    bar->addWidget(lowStock_);
    v->addLayout(bar);

    table_ = new QTableWidget(0, 8);
    table_->setHorizontalHeaderLabels({"Nom", "Référence", "Type", "Catégorie", "Emplacement", "Qté", "Prix", "Statut"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(0, 220);
    // Tri par colonne au clic sur l'en-tête (Qté et Prix triés numériquement).
    table_->setSortingEnabled(true);
    table_->horizontalHeader()->setSortIndicatorShown(true);
    table_->horizontalHeader()->setSectionsClickable(true);
    v->addWidget(table_, 1);

    connect(search_, &QLineEdit::textChanged, this, &InventoryPage::refresh);
    connect(kind_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InventoryPage::refresh);
    connect(lowStock_, &QCheckBox::toggled, this, &InventoryPage::refresh);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { editComponentRow(row); });

    // Filtre d'événements pour détecter le survol de la souris et afficher le tooltip avec photo
    table_->viewport()->installEventFilter(this);

    // Raccourcis d'action via double-clic ; boutons contextuels en bas.
    auto* actions = new QHBoxLayout;
    auto* edit = uikit::button("Modifier", icons::Glyph::Edit, "ghost");
    auto* del = uikit::button("Supprimer", icons::Glyph::Delete, "danger");
    connect(edit, &QPushButton::clicked, this, &InventoryPage::editCurrent);
    connect(del, &QPushButton::clicked, this, &InventoryPage::deleteCurrent);
    actions->addStretch(1); actions->addWidget(edit); actions->addWidget(del);
    v->addLayout(actions);
}

void InventoryPage::showEvent(QShowEvent*) { refresh(); }

void InventoryPage::refresh() {
    // Index emplacement -> chemin lisible.
    std::map<Id, Location> byId;
    for (const auto& l : ctx_.inventory.listLocations()) byId[l.id] = l;
    auto path = [&](Id id) -> QString {
        QString p; int guard = 0;
        while (id != kNoId && guard++ < 64) {
            auto it = byId.find(id); if (it == byId.end()) break;
            QString n = QString::fromStdString(it->second.name);
            p = p.isEmpty() ? n : n + " › " + p;
            id = it->second.parentId;
        }
        return p;
    };

    ComponentFilter filter;
    filter.query = search_->text().trimmed().toStdString();
    filter.onlyLowStock = lowStock_->isChecked();
    const int k = kind_->currentData().toInt();
    if (k >= 0) { filter.hasKind = true; filter.kind = ComponentKind(k); }

    const auto items = ctx_.inventory.listComponents(filter);
    // On désactive le tri pendant le remplissage (sinon chaque insertion
    // réordonne la table) ; il est réactivé ensuite, ce qui applique l'ordre
    // de tri courant choisi par l'utilisateur.
    table_->setSortingEnabled(false);
    table_->setRowCount(0);
    for (const auto& c : items) {
        int r = table_->rowCount();
        table_->insertRow(r);
        auto* name = new QTableWidgetItem(QString::fromStdString(c.name));
        name->setData(Qt::UserRole, QString::fromStdString(c.id));
        table_->setItem(r, 0, name);
        table_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(c.reference)));
        // Colonne « Type » = le champ Type (référentiel : Résistance, Capteur…),
        // pas le genre kind (celui-ci reste accessible via le filtre déroulant).
        table_->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(c.type)));
        table_->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(c.category)));
        table_->setItem(r, 4, new QTableWidgetItem(path(c.locationId)));
        auto* qty = new NumericItem(QString::number(c.quantity), c.quantity);
        if (c.minStock > 0 && c.quantity <= c.minStock)
            qty->setForeground(QColor(Theme::instance().color("danger")));
        table_->setItem(r, 5, qty);
        table_->setItem(r, 6, new NumericItem(QString::number(c.price, 'f', 2) + " €", c.price));
        table_->setItem(r, 7, new QTableWidgetItem(statusLabel(c.status)));
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
    int r = table_->currentRow();
    if (r < 0) return;
    const QString name = table_->item(r, 0)->text();
    const Id id = table_->item(r, 0)->data(Qt::UserRole).toString().toStdString();
    if (QMessageBox::question(this, "Supprimer", "Supprimer « " + name + " » ?") != QMessageBox::Yes) return;
    ctx_.inventory.removeComponent(id);
    refresh();
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

    // Récupérer les documents du composant (photos)
    const auto documents = ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, id);

    // Chercher la première photo (fichier image)
    for (const auto& doc : documents) {
        if (!doc.isFile()) continue;

        // Vérifier si c'est une image par extension
        const QString fileName = QString::fromStdString(doc.fileName);
        const QString ext = fileName.section('.', -1).toLower();
        if (ext != "jpg" && ext != "jpeg" && ext != "png" && ext != "gif" && ext != "bmp" && ext != "webp")
            continue;

        // Vérifier si le contenu est disponible
        if (!ctx_.attachments.has(doc.blobHash)) continue;

        // Charger l'image depuis le magasin
        std::string bytes;
        if (!ctx_.attachments.get(doc.blobHash, bytes)) continue;

        QPixmap pixmap;
        if (!pixmap.loadFromData(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<int>(bytes.size())))
            continue;

        // Redimensionner l'image pour le tooltip (max 400x400)
        const int maxSize = 400;
        if (pixmap.width() > maxSize || pixmap.height() > maxSize) {
            pixmap = pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // Sauvegarder l'image dans un fichier temporaire pour l'afficher dans le tooltip
        const QString tmpDir = QDir::tempPath() + "/componenthub-tooltips";
        QDir().mkpath(tmpDir);
        const QString tmpPath = tmpDir + "/" + QString::fromStdString(doc.blobHash) + ".png";
        if (pixmap.save(tmpPath)) {
            // Afficher le tooltip avec l'image
            const QString html = QString("<img src=\"%1\" width=\"%2\" height=\"%3\">")
                .arg(tmpPath)
                .arg(pixmap.width())
                .arg(pixmap.height());
            return html;
        }
    }

    return QString();
}
