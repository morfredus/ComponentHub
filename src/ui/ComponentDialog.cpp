#include "ui/ComponentDialog.h"

#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <map>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "platform/Clock.h"

using namespace domain;

namespace {
struct Opt { const char* code; const char* label; };
const Opt kKinds[]  = {{"component","Composant"},{"module","Module"},{"tool","Outil"},{"consumable","Consommable"}};
const Opt kStatus[] = {{"to_test","À tester"},{"validating","En validation"},{"validated","Validé"},
                       {"defective","Défectueux"},{"archived","Archivé"}};

// Chemin lisible « A › B › C » d'un emplacement, via l'index parentId.
QString locationPath(Id id, const std::map<Id, Location>& byId) {
    QString path; int guard = 0;
    while (id != kNoId && guard++ < 64) {
        auto it = byId.find(id);
        if (it == byId.end()) break;
        QString n = QString::fromStdString(it->second.name);
        path = path.isEmpty() ? n : n + " › " + path;
        id = it->second.parentId;
    }
    return path;
}
} // namespace

ComponentDialog::ComponentDialog(chdesktop::AppContext& ctx, Id componentId, QWidget* parent)
    : QDialog(parent), ctx_(ctx) {
    if (componentId != kNoId) {
        if (auto c = ctx_.components.findById(componentId)) current_ = *c;
    }
    setWindowTitle(componentId == kNoId ? "Nouveau composant" : "Modifier le composant");
    resize(640, 620);
    buildUi();
    loadToForm();
    refreshChildTabs();
}

void ComponentDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;
    root->addWidget(tabs, 1);

    // --- Général ---
    auto* gen = new QWidget; auto* gf = new QFormLayout(gen);
    kind_ = new QComboBox; for (auto& k : kKinds) kind_->addItem(k.label, k.code);
    name_ = new QLineEdit; reference_ = new QLineEdit; manufacturer_ = new QLineEdit;
    category_ = new QComboBox; category_->setEditable(true);
    subcategory_ = new QLineEdit;
    type_ = new QComboBox; type_->setEditable(true);   // référentiel Types de composants
    status_ = new QComboBox; for (auto& s : kStatus) status_->addItem(s.label, s.code);
    state_ = new QLineEdit; origin_ = new QLineEdit;
    location_ = new QComboBox;
    description_ = new QPlainTextEdit; description_->setFixedHeight(60);
    gf->addRow("Type d'objet", kind_);
    gf->addRow("Nom *", name_);
    gf->addRow("Référence", reference_);
    gf->addRow("Fabricant", manufacturer_);
    gf->addRow("Catégorie", category_);
    gf->addRow("Sous-catégorie", subcategory_);
    gf->addRow("Type", type_);
    gf->addRow("Statut", status_);
    gf->addRow("État", state_);
    gf->addRow("Provenance", origin_);
    gf->addRow("Emplacement", location_);
    gf->addRow("Description", description_);
    tabs->addTab(gen, "Général");

    // --- Caractéristiques ---
    auto* ch = new QWidget; auto* cf = new QFormLayout(ch);
    voltage_ = new QLineEdit; current_field_ = new QLineEdit; interfaceType_ = new QLineEdit;
    protocols_ = new QLineEdit; i2cAddress_ = new QLineEdit; frequency_ = new QLineEdit;
    pinCount_ = new QSpinBox; pinCount_->setRange(0, 100000);
    compatibility_ = new QLineEdit;
    cf->addRow("Tension", voltage_);
    cf->addRow("Courant", current_field_);
    cf->addRow("Interface", interfaceType_);
    cf->addRow("Protocoles", protocols_);
    cf->addRow("Adresse I²C", i2cAddress_);
    cf->addRow("Fréquence", frequency_);
    cf->addRow("Nb de broches", pinCount_);
    cf->addRow("Compatibilité", compatibility_);
    tabs->addTab(ch, "Caractéristiques");

    // --- Achat & stock ---
    auto* buy = new QWidget; auto* bf = new QFormLayout(buy);
    price_ = new QDoubleSpinBox; price_->setRange(0, 1000000); price_->setDecimals(2); price_->setSuffix(" €");
    supplier_ = new QLineEdit;
    purchaseDate_ = new QLineEdit; purchaseDate_->setPlaceholderText("AAAA-MM-JJ");
    receptionDate_ = new QLineEdit; receptionDate_->setPlaceholderText("AAAA-MM-JJ");
    warranty_ = new QLineEdit;
    quantity_ = new QSpinBox; quantity_->setRange(0, 1000000);
    minStock_ = new QSpinBox; minStock_->setRange(0, 1000000);
    idealStock_ = new QSpinBox; idealStock_->setRange(0, 1000000);
    bf->addRow("Prix unitaire", price_);
    bf->addRow("Fournisseur", supplier_);
    bf->addRow("Date d'achat", purchaseDate_);
    bf->addRow("Date de réception", receptionDate_);
    bf->addRow("Garantie", warranty_);
    bf->addRow("Quantité", quantity_);
    bf->addRow("Stock minimum", minStock_);
    bf->addRow("Stock idéal", idealStock_);

    stockLabel_ = new QLabel;
    auto* mv = uikit::button("Mouvement de stock…", icons::Glyph::StockIn, "ghost");
    connect(mv, &QPushButton::clicked, this, &ComponentDialog::doStockMovement);
    auto* stockRow = new QHBoxLayout; stockRow->addWidget(stockLabel_, 1); stockRow->addWidget(mv);
    bf->addRow(stockRow);
    history_ = new QTableWidget(0, 4);
    history_->setHorizontalHeaderLabels({"Date", "Type", "Qté", "Note"});
    history_->horizontalHeader()->setStretchLastSection(true);
    history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_->verticalHeader()->setVisible(false);
    history_->setFixedHeight(140);
    bf->addRow(history_);
    tabs->addTab(buy, "Achat & stock");

    // --- Documents ---
    auto* doc = new QWidget; auto* dv = new QVBoxLayout(doc);
    childHint_ = uikit::hint("Enregistrez d'abord la fiche pour lui rattacher des documents.");
    dv->addWidget(childHint_);
    auto* docBar = new QHBoxLayout;
    auto* dAdd = uikit::button("Ajouter un lien", icons::Glyph::Add);
    auto* dImport = uikit::button("Importer un fichier", icons::Glyph::Add, "ghost");
    auto* dEdit = uikit::button("Modifier", icons::Glyph::Edit, "ghost");
    auto* dOpen = uikit::button("Ouvrir", icons::Glyph::Link, "ghost");
    auto* dDel = uikit::button("Supprimer", icons::Glyph::Delete, "danger");
    docBar->addWidget(dAdd); docBar->addWidget(dImport); docBar->addWidget(dEdit);
    docBar->addWidget(dOpen); docBar->addWidget(dDel); docBar->addStretch(1);
    dv->addLayout(docBar);
    documents_ = new QTableWidget(0, 3);
    documents_->setHorizontalHeaderLabels({"Titre", "Catégorie", "Lien / fichier"});
    documents_->horizontalHeader()->setStretchLastSection(true);
    documents_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    documents_->setSelectionBehavior(QAbstractItemView::SelectRows);
    documents_->verticalHeader()->setVisible(false);
    dv->addWidget(documents_, 1);
    tabs->addTab(doc, "Documents");
    connect(dAdd, &QPushButton::clicked, this, [this] { addOrEditDocument(kNoId); });
    connect(dImport, &QPushButton::clicked, this, &ComponentDialog::importDocumentFile);
    connect(dEdit, &QPushButton::clicked, this, [this] {
        auto* it = documents_->currentItem();
        if (it) addOrEditDocument(documents_->item(documents_->currentRow(), 0)->data(Qt::UserRole).toString().toStdString());
    });
    connect(dDel, &QPushButton::clicked, this, &ComponentDialog::deleteDocument);
    connect(dOpen, &QPushButton::clicked, this, &ComponentDialog::openDocument);

    // --- Notes ---
    auto* note = new QWidget; auto* nv = new QVBoxLayout(note);
    notes_ = new QPlainTextEdit;
    nv->addWidget(notes_);
    tabs->addTab(note, "Notes");

    // --- Boutons ---
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText("Enregistrer");
    bb->button(QDialogButtonBox::Cancel)->setText("Annuler");
    root->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, [this] { if (saveFromForm()) accept(); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ComponentDialog::loadToForm() {
    // Catégories connues (le champ reste libre).
    for (const auto& c : ctx_.inventory.listCategories())
        category_->addItem(QString::fromStdString(c.name));

    // Types de composants : proposés depuis le référentiel (champ éditable).
    type_->addItem("");
    for (const auto& r : ctx_.referentials.findByList("component_types"))
        type_->addItem(QString::fromStdString(r.value));

    // Emplacements, présentés par chemin hiérarchique.
    std::map<Id, Location> byId;
    for (const auto& l : ctx_.inventory.listLocations()) byId[l.id] = l;
    location_->addItem("(aucun)", QString());   // QString vide = kNoId
    for (const auto& kv : byId)
        location_->addItem(locationPath(kv.first, byId), QString::fromStdString(kv.first));

    const Component& c = current_;
    auto setCombo = [](QComboBox* box, const std::string& code) {
        int i = box->findData(QString::fromStdString(code));
        box->setCurrentIndex(i < 0 ? 0 : i);
    };
    setCombo(kind_, toString(c.kind));
    name_->setText(QString::fromStdString(c.name));
    reference_->setText(QString::fromStdString(c.reference));
    manufacturer_->setText(QString::fromStdString(c.manufacturer));
    category_->setCurrentText(QString::fromStdString(c.category));
    subcategory_->setText(QString::fromStdString(c.subcategory));
    type_->setCurrentText(QString::fromStdString(c.type));
    setCombo(status_, c.status);
    state_->setText(QString::fromStdString(c.state));
    origin_->setText(QString::fromStdString(c.origin));
    { int i = location_->findData(QString::fromStdString(c.locationId)); location_->setCurrentIndex(i < 0 ? 0 : i); }
    description_->setPlainText(QString::fromStdString(c.description));

    voltage_->setText(QString::fromStdString(c.voltage));
    current_field_->setText(QString::fromStdString(c.current));
    interfaceType_->setText(QString::fromStdString(c.interfaceType));
    protocols_->setText(QString::fromStdString(c.protocols));
    i2cAddress_->setText(QString::fromStdString(c.i2cAddress));
    frequency_->setText(QString::fromStdString(c.frequency));
    pinCount_->setValue(c.pinCount);
    compatibility_->setText(QString::fromStdString(c.compatibility));

    price_->setValue(c.price);
    supplier_->setText(QString::fromStdString(c.supplier));
    purchaseDate_->setText(QString::fromStdString(c.purchaseDate));
    receptionDate_->setText(QString::fromStdString(c.receptionDate));
    warranty_->setText(QString::fromStdString(c.warranty));
    quantity_->setValue(c.quantity);
    minStock_->setValue(c.minStock);
    idealStock_->setValue(c.idealStock);

    notes_->setPlainText(QString::fromStdString(c.notes));
}

bool ComponentDialog::saveFromForm() {
    if (name_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Champ requis", "Le nom est obligatoire.");
        return false;
    }
    Component& c = current_;
    c.kind = componentKindFromString(kind_->currentData().toString().toStdString());
    c.name = name_->text().trimmed().toStdString();
    c.reference = reference_->text().toStdString();
    c.manufacturer = manufacturer_->text().toStdString();
    c.category = category_->currentText().trimmed().toStdString();
    c.subcategory = subcategory_->text().toStdString();
    c.type = type_->currentText().trimmed().toStdString();
    c.status = status_->currentData().toString().toStdString();
    c.state = state_->text().toStdString();
    c.origin = origin_->text().toStdString();
    c.locationId = location_->currentData().toString().toStdString();
    c.description = description_->toPlainText().toStdString();

    c.voltage = voltage_->text().toStdString();
    c.current = current_field_->text().toStdString();
    c.interfaceType = interfaceType_->text().toStdString();
    c.protocols = protocols_->text().toStdString();
    c.i2cAddress = i2cAddress_->text().toStdString();
    c.frequency = frequency_->text().toStdString();
    c.pinCount = pinCount_->value();
    c.compatibility = compatibility_->text().toStdString();

    c.price = price_->value();
    c.supplier = supplier_->text().toStdString();
    c.purchaseDate = purchaseDate_->text().toStdString();
    c.receptionDate = receptionDate_->text().toStdString();
    c.warranty = warranty_->text().toStdString();
    c.quantity = quantity_->value();
    c.minStock = minStock_->value();
    c.idealStock = idealStock_->value();

    c.notes = notes_->toPlainText().toStdString();

    // Ajout inline au référentiel : si le type saisi n'existe pas encore, on
    // propose de l'ajouter sans quitter la fiche (nomenclature cohérente).
    if (!c.type.empty()) {
        bool known = false;
        for (const auto& r : ctx_.referentials.findByList("component_types"))
            if (r.value == c.type) { known = true; break; }
        if (!known) {
            const QString t = QString::fromStdString(c.type);
            if (QMessageBox::question(this, "Nouveau type",
                    "Le type « " + t + " » n'existe pas dans le référentiel. L'ajouter ?")
                == QMessageBox::Yes) {
                domain::RefItem r;
                r.list = "component_types";
                r.value = c.type;
                r.position = (int)ctx_.referentials.findByList("component_types").size();
                ctx_.referentials.save(r);
            }
        }
    }

    current_ = ctx_.inventory.saveComponent(c);  // récupère l'id assigné si création
    return true;
}

void ComponentDialog::refreshChildTabs() {
    const bool exists = current_.id != kNoId;
    childHint_->setVisible(!exists);
    documents_->setEnabled(exists);
    history_->setEnabled(exists);

    // Stock courant.
    stockLabel_->setText(exists ? QString("Stock actuel : %1").arg(current_.quantity)
                                : QString("Stock : enregistrez d'abord la fiche"));

    documents_->setRowCount(0);
    if (exists) {
        for (const auto& d : ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, current_.id)) {
            int r = documents_->rowCount();
            documents_->insertRow(r);
            auto* t = new QTableWidgetItem(QString::fromStdString(d.title));
            t->setData(Qt::UserRole, QString::fromStdString(d.id));
            documents_->setItem(r, 0, t);
            documents_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(d.category)));
            // Colonne « Lien / fichier » : pour une pièce jointe importée, on
            // montre le nom d'origine (pas le hash, illisible) ; pour un lien,
            // l'URL. Un fichier dont le contenu n'est pas encore là (reçu par
            // synchro mais binaire pas encore récupéré) est signalé.
            QString shown = d.isFile() ? QString::fromStdString(d.fileName)
                                       : QString::fromStdString(d.url);
            if (d.isFile() && !ctx_.attachments.has(d.blobHash))
                shown += "  (contenu non encore synchronisé)";
            documents_->setItem(r, 2, new QTableWidgetItem(shown));
        }
    }

    history_->setRowCount(0);
    if (exists) {
        auto movements = ctx_.inventory.history(current_.id);
        for (const auto& m : movements) {
            int r = history_->rowCount();
            history_->insertRow(r);
            history_->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(m.timestamp)));
            history_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(toString(m.type))));
            history_->setItem(r, 2, new QTableWidgetItem(QString::number(m.quantity)));
            history_->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(m.note)));
        }
    }
}

