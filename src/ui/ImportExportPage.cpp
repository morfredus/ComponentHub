#include "ui/ImportExportPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <map>
#include <string>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/Language.h"
#include "ui/AppConfigIO.h"
#include "platform/Tar.h"
#include "platform/Backup.h"
#include "csv.h"

using domain::CsvFormat;
using domain::ImportResult;

ImportExportPage::ImportExportPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(14);
    v->addWidget(uikit::pageTitle(chui::tr("Import / Export")));

    // --- Full backup -------------------------------------------------
    auto* backupBox = new QGroupBox(chui::tr("Full backup (everything)"));
    auto* bl = new QVBoxLayout(backupBox);
    bl->addWidget(uikit::hint(chui::tr("A .tar archive containing all data tables, attachments and configuration (hub address, options...) - one file to back up and restore everything.")));
    auto* brow = new QHBoxLayout;
    auto* doBackup = uikit::button(chui::tr("Back up..."), icons::Glyph::Backup);
    auto* doRestore = uikit::button(chui::tr("Restore..."), icons::Glyph::Restore, "danger");
    brow->addWidget(doBackup); brow->addWidget(doRestore); brow->addStretch(1);
    bl->addLayout(brow);
    v->addWidget(backupBox);
    connect(doBackup, &QPushButton::clicked, this, &ImportExportPage::backup);
    connect(doRestore, &QPushButton::clicked, this, &ImportExportPage::restore);

    // --- Backup of the configuration alone --------------------------------
    auto* cfgBox = new QGroupBox(chui::tr("Configuration only"));
    auto* cl = new QVBoxLayout(cfgBox);
    cl->addWidget(uikit::hint(chui::tr("Backs up or restores settings only (hub address, token, synchronization options, appearance) - without data. Useful for deploying the same settings on another workstation.")));
    auto* crow = new QHBoxLayout;
    auto* cfgBackup = uikit::button(chui::tr("Back up configuration..."), icons::Glyph::Backup, "ghost");
    auto* cfgRestore = uikit::button(chui::tr("Restore configuration..."), icons::Glyph::Restore, "ghost");
    crow->addWidget(cfgBackup); crow->addWidget(cfgRestore); crow->addStretch(1);
    cl->addLayout(crow);
    v->addWidget(cfgBox);
    connect(cfgBackup, &QPushButton::clicked, this, &ImportExportPage::backupConfig);
    connect(cfgRestore, &QPushButton::clicked, this, &ImportExportPage::restoreConfig);

    // --- CSV exports / imports per table ------------------------------------
    auto* csvBox = new QGroupBox(chui::tr("CSV tables (editable in a spreadsheet)"));
    auto* grid = new QGridLayout(csvBox);
    int row = 0;
    auto addRow = [&](const QString& label,
                      const std::function<void()>& onExport,
                      const std::function<void()>& onImport) {
        grid->addWidget(new QLabel(label), row, 0);
        // Import then Export order (consistent with the Reference lists page).
        auto* imp = uikit::button(chui::tr("Import"), icons::Glyph::Restore, "ghost");
        auto* exp = uikit::button(chui::tr("Export"), icons::Glyph::Backup, "ghost");
        connect(imp, &QPushButton::clicked, this, onImport);
        connect(exp, &QPushButton::clicked, this, onExport);
        grid->addWidget(imp, row, 1);
        grid->addWidget(exp, row, 2);
        row++;
    };

    addRow(chui::tr("Components (ComponentHub)"),
        [this] { exportCsv([this] { return ctx_.importExport.exportCsv(CsvFormat::Native); }, "componenthub_export.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importCsv(s, CsvFormat::Native); }); });
    addRow(chui::tr("Components (Bomist)"),
        [this] { exportCsv([this] { return ctx_.importExport.exportCsv(CsvFormat::Bomist); }, "bomist_export.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importCsv(s, CsvFormat::Bomist); }); });
    addRow(chui::tr("Categories"),
        [this] { exportCsv([this] { return ctx_.importExport.exportCategoriesCsv(); }, "componenthub_categories.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importCategoriesCsv(s); }); });
    addRow(chui::tr("Locations"),
        [this] { exportCsv([this] { return ctx_.importExport.exportLocationsCsv(); }, "componenthub_locations.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importLocationsCsv(s); }); });
    addRow(chui::tr("Projects"),
        [this] { exportCsv([this] { return ctx_.importExport.exportProjectsCsv(); }, "componenthub_projects.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importProjectsCsv(s); }); });

    v->addWidget(csvBox);
    v->addStretch(1);
}

void ImportExportPage::exportCsv(const std::function<std::string()>& producer, const QString& defaultName) {
    const QString path = QFileDialog::getSaveFileName(this, chui::tr("Export to CSV"), defaultName, "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, chui::tr("Export", "title"), chui::tr("Unable to write the file."));
        return;
    }
    f.write(domain::kUtf8Bom);              // UTF-8 BOM -> correct "€" and accents in a spreadsheet
    f.write(producer().c_str());
    f.close();
    QMessageBox::information(this, chui::tr("Export", "title"), chui::tr("Export complete:\n%1").arg(path));
}

void ImportExportPage::importCsv(const std::function<ImportResult(const std::string&)>& consumer) {
    const QString path = QFileDialog::getOpenFileName(this, chui::tr("Import CSV"), QString(), "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, chui::tr("Import", "title"), chui::tr("Unable to read the file."));
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    const ImportResult r = consumer(std::string(raw.constData(), (size_t)raw.size()));
    QString msg = QString(chui::tr("%1 created, %2 updated, %3 failed."))
                      .arg(r.created).arg(r.updated).arg(r.failed);
    for (const auto& e : r.errors) msg += "\n• " + chui::archiveError(QString::fromStdString(e));
    QMessageBox::information(this, chui::tr("Import complete"), msg);
}

void ImportExportPage::backup() {
    const QString path = QFileDialog::getSaveFileName(this, chui::tr("Full backup"), "componenthub_backup.tar", "Archive TAR (*.tar)");
    if (path.isEmpty()) return;
    const QString dataDir = QString::fromStdString(ctx_.dir());
    // Old config file left by previous versions (it contained the token):
    // the configuration is now added directly to the archive, never written in clear text
    // in the data folder.
    QFile::remove(chui::configPathInDir(dataDir));

    // Consistent snapshot of the database (VACUUM INTO) + auxiliary files + configuration WITHOUT
    // the synchronization token (to be entered again after a restore on another machine).
    std::string error;
    const std::map<std::string, std::string> extra{
        {"app_config.json", chui::configJson(false).toStdString()}};
    if (chdesktop::backup::create(ctx_.database(), ctx_.dir(), path.toStdString(), extra, error))
        QMessageBox::information(this, chui::tr("Backup"),
            chui::tr("Full backup created (data, attachments and configuration, without the sync token):\n%1").arg(path));
    else
        QMessageBox::warning(this, chui::tr("Backup"),
            chui::tr("Failed to create the archive.") + "\n" + chui::archiveError(QString::fromStdString(error)));
}

void ImportExportPage::restore() {
    const QString path = QFileDialog::getOpenFileName(this, chui::tr("Restore a backup"), QString(), "Archive TAR (*.tar)");
    if (path.isEmpty()) return;
    if (!chdesktop::tar::looksLikeTar(path.toStdString())) {
        QMessageBox::warning(this, chui::tr("Restore", "title"), chui::tr("This file is not a ComponentHub backup (.tar)."));
        return;
    }
    if (QMessageBox::question(this, chui::tr("Restore"),
            chui::tr("All current data (components, categories, locations, stock movements, documents, projects) will be REPLACED by the backup at the next start.\n\nThe previous database (componenthub.sqlite.bak) and the previous attachments (attachments.bak folder) will be kept alongside. Continue?")) != QMessageBox::Yes)
        return;

    // The database is open: it is not overwritten while running. The archive is extracted separately and
    // validated (integrity_check); the replacement happens at the next start, before
    // the database is opened (see applyPendingRestore in main.cpp).
    std::vector<std::string> warnings;
    std::string error;
    if (!chdesktop::backup::stageRestore(path.toStdString(), ctx_.dir(), warnings, error)) {
        QMessageBox::warning(this, chui::tr("Restore", "title"),
            chui::tr("Invalid backup, nothing was changed:\n%1").arg(chui::archiveError(QString::fromStdString(error))));
        return;
    }
    // Configuration contained in the archive: applied right away (without a token, so the
    // current token is kept), then removed so that it does not end up in the data folder.
    const QString cfgFile = QString::fromStdString(ctx_.dir()) + "/" + chdesktop::backup::kPendingDir + "/app_config.json";
    const bool cfgRestored = QFile::exists(cfgFile) && chui::importConfig(cfgFile);
    QFile::remove(cfgFile);

    QString msg = chui::tr("Backup verified and queued.");
    if (cfgRestored) msg += chui::tr("\nConfiguration restored.");
    for (const auto& e : warnings) msg += "\n• " + chui::archiveError(QString::fromStdString(e));
    msg += chui::tr("\n\nRESTART the application to finish the restoration: the data will only be replaced at that point.");
    QMessageBox::information(this, chui::tr("Restore ready"), msg);
}

void ImportExportPage::backupConfig() {
    const QString path = QFileDialog::getSaveFileName(
         this, chui::tr("Back up configuration"), "componenthub_config.json", "Configuration JSON (*.json)");
    if (path.isEmpty()) return;
    // The synchronization token is a secret (and travels over plain HTTP to the hub: trusted
    // LAN only): never exported without an explicit request.
    const bool withToken = QMessageBox::question(this, chui::tr("Configuration"),
        chui::tr("Include the synchronization token in the file?\n(It will be readable in clear text.)"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    if (chui::exportConfig(path, withToken))
        QMessageBox::information(this, chui::tr("Configuration"), chui::tr("Configuration saved:\n%1").arg(path));
    else
        QMessageBox::warning(this, chui::tr("Configuration"), chui::tr("Failed to write the file."));
}

void ImportExportPage::restoreConfig() {
    const QString path = QFileDialog::getOpenFileName(
         this, chui::tr("Restore configuration"), QString(), "Configuration JSON (*.json)");
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, chui::tr("Restore configuration"),
            chui::tr("Current settings (hub address, options, appearance) will be replaced by those in the file. Continue?")) != QMessageBox::Yes)
        return;
    if (chui::importConfig(path))
        QMessageBox::information(this, chui::tr("Configuration"),
            chui::tr("Configuration restored.\nRestart the application to apply it fully."));
    else
        QMessageBox::warning(this, chui::tr("Configuration"), chui::tr("Invalid configuration file."));
}
