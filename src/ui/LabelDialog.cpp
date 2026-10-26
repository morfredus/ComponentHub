#include "ui/LabelDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrintDialog>
#include <QPageSize>
#include <QMarginsF>
#include <QSettings>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QPdfWriter>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QVector>
#include <QStringList>
#include <QComboBox>
#include <QSignalBlocker>
#include <algorithm>
#include <memory>

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
        if (!f.id.isEmpty() && !f.name.isEmpty()) formats.append(f);
    }
    settings.endArray();
    return formats;
}

void saveCustomFormats(const QVector<LabelFormat>& formats) {
    QSettings settings("morfredus", "ComponentHub");
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

QStringList componentSpecifications(const domain::Component& component) {
    QStringList specifications;
    if (!component.voltage.empty()) specifications << QString::fromStdString(component.voltage);
    if (!component.current.empty()) specifications << QString::fromStdString(component.current);
    if (!component.interfaceType.empty()) specifications << QString::fromStdString(component.interfaceType);
    if (!component.protocols.empty()) specifications << QString::fromStdString(component.protocols);
    if (!component.i2cAddress.empty()) specifications << QString::fromStdString(component.i2cAddress);
    if (!component.frequency.empty()) specifications << QString::fromStdString(component.frequency);
    if (component.pinCount > 0) specifications << QStringLiteral("%1 pins").arg(component.pinCount);
    if (!component.compatibility.empty()) specifications << QString::fromStdString(component.compatibility);
    return specifications;
}

int fittingPointSize(const QString& text, int requested, const QFont& base, int width) {
    int size = requested;
    while (size > 6) {
        QFont font = base;
        font.setPointSize(size);
        if (QFontMetrics(font).horizontalAdvance(text) <= width) break;
        --size;
    }
    return size;
}

int fittingPixelSize(const QString& text, int requested, const QFont& base, int width) {
    int size = requested;
    while (size > 8) {
        QFont font = base;
        font.setPixelSize(size);
        if (QFontMetrics(font).horizontalAdvance(text) <= width) break;
        --size;
    }
    return size;
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

        struct Line { QString text; int size; };
        QVector<Line> lines;
        const auto& c = components.at(index);
        for (int i = 0; i < fields.size(); ++i) if (fields.at(i).enabled) {
            const int fontSize = fields.at(i).size;
            if (i == 0) lines.append({QString::fromStdString(c.name), fontSize});
            else if (i == 1 && !c.reference.empty()) lines.append({"Ref: " + QString::fromStdString(c.reference), fontSize});
            else if (i == 2 && !c.category.empty()) lines.append({"Category: " + QString::fromStdString(c.category), fontSize});
            else if (i == 3 && !c.subcategory.empty()) lines.append({"Subcategory: " + QString::fromStdString(c.subcategory), fontSize});
            else if (i == 4 && !c.type.empty()) lines.append({"Type: " + QString::fromStdString(c.type), fontSize});
            else if (i == 5) {
                const QStringList specifications = componentSpecifications(c);
                if (!specifications.isEmpty()) lines.append({specifications.join(" "), fontSize});
            }
        }

        painter.setPen(Qt::black);
        const int lineSpacing = 7;
        QVector<int> lineHeights;
        int totalHeight = lineSpacing * std::max(0, static_cast<int>(lines.size()) - 1);
        for (const auto& line : lines) {
            QFont font = painter.font();
            const int requestedPixels = std::max(8, static_cast<int>(
                std::lround(line.size * 96.0 / 72.0 * labelW / 400.0)));
            font.setPixelSize(fittingPixelSize(line.text, requestedPixels, font,
                                               static_cast<int>(label.width() - labelW * 0.10)));
            const int height = QFontMetrics(font).height();
            lineHeights.append(height);
            totalHeight += height;
        }
        int y = static_cast<int>(label.top() + (labelH - totalHeight) / 2.0);
        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const auto& line = lines.at(lineIndex);
            QFont font = painter.font();
            const int requestedPixels = std::max(8, static_cast<int>(
                std::lround(line.size * 96.0 / 72.0 * labelW / 400.0)));
            font.setPixelSize(fittingPixelSize(line.text, requestedPixels, font,
                                               static_cast<int>(label.width() - labelW * 0.10)));
            painter.setFont(font);
            painter.drawText(label.left() + labelW * 0.05, y,
                             label.width() - labelW * 0.10, lineHeights.at(lineIndex),
                             Qt::AlignLeft | Qt::AlignVCenter, line.text);
            y += lineHeights.at(lineIndex) + lineSpacing;
        }
    }
}