void ComponentDialog::addOrEditDocument(Id docId) {
    if (current_.id == kNoId) return;
    Document d;
    d.ownerKind = DocumentOwnerKind::Component;
    d.ownerId = current_.id;
    if (docId != kNoId)
        for (const auto& e : ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, current_.id))
            if (e.id == docId) { d = e; break; }

    bool ok = false;
    QString title = QInputDialog::getText(this, "Document", "Titre :", QLineEdit::Normal,
                                          QString::fromStdString(d.title), &ok);
    if (!ok || title.trimmed().isEmpty()) return;
    QString category = QInputDialog::getText(this, "Document", "Catégorie (datasheet, manuel, pinout, lien…) :",
                                             QLineEdit::Normal, QString::fromStdString(d.category), &ok);
    if (!ok) return;
    QString url = QInputDialog::getText(this, "Document", "Lien (URL http… ou chemin de fichier) :",
                                        QLineEdit::Normal, QString::fromStdString(d.url), &ok);
    if (!ok) return;
    d.title = title.trimmed().toStdString();
    d.category = category.trimmed().toStdString();
    d.url = url.trimmed().toStdString();
    ctx_.documents_service.save(d);
    refreshChildTabs();
}

void ComponentDialog::importDocumentFile() {
    if (current_.id == kNoId) return;
    const QString path = QFileDialog::getOpenFileName(this, "Importer un fichier", QString());
    if (path.isEmpty()) return;

    // Copie le fichier dans le magasin, sous le hash de son contenu. Le contenu
    // devient indépendant du fichier d'origine (déplaçable, supprimable).
    const auto res = ctx_.attachments.importFile(path.toStdString());
    if (!res.ok) {
        QMessageBox::warning(this, "Import", "Impossible de lire le fichier sélectionné.");
        return;
    }

    const QFileInfo fi(path);
    Document d;
    d.ownerKind = DocumentOwnerKind::Component;
    d.ownerId = current_.id;
    d.title = fi.fileName().toStdString();   // proposition, modifiable ensuite
    d.category = "fichier";
    d.blobHash = res.hash;
    d.fileName = fi.fileName().toStdString();
    d.sizeBytes = res.sizeBytes;
    ctx_.documents_service.save(d);
    refreshChildTabs();
}

