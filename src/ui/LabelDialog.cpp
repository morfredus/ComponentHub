#include "ui/LabelDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMarginsF>
#include <QMessageBox>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include "ui/Language.h"

namespace {

struct LabelFormat {
    QString id;
    QString name;
    double pageWidthMm = 210;
    double pageHeightMm = 297;
    int columns = 6;
    int rows = 16;
    double labelWidthMm = 30.5;
    double labelHeightMm = 16.9;
    double marginLeftMm = 7;
    double marginTopMm = 13.3;
    double pitchXmm = 32.6;
    double pitchYmm = 16.9;
    bool builtin = false;
};

QVector<LabelFormat> customFormats() {
    QSettings settings("morfredus", "ComponentHub");
    QVector<LabelFormat> formats;
    const int count = settings.beginReadArray("labels/formats");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        LabelFormat f;
        f.id = settings.value("id").toString();
        f.name = settings.value("name").toString();
        f.pageWidthMm = settings.value("pageWidthMm", 210).toDouble();
        f.pageHeightMm = settings.value("pageHeightMm", 297).toDouble();
        f.columns = settings.value("columns", 6).toInt();
        f.rows = settings.value("rows", 16).toInt();
        f.labelWidthMm = settings.value("labelWidthMm", 30.5).toDouble();
        f.labelHeightMm = settings.value("labelHeightMm", 16.9).toDouble();
        f.marginLeftMm = settings.value("marginLeftMm", 7).toDouble();
        f.marginTopMm = settings.value("marginTopMm", 13.3).toDouble();
        f.pitchXmm = settings.value("pitchXmm", f.labelWidthMm).toDouble();
        f.pitchYmm = settings.value("pitchYmm", f.labelHeightMm).toDouble();
        // Une entree corrompue (dimension nulle, grille vide) ferait diviser par zero au rendu.
        const bool sane = f.columns > 0 && f.rows > 0 && f.pageWidthMm > 0 && f.pageHeightMm > 0
                       && f.labelWidthMm > 0 && f.labelHeightMm > 0;
        if (!f.id.isEmpty() && !f.name.isEmpty() && sane) formats.append(f);
    }
    settings.endArray();
    return formats;
}

void saveCustomFormats(const QVector<LabelFormat>& formats) {
    QSettings settings("morfredus", "ComponentHub");
    // beginWriteArray n'efface pas les anciennes entrees : sans remove(), supprimer un format
    // laisserait l'ancien dernier element dans les QSettings.
    settings.remove("labels/formats");
    settings.beginWriteArray("labels/formats");
    for (int i = 0; i < formats.size(); ++i) {
        settings.setArrayIndex(i);
        const auto& f = formats.at(i);
        settings.setValue("id", f.id); settings.setValue("name", f.name);
        settings.setValue("pageWidthMm", f.pageWidthMm); settings.setValue("pageHeightMm", f.pageHeightMm);
        settings.setValue("columns", f.columns); settings.setValue("rows", f.rows);
        settings.setValue("labelWidthMm", f.labelWidthMm); settings.setValue("labelHeightMm", f.labelHeightMm);
        settings.setValue("marginLeftMm", f.marginLeftMm); settings.setValue("marginTopMm", f.marginTopMm);
        settings.setValue("pitchXmm", f.pitchXmm); settings.setValue("pitchYmm", f.pitchYmm);
    }
    settings.endArray();
}

// Formats livres avec le logiciel, puis formats personnalises (QSettings). Un identifiant
// inconnu (format supprime entre-temps) renvoie std::nullopt : l'appelant previent l'utilisateur
// au lieu d'imprimer avec des valeurs par defaut.
std::optional<LabelFormat> findFormat(const QString& id) {
    LabelFormat f;
    f.builtin = true;
    f.id = id;
    if (id == "avery8728") {
        f.name = "Avery No. 8728";  // les valeurs par defaut de LabelFormat sont celles de l'Avery 8728
        return f;
    }
    if (id == "avery8790") {
        f.name = "Avery No. 8790";
        f.columns = 10; f.rows = 27;
        f.labelWidthMm = 17.8; f.labelHeightMm = 10;
        f.marginLeftMm = 4; f.marginTopMm = 14.01;
        f.pitchXmm = 20.5; f.pitchYmm = 10;
        return f;
    }
    for (const auto& candidate : customFormats())
        if (candidate.id == id) return candidate;
    return std::nullopt;
}

