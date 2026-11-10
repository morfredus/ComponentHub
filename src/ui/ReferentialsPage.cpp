#include "ui/ReferentialsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <set>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Language.h"
#include "csv.h"

using domain::RefItem;
using domain::Component;

namespace {
// Component field linked to a reference list (to propagate renames/merges).
enum Bind { None, Type, Manufacturer, Supplier, State };

struct RefDef { const char* key; const char* label; int bind; };
const RefDef kDefs[] = {
    {"component_types", CHUI_NOOP("Component types"), Type},
    {"manufacturers",   CHUI_NOOP("Manufacturers"),          Manufacturer},
    {"packages",        CHUI_NOOP("Packages"),  None},
    {"suppliers",       CHUI_NOOP("Suppliers"),        Supplier},
    {"technologies",    CHUI_NOOP("Technologies"),        None},
    {"states",          CHUI_NOOP("States"),               State},
    {"keywords",        CHUI_NOOP("Keywords"),           None},
};

// Default values of the first list (types); translated with the rest of the interface.
const char* const kDefaultTypes[] = {
    CHUI_NOOP("Resistor"), CHUI_NOOP("Capacitor"), CHUI_NOOP("Diode"), CHUI_NOOP("LED"),
    CHUI_NOOP("Transistor"), CHUI_NOOP("MOSFET"), CHUI_NOOP("Regulator"), CHUI_NOOP("Integrated circuit"),
    CHUI_NOOP("Microcontroller"), CHUI_NOOP("Sensor"), CHUI_NOOP("Display"), CHUI_NOOP("Connector"),
    CHUI_NOOP("Relay"), CHUI_NOOP("Module"), CHUI_NOOP("Power supply"), CHUI_NOOP("Crystal"),
    CHUI_NOOP("Inductor"), CHUI_NOOP("Fuse"), CHUI_NOOP("Push button"), CHUI_NOOP("Potentiometer"),
};

std::string* fieldOf(Component& c, int bind) {
    switch (bind) {
        case Type:         return &c.type;
        case Manufacturer: return &c.manufacturer;
        case Supplier:     return &c.supplier;
        case State:        return &c.state;
        default:           return nullptr;
    }
}
} // namespace

ReferentialsPage::ReferentialsPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(12);
    v->addWidget(uikit::pageTitle(chui::tr("Reference data")));
    v->addWidget(uikit::hint(chui::tr("Standardized values proposed during data entry for consistent naming (avoid \"Sensor / Sensors / Capteur\"). Renaming or merging a value from a linked reference list (type, manufacturer, supplier, state) updates the affected components.")));

    auto* split = new QHBoxLayout;
    v->addLayout(split, 1);

    referentials_ = new QListWidget;
    referentials_->setFixedWidth(230);
    split->addWidget(referentials_);

    auto* right = new QVBoxLayout;
    auto* bar = new QHBoxLayout;
    auto* add = uikit::button(chui::tr("Add"), icons::Glyph::Add);
    auto* edit = uikit::button(chui::tr("Rename"), icons::Glyph::Edit, "ghost");
    auto* del = uikit::button(chui::tr("Delete"), icons::Glyph::Delete, "danger");
    auto* up = uikit::button("", icons::Glyph::MoveUp, "ghost");
    auto* down = uikit::button("", icons::Glyph::MoveDown, "ghost");
    auto* merge = uikit::button(chui::tr("Merge"), icons::Glyph::Merge, "ghost");
    bar->addWidget(add); bar->addWidget(edit); bar->addWidget(del);
    bar->addWidget(up); bar->addWidget(down); bar->addWidget(merge);
    bar->addStretch(1);
    auto* imp = uikit::button(chui::tr("Import"), icons::Glyph::Restore, "ghost");
    auto* exp = uikit::button(chui::tr("Export"), icons::Glyph::Backup, "ghost");
    bar->addWidget(imp); bar->addWidget(exp);
    right->addLayout(bar);

    values_ = new QListWidget;
    values_->setSelectionMode(QAbstractItemView::ExtendedSelection);  // multi-selection for merging
    right->addWidget(values_, 1);
    split->addLayout(right, 1);

    connect(referentials_, &QListWidget::currentRowChanged, this, [this](int) { selectReferential(); });
    connect(add, &QPushButton::clicked, this, &ReferentialsPage::addValue);
    connect(edit, &QPushButton::clicked, this, &ReferentialsPage::editValue);
    connect(values_, &QListWidget::itemDoubleClicked, this, &ReferentialsPage::editValue);
    connect(del, &QPushButton::clicked, this, &ReferentialsPage::deleteValue);
    connect(up, &QPushButton::clicked, this, [this] { moveValue(-1); });
    connect(down, &QPushButton::clicked, this, [this] { moveValue(1); });
    connect(merge, &QPushButton::clicked, this, &ReferentialsPage::mergeValues);
    connect(imp, &QPushButton::clicked, this, &ReferentialsPage::importCsv);
    connect(exp, &QPushButton::clicked, this, &ReferentialsPage::exportCsv);

    loadDefinitions();
}