class LabelPreview : public QWidget {
public:
    explicit LabelPreview(const domain::Component& component, QWidget* parent = nullptr)
        : QWidget(parent), component_(&component) {
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

        struct Line { QString text; int size; };
        QVector<Line> lines;
        if (fields_.value(0).enabled) lines.append({QString::fromStdString(component_->name), fields_.value(0).size});
        if (fields_.value(1).enabled && !component_->reference.empty())
            lines.append({QStringLiteral("Ref: ") + QString::fromStdString(component_->reference), fields_.value(1).size});
        if (fields_.value(2).enabled && !component_->category.empty())
            lines.append({QStringLiteral("Category: ") + QString::fromStdString(component_->category), fields_.value(2).size});
        if (fields_.value(3).enabled && !component_->subcategory.empty())
            lines.append({QStringLiteral("Subcategory: ") + QString::fromStdString(component_->subcategory), fields_.value(3).size});
        if (fields_.value(4).enabled && !component_->type.empty())
            lines.append({QStringLiteral("Type: ") + QString::fromStdString(component_->type), fields_.value(4).size});
        if (fields_.value(5).enabled) {
            const QStringList specifications = componentSpecifications(*component_);
            if (!specifications.isEmpty()) lines.append({specifications.join("  "), fields_.value(5).size});
        }

        painter.setPen(QColor("#18202a"));
        const int lineSpacing = 7;
        QVector<int> lineHeights;
        int totalHeight = lineSpacing * std::max(0, static_cast<int>(lines.size()) - 1);
        for (const auto& line : lines) {
            QFont font = painter.font();
            font.setPointSize(fittingPointSize(line.text, line.size, font,
                                               static_cast<int>(label.width() - 44)));
            const int height = QFontMetrics(font).height();
            lineHeights.append(height);
            totalHeight += height;
        }
        int y = static_cast<int>(label.center().y() - totalHeight / 2.0);
        for (int i = 0; i < lines.size(); ++i) {
            const auto& line = lines.at(i);
            QFont font = painter.font();
            font.setPointSize(fittingPointSize(line.text, line.size, font,
                                               static_cast<int>(label.width() - 44)));
            painter.setFont(font);
            painter.drawText(label.left() + 22, y, label.width() - 44, lineHeights.at(i),
                             Qt::AlignLeft | Qt::AlignVCenter, line.text);
            y += lineHeights.at(i) + lineSpacing;
        }
    }

private:
    const domain::Component* component_;
    QVector<LabelField> fields_{{true, 24}, {true, 16}, {false, 16}, {false, 16}, {false, 16}, {true, 16}};
    double labelAspect_ = 30.5 / 16.9;
};

} // namespace