bool editFormat(QWidget* parent, LabelFormat& f) {
    QDialog dialog(parent);
    dialog.setWindowTitle(chui::tr("New custom label format"));
    auto* form = new QFormLayout(&dialog);
    auto* name = new QLineEdit(f.name);
    auto* pageW = new QDoubleSpinBox; pageW->setRange(1, 1000); pageW->setValue(f.pageWidthMm); pageW->setSuffix(" mm");
    auto* pageH = new QDoubleSpinBox; pageH->setRange(1, 1000); pageH->setValue(f.pageHeightMm); pageH->setSuffix(" mm");
    auto* cols = new QSpinBox; cols->setRange(1, 100); cols->setValue(f.columns);
    auto* rows = new QSpinBox; rows->setRange(1, 100); rows->setValue(f.rows);
    auto* labelW = new QDoubleSpinBox; labelW->setRange(1, 500); labelW->setValue(f.labelWidthMm); labelW->setSuffix(" mm");
    auto* labelH = new QDoubleSpinBox; labelH->setRange(1, 500); labelH->setValue(f.labelHeightMm); labelH->setSuffix(" mm");
    auto* marginL = new QDoubleSpinBox; marginL->setRange(0, 500); marginL->setValue(f.marginLeftMm); marginL->setSuffix(" mm");
    auto* marginT = new QDoubleSpinBox; marginT->setRange(0, 500); marginT->setValue(f.marginTopMm); marginT->setSuffix(" mm");
    auto* pitchX = new QDoubleSpinBox; pitchX->setRange(1, 500); pitchX->setValue(f.pitchXmm); pitchX->setSuffix(" mm");
    auto* pitchY = new QDoubleSpinBox; pitchY->setRange(1, 500); pitchY->setValue(f.pitchYmm); pitchY->setSuffix(" mm");
    form->addRow(chui::tr("Name"), name); form->addRow(chui::tr("Page width"), pageW); form->addRow(chui::tr("Page height"), pageH);
    form->addRow(chui::tr("Columns"), cols); form->addRow(chui::tr("Rows"), rows); form->addRow(chui::tr("Label width"), labelW); form->addRow(chui::tr("Label height"), labelH);
    form->addRow(chui::tr("Left margin"), marginL); form->addRow(chui::tr("Top margin"), marginT);
    form->addRow(chui::tr("Horizontal pitch"), pitchX); form->addRow(chui::tr("Vertical pitch"), pitchY);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) return false;
    f.name = name->text().trimmed(); f.pageWidthMm = pageW->value(); f.pageHeightMm = pageH->value(); f.columns = cols->value(); f.rows = rows->value();
    f.labelWidthMm = labelW->value(); f.labelHeightMm = labelH->value(); f.marginLeftMm = marginL->value(); f.marginTopMm = marginT->value();
    f.pitchXmm = pitchX->value(); f.pitchYmm = pitchY->value();
    return true;
}

struct LabelField {
    bool enabled;
    int size;
};

struct Line {
    QString text;
    int size;
};

QStringList componentSpecifications(const domain::Component& component) {
    QStringList specifications;
    if (!component.voltage.empty()) specifications << QString::fromStdString(component.voltage);
    if (!component.current.empty()) specifications << QString::fromStdString(component.current);
    if (!component.interfaceType.empty()) specifications << QString::fromStdString(component.interfaceType);
    if (!component.protocols.empty()) specifications << QString::fromStdString(component.protocols);
    if (!component.i2cAddress.empty()) specifications << QString::fromStdString(component.i2cAddress);
    if (!component.frequency.empty()) specifications << QString::fromStdString(component.frequency);
    if (component.pinCount > 0) specifications << chui::tr("%1 pins").arg(component.pinCount);
    if (!component.compatibility.empty()) specifications << QString::fromStdString(component.compatibility);
    return specifications;
}