void ReferentialsPage::showEvent(QShowEvent*) {
    if (referentials_->currentRow() < 0 && referentials_->count() > 0) referentials_->setCurrentRow(0);
    else selectReferential();
}

void ReferentialsPage::loadDefinitions() {
    referentials_->clear();
    for (const auto& d : kDefs) {
        auto* it = new QListWidgetItem(chui::tr(d.label));
        it->setData(Qt::UserRole, QString::fromUtf8(d.key));
        it->setData(Qt::UserRole + 1, d.bind);
        referentials_->addItem(it);
    }
    referentials_->setCurrentRow(0);
}

QString ReferentialsPage::currentKey() const {
    auto* it = referentials_->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}
int ReferentialsPage::currentBind() const {
    auto* it = referentials_->currentItem();
    return it ? it->data(Qt::UserRole + 1).toInt() : None;
}

void ReferentialsPage::selectReferential() {
    const QString key = currentKey();
    if (key.isEmpty()) return;
    seedIfEmpty(key, currentBind());
    reloadValues();
}

void ReferentialsPage::seedIfEmpty(const QString& key, int bind) {
    if (!ctx_.referentials.findByList(key.toStdString()).empty()) return;

    // Initial values: the distinct values already present in the
    // components (for a linked field) + the default list for types.
    std::set<std::string> values;
    if (bind != None) {
        for (const auto& c : ctx_.components.findAll()) {
            if (std::string* f = const_cast<std::string*>(fieldOf(const_cast<Component&>(c), bind)))
                if (!f->empty()) values.insert(*f);
        }
    }
    if (key == "component_types") {
        for (const char* v : kDefaultTypes) values.insert(chui::tr(v).toStdString());
    }
    if (values.empty()) return;

    std::vector<RefItem> items;
    int pos = 0;
    for (const auto& val : values) {  // std::set -> alphabetical order
        RefItem r; r.list = key.toStdString(); r.value = val; r.position = pos++;
        items.push_back(r);
    }
    ctx_.referentials.saveAll(items);
}

void ReferentialsPage::reloadValues() {
    const QString keep = values_->currentItem() ? values_->currentItem()->text() : QString();
    values_->clear();
    for (const auto& r : ctx_.referentials.findByList(currentKey().toStdString())) {
        auto* it = new QListWidgetItem(QString::fromStdString(r.value));
        it->setData(Qt::UserRole, QString::fromStdString(r.id));
        values_->addItem(it);
        if (it->text() == keep) values_->setCurrentItem(it);
    }
}

int ReferentialsPage::usageCount(const QString& value, int bind) const {
    if (bind == None) return 0;
    int n = 0;
    const std::string v = value.toStdString();
    for (auto& c : ctx_.components.findAll()) {
        Component cc = c;
        if (std::string* f = fieldOf(cc, bind)) if (*f == v) n++;
    }
    return n;
}

int ReferentialsPage::reassignComponentField(const QString& from, const QString& to, int bind) {
    if (bind == None) return 0;
    const std::string f = from.toStdString(), t = to.toStdString();
    std::vector<Component> changed;
    for (auto& c : ctx_.components.findAll()) {
        Component cc = c;
        if (std::string* field = fieldOf(cc, bind)) {
            if (*field == f) { *field = t; changed.push_back(cc); }
        }
    }
    if (!changed.empty()) ctx_.components.saveAll(changed);
    return (int)changed.size();
}

