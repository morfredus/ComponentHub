#include "ui/ImportExportPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>

#include "AppContext.h"
#include "ui/UiKit.h"
#include "ui/AppConfigIO.h"
#include "platform/Tar.h"
#include "csv.h"

using domain::CsvFormat;
using domain::ImportResult;

ImportExportPage::ImportExportPage(chdesktop::AppContext& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(14);
    v->addWidget(uikit::pageTitle("Import / Export"));

    // --- Sauvegarde complète -------------------------------------------------
    auto* backupBox = new QGroupBox("Sauvegarde complète (tout compris)");
    auto* bl = new QVBoxLayout(backupBox);
    bl->addWidget(uikit::hint("Archive .tar réunissant toutes les tables de données, les fichiers "
                              "joints ET la configuration (adresse du hub, options…) — un seul "
                              "fichier pour tout sauvegarder puis tout restaurer."));
    auto* brow = new QHBoxLayout;
    auto* doBackup = uikit::button("Sauvegarder…", icons::Glyph::Backup);
    auto* doRestore = uikit::button("Restaurer…", icons::Glyph::Restore, "danger");
    brow->addWidget(doBackup); brow->addWidget(doRestore); brow->addStretch(1);
    bl->addLayout(brow);
    v->addWidget(backupBox);
    connect(doBackup, &QPushButton::clicked, this, &ImportExportPage::backup);
    connect(doRestore, &QPushButton::clicked, this, &ImportExportPage::restore);

    // --- Sauvegarde de la configuration seule --------------------------------
    auto* cfgBox = new QGroupBox("Configuration seule");
    auto* cl = new QVBoxLayout(cfgBox);
    cl->addWidget(uikit::hint("Sauvegarde/restaure uniquement les réglages (adresse du hub, jeton, "
                              "options de synchro, apparence) — sans les données. Pratique pour "
                              "déployer les mêmes réglages sur un autre poste."));
    auto* crow = new QHBoxLayout;
    auto* cfgBackup = uikit::button("Sauvegarder la config…", icons::Glyph::Backup, "ghost");
    auto* cfgRestore = uikit::button("Restaurer la config…", icons::Glyph::Restore, "ghost");
    crow->addWidget(cfgBackup); crow->addWidget(cfgRestore); crow->addStretch(1);
    cl->addLayout(crow);
    v->addWidget(cfgBox);
    connect(cfgBackup, &QPushButton::clicked, this, &ImportExportPage::backupConfig);
    connect(cfgRestore, &QPushButton::clicked, this, &ImportExportPage::restoreConfig);

    // --- Exports / imports CSV par table ------------------------------------
    auto* csvBox = new QGroupBox("Tables au format CSV (éditables en tableur)");
    auto* grid = new QGridLayout(csvBox);
    int row = 0;
    auto addRow = [&](const QString& label,
                      const std::function<void()>& onExport,
                      const std::function<void()>& onImport) {
        grid->addWidget(new QLabel(label), row, 0);
        // Ordre Importer puis Exporter (cohérent avec la page Référentiels).
        auto* imp = uikit::button("Importer", icons::Glyph::Restore, "ghost");
        auto* exp = uikit::button("Exporter", icons::Glyph::Backup, "ghost");
        connect(imp, &QPushButton::clicked, this, onImport);
        connect(exp, &QPushButton::clicked, this, onExport);
        grid->addWidget(imp, row, 1);
        grid->addWidget(exp, row, 2);
        row++;
    };

    addRow("Composants (ComponentHub)",
        [this] { exportCsv([this] { return ctx_.importExport.exportCsv(CsvFormat::Native); }, "componenthub_export.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importCsv(s, CsvFormat::Native); }); });
    addRow("Composants (Bomist)",
        [this] { exportCsv([this] { return ctx_.importExport.exportCsv(CsvFormat::Bomist); }, "bomist_export.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importCsv(s, CsvFormat::Bomist); }); });
    addRow("Catégories",
        [this] { exportCsv([this] { return ctx_.importExport.exportCategoriesCsv(); }, "componenthub_categories.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importCategoriesCsv(s); }); });
    addRow("Emplacements",
        [this] { exportCsv([this] { return ctx_.importExport.exportLocationsCsv(); }, "componenthub_locations.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importLocationsCsv(s); }); });
    addRow("Projets",
        [this] { exportCsv([this] { return ctx_.importExport.exportProjectsCsv(); }, "componenthub_projects.csv"); },
        [this] { importCsv([this](const std::string& s) { return ctx_.importExport.importProjectsCsv(s); }); });

    v->addWidget(csvBox);
    v->addStretch(1);
}

void ImportExportPage::exportCsv(const std::function<std::string()>& producer, const QString& defaultName) {
    const QString path = QFileDialog::getSaveFileName(this, "Exporter en CSV", defaultName, "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Export", "Impossible d'écrire le fichier.");
        return;
    }
    f.write(domain::kUtf8Bom);              // BOM UTF-8 -> « € » et accents corrects en tableur
    f.write(producer().c_str());
    f.close();
    QMessageBox::information(this, "Export", "Export terminé :\n" + path);
}

void ImportExportPage::importCsv(const std::function<ImportResult(const std::string&)>& consumer) {
    const QString path = QFileDialog::getOpenFileName(this, "Importer un CSV", QString(), "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Import", "Impossible de lire le fichier.");
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    const ImportResult r = consumer(std::string(raw.constData(), (size_t)raw.size()));
    QString msg = QString("%1 créé(s), %2 mis à jour, %3 échec(s).")
                      .arg(r.created).arg(r.updated).arg(r.failed);
    for (const auto& e : r.errors) msg += "\n• " + QString::fromStdString(e);
    QMessageBox::information(this, "Import terminé", msg);
}

void ImportExportPage::backup() {
    const QString path = QFileDialog::getSaveFileName(this, "Sauvegarde complète", "componenthub_backup.tar", "Archive TAR (*.tar)");
    if (path.isEmpty()) return;
    // Dépose la configuration (QSettings) dans le dossier de données AVANT
    // l'archivage, pour qu'elle soit incluse dans le .tar.
    chui::exportConfig(chui::configPathInDir(QString::fromStdString(ctx_.dir())));
    if (chdesktop::tar::pack(path.toStdString(), ctx_.dir()))
        QMessageBox::information(this, "Sauvegarde", "Sauvegarde complète créée (données + configuration) :\n" + path);
    else
        QMessageBox::warning(this, "Sauvegarde", "Échec de la création de l'archive.");
}

void ImportExportPage::restore() {
    const QString path = QFileDialog::getOpenFileName(this, "Restaurer une sauvegarde", QString(), "Archive TAR (*.tar)");
    if (path.isEmpty()) return;
    if (!chdesktop::tar::looksLikeTar(path.toStdString())) {
        QMessageBox::warning(this, "Restauration", "Ce fichier n'est pas une sauvegarde ComponentHub (.tar).");
        return;
    }
    if (QMessageBox::question(this, "Restaurer",
            "Toutes les données actuelles (composants, catégories, emplacements, "
            "mouvements de stock, documents, projets) seront ÉCRASÉES par la sauvegarde.\n\n"
            "Cette action est irréversible. Continuer ?") != QMessageBox::Yes)
        return;

    std::vector<std::string> errors;
    const int n = chdesktop::tar::extract(path.toStdString(), ctx_.dir(), errors);
    // Recharge la configuration si l'archive en contenait une.
    const QString cfgFile = chui::configPathInDir(QString::fromStdString(ctx_.dir()));
    const bool cfgRestored = QFile::exists(cfgFile) && chui::importConfig(cfgFile);
    QString msg = QString("%1 fichier(s) restauré(s).").arg(n);
    if (cfgRestored) msg += "\nConfiguration restaurée.";
    for (const auto& e : errors) msg += "\n• " + QString::fromStdString(e);
    msg += "\n\nRedémarrez l'application pour recharger les données et la configuration.";
    QMessageBox::information(this, "Restauration terminée", msg);
}

void ImportExportPage::backupConfig() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Sauvegarder la configuration", "componenthub_config.json", "Configuration JSON (*.json)");
    if (path.isEmpty()) return;
    if (chui::exportConfig(path))
        QMessageBox::information(this, "Configuration", "Configuration sauvegardée :\n" + path);
    else
        QMessageBox::warning(this, "Configuration", "Échec de l'écriture du fichier.");
}

void ImportExportPage::restoreConfig() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Restaurer la configuration", QString(), "Configuration JSON (*.json)");
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, "Restaurer la configuration",
            "Les réglages actuels (adresse du hub, options, apparence) seront remplacés "
            "par ceux du fichier. Continuer ?") != QMessageBox::Yes)
        return;
    if (chui::importConfig(path))
        QMessageBox::information(this, "Configuration",
            "Configuration restaurée.\nRedémarrez l'application pour l'appliquer entièrement.");
    else
        QMessageBox::warning(this, "Configuration", "Fichier de configuration invalide.");
}