// Source unique des lignes d'une etiquette : l'apercu et l'impression l'appellent tous les
// deux, pour que l'ecran montre exactement ce qui sort sur le papier.
QVector<Line> buildLines(const domain::Component& c, const QVector<LabelField>& fields) {
    QVector<Line> lines;
    auto enabled = [&fields](int i) { return i < fields.size() && fields.at(i).enabled; };
    auto add = [&](int i, const QString& prefix, const std::string& value) {
        if (enabled(i) && !value.empty())
            lines.append({prefix + QString::fromStdString(value), fields.at(i).size});
    };
    if (enabled(0)) lines.append({QString::fromStdString(c.name), fields.at(0).size});
    add(1, chui::tr("Référence") + ": ", c.reference);
    add(2, chui::tr("Catégorie") + ": ", c.category);
    add(3, chui::tr("Sous-catégorie") + ": ", c.subcategory);
    add(4, chui::tr("Type") + ": ", c.type);
    if (enabled(5)) {
        const QStringList specifications = componentSpecifications(c);
        if (!specifications.isEmpty()) lines.append({specifications.join(" "), fields.at(5).size});
    }
    return lines;
}

int fittingPixelSize(const QString& text, int requested, const QFont& base, int width,
                     const QPaintDevice* device) {
    int size = requested;
    while (size > 4) {
        QFont font = base;
        font.setPixelSize(size);
        if (QFontMetrics(font, device).horizontalAdvance(text) <= width) break;
        --size;
    }
    return size;
}

// Dessine les lignes centrees verticalement dans `label`, jamais en dehors : si le bloc est
// trop haut (petite etiquette, beaucoup de champs), toutes les polices sont reduites d'un cran
// jusqu'a ce qu'il tienne, et le dessin est rogne au rectangle de l'etiquette par securite.
// Les tailles sont proportionnelles a la largeur de l'etiquette, ce qui donne le meme rendu
// a l'ecran (apercu) et sur papier (300 dpi).
void drawLabelText(QPainter& painter, const QRectF& label, const QVector<Line>& lines) {
    if (lines.isEmpty()) return;
    const int padX = static_cast<int>(label.width() * 0.05);
    const int textWidth = std::max(1, static_cast<int>(label.width()) - 2 * padX);
    const int spacing = std::max(1, static_cast<int>(label.height() * 0.04));

    QVector<QFont> fonts;
    QVector<int> heights;
    int totalHeight = 0;
    for (double scale = 1.0;; scale *= 0.9) {
        fonts.clear();
        heights.clear();
        totalHeight = spacing * (static_cast<int>(lines.size()) - 1);
        for (const auto& line : lines) {
            QFont font = painter.font();
            const int requested = std::max(4, static_cast<int>(
                std::lround(line.size * 96.0 / 72.0 * label.width() / 400.0 * scale)));
            font.setPixelSize(fittingPixelSize(line.text, requested, font, textWidth, painter.device()));
            fonts.append(font);
            heights.append(QFontMetrics(font, painter.device()).height());
            totalHeight += heights.last();
        }
        if (totalHeight <= label.height() * 0.95 || scale < 0.2) break;
    }

    painter.save();
    painter.setClipRect(label);
    int y = static_cast<int>(label.top() + (label.height() - totalHeight) / 2.0);
    for (int i = 0; i < lines.size(); ++i) {
        painter.setFont(fonts.at(i));
        painter.drawText(QRectF(label.left() + padX, y, textWidth, heights.at(i)),
                         Qt::AlignLeft | Qt::AlignVCenter, lines.at(i).text);
        y += heights.at(i) + spacing;
    }
    painter.restore();
}