void ReferentialsPage::addValue() {
    if (currentKey().isEmpty()) return;
    const auto existing = ctx_.referentials.findByList(currentKey().toStdString());
    auto exists = [&](const QString& v) {
        for (const auto& r : existing)
            if (QString::fromStdString(r.value).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    // Asks again while the typed value already exists (blocking popup, pre-filled
    // input to let the user correct it).
    QString val;
    while (true) {
        bool ok = false;
        val = QInputDialog::getText(this, chui::tr("New value"), chui::tr("Value:"), QLineEdit::Normal, val, &ok).trimmed();
        if (!ok || val.isEmpty()) return;
        if (!exists(val)) break;
        QMessageBox::warning(this, chui::tr("Name already used"), chui::tr("\"%1\" already exists in this reference list.\nChoose another name or correct your entry.").arg(val));
    }

    RefItem r;
    r.list = currentKey().toStdString();
    r.value = val.toStdString();
    r.position = (int)existing.size();
    ctx_.referentials.save(r);
    reloadValues();
}

void ReferentialsPage::editValue() {
    auto* it = values_->currentItem();
    if (!it) return;
    const domain::Id id = it->data(Qt::UserRole).toString().toStdString();
    const QString oldVal = it->text();

    const auto siblings = ctx_.referentials.findByList(currentKey().toStdString());
    // Rejects a duplicate of ANOTHER value (the value itself is excluded, to
    // allow a simple change of case).
    auto existsOther = [&](const QString& v) {
        for (const auto& r : siblings)
            if (r.id != id && QString::fromStdString(r.value).compare(v, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    QString val = oldVal;
    while (true) {
        bool ok = false;
        val = QInputDialog::getText(this, chui::tr("Rename"), chui::tr("Value:"), QLineEdit::Normal, val, &ok).trimmed();
        if (!ok || val.isEmpty() || val == oldVal) return;
        if (!existsOther(val)) break;
        QMessageBox::warning(this, chui::tr("Name already used"), chui::tr("\"%1\" already exists in this reference list.\nChoose another name or correct your entry.").arg(val));
    }

    RefItem r;
    r.id = id;
    r.list = currentKey().toStdString();
    r.value = val.toStdString();
    for (const auto& e : siblings) if (e.id == id) { r.position = e.position; break; }
    ctx_.referentials.save(r);

    const int n = reassignComponentField(oldVal, val, currentBind());
    reloadValues();
    if (n > 0) QMessageBox::information(this, chui::tr("Renamed"), QString(chui::tr("%1 component(s) updated.")).arg(n));
}

void ReferentialsPage::deleteValue() {
    auto* it = values_->currentItem();
    if (!it) return;
    const QString val = it->text();
    const int used = usageCount(val, currentBind());
    QString msg = chui::tr("Delete the value \"%1\" from the reference list?").arg(val);
    if (used > 0)
        msg += "\n\n" + chui::tr("%1 component(s) still use it; their field will not be changed (consider merging instead of deleting to reassign).").arg(used);
    if (QMessageBox::question(this, chui::tr("Delete"), msg) != QMessageBox::Yes) return;
    ctx_.referentials.remove(it->data(Qt::UserRole).toString().toStdString());
    reloadValues();
}

void ReferentialsPage::moveValue(int delta) {
    int row = values_->currentRow();
    if (row < 0) return;
    auto items = ctx_.referentials.findByList(currentKey().toStdString());
    int target = row + delta;
    if (target < 0 || target >= (int)items.size()) return;
    std::swap(items[row].position, items[target].position);
    ctx_.referentials.saveAll({items[row], items[target]});
    reloadValues();
    values_->setCurrentRow(target);
}

void ReferentialsPage::mergeValues() {
    auto selected = values_->selectedItems();
    if (selected.size() < 2) {
        QMessageBox::information(this, chui::tr("Merge", "title"), chui::tr("Select at least two values to merge (Ctrl-click for multiple selection)."));
        return;
    }
    QStringList names;
    for (auto* it : selected) names << it->text();

    // Choice of the target value (the one kept).
    QDialog dlg(this);
    dlg.setWindowTitle(chui::tr("Merge values"));
    auto* f = new QFormLayout(&dlg);
    f->addRow(new QLabel(chui::tr("The selected values will be merged into one.")));
    auto* target = new QComboBox; target->addItems(names);
    f->addRow(chui::tr("Value to keep"), target);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    f->addRow(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString keep = target->currentText();
    const int bind = currentBind();
    int reassigned = 0;
    for (auto* it : selected) {
        const QString src = it->text();
        if (src == keep) continue;
        reassigned += reassignComponentField(src, keep, bind);
        ctx_.referentials.remove(it->data(Qt::UserRole).toString().toStdString());  // removes the source value from the reference list
    }
    reloadValues();
    QMessageBox::information(this, chui::tr("Merge complete"),
        QString(chui::tr("Values merged into \"%1\".%2")).arg(keep)
            .arg(reassigned > 0 ? QString(chui::tr("\n%1 component(s) reassigned.")).arg(reassigned) : QString()));
}

void ReferentialsPage::exportCsv() {
    if (currentKey().isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(this, chui::tr("Export reference list"),
        "referentiel_" + currentKey() + ".csv", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { QMessageBox::warning(this, chui::tr("Export", "title"), chui::tr("Unable to write.")); return; }
    f.write(domain::kUtf8Bom);
    for (const auto& r : ctx_.referentials.findByList(currentKey().toStdString()))
        f.write((r.value + "\n").c_str());
    f.close();
    QMessageBox::information(this, chui::tr("Export", "title"), chui::tr("Reference list exported:\n%1").arg(path));
}

void ReferentialsPage::importCsv() {
    if (currentKey().isEmpty()) return;
    const QString path = QFileDialog::getOpenFileName(this, chui::tr("Import reference list"), QString(), "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, chui::tr("Import", "title"), chui::tr("Unable to read.")); return; }
    QString text = QString::fromUtf8(f.readAll());
    f.close();
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);  // BOM

    const std::string list = currentKey().toStdString();
    auto existing = ctx_.referentials.findByList(list);
    std::set<QString> known;
    for (const auto& r : existing) known.insert(QString::fromStdString(r.value).toLower());
    int pos = (int)existing.size(), added = 0;
    for (QString line : text.split('\n')) {
        line = line.split(';').first().trimmed();  // 1st column if separators
        if (line.isEmpty() || line.toLower() == "value" || line.toLower() == "valeur") continue;
        if (known.count(line.toLower())) continue;
        known.insert(line.toLower());
        RefItem r; r.list = list; r.value = line.toStdString(); r.position = pos++;
        ctx_.referentials.save(r);
        added++;
    }
    reloadValues();
    QMessageBox::information(this, chui::tr("Import complete"), QString(chui::tr("%1 value(s) added.")).arg(added));
}