void ComponentDialog::openDocument() {
    const int r = documents_->currentRow();
    if (r < 0) return;
    const Id docId = documents_->item(r, 0)->data(Qt::UserRole).toString().toStdString();

    Document doc; bool found = false;
    for (const auto& e : ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, current_.id))
        if (e.id == docId) { doc = e; found = true; break; }
    if (!found) return;

    if (doc.isFile()) {
        std::string bytes;
        if (!ctx_.attachments.get(doc.blobHash, bytes)) {
            QMessageBox::information(this, "Pièce jointe",
                "Le contenu de ce fichier n'est pas encore présent sur ce poste. "
                "Il sera récupéré à la prochaine synchronisation avec morfSync.");
            return;
        }
        // Le magasin nomme le fichier par son hash (sans extension) : on
        // matérialise une copie temporaire au nom d'origine pour que le système
        // sache avec quelle application l'ouvrir.
        const QString name = doc.fileName.empty() ? QStringLiteral("piece-jointe")
                                                   : QString::fromStdString(doc.fileName);
        const QString tmpDir = QDir::tempPath() + "/componenthub-attachments";
        QDir().mkpath(tmpDir);
        const QString tmpPath = tmpDir + "/" + name;
        QFile f(tmpPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(bytes.data(), static_cast<qint64>(bytes.size()));
            f.close();
            QDesktopServices::openUrl(QUrl::fromLocalFile(tmpPath));
        }
        return;
    }
    if (!doc.url.empty())
        QDesktopServices::openUrl(QUrl::fromUserInput(QString::fromStdString(doc.url)));
}