void renderLabels(QPagedPaintDevice& device, QPainter& painter,
                  const std::vector<domain::Component>& components,
                  const LabelFormat& format, const QVector<LabelField>& fields,
                  int startSlot, bool calibrationBorders) {
    const int labelsPerPage = format.columns * format.rows;
    const qreal mm = device.width() / format.pageWidthMm;
    const qreal labelW = format.labelWidthMm * mm;
    const qreal labelH = format.labelHeightMm * mm;
    const qreal marginX = format.marginLeftMm * mm;
    const qreal marginY = format.marginTopMm * mm;

    for (int index = 0; index < static_cast<int>(components.size()); ++index) {
        const int absoluteSlot = startSlot + index;
        if (absoluteSlot && absoluteSlot % labelsPerPage == 0) device.newPage();
        const int slot = absoluteSlot % labelsPerPage;
        const QRectF label(marginX + (slot % format.columns) * format.pitchXmm * mm,
                           marginY + (slot / format.columns) * format.pitchYmm * mm,
                           labelW, labelH);
        if (calibrationBorders) {
            painter.setPen(QPen(Qt::red, 1));
            painter.drawRect(label);
        }
        painter.setPen(Qt::black);
        drawLabelText(painter, label, buildLines(components.at(index), fields));
    }
}

class LabelPreview : public QWidget {
public:
    // `component` peut etre nul (aucune etiquette a montrer) : l'apercu reste alors vide.
    explicit LabelPreview(const domain::Component* component, QWidget* parent = nullptr)
        : QWidget(parent), component_(component) {
        setMinimumSize(430, 250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    void setFields(const QVector<LabelField>& fields) {
        fields_ = fields;
        update();
    }

    void setComponent(const domain::Component& component) {
        component_ = &component;
        update();
    }

    void setLabelSize(double widthMm, double heightMm) {
        labelAspect_ = widthMm / heightMm;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Window));
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal availableWidth = width() - 56;
        const qreal availableHeight = height() - 70;
        qreal labelWidth = availableWidth;
        qreal labelHeight = labelWidth / labelAspect_;
        if (labelHeight > availableHeight) {
            labelHeight = availableHeight;
            labelWidth = labelHeight * labelAspect_;
        }
        const QRectF label((width() - labelWidth) / 2.0,
                           (height() - labelHeight) / 2.0,
                           labelWidth, labelHeight);
        painter.setBrush(Qt::white);
        painter.setPen(QPen(QColor("#88909a"), 2));
        painter.drawRoundedRect(label, 8, 8);

        if (!component_) return;
        painter.setPen(QColor("#18202a"));
        drawLabelText(painter, label, buildLines(*component_, fields_));
    }

private:
    const domain::Component* component_;
    QVector<LabelField> fields_{{true, 24}, {true, 16}, {false, 16}, {false, 16}, {false, 16}, {true, 16}};
    double labelAspect_ = 30.5 / 16.9;
};

int pointSizeFor(int comboIndex) { return comboIndex == 0 ? 24 : comboIndex == 1 ? 16 : 12; }

} // namespace