LabelDialog::LabelDialog(const std::vector<domain::Component>& components, QWidget* parent)
    : QDialog(parent) {
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
    QVector<bool> selected{true, true, false, false, false, true};
    QVector<QCheckBox*> checks;
    QVector<QComboBox*> sizes;
    const QStringList sizeLabels = {chui::tr("Large"), chui::tr("Medium"), chui::tr("Small")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* row = new QWidget(options);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* check = new QCheckBox(labels.at(i));
        check->setChecked(selected.at(i));
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
    if (components.empty()) return;
    auto* preview = new LabelPreview(components.front());
    preview->setLabelSize(30.5, 16.9);
    previewLayout->addWidget(preview);
    auto* previous = new QPushButton("<", previewBox);
    auto* next = new QPushButton(">", previewBox);
    auto* previewPosition = new QLabel(QStringLiteral("1/%1").arg(components.size()), previewBox);
    previewPosition->setAlignment(Qt::AlignCenter);
    previous->setEnabled(false);
    next->setEnabled(components.size() > 1);
    auto* previewNavigation = new QHBoxLayout;
    previewNavigation->addStretch(1);
    previewNavigation->addWidget(previous);
    previewNavigation->addWidget(previewPosition);
    previewNavigation->addWidget(next);
    previewNavigation->addStretch(1);
    previewLayout->addLayout(previewNavigation);
    auto previewIndex = std::make_shared<int>(0);
    auto updatePreviewPosition = [previewPosition, previewIndex, &components] {
        previewPosition->setText(QStringLiteral("%1/%2").arg(*previewIndex + 1).arg(components.size()));
    };
    connect(previous, &QPushButton::clicked, this, [&, preview, previous, next, previewIndex, updatePreviewPosition] {
        if (*previewIndex <= 0) return;
        preview->setComponent(components.at(--*previewIndex));
        previous->setEnabled(*previewIndex > 0);
        next->setEnabled(*previewIndex + 1 < static_cast<int>(components.size()));
        updatePreviewPosition();
    });
    connect(next, &QPushButton::clicked, this, [&, preview, previous, next, previewIndex, updatePreviewPosition] {
        if (*previewIndex + 1 >= static_cast<int>(components.size())) return;
        preview->setComponent(components.at(++*previewIndex));
        previous->setEnabled(*previewIndex > 0);
        next->setEnabled(*previewIndex + 1 < static_cast<int>(components.size()));
        updatePreviewPosition();
    });
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

    auto updatePreview = [checks, sizes, preview] {
        QVector<LabelField> fields;
        for (int i = 0; i < sizes.size(); ++i)
            fields.append({checks.at(i)->isChecked(), sizes.at(i)->currentIndex() == 0 ? 24 : sizes.at(i)->currentIndex() == 1 ? 16 : 12});
        preview->setFields(fields);
    };
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
    auto loadFields = [checks, sizes, paperBox, updatePreview] {
        const QString id = paperBox->currentData().toString();
        QSettings settings("morfredus", "ComponentHub");
        settings.beginGroup("labels/fields/" + id);
        const QVector<bool> defaults{true, true, false, false, false, true};
        for (int i = 0; i < checks.size(); ++i) {
            const QSignalBlocker checkBlocker(checks.at(i));
            const QSignalBlocker sizeBlocker(sizes.at(i));
            checks.at(i)->setChecked(settings.value(QStringLiteral("enabled%1").arg(i), defaults.at(i)).toBool());
            sizes.at(i)->setCurrentIndex(settings.value(QStringLiteral("size%1").arg(i), i == 0 ? 0 : 1).toInt());
        }
        settings.endGroup();
        updatePreview();
    };

    connect(paperBox, &QComboBox::currentIndexChanged, this, [paperBox, preview, startRow, startColumn, editFormatButton, deleteFormat, loadFields] {
        const bool custom = paperBox->currentData().toString() != "avery8728"
                         && paperBox->currentData().toString() != "avery8790";
        editFormatButton->setEnabled(custom);
        deleteFormat->setEnabled(custom);
        QSettings settings("morfredus", "ComponentHub");
        settings.setValue("labels/lastFormat", paperBox->currentData().toString());
        if (paperBox->currentData().toString() == "avery8728") {
            preview->setLabelSize(30.5, 16.9);
            startRow->setMaximum(16); startColumn->setMaximum(6);
            loadFields();
            return;
        }
        if (paperBox->currentData().toString() == "avery8790") {
            preview->setLabelSize(17.8, 10);
            startRow->setMaximum(27); startColumn->setMaximum(10);
            loadFields();
            return;
        }
        for (const auto& format : customFormats()) {
            if (format.id == paperBox->currentData().toString()) {
                preview->setLabelSize(format.labelWidthMm, format.labelHeightMm);
                startRow->setMaximum(format.rows); startColumn->setMaximum(format.columns);
                loadFields();
                return;
            }
        }
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
                paperBox->setCurrentIndex(index);
                return;
            }
        }
    });
    connect(deleteFormat, &QPushButton::clicked, this, [paperBox, this] {
        if (paperBox->currentData().toString() == "avery8728") return;
        auto formats = customFormats();
        formats.erase(std::remove_if(formats.begin(), formats.end(), [paperBox](const LabelFormat& f) {
            return f.id == paperBox->currentData().toString();
        }), formats.end());
        saveCustomFormats(formats); paperBox->removeItem(paperBox->currentIndex());
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

    QSettings settings("morfredus", "ComponentHub");
    const int savedFormat = paperBox->findData(settings.value("labels/lastFormat", "avery8728").toString());
    paperBox->setCurrentIndex(savedFormat >= 0 ? savedFormat : 0);
    loadFields();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* print = new QPushButton(chui::tr("Print"), this);
    auto* pdf = new QPushButton(chui::tr("Generate PDF"), this);
    buttons->addButton(print, QDialogButtonBox::ActionRole);
    buttons->addButton(pdf, QDialogButtonBox::ActionRole);
    if (auto* close = buttons->button(QDialogButtonBox::Close))
        close->setText(chui::tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto selectedFields = [checks, sizes] {
        QVector<LabelField> fields;
        for (int i = 0; i < sizes.size(); ++i)
            fields.append({checks.at(i)->isChecked(), sizes.at(i)->currentIndex() == 0 ? 24 : sizes.at(i)->currentIndex() == 1 ? 16 : 12});
        return fields;
    };
    auto selectedFormat = [paperBox] {
        LabelFormat format;
        if (paperBox->currentData().toString() == "avery8728") {
            format.name = "Avery No. 8728";
            format.pageWidthMm = 210; format.pageHeightMm = 297;
            format.columns = 6; format.rows = 16;
            format.labelWidthMm = 30.5; format.labelHeightMm = 16.9;
            format.marginLeftMm = 7; format.marginTopMm = 13.3;
            format.pitchXmm = 32.6; format.pitchYmm = 16.9;
        } else if (paperBox->currentData().toString() == "avery8790") {
            format.name = "Avery No. 8790";
            format.pageWidthMm = 210; format.pageHeightMm = 297;
            format.columns = 10; format.rows = 27;
            format.labelWidthMm = 17.8; format.labelHeightMm = 10;
            format.marginLeftMm = 4; format.marginTopMm = 14.01;
            format.pitchXmm = 20.5; format.pitchYmm = 10;
        } else {
            for (const auto& candidate : customFormats())
                if (candidate.id == paperBox->currentData().toString()) format = candidate;
        }
        return format;
    };
    auto startSlot = [startRow, startColumn, paperBox] {
        int columns = 6;
        if (paperBox->currentData().toString() == "avery8790") columns = 10;
        else if (paperBox->currentData().toString() != "avery8728") {
            for (const auto& format : customFormats())
                if (format.id == paperBox->currentData().toString()) columns = format.columns;
        }
        return (startRow->value() - 1) * columns + (startColumn->value() - 1);
    };
    connect(print, &QPushButton::clicked, this, [components, paperBox, startRow, startColumn, border,
                                                  selectedFields, selectedFormat, startSlot, this] {
        QPrinter printer(QPrinter::HighResolution);
        const LabelFormat format = selectedFormat();
        printer.setPageSize(QPageSize(QSizeF(format.pageWidthMm, format.pageHeightMm), QPageSize::Millimeter));
        printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
        QPrintDialog dialog(&printer, this);
        dialog.setWindowTitle(chui::tr("Print labels"));
        if (dialog.exec() != QDialog::Accepted) return;
        const int firstSlot = startSlot();
        if (firstSlot >= format.columns * format.rows) return;
        QPainter painter(&printer);
        renderLabels(printer, painter, components, format, selectedFields(), firstSlot, border->isChecked());
        painter.end();
    });
    connect(pdf, &QPushButton::clicked, this, [components, paperBox, startRow, startColumn, border,
                                                selectedFields, selectedFormat, startSlot, this] {
        const QString path = QFileDialog::getSaveFileName(this, chui::tr("Generate PDF"),
                                                          "componenthub-labels.pdf", "PDF (*.pdf)");
        if (path.isEmpty()) return;
        const LabelFormat format = selectedFormat();
        const int firstSlot = startSlot();
        const int labelsPerPage = format.columns * format.rows;
        if (firstSlot >= labelsPerPage) return;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QSizeF(format.pageWidthMm, format.pageHeightMm), QPageSize::Millimeter));
        // The label coordinates are measured from the physical page edge. Do
        // not let QPdfWriter add its default printable margins.
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
        writer.setResolution(300);
        QPainter painter(&writer);
        renderLabels(writer, painter, components, format, selectedFields(), firstSlot, border->isChecked());
        painter.end();
        QMessageBox::information(this, chui::tr("Generate PDF"), chui::tr("PDF created successfully."));
    });
    root->addWidget(buttons);
}
