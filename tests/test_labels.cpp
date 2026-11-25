#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTranslator>

#include "ui/LabelDialog.h"
#include "ui/LabelPrinting.h"
#include "test_util.h"

// A translator independent of installed catalogs, exercising the same chui context.
class TestTranslator : public QTranslator {
public:
    bool isEmpty() const override { return false; }
    QString translate(const char* context, const char* source, const char*, int) const override {
        if (QString::fromUtf8(context) == "chui" && QString::fromUtf8(source) == "Reference")
            return "Referencia";
        return {};
    }
};

static QComboBox* paperBox(LabelDialog& dialog) {
    for (auto* combo : dialog.findChildren<QComboBox*>())
        if (combo->findData("avery8728") >= 0) return combo;
    return nullptr;
}

static void testPrinterMargins() {
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setResolution(300);
    const QSizeF paperSize(210, 297);
    chui::configureLabelPrinter(printer, paperSize);

    // Simulate the print dialog replacing the layout and adding printer margins.
    printer.setFullPage(false);
    printer.setPageSize(QPageSize(QPageSize::Letter));
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageMargins(QMarginsF(12, 15, 12, 15), QPageLayout::Millimeter);
    CHECK(!printer.fullPage());
    CHECK(printer.pageRect(QPrinter::DevicePixel) != printer.paperRect(QPrinter::DevicePixel));

    chui::configureLabelPrinter(printer, paperSize);
    CHECK(printer.fullPage());
    CHECK(printer.pageLayout().orientation() == QPageLayout::Portrait);
    CHECK(printer.pageLayout().pageSize().size(QPageSize::Millimeter) == paperSize);
    CHECK(printer.pageLayout().margins(QPageLayout::Millimeter) == QMarginsF());
    CHECK(printer.pageRect(QPrinter::DevicePixel) == printer.paperRect(QPrinter::DevicePixel));
    // The renderer derives mm from device.width(): it must cover the full sheet.
    CHECK(printer.width() == printer.pageLayout().fullRectPixels(printer.resolution()).width());
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    testPrinterMargins();
    QTemporaryDir settingsDir;
    CHECK(settingsDir.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir.path());
    QSettings settings("morfredus", "ComponentHub");
    // Settings from an older release, without caption keys.
    settings.setValue("labels/fields/avery8728/enabled1", false);
    settings.setValue("labels/fields/avery8728/size1", 2);
    settings.sync();
    {
        LabelDialog dialog({});
        QList<QLineEdit*> captions;
        for (int i = 0; i < 6; ++i)
            captions.append(dialog.findChild<QLineEdit*>(QStringLiteral("labelFieldCaption%1").arg(i)));
        CHECK(captions.size() == 6);
        CHECK(captions.at(0)->text() == "Name");
        CHECK(captions.at(1)->text() == "Reference");
        CHECK(!dialog.findChild<QCheckBox*>("labelFieldEnabled1")->isChecked());
        CHECK(dialog.findChild<QComboBox*>("labelFieldSize1")->currentIndex() == 2);
        captions.at(1)->setText("Ref");
        captions.at(0)->clear();
        auto* paper = paperBox(dialog);
        CHECK(paper != nullptr);
        paper->setCurrentIndex(paper->findData("avery8790"));
        CHECK(captions.at(0)->text() == "Name");
        CHECK(captions.at(1)->text() == "Reference");
        captions.at(1)->setText("R");
        paper->setCurrentIndex(paper->findData("avery8728"));
        CHECK(captions.at(0)->text().isEmpty());
        CHECK(captions.at(1)->text() == "Ref");
        CHECK(!dialog.findChild<QCheckBox*>("labelFieldEnabled1")->isChecked());
    }
    TestTranslator translator;
    app.installTranslator(&translator);
    {
        LabelDialog dialog({});
        QList<QLineEdit*> captions;
        for (int i = 0; i < 6; ++i)
            captions.append(dialog.findChild<QLineEdit*>(QStringLiteral("labelFieldCaption%1").arg(i)));
        CHECK(captions.at(0)->text().isEmpty());
        CHECK(captions.at(1)->text() == "Ref"); // User text survives a language change.
        auto* paper = paperBox(dialog);
        paper->setCurrentIndex(paper->findData("avery8790"));
        CHECK(captions.at(1)->text() == "R");
    }
    // Untouched defaults must remain translated defaults after other preferences change.
    settings.remove("labels/fields/avery8790/caption1");
    settings.setValue("labels/lastFormat", "avery8790");
    settings.sync();
    {
        LabelDialog dialog({});
        CHECK(dialog.findChild<QLineEdit*>("labelFieldCaption1")->text() == "Referencia");
        dialog.findChild<QCheckBox*>("labelFieldEnabled1")->toggle();
    }
    settings.sync();
    CHECK(!settings.contains("labels/fields/avery8790/caption1"));
    app.removeTranslator(&translator);
    std::printf(g_failures ? "%d failure(s)\n" : "label preferences: ok\n", g_failures);
    return g_failures ? 1 : 0;
}