LabelDialog::LabelDialog(const std::vector<domain::Component>& components, QWidget* parent)
    : QDialog(parent), components_(components) {
    setWindowTitle(chui::tr("Labels"));
    resize(760, 430);

    auto* root = new QVBoxLayout(this);
    auto* content = new QHBoxLayout;
    auto* options = new QGroupBox(chui::tr("Label fields"));
    auto* optionLayout = new QVBoxLayout(options);
    const QStringList labels = {
        chui::tr("Nom"), chui::tr("Référence"), chui::tr("Catégorie"),
        chui::tr("Sous-catégorie"), chui::tr("Type"), chui::tr("Caractéristiques")
    };
    const QVector<bool> defaults{true, true, false, false, false, true};
    QVector<QCheckBox*> checks;
    QVector<QComboBox*> sizes;
    const QStringList sizeLabels = {chui::tr("Large"), chui::tr("Medium"), chui::tr("Small")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* row = new QWidget(options);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* check = new QCheckBox(labels.at(i));
        check->setChecked(defaults.at(i));
        checks.append(check);
        auto* size = new QComboBox;
        size->addItems(sizeLabels);
        size->setCurrentIndex(i == 0 ? 0 : 1);
        sizes.append(size);
        rowLayout->addWidget(check, 1);
        rowLayout->addWidget(size);
        optionLayout->addWidget(row);
    }
    optionLayout->addStretch(1);

    auto* previewBox = new QGroupBox(chui::tr("Preview"));
    auto* previewLayout = new QVBoxLayout(previewBox);
    // components_ est un membre : les pointeurs donnes a l'apercu restent valides tant que le
    // dialogue existe, et il n'est jamais modifie apres la construction.
    auto* preview = new LabelPreview(components_.empty() ? nullptr : &components_.front());
    preview->setLabelSize(30.5, 16.9);
    previewLayout->addWidget(preview);
    auto* previous = new QPushButton("<", previewBox);
    auto* next = new QPushButton(">", previewBox);
    auto* previewPosition = new QLabel(previewBox);
    previewPosition->setAlignment(Qt::AlignCenter);
    auto* previewNavigation = new QHBoxLayout;
    previewNavigation->addStretch(1);
    previewNavigation->addWidget(previous);
    previewNavigation->addWidget(previewPosition);
    previewNavigation->addWidget(next);
    previewNavigation->addStretch(1);
    previewLayout->addLayout(previewNavigation);
    auto previewIndex = std::make_shared<int>(0);
    const int total = static_cast<int>(components_.size());
    auto showPreview = [this, preview, previous, next, previewPosition, previewIndex, total] {
        if (total > 0) preview->setComponent(components_.at(static_cast<size_t>(*previewIndex)));
        previous->setEnabled(*previewIndex > 0);
        next->setEnabled(*previewIndex + 1 < total);
        previewPosition->setText(QStringLiteral("%1/%2").arg(total > 0 ? *previewIndex + 1 : 0).arg(total));
    };
    connect(previous, &QPushButton::clicked, this, [previewIndex, showPreview] {
        if (*previewIndex <= 0) return;
        --*previewIndex;
        showPreview();
    });
    connect(next, &QPushButton::clicked, this, [previewIndex, total, showPreview] {
        if (*previewIndex + 1 >= total) return;
        ++*previewIndex;
        showPreview();
    });
    showPreview();
    content->addWidget(options);
    content->addWidget(previewBox, 1);
    root->addLayout(content, 1);

    auto* paper = new QGroupBox(chui::tr("Paper configuration"));
    auto* paperLayout = new QVBoxLayout(paper);
    auto* formatRow = new QHBoxLayout;
    auto* optionsRow = new QHBoxLayout;
    auto* paperBox = new QComboBox;
    paperBox->addItem(chui::tr("Avery No. 8728 - 30.5 x 16.9 mm - 6 x 16 labels"), "avery8728");
    paperBox->addItem(chui::tr("Avery No. 8790 - 17.8 x 10 mm - 10 x 27 labels"), "avery8790");
    for (const auto& format : customFormats()) paperBox->addItem(format.name, format.id);
    auto* newFormat = new QPushButton(chui::tr("New custom format"), paper);
    auto* editFormatButton = new QPushButton(chui::tr("Edit format"), paper);
    auto* deleteFormat = new QPushButton(chui::tr("Delete format"), paper);
    auto* border = new QCheckBox(chui::tr("Show calibration borders"), paper);
    auto* startRow = new QSpinBox;
    auto* startColumn = new QSpinBox;
    startRow->setRange(1, 16); startRow->setValue(1);
    startColumn->setRange(1, 6); startColumn->setValue(1);
    formatRow->addWidget(paperBox, 1);
    formatRow->addWidget(newFormat); formatRow->addWidget(editFormatButton); formatRow->addWidget(deleteFormat);
    editFormatButton->setEnabled(false);
    deleteFormat->setEnabled(false);
    optionsRow->addWidget(new QLabel(chui::tr("Start row"), paper)); optionsRow->addWidget(startRow);
    optionsRow->addWidget(new QLabel(chui::tr("Start column"), paper)); optionsRow->addWidget(startColumn);
    optionsRow->addWidget(border); optionsRow->addStretch(1);
    paperLayout->addLayout(formatRow);
    paperLayout->addLayout(optionsRow);
    root->addWidget(paper);

    auto selectedFields = [checks, sizes] {
        QVector<LabelField> fields;
        for (int i = 0; i < sizes.size(); ++i)
            fields.append({checks.at(i)->isChecked(), pointSizeFor(sizes.at(i)->currentIndex())});
        return fields;
    };
    auto updatePreview = [preview, selectedFields] { preview->setFields(selectedFields()); };
    auto saveFields = [checks, sizes, paperBox] {
        const QString id = paperBox->currentData().toString();
        if (id.isEmpty()) return;
        QSettings settings("morfredus", "ComponentHub");
        settings.beginGroup("labels/fields/" + id);
        for (int i = 0; i < checks.size(); ++i) {
            settings.setValue(QStringLiteral("enabled%1").arg(i), checks.at(i)->isChecked());
            settings.setValue(QStringLiteral("size%1").arg(i), sizes.at(i)->currentIndex());
        }
        settings.endGroup();
    };
    auto loadFields = [checks, sizes, paperBox, defaults, updatePreview] {
        const QString id = paperBox->currentData().toString();
        QSettings settings("morfredus", "ComponentHub");
        settings.beginGroup("labels/fields/" + id);
        for (int i = 0; i < checks.size(); ++i) {
            const QSignalBlocker checkBlocker(checks.at(i));
            const QSignalBlocker sizeBlocker(sizes.at(i));
            checks.at(i)->setChecked(settings.value(QStringLiteral("enabled%1").arg(i), defaults.at(i)).toBool());
            // Un index enregistre hors plage (fichier edite a la main) retomberait sur -1 : on borne.
            const int saved = settings.value(QStringLiteral("size%1").arg(i), i == 0 ? 0 : 1).toInt();
            sizes.at(i)->setCurrentIndex(std::clamp(saved, 0, sizes.at(i)->count() - 1));
        }
        settings.endGroup();
        updatePreview();
    };

    connect(paperBox, &QComboBox::currentIndexChanged, this,
            [paperBox, preview, startRow, startColumn, editFormatButton, deleteFormat, loadFields] {
        const QString id = paperBox->currentData().toString();
        const auto format = findFormat(id);
        const bool custom = format && !format->builtin;
        editFormatButton->setEnabled(custom);
        deleteFormat->setEnabled(custom);
        QSettings("morfredus", "ComponentHub").setValue("labels/lastFormat", id);
        if (format) {
            preview->setLabelSize(format->labelWidthMm, format->labelHeightMm);
            startRow->setMaximum(format->rows);
            startColumn->setMaximum(format->columns);
        }
        loadFields();
    });

    connect(newFormat, &QPushButton::clicked, this, [paperBox, this] {
        LabelFormat format; format.id = "custom-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        if (!editFormat(this, format)) return;
        auto formats = customFormats(); formats.append(format); saveCustomFormats(formats);
        paperBox->addItem(format.name, format.id); paperBox->setCurrentIndex(paperBox->count() - 1);
    });
    connect(editFormatButton, &QPushButton::clicked, this, [paperBox, this] {
        const QString id = paperBox->currentData().toString();
        auto formats = customFormats();
        for (auto& format : formats) {
            if (format.id == id && editFormat(this, format)) {
                saveCustomFormats(formats);
                const int index = paperBox->currentIndex();
                paperBox->setItemText(index, format.name);
                // Meme index : currentIndexChanged ne part pas tout seul, or l'apercu et les
                // bornes de depart dependent des nouvelles dimensions.
                emit paperBox->currentIndexChanged(index);
                return;
            }
        }
    });
    connect(deleteFormat, &QPushButton::clicked, this, [paperBox, this] {
        const QString id = paperBox->currentData().toString();
        const auto format = findFormat(id);
        if (!format || format->builtin) return;
        if (QMessageBox::question(this, chui::tr("Delete format"), format->name + " ?") != QMessageBox::Yes) return;
        auto formats = customFormats();
        formats.erase(std::remove_if(formats.begin(), formats.end(),
                                     [&id](const LabelFormat& f) { return f.id == id; }),
                      formats.end());
        saveCustomFormats(formats);
        paperBox->removeItem(paperBox->currentIndex());
    });

    for (auto* check : checks)
        connect(check, &QCheckBox::toggled, this, [updatePreview, saveFields] {
            updatePreview();
            saveFields();
        });
    for (auto* size : sizes)
        connect(size, &QComboBox::currentIndexChanged, this, [updatePreview, saveFields] {
            updatePreview();
            saveFields();
        });

    const int savedFormat = paperBox->findData(
        QSettings("morfredus", "ComponentHub").value("labels/lastFormat", "avery8728").toString());
    const int initialFormat = savedFormat >= 0 ? savedFormat : 0;
    if (paperBox->currentIndex() == initialFormat) {
        // Deja sur cet index : le signal ne partira pas, on applique l'etat a la main.
        emit paperBox->currentIndexChanged(initialFormat);
    } else {
        paperBox->setCurrentIndex(initialFormat);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* print = new QPushButton(chui::tr("Print"), this);
    auto* pdf = new QPushButton(chui::tr("Generate PDF"), this);
    print->setEnabled(!components_.empty());
    pdf->setEnabled(!components_.empty());
    buttons->addButton(print, QDialogButtonBox::ActionRole);
    buttons->addButton(pdf, QDialogButtonBox::ActionRole);
    if (auto* close = buttons->button(QDialogButtonBox::Close))
        close->setText(chui::tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Format choisi + premier emplacement, ou std::nullopt (avec message) si le format n'existe plus.
    struct Job { LabelFormat format; int firstSlot; };
    auto currentJob = [paperBox, startRow, startColumn, this]() -> std::optional<Job> {
        const auto format = findFormat(paperBox->currentData().toString());
        if (!format) {
            QMessageBox::warning(this, chui::tr("Labels"), chui::tr("This label format no longer exists."));
            return std::nullopt;
        }
        const int firstSlot = (startRow->value() - 1) * format->columns + (startColumn->value() - 1);
        if (firstSlot >= format->columns * format->rows) return std::nullopt;
        return Job{*format, firstSlot};
    };
    connect(print, &QPushButton::clicked, this, [border, selectedFields, currentJob, this] {
        const auto job = currentJob();
        if (!job) return;
        QPrinter printer(QPrinter::HighResolution);
        printer.setPageSize(QPageSize(QSizeF(job->format.pageWidthMm, job->format.pageHeightMm), QPageSize::Millimeter));
        printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
        QPrintDialog dialog(&printer, this);
        dialog.setWindowTitle(chui::tr("Print labels"));
        if (dialog.exec() != QDialog::Accepted) return;
        QPainter painter(&printer);
        if (!painter.isActive()) {
            QMessageBox::warning(this, chui::tr("Print labels"), chui::tr("Unable to start printing."));
            return;
        }
        renderLabels(printer, painter, components_, job->format, selectedFields(), job->firstSlot, border->isChecked());
        painter.end();
    });
    connect(pdf, &QPushButton::clicked, this, [border, selectedFields, currentJob, this] {
        const auto job = currentJob();
        if (!job) return;
        const QString path = QFileDialog::getSaveFileName(this, chui::tr("Generate PDF"),
                                                          "componenthub-labels.pdf", "PDF (*.pdf)");
        if (path.isEmpty()) return;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QSizeF(job->format.pageWidthMm, job->format.pageHeightMm), QPageSize::Millimeter));
        // The label coordinates are measured from the physical page edge. Do
        // not let QPdfWriter add its default printable margins.
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
        writer.setResolution(300);
        QPainter painter(&writer);
        // Chemin non inscriptible : le painter reste inactif, on le dit au lieu d'annoncer un succes.
        if (!painter.isActive()) {
            QMessageBox::warning(this, chui::tr("Generate PDF"), chui::tr("Unable to write the PDF file."));
            return;
        }
        renderLabels(writer, painter, components_, job->format, selectedFields(), job->firstSlot, border->isChecked());
        painter.end();
        QMessageBox::information(this, chui::tr("Generate PDF"), chui::tr("PDF created successfully."));
    });
    root->addWidget(buttons);
}
