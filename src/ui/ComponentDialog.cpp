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
#include "ui/Language.h"
#include "platform/Clock.h"

using namespace domain;

namespace {
struct Opt { const char* code; const char* label; };
    const Opt kKinds[]  = {{"component",CHUI_NOOP("Component")},{"module",CHUI_NOOP("Module")},{"tool",CHUI_NOOP("Tool")},{"consumable",CHUI_NOOP("Consumable")}};
    const Opt kStatus[] = {{"to_test",CHUI_NOOP("To test")},{"validating",CHUI_NOOP("Validating")},{"validated",CHUI_NOOP("Validated")},
                           {"defective",CHUI_NOOP("Defective")},{"archived",CHUI_NOOP("Archived")}};

// Readable path "A › B › C" of a location, using the parentId index.
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
    setWindowTitle(componentId == kNoId ? chui::tr("New component") : chui::tr("Edit component"));
    resize(640, 620);
    buildUi();
    loadToForm();
    refreshChildTabs();
}

void ComponentDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;
    root->addWidget(tabs, 1);

    // --- General ---
    auto* gen = new QWidget; auto* gf = new QFormLayout(gen);
    kind_ = new QComboBox; for (auto& k : kKinds) kind_->addItem(chui::tr(k.label), k.code);
    name_ = new QLineEdit; reference_ = new QLineEdit; manufacturer_ = new QLineEdit;
    category_ = new QComboBox; category_->setEditable(true);
    subcategory_ = new QLineEdit;
    type_ = new QComboBox; type_->setEditable(true);   // "Component types" reference list
    status_ = new QComboBox; for (auto& s : kStatus) status_->addItem(chui::tr(s.label), s.code);
    state_ = new QLineEdit; origin_ = new QLineEdit;
    location_ = new QComboBox;
    description_ = new QPlainTextEdit; description_->setFixedHeight(60);
    gf->addRow(chui::tr("Object type"), kind_);
    gf->addRow(chui::tr("Name *"), name_);
    gf->addRow(chui::tr("Reference"), reference_);
    gf->addRow(chui::tr("Manufacturer"), manufacturer_);
    gf->addRow(chui::tr("Category"), category_);
    gf->addRow(chui::tr("Subcategory"), subcategory_);
    gf->addRow(chui::tr("Type"), type_);
    gf->addRow(chui::tr("Status"), status_);
    gf->addRow(chui::tr("State"), state_);
    gf->addRow(chui::tr("Origin"), origin_);
    gf->addRow(chui::tr("Location"), location_);
    gf->addRow(chui::tr("Description"), description_);
    tabs->addTab(gen, chui::tr("General"));

    // --- Specifications ---
    auto* ch = new QWidget; auto* cf = new QFormLayout(ch);
    voltage_ = new QLineEdit; current_field_ = new QLineEdit; interfaceType_ = new QLineEdit;
    protocols_ = new QLineEdit; i2cAddress_ = new QLineEdit; frequency_ = new QLineEdit;
    pinCount_ = new QSpinBox; pinCount_->setRange(0, 100000);
    compatibility_ = new QLineEdit;
    cf->addRow(chui::tr("Voltage"), voltage_);
    cf->addRow(chui::tr("Current"), current_field_);
    cf->addRow(chui::tr("Interface"), interfaceType_);
    cf->addRow(chui::tr("Protocols"), protocols_);
    cf->addRow(chui::tr("I²C address"), i2cAddress_);
    cf->addRow(chui::tr("Frequency"), frequency_);
    cf->addRow(chui::tr("Pin count"), pinCount_);
    cf->addRow(chui::tr("Compatibility"), compatibility_);
    tabs->addTab(ch, chui::tr("Specifications"));

    // --- Purchase & stock ---
    auto* buy = new QWidget; auto* bf = new QFormLayout(buy);
    price_ = new QDoubleSpinBox; price_->setRange(0, 1000000); price_->setDecimals(2); price_->setSuffix(" " + chui::currencySymbol(chui::currency()));
    supplier_ = new QLineEdit;
    purchaseDate_ = new QLineEdit; purchaseDate_->setPlaceholderText(chui::tr("YYYY-MM-DD"));
    receptionDate_ = new QLineEdit; receptionDate_->setPlaceholderText(chui::tr("YYYY-MM-DD"));
    warranty_ = new QLineEdit;
    quantity_ = new QSpinBox; quantity_->setRange(0, 1000000);
    minStock_ = new QSpinBox; minStock_->setRange(0, 1000000);
    idealStock_ = new QSpinBox; idealStock_->setRange(0, 1000000);
    bf->addRow(chui::tr("Unit price"), price_);
    bf->addRow(chui::tr("Supplier"), supplier_);
    bf->addRow(chui::tr("Purchase date"), purchaseDate_);
    bf->addRow(chui::tr("Reception date"), receptionDate_);
    bf->addRow(chui::tr("Warranty"), warranty_);
    bf->addRow(chui::tr("Quantity"), quantity_);
    bf->addRow(chui::tr("Minimum stock"), minStock_);
    bf->addRow(chui::tr("Ideal stock"), idealStock_);

    stockLabel_ = new QLabel;
    auto* mv = uikit::button(chui::tr("Stock movement..."), icons::Glyph::StockIn, "ghost");
    connect(mv, &QPushButton::clicked, this, &ComponentDialog::doStockMovement);
    auto* stockRow = new QHBoxLayout; stockRow->addWidget(stockLabel_, 1); stockRow->addWidget(mv);
    bf->addRow(stockRow);
    history_ = new QTableWidget(0, 4);
    history_->setHorizontalHeaderLabels({chui::tr("Date"), chui::tr("Type"), chui::tr("Qty"), chui::tr("Note")});
    history_->horizontalHeader()->setStretchLastSection(true);
    history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_->verticalHeader()->setVisible(false);
    history_->setFixedHeight(140);
    bf->addRow(history_);
    tabs->addTab(buy, chui::tr("Purchase & stock"));

    // --- Documents ---
    auto* doc = new QWidget; auto* dv = new QVBoxLayout(doc);
    childHint_ = uikit::hint(chui::tr("Save the component first to attach documents to it."));
    dv->addWidget(childHint_);
    auto* docBar = new QHBoxLayout;
    auto* dAdd = uikit::button(chui::tr("Add link"), icons::Glyph::Add);
    auto* dImport = uikit::button(chui::tr("Import file"), icons::Glyph::Add, "ghost");
    auto* dEdit = uikit::button(chui::tr("Edit"), icons::Glyph::Edit, "ghost");
    auto* dOpen = uikit::button(chui::tr("Open"), icons::Glyph::Link, "ghost");
    auto* dDel = uikit::button(chui::tr("Delete"), icons::Glyph::Delete, "danger");
    docBar->addWidget(dAdd); docBar->addWidget(dImport); docBar->addWidget(dEdit);
    docBar->addWidget(dOpen); docBar->addWidget(dDel); docBar->addStretch(1);
    dv->addLayout(docBar);
    documents_ = new QTableWidget(0, 3);
    documents_->setHorizontalHeaderLabels({chui::tr("Title"), chui::tr("Category"), chui::tr("Link / file")});
    documents_->horizontalHeader()->setStretchLastSection(true);
    documents_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    documents_->setSelectionBehavior(QAbstractItemView::SelectRows);
    documents_->verticalHeader()->setVisible(false);
    dv->addWidget(documents_, 1);
    tabs->addTab(doc, chui::tr("Documents"));
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
    tabs->addTab(note, chui::tr("Notes"));

    // --- Buttons ---
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(chui::tr("Save"));
    bb->button(QDialogButtonBox::Cancel)->setText(chui::tr("Cancel"));
    root->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, [this] { if (saveFromForm()) accept(); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ComponentDialog::loadToForm() {
    // Known categories (the field remains free).
    for (const auto& c : ctx_.inventory.listCategories())
        category_->addItem(QString::fromStdString(c.name));

    // Component types: offered from the reference list (editable field).
    type_->addItem("");
    for (const auto& r : ctx_.referentials.findByList("component_types"))
        type_->addItem(QString::fromStdString(r.value));

    // Locations, shown by hierarchical path.
    std::map<Id, Location> byId;
    for (const auto& l : ctx_.inventory.listLocations()) byId[l.id] = l;
    location_->addItem(chui::tr("(none)"), QString());   // QString vide = kNoId
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
        QMessageBox::warning(this, chui::tr("Required field"), chui::tr("The name is required."));
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

    // Inline addition to the reference list: if the typed type does not exist yet, we
    // offer to add it without leaving the record (consistent naming).
    if (!c.type.empty()) {
        bool known = false;
        for (const auto& r : ctx_.referentials.findByList("component_types"))
            if (r.value == c.type) { known = true; break; }
        if (!known) {
            const QString t = QString::fromStdString(c.type);
            if (QMessageBox::question(this, chui::tr("New type"),
                    chui::tr("The type \"%1\" does not exist in the reference list. Add it?").arg(t))
                == QMessageBox::Yes) {
                domain::RefItem r;
                r.list = "component_types";
                r.value = c.type;
                r.position = (int)ctx_.referentials.findByList("component_types").size();
                ctx_.referentials.save(r);
            }
        }
    }

    current_ = ctx_.inventory.saveComponent(c);  // gets the assigned id if it was a creation
    return true;
}

void ComponentDialog::refreshChildTabs() {
    const bool exists = current_.id != kNoId;
    childHint_->setVisible(!exists);
    documents_->setEnabled(exists);
    history_->setEnabled(exists);

    // Current stock.
    stockLabel_->setText(exists ? QString(chui::tr("Current stock: %1")).arg(current_.quantity)
                                : chui::tr("Stock: save the component first"));

    documents_->setRowCount(0);
    if (exists) {
        for (const auto& d : ctx_.documents_service.listForOwner(DocumentOwnerKind::Component, current_.id)) {
            int r = documents_->rowCount();
            documents_->insertRow(r);
            auto* t = new QTableWidgetItem(QString::fromStdString(d.title));
            t->setData(Qt::UserRole, QString::fromStdString(d.id));
            documents_->setItem(r, 0, t);
            documents_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(d.category)));
            // "Link / file" column: for an imported attachment, we
            // show the original name (not the hash, unreadable); for a link,
            // the URL. A file whose content is not there yet (received through
            // synchronization but binary not fetched yet) is flagged.
            QString shown = d.isFile() ? QString::fromStdString(d.fileName)
                                       : QString::fromStdString(d.url);
            if (d.isFile() && !ctx_.attachments.has(d.blobHash))
                shown += chui::tr("  (content not synchronized yet)");
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
    QString title = QInputDialog::getText(this, chui::tr("Document"), chui::tr("Title:"), QLineEdit::Normal,
                                          QString::fromStdString(d.title), &ok);
    if (!ok || title.trimmed().isEmpty()) return;
    QString category = QInputDialog::getText(this, chui::tr("Document"), chui::tr("Category (datasheet, manual, pinout, link...):"),
                                             QLineEdit::Normal, QString::fromStdString(d.category), &ok);
    if (!ok) return;
    QString url = QInputDialog::getText(this, chui::tr("Document"), chui::tr("Link (HTTP URL... or file path):"),
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
    const QString path = QFileDialog::getOpenFileName(this, chui::tr("Import file"), QString());
    if (path.isEmpty()) return;

    // Copies the file into the store, under the hash of its content. The content
    // becomes independent of the original file (movable, deletable).
    const auto res = ctx_.attachments.importFile(path.toStdString());
    if (!res.ok) {
        QMessageBox::warning(this, chui::tr("Import", "title"), chui::tr("Unable to read the selected file."));
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
            QMessageBox::information(this, chui::tr("Attachment"),
                chui::tr("The content of this file is not yet available on this computer. It will be retrieved during the next synchronization with morfSync."));
            return;
        }
        // The store names the file after its hash (no extension): we
        // create a temporary copy under the original name so that the system
        // knows which application to open it with.
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
    if (QMessageBox::question(this, chui::tr("Delete"), chui::tr("Delete this document?")) != QMessageBox::Yes) return;
    ctx_.documents_service.remove(id);
    refreshChildTabs();
}

void ComponentDialog::doStockMovement() {
    if (current_.id == kNoId) {
        QMessageBox::information(this, chui::tr("Stock"), chui::tr("Save the component first."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(chui::tr("Stock movement"));
    auto* f = new QFormLayout(&dlg);
    auto* type = new QComboBox;
    type->addItem(chui::tr("Incoming (+)"), "in");
    type->addItem(chui::tr("Outgoing (−)"), "out");
    type->addItem(chui::tr("Correction (=)"), "correction");
    type->addItem(chui::tr("Inventory (=)"), "inventory");
    auto* qty = new QSpinBox; qty->setRange(0, 1000000);
    auto* note = new QLineEdit;
    f->addRow(chui::tr("Type"), type);
    f->addRow(chui::tr("Quantity"), qty);
    f->addRow(chui::tr("Note"), note);
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
