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
#include "ui/Language.h"
#include "platform/Clock.h"

using namespace domain;

namespace {
QString statusLabel(const std::string& s) {
    if (s == "validated")  return chui::tr("Validé");
    if (s == "validating") return chui::tr("En validation");
    if (s == "defective")  return chui::tr("Défectueux");
    if (s == "archived")   return chui::tr("Archivé");
    return chui::tr("À tester");
}

// Chemin lisible d'un emplacement : « Atelier › Armoire A › Tiroir 3 ».
QString locationPath(const std::map<Id, Location>& byId, Id id) {
    QString p; int guard = 0;  // garde-fou contre une boucle de parents corrompue
    while (id != kNoId && guard++ < 64) {
        auto it = byId.find(id); if (it == byId.end()) break;
        const QString n = QString::fromStdString(it->second.name);
        p = p.isEmpty() ? n : n + " › " + p;
        id = it->second.parentId;
    }
    return p;
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

// Cellule texte triée sans tenir compte de la casse : « abc » se classe avec
// « ABC » (le tri brut de Qt place toutes les majuscules avant les minuscules).
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
    header->addWidget(uikit::pageTitle(chui::tr("Inventaire")));
    header->addStretch(1);
    auto* add = uikit::button(chui::tr("Ajouter"), icons::Glyph::Add);
    add->setToolTip(chui::tr("Nouvelle fiche (Ctrl+N)"));
    connect(add, &QPushButton::clicked, this, &InventoryPage::addComponent);
    header->addWidget(add);
    v->addLayout(header);

    auto* bar = new QHBoxLayout;
    search_ = new QLineEdit; search_->setObjectName("search");
    search_->setPlaceholderText(chui::tr("Rechercher (nom, référence, fabricant, notes…)"));
    search_->setClearButtonEnabled(true);
    kind_ = new QComboBox;
    kind_->addItem(chui::tr("Tous les objets"), -1);
    kind_->addItem(chui::tr("Composants"), int(ComponentKind::Component));
    kind_->addItem(chui::tr("Modules"), int(ComponentKind::Module));
    kind_->addItem(chui::tr("Outils"), int(ComponentKind::Tool));
    kind_->addItem(chui::tr("Consommables"), int(ComponentKind::Consumable));
    lowStock_ = new QCheckBox(chui::tr("Stock faible"));
    location_ = new QComboBox;
    category_ = new QComboBox;
    status_ = new QComboBox;
    status_->addItem(chui::tr("Tous les statuts"), "");
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
    table_->setHorizontalHeaderLabels({chui::tr("Nom"), chui::tr("Référence"), chui::tr("Type"), chui::tr("Catégorie"), chui::tr("Emplacement"), chui::tr("Qté"), chui::tr("Prix"), chui::tr("Statut")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(0, 220);
    // Tri par colonne au clic sur l'en-tête (Qté et Prix triés numériquement).
    // Ordre par défaut : colonne Nom, croissant (chiffres puis A-Z). Sans cela,
    // Qt démarre en décroissant (Z-A puis chiffres).
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

    // Menu clic droit sur une ligne.
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &InventoryPage::showContextMenu);

    // Raccourcis clavier : globaux à la page (Ctrl+N, Ctrl+F) ou propres au tableau.
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

    // Filtre d'événements pour détecter le survol de la souris et afficher le tooltip avec photo
    table_->viewport()->installEventFilter(this);

    // Raccourcis d'action via double-clic ; boutons contextuels en bas.
    auto* actions = new QHBoxLayout;
    auto* edit = uikit::button(chui::tr("Modifier"), icons::Glyph::Edit, "ghost");
    auto* del = uikit::button(chui::tr("Supprimer"), icons::Glyph::Delete, "danger");
    connect(edit, &QPushButton::clicked, this, &InventoryPage::editCurrent);
    connect(del, &QPushButton::clicked, this, &InventoryPage::deleteCurrent);
    actions->addStretch(1); actions->addWidget(edit); actions->addWidget(del);
    v->addLayout(actions);
}

void InventoryPage::showEvent(QShowEvent*) { reloadFilters(); refresh(); }

void InventoryPage::reloadFilters() {
    // Reconstruit les listes (emplacements, catégories) en gardant le choix courant.
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
    location_->addItem(chui::tr("Tous les emplacements"), "");
    for (const auto& [p, id] : locs) location_->addItem(p, QString::fromStdString(id));
    location_->setCurrentIndex(std::max(0, location_->findData(keepLoc)));
    category_->clear();
    category_->addItem(chui::tr("Toutes les catégories"), "");
    for (const auto& c : ctx_.inventory.listCategories())
        category_->addItem(QString::fromStdString(c.name), QString::fromStdString(c.name));
    category_->setCurrentIndex(std::max(0, category_->findData(keepCat)));
}

void InventoryPage::refresh() {
    // Index emplacement -> chemin lisible.
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

    // Filtre emplacement : l'emplacement choisi ET tous ses sous-emplacements
    // (« qu'y a-t-il dans cette armoire ? »), d'où le post-filtrage ici.
    std::set<Id> locSet;
    const Id wantLoc = location_->currentData().toString().toStdString();
    if (!wantLoc.empty()) {
        locSet.insert(wantLoc);
        for (bool grew = true; grew;) {  // point fixe : ajoute les enfants jusqu'à stabilité
            grew = false;
            for (const auto& [id, l] : byId)
                if (locSet.count(l.parentId) && locSet.insert(id).second) grew = true;
        }
    }

    auto items = ctx_.inventory.listComponents(filter);
    if (!wantLoc.empty())
        items.erase(std::remove_if(items.begin(), items.end(),
            [&](const Component& c) { return !locSet.count(c.locationId); }), items.end());
    // On désactive le tri pendant le remplissage (sinon chaque insertion
    // réordonne la table) ; il est réactivé ensuite, ce qui applique l'ordre
    // de tri courant choisi par l'utilisateur.
    table_->setSortingEnabled(false);
    table_->setRowCount(0);
    for (const auto& c : items) {
        int r = table_->rowCount();
        table_->insertRow(r);
        auto* name = new TextItem(QString::fromStdString(c.name));
        name->setData(Qt::UserRole, QString::fromStdString(c.id));
        table_->setItem(r, 0, name);
        table_->setItem(r, 1, new TextItem(QString::fromStdString(c.reference)));
        // Colonne « Type » = le champ Type (référentiel : Résistance, Capteur…),
        // pas le genre kind (celui-ci reste accessible via le filtre déroulant).
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
    int r = table_->currentRow();
    if (r < 0) return;
    const QString name = table_->item(r, 0)->text();
    const Id id = table_->item(r, 0)->data(Qt::UserRole).toString().toStdString();
    if (QMessageBox::question(this, chui::tr("Supprimer"), chui::tr("Supprimer « ") + name + chui::tr(" » ?")) != QMessageBox::Yes) return;
    ctx_.inventory.removeComponent(id);
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

// Mouvement de stock : « + / − » d'une unité (delta) ou valeur absolue (setTo >= 0).
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
        if (c->quantity <= 0) return;  // jamais de stock négatif
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
    const int q = QInputDialog::getInt(this, chui::tr("Définir la quantité"), chui::tr("Quantité :"),
                                       c->quantity, 0, 1000000, 1, &ok);
    if (ok && q != c->quantity) adjustStock(0, q);
}

void InventoryPage::duplicateCurrent() {
    auto c = findComponent(currentId());
    if (!c) return;
    Component copy = *c;
    copy.id = kNoId;               // nouvelle identité
    copy.meta = SyncMeta();        // nouvelle enveloppe de synchronisation
    copy.name += chui::tr(" (copie)").toStdString();
    // Les documents ne sont pas copiés (ils restent sur la fiche d'origine).
    const Component saved = ctx_.inventory.saveComponent(copy);
    refresh();
    selectById(saved.id);
}

void InventoryPage::moveCurrent() {
    const Id id = currentId();
    auto c = id.empty() ? std::nullopt : findComponent(id);
    if (!c) return;
    // Liste des emplacements déjà construite dans le filtre (entrée 0 = « tous »).
    QStringList names{chui::tr("(aucun)")};
    std::vector<Id> ids{kNoId};
    int cur = 0;
    for (int i = 1; i < location_->count(); ++i) {
        names << location_->itemText(i);
        ids.push_back(location_->itemData(i).toString().toStdString());
        if (ids.back() == c->locationId) cur = i;
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, chui::tr("Changer d'emplacement"),
        chui::tr("Nouvel emplacement :"), names, cur, false, &ok);
    if (!ok) return;
    c->locationId = ids[names.indexOf(pick)];
    ctx_.inventory.saveComponent(*c);
    refresh();
    selectById(id);
}

// Première fiche « datasheet » de type lien (un fichier importé n'est pas ouvert ici).
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
    m.addAction(chui::tr("Modifier"), this, [this] { editCurrent(); });
    m.addAction(chui::tr("Dupliquer"), this, [this] { duplicateCurrent(); });
    m.addSeparator();
    m.addAction(chui::tr("Ajouter 1 (+)"), this, [this] { adjustStock(+1); });
    m.addAction(chui::tr("Retirer 1 (−)"), this, [this] { adjustStock(-1); });
    m.addAction(chui::tr("Définir la quantité…"), this, [this] { setQuantityCurrent(); });
    m.addAction(chui::tr("Changer d'emplacement…"), this, [this] { moveCurrent(); });
    m.addAction(chui::tr("Ouvrir la datasheet"), this, [this] { openDatasheet(); })->setEnabled(hasSheet);
    m.addSeparator();
    m.addAction(chui::tr("Supprimer"), this, [this] { deleteCurrent(); });
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