void ComponentDialog::deleteDocument() {
    int r = documents_->currentRow();
    if (r < 0) return;
    const Id id = documents_->item(r, 0)->data(Qt::UserRole).toString().toStdString();
    if (QMessageBox::question(this, "Supprimer", "Supprimer ce document ?") != QMessageBox::Yes) return;
    ctx_.documents_service.remove(id);
    refreshChildTabs();
}

void ComponentDialog::doStockMovement() {
    if (current_.id == kNoId) {
        QMessageBox::information(this, "Stock", "Enregistrez d'abord la fiche.");
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle("Mouvement de stock");
    auto* f = new QFormLayout(&dlg);
    auto* type = new QComboBox;
    type->addItem("Entrée (+)", "in");
    type->addItem("Sortie (−)", "out");
    type->addItem("Correction (=)", "correction");
    type->addItem("Inventaire (=)", "inventory");
    auto* qty = new QSpinBox; qty->setRange(0, 1000000);
    auto* note = new QLineEdit;
    f->addRow("Type", type);
    f->addRow("Quantité", qty);
    f->addRow("Note", note);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    f->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    StockMovement m;
    m.componentId = current_.id;
    m.type = stockMovementTypeFromString(type->currentData().toString().toStdString());
    m.quantity = qty->value();
    m.note = note->text().toStdString();
    m.timestamp = chdesktop::nowIso();
    if (auto updated = ctx_.inventory.recordMovement(m)) {
        current_ = *updated;
        quantity_->setValue(current_.quantity);
    }
    refreshChildTabs();
}
